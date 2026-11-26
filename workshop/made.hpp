// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_MADE_HPP
#define ZENGINE_WORKSHOP_MADE_HPP

// What Workshop's weave is made with: the words and paths its host fixed at the launch, handed as
// the one value the weave is made with (Loom's creation value, `ZEN_MADE_WITH`) and never as a
// pointer into the host, so a loaded image is handed the same by its load. Data and never a door:
// whatever else Workshop needs it asks or reads by message, as every participant does.
// Workshop law: agents/workshop/session.md

#include "workshop/attention.hpp"
#include "workshop/setup.hpp" // `PaneRef`

#include <zen/weave/shape.hpp>

#include <string>
#include <vector>

namespace zengine::workshop {

/// ONE CONDITION THE HOST KNEW AT THE LAUNCH, as Workshop is made with it: a `Condition`'s fields.
// WL-ATTN-01 -- attention/docs/attention-laws.md
struct LaunchCondition {
    std::string key;
    std::string compact;
    std::string detail;
    graphics::Color ink = graphics::colors::accent;
    std::string action;
    ZEN_SHAPE(LaunchCondition, 1, ZEN_FIELD(key), ZEN_FIELD(compact), ZEN_FIELD(detail),
              ZEN_FIELD(ink), ZEN_FIELD(action));
};

/// WHAT WORKSHOP IS MADE WITH, read once at its making.
// WL-SESSION-01 -- agents/workshop/session.md
struct WorkshopMade {
    /// The weaver's own files: the KEYMAP read and written, the presentation PREFERENCES read
    /// and written, the LAST SESSION a window's room is kept beside. Empty is none, which the
    /// desk says.
    // WL-KEY-07 -- agents/workshop/keyboard.md; WL-SESSION-01 -- agents/workshop/session.md
    std::string keymap_path;
    std::string prefs_path;
    std::string session_path;
    /// WHERE THE THEMES ARE, as the theme role is told: the weaver's own folder, empty where this
    /// launch keeps none, and the folder of the themes shipped beside this host.
    std::string themes_path;
    std::string shipped_themes_path;
    /// WHETHER THIS HOST'S TUI WINDOW READS ITS KEYS FROM A POSIX TERMINAL
    /// (`input/terminal_reader.hpp`): shown in that window, Workshop puts each pane row's POSIX key
    /// in force in place of its default. A host sets it on a build that is not Windows.
    // WL-KEY-08 -- agents/workshop/keyboard.md
    bool posix_terminal = false;
    /// The one pane whose presentation the host claims and commits jointly with its document, by
    /// its provider's role and its pane; an empty provider means none is managed.
    std::string managed_provider;
    std::string managed_pane;
    /// An object document this run was pointed at or found and will not open, or empty: said once
    /// at startup and left exactly as it is.
    // WL-DOC-22 -- agents/workshop/document-file.md
    std::string retired_document;
    /// ONE HUMAN-READABLE SENTENCE ABOUT THE LEGACY-FILE TRANSITION, or empty.
    // WL-SESSION-03 -- agents/workshop/session.md
    std::string transition_note;
    /// WHAT THE START SAID OF A REMEMBERED CHOICE THE PLAN NO LONGER AUTHORS, said once. Which
    /// choice holds each switchable role is the realization owner's claim (`ChoicesHeld`).
    // WL-SWITCH-08 -- agents/workshop/editor-switch.md
    std::string choice_note;
    /// What the host already knew was true at the launch, and is for the whole run. What the plan
    /// could not load the realization owner claims (`RowsNotLoaded`), read at each repaint.
    // WL-ATTN-01 -- attention/docs/attention-laws.md
    std::vector<LaunchCondition> conditions;
    ZEN_SHAPE(WorkshopMade, 1, ZEN_FIELD(keymap_path), ZEN_FIELD(prefs_path),
              ZEN_FIELD(session_path), ZEN_FIELD(themes_path), ZEN_FIELD(shipped_themes_path),
              ZEN_FIELD(posix_terminal), ZEN_FIELD(managed_provider), ZEN_FIELD(managed_pane),
              ZEN_FIELD(retired_document), ZEN_FIELD(transition_note), ZEN_FIELD(choice_note),
              ZEN_FIELD(conditions));

    /// The managed pane, as the setup names a pane.
    PaneRef managed() const { return PaneRef{managed_provider, managed_pane}; }
    /// Name `ref` the managed pane.
    void manage(const PaneRef& ref) {
        managed_provider = ref.provider;
        managed_pane = ref.pane;
    }
    /// Add a condition the host knows now.
    void know(const Condition& c) {
        conditions.push_back(LaunchCondition{c.key, c.compact, c.detail, c.ink, c.action});
    }
};

/// A condition Workshop was made with, as the desk keeps it.
inline Condition condition_of(const LaunchCondition& c) {
    return Condition{c.key, c.compact, c.detail, c.ink, c.action};
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_MADE_HPP
