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
#include "operator/host.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"

#include <zen/kind.hpp>
#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <memory>
#include <optional>
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

/// WHAT THIS PACKAGE CONTRIBUTES TO A HOST (PROV-0) — its domain composition, and
/// not one primitive.
///
/// The catalog inside is AUTHORING SCAFFOLDING and dies at the closing brace.
/// `Builder` resolves each step as it is written, so it needs the primitives'
/// SIGNATURES present to derive the output type and to snapshot the two content ids
/// each node was authored against — and needing a signature to compose against is
/// not owning the power. What comes out is a graph whose nodes say `math.max` and
/// `logic.select_int`, and whoever provides those in the world this rule lands in is
/// whose implementation it spends.
///
/// THAT IS WHY THE TIMER NO LONGER SUPPLIES THEM. Before PROV-0 the host published
/// `standard_operators()` — the primitives and the rule together, out of this
/// package — so replacing `math.max` meant replacing something the Timer owned. Now
/// the Timer artifact contributes exactly one power, the basic provider contributes
/// the two it names, and the host composes what it was given.
inline std::vector<op::OperatorDef> provider_contributions() {
    op::Catalog against;
    op::publish_primitives(against);
    std::vector<op::OperatorDef> defs;
    defs.push_back(normalize_delay(against));
    return defs;
}

/// THE NO-HOST ARRANGEMENT'S VOCABULARY: the basic primitive definitions plus this
/// package's own composition, ASSEMBLED LOCALLY because nobody is claiming semantic
/// authority here.
///
/// It is a VALUE, not a registry and not a singleton. A fallback Timer holds one,
/// and anybody who wants a different vocabulary builds a different catalog rather
/// than editing somebody else's.
///
/// IT IS NOT A HOST'S CATALOG AND MUST NOT BE MISTAKEN FOR ONE. It used to be
/// called `standard_operators()`, and the name was a claim this package had stopped
/// being able to make: Workshop called it to manufacture the whole process's
/// semantic vocabulary, which meant the Timer package owned every power in the
/// system. What this function is FOR is the arrangement where there is no host at
/// all — `snake`, a plain Loom, any program that predates the operator seam — and
/// where assembling a local vocabulary is the honest thing rather than a fallback
/// from a failure. It reuses the same two definitions the basic provider
/// contributes, so a standalone Timer needs no second artifact to keep behaving
/// exactly as it always did.
inline op::Catalog fallback_vocabulary() {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    catalog.publish(normalize_delay(catalog));
    return catalog;
}

/// THE ASK, spelled once for both doors (CAT-0). The pack is built against the
/// INPUT SCHEMA THE AUTHORITY ANSWERED WITH — a definition found in a local
/// catalog, or a contract described across the operator-host seam — which is why
/// this takes a schema rather than either of them.
///
/// Two consumers of one spelling is the whole reason it is a function: a
/// host-backed Timer and a fallback Timer must not be two ways of writing down
/// the same two ports.
inline loom::Value normalize_ask(const std::shared_ptr<const loom::Schema>& inputs,
                                 std::int64_t delay_ms, bool repeat) {
    loom::Value ask(inputs);
    ask.set(kAuthoredDelayPort, loom::Cell::integer(delay_ms));
    ask.set(kRepeatPort, loom::Cell::boolean(repeat));
    return ask;
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
    const op::Evaluation answer =
        catalog.evaluate(kNormalizeDelay, normalize_ask(rule->inputs(), delay_ms, repeat));
    if (!answer) {
        throw std::invalid_argument(answer.reason());
    }
    return answer.value().at(0)->as_int();
}

/// WHICH SEMANTIC AUTHORITY A TIMER SPENDS (CAT-0) - one of exactly two, chosen
/// once, and never both.
///
/// SEM-0 gave the delay rule ONE AUTHORING. That was not enough, and CAT-0 is
/// the measurement of why: an authoring instantiated twice is two runtime
/// catalogs, and the moment either can be REPLACED the two consumers reading
/// them stop agreeing without anybody editing a rule. Live agreement needs one
/// CURRENT truth, not one authored one.
///
///     HOST-BACKED       a host offered this instance its operator surface, and
///                       every normalization goes back across that seam to the
///                       host's own catalog, resolved at the moment of the call.
///                       There is no catalog in this object at all.
///
///     LOCAL-FALLBACK    nobody offered anything, so this Timer carries the
///                       vocabulary this repository authors and spends that.
///                       The floor, and a supported arrangement rather than a
///                       degraded one: `snake` and every host that predates the
///                       operator seam land here and are not warned at.
///
/// THE CHOICE IS THE CONSTRUCTOR'S, and it is fixed for this object's life.
/// Re-asking on every schedule would mean a Timer whose semantics depend on
/// which load happened to be in flight, which is the one thing a scoped offer is
/// arranged to make impossible.
///
/// AND A HOST-BACKED TIMER NEVER FALLS BACK. There is no path from a host that
/// refused to a local evaluation: `local_` is empty in that mode, so "quietly
/// evaluate our own copy" is not a branch somebody forgot to write - it is
/// unrepresentable. A host that cannot serve the rule is refused at construction
/// (below), which is the earliest and deepest place the fact is knowable, and a
/// host that fails afterwards is a throw rather than a second answer.
class DelayAuthority {
public:
    /// LOCAL-FALLBACK over the vocabulary this repository authors.
    DelayAuthority() : local_(fallback_vocabulary()) {}

