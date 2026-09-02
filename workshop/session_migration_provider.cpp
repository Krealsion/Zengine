// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE WORKSHOP SESSION HISTORY, AS AN ARTIFACT — the conversions that bring an
// older session file forward to the shape this Workshop admits.
// Workshop law: agents/workshop/migration.md

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
/// active supplier of a conversion.
// WL-MIG-01 -- agents/workshop/migration.md
ZENGINE_OPERATOR_PROVIDER("zengine.workshop.session_history", session_conversions)
