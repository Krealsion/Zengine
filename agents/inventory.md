# The inventory weave

Routed behind [AGENTS.md](../AGENTS.md). Public contract:
[the inventory](../docs/reference/inventory.md). Public headers also follow
[packaging](packaging.md); evidence follows [verification](verification.md). The guest
`"inventory"` power lives in `workshop/guests.hpp`/`guests.cpp`, documented in
[workshop/external-host.md](../docs/workshop/external-host.md).

- `inventory/codec.hpp` and `inventory/vocabulary.hpp` are public (`zengine::inventory`);
  `inventory/weave.hpp` is not -- Workshop is the only mounter, and a stranger reaches the
  weave only by message, never by constructing one.
- The pair envelope (`zengine.inventory.Pair v1`) is deliberately the same shape as
  `message_draft::Library`'s own encoding minus `OptionalProjection`: both collect each entry's
  schema as a root, agree the closure through one throwaway `Registry::claim` used purely as a
  consistency check, and carry `zen.AcceptedShapes`'s own `accepted`/`referenced` split
  (`zen/weave/describe.hpp`) plus native `serialize()` bytes per root. The inventory never
  relaxes required-field presence: a Set is complete admission or refusal, never a draft.
- `InventoryCaptureDescribe` is the one item schema this weave has compiled knowledge of
  (`zen.PokeDescribe` -> `zen.PokeStructure`), and it is a deliberate, narrow exception: every
  woven Weave answers `PokeDescribe` unconditionally (the no-secret-state floor,
  `zen/weave/poke.hpp`), and `PokeStructure` is a real `ZEN_SHAPE` struct the authoring sugar
  already knows how to receive -- unlike `zen.AcceptedShapes`, which is hand-built with no C++
  struct counterpart and so cannot sit in a `WeaveBase`'s `Accept<...>` without one. `InventorySet`
  remains the generic door; `InventoryCaptureDescribe` is sugar over it, not a second contract.
- The capture ask is a single-step `loom::AskBook` conversation (capacity 1: at most one
  in-flight capture per inventory), matched by correlation and bus-stamped sender exactly as
  `examples/workshop-probe/probe.hpp` matches its own, and `zen.DispatchRefused` is explicitly
  accepted so a target role nobody holds is refused in words rather than left pending forever
  (Loom has no ask timeout; an unmatched conversation would otherwise never settle).
- `CaptureContext.answered_by` is read from `Mail::sender()` on the received `PokeStructure`,
  never from the reply's own fields -- the reason the capture is done server-side, inside the
  weave that opened the ask, rather than relayed through a client that already decoded the answer.
- This weave's own grant (`inventory_grant()`) is unrelated to any guest's: it is what lets the
  weave itself send `zen.PokeDescribe` to an arbitrary target and answer with its own three
  reply shapes. A guest's `"inventory"` row grant (`workshop/guests.cpp`) is the separate,
  narrower question of which guests may reach this weave's doors at all.
- `tests/test_inventory.cpp` covers the codec (a runtime-defined schema with a nested message
  and list, independent custody, required-field refusal, schema conflict, malformed bytes) and
  the weave (truthful empty/replacement/refusal, a second differently-shaped item, receiver
  non-aliasing, an earlier snapshot surviving a later Set, a real CaptureDescribe round trip with
  attributable metadata, an unheld target role, and one capture in flight refusing a second).
  `tests/test_workshop_guests.cpp` covers what the `"inventory"` guest power reaches. The one
  live external-Loom-host journey is `external-host/tools/workshop/inventory_capture.py`
  (`loom-tool.json` for its manifest), evidenced in the phase's own reportback rather than
  repeated here.
