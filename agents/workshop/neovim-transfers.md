# Workshop law — what the Neovim-backed Editor carries out and takes in

Register `WL-NVIM`, continued from [`neovim.md`](neovim.md): the Neovim-backed Editor in
Workshop's typed carry -- a selection out, material in as data, a location back through the
managed opening -- and a change Neovim holds. The shapes are `source-transfer/`'s; the standard
Editor's halves are [`editor-transfers.md`](editor-transfers.md). One law per heading; cite by
ID. Router: [`../workshop.md`](../workshop.md).

## WL-NVIM-10 — Neovim's selection leaves as its own yank takes it

LAW — The Visual or Select selection leaves as an owned `SourceText` equal to Neovim's own yank, unsaved edits included, by a drag begun on the highlight, right-click Extract, or `ctrl+r`.

MEANS
- a highlight press is Neovim's click; its first motion restores the selection with `gv`;
- `ctrl+r` is declared only in Visual or Select mode, where Neovim gives it no meaning;
- a right press on the highlight offers Extract and Neovim's own menu; off it, Neovim's.

DOES NOT MEAN — that the copy is Neovim's register: no register, mark or mode is moved.

PROVEN BY — `neovim-editor/pane.cpp` `press_at`, `carry_snapshot`, `snapshot_now`,
`carries_selection`, `declare`; `neovim/lua.hpp` `kSelection`;
`tests/test_workshop_neovim_transfers.cpp` case `"a Visual selection dragged from its highlight
lands in a named Inventory folder as exactly what Neovim's yank takes, unsaved edits included,
and Neovim keeps its selection and its buffer"`, case `"ctrl+r carries a linewise selection as
lines and a block as a block; in Insert mode ctrl+r stays Neovim's, and ctrl+k stays the
desktop's"`, case
`"right-click on the Visual highlight offers Extract and Neovim's own menu; off the highlight
the right press is Neovim's alone"`, case `"a press on the Visual highlight that never moves is
Neovim's own click, and a drag begun off the highlight is Neovim's own sweep and carries
nothing"`; `tests/test_neovim_live.cpp` case `"the selection taken for a copy is exactly what
Neovim's own yank takes, for every kind of Visual selection, and taking it moves nothing"`.
WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-NVIM-11 — What is dropped on Neovim is data, where it was aimed

LAW — A drop is inserted by `nvim_buf_set_text` as one undo block at the aimed cell of the picture it names, in the buffer and changedtick it saw, in Normal or Insert mode or onto the highlight.

MEANS
- the picture fingerprints the rows, caret, range and generation said; a moved screen refuses;
- any UTF-8 but NUL is data: Escape, key notation and a bare CR stay characters;
- a block, another mode, a read-only buffer or a pending command refuses in Neovim's words.

DOES NOT MEAN — that a `cpp` buffer takes C++ unasked: a command is its line, C++ whole lines.

PROVEN BY — `neovim-editor/pane.cpp` `receive`, `insert_lines`, `say`, `fresh`,
`picture_hash`, `insert_cpp`; `neovim/lua.hpp` `kDrop`;
`tests/test_workshop_neovim_transfers.cpp` case `"dropped text lands in Neovim as data where the
hand aimed, as one undo step, replaces the Visual highlight only when dropped onto it, and
writes nothing"`, case `"a drop aimed at a picture Neovim has since redrawn is refused and
changes nothing"`, case `"a drop into a mode Neovim is still in the middle of is refused in
Neovim's words and changes nothing"`, case `"a saved command dropped on a text buffer becomes
its Terminal line and is never sent; in a cpp buffer a choice offers generated C++, which one
undo removes, and a pending choice refuses a switch"`; `tests/test_neovim_live.cpp` case `"a drop
in Insert mode is its own undo step and leaves Insert mode as it was; a stale buffer or screen
refuses it"`, case `"a drop into a read-only or unmodifiable buffer, or while Neovim waits for
the rest of a command, is refused and changes nothing"`.
WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-NVIM-12 — A location leaves from the status row and returns through the managed opening

