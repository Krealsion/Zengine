// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The loadable weave `zengine-window-sdl`: the OS window (graphics/window/window_sdl.hpp) over its
// SDL3 backend (graphics/window/window_sdl_backend.hpp). It opens its window when it is made and
// claims SDL's queue while it is loaded, so an SDL Input reader beside it reads none of it.
// Reloaded, it leaves its window to the instance after it. Where it cannot open -- no display, no
// driver SDL can use -- it says why on standard error and refuses to be made, so its load fails
// and nothing of it stays.

#include "graphics/window/window_sdl_backend.hpp"

#include <zen/kernel/export.hpp>

namespace {

using SdlWindow =
    zengine::graphics::window::SdlWindowT<zengine::graphics::window::ProcessSdlBackend>;

} // namespace

ZEN_EXPORT_WEAVE(SdlWindow)
