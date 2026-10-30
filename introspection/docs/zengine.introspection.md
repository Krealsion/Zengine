# zengine.introspection -- Loaded, Project and Powers

**Manual.** The panes that show what the running program is made of: `Loaded`, which weaves the
kernel has loaded; `Project`, what your project asked for and what came of it; and `Powers`, which
powers the program can use and who supplies each. One weave offers all of them, built as
`zengine-introspection`; other weaves reach it by its *office*, the name it answers to,
`zengine.introspection`. Its pane keys are `loaded`, `arrangement` (the pane named `Project`) and
`powers`, and this page is the manual of each.

## About

Each pane answers its own question from its own owner, and the panes disagree on purpose. `Loaded`
asks the kernel's Weave Manager which weaves it loaded (`zen.ListLoaded`). `Project` asks the
host's arrangement door, [`zengine.arrangement`](zengine.arrangement.md), what the project's load
plan asked for and what realization made of each row. `Powers` asks the discovery door,
[`zengine.powers`](zengine.powers.md), which powers resolve here, and the sample door,
[`zengine.sources`](zengine.sources.md), to run a Source when you ask. A provider such as
`zengine-operators-basic` is a row of `Project` and absent from `Loaded`: no kernel loads it.

The panes report and do not act. None of them loads, unloads, mounts, overlays or replaces
anything, and browsing `Powers` runs no power: a Source runs once, when you ask for a sample.

A pane asks its owner when Workshop gives it room: when it opens, moves or changes size, and when a
reloaded weave offers it again. `Loaded` asks again when you scroll it or change its search,
`Project` when you scroll it, and `Powers` when your search or your selection changes. Between
readings the rows are a snapshot, and the pane's last line says whose. A host that mounts no
arrangement or discovery door leaves `Project` and `Powers` at `(waiting for the provider)`.

## Use

Open any of them from the Pane Manager, <!-- key desktop.panes -->`ctrl+p`<!-- /key -->. Keys act
only while a pane holds your keys: press into it first.