LAW — The file's location leaves from the status row's drag or menu, or a key the maker binds; a dropped one opens through the managed opening once Workshop approves the gesture; a missing file refuses.

MEANS
- `neovim.location` has no default key: every plain ctrl+letter is Neovim's or the desktop's;
- Neovim keeps a modified buffer hidden beside the one it opens: unsaved work is kept;
- the cursor moves only in that buffer as shown, on a line still as saved: whole, or its start.

DOES NOT MEAN — that a capped line proves the whole line: past the observation it is unread.

PROVEN BY — `neovim-editor/pane.cpp` `acquire_location`, `open_location`, `settle_location`;
`declare`; `neovim/lua.hpp` `kLocate`; `source-transfer/material.hpp` `observe_line`,
`whole_line`; `tests/test_workshop_neovim_transfers.cpp` case `"the
status row carries this file's location, which reopens the file through the managed opening at
its line; Neovim's unsaved buffer is kept, and a changed line or a missing file is refused in
words"`, case `"a location's cursor Neovim holds lands once Neovim stops waiting, only on a line
still as saved: a line that gained text at its end declines it"`, case `"a location carried from
a long line keeps whole characters where the bound cuts it, is stored, and reopens its file at
that line by the line's saved beginning"`; `tests/test_neovim_live.cpp` case `"a location's
cursor is placed only in the buffer it names, unchanged, on a line that still reads as it did"`,
case `"a location's saved line must still read exactly while it was whole, and begin the line
once the bound cut it; no saved line checks nothing, and a refusal moves nothing"`.
WHY — `agents/decisions/editors-carry-copies-by-the-typed-carry.md`

## WL-NVIM-13 — A change Neovim holds is the Editor's until Neovim answers it

LAW — A drop or a location's cursor Neovim leaves unanswered is held, never refused: sent once with its whole target, it is said once when Neovim runs it or ends, and until then nothing else changes.

MEANS
- one at a time, named on the status row; another drop, an open and a switch refuse meanwhile;
- Neovim checks the buffer, changedtick, row or line again when it runs it: none retargets;
- keys still reach Neovim; if Neovim ends first the change ends with it, and nothing is written.

DOES NOT MEAN — that every unanswered request is held: an adoption, an open's preparation and
its showing are not.

PROVEN BY — `neovim/host.hpp` `Host::ask`, `Host::Asked`, `Host::call_now`;
`neovim-editor/pane.cpp` `change`, `settle_held`, `heard_of`, `insert_lines`, `settle_location`,
`judge_now`, `status_text`; `neovim/lua.hpp` `kDrop`, `kLocate`; `tests/test_neovim_live.cpp`
case `"a change asked while Neovim waits for input is outstanding, not refused: Neovim runs it
when the wait ends, after the keys typed with its end, and its answer arrives exactly once"`,
case `"a change Neovim holds when it ends is answered exactly once, with the ending as its
error"`; `tests/test_workshop_neovim_transfers.cpp` case `"a command chosen from the drop's menu
while Neovim waits for input is held, never refused: typing still reaches Neovim, another drop,
an open and a switch wait for it, and it goes in once Neovim stops waiting, said once, one undo
taking it back"`, case `"a drop Neovim holds finds its buffer changed when it runs and is refused
then, said once, with the change alone in the buffer"`, case `"a drop Neovim holds when Neovim
is stopped ends with it, said once: nothing was written, and the Editor holds no document"`,
case `"a location's cursor Neovim holds lands once Neovim stops waiting, only on a line still as
saved: a line that gained text at its end declines it"`.
WHY — `agents/decisions/a-held-change-is-owned-until-answered.md`

## Do not assume

- That the gated cases ran on a lane with no Neovim: they are behind the `neovim` gate.
- That a timeout's words are a refusal: a request Neovim was sent still runs when its wait ends.
