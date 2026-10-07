# Workshop law — the Editor

Register `WL-EDIT`: one source document, held by the Editor pane weave, presented through the
pane protocol. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).
Paths: [`project.md`](project.md); caret: [`pane-caret.md`](pane-caret.md); sweep:
[`text-box.md`](text-box.md).

Retired: WL-EDIT-04.

## WL-EDIT-01 — One document, weave-owned, presented by a pane

LAW — The Editor is a loaded weave (`zengine.editor`) holding the one open document — path, bytes, saved copy, convention, epoch, caret, selection, history, viewport — and Workshop's part is a pane.

MEANS
- `EditorState` and `EditorBuffer` moved with it whole; the host holds no path, no bytes;
- the seam carries values only: no `Session&`, no `HostContext`, no pointer of any kind.

PROVEN BY — `editor-pane/editor.hpp` `EditorState`, `EditorBuffer`, `kEditorUndoDepth`,
`kEditorUndoBudgetBytes`; `editor-pane/pane.cpp`
`EditorPaneWeave`, `e_`; `editor-pane/vocabulary.hpp` `kEditorPaneRole`, `kEditorPane`;
`workshop/default-load-plan.json`; `tests/test_workshop_panes_editor.cpp` case `"the Editor is an
ordinary arranged pane, offered by an office"`, case `"the editor this host used to
compile is named by no presentation source"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-02 — The first multiline consumer owns its multiline machinery

LAW — The single-line component is untouched; the buffer's gestures are declared in its own vocabulary and swept against its consume both ways; a future backend replaces the buffer as one unit.

DOES NOT MEAN
- that a replacement may touch path custody, save authority or presentation.

PROVEN BY — `editor-pane/editor.hpp` `EditorBuffer`, `kEditorVocabulary`,
`EditorBuffer::consume`; `component/text_box.hpp` `TextBox`; `editor-pane/pane.cpp`
`on(PaneKey)`; `tests/test_editor.cpp` case `"the editor's declared vocabulary and consume agree,
both directions"`, case `"undo groups typing, treats joins and pastes as one edit,
and redo returns"`, case `"set_lines wipes the history -- undo cannot
resurrect another document"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-03 — Custody is the weave's, and that is the no-silent-loss floor

LAW — Hide, move, cover, reorder or remove the pane and no document is touched; a dirty buffer refuses another source and, asked, an orderly quit; `editor.discard` (`ctrl+d`) is the one discard door.

MEANS
- discard is undoable through `revert_to`, which keeps the history;
- process death still loses drafts: no crash recovery.

PROVEN BY — `editor-pane/pane.cpp` `judge_source`, `discard_source_edits`,
`on(PaneQuitRequested)`; `editor-pane/editor.hpp` `EditorBuffer::revert_to`, `EditorState`;
`workshop/weave_run.cpp` `quit`, `on(PaneQuitAnswered)`; `tests/test_workshop_panes_editor.cpp`
case `"removing and reopening the pane cannot lose a byte, a caret, or a step of history"`, case
`"a dirty buffer refuses a different source, and a save opens the way"`, case `"an
orderly quit refuses while source is unsaved, and proceeds once it is not"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-05 — One promise at two doors: `zengine.opening` and `zengine.editor`

LAW — Asked to prepare, the Editor judges with nothing moved, builds a candidate beside the document and offers its identity; the candidate becomes the document only in the showing hook; the old door relays.

MEANS
- the current document stays editable; an edit moves its claim and aborts the operation;
- a refusal drops the candidate; a paste in flight, or a dirty document, refuses;
- the old door installs nothing alone; its relay is refused in words or matched by attempt.

DOES NOT MEAN
- that a candidate rides a reload: a successor shown what it did not prepare declines.

PROVEN BY — `editor-pane/pane.cpp` `on(OpenSourceRequested)`, `on(SourceOpened)`,
`on(PrepareSourceRequested)`, `on_claim_published`, `judge_source`, `Candidate`, `Relay`,
`activate`; `editor-pane/editor.hpp` `source_in`, `EditorState`;
`workshop/pane_seam_vocabulary.hpp` `OpenSourceRequested`, `SourceOpened`, `kEditorRole`;
`workshop/open_seam_vocabulary.hpp` `PrepareSourceRequested`, `SourcePrepared`, `kOpeningRole`;
`tests/test_workshop_panes_editor.cpp` case
`"a clipboard answer refuses the open wherever it lands, and A keeps its paste"`, case
`"the old door still opens and shows, or refuses truthfully, by a kept answer right"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-EDIT-06 — Identity is a normalized spelling, not a filesystem object

