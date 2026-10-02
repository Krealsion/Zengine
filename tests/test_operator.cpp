// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Operator suite -- what a named semantic truth IS, and what it takes for two consumers to
// spend the same one: a signature derived from ordinary C++, not restated beside it; discovery
// and invocation reading one record; `timer.normalize_delay` as a graph over published
// primitives with NO native semantics of its own; an independent translation unit evaluating it
// by name; and the proof those two are not agreeing implementations. The Timer's own execution
// of the rule is pinned in test_timer.cpp, with the weave it is about.

#include "doctest.h"

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"
#include "operator_fixture.hpp"
#include "operator_stranger.hpp"
#include "timer/normalize.hpp"

#include <zen/gate.hpp>
#include <zen/kind.hpp>
#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace op = zengine::op;
namespace tmr = zengine::timer;

namespace {

std::int64_t int_answer(const op::Evaluation& e) { return e.value().at(0)->as_int(); }

std::int64_t same_int(std::int64_t x) { return x; }

// Fold bodies, each `(acc, count, ...) -> acc` in a different way.
std::int64_t plus_one(std::int64_t acc, std::int64_t) { return acc + 1; }
std::int64_t last_count(std::int64_t, std::int64_t count) { return count; }
std::int64_t scaled(std::int64_t acc, std::int64_t count, std::int64_t n) { return acc + count * n; }
std::int64_t capped(std::int64_t acc, std::int64_t count, std::int64_t cap) {
    if (acc + count > cap) {
        throw op::Refusal("'t.capped' refuses a total above " + std::to_string(cap));
    }
    return acc + count;
}
std::int64_t doubled(std::int64_t lhs, std::int64_t rhs) { return lhs + 2 * rhs; }
bool never(std::int64_t, std::int64_t) { return false; }

/// The four ports a fold of its own takes from the composite: start, limit, step, initial.
std::vector<loom::Field> fold_inputs() {
    const loom::TypeRef i = loom::type_of(loom::Kind::Int);
    return {loom::Field{"start", i, true}, loom::Field{"limit", i, true},
            loom::Field{"step", i, true}, loom::Field{"initial", i, true}};
}

/// `identity(start, limit, step, initial) = fold body(count -> count_port, acc -> acc_port)`.
op::OperatorDef folding(const op::Catalog& catalog, const std::string& identity,
                        const std::string& body, const std::string& count_port,
                        const std::string& acc_port) {
    op::Builder b(catalog, identity, fold_inputs());
    const op::Builder::Ref answer =
        b.fold(body, count_port, acc_port,
               {b.input("start"), b.input("limit"), b.input("step"), b.input("initial")});
    return std::move(b).result("result", answer);
}

op::Evaluation run_fold(const op::Catalog& catalog, const std::string& identity,
                        std::int64_t start, std::int64_t limit, std::int64_t step,
                        std::int64_t initial) {
    loom::Value ask(catalog.find(identity)->inputs());
    ask.set("start", loom::Cell::integer(start));
    ask.set("limit", loom::Cell::integer(limit));
    ask.set("step", loom::Cell::integer(step));
    ask.set("initial", loom::Cell::integer(initial));
    return catalog.evaluate(identity, std::move(ask));
}

/// One port, `x : Int`, the shape every operator below takes.
std::vector<loom::Field> one_int() {
    return {loom::Field{"x", loom::type_of(loom::Kind::Int), true}};
}

/// A composite of `width` steps, each spending `step` on its own input, answering the last.
op::OperatorDef wide(const op::Catalog& catalog, const std::string& identity,
                     const std::string& step, int width) {
    op::Builder b(catalog, identity, one_int());
    op::Builder::Ref last = b.input("x");
    for (int i = 0; i < width; ++i) {
        last = b.call(step, {b.input("x")});
    }
    return std::move(b).result("result", last);
}

} // namespace

// ---- 1. the signature is the compiler's ------------------------------------

TEST_CASE("a native operator's arity and every type come from the C++ signature") {
    const op::OperatorDef max = op::make_operator<&op::max_int>(op::kMaxInt, {"lhs", "rhs"},
                                                                "result");

    CHECK(max.identity() == "math.max");
    CHECK(max.inputs()->name() == "math.max.in");
    CHECK(max.inputs()->version() == 1);
    REQUIRE(max.inputs()->fields().size() == 2);
    CHECK(max.inputs()->fields()[0].name == "lhs");
    CHECK(max.inputs()->fields()[0].type.kind == loom::Kind::Int);
    CHECK(max.inputs()->fields()[1].name == "rhs");
    CHECK(max.inputs()->fields()[1].type.kind == loom::Kind::Int);

    CHECK(max.outputs()->name() == "math.max.out");
    REQUIRE(max.outputs()->fields().size() == 1);
    CHECK(max.outputs()->fields()[0].name == "result");
    CHECK(max.outputs()->fields()[0].type.kind == loom::Kind::Int);

    // The one thing NOT derivable: nothing anywhere restates that lhs is an Int.
    // A mixed signature proves the derivation is per parameter rather than a
    // guess made once.
    const op::OperatorDef select = op::make_operator<&op::select_int>(
        op::kSelectInt, {"condition", "when_true", "when_false"}, "result");
    REQUIRE(select.inputs()->fields().size() == 3);
    CHECK(select.inputs()->fields()[0].type.kind == loom::Kind::Bool);
    CHECK(select.inputs()->fields()[1].type.kind == loom::Kind::Int);
    CHECK(select.inputs()->fields()[2].type.kind == loom::Kind::Int);
    CHECK(select.outputs()->fields()[0].type.kind == loom::Kind::Int);

    // Arity is the compiler's too, and it is a COMPILE error to disagree with
    // it: the port array's size is `arity_of<F>`. Stated as a value here because
    // the refusal itself cannot be a runtime case.
    CHECK(op::arity_of<&op::max_int> == 2);
    CHECK(op::arity_of<&op::select_int> == 3);
}

