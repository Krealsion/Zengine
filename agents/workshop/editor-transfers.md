# Workshop law — what the Editor carries out and takes in

Register `WL-EDIT`, continued from [`editor.md`](editor.md): text, a command and a file location
carried between the standard Editor and a receiving pane by Workshop's typed carry. The shapes
are `source-transfer/`'s; the carry and its approval are the protocol's
([`../panes.md`](../panes.md)); the Neovim-backed Editor's halves are [`neovim.md`](neovim.md).
One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-EDIT-17 — Dropped text is one undoable edit where it landed

LAW — Dropped text is inserted at the painted landing character as one structural edit, replacing the selection only when it lands on the painted highlight; nothing is saved, built or run.

MEANS
- the byte law: tab and printable ASCII; LF or CRLF is one break, in the document's convention;
- one undo takes the drop back, caret and selection as they stood; the document is dirty;
- a refusal changes nothing, history included; an Info Text field inserts its text.

DOES NOT MEAN — that a drop types: no byte of it is a key, and nothing reaches the Terminal.

PROVEN BY — `editor-pane/editor.hpp` `EditorBuffer::insert_at`; `editor-pane/pane.cpp`
`receive`, `insert_lines`, `landing`, `insertion_refusal`; `source-transfer/text.hpp`
`standard_lines`; `tests/test_workshop_editor_transfers.cpp` case `"dropped text is inserted at
the painted landing character as one undoable edit, replaces the highlight only when dropped
onto it, and saves nothing"`, case `"text the standard Editor cannot hold is refused whole, and
the document, its selection and its history stay as they were"`;
`tests/test_source_transfer.cpp` case `"the standard Editor's law splits LF and CRLF, keeps
tabs, and refuses what it cannot hold whole"`, case `"a dropped pair is read as text, a
location, a command with the address its capture supplied, or refused in words"`.
WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-EDIT-18 — A drop and a highlight press are judged against the picture they aimed at

LAW — The picture number moves exactly when the row-to-meaning map does; a drop naming another picture is refused, and a press naming one is an ordinary press that remembers nothing.

MEANS
- the map is the epoch, the revision, the viewport, the chrome rows, the room, the selection;
- a caret move alone keeps the picture; a scroll, an edit or a notice row moves it.

PROVEN BY — `editor-pane/pane.cpp` `say`, `PictureKey`, `press_at`, `receive`;
`workshop/pane_carry.hpp` `PaneValueDrop`; `tests/test_workshop_editor_transfers.cpp` case `"a
drop aimed at a picture the text has since left is refused and changes nothing"`.
WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-EDIT-19 — A copy leaves only from an established selection, and the document stays

LAW — The selection's buffer text, unsaved edits included, leaves as an owned `SourceText` beside its observation, by a drag begun on the highlight, right-click Extract, or `ctrl+e`.

MEANS
- a press on the highlight is an ordinary press; its first motion restores the selection;
- a drag begun elsewhere sweeps; a right press off the highlight offers nothing;
- the carry is approved for the actor's gesture; a copy over the carrier's bound is refused.

DOES NOT MEAN — that the copy follows later edits, or that its observation is authority.

PROVEN BY — `editor-pane/pane.cpp` `press_at`, `acquire`, `mark_now`, `on_highlight`;
`source-transfer/material.hpp` `text_pair`, `kMaxCarryBytes`;
`source-transfer/vocabulary.hpp` `SourceText`, `SourceSelection`;
`tests/test_workshop_editor_transfers.cpp` case `"a selection dragged from its highlight lands in
a named Inventory folder as an owned copy of the buffer's text, unsaved edits included, and
nothing of the document moves"`, case `"a press on the highlight that never moves is an
ordinary press, and a drag begun off the highlight sweeps and carries nothing, even across the
pane's edge"`, case `"right-click on the highlight offers Extract, which carries by
pick-and-place; right-click off it offers nothing"`, case `"the keyboard carries the selection by
pick-and-place, and with nothing selected it carries nothing in its place"`, case `"an actor
without the carry cannot extract, and one without the open may carry a location but not open
it"`; `tests/test_source_transfer.cpp` case `"a captured selection is an owned text item with
its observation beside it, and a copy the carrier cannot hold is refused, never cut"`.
WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-EDIT-20 — A dropped command is text to edit, and C++ is a separate choice

LAW — A dropped command inserts its Terminal line, never sent; only in a C++ document does a separate menu choice insert a generated function that builds the value instead.

MEANS
- the fields present, in order, proved by Loom's lexer; a missing field stays missing, said;
- the destination is the capture's role or publish address, else the visible `<address>`;
- C++ lands as whole lines above the landing line, selected, one edit; a `.h` asks to be C++.

DOES NOT MEAN — that anything is sent, saved or built, or a default invented for a hole.

PROVEN BY — `editor-pane/pane.cpp` `insert_command`, `choose_drop`, `insert_cpp`;
`source-transfer/command_line.hpp` `terminal_line`, `kAddressPlaceholder`;
`source-transfer/cpp.hpp` `cpp_value_function`, `cpp_document`;
`tests/test_workshop_editor_transfers.cpp` case `"a saved Terminal command dropped on a text
document becomes its editable Terminal line and is never sent; a preset's missing fields stay
missing"`, case `"in a C++ document a dropped command offers its Terminal line or generated
C++; the C++ lands selected with its includes named, and one undo removes it"`;
`tests/test_source_transfer.cpp` case `"a command becomes the Terminal's line: present fields
in order, text quoted, and Loom's lexer reads every field back exactly"`, case `"a value the
Terminal grammar cannot spell is refused whole, and a missing required field is left missing,
never filled"`, case `"generated C++ is the golden the compiled witness builds: the value's own
schema, escaped data, no send, and the includes it lacks named"`, case `"generated C++ escapes
every payload byte as data, leaves a preset's missing required field as a labelled hole, and
refuses a nested shape"`, case `"C++ generation is offered for C++ documents by extension, and
a .h is honestly ambiguous"`.
WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-EDIT-21 — A dropped location reopens through the managed opening, never around it

LAW — A dropped location opens its exact absolute path through the opening office once Workshop approves the dropping actor; the caret moves only in that unmoved document, on a line still as saved.

MEANS
- `ctrl+l` or the status row carries it; the unsaved-work floor stands, refused in words;
- a missing file, or one under another root, is refused naming the root it was saved under;
- rebinding is editing the path in Info; a guest's gesture needs its `open` power.

DOES NOT MEAN — that a location is text: it is never inserted, and never follows a name.

PROVEN BY — `editor-pane/pane.cpp` `open_location`, `settle_location`, `handoff_refusal`;
`source-transfer/material.hpp` `location_pair`; `workshop/guests.cpp` `grant_for`;
`workshop/guests.hpp` `kPowerOpen`; `tests/test_workshop_editor_transfers.cpp` case `"a saved
location reopens its file through the managed opening at its line, and never over unsaved
work"`, case `"a location saved under another root opens that exact file and says so, and when
it is gone it is refused and never replaced by this root's same-named file"`, case `"an actor
without the carry cannot extract, and one without the open may carry a location but not open
it"`; `tests/test_workshop_guests.cpp` case `"open power reaches only the managed opening, and
no other power opens a source"`.
WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## Do not assume

- That the standard Editor holds what Neovim holds: its byte law refuses UTF-8 beyond ASCII,
  control bytes and a block, in words, where Neovim inserts them as data.
- That a drop waits for a switch: a pending choice or location open refuses the switch.
