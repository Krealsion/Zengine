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
    if (c.none() || c.from == color_from::kPool) {
        return c;
    }
    return c.like.empty() ? Color(colors::text) : Color{color_from::kPool, c.like, 0, {}};
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
