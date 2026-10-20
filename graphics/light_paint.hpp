// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_LIGHT_PAINT_HPP
#define ZENGINE_GRAPHICS_LIGHT_PAINT_HPP

// Painting nodes for a medium of light: every operator, in the area's own pixels, into the
// primitives a renderer draws -- filled rectangles (quads), lines, runs of text and images -- each
// in an exact colour: a paint's own, else the look's for its role. A node paints in its parent
// groups' space and clip. A node's primitives cross the painter seam packed in its `NodePaint`'s
// `cells` (`pack_node`), behind a tag that tells them from a character painter's runs. A skin for
// light is this painter and a Look (graphics/painter_export.hpp); it keeps nothing between calls.
// Reference: graphics/docs/graphics.md, "The operators".

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

/// The width of one character of text and the height of a line: a cell's, so text stands one cell
/// a character, as a window of light reads it and as every owner sets text in cells.
inline constexpr std::int64_t kAdvance = kCellPx;
inline constexpr std::int64_t kLine = kCellPx;

/// The tag that opens a node painted for light: 'L', 'G', 'T' and this layout's version.
inline constexpr std::array<std::uint8_t, 4> kTag = {'L', 'G', 'T', 1};

/// A filled rectangle; a line; a run of text from the top left of its line; an image of `pw` x
/// `ph` pixels (red, green, blue, alpha) shown in (x, y, w, h). Every colour is 0xRRGGBBAA.
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
    bool operator==(const TextRun&) const = default;
};
struct ImageBlit {
    std::int32_t x = 0, y = 0, w = 0, h = 0, pw = 0, ph = 0;
    loom::Bytes rgba;
    bool operator==(const ImageBlit&) const = default;
};

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
/// line as x1, y1, x2, y2 and its RGBA, 20 bytes; each text as x, y, its RGBA, its length and its
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
        std::uint32_t len = 0;
        const std::uint8_t* at = nullptr;
        if (!r.i32(t.x) || !r.i32(t.y) || !r.u32(t.rgba) || !r.u32(len) || !r.bytes(len, at)) {
            return std::nullopt;
        }
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

/// A look for light: each role's colour (0xRRGGBB), and the ground the window is cleared to.
struct Look {
    std::map<std::int64_t, std::int64_t> colour;
    std::int64_t ground = 0x101418;
};

/// The day look: dark ink on a light ground.
inline Look day_look() {
    Look l;
    l.colour = {{role::kFill, 0x2b2b2b},   {role::kAccent, 0xd08700},
                {role::kMuted, 0x9aa0a6},  {role::kAlert, 0xc5221f},
                {role::kGround, 0xf5f5f0}, {role::kMediumGround, 0xf5f5f0},
                {role::kGuide, 0x1a73e8}};
    l.ground = 0xf5f5f0;
    return l;
}

/// The night look: light ink on a dark ground.
inline Look night_look() {
    Look l;
    l.colour = {{role::kFill, 0xd8dee9},   {role::kAccent, 0xebcb8b},
                {role::kMuted, 0x4c566a},  {role::kAlert, 0xbf616a},
                {role::kGround, 0x2e3440}, {role::kMediumGround, 0x101418},
                {role::kGuide, 0x88c0d0}};
    l.ground = 0x101418;
    return l;
}

/// An exact 0xRRGGBB, opaque.
inline std::uint32_t rgba_of(std::int64_t rgb) {
    return (static_cast<std::uint32_t>(rgb & 0xffffff) << 8) | 0xffu;
}

class Painter {
public:
    explicit Painter(const Look& look) : look_(look) {}

    /// One node's primitives, drawn in `space`. Operands that do not read as their operator's
    /// paint nothing; an operator this medium does not know paints its name, muted.
    void paint(const std::string& op, const loom::Bytes& args, const term::Space& space,
               NodePrimitives& out) const {
        space_ = space;
        out_ = &out;
        if (op == op::kRect) {
            if (auto a = from_body<RectArgs>(args)) {
                rect(a->x, a->y, a->w, a->h, a->fill, a->stroke);
            }
        } else if (op == op::kEllipse) {
            if (auto a = from_body<EllipseArgs>(args)) {
                ellipse(*a);
            }
        } else if (op == op::kLine) {
            if (auto a = from_body<LineArgs>(args)) {
                line(a->x1 + space_.dx, a->y1 + space_.dy, a->x2 + space_.dx, a->y2 + space_.dy,
                     a->ink);
            }
        } else if (op == op::kPath) {
            if (auto a = from_body<PathArgs>(args)) {
                path(*a);
            }
        } else if (op == op::kText) {
            if (auto a = from_body<TextArgs>(args)) {
                text(a->x, a->y, a->text, a->ink);
            }
        } else if (op == op::kImage) {
            if (auto a = from_body<ImageArgs>(args)) {
                image(*a);
            }
        } else if (op != op::kGroup) {
            text(0, 0, "?" + op, by_role(role::kMuted));
        }
    }

private:
    /// A paint's colour: its own, else its role's in the look, else the look's fill.
    std::uint32_t colour(const Paint& p) const {
        if (p.rgb >= 0) {
            return rgba_of(p.rgb);
        }
        auto it = look_.colour.find(p.role);
        if (it == look_.colour.end()) {
            it = look_.colour.find(role::kFill);
        }
        return rgba_of(it != look_.colour.end() ? it->second : 0xc8c8c8);
    }

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

