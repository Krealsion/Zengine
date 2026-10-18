// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_TERM_PAINT_HPP
#define ZENGINE_GRAPHICS_TERM_PAINT_HPP

// Painting nodes for a medium of characters: every operator, natively or by its fallback, into the
// area's cells (`kCellPx` pixels a cell, each edge floored to its cell). A skin for a terminal is
// this painter and a Look, its glyphs (graphics/painter_export.hpp); it keeps nothing between calls.
// Reference: graphics/docs/graphics.md, "The operators".

#include "graphics/implied.hpp"
#include "graphics/ops.hpp"
#include "graphics/painting.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace zengine::graphics::term {

struct Cell {
    std::int64_t col = 0, row = 0;
    char glyph = ' ';
    std::int64_t role = role::kFill;
};

/// Cells packed as runs: each run a row's neighbouring cells of one role -- its column and row as
/// two bytes each, low byte first, the role, the count (1 to 255), then that many glyphs.
inline loom::Bytes pack(const std::vector<Cell>& cells) {
    loom::Bytes out;
    std::size_t i = 0;
    while (i < cells.size()) {
        std::size_t n = 1;
        while (i + n < cells.size() && n < 255 && cells[i + n].row == cells[i].row &&
               cells[i + n].col == cells[i].col + static_cast<std::int64_t>(n) &&
               cells[i + n].role == cells[i].role) {
            ++n;
        }
        const Cell& c = cells[i];
        out.push_back(static_cast<std::uint8_t>(c.col & 0xff));
        out.push_back(static_cast<std::uint8_t>((c.col >> 8) & 0xff));
        out.push_back(static_cast<std::uint8_t>(c.row & 0xff));
        out.push_back(static_cast<std::uint8_t>((c.row >> 8) & 0xff));
        out.push_back(static_cast<std::uint8_t>(c.role & 0xff));
        out.push_back(static_cast<std::uint8_t>(n));
        for (std::size_t k = 0; k < n; ++k) {
            out.push_back(static_cast<std::uint8_t>(cells[i + k].glyph));
        }
        i += n;
    }
    return out;
}

inline std::vector<Cell> unpack(const loom::Bytes& bytes) {
    std::vector<Cell> out;
    std::size_t i = 0;
    while (i + 6 <= bytes.size()) {
        const std::int64_t col = static_cast<std::int64_t>(bytes[i] | (bytes[i + 1] << 8));
        const std::int64_t row = static_cast<std::int64_t>(bytes[i + 2] | (bytes[i + 3] << 8));
        const std::int64_t role = static_cast<std::int64_t>(static_cast<std::int8_t>(bytes[i + 4]));
        const std::size_t n = bytes[i + 5];
        i += 6;
        for (std::size_t k = 0; k < n && i < bytes.size(); ++k, ++i) {
            out.push_back(Cell{col + static_cast<std::int64_t>(k), row,
                               static_cast<char>(bytes[i]), role});
        }
    }
    return out;
}

/// A look: a glyph for each role's fill, the glyphs an outline wears, and whether it shows roles
/// in colour or every cell in the terminal's own ink.
struct Look {
    std::map<std::int64_t, char> fill;
    char corner = '+', across = '-', down = '|';
    bool mono = false;
};

/// A path flattened: each subpath a polygon of points.
using Polygon = std::vector<std::pair<double, double>>;

