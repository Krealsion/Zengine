// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_OPERANDS_HPP
#define ZENGINE_GRAPHICS_OPERANDS_HPP

// The seven operators' operands, one union a node draws by: the case it holds is its operator; and
// the looks nodes wear, said once in a drawing. Coordinates are the area's own pixels, relative to
// the node's parent group. Every colour a look carries is a `Color` (graphics/color.hpp), which the
// window turns into a colour when it paints; the window paints every operator, natively or by the
// fallback its medium names (graphics/docs/graphics.md, "The operators").

#include "graphics/color.hpp"
#include "graphics/text_style.hpp"

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
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

/// rect: a rectangle, filled and outlined, either or both, as its look (`Paint`) says.
struct RectArgs {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    friend bool operator==(const RectArgs&, const RectArgs&) = default;
    /// Version 3: its fill and stroke are its look's.
    ZEN_SHAPE(RectArgs, 3, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h));
};

/// ellipse: the ellipse inscribed in a box, filled and outlined, either or both, as its look
/// (`Paint`) says.
struct EllipseArgs {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    friend bool operator==(const EllipseArgs&, const EllipseArgs&) = default;
    /// Version 3: its fill and stroke are its look's.
    ZEN_SHAPE(EllipseArgs, 3, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h));
};

/// line: a straight line between two points, at any angle, in its look's ink (`Ink`).
struct LineArgs {
    std::int64_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    friend bool operator==(const LineArgs&, const LineArgs&) = default;
    /// Version 3: its ink is its look's.
    ZEN_SHAPE(LineArgs, 3, ZEN_FIELD(x1), ZEN_FIELD(y1), ZEN_FIELD(x2), ZEN_FIELD(y2));
};

/// path: SVG path data, absolute and relative (M L H V C Q Z), filled even-odd and stroked as its
/// look (`Paint`) says.
struct PathArgs {
    std::string d;
    friend bool operator==(const PathArgs&, const PathArgs&) = default;
    /// Version 3: its fill and stroke are its look's.
    ZEN_SHAPE(PathArgs, 3, ZEN_FIELD(d));
};

/// text: words from (x, y), the top of their line, every character drawn, a character every advance
/// of its size by the columns it takes (`component::character_columns`), set as its look
/// (`TextLook`) says.
struct TextArgs {
    std::int64_t x = 0, y = 0;
    std::string text;
    friend bool operator==(const TextArgs&, const TextArgs&) = default;
    /// Version 5: its style, colour, size, font and weight are its look's.
    ZEN_SHAPE(TextArgs, 5, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text));
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

/// A rectangle's, an ellipse's or a path's look: its fill and its stroke, either none.
struct Paint {
    Color fill;
    Color stroke;
    friend bool operator==(const Paint&, const Paint&) = default;
    ZEN_SHAPE(Paint, 1, ZEN_FIELD(fill), ZEN_FIELD(stroke));
};

/// A line's look: its ink.
struct Ink {
    Color ink;
    friend bool operator==(const Ink&, const Ink&) = default;
    ZEN_SHAPE(Ink, 1, ZEN_FIELD(ink));
};

/// WHAT A NODE WEARS, said once in a drawing for every node that wears it: a rectangle's, an
/// ellipse's or a path's paint, a line's ink, or a text's look. A group and an image wear none, and
/// a node that names none wears its operator's default (`look_or_default`).
struct Look {
    using Is = std::variant<Paint, Ink, TextLook>;
    Is is;
    ZEN_UNION(Look, 1, is, "paint", "ink", "text");
    friend bool operator==(const Look&, const Look&) = default;

    Look() = default;
    /// Each of the three is a look, as a variant holds one of its alternatives.
    template <class L>
        requires std::is_constructible_v<Is, L&&>
    Look(L&& l) : is(std::forward<L>(l)) {}
};

namespace detail {
inline void mix_into(std::size_t& h, std::size_t v) {
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
}
template <class T>
void hash_into(std::size_t& h, const T& v) {
    if constexpr (std::is_integral_v<T>) {
        mix_into(h, std::hash<T>{}(v));
    } else if constexpr (std::is_same_v<T, std::string>) {
        mix_into(h, std::hash<std::string>{}(v));
    } else if constexpr (requires { T::zen_union(); }) {
        mix_into(h, (v.*T::zen_union()).index());
        std::visit([&h](const auto& c) { hash_into(h, c); }, v.*T::zen_union());
    } else {
        std::apply([&](const auto&... f) { (hash_into(h, v.*f.ptr), ...); }, T::zen_fields());
    }
}
} // namespace detail

/// A LOOK'S HASH, over every field it declares, so a look is found among many in one step.
struct LookHash {
    std::size_t operator()(const Look& look) const {
        std::size_t h = 0;
        detail::hash_into(h, look);
        return h;
    }
};

/// A node that wears no look names this.
inline constexpr std::int64_t kNoLook = -1;

/// A look read as `L` (`Paint`, `Ink` or `TextLook`), or null when it is another or none.
template <class L>
const L* look_as(const Look* look) {
    return look != nullptr ? std::get_if<L>(&look->is) : nullptr;
}

/// THE ONE PRIMITIVE. Its `path` is its owner's name for it, unique in its area, and its place: the
/// segments before the last name the node it lives in. `look` names the look it wears among the
/// ones its drawing says (`Drawn::looks`), by its place there, `kNoLook` for none; `draw` is what
/// it draws, its operator the case it holds.
struct Node {
    std::string path;
    std::int64_t look = kNoLook;
    Operands draw;
    ZEN_SHAPE(Node, 2, ZEN_FIELD(path), ZEN_FIELD(look), ZEN_FIELD(draw));
};

/// Whether a look is one an operator wears: paint for a rectangle, an ellipse or a path; ink for a
/// line; a text's look for a text; none for a group or an image, and none for any, its default.
inline bool wears(const Operands& draw, const Look* look) {
    const bool paint = std::holds_alternative<RectArgs>(draw.is) ||
                       std::holds_alternative<EllipseArgs>(draw.is) ||
                       std::holds_alternative<PathArgs>(draw.is);
    if (look == nullptr) {
        return true;
    }
    if (paint) {
        return std::holds_alternative<Paint>(look->is);
    }
    if (std::holds_alternative<LineArgs>(draw.is)) {
        return std::holds_alternative<Ink>(look->is);
    }
    if (std::holds_alternative<TextArgs>(draw.is)) {
        return std::holds_alternative<TextLook>(look->is);
    }
    return false;
}

/// The look a node wears: `look`, or where it names none its operator's default -- a text in the
/// pool's `body` style and its colour, a paint and an ink of none; null for a group or an image.
inline const Look* look_or_default(const Operands& draw, const Look* look) {
    static const Look paint{Paint{}};
    static const Look ink{Ink{}};
    static const Look text{TextLook{}};
    if (look != nullptr) {
        return look;
    }
    if (std::holds_alternative<TextArgs>(draw.is)) {
        return &text;
    }
    if (std::holds_alternative<LineArgs>(draw.is)) {
        return &ink;
    }
    if (std::holds_alternative<RectArgs>(draw.is) || std::holds_alternative<EllipseArgs>(draw.is) ||
        std::holds_alternative<PathArgs>(draw.is)) {
        return &paint;
    }
    return nullptr;
}

/// The bytes a node's operands hold past their fixed fields: a text's words, a path's data, an
/// image's pixels; none for the rest.
inline std::size_t operand_bytes(const Operands& draw) {
    if (const auto* t = std::get_if<TextArgs>(&draw.is)) {
        return t->text.size();
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
