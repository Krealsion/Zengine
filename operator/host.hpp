// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_HOST_HPP
#define ZENGINE_OPERATOR_HOST_HPP

// WHAT A LOADED CONSUMER HOLDS (OPH-0) — the far side of host_abi.h, and the
// whole of what a dynamically loaded tool needs to know about operators.
//
// A weave writes ONE line at namespace scope,
//
//     ZENGINE_OPERATOR_CONSUMER();
//
// which exports the optional symbol a Zengine host looks for, and then takes the
// offer in its constructor:
//
//     class MyWeave ... {
//         op::OperatorHost operators_ = op::OperatorHost::offered();
//     };
//
// From there it asks:
//
//     const op::HostSignature rule = operators_.describe("timer.normalize_delay");
//     const op::HostAnswer    a    = operators_.evaluate(rule, pack);
//
// and gets back real `loom::Schema`s and a real admitted `loom::Value` — never a
// catalog, never a callable, never an index.
//
// EVALUATION TAKES THE CONTRACT AND NOT THE NAME, and the missing convenience is
// the honest part. Turning bytes into a `loom::Value` needs a door, so an
// `evaluate("id", pack)` would have to fetch the description on every call — a
// second crossing per evaluation, hidden inside a spelling that looks free. A
// consumer describes once and spends many times, which is also what a real one
// does: a form is built from the contract, and then the maker types.
//
// HOLDING A `HostSignature` IS NOT HOLDING A RESOLUTION. It is an identity and
// two schemas the consumer built for itself — LOG-R1's DURABLE half exactly —
// and no pointer, index or callable into the host. The host still resolves its
// own definition at every single call, so a rule that changed underneath is
// spent as it is now, not as it was described.
//
// WHAT CROSSES IS NOT OPERATOR TRUTH. It is the ability to ASK the host to spend
// its own current truth, twice: once for the contract and once for the answer.
// Both come from the same `OperatorDef` on the far side, resolved at the moment
// of the call, so a consumer cannot hold a stale reading of a rule that has
// since changed underneath it. There is nothing here to hold.
//
// THE OFFER IS PER-INSTANCE, and that is the reason for its odd shape. The
// exported symbol is necessarily module-scope — a C export has no other scope —
// but the storage behind it is EMPTY except during one load: the host offers,
// the kernel calls `create()`, the instance takes a COPY, and the host
// withdraws. Two instances of one image therefore receive two offers rather than
// sharing one durable module-wide binding, and an image whose instance is
// destroyed leaves nothing behind for the next one to find.
//
// WHY A COPY. The table is plain data and copying it is what lets the host hold
// it in a temporary. What the consumer does depend on is `ctx` and the function
// pointers, which live in the HOST image — so the host must outlive every
// consumer it offered to, and it says so where it makes the offer.
//
// WHAT IS DELIBERATELY ABSENT. No enumeration ("what operators are there?"), no
// caching of a description, no way to publish, no callable, no subscription, and
// no service locator: this object answers about operators and about nothing
// else. A second injected capability, if one is ever earned, gets its own
// object rather than a field here.

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

/// `zengine.OperatorDesc v1` — an operator's whole contract, as bytes.
///
/// IT IS `zen.Manifest`'S SHAPE, deliberately. A manifest carries a `referenced`
/// section holding the post-order closure of every schema its accept-set and
/// state nest, listed before anything that references them, so one forward pass
/// resolves it. An operator's two port schemas nest exactly the same way, so the
/// same section, the same `zen.SchemaDesc v1` entries, the same
/// `collect_referenced` and the same `decode_referenced` do the whole job. There
/// is no second schema language on this seam and there must never be one.
///
/// The field is optional for the manifest's reason: a flat operator — and
/// `timer.normalize_delay`, whose ports are an Int, a Bool and an Int, is flat —
/// emits no closure at all.
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

/// Encode one operator's contract. Takes the two SCHEMAS rather than an
/// `OperatorDef`, which is what lets this one definition live on the consumer's
/// side of the fence and still be the encoder the host uses: the host has the
/// definition, both have the codec, and neither has a second copy of the shape.
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

/// An operator's contract, as a consumer learned it.
///
/// The schemas are the consumer's OWN objects, rebuilt from the descriptor — not
/// pointers into the host. They are ordinary `loom::Schema`s, which is what lets
/// a consumer build an argument pack, admit an answer, and read a port's kind
/// with the same code it would use for any other shape.
///
/// NOTHING HERE NEEDS A REGISTRY TO STAY ALIVE. A decoded schema owns every
/// schema it nests through the `shared_ptr` in its own `TypeRef`s, so the
/// dependency registry `describe` decodes against is a local that dies at the
/// end of the call. Keeping one beside these would be claiming a lifetime that
/// is already structural.
struct HostSignature {
    ZengineOperatorStatus status = ZENGINE_OP_ERR_NO_HOST;
    std::string identity;
    std::shared_ptr<const loom::Schema> inputs;
    std::shared_ptr<const loom::Schema> outputs;

    bool ok() const noexcept { return status == ZENGINE_OP_OK; }
    explicit operator bool() const noexcept { return ok(); }
};

/// What an evaluation across the seam answered.
///
/// VALUE OR REASON, NEVER BOTH — `loom::Admission`'s shape and `op::Evaluation`'s,
/// for their reason: a caller that can hold both eventually reads one while the
/// other was the truth. The status says WHICH KIND of trouble, and the reason
/// carries the host's own prose VERBATIM, because the sentence belongs to
/// whichever layer detected it and a second wording of it would be a second
/// answer.
struct HostAnswer {
    ZengineOperatorStatus status = ZENGINE_OP_ERR_NO_HOST;
    std::optional<loom::Value> value;
    std::string reason;

