// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_TEXT_STYLE_HPP
#define ZENGINE_GRAPHICS_TEXT_STYLE_HPP

// What text a draw carries is set in beside its colour: a style, and any field of it changed for
// this one draw. A style is a font, a size and a colour, each a root of the shared pool, and a
// weight; a size and a font are roots by name, or a size the drawing's pane exposes as a slot.
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
#include <utility>

namespace zengine::graphics {

/// How heavy a text's strokes are.
namespace text_weight {
inline constexpr std::int64_t kRegular = 0;
inline constexpr std::int64_t kBold = 1;
/// The weight the text's style gives it.
inline constexpr std::int64_t kOfStyle = -1;
} // namespace text_weight

/// Where a text size comes from.
namespace size_from {
inline constexpr std::int64_t kPool = 2; ///< the shared pool's `name`, as the area's palette says
/// The slot `name` the area's owner exposes, as the area's palette says for that owner; `points`
/// where the palette sets none.
inline constexpr std::int64_t kSlot = 3;
} // namespace size_from

/// Where a text's style comes from.
namespace style_from {
inline constexpr std::int64_t kPool = 2; ///< the shared pool's `name`, as the area's palette says
} // namespace style_from

/// The pool's style a text is set in where it names none.
inline constexpr const char* kBodyStyle = "body";
/// The pool's font a style naming none sets its text in.
inline constexpr const char* kTextFont = "text";

/// A TEXT SIZE A DRAW CARRIES: a pool size or a slot, by name ("" its style's size), with a slot's
/// points where no palette sets it.
struct TextSize {
    std::int64_t from = size_from::kPool;
    std::string name;
    std::int64_t points = 0;
    ZEN_SHAPE(TextSize, 1, ZEN_FIELD(from), ZEN_FIELD(name), ZEN_FIELD(points));

    /// Whether it names no size, so its style's stands.
    bool none() const { return from == size_from::kPool && name.empty(); }
    friend bool operator==(const TextSize&, const TextSize&) = default;
};

/// A SIZE FROM THE SHARED POOL, as the pool's generated header declares it
/// (`zengine::graphics::sizes::large`): the name and nothing else.
struct PoolSize {
    const char* name;
    operator TextSize() const { return TextSize{size_from::kPool, name, 0}; }
};

/// A SIZE A PANE EXPOSES, as its own generated header declares it: its name and the points it
/// sets text at where no theme sets it.
struct SlotSize {
    const char* name;
    std::int64_t points;
    operator TextSize() const { return TextSize{size_from::kSlot, name, points}; }
};

/// A FONT FROM THE SHARED POOL, as the pool's generated header declares it
/// (`zengine::graphics::fonts::text`): the name and nothing else.
struct PoolFont {
    const char* name;
    operator std::string() const { return name; }
};

/// A STYLE A DRAW CARRIES, by name ("" the pool's `body`).
struct TextStyle {
    std::int64_t from = style_from::kPool;
    std::string name;
    ZEN_SHAPE(TextStyle, 1, ZEN_FIELD(from), ZEN_FIELD(name));

    /// The pool style's name this one reads as: its own, or `body` where it names none.
    std::string pool_name() const { return name.empty() ? std::string(kBodyStyle) : name; }
    friend bool operator==(const TextStyle&, const TextStyle&) = default;
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
    operator TextStyle() const { return TextStyle{style_from::kPool, name}; }
    operator TextLook() const { return TextLook{TextStyle(*this), {}, {}, {}, text_weight::kOfStyle}; }
    TextLook with_color(Color c) const { return TextLook(*this).with_color(std::move(c)); }
    TextLook with_size(TextSize s) const { return TextLook(*this).with_size(std::move(s)); }
    TextLook with_font(std::string f) const { return TextLook(*this).with_font(std::move(f)); }
    TextLook with_weight(std::int64_t w) const { return TextLook(*this).with_weight(w); }
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_TEXT_STYLE_HPP
