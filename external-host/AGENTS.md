# `external-host/` — Workshop from another host: working on it

The router for `external-host/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `CMakeLists.txt` | `zengine-guest-vocabulary`, a loadable weave, built without the sanitizer |
| `vocabulary_weave.hpp` | the shapes another Loom host declares to reach Workshop; no office |
| `vocabulary_weave.cpp` | exports that weave; a `loom-boot.json` row boots it |
| `connections-pane/` | `zengine-connections-pane`, loadable weave: the Connections pane |
| `connections-pane/vocabulary.hpp` | office `zengine.connections`, pane key, stem, state |
| `demo-control/` | `zengine-demo-control`, loadable weave: a setup's Demo pane and Reset |
| `demo-control/control.hpp` | the weave, office `zengine.demo`; `vocabulary.hpp`: `Demo*` shapes |
| `demo.py` | the setup launcher: list, describe, start, status, walk, reset, stop, export |
| `tools/workshop/` | the `workshop` tool package: `loom-tool.json` and helper modules |
| `tools/workshop/setups/` | the described setups, each a folder: `setup.json`, `desk.json`, `README.md` |
| `tools/workshop/toolboxes/` | the inspection workbench's packaged toolboxes (`workbench.py`) |
| `tools/desk/` | the `desk` tool package: `desk/read` and `desk/watch`, the desk written as text |

## Law

No register of its own. Workshop's side is in `agents/workshop/guests.md` (a guest's hand) and
`agents/workshop/desk-read.md` (the desk said whole), which
[Workshop's router](../workshop/AGENTS.md) routes; both panes keep the pane seam's law,
`agents/panes.md`, which the root routes. The guests file and the tools are public in
[Drive Workshop from another host](docs/external-host.md).

## Suites

- `demo_recipes` — setups and their preparation, no window (`tests/session/demo_recipes.py`)
- `guest_journey` — a real Workshop admitting this program as a guest (`tests/guest_journey.cpp`)
- `guest_vocabulary` — the weave's shapes resolve by identity (`tests/test_guest_vocabulary.cpp`)
- `workshop_journey` — a Loom session runs the tools (`tests/session/workshop_journey.py`)

`demo_recipes` and `workshop_journey` need the `session` gate; the journey also runs the
`workshop_*_checks.py` and `desk_*_checks.py` beside it. Its parts' cases in shared suites:
`workshop_panes` (`tests/test_workshop_panes_attention.cpp`, the Connections pane;
`tests/test_workshop_demo.cpp`, the demo controls); the Connections pane's stem and source in
`workshop_load` and `workshop_files`. The guest door is `workshop_guests`
(`tests/test_workshop_guests.cpp`).

## Host side

- Staged beside the host, the Connections pane listed as a development pane:
  `workshop/CMakeLists.txt`; its plan rows: `workshop/default-load-plan.json`,
  `workshop/graphical-load-plan.json`. `demo.py` adds `zengine-demo-control` to a setup's plan.
- The guest door, mounted by `workshop/workshop.cpp` under `--guests`: `workshop/guest_door.hpp`;
  the guests file and its powers: `workshop/guests.hpp`, `workshop/guests.cpp` (`demo` grants the
  `Demo*` shapes to `zengine.demo`); a guest's hand: `workshop/actor_scope.hpp`.
- Seam vocabulary: `workshop/guest_seam_vocabulary.hpp` (`GuestConnections`, asked of
  `zengine.guests`); setup preparation's owner doors: `workshop/setup_control.hpp`.
- Installed by `cmake/ZengineInstall.cmake` (the weave, both tool packages under
  `share/zengine/loom-tools`); the public setups page: `workshop/docs/demo-setups.md`.

## Pages

- [Author and check a recipe through an ELH](docs/elh-recipe-journey.md)
- [Drive Workshop from another host](docs/external-host.md)

## Practices

- [Connections](../docs/contributing/best-practices.md#connections-external-hostconnections-pane)
- [The external host](../docs/contributing/best-practices.md#the-external-host-external-host-external-hostdemo-control)
