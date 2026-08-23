// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_PROVIDER_HPP
#define ZENGINE_OPERATOR_PROVIDER_HPP

// WHAT A PROVIDER CONTRIBUTES, AS BYTES (PROV-0) — and the provider's side of the
// seam, which is one line at namespace scope.
//
//     std::vector<op::OperatorDef> my_powers() { ... }
//     ZENGINE_OPERATOR_PROVIDER("my.provider", my_powers)
//
// TWO KINDS OF CONTRIBUTION, AND THE DIFFERENCE IS THE PHASE.
//
//     NATIVE      the implementation is code in this image. What crosses is the
//                 contract; the code is reached by index while the image is held.
//     COMPOSITE   the implementation IS a graph over other identities. What
//                 crosses is the GRAPH — nodes still saying `math.max` — so the
//                 host resolves those names against whatever currently provides
//                 them, every time it spends the rule.
//
// If a composite crossed as an opaque callback into its own image, the provider
// would be evaluating its own private graph and a power replaced underneath could
// never propagate through it. Transitive replacement is not a feature bolted onto
// this seam; it is what carrying the structure across it MEANS.
//
// IT IS `zen.Manifest`'S SHAPE, deliberately, exactly as `zengine.OperatorDesc` is.
// A manifest carries a `referenced` section holding the post-order closure of every
// schema it nests, listed before anything that references them, so one forward pass
// resolves it; an operator's two port schemas nest the same way, so the same
// section, the same `zen.SchemaDesc v1` entries, the same `collect_referenced` and
// the same `decode_referenced` do the whole job. There is NO SECOND SCHEMA LANGUAGE
// on this seam and there must never be one.
//
// AND NO SECOND GRAPH TYPE. The three shapes below are `op::Composite`, `op::Node`
// and `op::Binding` written as Loom values, field for field. They exist because
// those three C++ types cannot cross a C ABI, not because the ABI wanted a
// different graph: decoding one answers with `op::Composite` itself, which is what
// the one evaluator already walks.
//
// WHAT THE GRAPH CANNOT CARRY, and it is bounded by what the AUTHORING SURFACE can
// produce rather than by imagination. `op::Builder` mints exactly two kinds of
// constant, an Int and a Bool, so those are the two this codec carries and a third
// is REFUSED by name rather than silently dropped. No cycles (a binding may only
// name an earlier node, and that is structural in `Builder`), no state, no effects,
// no presentation and no coordinates: an acyclic value graph and nothing else.

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/provider_abi.h"

#include <zen/gate.hpp>
#include <zen/kernel/schema_codec.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::op {

// ---- the four shapes a contribution travels as ------------------------------

/// Where one argument of one node comes from — `op::Binding`, as a Loom value.
///
/// `from` is the enumerator's own value, and the three optional fields are the
/// three sources' payloads. Optional rather than a variant because a Loom schema
/// has no variant, and because ABSENCE is exactly the right word: an Input binding
/// has no node index, and saying so by omitting the field is how every other
/// optional in this system says it.
inline std::shared_ptr<const loom::Schema> composition_binding_schema() {
    static const auto s = loom::SchemaBuilder("zengine.OperatorBinding", 1)
                              .field("from", loom::Kind::Int)
                              .field("input", loom::Kind::Text, /*required=*/false)
                              .field("node", loom::Kind::Int, /*required=*/false)
                              .field("int_constant", loom::Kind::Int, /*required=*/false)
                              .field("bool_constant", loom::Kind::Bool, /*required=*/false)
                              .build();
    return s;
}

/// One step — `op::Node`, as a Loom value.
///
/// `authored_in` and `authored_out` are the two `loom::ContentId`s the composition
/// was written against, carried in Int fields because a ContentId is 64 bits and an
/// Int field is where 64 bits live. They travel because they are the difference
/// between "the catalog has something by that name" and "the catalog has the thing
/// this rule was written for" — the sentence a reader needs when a provider
/// reshapes a power, and a sentence a node that recorded no signature could never
/// say.
inline std::shared_ptr<const loom::Schema> composition_node_schema() {
    static const auto s =
        loom::SchemaBuilder("zengine.OperatorNode", 1)
            .field("identity", loom::Kind::Text)
            .field("authored_in", loom::Kind::Int)
            .field("authored_out", loom::Kind::Int)
            .list("arguments", loom::type_message(composition_binding_schema()),
                  /*required=*/false)
            .build();
    return s;
}

