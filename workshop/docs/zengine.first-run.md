# zengine.first-run -- the quick setup and Start here

**Manual.** The quick setup asks, once, how you edit text, and lists the ready-to-use setups, each
with Start. Then **Start here** walks the story a step at a time -- make a view, wire it into Flow,
show Flow's answer, make it an app, start it -- each box ticked as it happens. Both are panes of
the weave in the office `zengine.first-run`, built as `zengine-first-run` and loaded by a row of the
shipped plans that load a window. The quick setup shows itself at your first start; find either
again as any pane, in the Pane Manager (<!-- key desktop.panes -->`ctrl+p`<!-- /key -->) or the
palette (<!-- key desktop.palette -->`ctrl+space`<!-- /key -->), as Quick setup and Start here.

## About

The weave keeps two office-wide settings at [`zengine.layouts`](zengine.layouts.md): `answered`,
set by **Done**, **Skip** or **The story**, and `story`, the steps ticked -- their numbers, `h`
after one you ticked by hand, as `1 2h 4`. At a start whose run keeps a settings file and
`answered` is not set, the quick setup shows itself in front, holding the keys. A run with no
settings file (`--isolated` without a `--prefs` path), or a settings file the layouts' owner refused
at launch, keeps no settings, so neither pane shows by itself there. Nor does the quick setup on a
desk where a pane beyond Layouts, Info and Notices is already open: that pane keeps the keys, and
the quick setup shows at the next start. Clear `answered` in the pane's gear and it shows again.

**Start here** opens when the quick setup is put down, holding the keys, and at each later start
that keeps settings until the story is done -- beside the pane that holds the keys, never taking
them.