    bool ok() const noexcept { return status == ZENGINE_OP_OK && value.has_value(); }
    explicit operator bool() const noexcept { return ok(); }
};

namespace detail {

/// Append into a `std::string` the consumer owns. The host copies nothing and
/// frees nothing; the sink is the consumer's storage written by the host during
/// the call, which is `ZenByteSink`'s whole contract.
inline void host_sink_write(void* ctx, const std::uint8_t* data, std::size_t len) {
    static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), len);
}

inline ZenByteSink sink_into(std::string& out) {
    ZenByteSink sink;
    sink.ctx = &out;
    sink.write = &host_sink_write;
    return sink;
}

/// Take an offered table into `slot`, or refuse it.
///
/// THE VERSION IS CHECKED BEFORE ANYTHING IS STORED, on this side as well as on
/// the host's, and the two checks are not redundant: each side is the only one
/// that knows what IT was compiled against. A null `api` is a WITHDRAWAL and
/// always succeeds — the host must be able to take its offer back whatever
/// happened during the load.
///
/// A free function rather than a method so a suite can drive it with a table it
/// built by hand, which is the only way to reach the consumer-side refusal
/// without shipping an artifact from another era.
inline ZengineOperatorStatus accept_offer_into(ZengineOperatorHostApiV1& slot,
                                               const ZengineOperatorHostApiV1* api) noexcept {
    if (api == nullptr) {
        slot = ZengineOperatorHostApiV1{};
        return ZENGINE_OP_OK;
    }
    if (api->abi_version != ZENGINE_OPERATOR_ABI_VERSION) {
        // Refused, not guessed: a table whose shape we cannot vouch for is never
        // called, and the slot is left exactly as it was.
        return ZENGINE_OP_ERR_ABI;
    }
    slot = *api;
    return ZENGINE_OP_OK;
}

/// THIS IMAGE'S OFFER SLOT — defined by `ZENGINE_OPERATOR_CONSUMER()` and
/// nowhere else.
///
/// DECLARED HERE AND DEFINED BY THE MACRO, which is the whole of why the storage
/// is honest. A `static` inside an INLINE function would be vague-linkage: on
/// ELF the host executable's copy interposes into an RTLD_LOCAL library, and on
/// PE it does not, so the same code would mean different things on the two
/// platforms this project ships. A static inside the macro's NON-inline
/// definition is a local symbol in exactly one image, on both.
///
/// A consumer that uses `OperatorHost` and forgets the macro gets a link error
/// naming this function, which is the failure to want: loud, at build time, and
/// impossible to mistake for "the host did not offer".
ZengineOperatorHostApiV1& offered_host_slot() noexcept;

} // namespace detail

// ---- the consumer's handle -------------------------------------------------

/// The host's operator power, as a loaded consumer holds it.
///
/// Copyable plain data: it is a table and a context, and holding two of them is
/// holding one thing twice. It owns nothing, frees nothing, and resolves nothing
/// — every question goes back across the seam to the host's current catalog at
/// the moment it is asked.
class OperatorHost {
public:
    /// Unbound: `describe` and `evaluate` answer ZENGINE_OP_ERR_NO_HOST. This is
    /// the ordinary state of an ordinary weave and is not an error.
    OperatorHost() = default;

    /// TAKE THE OFFER MADE TO THE INSTANCE BEING CREATED. Call it in the weave's
    /// constructor; the value is a copy, so it keeps working after the host has
    /// withdrawn the offer for the next load.
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

        // The descriptor is bytes from another image, so it goes through the one
        // gate exactly as a message does. A host that emitted nonsense is caught
        // here rather than believed.
        const loom::Unverified u = loom::parse(bytes);
        const loom::Admission admitted = loom::admit(u, operator_desc_schema());
        if (!admitted) {
            sig.status = ZENGINE_OP_ERR_MALFORMED;
            return sig;
        }
        const loom::Value& desc = admitted.value();

        loom::Registry vocabulary;
        try {
            // The closure first, front to back — the encoder's post-order
            // guarantee is what makes one pass enough — then the two ports,
            // whose nested references now resolve.
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

    /// Spend the host's current definition of `contract.identity` over `args`.
    ///
    /// `args` must claim the operator's input schema — the one `describe`
    /// answered with. Anything else is the GATE's refusal on the far side, in
    /// the gate's own words.
    HostAnswer evaluate(const HostSignature& contract, const loom::Value& args) const {
        HostAnswer out;
        if (!bound() || api_.evaluate == nullptr) {
            out.status = ZENGINE_OP_ERR_NO_HOST;
            return out;
        }
        if (!contract.ok() || !contract.outputs) {
            // A contract this consumer never obtained cannot admit an answer,
            // and inventing a door here would be admitting bytes against a shape
            // nobody described.
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

        // The answer is bytes from another image and is admitted here too. The
        // door is the OUTPUT schema this consumer decoded for itself, so an
        // answer that does not match the contract this consumer was told is
        // caught on the way in rather than believed.
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

/// DECLARE THIS IMAGE OPERATOR-AWARE — one line at namespace scope, and the whole
/// of what a consumer library writes.
///
/// It emits two things and no more: this image's offer slot (see
/// `detail::offered_host_slot` for why it must be a non-inline definition), and
/// the one exported symbol a Zengine host looks up. Writing it costs a consumer
/// nothing at load time in a host that knows nothing about operators — an
/// unresolved offer leaves the slot empty and every ask answers
/// ZENGINE_OP_ERR_NO_HOST.
///
/// The trailing semicolon at the call site is swallowed by the `extern "C"`
/// block, so `ZENGINE_OPERATOR_CONSUMER();` reads like a statement and compiles
/// as a declaration under -Wpedantic.
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