/// An acyclic value graph — `op::Composite`, as a Loom value.
inline std::shared_ptr<const loom::Schema> composition_schema() {
    static const auto s = loom::SchemaBuilder("zengine.OperatorComposition", 1)
                              .list("nodes", loom::type_message(composition_node_schema()))
                              .field("result", loom::Kind::Int)
                              .build();
    return s;
}

/// ONE CONTRIBUTION, whole.
///
/// `composition` ABSENT means native — the implementation is in the provider's
/// image and is reached by index. Its presence is the entire fork, and it is a
/// question the host asks of the bytes rather than a flag the provider sets beside
/// them.
inline std::shared_ptr<const loom::Schema> operator_contribution_schema() {
    static const auto s =
        loom::SchemaBuilder("zengine.OperatorContribution", 1)
            .field("identity", loom::Kind::Text)
            .list("referenced", loom::type_message(loom::schema_desc_schema()),
                  /*required=*/false)
            .message("inputs", loom::schema_desc_schema())
            .message("outputs", loom::schema_desc_schema())
            .message("composition", composition_schema(), /*required=*/false)
            .build();
    return s;
}

// ---- encoding ---------------------------------------------------------------

namespace detail {

inline loom::Value encode_binding(const Binding& b) {
    loom::Value v(composition_binding_schema());
    v.set("from", loom::Cell::integer(static_cast<std::int64_t>(b.from())));
    switch (b.from()) {
    case Binding::From::Input:
        v.set("input", loom::Cell::text(b.input_name()));
        break;
    case Binding::From::Node:
        v.set("node", loom::Cell::integer(static_cast<std::int64_t>(b.node_index())));
        break;
    case Binding::From::Constant: {
        const loom::Cell& c = b.constant_cell();
        if (c.kind() == loom::Kind::Int) {
            v.set("int_constant", loom::Cell::integer(c.as_int()));
        } else if (c.kind() == loom::Kind::Bool) {
            v.set("bool_constant", loom::Cell::boolean(c.as_bool()));
        } else {
            // REFUSED BY NAME. `Builder` mints Int and Bool constants and nothing
            // else, so a third kind here means the authoring surface grew one and
            // this codec did not — which must stop the encode rather than quietly
            // ship a graph with a hole in it.
            throw std::invalid_argument(
                std::string("a provider contribution cannot carry a ") +
                loom::name_of(c.kind()) + " constant; this seam carries Int and Bool");
        }
        break;
    }
    }
    return v;
}

inline loom::Value encode_node(const Node& n) {
    loom::Value v(composition_node_schema());
    v.set("identity", loom::Cell::text(n.identity));
    v.set("authored_in", loom::Cell::integer(static_cast<std::int64_t>(n.authored_in)));
    v.set("authored_out", loom::Cell::integer(static_cast<std::int64_t>(n.authored_out)));
    if (!n.arguments.empty()) {
        std::vector<loom::Cell> args;
        args.reserve(n.arguments.size());
        for (const Binding& b : n.arguments) {
            args.push_back(loom::Cell::message(encode_binding(b)));
        }
        v.set("arguments", loom::Cell::list(std::move(args)));
    }
    return v;
}

inline loom::Value encode_composition(const Composite& c) {
    loom::Value v(composition_schema());
    std::vector<loom::Cell> nodes;
    nodes.reserve(c.nodes.size());
    for (const Node& n : c.nodes) {
        nodes.push_back(loom::Cell::message(encode_node(n)));
    }
    v.set("nodes", loom::Cell::list(std::move(nodes)));
    v.set("result", loom::Cell::integer(static_cast<std::int64_t>(c.result_node)));
    return v;
}

} // namespace detail

/// Encode one contribution. Derived from the `OperatorDef` the provider would
/// spend, and from nothing else: there is no hand-written descriptor beside a
/// definition here and there must never be one, because a description that could
/// disagree with the thing it describes is exactly the second copy this whole
/// substrate exists to remove.
inline loom::Value encode_contribution(const OperatorDef& def) {
    loom::Value desc(operator_contribution_schema());
    desc.set("identity", loom::Cell::text(def.identity()));

    std::vector<std::shared_ptr<const loom::Schema>> referenced;
    loom::collect_referenced(*def.inputs(), referenced);
    loom::collect_referenced(*def.outputs(), referenced);
    if (!referenced.empty()) {
        std::vector<loom::Cell> refs;
        refs.reserve(referenced.size());
        for (const auto& s : referenced) {
            refs.push_back(loom::Cell::message(loom::encode_schema(*s)));
        }
        desc.set("referenced", loom::Cell::list(std::move(refs)));
    }
    desc.set("inputs", loom::Cell::message(loom::encode_schema(*def.inputs())));
    desc.set("outputs", loom::Cell::message(loom::encode_schema(*def.outputs())));
    if (def.is_composite()) {
        desc.set("composition", loom::Cell::message(detail::encode_composition(*def.composition())));
    }
    return desc;
}

