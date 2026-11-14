// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_LIGHT_PAINT_HPP
#define ZENGINE_GRAPHICS_LIGHT_PAINT_HPP

// Painting nodes for a medium of light: every operator, in the area's own pixels, into the
// primitives a renderer draws -- filled rectangles (quads), lines, runs of text and images -- each
// in the exact colour the window resolved for its node (`PaintNode::inks`). A node paints in its
// parent groups' space and clip. A node's primitives come back packed in its `NodePaint`'s `cells`
// (`pack_node`), behind a tag that tells them from a character painter's runs. It knows no colour
// and keeps nothing between calls.
// Reference: graphics/docs/graphics.md, "The operators".

#include "component/text.hpp"
#include "graphics/implied.hpp"
#include "graphics/ops.hpp"
#include "graphics/painting.hpp"
#include "graphics/term_paint.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::graphics::light {

/// The tag that opens a node painted for light: 'L', 'G', 'T' and this layout's version.
inline constexpr std::array<std::uint8_t, 4> kTag = {'L', 'G', 'T', 3};

/// A filled rectangle; a line; a run of text from the top left of its line, less the pixel rows a
/// clip cut from its top and its foot, its characters a column `advance` pixels wide each by the
/// columns the text core gives them, a `line` tall, set in `face` at `points`, regular or bold
/// (`text_weight::`); an image of `pw` x `ph` pixels (red, green, blue, alpha) shown in (x, y, w,
/// h). Every colour is 0xRRGGBBAA.
struct Quad {
    std::int32_t x = 0, y = 0, w = 0, h = 0;
    std::uint32_t rgba = 0;
    bool operator==(const Quad&) const = default;
};
struct Line {
    std::int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    std::uint32_t rgba = 0;
    bool operator==(const Line&) const = default;
};
struct TextRun {
    std::int32_t x = 0, y = 0;
    std::uint32_t rgba = 0;
    std::string text;
    std::int32_t cut_top = 0, cut_foot = 0;
    std::int32_t advance = 0, line = 0;
    std::int32_t points = 0, weight = 0;
    std::string face{};
    bool operator==(const TextRun&) const = default;
};
struct ImageBlit {
    std::int32_t x = 0, y = 0, w = 0, h = 0, pw = 0, ph = 0;
    loom::Bytes rgba;
    bool operator==(const ImageBlit&) const = default;
};

/// ONE CHARACTER OF A RUN WHERE IT STANDS: its left edge in the run's pixels, its glyph -- its UTF-8
/// and the characters of no column after it -- and the columns it takes.
struct RunCharacter {
    std::int64_t x = 0;
    std::string glyph;
    int columns = 1;
};

/// A run's characters in order, a column `advance` pixels wide each (the run's own, or `fallback`
/// where it carries none), by the columns the text core gives them.
inline std::vector<RunCharacter> characters_of(const std::string& text, std::int64_t x,
                                               std::int64_t advance) {
    std::vector<RunCharacter> out;
    for (std::size_t i = 0; i < text.size(); i = component::character_end(text, i)) {
        const std::size_t end = component::character_end(text, i);
        const int w = component::character_columns(text, i);
        if (w == 0 && !out.empty()) {
            out.back().glyph.append(text, i, end - i);
            continue;
        }
        out.push_back(RunCharacter{x, text.substr(i, end - i), w});
        x += advance * w;
    }
    return out;
}

/// One node as painted for light, in its area's pixels: drawn quads first, then lines, text and
/// images.
struct NodePrimitives {
    std::vector<Quad> quads;
    std::vector<Line> lines;
    std::vector<TextRun> texts;
    std::vector<ImageBlit> images;
    bool operator==(const NodePrimitives&) const = default;
    bool empty() const {
        return quads.empty() && lines.empty() && texts.empty() && images.empty();
    }
};

namespace detail {

inline void put32(loom::Bytes& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xffu));
    }
}

