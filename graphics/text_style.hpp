// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_TEXT_STYLE_HPP
#define ZENGINE_GRAPHICS_TEXT_STYLE_HPP

// What text a draw carries is set in beside its colour: a style, and any field of it changed for
// this one draw. A style is a font, a size and a colour, each a root of the shared pool, and a
// weight; a size and a style are roots by name, or own values the drawing's pane declares.
// The window turns a name into points and a face when it paints, from the palette the area wears,
// and offers each size's and style's measure in the room it tells an owner. The names are C++
// names generated from JSON with the pool's colours (cmake/ZengineNames.cmake):
// `zengine::graphics::styles`, `sizes` and `fonts`. Reference: graphics/docs/graphics.md, "Text:
// styles, sizes, fonts, weight".

#include "graphics/color.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace zengine::graphics {

/// How heavy a text's strokes are.
namespace text_weight {
inline constexpr std::int64_t kRegular = 0;
inline constexpr std::int64_t kBold = 1;
/// The weight the text's style gives it.
inline constexpr std::int64_t kOfStyle = -1;
} // namespace text_weight

/// The pool's style a text is set in where it names none.
inline constexpr const char* kBodyStyle = "body";
/// The pool's font a style naming none sets its text in.
inline constexpr const char* kTextFont = "text";

/// A pool size `name`, as the area's palette says; "" names none, so the text's style's stands.
struct SizePool {
    std::string name;
    friend bool operator==(const SizePool&, const SizePool&) = default;
    ZEN_SHAPE(SizePool, 1, ZEN_FIELD(name));
};

/// The own size `name` the area's owner declares, as the area's palette says for that owner; else
/// its default, `points`.
struct SizeOwn {
    std::string name;
    std::int64_t points = 0;
    friend bool operator==(const SizeOwn&, const SizeOwn&) = default;
    ZEN_SHAPE(SizeOwn, 1, ZEN_FIELD(name), ZEN_FIELD(points));
};

/// The own size `name` the area's owner declares, as the area's palette says for that owner; else
/// its default, the pool size `like`.
struct SizeOwnPool {
    std::string name;
    std::string like;
    friend bool operator==(const SizeOwnPool&, const SizeOwnPool&) = default;
    ZEN_SHAPE(SizeOwnPool, 1, ZEN_FIELD(name), ZEN_FIELD(like));
};

/// A TEXT SIZE A DRAW CARRIES: a pool size or an own value, the case it holds saying which.
struct TextSize {
    using Is = std::variant<SizePool, SizeOwn, SizeOwnPool>;
    Is is;
    /// Version 3: one case a kind of size, each holding only the fields it means.
    ZEN_UNION(TextSize, 3, is, "pool", "own", "own_pool");
    friend bool operator==(const TextSize&, const TextSize&) = default;

    TextSize() = default;
    /// Each of the three is a size, as a variant holds one of its alternatives.
    template <class S>
        requires std::is_constructible_v<Is, S&&>
    TextSize(S&& s) : is(std::forward<S>(s)) {}

    /// Whether it names no size, so its style's stands.
    bool none() const {
        const SizePool* p = std::get_if<SizePool>(&is);
        return p != nullptr && p->name.empty();
    }
};

/// A SIZE FROM THE SHARED POOL, as the pool's generated header declares it
/// (`zengine::graphics::sizes::large`): the name and nothing else.
struct PoolSize {
    const char* name;
    operator TextSize() const { return SizePool{name}; }
};

/// AN OWN SIZE A PANE DECLARES, as a header generated from its own values' JSON declares it: its
/// name and the points it sets text at where no theme sets it.
struct OwnSize {
    const char* name;
    std::int64_t points;
    operator TextSize() const { return SizeOwn{name, points}; }
};

/// AN OWN SIZE DECLARED WHERE IT IS USED: `name`, standing at the pool size `root` where no theme
/// sets it (`own("note", sizes::small_)`).
inline TextSize own(const char* name, PoolSize root) {
    return SizeOwnPool{name, root.name};
}

