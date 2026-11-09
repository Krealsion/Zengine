// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_OWN_HPP
#define ZENGINE_GRAPHICS_OWN_HPP

// A pane's own values: each declared where it is used, by name with its default (`own()`, in
// graphics/color.hpp and graphics/text_style.hpp), and drawn by that name. A draw carries the
// default beside the name, so a window paints an own value no theme sets without being told; the
// declaration below is what a pane says to whoever offers palettes, so a theme naming its own
// values is judged. Reference: graphics/docs/graphics.md, "A pane's own values".

#include "graphics/color.hpp"
#include "graphics/text_style.hpp"
#include "graphics/vocabulary.hpp"

#include <initializer_list>

namespace zengine::graphics {

/// An own colour as its pane declares it.
inline OwnDeclared declared(const Color& c) {
    OwnDeclared d{c.name, own_kind::kColor, {}, 0, 0};
    if (c.from == color_from::kOwnPool) {
        d.default_name = c.like;
    } else {
        d.rgba = c.rgba;
    }
    return d;
}

/// An own size as its pane declares it.
inline OwnDeclared declared(const TextSize& s) {
    OwnDeclared d{s.name, own_kind::kSize, {}, 0, 0};
    if (s.from == size_from::kOwnPool) {
        d.default_name = s.like;
    } else {
        d.points = s.points;
    }
    return d;
}

/// An own style as its pane declares it.
inline OwnDeclared declared(const TextLook& l) {
    return OwnDeclared{l.style.name, own_kind::kStyle, l.style.like, 0, 0};
}

/// EVERY OWN VALUE A PANE DECLARES, as it says them to whoever offers palettes.
inline OwnValuesDeclared declared(std::initializer_list<Color> colors,
                                  std::initializer_list<TextSize> sizes = {},
                                  std::initializer_list<TextLook> styles = {}) {
    OwnValuesDeclared out;
    for (const Color& c : colors) {
        out.values.push_back(declared(c));
    }
    for (const TextSize& s : sizes) {
        out.values.push_back(declared(s));
    }
    for (const TextLook& l : styles) {
        out.values.push_back(declared(l));
    }
    return out;
}

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_OWN_HPP