/// A reader of packed bytes that refuses to read past their end.
class Reader {
public:
    explicit Reader(const loom::Bytes& b) : b_(b) {}
    bool u32(std::uint32_t& out) {
        if (b_.size() - at_ < 4) {
            return false;
        }
        out = static_cast<std::uint32_t>(b_[at_]) |
              (static_cast<std::uint32_t>(b_[at_ + 1]) << 8) |
              (static_cast<std::uint32_t>(b_[at_ + 2]) << 16) |
              (static_cast<std::uint32_t>(b_[at_ + 3]) << 24);
        at_ += 4;
        return true;
    }
    bool i32(std::int32_t& out) {
        std::uint32_t v = 0;
        if (!u32(v)) {
            return false;
        }
        out = static_cast<std::int32_t>(v);
        return true;
    }
    bool bytes(std::size_t n, const std::uint8_t*& out) {
        if (b_.size() - at_ < n) {
            return false;
        }
        out = b_.data() + at_;
        at_ += n;
        return true;
    }
    std::size_t left() const { return b_.size() - at_; }

private:
    const loom::Bytes& b_;
    std::size_t at_ = 0;
};

} // namespace detail

/// Whether bytes are a node painted for light: they open with `kTag`.
inline bool is_light(const loom::Bytes& b) {
    return b.size() >= kTag.size() && std::equal(kTag.begin(), kTag.end(), b.begin());
}

/// A node's primitives packed, little end first: the tag; the counts of quads, lines, texts and
/// images (four uint32); each quad as x, y, w, h (four int32) and its RGBA (uint32), 20 bytes; each
/// line as x1, y1, x2, y2 and its RGBA, 20 bytes; each text as x, y, its RGBA, its cut top and
/// foot, its advance, line, points and weight, its face's length and bytes, its length and its
/// bytes; each image as x, y, w, h, pw, ph, the length of its pixels and `pw * ph * 4` bytes.
inline loom::Bytes pack_node(const NodePrimitives& n) {
    loom::Bytes b(kTag.begin(), kTag.end());
    detail::put32(b, static_cast<std::uint32_t>(n.quads.size()));
    detail::put32(b, static_cast<std::uint32_t>(n.lines.size()));
    detail::put32(b, static_cast<std::uint32_t>(n.texts.size()));
    detail::put32(b, static_cast<std::uint32_t>(n.images.size()));
    for (const Quad& q : n.quads) {
        for (const std::int32_t v : {q.x, q.y, q.w, q.h}) {
            detail::put32(b, static_cast<std::uint32_t>(v));
        }
        detail::put32(b, q.rgba);
    }
    for (const Line& l : n.lines) {
        for (const std::int32_t v : {l.x1, l.y1, l.x2, l.y2}) {
            detail::put32(b, static_cast<std::uint32_t>(v));
        }
        detail::put32(b, l.rgba);
    }
    for (const TextRun& t : n.texts) {
        detail::put32(b, static_cast<std::uint32_t>(t.x));
        detail::put32(b, static_cast<std::uint32_t>(t.y));
        detail::put32(b, t.rgba);
        detail::put32(b, static_cast<std::uint32_t>(t.cut_top));
        detail::put32(b, static_cast<std::uint32_t>(t.cut_foot));
        for (const std::int32_t v : {t.advance, t.line, t.points, t.weight}) {
            detail::put32(b, static_cast<std::uint32_t>(v));
        }
        detail::put32(b, static_cast<std::uint32_t>(t.face.size()));
        b.insert(b.end(), t.face.begin(), t.face.end());
        detail::put32(b, static_cast<std::uint32_t>(t.text.size()));
        b.insert(b.end(), t.text.begin(), t.text.end());
    }
    for (const ImageBlit& im : n.images) {
        for (const std::int32_t v : {im.x, im.y, im.w, im.h, im.pw, im.ph}) {
            detail::put32(b, static_cast<std::uint32_t>(v));
        }
        detail::put32(b, static_cast<std::uint32_t>(im.rgba.size()));
        b.insert(b.end(), im.rgba.begin(), im.rgba.end());
    }
    return b;
}

