// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The classic skin, `zengine-paint-classic`: painter code for a terminal window. One character a
// cell, an ASCII border, every cell in the terminal's own ink.

#include "graphics/painter_export.hpp"

namespace {

namespace gfx = zengine::graphics;

struct Classic {
    static constexpr const char* kName = "classic";
    static gfx::term::Look look() {
        gfx::term::Look l;
        l.fill = {{gfx::role::kFill, 'o'},
                  {gfx::role::kAccent, 'O'},
                  {gfx::role::kMuted, '.'},
                  {gfx::role::kAlert, '*'},
                  {gfx::role::kGround, ' '}};
        l.mono = true;
        return l;
    }
};

} // namespace

ZENGINE_EXPORT_PAINTER(Classic)
