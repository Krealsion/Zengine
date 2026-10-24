# zengine.composer -- Compose

**Manual.** The pane you write a message in. Select a weave in the Loaded pane, and Compose asks it
which messages it accepts; choose one, fill in its fields and submit it, and the message goes to
that weave. It is a *weave*: a piece of Workshop that is loaded, replaced and spoken to by messages,
built as `zengine-composer`. Other weaves reach it by its *office*, the name it answers to,
`zengine.composer`, and it offers one pane, `compose`. New to it? Start with
[reuse a stored command through Compose](../../inventory/docs/inventory-compose.md).

## About

Compose holds no form of its own. When you select a weave, it asks that weave's office what it
accepts (`zen.DescribeAccepted`, Loom's own question) and lists every message shape in the answer by
name and version, `StartTimer v1`. Choosing one opens a form built from the shape itself: one row
per field, in the order the shape declares them, each with the type the shape spells (`Int`, `Text`,
`List<Bytes>`). A field starts *absent*; typing into it makes it present, and present-and-empty is
not the same as absent. A field of a message, a list or bytes cannot be typed: it takes a value
dropped onto it.

Submitting sends the message to the office the selection named, from `zengine.composer`, once
Workshop has said the gesture's actor may send that shape to that office. Compose says
`SUBMITTED -- queued; outcome belongs to the receiver` and nothing stronger: what the message does
is the receiving weave's to say. A finished command, or an unfinished preset, can be kept in
Inventory and dropped back onto Compose; a drop fills the form, and only Submit sends it.

Reloaded in place, Compose keeps the weave you selected and the form you are writing: each
field's presence and text, valid or not, and the values a drop gave it. It asks the weave again
what it accepts, and opens the form on the answer, saying `The form was kept across a reload --
review, then Submit`; a message the weave no longer accepts drops the form, `The form for Curious
v2 was dropped: @zengine.stranger no longer accepts it`. A submission or a store still waiting is
dropped, `The submission waiting when Compose was reloaded was dropped; submit again`, and a paste
in flight is not carried.

## Use

Open it from the Pane Manager, <!-- key desktop.panes -->`ctrl+p`<!-- /key -->, then press a row in
the Loaded pane. Its keys act only while it holds your keys: press into it first.
[`compose.up`](#composeup) and [`compose.down`](#composedown) move the cursor, and the mouse wheel
moves it too; choose a message with [`compose.enter`](#composeenter), or press its row. In the
form, [`compose.include`](#composeinclude) includes the field under the cursor or leaves it absent
(a `Bool` field goes absent, `false`, `true`, absent), typing writes its value, and the ordinary
editing keys move, select, copy and paste inside it. [`compose.back`](#composeback) leaves the form
for the list; at the list a bare Escape is handed back to Workshop. A key no command names changes
nothing. A right press opens Workshop's pane menu.

Drop an Inventory entry onto a field's row to make it that field's value, or onto any other row to
fill the whole form with the command or preset it holds. A drop never sends. It is refused, and the
notice says why: `Drop refused: form is busy or its picture changed`; `Drop refused: Field already
has a value; Tab excludes it before replacement`; `Drop refused: Existing draft kept; go Back before
dropping another command`; `Drop refused: Target does not accept this message shape`; `Drop refused:
Unsupported reference kind`; or `Drop refused: ` and the reason the value does not fit the field.
A drop that worked says `Copied data into form -- review, then Submit`.

While a submission or a store waits on Workshop or Inventory, the form is frozen: no key, press,
command or selection changes it until the answer comes.

## Commands

### `compose.enter`

Acts on the row under the cursor: in the list, opens the form on that message; in the form, on
`[ Submit ]` it submits and on `[ Back ]` it goes back. On a field row it does nothing.

- **Takes:** the cursor's row. Default key <!-- key compose.enter -->`return`<!-- /key -->; a press
  on a message's row does the same.
- **Answers:** the form, headed `StartTimer v1 -> @zengine.timer`, its notice `up/down move, tab
  include, enter acts, esc back`; on a control, as [`compose.submit`](#composesubmit) or
  [`compose.back`](#composeback).
- **Refuses:** nothing is said: on a field row, while nothing is listed, or while the form is frozen,
  nothing happens.
- **Example:** select the Timer in the Loaded pane, move to `StartTimer v1`, press
  <!-- key compose.enter -->`return`<!-- /key -->.

### `compose.submit`

Sends the message the form composes to the office it is addressed to. Every required field must be
present and every value must fit its field's type; an optional field left absent is left out.

- **Takes:** the open form. Default key <!-- key compose.submit -->`ctrl+return`<!-- /key -->
  (`ctrl+e` from a POSIX terminal); a press on `[ Submit ]`.
- **Answers:** first `Checking submission authority`, while Workshop judges whether your gesture's
  actor may send that shape to that office. Then the message goes, sent as `zengine.composer`, and
  the notice is `SUBMITTED -- queued; outcome belongs to the receiver`. The message is the shape you
  chose, whatever the target accepts; Compose waits for no answer to it, and shows only a refusal.
- **Refuses:** before anything is sent: `still needed: id, delay_ms`, naming each required field
  still absent; a value that does not fit its field, as `` field 'delay_ms' (Int) cannot take this
  value `` for `1O00`; `the input actor has no authority for this operation`, or Workshop's
  other reason; `Permission request could not be queued`. After: `Submission could not be queued`;
  `Submission was not delivered: ` and Loom's reason; `Receiver refused: ` and the receiver's.
- **Example:** fill `id` with `tick` and `delay_ms` with `1000`, Tab `repeat` to `false`, press
  <!-- key compose.submit -->`ctrl+return`<!-- /key -->.

### `compose.store`

Keeps the finished command in Inventory, labelled with the shape's name, beside a note of the office
it was written for. Nothing is sent to the target.

- **Takes:** the open form, complete. Default key <!-- key compose.store -->`ctrl+s`<!-- /key -->.
- **Answers:** `Checking this operation's authority` while Workshop judges the store, `Waiting for
  inventory` while the Inventory adds it
  ([`InventoryAdd`](../../inventory/docs/zengine.inventory.md#inventoryadd)), then `Stored as a new
  inventory entry`.
- **Refuses:** `Complete the command before storing it`; Workshop's reason, as `the input actor has
  no authority for this operation`; `The permission request could not be queued`; `The inventory
  request could not be queued`; `Inventory operation was not delivered: ` and Loom's reason;
  Inventory's own refusal; `Inventory answered about a different entry`. With no form open, nothing
  happens.
- **Example:** with every required field filled, press
  <!-- key compose.store -->`ctrl+s`<!-- /key -->.

### `compose.preset`

Keeps the form as it stands, finished or not, as a preset in Inventory, labelled `<shape> preset`:
the fields you included and their values, against the shape they belong to. A field you left absent
or unset is not in it.

- **Takes:** the open form. Default key <!-- key compose.preset -->`ctrl+b`<!-- /key -->.
- **Answers:** as [`compose.store`](#composestore): `Checking this operation's authority`, `Waiting
  for inventory` ([`InventoryAdd`](../../inventory/docs/zengine.inventory.md#inventoryadd)), `Stored
  as a new inventory entry`.
- **Refuses:** the refusals of [`compose.store`](#composestore) after its first, and a value that
  cannot be kept in its own words. With no form open, nothing happens.
- **Example:** fill `id` only, press <!-- key compose.preset -->`ctrl+b`<!-- /key -->; drop the preset
  back later and finish it.

### `compose.unset`

Makes the field under the cursor absent again and throws its value away; including it again starts
it empty.

- **Takes:** the field under the cursor. Default key <!-- key compose.unset -->`ctrl+u`<!-- /key -->.
- **Answers:** the row reads `(required)` or `(absent)`, and the notice `Field unset; its old value
  was discarded`.
- **Refuses:** nothing is said: off a field row, or while the form is frozen, nothing happens.
- **Example:** on `delay_ms`, press <!-- key compose.unset -->`ctrl+u`<!-- /key -->.

### `compose.back`

Leaves the form and returns to the list of messages; the form and everything typed in it go.

- **Takes:** the open form. Default key <!-- key compose.back -->`escape`<!-- /key --> in the form,
  none in the list; a press on `[ Back ]` does the same.
- **Answers:** the list, its cursor on the first message.
- **Refuses:** nothing is said: in the list, or while the form is frozen, nothing happens.
- **Example:** press `[ Back ]`.

### `compose.up`

Moves the cursor one row up, in the list or in the form; it stops at the first row.

- **Takes:** the cursor. Default key <!-- key compose.up -->`up`<!-- /key -->; the mouse wheel moves it
  too.
- **Answers:** the cursor on the row above.
- **Refuses:** nothing is said: at the first row, while nothing is listed, or while the form is
  frozen, nothing happens.
- **Example:** in the form, press <!-- key compose.up -->`up`<!-- /key --> to reach the field above.

### `compose.down`

Moves the cursor one row down, in the list or in the form; it stops at the last row.

- **Takes:** the cursor. Default key <!-- key compose.down -->`down`<!-- /key -->; the mouse wheel moves
  it too.
- **Answers:** the cursor on the row below.
- **Refuses:** nothing is said: at the last row, while nothing is listed, or while the form is
  frozen, nothing happens.
- **Example:** in the list, press <!-- key compose.down -->`down`<!-- /key --> to reach the next message.

### `compose.include`

Includes the field under the cursor or leaves it absent; a `Bool` field goes absent, `false`,
`true`, absent.

- **Takes:** the field under the cursor. Default key <!-- key compose.include -->`tab`<!-- /key --> in
  the form, none in the list.
- **Answers:** the row shows the field included, or reads `(required)` or `(absent)` once it is
  left out.
- **Refuses:** nothing is said: off a field row, on a field Compose cannot type that holds no
  value, or while the form is frozen, nothing happens.
- **Example:** on `repeat`, press <!-- key compose.include -->`tab`<!-- /key --> to make it `false`.

## Hears

The pane protocol's shapes are Workshop's, declared in
[`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp) and the headers it includes;
asked about one, Compose answers that its page has no section for it.

### `LoadedSelected`

`LoadedSelected` v1, from `zengine.introspection`
([`introspection/vocabulary.hpp`](../../introspection/vocabulary.hpp)): the weave you pressed in the
Loaded pane. Compose drops the last target's list and form and asks the new one what it accepts.
From any other office it is ignored.

### `ClipboardCopy`

`ClipboardCopy` v1 ([`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp)): a copy made anywhere
in Workshop, kept so that a paste into a field has it.

### `ClipboardText`

`ClipboardText` v1, from the window that shows the pane: the text a paste into a field asked for. It
lands in the field that asked, and nowhere once that form or field has changed.

### `InventoryEntry`

`InventoryEntry` v1, from `zengine.inventory`
([`inventory/vocabulary.hpp`](../../inventory/vocabulary.hpp)): the entry a store or a preset made.

### `AreaRoom`

`AreaRoom` v4, from the graphics window that shows the area `zengine.composer`, as that window's
office ([`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp)): Compose's room there and the
window's text. Compose draws its rows in it over the room's ground, a line of that text each, its
caret a bar, whole in a new window, incarnation, size or measure, or an empty area, then only what
changed.

### `DrawnRefused`

`DrawnRefused` v1, from that window: rows it refused, and why. What it shows stays as it was. A
refused change is followed at once by the rows last sent, whole; after a refused whole drawing,
nothing is sent until Compose's rows change, and that drawing goes whole.

### `ActionRequested`

`ActionRequested` v1, from that window, where it shows Compose's area deaf to the pointer: a press
on one of Compose's rows there, by the row's name, which is the press Workshop's canvas makes on that
row.

### `AreaButtonChanged`

`AreaButtonChanged` v3, from that window: a button pressed in Compose's area, at its point, with
what the window drew there. The first button on a row presses that row, read against whether the
keys were already Compose's as Workshop last said; the third is handed back, under the window's
number for the press, so Workshop's own pane menu opens there. One the window landed on another row
than Compose shows there now is refused in words. A release means nothing.

### `AreaPointerMoved`

`AreaPointerMoved` v1, from that window: the pointer moved in Compose's area. It means nothing to
rows.

### `AreaWheelTurned`

`AreaWheelTurned` v1, from that window: the wheel over Compose's area, which is the wheel on
Workshop's canvas.

## Follow

Compose publishes one thing: `ClipboardCopy` when you copy or cut inside a field, so the rest of
Workshop can paste it. Everything else it says goes to Workshop, as its rows, or to one office as a
question. What a submitted message did is its receiver's to say: follow that office. Compose's own
state, `ComposerState`, is its counters -- how often it offered its pane, was given room, took a
target, asked, heard an answer, refused a sender, and handed a message to the bus (`submitted`
counts messages queued, not delivered) -- and what a reload in place keeps: the target, the form
open on it, and what the reload ended before it was answered.

## Shows

### `to`

The office a message is addressed to, `to @zengine.timer`. With nothing selected the pane reads
`no weave selected` and `press a row in the Loaded pane`; for a weave with no office, `selected
<library>` and `(no messaging role observed -- nothing to address)`.

### `accepted`

How many messages the target accepts, `accepted messages -- 11`; until it answers, `asked what it
accepts -- no answer observed yet`. An ask that fails says so in the notice: `Discovery could not be
queued`, `Discovery was not delivered: ` and Loom's reason, `the discovery request was refused`, or
`could not read the answer -- ` and why.

### `message`

One accepted message, `StartTimer v1`, the cursor's marked `>`. A list longer than the pane ends
with `... 3 above, 5 below`.

### `form`

The message being written and where it goes, `StartTimer v1 -> @zengine.timer`.

### `field`

One field, `name:Type` and its value: `[tick]` present, `[]` present and empty, `(required)` or
`(absent)` when absent, `[false]` a chosen false, `(drop compatible value)` or `(no text form; drop
complete message)` for a field that takes only a drop, `[copied message/list]` once it has one.

### `controls`

`[ Submit ]` and `[ Back ]`, at the foot of the form.

### `notice`

The last thing Compose has to say: a hint, an outcome or a refusal, in the words above.

### `from`

The library the target was loaded from, `from zengine-timer`, when there is room.

## Files

Compose reads and writes no file. What it stores lives in Inventory, with the note
[`ComposerCommandContext`](../command_context.hpp) beside it.

## See also

- [Reuse a stored command through Compose](../../inventory/docs/inventory-compose.md): choose a
  message, fill it, store it, bring it back and send it, step by step.
- [Reusable message and value drafts](../../message-draft/docs/message-drafts.md): the values and
  presets a form is made of.
- [`composer/vocabulary.hpp`](../vocabulary.hpp): the office, the pane and the names a weaver reads.
