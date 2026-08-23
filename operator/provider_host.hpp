// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_PROVIDER_HOST_HPP
#define ZENGINE_OPERATOR_PROVIDER_HOST_HPP

// MOUNTING A PROVIDER (PROV-0) — the host's side of provider_abi.h, and the whole
// of what a host writes to acquire semantic power it does not author.
//
//     op::Catalog operators;                                  // authors NOTHING
//     op::mount_provider(operators, dir + "/zengine-operators-basic.so");
//     op::mount_provider(operators, dir + "/zengine-timer.so");
//     op::OperatorHostSurface surface(operators);
//
// After those four lines the host's live resolution carries `math.max`,
// `logic.select_int` and `timer.normalize_delay`, and the host's own translation
// unit contains not one of them. It knows WHICH ARTIFACTS to mount and HOW to host
// operators; it does not know what any of them means.
//
// ---- WHAT A MOUNT OWNS, AND FOR HOW LONG -----------------------------------
//
//     ProviderRecord      identity + the table + ONE SHARE OF THE IMAGE
//         held by         every NATIVE contribution's callable (through the
//                         closure), and by the catalog, until the provider is
//                         unmounted
//
// LOG-R1 measured the mistake this shape avoids: a hold that covered the IMAGE but
// not the definition's own record dangled on the host's side the moment the
// provider was replaced. So the hold is the whole record, the record holds the
// image, and the image is therefore released exactly when the last thing that could
// call into it goes -- by refcount, not by any statement ordering it. That is
// `loom::LoadedLibrary`'s own argument applied one layer out.
//
// A COMPOSITE HOLDS THE RECORD TOO, and it does not need to. A composition is data:
// strings, integers and cells, evaluated by the host's own evaluator with no call
// into the provider at all. It keeps the record alive anyway, because "mounted"
// should mean one thing rather than two, and because a provider whose image had
// already closed while it was still listed as mounted is a state nobody could
// explain. The custody is `Catalog::mount`'s parameter, so the catalog releases it
// AFTER the contributions -- which is the destruction order the whole design turns
// on.
//
// ---- WHAT DOES NOT CROSS ----------------------------------------------------
//
// No `op::Catalog`, no `OperatorDef`, no `std::function`, no STL container, no
// exception and no raw callable. What comes back from `describe` is bytes; what
// goes into `invoke` is bytes; the index in between is provider-local, transient,
// and never an operator's durable meaning.

#include "operator/catalog.hpp"
#include "operator/host.hpp"
#include "operator/image.hpp"
#include "operator/operator.hpp"
#include "operator/provider.hpp"
#include "operator/provider_abi.h"

#include <zen/gate.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace zengine::op {

/// A MOUNTED PROVIDER: who it is, how to reach it, and what keeps it reachable.
///
/// Non-copyable and non-movable because `image` is: the share IS this object's
/// identity, and a record that could move would leave live callables naming where
/// it used to be. It is always held by `shared_ptr`, which is what lets a callable
/// and the catalog share one custody with no rule about which of them goes first.
///
/// MEMBER ORDER IS THE LIFETIME CLAIM, and it is the only place it is stated:
/// `image` is declared last, so it is destroyed last, so `table` -- which points
/// into the image -- can never be read after the mapping is gone.
struct ProviderRecord {
    ProviderRecord(std::string id, const std::string& path)
        : identity(std::move(id)), image(path) {}

    ProviderRecord(const ProviderRecord&) = delete;
    ProviderRecord& operator=(const ProviderRecord&) = delete;
    ProviderRecord(ProviderRecord&&) = delete;
    ProviderRecord& operator=(ProviderRecord&&) = delete;

    std::string identity;
    const ZengineOperatorProviderV1* table = nullptr;
    ImageShare image;
};

