// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's pane management: what a maker is arranging, and how.
// Workshop law: agents/workshop/focus.md (+9 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- PANE MANAGEMENT: what a maker is ARRANGING, and how ------------------------------

// WL-ARR-09 -- agents/workshop/arrangement.md
FineRect pane_edge_cell(const FineRect& r, std::int64_t edge) noexcept {
    const std::int64_t cell = surface::kCellSubs;
    const std::int64_t x0 = r.x;
    const std::int64_t x1 = r.w > cell ? r.x + r.w - cell : r.x;
    const std::int64_t y0 = r.y;
    const std::int64_t y1 = r.h > cell ? r.y + r.h - cell : r.y;
    const std::int64_t xm = r.w > cell ? r.x + (r.w - cell) / 2 : r.x;
    const std::int64_t ym = r.h > cell ? r.y + (r.h - cell) / 2 : r.y;
    switch (edge) {
    case pane_edge::kLeft: return FineRect{x0, ym, cell, cell};
    case pane_edge::kRight: return FineRect{x1, ym, cell, cell};
    case pane_edge::kTop: return FineRect{xm, y0, cell, cell};
    case pane_edge::kBottom: return FineRect{xm, y1, cell, cell};
    case pane_edge::kTopLeft: return FineRect{x0, y0, cell, cell};
    case pane_edge::kTopRight: return FineRect{x1, y0, cell, cell};
    case pane_edge::kBottomLeft: return FineRect{x0, y1, cell, cell};
    case pane_edge::kBottomRight: return FineRect{x1, y1, cell, cell};
    default: return FineRect{};
    }
}

// WL-ARR-01 -- agents/workshop/arrangement.md; WL-GEO-07 -- agents/workshop/geometry.md
std::int64_t pane_edge_at(const FineRect& r, std::int64_t sx, std::int64_t sy,
                          std::int64_t grain) noexcept {
    if (!r.contains_at(sx, sy, grain)) {
        return kNoPaneEdge;
    }
    const std::int64_t band_w = r.w < kPaneEdgeBandSubs ? r.w : kPaneEdgeBandSubs;
    const std::int64_t band_h = r.h < kPaneEdgeBandSubs ? r.h : kPaneEdgeBandSubs;
    const bool left = surface::sub_span_contains(r.x, band_w, sx, grain);
    const bool right =
        surface::sub_span_contains(surface::add_cells(r.x, r.w - band_w), band_w, sx, grain);
    const bool top = surface::sub_span_contains(r.y, band_h, sy, grain);
    const bool bottom =
        surface::sub_span_contains(surface::add_cells(r.y, r.h - band_h), band_h, sy, grain);
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
    if (is_runtime_kind(keyboard_pane(s.panels))) {
        return KeyContext::kPane;
    }
    return KeyContext::kCommand;
}

// WL-KEY-03 -- agents/workshop/keyboard.md; WL-FOCUS-06, WL-FOCUS-09 -- agents/workshop/focus.md
// WL-ARR-14 -- agents/workshop/arrangement.md; WL-CTX-08 -- agents/workshop/contextual.md
KeyContext keyboard_context(const Session& s) {
    if (s.arrange.open) {
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
    return keyboard_pane(s.panels);
}

/// Where the keys went to a list or to nothing, as against a place a maker types into, which
/// keeps its own keys: the contexts in which the application's default rows are answered.
// WL-ARR-13, WL-ARR-14 -- agents/workshop/arrangement.md
// WL-DESK-02 -- agents/workshop/desktop.md
bool default_row_context(KeyContext c) {
    return c == KeyContext::kCommand;
}

} // namespace zengine::workshop
