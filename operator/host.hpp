// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_HOST_HPP
#define ZENGINE_OPERATOR_HOST_HPP

// What a loaded consumer holds: the far side of host_abi.h, and all a dynamically loaded tool
// needs to know about operators. It writes `ZENGINE_OPERATOR_CONSUMER();` at namespace scope,
// takes the offer in its constructor (`op::OperatorHost::offered()`), describes a rule once and
// evaluates it as often as it likes, getting real `loom::Schema`s and admitted `loom::Value`s --
// never a catalog, a callable or an index.
// Reference: docs/reference/operator-host.md.

// Evaluation takes the contract, not the name: bytes become a `loom::Value` only at a door, so
// `evaluate("id", pack)` would hide a second crossing per call. A `HostSignature` is an identity
// and two schemas the consumer built for itself, not a resolution: the host resolves its own
// definition at every call, so a rule that changed underneath is spent as it is now.

// The offer is per instance. The exported symbol is module-scope, but the storage behind it is
// empty except during one load: the host offers, the kernel calls `create()`, the instance
// takes a copy, and the host withdraws -- so two instances of one image receive two offers. The
// copy's `ctx` and function pointers live in the host image, so the host outlives every
// consumer it offered to. Deliberately absent: enumeration, a cached description, publishing,
// a callable, a subscription, a service locator.

#include "operator/host_abi.h"

#include <zen/kernel/schema_codec.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::op {

// ---- the descriptor both sides agree on ------------------------------------

/// `zengine.OperatorDesc v1`: an operator's whole contract, as bytes, in `zen.Manifest`'s shape
/// (a `referenced` post-order closure of `zen.SchemaDesc v1` entries), so there is no second
/// schema language on this seam. The closure is optional, as in a manifest: a flat operator
/// emits none.
inline std::shared_ptr<const loom::Schema> operator_desc_schema() {
    static const auto s =
        loom::SchemaBuilder("zengine.OperatorDesc", 1)
            .field("identity", loom::Kind::Text)
            .list("referenced", loom::type_message(loom::schema_desc_schema()),
                  /*required=*/false)
            .message("inputs", loom::schema_desc_schema())
            .message("outputs", loom::schema_desc_schema())
            .build();
    return s;
}

/// Encode one operator's contract from its two schemas rather than an `OperatorDef`, so this one
/// encoder lives on the consumer's side of the fence and is still the one the host uses.
inline loom::Value encode_operator_desc(std::string_view identity,
                                        const loom::Schema& inputs,
                                        const loom::Schema& outputs) {
    loom::Value desc(operator_desc_schema());
    desc.set("identity", loom::Cell::text(std::string(identity)));

    std::vector<std::shared_ptr<const loom::Schema>> referenced;
    loom::collect_referenced(inputs, referenced);
    loom::collect_referenced(outputs, referenced);
    if (!referenced.empty()) {
        std::vector<loom::Cell> refs;
        refs.reserve(referenced.size());
        for (const auto& s : referenced) {
            refs.push_back(loom::Cell::message(loom::encode_schema(*s)));
        }
        desc.set("referenced", loom::Cell::list(std::move(refs)));
    }
    desc.set("inputs", loom::Cell::message(loom::encode_schema(inputs)));
    desc.set("outputs", loom::Cell::message(loom::encode_schema(outputs)));
    return desc;
}

/// An operator's contract, as a consumer learned it: the schemas are its own objects, rebuilt
/// from the descriptor rather than pointers into the host, so it builds packs and admits answers
/// with ordinary code. A decoded schema owns what it nests, so the registry `describe` decodes
/// against dies at the end of the call.
struct HostSignature {
    ZengineOperatorStatus status = ZENGINE_OP_ERR_NO_HOST;
    std::string identity;
    std::shared_ptr<const loom::Schema> inputs;
    std::shared_ptr<const loom::Schema> outputs;

    bool ok() const noexcept { return status == ZENGINE_OP_OK; }
    explicit operator bool() const noexcept { return ok(); }
};

/// What an evaluation across the seam answered: a value or a reason, never both. The status says
/// which kind of trouble; the reason carries the host's prose verbatim, since the sentence
/// belongs to whichever layer detected it.
struct HostAnswer {
    ZengineOperatorStatus status = ZENGINE_OP_ERR_NO_HOST;
    std::optional<loom::Value> value;
    std::string reason;

