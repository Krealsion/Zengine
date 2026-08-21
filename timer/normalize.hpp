// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_NORMALIZE_HPP
#define ZENGINE_TIMER_NORMALIZE_HPP

// WHAT A TIMER MAKES OF A DELAY (SEM-0) — the one place that rule lives.
//
// A maker may author
//
//     delay_ms = -500
//     repeat   = true
//
// and every structural check in the system says the Int is fine, while the
// Timer schedules 1 ms. That difference is a real product fact, not an
// implementation detail: AAF-R0 measured `EnsureTimer` reading `-500` back as
// `preserved_remaining` for a 1 ms hot beat, because a preflight that shares
// execution's truth also shares its normalization and nobody could see it.
//
// The problem was never that the Timer normalizes. It was that the rule lived in
// three lines of private arithmetic no other surface could reach, so anything
// that wanted to say what a Timer WOULD do had to hold a second copy of it —
// and a second copy of a rule is a second answer waiting for one of them to be
// edited.
//
// SO THE RULE IS A COMPOSITION, and it is this:
//
//     timer.normalize_delay(delay_ms : Int, repeat : Bool) -> effective_delay : Int
//
//         floor_zero = math.max(delay_ms, 0)
//         floor_one  = math.max(floor_zero, 1)
//         effective  = logic.select_int(repeat, floor_one, floor_zero)
//
// A repeating delay below 1 ms is a hot spin wearing a timer's clothes; a
// negative delay fires on the next beat. Those two sentences are the whole of
// what this rule knows, and it knows nothing else — not whether an id is taken,
// not whether a requester may ask, not whether a beat will ever arrive.
//
// NOTHING NATIVE IMPLEMENTS IT. `timer.normalize_delay` carries no C++ body at
// all: it is three nodes over two published primitives, evaluated by the one
// evaluator every other consumer uses. That is what makes a second consumer
// possible without compiling against this file, and it is what makes the
// difference between the Timer and that consumer UNREPRESENTABLE — there is no
// second implementation to disagree with.

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"

#include <zen/kind.hpp>
#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace zengine::timer {

/// The rule's identity, and its three port names. Spelled once, because a
/// consumer names them and a suite asserts them: two spellings of a port is how
/// a caller and a callee stop meaning the same thing.
inline constexpr const char* kNormalizeDelay = "timer.normalize_delay";
inline constexpr const char* kAuthoredDelayPort = "delay_ms";
inline constexpr const char* kRepeatPort = "repeat";
inline constexpr const char* kEffectiveDelayPort = "effective_delay";

/// Author the rule against a catalog that already carries the primitives.
///
/// The output schema is DERIVED — `Builder` resolves each step as it is written,
/// so what `effective_delay` is comes from what `logic.select_int` answers with,
/// not from a type restated here. The two INPUT ports are the one place in this
/// package where a Loom type is written by hand, and they are written once,
/// because a composite has no C++ signature to take them from.
inline op::OperatorDef normalize_delay(const op::Catalog& primitives) {
    op::Builder rule(primitives, kNormalizeDelay,
                     {loom::Field{kAuthoredDelayPort, loom::type_of(loom::Kind::Int), true},
                      loom::Field{kRepeatPort, loom::type_of(loom::Kind::Bool), true}});

    const op::Builder::Ref floor_zero =
        rule.call(op::kMaxInt, {rule.input(kAuthoredDelayPort), rule.constant(std::int64_t{0})});
    const op::Builder::Ref floor_one =
        rule.call(op::kMaxInt, {floor_zero, rule.constant(std::int64_t{1})});
    const op::Builder::Ref effective =
        rule.call(op::kSelectInt, {rule.input(kRepeatPort), floor_one, floor_zero});

    return std::move(rule).result(kEffectiveDelayPort, effective);
}

/// Everything the Timer's semantics are made of: two primitive powers and the
/// one rule composed from them.
///
/// It is a VALUE, not a registry and not a singleton. A Timer holds one, a
/// consumer may hold the same one, and anybody who wants a different vocabulary
/// builds a different catalog rather than editing somebody else's.
inline op::Catalog standard_operators() {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    catalog.publish(normalize_delay(catalog));
    return catalog;
}

/// Spell the ask, run it, read the answer. This function knows how to CALL the
/// rule and nothing about what the rule says — which is exactly the amount an
/// ordinary helper is allowed to know once the meaning has an owner.
///
/// It is total for any catalog that ACCEPTED the rule, because `Builder`
/// resolves and signature-checks every step at authorship and `Catalog` has no
/// erase, no replace and no rebind. A catalog that never carried the rule, or
/// one assembled by hand out of steps that do not agree, is an authoring
/// mistake rather than a runtime condition, and it says so — refusing loudly is
/// the only honest answer, because inventing a delay here would be the second
/// copy of the rule this whole phase exists to remove.
inline std::int64_t effective_delay(const op::Catalog& catalog, std::int64_t delay_ms,
                                    bool repeat) {
    const op::OperatorDef* rule = catalog.find(kNormalizeDelay);
    if (rule == nullptr) {
        throw std::invalid_argument("this catalog does not carry '" +
                                    std::string(kNormalizeDelay) + "'");
    }
    loom::Value ask(rule->inputs());
    ask.set(kAuthoredDelayPort, loom::Cell::integer(delay_ms));
    ask.set(kRepeatPort, loom::Cell::boolean(repeat));

    const op::Evaluation answer = catalog.evaluate(kNormalizeDelay, std::move(ask));
    if (!answer) {
        throw std::invalid_argument(answer.reason());
    }
    return answer.value().at(0)->as_int();
}

} // namespace zengine::timer

#endif // ZENGINE_TIMER_NORMALIZE_HPP
