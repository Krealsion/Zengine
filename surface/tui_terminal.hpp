// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_TUI_TERMINAL_HPP
#define ZENGINE_SURFACE_TUI_TERMINAL_HPP

// A terminal's output side under Surface's spelling: the claim, its modes and each role's ink are
// the terminal window's (graphics/window/terminal_io.hpp), and a terminal Skin claims the terminal
// with them as the window does. Surface law: agents/surface.md

#include "terminal_size.hpp"

namespace zengine::surface {

using zengine::graphics::window::kTuiPointerIsInBand;
using zengine::graphics::window::kTuiPointerOff;
using zengine::graphics::window::kTuiPointerOn;
using zengine::graphics::window::sgr_for_role;
using zengine::graphics::window::tui_enter_sequence;
using zengine::graphics::window::tui_leave_sequence;
using zengine::graphics::window::TuiTerminal;

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_TUI_TERMINAL_HPP
