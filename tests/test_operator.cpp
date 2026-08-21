// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Operator suite (SEM-0) — what a named semantic truth IS, and what it takes
// for two consumers to spend the same one.
//
// The tiers, and what each is for:
//
//   1  DERIVATION   that a signature comes from ordinary C++ and is not restated
//                   beside it, and that a derived port schema is byte-for-byte
//                   the hand-built one.
//   2  THE STORE    that discovery and invocation are one record read twice.
//   3  COMPOSITION  that `timer.normalize_delay` is a graph over published
//                   primitives and carries NO native semantics of its own —
//                   which is the difference between proving registration works
//                   and proving composition does.
//   4  THE STRANGER an independent translation unit evaluating the same rule
//                   knowing only a catalog and a name.
//   5  ONE PATH     that tier 3 and tier 4 are not two implementations that
//                   happen to agree.
//
// The Timer's own execution of the rule is pinned next door, in test_timer.cpp,
// because that claim is about a running weave and belongs with the weave.

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
    const op::Catalog catalog = tmr::standard_operators();

    const std::vector<std::string> names = catalog.identities();
    CHECK(names.size() == catalog.size());
    CHECK(names.size() == 3);

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
    const op::Catalog catalog = tmr::standard_operators();
    const op::OperatorDef* rule = catalog.find(tmr::kNormalizeDelay);
    REQUIRE(rule != nullptr);

    // THE LOAD-BEARING ASSERTION OF THE PHASE. A native `normalize_delay`
    // registered under this identity would satisfy every other case in this file
    // and would prove only that registration works.
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
    const op::Catalog catalog = tmr::standard_operators();
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
    const op::Catalog catalog = tmr::standard_operators();
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
    const op::Catalog catalog = tmr::standard_operators();

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
    const op::Catalog catalog = tmr::standard_operators();

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
    const op::Catalog honest = tmr::standard_operators();
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
