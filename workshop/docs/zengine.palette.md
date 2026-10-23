# zengine.palette -- the Command Palette

**Manual.** The palette lists everything you can do by name -- every action in force and every
pane -- and runs the one you choose. It is the weave in the office `zengine.palette`, built as
`zengine-palette` and loaded by a row of the shipped plans that load a window. Open it from
anywhere with <!-- key desktop.palette -->`ctrl+space`<!-- /key -->, the desktop's
[`desktop.palette`](zengine.desktop.md#desktoppalette); in the OS window by tapping Shift twice,
<!-- key desktop.palette-shift -->`double-shift`<!-- /key -->; or as any pane from the Pane
Manager.

## About

The palette keeps no list of its own. It reads what Workshop says to any presenter: the keymap in
force ([`KeymapShown`](../desktop_seam_vocabulary.hpp)) and the pane inventory
([`PaneInventory`](../desktop_seam_vocabulary.hpp)). An entry is an action -- Workshop's own
rows reachable from command mode or above every mode, the application's rows above every mode, and
each open pane's rows in the mode it is in -- or a pane, open or not. The text box's own keys, a
mode you are in only while it is open (the context actions, arranging, naming a layout), and the
palette's own rows are not entries.

It finds; it grants nothing. A chosen entry is asked of Workshop
([`CommandRunRequested`](zengine.workshop.md#commandrunrequested)), which performs it by the path
its key or press takes, as your hand, and judges it where that path is judged: a pane only the
weaver's hand reaches is refused to a guest's run as to a guest's key. A row bound to no key is
run all the same, as one you could bind and press. A row you switched off in your keymap file
(`none`) is listed `off` and runs nothing.

What you chose this run comes first next time; that history is the weave's state, kept across a
reload in place and never written. The filter, the cursor and a run in flight belong to one
opening.

## Use

Type to filter: an entry matches when each word you type is found in its label, whose it is, or
its id. Entries you chose earlier come first, the most recent first; when a word names a pane, the
panes come before the actions. Each line ends with the entry's key as your keymap spells it, `-`
for none, `off` for one switched off, `+1` beside an action with a second key, and `[gone]` for a
pane nothing offers now.

Move with [`palette.up`](#paletteup) and [`palette.down`](#palettedown), or press a line; run it
with [`palette.run`](#paletterun). The palette is put down as the entry runs, and the keys go back
to the pane they were in when it opened. [`palette.close`](#paletteclose) puts it down with nothing
run. A press on a line moves the cursor there and runs nothing: a press in the window is not a key
Workshop numbered.

The palette holds at most <!-- value kMaxEntries -->1024<!-- /value --> entries and says how many
it left out past that; the filter holds at most <!-- value kMaxFilterBytes -->64<!-- /value -->
characters, and the run remembers <!-- value kMaxHistory -->32<!-- /value --> choices.

![The palette open below Flow, filtered by `apply` to Flow's row with no key](images/palette-apply.png)

## Commands

### `palette.up`

Moves the cursor to the entry above.

- **Takes:** the list as filtered. Default key <!-- key palette.up -->`up`<!-- /key -->.
- **Answers:** the cursor one entry up, stopping at the first.
- **Refuses:** nothing.
- **Example:** type `term`, press <!-- key palette.up -->`up`<!-- /key -->.

### `palette.down`

Moves the cursor to the entry below.

- **Takes:** the list as filtered. Default key <!-- key palette.down -->`down`<!-- /key -->.
- **Answers:** the cursor one entry down, stopping at the last.
- **Refuses:** nothing.
- **Example:** press <!-- key palette.down -->`down`<!-- /key --> twice.

### `palette.run`

Runs the entry under the cursor.

- **Takes:** the entry under the cursor. Default key <!-- key palette.run -->`return`<!-- /key -->.
- **Answers:** asks Workshop to run it
  ([`CommandRunRequested`](zengine.workshop.md#commandrunrequested)) under this key's number;
  Workshop puts the palette down and performs the entry -- an action by its key's path, a pane
  shown and focused -- and says on the band what it did or why not.
- **Refuses:** `nothing matches the filter`; `switched off in your keymap -- <id> is none`; `<pane>
  is not here now: nothing offers it`; `the last entry chosen is still being run`; Workshop's
  refusal, as it says it.
- **Example:** type `flow run`, press <!-- key palette.run -->`return`<!-- /key -->.

### `palette.close`

Puts the palette down with nothing run.

- **Takes:** nothing. Default key <!-- key palette.close -->`escape`<!-- /key -->.
- **Answers:** asks Workshop to put it down
  ([`CommandRunRequested`](zengine.workshop.md#commandrunrequested)); the keys go back to the pane
  they were in when it opened.
- **Refuses:** `the last entry chosen is still being run`.
- **Example:** press <!-- key palette.close -->`escape`<!-- /key -->.

## Hears

### `KeymapShown`

[`KeymapShown`](../desktop_seam_vocabulary.hpp), from Workshop: the keymap in force, which the
action entries are read from again. Asked when the palette starts and each time it opens.

### `PaneInventory`

[`PaneInventory`](../desktop_seam_vocabulary.hpp), from Workshop: the panes, which the pane
entries are read from again, and which panes are open, whose rows are listed.

### `CommandRunAnswered`

[`CommandRunAnswered`](../command_vocabulary.hpp), Workshop's word on a run: the
entry remembered when it ran, its refusal said when not.

### `AreaRoom`

`AreaRoom`, from the graphics window that shows the area `zengine.palette`
([`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp)): its room there, where it draws the
filter, the count and the entries.

### `AreaButtonChanged`

`AreaButtonChanged`, from that window: a press on an entry's line, which moves the cursor there.

## Shows

### `palette`

The filter `find: ` with its caret, the count of entries shown of those listed (or the last
notice), then one line an entry: the marker `> ` at the cursor, the label, whose it is, and its key
at the line's end.

## See also

- [Hotkeys](hotkeys.md): the keymap the action entries are read from, and how to move a key.
- [The desktop's manual](zengine.desktop.md): the application rows, `desktop.palette` among them.
