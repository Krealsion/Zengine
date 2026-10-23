// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_COMPOSE_HPP
#define ZENGINE_GRAPHICS_WINDOW_COMPOSE_HPP

// A window's picture: each node painted once, when it changes, by its area's skin, and kept
// painted, so a change paints only what changed, and the composition and a press read what was
// painted. What it keeps painted is the window's memory, never its state: a reload paints it
// again from the nodes its state keeps. Reference: graphics/docs/zengine.window.terminal.md.

#include "graphics/ops.hpp"
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

/// The window's cells to show again: a flag a cell over a grid of the window's size, and the box
/// the flagged cells span. A cell outside the grid is never shown, so it is never flagged.
class DirtyCells {
public:
    /// A grid of `cols` by `rows` cells, none flagged.
    void size(std::int64_t cols, std::int64_t rows) {
        cols_ = (std::max<std::int64_t>)(cols, 0);
        rows_ = (std::max<std::int64_t>)(rows, 0);
        flags_.assign(static_cast<std::size_t>(cols_ * rows_), 0);
        count_ = 0;
    }
    void mark(std::int64_t c, std::int64_t r) {
        if (c < 0 || r < 0 || c >= cols_ || r >= rows_) {
            return;
        }
        char& f = flags_[index(c, r)];
        if (f != 0) {
            return;
        }
        f = 1;
        if (count_ == 0) {
            c0_ = c1_ = c;
            r0_ = r1_ = r;
        } else {
            c0_ = (std::min)(c0_, c);
            c1_ = (std::max)(c1_, c);
            r0_ = (std::min)(r0_, r);
            r1_ = (std::max)(r1_, r);
        }
        ++count_;
    }
    void mark_all() {
        std::fill(flags_.begin(), flags_.end(), char{1});
        count_ = static_cast<std::size_t>(cols_ * rows_);
        c0_ = r0_ = 0;
        c1_ = cols_ - 1;
        r1_ = rows_ - 1;
    }
    /// Every flag let go, over the box they span alone.
    void clear() {
        if (count_ == 0) {
            return;
        }
        for (std::int64_t r = r0_; r <= r1_; ++r) {
            std::fill(flags_.begin() + static_cast<std::ptrdiff_t>(index(c0_, r)),
                      flags_.begin() + static_cast<std::ptrdiff_t>(index(c1_, r)) + 1, char{0});
        }
        count_ = 0;
    }
    bool empty() const { return count_ == 0; }
    std::size_t count() const { return count_; }
    bool marked(std::int64_t c, std::int64_t r) const {
        return c >= c0_ && c <= c1_ && r >= r0_ && r <= r1_ && flags_[index(c, r)] != 0;
    }
    /// The box the flagged cells span, its corners inclusive; meaningless while `empty()`.
    std::int64_t c0() const { return c0_; }
    std::int64_t r0() const { return r0_; }
    std::int64_t c1() const { return c1_; }
    std::int64_t r1() const { return r1_; }

private:
    std::size_t index(std::int64_t c, std::int64_t r) const {
        return static_cast<std::size_t>(r * cols_ + c);
    }
    std::int64_t cols_ = 0, rows_ = 0;
    std::vector<char> flags_;
    std::size_t count_ = 0;
    std::int64_t c0_ = 0, r0_ = 0, c1_ = 0, r1_ = 0;
};

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
               DirtyCells& into) const {
        const Painted* p = of(area);
        if (p == nullptr) {
            return;
        }
        const auto n = p->find(path);
        if (n == p->end()) {
            return;
        }
        for (const term::Cell& c : n->second.cells) {
            into.mark(pl.col + c.col, pl.row + c.row);
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
/// The cells are settled on a grid over the box `dirty` spans, front area first.
inline std::vector<term::Cell> compose(const std::vector<WinArea>& areas, const Cache& cache,
                                       const DirtyCells& dirty) {
    if (dirty.empty()) {
        return {};
    }
    const std::int64_t c0 = dirty.c0(), c1 = dirty.c1();
    const std::int64_t r0 = dirty.r0(), r1 = dirty.r1();
    const std::int64_t w = c1 - c0 + 1;
    const auto at = [&](std::int64_t c, std::int64_t r) {
        return static_cast<std::size_t>((r - r0) * w + (c - c0));
    };
    const auto n = static_cast<std::size_t>(w * (r1 - r0 + 1));
    const auto wanted = [&](std::int64_t c, std::int64_t r) { return dirty.marked(c, r); };
    // Each cell's top so far: the area that holds it (its place front first) and the node.
    struct Top {
        std::size_t area = 0;
        const std::vector<std::int64_t>* key = nullptr;
        char glyph = ' ';
        std::int64_t role = role::kFill;
    };
    std::vector<Top> tops(n);
    const auto front = front_to_back(areas);
    for (std::size_t i = 0; i < front.size(); ++i) {
        const Painted* p = cache.of(front[i]->area);
        if (p == nullptr) {
            continue;
        }
        const Place pl = place_of(*front[i]);
        for (const auto& [path, node] : *p) {
            for (const term::Cell& c : node.cells) {
                const std::int64_t col = pl.col + c.col, row = pl.row + c.row;
                if (col < c0 || col > c1 || row < r0 || row > r1 || !pl.holds(col, row) ||
                    !wanted(col, row)) {
                    continue;
                }
                Top& t = tops[at(col, row)];
                if (t.key == nullptr || (t.area == i && *t.key < node.key)) {
                    t = Top{i, &node.key, c.glyph, c.role};
                }
            }
        }
    }
    std::vector<term::Cell> out;
    out.reserve(dirty.count());
    for (std::int64_t r = r0; r <= r1; ++r) {
        for (std::int64_t c = c0; c <= c1; ++c) {
            if (wanted(c, r)) {
                const Top& t = tops[at(c, r)];
                out.push_back(term::Cell{c, r, t.glyph, t.role});
            }
        }
    }
    return out;
}

/// Where a pointer moment landed: the front-most area at the point that takes it (none where no
/// area does), the top node that area painted there ("" where it painted nothing), the point in the
/// area's pixels, and the area showing what lies under the hand -- that one, or where it painted
/// nothing the first behind it that did -- with its top node there. An area placed to hear the
/// pointer takes every point inside it; any other takes a point only where what it drew there takes
/// a press (`takes_press`), and lets the pointer through to the areas behind it everywhere else.
struct Landed {
    const WinArea* area = nullptr;
    std::string path;
    std::int64_t x = 0, y = 0;
    const WinArea* under = nullptr;
    std::string under_path;
};

/// A GROUP'S CLIP IN ITS AREA'S PIXELS, its far edges exclusive: its place carried through the
/// groups above it, and cut by their clips.
struct ClipBox {
    std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool holds(std::int64_t x, std::int64_t y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; }
};

/// EVERY GROUP OF AN AREA THAT CLIPS WHAT IT HOLDS, by its path, and their paint order (`key_of`):
/// a canvas part's group is its place, whether anything is drawn there or not.
inline std::vector<std::pair<std::string, ClipBox>> group_clips(const WinArea& a) {
    const auto at = index_of(a);
    std::vector<std::pair<std::string, ClipBox>> out;
    std::vector<std::vector<std::int64_t>> keys;
    for (const KeptNode& k : a.nodes) {
        if (k.node.op != op::kGroup) {
            continue;
        }
        const auto own = from_body<GroupArgs>(k.node.args);
        if (!own || own->clip_w <= 0 || own->clip_h <= 0) {
            continue;
        }
        std::int64_t dx = 0, dy = 0;
        bool clipped = false;
        ClipBox box;
        std::vector<std::string> chain;
        for (std::string p = parent_of(k.node.path); !p.empty(); p = parent_of(p)) {
            chain.push_back(p);
        }
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const auto g = at.find(*it);
            if (g == at.end() || g->second->node.op != op::kGroup) continue;
            const auto args = from_body<GroupArgs>(g->second->node.args);
            if (!args) continue;
            dx += args->x;
            dy += args->y;
            if (args->clip_w > 0 && args->clip_h > 0) {
                const ClipBox c{dx, dy, dx + args->clip_w, dy + args->clip_h};
                box = clipped ? ClipBox{(std::max)(box.x0, c.x0), (std::max)(box.y0, c.y0),
                                        (std::min)(box.x1, c.x1), (std::min)(box.y1, c.y1)}
                              : c;
                clipped = true;
            }
        }
        dx += own->x;
        dy += own->y;
        const ClipBox mine{dx, dy, dx + own->clip_w, dy + own->clip_h};
        box = clipped ? ClipBox{(std::max)(box.x0, mine.x0), (std::max)(box.y0, mine.y0),
                                (std::min)(box.x1, mine.x1), (std::min)(box.y1, mine.y1)}
                      : mine;
        if (box.x1 <= box.x0 || box.y1 <= box.y0) {
            continue;
        }
        const auto key = key_of(at, k.node.path);
        std::size_t i = keys.size();
        while (i > 0 && key < keys[i - 1]) --i;
        keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(i), key);
        out.insert(out.begin() + static_cast<std::ptrdiff_t>(i), {k.node.path, box});
    }
    return out;
}

