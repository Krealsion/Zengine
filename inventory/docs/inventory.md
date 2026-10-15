# The Inventory pair: a value with its typed metadata

**Reference.** Every value Inventory keeps, every value a pane carries into or out of it and every
entry a toolbox file holds is one *pair*: an *item*, the value itself, and its *metadata*, a list
of typed facts about how it was captured, in one byte envelope that carries its own schemas.
`inventory/codec.hpp` is the format (`encode_pair`, `decode_pair`); link `zengine::inventory`. The
office that keeps pairs is [`zengine.inventory`](zengine.inventory.md), and its pane
[`zengine.inventory-pane`](zengine.inventory-pane.md).

## The envelope

An item's schema, and each metadata entry's, are never compiled into Inventory: a pair is one
`Bytes` field to the office, and what it decodes to is the bytes' own business. That is what lets
an unrelated schema be stored beside the others with nothing rebuilt. The envelope is the shape
`zengine.inventory.Pair` v1, `codec.hpp`'s `pair_schema()`:

```
zengine.inventory.Pair v1 {
    schemas:  zen.AcceptedShapes   -- the combined schema-descriptor closure: the item's
                                       root first, then each metadata entry's, in order
    item:     Bytes                -- the item, native-serialized against schemas' first root
    metadata: List<Bytes>          -- one native-serialized entry per remaining root, aligned
}
```

`encode_pair(item, metadata)` admits the item and every metadata entry against their own declared
schemas first -- complete-data admission, never a draft's relaxed presence -- then agrees the whole
closure through one `loom::Registry::claim`, which also refuses two roots disagreeing under one
name and version before a byte is written. `decode_pair(bytes)` reverses it, recovering every
schema from the bytes alone: a reader needs only the ordinary `loom::Value` and `loom::Cell`
surface.

Every value is copied at the boundary: `encode_pair` serializes, `decode_pair` admits fresh values.
Changing a source value after encoding, or a decoded value after reading, reaches nothing else.

## Metadata: derived, not asserted

A metadata entry is data the capture code derived from the retrieval itself, never something
Inventory invents. A capture by [`InventoryCaptureDescribe`](zengine.inventory.md#inventorycapturedescribe)
or [`InventoryCaptureAdd`](zengine.inventory.md#inventorycaptureadd) stores two: `CaptureContext`
v1, below, and `CaptureRequest` v1 or `CaptureAddRequest` v1, the typed request.

| field | what it is |
|---|---|
| `requested_role` | a supplier claim: what the caller asked Inventory to query |
| `request_shape`, `request_version` | what Inventory actually sent (`zen.PokeDescribe v1`) |
| `answered_by` | an **observed** fact: the `WeaveId` Loom itself attested as the sender of the reply -- never read from the reply's own payload, and never a durable identity across a restart or a role that changes holders |
| `captured_at_epoch_s` | this process's own clock at the moment the answer settled -- a local reading, not a claim that anything else in the pair was observed at the same time |

After a restart the same `answered_by` can name a different participant: a saved capture is a past
observation. To inspect a supplier now, resolve the role again and record the new answer. Neither
an equal number nor an equal role proves continuity, and editing the stored item never reaches its
source. Stored metadata is custody, not certification: a claim in it never enlarges authority, and
a captured participant is not a renewed way to reach that participant.

Other producers write their own metadata: an Editor's selection arrives as a `SourceText` item and
a file place as a `SourceLocation`, each with its observation
([source material](../../editor/docs/source-transfer.md)).

## An incomplete command

The office admits every item and metadata value completely. A partial command travels inside the
complete `StoredDraft` envelope of [message drafts](../../message-draft/docs/message-drafts.md),
which keeps its original schema closure and its missing fields. Info unwraps it for editing;
Compose checks it against the receiver it is dropped on, and requires complete admission before
it sends ([presets](inventory-compose.md#make-a-reusable-preset-forward-or-backward)).

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
// ...InventoryAdd{pair: Bytes(bytes.begin(), bytes.end()), label: "bench"} to "zengine.inventory"...

// A generic reader needs none of the schemas above compiled in:
const inv::DecodedPair back = inv::decode_pair(bytes);
back.item.schema().name();              // "example.Reading"
back.item.get("celsius")->as_float();   // 21.5
back.metadata.front().get("sensor")->as_text(); // "bench-3"
```

## Reading a pair from a file

The install supplies a generic reader:

```text
zengine-inventory-read path/to/pair.bin
```

It validates the complete envelope, recovers the item and metadata schemas from those bytes, and
prints JSON with schema identities, nested field values and their schema descriptors. No sample
types or source-tree headers are needed. Exit 1 means the file could not be read or admitted; exit
2 is usage.

Two session tools write one. `workshop/inventory-collect --input target_role=zengine.input --input
label=Input` keeps a new entry for each capture, writes `pair.bin` and `entry.json`, then reads that
exact reference and revision back. `external-host/tools/workshop/inventory_capture.py` captures into
the compatibility slot and reads it back (`loom-session run <dir> workshop/inventory-capture --input
target_role=zengine.guests`); its guest needs the `inventory` power
([external-host/docs/external-host.md](../../external-host/docs/external-host.md)).
