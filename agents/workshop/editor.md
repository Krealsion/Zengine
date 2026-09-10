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

PROVEN BY — `editor-pane/editor.hpp` `EditorState`, `EditorBuffer`, `kEditorUndoDepth`,
`kEditorUndoBudgetBytes`; `editor-pane/pane.cpp`
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

PROVEN BY — `editor-pane/pane.cpp` `judge_source`, `discard_source_edits`,
`on(PaneQuitRequested)`; `editor-pane/editor.hpp` `EditorBuffer::revert_to`, `EditorState`;
`workshop/weave_run.cpp` `quit`, `on(PaneQuitAnswered)`; `tests/test_workshop_panes_editor.cpp`
case `"EDIT-W17: removing and reopening the pane cannot lose a byte, a caret, or a step of
history"`, case `"EDIT-W12: a dirty buffer refuses a different source, and a save opens the
way"`, case `"EDIT-W20: an orderly quit refuses while source is unsaved, and proceeds once it
is not"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-04 — `^s` follows the keyboard, as two declared identities

LAW — `^s` is two rows: `document.save`, active everywhere the keyboard's pane has not DECLARED that it stands in for it (`kUnlessOwned`), and the Editor's own `editor.save`, which declares exactly that.

MEANS
- so the object document's save is still the key in a layout name, a draft and every other pane;
- the two never both fire, and neither moves when a maker rebinds either (WL-KEY-15).

PROVEN BY — `workshop/keymap.hpp` `document.save`, `KeyContext::kUnlessOwned`,
`Keymap::row_active`; `workshop/pane_vocabulary.hpp` `kOwnableDocumentSave`;
`editor-pane/vocabulary.hpp` `kActionSave`; `editor-pane/pane.cpp` `declare`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W3: one physical ^s is the
document's save or the source's, by who holds the keys"`, case `"EDIT-W50: supersession is by
name, so moving either key moves neither meaning"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-EDIT-05 — The one door is `OpenSourceRequested` at `zengine.editor`, and it takes a path

LAW — Judge with nothing moved, hold every gesture, ask the desk to seat the pane, install on its word; the desk's delivery of the ask is the commitment, so a refusal at any step opens and authors nothing.

MEANS
- the answer is deferred across the ask; the candidate is bytes, not a second document;
- held gestures replay after the outcome, into the document it leaves; a quit meanwhile refuses;
- one at a time; Files hands a path, the Builder what `RecipeSourceSaid` named.

PROVEN BY — `editor-pane/pane.cpp` `on(OpenSourceRequested)`, `on(PaneRevealAnswered)`,
`judge_source`, `commit_source`, `settle`, `Pending`, `hold`, `replay_held`, `install`;
`editor-pane/editor.hpp` `source_in`, `kMaxSourceBytes`, `EditorState`;
`workshop/pane_seam_vocabulary.hpp` `OpenSourceRequested`, `SourceOpened`, `kEditorRole`;
`workshop/builder_seam_vocabulary.hpp` `RecipeSourceRequested`, `RecipeSourceSaid`;
`workshop/pane_doors.hpp` `ProjectDoor`; `tests/test_workshop_panes_editor.cpp` case
`"EDIT-W9: an opening that cannot be shown opens nothing, and the requester is told why"`, case
`"EDIT-W58: a clipboard answer refuses the open only if it lands before the ask"`, case
`"EDIT-W67: room lost before the commitment refuses the open, and nothing is authored or moved"`.
WHY — `agents/decisions/one-door-takes-a-path.md`

## WL-EDIT-06 — Identity is a normalized spelling, not a filesystem object

LAW — An absolute entrant is itself; a relative one is the project's file, and until the owner has answered it MEANS NOTHING and is refused — never spelled against the process directory.

MEANS
- an unanswered owner and one that authoritatively named no root are different facts;
- the second keeps its policy: spent as written, then `lexically_normal`;
- a late answer retargets nothing open: identity is fixed where it was resolved.

DOES NOT MEAN
- that case-folding and hard links are handled: named residuals.

PROVEN BY — `editor-pane/pane.cpp` `resolve`, `project_dir_`, `project_known_`,
`on(ProjectRoot)`; `workshop/persist.hpp` `resolved_against`; `editor-pane/editor.hpp`
`EditorState::path`; `tests/test_workshop_panes_editor.cpp` case `"EDIT-W51: a relative path is
the project's file, and means nothing until the project has said"`, case `"EDIT-W11:
re-requesting the open source reveals it and destroys nothing"`.
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

LAW — `reconcile` clamps the offsets always, follows the caret when a gesture asked (`follow_caret`) or the GRANTED ROOM changed, and deliberately not after the wheel or a reveal; it runs once, inside `say`.

MEANS
- a notice appearing or clearing changes the document's rows and is not a resize;
- asking for the OPEN source again is a reveal: a scrolled view is the maker's.

