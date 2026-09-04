// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- what is true right now, projected, and what can I do
// with this, presented -- compiled once into `zengine-workshop-logic` and linked by the host and
// every suite; the declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/attention.md (+2 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- WHAT IS TRUE RIGHT NOW, PROJECTED ---------------------------------------------------

std::string pane_content_key(const PaneRef& ref) {
    return "pane.content-refused." + ref_text(ref);
}

std::string pane_window_key(const PaneRef& ref) {
    return "pane.not-presented." + ref_text(ref);
}

// WL-ATTN-03, WL-ATTN-04, WL-ATTN-05 -- agents/workshop/attention.md
std::vector<Condition> attention_conditions(const Session& s,
                                            const ProjectFrontier& frontier) {
    std::vector<Condition> out = s.conditions.rows;
    const Screen sc = screen_of(s);

    // DERIVED: a provider's update this pane could not keep. The pane holds it (`refusal`
    // is the body's sentence, `refusal_why` the reason), clears it on the next valid
    // content, and knows nothing about this projection -- so the condition disappears
    // because the pane recovered, with no retraction call anywhere in the path. That is
    // the measured defect this replaced, closed by construction rather than by discipline.
    for (const ExternalPane& pane : s.panels.external) {
        if (pane.refusal.empty()) {
            continue;
        }
        const RuntimePane* named = s.panels.runtime.of_kind(pane.kind);
        if (named == nullptr) {
            continue; // a pane whose catalog row has gone has no subject to name
        }
        const PaneRef ref{named->provider, named->pane};
        out.push_back(Condition{pane_content_key(ref),
                                "`" + ref_text(ref) + "` refused an update",
                                pane.refusal_why.empty() ? pane.refusal : pane.refusal_why,
                                surface::role::kAlert, std::string()});
    }

    // DERIVED: a pane the maker authored, that this build can resolve, and of which no cell
    // is on the screen. The word and the remedy are `pane_state`'s own -- one enumeration,
    // one classifier, and the remedy column that was already written beside it.
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panels)) {
        const std::int64_t state = pane_state_of(s.panels, s.setup.active, sc, row);
        if (state != pane_state::kRefused && state != pane_state::kWaiting &&
            state != pane_state::kOffRoom) {
            continue;
        }
        out.push_back(Condition{pane_window_key(row.ref),
                                "`" + ref_text(row.ref) + "` " + pane_state_word(state),
                                std::string(pane_state_remedy(state)), surface::role::kAccent,
                                "workshop.manage"});
    }

    // DERIVED: realization is stopped at a row waiting on the maker. Informative and
    // actionable, and deliberately NOT an error -- "waiting to be built is not a failure
    // and is not silence either" is the host's own sentence about this exact state.
    if (frontier.waiting) {
        std::string detail = "realization is stopped at `" + frontier.artifact + "`";
        if (frontier.blocked > 0) {
            detail += " with " + std::to_string(frontier.blocked) + " authored row" +
                      (frontier.blocked == 1 ? "" : "s") + " behind it";
        }
        out.push_back(Condition{kFrontierKey, "project waiting on `" + frontier.artifact + "`",
                                detail, surface::role::kAccent, "builder.frontier"});
    }

    std::sort(out.begin(), out.end(), ranks_before);
    return out;
}

std::vector<Condition> attention_shown(const Session& s,
                                       const ProjectFrontier& frontier) {
    std::vector<Condition> out;
    for (Condition& c : attention_conditions(s, frontier)) {
        if (!s.attention.hides(c)) {
            out.push_back(std::move(c));
        }
    }
    return out;
}

// WL-ATTN-06 -- agents/workshop/attention.md
std::string attention_compact(const std::vector<Condition>& shown) {
    if (shown.empty()) {
        return std::string();
    }
    std::string line = shown.front().compact;
    if (shown.size() > 1) {
        line += " (+" + std::to_string(shown.size() - 1) + " more)";
    }
    return line;
}

