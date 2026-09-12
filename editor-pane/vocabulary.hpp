// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EDITOR_PANE_VOCABULARY_HPP
#define ZENGINE_EDITOR_PANE_VOCABULARY_HPP

// The Editor pane's DURABLE NAMES -- the office it holds, the one pane it offers, the action
// ids its keys answer to, and the state a same-shape reload keeps.
//
// WHY THE OFFICE IS `zengine.editor` AND NOT `zengine.workshop`. The Editor was a built-in
// panel of the host (`panel::kEditor`, `zengine.workshop/editor` in every saved desk), so the
// host's office would have looked like the honest address -- and it is exactly the address a
// migrated pane may not have: `zengine.workshop` is the host's singleton role and admission
// refuses a pane offered by whoever holds it as a forgery (WL-CAT-03). Files, the Builder,
// Info and the Terminal each wrote this paragraph before; this is the fifth pane it is true
// of, and the last of the arc.
//
// THE OFFICE IS ALSO THE DOOR. `OpenSourceRequested` was addressed to `zengine.workshop` while
// the host held the document, with a note that the sentence would one day go to
// `zengine.editor` and only the address would move. It has: `workshop/pane_seam_vocabulary.hpp`
// spells `kEditorRole` for the two askers, and a case checks that spelling against this one --
// the seam where a divergence would actually be caught.
//
// WHY THERE IS A RETIRED `PaneRef` TO CONVERT. Unlike the Terminal, the Editor WAS a panel:
// a catalog kind, a picker row, a reference a saved setup names. Every desk a maker saved with
// an Editor on it spells `zengine.workshop/editor`, and `workshop/pane_migration.hpp` converts
// that to `zengine.editor/editor` at load -- the office moves, the pane key does not, and the
// place stays the overlay stack the host's catalog always gave it.
//
// WHY THE FOUR ACTION IDS ARE THE BUILT-IN'S OWN SPELLINGS. `editor.save`, `editor.newline`,
// `editor.tab` and `editor.discard` were `KeyContext::kEditor`'s rows in `workshop/keymap.hpp`,
// on ctrl+s, Return, Tab and ctrl+d. They are this pane's `PaneActionRow`s now, spelled and
// defaulted exactly as they were, so a maker who authored an override for one keeps it. The
// editing vocabulary itself -- copy, cut, paste, select, undo, word movement -- is NOT declared:
// it is the buffer's own mechanics (`kEditorVocabulary`, editor.hpp), reached raw through
// `PaneKey` and deliberately not remappable, exactly as it was inside the host.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::editor_pane {

/// THE OFFICE THIS PANE HOLDS -- the durable half of its `PaneRef`, the address every source
/// request reaches it by, and the office the host asks before it quits. A ROLE, so a reloaded
/// incarnation is still the party a saved desk names and a Files row asks.
inline constexpr const char* kEditorPaneRole = "zengine.editor";

/// THE PANE KEY, in this office's namespace -- the built-in's own key, unchanged, so the one
/// conversion moves a maker's desk without touching where on it the pane sits.
inline constexpr const char* kEditorPane = "editor";

/// THE TWO LINES A MAKER READS IN THE PICKER, and what Workshop's pane header says after the
/// office. The built-in's own two, carried.
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

