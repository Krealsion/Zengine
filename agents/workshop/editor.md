# Workshop law — the Editor

Register `WL-EDIT`: one source document, held by the Editor pane weave, presented through the
pane protocol. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).
A source path's origin is [`project.md`](project.md); the caret is
[`pane-caret.md`](pane-caret.md); the sweep this host records is [`text-box.md`](text-box.md).

## WL-EDIT-01 — One document, weave-owned, presented by a pane

LAW — The Editor is a loaded weave (`zengine.editor`) holding the one open document — path, bytes, saved copy, convention, epoch, caret, selection, history, viewport — and Workshop's part is a pane.

MEANS
- `EditorState` and `EditorBuffer` moved with it whole; the host holds no path and no bytes;
- the seam carries values only: no `Session&`, no `HostContext`, no pointer of any kind.

PROVEN BY — `editor-pane/editor.hpp` `EditorState`, `EditorBuffer`, `EditorState::doc_epoch`,
`EditorBuffer::revision`, `kEditorUndoDepth`, `kEditorUndoBudgetBytes`; `editor-pane/pane.cpp`
`EditorPaneWeave`, `e_`; `editor-pane/vocabulary.hpp` `kEditorPaneRole`, `kEditorPane`;
`workshop/default-load-plan.json`; `tests/test_workshop_panes_editor.cpp` case `"EDIT-W1: the
Editor is an ordinary arranged pane, offered by an office"`, case `"EDIT-W48: the editor this
host used to compile is named by no presentation source"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-02 — The first multiline consumer owns its multiline machinery

LAW — The single-line component is untouched; the buffer's gestures are declared in its own vocabulary and swept against its consume both ways, and a future backend replaces the buffer as one unit.

DOES NOT MEAN
- that a replacement may touch path custody, save authority or the pane presentation.

PROVEN BY — `editor-pane/editor.hpp` `EditorBuffer`, `kEditorVocabulary`,
`EditorBuffer::consume`; `component/text_box.hpp` `TextBox`; `editor-pane/pane.cpp`
`on(PaneKey)`; `tests/test_editor.cpp` case `"EDIT-0: the editor's declared vocabulary and
consume agree, both directions"`, case `"EDIT-0: undo groups typing, treats joins and pastes as
one edit, and redo returns"`, case `"EDIT-0: set_lines wipes the history -- undo cannot
resurrect another document"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-03 — Custody is the weave's, and that is the no-silent-loss floor

LAW — Hide, move, cover, reorder or remove the pane and no document is touched; a dirty buffer refuses another source and, asked, an orderly quit; `editor.discard` (`ctrl+d`) is the one discard door.

MEANS
- discard is undoable through `revert_to`, which keeps the history;
- process death still loses drafts: no crash recovery is claimed.

PROVEN BY — `editor-pane/pane.cpp` `open_source`, `discard_source_edits`,
`on(PaneQuitRequested)`; `editor-pane/editor.hpp` `EditorBuffer::revert_to`, `EditorState`;
`workshop/weave_run.cpp` `quit`, `on(PaneQuitAnswered)`; `tests/test_workshop_panes_editor.cpp`
case `"EDIT-W17: removing and reopening the pane cannot lose a byte, a caret, or a step of
history"`, case `"EDIT-W12: a dirty buffer refuses a different source, and a save opens the
way"`, case `"EDIT-W20: an orderly quit refuses while source is unsaved, and proceeds once it
is not"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-04 — `^s` follows the keyboard, as two declared identities

LAW — `^s` is two rows: `document.save` where no text has the keys (`kNoText`), and the pane's own `editor.save` while it holds them; `^o` stays global, `^c` is copy in the Editor and quit elsewhere.

MEANS
- the two never meet: a `kNoText` row is not active in a pane, so the collision law admits it;
- ⚠ a line that takes text no longer answers `^s` with the document's save (`kNoEditor` did).

PROVEN BY — `workshop/keymap.hpp` `document.save`, `KeyContext::kNoText`,
`context_takes_text`; `editor-pane/vocabulary.hpp` `kActionSave`; `editor-pane/pane.cpp`
`declare`; `tests/test_workshop_panes_editor.cpp` case `"EDIT-W3: one physical ^s is the
document's save or the source's, by who holds the keys"`, case `"EDIT-W27: ^c copies, does not
quit, and the copy reaches the platform clipboard"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-EDIT-05 — The one door is `OpenSourceRequested` at `zengine.editor`, and it takes a path