/// WHAT A MOUNT DID, or precisely why it did nothing.
///
/// `provider` is the identity the ARTIFACT declared, not one the host chose, which
/// is what an unmount later needs and what a report names. It is filled in on the
/// failures that got far enough to read it, so a refusal can say who was refused.
struct MountResult {
    bool ok = false;
    std::string provider;
    std::string reason;
    std::size_t contributed = 0;

    explicit operator bool() const noexcept { return ok; }
};

namespace detail {

/// THE CALLABLE, and everything it must not be.
///
/// It is not a function pointer into another image, not a resolved definition and
/// not an index standing on its own: it is a closure over the whole provider RECORD
/// plus the transient index, so the code it reaches cannot outlive the image that
/// contains it. Every call serializes, crosses, and re-admits the answer at the
/// schema THIS host decoded -- so a provider that answered with the wrong shape is
/// caught on the way in rather than believed.
inline OperatorDef::Native provider_native(std::shared_ptr<const ProviderRecord> record,
                                           std::uint32_t index,
                                           std::shared_ptr<const loom::Schema> outputs) {
    return [record, index, outputs](const loom::Value& args) -> loom::Cell {
        const std::string packed = loom::serialize(args);
        std::string answer;
        std::string reason;
        ZenByteSink answer_sink = sink_into(answer);
        ZenByteSink reason_sink = sink_into(reason);
        const ZengineOperatorStatus status = record->table->invoke(
            record->table->ctx, index, reinterpret_cast<const std::uint8_t*>(packed.data()),
            packed.size(), answer_sink, reason_sink);
        if (status != ZENGINE_OP_OK) {
            // The provider's own words where it had any, and the number where it
            // did not -- never a second wording of a sentence somebody else wrote.
            throw std::runtime_error(
                reason.empty() ? ("provider '" + record->identity + "' answered status " +
                                  std::to_string(static_cast<int>(status)))
                               : reason);
        }
        const loom::Unverified unverified = loom::parse(answer);
        loom::Admission admitted = loom::admit(unverified, outputs);
        if (!admitted) {
            throw std::runtime_error("provider '" + record->identity +
                                     "' answered something its own declared output schema "
                                     "refuses: " +
                                     admitted.first_error().message());
        }
        const loom::Value& out = admitted.value();
        return *out.at(0);
    };
}

inline MountResult mount_refused(std::string provider, std::string why) {
    MountResult out;
    out.provider = std::move(provider);
    out.reason = std::move(why);
    return out;
}

} // namespace detail

