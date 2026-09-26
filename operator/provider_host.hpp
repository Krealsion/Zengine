// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_PROVIDER_HOST_HPP
#define ZENGINE_OPERATOR_PROVIDER_HOST_HPP

// Mounting a provider: the host's side of provider_abi.h, and all a host writes to acquire
// semantic power it does not author -- an empty `op::Catalog`, one `op::mount_provider` per
// artifact, then an `op::OperatorHostSurface` over it. The host knows which artifacts to mount
// and how to host operators, not what any of them means. Only bytes cross: no catalog,
// `OperatorDef`, `std::function`, container, exception or raw callable, and the index between
// `describe` and `invoke` is provider-local and transient.
// Reference: docs/reference/operator-providers.md.

// Custody: a `ProviderRecord` holds the table and one share of the image, and is held by every
// native contribution's callable and by the catalog until unmount, so the image is released
// when the last thing that could call into it goes -- by refcount, not statement order. A hold
// on the image alone would leave the definition's record dangling once a provider is replaced.
// A composite holds the record too, though it never calls in, so "mounted" means one thing; the
// catalog releases the custody after the contributions.

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

/// A mounted provider: who it is, how to reach it, and what keeps it reachable. Non-copyable and
/// non-movable because `image` is: a record that moved would leave live callables naming where
/// it was. Always held by `shared_ptr`, so a callable and the catalog share one custody with no
/// rule about which goes first. Member order is the lifetime claim: `image` is declared last, so
/// `table`, which points into it, is never read after the mapping is gone.
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

/// What a mount did, or why it did nothing. `provider` is the identity the artifact declared,
/// which an unmount later needs; it is filled in on failures that got far enough to read it.
struct MountResult {
    bool ok = false;
    std::string provider;
    std::string reason;
    std::size_t contributed = 0;

    explicit operator bool() const noexcept { return ok; }
};

namespace detail {

/// The callable: a closure over the whole provider record plus the transient index, never a
/// function pointer into another image, so the code it reaches cannot outlive its image. Every
/// call serializes, crosses, and re-admits the answer at the schema this host decoded, so a
/// wrong shape is caught rather than believed.
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
            // The provider's own words where it had any, the number where it did not.
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

/// Mount one provider artifact into a catalog: open the image, read its declared identity and
/// contributions, turn each into something the host can hold, and hand the batch to
/// `Catalog::mount`, which judges all before installing any. A refusal leaves the catalog
/// untouched and the image closed, since the record is a local until the catalog takes custody.
/// `mode` is the caller's intent: `Ordinary` refuses to cover a supplied power, and `Overlay`
/// covers it where the ports are the contract existing compositions were authored against.
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
    // The version is read before anything else about the table: it decides what every other
    // field means.
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
        // The only way a provider can say its own authoring failed across a C seam; a provider
        // of nothing is named rather than accepted as an empty success.
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
        // Bytes from another image go through the one gate like a message; the refusal names
        // which contribution.
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
                // A composition stays structure: its nodes resolve against this catalog at
                // every spend, so a power replaced underneath propagates with no rewrite,
                // rebinding or notification.
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
