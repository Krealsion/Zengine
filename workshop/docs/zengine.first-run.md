# zengine.first-run -- the quick setup

**Manual.** The quick setup asks, once, how you edit text, and shows where the ready-to-use setups
are. It is the weave in the office `zengine.first-run`, built as `zengine-first-run` and loaded by
a row of the shipped plans that load a window. It shows itself at your first start; find it again
as any pane, in the Pane Manager (<!-- key desktop.panes -->`ctrl+p`<!-- /key -->).

## About

The quick setup keeps one office-wide setting at [`zengine.layouts`](zengine.layouts.md):
`answered`, set by **Done** or **Skip**. At a start whose run keeps a settings file and
`answered` is not set, it shows itself in front, holding the keys. A run with no settings file
(`--isolated` without a `--prefs` path), or a settings file the layouts' owner refused at launch, keeps no
settings, so the quick setup shows nothing by itself there. Nor does it on a desk where a pane
beyond Layouts and Info is already open -- say, one you opened before it arrived: that pane keeps
the keys, and the quick setup shows at the next start. Clear `answered` in the pane's gear and it
shows again at the next start.

It changes nothing itself. Each answer goes to the owner of what it changes, and is said in that
owner's words:

- **Edit text with Neovim** asks the editor switch
  ([`zengine.editor-switch`](../../editor/docs/zengine.editor-switch.md)) to switch the Editor's
  office to the plan's `neovim` choice, or back to `standard` when it is unchecked. The box shows
  which choice holds the office, as the switch says it. A switch that would lose something asks
  first: the pane says what, and choosing the box again confirms it. The choice holding the office
  when you quit is the one the next start opens ([Neovim in
  Workshop](../../editor/docs/neovim.md#switch-while-you-work)).
- **Ready-to-use setups** names the pages that tell each one and how to start it ([demo
  setups](demo-setups.md)) and your first five minutes ([getting started](getting-started.md)).
- **Done** and **Skip** write `answered` through `zengine.layouts` and put the pane down when it
  has answered. A settings file that will not take the write is said, and the pane is put
  down all the same.

On a weaver's host with a guest door, only your own hand reaches the quick setup, as only yours
reaches the Terminal ([whose host this
is](../../external-host/docs/external-host.md#whose-host-this-is-and-what-each-power-reaches-there)).

## Use

Move with [`first-run.up`](#first-runup) and [`first-run.down`](#first-rundown), and choose with
[`first-run.choose`](#first-runchoose); or press the line of the box, Done or Skip.

## Commands

### `first-run.up`

Moves the cursor to the item above.

- **Takes:** the box, Done and Skip, in that order. Default key
  <!-- key first-run.up -->`up`<!-- /key -->.
- **Answers:** the cursor one item up, stopping at the box.
- **Refuses:** nothing.
- **Example:** press <!-- key first-run.up -->`up`<!-- /key -->.

### `first-run.down`

Moves the cursor to the item below.

- **Takes:** the box, Done and Skip, in that order. Default key
  <!-- key first-run.down -->`down`<!-- /key -->.
- **Answers:** the cursor one item down, stopping at Skip.
- **Refuses:** nothing.
- **Example:** press <!-- key first-run.down -->`down`<!-- /key --> twice to reach Skip.

### `first-run.choose`

Chooses the item under the cursor.

- **Takes:** the item under the cursor. Default key
  <!-- key first-run.choose -->`return`<!-- /key -->.
- **Answers:** on the box, asks the editor switch
  ([`EditorSwitchRequested`](../../editor/docs/zengine.editor-switch.md#editorswitchrequested)) for
  the other choice, or confirms the consent it asked
  ([`EditorSwitchConfirmed`](../../editor/docs/zengine.editor-switch.md#editorswitchconfirmed)); the
  switch's answer is shown under the box. On Done or Skip, writes `answered`
  ([`OfficeSettingCommitRequested`](zengine.layouts.md#officesettingcommitrequested)) and, when
  that is answered, puts the pane down
  ([`PaneCloseRequested`](zengine.workshop.md#panecloserequested)).
- **Refuses:** `the editor switch is still under way`; the switch's refusal, as it says it; a
  settings write Workshop refused, as `not kept: <its words>`.
- **Example:** press <!-- key first-run.choose -->`return`<!-- /key --> on the box.
