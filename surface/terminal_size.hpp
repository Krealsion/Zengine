// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_TERMINAL_SIZE_HPP
#define ZENGINE_SURFACE_TERMINAL_SIZE_HPP

// How big a terminal is, in character cells, under Surface's spelling: the question and its
// answer are the terminal window's (graphics/window/terminal_io.hpp), which a terminal Skin asks
// on its beat as the window does. Reference: surface/docs/surface.md.

#include "graphics/window/terminal_io.hpp"

namespace zengine::surface {

using zengine::graphics::window::native_terminal_size;
using zengine::graphics::window::TerminalSize;

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_TERMINAL_SIZE_HPP
