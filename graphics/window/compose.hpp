// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_COMPOSE_HPP
#define ZENGINE_GRAPHICS_WINDOW_COMPOSE_HPP

// A window's picture: each node painted once, when it changes, by its area's skin, and kept
// painted, so a change paints only what changed, and the composition and a press read what was
// painted. What it keeps painted is the window's memory, never its state: a reload paints it
// again from the nodes its state keeps. Reference: graphics/docs/zengine.window.terminal.md.

#include "graphics/term_paint.hpp"
#include "graphics/window/store.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace zengine::graphics::window {

using CellAt = std::pair<std::int64_t, std::int64_t>; ///< a column and a row of the window

/// Where an area's cells stand among its window's.
struct Place {
    std::int64_t col = 0, row = 0, cols = 0, rows = 0;
    bool holds(std::int64_t c, std::int64_t r) const {
        return c >= col && c < col + cols && r >= row && r < row + rows;
    }
};

inline Place place_of(const WinArea& a) {
    const std::int64_t c0 = term::floor_cell(a.x), r0 = term::floor_cell(a.y);
    return Place{c0, r0, term::floor_cell(a.x + a.w) - c0, term::floor_cell(a.y + a.h) - r0};
}

/// One node as painted: its place in the area's paint order, and its cells, cut to the area.
struct PaintedNode {
    std::vector<std::int64_t> key;
    std::vector<term::Cell> cells;
};
using Painted = std::map<std::string, PaintedNode>;

/// What the window keeps painted: each area's painted nodes.
class Cache {
public:
    void set(const std::string& area, const std::string& path, PaintedNode n) {
        by_area_[area][path] = std::move(n);
    }
    void drop(const std::string& area, const std::string& path) {
        const auto it = by_area_.find(area);
        if (it != by_area_.end()) {
            it->second.erase(path);
        }
    }
    void drop_area(const std::string& area) { by_area_.erase(area); }
    void clear() { by_area_.clear(); }

    const Painted* of(const std::string& area) const {
        const auto it = by_area_.find(area);
        return it == by_area_.end() ? nullptr : &it->second;
    }

    /// The window cells a painted node covers.
    void cover(const std::string& area, const std::string& path, const Place& pl,
               std::set<CellAt>& into) const {
        const Painted* p = of(area);
        if (p == nullptr) {
            return;
        }
        const auto n = p->find(path);
        if (n == p->end()) {
            return;
        }
        for (const term::Cell& c : n->second.cells) {
            into.insert({pl.col + c.col, pl.row + c.row});
        }
    }

private:
    std::map<std::string, Painted> by_area_;
};

/// The areas front first: a higher order stands in front.
inline std::vector<const WinArea*> front_to_back(const std::vector<WinArea>& areas) {
    std::vector<const WinArea*> out;
    for (const WinArea& a : areas) {
        out.push_back(&a);
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const WinArea* l, const WinArea* r) { return l->order > r->order; });
    return out;
}

/// The window's cells at `dirty`: each from the front-most area that painted it there, its top
/// node by paint order; an area shows what lies behind it wherever it painted nothing; else blank.
inline std::vector<term::Cell> compose(const std::vector<WinArea>& areas, const Cache& cache,
                                       const std::set<CellAt>& dirty) {
    struct Top {
        std::vector<std::int64_t> key;
        char glyph = ' ';
        std::int64_t role = role::kFill;
    };
    const auto front = front_to_back(areas);
    std::map<std::string, std::map<CellAt, Top>> tops;
    for (const WinArea* a : front) {
        const Painted* p = cache.of(a->area);
        if (p == nullptr) {
            continue;
        }
        const Place pl = place_of(*a);
        auto& t = tops[a->area];
        for (const auto& [path, n] : *p) {
            for (const term::Cell& c : n.cells) {
                if (dirty.count({pl.col + c.col, pl.row + c.row}) == 0) {
                    continue;
                }
                auto [it, fresh] = t.try_emplace({c.col, c.row}, Top{n.key, c.glyph, c.role});
                if (!fresh && it->second.key < n.key) {
                    it->second = Top{n.key, c.glyph, c.role};
                }
            }
        }
    }
    std::vector<term::Cell> out;
    for (const auto& [c, r] : dirty) {
        term::Cell cell{c, r, ' ', role::kFill};
        for (const WinArea* a : front) {
            const Place pl = place_of(*a);
            if (!pl.holds(c, r)) {
                continue;
            }
            const auto t = tops.find(a->area);
            if (t == tops.end()) {
                continue;
            }
            const auto hit = t->second.find({c - pl.col, r - pl.row});
            if (hit != t->second.end()) {
                cell.glyph = hit->second.glyph;
                cell.role = hit->second.role;
                break;
            }
        }
        out.push_back(cell);
    }
    std::sort(out.begin(), out.end(), [](const term::Cell& a, const term::Cell& b) {
        return a.row != b.row ? a.row < b.row : a.col < b.col;
    });
    return out;
}

/// Where a press landed: the front-most area at the point (none outside every area), the top node
/// painted there ("" where the area painted nothing), and the point in the area's pixels.
struct Landed {
    const WinArea* area = nullptr;
    std::string path;
    std::int64_t x = 0, y = 0;
};

inline Landed land(const std::vector<WinArea>& areas, const Cache& cache, std::int64_t col,
                   std::int64_t row) {
    Landed out;
    for (const WinArea* a : front_to_back(areas)) {
        const Place pl = place_of(*a);
        if (!pl.holds(col, row)) {
            continue;
        }
        if (out.area == nullptr) {
            out.area = a;
        }
        const Painted* p = cache.of(a->area);
        const PaintedNode* best = nullptr;
        std::string best_path;
        if (p != nullptr) {
            for (const auto& [path, n] : *p) {
                for (const term::Cell& c : n.cells) {
                    if (c.col == col - pl.col && c.row == row - pl.row &&
                        (best == nullptr || best->key < n.key)) {
                        best = &n;
                        best_path = path;
                    }
                }
            }
        }
        if (best != nullptr) {
            out.area = a;
            out.path = best_path;
            break;
        }
    }
    if (out.area != nullptr) {
        out.x = col * kCellPx + kCellPx / 2 - out.area->x;
        out.y = row * kCellPx + kCellPx / 2 - out.area->y;
    }
    return out;
}

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_COMPOSE_HPP
