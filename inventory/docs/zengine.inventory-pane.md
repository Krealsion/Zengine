# zengine.inventory-pane -- the Inventory pane

**Manual.** The pane you keep values in. It lists what Inventory holds, folder by folder, lets you
drag an entry into another pane, pops entries out into boxes, rows and columns, puts a stored
command on a hotkey, and saves the whole collection to a toolbox file. It is a *weave*, built as
`zengine-inventory-pane`, answering to the office `zengine.inventory-pane`. It offers one pane for
main Inventory, `inventory`, and one for each portable view it made. The entries themselves are
the Inventory office's, [`zengine.inventory`](zengine.inventory.md).

## About

The pane keeps no value. Every entry, name and folder is the Inventory office's, and the pane asks
it for each: a list when the office says the collection changed, a read before a value leaves, an
add, a rename, a filing, a removal. What the pane keeps is how you arranged it: the portable views
and which entries each holds, each entry's hotkey, and whether each view's hotkeys are on. That
arrangement survives a reload of the pane, and a toolbox file keeps it with the entries.

An entry is in one *folder*, the office's fact, and in one *place*: main Inventory or one portable
view, the pane's fact. One gesture changes one of them. Main Inventory lists the entries placed
there in the folder it shows; the location row counts the folder's entries placed in views.

A *portable view* is a *single* box, a *row* or a *column* of tiles, each a pane of its own, named
`inventory.<n>`. Dropping on a filled single box sends its entry back to main Inventory. A hotkey
runs the command an entry stores: the item's switch and its view's switch must both be on, and
Workshop's Desktop must admit the chord; a restored or duplicated hotkey starts off. Running reads
the entry again, refuses a command that is not complete, and asks Workshop for your authority to
send that shape to that office; `Queued ... to ...; delivery is not completion` means it was sent.

## Use

Open it from the Pane Manager, <!-- key desktop.panes -->`ctrl+p`<!-- /key -->; its keys act while
it holds yours. Press an entry and drag it onto another pane to give that pane a copy, or onto
another Inventory view to move it there; drop on a folder row, a crumb or `[Up]` to file it. A
press on a folder selects it and a second opens it. A value dropped here from another pane is
stored as a new entry where it lands, and a line opens: `Name this new item; Enter saves, Escape
keeps its generated name`.

Right-press a row for the menu: `Open folder`, `Rename folder...`, `Move to another folder...`,
`Remove empty folder` on a folder; on an entry `Grab live entry reference`, `Pick up a copy`,
`Rename entry...`, `Duplicate with next number`, `Duplicate and name...`, `Configure command
hotkey...`, `Enable item hotkey`, `Run configured command now`, `Return item to main Inventory` in a
view, `Remove entry...`; then `Pop out single box`, `Pop out row`, `Pop out column`, `Turn this
view's hotkeys ON`, `Save toolbox...`, `Restore toolbox...`, and in main Inventory `New folder
here...`. A choice that waits for a key takes your keys to this pane.

