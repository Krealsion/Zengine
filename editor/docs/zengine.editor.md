# zengine.editor -- the Editor

**Manual.** The pane you edit a source file in: it opens one file, edits it, and saves exactly what
you meant to save, so the next build reads it. It is a *weave*: a piece of Workshop that is loaded,
replaced and spoken to by messages. Other weaves reach it by its *office*, the name it answers to,
`zengine.editor`. Two weaves can hold that office, one at a time: the standard Editor,
`zengine-editor-pane`, and the Neovim-backed Editor, `zengine-neovim-editor`. This page is both
holders' manual. New to it? Start with [The source editor](editor.md) or
[Neovim in Workshop](neovim.md).

## About

The office has one pane, `editor`, and whichever weave holds the office draws it. The load plan
names the weave that starts and the choices a weaver may switch to, `standard` and `neovim` in
both shipped plans; the switch is the office [`zengine.editor-switch`](zengine.editor-switch.md),
which moves the document across: its bytes, unsaved edits, caret, selection and scroll. The pane's
title says which holder you are in, `Editor @zengine.editor` or `Neovim @zengine.editor`.

The document is the holder's own. Its file's bytes, your unsaved edits, the caret, the selection,
the undo history and the scroll live in the weave, and Workshop holds none of them. Closing,
covering or moving the pane changes nothing in the document. The standard Editor holds the
document itself; the Neovim-backed Editor starts a Neovim and holds the document in it, so every
key it does not declare is Neovim's own.

A file opens through Workshop's one opening, whoever asks: the Files pane, the Builder's
`builder.edit-source`, a pane's `edit code`, or a dropped file location. The document you have is
replaced only once the new one is read, judged and seated, and never while it holds unsaved edits.

Both holders carry copies out to Inventory or another pane, and take text, commands and file
locations in; nothing dropped is run, sent, saved or built. What each carried value holds is
[source material](source-transfer.md).

## Use

Open a file from the Files pane, or choose a recipe in the Builder and edit its source; the Editor
appears with your keys in it. Press into the pane to type; a press anywhere else gives your keys
back to Workshop.