LAW — Normalize, same-path reveal, dirty refusal, bounded read, `source_in`, install with `doc_epoch++` and a viewport reset, answer the asker, then ask to be shown; every referrer arrives through it.

MEANS
- Files hands it a row's path; the Builder, the path the project door named (`RecipeSourceSaid`);
- an office is required, the answer is the asker's alone, and the pane holds no provenance.

PROVEN BY — `editor-pane/pane.cpp` `on(OpenSourceRequested)`, `open_source`, `install`;
`editor-pane/editor.hpp` `source_in`, `kMaxSourceBytes`, `EditorState`;
`workshop/pane_seam_vocabulary.hpp` `OpenSourceRequested`, `SourceOpened`, `kEditorRole`;
`workshop/builder_seam_vocabulary.hpp` `RecipeSourceRequested`, `RecipeSourceSaid`;
`workshop/pane_doors.hpp` `ProjectDoor`; `tests/test_workshop_panes_editor.cpp` case
`"EDIT-W5: opening a source installs it, answers the asker, and asks to be shown"`, case
`"EDIT-W7: a missing file, an oversized one and refused bytes cost the asker the refusal and
nothing else"`; `tests/test_workshop_panes_builder.cpp` case `"BLD-WEAVE: `e` opens the chosen
recipe's source, resolved by the host"`.
WHY — `agents/decisions/one-door-takes-a-path.md`

## WL-EDIT-06 — Identity is a normalized spelling, not a filesystem object

LAW — Every entrant is made absolute against the project root the pane asked for, `lexically_normal` and forward-slashed, so `a.cpp` and `./a.cpp` are one document; nothing canonicalizes.

DOES NOT MEAN
- that case-folding and hard links are handled — they remain named residuals.

PROVEN BY — `editor-pane/pane.cpp` `open_source`, `project_dir_`; `workshop/persist.hpp`
`resolved_against`; `editor-pane/editor.hpp` `EditorState::path`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W6: a relative path is the PROJECT's file,
resolved through the project door"`, case `"EDIT-W11: re-requesting the open source reveals it
and destroys nothing"`.
WHY — `agents/decisions/one-door-takes-a-path.md`

## WL-EDIT-07 — The source-byte law is the media's honest reach

LAW — Printable ASCII plus tab, one line ending per document (LF or CRLF), a final newline as a final empty line; mixed endings, bare CR, control bytes and non-ASCII refuse whole, naming the line.

MEANS
- the convention is detected at open and spent on every inserted newline;
- `source_in`/`source_text` are exact inverses, and the file is never rewritten;
- typed and pasted text meet the same law at the weave's doors, and are refused in a row.

PROVEN BY — `editor-pane/editor.hpp` `source_in`, `source_text`, `pasteable_source`,
`line_ending`, `source_byte_ok`, `PasteableSource`; `editor-pane/pane.cpp` `on(PaneTextInput)`;
`tests/test_editor.cpp` case `"EDIT-0: source_in and source_text are inverse over everything
admitted"`; `tests/test_workshop_panes_editor.cpp` case `"EDIT-W34: a clipboard holding
non-ASCII refuses the paste, and typed non-ASCII is refused with a sentence"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-08 — Tabs expand at presentation only

LAW — Tabs expand only at presentation, at a four-column stop; one tab-geometry measurer (bytes to displayed columns and back, and the displayed slice) is what the rows, the press and the drag spend.

MEANS
- `kCaretCols` reserves the caret's column of every document row, the Terminal's own rule.

PROVEN BY — `editor-pane/editor.hpp` `EditorState::first_col`, `visual_col_of`,
`byte_of_visual_col`, `expanded_slice`, `kEditorTabStop`; `editor-pane/pane.cpp` `kCaretCols`,
`on(PanePressed)`; `tests/test_editor.cpp` case `"EDIT-0: tab geometry maps bytes and displayed
columns both ways, exactly"`; `tests/test_workshop_panes_editor.cpp` case `"EDIT-W35: a press
places the caret through the same tab geometry the paint used, and the caret is published
beside the rows"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-09 — The viewport reconciles once per composition

