// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The classic TUI Skin — the terminal medium wearing the classic style.
// Loading it claims the terminal's output side; unloading gives it back.
// Everything it is lives in the headers; this file is the shipping gesture.

#include "skin_tui.hpp"

#include <zen/kernel/export.hpp>

namespace {

using SkinTuiClassic = zengine::surface::SkinT<
    zengine::surface::TuiMedium<zengine::surface::ClassicStyle, zengine::surface::TuiTerminal>>;

} // namespace

ZEN_EXPORT_WEAVE(SkinTuiClassic)
