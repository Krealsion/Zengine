// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_COLOR_HPP
#define ZENGINE_GRAPHICS_COLOR_HPP

// A colour a draw carries: an exact value, a name from the shared pool, or a slot the drawing's
// pane exposes. The window turns a name into a colour when it paints, from the palette the area
// wears; nothing before it knows what a name looks like. The names are C++ names generated from
// JSON (cmake/ZengineNames.cmake): the pool's are `zengine::graphics::colors`, generated from
// graphics/pool.json, and a pane's slots are its own namespace's, so a name nobody declared does
// not build. Reference: graphics/docs/graphics.md, "Colours by name".

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace zengine::graphics {

/// Where a colour comes from.
namespace color_from {
inline constexpr std::int64_t kNone = 0;  ///< no paint at all
inline constexpr std::int64_t kExact = 1; ///< `rgba`, as it is
inline constexpr std::int64_t kPool = 2;  ///< the shared pool's `name`, as the area's palette says
/// The slot `name` the area's owner exposes, as the area's palette says for that owner; `rgba`
/// where the palette sets none.
inline constexpr std::int64_t kSlot = 3;
} // namespace color_from

/// A COLOUR A DRAW CARRIES: none, an exact 0xRRGGBBAA, a pool colour or a slot, by name. `like`
/// names the pool colour a medium that cannot show an exact colour or a slot's own shows in its
/// place (a terminal's ink and fill character); "" leaves the medium its nearest.
struct Color {
    std::int64_t from = color_from::kNone;
    std::string name;
    std::int64_t rgba = 0;
    std::string like;
    ZEN_SHAPE(Color, 1, ZEN_FIELD(from), ZEN_FIELD(name), ZEN_FIELD(rgba), ZEN_FIELD(like));

    bool none() const { return from == color_from::kNone; }
    friend bool operator==(const Color&, const Color&) = default;
};

/// An exact colour, opaque: 0xRRGGBB.
inline Color rgb(std::uint32_t value) {
    return Color{color_from::kExact, {}, static_cast<std::int64_t>((value & 0xffffffu) << 8 | 0xffu),
                 {}};
}

/// An exact colour with its alpha: 0xRRGGBBAA.
inline Color rgba(std::uint32_t value) {
    return Color{color_from::kExact, {}, static_cast<std::int64_t>(value), {}};
}

/// No paint at all.
inline Color no_color() { return Color{}; }

/// A NAME FROM THE SHARED POOL, as the pool's generated header declares it
/// (`zengine::graphics::colors::accent`). It is the name and nothing else.
struct PoolColor {
    const char* name;

    operator Color() const { return Color{color_from::kPool, name, 0, {}}; }
    friend bool operator==(const PoolColor& a, const PoolColor& b) {
        return std::string_view(a.name) == std::string_view(b.name);
    }
};

/// An exact colour, opaque, 0xRRGGBB, and the pool colour a medium that cannot show it shows in
/// its place.
inline Color rgb(std::uint32_t value, PoolColor like) {
    Color c = rgb(value);
    c.like = like.name;
    return c;
}

/// A SLOT A PANE EXPOSES, as its own generated header declares it: its name, the colour it paints
/// where no theme sets it (0xRRGGBBAA), and the pool colour a medium that cannot show that colour
/// shows instead.
struct SlotColor {
    const char* name;
    std::uint32_t rgba;
    const char* like;

    operator Color() const {
        return Color{color_from::kSlot, name, static_cast<std::int64_t>(rgba), like};
    }
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_COLOR_HPP
