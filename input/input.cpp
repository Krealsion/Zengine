// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Input weave library for the terminal and the Win32 console: only the platform edge. Each
// reader (terminal_reader.hpp) fetches native events and hands them to the pure translators in
// translate.hpp; the weave (input_weave.hpp) publishes what comes back. A reader owns the
// platform's input-side state for exactly the weave's lifetime: construction engages it,
// destruction restores it. Surface law: agents/surface.md

// The output side (alternate screen, VT processing) is the Skin's ground. With no console
// (redirected stdin, headless ctest, a pipe) the reader stays disabled and poll() yields nothing.
// The SDL reader is a separate artifact (input_sdl.cpp).

#include "input_weave.hpp"
#include "terminal_reader.hpp"

#include <zen/kernel/export.hpp>

namespace {

using InputWeave = zengine::input::InputWeaveT<zengine::input::PlatformReader>;

} // namespace

ZEN_EXPORT_WEAVE(InputWeave)
