// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The day skin, `zengine-paint-day`: painter code for a window of light. Dark ink on a light
// ground, every operator in exact colours (graphics/light_paint.hpp).

#include "graphics/painter_export.hpp"

namespace {

namespace gfx = zengine::graphics;

struct Day {
    static constexpr const char* kName = "day";
    static gfx::light::Look look() { return gfx::light::day_look(); }
};

} // namespace

ZENGINE_EXPORT_LIGHT_PAINTER(Day)