    void rect(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h, const Paint& fill,
              const Paint& stroke) const {
        x += space_.dx;
        y += space_.dy;
        if (!fill.none()) {
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

    void ellipse(const EllipseArgs& a) const {
        const double x = static_cast<double>(a.x + space_.dx);
        const double y = static_cast<double>(a.y + space_.dy);
        const double rx = static_cast<double>(a.w) / 2.0, ry = static_cast<double>(a.h) / 2.0;
        const double cx = x + rx, cy = y + ry;
        if (rx <= 0 || ry <= 0) {
            return;
        }
        if (!a.fill.none()) {
            const std::uint32_t c = colour(a.fill);
            for (std::int64_t r = 0; r < a.h; ++r) {
                const double py = static_cast<double>(r) + 0.5 - ry;
                const double half = rx * std::sqrt(std::max(0.0, 1.0 - (py * py) / (ry * ry)));
                quad(std::llround(cx - half), static_cast<std::int64_t>(y) + r,
                     std::llround(2 * half), 1, c);
            }
        }
        if (!a.stroke.none()) {
            constexpr int kSteps = 48;
            constexpr double kTurn = 6.283185307179586;
            for (int i = 0; i < kSteps; ++i) {
                const double t0 = kTurn * i / kSteps, t1 = kTurn * (i + 1) / kSteps;
                line(std::llround(cx + rx * std::cos(t0)), std::llround(cy + ry * std::sin(t0)),
                     std::llround(cx + rx * std::cos(t1)), std::llround(cy + ry * std::sin(t1)),
                     a.stroke);
            }
        }
    }

    /// A line, cut to the node's clip (Liang and Barsky's way); wholly outside it, nothing.
    void line(std::int64_t x1, std::int64_t y1, std::int64_t x2, std::int64_t y2,
              const Paint& ink) const {
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
    void path(const PathArgs& a) const {
        auto polys = term::flatten_path(a.d);
        for (term::Polygon& poly : polys) {
            for (auto& [px, py] : poly) {
                px += static_cast<double>(space_.dx);
                py += static_cast<double>(space_.dy);
            }
        }
        if (!a.fill.none() && !polys.empty()) {
            double miny = 1e18, maxy = -1e18;
            for (const term::Polygon& poly : polys) {
                for (const auto& [px, py] : poly) {
                    miny = std::min(miny, py);
                    maxy = std::max(maxy, py);
                }
            }
            const std::uint32_t c = colour(a.fill);
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
        if (!a.stroke.none()) {
            for (const term::Polygon& poly : polys) {
                for (std::size_t k = 1; k < poly.size(); ++k) {
                    line(std::llround(poly[k - 1].first), std::llround(poly[k - 1].second),
                         std::llround(poly[k].first), std::llround(poly[k].second), a.stroke);
                }
            }
        }
    }

    /// Printable ASCII as itself, any other character (all of its UTF-8 bytes) as one '?', a
    /// character every `kAdvance` pixels; a character outside the clip is left out, and a line
    /// whose top is outside it is not drawn. Text in the medium's ground paints as `kFill`.
    void text(std::int64_t x, std::int64_t y, const std::string& s, const Paint& ink) const {
        if (ink.none()) {
            return;
        }
        x += space_.dx;
        y += space_.dy;
        if (space_.clipped && (y < space_.cy0 || y >= space_.cy1)) {
            return;
        }
        std::string shown;
        std::int64_t at = x, first = x;
        for (const char raw : s) {
            const auto ch = static_cast<unsigned char>(raw);
            if (ch >= 0x80 && ch < 0xc0) {
                continue;
            }
            const char glyph = (ch >= 0x20 && ch < 0x7f) ? static_cast<char>(ch) : '?';
            const bool inside =
                !space_.clipped || (at >= space_.cx0 && at + kAdvance <= space_.cx1);
            if (inside) {
                if (shown.empty()) {
                    first = at;
                }
                shown.push_back(glyph);
            } else if (!shown.empty()) {
                break;
            }
            at += kAdvance;
        }
        if (shown.empty()) {
            return;
        }
        const bool ground = ink.rgb < 0 && ink.role == role::kMediumGround;
        out_->texts.push_back(TextRun{static_cast<std::int32_t>(first),
                                      static_cast<std::int32_t>(y),
                                      colour(ground ? by_role(role::kFill) : ink), shown});
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

    Look look_;
    mutable term::Space space_;
    mutable NodePrimitives* out_ = nullptr;
};

/// A whole task painted for light: each node but a group, its primitives packed in its cells.
inline PaintResult paint_task(const PaintTask& task, const Look& look) {
    const Painter painter(look);
    const auto at = term::spaces(task.nodes);
    PaintResult out;
    for (const PaintNode& n : task.nodes) {
        if (n.op == op::kGroup) {
            continue;
        }
        NodePrimitives prims;
        painter.paint(n.op, n.args, at.at(n.path), prims);
        out.nodes.push_back(NodePaint{n.path, pack_node(prims)});
    }
    return out;
}

/// Whether a point of the area, in its pixels, lands on what a node painted: inside a quad, a
/// run of text's box (`kAdvance` a character, `kLine` tall) or an image's, or within a pixel of
/// a line.
inline bool hits_node(const NodePrimitives& n, std::int64_t x, std::int64_t y) {
    const auto inside = [&](std::int64_t bx, std::int64_t by, std::int64_t bw, std::int64_t bh) {
        return x >= bx && y >= by && x < bx + bw && y < by + bh;
    };
    for (const Quad& q : n.quads) {
        if (inside(q.x, q.y, q.w, q.h)) {
            return true;
        }
    }
    for (const TextRun& t : n.texts) {
        if (inside(t.x, t.y, static_cast<std::int64_t>(t.text.size()) * kAdvance, kLine)) {
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
