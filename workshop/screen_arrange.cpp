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

// WL-PTR-01, WL-PTR-03 -- agents/workshop/pointer.md
bool doubles_a_click(const ClickMemory& prior, std::int64_t place, std::uint64_t epoch,
                     const component::WordSpan& word, std::int64_t now_ms) noexcept {
    if (!prior.armed || !word.present()) {
        return false;
    }
    if (prior.place != place || prior.epoch != epoch) {
        return false;
    }
    if (prior.word_begin != word.begin || prior.word_end != word.end) {
        return false;
    }
    const std::int64_t since = now_ms - prior.at_ms;
    return since >= 0 && since <= kDoubleClickMs;
}

// WL-TAB-10 -- agents/workshop/tab-run.md
bool doubles_a_tab_click(const TabClickMemory& prior, std::size_t at,
                         std::int64_t now_ms) noexcept {
    if (!prior.armed || prior.at != at) {
        return false;
    }
    const std::int64_t since = now_ms - prior.at_ms;
    return since >= 0 && since <= kDoubleClickMs;
}

ClickMemory click_landed(std::int64_t place, std::uint64_t epoch,
                         const component::WordSpan& word, std::int64_t now_ms) noexcept {
    return ClickMemory{true, place, epoch, word.begin, word.end, now_ms};
}

// WL-FOCUS-01, WL-FOCUS-02 -- agents/workshop/focus.md
bool editor_has_keyboard(const Session& s) {
    if (s.panels.keyboard != panel::kEditor || !s.editor.open_document() ||
        !s.panels.has(panel::kEditor)) {
        return false;
    }
    const FineRect where =
        bounds_of(s.panels, s.setup.active, panel::kEditor, screen_of(s)).rect;
    return where.w > 0 && where.h > 0;
}

// WL-FOCUS-01, WL-FOCUS-02, WL-FOCUS-04 -- agents/workshop/focus.md
// WL-FILES-07 -- agents/workshop/files.md
bool files_has_keyboard(const Session& s) {
    if (s.panels.keyboard != panel::kProjectFiles || !s.panels.has(panel::kProjectFiles)) {
        return false;
    }
    const FilesPane& pane = s.panels.files;
    if (!pane.listing.known && pane.current_dir.empty() && !s.marks.somewhere_to_go()) {
        return false;
    }
    const FineRect where =
        bounds_of(s.panels, s.setup.active, panel::kProjectFiles, screen_of(s)).rect;
    return where.w > 0 && where.h > 0;
}

// WL-FOCUS-01 -- agents/workshop/focus.md; WL-PED-07 -- agents/workshop/pane-manager.md
bool pane_editor_has_keyboard(const Session& s) {
    if (s.panels.keyboard != panel::kPaneEditor || !s.panels.has(panel::kPaneEditor)) {
        return false;
    }
    const FineRect where =
        bounds_of(s.panels, s.setup.active, panel::kPaneEditor, screen_of(s)).rect;
    return where.w > 0 && where.h > 0;
}

bool pane_editor_draft_live(const Session& s) {
    for (const Row& r : s.pane_editor.rows) {
        if (r.editing()) {
            return true;
        }
    }
    return false;
}

