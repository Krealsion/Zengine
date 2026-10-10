# `inventory/` — Inventory: working on it

The router for `inventory/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

The root's headers are the INTERFACE library `zengine-inventory-vocabulary`, exported as
`zengine::inventory`. Both weaves build only where the Loom provides `loom::kernel`.

| Path | What it is |
|---|---|
| `vocabulary.hpp` | the wire shapes and the office name `zengine.inventory`; installed |
| `codec.hpp` | the pair envelope: an item plus its metadata list; installed |
| `grant.hpp` | `inventory_grant()`, the bounded grant a host may give the office; installed |
| `weave.hpp`, `inventory.cpp` | `InventoryWeave`, the collection owner: weave `zengine-inventory` |
| `folders.hpp`, `archive.hpp` | folder names, bounds and trees; a toolbox archive's checks |
| `observation.hpp` | a structure observation's record, shared with Info's Sample Source |
| `pane_client.hpp` | a pane intent, its permission and owner answers; shared with Info, Composer |
| `read.cpp` | `zengine-inventory-read`, a program that prints a pair file as JSON |
| `inventory-pane/` | the pane, office `zengine.inventory-pane`: weave `zengine-inventory-pane` |
| `inventory-pane/slots.hpp` | portable box/row/column views and command hotkey bindings |
| `inventory-pane/toolbox.hpp`, `toolbox_file.hpp` | toolbox save and restore; the file format |
| `inventory-pane/vocabulary.hpp`, `command.hpp` | the pane's shapes (installed); a hotkey's send |

`inventory-pane/pane.cpp` is the pane weave; `browser.hpp` and `presentation.hpp` beside it hold
the folder browser's copy of a listing and what the pane draws.

## Law

- [inventory](../agents/inventory.md) — the inventory weave

The pane seam's law, `agents/panes.md`, and the installed surface's, `agents/packaging.md`, are
routed by the root.

## Suites

- `inventory` — the codec and the weave, mounted with a trusted grant: `tests/test_inventory.cpp`

Its cases in shared suites: `workshop_panes` (`tests/test_workshop_inventory_info.cpp`,
`tests/test_workshop_inventory_folders.cpp`, `tests/test_workshop_info_views.cpp`, on the rig
`tests/inventory_story.hpp`); `workshop_guests` (`tests/test_workshop_guests.cpp`, the guest
`inventory` and `toolbox` powers). `workshop_journey` reads captured pairs with
`zengine-inventory-read`.

## Host side

- `workshop/CMakeLists.txt` stages both weaves; `workshop/default-load-plan.json` and
  `workshop/graphical-load-plan.json` load them at `zengine.inventory`, `zengine.inventory-pane`.
- `workshop/admission.hpp` admits the `zengine.inventory` office with `inventory_grant()`.
- The pane asks Workshop through `workshop/pane_operation.hpp` (permission, actor scope),
  `workshop/pane_carry.hpp`, `workshop/pane_shortcuts.hpp` (hotkeys, at the Desktop) and
  `workshop/desktop_seam_vocabulary.hpp` (opening a portable view); `workshop/actor_scope.hpp`
  classes a toolbox save as a write.
- `workshop/guests.hpp`, `workshop/guests.cpp`: a guest's `inventory` and `toolbox` powers.
- `workshop/weave_terminal.cpp` answers `TerminalValueRequested`
  (`workshop/terminal_seam_vocabulary.hpp`) with a pair.
- `external-host/vocabulary_weave.hpp` declares both vocabularies for an external Loom host;
  `external-host/tools/workshop/inventory_*.py` drive Inventory from a session.

## Pages

- [Reuse a stored command through Compose](docs/inventory-compose.md)
- [Organize Inventory in named folders](docs/inventory-folders.md)
- [Portable inventory toolboxes](docs/inventory-slots.md)
- [Inventory: stored values with separate typed metadata](docs/inventory.md)
- [Save a toolbox for another session](docs/toolboxes.md)

## Practices

- [Inventory](../docs/contributing/best-practices.md#inventory-inventoryinventory-pane-inventory)