LAW — An absolute entrant is itself; a relative one is the project's file, and until the owner has answered it MEANS NOTHING and is refused — never spelled against the process directory.

MEANS
- an unanswered owner and one that named no root are different facts;
- the second keeps its policy: spent as written, then `lexically_normal`;
- a late answer retargets nothing open: identity is fixed at resolution.

DOES NOT MEAN
- that case-folding and hard links are handled.

PROVEN BY — `editor-pane/pane.cpp` `resolve`, `project_dir_`, `project_known_`,
`on(ProjectRoot)`; `workshop/persist.hpp` `resolved_against`; `editor-pane/editor.hpp`
`EditorState::path`; `tests/test_workshop_panes_editor.cpp` case `"a relative path is the
project's file, and means nothing until the project has said"`, case `"re-requesting the open
source reveals it and destroys nothing"`.
WHY — `agents/decisions/one-door-takes-a-path.md`

## WL-EDIT-07 — The source-byte law is the media's honest reach

LAW — Printable ASCII plus tab, one line ending per document (LF or CRLF), a final newline as a final empty line; mixed endings, bare CR, control bytes and non-ASCII refuse whole, naming the line.

MEANS
- the convention is detected at open and spent on each inserted newline;
- `source_in`/`source_text` are exact inverses; the file is never rewritten;
- typed and pasted text meet the same law at the doors, refused in a row.

PROVEN BY — `editor-pane/editor.hpp` `source_in`, `source_text`, `pasteable_source`,
`line_ending`, `source_byte_ok`, `PasteableSource`; `editor-pane/pane.cpp` `on(PaneTextInput)`;
`tests/test_editor.cpp` case `"source_in and source_text are inverse over everything admitted"`;
`tests/test_workshop_panes_editor.cpp` case `"a clipboard holding non-ASCII refuses
the paste, and typed non-ASCII is refused with a sentence"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-08 — Tabs expand at presentation only

LAW — Tabs expand only at presentation, at a four-column stop; one tab-geometry measurer (bytes to displayed columns and back, plus the slice) is what the rows, the press and the drag spend.

MEANS
- `kCaretCols` reserves the caret's column of every document row (the Terminal's rule).

PROVEN BY — `editor-pane/editor.hpp` `EditorState::first_col`, `visual_col_of`,
`byte_of_visual_col`, `expanded_slice`, `kEditorTabStop`; `editor-pane/pane.cpp` `kCaretCols`,
`press_at`; `tests/test_editor.cpp` case `"tab geometry maps bytes and displayed columns
both ways, exactly"`; `tests/test_workshop_panes_editor.cpp` case `"a press places the
caret through the same tab geometry the paint used, and the caret stands in its picture where the
press put it"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-09 — The viewport reconciles once per composition

LAW — `reconcile` clamps the offsets always, follows the caret when a gesture asked (`follow_caret`) or the GRANTED ROOM changed, and deliberately not after the wheel or a reveal; it runs once, inside `say`.

MEANS
- a notice appearing or clearing changes the document's rows and is not a resize;
- asking for the OPEN source again is a reveal: a scrolled view is the weaver's.