Every act that reads, writes or carries an entry, or changes the arrangement, asks Workshop for the
current actor's permission first, and says `Checking this operation's authority` or `Checking
authority for this view change` meanwhile; Workshop's refusal is the notice. One act waits at a
time: another says `Still waiting for the previous Inventory operation; folders still open`, and
browsing still works.

## Commands

### `inventory.copy`

On a folder, opens it. On an entry, picks up a copy to place in another pane.

- **Takes:** the selected row. Default key <!-- key inventory.copy -->`return`<!-- /key -->.
- **Answers:** a folder opens, `In Root > Workbench`. For an entry the pane reads it from the
  owner ([a read](zengine.inventory.md#inventoryread)) and asks Workshop to carry the copy:
  `Entry acquired; finish the gesture to place it`, then `Click a receiving pane; Escape cancels`.
- **Refuses:** `That folder is no longer here`; the owner's
  `this inventory entry is no longer here`; Workshop's reason when it carries nothing.
- **Example:** select an entry, press <!-- key inventory.copy -->`return`<!-- /key -->, press Info.

### `inventory.live`

Picks up a live reference to the selected entry: a receiver such as Info then edits that entry
itself, not a copy.

- **Takes:** the selected entry. Default key <!-- key inventory.live -->`ctrl+return`<!-- /key -->.
- **Answers:** as [`inventory.copy`](#inventorycopy); what is carried is the entry's reference,
  with the name it had and its revision as metadata.
- **Refuses:** `A folder is not a value; Enter opens it` on a folder; as
  [`inventory.copy`](#inventorycopy).
- **Example:** press <!-- key inventory.live -->`ctrl+return`<!-- /key -->, then press Info.

### `inventory.up`

Selects the row above: a folder or an entry.

- **Takes:** the view's rows. Default key <!-- key inventory.up -->`up`<!-- /key -->; the wheel walks
  the same way.
- **Answers:** the `>` marker moves; it stops at the first row. Navigation works while an act waits.
- **Refuses:** nothing.
- **Example:** <!-- key inventory.up -->`up`<!-- /key -->.

### `inventory.down`

Selects the row below.

- **Takes:** the view's rows. Default key <!-- key inventory.down -->`down`<!-- /key -->.
- **Answers:** the marker moves; it stops at the last row.
- **Refuses:** nothing.
- **Example:** <!-- key inventory.down -->`down`<!-- /key -->.

### `inventory.left`

Selects the tile before, in a row; anywhere else, as [`inventory.up`](#inventoryup).

- **Takes:** the view's entries. Default key <!-- key inventory.left -->`left`<!-- /key -->.
- **Answers:** the `*` marker moves; a row or column past its room pages to the tiles before.
- **Refuses:** nothing.
- **Example:** <!-- key inventory.left -->`left`<!-- /key -->.

### `inventory.right`

Selects the tile after; anywhere else, as [`inventory.down`](#inventorydown).

- **Takes:** the view's entries. Default key <!-- key inventory.right -->`right`<!-- /key -->.
- **Answers:** the marker moves; the view pages to the tiles after.
- **Refuses:** nothing.
- **Example:** <!-- key inventory.right -->`right`<!-- /key -->.

### `inventory.sort`

Cycles main Inventory's order of entries: added, name, type. Folders stay first, by name. The
selection stays on the same entry.

- **Takes:** main Inventory. Default key <!-- key inventory.sort -->`ctrl+s`<!-- /key -->.
- **Answers:** the [`heading`](#heading) row says `sort: added`, `sort: name` or `sort: type`. In a
  portable view it does nothing.
- **Refuses:** nothing.
- **Example:** <!-- key inventory.sort -->`ctrl+s`<!-- /key --> twice to sort by type.

### `inventory.refresh`

Asks the owner for the collection again.

- **Takes:** nothing. Default key <!-- key inventory.refresh -->`ctrl+r`<!-- /key -->.
- **Answers:** the rows are drawn from the owner's new [list](zengine.inventory.md#inventorylist).
  The pane asks on its own whenever the owner says the collection changed.
- **Refuses:** `Inventory list unavailable: ` and Loom's reason; `Inventory list request could not
  be queued`.
- **Example:** <!-- key inventory.refresh -->`ctrl+r`<!-- /key -->.

### `inventory.rename`

Renames the selected entry or folder on a line the pane opens.

- **Takes:** the selected row. Default key <!-- key inventory.rename -->`ctrl+n`<!-- /key -->.
- **Answers:** the line opens, `Name: ` or `Folder name: ` with the name in it
  ([commands on the line](#commands-on-the-line)); a folder's notice is `Rename folder 'Drafts';
  Enter saves, Escape cancels`. Saving asks the owner to
  [rename the entry](zengine.inventory.md#inventoryrename) or
  [the folder](zengine.inventory.md#inventoryfolderrename): `Inventory arrangement saved`, or
  `Renamed the folder to 'Drafts'`. The entry keeps its identity, data and hotkey.
- **Refuses:** the owner's words, such as `entry names must be at most 80 printable ASCII
  characters`, or a folder name's rule.
- **Example:** press <!-- key inventory.rename -->`ctrl+n`<!-- /key -->, type `Field sample`, press
  <!-- key inventory.name.save -->`return`<!-- /key -->.

### `inventory.remove`

Removes the selected entry, or an empty folder: the first press asks, the second removes.

- **Takes:** the selected row. Default key <!-- key inventory.remove -->`delete`<!-- /key -->; any
  other key in between disarms it.
- **Answers:** `Press Delete again to remove Sample`, then the owner
  [removes it](zengine.inventory.md#inventoryremove): `Entry updated`. On a folder, `Press Delete
  again to remove the empty folder 'Drafts'`, then `Removed empty folder 'Drafts'`.
- **Refuses:** `'Drafts' still holds 2 items; move them out first. Removing a folder never removes
  entries`; the owner's `the entry changed or its revision is exhausted; fetch a fresh copy`.
- **Example:** <!-- key inventory.remove -->`delete`<!-- /key -->,
  <!-- key inventory.remove -->`delete`<!-- /key -->.

### `inventory.toolbox.save`

Saves the collection and the arrangement to a toolbox file. Saving writes a file, so Workshop
judges it a write: on a weaver's host a guest's save is refused in words.

- **Takes:** the path typed on the line, `Toolbox file: `, which starts with the last path or
  `inventory.toolbox`; a relative path is from Workshop's working directory. Default key
  <!-- key inventory.toolbox.save -->`ctrl+shift+s`<!-- /key -->.
- **Answers:** `Enter saves this toolbox file; Escape cancels`; on Enter, `Checking authority for
  the toolbox file`, `Taking a toolbox snapshot` (the owner's
  [snapshot](zengine.inventory.md#inventorysnapshotrequested)), then `Saved 3 entries to toolbox:
  /home/me/inventory.toolbox`, the absolute path. A file there is replaced only once the new one is
  whole.
- **Refuses:** `choose a toolbox file path`; `a configured entry belongs to an unavailable
  inventory; resolve it before saving`; `toolbox file exceeds the 32 MiB limit`; the file system's
  reason; Workshop's refusal.
- **Example:** <!-- key inventory.toolbox.save -->`ctrl+shift+s`<!-- /key -->, type
  `work.toolbox`, <!-- key inventory.name.save -->`return`<!-- /key -->.

### `inventory.toolbox.restore`

Replaces the collection and the arrangement with a toolbox file's. Restored hotkeys and views are
off, folders come back and browsing starts at Root.

- **Takes:** the path typed on the line. Default key
  <!-- key inventory.toolbox.restore -->`ctrl+o`<!-- /key -->.
- **Answers:** `Enter reviews replacement; Escape cancels`; on Enter, `Enter again replaces the
  inventory; hotkeys start OFF. Escape cancels`; on Enter again, `Preparing toolbox restore`,
  `Restoring the toolbox against the current collection` (the owner's
  [restore](zengine.inventory.md#inventoryrestore)), `Entries restored with hotkeys OFF; clearing
  previous shortcuts`, then `Restored 3 entries; hotkeys OFF: /home/me/work.toolbox`.
- **Refuses:** `cannot read toolbox: /home/me/work.toolbox`; `toolbox format refused: not an
  Inventory toolbox file`; `toolbox format refused: version 3 is not one this Workshop reads (1 or
  2)`; `toolbox file exceeds the 32 MiB limit`; `` toolbox plus already offered views exceed <n>;
  restore in a fresh Workshop ``, `<n>` being `kMaxPortableViews`; the owner's refusals.
  `Entries restored with hotkeys OFF; Desktop cleanup refused: ` says the entries did come back.
- **Example:** <!-- key inventory.toolbox.restore -->`ctrl+o`<!-- /key -->, type the path, press
  <!-- key inventory.name.save -->`return`<!-- /key --> twice.

### `inventory.move.pick`

Picks the selected entry or folder to move to another folder; open the destination, then place it.

- **Takes:** the selected row, in main Inventory or a view. Default key
  <!-- key inventory.move.pick -->`ctrl+x`<!-- /key -->.
- **Answers:** `Moving 'Drafts': open the destination, then Ctrl+V or [Move here]; Escape cancels`;
  its row reads `[moving]` and the location row offers `[Move here]`.
- **Refuses:** `Select an entry or folder to move`.
- **Example:** <!-- key inventory.move.pick -->`ctrl+x`<!-- /key -->, open the destination, press
  <!-- key inventory.move.here -->`ctrl+v`<!-- /key -->.

### `inventory.parent`

Climbs to the folder above, selecting the one you came from.

- **Takes:** main Inventory's folder. Default key <!-- key inventory.parent -->`backspace`<!-- /key -->;
  the `[Up]` control. While a line is open the key is the line's.
- **Answers:** `Up to Root > Workbench`.
- **Refuses:** `Already at Root`.
- **Example:** <!-- key inventory.parent -->`backspace`<!-- /key -->.

### `inventory.root`

Goes to Root, selecting the top folder you came through. A press on a crumb goes to that folder.

- **Takes:** main Inventory. Default key <!-- key inventory.root -->`alt+home`<!-- /key -->.
- **Answers:** `In Root`.
- **Refuses:** `Already in Root`; for a crumb, `That folder is no longer here`.
- **Example:** <!-- key inventory.root -->`alt+home`<!-- /key -->.

### `inventory.folder.new`

Makes a folder in the folder main Inventory shows, named on a line the pane opens.

- **Takes:** main Inventory's folder. Default key <!-- key inventory.folder.new -->`ctrl+d`<!-- /key -->.
- **Answers:** `New folder in Root > Workbench; Enter creates, Escape cancels`, the line `New
  folder: `; on Enter the owner [makes it](zengine.inventory.md#inventoryfoldercreate): `Created
  folder 'Drafts' in Root > Workbench`, and it is selected.
- **Refuses:** before asking, `A folder needs a name`, `Folder names are 1 to 64 printable ASCII
  characters`, `Folder names cannot contain '/'`, `Folder names cannot start or end with a space`,
  `'.' and '..' are not folder names`; then the owner's, such as `'Drafts' is already a folder here`.
- **Example:** <!-- key inventory.folder.new -->`ctrl+d`<!-- /key -->, type `Drafts`, press
  <!-- key inventory.name.save -->`return`<!-- /key -->.

### `inventory.move.here`

Places the picked entry or folder in the folder main Inventory shows.

- **Takes:** the pick; main Inventory's folder. Default key
  <!-- key inventory.move.here -->`ctrl+v`<!-- /key -->; the `[Move here]` control.
- **Answers:** an entry is [filed](zengine.inventory.md#inventoryfile) from the folder it was picked
  in, `Filed 'Sample' in Root > Drafts`, with `; it stays placed in its view` for one in a view; a
  folder is [moved](zengine.inventory.md#inventoryfoldermove), `Moved 'Drafts' into Root >
  Workbench`.
- **Refuses:** `Pick an entry or folder to move first (Ctrl+X)`; `'Sample' is already in Root >
  Drafts`; `A folder cannot move into itself or a folder inside it`; `The item picked to move is no
  longer here`; the owner's `the entry is no longer in that folder; look again`.
- **Example:** <!-- key inventory.move.here -->`ctrl+v`<!-- /key -->.

### `inventory.move.cancel`

Declared only while an item is picked: lets the pick go.

- **Takes:** the pick. Default key <!-- key inventory.move.cancel -->`escape`<!-- /key -->.
- **Answers:** `Move cancelled; nothing changed`.
- **Refuses:** nothing.
- **Example:** <!-- key inventory.move.cancel -->`escape`<!-- /key -->.

## Commands on the line

While a name, a folder name, a hotkey or a toolbox path is being typed, these replace the view's
commands, and every other key is a character for the line.

### `inventory.name.save`

Commits the line.

- **Takes:** the text typed. Default key <!-- key inventory.name.save -->`return`<!-- /key -->.
- **Answers:** what the command that opened the line does with it. A hotkey's line, `Target Key: `,
  takes an office and a chord, `zengine.inventory alt+1`, and asks Workshop to change the
  arrangement: `Inventory arrangement saved`. A duplicate's name stores the copy: `Independent copy
  saved; copied hotkey is OFF`.
- **Refuses:** for a hotkey, `Use: target-office alt+1`, `Choose a bound key`, the keymap's word on
  the chord, `Choose a key and modifiers the Workshop keymap can name`; otherwise the opening
  command's refusals. A line refused before anything is asked stays open.
- **Example:** type `zengine.inventory alt+1`, press
  <!-- key inventory.name.save -->`return`<!-- /key -->.

### `inventory.name.cancel`

Closes the line; nothing is asked. For a new copy's name, the stored copy keeps its generated name.

- **Takes:** the line. Default key <!-- key inventory.name.cancel -->`escape`<!-- /key -->.
- **Answers:** `Edit cancelled`.
- **Refuses:** nothing.
- **Example:** <!-- key inventory.name.cancel -->`escape`<!-- /key -->.

## Hears

### `InventoryViewEdit`

`InventoryViewEdit` v1 changes the arrangement: `create` a view (`text` single, row or column,
optional `entry`), `move` an entry to `view` (before `before`), `context` a view's hotkeys on or off,
`bind` an entry to a target office in `text` and a key, `enable` its hotkey. Answers `zen.Ack`;
it never runs a command. Refuses `Finish the current inventory operation or editor first`, `Choose
single, row or column`, `That inventory view is unavailable`, and a chord Desktop refuses.

### `InventoryViewsRequested`

`InventoryViewsRequested` v1 asks for the arrangement: answers `InventoryViews` v1, every view with
its kind, whether its hotkeys are on and its entries in order, and every hotkey with its target,
chord and switch. Main Inventory's view is `inventory`.

### `InventoryToolboxSave`

`InventoryToolboxSave` v1 `{path}` saves a toolbox file, as
[`inventory.toolbox.save`](#inventorytoolboxsave): Workshop first judges its sender's authority to
write. Answers `InventoryToolboxFinished` v1 `{operation, path, entries}`, `save` and the absolute
path, or `zen.Refused` with the reason.

### `InventoryToolboxRestore`

`InventoryToolboxRestore` v1 `{path, replace}` restores one, as
[`inventory.toolbox.restore`](#inventorytoolboxrestore); `replace` false needs an empty collection.
It writes no file and asks the grant alone. Answers `InventoryToolboxFinished` v1, `restore`, or
`zen.Refused`.

## Follow

The pane follows the owner's `InventoryChanged` and asks for the list again; follow the owner the
same way rather than reading the pane ([Follow](zengine.inventory.md#follow)). The arrangement is
said only when asked, with [`InventoryViewsRequested`](#inventoryviewsrequested); each new view is
offered to Workshop as a pane, and appears in the Pane Manager.

## Shows

### `heading`

Main Inventory's first row: how many entries it lists, the order and its hotkeys,
`INVENTORY 3 | sort: added | hotkeys OFF`. A view's: `ON row 2` or `OFF single 1`.

### `location`

Once the collection has a folder, main Inventory's second row: `[Up]`, `[Move here]` while an item
is picked, the crumbs from Root, and the folder's entries placed in views, `+1 in views`. A narrow
room elides the middle crumbs, `Root > ... > Drafts`. At Root it reads `(Up)`.

### `folder`

A folder in the folder shown, first and by name: `Drafts/  (2)`, or `(empty)`; `[moving]` when
picked.

### `entry`

An entry: its hotkey, `[alt+1] ` or `[alt+1 off] `, its name, `[capture slot]` for the slot, and
`: ` its schema. `> ` marks the selection.

### `tile`

A portable view's entry, in a box five rows high: its chord above, `x` after one switched off, and
its name on two lines; `*` marks the selection. `Resize` when the room holds no whole tile.

### `notice`

The last row: what the last act said, or a hint.

## Files

**A toolbox file** is one native-serialized `InventoryToolbox` v2: the owner's archive (each
entry's key, name, pair, slot flag and folder key, and each folder's key, name and parent key),
each view's identity, kind and entry keys, and each hotkey's entry key, target office and chord.
It holds no grant, owner identity, revision, switch, pending act or screen position. A file of
version 1 has no folders and restores at Root; another version is refused. Reading checks the
whole file before anything is replaced: `kMaxToolboxFileBytes`
([`toolbox_file.hpp`](../inventory-pane/toolbox_file.hpp)) bounds it, the archive the owner's
bounds ([`InventoryRestore`](zengine.inventory.md#inventoryrestore)), and the views and hotkeys
`kMaxPortableViews` and `kMaxConfiguredHotkeys` ([`slots.hpp`](../inventory-pane/slots.hpp)).
One writer per path: it is a snapshot, not a database or a journal.

## See also

- [`zengine.inventory`](zengine.inventory.md), the office that keeps the entries.
- [Save a toolbox for another session](toolboxes.md),
  [organize Inventory in named folders](inventory-folders.md),
  [portable inventory toolboxes](inventory-slots.md) and
  [reuse a stored command through Compose](inventory-compose.md).
- The pane protocol's shapes: [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp)
  and [`workshop/pane_document.hpp`](../../workshop/pane_document.hpp).
