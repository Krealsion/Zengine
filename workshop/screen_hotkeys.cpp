// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's effective keymap, as a value.
// Workshop law: agents/workshop/desktop.md (+1 register; agents/workshop.md routes)

#include "screen.hpp"

#include <map>
#include <string_view>

namespace zengine::workshop {

// WL-DESK-11 -- agents/workshop/desktop-presenting.md
std::string keyboard_context_name(const Session& s, KeyContext ctx) {
    switch (ctx) {
    case KeyContext::kNaming: return "naming a layout";
    case KeyContext::kContext: return "the contextual actions";
    case KeyContext::kArrangePane: {
        return s.arrange.addressed() ? "arranging " + ref_text(s.arrange.pane)
                                     : "arranging a pane";
    }
    case KeyContext::kArrangeDesk: return "arranging the desk";
    case KeyContext::kArrangeReset: return "arranging -- reset";
    case KeyContext::kPane: {
        const std::int64_t typing = keyboard_pane(s.panes);
        const RuntimePane* row =
            typing == kNoPaneKind ? nullptr : s.panes.runtime.of_kind(typing);
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

/// WHETHER THE WEAVER'S FILE AUTHORED A ROW FOR THIS ID -- directly, or under the id it had
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

/// ...and as the Hotkeys view shows it, in the spelling the weaver chose for it.
// WL-KEY-16 -- agents/workshop/key-spelling.md
std::string shown_as(const Gesture& g, KeySpelling spelling) {
    return is_bound(g) ? spell_gesture(g, spelling) : std::string();
}

} // namespace

// WL-DESK-11 -- agents/workshop/desktop-presenting.md
KeymapShown keymap_shown(const Session& s, const std::string& file, const std::string& word) {
    const Keymap& k = s.keymap;
    KeymapShown out;
    out.file = file;
    out.word = word;
    const KeySpelling spelling = s.hotkeys_keys;
    out.spelling = spelling_word(spelling);
    // EACH ROW WITH ITS ADDRESS -- the office that declared it, the pane for a pane's row, the id --
    // and what a press of it may cause, as its declarer said it.
    const auto add = [&out, spelling](std::string group, std::string office, std::string pane,
                                      std::string id, std::string label, const Gesture& g,
                                      bool authored, bool remappable,
                                      std::vector<ActionCause> causes = {},
                                      std::string enters = {}) {
        out.rows.push_back(ShownBinding{std::move(group), std::move(office), std::move(pane),
                                        std::move(id), std::move(label), spelled(g),
                                        shown_as(g, spelling), authored, remappable,
                                        std::move(causes), std::move(enters)});
    };
    // THE APPLICATION'S ROWS FIRST, because they are what a weaver reaches for from anywhere:
    // the launches above every mode, and the default rows asked where nothing else took the key.
    for (const std::int64_t precedence : {app_precedence::kAboveModes, app_precedence::kDefault}) {
        for (const AppRow& row : k.app) {
            if (row.precedence != precedence) {
                continue;
            }
            add(precedence == app_precedence::kAboveModes
                    ? "above every mode -- the application's"
                    : "last, where nothing more specific took the key -- the application's",
                kDesktopRole, std::string(), row.id, row.label, row.gesture,
                authored_for(k, row.id), true, row.causes);
        }
    }
    // ...THEN THE HOST'S OWN, grouped by where each is answered, in the catalog's order. An
    // action with several rows in one group is listed once: one override moves all of them.
    const KeyContext order[] = {KeyContext::kGlobal,      KeyContext::kNoText,
                                KeyContext::kUnlessOwned, KeyContext::kCommand,
                                KeyContext::kContext,     KeyContext::kArrangeDesk,
                                KeyContext::kArrangePane, KeyContext::kArrangeReset,
                                KeyContext::kNaming};
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
            // ...ONE LINE PER KEY: an action with several keys in force is listed once per key,
            // in the authored order, so the pane can offer each key for removal on its own.
            for (const Gesture& g : k.row_gestures(row)) {
                add(keyboard_context_name(s, ctx), kWorkshopProvider, std::string(), row.id,
                    row.label, g, k.override_for(row.act) != nullptr, true);
            }
        }
    }
    // ...THEN EVERY PANE'S ROWS IN FORCE, under the name its office offered it by, each with the
    // causes its declaration gave it, and each pane's modes.
    for (const PaneRows& p : k.panes) {
        const RuntimePane* named = s.panes.runtime.of_kind(p.pane);
        const std::string group =
            named != nullptr ? "pane " + named->name + " @" + named->provider : "a pane";
        const std::string office = named != nullptr ? named->provider : std::string();
        const std::string pane = named != nullptr ? named->pane : std::string();
        std::map<std::string_view, const v3::PaneActionRow*> by_id;
        if (named != nullptr) {
            for (const v3::PaneActionRow& d : named->actions) by_id.emplace(d.id, &d);
        }
        for (const PaneRow& row : p.rows) {
            const auto found = by_id.find(row.id);
            const v3::PaneActionRow* declared = found != by_id.end() ? found->second : nullptr;
            add(group, office, pane, row.id, row.label, row.gesture, authored_for(k, row.id),
                true, declared != nullptr ? declared->causes : std::vector<ActionCause>{},
                declared != nullptr ? declared->enters : std::string());
        }
        if (named != nullptr && !named->mode.empty()) {
            out.modes.push_back(ShownModes{office, pane, named->mode, named->elsewhere});
        }
    }
    // ...AND THE TEXT BOX'S OWN KEYS, listed for discovery and marked: no file moves them.
    for (const component::EditingGesture& g : component::kEditingVocabulary) {
        add("the text box's own keys", std::string(), std::string(), std::string(), g.label,
            Gesture{g.scancode, g.modifiers}, false, false);
    }
    // ...AND WHAT THE WEAVER'S FILE SAYS OF IDS NO ROW ABOVE ANSWERS TO: a pane's other mode, a
    // pane not open. Each key it names, in the file's order, judged as the rows in force are; an
    // id the file names unreadably is left out, as its rows would be.
    std::vector<std::string> seen;
    for (const ShownBinding& row : out.rows) {
        seen.push_back(row.id);
    }
    for (const AuthoredOverride& o : k.authored) {
        const char* now = renamed_to(o.action);
        const std::string id = now != nullptr ? std::string(now) : o.action;
        bool listed = false;
        for (const std::string& already : seen) {
            listed = listed || already == id;
        }
        if (listed) {
            continue;
        }
        seen.push_back(id);
        const AuthoredGestures set = authored_gestures_for(k.authored, id, true);
        if (!set.outcome.accepted) {
            continue;
        }
        for (const Gesture& g : set.gestures) {
            out.moved.push_back(ShownBinding{"the weaver's keymap file", std::string(),
                                             std::string(), id, std::string(), spelled(g),
                                             shown_as(g, spelling), true, true, {}, {}});
        }
    }
    return out;
}

} // namespace zengine::workshop
