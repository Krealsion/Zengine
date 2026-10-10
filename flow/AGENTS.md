# `flow/` — Flow: working on it

The router for `flow/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `CMakeLists.txt` | `zengine-flow` (`zengine::flow`): header-only, no weave; `zengine-flow-tool` |
| `flow-host/` | the session host, headers only: `runtime.hpp` (`RuntimeHost`), `vocabulary.hpp` |
| `flow-pane/` | `zengine-flow-pane`, a loadable weave in office `zengine.flow`: `pane.cpp` |
| `flow-pane/*.hpp` | `model.hpp` edits, `view.hpp` picture, `find.hpp` door ask, `vocabulary.hpp` |
| `author.hpp` | `Draft`: a definition authored by text commands |
| `graph_edit.hpp`, `graph.hpp` | `GraphDraft` and node places; a definition as text lines and SVG |
| `shape.hpp` | the one model of an authored message shape, the View Builder's too |
| `project.hpp`, `workspace.hpp` | the saved project; `Workspace`: graph, layout, examples, forms |
| `generate.hpp` | C++ generation, `recover_cpp` and `write_generated` |
| `native.hpp`, `native_abi.h` | `NativeWeave` and the generated artifact's ABI |
| `compiled.hpp` | `CompiledModule`, which opens a generated artifact |
| `main.cpp`, `example.hpp` | the workbench, built as `zengine-flow` with a kernel; the thermostat |

## Law

- [flow-host](../agents/flow-host.md) — flow host integration
- [flow](../agents/flow.md) — flow authoring and generation

## Suites

- `flow` — the library, host and pane logic without Workshop: authoring, generation and
  generated weaves through the real kernel (`tests/test_flow.cpp`, its fixtures written by
  `tests/flow_generate.cpp`), sessions (`tests/test_flow_runtime.cpp`), the pane's model and
  picture (`tests/test_flow_graph.cpp`, `tests/test_flow_view.cpp`)
- `flow_pane` — the loaded `zengine-flow-pane` through the real kernel: authoring, runs, reload,
  dialogs, gestures, carried shapes and the discovery door, in `tests/test_flow_pane.cpp`

Its cases in shared suites: `workshop_panes` (`tests/test_workshop_panes_powers.cpp`: Flow
asking the discovery door; `tests/test_workshop_inventory_info.cpp`: values and shapes dragged
onto Flow's canvas), `workshop_files` (`tests/test_workshop_files.cpp`: the pane's recipe entry)
and `workshop_shapes` (`tests/census_headers.hpp`: its shapes). Outside CTest,
`tests/package/run.cmake` builds `tests/package/flow_author.cpp` and `tests/package/flow_host.cpp`
against the installed package.

## Host side

- `workshop/workshop.cpp` constructs and mounts `flow_host::RuntimeHost` over Workshop's own bus
  and operator catalog, in the office `zengine.flow.host`.
- `workshop/default-load-plan.json` and `workshop/graphical-load-plan.json` load
  `zengine-flow-pane` as optional role `zengine.flow`; `workshop/CMakeLists.txt` stages it and
  lists it among the recipe catalog's pane weaves.
- The pane asks `zengine.powers` (`workshop/powers_door.hpp`, `workshop/powers_vocabulary.hpp`),
  and asks to write and to carry through `workshop/pane_operation.hpp` and
  `workshop/pane_carry.hpp`, answered in `workshop/weave_operation.cpp`. It has no seam
  vocabulary of its own; it speaks the pane protocol (`workshop/pane_vocabulary.hpp`).
- `view/view-builder/` links `zengine-flow` for `shape.hpp`; `cmake/ZengineInstall.cmake`
  installs the root headers and both `vocabulary.hpp`s and exports `zengine::flow`.

## Pages

- [Make and run a Flow definition](docs/flow-guide.md)
- [Flow authoring and native generation](docs/flow-reference.md)
- [Flow in an existing host](docs/flow-runtime.md)
- [Author a running graph in Workshop](docs/flow.md)

## Practices

- [Flow](../docs/contributing/best-practices.md#flow-flowflow-pane-flow-flowflow-host)