    bool ok() const noexcept { return status == ZENGINE_OP_OK && value.has_value(); }
    explicit operator bool() const noexcept { return ok(); }
};

namespace detail {

/// Append into a `std::string` the consumer owns: the host writes it during the call and copies
/// or frees nothing (`ZenByteSink`'s contract).
inline void host_sink_write(void* ctx, const std::uint8_t* data, std::size_t len) {
    static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), len);
}

inline ZenByteSink sink_into(std::string& out) {
    ZenByteSink sink;
    sink.ctx = &out;
    sink.write = &host_sink_write;
    return sink;
}

/// Take an offered table into `slot`, or refuse it. The version is checked before anything is
/// stored, on this side as on the host's: each side alone knows what it was compiled against. A
/// null `api` is a withdrawal and always succeeds, so the host can take its offer back whatever
/// happened. A free function, so a suite can reach the consumer-side refusal with a hand-built
/// table.
inline ZengineOperatorStatus accept_offer_into(ZengineOperatorHostApiV1& slot,
                                               const ZengineOperatorHostApiV1* api) noexcept {
    if (api == nullptr) {
        slot = ZengineOperatorHostApiV1{};
        return ZENGINE_OP_OK;
    }
    if (api->abi_version != ZENGINE_OPERATOR_ABI_VERSION) {
        // Refused, not guessed: a table we cannot vouch for is never called, and the slot is
        // left as it was.
        return ZENGINE_OP_ERR_ABI;
    }
    slot = *api;
    return ZENGINE_OP_OK;
}

/// This image's offer slot: declared here and defined by `ZENGINE_OPERATOR_CONSUMER()`. A
/// `static` inside an inline function is vague-linkage -- on ELF the host executable's copy
/// interposes into an RTLD_LOCAL library, on PE it does not -- while the macro's non-inline
/// definition is local to exactly one image on both. A consumer that forgets the macro gets a
/// link error naming this function: loud, at build time, never "the host did not offer".
ZengineOperatorHostApiV1& offered_host_slot() noexcept;

} // namespace detail

// ---- the consumer's handle -------------------------------------------------

/// The host's operator power, as a loaded consumer holds it: copyable plain data, a table and a
/// context. It owns, frees and resolves nothing; every question crosses to the host's current
/// catalog when it is asked.
class OperatorHost {
public:
    /// Unbound: `describe` and `evaluate` answer ZENGINE_OP_ERR_NO_HOST. This is
    /// the ordinary state of an ordinary weave and is not an error.
    OperatorHost() = default;

    /// Take the offer made to the instance being created. Call it in the weave's constructor;
    /// the value is a copy, so it keeps working after the host withdraws the offer.
    static OperatorHost offered() noexcept { return OperatorHost(detail::offered_host_slot()); }

    /// Build one over a table directly — the native-host and suite spelling. A
    /// null or wrong-version table yields an unbound host rather than a throw.
    static OperatorHost over(const ZengineOperatorHostApiV1* api) noexcept {
        ZengineOperatorHostApiV1 slot{};
        if (detail::accept_offer_into(slot, api) != ZENGINE_OP_OK) {
            return OperatorHost();
        }
        return OperatorHost(slot);
    }

    /// Was anything offered to this instance at all?
    bool bound() const noexcept { return api_.abi_version == ZENGINE_OPERATOR_ABI_VERSION; }
    explicit operator bool() const noexcept { return bound(); }

    /// What the host publishes under `identity`, derived from its own definition.
    HostSignature describe(std::string_view identity) const {
        HostSignature sig;
        if (!bound() || api_.describe == nullptr) {
            sig.status = ZENGINE_OP_ERR_NO_HOST;
            return sig;
        }
        const std::string id(identity);
        std::string bytes;
        ZenByteSink sink = detail::sink_into(bytes);
        sig.status = api_.describe(api_.ctx, id.c_str(), sink);
        if (sig.status != ZENGINE_OP_OK) {
            return sig;
        }

        // The descriptor is bytes from another image, so it goes through the one gate like a
        // message: a host that emitted nonsense is caught, not believed.
        const loom::Unverified u = loom::parse(bytes);
        const loom::Admission admitted = loom::admit(u, operator_desc_schema());
        if (!admitted) {
            sig.status = ZENGINE_OP_ERR_MALFORMED;
            return sig;
        }
        const loom::Value& desc = admitted.value();

        loom::Registry vocabulary;
        try {
            // The closure first, front to back (the encoder's post-order makes one pass
            // enough), then the two ports, whose nested references now resolve.
            loom::decode_referenced(desc, vocabulary);
            sig.inputs = loom::decode_schema(*desc.get("inputs")->as_message(), vocabulary);
            sig.outputs = loom::decode_schema(*desc.get("outputs")->as_message(), vocabulary);
        } catch (const std::exception&) {
            sig.status = ZENGINE_OP_ERR_MALFORMED;
            sig.inputs.reset();
            sig.outputs.reset();
            return sig;
        }
        sig.identity = desc.get("identity")->as_text();
        sig.status = ZENGINE_OP_OK;
        return sig;
    }

