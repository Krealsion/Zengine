# `launcher/` — The launcher: working on it

The router for `launcher/`, a feature: where its law, its suites and the side of it others hold
are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `CMakeLists.txt` | `zengine-launcher-vocabulary`, `zengine-launcher-catalog` (header-only), `zengine-launcher`; the demo service and its setups fixed at configure |
| `weave.cpp` | `zengine-launcher`, office `zengine.launcher` |
| `catalog.hpp` | `launcher::judge_launch`: a kind and a name judged against the catalog; `launcher::listing`, what it can start now |
| `start.hpp` | `launcher::start_detached`; `launcher::runs_program`, whether a process still runs a program |
| `vocabulary.hpp` | `zengine.launcher`, `Launch`, `Launched`, `LaunchCatalogRequested`, `LaunchCatalog`, `LaunchEntry` |
| `configured.hpp.in` | the configured catalog, written to the build's `generated/launcher/configured.hpp` |
| `docs/zengine.launcher.md` | the manual, compiled in |

## Law

- [launcher](../agents/launcher.md) — what may be started, and how

## Suites

- `launcher` — the catalog's refusals, a demo's interpreter, a real detached start, the listing and
  whether an app runs, the weave's askers and each start said to any: `tests/test_launcher.cpp`

Its cases in shared suites: `view_builder` (`tests/test_view_builder.cpp`, Run app).

## Others' side

- The View Builder's Run app (`view/view-builder/pane.cpp`) and Files' start and demos
  (`files/files.cpp`) ask it, each after Workshop's approval for the hand; the palette
  (`workshop/palette/`) and the quick setup (`workshop/first-run/`) are admitted to ask it too.
- The app host writes the session file a listing reads (`app/session_file.hpp`).
- Workshop's four load plans load it as an optional row, and Workshop's directory stages it
  (`workshop/CMakeLists.txt`); installed with Zengine's artifacts (`cmake/ZengineInstall.cmake`).

## Pages

- [`zengine.launcher`](docs/zengine.launcher.md)
