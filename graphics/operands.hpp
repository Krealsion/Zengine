// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_OPERANDS_HPP
#define ZENGINE_GRAPHICS_OPERANDS_HPP

// The seven operators' operands, one union a node draws by: the case it holds is its operator.
// Coordinates are the area's own pixels, relative to the node's parent group. Every colour an
// operand carries is a `Color` (graphics/color.hpp), which the window turns into a colour when it
// paints; the window paints every operator, natively or by the fallback its medium names
// (graphics/docs/graphics.md, "The operators").

#include "graphics/color.hpp"
#include "graphics/text_style.hpp"

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace zengine::graphics {

/// group: its children are drawn offset by (x, y), and clipped to (0, 0, clip_w, clip_h) of that
/// space when both are positive.
struct GroupArgs {
    std::int64_t x = 0, y = 0;
    std::int64_t clip_w = 0, clip_h = 0;
    friend bool operator==(const GroupArgs&, const GroupArgs&) = default;
    ZEN_SHAPE(GroupArgs, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(clip_w), ZEN_FIELD(clip_h));
};

/// rect: a rectangle, filled and outlined, either or both.
struct RectArgs {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    Color fill;
    Color stroke;
    friend bool operator==(const RectArgs&, const RectArgs&) = default;
    ZEN_SHAPE(RectArgs, 2, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(fill),
              ZEN_FIELD(stroke));
};

/// ellipse: the ellipse inscribed in a box, filled and outlined, either or both.
struct EllipseArgs {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    Color fill;
    Color stroke;
    friend bool operator==(const EllipseArgs&, const EllipseArgs&) = default;
    ZEN_SHAPE(EllipseArgs, 2, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(fill), ZEN_FIELD(stroke));
};

/// line: a straight line between two points, at any angle.
struct LineArgs {
    std::int64_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    Color ink;
    friend bool operator==(const LineArgs&, const LineArgs&) = default;
    ZEN_SHAPE(LineArgs, 2, ZEN_FIELD(x1), ZEN_FIELD(y1), ZEN_FIELD(x2), ZEN_FIELD(y2),
              ZEN_FIELD(ink));
};

/// path: SVG path data, absolute and relative (M L H V C Q Z), filled even-odd and stroked.
struct PathArgs {
    std::string d;
    Color fill;
    Color stroke;
    friend bool operator==(const PathArgs&, const PathArgs&) = default;
    ZEN_SHAPE(PathArgs, 2, ZEN_FIELD(d), ZEN_FIELD(fill), ZEN_FIELD(stroke));
};

/// text: words from (x, y), the top of their line, every character drawn, a character every advance
/// of its size by the columns it takes (`component::character_columns`), in a style ("" the pool's
/// `body`) and any field of it this draw changes: its colour (none: the style's), its size (none:
/// the style's), its font ("": the style's) and its weight (`text_weight::kOfStyle`: the style's).
struct TextArgs {
    std::int64_t x = 0, y = 0;
    std::string text;
    Color ink;
    TextStyle style;
    TextSize size;
    std::string font;
    std::int64_t weight = text_weight::kOfStyle;
    /// Version 4: a text names its style, and its colour, size, font and weight change it.
    friend bool operator==(const TextArgs&, const TextArgs&) = default;
    ZEN_SHAPE(TextArgs, 4, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text), ZEN_FIELD(ink),
              ZEN_FIELD(style), ZEN_FIELD(size), ZEN_FIELD(font), ZEN_FIELD(weight));
};

/// image: `pw` x `ph` pixels, four bytes each (red, green, blue, alpha), shown in (x, y, w, h).
struct ImageArgs {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t pw = 0, ph = 0;
    loom::Bytes rgba;
    friend bool operator==(const ImageArgs&, const ImageArgs&) = default;
    ZEN_SHAPE(ImageArgs, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(pw),
              ZEN_FIELD(ph), ZEN_FIELD(rgba));
};

/// WHAT A NODE DRAWS: one operator's operands, the case named by its operator.
struct Operands {
    using Is = std::variant<GroupArgs, RectArgs, EllipseArgs, LineArgs, PathArgs, TextArgs,
                            ImageArgs>;
    Is is;
    ZEN_UNION(Operands, 1, is, "group", "rect", "ellipse", "line", "path", "text", "image");
    friend bool operator==(const Operands&, const Operands&) = default;

    Operands() = default;
    /// Each of the seven is a node's operands, as a variant holds one of its alternatives.
    template <class A>
        requires std::is_constructible_v<Is, A&&>
    Operands(A&& a) : is(std::forward<A>(a)) {}
};

/// THE ONE PRIMITIVE. Its `path` is its owner's name for it, unique in its area, and its place: the
/// segments before the last name the node it lives in. `draw` is what it draws, its operator the
/// case it holds.
struct Node {
    std::string path;
    Operands draw;
    ZEN_SHAPE(Node, 2, ZEN_FIELD(path), ZEN_FIELD(draw));
};

/// The bytes a node's operands hold past their fixed fields: a text's words, a path's data, an
/// image's pixels; none for the rest.
inline std::size_t operand_bytes(const Operands& draw) {
    if (const auto* t = std::get_if<TextArgs>(&draw.is)) {
        return t->text.size() + t->font.size();
    }
    if (const auto* p = std::get_if<PathArgs>(&draw.is)) {
        return p->d.size();
    }
    if (const auto* i = std::get_if<ImageArgs>(&draw.is)) {
        return i->rgba.size();
    }
    return 0;
}

/// What draws by its operands (a node, a painter's node) read as `A`, or null when its operator is
/// another.
template <class A, class Drawn>
const A* operands(const Drawn& n) {
    return std::get_if<A>(&n.draw.is);
}

/// The word what draws by its operands is drawn by: `group`, `rect`, `ellipse`, `line`, `path`, `text` or
/// `image`.
template <class Drawn>
std::string_view operator_of(const Drawn& n) {
    return Operands::zen_case_names[n.draw.is.index()];
}

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_OPERANDS_HPP
