# Workshop law — the source editor

Register `WL-EDIT`: one document, session-owned, presented by a pane. One law per heading; cite
by ID. Router: [`../workshop.md`](../workshop.md). Where a source path comes from is
[`project.md`](project.md) and [`files.md`](files.md).

## WL-EDIT-01 — One document, session-owned, presented by a pane

LAW — The editor's machinery is its own file, the session owns the one open document — path, saved copy, line ending, epoch, viewport — and the Editor pane in the overlay stack is only its presentation.

MEANS
- `EditorState`: the path, the saved copy (dirty derives), line ending, `doc_epoch`, viewport;
- `EditorBuffer`: lines, caret, anchor, preferred column, bounded snapshot undo, `revision()`.

PROVEN BY — `workshop/editor.hpp` `EditorState`, `EditorBuffer`, `doc_epoch`, `revision`,
`kEditorUndoDepth`, `kEditorUndoBudgetBytes`; `workshop/screen.hpp` `editor`; `workshop/panel.hpp`
`kEditor`; `workshop/weave.hpp` `save_source`; `tests/test_workshop_editor.cpp` case `"EDIT-0:
dirty derives by comparison -- editing back to the saved text is clean"`, case `"EDIT-0: the
revision moves with text, caret and selection, and with nothing else"`, case `"EDIT-0: the Editor
is an ordinary catalog pane with a durable reference"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-02 — The first multiline consumer owns its multiline machinery

LAW — The single-line component is untouched; the editor's gestures are declared in its own vocabulary and swept against its consume both ways, and a future backend replaces the buffer as one unit.

DOES NOT MEAN
- that a replacement may touch path custody, save authority or the pane presentation.

PROVEN BY — `workshop/editor.hpp` `EditorBuffer`, `kEditorVocabulary`, `consume`;
`component/text_box.hpp` `TextBox`; `workshop/weave.hpp` `editor_key`;
`tests/test_workshop_editor.cpp` case `"EDIT-0: the editor's declared vocabulary and consume
agree, both directions"`, case `"EDIT-0: undo groups typing, treats joins and pastes as one edit,
and redo returns"`, case `"EDIT-0: set_lines wipes the history -- undo cannot resurrect another
document"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-03 — The document is session state, and that is the no-silent-loss floor

LAW — Hide, move, cover, reorder or remove the pane and no document is touched; a dirty buffer refuses another source and an orderly quit, and `editor.discard` (`ctrl+d`) is the one discard door.

MEANS
- discard is undoable through `revert_to`, which keeps the history;
- arranging the pane moves its window and not one byte of source;
- process death still loses drafts: no crash recovery is claimed.

PROVEN BY — `workshop/weave.hpp` `open_source`, `discard_source_edits`, `quit`;
`workshop/editor.hpp` `revert_to`, `EditorState`; `workshop/keymap.hpp` `editor.discard`;
`tests/test_workshop_editor.cpp` case `"EDIT-0: removing and reopening the pane cannot lose a byte
of dirty source"`, case `"EDIT-0: a dirty buffer refuses a different source, and save or discard
opens the way"`, case `"EDIT-0: an orderly close refuses while source is unsaved, and proceeds
once it is not"`, case `"EDIT-0: discard is deliberate, scoped, undoable, and honest about nothing
to do"`, case `"EDIT-0: arranging the editor pane moves its window and not one byte of its
source"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-04 — `^s` follows the keyboard, as two declared identities

LAW — `^s` is two declared rows — the document's save everywhere but the editor, and the editor's own — while `^o` stays global and `^c` is copy in the editor and quit where nothing takes text.

PROVEN BY — `workshop/keymap.hpp` `kEditor`, `document.save`, `editor.save`, `kNoEditor`,
`context_takes_text`; `tests/test_workshop_editor.cpp` case `"EDIT-0: one physical ^s resolves
to the document's save or the editor's, by context"`, case `"EDIT-0: ^s in the editor saves the
SOURCE; elsewhere it keeps the document's meaning"`, case `"EDIT-0: ^o keeps its global
object-document meaning while the editor has the keys"`, case `"EDIT-0: ^c in the editor copies
-- it does not quit -- and quit stays a press away"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-EDIT-05 — The one door is `open_source(path, mail)`, and it takes a path

