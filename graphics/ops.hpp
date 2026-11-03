// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_OPS_HPP
#define ZENGINE_GRAPHICS_OPS_HPP

// The seven operators: each is a node's `op` and the shape of its operands. Coordinates are the
// area's own pixels, relative to the node's parent group. Every colour an operand carries is a
// `Color` (graphics/color.hpp), which the window turns into a colour when it paints; the window
// paints every operator, natively or by the fallback its medium names (graphics/docs/graphics.md,
// "The operators").

#include "graphics/color.hpp"
#include "graphics/colors.hpp" // generated from graphics/pool.json
#include "graphics/implied.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::graphics {

/// A ROLE READ AS THE POOL COLOUR IT MEANS (graphics/role.hpp), for a drawer that names a role:
/// `kFill` text, `kAccent` accent, `kMuted` quiet, `kAlert` alert, `kGround` background,
/// `kMediumGround` ground, `kGuide` guide, `kEdge` edge; a role it does not know is text.
inline PoolColor pool_color_of_role(std::int64_t r) {
    switch (r) {
    case role::kAccent: return colors::accent;
    case role::kMuted: return colors::quiet;
    case role::kAlert: return colors::alert;
    case role::kGround: return colors::background;
    case role::kMediumGround: return colors::ground;
    case role::kGuide: return colors::guide;
    case role::kEdge: return colors::edge;
    default: return colors::text;
    }
}

/// A role's pool colour; `role::kNone` is no paint at all.
inline Color by_role(std::int64_t r) {
    return r == role::kNone ? Color{} : Color(pool_color_of_role(r));
}

/// An exact 0xRRGGBB, opaque, beside the role whose pool colour a medium that cannot show it
/// shows in its place.
inline Color exact(std::int64_t r, std::int64_t value) {
    Color c = rgb(static_cast<std::uint32_t>(value));
    if (r != role::kNone) {
        c.like = pool_color_of_role(r).name;
    }
    return c;
}

inline Color no_paint() { return Color{}; }

/// The role a colour means, for a reader that sorts what it read by role: a pool colour's, or the
/// one an exact colour or a slot is `like`; `kFill` for any other, `kNone` for no paint.
inline std::int64_t role_of(const Color& c) {
    if (c.none()) {
        return role::kNone;
    }
    const std::string& name = c.from == color_from::kPool ? c.name : c.like;
    for (const std::int64_t r : {role::kAccent, role::kMuted, role::kAlert, role::kGround,
                                 role::kMediumGround, role::kGuide, role::kEdge}) {
        if (name == pool_color_of_role(r).name) {
            return r;
        }
    }
    return role::kFill;
}

/// The colour a drawer's code names by its older word.
using Paint = Color;

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
    Color fill;
    Color stroke;
    ZEN_SHAPE(RectArgs, 2, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(fill),
              ZEN_FIELD(stroke));
};

/// ellipse: the ellipse inscribed in a box, filled and outlined, either or both.
struct EllipseArgs {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    Color fill;
    Color stroke;
    ZEN_SHAPE(EllipseArgs, 2, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(fill), ZEN_FIELD(stroke));
};

/// line: a straight line between two points, at any angle.
struct LineArgs {
    std::int64_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    Color ink;
    ZEN_SHAPE(LineArgs, 2, ZEN_FIELD(x1), ZEN_FIELD(y1), ZEN_FIELD(x2), ZEN_FIELD(y2),
              ZEN_FIELD(ink));
};

/// path: SVG path data, absolute and relative (M L H V C Q Z), filled even-odd and stroked.
struct PathArgs {
    std::string d;
    Color fill;
    Color stroke;
    ZEN_SHAPE(PathArgs, 2, ZEN_FIELD(d), ZEN_FIELD(fill), ZEN_FIELD(stroke));
};

/// text: words from (x, y), the top of their line.
struct TextArgs {
    std::int64_t x = 0, y = 0;
    std::string text;
    Color ink;
    ZEN_SHAPE(TextArgs, 2, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text), ZEN_FIELD(ink));
};

/// image: `pw` x `ph` pixels, four bytes each (red, green, blue, alpha), shown in (x, y, w, h).
struct ImageArgs {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t pw = 0, ph = 0;
    loom::Bytes rgba;
    ZEN_SHAPE(ImageArgs, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(pw),
              ZEN_FIELD(ph), ZEN_FIELD(rgba));
};

/// The colours a node's operands carry, in the order a painter takes them: a rectangle's, an
/// ellipse's and a path's fill then stroke, a line's and a text's ink; none for a group, an image,
/// an operator this vocabulary does not know, or operands that do not read as their operator's.
inline std::vector<Color> colors_of(const std::string& op, const loom::Bytes& args) {
    if (op == op::kRect) {
        if (auto a = from_body<RectArgs>(args)) {
            return {a->fill, a->stroke};
        }
    } else if (op == op::kEllipse) {
        if (auto a = from_body<EllipseArgs>(args)) {
            return {a->fill, a->stroke};
        }
    } else if (op == op::kPath) {
        if (auto a = from_body<PathArgs>(args)) {
            return {a->fill, a->stroke};
        }
    } else if (op == op::kLine) {
        if (auto a = from_body<LineArgs>(args)) {
            return {a->ink};
        }
    } else if (op == op::kText) {
        if (auto a = from_body<TextArgs>(args)) {
            return {a->ink};
        }
    }
    return {};
}

namespace detail {
template <class Args>
std::string refused_unless(const std::string& op, const loom::Bytes& args) {
    if (from_body<Args>(args)) {
        return {};
    }
    const auto& schema = *loom::schema_of<Args>();
    return "its " + op + " operands do not read as " + schema.name() + " version " +
           std::to_string(schema.version()) +
           ", the one this window reads (version 1 named its colours by role, and is not read)";
}
} // namespace detail

/// Why a node's colour-bearing operands do not read as its operator's shape at this vocabulary's
/// version, naming the shape and its versions; "" where they read, or where the operator carries
/// no colour.
inline std::string operands_refused(const std::string& op, const loom::Bytes& args) {
    if (op == op::kRect) {
        return detail::refused_unless<RectArgs>(op, args);
    }
    if (op == op::kEllipse) {
        return detail::refused_unless<EllipseArgs>(op, args);
    }
    if (op == op::kLine) {
        return detail::refused_unless<LineArgs>(op, args);
    }
    if (op == op::kPath) {
        return detail::refused_unless<PathArgs>(op, args);
    }
    if (op == op::kText) {
        return detail::refused_unless<TextArgs>(op, args);
    }
    return {};
}

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_OPS_HPP
