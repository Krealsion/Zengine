# The inventory: one captured item, with its own typed metadata

**Reference.** `zengine::inventory` provides one message-reachable slot: empty, or one
associated *item* plus its *metadata* -- a separate, automatically derived list of typed facts
about how the item was captured. Include `inventory/codec.hpp` for the byte envelope
(`encode_pair`/`decode_pair`), or `inventory/vocabulary.hpp` for the wire shapes a stranger needs
to Set, Get or ask for a capture. The weave itself (`inventory/weave.hpp`) is internal: Workshop
mounts one, unconditionally, as an ordinary participant beside Files, the Builder and the rest.

## The one slot

The inventory holds either nothing, or one pair: an *item* (the pure data a maker wanted to
keep) and its *metadata* (a list of separate, typed observations about the item's capture).
**Set** replaces the whole pair as one operation. **Get** returns the pair, or an understandable
empty result (`occupied: false`, `pair` empty) before the first successful Set -- never a
refusal merely for being empty. A **refused** Set changes nothing: the previous pair, if any,
stays exactly as it was.

```
Set(pair) -> zen.Ack | zen.Refused{reason}
Get()     -> InventoryState{occupied, pair}
```

Both are addressed to the role `zengine.inventory`. Over a guest link (see
[workshop/external-host.md](../workshop/external-host.md)), reaching either needs the guest's own
`"inventory"` power -- a power of its own, never a reinterpretation of `"inspect"`'s read-only
discovery grant.

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
the inventory invents. `InventoryCaptureDescribe{target_role}` is the one capture source this
package ships: it asks `target_role`'s current holder `zen.PokeDescribe` -- the self-description
floor every woven Weave already answers unconditionally -- and stores the resulting
`zen.PokeStructure` as the item, with one automatically produced `CaptureContext` metadata entry:

| field | what it is |
|---|---|
| `requested_role` | a supplier claim: what the caller asked this weave to query |
| `request_shape`, `request_version` | what this weave actually sent (`zen.PokeDescribe v1`) |
| `answered_by` | an **observed** fact: the `WeaveId` Loom itself attested as the sender of the reply -- never read from the reply's own payload, and never a durable identity across a restart or a role that changes holders |
| `captured_at_epoch_s` | this process's own clock at the moment the answer settled -- a local reading, not a claim that anything else in the pair was observed simultaneously |

`InventorySet` remains the generic door for a caller that already holds an encoded pair of its
own, from any schema; `InventoryCaptureDescribe` is deliberately narrow rather than a general
grabber -- see `inventory/weave.hpp` for why `zen.PokeDescribe`/`zen.PokeStructure` earns this
one exception and nothing wider.

Captured metadata is custodianship, not certification: storing a claim does not make it a
host-authenticated fact, a metadata claim never enlarges authority, and a captured participant
reference is not a renewed way to reach that participant later.

## Lifetime and custody

Every value crossing into or out of the inventory is copied at the boundary -- the native
serialization in `encode_pair`, the fresh `loom::Value`s `decode_pair` admits, and the weave's own
`Bytes` field, which a `Get` answers with its own value-copy. Mutating a caller's source objects
after a Set, or mutating a receiver's own copy of a Get's bytes, reaches nothing this weave holds;
a snapshot already returned by an earlier Get stays independently readable after a later Set
replaces the slot.

The required lifetime is the current inventory instance's, for as long as the Workshop process
that mounted it runs. Disk persistence, replacement handoff across a reload, and restart recovery
are not provided by this phase; a future consumer that needs one of those names it as its own
requirement.

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

`external-host/tools/workshop/inventory_capture.py` runs the `InventoryCaptureDescribe` +
`InventoryGet` journey against a real running Workshop from a Loom session, and writes the
returned envelope whole as `pair.bin` (`loom-session run <dir> workshop/inventory-capture
--input target_role=zengine.guests`). See
[workshop/external-host.md](../workshop/external-host.md) for the session setup and the
`"inventory"` guest power a launch's guests file must grant.

## Following consumers

A concrete drag-out, bags, richer contextual acquisition from real pane subjects, and
persistence are named future consumers in the founder's private goals, not selected by this
phase. This page describes the storage/capture contract those consumers share; it does not
itself add a pane, an inspector, or a second metadata catalog.