Neither pane changes anything itself: each answer goes to the owner of what it changes and is said
in that owner's words. On a weaver's host with a guest door, only your own hand reaches either
pane, as only yours reaches the Terminal ([whose host this
is](../../external-host/docs/external-host.md#whose-host-this-is-and-what-each-power-reaches-there)).

## Use

In the quick setup, move with [`first-run.up`](#first-runup) and
[`first-run.down`](#first-rundown), and choose with [`first-run.choose`](#first-runchoose); in
Start here, with [`story.up`](#storyup), [`story.down`](#storydown) and
[`story.choose`](#storychoose). Or press an item's line.

The quick setup's items, top to bottom:

- **Edit text with Neovim** asks the editor switch
  ([`zengine.editor-switch`](../../editor/docs/zengine.editor-switch.md)) to switch the Editor's
  office to the plan's `neovim` choice, or back to `standard` when it is unchecked. The box shows
  which choice holds the office, as the switch says it. A switch that would lose something asks
  first: the pane says what, and choosing the box again confirms it. The choice holding the office
  when you quit is the one the next start opens ([Neovim in
  Workshop](../../editor/docs/neovim.md#switch-while-you-work)).
- **Ready-to-use setups** are the ones [the launcher](../../launcher/docs/zengine.launcher.md)
  lists, each with its own title and line. **Start** on one asks Workshop whether the hand that
  pressed may start it -- as Files' start is asked, classes `build` and `write`, so on a weaver's
  host a guest starts nothing -- then asks the launcher, and says what it answered: the setup
  started, with its process, going on without Workshop; or why nothing started. Each runs in a
  Workshop of its own ([demo setups](demo-setups.md)). What the launcher says of its setups -- in
  the package a stranger downloads, that a setup needs a Python 3.8 or newer on the PATH and none
  was found -- stands beneath them. With no demo service or no launcher, the pane says so.
- **The story**, **Done** and **Skip** write `answered`, put the pane down when that is answered,
  and open Start here. A settings file that will not take the write is said, and the pane is put
  down all the same.

## Commands

### `first-run.up`

Moves the quick setup's cursor to the item above.

- **Takes:** the box, each setup's Start, The story, Done and Skip, in that order. Default key
  <!-- key first-run.up -->`up`<!-- /key -->.
- **Answers:** the cursor one item up, stopping at the box.
- **Refuses:** nothing.
- **Example:** press <!-- key first-run.up -->`up`<!-- /key -->.

### `first-run.down`

Moves the quick setup's cursor to the item below.

- **Takes:** the box, each setup's Start, The story, Done and Skip, in that order. Default key
  <!-- key first-run.down -->`down`<!-- /key -->.
- **Answers:** the cursor one item down, stopping at Skip.
- **Refuses:** nothing.
- **Example:** press <!-- key first-run.down -->`down`<!-- /key --> to reach the first setup.

### `first-run.choose`

Chooses the quick setup's item under the cursor.

- **Takes:** the item under the cursor. Default key
  <!-- key first-run.choose -->`return`<!-- /key -->.
- **Answers:** on the box, asks the editor switch
  ([`EditorSwitchRequested`](../../editor/docs/zengine.editor-switch.md#editorswitchrequested)) for
  the other choice, or confirms the consent it asked
  ([`EditorSwitchConfirmed`](../../editor/docs/zengine.editor-switch.md#editorswitchconfirmed)); the
  switch's answer is shown under the box. On a setup's Start, asks the launcher for it
  ([`Launch`](../../launcher/docs/zengine.launcher.md#launch)) once Workshop allowed the hand. On
  The story, Done or Skip, writes `answered`
  ([`OfficeSettingCommitRequested`](zengine.layouts.md#officesettingcommitrequested)), puts the pane
  down ([`PaneCloseRequested`](zengine.layouts.md#panecloserequested)) when that is answered, and
  opens Start here ([`PaneLaunchRequested`](zengine.workshop.md#panelaunchrequested)).
- **Refuses:** `the editor switch is still under way`; the switch's refusal, as it says it; `a start
  is waiting for its answer; start again once it comes`; `nothing was started -- <why>`, in
  Workshop's or the launcher's words; a settings write Workshop refused, as `not kept: <its words>`.
- **Example:** press <!-- key first-run.choose -->`return`<!-- /key --> on the box.

### `story.up`

Moves Start here's cursor to the item above.

- **Takes:** each step's box and its Show me, in order. Default key
  <!-- key story.up -->`up`<!-- /key -->.
- **Answers:** the cursor one item up, stopping at the first box.
- **Refuses:** nothing.
- **Example:** press <!-- key story.up -->`up`<!-- /key -->.

### `story.down`

Moves Start here's cursor to the item below.

- **Takes:** each step's box and its Show me, in order. Default key
  <!-- key story.down -->`down`<!-- /key -->.
- **Answers:** the cursor one item down, stopping at the last Show me.
- **Refuses:** nothing.
- **Example:** press <!-- key story.down -->`down`<!-- /key --> to reach step 1's Show me.

### `story.choose`

Chooses Start here's item under the cursor.

- **Takes:** the item under the cursor. Default key
  <!-- key story.choose -->`return`<!-- /key -->.
- **Answers:** on a box, ticks it by hand, or unticks a hand's tick, and writes the ticks
  ([`OfficeSettingCommitRequested`](zengine.layouts.md#officesettingcommitrequested)); on Show me,
  opens the step's pane ([`PaneLaunchRequested`](zengine.workshop.md#panelaunchrequested)) and says
  its next move.
- **Refuses:** a box its owner ticked stays ticked, saying what ticked it; the launch's refusal, in
  Workshop's words; `the ticks were not kept: <its words>`.
- **Example:** press <!-- key story.choose -->`return`<!-- /key --> on step 1's Show me: the View
  Builder opens.

## Follow

Each box of Start here is ticked by a fact its owner says, never by the desk's words or your files:

| step | ticked when |
|---|---|
| 1. Make a view | a running view is on the desk (the inventory) |
| 2. Wire its button into Flow | an office accepted what one of its buttons said (the view's own wiring) |
| 3. Show Flow's answer in the view | the view was told a field it shows (the view's own wiring) |
| 4. Make it an app | the launcher lists an app in the project's `apps/`, or started one |
| 5. Start it, and close Workshop | the launcher started an app, or says one runs |

A step done earlier is ticked when its owner is asked, at each start. A press ticks a box by
hand, `ticked by you`, or unticks it; the owner's fact makes it the owner's. **Show me** opens the
step's pane -- the View Builder for 1, 4 and 5, Flow for 2 and 3 -- and says its next move, plainly;
a refusal is drawn as an alert. Step 4's names what Export app needs: the view saved, its flow
exported beside it.
