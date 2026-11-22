# Workshop from another host

**Lets a program on another Loom host (an external Loom host, or ELH), such as an agent's own
session, connect to a Workshop you are running: press its keys, take pictures of what it shows,
read its desk as text and act through its panes, as a guest you named and only with the powers
you gave it.**

The two run on one machine, joined by a local socket, and Workshop shows who is connected in
its Connections pane. You name each guest, and the powers it has, in a guests file Workshop reads
when it starts with `--guests`; a guest can do nothing its powers do not grant. The same tools
prepare [ready-to-use setups](../workshop/docs/demo-setups.md): a dedicated Workshop laid out for
one task, with a Reset demo button.

## Pages

- [Drive Workshop from another host](docs/external-host.md): say who may connect, link a host,
  and run the tool packages against Workshop
- [Author and check a recipe through an ELH](docs/elh-recipe-journey.md): write a new build
  recipe through Workshop's Files pane from another host's session, then check it

## Manuals

- [`zengine.guests`](../workshop/docs/zengine.guests.md), the guest door, beside it in Workshop's
  folder: who is connected, a guest's own row, and where Workshop's plan is
- [`zengine.connections`](docs/zengine.connections.md), the Connections pane
- [`zengine.demo`](docs/zengine.demo.md), a setup's Demo pane and the role its demo service
  attaches to

## Parts

| path | what it holds |
|---|---|
| `vocabulary_weave.hpp`, `vocabulary_weave.cpp` | `zengine-guest-vocabulary`, a loadable weave with no role: the message shapes the other host loads so it can speak to Workshop; a `loom-boot.json` row boots it |
| `connections-pane/` | `zengine-connections-pane`, a loadable weave: the Connections pane, the other hosts connected to this Workshop; its `vocabulary.hpp` holds the role `zengine.connections`, the pane key, the stem and the state |
| `demo-control/` | `zengine-demo-control`, a loadable weave: a setup's Demo pane, with its Reset demo button, role `zengine.demo` (`control.hpp`); its `vocabulary.hpp` holds the `Demo*` shapes |
| `demo.py` | starts, describes, walks, resets, stops and exports a ready-to-use setup |
| `tools/workshop/` | the `workshop` tool package, which reaches Workshop by message alone: `loom-tool.json` and its helper modules |
| `tools/workshop/setups/` | the ready-to-use setups, a folder each: `setup.json`, `desk.json` and a `README.md` |
| `tools/workshop/toolboxes/` | the inspection workbench's packaged toolboxes, which the `workbench` setup names (`workshop-machine/workbench.py`) |
| `tools/workshop-machine/` | the `workshop-machine` tool package, which uses `workshop` and also reaches this machine: a build tree, project files, a setup's files |
| `tools/accounts-machine/` | the `accounts-machine` tool package: `repo`, a checkout's Git and its pull requests, which reach the session user's accounts |
| `tools/desk/` | the `desk` tool package, by message alone: `desk/guide`, the guide marked, and `desk/resolve`, the controls that may cause a message |
| `tools/desk-machine/` | the `desk-machine` tool package, which uses `desk`: `read` and `watch`, Workshop's desk written out as text files |
| `docs/` | these pages and the two manuals carried here |
| `CMakeLists.txt` | `zengine-guest-vocabulary`, built without the sanitizer; each pane weave has its own `CMakeLists.txt` in its folder |

Where an installed Zengine carries loadable weaves, `zengine-guest-vocabulary` is among them, and
the five tool packages are under `share/zengine/loom-tools/`.

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`external-host/` adds to it.

**Law.** The folder has no law of its own. Workshop's side of a guest is in Workshop's registers,
[a guest's hand](../agents/workshop/guests.md) and [the desk said whole](../agents/workshop/desk-read.md),
which [Workshop's router](../workshop/AGENTS.md) routes; both panes keep
[the pane seam's law](../agents/panes.md), which the guide routes. The guests file and the tools
are public in [Drive Workshop from another host](docs/external-host.md).

**The manuals are compiled in.** `zengine-connections-pane` carries `docs/zengine.connections.md`
and `zengine-demo-control` carries `docs/zengine.demo.md`, and each answers its own when asked, so
an edit to either is a build: their manual witnesses stand among their cases, below, and the
`manual` check holds their form.

**Suites.** Each runs through the official lane, `tests/verify.cmake`. `demo_recipes`,
`tool_reach`, `tutorial_walks` and `workshop_journey` need the `session` gate, and `tutor_walk` the
`session-pty` gate.

| suite | what it proves | where |
|---|---|---|
| `demo_recipes` | setups and their preparation, with no window | `tests/session/demo_recipes.py` |
| `tool_reach` | a package's name says how far its tools reach: a tool of a package not named `-machine` reaches no process, file or socket by its imports, and only `accounts-machine` runs GitHub's client or Git's push | `tests/session/tool_reach.py` |
| `tutorial_walks` | every tutorial's `tutorial` walk replayed from an installed prefix | `tests/session/tutorial_walks.py` |
| `guest_journey` | a real Workshop admitting this program as a guest | `tests/guest_journey.cpp` |
| `guest_vocabulary` | the weave's shapes resolve by identity | `tests/test_guest_vocabulary.cpp` |
| `workshop_journey` | a Loom session runs the tools; the journey also runs the `workshop_*_checks.py` and `desk_*_checks.py` beside it | `tests/session/workshop_journey.py` |
| `tutor_walk` | a Tutor resolves and marks while the weaver presses, in a terminal Workshop whose weaver is a pseudo-terminal | `tests/session/tutor_walk.py` |

Its parts' cases in shared suites: `workshop_panes_inspect` (`tests/test_workshop_panes_attention.cpp`,
the Connections pane and its manual), `workshop_panes_seam` (`tests/test_workshop_demo.cpp`, the demo
controls, their census, `tests/actions/zengine.demo.txt`, and their manual) and `graphics`
(`tests/test_graphics.cpp`, the Connections pane drawn in a window); the Connections pane's stem and
source in `workshop_load` and `workshop_files`. The guest door, its manual among its cases, is
`workshop_guests` (`tests/test_workshop_guests.cpp`).

**The host's side.**

- Staging and loading: `workshop/CMakeLists.txt` stages the weaves beside the host and lists the
  Connections pane as a development pane; its plan row is in `workshop/default-load-plan.json`, and
  `demo.py` adds `zengine-demo-control` to a setup's plan.
- The guest door, mounted by `workshop/workshop.cpp` under `--guests`: `workshop/guest_door.hpp`;
  the guests file and its powers, `workshop/guests.hpp` and `workshop/guests.cpp` (`demo` grants
  the `Demo*` shapes to `zengine.demo`); a guest's hand, `workshop/actor_scope.hpp`. Its manual,
  `zengine.guests`, is carried by `zengine-workshop-guests`.
- The seam's vocabulary: `workshop/guest_seam_vocabulary.hpp` (`GuestConnections`, asked of
  `zengine.guests`); the doors a setup's preparation asks: `workshop/setup_control.hpp`.
- Installed by `cmake/ZengineInstall.cmake`: the weave, the five tool packages under
  `share/zengine/loom-tools`, and each feature's tutorials, exported into `workshop/setups/`. The
  public page of the setups is `workshop/docs/demo-setups.md`.

**Practice.** [Connections](../docs/contributing/best-practices.md#connections-external-hostconnections-pane)
and [the external host](../docs/contributing/best-practices.md#the-external-host-external-host-external-hostdemo-control).
