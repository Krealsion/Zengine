// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_OPERATOR_HPP
#define ZENGINE_OPERATOR_OPERATOR_HPP

// What an operator is: inputs -> computation -> outputs, with no participant, claimant,
// causality or state. Not a weave, a message or a Sense; its answer is returned, not delivered.
// Its signature is a pair of Loom schemas, input ports and output ports: every port's TypeRef
// comes from `loom::type_ref_for`, the pack is admitted by `loom::admit` (a missing port is a
// `MissingField`; no arity check is written), and `Schema::content_id()` versions a signature.
// Reference: docs/reference/operator-providers.md.

// C++ derives arity and every type from a function pointer; C++20 has no parameter source
// names, so port names are authored, and the identity is authored on purpose (derived from the
// symbol, a rename would invalidate every composition naming it). A wrong number of port names
// does not compile: the parameter is a `std::array` sized by `arity_of<F>`.
//
//     make_operator<&max_int>("math.max", {"lhs", "rhs"}, "result")

#include <zen/gate.hpp>
#include <zen/schema.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace zengine::op {

// ---- what an evaluation answers -------------------------------------------

/// Either a value or a reason, never both (`loom::Admission`'s shape): a caller that can hold
/// both eventually reads one while the other was the truth. The reason is prose from the deepest
/// layer whose vocabulary contains it, word for word; there is no operator error enum.
class Evaluation {
public:
    static Evaluation accept(loom::Value v) {
        Evaluation e;
        e.value_ = std::move(v);
        return e;
    }
    static Evaluation refuse(std::string why) {
        Evaluation e;
        e.reason_ = std::move(why);
        return e;
    }

    bool ok() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return ok(); }

    /// Precondition: ok(). Throws std::bad_optional_access otherwise.
    const loom::Value& value() const& { return value_.value(); }
    const std::string& reason() const noexcept { return reason_; }

private:
    Evaluation() = default;
    std::optional<loom::Value> value_;
    std::string reason_;
};

/// A native composition may propagate the refusal of an operator it spent.
/// Ordinary native exceptions are still diagnosed by Catalog as implementation failures.
class Refusal : public std::runtime_error {
public:
    explicit Refusal(std::string reason) : std::runtime_error(reason), reason_(std::move(reason)) {}
    const std::string& reason() const noexcept { return reason_; }
private:
    std::string reason_;
};

// ---- how many times a native body has actually run -------------------------

namespace detail {
inline std::uint64_t& invocation_counter() noexcept {
    static std::uint64_t count = 0;
    return count;
}
} // namespace detail

/// Process-wide count of native operator invocations, so a suite can prove two consumers spend
/// the same definition rather than two that agree. `loom::gate_invocations()`'s sibling, with
/// its caveats: monotonic and process-wide (read deltas), decides nothing, not a stability
/// guarantee. It counts native bodies only, so refactoring a rule does not move it.
inline std::uint64_t invocations() noexcept { return detail::invocation_counter(); }

// ---- a composition, as data ------------------------------------------------

/// Where one argument of one node comes from: the composite's own input, an earlier node's
/// answer, or a constant. Acyclicity is structural, not checked: `Builder` cannot make a
/// reference to node i before node i exists, so there is nowhere to write a cycle down.
class Binding {
public:
    enum class From { Input, Node, Constant };

    static Binding input(std::string name) {
        Binding b;
        b.from_ = From::Input;
        b.input_ = std::move(name);
        return b;
    }
    static Binding node(std::size_t index) {
        Binding b;
        b.from_ = From::Node;
        b.node_ = index;
        return b;
    }
    static Binding constant(loom::Cell cell) {
        Binding b;
        b.from_ = From::Constant;
        b.constant_ = std::move(cell);
        return b;
    }

    From from() const noexcept { return from_; }
    const std::string& input_name() const noexcept { return input_; }
    std::size_t node_index() const noexcept { return node_; }
    /// Precondition: from() == From::Constant.
    const loom::Cell& constant_cell() const { return constant_.value(); }

private:
    Binding() = default;
    From from_ = From::Input;
    std::string input_;
    std::size_t node_ = 0;
    std::optional<loom::Cell> constant_;
};

/// One step: an operator named by identity, its arguments, and the two content ids the
/// composition was authored against -- "the thing this rule was written for" rather than
/// "something by that name", for two integer compares per spend.
struct Node {
    std::string identity;
    std::vector<Binding> arguments;
    loom::ContentId authored_in = 0;
    loom::ContentId authored_out = 0;
};

/// An acyclic value graph, and nothing more. A node's answer is its operator's single output
/// port; `Builder` refuses a multi-output operator's answer as an argument rather than silently
/// meaning the first.
struct Composite {
    std::vector<Node> nodes;
    std::size_t result_node = 0; ///< whose answer IS the composite's answer
};

// ---- deriving a signature from an ordinary C++ function --------------------

