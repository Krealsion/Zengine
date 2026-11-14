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
    if (const ColorOwnPool* o = std::get_if<ColorOwnPool>(&c.is)) {
        return OwnDeclared{o->name, own_kind::kColor, o->like, 0, 0};
    }
    if (const ColorOwn* o = std::get_if<ColorOwn>(&c.is)) {
        return OwnDeclared{o->name, own_kind::kColor, {}, o->rgba, 0};
    }
    return OwnDeclared{{}, own_kind::kColor, {}, 0, 0};
}

/// An own size as its pane declares it.
inline OwnDeclared declared(const TextSize& s) {
    if (const SizeOwnPool* o = std::get_if<SizeOwnPool>(&s.is)) {
        return OwnDeclared{o->name, own_kind::kSize, o->like, 0, 0};
    }
    if (const SizeOwn* o = std::get_if<SizeOwn>(&s.is)) {
        return OwnDeclared{o->name, own_kind::kSize, {}, 0, o->points};
    }
    return OwnDeclared{{}, own_kind::kSize, {}, 0, 0};
}

/// An own style as its pane declares it.
inline OwnDeclared declared(const TextLook& l) {
    if (const StyleOwn* o = std::get_if<StyleOwn>(&l.style.is)) {
        return OwnDeclared{o->name, own_kind::kStyle, o->like, 0, 0};
    }
    return OwnDeclared{{}, own_kind::kStyle, {}, 0, 0};
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