/// A node's primitives read back: nothing when the bytes are not a node painted for light, end
/// early, run past their counts, or hold an image whose pixels are not `pw * ph * 4` bytes.
inline std::optional<NodePrimitives> unpack_node(const loom::Bytes& b) {
    if (!is_light(b)) {
        return std::nullopt;
    }
    detail::Reader r(b);
    const std::uint8_t* skip = nullptr;
    std::uint32_t quads = 0, lines = 0, texts = 0, images = 0;
    if (!r.bytes(kTag.size(), skip) || !r.u32(quads) || !r.u32(lines) || !r.u32(texts) ||
        !r.u32(images)) {
        return std::nullopt;
    }
    if (static_cast<std::uint64_t>(quads) * 20 + static_cast<std::uint64_t>(lines) * 20 >
        r.left()) {
        return std::nullopt;
    }
    NodePrimitives n;
    n.quads.reserve(quads);
    for (std::uint32_t i = 0; i < quads; ++i) {
        Quad q;
        if (!r.i32(q.x) || !r.i32(q.y) || !r.i32(q.w) || !r.i32(q.h) || !r.u32(q.rgba)) {
            return std::nullopt;
        }
        n.quads.push_back(q);
    }
    n.lines.reserve(lines);
    for (std::uint32_t i = 0; i < lines; ++i) {
        Line l;
        if (!r.i32(l.x1) || !r.i32(l.y1) || !r.i32(l.x2) || !r.i32(l.y2) || !r.u32(l.rgba)) {
            return std::nullopt;
        }
        n.lines.push_back(l);
    }
    for (std::uint32_t i = 0; i < texts; ++i) {
        TextRun t;
        std::uint32_t face_len = 0, len = 0;
        const std::uint8_t* face = nullptr;
        const std::uint8_t* at = nullptr;
        if (!r.i32(t.x) || !r.i32(t.y) || !r.u32(t.rgba) || !r.i32(t.cut_top) ||
            !r.i32(t.cut_foot) || !r.i32(t.advance) || !r.i32(t.line) || !r.i32(t.points) ||
            !r.i32(t.weight) || !r.u32(face_len) || !r.bytes(face_len, face) || !r.u32(len) ||
            !r.bytes(len, at)) {
            return std::nullopt;
        }
        t.face.assign(reinterpret_cast<const char*>(face), face_len);
        t.text.assign(reinterpret_cast<const char*>(at), len);
        n.texts.push_back(std::move(t));
    }
    for (std::uint32_t i = 0; i < images; ++i) {
        ImageBlit im;
        std::uint32_t len = 0;
        const std::uint8_t* at = nullptr;
        if (!r.i32(im.x) || !r.i32(im.y) || !r.i32(im.w) || !r.i32(im.h) || !r.i32(im.pw) ||
            !r.i32(im.ph) || !r.u32(len) || im.pw < 0 || im.ph < 0 ||
            static_cast<std::uint64_t>(im.pw) * static_cast<std::uint64_t>(im.ph) * 4 != len ||
            !r.bytes(len, at)) {
            return std::nullopt;
        }
        im.rgba.assign(at, at + len);
        n.images.push_back(std::move(im));
    }
    if (r.left() != 0) {
        return std::nullopt;
    }
    return n;
}

/// An exact 0xRRGGBB, opaque.
inline std::uint32_t rgba_of(std::int64_t rgb) {
    return (static_cast<std::uint32_t>(rgb & 0xffffff) << 8) | 0xffu;
}

class Painter {
public:
    /// A painter for `task`, setting text as `text` says: the window's face, measured.
    explicit Painter(const PaintTask& task, const TextMeasure& text = {})
        : task_(task), text_(text) {}