// WL-ATTN-09 -- agents/workshop/attention.md
void paint_attention(surface::SurfaceLayer& layer, const Session& s, const Screen& sc,
                     const ProjectFrontier& frontier) {
    if (!s.attention.open) {
        return;
    }
    const FineRect b = attention_bounds(sc);
    paint_panel_frame(layer, b, kTransientChrome);
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a slot with no room for a row says nothing rather than lying about the room
    }
    surface::SurfaceTextRegion region = panel_prose_region(place);
    const auto say = [&region, &place](const std::string& text, std::int64_t role) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    const std::vector<Condition> shown = attention_shown(s, frontier);
    say("ATTENTION -- " + std::to_string(shown.size()) +
            (shown.size() == 1 ? " condition, " : " conditions, ") +
            hotkey_text(s.keymap, Act::kAttentionDismiss) + " hides one, " +
            hotkey_text(s.keymap, Act::kAttentionClose) + " closes",
        surface::role::kAccent);
    const std::size_t budget =
        place.rows > 1 ? static_cast<std::size_t>(place.rows - 1) : 0;
    if (shown.empty()) {
        // NOTHING IS WRONG, SAID IN WORDS. A maker who opened this deliberately is owed an
        // answer, and an empty box is not one. (The compact indicator is already absent --
        // this surface is reachable whether or not anything is true.)
        if (budget > 0) {
            say("  nothing needs your attention right now", surface::role::kMuted);
        }
        layer.texts.push_back(std::move(region));
        return;
    }
    // THE CURSOR'S OWN BLOCK IS COMPOSED AND RESERVED BEFORE THE LIST IS WINDOWED, and that
    // ordering is the whole of this painter's honesty.
    //
    // A window computed over the COMPACT rows alone is right until the row it is keeping in
    // view spends three more beneath it -- and what then falls off the bottom of the region
    // is the omission marker, which is the one row that was there to say something had been
    // dropped. A region PADS what it was not given and SILENTLY DROPS what will not fit
    // (region.hpp's projection), so the over-spend is invisible in both media. A bound that
    // grows when it is exceeded is not a bound, so the block comes out of the budget first
    // and `list_window`'s three rules then run over what is left.
    //
    // THE CURSOR IS RESOLVED ONCE, HERE, and every question below spends the same answer.
    // The population is DERIVED, so it can shrink between a keystroke and a repaint with no
    // gesture in between -- and a painter that clamped in one place and compared the raw
    // value in another would compose an explanation for a row it then never marks.
    const std::size_t cursor =
        s.attention.cursor < shown.size() ? s.attention.cursor : shown.size() - 1;
    const Condition& at = shown[cursor];
    std::vector<std::string> block =
        detail::wrap(at.detail, place.columns - detail::kWrapIndent);
    if (!at.action.empty()) {
        // AN ACTION IS A NAME AND ITS GESTURE IS THE KEYMAP'S, resolved at this paint. The
        // row says what a maker could press somewhere else; nothing here can press it.
        for (const ActionRow& row : kActionCatalog) {
            if (at.action == row.id) {
                block.push_back("try: " + gesture_text(s.keymap.row_gesture(row)) + " " +
                                row.label);
                break;
            }
        }
    }
    // THE LIST KEEPS `list_window`'S OWN FLOOR OF THREE, so the cursor's row is always in
    // the window it is being explained inside of. Below four rows there is no explanation at
    // all rather than an explanation with no statement over it, and what the reserve cannot
    // hold is counted in the same words every other omission on this screen is counted in.
    const std::size_t reserve = budget >= 4 ? (block.size() < budget - 3 ? block.size()
                                                                        : budget - 3)
                                            : 0;
    const ListWindow win = list_window(shown.size(), cursor, budget - reserve);
    if (win.before > 0) {
        say("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
    }
    for (std::size_t i = win.first; i < win.first + win.count; ++i) {
        const Condition& c = shown[i];
        const bool here = i == cursor;
        say(std::string(here ? "> " : "  ") + c.compact, c.role);
        if (!here || reserve == 0) {
            continue;
        }
        const std::size_t said = block.size() > reserve ? reserve - 1 : block.size();
        for (std::size_t line = 0; line < said; ++line) {
            say("    " + block[line], surface::role::kMuted);
        }
        if (said < block.size()) {
            say("    " + omitted_text(block.size() - said, "more"), surface::role::kMuted);
        }
    }
    if (win.after > 0) {
        say("  " + omitted_text(win.after, "more"), surface::role::kMuted);
    }
    layer.texts.push_back(std::move(region));
}