TEST_CASE("a derived port schema IS the hand-built one — there is no second type system") {
    const op::OperatorDef max = op::make_operator<&op::max_int>(op::kMaxInt, {"lhs", "rhs"},
                                                                "result");
    const std::shared_ptr<const loom::Schema> by_hand =
        loom::SchemaBuilder("math.max.in", 1)
            .field("lhs", loom::Kind::Int)
            .field("rhs", loom::Kind::Int)
            .build();

    // Same identity AND same normalized structure. `content_id` is what the gate
    // itself compares, so this is the same equality the substrate already trusts
    // — and it is why a signature needs no version number of its own.
    CHECK(loom::same_identity(*max.inputs(), *by_hand));
    CHECK(max.inputs()->content_id() == by_hand->content_id());
}

TEST_CASE("a missing or mistyped argument is the GATE's refusal, in the gate's own words") {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    const op::OperatorDef* max = catalog.find(op::kMaxInt);
    REQUIRE(max != nullptr);

    // NO ARITY CHECK IS EVER WRITTEN. A port that was not supplied is a
    // MissingField, which is the one gate answering about a shape it declares.
    loom::Value half(max->inputs());
    half.set("lhs", loom::Cell::integer(3));
    const op::Evaluation missing = catalog.evaluate(op::kMaxInt, std::move(half));
    CHECK_FALSE(missing.ok());
    CHECK(missing.reason().find("rhs") != std::string::npos);

    loom::Value wrong(max->inputs());
    wrong.set("lhs", loom::Cell::integer(3));
    wrong.set("rhs", loom::Cell::text("seven"));
    const op::Evaluation mistyped = catalog.evaluate(op::kMaxInt, std::move(wrong));
    CHECK_FALSE(mistyped.ok());
    CHECK(mistyped.reason().find("expected Int") != std::string::npos);

    // And the gate really ran: the count it keeps for exactly this question moved.
    const std::uint64_t before = loom::gate_invocations();
    loom::Value good(max->inputs());
    good.set("lhs", loom::Cell::integer(3));
    good.set("rhs", loom::Cell::integer(9));
    const op::Evaluation ok = catalog.evaluate(op::kMaxInt, std::move(good));
    REQUIRE(ok.ok());
    CHECK(int_answer(ok) == 9);
    CHECK(loom::gate_invocations() > before);
}

// ---- 2. one store, read twice ----------------------------------------------

TEST_CASE("discovery and invocation come from ONE record") {
    const op::Catalog catalog = tmr::fallback_vocabulary();

    const std::vector<std::string> names = catalog.identities();
    CHECK(names.size() == catalog.size());
    CHECK(names == std::vector<std::string>{op::kLessInt, op::kSelectBool,
        op::kSelectInt, op::kAddInt, op::kMaxInt, tmr::kNormalizeDelay});

    // The claim is not "the list is right"; it is that a name a consumer can
    // DISCOVER is a name it can SPEND, because there is no second list to fall
    // out of step with the first.
    for (const std::string& name : names) {
        const op::OperatorDef* def = catalog.find(name);
        REQUIRE_MESSAGE(def != nullptr, "discoverable but not invocable: ", name);
        CHECK(def->identity() == name);
        CHECK(def->outputs()->fields().size() == 1);
    }
}

TEST_CASE("a duplicate identity is refused, and an unresolved one is NAMED") {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    CHECK_THROWS_AS(op::publish_primitives(catalog), std::invalid_argument);

    const op::OperatorDef* nothing = catalog.find("math.min");
    CHECK(nothing == nullptr);

    loom::Value empty(catalog.find(op::kMaxInt)->inputs());
    const op::Evaluation e = catalog.evaluate("math.min", std::move(empty));
    CHECK_FALSE(e.ok());
    CHECK(e.reason() == "unresolved operator reference 'math.min'");
}

// ---- 3. the rule is a COMPOSITION ------------------------------------------