LAW — `reconcile` clamps the offsets always, follows the caret when a gesture asked (`follow_caret`) or the body's room changed, and deliberately not after the wheel; it runs once, inside `say`.

PROVEN BY — `editor-pane/pane.cpp` `reconcile`, `say`; `editor-pane/editor.hpp`
`EditorState::follow_caret`; `tests/test_workshop_panes_editor.cpp` case `"EDIT-W40: keyboard
navigation scrolls the window and the caret never leaves it"`, case `"EDIT-W41: a horizontal
window follows the caret and recovers the room an erase frees"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-10 — `on(PointerWheel)` is Workshop's one wheel router

LAW — `on(PointerWheel)` routes every wheel: modes keep their ownership, the topmost occupancy decides (picker, then front order), a header row is not the body, and notches accumulate in `spend_wheel`.

MEANS
- both Pane Manager lists and the picker move their cursor by `kListWheelRows`;
- an external pane's body: the notches cross as `PaneWheel`; the Editor scrolls, caret still.

DOES NOT MEAN
- that there is a scroll framework, a scrollbar, a global offset map or a persisted position.

PROVEN BY — `workshop/weave_pane_editor.cpp` `pane_editor_wheel`; `workshop/weave_panels.cpp`
`picker_wheel`; `workshop/weave_pointer.cpp` `on(PointerWheel)`; `workshop/screen.hpp`
`kListWheelRows`; `workshop/screen_gestures.cpp` `list_window`; `workshop/screen_reveal.cpp`
`spend_wheel`; `editor-pane/pane.cpp` `on(PaneWheel)`; `tests/test_workshop_panes_editor.cpp`
case `"EDIT-W39: the wheel scrolls the body, moves no caret, and elsewhere reaches nothing"`;
`tests/test_workshop_panes_seam.cpp` case `"QR-18/SC-5: the picker's windowed inventory is
reached by the wheel"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-11 — A paste answer lands where the maker asked or nowhere

LAW — A pending paste pins the document epoch and the buffer revision it was asked for, so a replaced document strands the payload silently and a document that merely moved is told to paste again.

PROVEN BY — `editor-pane/pane.cpp` `begin_paste`, `on(ClipboardText)`, `Paste`;
`editor-pane/editor.hpp` `EditorState::doc_epoch`, `EditorBuffer::revision`,
`EditorBuffer::set_lines`, `EditorBuffer::paste_lines`, `EditorBuffer`, `EditorState`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W32: a late paste answer may not land at a
caret that has since moved"`, case `"EDIT-W33: a late answer for a replaced document is discarded
whole"`.
WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-EDIT-12 — The pane composes its rows, and the caret beside them

LAW — The pane composes a status row (dirty word, `L:C/N`, the path), a notice row where the room holds one, then the document through the viewport; the caret and the clipped selection are `PaneCaret`.

MEANS
- a room too small for both keeps the document's row: the notice stands in for the status row;
- one row is the status row alone, and the caret has nowhere to be.

PROVEN BY — `editor-pane/pane.cpp` `say`, `say_caret`, `status_text`, `kNoticeNeedsRows`;
`workshop/screen_external.cpp` `external_header`, `external_body_place`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W38: a selection that runs above the window
is clipped, and one wholly out of it is not said"`, case `"EDIT-W43: in a room too small for
both, the document keeps its rows and a notice stands in for the status row"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-13 — The pane asks to be shown, and no room refuses the reveal, not the document

LAW — After answering an open the pane sends `PaneRevealRequested`; the host seats it by the picker's trial seat, selects it and points the keys, or refuses in the picker's words and the document stays open.