// ---- WHAT CAN I DO WITH THIS, PRESENTED --------------------------------------------------

// WL-CTX-03, WL-CTX-04 -- agents/workshop/contextual.md
std::string context_entry_text(const ContextEntry& entry) {
    if (entry.is_group) {
        return std::string(entry.group) + " >";
    }
    return entry.row != nullptr ? entry.row->label : std::string();
}

std::int64_t context_label_columns(const std::vector<ContextEntry>& rows) {
    std::int64_t widest = 0;
    for (const ContextEntry& entry : rows) {
        const std::int64_t len = static_cast<std::int64_t>(context_entry_text(entry).size());
        widest = len > widest ? len : widest;
    }
    return widest;
}

// WL-CTX-06 -- agents/workshop/contextual.md; WL-TAB-12 -- agents/workshop/tab-run.md
std::string context_annotation(const Session& s, const ContextEntry& entry) {
    if (entry.is_group || entry.row == nullptr) {
        return std::string(); // folders are not actions and have no gesture to teach
    }
    const KeyContext beneath = keyboard_context_beneath_menu(s);
    bool requestable = false;
    for (const ActionRow& row : kActionCatalog) {
        if (row.act == entry.row->act && active_in(row.context, beneath)) {
            requestable = true;
            break;
        }
    }
    if (!requestable) {
        return std::string();
    }
    //...AND NEITHER DOES A ROW WITH NO GESTURE. The four layout-tab operations
    // are reached from this very menu and from no key; annotating them with `?` would
    // teach a maker a binding that does not exist, in the one surface whose annotation
    // exists to teach the faster way of doing what they just chose.
    if (!is_bound(s.keymap.gesture_of(entry.row->act))) {
        return std::string();
    }
    if (entry.row->act == Act::kObjectDelete && s.context.object != s.selected) {
        return std::string();
    }
    // ⚠ AND THE SAME REFINEMENT FOR A LAYOUT TAB, found by the live TUI witness
    // rather than by a case. `^w` closes the LIVE layout; this menu's row closes the
    // CAPTURED one. The two are the same act exactly when the tab a maker pointed at is
    // the one they are standing on, so the annotation is shown then and only then --
    // anything else teaches a key that acts on a different layout than the row it sits
    // beside, which is `object.delete`'s own reason one subject over.
    if (s.context.subject == context_subject::kLayout &&
        s.context.layout != s.setup.active_at) {
        return std::string();
    }
    return hotkey_text(s.keymap, entry.row->act);
}

std::string context_row_text(const Session& s, const ContextEntry& entry,
                             std::int64_t label_columns) {
    const std::string text = context_entry_text(entry);
    const std::string gesture = context_annotation(s, entry);
    if (gesture.empty()) {
        return text;
    }
    return detail::pad(text, static_cast<std::size_t>(label_columns + 2)) + gesture;
}

// WL-CTX-03 -- agents/workshop/contextual.md
FineRect context_bounds(const Session& s, const Screen& sc) {
    const ContextMenu& menu = s.context;
    const std::vector<ContextEntry> rows = context_population(menu.subject, menu.group);
    const std::int64_t label_cols = context_label_columns(rows);
    std::int64_t want_cols = 0;
    for (const ContextEntry& entry : rows) {
        const std::int64_t len =
            2 + static_cast<std::int64_t>(context_row_text(s, entry, label_cols).size());
        want_cols = len > want_cols ? len : want_cols;
    }
    want_cols = want_cols > kContextMaxCols ? kContextMaxCols : want_cols;
    const std::int64_t want_rows = static_cast<std::int64_t>(rows.size());
    std::int64_t x = menu.anchor_x;
    std::int64_t y = menu.anchor_y;
    if (!menu.anchored) {
        const ui::Rect slot = placement_bounds(placement::kOverlayStack, 0, sc);
        x = slot.x;
        y = slot.y;
    }
    return popup_bounds_at(want_cols, want_rows, x, y, sc);
}

