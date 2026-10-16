# zengine.inventory -- Inventory

**Manual.** Inventory: the weave that keeps the values you want to use again, answering to the
office `zengine.inventory` (an office is the name a weave answers to). Each value is an *entry*,
an item with its own record of how it was captured, filed in named folders. It is a loadable
weave, built as `zengine-inventory`; Workshop's default plans load it. Its pane is
[`zengine.inventory-pane`](zengine.inventory-pane.md); another weave, or an agent, reaches it by
the messages below.

## About

An entry holds a *pair*: the *item*, the value itself in any schema, and its *metadata*, a list of
typed facts about how it was captured. The pair is one self-describing byte envelope, so a reader
needs no compiled knowledge of either schema; [the pair format](inventory.md) is its own page.

Inventory keeps two kinds of entry. The *compatibility slot* holds at most one pair, replaced
whole by [`InventorySet`](#inventoryset) and by a capture with
[`InventoryCaptureDescribe`](#inventorycapturedescribe). The *saved entries* are a collection:
added, renamed, filed and removed one by one, at most `kMaxSavedEntries`
([`inventory/archive.hpp`](../archive.hpp)) beside the slot. Folders organize the collection;
they are never stored in a pair, and they grant and run nothing.

An entry is named by a *reference*, `{owner, entry}`: `owner` is a random identity of this
Inventory's collection, `entry` a random identity of one stored entry. A reference is a locator in
this process, not a pointer, a secret, a grant or a name that survives a restart: a reload in place
keeps `owner`, so every reference from before still reads and writes; a restore draws a new one,
and every reference from before refuses. Each entry has a *revision*; a change that names an old one
refuses and changes nothing. A refusal always changes nothing.

The collection lasts as long as this Inventory: a reload in place keeps it, under the same `owner`;
unloading and loading again starts empty, and a toolbox file restores it. A capture in flight
belongs to the image: after a reload its asker is never answered, and may ask again. Every value
is copied as it crosses in or out, so a later edit of a caller's bytes, or an answer's, reaches
nothing here.

## Use

Send to the office `zengine.inventory`. Each request is answered to its sender alone, with the
shape its section names or `zen.Refused` and the reason in its words; a change also publishes
[`InventoryChanged`](#follow). To read the collection, ask [`InventoryList`](#inventorylist) v2;
to keep a value, [`InventoryAdd`](#inventoryadd); to change one, read it with
[`InventoryRead`](#inventoryread) and write it back with [`InventoryWrite`](#inventorywrite) at the
revision you read.

Who may send: in Workshop, a pane asks Workshop for the current actor's permission first, so the
Inventory pane, Info and Compose act with your authority. A guest of the external host needs its
`inventory` power, which reaches every request here but the snapshot and restore; those stay
behind the pane's toolbox, under the `toolbox` power. Workshop admits this office with the bounded
`inventory_grant()` ([`inventory/grant.hpp`](../grant.hpp)), to which it adds the answer to
`ShapeDocumentRequested`; the artifact cannot widen its own grant.

## Commands

### `InventorySet`

`InventorySet` v1 replaces the compatibility slot with a pair, whole.

- **Takes:** `pair`, a complete encoded pair.
- **Answers:** `zen.Ack`. The slot is a new entry, with a new identity and revision 1, at the root,
  even for the same bytes; its old reference no longer reads. Saved entries are left alone.
- **Refuses:** `Set refused: ` and the codec's reason, for bytes that do not decode or a value
  its own schema does not admit; the slot keeps what it held.
- **Example:** `mail.send_to_role("zengine.inventory", InventorySet{pair});`

### `InventoryGet`

`InventoryGet` v1 asks for the compatibility slot's pair.

- **Takes:** nothing.
- **Answers:** `InventoryState` v1 `{occupied, pair}`: `occupied` false and `pair` empty while the
  slot is empty, which is an answer, not a refusal.
- **Refuses:** nothing.
- **Example:** read back what a capture stored, and compare it with the capture's own answer.

### `InventoryLocate`

`InventoryLocate` v1 asks for the compatibility slot as an entry.

- **Takes:** nothing.
- **Answers:** `InventoryEntry` v1 `{reference, revision, pair}`.
- **Refuses:** `the inventory is empty`.
- **Example:** locate the slot, then [`InventoryWrite`](#inventorywrite) it at that revision.

### `InventoryRead`

`InventoryRead` v1 asks for one entry by its reference: a saved entry or the slot.

- **Takes:** `reference`.
- **Answers:** `InventoryEntry` v1, a copy of the entry as it is now.
- **Refuses:** `this inventory entry is no longer here`, for an entry removed or replaced, or a
  reference of another owner.
- **Example:** Info reads an entry dropped on it as a live reference before editing it.

### `InventoryWrite`

`InventoryWrite` v1 replaces one entry's pair, keeping its identity, if it is still at the revision
you name.

- **Takes:** `reference`; `revision`, the one you read; `pair`.
- **Answers:** `InventoryEntry` v1 at the next revision. Its label, folder and identity stay.
- **Refuses:** `this inventory entry is no longer here`; `the entry changed or its revision is
  exhausted; fetch a fresh copy` for a saved entry, `the entry changed; fetch a fresh copy before
  saving` for the slot; `save refused: ` and the codec's reason for a pair that does not decode.
- **Example:** Info's save of a linked draft is a write at the revision it last read.

### `InventoryAdd`

`InventoryAdd` adds a saved entry: v1 at the root, v2 into a folder.

- **Takes:** `pair`; `label`, its name, empty to show the item's schema name; v2 `folder`, a
  folder reference (`folder` empty for the root).
- **Answers:** `InventoryEntry` v1 for the new entry, revision 1.
- **Refuses:** `` inventory has reached its <n> saved-entry limit ``, `<n>` being
  `kMaxSavedEntries`; `entry names must be at most 80 printable ASCII characters`; the codec's
  reason for a pair that does not decode; v2 `that folder is no longer here`.
- **Example:** the pane stores a value dropped on it with v2, into the folder it shows.

### `InventoryList`

`InventoryList` asks for every entry's summary; no pair bytes travel.

- **Takes:** nothing.
- **Answers:** v1 `InventoryListed` v1 `{entries}`, each `InventorySummary` v1 `{reference,
  revision, label, schema, version, capture_slot}`, the slot first; v2 `InventoryListed` v2
  `{owner, revision, entries, folders}`, at one collection revision, each summary also naming its
  folder, and every folder as `InventoryFolderState` v1. v1 says nothing of folders.
- **Refuses:** nothing.
- **Example:** on each `InventoryChanged`, the pane asks v2 and draws the folder it shows.

### `InventoryRename`

`InventoryRename` v1 changes one entry's label.

- **Takes:** `reference`; `revision`; `label`.
- **Answers:** `InventoryEntry` v1 at the next revision; its identity, pair and folder stay.
- **Refuses:** `this inventory entry is no longer here`; `the entry changed or its revision is
  exhausted; fetch a fresh copy`; `entry names must be at most 80 printable ASCII characters`.
- **Example:** the pane's [`inventory.rename`](zengine.inventory-pane.md#inventoryrename).

### `InventoryRemove`

`InventoryRemove` v1 removes one entry, a saved entry or the slot.

- **Takes:** `reference`; `revision`.
- **Answers:** `zen.Ack`.
- **Refuses:** `this inventory entry is no longer here`; `the entry changed or its revision is
  exhausted; fetch a fresh copy`.
- **Example:** the pane's [`inventory.remove`](zengine.inventory-pane.md#inventoryremove), pressed
  twice.

### `InventoryFolderCreate`

`InventoryFolderCreate` v1 makes a folder inside another, or at the root.

- **Takes:** `parent`, a folder reference `{owner, folder}`, `folder` empty for the root; `name`.
  A name is 1 to 64 printable ASCII characters, without `/`, a leading or trailing space, `.` or
  `..`, kept exactly as sent; two folders in one place may not share a name ignoring ASCII case.
- **Answers:** `InventoryFolderState` v1 `{folder, revision, name, parent}`, revision 1.
- **Refuses:** `that folder is no longer here`; `A folder needs a name`; `Folder names are 1 to 64
  printable ASCII characters`; `Folder names cannot contain '/'`; `Folder names cannot start or end
  with a space`; `'.' and '..' are not folder names`; `Inventory holds at most 128 folders`;
  `folders nest at most eight deep`; `'Drafts' is already a folder here`.
- **Example:** `InventoryFolderCreate{{owner, ""}, "Workbench"}`.

### `InventoryFolderRename`

`InventoryFolderRename` v1 renames a folder at the revision you name.

- **Takes:** `folder`; `revision`; `name`, under the same rules as
  [`InventoryFolderCreate`](#inventoryfoldercreate).
- **Answers:** `InventoryFolderState` v1 at the next revision.
- **Refuses:** `that folder is no longer here`; `the folder changed; look again before changing
  it`; the name's refusals; `'Drafts' is already a folder there`.
- **Example:** the pane's [`inventory.rename`](zengine.inventory-pane.md#inventoryrename) on a
  folder row.

### `InventoryFolderMove`

`InventoryFolderMove` v1 moves a folder, with everything in it, into another.

- **Takes:** `folder`; `revision`; `into`, the destination folder reference.
- **Answers:** `InventoryFolderState` v1 at the next revision; a move into the folder it is in
  answers it unchanged.
- **Refuses:** `that folder is no longer here`; `the folder changed; look again before changing
  it`; `the destination folder is no longer here`; `a folder cannot move into itself or a folder
  inside it`; `that would nest folders more than eight deep`; `'Drafts' is already a folder there`.
- **Example:** the pane's [`inventory.move.here`](zengine.inventory-pane.md#inventorymovehere) with
  a folder picked.

### `InventoryFolderRemove`

`InventoryFolderRemove` v1 removes an empty folder. It never removes an entry.

- **Takes:** `folder`; `revision`.
- **Answers:** `zen.Ack`.
- **Refuses:** `that folder is no longer here`; `the folder changed; look again before changing
  it`; `'Drafts' still holds 1 entry and 0 folders; move them out first. Removing a folder never
  removes entries`. An entry placed in one of the pane's views is still the folder's.
- **Example:** the pane's [`inventory.remove`](zengine.inventory-pane.md#inventoryremove) on an
  empty folder.

### `InventoryFile`

`InventoryFile` v1 moves one entry from the folder it is in into another, only if it is still in
the folder you name, so a move begun on an older picture refuses rather than taking the entry from
where it went since.

- **Takes:** `reference`; `from`, the folder you saw it in; `into`.
- **Answers:** `InventoryEntry` v1. Filing is not an edit: identity, revision, pair and label stay,
  so a draft read at that revision can still save. A move into its own folder changes nothing.
- **Refuses:** `this inventory entry is no longer here`; `the entry is no longer in that folder;
  look again`; `the destination folder is no longer here`.
- **Example:** a drag in the pane dropped on a folder row.

### `InventorySnapshotRequested`

`InventorySnapshotRequested` asks for the whole collection as an archive, for a toolbox file.

- **Takes:** nothing.
- **Answers:** v2 `InventorySnapshot` v2 `{owner, revision, archive}`, its `InventoryArchive` v2
  holding every entry (`InventorySavedEntry` v2 `{key, label, pair, capture_slot, folder}`) and
  every folder (`InventorySavedFolder` v1 `{key, name, parent}`); a key is the live identity,
  standing for it inside the archive only. v1 answers `InventorySnapshot` v1, a flat archive.
- **Refuses:** v1 `this inventory has folders; ask for a version 2 snapshot` when anything is
  filed; an archive past the bounds [`InventoryRestore`](#inventoryrestore) names.
- **Example:** the pane's [`inventory.toolbox.save`](zengine.inventory-pane.md#inventorytoolboxsave)
  asks v2, then writes the file.

### `InventoryRestore`

`InventoryRestore` replaces the whole collection with an archive, if nothing changed since the
snapshot you name. v1 restores a flat archive at the root.

- **Takes:** `owner` and `revision`, from a fresh snapshot; `archive`; `replace`, true to replace a
  collection that is not empty.
- **Answers:** `InventoryRestored` v1 `{owner, entries}`: the new owner identity and how many
  entries came back. Every revision restarts at 1; folder keys are kept; the old references refuse.
- **Refuses:** `finish the pending capture before restoring a toolbox`; `inventory changed while
  preparing restore; request restore again`; `inventory is not empty; explicitly choose replace to
  restore this toolbox`; and the archive's: `toolbox exceeds the inventory capacity`, `toolbox
  pairs exceed the 8 MiB limit`, `toolbox has an invalid or duplicate entry key`, `a toolbox entry
  names a missing folder`, and the folder rules. The whole archive is checked first.
- **Example:** the pane's
  [`inventory.toolbox.restore`](zengine.inventory-pane.md#inventorytoolboxrestore).

### `InventoryCaptureAdd`

`InventoryCaptureAdd` v1 captures a weave's description into a new saved entry.

- **Takes:** `target_role`, the office to describe; `label`.
- **Answers:** asks the office's holder `zen.PokeDescribe`, the description every weave gives of
  itself, then stores its `zen.PokeStructure` as the item, with `CaptureContext` v1 and
  `CaptureAddRequest` v1 as metadata; answers the new entry's `InventoryEntry` v1.
- **Refuses:** as [`InventoryCaptureDescribe`](#inventorycapturedescribe), and `entry names must
  be at most 80 printable ASCII characters`.
- **Example:** `InventoryCaptureAdd{"zengine.input", "Input"}`.

### `InventoryCaptureDescribe`

`InventoryCaptureDescribe` v1 captures a weave's description into the compatibility slot.

- **Takes:** `target_role`.
- **Answers:** `InventoryCaptured` v1 `{pair}`, the capture's own copy, which a later Set does not
  change. The metadata is `CaptureContext` v1, what Inventory observed itself: the role asked, the
  shape it sent, the weave Loom attests answered and this process's clock when it did, and
  `CaptureRequest` v1, the request. A capture is a past observation: the same weave number may name
  another weave after a restart, and editing the item never reaches its source.
- **Refuses:** `Inventory capture needs a target_role`; `a capture is already in flight for this
  inventory`; `'zengine.input' could not be asked zen.PokeDescribe: ` and Loom's reason; `capture
  request could not be queued`; `this inventory's own book is full`; `this delivery cannot defer
  its answer`; `capture could not be stored: ` and why. A target that never answers leaves the
  capture waiting; only Loom's answer to Inventory's own ask settles it.
- **Example:** `InventoryCaptureDescribe{"zengine.guests"}`, then `InventoryGet`.

## Follow

Inventory publishes `InventoryChanged` v1 from `zengine.inventory` after every change it makes and
when it starts. It carries nothing: it says the collection moved, and a follower asks
[`InventoryList`](#inventorylist) again. The v2 list's `revision` counts the changes since this
Inventory was loaded or last restored, a reload in place counting on, and its `owner` says which
collection answered.

## See also

- [`zengine.inventory-pane`](zengine.inventory-pane.md), the pane.
- [The pair format](inventory.md): the envelope, its metadata, and `zengine-inventory-read`.
- [Organize Inventory in named folders](inventory-folders.md),
  [save a toolbox](toolboxes.md), [reuse a stored command through Compose](inventory-compose.md).
