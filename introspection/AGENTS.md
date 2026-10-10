# `introspection/` — Loaded, Project and Powers: working on it

The router for `introspection/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| part | what it is |
|---|---|
| `introspection.cpp` | the loadable weave `zengine-introspection`: office `zengine.introspection` |
| `vocabulary.hpp` | durable names: office, pane keys, Powers action ids, stem, `LoadedSelected` |
| `loaded.hpp` | the Loaded pane's pure view: `parse_loaded`, `project_loaded`, `mark_selected` |
| `resolved.hpp` | the Project pane's pure view, `project_arrangement`; the budget Powers shares |
| `powers.hpp` | the Powers pane's pure half: `PowersUi`, `powers_question`, `project_powers_ui` |
| `CMakeLists.txt` | `zengine-introspection-view` (INTERFACE, the pure headers) and the weave |
| `docs/introspection.md` | the reference page; `docs/images/` holds its picture |

## Law

No register of its own. Its law is the pane seam's, `agents/panes.md` (the Loaded pane, "The
system can show what it is", the Powers pane), which the root routes; the reference page is
[docs/introspection.md](docs/introspection.md).

## Suites

No CTest entry of its own. Its cases in shared suites: `workshop_panes`
(`tests/test_workshop_panes_introspection.cpp` and `tests/test_workshop_panes_sampling.cpp`,
which also read this folder's sources as text, and `tests/test_workshop_panes_input.cpp`),
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
  `zen.ListLoaded`.
- `composer/composer.cpp` hears its `LoadedSelected`.

## Pages

- [Introspection — the `Loaded`, `Project` and `Powers` panes](docs/introspection.md)

## Practices

- [Loaded, Project and Powers](../docs/contributing/best-practices.md#loaded-project-and-powers-introspection)
