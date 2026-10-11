# `builder/` — The Builder: working on it

The router for `builder/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| part | what it is |
|---|---|
| `vocabulary.hpp` | a build's shapes and offices: `zengine.builder`, `zengine.build-runner` |
| `recipe.hpp` | an authored recipe as pure data: a name, its artifact, one CMake mechanism |
| `generate.hpp` | a recipe as one `cmake -P` driver, and a single source's generated project |
| `run.hpp` | process custody: `start_recipe`, `RunningRecipe`; not a shell |
| `runner.hpp` | `BuildRunnerWeave`, the runner: the one weave that starts and holds a process |
| `weave.hpp` | `BuilderWeave`, the tool: recipe names, artifacts, `BuildStatus`; starts nothing |
| `CMakeLists.txt` | `zengine-builder-vocabulary`, one header-only INTERFACE target; no weave |
| `builder-pane/` | the pane: `zengine-builder-pane`, a loadable weave (needs `loom::kernel`) |
| `builder-pane/vocabulary.hpp` | its office `zengine.builder-pane`, pane key, action ids, state |
| `builder-pane/pane.cpp` | `BuilderPaneWeave`: its rows, its keys, its asks of the tool and host |

## Law

- [authoring](../agents/workshop/authoring.md) `WL-AUTH` — the two authored files gain a writer, and
  it is the weaver's own act; witnessed by `workshop_panes` (files: the chooser; builder: `o`),
  `workshop_load` (the append door and the plan writer), `workshop_files`
- [build-output](../agents/workshop/build-output.md) — what a build said; witnessed by `builder`
  (the runner's bytes, the kept record), then `workshop_panes` (output)
- [project](../agents/workshop/project.md) `WL-PROJ` — the project anchor and the recipe catalog;
  witnessed by `workshop_files` and `workshop_panes` (builder, editor), then `workshop_load` and
  `files_weave`

The build conversation's own law, `agents/realization.md`, and a pane's code,
`agents/workshop/code.md` (`WL-CODE`: a running pane followed to its source and on to its build, the
development launch), are routed by [workshop/AGENTS.md](../workshop/AGENTS.md).

## Suites

- `builder` — real child processes (`cmake -E …`, `tests/slow_build.cmake`) through the tool and
  runner mounted in-process: `tests/test_builder.cpp`, over the fixture tree `tests/buildfixture/`

Its cases in shared suites: `workshop_panes` (`tests/test_workshop_panes_builder.cpp`,
`tests/test_workshop_panes_output.cpp`, `tests/test_workshop_panes_code.cpp`), `workshop_files`
(`tests/test_workshop_files.cpp`), `workshop_load` (`tests/test_workshop_load.cpp`).

## Host side

- the mount: `workshop/workshop.cpp` mounts the runner and the tool in-process, each with its own
  grant; the runner's one CMake is `ZENGINE_BUILDER_CMAKE`, set in `workshop/CMakeLists.txt`
- the catalog both read: `workshop/recipes.hpp`, `workshop/recipe_persist.hpp`, shipped from
  `workshop/default-build-recipes.json.in`; `workshop/authoring.hpp` writes recipe and plan rows
- realization: `PlanBooter` in `workshop/load_execute.hpp` holds `zengine.realization`;
  `workshop/staging.hpp` places a built product, a reload's copy and a promotion
- the pane: a row in `workshop/default-load-plan.json` and `workshop/graphical-load-plan.json`,
  staged by `workshop/CMakeLists.txt`; it asks through `workshop/builder_seam_vocabulary.hpp`,
  answered by `ProjectDoor` and `PlanDoor` in `workshop/pane_doors.hpp`
- the rest: `workshop/pane_migration.hpp` moves an old desk's Builder pane to its office;
  `workshop/guests.cpp` lets a guest that sees `BuildStatus` ask for it

## Pages

- [The Builder pane](docs/zengine.builder-pane.md), a manual, compiled into the pane
  (`MANUAL` in `builder-pane/CMakeLists.txt`) and answered on ask (`PaneDocumentRequested`)
- [The Builder](docs/zengine.builder.md), a manual, compiled into whatever mounts the office
  (`zengine_manual` in `CMakeLists.txt`); the office answers a shape's section from it
- [The recipe file](docs/recipes.md)
- [Build your first pane](docs/tutorials/build-your-first-pane/README.md), a tutorial
- [Edit a running pane](docs/edit-a-running-pane.md)

## Practices

- [The Builder](../docs/contributing/best-practices.md#the-builder-builderbuilder-pane-builder)
