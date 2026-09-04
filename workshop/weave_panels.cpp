// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- the dynamic panels -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/panes-and-windows.md (+2 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- The dynamic panels --------------------------------------------------

// WL-PANE-12 -- agents/workshop/panes-and-windows.md
// WL-PANE-12 -- agents/workshop/panes-and-windows.md
std::vector<CatalogRow> WorkshopWeave::picker_population() const {
    return inventory_rows(session_.setup.active, session_.panels);
}

void WorkshopWeave::open_picker() {
    session_.panels.picker.open = true;
    session_.panels.picker.cursor = 0;
    say("+ panel -- " + hotkey(Act::kPickerUp) + "/" + hotkey(Act::kPickerDown) +
            " chooses, " + hotkey(Act::kPickerChoose) + " opens or removes, " +
            hotkey(Act::kPickerClose) + " or " + hotkey(Act::kPicker) + " cancels",
        false);
}

// WL-PANE-12 -- agents/workshop/panes-and-windows.md
void WorkshopWeave::picker_move(std::int64_t by) {
    PanelPicker& picker = session_.panels.picker;
    const std::size_t total = picker_population().size();
    if (total == 0) {
        picker.cursor = 0;
        return;
    }
    if (picker.cursor >= total) {
        picker.cursor = total - 1;
    }
    if (by < 0) {
        const std::size_t up = static_cast<std::size_t>(-by);
        picker.cursor = picker.cursor > up ? picker.cursor - up : 0;
    } else {
        const std::size_t down = static_cast<std::size_t>(by);
        picker.cursor = picker.cursor + down < total ? picker.cursor + down : total - 1;
    }
}

// WL-EDIT-10 -- agents/workshop/editor.md
void WorkshopWeave::picker_wheel(const zengine::input::PointerWheel& w, loom::Mail& mail) {
    PanelPicker& picker = session_.panels.picker;
    const std::int64_t rows = spend_wheel(picker.wheel_accum, w.dy, kListWheelRows);
    if (rows == 0) {
        return;
    }
    const std::size_t was = picker.cursor;
    picker_move(-rows);
    if (picker.cursor == was) {
        return; // already at the edge: nothing moved, nothing repaints
    }
    repaint(mail);
}

void WorkshopWeave::picker_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    PanelPicker& picker = session_.panels.picker;
    // THE CURSOR IS REPAIRED THROUGH THE POPULATION'S OWN OWNER, BEFORE ANYTHING INDEXES
    // IT. The list can shrink under an open picker -- a provider going away
    // takes its runtime rows with it -- and every question below is about a row.
    const std::size_t population = picker_population().size();
    if (picker.cursor >= population) {
        picker.cursor = population == 0 ? 0 : population - 1;
    }
    switch (session_.keymap.action_for(KeyContext::kPicker, k.scancode, k.modifiers)) {
    case Act::kPickerUp: picker_move(-1); break;
    case Act::kPickerDown: picker_move(+1); break;
    case Act::kPickerChoose: choose_panel(mail); break;
    case Act::kPickerClose:
        picker.open = false;
        say("no panel opened or removed", false);
        break;
    default:
        // THE KEY THAT OPENED IT CLOSES IT -- the terminal overlay's rule, and since
        // the keymap it follows the OPENER'S effective binding wherever the maker moved
        // it: dispatch consults the same truth the row-0 hint spells, so `p` closes
        // exactly while `p` opens.
        if (session_.keymap.matches(Act::kPicker, k.scancode, k.modifiers)) {
            picker.open = false;
            say("no panel opened or removed", false);
        }
        break;
    }
}

// WL-PED-05 -- agents/workshop/pane-manager.md
// WL-PANE-12 -- agents/workshop/panes-and-windows.md
void WorkshopWeave::choose_panel(loom::Mail& mail) {
    PanelPicker& picker = session_.panels.picker;
    const std::vector<CatalogRow> rows = picker_population();
    picker.open = false;
    if (picker.cursor >= rows.size()) {
        // THE BELT, NOT THE DOOR. The cursor is bounded where it moves -- through the
        // same owner this list came from -- and a population that shrank under an open
        // picker (a provider going away) is the one way it can be past the end.
        return;
    }
    toggle_participation(rows[picker.cursor], hotkey(Act::kPicker), mail);
}

// WL-PED-05 -- agents/workshop/pane-manager.md
// WL-PANE-12 -- agents/workshop/panes-and-windows.md
void WorkshopWeave::toggle_participation(const CatalogRow& chosen, const std::string& again,
                                         loom::Mail& mail) {
    const PaneRef ref = chosen.ref;
    if (remove_pane(session_.setup.active, ref)) {
        // A REMOVAL WORKS ON A WAITING ROW EXACTLY AS ON AN OPEN ONE. The maker
        // authored the intent; whether this screen currently has room to seat it is
        // Workshop's problem and not a reason to make the intent unremovable.
        apply_setup(mail);
        // WHAT IT WAS PRESENTING IS UNTOUCHED, and one sentence covers both
        // kinds because it is the same sentence: the Builder tool keeps its
        // target, its history and its running count of asks; the document
        // keeps every object, the selection and the inspector's rows. A
        // panel is a presentation, and removing one removes a presentation.
        say("removed " + chosen.name + " -- " + again +
                " brings it back; nothing behind it was touched",
            false);
        return;
    }
    // A NEW ROW IS REFUSED BEFORE THE SETUP MOVES IF IT COULD NOT BE SEATED.
    // The order is the whole of the guarantee: the capacity question is asked against
    // the setup this gesture WOULD produce, and the active setup is left untouched when
    // the answer is no. Adding first and letting `reconcile` drop it into `waiting`
    // would leave a maker with an authored pane they never saw and did not knowingly
    // author, which is a picker that edits a file behind its own refusal.
    Setup candidate = session_.setup.active;
    (void)add_pane(candidate, ref);
    const Seating trial = seat_panes(candidate, session_.panels,
                                     stack_capacity(screen_of(session_)));
    for (const std::int64_t k : trial.waiting) {
        if (k == chosen.kind) {
            say("no room for " + chosen.name +
                    " on this screen -- make the window taller, then p again",
                true);
            return;
        }
    }
    session_.setup.active = std::move(candidate);
    // AND THE PANEL ASKS THE TOOL WHAT IT IS -- inside `apply_setup`, for
    // every kind it newly opened. A presentation that was handed its
    // subject's facts by whoever built it would be showing the builder's
    // opinion; this one shows the tool's answer, and shows nothing until it
    // has one.
    //
    // INFO ASKS NOBODY, and the absence of a second branch there is the
    // phase's structural claim, unchanged: opening it sends no message,
    // touches no role and needs no weave mounted anywhere. A Workshop
    // hosting no tools at all opens Info and it works.
    apply_setup(mail);
    say(std::string("opened ") + chosen.name + " -- " + again + " removes it",
        false);
}

} // namespace zengine::workshop
