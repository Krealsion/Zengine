// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The block TUI Skin: the terminal medium wearing the block style. A library of its own, so a
// swap to it mid-game is unmistakable.

#include "skin_tui.hpp"

#include <zen/kernel/export.hpp>

namespace {

using SkinTuiBlock = zengine::surface::SkinT<
    zengine::surface::TuiMedium<zengine::surface::BlockStyle, zengine::surface::TuiTerminal>>;

} // namespace

ZEN_EXPORT_WEAVE(SkinTuiBlock)
