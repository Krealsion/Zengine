# Workshop law — what the Editor carries out and takes in

Register `WL-EDIT`, continued from [`editor.md`](editor.md): text, a command and a file location
carried between the standard Editor and a receiving pane by Workshop's typed carry. The shapes
are `editor/source-transfer/`'s; the carry and its approval are the protocol's
([`../panes.md`](../panes.md)); the Neovim-backed Editor's halves are
[`neovim-transfers.md`](neovim-transfers.md).
One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-EDIT-17 — Dropped text is one undoable edit where it landed

LAW — Dropped text is inserted at the painted landing character as one structural edit, replacing the selection only when it lands on the painted highlight; nothing is saved, built or run.

MEANS
- the byte law: tab and printable ASCII; LF or CRLF is one break, in the document's convention;
- one undo takes the drop back, caret and selection as they stood; the document is dirty;
- a refusal changes nothing, history included; one beside the rows is refused; an Info Text field inserts its text.

DOES NOT MEAN — that a drop types: no byte of it is a key, and nothing reaches the Terminal.

WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-EDIT-18 — A drop and a highlight press are judged against the rows they landed on

LAW — A drop and a highlight press are read against the row-to-meaning map the pane drew where they landed; a highlight press off the highlight is an ordinary one that remembers nothing.

MEANS
- the map is the epoch, the revision, the viewport, the chrome rows, the room, the selection;
- a caret move alone keeps the map; a scroll, an edit or a notice row moves it.

DOES NOT MEAN
- that a drop the window landed before the map moved is refused: which drawing a window press names is a decision held open, its witness held as an expected failure.

WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-EDIT-19 — A copy leaves only from an established selection, and the document stays

LAW — The selection's buffer text, unsaved edits included, leaves as an owned `SourceText` beside its observation, by a drag begun on the highlight, right-click Extract, or `ctrl+e`.

MEANS
- a press on the highlight is an ordinary press; its first motion restores the selection;
- a drag begun elsewhere sweeps; a right press off the highlight is handed back to the host;
- the carry is approved for the actor's gesture; a copy over the carrier's bound is refused.

DOES NOT MEAN — that the copy follows later edits, or that its observation is authority.

WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-EDIT-20 — A dropped command is text to edit, and C++ is a separate choice

LAW — A dropped command inserts its Terminal line, never sent; only in a C++ document does a separate menu choice insert a generated function that builds the value instead.

MEANS
- the fields present, in order, proved by Loom's lexer; a missing field stays missing, said;
- the destination is the capture's role or publish address, else the visible `<address>`;
- C++ lands as whole lines above the landing line, selected, one edit; a `.h` asks to be C++.

DOES NOT MEAN — that anything is sent, saved or built, or a default invented for a hole.

WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-EDIT-21 — A dropped location reopens through the managed opening, never around it

LAW — A dropped location opens its exact absolute path through the opening office once Workshop approves the dropping actor; the caret moves only in that unmoved document, on a line still as saved.

MEANS
- `ctrl+l` or the status row carries it; the unsaved-work floor stands, refused in words;
- another root's path opens as that path, said so; a missing one is refused, naming its root;
- a line saved whole must still read so, a capped one begin so; rebinding is editing in Info.

DOES NOT MEAN — that a location is text or a name: it is never inserted, and never rebound.

WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## Do not assume

- That the standard Editor holds what Neovim holds: its byte law refuses UTF-8 beyond ASCII,
  control bytes and a block, in words, where Neovim inserts them as data.
- That a drop waits for a switch: a pending choice or location open refuses the switch.

## Proven by

### WL-EDIT-17
`editor/editor-pane/editor.hpp` EditorBuffer::insert_at
`editor/editor-pane/transfers.hpp` receive insert_lines landing insertion_refusal
`editor/source-transfer/text.hpp` standard_lines
`tests/test_workshop_editor_transfers.cpp` "dropped text is inserted at the painted landing
character as one undoable edit, replaces the highlight only when dropped onto it, and saves nothing"
"text the standard Editor cannot hold is refused whole, and the document, its selection and its
history stay as they were"
`tests/test_source_transfer.cpp` "the standard Editor's law splits LF and CRLF, keeps tabs, and
refuses what it cannot hold whole" "a dropped pair is read as text, a location, a command with the
address its capture supplied, or refused in words"