namespace detail {

template <class F>
struct signature_of;
template <class R, class... A>
struct signature_of<R (*)(A...)> {
    using result = R;
    using args = std::tuple<A...>;
    static constexpr std::size_t arity = sizeof...(A);
};

/// `constexpr auto f = &fn;` has type `R (*const)(A...)` -- the cv must come off
/// before the pointer type can be decomposed.
template <auto F>
using signature = signature_of<std::remove_cv_t<decltype(F)>>;

template <class T>
T cell_as(const loom::Cell& c) {
    T out{};
    loom::from_cell(out, c);
    return out;
}

template <class Args, std::size_t... I>
std::vector<loom::Field> input_fields(const std::array<std::string_view, sizeof...(I)>& names,
                                      std::index_sequence<I...>) {
    return std::vector<loom::Field>{
        loom::Field{std::string(names[I]),
                    loom::type_ref_for<std::decay_t<std::tuple_element_t<I, Args>>>::get(),
                    /*required=*/true}...};
}

template <auto F, class Args, std::size_t N, std::size_t... I>
loom::Cell call_with(const loom::Value& in, const std::array<std::string_view, N>& names,
                     std::index_sequence<I...>) {
    // Every argument is read by NAME off the admitted pack, so the C++ parameter
    // order and the port order are one fact stated once.
    return loom::to_cell(
        F(cell_as<std::decay_t<std::tuple_element_t<I, Args>>>(*in.get(names[I]))...));
}

} // namespace detail

/// How many arguments this function takes -- the number a caller must supply
/// port names for, and the reason supplying the wrong count is a compile error.
template <auto F>
inline constexpr std::size_t arity_of = detail::signature<F>::arity;

// ---- an operator ------------------------------------------------------------

/// A stable identity, a pair of Loom schemas, and a body: a native leaf carries a callable, a
/// composite a graph over other identities. `is_composite()` is public so a suite can assert a
/// rule is a composition, not a native reimplementation wearing an operator's name.
class OperatorDef {
public:
    using Native = std::function<loom::Cell(const loom::Value&)>;

    OperatorDef(std::string identity, std::shared_ptr<const loom::Schema> in,
                std::shared_ptr<const loom::Schema> out, Native body)
        : identity_(std::move(identity)), in_(std::move(in)), out_(std::move(out)),
          native_(std::move(body)) {}

    OperatorDef(std::string identity, std::shared_ptr<const loom::Schema> in,
                std::shared_ptr<const loom::Schema> out, Composite body)
        : identity_(std::move(identity)), in_(std::move(in)), out_(std::move(out)),
          composite_(std::make_shared<const Composite>(std::move(body))) {}

    const std::string& identity() const noexcept { return identity_; }
    const std::shared_ptr<const loom::Schema>& inputs() const noexcept { return in_; }
    const std::shared_ptr<const loom::Schema>& outputs() const noexcept { return out_; }

    bool is_composite() const noexcept { return composite_ != nullptr; }
    /// The graph, or nullptr for a native leaf. A pointer, not a reference with a precondition:
    /// a caller can satisfy a precondition for the wrong object, but must look at a null.
    const Composite* composition() const noexcept { return composite_.get(); }

    /// Run a native leaf and answer with its single output datum. Precondition: !is_composite(),
    /// and `args` already admitted at the input schema. A composite is a walk over the catalog
    /// it was published into, and only the catalog runs it.
    loom::Cell invoke_native(const loom::Value& args) const {
        ++detail::invocation_counter();
        return native_(args);
    }

private:
    std::string identity_;
    std::shared_ptr<const loom::Schema> in_;
    std::shared_ptr<const loom::Schema> out_;
    Native native_;
    std::shared_ptr<const Composite> composite_;
};

/// Give an ordinary C++ function an operator identity: the port names and the identity are
/// authored, arity and every Loom type are the compiler's. `F` cannot be a block-scope lambda
/// (its `_FUN` has no linkage); use a namespace-scope function.
template <auto F>
OperatorDef make_operator(std::string identity, std::array<std::string_view, arity_of<F>> ports,
                          std::string_view result_port) {
    using Sig = detail::signature<F>;
    using Args = typename Sig::args;

    // Built through `loom::SchemaBuilder`, so a port shape is byte-for-byte a hand-built one,
    // same content id, and the name may be dotted, which `ZEN_SHAPE` cannot spell.
    auto in = loom::make_schema(identity + ".in", 1,
                                detail::input_fields<Args>(
                                    ports, std::make_index_sequence<Sig::arity>{}));
    auto out = loom::make_schema(
        identity + ".out", 1,
        std::vector<loom::Field>{
            loom::Field{std::string(result_port),
                        loom::type_ref_for<std::decay_t<typename Sig::result>>::get(),
                        /*required=*/true}});

    OperatorDef::Native body = [ports](const loom::Value& args) {
        return detail::call_with<F, Args>(args, ports, std::make_index_sequence<Sig::arity>{});
    };
    return OperatorDef(std::move(identity), std::move(in), std::move(out), std::move(body));
}

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_OPERATOR_HPP