void paint_context(surface::SurfaceLayer& layer, const Session& s, const Screen& sc) {
    if (!s.context.open) {
        return;
    }
    const FineRect b = context_bounds(s, sc);
    paint_panel_frame(layer, b, kTransientChrome);
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a popup with no room for a row says nothing rather than lying about the room
    }
    surface::SurfaceTextRegion region = panel_prose_region(place);
    const auto say = [&region, &place](const std::string& text, std::int64_t role) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    // THE FIRST ROW IS AN ACTION. Nothing announces that a menu of actions
    // contains actions, and nothing restates the two gestures the band's legend is
    // already saying in the maker's own bindings for as long as this surface is open.
    const ContextMenu& menu = s.context;
    const std::vector<ContextEntry> rows = context_population(menu.subject, menu.group);
    const std::int64_t label_cols = context_label_columns(rows);
    const std::size_t budget = static_cast<std::size_t>(place.rows);
    const std::size_t cursor = context_cursor_bound(menu.cursor, rows.size());
    const ListWindow win = list_window(rows.size(), cursor, budget);
    if (win.before > 0) {
        say("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
    }
    for (std::size_t i = win.first; i < win.first + win.count; ++i) {
        const bool here = i == cursor;
        say(std::string(here ? "> " : "  ") + context_row_text(s, rows[i], label_cols),
            here ? surface::role::kAccent : surface::role::kFill);
    }
    if (win.after > 0) {
        say("  " + omitted_text(win.after, "more"), surface::role::kMuted);
    }
    layer.texts.push_back(std::move(region));
}

ContextPressAt context_press_at(const Session& s, const Screen& sc, std::int64_t space,
                                std::int64_t x, std::int64_t y, const PointedAt& at) {
    ContextPressAt out;
    if (!s.context.open) {
        return out;
    }
    const FineRect b = context_bounds(s, sc);
    if (!b.contains_at(at.sub.x, at.sub.y, at.grain)) {
        return out;
    }
    out.inside = true;
    // THE SAME CALL THE PAINTER MAKES, and not a re-derivation beside it: the
    // painter asks `panel_prose_place` for this rectangle and so does this, so the chrome
    // inset, the metric and the row budget are one answer rather than two that agree
    // today. `prose_at` takes the region's CELL origin (the number on the published
    // `SurfaceTextRegion`), so the wire spelling of the same interior is what it is handed.
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return out;
    }
    const surface::SurfaceTextRegion wire = panel_prose_region(place);
    const ProseAt where = prose_at(space, x, y, wire.x, wire.y, place.fit);
    if (!where.understood || where.column < 0 || where.column >= place.columns ||
        where.row < 0 || where.row >= place.rows) {
        return out;
    }
    // PAINTED ROW i IS POPULATION ROW i. No heading is reserved any more, so there is no
    // offset here and none in the painter -- the one arithmetic that could have made a
    // press choose a different row from the one under it is simply gone.
    const std::vector<ContextEntry> rows =
        context_population(s.context.subject, s.context.group);
    const std::size_t budget = static_cast<std::size_t>(place.rows);
    const std::size_t cursor = context_cursor_bound(s.context.cursor, rows.size());
    const ListWindow win = list_window(rows.size(), cursor, budget);
    std::int64_t offset = where.row;
    if (win.before > 0) {
        if (offset == 0) {
            return out; // the `... n earlier` marker
        }
        --offset;
    }
    if (static_cast<std::size_t>(offset) >= win.count) {
        return out; // the `... n more` marker, or the region's own emptiness
    }
    out.entry = true;
    out.index = win.first + static_cast<std::size_t>(offset);
    return out;
}

} // namespace zengine::workshop
