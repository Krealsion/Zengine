// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A PROVIDER THAT SUPPLIES `math.max` AS A MIN (PROV-0 §21) — SEM-0's instrument,
// now an artifact.
//
// It answers the one question a matrix of correct answers cannot: whether the real
// Timer's rule and a loaded stranger spend the SAME contribution, or two
// implementations that happen to agree. Two agreeing implementations are
// indistinguishable from one shared power until the power CHANGES -- so this
// artifact changes it, from outside, in a way nothing structural can notice.
//
// THE SUBSTITUTION IS DELIBERATELY INVISIBLE to every check the system makes: same
// identity, same port names, same C++ types, therefore the same two content ids,
// therefore accepted by the compatibility rule that exists to catch a RESHAPED
// power. That is correct rather than a hole -- a different implementation of a
// published signature is exactly what a replaced provider IS, and refusing it would
// be refusing replacement itself.
//
// SEM-0 and CAT-0 did this in-process, by handing a host a differently-built
// catalog. What PROV-0 changes is that nobody builds a catalog: the host mounts
// this file over the basic provider, at run time, with an explicit overlay, and
// unmounts it again -- and the Timer that moves was neither rebuilt nor told.
//
// IT IS NOT A WEAVE and it supplies exactly one power. Nothing here knows what a
// Timer is, what a millisecond is, or that a composition exists.

#include "operator/operator.hpp"
#include "operator/primitives.hpp"
#include "operator/provider.hpp"

#include <cstdint>
#include <vector>

namespace {

/// A `math.max` that is a min. Namespace scope, because a block-scope lambda cannot
/// be a `make_operator<&F>` argument at all.
std::int64_t smaller(std::int64_t lhs, std::int64_t rhs) { return lhs < rhs ? lhs : rhs; }

std::vector<zengine::op::OperatorDef> substitute() {
    std::vector<zengine::op::OperatorDef> defs;
    // THE PORT NAMES ARE THE ONES `operator/primitives.hpp` AUTHORS, and they have
    // to be: a schema's content id is over its name, version and fields, so a
    // different port name here would be a different signature and the overlay would
    // be refused for the right reason at the wrong time.
    defs.push_back(zengine::op::make_operator<&smaller>(zengine::op::kMaxInt, {"lhs", "rhs"},
                                                        "result"));
    return defs;
}

} // namespace

ZENGINE_OPERATOR_PROVIDER("zengine.operators.test.min", substitute)
