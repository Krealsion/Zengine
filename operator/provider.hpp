// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_PROVIDER_HPP
#define ZENGINE_OPERATOR_PROVIDER_HPP

// What a provider contributes, as bytes, and the provider's side of the seam: one line at
// namespace scope, `ZENGINE_OPERATOR_PROVIDER("my.provider", my_powers)`. A native contribution
// crosses as its contract and is reached by index while the image is held; a composite crosses
// as its graph, its nodes still naming `math.max`, so the host resolves them against whatever
// provides them at every spend, and a power replaced underneath propagates through it.
// Reference: docs/reference/operator-providers.md.

// The encoding is `zen.Manifest`'s shape: a `referenced` section carries the post-order closure
// of nested schemas as `zen.SchemaDesc v1` entries (no second schema language), and the graph
// shapes are `op::Composite`, `op::Node` and `op::Binding` as Loom values (no second graph
// type). It carries what `op::Builder` can author, an acyclic value graph with Int and Bool
// constants; a third constant kind is refused by name.

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

/// Where one argument of one node comes from: `op::Binding` as a Loom value. `from` is the
/// enumerator's value and the optional fields are the three sources' payloads -- optional, since
/// a Loom schema has no variant and absence says "not this source".
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

/// One step: `op::Node` as a Loom value. `authored_in` and `authored_out` are the
/// `loom::ContentId`s the composition was written against (64 bits, so Int fields): they tell
/// "something by that name" from "the thing this rule was written for" when a power is reshaped.
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

/// One contribution, whole. An absent `composition` means native: the implementation is in the
/// provider's image, reached by index. Its presence is the whole fork, a question the host asks
/// of the bytes rather than a flag set beside them.
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
            // Refused by name: `Builder` mints Int and Bool constants only, so a third kind
            // means the authoring surface grew one and this codec did not.
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

/// Encode one contribution, derived from the `OperatorDef` the provider would spend and nothing
/// else: a hand-written descriptor could disagree with what it describes.
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

/// A contribution as the host learned it: an identity, two schemas the host built for itself
/// from the descriptor, and a graph or nothing. Nothing here points into the provider: a decoded
/// schema owns what it nests, and a graph is strings, integers and cells. A native
/// contribution's code is the one thing that needs the provider alive, and it is not here.
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
        // Acyclicity is re-established on arrival: `Builder` makes it structural when
        // authoring, but these bytes came from another image, and a forward reference would make
        // the evaluator's single forward pass read an answer not yet computed.
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

/// Turn one contribution's bytes back into something a host can hold. Throws on anything
/// malformed; callers across the seam turn that into a refusal.
inline DecodedContribution decode_contribution(const loom::Value& desc) {
    DecodedContribution out;
    loom::Registry vocabulary;
    // The closure first, front to back (the encoder's post-order makes one pass enough), then
    // the two ports, whose nested references now resolve.
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

/// A provider image's authored definitions, and the two answers it owes a host. It holds
/// `OperatorDef`s by value and no catalog, host or resolution state: a provider authors, and
/// which authoring is in force is the host's to decide.
class ProviderDefinitions {
public:
    explicit ProviderDefinitions(std::vector<OperatorDef> defs) : defs_(std::move(defs)) {}

    std::uint32_t count() const noexcept { return static_cast<std::uint32_t>(defs_.size()); }

    /// No exception crosses this seam; a throw becomes `ZENGINE_OP_ERR_PROVIDER_FAILED`.
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

    /// Spend a native contribution. A composite is refused rather than run: the host holds that
    /// graph, and running a private copy of it would be a second answer.
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
            // The gate on this side too: the host admitted the pack at the schema it decoded
            // from this descriptor, but bytes from another image go through the one gate anyway.
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

/// This image's definitions: declared here and defined by the macro. A `static` inside an inline
/// function is vague-linkage -- on ELF the host executable's copy interposes into an RTLD_LOCAL
/// library, on PE it does not -- while the macro's non-inline definition is local to exactly one
/// image on both. A provider that forgets the macro gets a link error naming this function.
/// Null means the image's own authoring failed; the host reads a count of zero and refuses it.
const ProviderDefinitions* provider_definitions() noexcept;

} // namespace detail

} // namespace zengine::op

/// Declare this image an operator provider: one line at namespace scope. `IDENTITY` is a string
/// literal, the provider's logical name a host mounts, unmounts and reports as active. `AUTHOR`
/// is callable with no arguments, answering `std::vector<zengine::op::OperatorDef>`; it runs
/// once, on the first call across this seam, and a throw leaves this image providing nothing.
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
