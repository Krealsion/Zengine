// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The night skin, `zengine-paint-night`: painter code for a window of light. Light ink on a dark
// ground, every operator in exact colours (graphics/light_paint.hpp).

#include "graphics/painter_export.hpp"

namespace {

namespace gfx = zengine::graphics;

struct Night {
    static constexpr const char* kName = "night";
    static gfx::light::Look look() { return gfx::light::night_look(); }
};

} // namespace

ZENGINE_EXPORT_LIGHT_PAINTER(Night)
