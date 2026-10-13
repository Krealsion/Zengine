// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_OPS_HPP
#define ZENGINE_GRAPHICS_OPS_HPP

// The seven operators: each is a node's `op` and the shape of its operands. Coordinates are the
// area's own pixels, relative to the node's parent group. Every skin draws every operator, natively
// or by the fallback its medium names (graphics/docs/graphics.md, "The operators").

#include "graphics/vocabulary.hpp"

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::graphics {

/// A colour by the theme's role, or an exact 0xRRGGBB beside the role a medium without colour
/// shows; `role::kNone` with no exact colour is no paint at all.
struct Paint {
    std::int64_t role = role::kNone;
    std::int64_t rgb = -1;
    ZEN_SHAPE(Paint, 1, ZEN_FIELD(role), ZEN_FIELD(rgb));

    bool none() const { return role == role::kNone && rgb < 0; }
};

inline Paint by_role(std::int64_t r) { return Paint{r, -1}; }
inline Paint exact(std::int64_t r, std::int64_t rgb) { return Paint{r, rgb}; }
inline Paint no_paint() { return Paint{}; }

namespace op {
inline constexpr const char* kGroup = "group";
inline constexpr const char* kRect = "rect";
inline constexpr const char* kEllipse = "ellipse";
inline constexpr const char* kLine = "line";
inline constexpr const char* kPath = "path";
inline constexpr const char* kText = "text";
inline constexpr const char* kImage = "image";
} // namespace op

/// group: its children are drawn offset by (x, y), and clipped to (0, 0, clip_w, clip_h) of that
/// space when both are positive.
struct GroupArgs {
    std::int64_t x = 0, y = 0;
    std::int64_t clip_w = 0, clip_h = 0;
    ZEN_SHAPE(GroupArgs, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(clip_w), ZEN_FIELD(clip_h));
};

/// rect: a rectangle, filled and outlined, either or both.
struct RectArgs {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    Paint fill;
    Paint stroke;
    ZEN_SHAPE(RectArgs, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(fill),
              ZEN_FIELD(stroke));
};

/// ellipse: the ellipse inscribed in a box, filled and outlined, either or both.
struct EllipseArgs {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    Paint fill;
    Paint stroke;
    ZEN_SHAPE(EllipseArgs, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(fill), ZEN_FIELD(stroke));
};

/// line: a straight line between two points, at any angle.
struct LineArgs {
    std::int64_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    Paint ink;
    ZEN_SHAPE(LineArgs, 1, ZEN_FIELD(x1), ZEN_FIELD(y1), ZEN_FIELD(x2), ZEN_FIELD(y2),
              ZEN_FIELD(ink));
};

/// path: SVG path data, absolute and relative (M L H V C Q Z), filled even-odd and stroked.
struct PathArgs {
    std::string d;
    Paint fill;
    Paint stroke;
    ZEN_SHAPE(PathArgs, 1, ZEN_FIELD(d), ZEN_FIELD(fill), ZEN_FIELD(stroke));
};

/// text: words from (x, y), the top of their line.
struct TextArgs {
    std::int64_t x = 0, y = 0;
    std::string text;
    Paint ink;
    ZEN_SHAPE(TextArgs, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text), ZEN_FIELD(ink));
};

/// image: `pw` x `ph` pixels, four bytes each (red, green, blue, alpha), shown in (x, y, w, h).
struct ImageArgs {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t pw = 0, ph = 0;
    loom::Bytes rgba;
    ZEN_SHAPE(ImageArgs, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(pw),
              ZEN_FIELD(ph), ZEN_FIELD(rgba));
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_OPS_HPP