- **Loaded**: press a weave's row to select it. The row is marked `> `, and the pane says which
  weave you chose ([Follow](#follow)); the Composer takes it as its target. The wheel scrolls the
  list without moving your choice. Type to search: the rows narrow to the weaves whose name or
  office holds it, ignoring case, and Backspace widens them; a choice the search hides stays
  chosen. Escape lets the choice go; with nothing chosen, Escape puts the pane down.
- **Project**: read it. The wheel moves its list an artifact a notch, and so do
  [`project.up`](#projectup) and [`project.down`](#projectdown) once it holds your keys; a press
  gives it the keys and does nothing else.
- **Powers**: type to search; the query is the pane's one field, edited, copied and pasted as any
  text field is. Text carrying a byte that is not printable ASCII is refused whole. Choose a view
  ([`powers.view`](#powersview)), walk the list ([`powers.down`](#powersdown)) or press a power to
  select it, and sample a selected Source ([`powers.sample`](#powerssample)). Press a power and
  drag to carry its operator reference, `zengine.OperatorRef`, to Inventory or Flow; a form such
  as `fold` carries none. Escape lets the selection go.

A press on a power selects it and nothing more; `[ Sample ]` samples and selects nothing.

Each layout keeps three settings for Powers, set in Info or by message
([`PaneSettingCommitRequested`](../../workshop/docs/zengine.layouts.md#panesettingcommitrequested)): its first
row's columns, `view-columns`, pinned, and `composite-columns`, each at least its control's label
and padded past it, and `gap`, the spaces between the row's parts. A value one cannot use keeps
the default and is said to Workshop; Loaded and Project take none.

## Commands

The `Powers` pane's, then `Project`'s; `Loaded` declares none.

### `powers.view`

Switches between the two views of the one catalog: *Sources*, which answer with nothing supplied by
you, and *Operators*, which transform what you give them (a conversion, one version of a shape in
and another out, is among them). Which view a power is in is read off its definition when asked.

- **Takes:** the view shown. Default key <!-- key powers.view -->`tab`<!-- /key -->; a press on
  `Sources` or `Operators` in the first row does [`powers.sources`](#powerssources) or
  [`powers.operators`](#powersoperators).
- **Answers:** the brackets move, `[Sources] Operators` or `Sources [Operators]`, and the list is
  asked again of the discovery door ([`FindPowers`](zengine.powers.md#findpowers)), with the
  selected power's description ([`DescribePower`](zengine.powers.md#describepower)). Each view keeps
  its own selection, by identity: leave a view and come back, and your power is still selected
  there.
- **Refuses:** nothing.
- **Example:** press <!-- key powers.view -->`tab`<!-- /key --> and read `Sources [Operators]  -/3`.

### `powers.up`

Moves the selection to the previous power the list shows. A selection the search, the filter or a
short pane hides is still held; moving starts again from the list's first row.

- **Takes:** the list on screen. Default key <!-- key powers.up -->`up`<!-- /key -->; the wheel moves
  a row a notch.
- **Answers:** the `> ` mark and the position, `2/3`, move, and the discovery door is asked about
  the power now selected ([`DescribePower`](zengine.powers.md#describepower)); its
  detail shows under the list ([`yields`](#yields), [`active`](#active)).
- **Refuses:** nothing; at the first row, or in an empty list, nothing moves.
- **Example:** press <!-- key powers.up -->`up`<!-- /key -->.

### `powers.down`

Moves the selection to the next power the list shows. Otherwise as [`powers.up`](#powersup).

- **Takes:** the list on screen. Default key <!-- key powers.down -->`down`<!-- /key -->.
- **Answers:** the mark moves down one, and the door is asked about the power now selected
  ([`DescribePower`](zengine.powers.md#describepower)).
- **Refuses:** nothing; at the last row nothing moves.
- **Example:** in a cold pane, press <!-- key powers.down -->`down`<!-- /key --> to select the first
  power, `1/3`.

### `powers.sample`

Samples the selected Source: the host runs it once, now, and the pane keeps what it said.

- **Takes:** the selected power, in the Sources view. Default key
  <!-- key powers.sample -->`return`<!-- /key -->; the control `[ Sample ]`, under a selected Source.
- **Answers:** asks the sample door to run it
  ([`SampleRequested`](zengine.sources.md#samplerequested)). Its answer shows under the detail as
  `sampled when asked  zengine.project.anchor` and the value's lines, or `sample refused when asked`
  and the refusal in its owner's words. What a sample said is history: it stays through view
  switches, searches, a new selection and its provider unloading, and nothing refreshes it. Each
  press runs the Source again.
- **Refuses:** nothing it can say: with nothing selected, or in the Operators view, there is
  nothing to sample and nothing is asked, since sampling supplies no inputs. The door's refusals
  show as the sample, such as `unresolved operator reference 'zengine.project.anchor'` for a power
  gone since you selected it.
- **Example:** select `zengine.project.anchor`, press <!-- key powers.sample -->`return`<!-- /key -->.

### `powers.sources`

Shows the Sources view. Otherwise as [`powers.view`](#powersview).

- **Takes:** the view shown. No default key; a press on `Sources` in the first row.
- **Answers:** `[Sources] Operators`, and the list asked again
  ([`FindPowers`](zengine.powers.md#findpowers)) with the selected power's description
  ([`DescribePower`](zengine.powers.md#describepower)); nothing when Sources is shown.
- **Refuses:** nothing.
- **Example:** press `Sources`.

### `powers.operators`

Shows the Operators view. Otherwise as [`powers.view`](#powersview).

- **Takes:** the view shown. No default key; a press on `Operators` in the first row.
- **Answers:** `Sources [Operators]`, and the list asked again
  ([`FindPowers`](zengine.powers.md#findpowers)) with the selected power's description
  ([`DescribePower`](zengine.powers.md#describepower)); nothing when Operators is shown.
- **Refuses:** nothing.
- **Example:** press `Operators`.

### `powers.composite`

Shows only the powers whose contribution in force is composite, or every power again. *Composite*
says one thing: the implementation in force has known compositional structure. It does not mean a
power can be opened or edited.

- **Takes:** the filter, in both views at once. No default key; the control `[ ] Composite`, drawn
  in the first row when the pane is wide enough.
- **Answers:** the control reads `[x] Composite` or `[ ] Composite`, and the list is asked again of
  the discovery door ([`FindPowers`](zengine.powers.md#findpowers)). A composite
  power's row ends ` (composite)`.
- **Refuses:** nothing; a filter nothing passes shows `none here match the current search`.
- **Example:** press `[ ] Composite` in the Operators view.

### `project.up`

Moves Project's list up an artifact: the block above the first one shown comes into the pane,
whole.

- **Takes:** the list on screen. Default key <!-- key project.up -->`up`<!-- /key -->; the wheel
  moves it an artifact a notch.
- **Answers:** the arrangement door is asked again
  ([`ArrangementRequested`](zengine.arrangement.md#arrangementrequested)), and its answer is
  drawn from the artifact now first; the row that counts what is not shown says what is above it
  too, `  ... 1 earlier, 3 more`.
- **Refuses:** nothing; at the first artifact, or with every artifact shown, nothing moves and
  nothing is asked.
- **Example:** press <!-- key project.up -->`up`<!-- /key -->.

### `project.down`

Moves Project's list down an artifact. Otherwise as [`project.up`](#projectup).

- **Takes:** the list on screen. Default key <!-- key project.down -->`down`<!-- /key -->.
- **Answers:** the door is asked again
  ([`ArrangementRequested`](zengine.arrangement.md#arrangementrequested)), and the next artifact
  comes into the pane.
- **Refuses:** nothing; once the last artifact shows, nothing moves and nothing is asked.
- **Example:** in a pane that counts `  ... 5 more`, press <!-- key project.down -->`down`<!-- /key -->.

## Follow

A press on a weave's row of `Loaded` publishes `LoadedSelected` v1 as `zengine.introspection`:
`pane`, the pane key `loaded`; `library`, the name the kernel loaded the weave under; `role`, the
office it was bound to at load, empty when none. An observer accepts the shape and reads whose fact
it is from the authored office. It is said once a press, again when the same row is pressed again,
and never when a selection clears. The name is not a weave id and not proof the weave runs now, and
hearing it grants no reach to the weave it names. Nothing else is said unasked but
`PaneSettingsAsked`, to the layouts' owner, for Powers' row when the office arrives, and
`PaneSettingUnused`, to it, when a layout hands a pane a setting it cannot use.

## Shows

### `loaded`

`loaded weaves -- 4`, then a row a weave in the kernel's own order (by name): `  zengine-timer
@zengine.timer`, `@(no role)` when it was bound none, `> ` when chosen. A list that does not fit is
windowed and counted, `  ... 2 earlier, 3 more`. Then `in-process weaves are not in the kernel's
map`: Workshop's own weaves are not in this list. `snapshot from zen.ListLoaded` when there is room.
Searched: `loaded weaves -- 1 of 4 match [timer__________]`, a press on the box placing its caret.

### `arrangement`

`6 of 6 artifacts resolved -- 2 providers, 5 weaves` (with `, 1 unavailable` when an optional row
was stepped over), then a block an authored artifact, in plan order: its stem, its
[`authored`](#authored) row, then its [`resolved`](#resolved) rows or its [state](#state). A block
is shown whole or counted, `  ... 3 more`, and `  ... 2 earlier, 3 more` once the list is moved
down. Then `in-process participants are not authored
artifacts`, and `plan: <the file the host read>` when there is room.

### `authored`

`    authored  provider normal, weave zengine.timer`: what the plan file says of the artifact, its
provider's mount mode (`normal` or `overlay`) and the office its weave is to hold. What a person
wrote, and would be true again tomorrow.

### `resolved`

`    resolved  provider zengine.operators.basic, 2 powers`: the identity the artifact declared for
itself and how many powers it installed. `    resolved  weave #11, operator host offered`: the
weave id this kernel minted this run, and how the operator handoff around its load ended. No
resolved office: `Loaded` shows it.

### `state`

A row not running says where realization has got with it: `(loading)` and `(not reached)`, a
project coming up; `(refused)`; `(switched: its office is held by another choice)`; `(unavailable:
optional, and this run stepped over it)`. Then `    why   ` the refusing layer's sentence and
`    next  ` what you can do, from [`zengine.arrangement`](zengine.arrangement.md#arrangementrequested).

### `powers`

The first row: `[Sources] Operators  2/3  [ ] Composite  find:larger`, the view, where the
selection is in the list (`-` for none), the filter and the query; a narrow pane drops the filter,
then the query, then the position. Then a row a power, in the catalog's order, ending ` (composite)`
or ` (not offered)`; `  ... 2 above, 5 below`; `  + 12 more match -- narrow the search`. Or `no
sources resolve here`, `no operators resolve here`, `none here match the current search`.

### `yields`

Under the selected power: `  yields zengine.ProjectAnchor v1`, what a sample would produce, read
off its definition and not run; `[ Sample ]` for a Source; what it is for, in its contributor's
words (a byte a row cannot carry drawn `?`); `  not offered for reuse: another participant's own`
when so; its signature.

### `active`

`  active    zengine.operators.basic`: the contribution an evaluation spends now, ` (composite)`
when it is; `  shadowed  <provider> -- <its words>` for each contribution an overlay covers;
`(this host)` for one the host published with no provider's name.

### `sampled`

`sampled when asked  zengine.project.anchor`, then the value's lines, or `sample refused when asked`
and the refusal: what the Source said when you asked, never refreshed. Lines past the room are
counted, `  ... 2 more`.

### `small-print`

The last rows, each when there is room: `this pane describes this host's operator resolution
only`; `3 powers resolve here -- from 2 providers`, the whole catalog the door read; and `snapshot
from zengine.powers, when the search is asked`.

## Files

The panes read and write no file. `Project`'s last row names the plan file the host read.

## See also

- [`zengine.arrangement`](zengine.arrangement.md), [`zengine.powers`](zengine.powers.md) and
  [`zengine.sources`](zengine.sources.md), the offices the panes ask.
- [A picture of `Powers`](images/powers-find.png), searching the Operators for `larger`.
- [Sources and Operators](../../operator/docs/operator-sources.md), and
  [what a contribution says about itself](../../operator/docs/operator-providers.md#what-a-contribution-says-about-itself).
- [Inventory to Compose](../../inventory/docs/inventory-compose.md), where a weave chosen in
  `Loaded` is composed for.
- The pane protocol the panes speak: [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp);
  the manual's: [`workshop/pane_document.hpp`](../../workshop/pane_document.hpp).
