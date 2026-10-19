// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s command mode: each key bound there calls one operation, the keyboard's way
// to a pane, and the presentation preferences a toggle states and saves.
// Workshop law: agents/workshop/keyboard.md (+3 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// The presentation preferences, each one session value the repaint spends: pane titles
// (`refresh_external_rooms` re-grants the rooms whose capacity moved) and how the band and the
// Hotkeys view spell a key. A toggle states one, so it is written to the prefs file then, under
// three walls: no path is live-only, a refused file is never overwritten (`prefs_bad_`), and a
// failed write says so.
// WL-KEY-16 -- agents/workshop/key-spelling.md
void WorkshopWeave::state_presentation(Act a) {
    const char* what = a == Act::kPaneTitles ? "pane titles were not toggled"
                       : a == Act::kBandKeys ? "the band's keys were not respelled"
                                             : "the Hotkeys view's keys were not respelled";
    // The toggle is saved as it is stated, so it is a file write, judged for the hand.
    if (const std::string refused = judge_classes(input_actor_, {scope::kWrite}, {});
        !refused.empty()) {
        say(std::string(what) + " -- " + refused, true);
        return;
    }
    load_prefs(); // a toggle before any surface exists still toggles the truth
    const auto other = [](KeySpelling s) {
        return s == KeySpelling::kFull ? KeySpelling::kShort : KeySpelling::kFull;
    };
    std::string note;
    if (a == Act::kPaneTitles) {
        session_.pane_titles = !session_.pane_titles;
        note = session_.pane_titles
                   ? "pane titles shown"
                   : "pane titles hidden -- a pane holding the keyboard still shows its own";
    } else if (a == Act::kBandKeys) {
        session_.band_keys = other(session_.band_keys);
        note = session_.band_keys == KeySpelling::kFull
                   ? "the band spells keys in full, as a keymap file writes them"
                   : "the band spells keys short";
    } else {
        session_.hotkeys_keys = other(session_.hotkeys_keys);
        note = session_.hotkeys_keys == KeySpelling::kFull
                   ? "the Hotkeys view and a pane's manual spell keys in full, as a keymap file "
                     "writes them"
                   : "the Hotkeys view and a pane's manual spell keys short";
    }
    // THE TOGGLE IS WORKSHOP'S OWN SETTING STATED: kept where the settings door keeps it, so the
    // file holds one truth.
    prefs_persist::Presentation said = prefs_persist::presentation_of(prefs_);
    said.titles_shown = session_.pane_titles;
    said.band_keys = session_.band_keys;
    said.hotkeys_keys = session_.hotkeys_keys;
    prefs_persist::put_presentation(prefs_, said);
    bool bad = false;
    if (!host_->prefs_path.empty()) {
        if (const std::string not_written = write_prefs(); !not_written.empty()) {
            note += "; not saved: " + not_written;
            bad = true;
        }
    }
    say(note, bad);
}

namespace {

/// Where a direction looks from the selected pane, in the words a refusal says it.
const char* direction_words(Act a) {
    switch (a) {
    case Act::kFocusLeft: return "to the left of";
    case Act::kFocusRight: return "to the right of";
    case Act::kFocusUp: return "above";
    default: return "below";
    }
}

} // namespace