TEST_CASE("timer.normalize_delay is three nodes over published primitives, and no native body") {
    const op::Catalog catalog = tmr::fallback_vocabulary();
    const op::OperatorDef* rule = catalog.find(tmr::kNormalizeDelay);
    REQUIRE(rule != nullptr);

    // THE LOAD-BEARING ASSERTION: a native `normalize_delay` registered under this identity
    // would satisfy every other case in this file and prove only that registration works.
    REQUIRE(rule->is_composite());

    const op::Composite& graph = *rule->composition();
    REQUIRE(graph.nodes.size() == 3);
    CHECK(graph.nodes[0].identity == op::kMaxInt);
    CHECK(graph.nodes[1].identity == op::kMaxInt);
    CHECK(graph.nodes[2].identity == op::kSelectInt);
    CHECK(graph.result_node == 2);

    // Every step names something the catalog publishes, at the signature it was
    // authored against — the two ContentIds a saved reference would carry.
    for (const op::Node& node : graph.nodes) {
        const op::OperatorDef* step = catalog.find(node.identity);
        REQUIRE(step != nullptr);
        CHECK(step->inputs()->content_id() == node.authored_in);
        CHECK(step->outputs()->content_id() == node.authored_out);
        CHECK_FALSE(step->is_composite());
    }

    // Acyclicity is STRUCTURAL: a node binding may only name an earlier node.
    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        for (const op::Binding& b : graph.nodes[i].arguments) {
            if (b.from() == op::Binding::From::Node) {
                CHECK(b.node_index() < i);
            }
        }
    }

    // The signature, derived where it could be: the two inputs are authored (a
    // composite has no C++ signature), the OUTPUT type is not — it is whatever
    // `logic.select_int` answers with.
    REQUIRE(rule->inputs()->fields().size() == 2);
    CHECK(rule->inputs()->fields()[0].name == "delay_ms");
    CHECK(rule->inputs()->fields()[0].type.kind == loom::Kind::Int);
    CHECK(rule->inputs()->fields()[1].name == "repeat");
    CHECK(rule->inputs()->fields()[1].type.kind == loom::Kind::Bool);
    REQUIRE(rule->outputs()->fields().size() == 1);
    CHECK(rule->outputs()->fields()[0].name == "effective_delay");
    CHECK(rule->outputs()->fields()[0].type.kind == loom::Kind::Int);
}

TEST_CASE("the composite computes the whole matrix, and computes it with the primitives") {
    const op::Catalog catalog = tmr::fallback_vocabulary();
    constexpr std::int64_t kBig = 86'400'000;
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();

    CHECK(tmr::effective_delay(catalog, -500, false) == 0);
    CHECK(tmr::effective_delay(catalog, -500, true) == 1);
    CHECK(tmr::effective_delay(catalog, -1, false) == 0);
    CHECK(tmr::effective_delay(catalog, -1, true) == 1);
    CHECK(tmr::effective_delay(catalog, 0, false) == 0);
    CHECK(tmr::effective_delay(catalog, 0, true) == 1);
    CHECK(tmr::effective_delay(catalog, 1, false) == 1);
    CHECK(tmr::effective_delay(catalog, 1, true) == 1);
    CHECK(tmr::effective_delay(catalog, 2, false) == 2);
    CHECK(tmr::effective_delay(catalog, 2, true) == 2);
    CHECK(tmr::effective_delay(catalog, kBig, false) == kBig);
    CHECK(tmr::effective_delay(catalog, kBig, true) == kBig);
    CHECK(tmr::effective_delay(catalog, kMax, false) == kMax);
    CHECK(tmr::effective_delay(catalog, kMax, true) == kMax);

    // EXACTLY THREE PRIMITIVE INVOCATIONS PER EVALUATION — two maxes and a
    // select, and nothing else. A composite that had quietly acquired a native
    // shortcut would move this number, and so would a fourth node nobody meant
    // to add. The counter is process-wide and monotonic, so the claim is a
    // delta, exactly as `loom::gate_invocations()` is read.
    const std::uint64_t before = op::invocations();
    CHECK(tmr::effective_delay(catalog, -500, true) == 1);
    CHECK(op::invocations() - before == 3);
}