/// What one area shows of a window of `cols` by `rows` cells (`AreaShown`), in the area's pixels:
/// the runs its text nodes show on each row, and each record naming an action as the part it names
/// -- the box of the cells its subtree shows, and the middle of the cells where a press reaches it.
/// A cell shows the area's top node there unless an area in front painted it -- one `reader` owns
/// or arranges aside, as what the reader draws or places is its own to know and judge. "" when
/// said, else why not.
inline std::string read_area(const std::vector<WinArea>& areas, const Cache& cache,
                             const WinArea& a, std::int64_t cols, std::int64_t rows,
                             const std::string& reader, std::vector<AreaWord>& words,
                             std::vector<AreaPart>& parts) {
    const Place pl = place_of(a);
    const std::int64_t c0 = (std::max<std::int64_t>)(pl.col, 0);
    const std::int64_t r0 = (std::max<std::int64_t>)(pl.row, 0);
    const std::int64_t c1 = (std::min)(pl.col + pl.cols, cols);
    const std::int64_t r1 = (std::min)(pl.row + pl.rows, rows);
    if (c1 <= c0 || r1 <= r0) {
        return {};
    }
    const std::int64_t w = c1 - c0;
    const auto at = [&](std::int64_t c, std::int64_t r) {
        return static_cast<std::size_t>((r - r0) * w + (c - c0));
    };
    struct Top {
        const std::string* path = nullptr;
        const std::vector<std::int64_t>* key = nullptr;
        char glyph = ' ';
        bool hidden = false;
    };
    std::vector<Top> grid(static_cast<std::size_t>(w * (r1 - r0)));
    const auto inside = [&](std::int64_t c, std::int64_t r) {
        return c >= c0 && c < c1 && r >= r0 && r < r1;
    };
    // HIDDEN WHERE AN AREA IN FRONT PAINTED, as the composition shows the front-most.
    for (const WinArea* f : front_to_back(areas)) {
        if (f->area == a.area) {
            break;
        }
        const Painted* p =
            f->owner == reader || f->arranger == reader ? nullptr : cache.of(f->area);
        if (p == nullptr) {
            continue;
        }
        const Place fp = place_of(*f);
        for (const auto& [path, node] : *p) {
            for (const term::Cell& c : node.cells) {
                const std::int64_t col = fp.col + c.col, row = fp.row + c.row;
                if (inside(col, row) && fp.holds(col, row)) {
                    grid[at(col, row)].hidden = true;
                }
            }
        }
    }
    if (const Painted* own = cache.of(a.area)) {
        for (const auto& [path, node] : *own) {
            for (const term::Cell& c : node.cells) {
                const std::int64_t col = pl.col + c.col, row = pl.row + c.row;
                if (!inside(col, row)) {
                    continue;
                }
                Top& t = grid[at(col, row)];
                if (t.key == nullptr || *t.key < node.key) {
                    t.path = &path;
                    t.key = &node.key;
                    t.glyph = c.glyph;
                }
            }
        }
    }
    std::set<std::string> texts;
    for (const KeptNode& k : a.nodes) {
        if (k.node.op == op::kText) {
            texts.insert(k.node.path);
        }
    }
    // THE WORDS: each row's runs of cells a text node shows, the blanks after the last left off.
    for (std::int64_t r = r0; r < r1; ++r) {
        std::string run;
        std::int64_t from = -1;
        const auto flush = [&]() {
            run.erase(run.find_last_not_of(' ') + 1);
            if (!run.empty()) {
                words.push_back(AreaWord{from * kCellPx - a.x, r * kCellPx - a.y, run});
            }
            run.clear();
            from = -1;
        };
        for (std::int64_t c = c0; c < c1; ++c) {
            const Top& t = grid[at(c, r)];
            if (t.path != nullptr && !t.hidden && texts.count(*t.path) != 0) {
                if (from < 0) {
                    from = c;
                }
                run.push_back(t.glyph);
            } else {
                flush();
            }
        }
        flush();
        if (words.size() > kMaxAreaWords) {
            return "area '" + a.area + "' shows more than " + std::to_string(kMaxAreaWords) +
                   " runs of text";
        }
    }
    // THE PARTS: every action a record names, one part however many records name it (a row's part
    // drawn in runs around another's), by the cells their subtrees show, and the cells where one
    // of its own records is the one a press reaches.
    std::map<std::string, std::size_t> named;     // a record's path: its action's part
    std::map<std::string, std::size_t> part_of;   // an action: its part
    std::vector<const ElementRecord*> first_of;   // a part: the first record naming it
    for (std::size_t i = 0; i < a.records.size(); ++i) {
        if (!a.records[i].action.empty() && !a.records[i].path.empty()) {
            const auto [it, fresh] = part_of.try_emplace(a.records[i].action, first_of.size());
            if (fresh) {
                first_of.push_back(&a.records[i]);
            }
            named[a.records[i].path] = it->second;
        }
    }
    if (named.empty()) {
        return {};
    }
    const std::map<std::string, const ElementRecord*> reached = records_by_node(a);
    // THE CLIPS OF ITS PARTS' GROUPS, and whether one paints on a cell, as the painter cuts one.
    const std::vector<std::pair<std::string, ClipBox>> clips = group_clips(a);
    const auto paints_on = [&a](const ClipBox& b, std::int64_t c, std::int64_t r) {
        const std::int64_t ax = c * kCellPx - a.x, ay = r * kCellPx - a.y;
        return term::floor_cell(b.x0) * kCellPx <= ax && ax < term::floor_cell(b.x1) * kCellPx &&
               term::floor_cell(b.y0) * kCellPx <= ay && ay < term::floor_cell(b.y1) * kCellPx;
    };
    const auto in_clip = [&](std::int64_t c, std::int64_t r) {
        for (const auto& [path, box] : clips) {
            if (paints_on(box, c, r)) return true;
        }
        return false;
    };
    struct Seen {
        bool any = false;
        std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        std::vector<CellAt> own;
        std::vector<CellAt> lettered; ///< the own cells among a row's characters, not after them
    };
    std::vector<Seen> seen(first_of.size());
    for (std::int64_t r = r0; r < r1; ++r) {
        for (std::int64_t c = c0; c < c1; ++c) {
            const Top& t = grid[at(c, r)];
            if (t.path == nullptr || t.hidden) {
                continue;
            }
            for (std::string p = *t.path; !p.empty(); p = parent_of(p)) {
                const auto it = named.find(p);
                if (it == named.end()) {
                    continue;
                }
                Seen& s = seen[it->second];
                s.x0 = s.any ? (std::min)(s.x0, c) : c;
                s.y0 = s.any ? (std::min)(s.y0, r) : r;
                s.x1 = s.any ? (std::max)(s.x1, c) : c;
                s.y1 = s.any ? (std::max)(s.y1, r) : r;
                s.any = true;
            }
            const auto own = reached.find(*t.path);
            if (own != reached.end() && named.count(own->second->path) != 0 &&
                !in_clip(c, r)) {
                Seen& s = seen[named.at(own->second->path)];
                s.own.push_back({c, r});
            }
        }
    }
    // ...AND A PART WHOSE RECORD IS A GROUP THAT CLIPS IS PLACED BY ITS CLIP, as far as the area
    // shows it: every cell the clip paints on, drawn there or not, and as its own each such cell
    // no later such part's clip holds, a press there being its pane's to read as the later's.
    std::vector<std::int64_t> clip_part(clips.size(), -1);
    for (std::size_t g = 0; g < clips.size(); ++g) {
        const auto it = named.find(clips[g].first);
        clip_part[g] = it == named.end() ? -1 : static_cast<std::int64_t>(it->second);
    }
    for (std::int64_t r = r0; r < r1 && !clips.empty(); ++r) {
        for (std::int64_t c = c0; c < c1; ++c) {
            if (grid[at(c, r)].hidden) {
                continue;
            }
            std::int64_t last = -1; // the last clip on the cell, named or not: a press there is its
            for (std::size_t g = 0; g < clips.size(); ++g) {
                if (!paints_on(clips[g].second, c, r)) continue;
                last = static_cast<std::int64_t>(g);
                if (clip_part[g] < 0) continue;
                Seen& s = seen[static_cast<std::size_t>(clip_part[g])];
                s.x0 = s.any ? (std::min)(s.x0, c) : c;
                s.y0 = s.any ? (std::min)(s.y0, r) : r;
                s.x1 = s.any ? (std::max)(s.x1, c) : c;
                s.y1 = s.any ? (std::max)(s.y1, r) : r;
                s.any = true;
            }
            if (last >= 0 && clip_part[static_cast<std::size_t>(last)] >= 0) {
                const Top& t = grid[at(c, r)];
                const auto top = t.path == nullptr ? reached.end() : reached.find(*t.path);
                // ...unless a part outside the clips is what a press there reaches
                if (top == reached.end() || named.count(top->second->path) == 0 ||
                    named.at(top->second->path) ==
                        static_cast<std::size_t>(clip_part[static_cast<std::size_t>(last)])) {
                    Seen& s = seen[static_cast<std::size_t>(clip_part[static_cast<std::size_t>(last)])];
                    if (std::find(s.own.begin(), s.own.end(), CellAt{c, r}) == s.own.end()) {
                        s.own.push_back({c, r});
                    }
                }
            }
        }
    }
    // EACH ROW'S LAST CHARACTER: a part's own cells up to it are among its characters, and the
    // blanks after it are padding.
    std::vector<std::int64_t> last_char(static_cast<std::size_t>(r1 - r0), c0 - 1);
    for (std::int64_t r = r0; r < r1; ++r) {
        for (std::int64_t c = c0; c < c1; ++c) {
            const Top& t = grid[at(c, r)];
            if (t.path != nullptr && !t.hidden && t.glyph != ' ') {
                last_char[static_cast<std::size_t>(r - r0)] = c;
            }
        }
    }
    for (std::size_t i = 0; i < first_of.size(); ++i) {
        Seen& s = seen[i];
        if (!s.any) {
            continue;
        }
        std::sort(s.own.begin(), s.own.end(), [](const CellAt& l, const CellAt& r) {
            return l.second != r.second ? l.second < r.second : l.first < r.first;
        });
        for (const CellAt& cell : s.own) {
            if (cell.first <= last_char[static_cast<std::size_t>(cell.second - r0)]) {
                s.lettered.push_back(cell);
            }
        }
        const ElementRecord& rec = *first_of[i];
        AreaPart part{rec.action,
                      record_available(rec),
                      s.x0 * kCellPx - a.x,
                      s.y0 * kCellPx - a.y,
                      (s.x1 - s.x0 + 1) * kCellPx,
                      (s.y1 - s.y0 + 1) * kCellPx,
                      false,
                      0,
                      0};
        // ITS POINT: the middle of the cells of its own that show a character, else of its own.
        const std::vector<CellAt>& cells = s.lettered.empty() ? s.own : s.lettered;
        if (!cells.empty()) {
            const CellAt& mid = cells[(cells.size() - 1) / 2];
            part.pressed = true;
            part.px = mid.first * kCellPx + kCellPx / 2 - a.x;
            part.py = mid.second * kCellPx + kCellPx / 2 - a.y;
        }
        parts.push_back(std::move(part));
    }
    return {};
}

inline Landed land(const std::vector<WinArea>& areas, const Cache& cache, std::int64_t col,
                   std::int64_t row, bool wheel = false, std::int64_t button = 0) {
    // AN AREA THAT LETS THE THIRD BUTTON THROUGH still takes it where no area behind it does.
    if (button == 3) {
        Landed through = land(areas, cache, col, row, wheel, -3);
        if (through.area != nullptr) {
            return through;
        }
        button = 0;
    }
    Landed out;
    for (const WinArea* a : front_to_back(areas)) {
        const Place pl = place_of(*a);
        if (!pl.holds(col, row)) {
            continue;
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
        if (out.area != nullptr) {
            if (best != nullptr) {
                out.under = a;
                out.under_path = best_path;
                break;
            }
            continue;
        }
        if ((wheel && a->pointer && !a->wheel) || (button == -3 && a->pointer && !a->right)) {
            continue;
        }
        if (a->pointer || (best != nullptr && takes_press(*a, best_path))) {
            out.area = a;
            out.path = best != nullptr ? best_path : std::string();
            if (best != nullptr) {
                out.under = a;
                out.under_path = best_path;
                break;
            }
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
