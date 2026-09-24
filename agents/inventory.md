# The inventory weave

Routed behind [AGENTS.md](../AGENTS.md). Public contract:
[inventory](../docs/reference/inventory.md). Packaging follows [packaging](packaging.md);
verification follows [verification](verification.md).

- `zengine::inventory` exports the codec, vocabulary and explicit `inventory_grant()` helper.
  `zengine-inventory` is an ordinary
  loadable artifact, installed and selected by Workshop's authored load plans. No native host
  mount owns its office. Unload/load starts empty; an explicit toolbox restore repopulates it.
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
- References identify an owner and one entry, never a raw pointer or the slot's next occupant.
  Set/Capture replace identity; Write checks revision and preserves identity. Refusals leave
  storage unchanged. `test_inventory.cpp` owns these cases.
- `inventory-pane/` owns the collection presentation; `info-pane/value_view.hpp` owns each Info
  view's value draft (copy or linked entry) using `message-draft::Draft`. Metadata remains separate and
  read-only in this UI. A late save answer never erases newer edits; a failed fresh read never
  silently retargets a replaced entry. The loaded-pane stories are in
  `tests/test_workshop_inventory_info.cpp` and `tests/test_workshop_info_views.cpp`.
- The shared image-local `inventory/pane_client.hpp` binds permission and owner replies with
  authenticated answers; Loom's `RoleRequest` retains each stage's actual typed send and matches
  dispatch refusals against the exact attempt and address. Domain validation precedes forgetting;
  these pending records do not belong in reload state. Only the current actor's input gesture can authorize
  an operation; a reference carries no authority.

- Collection Add/CaptureAdd append independent entries; List returns summaries. Rename/Remove
  require the current entry revision. The legacy Set/Get/CaptureDescribe slot remains separate
  and cannot overwrite appended entries. Up to 256 saved entries coexist with that slot.
- Default primary dragging carries the owned pair with an image-local transfer token. Inventory views move the referenced placement; other receiving panes copy. Secondary acquisition carries an explicit
  live reference. Info's copy save creates a new entry; later saves address that new identity.
  A value whose schema happens to be InventoryReference remains data on the copy route.
- Inventory list pictures map rows to entry identities with `component::RowMap`. Sorting and
  renaming cannot redirect a queued press; source data is read by reference, never row index.
  The small-room projection uses `cursor_window` and accounts for its marker rows.

- Info recognizes StoredDraft as a preset and edits its enclosed original-schema Draft.
  Make-preset always forks a copy; unset removes content. Copy save appends, live save checks
  the exact reference/revision. Pending saves cannot be retargeted by conversion.
- Info field pickup owns a selected-branch FieldValue copy before requesting current actor
  permission for PaneValueCarryRequested. Authentic permission and carry answers plus exact
  dispatch refusals settle it. Pickup blocks reset/replacement; metadata remains read-only.
  The guest inventory power includes this acquisition, never authority to submit other shapes.

- Info offers `info` plus slots `info.2`..`info.4`, each offered on first use and reused, never
  minted. Each view owns draft, structural selection, RowMap picture and every pending record;
  weave-wide monotonic correlations plus answer provenance let only the asking view settle.
  Retiring drops the records, so an old incarnation's late answer settles nothing. Only `info`
  keeps pane-property inspection (Workshop holds one subject per office).
- Controls, keys and menu rows call one act; availability and refusals share one reason. A
  primary drag acquires a field only on the first motion; a FieldValue drop fills the pictured
  field row after `require_type`, whole-value drops keep open-subject meaning and never replace a
  dirty draft, and stale pictures or metadata targets refuse unchanged.
- Watch spends Workshop's observation lease (`agents/panes.md`): invalidation-driven, one cycle
  per view, a trigger during a cycle remembered once. Clean drafts adopt and mark changes; dirty
  drafts keep text and base revision and hold only the newest entry. Pause, close, hide, reload,
  entry loss or a refused continuation end it; restore and fork never start it.
- Sample Source asks only a recorded PokeDescribe route (`inventory/observation.hpp`), per view,
  under current actor permission, never a stored request. The answer's pair records Loom's
  attested sender afresh; stored data changes only on Save or Save copy. A differing WeaveId proves
  another incarnation; an equal one proves nothing across processes.

- Portable placement and binding configuration belong to inventory-pane, never the data owner.
  Main Inventory and new contexts start inactive. Explicit duplication appends an independent
  owner entry and copies binding settings disabled. Movement never executes. A filled single box
  returns its displaced reference to main Inventory; an unsuccessful proposal retains placement.
- Desktop admits active globals atomically through PaneShortcuts. Invocation traverses Workshop's
  current attributed action and checks actor permission for the stored message's actual target
  and version. Configuration grants and metadata do not authorize execution. Complete messages
  meet the destination's current gate; incomplete presets refuse before sending.
- Tests in test_workshop_inventory_info.cpp exercise portable layouts, duplication, collisions,
  current actor authority and attributed dragging. Public vocabulary joins the package witness;
  inventory-slots-demo provides the maintained visible journey. Pending tokens/read/permission
  books remain image-local. Restored references never rebind to a different owner by label.
- Portable tile interiors and their hit spans come from one composition. Shared borders and
  clipped tiles are not entry targets. Copy-drop naming begins only after owner storage succeeds;
  cancelling it leaves the stored value. Rename uses the current naming gesture's authority and
  exact entry revision, never a cached grant from the earlier drop.

- Collection snapshots have an image-local owner/revision fence. Restore validates every row
  and pair before committing, refuses pending capture or a concurrent writer, and rotates the
  owner nonce. Stable archive keys reconnect portable configuration, never old live references.
- Toolbox file/presentation coordination belongs to inventory-pane; collection replacement stays
  in inventory. The file excludes live owner/stamp, activation, pending work and authority. Schema
  closures remain stored data, not registry publication. References embedded in user values are
  unchanged. Source absence is not a reason to mutate historical data.
- Restored binding/context switches are OFF. Unused offered views become empty inactive spares;
  repeated restore reuses identities. Desktop cleanup follows the owner commit; a later refusal
  explicitly reports that the collection already changed. No distributed rollback is claimed.
- Guest file access uses the separate `toolbox` power, checked for injected actors too. A file
  operation never executes stored commands. The bounded file reader and existing single-writer
  replacement helper preserve the previous completed file on failure. Tests in test_inventory.cpp
  and test_workshop_inventory_info.cpp cover files, conflicting writers, fresh references,
  inactive configuration and attributed file access.