TEST_CASE("an authoring mistake is refused where it is written, not where it is spent") {
    op::Catalog catalog;
    op::publish_primitives(catalog);

    // An operator nobody published.
    CHECK_THROWS_AS(
        [&] {
            op::Builder b(catalog, "probe.unknown_step",
                          {loom::Field{"n", loom::type_of(loom::Kind::Int), true}});
            b.call("math.min", {b.input("n"), b.constant(std::int64_t{0})});
        }(),
        std::invalid_argument);

    // The wrong NUMBER of arguments. (The wrong number of PORT NAMES, one layer
    // up, cannot be a case at all — it does not compile.)
    CHECK_THROWS_AS(
        [&] {
            op::Builder b(catalog, "probe.wrong_arity",
                          {loom::Field{"n", loom::type_of(loom::Kind::Int), true}});
            b.call(op::kMaxInt, {b.input("n")});
        }(),
        std::invalid_argument);

    // The wrong TYPE, caught against the port the step declares.
    CHECK_THROWS_AS(
        [&] {
            op::Builder b(catalog, "probe.wrong_type",
                          {loom::Field{"flag", loom::type_of(loom::Kind::Bool), true}});
            b.call(op::kMaxInt, {b.input("flag"), b.constant(std::int64_t{0})});
        }(),
        std::invalid_argument);

    // An input that is not declared.
    CHECK_THROWS_AS(
        [&] {
            op::Builder b(catalog, "probe.no_such_input",
                          {loom::Field{"n", loom::type_of(loom::Kind::Int), true}});
            b.input("m");
        }(),
        std::invalid_argument);

    // A "composite" that computes nothing is not an operator.
    CHECK_THROWS_AS(
        [&] {
            op::Builder b(catalog, "probe.identity",
                          {loom::Field{"n", loom::type_of(loom::Kind::Int), true}});
            const op::Builder::Ref passthrough = b.input("n");
            return std::move(b).result("out", passthrough);
        }(),
        std::invalid_argument);
}

TEST_CASE("a step resolved at a DIFFERENT signature is named, never silently spent") {
    // Two catalogs whose `math.max` differ only in a PORT NAME — which is a
    // schema change, so the content ids differ and the composition authored
    // against one cannot be run against the other.
    op::Catalog authored;
    op::publish_primitives(authored);
    const op::OperatorDef rule = tmr::normalize_delay(authored);

    op::Catalog renamed;
    renamed.publish(op::make_operator<&op::max_int>(op::kMaxInt, {"left", "right"}, "result"));
    renamed.publish(op::make_operator<&op::select_int>(
        op::kSelectInt, {"condition", "when_true", "when_false"}, "result"));
    renamed.publish(rule);

    loom::Value ask(rule.inputs());
    ask.set("delay_ms", loom::Cell::integer(-500));
    ask.set("repeat", loom::Cell::boolean(true));
    const op::Evaluation e = renamed.evaluate(tmr::kNormalizeDelay, std::move(ask));
    CHECK_FALSE(e.ok());
    CHECK(e.reason().find("not the signature this composition was authored against") !=
          std::string::npos);
    // FOUND, and not the thing this was written for. A reference that recorded
    // no signature would have answered 1 and been wrong in silence the day a
    // port moved.
    CHECK(e.reason().find("unresolved") == std::string::npos);

    // A step that is simply GONE says so, and says which one.
    op::Catalog partial;
    partial.publish(op::make_operator<&op::select_int>(
        op::kSelectInt, {"condition", "when_true", "when_false"}, "result"));
    partial.publish(rule);
    loom::Value again(rule.inputs());
    again.set("delay_ms", loom::Cell::integer(-500));
    again.set("repeat", loom::Cell::boolean(true));
    const op::Evaluation missing = partial.evaluate(tmr::kNormalizeDelay, std::move(again));
    CHECK_FALSE(missing.ok());
    CHECK(missing.reason().find("unresolved operator reference 'math.max'") != std::string::npos);
}

// ---- 4. the independent consumer -------------------------------------------

TEST_CASE("a stranger reads the rule's ports off the rule itself") {
    const op::Catalog catalog = tmr::fallback_vocabulary();
    const stranger::Signature sig = stranger::describe(catalog, "timer.normalize_delay");

    REQUIRE(sig.found);
    CHECK(sig.composite);
    REQUIRE(sig.inputs.size() == 2);
    CHECK(sig.inputs[0].name == "delay_ms");
    CHECK(sig.inputs[0].kind == "Int");
    CHECK(sig.inputs[1].name == "repeat");
    CHECK(sig.inputs[1].kind == "Bool");
    REQUIRE(sig.outputs.size() == 1);
    CHECK(sig.outputs[0].name == "effective_delay");
    CHECK(sig.outputs[0].kind == "Int");

    CHECK_FALSE(stranger::describe(catalog, "timer.no_such_rule").found);
}

TEST_CASE("a stranger evaluates the rule over the whole matrix, from text") {
    const op::Catalog catalog = tmr::fallback_vocabulary();

    struct Case {
        const char* delay;
        const char* repeat;
        const char* effective;
    };
    const Case cases[] = {
        {"-500", "false", "0"},       {"-500", "true", "1"},
        {"-1", "false", "0"},         {"-1", "true", "1"},
        {"0", "false", "0"},          {"0", "true", "1"},
        {"1", "false", "1"},          {"1", "true", "1"},
        {"2", "false", "2"},          {"2", "true", "2"},
        {"86400000", "false", "86400000"}, {"86400000", "true", "86400000"},
    };
    for (const Case& c : cases) {
        const stranger::Reading r = stranger::ask(
            catalog, "timer.normalize_delay", {{"delay_ms", c.delay}, {"repeat", c.repeat}});
        REQUIRE_MESSAGE(r.ok, "refused: ", r.reason);
        CHECK(r.port == "effective_delay");
        CHECK(r.answer == c.effective);
    }
}

