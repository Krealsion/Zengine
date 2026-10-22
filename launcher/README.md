# The launcher

**Starts an app made in Workshop, or a demo, as a process of its own that goes on after Workshop
quits.** Only from a catalog fixed when Zengine was configured, and only when the View Builder or
Files ask, each after Workshop allowed the hand that pressed.

## Pages

- [`zengine.launcher`](docs/zengine.launcher.md): the manual -- the catalog, a detached start, and
  `Launch`.

## What is in this folder

| | |
|---|---|
| `weave.cpp` | `zengine-launcher`, office `zengine.launcher`: judges a start, starts it, answers |
| `catalog.hpp` | what may be started: an app in the project's `apps/`, a demo the service lists |
| `start.hpp` | a process started detached and kept by nobody, on Windows and on POSIX |
| `vocabulary.hpp` | the office, the two kinds, `Launch` and `Launched` |
| `configured.hpp.in` | the demo service and its setups, written into the build when it is configured |
| `CMakeLists.txt` | the catalog fixed at configure, and the weave |
| `docs/` | the manual |

Working on it: [AGENTS.md](AGENTS.md).
