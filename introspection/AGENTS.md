# `introspection/` — Loaded, Project and Powers: working on it

The router for `introspection/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| part | what it is |
|---|---|
| `introspection.cpp` | the loadable weave `zengine-introspection`: office `zengine.introspection`, each delivery routed to its pane's seat |
| `seats.hpp` | the weave's three panes: `PaneSeat` (a room, its question, its pictures), `LoadedSeat`, `ArrangementSeat`, `PowersSeat` |
| `vocabulary.hpp` | durable names: office, pane keys, Powers action ids, stem, `LoadedSelected` |
| `loaded.hpp` | the Loaded pane's pure view: `parse_loaded`, `project_loaded`, `mark_selected` |
| `resolved.hpp` | the Project pane's pure view, `project_arrangement`; the budget Powers shares |
| `powers.hpp` | the Powers pane's pure half: `PowersUi`, `powers_question`, `project_powers_ui` |
| `powers_detail.hpp` | the Powers pane's words and row geometry: the chrome fit, the list's window, a power's line, the detail and sample rows |
| `CMakeLists.txt` | `zengine-introspection-view` (INTERFACE, the pure headers) and the weave, carrying its manual |
| `docs/zengine.introspection.md` | the weave's manual, answered for each of its panes; `docs/images/` holds its picture |
| `docs/zengine.arrangement.md`, `docs/zengine.powers.md`, `docs/zengine.sources.md` | the manuals of the three Workshop doors the panes ask, carried by `workshop/CMakeLists.txt`'s targets |

## Law

No register of its own. Its law is the pane seam's, `agents/panes.md` (the Loaded pane, "The
system can show what it is", the Powers pane), which the root routes; the manual is
[docs/zengine.introspection.md](docs/zengine.introspection.md).

## Suites

No CTest entry of its own. Its cases in shared suites: `workshop_panes_inspect`
(`tests/test_workshop_panes_introspection.cpp` and `tests/test_workshop_panes_sampling.cpp`,
which also read this folder's sources as text), `workshop_panes_terminal`
(`tests/test_workshop_panes_powers.cpp`) and `workshop_panes_hand`
(`tests/test_workshop_panes_input.cpp`); the panes' manual witness and their census driver, whose
rows are `tests/actions/zengine.introspection.txt`, are the first file's, and each door's manual
witness stands beside its cases),
`workshop_screen` (`tests/test_workshop_screen.cpp`), `workshop_load`
(`tests/test_workshop_load.cpp`) and `workshop_document` (`tests/test_workshop_document.cpp`,
a Compose target picked on the Loaded pane).

## Host side

- Booted by `workshop/default-load-plan.json` and `workshop/graphical-load-plan.json` (an
  optional row, office `zengine.introspection`); staged and listed as a pane weave in
  `workshop/CMakeLists.txt`.
- Its panes cross the external pane protocol: `workshop/pane_vocabulary.hpp` and the
  `workshop/pane_*.hpp` parts `introspection.cpp` includes.
- The doors it asks, mounted in `workshop/workshop.cpp`: `zengine.arrangement`
  (`workshop/arrangement.hpp`, `workshop/arrangement_vocabulary.hpp`), `zengine.powers`
  (`workshop/powers_door.hpp`, `workshop/powers_vocabulary.hpp`) and `zengine.sources`
  (`workshop/sample_door.hpp`, `workshop/sample_vocabulary.hpp`); Loom's Manager answers
  `zen.ListLoaded`. Each door carries its manual from this folder's `docs/`
  (`zengine_manual` on `zengine-workshop-load` and `zengine-workshop-vocabulary`) and answers
  `ShapeDocumentRequested` from it.
- `composer/composer.cpp` hears its `LoadedSelected`.

## Pages

- [`zengine.introspection` -- Loaded, Project and Powers](docs/zengine.introspection.md), the manual
- [`zengine.arrangement`](docs/zengine.arrangement.md), [`zengine.powers`](docs/zengine.powers.md)
  and [`zengine.sources`](docs/zengine.sources.md), the doors' manuals

## Practices

- [Loaded, Project and Powers](../docs/contributing/best-practices.md#loaded-project-and-powers-introspection)