In the standard Editor, typing edits the source and the ordinary editing keys work across lines:
moving, selecting, erasing, copy, cut, paste, select all, undo and redo. Those keys are the
buffer's own (`kEditorVocabulary`, `editor/editor-pane/editor.hpp`) and are not remappable; the
Hotkeys pane, <!-- key desktop.hotkeys -->`ctrl+k`<!-- /key -->, lists the six rows below under
the Editor's heading and describes the rest as the pane's own. The mouse places the caret, a drag
sweeps a selection, and the wheel scrolls without moving the caret. Save with [`editor.save`](#editorsave) before you build: an unsaved document is
not built.

In the Neovim-backed Editor, every key is Neovim's but the rows under
[Commands in Neovim](#commands-in-neovim): its modes, motions, `:` commands, search, undo and
macros. A press places Neovim's cursor, a drag selects, the wheel scrolls, and a right press is
Neovim's own except on its Visual highlight or the `status` row.

To switch holders, ask the switch from Workshop's Terminal,
<!-- key desktop.terminal -->`ctrl+t`<!-- /key -->:
`ask @zengine.editor-switch EditorSwitchRequested 1 destination=neovim`, or `destination=standard`
([`EditorSwitchRequested`](zengine.editor-switch.md#editorswitchrequested)).

## Commands

The standard Editor's declared rows. Each moves in your keymap file like any pane's row, and acts
only while the Editor holds your keys.

### `editor.save`

Writes the document to its file: the bytes you edited, in the file's own line endings, and nothing
else.

- **Takes:** the open document. Default key <!-- key editor.save -->`ctrl+s`<!-- /key -->.
- **Answers:** the file is written whole through `<file>.saving`, which then replaces it, so a
  failed write leaves the file as it was. The notice says `saved -- <the file>`, and the `status`
  row reads `saved`.
- **Refuses:** `no source is open -- nothing was saved`; a write the system refuses, in its words,
  as `cannot write <the file>.saving` or `` cannot replace <the file>: <the reason> -- it is
  unchanged ``, the document and its `UNSAVED` left as they were.
- **Example:** edit `tally.cpp`, press <!-- key editor.save -->`ctrl+s`<!-- /key -->, then
  <!-- key builder.build -->`b`<!-- /key --> in the Builder.

### `editor.newline`

Splits the caret's line: a line break in the file's own convention, LF or CRLF.

- **Takes:** the caret. Default key <!-- key editor.newline -->`return`<!-- /key -->.
- **Answers:** the document gains a line; the `status` row reads `UNSAVED`.
- **Refuses:** nothing; with no source open it does nothing.
- **Example:** press <!-- key editor.newline -->`return`<!-- /key --> at the end of a line.

### `editor.tab`

Inserts one tab byte, kept as a tab in the file. It is drawn to a fixed tab stop
(`kEditorTabStop`, `editor/editor-pane/editor.hpp`), and the caret, a press and a selection count
it as the columns it is drawn across.

- **Takes:** the caret. Default key <!-- key editor.tab -->`tab`<!-- /key -->.
- **Answers:** one tab byte at the caret; the `status` row reads `UNSAVED`.
- **Refuses:** nothing; with no source open it does nothing.
- **Example:** press <!-- key editor.tab -->`tab`<!-- /key --> at the start of a line.

### `editor.discard`

Puts the document back to its last saved state, deliberately, as one edit that undo takes back.

- **Takes:** the open document. Default key <!-- key editor.discard -->`ctrl+d`<!-- /key -->.
- **Answers:** `discarded unsaved edits; undo takes them back -- <the file> is back to its last
  saved state`; the `status` row reads `saved`.
- **Refuses:** `no source is open -- nothing to discard`; `the source matches its last saved state
  -- nothing to discard`.
- **Example:** after edits you do not want, press <!-- key editor.discard -->`ctrl+d`<!-- /key -->.

### `editor.extract`

Picks up a copy of the selection, to put down on a receiving pane: an Inventory folder, or the
list. A drag that starts inside the highlight, or a right press on it, **Extract selection to
Inventory**, does the same.

- **Takes:** the selection as it stands, unsaved edits included. Default key
  <!-- key editor.extract -->`ctrl+e`<!-- /key -->.
- **Answers:** Workshop is asked to approve the carry for your press, then carries the copy; the
  notice says `carrying a copy of <how much> of <the file> -- click a receiving pane, or Escape;
  this document is unchanged`. A click on a receiving pane puts it down;
  <!-- key desktop.deselect -->`escape`<!-- /key --> puts it away.
- **Refuses:** each beginning `nothing was carried -- `: `select text first; nothing else is carried
  in its place`; `no source is open`; `the selection changed after you pointed at it`; `a copy is
  already on its way to Workshop`; the copy's own refusal past the carry's bound (`kMaxCarryBytes`,
  `workshop/pane_carry.hpp`), never cut; and Workshop's reason when it does not approve.
- **Example:** select a function, press <!-- key editor.extract -->`ctrl+e`<!-- /key -->, click
  Inventory's Snippets folder.

### `editor.location`

Picks up this file's location: its path and the caret's line and column, with the project root and
the caret line's text beside them. A drag that starts on the `status` row, or a right press on it,
**Carry this file's location**, does the same.

- **Takes:** the open file and the caret. Default key
  <!-- key editor.location -->`ctrl+l`<!-- /key -->.
- **Answers:** as [`editor.extract`](#editorextract), the notice naming `the location of <the file>
  at line <n>`. Dropped on an Editor later, the location opens its file and, where the saved line
  still reads as it did, moves the caret to it.
- **Refuses:** as [`editor.extract`](#editorextract): `nothing was carried -- no source is open`,
  `nothing was carried -- the document changed after you pointed at it`, and Workshop's reason.
- **Example:** press <!-- key editor.location -->`ctrl+l`<!-- /key -->, click an Inventory folder.

## Commands in Neovim

The Neovim-backed Editor's declared rows, while it holds the office. Every other key reaches
Neovim.

### `neovim.write`

Writes Neovim's current buffer, `:write`, without leaving the mode you are in.

- **Takes:** Neovim's current buffer. Default key <!-- key neovim.write -->`ctrl+s`<!-- /key -->.
- **Answers:** Neovim writes the file and says so on its own screen; the `status` row reads
  `saved`.
- **Refuses:** `Neovim is not running -- open a source to start it`, with why it is not running
  when it knows; a write Neovim refuses is said by Neovim, on its screen.
- **Example:** in Insert mode, press <!-- key neovim.write -->`ctrl+s`<!-- /key --> and keep typing.

### `neovim.jump-older`

Neovim's own jump back, `<C-o>`; from Insert mode, one Normal command.

- **Takes:** Neovim's jumplist. Default key <!-- key neovim.jump-older -->`ctrl+o`<!-- /key -->.
- **Answers:** Neovim moves its cursor as `<C-o>` does.
- **Refuses:** `Neovim is not running -- open a source to start it`.
- **Example:** after a search, press <!-- key neovim.jump-older -->`ctrl+o`<!-- /key --> to go back.

### `neovim.location`

Picks up this file's location at Neovim's cursor, as [`editor.location`](#editorlocation) does. It
has no default key: in Normal mode every plain control letter is Neovim's or the desktop's, so bind
it in your keymap file; a drag from the `status` row, or a right press there, **Carry this file's
location**, carries it meanwhile.

- **Takes:** Neovim's current buffer and cursor, asked of Neovim when you press. No default key.
- **Answers:** as [`editor.extract`](#editorextract): the notice says `carrying a copy of the
  location of <the file> at line <n> -- click a receiving pane, or Escape; Neovim's buffer is
  unchanged`.
- **Refuses:** each beginning `nothing was carried -- `: `Neovim's current buffer names no file`;
  `Neovim could not say where its cursor is (<why>)`; `a copy is already on its way to Workshop`;
  and Workshop's reason.
- **Example:** with the row bound in your keymap file, press it and click an Inventory folder.

### `neovim.extract`

Picks up a copy of the Visual or Select selection, as Neovim's own yank takes it: characterwise,
linewise or a block. It is declared only while a selection stands; in every other mode its key is
Neovim's own (redo, or Insert's register).

- **Takes:** the selection Neovim holds when you press, unsaved changes included. Default key
  <!-- key neovim.extract -->`ctrl+r`<!-- /key -->.
- **Answers:** as [`editor.extract`](#editorextract), the notice ending `Neovim's buffer is
  unchanged`. A drag from inside the highlight, or a right press on it, **Extract selection to
  Inventory**, does the same.
- **Refuses:** `nothing was carried -- ` and Neovim's reason, such as a selection holding a NUL
  byte; `nothing was carried -- a copy is already on its way to Workshop`; and Workshop's reason.
  When Neovim has left Visual mode since the row was declared, the key reaches Neovim instead.
- **Example:** `viw` on a word, press <!-- key neovim.extract -->`ctrl+r`<!-- /key -->, click an
  Inventory folder.

## Hears

### `OpenSourceRequested`

`OpenSourceRequested` v1 ([`workshop/pane_seam_vocabulary.hpp`](../../workshop/pane_seam_vocabulary.hpp)):
open this file. The holder hands it to the opening office, `zengine.opening`, and answers with what
became of it. The standard Editor refuses
`the Editor holds unsaved changes to <path> -- save source or discard source edits in the Editor
first; nothing was opened`; Neovim opens the file in a new buffer beside any unsaved one.

### `PaneQuitRequested`

`PaneQuitRequested` v1, Workshop's orderly quit: may Workshop end now? The standard Editor refuses
`the Editor holds unsaved changes to <path> -- save source or discard source edits in the Editor
first; Workshop stays open`. Neovim refuses `Neovim holds unsaved changes to <files> -- write them
(:w) or discard them in Neovim first; Workshop stays open`, or a running terminal job. Both refuse
while a switch holds them still.

### `NeovimStartRequested`

`NeovimStartRequested` v1 ([`editor/neovim-editor/vocabulary.hpp`](../neovim-editor/vocabulary.hpp)),
for a Loom with no Workshop: start Neovim headless and listening, at `listen` or an address the
weave makes, editing `path` when named. Answered `zen.Result`, `<Neovim> is listening at <address>
... -- attach its interface with: nvim --server <address> --remote-ui`, or `zen.Refused`, as
`Neovim is already running (...) -- stop it first with NeovimStopRequested`.

### `NeovimStatusRequested`

`NeovimStatusRequested` v1: what runs and what it holds, asked of Neovim now. Answered
`zen.Result`: `<Neovim> (<how it is attached>) is editing <file>, saved, in mode n`, or `Neovim is
not running` and why.

### `NeovimStopRequested`

`NeovimStopRequested` v1: end the Neovim this weave runs. Refused while a buffer holds unsaved
changes, `Neovim holds unsaved changes to <files> -- write them first, or stop with discard=true to
lose them`, unless `discard` is true; answered `Neovim ended (status 0)`.

## Follow

The Editor publishes nothing of its own; its pane is its account. A probe reads the holder's
declared fields with `zen.PokeRead`: the standard Editor's `EditorPaneState`
([`editor/editor-pane/vocabulary.hpp`](../editor-pane/vocabulary.hpp)), the document whole, with
`path`, `caret_row` and `notice`; the Neovim-backed Editor's `NeovimEditorState`
([`editor/neovim-editor/vocabulary.hpp`](../neovim-editor/vocabulary.hpp)), with `running`,
`ready`, `profile`, `path`, `modified` and `mode`, written from what Neovim last said. A switch's
own account is the switch's ([Follow](zengine.editor-switch.md#follow)).

## Shows

### `status`

The pane's first row. The standard Editor's: `UNSAVED` or `saved`, the caret's line and column of
the lines, and the file, cut from its front when narrow, `saved L12:C5/40 -- <the file>`; empty,
`no source open -- Return on a file in Files, or e in the Builder`. Neovim's: `saved NORMAL clean --
<the file>`, its mode and profile; `Neovim is not running -- open a source to start it`.

### `line`

The standard Editor's document rows, each `line:<n>` by its line number. A notice stands on its
own row under `status` until your next act, or in its place in a room too small for both.

## Files

- **The source.** The standard Editor reads and writes one file, a path the asker names: absolute
  as it is, relative against the project, the directory Workshop was launched in, and refused
  until the project has said where that is. It holds tab and printable ASCII, one line convention,
  and up to `kMaxSourceBytes` (`editor/editor-pane/editor.hpp`); a file outside that is refused
  whole and left untouched, naming the line: `the file mixes CRLF and LF line endings -- ...`,
  `line 3 holds a byte outside plain ASCII (0xc3) -- this editor cannot edit it truthfully, and the
  file is untouched`. Whether the file ends in a newline is kept.
- **Neovim's files.** The Neovim-backed Editor's Neovim reads and writes its buffers itself. The
  program is `ZENGINE_NEOVIM`, or `nvim` on the search path; its configuration is
  `ZENGINE_NEOVIM_PROFILE`: `clean` (no configuration, its state kept apart under the name
  `zengine-neovim-clean`), `user`, or an init file. A value that names neither and no file refuses
  the start: `` ZENGINE_NEOVIM_PROFILE is `User`, which is neither `clean` nor `user` and names no
  init file (looked for <path>) -- Neovim was not started ``.

## See also

- [The source editor](editor.md) and [Neovim in Workshop](neovim.md): editing, carrying and
  switching, walked through.
- [`zengine.editor-switch`](zengine.editor-switch.md), the switch, and
  [what crosses a switch](editor-switch.md).
- [Source material](source-transfer.md): what a carried selection, command or location holds.
