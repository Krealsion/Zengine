// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_OPS_HPP
#define ZENGINE_GRAPHICS_OPS_HPP

// What a painter reads of a node's look (graphics/operands.hpp): the colours it carries, and the
// pool colour a colour shows as where only the pool's are shown.

#include "graphics/color.hpp"
#include "graphics/colors.hpp" // generated from graphics/pool.json
#include "graphics/operands.hpp"
#include "graphics/vocabulary.hpp"

#include <type_traits>
#include <variant>
#include <vector>

namespace zengine::graphics {

/// THE POOL COLOUR A COLOUR SHOWS AS where only the pool's colours are shown: a pool colour as
/// itself, an exact colour or a slot as the pool colour it is `like`, or `text` where it names
/// none; no paint stays no paint.
inline Color pool_color_of(const Color& c) {
    const std::string* like = nullptr;
    if (const ColorExact* e = std::get_if<ColorExact>(&c.is)) {
        like = &e->like;
    } else if (const ColorOwn* o = std::get_if<ColorOwn>(&c.is)) {
        like = &o->like;
    } else if (const ColorOwnPool* p = std::get_if<ColorOwnPool>(&c.is)) {
        like = &p->like;
    } else {
        return c;
    }
    return like->empty() ? Color(colors::text) : Color(ColorPool{*like});
}

/// The colours a look carries, in the order a painter takes them: a paint's fill then stroke, an
/// ink's ink, a text's ink.
inline std::vector<Color> colors_of(const Look& look) {
    return std::visit(
        [](const auto& l) -> std::vector<Color> {
            using L = std::decay_t<decltype(l)>;
            if constexpr (std::is_same_v<L, Paint>) {
                return {l.fill, l.stroke};
            } else {
                return {l.ink};
            }
        },
        look.is);
}

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_OPS_HPP