// ---- decoding ---------------------------------------------------------------

/// A contribution as the host learned it: an identity, two schemas the host built
/// for ITSELF out of the descriptor, and either a graph or nothing.
///
/// NOTHING HERE POINTS INTO THE PROVIDER. A decoded schema owns every schema it
/// nests through the `shared_ptr`s in its own `TypeRef`s, and a decoded graph is
/// strings, integers and cells. What still needs the provider alive is a NATIVE
/// contribution's code, and that is the one thing this struct does not carry.
struct DecodedContribution {
    std::string identity;
    std::shared_ptr<const loom::Schema> inputs;
    std::shared_ptr<const loom::Schema> outputs;
    std::optional<Composite> composition;
};

namespace detail {

inline Binding decode_binding(const loom::Value& v) {
    const auto from = static_cast<Binding::From>(v.get("from")->as_int());
    switch (from) {
    case Binding::From::Input: {
        const loom::Cell* name = v.get("input");
        if (name == nullptr) {
            throw std::invalid_argument("an input binding names no input");
        }
        return Binding::input(name->as_text());
    }
    case Binding::From::Node: {
        const loom::Cell* index = v.get("node");
        if (index == nullptr) {
            throw std::invalid_argument("a node binding names no node");
        }
        return Binding::node(static_cast<std::size_t>(index->as_int()));
    }
    case Binding::From::Constant: {
        if (const loom::Cell* i = v.get("int_constant"); i != nullptr) {
            return Binding::constant(loom::Cell::integer(i->as_int()));
        }
        if (const loom::Cell* b = v.get("bool_constant"); b != nullptr) {
            return Binding::constant(loom::Cell::boolean(b->as_bool()));
        }
        throw std::invalid_argument("a constant binding carries no constant");
    }
    }
    throw std::invalid_argument("a binding names a source this host does not know");
}

inline Composite decode_composition(const loom::Value& v) {
    Composite graph;
    const loom::Cell* nodes = v.get("nodes");
    if (nodes == nullptr) {
        throw std::invalid_argument("a composition carries no nodes");
    }
    for (const loom::Cell& nc : nodes->as_list()) {
        const loom::Value& nv = *nc.as_message();
        Node node;
        node.identity = nv.get("identity")->as_text();
        node.authored_in = static_cast<loom::ContentId>(nv.get("authored_in")->as_int());
        node.authored_out = static_cast<loom::ContentId>(nv.get("authored_out")->as_int());
        if (const loom::Cell* args = nv.get("arguments"); args != nullptr) {
            for (const loom::Cell& ac : args->as_list()) {
                node.arguments.push_back(decode_binding(*ac.as_message()));
            }
        }
        // ACYCLICITY IS RE-ESTABLISHED ON ARRIVAL, not assumed. `Builder` makes it
        // structural on the authoring side -- a reference to node i cannot exist
        // before node i does -- but these bytes came from another image and a
        // forward reference here would make the one evaluator's single forward
        // pass read an answer that has not been computed.
        for (const Binding& b : node.arguments) {
            if (b.from() == Binding::From::Node && b.node_index() >= graph.nodes.size()) {
                throw std::invalid_argument("step " + std::to_string(graph.nodes.size()) +
                                            " of '" + node.identity +
                                            "' names a step that does not precede it");
            }
        }
        graph.nodes.push_back(std::move(node));
    }
    graph.result_node = static_cast<std::size_t>(v.get("result")->as_int());
    if (graph.result_node >= graph.nodes.size()) {
        throw std::invalid_argument("a composition names no result step");
    }
    return graph;
}

} // namespace detail

