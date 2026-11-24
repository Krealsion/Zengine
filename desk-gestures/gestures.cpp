// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#include "desk-gestures/gestures.hpp"

#include "input/vocabulary.hpp" // `space` -- which medium's numbers a pointer reported in

#include <algorithm>
#include <cstdlib>
#include <string>

namespace zengine::desk_gestures {

// ---- the geometry ---------------------------------------------------------------------------

PointedAt canvas_point_of(std::int64_t space, std::int64_t x, std::int64_t y) noexcept {
    if (space == input::space::kCells) {
        return PointedAt{true, CanvasPoint{x, y},
                         CanvasPoint{graphics::px_of_cells(x), graphics::px_of_cells(y)},
                         graphics::kCellGrainPx};
    }
    if (space == input::space::kPixels) {
        return PointedAt{true, CanvasPoint{graphics::cell_of_pixel(x), graphics::cell_of_pixel(y)},
                         CanvasPoint{x, y}, graphics::kPixelGrainPx};
    }
    return PointedAt{};
}

PixelRect pane_edge_cell(const PixelRect& r, std::int64_t edge) noexcept {
    const std::int64_t cell = graphics::kCellPx;
    const std::int64_t x0 = r.x;
    const std::int64_t x1 = r.w > cell ? r.x + r.w - cell : r.x;
    const std::int64_t y0 = r.y;
    const std::int64_t y1 = r.h > cell ? r.y + r.h - cell : r.y;
    const std::int64_t xm = r.w > cell ? r.x + (r.w - cell) / 2 : r.x;
    const std::int64_t ym = r.h > cell ? r.y + (r.h - cell) / 2 : r.y;
    switch (edge) {
    case pane_edge::kLeft: return PixelRect{x0, ym, cell, cell};
    case pane_edge::kRight: return PixelRect{x1, ym, cell, cell};
    case pane_edge::kTop: return PixelRect{xm, y0, cell, cell};
    case pane_edge::kBottom: return PixelRect{xm, y1, cell, cell};
    case pane_edge::kTopLeft: return PixelRect{x0, y0, cell, cell};
    case pane_edge::kTopRight: return PixelRect{x1, y0, cell, cell};
    case pane_edge::kBottomLeft: return PixelRect{x0, y1, cell, cell};
    case pane_edge::kBottomRight: return PixelRect{x1, y1, cell, cell};
    default: return PixelRect{};
    }
}

// WL-GEO-07 -- agents/workshop/geometry.md
std::int64_t pane_edge_at(const PixelRect& r, std::int64_t sx, std::int64_t sy,
                          std::int64_t grain) noexcept {
    if (!r.contains_at(sx, sy, grain)) {
        return kNoPaneEdge;
    }
    const std::int64_t band_w = r.w < kPaneEdgeBandPx ? r.w : kPaneEdgeBandPx;
    const std::int64_t band_h = r.h < kPaneEdgeBandPx ? r.h : kPaneEdgeBandPx;
    const bool left = graphics::px_span_contains(r.x, band_w, sx, grain);
    const bool right =
        graphics::px_span_contains(graphics::add_cells(r.x, r.w - band_w), band_w, sx, grain);
    const bool top = graphics::px_span_contains(r.y, band_h, sy, grain);
    const bool bottom =
        graphics::px_span_contains(graphics::add_cells(r.y, r.h - band_h), band_h, sy, grain);
    if (top && left) {
        return pane_edge::kTopLeft;
    }
    if (top && right) {
        return pane_edge::kTopRight;
    }
    if (bottom && left) {
        return pane_edge::kBottomLeft;
    }
    if (bottom && right) {
        return pane_edge::kBottomRight;
    }
    if (left) {
        return pane_edge::kLeft;
    }
    if (right) {
        return pane_edge::kRight;
    }
    if (top) {
        return pane_edge::kTop;
    }
    if (bottom) {
        return pane_edge::kBottom;
    }
    return kNoPaneEdge;
}

// ---- a pane taken by an edge or by its body, moved or sized ---------------------------------

PaneWindowProposal pane_window_proposal(std::int64_t edge, std::int64_t base_x,
                                        std::int64_t base_y, std::int64_t base_w,
                                        std::int64_t base_h, std::int64_t dx,
                                        std::int64_t dy) noexcept {
    PaneWindowProposal out{base_x, base_y, base_w, base_h, false, false};
    const bool wide = edge == pane_edge::kLeft || edge == pane_edge::kRight ||
                      edge == pane_edge::kTopLeft || edge == pane_edge::kTopRight ||
                      edge == pane_edge::kBottomLeft || edge == pane_edge::kBottomRight;
    const bool tall = edge == pane_edge::kTop || edge == pane_edge::kBottom ||
                      edge == pane_edge::kTopLeft || edge == pane_edge::kTopRight ||
                      edge == pane_edge::kBottomLeft || edge == pane_edge::kBottomRight;
    const bool leftwards = edge == pane_edge::kLeft || edge == pane_edge::kTopLeft ||
                           edge == pane_edge::kBottomLeft;
    const bool upwards = edge == pane_edge::kTop || edge == pane_edge::kTopLeft ||
                         edge == pane_edge::kTopRight;
    if (wide) {
        out.w = saturating_step(base_w, leftwards ? saturating_minus(0, dx) : dx);
        if (leftwards) {
            // The RIGHT edge is the anchor: base_x + base_w == x' + w', rearranged.
            out.x = saturating_minus(saturating_step(base_x, base_w), out.w);
            out.place_moved_x = true;
        }
    }
    if (tall) {
        out.h = saturating_step(base_h, upwards ? saturating_minus(0, dy) : dy);
        if (upwards) {
            // The BOTTOM edge is the anchor.
            out.y = saturating_minus(saturating_step(base_y, base_h), out.h);
            out.place_moved_y = true;
        }
    }
    return out;
}

std::optional<PaneHold> hold_at(const PixelRect& drawn, const PixelRect& resolved,
                                const AuthoredPixels& authored, const PointedAt& at,
                                bool top_moves) noexcept {
    if (drawn.empty()) {
        return std::nullopt;
    }
    const std::int64_t edge = pane_edge_at(drawn, at.px.x, at.px.y, at.grain);
    if (edge != kNoPaneEdge && !(top_moves && edge == pane_edge::kTop)) {
        // The affordance is on the drawn boundary, and the base is the window as the row authors
        // it in pixels, else as it resolved: an anchored top or left pull authors place and size
        // from this one captured window, as a key does.
        PaneHold hold;
        hold.sizing = true;
        hold.edge = edge;
        hold.from_x = at.px.x;
        hold.from_y = at.px.y;
        hold.base_x = authored.x.value_or(resolved.x);
        hold.base_y = authored.y.value_or(resolved.y);
        hold.base_w = authored.w.value_or(resolved.w);
        hold.base_h = authored.h.value_or(resolved.h);
        return hold;
    }
    if (drawn.contains_at(at.px.x, at.px.y, at.grain)) {
        PaneHold hold;
        hold.grab_dx = saturating_minus(at.px.x, drawn.x);
        hold.grab_dy = saturating_minus(at.px.y, drawn.y);
        return hold;
    }
    return std::nullopt;
}

PaneWindowProposal sized_to(const PaneHold& hold, std::int64_t px_x, std::int64_t px_y) noexcept {
    return pane_window_proposal(hold.edge, hold.base_x, hold.base_y, hold.base_w, hold.base_h,
                                saturating_minus(px_x, hold.from_x),
                                saturating_minus(px_y, hold.from_y));
}

CanvasPoint moved_to(const PaneHold& hold, std::int64_t px_x, std::int64_t px_y) noexcept {
    return CanvasPoint{saturating_minus(px_x, hold.grab_dx), saturating_minus(px_y, hold.grab_dy)};
}

namespace {

/// Which edges of one axis the hand moves: both (a move), the low or the high one (a pull), or
/// neither.
enum class Moving { kNone, kLow, kHigh, kBoth };

Moving moving_x(std::int64_t edge) noexcept {
    switch (edge) {
    case kNoPaneEdge: return Moving::kBoth;
    case pane_edge::kLeft:
    case pane_edge::kTopLeft:
    case pane_edge::kBottomLeft: return Moving::kLow;
    case pane_edge::kRight:
    case pane_edge::kTopRight:
    case pane_edge::kBottomRight: return Moving::kHigh;
    default: return Moving::kNone;
    }
}

Moving moving_y(std::int64_t edge) noexcept {
    switch (edge) {
    case kNoPaneEdge: return Moving::kBoth;
    case pane_edge::kTop:
    case pane_edge::kTopLeft:
    case pane_edge::kTopRight: return Moving::kLow;
    case pane_edge::kBottom:
    case pane_edge::kBottomLeft:
    case pane_edge::kBottomRight: return Moving::kHigh;
    default: return Moving::kNone;
    }
}

struct Axis {
    std::int64_t at = 0;
    std::int64_t size = 0;
    std::optional<std::int64_t> met;
};

/// One axis: the edges `moving` names come to the nearest line within `reach_px`, and a travel
/// that would author a place or an extent outside `limits` is not taken. What is not written is
/// not judged: a high pull leaves the place as it stood.
Axis snap_axis(std::int64_t at, std::int64_t size, Moving moving,
               const std::vector<std::int64_t>& lines, std::int64_t reach_px,
               const WindowLimits& limits) noexcept {
    if (moving == Moving::kNone) {
        return Axis{at, size, std::nullopt};
    }
    const std::int64_t end = saturating_step(at, size);
    const auto travelled = [&](std::int64_t travel) -> std::optional<Axis> {
        Axis s{at, size, std::nullopt};
        if (moving != Moving::kHigh) {
            s.at = saturating_step(at, travel);
            if (s.at < 0 || s.at > limits.max_px) {
                return std::nullopt;
            }
        }
        if (moving == Moving::kLow) {
            s.size = saturating_minus(end, s.at);
        } else if (moving == Moving::kHigh) {
            s.size = saturating_step(size, travel);
        }
        if (moving != Moving::kBoth && (s.size < limits.min_extent || s.size > limits.max_px)) {
            return std::nullopt;
        }
        return s;
    };
    std::optional<Axis> best;
    std::int64_t nearest = reach_px + 1;
    const auto meet = [&](std::int64_t edge) {
        for (const std::int64_t line : lines) {
            const std::int64_t gap =
                line > edge ? saturating_minus(line, edge) : saturating_minus(edge, line);
            if (gap >= nearest) {
                continue;
            }
            if (std::optional<Axis> s = travelled(saturating_minus(line, edge))) {
                nearest = gap;
                s->met = line;
                best = s;
            }
        }
    };
    if (moving != Moving::kHigh) {
        meet(at);
    }
    if (moving != Moving::kLow) {
        meet(end);
    }
    return best.has_value() ? *best : Axis{at, size, std::nullopt};
}

} // namespace

SnappedWindow snap_pane_window(const PaneWindowProposal& want, std::int64_t edge,
                               const PaneSnapLines& lines, std::int64_t reach_px,
                               const WindowLimits& limits) noexcept {
    SnappedWindow out{want, std::nullopt, std::nullopt};
    const Axis x = snap_axis(want.x, want.w, moving_x(edge), lines.xs, reach_px, limits);
    const Axis y = snap_axis(want.y, want.h, moving_y(edge), lines.ys, reach_px, limits);
    out.want.x = x.at;
    out.want.w = x.size;
    out.met_x = x.met;
    out.want.y = y.at;
    out.want.h = y.size;
    out.met_y = y.met;
    return out;
}

// ---- a layout tab: pressed, pressed twice, dragged; and its name committed ------------------

// WL-PTR-01 -- agents/workshop/pointer.md
bool doubles_a_tab_click(const TabClickMemory& prior, std::size_t at,
                         std::int64_t clicks) noexcept {
    return clicks >= 2 && prior.armed && prior.at == at;
}

TabPressAsk tab_press(const TabClickMemory& prior, const LayoutTabPress& tab,
                      std::int64_t clicks) noexcept {
    if (!tab.hit) {
        return TabPressAsk{TabPressAsk::Kind::kNone, 0, prior};
    }
    if (tab.create) {
        return TabPressAsk{TabPressAsk::Kind::kNew, 0, TabClickMemory{}};
    }
    if (doubles_a_tab_click(prior, tab.at, clicks)) {
        return TabPressAsk{TabPressAsk::Kind::kRename, tab.at, TabClickMemory{}};
    }
    return TabPressAsk{TabPressAsk::Kind::kSwitch, tab.at, TabClickMemory{true, tab.at}};
}

std::optional<std::size_t> tab_drag_to(const LayoutTabPress& over, std::size_t live_at,
                                       std::size_t count) noexcept {
    if (!over.hit || over.create || over.at == live_at || over.at >= count) {
        return std::nullopt;
    }
    return over.at;
}

NamingCommit naming_commit(bool name_legal, std::size_t at, std::size_t count) noexcept {
    if (!name_legal) {
        return NamingCommit::kRefused;
    }
    return at < count ? NamingCommit::kRename : NamingCommit::kGone;
}

// ---- the keyboard's arrangement -------------------------------------------------------------

CanvasPoint nudged(const PixelRect& base, std::int64_t dx_cells, std::int64_t dy_cells) noexcept {
    return CanvasPoint{saturating_step(base.x, dx_cells * graphics::kCellPx),
                       saturating_step(base.y, dy_cells * graphics::kCellPx)};
}

PaneWindowProposal grown(const PixelRect& base, std::int64_t dx_cells,
                         std::int64_t dy_cells) noexcept {
    return pane_window_proposal(pane_edge::kBottomRight, base.x, base.y, base.w, base.h,
                                dx_cells * graphics::kCellPx, dy_cells * graphics::kCellPx);
}

std::size_t arrange_step_to(std::size_t count, std::optional<std::size_t> at,
                            std::int64_t by) noexcept {
    if (!at.has_value()) {
        return by > 0 ? 0 : count - 1;
    }
    return by > 0 ? (*at + 1) % count : (*at + count - 1) % count;
}

// ---- the keyboard's selection moved ---------------------------------------------------------

std::vector<Selectable> reading_order(std::vector<Selectable> panes) {
    std::stable_sort(panes.begin(), panes.end(), [](const Selectable& a, const Selectable& b) {
        if (a.rect.y != b.rect.y) {
            return a.rect.y < b.rect.y;
        }
        if (a.rect.x != b.rect.x) {
            return a.rect.x < b.rect.x;
        }
        return a.depth > b.depth;
    });
    return panes;
}

std::optional<std::int64_t> cycle_from(const std::vector<std::int64_t>& order, std::int64_t from,
                                       std::int64_t step) noexcept {
    if (order.empty()) {
        return std::nullopt;
    }
    const auto n = static_cast<std::int64_t>(order.size());
    const auto at = std::find(order.begin(), order.end(), from);
    if (at == order.end()) {
        return step >= 0 ? order.front() : order.back();
    }
    const std::int64_t i = static_cast<std::int64_t>(at - order.begin());
    return order[static_cast<std::size_t>(((i + step) % n + n) % n)];
}

std::optional<std::int64_t> neighbour(const std::vector<Selectable>& panes, std::int64_t from,
                                      Toward dir) {
    const bool horizontal = dir == Toward::kLeft || dir == Toward::kRight;
    const auto here = std::find_if(panes.begin(), panes.end(),
                                   [from](const Selectable& p) { return p.kind == from; });
    if (here == panes.end()) {
        if (panes.empty()) {
            return std::nullopt;
        }
        return panes.front().kind;
    }
    const PixelRect f = here->rect;
    const std::int64_t fcx = f.x + f.w / 2;
    const std::int64_t fcy = f.y + f.h / 2;
    // Past the edge, as the W3C draft filters; where panes overlap or share a border, past the
    // centre, so a pane half over another is still reachable.
    const auto beyond = [&](const PixelRect& r, bool edge) {
        const std::int64_t cx = r.x + r.w / 2;
        const std::int64_t cy = r.y + r.h / 2;
        switch (dir) {
        case Toward::kLeft: return edge ? cx <= f.x : cx < fcx;
        case Toward::kRight: return edge ? cx >= f.x + f.w : cx > fcx;
        case Toward::kUp: return edge ? cy <= f.y : cy < fcy;
        default: return edge ? cy >= f.y + f.h : cy > fcy;
        }
    };
    const auto score = [&](const PixelRect& r) {
        std::int64_t gap = 0;
        switch (dir) {
        case Toward::kLeft: gap = f.x - (r.x + r.w); break;
        case Toward::kRight: gap = r.x - (f.x + f.w); break;
        case Toward::kUp: gap = f.y - (r.y + r.h); break;
        default: gap = r.y - (f.y + f.h); break;
        }
        const std::int64_t across =
            horizontal ? std::abs(r.y + r.h / 2 - fcy) : std::abs(r.x + r.w / 2 - fcx);
        return (gap > 0 ? gap : 0) + 2 * across;
    };
    for (const bool edge : {true, false}) {
        const Selectable* best = nullptr;
        std::int64_t best_score = 0;
        for (const Selectable& p : panes) {
            if (p.kind == from || !beyond(p.rect, edge)) {
                continue;
            }
            const std::int64_t points = score(p.rect);
            if (best == nullptr || points < best_score ||
                (points == best_score && p.depth > best->depth)) {
                best = &p;
                best_score = points;
            }
        }
        if (best != nullptr) {
            return best->kind;
        }
    }
    return std::nullopt;
}

// ---- a bounded list ---------------------------------------------------------------------------

ListWindow list_window(std::size_t total, std::size_t selected_at, std::size_t rows) {
    ListWindow w;
    if (total == 0 || rows == 0) {
        w.after = total; // no room at all: everything there is, is missing
        return w;
    }
    if (total <= rows) {
        w.count = total; // rule 1
        return w;
    }
    if (rows < 3) {
        // Too few lines to seat an entry between two markers, so no window can obey rules 2 and 3
        // together: it spends what it has on the omission, since a list never drops entries
        // quietly.
        w.after = total;
        return w;
    }
    if (selected_at >= total) {
        selected_at = 0; // nothing selected, or a selection that outlived its entry
    }
    // One marker's worth of room. Both single-marker windows are this wide, and
    // both leave a non-empty count because `total > rows`.
    const std::size_t one_marker = rows - 1;
    if (selected_at < one_marker) {
        w.count = one_marker;
        w.after = total - w.count;
        return w;
    }
    const std::size_t tail = total - one_marker;
    if (selected_at >= tail) {
        w.first = tail;
        w.count = one_marker;
        w.before = tail;
        return w;
    }
    // The selection is far enough from both ends that both walls are real, so
    // both are said. `first` is the earliest run that reaches the selection,
    // which is at least 2 here (selected_at >= rows - 1 and count == rows - 2),
    // so neither subtraction can leave the number line.
    w.count = rows - 2;
    w.first = selected_at + 1 - w.count;
    w.before = w.first;
    w.after = total - w.first - w.count;
    return w;
}

std::string omitted_text(std::size_t how_many, const char* which) {
    return "... " + std::to_string(how_many) + " " + which;
}

} // namespace zengine::desk_gestures