    /// Spend the host's current definition of `contract.identity` over `args`, which must claim
    /// the input schema `describe` answered with; anything else is the gate's refusal on the far
    /// side, in its own words.
    HostAnswer evaluate(const HostSignature& contract, const loom::Value& args) const {
        HostAnswer out;
        if (!bound() || api_.evaluate == nullptr) {
            out.status = ZENGINE_OP_ERR_NO_HOST;
            return out;
        }
        if (!contract.ok() || !contract.outputs) {
            // A contract this consumer never obtained cannot admit an answer; inventing a door
            // would admit bytes against a shape nobody described.
            out.status = ZENGINE_OP_ERR_NOT_FOUND;
            return out;
        }
        const std::string& id = contract.identity;
        std::string packed;
        try {
            packed = loom::serialize(args);
        } catch (const std::exception& e) {
            // A pack this side could not even write down never reaches the seam.
            out.status = ZENGINE_OP_ERR_MALFORMED;
            out.reason = e.what();
            return out;
        }

        std::string answer;
        std::string reason;
        ZenByteSink answer_sink = detail::sink_into(answer);
        ZenByteSink reason_sink = detail::sink_into(reason);
        out.status = api_.evaluate(api_.ctx, id.c_str(),
                                   reinterpret_cast<const std::uint8_t*>(packed.data()),
                                   packed.size(), answer_sink, reason_sink);
        if (out.status != ZENGINE_OP_OK) {
            out.reason = std::move(reason);
            return out;
        }

        // The answer is bytes from another image, admitted here at the output schema this
        // consumer decoded for itself: one that does not match its contract is caught.
        const loom::Unverified u = loom::parse(answer);
        loom::Admission admitted = loom::admit(u, contract.outputs);
        if (!admitted) {
            out.status = ZENGINE_OP_ERR_MALFORMED;
            out.reason = admitted.first_error().message();
            return out;
        }
        out.value = std::move(admitted).value();
        return out;
    }

private:
    explicit OperatorHost(const ZengineOperatorHostApiV1& api) noexcept : api_(api) {}

    ZengineOperatorHostApiV1 api_{};
};

} // namespace zengine::op

/// Declare this image operator-aware: one line at namespace scope. It emits this image's offer
/// slot (see `detail::offered_host_slot`) and the one exported symbol a Zengine host looks up;
/// in a host that knows nothing of operators it costs nothing, and every ask answers
/// `ZENGINE_OP_ERR_NO_HOST`. The `extern "C"` block swallows the call site's semicolon, so
/// `ZENGINE_OPERATOR_CONSUMER();` compiles as a declaration under -Wpedantic.
#define ZENGINE_OPERATOR_CONSUMER()                                                          \
    namespace zengine::op::detail {                                                          \
    ZengineOperatorHostApiV1& offered_host_slot() noexcept {                                 \
        static ZengineOperatorHostApiV1 slot{};                                              \
        return slot;                                                                         \
    }                                                                                        \
    }                                                                                        \
    extern "C" {                                                                             \
    static ZengineOperatorStatus zengine_operator_offer_(const ZengineOperatorHostApiV1* a) { \
        return ::zengine::op::detail::accept_offer_into(                                     \
            ::zengine::op::detail::offered_host_slot(), a);                                  \
    }                                                                                        \
    ZEN_KERNEL_EXPORT const ZengineOperatorConsumerV1* zengine_operator_consumer(void) {      \
        static const ZengineOperatorConsumerV1 table = {                                     \
            .abi_version = ZENGINE_OPERATOR_ABI_VERSION, .offer = zengine_operator_offer_};  \
        return &table;                                                                       \
    }                                                                                        \
    }

#endif // ZENGINE_OPERATOR_HOST_HPP