TEST_CASE("a stranger's refusals belong to whoever owns the reason") {
    const op::Catalog catalog = tmr::fallback_vocabulary();

    const stranger::Reading unknown =
        stranger::ask(catalog, "timer.no_such_rule", {{"delay_ms", "1"}});
    CHECK_FALSE(unknown.ok);
    CHECK(unknown.reason.find("publishes no") != std::string::npos);

    const stranger::Reading no_port =
        stranger::ask(catalog, "timer.normalize_delay",
                      {{"delay_ms", "1"}, {"repeat", "true"}, {"units", "ms"}});
    CHECK_FALSE(no_port.ok);
    CHECK(no_port.reason.find("no input port named 'units'") != std::string::npos);

    const stranger::Reading not_an_int = stranger::ask(
        catalog, "timer.normalize_delay", {{"delay_ms", "soon"}, {"repeat", "true"}});
    CHECK_FALSE(not_an_int.ok);
    CHECK(not_an_int.reason == "'soon' is not an Int");

    // A port left out is the GATE's sentence, not the reader's — the reader
    // counts nothing.
    const stranger::Reading short_pack =
        stranger::ask(catalog, "timer.normalize_delay", {{"delay_ms", "1"}});
    CHECK_FALSE(short_pack.ok);
    CHECK(short_pack.reason.find("repeat") != std::string::npos);
}

// ---- 5. one path, not two that agree ---------------------------------------

TEST_CASE("replace a primitive UNDER the rule and every consumer of it moves together") {
    const op::Catalog honest = tmr::fallback_vocabulary();
    const op::Catalog sabotaged = zengine::testing::sabotaged_operators();

    // The saboteur is indistinguishable STRUCTURALLY: same identity, same port
    // names, same types, therefore the same two content ids. Nothing refuses it,
    // and nothing should — it is a different implementation of a published
    // signature, which is exactly what a hot-replaced provider is.
    CHECK(sabotaged.find(op::kMaxInt)->inputs()->content_id() ==
          honest.find(op::kMaxInt)->inputs()->content_id());

    // `max` became `min`, so the rule now floors nothing: max(max(-500,0),1)
    // becomes min(min(-500,0),1) == -500.
    CHECK(tmr::effective_delay(honest, -500, true) == 1);
    CHECK(tmr::effective_delay(sabotaged, -500, true) == -500);

    // ...and the stranger, which shares no line of code with the caller above,
    // says the same new thing about the same catalog. Two implementations that
    // merely agreed could not do this: one of them would still say 1.
    const stranger::Reading honest_read =
        stranger::ask(honest, "timer.normalize_delay", {{"delay_ms", "-500"}, {"repeat", "true"}});
    const stranger::Reading sabotaged_read = stranger::ask(
        sabotaged, "timer.normalize_delay", {{"delay_ms", "-500"}, {"repeat", "true"}});
    REQUIRE(honest_read.ok);
    REQUIRE(sabotaged_read.ok);
    CHECK(honest_read.answer == "1");
    CHECK(sabotaged_read.answer == "-500");

    // The composition itself is BYTE-IDENTICAL across the two catalogs — the
    // graph did not change, the leaf did. That is what "the rule has one owner
    // and its parts have theirs" looks like from the inside.
    const op::Composite* a = honest.find(tmr::kNormalizeDelay)->composition();
    const op::Composite* b = sabotaged.find(tmr::kNormalizeDelay)->composition();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a->nodes.size() == b->nodes.size());
    for (std::size_t i = 0; i < a->nodes.size(); ++i) {
        CHECK(a->nodes[i].identity == b->nodes[i].identity);
        CHECK(a->nodes[i].authored_in == b->nodes[i].authored_in);
        CHECK(a->nodes[i].authored_out == b->nodes[i].authored_out);
    }
}

// ---- 5. what one evaluation may spend --------------------------------------

