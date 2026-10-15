// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// A second terminal Skin, a library of its own, so a Skin replaced mid-run has another to be
// replaced by: the same terminal medium as the classic one.

#include "skin.hpp"
#include "skin_tui.hpp"

#include <zen/kernel/export.hpp>

namespace {

using SkinTuiBlock =
    zengine::surface::SkinT<zengine::surface::TuiMedium<zengine::surface::TuiTerminal>>;

} // namespace

ZEN_EXPORT_WEAVE(SkinTuiBlock)