// WL-KEY-01 -- agents/workshop/keyboard.md
// WL-GUEST-09 -- agents/workshop/guests.md
// WL-FOCUS-12 -- agents/workshop/focus.md
void WorkshopWeave::command(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    // Each arm calls one operation. A weaver's keymap row naming a retired id is kept byte for byte
    // and said at load (`kRetiredActions`); nothing here answers it.
    const Act a = session_.keymap.action_for(KeyContext::kCommand, k.scancode, k.modifiers);
    switch (a) {
    // The two setup gestures, ordinary commands: `s` writes the setup file and `r` reads it;
    // naming a layout is `layout.rename`'s.
    case Act::kSetupSave: save_setup(); break;
    case Act::kSetupRestore: restore_setup(mail); break;
    // The layout shelf: stepping is over the whole population, painted or not. The four that take
    // a position reach the keyboard only through the live layout: without a captured subject, a
    // keyboard can truthfully name only that one.
    case Act::kLayoutNext: step_layout(+1, mail); break;
    case Act::kLayoutPrevious: step_layout(-1, mail); break;
    case Act::kLayoutNew: new_layout(mail); break;
    case Act::kLayoutRemove: drop_layout(session_.setup.active_at, mail); break;
    case Act::kLayoutRename: open_layout_rename(session_.setup.active_at); break;
    case Act::kLayoutDuplicate:
        duplicate_layout(session_.setup.active_at, mail);
        break;
    case Act::kLayoutMoveLeft: shift_layout(session_.setup.active_at, -1); break;
    case Act::kLayoutMoveRight: shift_layout(session_.setup.active_at, +1); break;
    // Arrange the desk: a printable trigger pays the swallow rule and buys a mode whose own keys
    // need no modifier.
    case Act::kArrangeDesk: open_arrange_desk(); break;
    // WHAT CAN I DO WITH THIS? -- the contextual-action surface, on the subject
    // command mode can truthfully name.
    case Act::kContextOpen: open_context_ambient(); break;
    case Act::kPaneTitles:
    case Act::kBandKeys:
    case Act::kHotkeysKeys: state_presentation(a); break;
    case Act::kQuit: quit(mail); break;
    // THE KEYBOARD'S WAY TO A PANE: the selection moves, lifting the pane it reaches as a press
    // would, and writes the keys nowhere; only `focus.enter` gives a pane the keys, and it sends
    // the pane no press, so arriving never activates a row (WL-FOCUS-04).
    case Act::kFocusNext:
    case Act::kFocusPrevious:
    case Act::kFocusLeft:
    case Act::kFocusRight:
    case Act::kFocusUp:
    case Act::kFocusDown: {
        const Screen sc = screen_of(session_);
        const std::int64_t from = selected_pane(session_.panes);
        const std::vector<std::int64_t> order =
            selectable_panes(session_.panes, session_.setup.active, sc);
        if (order.empty()) {
            say("no open pane to select", true);
            break;
        }
        const bool cycling = a == Act::kFocusNext || a == Act::kFocusPrevious;
        const std::optional<std::int64_t> to =
            cycling ? cycle_pane(session_.panes, session_.setup.active, sc, from,
                                 a == Act::kFocusNext ? 1 : -1)
                    : spatial_neighbour(session_.panes, session_.setup.active, sc, from, a);
        if (!to.has_value()) {
            say(std::string("no pane ") + direction_words(a) + " " +
                    kind_name(session_.panes, from),
                false);
            break;
        }
        session_.panes.selected = *to;
        session_.panes.keyboard = kNoPaneKind;
        say("selected " + kind_name(session_.panes, *to) + " -- " + hotkey(Act::kFocusEnter) +
                " gives it the keys",
            false);
        break;
    }
    case Act::kFocusEnter: {
        std::int64_t to = selected_pane(session_.panes);
        if (to == kNoPaneKind || !kind_takes_keyboard(to)) {
            const std::optional<std::int64_t> first = cycle_pane(
                session_.panes, session_.setup.active, screen_of(session_), kNoPaneKind, 1);
            if (!first.has_value()) {
                say("no open pane to enter", true);
                break;
            }
            to = *first;
        }
        session_.panes.selected = to;
        session_.panes.keyboard = to;
        if (keyboard_pane() != to) {
            say(kind_name(session_.panes, to) + " takes the keys once it has its room", false);
            break;
        }
        say("typing goes to " + kind_name(session_.panes, to) + " -- " +
                hotkey(Act::kFocusLeave) + " gives Workshop the keys back",
            false);
        break;
    }
    // EDIT CODE BOUND TO A KEY NAMES NO PANE, so it acts on none. Command mode's subject is the
    // selected object or the room (`open_context_ambient`), never a pane, and guessing one --
    // the selected pane, the keyboard's -- would be the pointing/selecting collapse WL-CTX-01
    // refuses. The key still does something truthful: it says where the gesture lives
    // (agents/workshop/code.md).
    case Act::kEditCode:
        say("edit code follows a pane you point at -- right-press the pane, then choose edit "
            "code",
            true);
        break;
    default: break;
    }
}

} // namespace zengine::workshop