TEST_CASE("a call cycle through identities is refused in words when it would nest past the "
          "evaluation's budget, and the next evaluation starts with nothing spent") {
    op::Catalog catalog;
    // `cyc.b` first as a native leaf, so `cyc.a` can be authored against its ports; then a
    // composite `cyc.b` overlays it at the same signature and names `cyc.a`. Neither graph holds a
    // cycle; the catalog does, through identities, closed by a later mount.
    REQUIRE(catalog.mount("cyc.leaf", {op::make_operator<&same_int>("cyc.b", {"x"}, "result")}));
    op::Builder a(catalog, "cyc.a", one_int());
    const op::Builder::Ref to_b = a.call("cyc.b", {a.input("x")});
    REQUIRE(catalog.mount("cyc.one", {std::move(a).result("result", to_b)}));
    op::Builder b(catalog, "cyc.b", one_int());
    const op::Builder::Ref to_a = b.call("cyc.a", {b.input("x")});
    REQUIRE(catalog.mount("cyc.two", {std::move(b).result("result", to_a)}, op::MountMode::Overlay));

    const std::uint64_t before = op::invocations();
    loom::Value ask(catalog.find("cyc.a")->inputs());
    ask.set("x", loom::Cell::integer(7));
    const op::Evaluation cycled = catalog.evaluate("cyc.a", ask);
    REQUIRE_FALSE(cycled.ok());
    // The spend refused is the 33rd nesting: a, b, a, b ... the 32nd is 'cyc.b', so 'cyc.a' next.
    CHECK(cycled.reason() ==
          "spending 'cyc.a' would nest this evaluation " +
              std::to_string(op::kEvaluationDepth + 1) + " operators deep, past its budget of " +
              std::to_string(op::kEvaluationDepth) +
              ": an operator that reaches itself through identities nests without end");
    CHECK(op::invocations() == before);

    // Unmount the overlay and the same evaluation answers: nothing of the refused one was kept.
    REQUIRE(catalog.unmount("cyc.two"));
    const op::Evaluation answered = catalog.evaluate("cyc.a", ask);
    REQUIRE(answered.ok());
    CHECK(int_answer(answered) == 7);
}

TEST_CASE("an evaluation is refused at the spend that would pass its budget of spends, however "
          "shallow, and its budget is not the next evaluation's") {
    op::Catalog catalog;
    REQUIRE(catalog.mount("wide.leaf",
                          {op::make_operator<&same_int>("wide.leaf", {"x"}, "result")}));
    REQUIRE(catalog.mount("wide.one", {wide(catalog, "wide.one", "wide.leaf", 50)}));
    REQUIRE(catalog.mount("wide.two", {wide(catalog, "wide.two", "wide.one", 50)}));
    REQUIRE(catalog.mount("wide.three", {wide(catalog, "wide.three", "wide.two", 50)}));
    loom::Value ask(catalog.find("wide.three")->inputs());
    ask.set("x", loom::Cell::integer(3));

    // `wide.two` spends 1 + 50 * (1 + 50) = 2551 operators; `wide.three` would spend 127551, four
    // deep at most. The spend that would pass the budget is refused, and nothing past it runs:
    // 39 whole `wide.two`s, 9 whole `wide.one`s and 49 leaves had run, 97999 native bodies.
    const std::uint64_t before = op::invocations();
    const op::Evaluation refused = catalog.evaluate("wide.three", ask);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.reason() == "spending 'wide.leaf' would pass this evaluation's budget of " +
                                  std::to_string(op::kEvaluationSpends) + " operator spends");
    CHECK(op::invocations() - before == 97999);

    // The next evaluation is a new one, and a copy of the catalog is another evaluator.
    loom::Value two(catalog.find("wide.two")->inputs());
    two.set("x", loom::Cell::integer(3));
    const op::Evaluation answered = catalog.evaluate("wide.two", two);
    REQUIRE(answered.ok());
    CHECK(int_answer(answered) == 3);
    const op::Catalog copy = catalog;
    CHECK(copy.evaluate("wide.two", two).ok());
}

// ---- 6. the fold ------------------------------------------------------------

TEST_CASE("a fold counts first and iterates by count: the step 1, a negative step, a larger step, "
          "a step away from the limit, and the extremes of Int") {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    REQUIRE(catalog.mount("t.bodies",
                          {op::make_operator<&plus_one>("t.plus_one", {"acc", "count"}, "result"),
                           op::make_operator<&last_count>("t.last", {"acc", "count"}, "result")}));
    REQUIRE(catalog.mount("t.folds", {folding(catalog, "t.sum", op::kAddInt, "rhs", "lhs"),
                                       folding(catalog, "t.times", "t.plus_one", "count", "acc"),
                                       folding(catalog, "t.last_of", "t.last", "count", "acc")}));
    CHECK(catalog.find("t.sum")->composition()->nodes[0].fold.has_value());

    const std::uint64_t before = op::invocations();
    CHECK(int_answer(run_fold(catalog, "t.sum", 0, 10, 1, 0)) == 45);
    CHECK(op::invocations() - before == 10); // one body spend per count, and nothing else
    CHECK(int_answer(run_fold(catalog, "t.sum", 10, 0, -2, 0)) == 30);  // 10, 8, 6, 4, 2
    CHECK(int_answer(run_fold(catalog, "t.sum", 0, 10, 3, 0)) == 18);   // 0, 3, 6, 9
    CHECK(int_answer(run_fold(catalog, "t.sum", 0, 10, -1, 7)) == 7);   // runs zero times
    CHECK(int_answer(run_fold(catalog, "t.sum", 5, 5, 1, 7)) == 7);     // the limit is exclusive
    CHECK(int_answer(run_fold(catalog, "t.times", 0, 10, 3, 0)) == 4);

    // COUNTED BY k, NEVER BY REPEATED ADDITION: from the least Int toward the greatest by the
    // greatest visits the least, -1 and the greatest less one, and nothing overflows.
    const std::int64_t lo = std::numeric_limits<std::int64_t>::min();
    const std::int64_t hi = std::numeric_limits<std::int64_t>::max();
    CHECK(int_answer(run_fold(catalog, "t.times", lo, hi, hi, 0)) == 3);
    CHECK(int_answer(run_fold(catalog, "t.last_of", lo, hi, hi, 0)) == hi - 1);
    CHECK(int_answer(run_fold(catalog, "t.last_of", hi, lo, lo, 0)) == -1);
    CHECK(int_answer(run_fold(catalog, "t.times", hi, lo, lo, 0)) == 2);
}

