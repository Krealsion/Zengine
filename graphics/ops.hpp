// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_OPS_HPP
#define ZENGINE_GRAPHICS_OPS_HPP

// What a painter reads of a node's look and operands (graphics/operands.hpp): the colours they
// carry, and the pool colour a colour shows as where only the pool's are shown.

#include "graphics/color.hpp"
#include "graphics/colors.hpp" // generated from graphics/pool.json
#include "graphics/operands.hpp"
#include "graphics/vocabulary.hpp"

#include <cstdint>
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

/// THE COLOURS AN OPERATOR'S OPERANDS CARRY, in the order a painter takes them: a sheet's frames'
/// each ink then ground, frame by frame; points' colours in their order; none for the rest, whose
/// colours are their look's.
inline std::vector<Color> colors_of(const Operands& draw) {
    std::vector<Color> out;
    if (const auto* s = std::get_if<SheetArgs>(&draw.is)) {
        out.reserve(s->frames.size() * 2);
        for (const Frame& f : s->frames) {
            out.push_back(f.ink);
            out.push_back(f.ground);
        }
    } else if (const auto* p = std::get_if<PointsArgs>(&draw.is)) {
        out = p->colors;
    }
    return out;
}

/// Where a sheet's frame's ink and ground stand among the colours its operands carry
/// (`colors_of(const Operands&)`), the frame counted from 1.
inline std::int64_t frame_ink_at(std::int64_t frame) { return (frame - 1) * 2; }
inline std::int64_t frame_ground_at(std::int64_t frame) { return (frame - 1) * 2 + 1; }

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_OPS_HPP