PROVEN BY — `editor-pane/pane.cpp` `reconcile`, `say`; `editor-pane/editor.hpp`
`EditorState::follow_caret`, `EditorState::last_rows`; `tests/test_workshop_panes_editor.cpp`
case `"EDIT-W40: keyboard navigation scrolls the window and the caret never leaves it"`, case
`"EDIT-W66: asking for the open source again moves the pane, never the view"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-11 — A paste answer lands where the maker asked or nowhere

LAW — A pending paste pins the document epoch and the buffer revision it was asked for, so a replaced document strands the payload silently and a document that merely moved is told to paste again.

MEANS
- it retires with its subject: a new document clears the flight, so a clean one is not

PROVEN BY — `editor-pane/pane.cpp` `begin_paste`, `on(ClipboardText)`, `Paste`, `install`;
`editor-pane/editor.hpp` `EditorState::doc_epoch`, `EditorBuffer::revision`,
`EditorBuffer::paste_lines`, `EditorBuffer::set_lines`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W32: a late paste answer may not land at a
caret that has since moved"`, case `"EDIT-W54: a paste retires with the document it was asked
for"`, case `"EDIT-W55: a dirty document with no paste in flight still refuses the exit"`.
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

LAW — A reveal is one ask, made when the pane's act needs nothing more: in that delivery the desk seats, selects and points the keys at the pane, or refuses with nothing moved.

MEANS
- so a refusal anywhere leaves the setup, the selection and the keyboard as they were;
- the desk holds nothing between deliveries: two asks are two seats, judged in order;
- a reveal naming a pane the office never offered is refused; from nobody, dropped.

PROVEN BY — `workshop/weave_seam.cpp` `on(PaneRevealRequested)`;
`workshop/pane_vocabulary.hpp` `PaneRevealRequested`, `PaneRevealAnswered`;
`workshop/screen.hpp` `stack_slots_that_fit`; `editor-pane/pane.cpp` `on(PaneRevealAnswered)`;
`tests/test_workshop_panes_editor.cpp` case
`"EDIT-W9: an opening that cannot be shown opens nothing, and the requester is told why"`, case
`"EDIT-W10: a reveal from an office that offered no such pane is dropped"`, case
`"EDIT-W68: a resize after the commitment is an ordinary presentation change"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-14 — The exit is asked of the room, and decided by the answer

LAW — `quit` publishes `PaneQuitRequested` and counts Loom's accepters: zero ends the process now; otherwise every gesture is held until the last `PaneQuitAnswered`, and a refusal replays them and stays.

MEANS
- a pane answers about the instant; a paste still arriving, or an open being seated, refuses;
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

LAW — `EditorPaneState` is one truth: the pane mirrors its live document into it at each composition, so Loom's `snapshot` carries a reload and `zen.PokeRead` answers what the maker sees.

MEANS
- every advertised field reads live; there is no hook to refresh state at a poke;
- the Texts are rebuilt when the BYTES move (`content_revision`), never on a gesture;
- the room last composed for rides too: an unchanged room after a reload is no resize.

DOES NOT MEAN
- that the undo history, an operation in flight or the wheel fraction ride: they do not.

PROVEN BY — `editor-pane/vocabulary.hpp` `EditorPaneState`, `EditorPaneState::text_builds`;
`editor-pane/pane.cpp` `mirror_state`, `revive`, `restore_from_state`, `saved_stamp_`;
`editor-pane/editor.hpp` `EditorBuffer::restore_selection`, `EditorBuffer::content_revision`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W64: the mirror is rebuilt when the bytes
move and at no other time"`; `tests/test_workshop_load.cpp` case
`"RELOAD-2/VD-26: an unchanged room after a reload is not a resize, and the view it was scrolled
to stands"`, case `"RELOAD-3/VD-26: the reloaded pane reads live, not out of the snapshot it
revived from"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-16 — A sweep arrives as positions, unclamped, and the pane says what they mean

LAW — A `PaneDragged` extends the gesture a `PanePressed` began and means nothing otherwise; the unclamped position is read against the chrome THAT PRESS SAW, and a row past either edge steps the window.

MEANS
- a press taken as focus alone begins no gesture: the motions after it sweep nothing;
- a pointer gesture composes no new rows, so the notice and the picture survive it;
- the host records only which pane the press began in (WL-TEXT-14) and sends no release.

PROVEN BY — `editor-pane/pane.cpp` `on(PaneDragged)`, `on(PanePressed)`, `Drag`;
`editor-pane/editor.hpp` `EditorBuffer::drag_to`; `workshop/pane_vocabulary.hpp` `PaneDragged`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W53: a press that only focuses begins no
sweep, and a gesture keeps the geometry it was made against"`, case `"EDIT-W45: a sweep in a pane
that lost its seat ends, and sends nothing"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## Do not assume

- That an empty Editor leaves the keys to command mode — a runtime pane holds them from the
  press that pointed at it (WL-FOCUS-01); the built-in did not.
