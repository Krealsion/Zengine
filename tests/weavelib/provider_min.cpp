// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A provider supplying `math.max` as a min: whether the real Timer's rule and a loaded stranger
// spend one contribution or two that agree cannot be told until the power changes, so this
// artifact changes it from outside. Same identity, port names and types, so the same content ids
// and an overlay the compatibility rule accepts, as it must: a replaced provider is exactly this.
// The host mounts it over the basic provider at run time and unmounts it, and the Timer that
// moves is neither rebuilt nor told. Not a weave; it supplies one power.

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
