// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The basic operator provider: the scalar primitives `operator/primitives.hpp` authors, offered
// to whoever hosts them. A provider and not a weave: it exports `zengine_operator_provider` and
// no `zen_weave_abi`, so no Kernel loads it, it has no WeaveId, role, grant or manifest, and it
// never sees a bus; a host opens it, reads its definitions and holds it while they are
// installed. It supplies no second authoring of anything; it says who provides them.
// Operator law: agents/operators.md

#include "operator/primitives.hpp"
#include "operator/provider.hpp"

#include <vector>

namespace {

/// The powers this artifact supplies, authored by the package that owns them.
std::vector<zengine::op::OperatorDef> basic_operators() {
    return zengine::op::primitive_definitions();
}

} // namespace

/// The provider's logical identity: what a host mounts, unmounts and reports as the active
/// supplier. Not part of an operator's identity -- `math.max` is `math.max` whoever supplies
/// it -- so another provider can supply it later without a composition being rewritten.
ZENGINE_OPERATOR_PROVIDER("zengine.operators.basic", basic_operators)