/// MOUNT ONE PROVIDER ARTIFACT INTO A CATALOG.
///
/// Opens the image, reads its declared identity and contributions, turns each into
/// something the host can hold, and hands the batch to `Catalog::mount` -- which
/// judges every one of them before installing any. A refusal at any point leaves the
/// catalog untouched and the image closed, because the record that holds it is a
/// local until the catalog takes custody of it.
///
/// `mode` is the caller's INTENT and the only thing the catalog cannot see for
/// itself: `Ordinary` refuses to cover a power somebody already supplies,
/// `Overlay` says cover it deliberately -- and even then only where the ports are
/// the contract existing compositions were authored against.
inline MountResult mount_provider(Catalog& into, const std::string& artifact_path,
                                  MountMode mode = MountMode::Ordinary) {
    auto record = std::make_shared<ProviderRecord>(std::string(), artifact_path);
    if (!record->image.open()) {
        return detail::mount_refused(
            std::string(), "could not open '" + artifact_path + "' to look for operators");
    }
    void* symbol = record->image.symbol(ZENGINE_OPERATOR_PROVIDER_SYMBOL);
    if (symbol == nullptr) {
        return detail::mount_refused(std::string(), "'" + artifact_path +
                                                        "' exports no operator provider surface");
    }
    using ProviderFn = const ZengineOperatorProviderV1* (*)(void);
    ProviderFn entry = nullptr;
    std::memcpy(&entry, &symbol, sizeof(entry)); // object->function, the -Wpedantic-clean way
    const ZengineOperatorProviderV1* table = entry();
    if (table == nullptr) {
        return detail::mount_refused(std::string(),
                                     "'" + artifact_path + "' exports " +
                                         ZENGINE_OPERATOR_PROVIDER_SYMBOL +
                                         " and answers with no table at all");
    }
    // THE VERSION IS READ BEFORE ANYTHING ELSE ABOUT THE TABLE, because every other
    // field's MEANING is what the version decides. Judging `count` or `identity`
    // first would be reading slots whose shape is exactly what is in doubt.
    if (table->abi_version != ZENGINE_OPERATOR_PROVIDER_ABI_VERSION) {
        return detail::mount_refused(
            std::string(), "'" + artifact_path + "' offers operator provider surface v" +
                               std::to_string(table->abi_version) + "; this host speaks v" +
                               std::to_string(ZENGINE_OPERATOR_PROVIDER_ABI_VERSION));
    }
    if (table->identity == nullptr || table->identity[0] == '\0' || table->describe == nullptr ||
        table->invoke == nullptr) {
        return detail::mount_refused(std::string(), "'" + artifact_path +
                                                        "' declares operator provider surface v" +
                                                        std::to_string(table->abi_version) +
                                                        " and does not fill it in");
    }
    record->identity = table->identity;
    record->table = table;
    if (table->count == 0) {
        // The only way a provider can say "my own authoring failed" across a C
        // seam, and an honest provider of nothing is equally useless. Named rather
        // than accepted as an empty success.
        return detail::mount_refused(record->identity,
                                     "provider '" + record->identity + "' in '" + artifact_path +
                                         "' contributes nothing");
    }

    std::vector<OperatorDef> definitions;
    definitions.reserve(table->count);
    for (std::uint32_t index = 0; index < table->count; ++index) {
        std::string bytes;
        ZenByteSink sink = detail::sink_into(bytes);
        const ZengineOperatorStatus status = table->describe(table->ctx, index, sink);
        if (status != ZENGINE_OP_OK) {
            return detail::mount_refused(record->identity,
                                         "provider '" + record->identity +
                                             "' could not describe contribution " +
                                             std::to_string(index) + " (status " +
                                             std::to_string(static_cast<int>(status)) + ")");
        }
        // Bytes from another image go through the one gate exactly as a message
        // does. A provider that emitted nonsense is refused here rather than
        // believed, and the refusal names which contribution.
        const loom::Unverified unverified = loom::parse(bytes);
        const loom::Admission admitted = loom::admit(unverified, operator_contribution_schema());
        if (!admitted) {
            return detail::mount_refused(record->identity,
                                         "provider '" + record->identity +
                                             "' described contribution " + std::to_string(index) +
                                             " in a shape this host refuses: " +
                                             admitted.first_error().message());
        }
        try {
            DecodedContribution said = decode_contribution(admitted.value());
            if (said.composition) {
                // A COMPOSITION IS STRUCTURE, and it stays structure. Its nodes name
                // logical identities and resolve against this catalog at every
                // spend, so a power replaced underneath propagates through it with
                // no rewrite, no rebinding pass and no notification.
                definitions.emplace_back(said.identity, said.inputs, said.outputs,
                                         std::move(*said.composition));
            } else {
                definitions.emplace_back(
                    said.identity, said.inputs, said.outputs,
                    detail::provider_native(record, index, said.outputs));
            }
        } catch (const std::exception& e) {
            return detail::mount_refused(record->identity,
                                         "provider '" + record->identity +
                                             "' described contribution " + std::to_string(index) +
                                             " unusably: " + e.what());
        }
    }

    const std::size_t offered = definitions.size();
    MountReport installed = into.mount(record->identity, std::move(definitions), mode, record);
    if (!installed) {
        return detail::mount_refused(record->identity, installed.reason);
    }
    MountResult out;
    out.ok = true;
    out.provider = record->identity;
    out.contributed = offered;
    return out;
}

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_PROVIDER_HOST_HPP
