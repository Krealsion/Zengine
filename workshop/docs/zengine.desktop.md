# zengine.desktop -- the desk

**Manual.** What the desk does by default: the keys that open the Terminal, the Pane Manager, the
Hotkeys pane and a pane's manual wherever you are, and those three panes. It is a *weave*, a piece
of Workshop that is loaded, replaced and spoken to by messages, built as `zengine-desktop-pane`;
other weaves reach it by its *office*, the name it answers to, `zengine.desktop`. Workshop itself,
which owns the room, the keys and the desk these panes sit on, is
[`zengine.workshop`](zengine.workshop.md).

## About

The desk is an ordinary weave you can replace. It declares the *application's keys*, which act
wherever you stand, and offers three panes: the **Pane Manager**, every pane this Workshop can show
and whether it is shown; the **Hotkeys** pane, every key in force and the way to move one; and the
**Manual**, the manual of the pane whose `?` you pressed.

It holds no power of its own. Opening, focusing and hiding a pane, moving a key and putting a
selection down are each asked of Workshop, which judges them against the desk as it is and answers;
the desk shows that answer. It cannot open a pane nothing offers, load an artifact, read another
pane's rows or keep you from quitting. What a key means is Workshop's: the Hotkeys pane prints the
keymap Workshop resolved, never its own idea of it.

## Use

Press <!-- key desktop.panes -->`ctrl+p`<!-- /key --> for the Pane Manager, choose a row, and press
<!-- key launcher.open -->`return`<!-- /key --> to show or focus that pane. Press
<!-- key desktop.hotkeys -->`ctrl+k`<!-- /key --> to see every key, and right-click a key there (or
press <!-- key hotkeys.menu -->`m`<!-- /key -->) to change it. Press the `?` at a pane's top right,
or <!-- key desktop.manual -->`ctrl+f`<!-- /key --> while a pane holds your keys, to read its
manual.

A pane's own keys act only while it holds your keys: press into it first. The application's keys
below act everywhere, above every pane's mode, except Escape, which a pane hears first.

A press names the picture it was aimed at: when the list moved under your hand the press does
nothing and the pane says `the list moved -- press again`.

