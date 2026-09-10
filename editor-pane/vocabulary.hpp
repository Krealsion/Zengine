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
/// given), a PASTE IN FLIGHT (the ask was the old incarnation's; its answer arrives to a pane
/// that is no longer waiting), the WHEEL FRACTION and the FOLLOW FLAG (presentation, spent at
/// the next repaint anyway). Hiding, covering, moving, reordering or removing the PANE keeps
/// all of those, because none of them touches this weave.
///
/// `snapshot()` is overridden to build this from the live buffer at the moment Loom asks,
/// so no keystroke pays for a copy of the document it did not need.
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
    ZEN_SHAPE(EditorPaneState, 1, ZEN_FIELD(path), ZEN_FIELD(text), ZEN_FIELD(saved_text),
              ZEN_FIELD(convention), ZEN_FIELD(doc_epoch), ZEN_FIELD(caret_row),
              ZEN_FIELD(caret_byte), ZEN_FIELD(anchor_row), ZEN_FIELD(anchor_byte),
              ZEN_FIELD(first_row), ZEN_FIELD(first_col));
};

} // namespace zengine::editor_pane

#endif // ZENGINE_EDITOR_PANE_VOCABULARY_HPP