### WL-EDIT-18
`editor/editor-pane/pane.cpp` say
`editor/editor-pane/transfers.hpp` press_at receive
`workshop/pane_carry.hpp` PaneCanvasValueDrop
`tests/test_workshop_editor_transfers.cpp` "a drop aimed at a picture the text has since left is
refused and changes nothing" "a press aimed at a picture where the text it lands on was not yet
selected begins no carry; the drag sweeps"

### WL-EDIT-19
`editor/editor-pane/transfers.hpp` press_at acquire mark_now on_highlight
`editor/source-transfer/material.hpp` text_pair kMaxCarryBytes
`editor/source-transfer/vocabulary.hpp` SourceText SourceSelection
`tests/test_workshop_editor_transfers.cpp` "a selection dragged from its highlight lands in a named
Inventory folder as an owned copy of the buffer's text, unsaved edits included, and nothing of the
document moves" "a press on the highlight that never moves is an ordinary press, and a drag begun
off the highlight sweeps and carries nothing, even across the pane's edge" "right-click on the
highlight offers Extract above the standard rows, which carries by pick-and-place; right-click off
it opens the standard pane menu" "the keyboard carries the selection by pick-and-place, and with
nothing selected it carries nothing in its place" "an actor without the carry cannot extract, and
one without the open may carry a location but not open it"
`tests/test_source_transfer.cpp` "a captured selection is an owned text item with its observation
beside it, and a copy the carrier cannot hold is refused, never cut"

### WL-EDIT-20
`editor/editor-pane/transfers.hpp` insert_command choose_drop insert_cpp
`editor/source-transfer/command_line.hpp` terminal_line kAddressPlaceholder
`editor/source-transfer/cpp.hpp` cpp_value_function cpp_document
`tests/test_workshop_editor_transfers.cpp` "a saved Terminal command dropped on a text document
becomes its editable Terminal line and is never sent; a preset's missing fields stay missing" "in a
C++ document a dropped command offers its Terminal line or generated C++; the C++ lands selected
with its includes named, and one undo removes it"
`tests/test_source_transfer.cpp` "a command becomes the Terminal's line: present fields in order,
text quoted, and Loom's lexer reads every field back exactly" "a value the Terminal grammar cannot
spell is refused whole, and a missing required field is left missing, never filled" "generated C++
is the golden the compiled witness builds: the value's own schema, escaped data, no send, and the
includes it lacks named" "generated C++ escapes every payload byte as data, leaves a preset's
missing required field as a labelled hole, and refuses a nested shape" "C++ generation is offered
for C++ documents by extension, and a .h is honestly ambiguous" "generated C++ keeps every byte of
every string: a NUL is written as a std::string literal, a name is escaped in code and in comments,
and the compiled witness builds the same value"

### WL-EDIT-21
`editor/editor-pane/transfers.hpp` open_location settle_location
`editor/editor-pane/opening_switch.hpp` handoff_refusal
`editor/source-transfer/material.hpp` location_pair observe_line still_reads
`workshop/guests.cpp` grant_for
`workshop/guests.hpp` kPowerOpen
`tests/test_workshop_editor_transfers.cpp` "a saved location reopens its file through the managed
opening at its line, and never over unsaved work" "a location whose saved line has since gained text
at its end opens its file and leaves the caret and the text alone, and one still as saved takes the
caret" "a location saved under another root opens that exact file and says so, and when it is gone
it is refused and never replaced by this root's same-named file" "an actor without the carry cannot
extract, and one without the open may carry a location but not open it"
`tests/test_source_transfer.cpp` "a location observes its caret line whole below the bound and as a
prefix at it, never half a character, and a whole line must still read exactly"
`tests/test_workshop_guests.cpp` "open power reaches only the managed opening, and no other power
opens a source"