    /// One node's primitives, drawn in `space`, in its colours.
    void paint(const PaintNode& node, const term::Space& space, NodePrimitives& out) const {
        space_ = space;
        out_ = &out;
        inks_ = &node.inks;
        const Operands::Is& draw = node.draw.is;
        if (const auto* a = std::get_if<RectArgs>(&draw)) {
            rect(a->x, a->y, a->w, a->h);
        } else if (const auto* e = std::get_if<EllipseArgs>(&draw)) {
            ellipse(*e);
        } else if (const auto* l = std::get_if<LineArgs>(&draw)) {
            line(l->x1 + space_.dx, l->y1 + space_.dy, l->x2 + space_.dx, l->y2 + space_.dy,
                 ink_of(0));
        } else if (const auto* p = std::get_if<PathArgs>(&draw)) {
            path(*p);
        } else if (const auto* t = std::get_if<TextArgs>(&draw)) {
            const bool own = node.text.advance > 0 && node.text.line > 0;
            text(t->x, t->y, t->text, ink_of(0), own ? node.text.measure() : text_,
                 own ? node.text.points : 0, node.weight_of(t->weight), node.face);
        } else if (const auto* i = std::get_if<ImageArgs>(&draw)) {
            image(*i);
        }
    }

private:
    /// The node's colour `i`: its fill, its stroke, or its ink; no paint where it names none.
    const PaintInk& ink_of(std::size_t i) const {
        static const PaintInk kNoPaint{};
        return i < inks_->size() ? (*inks_)[i] : kNoPaint;
    }

    static std::uint32_t colour(const PaintInk& ink) { return static_cast<std::uint32_t>(ink.rgba); }

    /// A quad, cut to the node's clip.
    void quad(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h,
              std::uint32_t c) const {
        if (space_.clipped) {
            const std::int64_t x1 = std::min(x + w, space_.cx1);
            const std::int64_t y1 = std::min(y + h, space_.cy1);
            x = std::max(x, space_.cx0);
            y = std::max(y, space_.cy0);
            w = x1 - x;
            h = y1 - y;
        }
        if (w <= 0 || h <= 0) {
            return;
        }
        out_->quads.push_back(Quad{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
                                   static_cast<std::int32_t>(w), static_cast<std::int32_t>(h),
                                   c});
    }

    /// A rectangle in colours 0 (its fill) and 1 (its stroke).
    void rect(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) const {
        const PaintInk& fill = ink_of(0);
        const PaintInk& stroke = ink_of(1);
        // A FILL SHOWING WHAT LIES BENEATH IT -- a veil -- is blended as an image of its one colour
        // is, where a quad covers.
        if (!fill.none() && (colour(fill) & 0xffu) != 0xffu) {
            const std::uint32_t c = colour(fill);
            const loom::Bytes pixel{static_cast<std::uint8_t>(c >> 24),
                                    static_cast<std::uint8_t>(c >> 16),
                                    static_cast<std::uint8_t>(c >> 8), static_cast<std::uint8_t>(c)};
            image(ImageArgs{x, y, w, h, 1, 1, pixel});
        }
        x += space_.dx;
        y += space_.dy;
        if (!fill.none() && (colour(fill) & 0xffu) == 0xffu) {
            quad(x, y, w, h, colour(fill));
        }
        if (!stroke.none()) {
            const std::uint32_t c = colour(stroke);
            quad(x, y, w, 1, c);
            quad(x, y + h - 1, w, 1, c);
            quad(x, y, 1, h, c);
            quad(x + w - 1, y, 1, h, c);
        }
    }

    /// An ellipse in colours 0 (its fill) and 1 (its rim).
    void ellipse(const EllipseArgs& a) const {
        const PaintInk& fill = ink_of(0);
        const PaintInk& stroke = ink_of(1);
        const double x = static_cast<double>(a.x + space_.dx);
        const double y = static_cast<double>(a.y + space_.dy);
        const double rx = static_cast<double>(a.w) / 2.0, ry = static_cast<double>(a.h) / 2.0;
        const double cx = x + rx, cy = y + ry;
        if (rx <= 0 || ry <= 0) {
            return;
        }
        if (!fill.none()) {
            const std::uint32_t c = colour(fill);
            for (std::int64_t r = 0; r < a.h; ++r) {
                const double py = static_cast<double>(r) + 0.5 - ry;
                const double half = rx * std::sqrt(std::max(0.0, 1.0 - (py * py) / (ry * ry)));
                quad(std::llround(cx - half), static_cast<std::int64_t>(y) + r,
                     std::llround(2 * half), 1, c);
            }
        }
        if (!stroke.none()) {
            constexpr int kSteps = 48;
            constexpr double kTurn = 6.283185307179586;
            for (int i = 0; i < kSteps; ++i) {
                const double t0 = kTurn * i / kSteps, t1 = kTurn * (i + 1) / kSteps;
                line(std::llround(cx + rx * std::cos(t0)), std::llround(cy + ry * std::sin(t0)),
                     std::llround(cx + rx * std::cos(t1)), std::llround(cy + ry * std::sin(t1)),
                     stroke);
            }
        }
    }

