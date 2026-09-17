// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_EDITOR_SWITCH_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_EDITOR_SWITCH_VOCABULARY_HPP

// SWITCHING THE EDITOR, AS A MAKER ASKS FOR IT (WL-SWITCH, agents/workshop/editor-switch.md).
//
// The office `zengine.editor-switch` switches `zengine.editor` between the choices the project's
// load plan authors for it (`load_plan.hpp`'s `ChoiceIntent`), carrying the document across. Four
// ordinary messages, each answered with `EditorSwitchAnswered` by Loom's own ask correlation:
//
//     EditorSwitchRequested       {destination}   switch to the choice with this name
//     EditorSwitchConfirmed       {op, consent}   agree to the losses a switch named, by digest
//     EditorSwitchCancelled       {op}            stop a switch that has not committed
//     EditorSwitchStatusRequested {}              which choice is active, which are authored
//
// FROM WORKSHOP'S TERMINAL, AS TYPED (the participant's own grammar -- a word that is not a
// number is Text, which is why a consent begins with a letter):
//
//     ask @zengine.editor-switch EditorSwitchRequested 1 destination=neovim
//     ask @zengine.editor-switch EditorSwitchConfirmed 1 op=3 consent=c4f2a91c
//     ask @zengine.editor-switch EditorSwitchStatusRequested 1
//
// THE OUTCOMES, one word each, in `EditorSwitchAnswered::outcome`:
//
//     switched             the office moved and the document crossed; `resets` and `notes` say
//                          what did not cross exactly
//     already-active       the destination already holds the office; nothing was loaded
//     needs-confirmation   the switch would lose `losses`; nothing was loaded; confirm with `op`
//                          and `consent`, or cancel
//     refused              nothing moved: `detail` is the owner's own words
//     cancelled            a pending switch was stopped; nothing moved
//     superseded           a newer request replaced this one while it awaited confirmation
//     failed-after-commit  the office moved and the successor did not prove itself; `detail`
//                          says what it said, and the retired editor was kept
//     status               the answer to a status request
//
// WHILE A SWITCH IS UNDER WAY the coordinator publishes `EditorSwitchProgress`, which Workshop
// keeps as a standing condition, so a maker who asked from the Terminal can see what it waits on.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// THE OFFICE THAT SWITCHES THE EDITOR. A ROLE, for `kOpeningRole`'s reason: an asker names the
/// service, not the weave that happens to provide it.
inline constexpr const char* kEditorSwitchRole = "zengine.editor-switch";

namespace switch_outcome {
inline constexpr const char* kSwitched = "switched";
inline constexpr const char* kAlreadyActive = "already-active";
inline constexpr const char* kNeedsConfirmation = "needs-confirmation";
inline constexpr const char* kRefused = "refused";
inline constexpr const char* kCancelled = "cancelled";
inline constexpr const char* kSuperseded = "superseded";
inline constexpr const char* kFailedAfterCommit = "failed-after-commit";
inline constexpr const char* kStatus = "status";
} // namespace switch_outcome

struct EditorSwitchRequested {
    std::string destination; ///< an authored choice's name
    ZEN_SHAPE(EditorSwitchRequested, 1, ZEN_FIELD(destination));
};

struct EditorSwitchConfirmed {
    std::int64_t op = 0;
    std::string consent; ///< the digest a `needs-confirmation` answer carried
    ZEN_SHAPE(EditorSwitchConfirmed, 1, ZEN_FIELD(op), ZEN_FIELD(consent));
};

struct EditorSwitchCancelled {
    std::int64_t op = 0;
    ZEN_SHAPE(EditorSwitchCancelled, 1, ZEN_FIELD(op));
};

struct EditorSwitchStatusRequested {
    ZEN_SHAPE(EditorSwitchStatusRequested, 1);
};

/// EVERY ANSWER THE OFFICE GIVES. `op` names the switch (0 when none began); `active` is the
/// choice holding the office when the answer was written; `choices` are the authored names.
struct EditorSwitchAnswered {
    std::int64_t op = 0;
    std::string outcome;
    std::string active;
    std::string destination;
    std::string consent;
    std::string detail;
    std::vector<std::string> losses;
    std::vector<std::string> resets;
    std::vector<std::string> notes;
    std::vector<std::string> choices;
    ZEN_SHAPE(EditorSwitchAnswered, 1, ZEN_FIELD(op), ZEN_FIELD(outcome), ZEN_FIELD(active),
              ZEN_FIELD(destination), ZEN_FIELD(consent), ZEN_FIELD(detail), ZEN_FIELD(losses),
              ZEN_FIELD(resets), ZEN_FIELD(notes), ZEN_FIELD(choices));
};

/// WHAT A SWITCH IS DOING, AND WHAT IT CAME TO, published: while it is under way, its stage in a
/// word (`judging`, `awaiting-confirmation`, `warming`, `boundary`, `adopting`, `proving`,
/// `retiring`), whom it waits on, and -- awaiting confirmation -- the consent a maker types; when it
/// ends (`pending` false), the outcome and its words, exactly as the answer says them. Every answer
/// the office gives is published this way, so a maker who asked from a Terminal that shows no
/// answer's fields still reads what came of it on the desk.
struct EditorSwitchProgress {
    std::int64_t op = 0;
    std::string destination;
    std::string stage;
    std::string awaiting;
    bool pending = false;
    std::string consent;
    std::string outcome;
    std::string detail;
    ZEN_SHAPE(EditorSwitchProgress, 1, ZEN_FIELD(op), ZEN_FIELD(destination), ZEN_FIELD(stage),
              ZEN_FIELD(awaiting), ZEN_FIELD(pending), ZEN_FIELD(consent), ZEN_FIELD(outcome),
              ZEN_FIELD(detail));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_EDITOR_SWITCH_VOCABULARY_HPP
