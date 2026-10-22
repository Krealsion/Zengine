# `app/` — The app host: working on it

The router for `app/`, a host: where its law, its suites, the side of it other hosts hold and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `CMakeLists.txt` | `zengine-app-vocabulary`, `zengine-app-folder` and `zengine-app-host` (header-only), `zengine-app`, `zengine-app-weave`; the artifacts staged beside the program |
| `main.cpp` | `zengine-app`: the folder, the artifact directory, the console's loop of turns and lines |
| `host.hpp` | `app::AppHost`: the composition, the operator seat, the place door, the close, the console |
| `weave.cpp` | `zengine-app-weave`, office `zengine.app`: runs flows then views, arranges the window |
| `files.hpp` | `app.json` read through the compat codec, and the app's files, bounded |
| `folder.hpp` | an app's folder written from a saved view and its flow, all judged first: the View Builder's Export app |
| `program.hpp` | where the running program stands, and `zengine-app` beside it |
| `vocabulary.hpp` | `zengine.app`, `zengine.app.host`, `AppPlaceAsked`, `AppPlaceSaid`, `ZengineAppFile` |
| `docs/app.md` | the reference: an app's folder, its console, its end |

## Law

- [app](../agents/app.md) — the app host

## Suites

- `app_host` — the probe's plan under the console, the window, the close: `tests/test_app_host.cpp`

Its cases in shared suites: `view` (`tests/test_view.cpp`, a view taking its window's keys) and
`view_builder` (`tests/test_view_builder.cpp`, Export app writing an app's folder). The
installed-package witness, `tests/package/run.cmake` (not a CTest entry), starts the installed
`zengine-app` and decides its rows at its console.

## Host side

- The plan's executor and its codec are Workshop's, header-only: `workshop/load_execute.hpp`,
  `workshop/load_persist.hpp` (`zengine-workshop-load`).
- The Flow host and the view host, composed as Workshop composes them: `flow/flow-host/runtime.hpp`,
  `view/host.hpp`.
- The decisions, the warden and the `authority` command are Loom's installed `loom::host-policy`.
- The root `CMakeLists.txt` adds this folder after `workshop/`, and `examples/tally-app/` after it.
- Installed: `zengine-app` and `zengine-app-weave`, `cmake/ZengineInstall.cmake`. In a build,
  staged beside Workshop too (`workshop/CMakeLists.txt`), where the View Builder's Export app
  (`view/view-builder/pane.cpp`) names it in an app's launchers.

## Pages

- [Running an app on its own](docs/app.md)

## Practices

- [Write Loom, not host code](../AGENTS.md#intent-evidence-and-architectural-fit)
