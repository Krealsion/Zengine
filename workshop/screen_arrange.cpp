// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- pane management: what a maker is arranging, and how --
// compiled once into `zengine-workshop-logic` and linked by the host and every suite; the
// declarations, the constants and the constexpr functions stay in the header.
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

// (`doubles_a_click` WAS HERE, the word-selecting double-click's qualification, and left with
// its one spender; `screen.hpp` says so where its record stood.)

// WL-PTR-01 -- agents/workshop/pointer.md; WL-TAB-10 -- agents/workshop/tab-run.md
bool doubles_a_tab_click(const TabClickMemory& prior, std::size_t at,
                         std::int64_t now_ms) noexcept {
    if (!prior.armed || prior.at != at) {
        return false;
    }
    const std::int64_t since = now_ms - prior.at_ms;
    return since >= 0 && since <= kDoubleClickMs;
}

// ⭐ `editor_has_keyboard` WAS HERE AND IS GONE: the Editor is a pane, and a pane's readiness
// is `keyboard_pane`'s one answer -- open, runtime kind, room granted -- resolved fresh at
// every spend like every other pane's. The "needs a document open" clause that made the
// built-in a candidate that could decline the keys is the Editor weave's own now: an empty
// Editor takes the keys as any pane does and does nothing with them.

// ⭐ `pane_editor_has_keyboard` AND `pane_editor_draft_live` WERE HERE: the host's Pane Manager
// was the last built-in that took the keyboard, and its draft the last this host held.

// WL-KEY-03 -- agents/workshop/keyboard.md; WL-FOCUS-06 -- agents/workshop/focus.md
// WL-CTX-06 -- agents/workshop/contextual.md
KeyContext keyboard_context_beneath_menu(const Session& s) {
    if (s.setup.naming.open) {
        return KeyContext::kNaming;
    }
    // ⚠ THE PANE CREATOR'S NAME PROMPT AND THE `p` PICKER WERE MODES HERE, in that order. The
    // name is typed in the desktop's Pane Manager now, as that pane's own draft, and the picker
    // retired: its two duties are the Pane Manager's rows over the launch and close doors.
    // ⚠ THE CURRENT-CONDITION VIEW WAS A MODE HERE TOO, and is a pane (`Zengine/attention-pane/`).
    if (is_runtime_kind(keyboard_pane(s.panels))) {
        return KeyContext::kPane;
    }
    // ⭐ THE HOST'S PANE MANAGER WAS THE LAST BRANCH HERE -- a built-in candidate for the keys,
    // `kDraft` while one of its rows was being typed into and `kPaneEditor` otherwise. Every
    // pane that takes the keys now is a runtime pane, answered by the branch above.
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
    // THE CONTEXTUAL-ACTION SURFACE IS A MODE AT THE TOP OF THE MODES' BAND:
    // below the arrangement scopes, above everything a press or a draft
    // could otherwise reach. It must answer before a focused pane and a live draft or its
    // own navigation keys would leak into the thing beneath it -- the first-refusal rule
    // -- and it sits above the other transient overlays because it is opened by the LATER
    // deliberate gesture whenever both are somehow open at once.
    // ...AND A PANE'S MENU PRESENTED BY ITS PRESENTER IS THE SAME KIND OF SURFACE: its keys are
    // read through the same contextual rows a maker can move, and forwarded to the presenter.
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

// ⭐ WAS `escape_may_shed_selection`, AND THE RENAME IS THE MIGRATION (WL-DESK-02). The
// predicate never was about Escape: it is about WHERE THE KEYS WENT -- a list or nothing, as
// against a place a maker types into, which keeps its own keys while it holds them. Escape was
// simply the only gesture that had ever asked. Now that the application's default rows are a
// declared, replaceable set, any of them is answered exactly here, under exactly this test.
// WL-ARR-13, WL-ARR-14 -- agents/workshop/arrangement.md
// WL-DESK-02 -- agents/workshop/desktop.md
bool default_row_context(KeyContext c) {
    // (The host's Pane Manager's list was the other such place, until it retired.)
    return c == KeyContext::kCommand;
}

} // namespace zengine::workshop
