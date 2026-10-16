# zengine.info -- Info

**Manual.** The pane for looking closely at things: the panes on your desk and the properties of the
one you pick, and typed values kept in Inventory, several at once. It is a *weave*: a piece of
Workshop that is loaded, replaced and spoken to by messages, built as `zengine-info-pane`. Other
weaves reach it by its *office*, the name it answers to, `zengine.info`. Its panes are `info`, the
default, and the value views `info.2`, `info.3` and on, up to `kMaxInfoViews` in all
([`info/vocabulary.hpp`](../vocabulary.hpp)). New to it? Start with
[Independent Info views and the inspection workbench](info-views.md).

## About

The default pane, `info`, holds two lists: `PANES`, every pane Workshop knows, and the properties of
the one you chose to inspect, where you can change a value. A *value view* holds one value instead:
a sample captured into Inventory, an unfinished command, a stored note. Each view keeps its own
value, edits, selection and the asks it waits on, so nothing you do in one view, or in another
pane, replaces what another shows. The default pane becomes a value view while it holds a value.

Info owns nothing it shows. The list of panes and the inspected pane's rows are Workshop's
reading, told to Info whole each time they change
([`PaneInventory`](../../workshop/desktop_seam_vocabulary.hpp),
[`PaneSubjectShown`](../../workshop/inspection_seam_vocabulary.hpp)). Info names the pane to inspect
by asking Workshop, and a property you change is written into the desk by Workshop, whose rows they
are. A value is Inventory's: every read, save and copy asks Workshop first whether you may, then
asks Inventory, and a refusal is said in the owner's words.

A reload of Info keeps where you were in the default pane: the pane the list's cursor holds, the
property row, which list has the keys. It keeps every value view too, as `InfoPaneState` carries
it: its title, the value or draft it holds with your unsaved edits, the entry it is linked to at
the revision it read, and the field selected. Nothing is watched after it: a view that was watching
says `Watch ended when Info was reloaded; Watch follows the entry again`, and `Refresh` or `Watch`
asks again at your press. A restart of Workshop shows only the default pane.

## Use

Workshop's default desk shows Info in its right column; the Pane Manager,
<!-- key desktop.panes -->`ctrl+p`<!-- /key -->, opens any of its panes. Its keys act only while it
holds your keys: press into it first. In the lists, press a pane's row to inspect it, then a
property's row and <!-- key info.edit -->`return`<!-- /key --> to change its value. To open a value,
drag an Inventory entry onto a view for a copy, or grab it live in Inventory and press the view to
link it; <!-- key info.view.new -->`ctrl+n`<!-- /key --> opens another view.

A value view draws a control for each of its commands: `[Save]` acts, `(Save)` cannot act now and
still answers when pressed, with its reason, `` Save unavailable: <the reason> ``. `[...]` opens
the view's menu, which holds every command the width left out.

The sentence above the rows is the answer to your last act, and your next act clears it. A refusal
from an owner is said in the owner's words.

## Commands

These are the default pane's while it shows its lists. The same keys mean the list's act or the
properties' act, by the list that holds the keys.

### `info.up`

Moves the cursor of the list that holds the keys up one row.

- **Takes:** the list with the keys. In the properties it steps over a section's heading, which holds
  no value. Default key <!-- key info.up -->`up`<!-- /key -->; its label says `pane up` or `row up`.
- **Answers:** the `>` moves. The pane list's cursor holds a pane by identity, so a pane added above
  moves the cursor with it.
- **Refuses:** nothing; at the first row nothing moves.
- **Example:** <!-- key info.up -->`up`<!-- /key -->.

### `info.down`

Moves the cursor of the list that holds the keys down one row.

- **Takes:** the list with the keys. Default key <!-- key info.down -->`down`<!-- /key -->; its
  label says `pane down` or `row down`.
