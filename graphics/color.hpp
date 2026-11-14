// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_COLOR_HPP
#define ZENGINE_GRAPHICS_COLOR_HPP

// A colour a draw carries: an exact value, a name from the shared pool, or an own value the
// drawing's pane declares where it uses it. The window turns a name into a colour when it paints,
// from the palette the area wears; nothing before it knows what a name looks like. The pool's names
// are C++ names generated from JSON (cmake/ZengineNames.cmake), `zengine::graphics::colors` from
// graphics/pool.json, so a name nobody declared does not build; a pane declares its own with
// `own()` (graphics/own.hpp). Reference: graphics/docs/graphics.md, "Colours by name".

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace zengine::graphics {

/// No paint at all.
struct ColorNone {
    friend bool operator==(const ColorNone&, const ColorNone&) = default;
    ZEN_SHAPE(ColorNone, 1);
};

/// An exact colour, 0xRRGGBBAA, as it is; `like` the pool colour a medium that cannot show it (a
/// terminal's ink and fill character) shows in its place, "" leaving the medium its nearest.
struct ColorExact {
    std::int64_t rgba = 0;
    std::string like;
    friend bool operator==(const ColorExact&, const ColorExact&) = default;
    ZEN_SHAPE(ColorExact, 1, ZEN_FIELD(rgba), ZEN_FIELD(like));
};

/// The shared pool's colour `name`, as the area's palette says.
struct ColorPool {
    std::string name;
    friend bool operator==(const ColorPool&, const ColorPool&) = default;
    ZEN_SHAPE(ColorPool, 1, ZEN_FIELD(name));
};

/// The own value `name` the area's owner declares, as the area's palette says for that owner; else
/// its default, the exact `rgba`, shown where a medium cannot show it as the pool colour `like`.
struct ColorOwn {
    std::string name;
    std::int64_t rgba = 0;
    std::string like;
    friend bool operator==(const ColorOwn&, const ColorOwn&) = default;
    ZEN_SHAPE(ColorOwn, 1, ZEN_FIELD(name), ZEN_FIELD(rgba), ZEN_FIELD(like));
};

/// The own value `name` the area's owner declares, as the area's palette says for that owner; else
/// its default, the pool colour `like`.
struct ColorOwnPool {
    std::string name;
    std::string like;
    friend bool operator==(const ColorOwnPool&, const ColorOwnPool&) = default;
    ZEN_SHAPE(ColorOwnPool, 1, ZEN_FIELD(name), ZEN_FIELD(like));
};

/// A COLOUR A DRAW CARRIES: none, an exact value, a pool colour or an own value, the case it holds
/// saying which.
struct Color {
    using Is = std::variant<ColorNone, ColorExact, ColorPool, ColorOwn, ColorOwnPool>;
    Is is;
    /// Version 2: one case a kind of colour, each holding only the fields it means.
    ZEN_UNION(Color, 2, is, "none", "exact", "pool", "own", "own_pool");
    friend bool operator==(const Color&, const Color&) = default;

    Color() = default;
    /// Each of the five is a colour, as a variant holds one of its alternatives.
    template <class C>
        requires std::is_constructible_v<Is, C&&>
    Color(C&& c) : is(std::forward<C>(c)) {}

    bool none() const { return std::holds_alternative<ColorNone>(is); }
    /// The name it goes by: a pool colour's or an own value's; "" for none or an exact colour.
    std::string_view name() const {
        return std::visit(
            [](const auto& c) -> std::string_view {
                if constexpr (requires { c.name; }) {
                    return c.name;
                } else {
                    return {};
                }
            },
            is);
    }
};

/// An exact colour, opaque: 0xRRGGBB.
inline Color rgb(std::uint32_t value) {
    return ColorExact{static_cast<std::int64_t>((value & 0xffffffu) << 8 | 0xffu), {}};
}

/// An exact colour with its alpha: 0xRRGGBBAA.
inline Color rgba(std::uint32_t value) {
    return ColorExact{static_cast<std::int64_t>(value), {}};
}

/// No paint at all.
inline Color no_color() { return Color{}; }

/// A NAME FROM THE SHARED POOL, as the pool's generated header declares it
/// (`zengine::graphics::colors::accent`). It is the name and nothing else.
struct PoolColor {
    const char* name;

    operator Color() const { return ColorPool{name}; }
    friend bool operator==(const PoolColor& a, const PoolColor& b) {
        return std::string_view(a.name) == std::string_view(b.name);
    }
};

/// An exact colour, opaque, 0xRRGGBB, and the pool colour a medium that cannot show it shows in
/// its place.
inline Color rgb(std::uint32_t value, PoolColor like) {
    return ColorExact{static_cast<std::int64_t>((value & 0xffffffu) << 8 | 0xffu), like.name};
}

/// AN OWN COLOUR A PANE DECLARES, as a header generated from its own values' JSON declares it: its
/// name, the colour it paints where no theme sets it (0xRRGGBBAA), and the pool colour a medium
/// that cannot show that colour shows instead.
struct OwnColor {
    const char* name;
    std::uint32_t rgba;
    const char* like;

    operator Color() const {
        return ColorOwn{name, static_cast<std::int64_t>(rgba), like};
    }
};

/// AN OWN COLOUR DECLARED WHERE IT IS USED: `name`, painted as the pool colour `root` where no
/// theme sets it (`own("wire", colors::guide)`).
inline Color own(const char* name, PoolColor root) {
    return ColorOwnPool{name, root.name};
}

/// ...painted as the colour `exact` where no theme sets it: an exact colour, shown in a medium that
/// cannot show it as the pool colour it is `like` (`own("kept", rgb(0x8fbcbb, colors::accent))`),
/// or a pool colour.
inline Color own(const char* name, const Color& exact) {
    if (const ColorExact* e = std::get_if<ColorExact>(&exact.is)) {
        return ColorOwn{name, e->rgba, e->like};
    }
    const ColorPool* p = std::get_if<ColorPool>(&exact.is);
    return ColorOwnPool{name, p != nullptr ? p->name : std::string()};
}

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_COLOR_HPP