Each layout keeps one setting for Hotkeys in its row, written from Info's SETTINGS rows or by
message ([`PaneSettingCommitRequested`](zengine.layouts.md#panesettingcommitrequested)): `help`, pinned, whether
the footer shows the keymap file and how a key is written there. Off, those rows are the list's; a
notice and what reading the file came to still show. A value it cannot use leaves the default in
force, and Workshop says it with the row.

## Commands

### `desktop.terminal`

Shows the Terminal, or focuses it when it is already on the desk.

- **Takes:** nothing; it acts wherever you are. Default key
  <!-- key desktop.terminal -->`ctrl+t`<!-- /key -->.
- **Answers:** asks Workshop to show `zengine.terminal`'s pane
  ([`PaneLaunchRequested`](zengine.workshop.md#panelaunchrequested)). Workshop seats it and gives it
  your keys.
- **Refuses:** in Workshop's words, as `` `terminal` is not here yet -- this run is still loading
  `zengine.terminal`; launch it again once it has arrived ``, or `` Terminal is not available --
  nothing holds `zengine.terminal` now ... ``; the Pane Manager shows the refusal.
- **Example:** press <!-- key desktop.terminal -->`ctrl+t`<!-- /key -->.

### `desktop.panes`

Shows the Pane Manager, focused, when it is hidden, hides it when it is shown, and brings it
forward, focused, when another pane covers it and the weaver has moved on from it: one they come
straight from, the last pane their keys or presses reached, is hidden even when covered.

- **Takes:** the desk as Workshop holds it now, not as the Pane Manager last saw it. Default key
  <!-- key desktop.panes -->`ctrl+p`<!-- /key -->.
- **Answers:** asks Workshop to toggle the Pane Manager
  ([`PaneToggleRequested`](zengine.workshop.md#panetogglerequested)); Workshop says `showed Pane
  Manager`, `hid Pane Manager -- its provider and what it holds are untouched; showing it brings
  it back`, or `brought Pane Manager forward -- it was covered`.
- **Refuses:** Workshop's refusal, shown in the Pane Manager when it is shown.
- **Example:** press <!-- key desktop.panes -->`ctrl+p`<!-- /key --> twice: shown, then hidden.

### `desktop.hotkeys`

Shows the Hotkeys pane, or focuses it.

- **Takes:** nothing. Default key <!-- key desktop.hotkeys -->`ctrl+k`<!-- /key -->.
- **Answers:** asks Workshop to show it
  ([`PaneLaunchRequested`](zengine.workshop.md#panelaunchrequested)).
- **Refuses:** Workshop's refusal, as for [`desktop.terminal`](#desktopterminal).
- **Example:** press <!-- key desktop.hotkeys -->`ctrl+k`<!-- /key -->.

### `desktop.deselect`

Puts your selection down: the last meaning Escape has, after every pane and menu that wanted it.

- **Takes:** nothing. Default key <!-- key desktop.deselect -->`escape`<!-- /key -->, in the
  default class, so a pane's own Escape is heard first.
- **Answers:** asks Workshop to put the selection down
  ([`DeselectRequested`](zengine.workshop.md#deselectrequested)), under the number of the press it
  came from.
- **Refuses:** nothing; Workshop decides what was selected. Write `desktop.deselect` as `none` in
  your keymap file and Escape means nothing here.
- **Example:** with a pane selected and nothing else wanting Escape, press
  <!-- key desktop.deselect -->`escape`<!-- /key -->.

### `desktop.manual`

Shows a pane's manual in the Manual pane: the pane whose `?` you pressed, or, from the keyboard, the
pane holding your keys.

- **Takes:** the pane the press was on. Default key <!-- key desktop.manual -->`ctrl+f`<!-- /key -->;
  the `?` at a pane's top right.
- **Answers:** asks that pane's office for its whole manual
  ([`PaneDocumentRequested`](../pane_document.hpp)), under a number of the desk's own so only that
  answer is shown, and asks Workshop to show the Manual
  ([`PaneLaunchRequested`](zengine.workshop.md#panelaunchrequested)), which Workshop opens over that
  pane, a little of it showing on every edge. The Manual says `asking
  zengine.builder-pane for its manual` until the answer comes.
- **Refuses:** `no pane was named: press the ? at the top right of a pane, or the manual key while a
  pane holds your keys`; `the ask for the manual of <office> was not sent`; `the manual of <office>
  could not be asked (<Loom's reason>)`; and the pane's own refusal, as `zengine.files offers no
  pane `x``.
- **Example:** press the `?` on the Builder pane; the Manual shows `MANUAL --
  zengine.builder-pane/builder`.

### `desktop.palette`

Shows the [Command Palette](zengine.palette.md) over the pane holding your keys, or hides it:
everything you can do by name, run by Workshop as your key would run it.

- **Takes:** nothing. Default key <!-- key desktop.palette -->`ctrl+space`<!-- /key -->, above
  every mode; it types no space into the pane it opens over. A pane whose keys are another
  program's may keep the key for that program (Neovim does).
- **Answers:** asks Workshop to show or hide the palette
  ([`PaneToggleRequested`](zengine.workshop.md#panetogglerequested)), which Workshop judges against
  the desk as it is.
- **Refuses:** nothing; a plan that loads no palette shows `zengine.palette` as nothing offering it.
- **Example:** in the Editor, press <!-- key desktop.palette -->`ctrl+space`<!-- /key -->, type
  `save`, press Return.

### `desktop.palette-shift`

Shows or hides the [Command Palette](zengine.palette.md), as
[`desktop.palette`](#desktoppalette) does, on Shift tapped twice: the OS window's second key for it.

- **Takes:** nothing. Default key <!-- key desktop.palette-shift -->`double-shift`<!-- /key -->:
  Shift pressed alone and let go, twice, within a double-click's time, with nothing between; a
  terminal reports no Shift alone, and the load says so.
- **Answers:** asks Workshop to show or hide the palette
  ([`PaneToggleRequested`](zengine.workshop.md#panetogglerequested)).
- **Refuses:** nothing; a held Shift, a key between the taps, a slow pair and another hand's tap
  are no double Shift.
- **Example:** in the OS window, tap Shift twice.

## Commands in the Pane Manager

The Pane Manager lists every pane Workshop can show, one row each, by name, alphabetically. The
marker `>` stands on the row you chose; you choose with the arrows, the wheel, or a press on the
row's name. The wheel scrolls the rows hidden its way first, and moves the marker once none is.

### `launcher.up`

Moves your choice up one row.

- **Takes:** the list. Default key <!-- key launcher.up -->`up`<!-- /key -->; the wheel, a notch at
  a time, once no row is hidden above.
- **Answers:** the marker moves; it stops at the first row.
- **Refuses:** nothing.
- **Example:** <!-- key launcher.up -->`up`<!-- /key -->.

### `launcher.down`

Moves your choice down one row.

- **Takes:** the list. Default key <!-- key launcher.down -->`down`<!-- /key -->.
- **Answers:** the marker moves; it stops at the last row.
- **Refuses:** nothing.
- **Example:** <!-- key launcher.down -->`down`<!-- /key -->.

### `launcher.open`

Shows the chosen pane, or focuses it and brings it to the front when it is already shown.

- **Takes:** the pane your marker holds, by its office and key, not its place in the list. Default
  key <!-- key launcher.open -->`return`<!-- /key -->; a second press on the chosen row's name while
  the Pane Manager holds your keys.
- **Answers:** asks Workshop to show that pane
  ([`PaneLaunchRequested`](zengine.workshop.md#panelaunchrequested)); its row's mark becomes
  `[open]`.
- **Refuses:** `Return showed nothing -- choose a row first`, when the pane you chose has left the
  list; Workshop's refusal as the notice, as `no pane `x` from `zengine.y` -- nothing has offered
  it, and a launch loads nothing` or `<name> is not available -- ...`.
- **Example:** <!-- key launcher.down -->`down`<!-- /key --> to `Builder`, then
  <!-- key launcher.open -->`return`<!-- /key -->.

### `launcher.close`

Hides the chosen pane: it leaves the desk, and the weave behind it keeps running with what it holds.

- **Takes:** the pane your marker holds. Default key <!-- key launcher.close -->`x`<!-- /key -->.
- **Answers:** asks the layouts to take its row off the desk
  ([`PaneCloseRequested`](zengine.layouts.md#panecloserequested)); its mark becomes `[    ]`.
- **Refuses:** `x hid nothing -- choose a row first`; the layouts' refusal, as `<name> is not on this
  desk -- nothing to hide, and a hide shows nothing`.
- **Example:** choose `Info`, press <!-- key launcher.close -->`x`<!-- /key -->.

### `launcher.menu`

Opens the chosen row's menu beside it: show, focus or hide that pane, manage it, inspect it.

- **Takes:** the chosen row. Default key <!-- key launcher.menu -->`m`<!-- /key -->; a right press on
  a row offers that row's menu.
- **Answers:** the menu, presented by the menu presenter: `show <name>` (or `focus <name>` and `hide
  <name>`), `manage <name> >`, Workshop's own menu for that pane, and `inspect in Info`, which asks
  `zengine.info` to inspect it as its own inspect does: `asking Info to inspect <name>`, then `Info
  inspects <name>`, `Info did not inspect <name> -- <Info's reason>`, or `Info could not be asked
  to inspect <name> (<the bus's reason>)` when Info is not loaded; `inspect in Info not sent --
  nothing was queued`. Only the newest ask is answered.
- **Refuses:** `no row is chosen -- choose one, then open its menu`; a line chosen after its pane
  left the list, `<pane> left the list -- nothing done`.
- **Example:** press <!-- key launcher.menu -->`m`<!-- /key --> and choose `hide Info`.

### `launcher.new`

Shows the View Builder, where you make a pane of your own.

- **Takes:** nothing. Default key <!-- key launcher.new -->`n`<!-- /key -->.
- **Answers:** asks Workshop to show `zengine.view.builder`'s pane
  ([`PaneLaunchRequested`](zengine.workshop.md#panelaunchrequested)).
- **Refuses:** Workshop's refusal, when the View Builder is not loaded.
- **Example:** press <!-- key launcher.new -->`n`<!-- /key -->.

## Commands in the Hotkeys table

The Hotkeys pane lists every key in force, grouped by where it is answered, with its label and its
action id; `*` marks a key your keymap file moved. A key is changed through a row's menu, and the
change is Workshop's to make, live, and to write to your keymap file.

### `hotkeys.up`

Moves the cursor up one key, over the group headings.

- **Takes:** the table. Default key <!-- key hotkeys.up -->`up`<!-- /key -->; the wheel moves one
  row a notch; a press on a row chooses it.
- **Answers:** the cursor moves; it stops at the first key.
- **Refuses:** nothing.
- **Example:** <!-- key hotkeys.up -->`up`<!-- /key -->.

### `hotkeys.down`

Moves the cursor down one key.

- **Takes:** the table. Default key <!-- key hotkeys.down -->`down`<!-- /key -->.
- **Answers:** the cursor moves; it stops at the last key.
- **Refuses:** nothing.
- **Example:** <!-- key hotkeys.down -->`down`<!-- /key -->.

### `hotkeys.top`

Moves the cursor to the first key.

- **Takes:** the table. Default key <!-- key hotkeys.top -->`home`<!-- /key -->.
- **Answers:** the cursor moves.
- **Refuses:** nothing.
- **Example:** <!-- key hotkeys.top -->`home`<!-- /key -->.

### `hotkeys.bottom`

Moves the cursor to the last key.

- **Takes:** the table. Default key <!-- key hotkeys.bottom -->`end`<!-- /key -->.
- **Answers:** the cursor moves.
- **Refuses:** nothing.
- **Example:** <!-- key hotkeys.bottom -->`end`<!-- /key -->.

### `hotkeys.menu`

Opens the chosen key's menu: `Modify (press a key)`, `Modify (type a spelling)`, `Add a key
(press)`, `Add a key (type)`, ``Remove `<key>` ``, `Disable` and `Reset to default`.

- **Takes:** the key under the cursor; a key marked `(not remappable)` has no menu. Default key
  <!-- key hotkeys.menu -->`m`<!-- /key -->; a right press on a key's row.
- **Answers:** the menu. Choosing `press` waits for the key you press next, `type` opens a line for
  its spelling ([commands on a key being changed](#commands-on-a-key-being-changed)); `Remove`,
  `Disable` and `Reset` ask Workshop at once. The notice is `` asked the host to change `<id>` ``,
  then Workshop's sentence, as `` `desktop.terminal` now `ctrl+g` -- written to the keymap file ``.
- **Refuses:** `no row is chosen -- choose one, then open its menu`; `that row moved -- open its
  menu again`; `edit not sent -- an earlier edit is still unanswered`; Workshop's refusal, with
  nothing changed.
- **Example:** on `desktop.terminal`, press <!-- key hotkeys.menu -->`m`<!-- /key --> and choose
  `Modify (press a key)`.

## Commands on a key being changed

While a key is being captured only Escape is declared, so the key you press is the one taken;
while a spelling is typed, Return and Escape are, and every other key reaches the line.

### `hotkeys.commit`

Uses the spelling you typed for the key you chose.

- **Takes:** the line, `key for <id>: `, spelled as a keymap file spells a key (`ctrl+q`, `return`,
  `shift+b`). Default key <!-- key hotkeys.commit -->`return`<!-- /key -->.
- **Answers:** asks Workshop for the change
  ([`KeymapEditRequested`](zengine.workshop.md#keymapeditrequested)); the notice is Workshop's
  sentence, as `` `layout.next` also `ctrl+g` -- written to the keymap file ``, or `... -- isolated
  run -- no keymap file; the change stands for this run`.
- **Refuses:** text outside plain ASCII is not typed, `not typed -- a key's spelling is plain ASCII
  on one line`; `edit not submitted -- nothing was queued`; `edit not delivered -- nothing changed
  (<Loom's reason>)`; Workshop's refusal, as a key two actions would share.
- **Example:** choose `Modify (type a spelling)`, type `ctrl+g`, press
  <!-- key hotkeys.commit -->`return`<!-- /key -->.

### `hotkeys.cancel`

Leaves the capture or the line; nothing changes.

- **Takes:** the capture or the line. Default key <!-- key hotkeys.cancel -->`escape`<!-- /key -->.
- **Answers:** `capture cancelled`, or `spelling cancelled`.
- **Refuses:** nothing. A chord that opens a tool above every mode, such as
  <!-- key desktop.panes -->`ctrl+p`<!-- /key -->, never reaches a capture: type its spelling
  instead.
- **Example:** <!-- key hotkeys.cancel -->`escape`<!-- /key -->.

## Commands in the Manual

### `manual.up`

One line up.

- **Takes:** the manual shown. Default key <!-- key manual.up -->`up`<!-- /key -->; the wheel.
- **Answers:** the lines move; at the first nothing moves.
- **Refuses:** nothing.
- **Example:** <!-- key manual.up -->`up`<!-- /key -->.

### `manual.down`

One line down.

- **Takes:** the manual shown. Default key <!-- key manual.down -->`down`<!-- /key -->.
- **Answers:** the lines move; the last line says how many more are below.
- **Refuses:** nothing.
- **Example:** <!-- key manual.down -->`down`<!-- /key -->.

### `manual.top`

The first line.

- **Takes:** the manual shown. Default key <!-- key manual.top -->`home`<!-- /key -->.
- **Answers:** the manual from its first line.
- **Refuses:** nothing.
- **Example:** <!-- key manual.top -->`home`<!-- /key -->.

### `manual.bottom`

The last lines.

- **Takes:** the manual shown. Default key <!-- key manual.bottom -->`end`<!-- /key -->.
- **Answers:** the manual's last lines.
- **Refuses:** nothing.
- **Example:** <!-- key manual.bottom -->`end`<!-- /key -->.

### `manual.close`

Closes the Manual; its words go with it.

- **Takes:** the Manual. Default key <!-- key manual.close -->`escape`<!-- /key -->.
- **Answers:** asks the layouts to take the Manual off the desk
  ([`PaneCloseRequested`](zengine.layouts.md#panecloserequested)). Nothing keeps a pane's manual
  once the Manual has left the desk.
- **Refuses:** nothing.
- **Example:** <!-- key manual.close -->`escape`<!-- /key -->.

## Hears

### `PaneShortcuts`

`PaneShortcuts` v1, from any office: the keys that office wants to act everywhere, each naming one of
its panes and one of that pane's actions. The desk adds each as an application key, asks Workshop to
admit them with its own, and answers `PaneShortcutsAnswered` once Workshop has judged them, one
declaration at a time and within a bound. A press of one is sent to Workshop as
`PaneShortcutInvoked`, naming that pane and action.

## Follow

The desk shows what Workshop says: the panes there are (`PaneInventory`), the keys in force
(`KeymapShown`) and Workshop's verdict on each of its declarations (`ActionsJudged`,
`ActionsWithdrawn`), each from `zengine.workshop`. An observer follows those instead of reading
these panes. A declaration Workshop refused shows as `keys refused: <its reason>` in the pane it was
about; one withdrawn, `keys withdrawn: <its reason>`. The desk tells the layouts' owner
`PaneSettingUnused` when a layout hands one of its panes a setting it cannot use.

## Shows

### `pane`

A Pane Manager row, named `pane:<office>/<pane>`: `> [open] Builder`, under the heading `PANES --
12` (`PANES (waiting)` until Workshop has said anything). The marker is `>` on your choice, and `?`
when the pane you chose has left the list. Under the list stand the latest notice and what stays
true, as `Info left the list -- choose a row before Return shows anything`.

### `mark`

A row's mark, named `mark:<office>/<pane>`: `[open]` (on the desk), `[    ]` (not on it), `[load]`
(this run is still loading its provider) or `[gone]` (nothing provides it now: build its provider,
then launch again). A press on it shows or hides the pane.

### `binding`

A Hotkeys row, named `binding:<group>/<id>/<key>`: the key, its label, its id and its marks, under
the heading `HOTKEYS -- 41 in force; * moved by your file; right-click or `m` on a row to change it`.
Below the table: the last notice, `keymap file: <path>` or `(none)` and what reading it came to.

### `line`

The spelling line, `key for <id>: `, while [`hotkeys.commit`](#hotkeyscommit) waits for a spelling.

## Files

The desk reads and writes no file. The keymap file the Hotkeys pane names is Workshop's, which reads
it at launch and writes each change you make here
([the keymap file](hotkeys.md#the-keymap-file)). Which pane a row opens is the inventory Workshop
holds; where panes sit is the live layout [`zengine.layouts`](zengine.layouts.md) keeps
([setups](setups.md)).

## See also

- [`zengine.workshop`](zengine.workshop.md), the host the desk asks.
- [Hotkeys and the keymap](hotkeys.md): how a key is resolved, and the keymap file.
- [Panes](panes.md): moving, sizing and ordering panes on the desk.
- [Getting started with Workshop](getting-started.md).