TEST_CASE("a fold refuses a step of 0, a count past its bound and a stale body in words, before "
          "its body is spent once") {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    REQUIRE(catalog.mount("t.folds", {folding(catalog, "t.sum", op::kAddInt, "rhs", "lhs")}));
    const std::uint64_t before = op::invocations();

    const op::Evaluation zero = run_fold(catalog, "t.sum", 0, 10, 0, 0);
    REQUIRE_FALSE(zero.ok());
    CHECK(zero.reason() == "'t.sum' step 0: a step of 0 never moves the count from 0 toward 10");

    const op::Evaluation past = run_fold(catalog, "t.sum", 0, 2000000, 1, 0);
    REQUIRE_FALSE(past.ok());
    CHECK(past.reason() == "'t.sum' step 0: this fold would count 2000000 times from 0 toward "
                          "2000000 by 1, and a fold counts at most " +
                              std::to_string(op::kMaxFoldCount) + " times");
    CHECK(int_answer(run_fold(catalog, "t.sum", 0, static_cast<std::int64_t>(op::kMaxFoldCount), 1,
                              0)) == static_cast<std::int64_t>(op::kMaxFoldCount) *
                                         static_cast<std::int64_t>(op::kMaxFoldCount - 1) / 2);
    CHECK(op::invocations() - before == op::kMaxFoldCount);

    // THE BODY IS A REFERENCE: found at another signature it is refused, never re-bound; gone, it
    // is the catalog's own sentence.
    op::Catalog reshaped;
    reshaped.publish(op::make_operator<&op::add_int>(op::kAddInt, {"left", "right"}, "result"));
    reshaped.publish(*catalog.find("t.sum"));
    const op::Evaluation stale = run_fold(reshaped, "t.sum", 0, 10, 1, 0);
    REQUIRE_FALSE(stale.ok());
    CHECK(stale.reason() ==
          "'t.sum' step 0: 'math.add' is not the signature this composition was authored against");
    op::Catalog bare;
    bare.publish(*catalog.find("t.sum"));
    CHECK(run_fold(bare, "t.sum", 0, 10, 1, 0).reason() ==
          "'t.sum' step 0: unresolved operator reference 'math.add'");
}

TEST_CASE("a fold's other body ports are wired from scope, and a body's refusal is the fold's, "
          "naming the iteration and its count") {
    op::Catalog catalog;
    REQUIRE(catalog.mount("t.bodies",
        {op::make_operator<&scaled>("t.scaled", {"acc", "count", "n"}, "result"),
         op::make_operator<&capped>("t.capped", {"acc", "count", "cap"}, "result")}));
    auto with_extra = [&](const std::string& identity, const std::string& body,
                          const std::string& extra) {
        std::vector<loom::Field> ports = fold_inputs();
        ports.push_back(loom::Field{extra, loom::type_of(loom::Kind::Int), true});
        op::Builder b(catalog, identity, ports);
        const op::Builder::Ref answer = b.fold(body, "count", "acc",
            {b.input("start"), b.input("limit"), b.input("step"), b.input("initial"),
             b.input(extra)});
        return std::move(b).result("result", answer);
    };
    REQUIRE(catalog.mount("t.folds", {with_extra("t.table", "t.scaled", "n"),
                                       with_extra("t.capped_sum", "t.capped", "cap")}));
    const auto ask = [&](const std::string& identity, std::int64_t limit, const char* extra,
                         std::int64_t value) {
        loom::Value v(catalog.find(identity)->inputs());
        v.set("start", loom::Cell::integer(1));
        v.set("limit", loom::Cell::integer(limit));
        v.set("step", loom::Cell::integer(1));
        v.set("initial", loom::Cell::integer(0));
        v.set(extra, loom::Cell::integer(value));
        return catalog.evaluate(identity, std::move(v));
    };
    CHECK(int_answer(ask("t.table", 11, "n", 7)) == 7 * 55); // 7 * (1 + ... + 10), n bound once
    const op::Evaluation capped_out = ask("t.capped_sum", 100, "cap", 100);
    REQUIRE_FALSE(capped_out.ok());
    CHECK(capped_out.reason() ==
          "'t.capped_sum' step 0: iteration 13 (count 14): 't.capped' refuses a total above 100");
}