    /// A line, cut to the node's clip (Liang and Barsky's way); wholly outside it, nothing.
    void line(std::int64_t x1, std::int64_t y1, std::int64_t x2, std::int64_t y2,
              const PaintInk& ink) const {
        if (ink.none()) {
            return;
        }
        if (space_.clipped) {
            double t0 = 0, t1 = 1;
            const double dx = static_cast<double>(x2 - x1), dy = static_cast<double>(y2 - y1);
            const double p[4] = {-dx, dx, -dy, dy};
            const double q[4] = {static_cast<double>(x1 - space_.cx0),
                                 static_cast<double>(space_.cx1 - 1 - x1),
                                 static_cast<double>(y1 - space_.cy0),
                                 static_cast<double>(space_.cy1 - 1 - y1)};
            for (int k = 0; k < 4; ++k) {
                if (p[k] == 0) {
                    if (q[k] < 0) {
                        return;
                    }
                    continue;
                }
                const double t = q[k] / p[k];
                if (p[k] < 0) {
                    t0 = std::max(t0, t);
                } else {
                    t1 = std::min(t1, t);
                }
            }
            if (t0 > t1) {
                return;
            }
            const std::int64_t nx1 = x1 + std::llround(t0 * dx), ny1 = y1 + std::llround(t0 * dy);
            const std::int64_t nx2 = x1 + std::llround(t1 * dx), ny2 = y1 + std::llround(t1 * dy);
            x1 = nx1;
            y1 = ny1;
            x2 = nx2;
            y2 = ny2;
        }
        out_->lines.push_back(Line{static_cast<std::int32_t>(x1), static_cast<std::int32_t>(y1),
                                   static_cast<std::int32_t>(x2), static_cast<std::int32_t>(y2),
                                   colour(ink)});
    }

