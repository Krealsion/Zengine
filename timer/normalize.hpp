// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_NORMALIZE_HPP
#define ZENGINE_TIMER_NORMALIZE_HPP

// The rule for the delay a Timer schedules, and the one place it is written. `timer.normalize_delay`
// is an operator built from two published primitives, with no C++ body of its own, and the one
// evaluator every user of the rule calls runs it. So a second user of the rule, such as a tool that
// previews a delay, needs no copy of it and cannot disagree with the Timer. A repeating delay below
// 1 ms becomes 1, and a negative delay fires at the next wake (`timer/docs/zengine.timer.md`,
// About).

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

/// The rule's name and its three port names, each written once, because a caller names them and
/// a suite asserts them, and two spellings of one port are how a caller and the rule stop meaning
/// the same thing.
inline constexpr const char* kNormalizeDelay = "timer.normalize_delay";
inline constexpr const char* kAuthoredDelayPort = "delay_ms";
inline constexpr const char* kRepeatPort = "repeat";
inline constexpr const char* kEffectiveDelayPort = "effective_delay";

/// Builds the rule against a catalog that already holds the primitives. The output's schema is
/// worked out as each step resolves. The two input ports are written out here, because a rule
/// built from steps has no C++ signature to take them from.
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

    return std::move(rule).result(kEffectiveDelayPort, effective,
                                  "the delay a Timer schedules for an authored delay: never "
                                  "negative, and at least 1 ms when it repeats");
}

/// What this package supplies to a host: the rule, and none of the primitives it is built from.
/// The catalog made here only gives the rule signatures to build against, and it ends with the
/// function. The rule names `math.max` and `logic.select_int`, and wherever it is installed it
/// calls whichever weave provides those there.
inline std::vector<op::OperatorDef> provider_contributions() {
    op::Catalog against;
    op::publish_primitives(against);
    std::vector<op::OperatorDef> defs;
    defs.push_back(normalize_delay(against));
    return defs;
}

/// The catalog a Timer uses when no host offers one: the basic primitives and this rule, put
/// together here, as in the snake example, a plain Loom, or any program without an operator host.
/// It is a value the Timer owns, never a shared registry, and never a host's catalog.
inline op::Catalog fallback_vocabulary() {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    catalog.publish(normalize_delay(catalog));
    return catalog;
}

/// The rule's input for one delay, written once for both ways of running it, against the input
/// schema that the catalog or the host described.
inline loom::Value normalize_ask(const std::shared_ptr<const loom::Schema>& inputs,
                                 std::int64_t delay_ms, bool repeat) {
    loom::Value ask(inputs);
    ask.set(kAuthoredDelayPort, loom::Cell::integer(delay_ms));
    ask.set(kRepeatPort, loom::Cell::boolean(repeat));
    return ask;
}

/// Runs the rule on one delay and returns its answer; this function knows nothing of what the rule
/// says. It always answers for a catalog that holds the rule. A catalog without it is a mistake in
/// the program, and it throws, because making up a delay here would be a second copy of the rule.
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

/// Whose copy of the delay rule a Timer uses: one of two, chosen once by the constructor and kept
/// for the Timer's life. Backed by a host, every delay is sent to the host's catalog and the rule
/// is looked up there at each call, and no catalog is kept here. Local, the Timer uses the catalog
/// this repository builds, which is a supported arrangement and not a lesser one. A host-backed
/// Timer never falls back to the local catalog: `local_` is empty then, a host that cannot serve
/// the rule is refused in the constructor, and a later failure throws.
class DelayAuthority {
public:
    /// A local Timer, using the catalog this repository builds.
    DelayAuthority() : local_(fallback_vocabulary()) {}

    /// A local Timer over a catalog the caller chose. A suite uses it to replace a primitive under
    /// the rule and see a running Timer and a separate reader of the rule change together.
    explicit DelayAuthority(op::Catalog local) : local_(std::move(local)) {}

    /// What a loaded Timer does with the operator host it was offered. An empty offer means no
    /// host was supplied, so the Timer is local; it never means recovery from a host that failed.
    /// A real host must publish `timer.normalize_delay` with the signature this Timer was built
    /// against, compared with `same_identity` to the signature `normalize_delay` builds. Otherwise
    /// the constructor throws, and the throw is the refusal: `create()` returns null and the Kernel
    /// refuses the load.
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

    /// Which of the two this is. It is for diagnosis only: nothing about the answer changes which
    /// rule `effective_delay` runs, and a case must not prove which rule runs by reading it.
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

    /// The delay this Timer schedules for the delay a weave asked for. Both branches run
    /// `timer.normalize_delay`, and they differ only in where the rule is looked up.
    std::int64_t effective_delay(std::int64_t delay_ms, bool repeat) const {
        if (local_) {
            return timer::effective_delay(*local_, delay_ms, repeat);
        }
        // The rule is looked up in the host at each call: `contract_` holds a name and two
        // schemas, never a pointer into the host, so a rule the host changed is run as it is now.
        const op::HostAnswer answer =
            host_.evaluate(contract_, normalize_ask(contract_.inputs, delay_ms, repeat));
        if (!answer.ok()) {
            // This cannot happen while the host's catalog outlives the Timer and cannot be edited.
            // It throws, because the only other way to write this branch is a silent fallback.
            throw std::invalid_argument("'" + std::string(kNormalizeDelay) +
                                        "' was refused by this Timer's operator host: " +
                                        answer.reason);
        }
        return answer.value->at(0)->as_int();
    }

private:
    /// Exactly one of the two is in use. A host-backed Timer leaves `local_` empty, so no later
    /// change to this class can reach a local catalog by accident.
    std::optional<op::Catalog> local_;
    op::OperatorHost host_;
    op::HostSignature contract_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_NORMALIZE_HPP
