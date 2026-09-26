// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_NORMALIZE_HPP
#define ZENGINE_TIMER_NORMALIZE_HPP

// What a Timer makes of a delay, the one place the rule lives: `timer.normalize_delay` is a
// composition over two published primitives, with no C++ body, evaluated by the one evaluator
// every consumer uses -- so a second consumer needs no copy of it, and cannot disagree with the
// Timer. A repeating delay below 1 ms becomes 1; a negative delay fires on the next beat
// (docs/reference/timer-protocol.md).
// Timer law: docs/laws/timer-laws.md

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

/// Author the rule against a catalog that already carries the primitives. The output schema is
/// derived as each step resolves; the two input ports are written once, by hand, since a
/// composite has no C++ signature to take them from.
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

/// What this package contributes to a host: its composition, and not one primitive. The catalog
/// inside is authoring scaffolding (signatures to compose against, not ownership of the power)
/// and dies at the brace; the graph names `math.max` and `logic.select_int`, spent from
/// whoever provides them where the rule lands.
inline std::vector<op::OperatorDef> provider_contributions() {
    op::Catalog against;
    op::publish_primitives(against);
    std::vector<op::OperatorDef> defs;
    defs.push_back(normalize_delay(against));
    return defs;
}

/// The no-host arrangement's vocabulary: the basic primitives plus this composition, assembled
/// locally because nobody claims semantic authority here (`snake`, a plain Loom, any program
/// without the operator seam). A value, never a registry, and never a host's catalog.
inline op::Catalog fallback_vocabulary() {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    catalog.publish(normalize_delay(catalog));
    return catalog;
}

/// The ask, spelled once for both doors, against the input schema the authority answered with.
inline loom::Value normalize_ask(const std::shared_ptr<const loom::Schema>& inputs,
                                 std::int64_t delay_ms, bool repeat) {
    loom::Value ask(inputs);
    ask.set(kAuthoredDelayPort, loom::Cell::integer(delay_ms));
    ask.set(kRepeatPort, loom::Cell::boolean(repeat));
    return ask;
}

/// Spell the ask, run it, read the answer: this calls the rule and knows nothing of what it
/// says. Total for a catalog that accepted the rule; one that never carried it is an authoring
/// mistake, refused loudly, since inventing a delay would be a second copy of the rule.
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

/// Which semantic authority a Timer spends: one of two, chosen once by the constructor and fixed
/// for its life. Host-backed: every normalization crosses to the host's catalog, resolved at the
/// call, and no catalog is held here. Local fallback: the vocabulary this repository authors, a
/// supported arrangement and not a degraded one. A host-backed Timer never falls back: `local_`
/// is empty in that mode, a host that cannot serve the rule is refused at construction, and a
/// later failure throws.
class DelayAuthority {
public:
    /// LOCAL-FALLBACK over the vocabulary this repository authors.
    DelayAuthority() : local_(fallback_vocabulary()) {}

    /// Local fallback over a catalog the caller chose: how a suite replaces a primitive beneath
    /// the rule and watches a running weave and an independent reader move together.
    explicit DelayAuthority(op::Catalog local) : local_(std::move(local)) {}

    /// What a loaded Timer does with what it was offered. An unbound host means local fallback --
    /// nothing was supplied -- never recovery from a host that failed. A bound host must publish
    /// `timer.normalize_delay` at the signature this Timer was authored against (`same_identity`,
    /// against `normalize_delay`'s own authoring). Otherwise it throws, and the throw is the
    /// refusal: `create()` returns null and the Kernel refuses the load.
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

    /// The catalog this Timer carries, in local fallback only; a host-backed one carries none.
    const op::Catalog& operators() const {
        if (!local_) {
            throw std::invalid_argument(
                "this Timer is host-backed: its operator truth lives in the host's catalog, "
                "and there is no local one to hand out");
        }
        return *local_;
    }

    /// What this Timer makes of an authored delay: both branches spend `timer.normalize_delay`
    /// and differ only in where the identity resolves.
    std::int64_t effective_delay(std::int64_t delay_ms, bool repeat) const {
        if (local_) {
            return timer::effective_delay(*local_, delay_ms, repeat);
        }
        // Resolved at spend, on the far side: `contract_` is an identity and two schemas, never a
        // handle into the host, so a rule that changed underneath is spent as it is now.
        const op::HostAnswer answer =
            host_.evaluate(contract_, normalize_ask(contract_.inputs, delay_ms, repeat));
        if (!answer.ok()) {
            // Unreachable while the host's catalog outlives this Timer and cannot be edited; it
            // throws, since the other spelling of this branch is the silent fallback.
            throw std::invalid_argument("'" + std::string(kNormalizeDelay) +
                                        "' was refused by this Timer's operator host: " +
                                        answer.reason);
        }
        return answer.value->at(0)->as_int();
    }

private:
    /// Exactly one of these is engaged: host-backed leaves `local_` empty, so no later edit can
    /// reach a local catalog by accident.
    std::optional<op::Catalog> local_;
    op::OperatorHost host_;
    op::HostSignature contract_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_NORMALIZE_HPP
