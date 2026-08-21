// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_PRIMITIVES_HPP
#define ZENGINE_OPERATOR_PRIMITIVES_HPP

// THE PRIMITIVE VOCABULARY, AND IT IS TWO POWERS (SEM-0).
//
// A primitive leaf is an ordinary C++ function. Nothing about the shape of these
// two is operator-flavoured -- no context, no registration macro, no base class,
// no return channel -- and that is the point: composition consumes existing
// power, and native code introduces only power the vocabulary genuinely lacks.
//
// WHAT FORCED EACH ONE. The Timer's delay rule is
//
//     effective = repeat ? max(max(delay_ms, 0), 1) : max(delay_ms, 0)
//
// and the only two things in it that cannot be spelled with something already
// published are TAKING THE LARGER OF TWO INTEGERS and CHOOSING BETWEEN TWO
// INTEGERS ON A CONDITION. Zengine had no operator vocabulary at all before this
// phase, so both are new; neither is irreducible in the abstract, but each is
// irreducible in the only sense that matters here -- remove it and the rule
// cannot be written as a composition at all.
//
// WHAT IS DELIBERATELY NOT HERE. `math.min`, `math.clamp`, `logic.and`,
// `logic.or`, `logic.not`, `compare.greater_than`, `compare.equals`, a Float
// max, a Text select. A future logic system will want most of them and none of
// them is wanted YET, and a vocabulary grown against an imagined consumer is how
// a package acquires operators nothing has ever composed. `math.clamp` in
// particular would be the tempting one and it is the wrong one: it would make
// the Timer's rule a SINGLE native call, which is exactly the shape §6 forbids,
// because a rule that is one primitive proves registration and proves nothing
// about composition.
//
// WHY HERE AND NOT IN timer/. They are called `math.max` and `logic.select_int`,
// not `timer.something`: filing arithmetic inside the Timer package would make
// the Timer the owner of a power that is nobody's in particular, and the second
// consumer would then either reach into the Timer or write its own.
//
// PURITY IS A CONSTRAINT ON THE DOOR, NOT ON THE ROOM. Registration by value
// removes the ARGUMENT path -- an operator cannot be HANDED a Bus, a Kernel or a
// Workshop -- and it does not remove the AMBIENT one: native C++ in the same
// image can still read a global. No hostile-code claim is implied or possible.

#include "operator/catalog.hpp"
#include "operator/operator.hpp"

#include <algorithm>
#include <cstdint>

namespace zengine::op {

/// The larger of two integers.
inline std::int64_t max_int(std::int64_t lhs, std::int64_t rhs) { return std::max(lhs, rhs); }

/// One of two integers, chosen by a condition.
inline std::int64_t select_int(bool condition, std::int64_t when_true, std::int64_t when_false) {
    return condition ? when_true : when_false;
}

inline constexpr const char* kMaxInt = "math.max";
inline constexpr const char* kSelectInt = "logic.select_int";

/// Publish the primitive leaves into a catalog. Two calls, and the only thing
/// authored in either is the identity and the port names.
inline void publish_primitives(Catalog& catalog) {
    catalog.publish(make_operator<&max_int>(kMaxInt, {"lhs", "rhs"}, "result"));
    catalog.publish(make_operator<&select_int>(kSelectInt,
                                               {"condition", "when_true", "when_false"}, "result"));
}

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_PRIMITIVES_HPP
