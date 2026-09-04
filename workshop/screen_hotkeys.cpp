// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- the full hotkey view -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/keyboard.md (+1 register; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- THE FULL HOTKEY VIEW -------------------------------------------------------------

// WL-KEY-11 -- agents/workshop/keyboard.md
std::string keyboard_context_name(const Session& s, KeyContext ctx) {
    switch (ctx) {
    case KeyContext::kTerminal: return "the terminal line";
    case KeyContext::kNaming: return "naming a layout";
    case KeyContext::kPaneNaming: return "naming a new pane";
    case KeyContext::kPicker: return "the + panel picker";
    case KeyContext::kAttention: return "what needs attention";
    case KeyContext::kContext: return "the contextual actions";
    case KeyContext::kArrangePane: {
        return s.arrange.addressed() ? "arranging " + ref_text(s.arrange.pane)
                                     : "arranging a pane";
    }
    case KeyContext::kArrangeDesk: return "arranging the desk";
    case KeyContext::kArrangeReset: return "arranging -- reset";
    case KeyContext::kDraft: return "editing a property";
    case KeyContext::kEditor: return "the source editor";
    case KeyContext::kFiles: return "the project browser";
    case KeyContext::kPaneEditor: return "the Pane Manager";
    case KeyContext::kPane: {
        const std::int64_t typing = keyboard_pane(s.panels);
        const RuntimePane* row =
            typing == kNoPaneKind ? nullptr : s.panels.runtime.of_kind(typing);
        return row != nullptr ? "pane " + row->name + " @" + row->provider
                              : "a focused pane";
    }
    default: return "command mode";
    }
}

// WL-KEY-10, WL-KEY-11 -- agents/workshop/keyboard.md
std::vector<HotkeyRow> hotkeys_rows(const Session& s) {
    const KeyContext ctx = keyboard_context(s);
    const Keymap& k = s.keymap;
    std::vector<HotkeyRow> rows;
    const auto entry = [&rows](const std::string& gesture, const std::string& label) {
        rows.push_back(HotkeyRow{"  " + detail::pad(gesture, 14) + label,
                                 surface::role::kFill});
    };
    const auto group = [&rows](const std::string& name) {
        if (rows.size() > 1) { // a blank row between groups; none under the heading
            rows.push_back(HotkeyRow{std::string(), surface::role::kMuted});
        }
        rows.push_back(HotkeyRow{name, surface::role::kAccent});
    };

    rows.push_back(HotkeyRow{"HOTKEYS -- " + hotkey_text(k, Act::kHotkeys) + " or esc closes",
                             surface::role::kAccent});
    group(keyboard_context_name(s, ctx));
    if (ctx == KeyContext::kPane) {
        // THE HONEST WHOLE OF A PANE'S KEY STORY. Workshop forwards every ordinary key
        // and every character uninterpreted and is deliberately never told what they
        // mean (the seam's own doctrine), so the one truthful sentence is ownership --
        // pretending to know a provider's bindings would be a claim made out of silence.
        rows.push_back(HotkeyRow{"  every ordinary key and character goes to the pane;",
                                 surface::role::kFill});
        rows.push_back(HotkeyRow{"  what each one means there is the provider's own.",
                                 surface::role::kFill});
    } else {
        for (const ActionRow& row : kActionCatalog) {
            if (row.context == ctx) {
                entry(gesture_text(k.row_gesture(row)), row.label);
            }
        }
    }
    bool above = false;
    for (const ActionRow& row : kActionCatalog) {
        const bool is_class =
            row.context == KeyContext::kGlobal || row.context == KeyContext::kNoText ||
            row.context == KeyContext::kNoEditor;
        if (!is_class || !active_in(row.context, ctx)) {
            continue;
        }
        if (!above) {
            group("answered above every mode");
            above = true;
        }
        entry(gesture_text(k.row_gesture(row)), row.label);
    }
    if (ctx == KeyContext::kTerminal || ctx == KeyContext::kNaming ||
        ctx == KeyContext::kPaneNaming || ctx == KeyContext::kDraft) {
        group("the text box's own keys (not remappable)");
        for (const component::EditingGesture& g : component::kEditingVocabulary) {
            entry(gesture_text(Gesture{g.scancode, g.modifiers}), g.label);
        }
    }
    // THE EDITOR'S OWN MECHANICS, from its declaration rows (editor.hpp) exactly as the
    // component's come from theirs: shown for discovery, marked not remappable, their
    // executable truth being `EditorBuffer::consume` and never this keymap.
    if (ctx == KeyContext::kEditor) {
        group("the editor's own keys (not remappable)");
        for (const component::EditingGesture& g : kEditorVocabulary) {
            entry(gesture_text(Gesture{g.scancode, g.modifiers}), g.label);
        }
    }
    return rows;
}