    /// LOCAL-FALLBACK over a catalog the caller chose - SEM-0's seam, unchanged.
    /// It is what lets a suite replace a primitive underneath the rule and watch
    /// a running weave and an independent reader move together.
    explicit DelayAuthority(op::Catalog local) : local_(std::move(local)) {}

    /// WHAT A LOADED TIMER DOES WITH WHAT IT WAS OFFERED.
    ///
    /// An UNBOUND host is not a failure: it is the ordinary state of a weave
    /// nobody offered anything to, and it means LOCAL-FALLBACK. That covers the
    /// host that never heard of operators AND the handoff that was refused on its
    /// version - in both cases nothing was supplied, which the host knows from
    /// its own `OperatorOffer::outcome()` and may act on there. Fallback chooses
    /// an authority only where none arrived; it is never recovery from one that
    /// arrived and then failed.
    ///
    /// A BOUND host is checked before it is accepted, because "not silently" has
    /// to mean something at a moment somebody can see. The host must publish
    /// `timer.normalize_delay` and it must publish it at the signature this Timer
    /// was authored against - same name, same version, same normalized structure,
    /// which is exactly what `loom::same_identity` already answers and what
    /// `Schema::content_id()` already versions. No hash of this phase's own
    /// invention, and no second description of the rule: the expectation is
    /// derived from `normalize_delay`, the one authoring -- the SAME call this
    /// package's provider contribution is built from -- and the scaffolding it needs
    /// dies at the closing brace. A host-backed Timer holds no catalog at all.
    ///
    /// It THROWS, and the throw is the refusal. Inside a loaded artifact it
    /// travels exactly one frame: `create()` catches it, returns null, and the
    /// Kernel refuses the load with `library create() returned null`. A Timer
    /// that could not get the semantics it was promised must not become the
    /// Timer.
    explicit DelayAuthority(op::OperatorHost offered) {
        if (!offered.bound()) {
            local_.emplace(fallback_vocabulary());
            return;
        }
        const op::HostSignature contract = offered.describe(kNormalizeDelay);
        if (!contract.ok()) {
            throw std::invalid_argument(
                "this operator host publishes no '" + std::string(kNormalizeDelay) +
                "' (status " + std::to_string(static_cast<int>(contract.status)) +
                "); a Timer offered a host must spend it, so this one refuses to run");
        }
        op::Catalog against;
        op::publish_primitives(against);
        const op::OperatorDef mine = normalize_delay(against);
        if (!loom::same_identity(*contract.inputs, *mine.inputs()) ||
            !loom::same_identity(*contract.outputs, *mine.outputs())) {
            throw std::invalid_argument(
                "this operator host's '" + std::string(kNormalizeDelay) +
                "' is not the signature this Timer was authored against; a Timer offered a "
                "host must spend it, so this one refuses to run");
        }
        host_ = offered;
        contract_ = contract;
    }

    /// Which of the two this is. A diagnostic, never a door: nothing about the
    /// answer changes what `effective_delay` spends, and no case may prove
    /// canonicality by reading it.
    bool host_backed() const noexcept { return !local_.has_value(); }

    /// The catalog this Timer carries - LOCAL-FALLBACK only, because a
    /// host-backed one carries none. Asking a host-backed authority for a
    /// catalog is asking for the object CAT-0 exists to stop having two of.
    const op::Catalog& operators() const {
        if (!local_) {
            throw std::invalid_argument(
                "this Timer is host-backed: its operator truth lives in the host's catalog, "
                "and there is no local one to hand out");
        }
        return *local_;
    }

    /// WHAT THIS TIMER MAKES OF AN AUTHORED DELAY. The one semantic entrance,
    /// and the only difference between its two branches is WHERE THE IDENTITY
    /// RESOLVES. Both spend `timer.normalize_delay`; neither knows what it says.
    /// There is no arithmetic in this file to disagree with it and no second
    /// implementation for one of them to reach.
    std::int64_t effective_delay(std::int64_t delay_ms, bool repeat) const {
        if (local_) {
            return timer::effective_delay(*local_, delay_ms, repeat);
        }
        // RESOLVED AT SPEND, on the far side. `contract_` is an identity and two
        // schemas this instance built for itself - never a pointer, an index or a
        // callable into the host - so the host resolves its own current
        // definition on every single call, and a rule that changed underneath is
        // spent as it is now rather than as it was described.
        const op::HostAnswer answer =
            host_.evaluate(contract_, normalize_ask(contract_.inputs, delay_ms, repeat));
        if (!answer.ok()) {
            // Unreachable against a host whose catalog outlives this Timer and
            // cannot be edited, which is every arrangement that exists today. It
            // is written anyway, and it THROWS, because the only other spelling
            // of this branch is the silent fallback the whole phase forbids.
            throw std::invalid_argument("'" + std::string(kNormalizeDelay) +
                                        "' was refused by this Timer's operator host: " +
                                        answer.reason);
        }
        return answer.value->at(0)->as_int();
    }

private:
    /// EXACTLY ONE OF THESE IS ENGAGED, and that is the structure doing the work
    /// rather than a comment asking for care. In HOST-BACKED mode `local_` is
    /// empty, so there is no local catalog in the object for a later edit to
    /// reach by accident.
    std::optional<op::Catalog> local_;
    op::OperatorHost host_;
    op::HostSignature contract_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_NORMALIZE_HPP