TEST_CASE("a fold resolves its body at the spend: an overlay mounted between evaluations changes "
          "the next answer, and unmounting it changes it back") {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    REQUIRE(catalog.mount("t.folds", {folding(catalog, "t.sum", op::kAddInt, "rhs", "lhs")}));
    CHECK(int_answer(run_fold(catalog, "t.sum", 0, 5, 1, 0)) == 10);
    REQUIRE(catalog.mount("t.doubled",
                          {op::make_operator<&doubled>(op::kAddInt, {"lhs", "rhs"}, "result")},
                          op::MountMode::Overlay));
    CHECK(int_answer(run_fold(catalog, "t.sum", 0, 5, 1, 0)) == 20);
    REQUIRE(catalog.unmount("t.doubled"));
    CHECK(int_answer(run_fold(catalog, "t.sum", 0, 5, 1, 0)) == 10);
}

TEST_CASE("nested folds draw on one evaluation's budget, refused at the spend that would pass it") {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    // `t.inner(acc, count)` = acc + (0 + 1 + ... + 999), itself a fold; `t.outer` folds it 1000
    // times. Each outer count spends `t.inner` and its 1000 additions: 1001 spends.
    op::Builder inner(catalog, "t.inner",
                      {loom::Field{"acc", loom::type_of(loom::Kind::Int), true},
                       loom::Field{"count", loom::type_of(loom::Kind::Int), true}});
    const op::Builder::Ref summed = inner.fold(op::kAddInt, "rhs", "lhs",
        {inner.constant(std::int64_t{0}), inner.constant(std::int64_t{1000}),
         inner.constant(std::int64_t{1}), inner.input("acc")});
    REQUIRE(catalog.mount("t.inner", {std::move(inner).result("result", summed)}));
    REQUIRE(catalog.mount("t.outer", {folding(catalog, "t.outer", "t.inner", "count", "acc")}));

    const std::uint64_t before = op::invocations();
    const op::Evaluation nested = run_fold(catalog, "t.outer", 0, 1000, 1, 0);
    REQUIRE_FALSE(nested.ok());
    // 1 + 99 * 1001 spends, then `t.inner` and 899 additions: the 900th is the one refused.
    CHECK(nested.reason() == "'t.outer' step 0: iteration 99 (count 99): 't.inner' step 0: "
                             "iteration 899 (count 899): spending 'math.add' would pass this "
                             "evaluation's budget of " +
                                 std::to_string(op::kEvaluationSpends) + " operator spends");
    CHECK(op::invocations() - before == 99 * 1000 + 899);
    // Under the budget the same nesting answers: 10 outer counts of 499500 each.
    CHECK(int_answer(run_fold(catalog, "t.outer", 0, 10, 1, 0)) == 10 * 499500);
}

TEST_CASE("a body that cannot be a fold's is refused by name where the fold is written") {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    REQUIRE(catalog.mount("t.bodies",
        {op::make_operator<&never>("t.never", {"acc", "count"}, "result"),
         op::make_operator<&scaled>("t.start", {"acc", "count", "start"}, "result")}));
    const auto refusal = [&](const std::string& body, const std::string& count,
                             const std::string& acc, int extra) {
        op::Builder b(catalog, "t.bad", fold_inputs());
        std::vector<op::Builder::Ref> args{b.input("start"), b.input("limit"), b.input("step"),
                                           b.input("initial")};
        for (int i = 0; i < extra; ++i) {
            args.push_back(b.input("start"));
        }
        if (extra < 0) {
            args.push_back(b.constant(true));
        }
        try {
            (void)b.fold(body, count, acc, args);
        } catch (const std::invalid_argument& e) {
            return std::string(e.what());
        }
        return std::string("accepted");
    };
    CHECK(refusal(op::kAddInt, "count", "lhs", 0) == "'math.add' has no port 'count' for the count");
    CHECK(refusal(op::kAddInt, "rhs", "rhs", 0) ==
          "the count and the accumulator are two ports; 'math.add' was given 'rhs' for both");
    CHECK(refusal(op::kAddInt, "rhs", "acc", 0) ==
          "'math.add' has no port 'acc' for the accumulator");
    CHECK(refusal(op::kSelectInt, "condition", "when_true", 0) ==
          "'logic.select_int' port 'condition' is Bool, and the count is an Int");
    CHECK(refusal("t.never", "count", "acc", 0) ==
          "'t.never' port 'acc' is Int and it answers Bool; the accumulator and the answer are one "
          "type");
    CHECK(refusal("t.start", "count", "acc", 1) ==
          "'t.start' has a port named 'start', which the fold's own ports already use");
    CHECK(refusal(op::kAddInt, "rhs", "lhs", 1) ==
          "a fold over 'math.add' takes 4 arguments, not 5");
    CHECK(refusal("t.nobody", "rhs", "lhs", 0) == "'t.bad' names an unpublished operator 't.nobody'");
    // select_int folds too: the count to one integer, the accumulator to the other, and the
    // condition wired from scope.
    CHECK(refusal(op::kSelectInt, "when_false", "when_true", -1) == "accepted");
}