// WL-CTX-03 -- agents/workshop/contextual.md; WL-KEY-10 -- agents/workshop/keyboard.md
FineRect hotkeys_bounds(const Session& s, const Screen& sc) {
    const std::vector<HotkeyRow> rows = hotkeys_rows(s);
    std::int64_t want_cols = 0;
    for (const HotkeyRow& row : rows) {
        const std::int64_t len = static_cast<std::int64_t>(row.text.size());
        want_cols = len > want_cols ? len : want_cols;
    }
    const std::int64_t want_rows = static_cast<std::int64_t>(rows.size());
    const ui::Rect corner = cells_covered(overlay_column(sc));
    std::int64_t x = corner.x;
    std::int64_t y = corner.y;
    const std::int64_t chosen = selected_pane(s.panels);
    if (chosen != kNoPaneKind) {
        const FineRect anchor = bounds_of(s.panels, s.setup.active, chosen, sc).rect;
        if (anchor.w > 0 && anchor.h > 0) {
            // THE ANCHOR IS A CELL CORNER. The view is screen furniture and never moves by
            // less than a cell -- `picker_bounds`'s own rule -- so the pane's fine top-left
            // is read at the cell grain it is drawn on, which is also where its visible
            // boundary is.
            const ui::Rect at = cells_covered(anchor);
            x = at.x;
            y = at.y;
        }
    }
    return popup_bounds_at(want_cols, want_rows, x, y, sc);
}

void paint_hotkeys(surface::SurfaceLayer& layer, const Session& s, const Screen& sc) {
    if (!s.hotkeys.open) {
        return;
    }
    const FineRect b = hotkeys_bounds(s, sc);
    paint_panel_frame(layer, b, kTransientChrome);
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a place with no room for a row says nothing rather than lying about the room
    }
    // THE SAME ROWS THE EXTENT WAS MEASURED FROM, so on a character medium every one
    // of them lands whole on its own row, and on the shipped face the same holds with the
    // face's slack to spare.
    const std::vector<HotkeyRow> rows = hotkeys_rows(s);
    surface::SurfaceTextRegion region = panel_prose_region(place);
    const auto say = [&region, &place](const std::string& text, std::int64_t role) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    // NO CURSOR, SO NO WINDOW TO KEEP IT IN: where the room is smaller than the list, the
    // list is cut at the room and the cut is counted, the completion list's own wording --
    // this place cannot show a row AND tell a maker what it is hiding, so it tells them.
    // The heading is row 0 of the composition and survives a one-row room; the marker
    // takes a row only where one is spare.
    const std::size_t budget = static_cast<std::size_t>(place.rows);
    const bool cut = rows.size() > budget;
    std::size_t shown = rows.size();
    if (cut) {
        shown = budget > 1 ? budget - 1 : budget;
    }
    for (std::size_t i = 0; i < shown; ++i) {
        say(rows[i].text, rows[i].role);
    }
    if (cut && budget > 1) {
        say("  " + omitted_text(rows.size() - shown, "more"), surface::role::kMuted);
    }
    layer.texts.push_back(std::move(region));
}

} // namespace zengine::workshop