MEANS
- a reveal from an office that offered no such pane, or from nobody, is dropped;
- the host says which pane asked, so the notice line is about what just happened.

PROVEN BY — `workshop/weave_seam.cpp` `on(PaneRevealRequested)`; `workshop/pane_vocabulary.hpp`
`PaneRevealRequested`; `workshop/screen.hpp` `stack_slots_that_fit`; `editor-pane/pane.cpp`
`on(OpenSourceRequested)`; `tests/test_workshop_panes_editor.cpp` case `"EDIT-W9: with no room to
seat it, the document is open and the reveal is refused in the picker's words"`, case `"EDIT-W10:
a reveal from an office that offered no such pane is dropped"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-14 — The exit is asked of the room, and decided by the answer

LAW — `quit` publishes `PaneQuitRequested` and counts Loom's accepters: zero ends the process now; otherwise every gesture is held until the last `PaneQuitAnswered`, and a refusal replays them and stays.

MEANS
- a pane answers about the instant it answers, and a paste still arriving refuses;
- a maker-made pane's dirty definition still refuses synchronously, before the ask.

DOES NOT MEAN
- that an accepter which never answers is handled: the quit stays open, and is named so.

PROVEN BY — `workshop/weave_run.cpp` `quit`, `finish_quit`, `on(PaneQuitAnswered)`,
`hold_input`, `replay_held`; `workshop/weave.hpp` `HeldInput`, `kMaxHeldInput`, `quitting_`;
`workshop/pane_vocabulary.hpp` `PaneQuitRequested`, `PaneQuitAnswered`; `editor-pane/pane.cpp`
`on(PaneQuitRequested)`, `kPasteInFlight`; `tests/test_workshop_panes_editor.cpp` case
`"EDIT-W22: a Workshop with no custodian in the room quits at once"`, case `"EDIT-W23: a forged
quit answer moves nothing -- only Loom's answer to the host's ask decides"`, case `"EDIT-W24: an
edit racing the exit check is judged at the answer, and a refused quit costs no keystroke"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-15 — A same-shape reload carries the document, not its history

LAW — `EditorPaneState` is the document as two Texts and nine Ints, built at `snapshot` and put back in `revive`; the undo history, a paste in flight, the wheel fraction and the follow flag are not in it.

MEANS
- the bytes ride as one Text: a four-megabyte source is eleven cells of Loom's 65,536-cell budget;
- the reloaded pane re-offers itself and is re-granted its room; the snapshot is built on demand.

PROVEN BY — `editor-pane/vocabulary.hpp` `EditorPaneState`; `editor-pane/pane.cpp` `snapshot`,
`revive`, `restore_from_state`; `editor-pane/editor.hpp` `EditorBuffer::restore_selection`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W19: the state a same-shape reload keeps is
the DOCUMENT, and the shape says what it is not"`; `tests/test_workshop_load.cpp` case
`"RELOAD-1/VD-25: a four-megabyte dirty document rides a reload in place, and the reloaded pane
still refuses the quit"`, case `"RELOAD-1/VD-25: an Editor with no document reloads to no
document, and permits the quit"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-16 — A sweep arrives as positions, unclamped, and the pane says what they mean

LAW — `PaneDragged` carries the lattice position under the hand, resolved against the pane's body at each motion and not clamped; a row past either edge steps the window one row, and the view follows.

MEANS
- the host records only which pane the press began in (WL-TEXT-14) and sends no release.

PROVEN BY — `editor-pane/pane.cpp` `on(PaneDragged)`; `editor-pane/editor.hpp`
`EditorBuffer::drag_to`; `workshop/pane_vocabulary.hpp` `PaneDragged`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W37: a drag past the body's bottom edge steps
the window, one row per motion"`, case `"EDIT-W45: a sweep in a pane that lost its seat ends, and
sends nothing"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## Do not assume

- That "… N more" is unreachable — the wheel reaches it wherever a cursor is (WL-EDIT-10).
- That an empty Editor leaves the keys to command mode — a runtime pane holds them from the
  press that pointed at it (WL-FOCUS-01); the built-in did not.