    /// A fill by spans a pixel row (even-odd over every subpath), a stroke by lines.
    /// A path in colours 0 (its fill) and 1 (its stroke).
    void path(const PathArgs& a) const {
        const PaintInk& fill = ink_of(0);
        const PaintInk& stroke = ink_of(1);
        auto polys = term::flatten_path(a.d);
        for (term::Polygon& poly : polys) {
            for (auto& [px, py] : poly) {
                px += static_cast<double>(space_.dx);
                py += static_cast<double>(space_.dy);
            }
        }
        if (!fill.none() && !polys.empty()) {
            double miny = 1e18, maxy = -1e18;
            for (const term::Polygon& poly : polys) {
                for (const auto& [px, py] : poly) {
                    miny = std::min(miny, py);
                    maxy = std::max(maxy, py);
                }
            }
            const std::uint32_t c = colour(fill);
            for (auto y = static_cast<std::int64_t>(std::floor(miny));
                 y <= static_cast<std::int64_t>(std::floor(maxy)); ++y) {
                const double py = static_cast<double>(y) + 0.5;
                std::vector<double> xs;
                for (const term::Polygon& poly : polys) {
                    for (std::size_t k = 0, j = poly.size() - 1; k < poly.size(); j = k++) {
                        const auto [xi, yi] = poly[k];
                        const auto [xj, yj] = poly[j];
                        if ((yi > py) != (yj > py)) {
                            xs.push_back((xj - xi) * (py - yi) / (yj - yi) + xi);
                        }
                    }
                }
                std::sort(xs.begin(), xs.end());
                for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
                    const auto x0 = static_cast<std::int64_t>(std::llround(xs[k]));
                    const auto x1 = static_cast<std::int64_t>(std::llround(xs[k + 1]));
                    quad(x0, y, x1 - x0, 1, c);
                }
            }
        }
        if (!stroke.none()) {
            for (const term::Polygon& poly : polys) {
                for (std::size_t k = 1; k < poly.size(); ++k) {
                    line(std::llround(poly[k - 1].first), std::llround(poly[k - 1].second),
                         std::llround(poly[k].first), std::llround(poly[k].second), stroke);
                }
            }
        }
    }

    /// Every character as itself, in the columns the text core gives it (`character_columns`), a
    /// column every advance of `m`, in `face` at `points` (0 the window's own), at `weight`: a
    /// character of no column stays with the one before it, and a control character or bytes that
    /// spell none is a `?` in its column. A character a clip would cut is left out whole, and a
    /// line the clip crosses is cut by its pixels: the rows above or below the clip are said cut,
    /// and a line wholly outside it is not drawn.
    void text(std::int64_t x, std::int64_t y, const std::string& s, const PaintInk& ink,
              const TextMeasure& m, std::int64_t points, std::int64_t weight,
              const std::string& face) const {
        if (ink.none()) {
            return;
        }
        x += space_.dx;
        y += space_.dy;
        const std::int64_t tall = m.line > 0 ? m.line : 1;
        std::int64_t cut_top = 0, cut_foot = 0;
        if (space_.clipped) {
            if (y + tall <= space_.cy0 || y >= space_.cy1) {
                return;
            }
            cut_top = std::max<std::int64_t>(0, space_.cy0 - y);
            cut_foot = std::max<std::int64_t>(0, y + tall - space_.cy1);
        }
        std::string shown;
        std::int64_t at = x, first = x;
        bool kept = false; // whether the character before this one was shown
        for (std::size_t i = 0; i < s.size(); i = component::character_end(s, i)) {
            const std::size_t end = component::character_end(s, i);
            const int w = component::character_columns(s, i);
            if (w == 0) {
                if (kept) {
                    shown.append(s, i, end - i);
                }
                continue;
            }
            const char32_t cp = component::code_point_at(s, i);
            const bool drawable = cp != component::kNotACharacter && cp >= 0x20u &&
                                  !(cp >= 0x7Fu && cp < 0xA0u);
            const std::int64_t wide = m.advance * w;
            const bool inside = !space_.clipped || (at >= space_.cx0 && at + wide <= space_.cx1);
            if (inside) {
                if (shown.empty()) {
                    first = at;
                }
                if (drawable) {
                    shown.append(s, i, end - i);
                } else {
                    shown.push_back('?');
                }
            } else if (!shown.empty()) {
                break;
            }
            kept = inside;
            at += wide;
        }
        if (shown.empty()) {
            return;
        }
        TextRun run;
        run.x = static_cast<std::int32_t>(first);
        run.y = static_cast<std::int32_t>(y);
        run.rgba = colour(ink);
        run.text = shown;
        run.cut_top = static_cast<std::int32_t>(cut_top);
        run.cut_foot = static_cast<std::int32_t>(cut_foot);
        run.advance = static_cast<std::int32_t>(m.advance);
        run.line = static_cast<std::int32_t>(m.line);
        run.points = static_cast<std::int32_t>(points);
        run.weight = static_cast<std::int32_t>(weight);
        run.face = face;
        out_->texts.push_back(std::move(run));
    }

    /// An image in its box, cut to the clip: the pixels of the part shown kept, scaled into it.
    void image(const ImageArgs& a) const {
        if (a.pw <= 0 || a.ph <= 0 || a.w <= 0 || a.h <= 0 ||
            a.rgba.size() < static_cast<std::size_t>(a.pw * a.ph * 4)) {
            return;
        }
        const std::int64_t x = a.x + space_.dx, y = a.y + space_.dy;
        std::int64_t vx0 = x, vy0 = y, vx1 = x + a.w, vy1 = y + a.h;
        if (space_.clipped) {
            vx0 = std::max(vx0, space_.cx0);
            vy0 = std::max(vy0, space_.cy0);
            vx1 = std::min(vx1, space_.cx1);
            vy1 = std::min(vy1, space_.cy1);
        }
        if (vx1 <= vx0 || vy1 <= vy0) {
            return;
        }
        const std::int64_t sx0 = (vx0 - x) * a.pw / a.w;
        const std::int64_t sy0 = (vy0 - y) * a.ph / a.h;
        const std::int64_t sx1 = std::max(sx0 + 1, ((vx1 - x) * a.pw + a.w - 1) / a.w);
        const std::int64_t sy1 = std::max(sy0 + 1, ((vy1 - y) * a.ph + a.h - 1) / a.h);
        ImageBlit im{static_cast<std::int32_t>(vx0), static_cast<std::int32_t>(vy0),
                     static_cast<std::int32_t>(vx1 - vx0), static_cast<std::int32_t>(vy1 - vy0),
                     static_cast<std::int32_t>(std::min(sx1, a.pw) - sx0),
                     static_cast<std::int32_t>(std::min(sy1, a.ph) - sy0), {}};
        im.rgba.reserve(static_cast<std::size_t>(im.pw) * static_cast<std::size_t>(im.ph) * 4);
        for (std::int64_t py = sy0; py < sy0 + im.ph; ++py) {
            const auto row = static_cast<std::size_t>((py * a.pw + sx0) * 4);
            im.rgba.insert(im.rgba.end(), a.rgba.begin() + static_cast<std::ptrdiff_t>(row),
                           a.rgba.begin() + static_cast<std::ptrdiff_t>(row) + im.pw * 4);
        }
        out_->images.push_back(std::move(im));
    }

    const PaintTask& task_;
    TextMeasure text_;
    mutable term::Space space_;
    mutable NodePrimitives* out_ = nullptr;
    mutable const std::vector<PaintInk>* inks_ = nullptr;
};

