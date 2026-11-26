# The launcher

**Starts an app made in Workshop, or a demo, as a process of its own that goes on after Workshop
quits.** Only from a catalog fixed when Zengine was configured, and only when the View Builder,
Files, the palette or the quick setup ask, each after Workshop allowed the hand that pressed. Any
role may ask what it can start, and whether an app it started still runs.

No message names a program, a path or an argument: a start names a kind, `app` or `demo`, and a
name, and nothing a message says widens the catalog. In the package a stranger downloads, the
catalog's paths are the package's own, found above the running program, and its Python is one
found on the PATH at each listing and each start, never one a message names. With none on the PATH
the setups are still listed, a setup's start is refused saying what to install, and an app still
starts.

## Pages

- [`zengine.launcher`](docs/zengine.launcher.md): the manual -- the catalog, a detached start, and
  `Launch`.
- [The launcher's laws](docs/launcher-laws.md): who may ask for a start, and whose approval of the
  hand the launcher trusts.

## Parts

| file | what it holds |
|---|---|
| `weave.cpp` | `zengine-launcher`, role `zengine.launcher`: judges a start, starts it, answers; `kAskers`, the four roles that may ask for one |
| `catalog.hpp` | what may be started, judged before anything starts (`judge_launch`): an app in the project's `apps/`, a demo the service lists; `listing`, what can be started now; `find_python`, a package's Python |
| `start.hpp` | `start_detached`, a process started detached and kept by nobody, on Windows and on POSIX; `runs_program`, whether a process still runs a program |
| `vocabulary.hpp` | the role, the two kinds, `Launch` and `Launched`, and the listing's ask and answer, `LaunchCatalogRequested` and `LaunchCatalog` |
| `configured.hpp.in` | the demo service and its setups, written into the build tree's `generated/` folder as `launcher/configured.hpp` when it is configured |
| `CMakeLists.txt` | `zengine-launcher-vocabulary`, `zengine-launcher-catalog` (header-only, so the suite judges as the weave does) and `zengine-launcher`; the catalog fixed at configure |
| `docs/zengine.launcher.md` | the manual, compiled in |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`launcher/` adds to it.

**Law.** One numbered law, on [the launcher's laws](docs/launcher-laws.md): only its four askers
may ask it for a start, each after Workshop allowed the hand that pressed, and the launcher trusts
that approval (LAUNCHER-01). Its proofs are listed in `tests/laws/launcher.txt`. What it promises
the roles that ask it is its manual's, and a rule about one declaration is in that declaration's
comment.

**Suites.** Each runs through the official lane, `tests/verify.cmake`.

| suite | what it proves | where |
|---|---|---|
| `launcher` | the catalog's refusals, a demo's interpreter, a real detached start, the listing and whether an app runs, the weave's askers, and each start said to any role | `tests/test_launcher.cpp` |

The View Builder's suite, `view_builder` (`tests/test_view_builder.cpp`), holds the case for its
Run app.

**The host's side.**

- The View Builder's Run app (`view/view-builder/pane.cpp`), Files' start and its demos
  (`files/files.cpp`), the palette's setups and apps (`workshop/palette/palette.cpp`) and the quick
  setup's Start (`workshop/first-run/first_run.cpp`) ask it, each after Workshop's approval for the
  hand that pressed (LAUNCHER-01).
- The app host writes the session file a listing reads (`app/session_file.hpp`).
- Workshop's four load plans load it as an optional row, and Workshop's directory stages it
  (`workshop/CMakeLists.txt`); it is installed with Zengine's artifacts
  (`cmake/ZengineInstall.cmake`).
