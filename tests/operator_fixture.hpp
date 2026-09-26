// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_OPERATOR_FIXTURE_HPP
#define ZENGINE_TESTS_OPERATOR_FIXTURE_HPP

// A CATALOG WITH ONE PRIMITIVE REPLACED, for the question a matrix of correct answers cannot
// answer: whether two consumers spend the SAME definition, or two implementations that agree.
// The substitution is invisible to every check -- same identity, ports and types, so the same
// content ids, admitted by the signature check that catches a RESHAPED operator, as a replaced
// provider should be. Two suites share it: one asks whether an independent READER moves, the
// timer suite whether a RUNNING WEAVE does.

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"
#include "timer/normalize.hpp"

#include <cstdint>

namespace zengine::testing {

/// A `math.max` that is a min. Namespace scope, because a block-scope lambda
/// cannot be a `make_operator<&F>` argument at all.
inline std::int64_t saboteur_min(std::int64_t lhs, std::int64_t rhs) {
    return lhs < rhs ? lhs : rhs;
}

/// The standard vocabulary with `math.max` replaced, and `timer.normalize_delay`
/// composed over it BY THE SAME AUTHORING CODE the shipped catalog uses. The
/// graph is byte-identical; only the leaf differs.
inline op::Catalog sabotaged_operators() {
    op::Catalog catalog;
    catalog.publish(op::make_operator<&saboteur_min>(op::kMaxInt, {"lhs", "rhs"}, "result"));
    catalog.publish(op::make_operator<&op::select_int>(
        op::kSelectInt, {"condition", "when_true", "when_false"}, "result"));
    catalog.publish(timer::normalize_delay(catalog));
    return catalog;
}

/// What that substitution does to the rule, worked through once so a reader need
/// not: `max(max(-500, 0), 1)` becomes `min(min(-500, 0), 1)`, so a repeating
/// -500 stops being floored and stays -500.
inline constexpr std::int64_t kSabotagedRepeatingDelay = -500;

} // namespace zengine::testing

#endif // ZENGINE_TESTS_OPERATOR_FIXTURE_HPP
