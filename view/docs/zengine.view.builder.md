# zengine.view.builder -- the View Builder

**Manual.** The pane where you make a *view* by hand: a small panel of number fields, buttons and
labels, placed in whole pixels on a design canvas, then run in a pane of its own. It is a *weave*: a
piece of Workshop that is loaded, replaced and spoken to by messages, built as
`zengine-view-builder`. Other weaves reach it by its *office*, the name it answers to,
`zengine.view.builder`; its pane is `view-builder`. New to it? Start with
[Make a small panel by hand in the View Builder](view-builder.md).

## About

A view is data, not C++: its name, its size, its elements, the field each label shows and the
message each button says, its *intent* ([the description](view.md#the-description)). The view's name
is the office it runs under and the namespace of what it says.

The pane draws the view you are making with the view's own picture code, at its own pixels, on the
design canvas in its middle, so what you see there is what runs. Along the top is the bar, `[New]`
`[Open]` `[Save]` `[Run]` `[Apply]` `[Stop]` `[Export app]` `[Run app]`; `[Save*]` says the view is unsaved,
`[Running]` that it runs. Down the left: the view's name in `View`, its file in `File`, `Add` with `[Label]`
`[Number]` `[Button]`, its `Size` and the `Grid` in boxes, and the list of its elements. The
selected element's values sit in boxes on the right in a wide pane, below the list in a narrow one.
Beneath the canvas a row says the view's name, `running` or `stopped`, and its file; the last two
rows are the notice, where every answer and refusal is said. It draws in a graphics window too
([Hears](#arearoom)).

The pane runs nothing itself. It asks the view host,
[`zengine.view.host`](zengine.view.host.md), to run your view, change it or stop it, and shows the
host's answer on the notice. The running view is a participant of its own, under the view's name,
in a pane of its own. Reloaded in place, the pane keeps your view, its
file and whether it ran, and asks the host again about a view it ran: `The view host still runs
tally.panel`; or, when the view stopped meanwhile, it runs again, `The view had stopped; registered
tally.panel`. A save asks Workshop first whether the hand that pressed it may write a file.

## Use

Open the Pane Manager, <!-- key desktop.panes -->`ctrl+p`<!-- /key -->, and press
<!-- key launcher.new -->`n`<!-- /key -->, or choose `View Builder` in its list. Its keys act only
while it holds your keys.

- **Make** an element by dragging a kind from `Add` onto the canvas; a click on a kind makes one
  below the last.
- **Select** one by clicking it on the canvas or in the list; **move** it by dragging it, and
  **size** it by dragging a handle. A place by hand snaps to the edges within the layout's
  `snap-reach` and to the `Grid`; hold `Alt` to set every snap aside. The arrow keys move it a pixel, a cell with `Shift`; `Delete`
  removes it; `Escape` puts a choice or the selection down.
- **Size** the view by its handles, or type into the `Size` boxes. Nothing sits outside the view:
  a value or a key that would cross its edge is refused in words.
- **Pan** the canvas with the middle button.
- **Type** a value: click its box, type, press `Return`; `Tab` keeps it and moves to the next box,
  `Escape` puts it back. A value the view's rules refuse stays in its box, the reason on the notice.
- **Make an intent**: select a button and press `[Make intent]`.
- **Carry** what a button says: drag its `says` words out, or right-press them and choose `Carry`.
  Drop a shape or a field on a label, on the canvas or its row, and the label shows it.

Then [`view.run`](#viewrun) it, [`view.apply`](#viewapply) each change, [`view.save`](#viewsave) it
and [`view.export`](#viewexport) it as an app. Each command but
[`view.choices`](#viewchoices) is a control on the bar. A value being typed is kept before any command acts; if it is refused, the
command does nothing.

Each layout keeps three settings for the pane, set in Info or by message
([`PaneSettingCommitRequested`](../../workshop/docs/zengine.layouts.md#panesettingcommitrequested)):
`snap-reach`, pinned, in pixels, and `new-width` and `new-height`, the size [`view.new`](#viewnew)
makes a view at. A value one cannot use keeps its default and is said to Workshop.

## Commands

### `view.save`

Writes the view to the file named in `File`, and remembers it for the next launch.

- **Takes:** the view and the `File` box. Default key <!-- key view.save -->`ctrl+s`<!-- /key -->; the
  control `[Save]`.
- **Answers:** asks Workshop to allow a write for the hand that pressed it, and writes nothing until
  it answers; then `Saved tally.view`, and the project's `view-builder.json` names that file
  ([Files](#files)). A write of it that fails is said after the notice,
  `(not remembered for the next launch: <the reason>)`, and made again at the next chance.
- **Refuses:** `name a file to save the view to`, with nothing asked; `A save is waiting for
  Workshop's answer; save again once it comes`; `Saving was refused: <Workshop's reason>`, as a
  guest's on a weaver's host; `Not saved: File no longer names <the file>; save again`;
  `Not saved: Workshop could not be asked to allow it`; `The save waiting for Workshop was dropped;
  save again`, when the pane was reloaded meanwhile; and the file door's reason when it cannot write.
- **Example:** type `tally.view` under `File`, press <!-- key view.save -->`ctrl+s`<!-- /key -->.

### `view.open`

Reads the view in the file named in `File`, in place of the one you have; it is shown from its top
left corner.

- **Takes:** the `File` box. Default key <!-- key view.open -->`ctrl+o`<!-- /key -->; the control
  `[Open]`.
- **Answers:** `Opened tally.view`.
- **Refuses:** over an unsaved view, the first press says `The view is unsaved: Save it, or press
  Open again to discard it`, and the bar says `[Open: discard?]`; `Type the view's file under File,
  then Open`; `stop the running view before opening another`; and the reason the file could not be
  read or is not a view ([the description](view.md#the-description)).
- **Example:** type `tally.view` under `File`, press <!-- key view.open -->`ctrl+o`<!-- /key -->.

### `view.run`

Asks the view host to run the view: it becomes a participant of its own, under the view's name, and
its pane opens.

- **Takes:** the view as it stands. Default key <!-- key view.run -->`ctrl+r`<!-- /key -->; the control
  `[Run]`.
- **Answers:** `run asked of the view host`, then the host's answer, as `registered tally.panel; it
  waits until told tally.Total`. The bar then says `[Running]`, and `view-builder.json` records
  that it runs. The ask is [`ViewRun`](view.md#the-view-host), answered `ViewAnswer`
  ([view/vocabulary.hpp](../vocabulary.hpp)).
- **Refuses:** `run refused: ` and the host's reason, as `this session already runs a view; apply a
  change or stop it first`, or `` `tally.panel` is already held by a running participant ``;
  `run was not delivered: <the reason>`; `run could not be queued`; `16 asks are unanswered; wait
  for the view host`.
- **Example:** after making the tally panel, press <!-- key view.run -->`ctrl+r`<!-- /key -->.

### `view.new`

Starts a new view, `my.view`, empty, with no file, at the layout's `new-width` and `new-height`.

- **Takes:** nothing. No default key; the control `[New]`.
- **Answers:** `new complete`; the canvas shows the new view from its corner.
- **Refuses:** over an unsaved view, the first press says `The view is unsaved: Save it, or press
  New again to discard it`, and the bar says `[New: discard?]`; `stop the running view before
  starting another`.
- **Example:** press `[New]`, then type the view's name under `View`.

### `view.export`

Makes an app of the saved view and its flow: a folder that runs with no Workshop, in an app host of
its own, `zengine-app` ([the app host](../../app/docs/app.md)). The app is named after the view's
file, and its flow is the `.flow` beside that file with the same name, as Flow's
[`ask-export`](../../flow/docs/zengine.flow.md#ask-export) writes it.

- **Takes:** the view's saved file, `tally.flow` beside `tally.view`, and the project. No default
  key; the control `[Export app]`.
- **Answers:** asks Workshop to allow a write for the hand that pressed it, as
  [`view.save`](#viewsave) does, and writes nothing until it answers; then writes
  `<project>/apps/tally/` ([Files](#files)) and says `Exported <the folder>: start it with its
  run.cmd or run.sh; its console asks about zengine-operators-basic, zengine-app-weave,
  zengine-graphics, zengine-window-sdl, zengine-timer on its first start`. It writes no decision:
  the app's console asks each of those once, and the app's folder keeps what it says.
- **Refuses:** each after `Not exported: `: `save the view first; an app is made from its saved
  file`; `no tally.flow beside tally.view: export its flow from Flow as tally.flow first`; a `.flow`
  or a view's file that is not one; `'my tally' is not a plain folder name`; `no project
  directory`; `no zengine-app beside this Workshop`; Workshop's reason, as a guest's on a weaver's
  host; `the view changed while Workshop was asked; export again`; the file door's reason, naming
  what was written before it stopped. Also `An export is waiting for Workshop's answer; export again
  once it comes`, and `The export waiting for Workshop was dropped; export again` after a reload.
  Over an app already there, the first press says `<the folder> is there: press Export again to
  replace the app's files in it`, and the bar says `[Export: replace?]`.
- **Example:** save the tally panel as `tally.view`, export `tally` from Flow as `tally.flow`, press
  `[Export app]`.

### `view.run-app`

Starts the app the saved view was exported as, on its own: a process of the app host's that goes on
after Workshop quits ([the launcher](../../launcher/docs/zengine.launcher.md)).

- **Takes:** the view's saved file, whose name the app has. No default key; the control `[Run app]`.
- **Answers:** asks Workshop to allow a build and a write for the hand that pressed it, then asks
  the launcher for the app by its name ([`Launch`](../../launcher/docs/zengine.launcher.md#launch));
  `Starting the app tally`, then `Started the app tally (process 4242): its console asks what it
  may run, and it goes on without Workshop`; on POSIX, `...: it goes on without Workshop, with no
  console: an undecided row ends it, said in its app.log; run.sh in a terminal decides it`.
- **Refuses:** `Not started: save the view and export it first; the app is named after its file`;
  `Not started: <Workshop's reason>`, as a guest's on a weaver's host; `Not started: <the
  launcher's reason>`, as `no app 'tally' in <project>/apps`; `A start is waiting for its answer;
  run the app again once it comes`.
- **Example:** after `[Export app]`, press `[Run app]`.

### `view.apply`

Changes the running view to the one you have.

- **Takes:** the view as it stands, and the view running. No default key; the control `[Apply]`.
- **Answers:** `apply asked of the view host`, then the host's answer. With the same shapes it is
  `applied in place: tally.panel tells and says the same shapes`, and what was typed into the
  running view stays; with other shapes, a label bound to a new field say, it is `registered
  tally.panel afresh: its shapes changed` and what it waits for, and its fields start again from the
  description. The ask is [`ViewApply`](view.md#the-view-host)
  ([view/vocabulary.hpp](../vocabulary.hpp)).
- **Refuses:** `apply refused: ` and the host's reason, as `no view runs in this session for this
  participant`, or Loom's words when the changed view cannot register beside the one running, which
  goes on as it was; `apply was not delivered: <the reason>`.
- **Example:** move a label, press `[Apply]`.

### `view.stop`

Stops the running view: its pane says it stopped, then the participant is gone.

- **Takes:** the view running. No default key; the control `[Stop]`.
- **Answers:** `stop asked of the view host`, then `stopped tally.panel`; the bar says `[Run]`
  again, and `view-builder.json` records that it does not run. The ask is
  [`ViewStop`](view.md#the-view-host) ([view/vocabulary.hpp](../vocabulary.hpp)).
- **Refuses:** `stop refused: no view runs in this session for this participant`; `stop was not
  delivered: <the reason>`.
- **Example:** press `[Stop]`.

### `view.choices`

Shows the next of the fields a label could show, when a shape carried onto it has more than the
pane's rows hold.

- **Takes:** the field choices shown. No default key; the control `[More]`, under the choices.
- **Answers:** the next choices, each a button; pressing one binds the label, `` `total` shows
  tally.Total.total ``.
- **Refuses:** nothing; while no `[More]` shows, nothing moves.
- **Example:** carry a shape of many fields onto a label, press `[More]`.

## Hears

### `ViewEdit`

`ViewEdit` v1 ([vocabulary.hpp](../view-builder/vocabulary.hpp)), from a weave granted it: one edit
by name, `action` and its `arguments`, as the controls make them; the edit `describe` says their
grammar on the notice. Answered to the asker with `ViewEdited` v1: whether it was taken, the
notice, and the view's bytes. An edit is taken whole or refused whole. Its `save` writes at once:
only a hand's save asks Workshop.

### `AreaRoom`

`AreaRoom` v4, from the graphics window that shows the area `zengine.view.builder`, as that
window's office ([`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp)): the pane's room
there, in the window's unit and text. It draws its picture for that room on its ground, whole in a
new window, incarnation, size or measure, or an empty area, then only what changed. Each time it
starts it asks the graphics directory, `zengine.graphics`, for this room (`RoomAsked` v1).

### `DrawnRefused`

`DrawnRefused` v1, from that window: a drawing it refused, and why. What it shows stays as it was.
A refused change is drawn again whole at once; a refused whole picture is said on the notice, `The
graphics window refused the View Builder's picture: <the reason>`, and the pane draws there again
at its next change.

### `ActionRequested`

`ActionRequested` v1, from a window showing the area deaf to the pointer: a press on one of the
pane's places there, by the name its Workshop picture gives it -- `control:view.<action>`,
`kind:<kind>`, `list:<id>`, `element:<id>`, `box:<field>` -- a click let go where it was pressed:
a kind makes one below the last, an element or its row is selected, a box opens with the caret
after its value. It begins no drag and carries nothing, and Workshop refuses a save it asks.

### `AreaButtonChanged`

`AreaButtonChanged` v3, from that window: a button pressed or let go in the pane's area, on the part
the window drew there: a kind held to make where it is let go, an element held to move or resize, a
box opened with the caret where pressed, the canvas panned by the middle button, a right press on
what a button says offered to carry out, else handed back. One on another part than the pane shows
there now is refused in words. Its moves and release carry the press's number.

### `AreaPointerMoved`

`AreaPointerMoved` v1, from that window: the pointer moved in the pane's area. While a press there
is held it drags as on Workshop's canvas; with none held it marks the element it rests on.

### `AreaWheelTurned`

`AreaWheelTurned` v1, from that window: the wheel over the pane's area, which scrolls the element
list as on Workshop's canvas.

### `AreaHoldEnded`

`AreaHoldEnded` v1, from that window, about the pane's area: the press held there ended with no
release, a lost press, which puts back what its drag moved.

### `AreaPointerLeft`

`AreaPointerLeft` v1, from that window, about the pane's area: the pointer left it, and the mark of
what it rested on goes.

## Follow

The View Builder publishes nothing. Its asks go to the view host, which answers it alone, and what
it made of each is on its notice. It tells the layouts' owner `PaneSettingUnused` when a layout hands
it a setting it cannot use. To follow a running view, follow the view: it is a participant of
its own, publishing its intents' shapes to whoever accepts them
([the view renderer](view.md#the-view-renderer)).

## Files

- **The view file**, any path you type under `File`, a path that is not absolute taken from the
  directory Workshop runs in: the view's description, saved whole
  ([the description](view.md#the-description)). The pan and the grid are not in it.
- **`view-builder.json`**, in the project's directory, which `zengine.project` names: the view file
  the builder has open, relative to the project when it is inside it, and whether its view runs.
  It names only a file a view was saved to or opened from, never a name typed into `File` and not
  saved. It is written when either changes.
- **At a launch** the builder reads it, opens that file, and if its view ran, runs it again, its
  pane where your desk left it. It says why when it cannot: `No project directory: the view this
  builder runs is not remembered across a launch`; `view-builder.json is not this builder's file`;
  `Could not open <the file>: <the reason>`, or `Could not run <the file> again: <the reason>`. A
  view file it could not open leaves `view-builder.json` as it was until you open, save or start a
  view, so putting the file back brings the view back.
- **`apps/<name>/`**, in the project's directory, written by [`view.export`](#viewexport): the app's
  plan `app-plan.json`, `app.json`, `<name>.view` and `<name>.flow` copied from the files it was made
  from, and the launchers `run.cmd` and `run.sh`, which start the `zengine-app` beside Workshop, or
  the one the project's `zengine-app.path` names, on that folder ([an app's
  folder](../../app/docs/app.md#an-apps-folder)). An export again replaces
  those six files and touches nothing else there: `app-authority.json`, where the app's console
  keeps its decisions, stays.

## See also

- [Make a small panel by hand in the View Builder](view-builder.md): the tally panel, made, run and
  joined to Flow by dragging its shapes.
- [Described views](view.md): the description and its file format, the view host, and the
  renderer a running view is drawn by.
- [`zengine.view.host`](zengine.view.host.md): the view host's manual, every ask it answers.
- The shapes: the view host's asks in [view/vocabulary.hpp](../vocabulary.hpp); the builder's own in
  [view/view-builder/vocabulary.hpp](../view-builder/vocabulary.hpp); the pane protocol's in
  [workshop/pane_vocabulary.hpp](../../workshop/pane_vocabulary.hpp),
  [workshop/pane_operation.hpp](../../workshop/pane_operation.hpp) and
  [workshop/pane_carry.hpp](../../workshop/pane_carry.hpp).
