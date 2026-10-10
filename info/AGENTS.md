# `info/` — Info: working on it

The router for `info/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `CMakeLists.txt` | target `zengine-info-pane`, a loadable weave; needs `loom::kernel` |
| `pane.cpp` | the weave: the panes list, one pane's properties, and the value views it offers |
| `value_view.hpp` | one value view: its draft, field selection, watch, sample and pending asks |
| `vocabulary.hpp` | office `zengine.info`, pane keys, stem, action ids, `InfoPaneState` |
| `docs/info-views.md` | the weaver's page; its pictures are under `docs/images/` |

## Law

- [info-body](../agents/workshop/info-body.md) `WL-INFO` — Info's pane-property view in
  `info/pane.cpp`; witnessed by `workshop_panes` (info, settings)
- [info-controls](../agents/workshop/info-controls.md) — the Info controls; witnessed by
  `workshop_panes` (info)

The value views' law (`info/value_view.hpp`: the views, drafts, presets, field pickup) is in
`agents/inventory.md`, which [inventory/AGENTS.md](../inventory/AGENTS.md) routes; the subject rows
Info shows and the settings it writes are `agents/workshop/pane-manager.md` and
`agents/workshop/settings.md`, which [workshop/AGENTS.md](../workshop/AGENTS.md) routes.

## Suites

No entry of its own. Its cases in shared suites: `workshop_panes`
(`tests/test_workshop_panes_info.cpp`, the loaded pane over the host's rows;
`tests/test_workshop_info_views.cpp`, the value views; `tests/test_workshop_inventory_info.cpp`,
Inventory through Info) and `workshop_persistence` (`tests/test_workshop_persistence.cpp`, the
desk row naming `zengine.info/info`). The subject rows it shows are the host's, witnessed in
`workshop_host` (`tests/test_workshop_host.cpp`). The rig in `tests/workshop_support.hpp` resolves
its stem to `WORKSHOP_SO_INFO_PANE`, and `tests/inventory_story.hpp` loads it beside Inventory;
`workshop_files` (`tests/test_workshop_files.cpp`) checks its development-catalog row.

## Host side

- Plan rows: `workshop/default-load-plan.json`, `workshop/graphical-load-plan.json` (optional,
  office `zengine.info`); staged beside the host and listed as a development pane in
  `workshop/CMakeLists.txt`.
- The desk row: `default_setup` in `workshop/setup.hpp` puts it in the right column, its office
  spelled in `workshop/panes.hpp`; a saved `zengine.workshop/info` converts in
  `workshop/pane_migration.hpp`.
- The inspection seam: `workshop/inspection_seam_vocabulary.hpp`, answered in
  `workshop/weave_inspection.cpp` over rows from `workshop/screen_pane_subject.cpp`; Workshop's
  grant to say the picture, `workshop/grant.cpp`.
- The value views ask Workshop's operation and observation doors (`workshop/pane_operation.hpp`,
  `workshop/weave_operation.cpp`) and Inventory (`inventory/pane_client.hpp`,
  `inventory/observation.hpp`).
- Who asks its office: the Pane Manager's inspect row (`kInfoRole` in
  `workshop/desktop-pane/vocabulary.hpp`), and the demo power's reset in `workshop/guests.cpp`.

## Pages

- [Independent Info views and the inspection workbench](docs/info-views.md)

## Practices

- [Info](../docs/contributing/best-practices.md#info-info)
