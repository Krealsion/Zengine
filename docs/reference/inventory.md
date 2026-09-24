# Inventory: stored values with separate typed metadata

**Reference.** `zengine::inventory` provides a collection of independently named entries. Each holds an
*item* plus its *metadata* -- a separate, automatically derived list of typed facts
about how the item was captured. Include `inventory/codec.hpp` for the byte envelope
(`encode_pair`/`decode_pair`), or `inventory/vocabulary.hpp` for the wire shapes a stranger needs
to add, list, read, update or capture entries. The installed `zengine-inventory` artifact is an ordinary
loadable weave. Workshop's default plans load it at `zengine.inventory`; a standalone Loom can
load the same artifact under its own explicit grants. Unloading and loading it again starts
with an empty collection, without replacing the host. Explicit [toolbox files](../workshop/toolboxes.md)
can restore saved entries and portable configuration into that fresh instance.

## Collection and compatibility slot

`InventoryAdd{pair, label}` appends an entry at the root and answers `InventoryEntry`;
`v2::InventoryAdd{pair, label, folder}` appends it to a [named folder](#named-folders). An empty
label uses the item schema name in the presentation. Names accept up to 80 printable ASCII characters.
There are at most 256 saved entries, plus the compatibility slot below; a full collection
refuses an addition without removing anything. Toolbox snapshots also impose a byte limit.

`InventoryList{}` answers `InventoryListed{entries}`: each summary gives its reference, revision,
label, schema name/version, and whether it is the compatibility capture slot. Lists contain no
item bytes. `v2::InventoryList{}` answers `v2::InventoryListed{owner, revision, entries, folders}`,
where each entry also names its folder and every folder is listed, at one collection revision. `InventoryRename{reference, revision, label}` returns the updated entry;
`InventoryRemove{reference, revision}` answers Ack. Both check the current revision. Read/Write
address either kind of entry by identity; sorting never changes an identity.

`InventoryCaptureAdd{target_role, label}` captures into a new saved entry and answers its own
`InventoryEntry`. Its metadata includes `CaptureContext` and typed `CaptureAddRequest`.
`InventoryChanged` is an authored invalidation, sent after mutations and activation; it carries
no mirrored collection. Consumers ask List again.

The older capture slot remains available for existing clients. Its three doors affect only that
slot, leaving saved entries intact:

The compatibility slot holds either nothing, or one pair: an *item* (the pure data a maker wanted to
keep) and its *metadata* (a list of separate, typed observations about the item's capture).
**Set** replaces the whole pair as one operation. **Get** returns the pair, or an understandable
empty result (`occupied: false`, `pair` empty) before the first successful Set -- never a
refusal merely for being empty. A **refused** Set changes nothing: the previous pair, if any,
stays exactly as it was.

```
Set(pair) -> zen.Ack | zen.Refused{reason}
Get()     -> InventoryState{occupied, pair}
InventoryCaptureDescribe{target_role} -> InventoryCaptured{pair} | zen.Refused{reason}
```

All three are addressed to the role `zengine.inventory`. Over a guest link (see
[workshop/external-host.md](../workshop/external-host.md)), reaching them needs the guest's own
`"inventory"` power -- a power of its own, never a reinterpretation of `"inspect"`'s read-only
discovery grant.

A host can choose `inventory/grant.hpp`'s `inventory_grant()`: description requests, inventory
answers and the ordinary substrate answers, with no unrelated send or Sense-read authority.
Workshop selects this bounded grant for the `zengine.inventory` office in
`workshop/admission.hpp`, preserving it across ordinary loads and replacements. Other loaded
offices retain Workshop's existing admission policy; the artifact cannot approve its own grant.

## Inspect and edit through Workshop

Workshop's load plans also offer an **Inventory** pane. Open it and **Info** from the desktop's
pane list. **Drag an entry with the primary mouse button into Info** to inspect and edit an
independent copy. A click without movement only selects the entry. Enter picks up a copy for
keyboard pick-and-place, followed by a click in a receiver. Escape cancels either transfer.

For a live edit, right-click the entry and choose **Grab live entry reference**, or press
Ctrl+Enter on the selected entry, then click Info. Right-click also offers rename and removal.
The primary drag never removes the source entry. Inventory itself accepts a dropped value as
a new entry; dropping a copy back into Inventory duplicates it.

Both Editors carry into Inventory the same way: a selection arrives as a `SourceText` entry and a
file place as a `SourceLocation` entry, each with its observation as metadata, and an entry
dragged back onto an Editor is inserted as text, as a command's Terminal line, or reopened as a
file place ([source material](source-transfer.md)).

Use Up/Down or the wheel to navigate; Ctrl+S cycles added/name/type sorting while retaining the
selected identity. Ctrl+N renames, Delete asks for a second Delete to confirm removal, and
Ctrl+R refreshes the list. Once the collection has folders, Inventory browses one folder at a
time, with a location row, `[Up]`, crumbs, Backspace, Alt+Home, Ctrl+D (new folder) and
Ctrl+X/Ctrl+V (move); see [organize Inventory in named folders](../workshop/inventory-folders.md). Names and ordering are presentation choices; saved values and
references do not change position-dependent meaning. Small rooms retain a visible selected
entry and mark omitted rows when there is room.

Info shows the item's schema identity, field paths and values, followed by the separate capture
metadata. Select a scalar item field and press Enter to edit it; Enter keeps the field change
in the local draft and Escape cancels that field edit. Nested messages and lists expose their
existing scalar descendants. Metadata and byte fields are read-only in this presentation.

- **Ctrl+S** saves a received copy as a new entry, then edits that new entry on later saves.
  A live-reference draft saves back to its original entry. Info labels these modes **COPY**
  and **LINKED**. **Ctrl+Shift+S** stores a separate copy and leaves the view as it was.
- **Ctrl+R** fetches a fresh copy of the same live entry; for a not-yet-saved copy, it restores
  the value received by the drop. With unsaved edits, press it again to
  confirm discarding them. A failed read retains the draft.
- **Ctrl+D** discards local edits in favor of the last saved/read copy, without querying a source.
- **Ctrl+I** switches between this entry and Info's existing pane-property view.

Saving does not write to the weave that originally supplied the captured data. Capture metadata
continues to describe that acquisition; it does not certify subsequently edited values. Drafts
belong to the current Info image and are not persisted across an Info reload or process exit.
Up to four [independent Info views](../workshop/info-views.md) hold separate drafts side by side,
exchange typed fields, watch a linked entry and sample a captured structure's source again.

Each acquisition, fresh read and save asks Workshop to authorize its initiating input actor.
Physical input comes from the input office; an external host needs both `input` and `inventory`
powers. Input permission alone cannot authorize these operations. The pane still needs its own
ordinary message grant. Carrying a reference grants neither permission nor ownership of its data.

## References and conditional saves

The following additional doors use the same `zengine.inventory` role and guest `inventory` power:

```text
InventoryLocate{}                         -> InventoryEntry | zen.Refused
InventoryRead{reference}                  -> InventoryEntry | zen.Refused
InventoryWrite{reference, revision, pair}  -> InventoryEntry | zen.Refused
```

`InventoryEntry` contains `{reference: {owner, entry}, revision, pair}`. `owner` is an opaque random identity for this inventory image; `entry` is an opaque random
identity for one stored object. Replacing or reloading the inventory image invalidates its
previous references, even if state was retained. These are
current-process locators, not pointers, secrets, grants or portable restart identities.

Set and successful CaptureDescribe replace the compatibility slot with a new entry identity, even for identical
bytes. Read and Write refuse a reference to the old entry. Write validates the whole pair and
the expected revision, then increments that revision while preserving entry identity. A stale
writer, a wrong owner or malformed data changes nothing. Locate addresses only the compatibility slot and refuses when that slot is empty;
the older Get door retains its successful `occupied=false` answer.

Info retains a refused draft. If the entry was replaced, discard the local draft explicitly
before acquiring the new slot; a refresh never silently follows the replacement. An edit made
while an earlier save is pending remains unsaved after that earlier save succeeds.

## Named folders

The collection's owner keeps named folders and each entry's folder, beside entry names. Folders
are organization: they are never stored in pair bytes or capture metadata, and they grant or
execute nothing. All doors use the `zengine.inventory` role and the guest `inventory` power:

```text
InventoryFolderCreate{parent, name}               -> InventoryFolderState | zen.Refused
InventoryFolderRename{folder, revision, name}     -> InventoryFolderState | zen.Refused
InventoryFolderMove{folder, revision, into}       -> InventoryFolderState | zen.Refused
InventoryFolderRemove{folder, revision}           -> zen.Ack | zen.Refused
InventoryFile{reference, from, into}              -> InventoryEntry | zen.Refused
```

`InventoryFolderReference{owner, folder}` identifies a folder; `folder` is empty for the root. Like
an entry reference it is a current-process locator: a restore or reload rotates `owner`, and a
reference to another owner refuses. `InventoryFolderState` gives the folder's reference, its
revision, name and parent. Rename and move advance the revision; rename, move and remove require
the current one. Names and paths describe folders; they never identify them.

`InventoryFile` moves an entry into `into` only if it is still in `from`, so an operation begun
against an older picture refuses instead of taking the entry from a newer place. Moving an entry
changes nothing else about it: identity, revision, pair, metadata and label stay as they were, so
a draft read at that revision can still save. A move into the folder it is in answers unchanged.

- Names are 1 to 64 printable ASCII characters, without `/`, a leading or trailing space, `.` or
  `..`, and are stored exactly as sent. Sibling folders cannot share a name ignoring ASCII case;
  the same name under another parent is allowed. Entry labels keep their own rules.
- At most 128 folders, nesting at most eight deep. A folder cannot move into itself or anything
  inside it; a move that would nest too deep or collide with a sibling's name refuses.
- `InventoryFolderRemove` removes only an empty folder: no member entry, including one placed in a
  portable view, and no subfolder. It never removes an entry.
- Add v1, `InventoryCaptureAdd`, `InventorySet` and a successful `InventoryCaptureDescribe` store
  at the root. A replaced compatibility slot is a new entry at the root, never inheriting the old
  occupant's folder. `v2::InventoryAdd` refuses a folder that is no longer here.

A refusal changes nothing. Every change publishes `InventoryChanged` and advances the collection
revision, so the snapshot and restore fence below covers organization too. `InventoryList` v1
stays flat for older readers.

## The pair, and why it needs no second schema kind

The item's schema, and each metadata entry's schema, are never compiled into this package.
`InventorySet.pair` is one opaque `Bytes` field: what it decodes to is entirely up to the bytes
themselves. This is what lets a second, entirely unrelated item schema Set into the same
inventory without recompiling anything here -- a fixed Message field would fix one schema, and a
list has one declared element type, so heterogeneity is bought with exactly one level of
indirection instead: a self-sufficient envelope, `inventory/codec.hpp`'s `pair_schema()`:

```
zengine.inventory.Pair v1 {
    schemas:  zen.AcceptedShapes   -- the combined schema-descriptor closure: the item's
                                       root first, then each metadata entry's, in order
    item:     Bytes                -- the item, native-serialized against schemas' first root
    metadata: List<Bytes>          -- one native-serialized entry per remaining root, aligned
}
```

`encode_pair(item, metadata)` admits the item and every metadata entry against their **own**
declared schema first -- this is complete-data admission, never a draft's relaxed presence
(`zengine::message_draft::Draft` is the package that wants that relaxation; this one does not) --
then agrees the whole closure through one `loom::Registry::claim`, which is also the check that
refuses two roots disagreeing under one (name, version) before a byte is written.
`decode_pair(bytes)` reverses it, recovering every schema **from the bytes alone**: a caller
needs no compiled knowledge of the item's or any metadata entry's shape to read one back, only
the ordinary `loom::Value`/`loom::Cell` surface every schema-driven reader already uses.

## Metadata: automatically derived, not asserted

A metadata entry is data the *capture code* derived from the actual retrieval, never something
the inventory invents. Both capture doors ask `target_role`'s current holder `zen.PokeDescribe` -- the self-description
floor every woven Weave already answers unconditionally -- and stores the resulting
`zen.PokeStructure` as the item. Two metadata entries are generated from this acquisition:
`CaptureContext`, described below, and either `CaptureRequest` or `CaptureAddRequest`, whose
`request` field preserves the corresponding typed request.

| field | what it is |
|---|---|
| `requested_role` | a supplier claim: what the caller asked this weave to query |
| `request_shape`, `request_version` | what this weave actually sent (`zen.PokeDescribe v1`) |
| `answered_by` | an **observed** fact: the `WeaveId` Loom itself attested as the sender of the reply -- never read from the reply's own payload, and never a durable identity across a restart or a role that changes holders |
| `captured_at_epoch_s` | this process's own clock at the moment the answer settled -- a local reading, not a claim that anything else in the pair was observed simultaneously |

After a restart, the same numeric `answered_by` can name a different participant. Treat a saved
capture as a past observation. To inspect a supplier now, resolve the requested role again and
record the new answer; the role is a route and may have another holder. Neither an equal number
nor an equal role proves continuity with the stored source. Editing or saving the stored item
does not operate on that source.

`InventorySet` remains the generic door for a caller that already holds an encoded pair of its
own, from any schema. `InventoryCaptureDescribe` captures the standard self-description;
discovering arbitrary application data needs a different acquisition adapter.

Captured metadata is custodianship, not certification: storing a claim does not make it a
host-authenticated fact, a metadata claim never enlarges authority, and a captured participant
reference is not a renewed way to reach that participant later.

Capture accepts only Loom-authenticated answers to its outstanding role request. It handles
dispatch refusal only when Loom's provenance and the actual outgoing send attempt agree.
Responders built with older Loom headers sent ordinary substrate replies: rebuild them to
support strict capture. No ABI bump is required. A target that does not answer leaves this
capture pending; another capture is refused while it remains pending, even over another link.

`InventoryCaptured.pair` is that capture's own snapshot. `InventoryGet` reads the current slot;
another Set or completed capture may replace it at any time. The tool compares these two
answers before claiming successful readback, and retains its own capture if they differ.

## Lifetime and custody

Every value crossing into or out of the inventory is copied at the boundary -- the native
serialization in `encode_pair`, the fresh `loom::Value`s `decode_pair` admits, and the weave's own
`Bytes` field, which a `Get` answers with its own value-copy. Mutating a caller's source objects
after a Set, or mutating a receiver's own copy of a Get's bytes, reaches nothing this weave holds;
a snapshot already returned by an earlier Get stays independently readable after a later Set
replaces the slot.

Storage lasts for the current inventory instance. Removing the source does not remove a saved
pair. A same-shape image reload retains the state through Loom's normal handoff, but invalidates
old references and announces a fresh list. Unloading and loading starts empty. Explicit
[toolbox restore](#durable-toolboxes) repopulates it after restart; automatic recovery and
cross-version state migration are not provided.

## A worked example

```cpp
#include "inventory/codec.hpp"

namespace inv = zengine::inventory;

// The item: pure data, any schema -- here, one built at runtime with no C++ struct at all.
auto schema = loom::SchemaBuilder("example.Reading", 1)
    .field("celsius", loom::Kind::Float).build();
loom::Value item(schema);
item.set("celsius", loom::Cell::real(21.5));

// One metadata entry: what the capture code actually knows.
loom::Value note(loom::SchemaBuilder("example.Source", 1)
    .field("sensor", loom::Kind::Text).build());
note.set("sensor", loom::Cell::text("bench-3"));

const std::string bytes = inv::encode_pair(item, {note});
// ...InventorySet{pair: Bytes(bytes.begin(), bytes.end())} to role "zengine.inventory"...

// A generic reader needs none of the schemas above compiled in:
const inv::DecodedPair back = inv::decode_pair(bytes);
back.item.schema().name();              // "example.Reading"
back.item.get("celsius")->as_float();   // 21.5
back.metadata.front().get("sensor")->as_text(); // "bench-3"
```

## The reusable capture route

Use `workshop/inventory-collect --input target_role=zengine.input --input label=Input` to keep a
new entry for each capture. It writes `pair.bin` and `entry.json`, then verifies that exact
reference and revision with Read. `workshop/drag` sends a press, requests timed Input motion, then releases
and captures the actual before/after picture; it does not equate dispatched input with
acceptance by the destination.

The older `external-host/tools/workshop/inventory_capture.py` runs the `InventoryCaptureDescribe` +
`InventoryGet` journey against a real running Workshop from a Loom session, and writes the
returned envelope whole as `pair.bin` (`loom-session run <dir> workshop/inventory-capture
--input target_role=zengine.guests`). See
[workshop/external-host.md](../workshop/external-host.md) for the session setup and the
`"inventory"` guest power a launch's guests file must grant.

The install also supplies a generic reader:

```text
zengine-inventory-read path/to/pair.bin
```

It validates the complete envelope, recovers the item and metadata schemas from those bytes,
and prints JSON with schema identities, nested field values and their schema descriptors.
No sample types or source-tree headers are needed. Exit 1 means the file could not be read or
admitted; exit 2 is usage.
For C++ consumers, link `zengine::inventory` and call `decode_pair` as above.

## Following consumers

[Compose](../workshop/inventory-compose.md) receives typed fields and complete commands. It can
store a complete form as a new entry; submitting remains a separate authorized action.
Incomplete presets, portable views and [durable toolboxes](#durable-toolboxes) use this same
envelope. Bags and richer acquisition remain later consumers.

## Incomplete command presets

Inventory still admits every ordinary item and metadata value completely. A partial command
travels inside the complete `StoredDraft` data envelope from [message drafts](message-drafts.md),
which retains its original schema closure and missing fields. Info unwraps it for editing;
Compose checks it against the selected receiver and requires complete admission before submission.
The [preset workflow](../workshop/inventory-compose.md#make-a-reusable-preset-forward-or-backward)
explains independent copies, true unset, typed field pickup and explicit execution.

Info's Ctrl+G picks up a typed field copy after current actor authorization for
`PaneValueCarryRequested v1 -> zengine.workshop`. The guest `inventory` power includes this
permission; `input` or `inspect` alone does not. It copies locally held data, including unsaved
edits, and retains only selected content and its decoding ancestors. It does not re-read the
source or claim that the value remains current. Metadata stays read-only in Info even when
one of its fields is copied elsewhere. A pending pickup refuses reset and conflicting actions.

## Portable boxes, strips and command hotkeys

Right-click an entry to pop out a **single box**, **row**, or **column**. Right-click a view's
heading to create an empty view. Primary dragging moves an entry between these inventory views;
dropping before a strip entry reorders it. Dropping onto a filled single box returns its old entry
to main Inventory. Data remains with `zengine.inventory`; placement never deletes or executes it.
Drops into Info/Compose and the explicit **Pick up a copy** action retain copy semantics.

Placement and folders are separate facts with separate owners, and one gesture changes one of
them: dropping on a folder row, crumb or `[Up]` files the entry (its folder); dropping anywhere
else moves its placement. Main Inventory lists the entries placed there in the folder it shows,
and counts that folder's members placed in views. An entry returned or displaced to main
Inventory appears in its own folder.

**Duplicate with next number** creates an independent pair under the next available numbered
name; **Duplicate and name** opens the name editor first. Both retain configured key/target
settings but start with the copy disabled. Editing a copy never writes the source.

New copy drops offer an optional name after successful storage. Escape leaves the generated name
and data intact. Right-click **Rename entry...** changes an existing label through the ordinary
revision-checked rename door under the current actor's authority. Label editing never changes
entry identity or its binding. Portable views use compact shared-border tiles, with full entry
names in selection/context and a horizontally scrolling editor for small rooms.

**Configure command hotkey** takes an explicit target office and key, for example
`zengine.inventory alt+1`. Then **Enable item hotkey** enables that item's configuration.
The view's **Turn hotkeys ON/OFF** is a separate activation context. Main Inventory and new views
start OFF. Bindings follow entry identity across movement and sorting. Closing a view changes its
visibility; its explicit activation setting remains. Active collisions refuse the whole proposed
change and keep the previous arrangement. No activation is inherited from a duplicate's source.

A hotkey (or **Run configured command now**) reads that exact entry, refuses changed/missing
identity, materializes a StoredDraft only if complete, and requests the current actor's authority
for the actual destination and versioned command. The destination gate checks its current schema
at delivery. Captured target metadata never chooses the destination or grants permission.
Queued is not completed; a known dispatch refusal or authenticated Refused is displayed.
Conditional commands retain their original revision arguments; execution does not refresh them.

`inventory-pane/vocabulary.hpp` (installed with `zengine::inventory`) declares
`InventoryViewsRequested -> InventoryViews` and `InventoryViewEdit -> Ack | Refused`, addressed to
`zengine.inventory-pane`. Edit operations are `create` (single/row/column in text), `move`
(destination view and optional before-reference), `bind` (explicit target/key), `enable` (item),
and `context` (view). Main's view id is `inventory`; other ids are returned by discovery.
These operations configure presentation and never send stored commands. Workshop's guest
`inventory` power includes them; it does not confer permission to execute unrelated messages.

This implementation bounds one presentation to twelve extra views and sixteen configured
bindings. Arrow keys/wheel browse overflowing strips. Entry identities, arrangement and bindings
survive a same-shape presentation reload; pending gestures/operations do not. Inventory owner
replacement invalidates old references instead of rebinding by label. Explicit toolbox snapshots
provide disk persistence; focus-dependent contexts and automatic migration remain separate work.

The [portable-slot demo](../workshop/inventory-slots.md) exercises the visible path from an ELH.

## Durable toolboxes

At `zengine.inventory-pane`, `InventoryToolboxSave{path}` and
`InventoryToolboxRestore{path, replace}` answer `InventoryToolboxFinished{operation, path, entries}`
or `zen.Refused`. The completion contains the resolved absolute host path. `replace=false` requires
an empty collection. The pane owns the file and portable configuration; the data weave still owns
the collection. Direct callers need ordinary grants; Workshop guests need the separate `toolbox`
power, including for the actor behind injected input. File access uses the host process's rights.

The data-owner protocol is `InventorySnapshotRequested -> InventorySnapshot{owner, revision,
archive}` and `InventoryRestore{owner, revision, archive, replace} -> InventoryRestored{owner,
entries}`. The image-local comparison stamp fences all collection mutations; an outstanding
capture refuses replacement. The owner validates the complete archive before committing. Every
restore rotates the live owner identity and resets entry revisions to one. Stable archive keys
are used to reconnect saved placements and bindings only; old live references are not revived.

The file is a native serialized `InventoryToolbox v2`, carrying a `v2::InventoryArchive`: rows with
entry keys, names, complete pair bytes, compatibility-slot flags and folder keys, plus folder rows
(key, name, parent key), with view kinds/entry keys and explicit binding targets/keys. The reader
checks the file's claimed version before admitting it: a version 1 file (written before folders)
is read with every entry at the root, and any other version refuses. The owner's
`v2::InventorySnapshotRequested` and `v2::InventoryRestore` carry the same archive; version 1 of
both remains for flat collections, and a version 1 snapshot of an organized collection refuses
rather than dropping its folders. View identities are retained for Workshop setup references.
It contains no grants, owner nonce, live revision stamp,
activation flags, pending work or screen coordinates. Pair schemas and nested metadata remain
self-contained. StoredDraft remains the complete wrapper around an incomplete command. Source
and runtime schema availability do not rewrite historical values or publish their schemas into
the host registry. Actual invocation still encounters the destination's current gate.

All restored contexts and item bindings are disabled. The pane prepares configuration before
the data commit, installs it on the authenticated result, then clears old Desktop shortcuts.
A later cleanup refusal says entries were already restored, rather than claiming rollback.
Missing answers remain pending. Loaded-image replacement can lose pending coordinator work;
inspect the actual collection before retrying. This is not a distributed transaction.

At most 257 rows (one compatibility slot plus 256 saved entries), 8 MiB total encoded pair data,
32 MiB file bytes, 128 folders eight deep, twelve views and sixteen bindings are accepted. Bounded
reading, schema checks, unique keys, folder names, parents, cycles, depth and every member's
folder, placement membership and binding validation all precede replacement. Folder keys are
kept, folder revisions restart at one, and browsing restarts at the root. Single-writer file
saves use the existing sibling-write/replace discipline; no automatic save, crash journal or
power-loss guarantee. Save refuses unresolved configured references rather than guessing by name.
Restore reuses matching offered view identities; unused ones remain empty inactive spares. A union
of old and saved identities exceeding twelve refuses before the data commit; use a fresh Workshop.

See [saving and restoring toolboxes](../workshop/toolboxes.md) for the maker and one-request ELH path.
