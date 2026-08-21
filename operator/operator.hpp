// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_OPERATOR_HPP
#define ZENGINE_OPERATOR_OPERATOR_HPP

// WHAT AN OPERATOR IS (SEM-0).
//
//     inputs -> computation -> outputs
//
// No participant, no claimant, no causality, no state. It is not a weave, not a
// message and not a Sense: a Sense key has no argument, and a message costs a
// PUMP GENERATION per node, so a three-node arithmetic rule spelled as
// conversation is three sequential turns of the bus.
//
// AN OPERATOR'S SIGNATURE IS A PAIR OF LOOM SCHEMAS -- one whose fields are the
// input ports, one whose fields are the output ports. That single decision is
// why nothing here is a second type system: every port's TypeRef comes from
// `loom::type_ref_for`, the same table every ZEN_SHAPE field goes through; the
// argument pack is admitted by `loom::admit`, the ONE gate, so a missing port is
// a `MissingField` and NO ARITY CHECK IS EVER WRITTEN; and a signature versions
// itself, because `Schema::content_id()` already is that number.
//
// WHAT C++ GIVES AND THE ONE THING IT DOES NOT. Arity, parameter types and the
// return type all derive from an ordinary function-pointer type by partial
// specialisation. Parameter SOURCE NAMES do not exist in C++20 at all -- not in
// `decltype`, not in `__PRETTY_FUNCTION__`, not in `__FUNCSIG__` -- so a port
// name must be authored. So must the identity, and that one is authored on
// purpose rather than for want of a mechanism: deriving it from the symbol would
// make a rename invalidate every composition that named it.
//
//     make_operator<&max_int>("math.max", {"lhs", "rhs"}, "result")
//                      \____/  \________________________________/
//                    C++ owns arity        authored, and only this
//                    and every type
//
// A wrong NUMBER of port names is a compile error, because the parameter is a
// `std::array` sized by `arity_of<F>` and nothing coerces to it.
//
// WHAT IS DELIBERATELY ABSENT. No effects, no state, no cycles, no scheduling,
// no visual graph, no operator error taxonomy, no discovery protocol, and no
// registration of port schemas with a Switchboard: an operator's answer is
// RETURNED, not delivered, so an output schema is not an `Emit<>` and the shape
// never needs to be routable.

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
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace zengine::op {

// ---- what an evaluation answers -------------------------------------------

/// Either a value or a reason, never both -- `loom::Admission`'s own shape, and
/// for its reason: a caller that can hold both eventually reads one while the
/// other was the truth.
///
/// The reason is PROSE from the deepest layer whose vocabulary contains it. A
/// refused argument pack says what `loom::Error::message()` says, word for word,
/// because the gate owns that sentence; a second wording of it would be a second
/// answer. There is no operator error enum, and the four things that can go
/// wrong are named in catalog.hpp where they are detected.
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

// ---- how many times a native body has actually run -------------------------

namespace detail {
inline std::uint64_t& invocation_counter() noexcept {
    static std::uint64_t count = 0;
    return count;
}
} // namespace detail

/// Process-wide count of native operator invocations.
///
/// Exposed so a suite can prove that two consumers spend the SAME definition
/// rather than two implementations that happen to agree -- exactly what
/// `loom::gate_invocations()` is exposed for one layer down, and with the same
/// caveats: it is monotonic and process-wide, so read DELTAS; it decides
/// nothing; and it is observability, not a stability guarantee.
///
/// It counts NATIVE bodies only. A composite is not an arithmetic step, it is a
/// walk over ones that are, and counting the walk too would make the number
/// depend on how a rule happened to be factored.
inline std::uint64_t invocations() noexcept { return detail::invocation_counter(); }

// ---- a composition, as data ------------------------------------------------

/// Where one argument of one node comes from. Three sources and there is no
/// fourth: the composite's own input, an EARLIER node's answer, or a constant.
///
/// ACYCLICITY IS STRUCTURAL, NOT CHECKED. A node binding may only name a node
/// with a smaller index, and `Builder` enforces that by construction -- a
/// reference to node i cannot exist before node i does. There is no cycle
/// detector here because there is nowhere to write a cycle down.
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

/// One step: an operator named by IDENTITY, its arguments, and the two content
/// ids the composition was AUTHORED AGAINST.
///
/// The pair is what turns "the catalog has something by that name" into "the
/// catalog has the thing this rule was written for". A node that recorded no
/// signature would bind silently to a re-shaped operator, and the sentence a
/// reader needs -- *found, but not the signature this was authored against* --
/// could never be said. It costs two integer compares at spend.
struct Node {
    std::string identity;
    std::vector<Binding> arguments;
    loom::ContentId authored_in = 0;
    loom::ContentId authored_out = 0;
};

/// An acyclic value graph, and nothing more.
///
/// A node's ANSWER is its operator's single output port. The day an operator
/// declares two, a binding gains a port name; until then `Builder` refuses to
/// take a multi-output operator's answer as an argument rather than silently
/// meaning the first one.
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

/// A stable identity, a pair of Loom schemas, and a body.
///
/// The body is one of exactly two things and the difference is a PUBLIC
/// question. A native leaf carries a callable; a composite carries a graph over
/// other identities. `is_composite()` exists so a suite can assert that a rule
/// is a COMPOSITION rather than a bespoke native reimplementation wearing an
/// operator's name -- which is the whole difference between proving that
/// registration works and proving that composition does.
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
    /// The graph, or nullptr for a native leaf.
    ///
    /// A POINTER rather than a reference with a precondition, and the difference
    /// was measured: a canary that turned this rule into a native body crashed a
    /// case which had asked `is_composite()` about a DIFFERENT definition three
    /// lines earlier. A precondition a caller can satisfy for the wrong object is
    /// a precondition; a null a caller must look at is a question.
    const Composite* composition() const noexcept { return composite_.get(); }

    /// Run a native leaf and answer with its single output datum.
    ///
    /// Precondition: !is_composite(), and `args` has already been admitted at
    /// this operator's input schema. A composite is NOT run here -- it is a walk
    /// over the catalog it was published into, and the catalog is what has one.
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

/// Give an ordinary C++ function an operator identity.
///
/// The port names and the identity are authored; the arity, every parameter's
/// Loom type and the result's Loom type are the compiler's. A block-scope lambda
/// cannot be passed as `F` (its `_FUN` has no linkage) -- use a namespace-scope
/// function, which is what a primitive should be anyway.
template <auto F>
OperatorDef make_operator(std::string identity, std::array<std::string_view, arity_of<F>> ports,
                          std::string_view result_port) {
    using Sig = detail::signature<F>;
    using Args = typename Sig::args;

    // The schemas are built through `loom::SchemaBuilder`, so a port shape is
    // byte-for-byte what a hand-built one would be, same content id -- and the
    // NAME may be dotted, which `ZEN_SHAPE` cannot spell.
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