/// Turn one contribution's bytes back into something a host can hold. Throws
/// `std::invalid_argument` (or whatever the codec throws) on anything malformed;
/// callers across the seam catch and turn that into a refusal, because a provider
/// that emitted nonsense must be refused rather than believed.
inline DecodedContribution decode_contribution(const loom::Value& desc) {
    DecodedContribution out;
    loom::Registry vocabulary;
    // The closure first, front to back -- the encoder's post-order guarantee is
    // what makes one pass enough -- then the two ports, whose nested references
    // now resolve.
    loom::decode_referenced(desc, vocabulary);
    out.inputs = loom::decode_schema(*desc.get("inputs")->as_message(), vocabulary);
    out.outputs = loom::decode_schema(*desc.get("outputs")->as_message(), vocabulary);
    out.identity = desc.get("identity")->as_text();
    if (const loom::Cell* graph = desc.get("composition"); graph != nullptr) {
        out.composition = detail::decode_composition(*graph->as_message());
    }
    return out;
}

// ---- the provider's side ----------------------------------------------------

/// A PROVIDER IMAGE'S AUTHORED DEFINITIONS, and the two answers it owes a host.
///
/// It holds `OperatorDef`s by value and nothing else: no catalog, no host, no
/// resolution state. That absence is the claim. A provider AUTHORS; deciding which
/// authoring is currently in force is somebody else's job, and an object here that
/// could answer that question would be a second resolution nobody could see.
class ProviderDefinitions {
public:
    explicit ProviderDefinitions(std::vector<OperatorDef> defs) : defs_(std::move(defs)) {}

    std::uint32_t count() const noexcept { return static_cast<std::uint32_t>(defs_.size()); }

    /// NO EXCEPTION CROSSES THIS SEAM, pointing the same way the kernel's adapter
    /// contains a library's throw, and pointing the other way from
    /// `OperatorHostSurface`'s thunks. That is why ZENGINE_OP_ERR_PROVIDER_FAILED
    /// exists at all.
    ZengineOperatorStatus describe(std::uint32_t index, ZenByteSink sink) const noexcept {
        if (index >= count()) {
            return ZENGINE_OP_ERR_NOT_FOUND;
        }
        try {
            const std::string bytes = loom::serialize(encode_contribution(defs_[index]));
            if (sink.write != nullptr) {
                sink.write(sink.ctx, reinterpret_cast<const std::uint8_t*>(bytes.data()),
                           bytes.size());
            }
            return ZENGINE_OP_OK;
        } catch (...) {
            return ZENGINE_OP_ERR_PROVIDER_FAILED;
        }
    }

    /// SPEND A NATIVE CONTRIBUTION. A composite is refused here rather than run,
    /// because the host is holding that graph and running a private copy of it
    /// would be the second answer this whole seam is arranged to prevent.
    ZengineOperatorStatus invoke(std::uint32_t index, const std::uint8_t* args,
                                 std::size_t args_len, ZenByteSink answer,
                                 ZenByteSink reason) const noexcept {
        if (index >= count()) {
            return ZENGINE_OP_ERR_NOT_FOUND;
        }
        try {
            const OperatorDef& def = defs_[index];
            if (def.is_composite()) {
                write(reason, "'" + def.identity() +
                                  "' is a composition; its provider does not evaluate it");
                return ZENGINE_OP_ERR_REFUSED;
            }
            const loom::Unverified unverified =
                loom::parse(std::string_view(reinterpret_cast<const char*>(args), args_len));
            if (!unverified.well_formed()) {
                return ZENGINE_OP_ERR_MALFORMED;
            }
            // THE GATE, ON THIS SIDE TOO. The host admitted the pack at the schema
            // it decoded from this very descriptor, so in a healthy arrangement
            // nothing here ever refuses -- and bytes from another image go through
            // the one gate anyway, because "it cannot happen" is not a reason to
            // believe them.
            loom::Admission admitted = loom::admit(unverified, def.inputs());
            if (!admitted) {
                write(reason, "'" + def.identity() + "' refused its arguments: " +
                                  admitted.first_error().message());
                return ZENGINE_OP_ERR_REFUSED;
            }
            loom::Value out(def.outputs());
            out.set(def.outputs()->fields()[0].name, def.invoke_native(admitted.value()));
            const std::string bytes = loom::serialize(out);
            write(answer, bytes);
            return ZENGINE_OP_OK;
        } catch (const std::exception& e) {
            write(reason, e.what());
            return ZENGINE_OP_ERR_PROVIDER_FAILED;
        } catch (...) {
            return ZENGINE_OP_ERR_PROVIDER_FAILED;
        }
    }

private:
    static void write(ZenByteSink sink, const std::string& bytes) noexcept {
        if (sink.write != nullptr) {
            sink.write(sink.ctx, reinterpret_cast<const std::uint8_t*>(bytes.data()),
                       bytes.size());
        }
    }