/// SVG path data (M L H V C Q Z, absolute and relative), flattened to polygons in the path's own
/// pixels; a command outside that subset ends the path there. Every painter reads a path so.
inline std::vector<Polygon> flatten_path(const std::string& d) {
    std::vector<Polygon> polys;
    Polygon cur;
    double x = 0, y = 0, sx = 0, sy = 0;
    std::size_t i = 0;
    char cmd = 0;
    const auto skip = [&] {
        while (i < d.size() &&
               (std::isspace(static_cast<unsigned char>(d[i])) != 0 || d[i] == ',')) {
            ++i;
        }
    };
    const auto number = [&](double& out) {
        skip();
        const char* start = d.c_str() + i;
        char* end = nullptr;
        out = std::strtod(start, &end);
        if (end == start) {
            return false;
        }
        i += static_cast<std::size_t>(end - start);
        return true;
    };
    const auto close = [&] {
        if (!cur.empty()) {
            polys.push_back(cur);
            cur.clear();
        }
    };
    for (int guard = 0; guard < 100000; ++guard) {
        skip();
        if (i >= d.size()) {
            break;
        }
        if (std::isalpha(static_cast<unsigned char>(d[i])) != 0) {
            cmd = d[i++];
        }
        const bool rel = std::islower(static_cast<unsigned char>(cmd)) != 0;
        const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));
        double a = 0, b = 0, c = 0, e = 0, f = 0, g = 0;
        if (up == 'Z') {
            close();
            x = sx;
            y = sy;
            if (i < d.size() && std::isalpha(static_cast<unsigned char>(d[i])) == 0) {
                ++i;
            }
            continue;
        }
        if (up == 'M' && number(a) && number(b)) {
            close();
            x = rel ? x + a : a;
            y = rel ? y + b : b;
            sx = x;
            sy = y;
            cur.push_back({x, y});
            cmd = rel ? 'l' : 'L';
        } else if (up == 'L' && number(a) && number(b)) {
            x = rel ? x + a : a;
            y = rel ? y + b : b;
            cur.push_back({x, y});
        } else if (up == 'H' && number(a)) {
            x = rel ? x + a : a;
            cur.push_back({x, y});
        } else if (up == 'V' && number(a)) {
            y = rel ? y + a : a;
            cur.push_back({x, y});
        } else if (up == 'C' && number(a) && number(b) && number(c) && number(e) &&
                   number(f) && number(g)) {
            const double x0 = x, y0 = y;
            const double x1 = rel ? x + a : a, y1 = rel ? y + b : b;
            const double x2 = rel ? x + c : c, y2 = rel ? y + e : e;
            const double x3 = rel ? x + f : f, y3 = rel ? y + g : g;
            for (int k = 1; k <= 16; ++k) {
                const double t = k / 16.0, u = 1 - t;
                cur.push_back({u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 +
                                   t * t * t * x3,
                               u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 +
                                   t * t * t * y3});
            }
            x = x3;
            y = y3;
        } else if (up == 'Q' && number(a) && number(b) && number(c) && number(e)) {
            const double x0 = x, y0 = y;
            const double x1 = rel ? x + a : a, y1 = rel ? y + b : b;
            const double x2 = rel ? x + c : c, y2 = rel ? y + e : e;
            for (int k = 1; k <= 16; ++k) {
                const double t = k / 16.0, u = 1 - t;
                cur.push_back({u * u * x0 + 2 * u * t * x1 + t * t * x2,
                               u * u * y0 + 2 * u * t * y1 + t * t * y2});
            }
            x = x2;
            y = y2;
        } else {
            break;
        }
    }
    close();
    return polys;
}

inline std::int64_t floor_cell(std::int64_t v) {
    return v >= 0 ? v / kCellPx : -((-v + kCellPx - 1) / kCellPx);
}

/// Where a node is drawn: its offset from the area's 0,0, and its clip in the area's pixels.
struct Space {
    std::int64_t dx = 0, dy = 0;
    bool clipped = false;
    std::int64_t cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;
};

class Painter {
public:
    explicit Painter(const Look& look) : look_(look) {}

