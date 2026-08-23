// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE BASIC OPERATOR PROVIDER (PROV-0) — `math.max` and `logic.select_int`, offered
// to whoever hosts them.
//
// IT IS A PROVIDER AND IT IS NOT A WEAVE, and that is the whole of what this file
// exists to demonstrate in production. It exports one symbol,
// `zengine_operator_provider`, and no `zen_weave_abi` at all: no Kernel loads it, it
// gets no WeaveId, no role, no grant, no manifest and no row in `zen.ListLoaded`,
// and it never sees a bus. A host opens it, reads two definitions out of it, and
// holds it for as long as those definitions are installed.
//
//     provider != weave
//
// That inequality could have been asserted and is instead measured: this artifact
// is mounted by the shipped Workshop and by the suite, and it counterfeits nothing.
// The Timer, next door, exports all three surfaces from one image and is a
// provider, a consumer and a participant at once -- three independent
// relationships, and this file is the proof that the first does not require the
// third.
//
// WHY THESE TWO AND NOT MORE. Because these two are what an existing composition
// actually consumes. `timer.normalize_delay` is `max(max(delay, 0), 1)` chosen
// against `repeat`, so the powers it needs are the larger of two integers and a
// choice between two integers -- and a provider grown against an imagined consumer
// is how a vocabulary acquires operators nothing has ever composed. The definitions
// themselves are `operator/primitives.hpp`'s, unmodified: this file supplies no
// second authoring of anything, it merely SAYS WHO PROVIDES IT.

#include "operator/primitives.hpp"
#include "operator/provider.hpp"

#include <vector>

namespace {

/// The powers this artifact supplies, authored by the package that owns them.
std::vector<zengine::op::OperatorDef> basic_operators() {
    return zengine::op::primitive_definitions();
}

} // namespace

/// The provider's logical identity — what a host mounts, unmounts, and reports as
/// the active supplier of a power. It is NOT part of an operator's identity:
/// `math.max` is `math.max` whoever supplies it, which is exactly what lets
/// somebody else supply it later without every composition being rewritten.
ZENGINE_OPERATOR_PROVIDER("zengine.operators.basic", basic_operators)