LAW — Normalize, same-path reveal, dirty refusal, bounded read, `source_in`, trial-seat, install with `doc_epoch++` and a viewport reset, focus and sentence; every referrer arrives through it.

MEANS
- Files hands it a row's path; `edit_source` (the Builder's `e`) keeps only the recipe half;
- `EditorState` holds no acquisition provenance: the editor owns the document, not the reason.

PROVEN BY — `workshop/weave.hpp` `open_source`, `edit_source`, `recipe_source`, `RecipeSource`;
`workshop/editor.hpp` `source_in`, `kMaxSourceBytes`, `EditorState`;
`tests/test_workshop_files.cpp` case `"EDIT-1: opening a row hands the path to the ONE editor
door"`, case `"EDIT-1: Builder and the browser open ONE document, however the path is spelled"`,
case `"EDIT-1: the door normalizes what a referrer hands it, not what a browser happened to
build"`; `tests/test_workshop_editor.cpp` case `"EDIT-0: `e` opens the chosen recipe's source,
focuses the editor, and says so"`.
WHY — `agents/decisions/one-door-takes-a-path.md`

## WL-EDIT-06 — Identity is a normalized spelling, not a filesystem object

LAW — Every entrant is made absolute against the project, `lexically_normal` and forward-slashed, so `a.cpp` and `./a.cpp` are one document; nothing canonicalizes.

DOES NOT MEAN
- that case-folding and hard links are handled — they remain named residuals.

PROVEN BY — `workshop/weave.hpp` `open_source`; `workshop/persist.hpp` `resolved_against`;
`workshop/editor.hpp` `EditorState::path`; `tests/test_workshop_files.cpp` case `"EDIT-1:
equivalent spellings of one file are one document"`; `tests/test_workshop_editor.cpp` case
`"EDIT-0: re-requesting the open source reveals it and destroys nothing"`.
WHY — `agents/decisions/one-door-takes-a-path.md`

## WL-EDIT-07 — The source-byte law is the media's honest reach

LAW — Printable ASCII plus tab, one line ending per document (LF or CRLF), a final newline as a final empty line; mixed endings, bare CR, control bytes and non-ASCII refuse whole, naming the line.

MEANS
- the convention is detected at open and spent on every inserted newline;
- `source_in`/`source_text` are exact inverses, and the file is never rewritten;
- typed and pasted text meet the same law at the weave's doors.

PROVEN BY — `workshop/editor.hpp` `source_in`, `source_text`, `pasteable_source`, `line_ending`,
`source_byte_ok`, `PasteableSource`; `workshop/weave.hpp` `editor_text`;
`tests/test_workshop_editor.cpp` case `"EDIT-0: source_in and source_text are inverse over
everything admitted"`, case `"EDIT-0: mixed endings, bare CR, control bytes and non-ASCII are
refused whole"`, case `"EDIT-0: CRLF and the final-newline state round-trip through open, edit,
save"`, case `"EDIT-0: typed non-ASCII is refused with a sentence, and the keystroke costs
nothing"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-08 — Tabs expand at presentation only

LAW — Tabs expand only at presentation, at a four-column stop, and one tab-geometry measurer — bytes to displayed columns and back, and the displayed slice — is what painter, press and drag spend.

MEANS
- `kEditorCaretCols` reserves the caret's column of every body row, `kTerminalCaretCols`' rule.

PROVEN BY — `workshop/editor.hpp` `first_col`, `visual_col_of`, `byte_of_visual_col`,
`expanded_slice`, `kEditorTabStop`; `workshop/screen.hpp` `kEditorCaretCols`, `EditorPressAt`;
`workshop/weave.hpp` `editor_press`; `tests/test_workshop_editor.cpp` case `"EDIT-0: tab geometry
maps bytes and displayed columns both ways, exactly"`, case `"EDIT-0: expanded_slice shows tabs as
spaces and windows by displayed columns"`, case `"EDIT-0: a press places the caret through the
same tab geometry the paint used"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-09 — The viewport reconciles once per repaint

LAW — `reconcile_editor_view` clamps the offsets always, follows the caret when a gesture asked (`follow_caret`) or the body's room changed, and deliberately not after the wheel.