PROVEN BY — `editor-pane/pane.cpp` `reconcile`, `say`; `editor-pane/editor.hpp`
`EditorState::follow_caret`, `EditorState::last_rows`; `tests/test_workshop_panes_editor.cpp`
case `"keyboard navigation scrolls the window and the caret never leaves it"`, case
`"asking for the open source again moves the pane, never the view"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-11 — A paste answer lands where the weaver asked or nowhere

LAW — A pending paste pins the document epoch and the buffer revision it was asked for, so a replaced document strands the payload silently and a document that merely moved is told to paste again.

MEANS
- it retires with its subject: a new document clears the flight, so a clean one is not

PROVEN BY — `editor-pane/pane.cpp` `begin_paste`, `on(ClipboardText)`, `Paste`, `activate`;
`editor-pane/editor.hpp` `EditorState::doc_epoch`, `EditorBuffer::revision`,
`EditorBuffer::paste_lines`, `EditorBuffer::set_lines`;
`tests/test_workshop_panes_editor.cpp` case `"a late paste answer may not land at a caret that has
since moved"`, case `"a paste retires with the document it was asked for"`, case
`"a dirty document with no paste in flight still refuses the exit"`.
WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-EDIT-12 — The pane composes its rows, the caret and the selection standing in them

LAW — The pane composes a status row (dirty word, `L:C/N`, the path), a notice row where the room holds one, then the document through the viewport; the caret and the clipped selection stand in them.

MEANS
- a room too small for both keeps the document's row: the notice replaces the status row;
- one row is the status row alone, and the caret has nowhere to be;
- they are its picture on its canvas, and prose rows with `PaneCaret` beside them to a host granting none.

PROVEN BY — `editor-pane/pane.cpp` `say`, `compose`, `caret_of`, `rows_caret`, `parts_of`,
`status_text`, `kNoticeNeedsRows`; `workshop/pane_canvas_rows.hpp` `rows_picture`;
`workshop/screen_external.cpp` `external_header`, `external_body_place`;
`tests/test_workshop_panes_editor.cpp` case `"a selection that runs above the window is clipped,
and one wholly out of it is not said"`, case `"in a room too small for both, the
document keeps its rows and a notice stands in for the status row"`, case `"a host that grants
the Editor no canvas is shown its rows and caret as prose, its presses reach nothing there, and
its keys still edit"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-13 — The desk's half: a trial on a copy, an admission, the publication applied whole

LAW — The desk judges a trial seat on a copy and moves nothing; asked to admit, it re-judges, keeps the content and offers its presentation; shown the publication, it applies seat, keys, room and content.

MEANS
- a change of its rows, an authored change or a routed input before the commitment aborts it; a canvas moved before the admission refuses it there;
- a presentation it holds no trial for is Declined, said, and re-claimed from the live desk;
- the content is the pane's picture in the room the trial reserved, or its rows (WL-OPEN-10).

PROVEN BY — `workshop/weave_managed.cpp` `on(PresentationTrialRequested)`,
`on(PresentationAdmitRequested)`, `on(v2::PresentationAdmitRequested)`, `on(ManagedOpenSettled)`,
`on_claim_published`, `show_presentation`, `trial_room`, `trial_room_stands`;
`workshop/weave_seam.cpp` `on(PaneRevealRequested)`; `workshop/screen.hpp`
`stack_slots_that_fit`; `workshop/open_seam_vocabulary.hpp` `PanePresentation`,
`ManagedOpenSettled`; `tests/test_workshop_panes_opening.cpp` case `"a canvas room that moves
while the document is prepared refuses its admission, though its rows and columns stand"`;
`tests/test_workshop_panes_editor.cpp` case
`"a resize after the commitment is an ordinary presentation change"`, case
`"the real desk, shown a presentation it holds no trial for, answers that it did not apply it --
Declined, not held, named, and re-claiming its own truth"`.
WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-EDIT-14 — The exit is asked of the room, and decided by the answer

