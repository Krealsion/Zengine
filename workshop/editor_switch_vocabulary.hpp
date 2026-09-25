// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_EDITOR_SWITCH_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_EDITOR_SWITCH_VOCABULARY_HPP

// Switching the Editor as a maker asks for it (WL-SWITCH, docs/reference/editor-switch.md): the
// office `zengine.editor-switch` moves `zengine.editor` between the choices the load plan authors,
// carrying the document. Four asks, each answered with `EditorSwitchAnswered`; while a switch is
// under way, `EditorSwitchProgress` keeps it on the desk as a standing condition.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The office that switches the Editor: an asker names the service, not the weave providing it.
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

/// Every answer the office gives. `op` names the switch (0 when none began); `active` is the
/// choice holding the office when it was written; `choices` are the authored names.
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

/// What a switch is doing, and what it came to, published: while under way its stage, whom it
/// waits on and any consent to type; when it ends (`pending` false), the outcome in the answer's
/// words -- so a maker who asked from a Terminal still reads what came of it on the desk.
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
