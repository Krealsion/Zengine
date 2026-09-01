// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE WORKSHOP SESSION HISTORY, AS AN ARTIFACT (MIG-0) — the conversions that bring an
// older session file forward to the shape this Workshop admits.
//
// IT IS A PROVIDER AND IT IS NOT A WEAVE, on `zengine-operators-basic`'s own terms: it
// exports `zengine_operator_provider` and no `zen_weave_abi` at all, so no Kernel loads it,
// it gets no WeaveId, no role, no grant, no manifest and no line in `zen.ListLoaded`, and
// it never sees a bus. A host opens it, reads its definitions out of it, and holds it for
// as long as those definitions are installed.
//
// WHAT IT PROVES THAT THE BASIC PROVIDER CANNOT. `zengine-operators-basic` supplies powers
// a maker composes with. This one supplies powers a DURABLE OWNER spends, on bytes nobody
// in this process wrote — and it is the measurement behind the sentence MIG-0 exists for:
//
//     Workshop's session reader no longer contains a single line about what a session
//     used to look like, and yesterday's files still open.
//
// If this artifact is not mounted, they do not open, and Workshop says so. That is the
// whole authority story, stated as an artifact rather than as a rule: a version claim can
// select among conversions a host already has, and it cannot cause this file to be loaded.
//
// THERE IS NO SEMANTICS IN THIS FILE. The shapes, the translations and the edge
// declarations are `workshop/session_history.hpp`'s, unmodified; this file merely SAYS WHO
// PROVIDES THEM — the same subtraction `operator/basic_provider.cpp` makes next door.

#include "operator/provider.hpp"
#include "session_history.hpp"

#include <vector>

namespace {

/// The conversions this artifact supplies, authored by the header that owns them.
std::vector<zengine::op::OperatorDef> session_conversions() {
    return zengine::workshop::session_history::conversions();
}

} // namespace

/// The provider's logical identity — what a host mounts, unmounts, and reports as the
/// active supplier of a conversion. It is NOT part of an edge's identity: a `WorkshopSession
/// v1 -> v4` conversion is that conversion whoever supplies it, which is exactly what lets
/// somebody else supply it later without the session reader being rewritten.
///
/// ⚠ AND THE ROW IN A LOAD PLAN NAMES THIS ARTIFACT AND NOT AN EDGE, which is why WUX-10
/// added a third conversion without touching a plan: what an arrangement authorizes is this
/// supplier, and what it supplies is the header's business.
ZENGINE_OPERATOR_PROVIDER("zengine.workshop.session_history", session_conversions)