LAW — `quit` publishes `PaneQuitRequested` and counts Loom's accepters: zero ends the process now; otherwise every gesture is held while the room is asked, and a refusal replays them and stays.

MEANS
- a pane answers about the instant; a paste still arriving, or an open being seated, refuses;
- a delivery Loom refused ends the quit as a refusal; the process stays open (WL-SESSION-19).

DOES NOT MEAN
- that an accepter which never answers is handled: the quit stays open, named so.

PROVEN BY — `workshop/weave_run.cpp` `quit`, `finish_quit`, `on(PaneQuitAnswered)`,
`hold_input`, `replay_held`; `workshop/weave.hpp` `HeldInput`, `kMaxHeldInput`, `quitting_`;
`workshop/pane_vocabulary.hpp` `PaneQuitRequested`, `PaneQuitAnswered`; `editor-pane/pane.cpp`
`on(PaneQuitRequested)`, `kPasteInFlight`; `tests/test_workshop_panes_editor.cpp` case
`"a Workshop with no custodian in the room quits at once"`, case `"a forged quit answer
moves nothing -- only Loom's answer to the host's ask decides"`, case `"an edit racing
the exit check is judged at the answer, and a refused quit costs no keystroke"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-15 — A same-shape reload carries the document, not its history

LAW — `EditorPaneState` is one truth: the pane mirrors its live document into it at each composition, so Loom's `snapshot` carries a reload and `zen.PokeRead` answers what the weaver sees.

MEANS
- every advertised field reads live; the mirror is written, never refreshed at a poke;
- the Texts are rebuilt when the BYTES move (`content_revision`), never on a gesture;
- the room last composed for rides too: an unchanged room after a reload is no resize.

DOES NOT MEAN
- that the undo history, an operation in flight or the wheel fraction ride.

PROVEN BY — `editor-pane/vocabulary.hpp` `EditorPaneState`, `EditorPaneState::text_builds`;
`editor-pane/pane.cpp` `mirror_state`, `revive`, `restore_from_state`, `saved_stamp_`;
`editor-pane/editor.hpp` `EditorBuffer::restore_selection`, `EditorBuffer::content_revision`;
`tests/test_workshop_panes_editor.cpp` case `"the mirror is rebuilt when the bytes move and at no
other time"`; `tests/test_workshop_load.cpp` case
`"an unchanged room after a reload is not a resize, and the view it was scrolled to stands"`, case
`"the reloaded pane reads live, not out of the snapshot it revived from"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-16 — A sweep arrives as places, unclamped, and the pane says what they mean

LAW — A held primary press's motion extends the gesture it began and means nothing otherwise; its unclamped place is read against the chrome THAT PRESS SAW, and a row past either edge steps the window.

MEANS
- a press taken as focus alone, or beside the rows, begins no gesture: later motions sweep nothing;
- a pointer gesture composes no new rows, so the notice and the picture survive it;
- its release or its loss ends it; the host ends the hold of a pane that left (WL-CANVAS-02).

PROVEN BY — `editor-pane/pane.cpp` `on(PaneCanvasPointer)`, `press_at`, `dragged`, `Drag`;
`editor-pane/editor.hpp` `EditorBuffer::drag_to`; `workshop/pane_canvas_vocabulary.hpp`
`PaneCanvasPointer`; `tests/test_workshop_panes_editor.cpp` case `"a press that only focuses
begins no sweep, and a gesture keeps the geometry it was made against"`, case `"a sweep in a pane
that lost its seat is lost with it, and a motion after the close moves nothing it selected"`, case
`"a press begins a sweep only where it named a row of the body"`.
WHY — `agents/decisions/the-editor-is-the-custodian.md`

## Do not assume

- That an empty Editor leaves the keys to command mode — a runtime pane holds them from
  the press that pointed at it (WL-FOCUS-01).