    /// One node's cells, drawn in `space`. Operands that do not read as their operator's paint
    /// nothing; an operator this medium does not know paints its name, muted.
    void paint(const std::string& op, const loom::Bytes& args, const Space& space,
               std::vector<Cell>& cells) const {
        space_ = space;
        out_ = &cells;
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
                line(a->x1, a->y1, a->x2, a->y2, a->ink);
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
    char glyph_of(const Paint& p) const {
        const auto it = look_.fill.find(p.role);
        return it != look_.fill.end() ? it->second : '#';
    }

    void put(std::int64_t col, std::int64_t row, char glyph, std::int64_t r) const {
        if (space_.clipped && (col < floor_cell(space_.cx0) || col >= floor_cell(space_.cx1) ||
                               row < floor_cell(space_.cy0) || row >= floor_cell(space_.cy1))) {
            return;
        }
        if (col < 0 || row < 0) {
            return;
        }
        out_->push_back(Cell{col, row, glyph, look_.mono || r < 0 ? role::kFill : r});
    }

    void rect(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h, const Paint& fill,
              const Paint& stroke) const {
        x += space_.dx;
        y += space_.dy;
        const std::int64_t c0 = floor_cell(x), c1 = floor_cell(x + w);
        const std::int64_t r0 = floor_cell(y), r1 = floor_cell(y + h);
        for (std::int64_t r = r0; r < r1; ++r) {
            for (std::int64_t c = c0; c < c1; ++c) {
                const bool edge = r == r0 || r == r1 - 1 || c == c0 || c == c1 - 1;
                if (edge && !stroke.none()) {
                    const bool across = r == r0 || r == r1 - 1;
                    const bool down = c == c0 || c == c1 - 1;
                    put(c, r, across && down ? look_.corner : across ? look_.across : look_.down,
                        stroke.role);
                } else if (!fill.none()) {
                    put(c, r, glyph_of(fill), fill.role);
                }
            }
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
        const auto inside = [&](std::int64_t c, std::int64_t r) {
            const double px = (static_cast<double>(c) + 0.5) * kCellPx;
            const double py = (static_cast<double>(r) + 0.5) * kCellPx;
            const double u = (px - cx) / rx, v = (py - cy) / ry;
            return u * u + v * v <= 1.0;
        };
        for (std::int64_t r = floor_cell(a.y + space_.dy); r <= floor_cell(a.y + space_.dy + a.h);
             ++r) {
            for (std::int64_t c = floor_cell(a.x + space_.dx);
                 c <= floor_cell(a.x + space_.dx + a.w); ++c) {
                if (!inside(c, r)) {
                    continue;
                }
                const bool rim = !inside(c - 1, r) || !inside(c + 1, r) || !inside(c, r - 1) ||
                                 !inside(c, r + 1);
                if (rim && !a.stroke.none()) {
                    put(c, r, 'o', a.stroke.role);
                } else if (!a.fill.none()) {
                    put(c, r, glyph_of(a.fill), a.fill.role);
                }
            }
        }
    }

    void line(std::int64_t x1, std::int64_t y1, std::int64_t x2, std::int64_t y2,
              const Paint& ink) const {
        if (ink.none()) {
            return;
        }
        std::int64_t c0 = floor_cell(x1 + space_.dx), r0 = floor_cell(y1 + space_.dy);
        const std::int64_t c1 = floor_cell(x2 + space_.dx), r1 = floor_cell(y2 + space_.dy);
        const std::int64_t dx = std::llabs(c1 - c0), dy = -std::llabs(r1 - r0);
        const std::int64_t sx = c0 < c1 ? 1 : -1, sy = r0 < r1 ? 1 : -1;
        char g = '-';
        const std::int64_t adx = std::llabs(x2 - x1), ady = std::llabs(y2 - y1);
        if (ady > 2 * adx) {
            g = '|';
        } else if (adx <= 2 * ady) {
            g = ((x2 - x1) * (y2 - y1) > 0) ? '\\' : '/';
        }
        std::int64_t err = dx + dy;
        for (int guard = 0; guard < 100000; ++guard) {
            put(c0, r0, g, ink.role);
            if (c0 == c1 && r0 == r1) {
                break;
            }
            const std::int64_t e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                c0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                r0 += sy;
            }
        }
    }

    using Polygon = term::Polygon;

    void path(const PathArgs& a) const {
        auto polys = flatten_path(a.d);
        for (Polygon& poly : polys) {
            for (auto& [px, py] : poly) {
                px += static_cast<double>(space_.dx);
                py += static_cast<double>(space_.dy);
            }
        }
        if (!a.fill.none()) {
            double minx = 1e18, miny = 1e18, maxx = -1e18, maxy = -1e18;
            for (const Polygon& poly : polys) {
                for (const auto& [px, py] : poly) {
                    minx = std::min(minx, px);
                    miny = std::min(miny, py);
                    maxx = std::max(maxx, px);
                    maxy = std::max(maxy, py);
                }
            }
            for (std::int64_t r = floor_cell(static_cast<std::int64_t>(miny));
                 r <= floor_cell(static_cast<std::int64_t>(maxy)); ++r) {
                for (std::int64_t c = floor_cell(static_cast<std::int64_t>(minx));
                     c <= floor_cell(static_cast<std::int64_t>(maxx)); ++c) {
                    const double px = (static_cast<double>(c) + 0.5) * kCellPx;
                    const double py = (static_cast<double>(r) + 0.5) * kCellPx;
                    bool in = false; // even-odd, over every subpath
                    for (const Polygon& poly : polys) {
                        for (std::size_t k = 0, j = poly.size() - 1; k < poly.size(); j = k++) {
                            const auto [xi, yi] = poly[k];
                            const auto [xj, yj] = poly[j];
                            if (((yi > py) != (yj > py)) &&
                                (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
                                in = !in;
                            }
                        }
                    }
                    if (in) {
                        put(c, r, glyph_of(a.fill), a.fill.role);
                    }
                }
            }
        }
        if (!a.stroke.none()) {
            const Space keep = space_;
            space_.dx = 0;
            space_.dy = 0;
            for (const Polygon& poly : polys) {
                for (std::size_t k = 1; k < poly.size(); ++k) {
                    line(static_cast<std::int64_t>(poly[k - 1].first),
                         static_cast<std::int64_t>(poly[k - 1].second),
                         static_cast<std::int64_t>(poly[k].first),
                         static_cast<std::int64_t>(poly[k].second), a.stroke);
                }
            }
            space_ = keep;
        }
    }

    /// One cell a character: printable ASCII as itself, any other character (all of its UTF-8
    /// bytes) as one '?'.
    void text(std::int64_t x, std::int64_t y, const std::string& s, const Paint& ink) const {
        const std::int64_t r = floor_cell(y + space_.dy);
        std::int64_t col = floor_cell(x + space_.dx);
        for (const char raw : s) {
            const auto ch = static_cast<unsigned char>(raw);
            if (ch >= 0x80 && ch < 0xc0) {
                continue;
            }
            put(col, r, (ch >= 0x20 && ch < 0x7f) ? static_cast<char>(ch) : '?', ink.role);
            ++col;
        }
    }

    /// A luminance ramp, one glyph a cell; a cell whose pixels are mostly transparent is left out.
    void image(const ImageArgs& a) const {
        if (a.pw <= 0 || a.ph <= 0 || a.w <= 0 || a.h <= 0 ||
            a.rgba.size() < static_cast<std::size_t>(a.pw * a.ph * 4)) {
            return;
        }
        static const char ramp[] = " .:-=+*#%@";
        const std::int64_t x = a.x + space_.dx, y = a.y + space_.dy;
        for (std::int64_t r = floor_cell(y); r < floor_cell(y + a.h); ++r) {
            for (std::int64_t c = floor_cell(x); c < floor_cell(x + a.w); ++c) {
                const std::int64_t sx0 = std::max<std::int64_t>(0, (c * kCellPx - x) * a.pw / a.w);
                const std::int64_t sx1 = std::min<std::int64_t>(
                    a.pw, std::max(sx0 + 1, ((c + 1) * kCellPx - x) * a.pw / a.w));
                const std::int64_t sy0 = std::max<std::int64_t>(0, (r * kCellPx - y) * a.ph / a.h);
                const std::int64_t sy1 = std::min<std::int64_t>(
                    a.ph, std::max(sy0 + 1, ((r + 1) * kCellPx - y) * a.ph / a.h));
                double lum = 0, alpha = 0;
                std::int64_t n = 0;
                for (std::int64_t py = sy0; py < sy1; ++py) {
                    for (std::int64_t px = sx0; px < sx1; ++px) {
                        const auto at = static_cast<std::size_t>((py * a.pw + px) * 4);
                        lum += 0.299 * a.rgba[at] + 0.587 * a.rgba[at + 1] + 0.114 * a.rgba[at + 2];
                        alpha += a.rgba[at + 3];
                        ++n;
                    }
                }
                if (n == 0 || alpha / static_cast<double>(n) < 128) {
                    continue;
                }
                const int level = static_cast<int>((lum / static_cast<double>(n)) * 9.0 / 255.0 + 0.5);
                put(c, r, ramp[std::clamp(level, 0, 9)], role::kFill);
            }
        }
    }

    Look look_;
    mutable Space space_;
    mutable std::vector<Cell>* out_ = nullptr;
};

/// Each node's space, from the groups along its path (a task carries every ancestor).
inline std::map<std::string, Space> spaces(const std::vector<PaintNode>& nodes) {
    std::map<std::string, const PaintNode*> at;
    for (const PaintNode& n : nodes) {
        at[n.path] = &n;
    }
    std::map<std::string, Space> out;
    for (const PaintNode& n : nodes) {
        std::vector<std::string> chain;
        for (std::string p = parent_of(n.path); !p.empty(); p = parent_of(p)) {
            chain.push_back(p);
        }
        Space s;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const auto g = at.find(*it);
            if (g == at.end() || g->second->op != op::kGroup) {
                continue;
            }
            const auto args = from_body<GroupArgs>(g->second->args);
            if (!args) {
                continue;
            }
            s.dx += args->x;
            s.dy += args->y;
            if (args->clip_w > 0 && args->clip_h > 0) {
                const std::int64_t x0 = s.dx, y0 = s.dy;
                const std::int64_t x1 = s.dx + args->clip_w, y1 = s.dy + args->clip_h;
                if (!s.clipped) {
                    s = Space{s.dx, s.dy, true, x0, y0, x1, y1};
                } else {
                    s.cx0 = std::max(s.cx0, x0);
                    s.cy0 = std::max(s.cy0, y0);
                    s.cx1 = std::min(s.cx1, x1);
                    s.cy1 = std::min(s.cy1, y1);
                }
            }
        }
        out[n.path] = s;
    }
    return out;
}

/// A whole task painted: each node but a group, with its cells.
inline PaintResult paint_task(const PaintTask& task, const Look& look) {
    const Painter painter(look);
    const auto at = spaces(task.nodes);
    PaintResult out;
    for (const PaintNode& n : task.nodes) {
        if (n.op == op::kGroup) {
            continue;
        }
        std::vector<Cell> cells;
        painter.paint(n.op, n.args, at.at(n.path), cells);
        out.nodes.push_back(NodePaint{n.path, pack(cells)});
    }
    return out;
}

} // namespace zengine::graphics::term

#endif // ZENGINE_GRAPHICS_TERM_PAINT_HPP