/// THE STATE A SAME-SHAPE RELOAD KEEPS (RELOAD-1): the one source document, whole.
///
/// ⚠ THE DOCUMENT CROSSES AS ONE TEXT AND NOT AS A LIST OF LINES, and the reason is a bound
/// this shape is written against rather than a preference. Loom decodes a reload snapshot
/// under `kMaxDecodedCells` (65,536 cells for the whole value; one per list element, one per
/// declared field) while a single Text field may hold `kMaxFieldBytes` (256 MiB). The editor
/// admits files up to `kMaxSourceBytes` (4 MiB), which is easily a hundred thousand lines: a
/// `List<Text>` of them would exceed the cell budget, the revive would be refused, and Loom's
/// named edge for a refused revive is a weave left UNAVAILABLE -- the reload that was meant to
/// keep the document would have lost it. Two Text fields cost two cells whatever the file
/// holds. `text` is exactly the bytes a save would write (`source_text`), so the convention
/// round-trips inside it and the new image admits it through the same `source_in` a file
/// meets.
///
/// WHAT IT KEEPS: the identity, the current bytes, the saved comparison, the line convention,
/// the document epoch (so a paste asked of the old image cannot land in the new one), the
/// caret, the anchor and the viewport's two offsets.
///
/// WHAT IT REFUSES TO KEEP, said rather than left to be discovered: the UNDO HISTORY (a run of
/// bounded snapshots that can be eight megabytes wide; the buffer that held it is the owner
/// that does not survive a reload, and the new one opens its history at the document it was
/// given), a PASTE IN FLIGHT and an OPEN IN FLIGHT (both were the old incarnation's asks; their
/// answers arrive to a pane that is no longer waiting), and the WHEEL FRACTION (a fraction of a
/// line, spent at the next notch).
///
/// ⚠ AND IT IS ALSO THE PANE'S DECLARED READ SURFACE, WHICH IS WHY IT IS WRITTEN EAGERLY.
/// Loom answers `zen.PokeRead` from `state_` itself, before any handler of this weave runs
/// (`WeaveBase::try_poke`, and `handle` is `final` so there is no hook to refresh it on the
/// way past). A shape kept only for reloads would therefore ADVERTISE every field below and
/// answer each of them with whatever the last revival left there -- empty on a pane that never
/// reloaded. So this pane mirrors its live document into `state_` at each composition instead,
/// and `snapshot()` is Loom's own: one truth, read two ways.
///
/// WHAT THAT COSTS, COUNTED RATHER THAN CLAIMED (VD-27). `text` is rebuilt when the BYTES move
/// -- `EditorBuffer::content_revision`, which the buffer bumps at `set_lines`, at every mutation
/// and at the two history doors -- and `saved_text` when the saved comparison is written. So
/// navigating, pointing, sweeping, scrolling, resizing and focusing rebuild NEITHER, and
/// `text_builds` below is the pane's own count of the times it did, readable by anybody who
/// wants to check that sentence instead of believing it.
///
/// ⚠ THE FIRST WRITING OF THIS KEYED ON `EditorBuffer::revision`, AND THAT WAS THE WRONG
/// QUESTION: that revision moves when the CARET moves, because a pending paste must notice that
/// its position went stale (WL-EDIT-11). A four-megabyte mirror keyed on it was replaced whole
/// by an arrow key, a press and every motion of a drag, with every byte identical -- measured,
/// and reported at the time as costing nothing. Two questions, two counters.
///
/// WHAT AN EDIT STILL PAYS, MEASURED AT THE BOUND (VM-PROBE-12): one `source_text` of the WHOLE
/// document -- a join of every line into one text -- on EVERY typed byte, because
/// `content_revision` moves on every mutation. The buffer's own snapshot is not of the same
/// order, and an earlier writing of this paragraph said it was: `remember` groups a run of
/// typing into one undo entry, so the history copies the document once per group where the
/// mirror joins it once per byte. At the bound the join is the larger part of a keystroke's
/// cost, and a million short lines cost far more than one long one. That is the standing price
/// of a read surface that cannot lie. A cheaper mirror needs a substrate hook that lets a weave
/// refresh its state before a poke is answered; there is none to call, and asking for one is a
/// decision of its own rather than a premise.
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
    /// THE ROOM THE DOCUMENT WAS LAST COMPOSED FOR -- body rows and text columns, the two
    /// numbers `reconcile` calls a resize by comparing. Carried, because a reload that forgot
    /// them made the first grant of an UNCHANGED room look like a resize and pulled the
    /// viewport back to the caret (VD-26).
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
    /// HOW MANY TIMES THIS PANE HAS MATERIALIZED ITS DOCUMENT into `text` -- the mirror's own
    /// cost, counted rather than claimed (VD-27). It rises when the BYTES move and at no other
    /// time, so a maker, a probe or a case can ask what a gesture actually cost instead of
    /// taking a comment's word for it. An `Int` that only grows; it is not part of the
    /// document and a reload carries it for continuity of the count alone.
    std::int64_t text_builds = 0;
    /// EXPERIMENTAL (editor-managed-open-slice): THE MANAGED OPERATION THAT INSTALLED THE
    /// CURRENT DOCUMENT, or 0 for a direct open or none. Carried so the document's latest
    /// claim (`EditorDocument`, workshop/open_seam_vocabulary.hpp) reads the same after a
    /// reload as before it; it is bookkeeping, never authority.
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
