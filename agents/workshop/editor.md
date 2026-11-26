# Workshop law — the Editor

Register `WL-EDIT`: one source document, held by the Editor pane weave, presented through the
pane protocol. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).
Paths: [`project.md`](project.md); caret: [`pane-caret.md`](pane-caret.md); sweep:
[the text box's laws](../../component/docs/component-laws.md).

Retired: WL-EDIT-04, WL-EDIT-08, WL-EDIT-09, WL-EDIT-12, WL-EDIT-13, WL-EDIT-16

## WL-EDIT-01 — One document, weave-owned, presented by a pane

LAW — The Editor is a loaded weave (`zengine.editor`) holding the one open document — path, bytes, saved copy, convention, epoch, caret, selection, history, viewport — and Workshop's part is a pane.

MEANS
- `EditorState` and `EditorBuffer` moved with it whole; the host holds no path, no bytes;
- the seam carries values only: no `Session&`, no `HostContext`, no pointer of any kind.

WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-02 — The first multiline consumer owns its multiline machinery

LAW — The single-line component is untouched; the buffer's gestures are declared in its own vocabulary and swept against its consume both ways; a future backend replaces the buffer as one unit.

DOES NOT MEAN
- that a replacement may touch path custody, save authority or presentation.

WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-03 — Custody is the weave's, and that is the no-silent-loss floor

LAW — Hide, move, cover, reorder or remove the pane and no document is touched; a dirty buffer refuses another source, a close and, asked, an orderly quit; only `editor.discard` or the quit box discards.

MEANS
- discard is undoable through `revert_to`, which keeps the history;
- process death still loses drafts: no crash recovery.

WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-05 — One promise at two doors: `zengine.opening` and `zengine.editor`

LAW — Asked to prepare, the Editor judges with nothing moved, builds a candidate beside the document and offers its identity; the candidate becomes the document only in the showing hook; the old door relays.

MEANS
- the current document stays editable; an edit moves its claim and aborts the operation;
- a refusal drops the candidate; a paste in flight, or a dirty document, refuses;
- the old door installs nothing alone; its relay is refused in words or matched by attempt.

DOES NOT MEAN
- that a candidate rides a reload: a successor shown what it did not prepare declines.

WHY — `agents/decisions/document-and-desk-publish-together.md`

## WL-EDIT-06 — Identity is a normalized spelling, not a filesystem object

LAW — An absolute entrant is itself; a relative one is the project's file, and until the owner has answered it MEANS NOTHING and is refused — never spelled against the process directory.

MEANS
- an unanswered owner and one that named no root are different facts;
- the second keeps its policy: spent as written, then `lexically_normal`;
- a late answer retargets nothing open: identity is fixed at resolution.

DOES NOT MEAN
- that case-folding and hard links are handled.

WHY — `agents/decisions/one-door-takes-a-path.md`

## WL-EDIT-07 — The source-byte law is the media's honest reach

LAW — Printable ASCII plus tab, one line ending per document (LF or CRLF), a final newline as a final empty line; mixed endings, bare CR, control bytes and non-ASCII refuse whole, naming the line.

MEANS
- the convention is detected at open and spent on each inserted newline;
- `source_in`/`source_text` are exact inverses; the file is never rewritten;
- typed and pasted text meet the same law at the doors, refused in a row.

WHY — `agents/decisions/the-first-multiline-consumer.md`

## WL-EDIT-11 — A paste answer lands where the weaver asked or nowhere

LAW — A pending paste pins the document epoch and the buffer revision it was asked for, so a replaced document strands the payload silently and a document that merely moved is told to paste again.

MEANS
- it retires with its subject: a new document clears the flight, so a clean one is not

WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-EDIT-14 — The exit is asked of the room, and decided by the answer

LAW — `quit` publishes `PaneQuitRequested` and counts Loom's accepters: zero ends the process now; otherwise every gesture is held while the room is asked, and a refusal replays them and stays.

MEANS
- a pane answers about the instant; a paste still arriving, or an open being seated, refuses;
- a delivery Loom refused ends the quit as a refusal; the process stays open (WL-SESSION-19).

DOES NOT MEAN
- that an accepter which never answers is handled: the quit stays open, named so.

WHY — `agents/decisions/the-editor-is-the-custodian.md`

## WL-EDIT-15 — A same-shape reload carries the document and its undo history

LAW — `EditorPaneState` is one truth: the pane mirrors its live document into it at each composition, so Loom's `snapshot` carries a reload and `zen.PokeRead` answers what the weaver sees.

MEANS
- every advertised field reads live; the mirror is written, never refreshed at a poke;
- the Texts are rebuilt when the BYTES move (`content_revision`), the undo steps when they move (`history_revision`), never on a gesture or a keystroke within a group;
- the room last composed for and the undo history, within the buffer's two bounds, ride too: an unchanged room after a reload is no resize.

DOES NOT MEAN
- that the redo steps, an operation in flight or the wheel fraction ride.

WHY — `agents/decisions/the-editor-is-the-custodian.md`

## Do not assume

- That an empty Editor leaves the keys to command mode — a runtime pane holds them from
  the press that pointed at it.

## Proven by

### WL-EDIT-01
`editor/editor-pane/editor.hpp` EditorState EditorBuffer kEditorUndoDepth kEditorUndoBudgetBytes
`editor/editor-pane/pane.cpp` EditorPaneWeave e_
`editor/editor-pane/vocabulary.hpp` kEditorPaneRole kEditorPane
`workshop/default-load-plan.json`
`tests/test_workshop_panes_editor.cpp` "the Editor is an ordinary arranged pane, offered by a
role" "the editor this host used to compile is named by no presentation source"

### WL-EDIT-02
`editor/editor-pane/editor.hpp` EditorBuffer kEditorVocabulary EditorBuffer::consume
`component/text_box.hpp` TextBox
`editor/editor-pane/pane.cpp` on(PaneKey)
`tests/test_editor.cpp` "the editor's declared vocabulary and consume agree, both directions" "undo
groups typing, treats joins and pastes as one edit, and redo returns" "set_lines wipes the history
-- undo cannot resurrect another document"

### WL-EDIT-03
`editor/editor-pane/pane.cpp` discard_source_edits close_source on(PaneQuitRequested)
`editor/editor-pane/opening_switch.hpp` judge_source
`editor/editor-pane/editor.hpp` EditorBuffer::revert_to EditorState
`workshop/weave_run.cpp` quit on(PaneQuitAnswered)
`tests/test_workshop_panes_editor.cpp` "removing and reopening the pane cannot lose a byte, a caret,
or a step of history" "a dirty buffer refuses a different source, and a save opens the way" "an
orderly quit refuses while source is unsaved, and proceeds once it is not" "a close bound in the
weaver's keymap refuses while a paste is still arriving, and lets the document go once it is saved"
`tests/test_workshop_editor_transfers.cpp` "Close file lets a clean document go, and refuses unsaved
work as the Editor's other exits do, leaving the document as it was"

### WL-EDIT-05
`editor/editor-pane/opening_switch.hpp` on(OpenSourceRequested) on(SourceOpened)
on(PrepareSourceRequested) on_claim_published judge_source activate
`editor/editor-pane/common.hpp` Candidate Relay
`editor/editor-pane/editor.hpp` source_in EditorState
`workshop/pane_seam_vocabulary.hpp` kEditorRole
`workshop/open_seam_vocabulary.hpp` OpenSourceRequested SourceOpened PrepareSourceRequested SourcePrepared
kOpeningRole
`tests/test_workshop_panes_editor.cpp` "a clipboard answer refuses the open wherever it lands, and A
keeps its paste" "the old door still opens and shows, or refuses truthfully, by a kept answer right"

### WL-EDIT-06
`editor/editor-pane/pane.cpp` project_dir_ project_known_ on(ProjectRoot)
`editor/editor-pane/opening_switch.hpp` resolve
`persist/file.hpp` resolved_against
`editor/editor-pane/editor.hpp` EditorState::path
`tests/test_workshop_panes_editor.cpp` "a relative path is the project's file, and means nothing
until the project has said" "re-requesting the open source reveals it and destroys nothing"

### WL-EDIT-07
`editor/editor-pane/editor.hpp` source_in source_text pasteable_source line_ending source_byte_ok
PasteableSource
`editor/editor-pane/pane.cpp` on(PaneTextInput)
`tests/test_editor.cpp` "source_in and source_text are inverse over everything admitted"
`tests/test_workshop_panes_editor.cpp` "a clipboard holding non-ASCII refuses the paste, and typed
non-ASCII is refused with a sentence"

### WL-EDIT-11
`editor/editor-pane/pane.cpp` begin_paste on(ClipboardText)
`editor/editor-pane/common.hpp` Paste
`editor/editor-pane/opening_switch.hpp` activate
`editor/editor-pane/editor.hpp` EditorState::doc_epoch EditorBuffer::revision
EditorBuffer::paste_lines EditorBuffer::set_lines
`tests/test_workshop_panes_editor.cpp` "a late paste answer may not land at a caret that has since
moved" "a paste retires with the document it was asked for" "a dirty document with no paste in
flight still refuses the exit"

### WL-EDIT-14
`workshop/weave_run.cpp` quit finish_quit on(PaneQuitAnswered) hold_input replay_held
`workshop/weave.hpp` HeldInput kMaxHeldInput quitting_
`pane/vocabulary.hpp` PaneQuitRequested PaneQuitAnswered
`editor/editor-pane/pane.cpp` on(PaneQuitRequested)
`editor/editor-pane/common.hpp` kPasteInFlight
`tests/test_workshop_panes_editor.cpp` "a Workshop with no custodian in the room quits at once" "a
forged quit answer moves nothing -- only Loom's answer to the host's ask decides" "an edit racing
the exit check is judged at the answer, and a refused quit costs no keystroke"

### WL-EDIT-15
`editor/editor-pane/vocabulary.hpp` EditorPaneState EditorPaneState::text_builds EditorUndoStep
`editor/editor-pane/pane.cpp` mirror_state revive restore_from_state saved_stamp_ mirrored_history_
`editor/editor-pane/editor.hpp` EditorBuffer::restore_selection EditorBuffer::content_revision
EditorBuffer::history_revision EditorBuffer::restore_history
`tests/test_workshop_panes_editor.cpp` "the mirror is rebuilt when the bytes move and at no other
time" "a reload in place carries the undo history, so an undo after it takes back an edit made
before it" "a carried undo history is put back within the buffer's two bounds: past either the
oldest steps go and the newest stays"
`tests/test_workshop_load.cpp` "an unchanged room after a reload is not a resize, and the view it
was scrolled to stands" "the reloaded pane reads live, not out of the snapshot it revived from"