/// A FONT FROM THE SHARED POOL, as the pool's generated header declares it
/// (`zengine::graphics::fonts::text`): the name and nothing else.
struct PoolFont {
    const char* name;
    operator std::string() const { return name; }
};

/// A pool style `name`, as the area's palette says; "" the pool's `body`.
struct StylePool {
    std::string name;
    friend bool operator==(const StylePool&, const StylePool&) = default;
    ZEN_SHAPE(StylePool, 1, ZEN_FIELD(name));
};

/// The own style `name` the area's owner declares, as the area's palette says for that owner; else
/// its default, the pool style `like`.
struct StyleOwn {
    std::string name;
    std::string like;
    friend bool operator==(const StyleOwn&, const StyleOwn&) = default;
    ZEN_SHAPE(StyleOwn, 1, ZEN_FIELD(name), ZEN_FIELD(like));
};

/// A STYLE A DRAW CARRIES: a pool style or an own value, the case it holds saying which.
struct TextStyle {
    using Is = std::variant<StylePool, StyleOwn>;
    Is is;
    /// Version 2: one case a kind of style, each holding only the fields it means.
    ZEN_UNION(TextStyle, 2, is, "pool", "own");
    friend bool operator==(const TextStyle&, const TextStyle&) = default;

    TextStyle() = default;
    /// Each of the two is a style, as a variant holds one of its alternatives.
    template <class S>
        requires std::is_constructible_v<Is, S&&>
    TextStyle(S&& s) : is(std::forward<S>(s)) {}
};

/// A TEXT'S LOOK AS A DRAW NAMES IT: its style, and each field that one draw changes -- its colour
/// (none: the style's), its size (none: the style's), its font ("": the style's) and its weight
/// (`text_weight::kOfStyle`: the style's).
struct TextLook {
    TextStyle style;
    Color ink;
    TextSize size;
    std::string font;
    std::int64_t weight = text_weight::kOfStyle;
    ZEN_SHAPE(TextLook, 1, ZEN_FIELD(style), ZEN_FIELD(ink), ZEN_FIELD(size), ZEN_FIELD(font),
              ZEN_FIELD(weight));
    friend bool operator==(const TextLook&, const TextLook&) = default;

    TextLook with_color(Color c) const {
        TextLook l = *this;
        l.ink = std::move(c);
        return l;
    }
    TextLook with_size(TextSize s) const {
        TextLook l = *this;
        l.size = std::move(s);
        return l;
    }
    TextLook with_font(std::string f) const {
        TextLook l = *this;
        l.font = std::move(f);
        return l;
    }
    TextLook with_weight(std::int64_t w) const {
        TextLook l = *this;
        l.weight = w;
        return l;
    }
};

/// A STYLE FROM THE SHARED POOL, as the pool's generated header declares it
/// (`zengine::graphics::styles::heading`): the name, a look drawn as it stands or with one field
/// changed (`styles::heading.with_color(colors::alert)`).
struct PoolStyle {
    const char* name;
    operator TextStyle() const { return StylePool{name}; }
    operator TextLook() const { return TextLook{TextStyle(*this), {}, {}, {}, text_weight::kOfStyle}; }
    TextLook with_color(Color c) const { return TextLook(*this).with_color(std::move(c)); }
    TextLook with_size(TextSize s) const { return TextLook(*this).with_size(std::move(s)); }
    TextLook with_font(std::string f) const { return TextLook(*this).with_font(std::move(f)); }
    TextLook with_weight(std::int64_t w) const { return TextLook(*this).with_weight(w); }
};

/// AN OWN STYLE DECLARED WHERE IT IS USED: `name`, set in the pool style `root` where no theme sets
/// it (`own("node-title", styles::heading)`), as it stands or with a field changed.
inline TextLook own(const char* name, PoolStyle root) {
    TextLook l = root;
    l.style = StyleOwn{name, root.name};
    return l;
}

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_TEXT_STYLE_HPP
