// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_OPERATOR_FIXTURE_HPP
#define ZENGINE_TESTS_OPERATOR_FIXTURE_HPP

// A CATALOG WITH ONE PRIMITIVE REPLACED (SEM-0 §11).
//
// It exists to answer the one question a matrix of correct answers cannot:
// whether two consumers spend the SAME definition, or two implementations that
// happen to agree. Two agreeing implementations are indistinguishable from one
// shared definition until the definition CHANGES — so this fixture changes it,
// underneath, in a way nothing structural can notice, and every honest consumer
// of the rule must move together.
//
// The substitution is deliberately invisible to every check the system makes:
// same identity, same port names, same C++ types, therefore the same two
// content ids, therefore admitted by the signature check that exists to catch a
// RESHAPED operator. That is correct — a different implementation of a published
// signature is exactly what a replaced provider is, and refusing it would be
// refusing hot replacement.
//
// It is shared by two suites because the claim has two halves: the operator
// suite asks whether an independent READER moves, and the timer suite asks
// whether a RUNNING WEAVE does.

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
