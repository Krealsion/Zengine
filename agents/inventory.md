# The inventory weave

Routed behind [AGENTS.md](../AGENTS.md). Public contract:
[inventory](../docs/reference/inventory.md). Packaging follows [packaging](packaging.md);
verification follows [verification](verification.md).

- `zengine::inventory` exports the codec, vocabulary and explicit `inventory_grant()` helper.
  `zengine-inventory` is an ordinary
  loadable artifact, installed and selected by Workshop's authored load plans. No native host
  mount owns its office. Unload/load starts an empty slot; persistence is not promised.
- `inventory/codec.hpp` carries complete item and metadata schema roots plus their dependency
  closure and separately serialized values. Every inner value admits against its declared
  root; malformed replacement retains the old pair. Draft presence relaxation does not apply.
- A role capture uses an `AskBook` of capacity one. The book is bookkeeping, not authentication:
  require `Mail::answers_ask()` before settling. Poke responders must be rebuilt with Loom's
  authenticated substrate-answer headers. Ordinary forged structures do nothing.
- Dispatch refusal requires `Mail::dispatch_refused()`, the bound outgoing attempt, and the
  authored role, shape, version and address kind. A payload with that shape alone does nothing.
- Successful capture answers `InventoryCaptured` with that operation's own stored pair. Get is
  the current shared slot, so the ELH tool compares it with the capture answer before claiming
  readback. A later writer cannot silently change which capture the tool reports.
- Capture metadata is separate from the pure structure: `CaptureContext` records observations;
  `CaptureRequest` retains the nested typed request. Stored claims confer no authority. A target
  that does not answer may leave the capture pending; a second link does not free this slot.
- `workshop/admission.hpp` preserves the inventory office's bounded grant when loading its
  artifact. Emit declarations do not grant authority. This grant is separate from a guest's
  `inventory` power, owned by `workshop/guests.hpp` and `guests.cpp`; see
  [external-host](../docs/workshop/external-host.md).
- `tests/test_inventory.cpp` covers admission, nested custody, forged replies/refusals, exact
  capture results and real artifact unload/load. The installed-package consumer uses the public
  codec and vocabulary; `zengine-inventory-read` renders arbitrary captured shapes. The tool's
  readback race is pinned by `tests/session/workshop_tool_checks.py`, run by `workshop_journey`.
