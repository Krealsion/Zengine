# The launcher

Routed behind [AGENTS.md](../AGENTS.md) for `launcher/`. Also read [app host](app.md) (what an app
is and the program that runs it). Public contract: [the manual](../launcher/docs/zengine.launcher.md).

- What may be started is a kind and a name in a catalog fixed at configure (`launcher/catalog.hpp`,
  `launcher/configured.hpp`): an app, a folder directly in the project's `apps/` -- a link out of
  it refused -- holding a plan the load plan's codec reads, started by the `zengine-app` beside the
  running program on that folder; or a demo, a setup the demo service lists, started by the
  configured interpreter running the configured `demo.py` on `<project>/demo-runs/<name>`. No
  message names a program, a path or an argument, and nothing a message says widens the catalog.
- A start is detached and kept by nobody (`launcher/start.hpp`): on Windows a console and a process
  group of its own, no inherited handle, out of the job where the job allows; on POSIX a session of
  its own, its input empty, its output appended to a log, every other descriptor closed. The
  answer names the process's id: it started, not that it runs. What a started app does next on
  this platform is one sentence, `kStartedAppGoesOn` (`launcher/vocabulary.hpp`), which every
  start's notice says. The build runner's child, which dies with its holder, is not this.
- `zengine.launcher` takes `Launch` only from `zengine.view.builder` and `zengine.files`; each asks
  Workshop first for the hand that pressed, classes `build` and `write` together, so a guest starts
  nothing on a weaver's host. It asks `zengine.project` where the project is at its start, and
  refuses a start before the answer.
