// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The block skin, `zengine-paint-block`: painter code for a terminal window, deliberately unlike
// the classic one so a choice of skin mid-run is unmistakable -- solid glyphs, a rule of '='
// for every edge, and each role in its own ink.

#include "graphics/painter_export.hpp"

namespace {

namespace gfx = zengine::graphics;

struct Block {
    static constexpr const char* kName = "block";
    static gfx::term::Look look() {
        gfx::term::Look l;
        l.fill = {{gfx::role::kFill, '#'},
                  {gfx::role::kAccent, '@'},
                  {gfx::role::kMuted, ':'},
                  {gfx::role::kAlert, '%'},
                  {gfx::role::kGround, ' '}};
        l.corner = '=';
        l.across = '=';
        l.down = '=';
        return l;
    }
};

} // namespace

ZENGINE_EXPORT_PAINTER(Block)