// WL-KEY-03 -- agents/workshop/keyboard.md; WL-FOCUS-06 -- agents/workshop/focus.md
// WL-ATTN-09 -- agents/workshop/attention.md; WL-CTX-06 -- agents/workshop/contextual.md
// WL-PED-07 -- agents/workshop/pane-manager.md
KeyContext keyboard_context_beneath_menu(const Session& s) {
    if (s.setup.naming.open) {
        return KeyContext::kNaming;
    }
    // THE PANE CREATOR'S NAME PROMPT IS THE LAYOUT-NAME EDITOR'S TWIN, in the same
    // position and for the same reason: a maker typing a name has the keyboard whole. The
    // two cannot be open at once -- each is reachable only from a context the other owns
    // -- and the order is written down anyway, because an ordering that rests on a
    // reachability proof is one refactor from being silently wrong.
    if (s.pane_naming.open) {
        return KeyContext::kPaneNaming;
    }
    if (s.panels.picker.open) {
        return KeyContext::kPicker;
    }
    // THE CURRENT-CONDITION VIEW IS A MODE, IN THE PICKER'S OWN PLACE: below the
    // Terminal and the arrangement scopes, above a focused pane and a live draft. It owns
    // the keyboard while it is open for the picker's reason -- it is a list with a cursor
    // and a gesture on the selected row -- and it is deliberately NOT keys-modal like the
    // hotkey view, because its gestures are real application actions that every help
    // surface and the maker's own keymap file must be able to see.
    if (s.attention.open) {
        return KeyContext::kAttention;
    }
    if (is_runtime_kind(keyboard_pane(s.panels))) {
        return KeyContext::kPane;
    }
    // THE SOURCE EDITOR IS A PLACE IN THE FOCUSED PANE'S FAMILY: the candidate is the
    // same last-pressed memory an external pane rides, so the two cannot both be the
    // answer, and whichever the maker pointed the keys at LAST is the one that speaks --
    // own symmetry, with the same resolved-fresh discipline behind it.
    if (editor_has_keyboard(s)) {
        return KeyContext::kEditor;
    }
    // AND THE PROJECT BROWSER IS THE THIRD MEMBER OF THAT FAMILY, on the same terms. The
    // three share one candidate field, so at most one of them can be the answer and the
    // order between these two branches decides nothing -- it is written down anyway,
    // because an ordering that rests on a mutual-exclusion proof is one refactor from
    // being silently wrong.
    if (files_has_keyboard(s)) {
        return KeyContext::kFiles;
    }
    // AND THE PANE EDITOR IS THE FOURTH MEMBER, on the same candidate field and
    // the same terms. A draft open on one of ITS rows takes the keys as text exactly as the
    // Info panel's draft does one branch down -- `kDraft` is one context whichever inspector
    // the row belongs to, and `editing_key` asks which by asking this chain.
    if (pane_editor_has_keyboard(s)) {
        return pane_editor_draft_live(s) ? KeyContext::kDraft : KeyContext::kPaneEditor;
    }
    for (const Row& r : s.rows) {
        if (r.editing()) {
            return KeyContext::kDraft;
        }
    }
    return KeyContext::kCommand;
}

// WL-KEY-03 -- agents/workshop/keyboard.md; WL-FOCUS-06, WL-FOCUS-09 -- agents/workshop/focus.md
// WL-ARR-14 -- agents/workshop/arrangement.md; WL-CTX-08 -- agents/workshop/contextual.md
KeyContext keyboard_context(const Session& s) {
    if (s.terminal.open) {
        return KeyContext::kTerminal;
    }
    if (s.arrange.open) {
        if (s.arrange.resetting) {
            return KeyContext::kArrangeReset;
        }
        return s.arrange.desk ? KeyContext::kArrangeDesk : KeyContext::kArrangePane;
    }
    // THE CONTEXTUAL-ACTION SURFACE IS A MODE AT THE TOP OF THE PICKER'S BAND:
    // below the Terminal and the arrangement scopes, above everything a press or a draft
    // could otherwise reach. It must answer before a focused pane and a live draft or its
    // own navigation keys would leak into the thing beneath it -- the first-refusal rule
    // -- and it sits above the other transient overlays because it is opened by the LATER
    // deliberate gesture whenever both are somehow open at once.
    if (s.context.open) {
        return KeyContext::kContext;
    }
    return keyboard_context_beneath_menu(s);
}

// WL-ARR-13, WL-ARR-14 -- agents/workshop/arrangement.md
bool escape_may_shed_selection(KeyContext c) {
    return c == KeyContext::kFiles || c == KeyContext::kPaneEditor || c == KeyContext::kCommand;
}

} // namespace zengine::workshop