    std::vector<OperatorDef> defs_;
};

namespace detail {

/// THIS IMAGE'S DEFINITIONS — declared here and DEFINED BY THE MACRO, which is the
/// whole of why the storage is honest.
///
/// A `static` inside an INLINE function would be vague-linkage: on ELF the host
/// executable's copy interposes into an RTLD_LOCAL library and on PE it does not,
/// so the same code would mean different things on the two platforms this project
/// ships. A static inside the macro's NON-inline definition is a local symbol in
/// exactly one image, on both. It is `offered_host_slot`'s lesson, unchanged, and a
/// provider that forgets the macro gets a link error naming this function.
///
/// It answers nullptr if the image's own authoring failed. That is the only way a
/// provider can say "I could not build my own definitions" across a C seam, and the
/// host reads it as a count of zero and refuses the mount.
const ProviderDefinitions* provider_definitions() noexcept;

} // namespace detail

} // namespace zengine::op

/// DECLARE THIS IMAGE AN OPERATOR PROVIDER — one line at namespace scope, and the
/// whole of what a providing library writes.
///
/// `IDENTITY` is a string literal: the provider's logical name, which is what a
/// host mounts, unmounts and reports as active. `AUTHOR` is anything callable with
/// no arguments answering `std::vector<zengine::op::OperatorDef>`; it runs ONCE, on
/// the first call across this seam, and a throw out of it leaves this image
/// providing nothing rather than travelling across C.
#define ZENGINE_OPERATOR_PROVIDER(IDENTITY, AUTHOR)                                          \
    namespace zengine::op::detail {                                                          \
    const ProviderDefinitions* provider_definitions() noexcept {                              \
        static const std::optional<ProviderDefinitions> built = [] {                          \
            std::optional<ProviderDefinitions> out;                                           \
            try {                                                                             \
                out.emplace(AUTHOR());                                                        \
            } catch (...) {                                                                   \
            }                                                                                 \
            return out;                                                                       \
        }();                                                                                  \
        return built ? &*built : nullptr;                                                     \
    }                                                                                         \
    }                                                                                         \
    extern "C" {                                                                              \
    static ZengineOperatorStatus zengine_operator_provider_describe_(void*, uint32_t index,   \
                                                                     ZenByteSink sink) {      \
        const ::zengine::op::ProviderDefinitions* d =                                         \
            ::zengine::op::detail::provider_definitions();                                    \
        return d == nullptr ? ZENGINE_OP_ERR_PROVIDER_FAILED : d->describe(index, sink);      \
    }                                                                                         \
    static ZengineOperatorStatus zengine_operator_provider_invoke_(                           \
        void*, uint32_t index, const uint8_t* args, size_t args_len, ZenByteSink answer,      \
        ZenByteSink reason) {                                                                 \
        const ::zengine::op::ProviderDefinitions* d =                                         \
            ::zengine::op::detail::provider_definitions();                                    \
        return d == nullptr ? ZENGINE_OP_ERR_PROVIDER_FAILED                                  \
                            : d->invoke(index, args, args_len, answer, reason);               \
    }                                                                                         \
    ZEN_KERNEL_EXPORT const ZengineOperatorProviderV1* zengine_operator_provider(void) {      \
        static const ZengineOperatorProviderV1 table = [] {                                   \
            ZengineOperatorProviderV1 t{};                                                    \
            t.abi_version = ZENGINE_OPERATOR_PROVIDER_ABI_VERSION;                            \
            t.ctx = nullptr;                                                                  \
            t.identity = IDENTITY;                                                            \
            const ::zengine::op::ProviderDefinitions* d =                                     \
                ::zengine::op::detail::provider_definitions();                                \
            t.count = d == nullptr ? 0u : d->count();                                         \
            t.describe = &zengine_operator_provider_describe_;                                \
            t.invoke = &zengine_operator_provider_invoke_;                                    \
            return t;                                                                         \
        }();                                                                                  \
        return &table;                                                                        \
    }                                                                                         \
    }

#endif // ZENGINE_OPERATOR_PROVIDER_HPP
