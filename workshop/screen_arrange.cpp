// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's pane management: what a weaver is arranging, and how, and where the keyboard's
// selection moves.
// Workshop law: agents/workshop/focus.md (+9 registers; agents/workshop.md routes)

#include "screen.hpp"

#include <algorithm>
#include <cstdlib>

namespace zengine::workshop {

// ---- PANE MANAGEMENT: what a weaver is ARRANGING, and how ------------------------------

// WL-ARR-09 -- agents/workshop/arrangement.md
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

// WL-ARR-01 -- agents/workshop/arrangement.md; WL-GEO-07 -- agents/workshop/geometry.md
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

// WL-PTR-01 -- agents/workshop/pointer.md; WL-TAB-10 -- agents/workshop/tab-run.md
bool doubles_a_tab_click(const TabClickMemory& prior, std::size_t at,
                         std::int64_t now_ms) noexcept {
    if (!prior.armed || prior.at != at) {
        return false;
    }
    const std::int64_t since = now_ms - prior.at_ms;
    return since >= 0 && since <= kDoubleClickMs;
}

// WL-KEY-03 -- agents/workshop/keyboard.md; WL-FOCUS-06 -- agents/workshop/focus.md
// WL-CTX-06 -- agents/workshop/contextual.md
KeyContext keyboard_context_beneath_menu(const Session& s) {
    if (s.setup.naming.open) {
        return KeyContext::kNaming;
    }
    if (is_runtime_kind(keyboard_pane(s.panes))) {
        return KeyContext::kPane;
    }
    return KeyContext::kCommand;
}

// WL-KEY-03 -- agents/workshop/keyboard.md; WL-FOCUS-06, WL-FOCUS-09 -- agents/workshop/focus.md
// WL-ARR-14 -- agents/workshop/arrangement.md; WL-CTX-08 -- agents/workshop/contextual.md
KeyContext keyboard_context(const Session& s) {
    if (s.arrange.open && !s.arrange.opened) {
        if (s.arrange.resetting) {
            return KeyContext::kArrangeReset;
        }
        return s.arrange.desk ? KeyContext::kArrangeDesk : KeyContext::kArrangePane;
    }
    // The contextual-action surface is a mode at the top of the modes' band, below the
    // arrangement scopes: it answers before a focused pane, or its navigation keys would leak
    // beneath it. A pane's menu presented by its presenter is the same kind of surface.
    if (s.context.open || s.presented.open) {
        return KeyContext::kContext;
    }
    return keyboard_context_beneath_menu(s);
}

// WL-FOCUS-04, WL-FOCUS-10 -- agents/workshop/focus.md
std::int64_t typing_pane(const Session& s) {
    // THE KEY HANDLER'S OWN ORDER, READ AS A VALUE: the resolved context decides
    // (`on(KeyPressed)`).
    if (keyboard_context(s) != KeyContext::kPane) {
        return kNoPaneKind;
    }
    return keyboard_pane(s.panes);
}

/// Where the keys went to a list or to nothing, as against a place a weaver types into, which
/// keeps its own keys: the contexts in which the application's default rows are answered.
// WL-ARR-13, WL-ARR-14 -- agents/workshop/arrangement.md
// WL-DESK-02 -- agents/workshop/desktop.md
bool default_row_context(KeyContext c) {
    return c == KeyContext::kCommand;
}

// ---- MOVING THE KEYBOARD'S SELECTION: reading order and the neighbour in a direction ----------

namespace {

/// One pane the selection may stand on: where it is now, and how far front it is authored.
struct Selectable {
    std::int64_t kind = kNoPaneKind;
    PixelRect rect{};
    std::size_t depth = 0; ///< its place in `presentation_order`, back to front
};

std::vector<Selectable> selectable_rects(const Panes& panes, const Setup& setup,
                                         const Screen& sc) {
    std::vector<Selectable> out;
    const std::vector<std::int64_t> order = presentation_order(setup, panes);
    for (std::size_t depth = 0; depth < order.size(); ++depth) {
        const std::int64_t kind = order[depth];
        if (!kind_takes_keyboard(kind)) {
            continue;
        }
        const PaneBounds where = bounds_of(panes, setup, kind, sc);
        if (!where.open || where.rect.empty()) {
            continue;
        }
        out.push_back(Selectable{kind, where.rect, depth});
    }
    std::stable_sort(out.begin(), out.end(), [](const Selectable& a, const Selectable& b) {
        if (a.rect.y != b.rect.y) {
            return a.rect.y < b.rect.y;
        }
        if (a.rect.x != b.rect.x) {
            return a.rect.x < b.rect.x;
        }
        return a.depth > b.depth;
    });
    return out;
}

} // namespace

// WL-FOCUS-12 -- agents/workshop/focus.md
std::vector<std::int64_t> selectable_panes(const Panes& panes, const Setup& setup,
                                           const Screen& sc) {
    std::vector<std::int64_t> out;
    for (const Selectable& p : selectable_rects(panes, setup, sc)) {
        out.push_back(p.kind);
    }
    return out;
}

// WL-FOCUS-12 -- agents/workshop/focus.md
std::optional<std::int64_t> cycle_pane(const Panes& panes, const Setup& setup, const Screen& sc,
                                       std::int64_t from, std::int64_t step) {
    const std::vector<std::int64_t> order = selectable_panes(panes, setup, sc);
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

// WL-FOCUS-12 -- agents/workshop/focus.md
std::optional<std::int64_t> spatial_neighbour(const Panes& panes, const Setup& setup,
                                              const Screen& sc, std::int64_t from, Act dir) {
    const bool horizontal = dir == Act::kFocusLeft || dir == Act::kFocusRight;
    if (!horizontal && dir != Act::kFocusUp && dir != Act::kFocusDown) {
        return std::nullopt;
    }
    const std::vector<Selectable> all = selectable_rects(panes, setup, sc);
    const auto here = std::find_if(all.begin(), all.end(),
                                   [from](const Selectable& p) { return p.kind == from; });
    if (here == all.end()) {
        if (all.empty()) {
            return std::nullopt;
        }
        return all.front().kind;
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
        case Act::kFocusLeft: return edge ? cx <= f.x : cx < fcx;
        case Act::kFocusRight: return edge ? cx >= f.x + f.w : cx > fcx;
        case Act::kFocusUp: return edge ? cy <= f.y : cy < fcy;
        default: return edge ? cy >= f.y + f.h : cy > fcy;
        }
    };
    const auto score = [&](const PixelRect& r) {
        std::int64_t gap = 0;
        switch (dir) {
        case Act::kFocusLeft: gap = f.x - (r.x + r.w); break;
        case Act::kFocusRight: gap = r.x - (f.x + f.w); break;
        case Act::kFocusUp: gap = f.y - (r.y + r.h); break;
        default: gap = r.y - (f.y + f.h); break;
        }
        const std::int64_t across =
            horizontal ? std::abs(r.y + r.h / 2 - fcy) : std::abs(r.x + r.w / 2 - fcx);
        return (gap > 0 ? gap : 0) + 2 * across;
    };
    for (const bool edge : {true, false}) {
        const Selectable* best = nullptr;
        std::int64_t best_score = 0;
        for (const Selectable& p : all) {
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

} // namespace zengine::workshop