- **Answers:** the `>` moves, as [`info.up`](#infoup).
- **Refuses:** nothing; at the last row nothing moves.
- **Example:** <!-- key info.down -->`down`<!-- /key -->.

### `info.edit`

In the pane list, inspects the pane under the cursor; in the properties, opens a draft on the
cursor's row ([commands in a property draft](#commands-in-a-property-draft)). A press on a pane's row
inspects it too, and a press on a property's row puts the cursor there.

- **Takes:** the cursor's row. Default key <!-- key info.edit -->`return`<!-- /key -->; its label
  says `inspect` or `edit`.
- **Answers:** to inspect, asks Workshop to make that pane the subject
  ([`InspectPaneRequested`](../../workshop/docs/zengine.workshop.md#inspectpanerequested)), and
  answers an ask this pane was still holding with [`PaneSubjectActed`](#inspectpanerequested),
  replaced; the properties then show its rows under `PANE <its name>`, and `*` marks it in the list.
  A draft holds the row's value, the caret at its end.
- **Refuses:** `Return inspected nothing -- choose a row first`, when the pane the cursor held left
  the list; `inspect not submitted -- nothing was queued`; `inspect not delivered -- nothing changed
  (<the bus's reason>)`; Workshop's own words for a pane it will not inspect. In the properties:
  `nothing is inspected yet -- Return on a pane inspects it`, and `` Window is not authored -- it is
  what the screen makes of the authored value `` for a row the screen makes, drawn dim.
- **Example:** press <!-- key info.edit -->`return`<!-- /key --> on `Layouts`, then
  <!-- key info.switch -->`tab`<!-- /key -->.

### `info.switch`

Moves the keys between the pane list and the properties.

- **Takes:** the list with the keys. Default key <!-- key info.switch -->`tab`<!-- /key -->; its
  label says `properties` or `panes`.
- **Answers:** the `>` moves to the other list, and [`info.up`](#infoup),
  [`info.down`](#infodown) and [`info.edit`](#infoedit) act there.
- **Refuses:** `nothing is inspected yet -- Return on a pane inspects it`, from the pane list while
  nothing is inspected. When the subject's rows go, the keys go back to the pane list.
- **Example:** <!-- key info.switch -->`tab`<!-- /key -->.

### `info.inventory`

Shows the value the default pane holds again, in place of the lists. Declared only while it holds
one and no draft is open.

- **Takes:** the value the default pane last held. Default key
  <!-- key info.inventory -->`ctrl+i`<!-- /key -->; in the lists' menu, `Show the value view`.
- **Answers:** the default pane is a value view again, as it was
  ([commands in a value view](#commands-in-a-value-view)).
- **Refuses:** nothing.
- **Example:** <!-- key info.inventory -->`ctrl+i`<!-- /key -->.

### `info.view.new`

Opens the lowest unused value view, shown on the desk, empty. From the lists and from any value view.

- **Takes:** a free view. Default key <!-- key info.view.new -->`ctrl+n`<!-- /key -->; in a value
  view the control `[New]`; in the lists' menu, `New value view`.
- **Answers:** asks Workshop to show the view
  ([`PaneLaunchRequested`](../../workshop/docs/zengine.workshop.md#panelaunchrequested)). The view
  that asked says `Opened Info 2`; the new one says `New view: drag an Inventory entry here, or a
  field onto a field`. A view is offered to Workshop the first time it is used and kept after, so
  the Pane Manager lists only views that were used.
- **Refuses:** `All four Info views are in use -- close one first`, ending ` (hidden: Info 3; the
  Pane Manager reopens them)` when some are only hidden; `New view not shown: <Workshop's reason>`.
  While a draft is open in the lists it is not declared.
- **Example:** <!-- key info.view.new -->`ctrl+n`<!-- /key -->, then drag an entry onto `Info 2`.

## Commands in a property draft

While a draft is open, the default pane declares these two and nothing else: every other key edits
the line, as any text line does ([`component/text_box.hpp`](../../component/text_box.hpp)), its
clipboard included. A press on another pane's row is refused, `finish the edit first -- commit it
or cancel it`. When Workshop's picture no longer shows the row the draft was opened on (another
pane, another desk put live, the rows' layout changed), the draft ends: `edit abandoned -- the
inspected pane, its desk or its rows changed; unwritten changes discarded`.

### `info.commit`

Asks for the draft's text to be written into its property.

- **Takes:** the draft: the rows it was opened on, its row and its text. Default key
  <!-- key info.commit -->`return`<!-- /key -->.
- **Answers:** asks Workshop to write it into the desk
  ([`PaneCommitRequested`](../../workshop/docs/zengine.workshop.md#panecommitrequested)), which it
  does only while its rows are still the ones the draft was opened on. Written, the draft closes and
  the row shows the value. Refused, the draft stays, beside the owner's words. An answer that comes
  after you typed more, or after the draft closed, says `earlier commit written -- later edits not
  sent`, `earlier commit refused -- <the words>`, `commit written -- it was sent before the draft
  closed` or `commit refused -- <the words>`.
- **Refuses:** `commit not sent -- an earlier commit is still unanswered`, keeping the draft; `commit
  not submitted -- nothing was queued or written`; `commit not delivered -- nothing was written (<the
  bus's reason>)`. The draft and its text stand for the next press.
- **Example:** on `Width` of `Layouts`, type `12`, press
  <!-- key info.commit -->`return`<!-- /key -->.

### `info.cancel`

Closes the draft; nothing is written.

- **Takes:** the draft. Default key <!-- key info.cancel -->`escape`<!-- /key -->.
- **Answers:** `edit cancelled -- unwritten changes discarded`; with a commit still unanswered,
  `commit already sent -- the draft is closed, and it may still be written`.
- **Refuses:** nothing.
- **Example:** <!-- key info.cancel -->`escape`<!-- /key -->.

## Commands in a value view

A value view is `info.2` and on, always, and the default pane while it holds a value. A whole value
dropped on a view opens a copy, `Independent copy; Save stores it as a new entry, Save copy keeps it
independent`; an entry grabbed live in Inventory and placed on it is read and linked, `Linked rev 2`,
or `Link refused: <the reason>; this view still shows what it showed`. Neither replaces unsaved work:
`Unsaved edits kept: a whole value would replace this draft. Save, Save copy or discard first -- or
drag a single field onto a field row`. A field copy dropped on a field row fills that field alone,
`Filled count (was 1); Save stores it`, or refuses with nothing changed: `Drop a field onto a field
row of this view`, `Capture metadata is read-only; drop onto an item field`, `This view changed
during the drag; nothing was filled -- drop again`, `Field drop refused, nothing changed: <the
reason>`.

A command that cannot act now says `<its name> unavailable: <the reason>`: before a value,
`open a value first: drag an Inventory entry here`. While a Refresh, a link or a Sample waits, the
draft is frozen: an edit, a drop, Discard, Unset and Make preset say `wait: this view is reading its
entry again -- Stop keeps this draft` (or `opening 'Beta'`, `sampling zengine.input`), and change
nothing.

### `info.value.up`

Selects the field before the selected one. The wheel walks the fields too.

- **Takes:** the fields, the value's then its capture's metadata. Default key
  <!-- key info.value.up -->`up`<!-- /key -->.
- **Answers:** the `>` moves. A field is held by its path, so new data keeps it selected; a field the
  new data lacks leaves nothing selected until you choose one.
- **Refuses:** `A field pickup is pending`; at the first field nothing moves.
- **Example:** <!-- key info.value.up -->`up`<!-- /key -->.

### `info.value.down`

Selects the field after the selected one.

- **Takes:** the fields. Default key <!-- key info.value.down -->`down`<!-- /key -->.
- **Answers:** the `>` moves, as [`info.value.up`](#infovalueup).
- **Refuses:** as [`info.value.up`](#infovalueup); at the last field nothing moves.
- **Example:** <!-- key info.value.down -->`down`<!-- /key -->.

### `info.value.edit`

Opens a line on the selected field ([commands in a field edit](#commands-in-a-field-edit)).

- **Takes:** the selected field, or a press on its row first. Default key
  <!-- key info.value.edit -->`return`<!-- /key -->.
- **Answers:** the line `> 1`, holding the field's value; `Enter keeps it; Escape cancels`.
- **Refuses:** `Choose a field first`; `The selected field is no longer here; choose a field`;
  `Capture metadata is read-only`; `Select a scalar field inside this value; byte fields are
  read-only here`; frozen, `Edit field unavailable: wait: ...`.
- **Example:** press the `count` row, then <!-- key info.value.edit -->`return`<!-- /key -->.

### `info.value.save`

Stores the draft. A linked draft is written to its entry only if the entry still has the revision it
was read at; a copy is stored as a new entry, which the view then links.

- **Takes:** the draft. Default key <!-- key info.value.save -->`ctrl+s`<!-- /key -->; the control
  `[Save]`.
- **Answers:** asks Workshop whether you may (`PaneOperationRequested`), `Checking this operation's
  authority`, then Inventory
  ([`InventoryWrite`](../../inventory/docs/zengine.inventory.md#inventorywrite) or
  [`InventoryAdd`](../../inventory/docs/zengine.inventory.md#inventoryadd)), `Waiting for
  inventory`; then `Saved rev 2`, `Saved as a new inventory entry; this view now links it`, or,
  after more typing, `Saved the submitted draft as rev 2; newer edits remain unsaved`.
- **Refuses:** `Save unavailable: nothing to save: this draft matches rev 1`; `Save unavailable:
  wait: a save is waiting for its answer`; `Save refused: <the reason>`, and for an entry another
  view changed, `... -- your text is kept. Refresh replaces it; Save copy stores it as a new entry`.
- **Example:** edit `count`, press <!-- key info.value.save -->`ctrl+s`<!-- /key -->.

### `info.value.save-copy`

Stores the draft as a new, separately named entry, and leaves this view as it was: still linked, its
edits unsaved.

- **Takes:** the draft. Default key <!-- key info.value.save-copy -->`ctrl+shift+s`<!-- /key -->; the
  control `[Save copy]`.
- **Answers:** asks Workshop, then Inventory
  ([`InventoryAdd`](../../inventory/docs/zengine.inventory.md#inventoryadd)), under the entry's name
  and ` copy`; `Saved a new entry 'Note copy'; this view still holds its link to rev 2` (or `its
  independent copy`).
- **Refuses:** `Save copy refused: <the reason>; nothing was stored`; `Save copy unavailable: wait:
  ...`.
- **Example:** <!-- key info.value.save-copy -->`ctrl+shift+s`<!-- /key -->.

### `info.value.fresh`

Reads the linked entry again, replacing the draft. This is where newer data a watch held is taken.
For a copy, restores the value as it was received.

- **Takes:** the linked entry. Default key <!-- key info.value.fresh -->`ctrl+r`<!-- /key -->; the
  control `[Refresh]`.
- **Answers:** with unsaved edits, first `Unsaved draft: press Refresh again to discard it and read
  the entry again`; then asks Workshop and Inventory
  ([`InventoryRead`](../../inventory/docs/zengine.inventory.md#inventoryread)), `Read rev 3 from
  inventory`. A copy: `Restored the received copy; the source entry is independent`.
- **Refuses:** `Read refused: <the reason>; the draft is kept`; `Refresh unavailable: wait: ...`.
- **Example:** after `NEWER rev 3 waiting`, press <!-- key info.value.fresh -->`ctrl+r`<!-- /key -->
  twice.

### `info.value.watch`

Follows the linked entry while the view is on the desk, or pauses following it. When Inventory says
something changed, the view reads its entry once; a change during that read is one more read, never
a queue. A clean draft takes the new revision and marks what changed `~`; a draft with edits keeps
every character and says `NEWER rev 3 waiting (Refresh accepts)`.

- **Takes:** the linked entry. Default key <!-- key info.value.watch -->`ctrl+l`<!-- /key -->; the
  control `[Watch]`, `[Pause]` while watching.
- **Answers:** asks Workshop for leave to read the entry on each change
  ([`PaneObservationRequested`](../../workshop/docs/zengine.workshop.md#paneobservationrequested)),
  `Asking to watch this entry`, then reads it
  ([`InventoryRead`](../../inventory/docs/zengine.inventory.md#inventoryread)), `Watching rev 2;
  changes appear here, your edits are never replaced`. Again: `Watch paused; nothing is read until
  Watch is pressed again`, and the watch ends at Workshop
  ([`PaneObservationEnded`](../../workshop/docs/zengine.workshop.md#paneobservationended)). Hiding
  the view, closing it, opening another value in it or leaving for the lists ends the watch, at
  Workshop too: `Watch ended: the view was hidden`.
- **Refuses:** `Watch unavailable: an independent copy has no linked entry to watch; link one
  (right-click Grab live)`; `...: this link is stale: its entry is no longer here`; `...: wait:
  Workshop has not yet answered this view's last watch request`; `Watch refused: <the reason>`.
- **Example:** link an entry, press <!-- key info.value.watch -->`ctrl+l`<!-- /key -->.

### `info.value.sample`

Asks the source a captured structure came from to describe its structure again. The answer
replaces the draft, unsaved; what is stored changes only when you save.

- **Takes:** a value whose capture records the role that described itself. Default key
  <!-- key info.value.sample -->`ctrl+e`<!-- /key -->; the control `[Sample]`.
- **Answers:** asks Workshop whether you may ask that role for `zen.PokeDescribe`, `Checking
  authority to ask zengine.input to describe its structure again`, then the role
  ([`zen.PokeDescribe`](https://github.com/Krealsion/Loom/blob/main/include/zen/weave/poke.hpp));
  `Sampled zengine.input: #12 answered now; unsaved`, or `Different incarnation: #14 answered
  zengine.input (stored capture: #12); unsaved`. An equal id proves nothing across a restart, and
  the words say so.
- **Refuses:** `Sample source unavailable: no supported source is recorded (only captured structure
  descriptions can be sampled again)`; `...: this is a stored command; sampling never runs commands`;
  `...: a preset is a draft to finish, not an observation of a source`; `...: unsaved edits: save,
  save a copy or discard before sampling`; `Sample refused: <the reason>`; `Source zengine.input is
  unavailable (<the reason>); showing the stored observation, now stale`.
- **Example:** <!-- key info.value.sample -->`ctrl+e`<!-- /key -->.

### `info.value.preset`

Makes the draft an independent preset copy: an unfinished value to finish and store, whose fields
can be unset. A linked view's watch ends.

- **Takes:** the draft. Default key <!-- key info.value.preset -->`ctrl+b`<!-- /key -->; in the menu,
  `Make preset copy`.
- **Answers:** `Independent preset copy; Ctrl+U unsets a field, Save stores it`; the title row reads
  `PRESET COPY`. A watch the view held ends, at Workshop too
  ([`PaneObservationEnded`](../../workshop/docs/zengine.workshop.md#paneobservationended)).
- **Refuses:** `Make preset copy unavailable: wait: ...`.
- **Example:** <!-- key info.value.preset -->`ctrl+b`<!-- /key -->.

### `info.value.unset`

Unsets the selected field of a preset. Its type stays what it was.

- **Takes:** the selected item field of a preset. Default key
  <!-- key info.value.unset -->`ctrl+u`<!-- /key -->; in the menu, `Unset field`.
- **Answers:** `Field unset; the schema is unchanged. Save stores the preset`.
- **Refuses:** `Make a preset copy with Ctrl+B before unsetting item fields`; `Choose a field first`;
  frozen, `Unset field unavailable: wait: ...`.
- **Example:** <!-- key info.value.unset -->`ctrl+u`<!-- /key -->.

### `info.value.grab`

Picks up a typed copy of the selected field, to drop on a field of another view, or of Compose. A
press on a field row that moves with the button down picks it up the same way.

- **Takes:** the selected field. Default key <!-- key info.value.grab -->`ctrl+g`<!-- /key -->; in the
  menu, `Pick up field`.
- **Answers:** asks Workshop whether you may carry a value (`PaneOperationRequested`), `Checking
  field acquisition authority`, then hands Workshop the copy to carry
  ([`PaneValueCarryRequested`](../../workshop/docs/zengine.workshop.md#panevaluecarryrequested)).
- **Refuses:** `Choose a field first`; `Field pickup refused: <the reason>`; `Wait for the inventory
  operation before picking up a field`; `Field copy exceeds the carry limit`; `Field pickup was not
  delivered: <the bus's reason>`.
- **Example:** select `target_role`, press <!-- key info.value.grab -->`ctrl+g`<!-- /key -->.

### `info.value.discard`

Throws away the draft's edits: the value is the one last saved or read again.

- **Takes:** the draft. Default key <!-- key info.value.discard -->`ctrl+d`<!-- /key -->; in the menu,
  `Discard edits`.
- **Answers:** `Local edits discarded; this is the last saved/read copy`.
- **Refuses:** frozen, `Discard edits unavailable: wait: ...`.
- **Example:** <!-- key info.value.discard -->`ctrl+d`<!-- /key -->.

### `info.view.fork`

Opens the lowest unused view with a copy of this view's draft, and nothing that waits: no pending
ask, no watch. A linked draft stays linked to the same entry.

- **Takes:** this view's draft. Default key <!-- key info.view.fork -->`ctrl+shift+n`<!-- /key -->;
  the control `[Fork]`.
- **Answers:** asks Workshop to show the new view
  ([`PaneLaunchRequested`](../../workshop/docs/zengine.workshop.md#panelaunchrequested)): `Forked
  into Info 2 fork`, the new view's title; the new view says `Forked from Info 2: an independent
  local draft; watch is off`. Nothing is stored.
- **Refuses:** `Fork view unavailable: this view holds no value`; as
  [`info.view.new`](#infoviewnew) when no view is free.
- **Example:** <!-- key info.view.fork -->`ctrl+shift+n`<!-- /key -->.

### `info.view.rename`

Opens a line for the view's name ([commands on the rename line](#commands-on-the-rename-line)). The
name is the view's, never the entry's.

- **Takes:** the view. Default key <!-- key info.view.rename -->`ctrl+m`<!-- /key -->; the control
  `[Rename]`.
- **Answers:** `Name this view; Enter keeps it, Escape cancels`, and the line, holding the name.
- **Refuses:** nothing.
- **Example:** <!-- key info.view.rename -->`ctrl+m`<!-- /key -->, type `Sample`.

### `info.view.menu`

Opens the view's menu: every command it has but this one and the two that move the selection,
each marked ` (unavailable)` when it cannot act now.

- **Takes:** the view. Default key <!-- key info.view.menu -->`ctrl+.`<!-- /key -->; the control
  `[...]`, or a right press on the view's rows.
- **Answers:** asks Workshop to present the menu (`PaneMenuRequested`); a line chosen acts as its
  command does.
- **Refuses:** `The actions menu could not be requested`; a right press on a picture the view no
  longer shows, `This view changed -- press again`.
- **Example:** <!-- key info.view.menu -->`ctrl+.`<!-- /key -->, choose `Discard edits`.

### `info.value.panes`

The default pane's way back to its lists. The value stays, for
[`info.inventory`](#infoinventory). A watch ends, since the view no longer shows the value.

- **Takes:** the default pane. Default key <!-- key info.value.panes -->`ctrl+i`<!-- /key -->; the
  control `[Panes]`.
- **Answers:** the lists; `Watch ended: this view left for pane properties` when it was watching,
  and the watch ends at Workshop
  ([`PaneObservationEnded`](../../workshop/docs/zengine.workshop.md#paneobservationended)).
- **Refuses:** nothing.
- **Example:** <!-- key info.value.panes -->`ctrl+i`<!-- /key -->.

### `info.view.close`

Retires the view and takes it off the desk. Nothing in Inventory is touched, and the view is the next
one [`info.view.new`](#infoviewnew) opens. The default pane has none.

- **Takes:** a value view. Default key <!-- key info.view.close -->`ctrl+shift+w`<!-- /key -->; the
  control `[Close]`.
- **Answers:** asks Workshop to close the pane
  ([`PaneCloseRequested`](../../workshop/docs/zengine.workshop.md#panecloserequested)), and a watch
  the view held ends there
  ([`PaneObservationEnded`](../../workshop/docs/zengine.workshop.md#paneobservationended)); an
  answer to the retired view's asks lands nowhere. Closing it from the desk or the Pane Manager only
  hides it.
- **Refuses:** `Close waits: a save is waiting for its answer`; `Unsaved edits in this view: press
  Close again to discard and close (Inventory data stays)`, or `Pending work in this view: ...`,
  until the second press.
- **Example:** <!-- key info.view.close -->`ctrl+shift+w`<!-- /key -->.

### `info.view.stop`

Stops waiting for a Refresh, a link or a Sample. Declared only while one waits; a save is never
stopped, since a write sent cannot be called back.

- **Takes:** the ask the view waits on. Default key <!-- key info.view.stop -->`escape`<!-- /key -->;
  the control `[Stop]`, drawn first.
- **Answers:** `Stopped waiting (opening 'Beta'); this draft is unchanged, and a late answer is
  ignored`. It says nothing of whether the owner failed.
- **Refuses:** nothing.
- **Example:** <!-- key info.view.stop -->`escape`<!-- /key -->.

## Commands in a field edit

While a field's line is open these two are declared; every other key edits the line.

### `info.value.field.accept`

Puts the line's text into the field, in this draft only.

- **Takes:** the line, read as the field's type. Default key
  <!-- key info.value.field.accept -->`return`<!-- /key -->.
- **Answers:** `Field changed in this draft (was 1); Save stores it`; the row is marked `*`.
- **Refuses:** the field's own reason when the text is not of its type; the line stays open.
- **Example:** type `41`, press <!-- key info.value.field.accept -->`return`<!-- /key -->.

### `info.value.field.cancel`

Closes the line; the field is as it was.

- **Takes:** the line. Default key <!-- key info.value.field.cancel -->`escape`<!-- /key -->.
- **Answers:** the field's row again.
- **Refuses:** nothing.
- **Example:** <!-- key info.value.field.cancel -->`escape`<!-- /key -->.

## Commands on the rename line

### `info.view.rename.accept`

Names the view with the line's text, trimmed, at most `kMaxViewTitle` characters, each one the
title cannot hold written `?`; an empty line gives back the view's own name, `Info 2`.

- **Takes:** the line. Default key <!-- key info.view.rename.accept -->`return`<!-- /key -->.
- **Answers:** `View renamed Sample`; the title row, and for a view other than the default the pane's
  header and the Pane Manager, say the new name.
- **Refuses:** nothing.
- **Example:** type `Sample`, press <!-- key info.view.rename.accept -->`return`<!-- /key -->.

### `info.view.rename.cancel`

Closes the line; the name is as it was.

- **Takes:** the line. Default key <!-- key info.view.rename.cancel -->`escape`<!-- /key -->.
- **Answers:** the view's rows again.
- **Refuses:** nothing.
- **Example:** <!-- key info.view.rename.cancel -->`escape`<!-- /key -->.

## Commands in an unused view

A view that was closed and is shown again, from the Pane Manager, shows `Info 2 -- unused view
slot`. A value dropped on it opens there as on any view.

### `info.view.use`

Takes the view into use, empty.

- **Takes:** the unused view. Default key <!-- key info.view.use -->`return`<!-- /key -->; the
  control `[Use this view]`.
- **Answers:** `This view is yours: drag an Inventory entry here`, and the value view's commands.
- **Refuses:** nothing.
- **Example:** <!-- key info.view.use -->`return`<!-- /key -->.

## Hears

### `PaneInventory`

From Workshop alone: every pane it knows, whole, each time the list changes
([`workshop/desktop_seam_vocabulary.hpp`](../../workshop/desktop_seam_vocabulary.hpp)). It also says
which of Info's views are on the desk; a hidden view keeps its draft and ends its watch.

### `PaneSubjectShown`

From Workshop alone: the pane inspected and its rows, whole, each time they change
([`workshop/inspection_seam_vocabulary.hpp`](../../workshop/inspection_seam_vocabulary.hpp)).
Workshop names what the rows address; a draft opened on other rows is abandoned.

### `InventoryChanged`

From `zengine.inventory` alone: something stored changed, and no value with it
([`inventory/vocabulary.hpp`](../../inventory/vocabulary.hpp)). Each watching view reads its entry
once.

### `PaneResetRequested`

A reset of one view ([`workshop/setup_control.hpp`](../../workshop/setup_control.hpp)), from a guest
with the `demo` power: the default pane drops its drafts and goes back to its lists; another view is
emptied. Refused, `Info reset needs its pane and no pending commit`, while that view waits on an ask.

### `InspectPaneRequested`

From an office, as the Pane Manager's `inspect in Info` row asks: the default pane inspects that
pane as [`info.edit`](#infoedit) does and answers with Workshop's `PaneSubjectActed`, or in its
own words: `finish the edit first -- commit it or cancel it` while a draft is open; an inspect not
submitted or not delivered, as `info.edit` says; `a newer inspect took its place -- nothing
changed`.

## Follow

Info says nothing unasked but its own rows: it publishes no account of what it did. An observer
follows the owners instead: Workshop's `PaneInventory` and `PaneSubjectShown` for the lists, and
for a value, Inventory's `InventoryChanged` and the entry itself. What a view shows now is read from
the rows Workshop holds for its pane, by their part names ([Shows](#shows)).

## Shows

### `panes`

`PANES -- 12`, then a row per pane, `>*Layouts -- open`: `>` the cursor, `?` where it holds a pane
that left the list, `*` the pane inspected; `open`, `closed` or `gone`. Each row's part is
`pane:<office>/<pane>`. What the room cannot hold is counted, `... 3 more`.

### `properties`

`PANE Layouts` over the inspected pane's rows, a label then its value; a dim row is not yours to
author, and the row a draft is open on is lit. Each row's part is `property:<label>`. With nothing
inspected, `(no subject -- Return on a pane inspects it)`.

### `notice`

Above the lists, the answer to the last act, or while the cursor holds a pane that left the list,
`Layouts left the list -- choose a row before Return inspects anything`.

### `title`

A value view's first row: its name, then what it holds, `Info 2 | LINKED 'Note' story.Item v1`:
`COPY`, `LINKED`, `LINK STALE` when the entry is gone, `PRESET` before either for a preset; `empty`
before a value.

### `state`

The view's state, `UNSAVED 1 edit(s) | NEWER rev 3 waiting (Refresh accepts) | watch ON read
20:05:10`: `saved rev 2`, `not stored`, `UNSAVED SAMPLE`, what it waits on, `watch off`, `entry no
longer here`, `source unavailable`. A tall room adds the entry, the sample's source and the last
attempts.

### `controls`

The commands as controls, `[Save] [Save copy] [Refresh] [Watch] [New] [Fork] [Sample] [Rename]
[Close] [...]`, wrapped to the width; each control's part is `control:<id>`.

### `fields`

A row per field, `>*count: 41`: `>` selected, `*` changed by you, `~` changed by the last observation,
cleared by your next act; ` (required)` on an absent required field. A capture's metadata follows,
dim and read-only, `meta[0].source`. Each row's part is `field:<path>`, or `field:meta[0].<path>`.

## Files

Info reads and writes no file. A property you commit is written into the desk by Workshop, which
keeps the desk; a value is kept by Inventory.

## See also

- [Independent Info views and the inspection workbench](info-views.md): several views side by side,
  a field filled from another view, a watched entry and a fresh sample, on a ready-made toolbox.
- [Inventory](../../inventory/README.md), which keeps the values.