PROVEN BY — `workshop/screen.hpp` `reconcile_editor_view`; `workshop/editor.hpp` `follow_caret`;
`workshop/weave.hpp` `editor_key`; `tests/test_workshop_editor.cpp` case `"EDIT-0: keyboard
navigation scrolls the window and the caret never leaves it"`, case `"EDIT-0: a resize reconciles
the viewport and does not strand the caret"`, case `"EDIT-0: a horizontal window follows the caret
and recovers the room an erase frees"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-10 — `on(PointerWheel)` is Workshop's one wheel router

LAW — `on(PointerWheel)` routes every wheel: modes keep their ownership, the topmost occupancy decides (picker, then front order), a header row is not the body, and notches accumulate in `spend_wheel`.

MEANS
- the editor's body (caret still), Project Files, both Pane Manager lists, the picker;
- every list's wheel moves its cursor by `kListWheelRows`: a list derives its window from it;
- an external pane's body: the notches cross the seam, and what they mean there is the protocol's.

DOES NOT MEAN
- that there is a scroll framework, a scrollbar, a global offset map or a persisted position.

PROVEN BY — `workshop/weave.hpp` `pane_editor_wheel`, `picker_wheel`, `on(PointerWheel)`,
`files_wheel`; `workshop/screen.hpp` `kListWheelRows`, `list_window`, `spend_wheel`;
`tests/test_workshop_editor.cpp` case `"EDIT-0: the wheel scrolls the editor's body, moves no
caret, and is consumed there"`, case `"EDIT-0: the wheel elsewhere scrolls nothing, and a covered
editor is not reached"`; `tests/test_workshop_files.cpp` case `"EDIT-1: the wheel moves the
browser's cursor and leaves the editor's alone"`; `tests/test_workshop_panels.cpp` case
`"QR-18/SC-5: the Pane Editor's two lists are reached by the wheel past their windows"`;
`tests/test_workshop_panes_seam.cpp` case `"QR-18/SC-5: the picker's windowed inventory is reached
by the wheel"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-11 — A paste answer lands where the maker asked or nowhere

LAW — A pending paste pins the document epoch and the buffer revision it was asked for, so a replaced document strands the payload silently and a document that merely moved is told to paste again.

PROVEN BY — `workshop/weave.hpp` `open_source`, `editor_doc`, `editor_revision`, `PendingPaste`;
`workshop/editor.hpp` `EditorState::doc_epoch`, `EditorBuffer::revision`, `set_lines`,
`paste_lines`; `tests/test_workshop_editor.cpp` case `"EDIT-0: a late paste answer may not land at
a caret that has since moved"`, case `"EDIT-0: a late answer for a replaced document is discarded
whole"`, case `"EDIT-0: copy here, paste there -- multiline, through the medium's own answer"`.
WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-EDIT-12 — The pane paints one region

LAW — The pane paints one region: a header row — dirty word first, then `L:C/N`, then the path, with the `> ` mark — and the document through the viewport, caret and selection as the region's own.

MEANS
- the body is `external_body_place` with `kEditorHeaderRows` — one arithmetic, not two.

PROVEN BY — `workshop/screen.hpp` `paint_editor`, `external_header`, `kEditorHeaderRows`,
`external_body_place`, `editor_header`; `workshop/weave.hpp` `editor_press`;
`tests/test_workshop_editor.cpp` case `"EDIT-0: the band and the header both say where typing
goes"`, case `"EDIT-0: a press on the editor's header focuses without moving the caret"`, case
`"EDIT-0: a drag sweeps a multiline selection, and the selection survives release"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-13 — At the minimum screen the Editor has no room

LAW — Where the overlay stack has no slot left the open door refuses and the refusal names the remedy.

PROVEN BY — `workshop/weave.hpp` `open_source`; `workshop/screen.hpp` `stack_slots_that_fit`;
`tests/test_workshop_editor.cpp` case `"EDIT-0: at the minimum screen there is no room, and the
refusal names the remedy"`.
WHY — `agents/decisions/one-door-takes-a-path.md`

## Do not assume

- That "… N more" is unreachable — the wheel reaches it on every surface Workshop windows and
  on an external pane whose provider spends the notches; Loaded is the one shipped pane that
  only counts (WL-EDIT-10).