/// The measure a task asks text set by: its own, or the cell's where it says none.
inline TextMeasure measure_of(const PaintTask& task) {
    return TextMeasure{task.text_advance > 0 ? task.text_advance : kCellPx,
                       task.text_line > 0 ? task.text_line : kCellPx};
}

/// A whole task painted for light: each node but a group, its primitives packed in its cells, its
/// text set by the task's measure.
inline PaintResult paint_task(const PaintTask& task) {
    const Painter painter(task, measure_of(task));
    const auto at = term::spaces(task.nodes);
    PaintResult out;
    for (const PaintNode& n : task.nodes) {
        if (operands<GroupArgs>(n) != nullptr) {
            continue;
        }
        NodePrimitives prims;
        painter.paint(n, at.at(n.path), prims);
        out.nodes.push_back(NodePaint{n.path, pack_node(prims)});
    }
    return out;
}

/// Whether a point of the area, in its pixels, lands on what a node painted: inside a quad, a
/// run of text's box (its own advance a column, a line of it tall, or `text`'s where it carries
/// none) or an image's, or within a pixel of a line.
inline bool hits_node(const NodePrimitives& n, std::int64_t x, std::int64_t y,
                      const TextMeasure& text) {
    const auto inside = [&](std::int64_t bx, std::int64_t by, std::int64_t bw, std::int64_t bh) {
        return x >= bx && y >= by && x < bx + bw && y < by + bh;
    };
    for (const Quad& q : n.quads) {
        if (inside(q.x, q.y, q.w, q.h)) {
            return true;
        }
    }
    for (const TextRun& t : n.texts) {
        const std::int64_t advance = t.advance > 0 ? t.advance : text.advance;
        const std::int64_t line = t.line > 0 ? t.line : text.line;
        if (inside(t.x, t.y,
                   static_cast<std::int64_t>(component::text_columns(t.text)) * advance, line)) {
            return true;
        }
    }
    for (const ImageBlit& im : n.images) {
        if (inside(im.x, im.y, im.w, im.h)) {
            return true;
        }
    }
    for (const Line& l : n.lines) {
        const double dx = l.x2 - l.x1, dy = l.y2 - l.y1;
        const double len2 = dx * dx + dy * dy;
        const double px = static_cast<double>(x) - l.x1, py = static_cast<double>(y) - l.y1;
        const double t = len2 > 0 ? std::clamp((px * dx + py * dy) / len2, 0.0, 1.0) : 0.0;
        const double ex = t * dx - px, ey = t * dy - py;
        if (ex * ex + ey * ey <= 1.0) {
            return true;
        }
    }
    return false;
}

} // namespace zengine::graphics::light

#endif // ZENGINE_GRAPHICS_LIGHT_PAINT_HPP
