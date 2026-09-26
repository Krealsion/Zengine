// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EDITOR_PANE_VOCABULARY_HPP
#define ZENGINE_EDITOR_PANE_VOCABULARY_HPP

// The Editor pane's durable names: the office it holds, the one pane it offers, the action ids
// its keys answer to, and the state a same-shape reload keeps. The office is `zengine.editor`,
// not the host's `zengine.workshop`, whose holder admission refuses as a pane's offerer
// (WL-CAT-03); `workshop/pane_seam_vocabulary.hpp` spells the same `kEditorRole` for askers. A
// desk saved as `zengine.workshop/editor` is converted at load (`workshop/pane_migration.hpp`).
// Workshop law: agents/workshop/editor.md

// The declared actions keep the spellings and keys makers' overrides name (`editor.save`
// ctrl+s, `editor.newline` Return, `editor.tab` Tab, `editor.discard` ctrl+d). The editing
// vocabulary -- copy, cut, paste, select, undo, word movement -- is the buffer's own
// (`kEditorVocabulary`), reached raw through `PaneKey`, and deliberately not remappable.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::editor_pane {

/// The office this pane holds: the durable half of its `PaneRef`, the address every source
/// request reaches it by, and the office the host asks before it quits. A role, so a reloaded
/// incarnation is still the party a saved desk names.
inline constexpr const char* kEditorPaneRole = "zengine.editor";

/// The pane key, in this office's namespace, unchanged by the conversion that moves a maker's
/// desk to this office.
inline constexpr const char* kEditorPane = "editor";

/// The two lines a maker reads about this pane: its name in the Pane Manager's list and its
/// summary in Info, also what Workshop's pane header says after the office.
inline constexpr const char* kEditorPaneName = "Editor";
inline constexpr const char* kEditorPaneSummary = "edit a source file";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kEditorPaneStem = "zengine-editor-pane";

// ---- THE ACTIONS THE PANE DECLARES (`PaneActions`, workshop/pane_vocabulary.hpp) ---------

inline constexpr const char* kActionSave = "editor.save";       ///< write the source to its file
inline constexpr const char* kActionNewline = "editor.newline"; ///< split the caret's line
inline constexpr const char* kActionTab = "editor.tab";         ///< insert one tab byte
inline constexpr const char* kActionDiscard = "editor.discard"; ///< back to the saved state
/// Carry a copy of the established selection, or of this file's location, to a receiving pane
/// by pick-and-place (the keyboard route beside the highlight's drag and the right-click menu).
inline constexpr const char* kActionExtract = "editor.extract";
inline constexpr const char* kActionLocation = "editor.location";

/// The state a same-shape reload keeps, and the pane's declared read surface (WL-EDIT-15): the
/// one source document, whole -- identity, current bytes, saved comparison, convention, epoch,
/// caret, anchor and viewport. Not the undo history, a paste or open in flight, or the wheel
/// fraction. The document crosses as one Text, since a list of lines would exceed Loom's decode
/// cell budget at the size the editor admits; why the mirror is eager and what it costs:
/// agents/decisions/the-editor-is-the-custodian.md.
struct EditorPaneState {
    std::string path;       ///< the normalized source identity; empty = no source open
    std::string text;       ///< the document as file bytes (`source_text`)
    std::string saved_text; ///< the saved comparison, the same way
    std::int64_t convention = 0;
    std::int64_t doc_epoch = 0;
    std::int64_t caret_row = 0;
    std::int64_t caret_byte = 0;
    std::int64_t anchor_row = 0;
    std::int64_t anchor_byte = 0;
    std::int64_t first_row = 0;
    std::int64_t first_col = 0;
    /// The room the document was last composed for (body rows, text columns): what `reconcile`
    /// compares to call a resize, carried so an unchanged room after a reload is not one.
    std::int64_t last_rows = 0;
    std::int64_t last_cols = 0;
    /// THE STANDING NOTICE, carried for the same reason and one better: it is a row of the
    /// pane's room, so a reload that dropped it changed the room the document had.
    std::string notice;
    bool notice_bad = false;
    /// WHERE THIS RUN BEGAN, as the project door said it, and WHETHER IT HAS SAID SO. The
    /// second field is the one a relative spelling turns on: an unanswered door and a door
    /// that authoritatively named no project are different facts (WL-EDIT-06).
    std::string project_dir;
    bool project_known = false;
    /// How many times this pane has materialized its document into `text`: the mirror's cost,
    /// counted. It rises when the bytes move and at no other time; not part of the document.
    std::int64_t text_builds = 0;
    /// The managed operation that installed the current document, or 0: carried so the
    /// document's claim (`EditorDocument`) reads the same after a reload. Bookkeeping, never
    /// authority.
    std::int64_t opened_by = 0;
    ZEN_SHAPE(EditorPaneState, 1, ZEN_FIELD(path), ZEN_FIELD(text), ZEN_FIELD(saved_text),
              ZEN_FIELD(convention), ZEN_FIELD(doc_epoch), ZEN_FIELD(caret_row),
              ZEN_FIELD(caret_byte), ZEN_FIELD(anchor_row), ZEN_FIELD(anchor_byte),
              ZEN_FIELD(first_row), ZEN_FIELD(first_col), ZEN_FIELD(last_rows),
              ZEN_FIELD(last_cols), ZEN_FIELD(notice), ZEN_FIELD(notice_bad),
              ZEN_FIELD(project_dir), ZEN_FIELD(project_known), ZEN_FIELD(text_builds),
              ZEN_FIELD(opened_by));
};

} // namespace zengine::editor_pane

#endif // ZENGINE_EDITOR_PANE_VOCABULARY_HPP
