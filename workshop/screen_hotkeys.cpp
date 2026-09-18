// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- the effective keymap as a value -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/desktop.md (+1 register; agents/workshop.md routes)
//
// ⭐ THE HOST'S KEY-LIST OVERLAY USED TO BE PAINTED FROM THIS FILE. It was a mode the host owned,
// opened by a global row in its own catalog; the list is the desktop's Hotkeys pane now, and what
// is left here is the one thing only the host can say: which bindings are in force, where.

#include "screen.hpp"

namespace zengine::workshop {

// WL-DESK-11 -- agents/workshop/desktop.md
std::string keyboard_context_name(const Session& s, KeyContext ctx) {
    switch (ctx) {
    case KeyContext::kNaming: return "naming a layout";
    case KeyContext::kPaneNaming: return "naming a new pane";
    case KeyContext::kPicker: return "the + panel picker";
    case KeyContext::kContext: return "the contextual actions";
    case KeyContext::kArrangePane: {
        return s.arrange.addressed() ? "arranging " + ref_text(s.arrange.pane)
                                     : "arranging a pane";
    }
    case KeyContext::kArrangeDesk: return "arranging the desk";
    case KeyContext::kArrangeReset: return "arranging -- reset";
    case KeyContext::kDraft: return "editing a property";
    case KeyContext::kPaneEditor: return "the Pane Manager";
    case KeyContext::kPane: {
        const std::int64_t typing = keyboard_pane(s.panels);
        const RuntimePane* row =
            typing == kNoPaneKind ? nullptr : s.panels.runtime.of_kind(typing);
        return row != nullptr ? "pane " + row->name + " @" + row->provider
                              : "a focused pane";
    }
    case KeyContext::kGlobal: return "above every mode";
    case KeyContext::kNoText: return "above every mode, unless text has the keys";
    case KeyContext::kUnlessOwned:
        return "above every mode, unless the pane holding the keys owns it";
    default: return "command mode";
    }
}

namespace {

/// WHETHER THE MAKER'S FILE AUTHORED A ROW FOR THIS ID -- directly, or under the id it had
/// before its owner changed (`kRenamedActions`), which is read as this one.
bool authored_for(const Keymap& k, const std::string& id) {
    for (const AuthoredOverride& o : k.authored) {
        const char* now = renamed_to(o.action);
        if (o.action == id || (now != nullptr && id == now)) {
            return true;
        }
    }
    return false;
}

std::string spelled(const Gesture& g) { return is_bound(g) ? gesture_word(g) : std::string(); }

} // namespace

// WL-DESK-11 -- agents/workshop/desktop.md
KeymapShown keymap_shown(const Session& s, const std::string& file, const std::string& word) {
    const Keymap& k = s.keymap;
    KeymapShown out;
    out.file = file;
    out.word = word;
    const auto add = [&out](std::string group, std::string id, std::string label,
                            std::string gesture, bool authored, bool remappable) {
        out.rows.push_back(ShownBinding{std::move(group), std::move(id), std::move(label),
                                        std::move(gesture), authored, remappable});
    };
    // THE APPLICATION'S ROWS FIRST, because they are what a maker reaches for from anywhere:
    // the launches above every mode, and the default rows asked where nothing else took the key.
    for (const std::int64_t precedence : {app_precedence::kAboveModes, app_precedence::kDefault}) {
        for (const AppRow& row : k.app) {
            if (row.precedence != precedence) {
                continue;
            }
            add(precedence == app_precedence::kAboveModes
                    ? "above every mode -- the application's"
                    : "last, where nothing more specific took the key -- the application's",
                row.id, row.label, spelled(row.gesture), authored_for(k, row.id), true);
        }
    }
    // ...THEN THE HOST'S OWN, grouped by where each is answered, in the catalog's order. An
    // action with several rows in one group is listed once: one override moves all of them.
    const KeyContext order[] = {KeyContext::kGlobal,       KeyContext::kNoText,
                                KeyContext::kUnlessOwned,  KeyContext::kCommand,
                                KeyContext::kContext,      KeyContext::kArrangeDesk,
                                KeyContext::kArrangePane,  KeyContext::kArrangeReset,
                                KeyContext::kNaming,       KeyContext::kPaneNaming,
                                KeyContext::kDraft,        KeyContext::kPicker,
                                KeyContext::kPaneEditor};
    for (const KeyContext ctx : order) {
        std::vector<Act> listed;
        for (const ActionRow& row : kActionCatalog) {
            if (row.context != ctx) {
                continue;
            }
            bool seen = false;
            for (const Act a : listed) {
                seen = seen || a == row.act;
            }
            if (seen) {
                continue;
            }
            listed.push_back(row.act);
            add(keyboard_context_name(s, ctx), row.id, row.label, spelled(k.row_gesture(row)),
                k.override_for(row.act) != nullptr, true);
        }
    }
    // ...THEN EVERY PANE'S ROWS IN FORCE, under the name its office offered it by.
    for (const PaneRows& p : k.panes) {
        const RuntimePane* named = s.panels.runtime.of_kind(p.pane);
        const std::string group =
            named != nullptr ? "pane " + named->name + " @" + named->provider : "a pane";
        for (const PaneRow& row : p.rows) {
            add(group, row.id, row.label, spelled(row.gesture), authored_for(k, row.id), true);
        }
    }
    // ...AND THE TEXT BOX'S OWN KEYS, listed for discovery and marked: no file moves them.
    for (const component::EditingGesture& g : component::kEditingVocabulary) {
        add("the text box's own keys", std::string(), g.label,
            gesture_word(Gesture{g.scancode, g.modifiers}), false, false);
    }
    return out;
}

} // namespace zengine::workshop
