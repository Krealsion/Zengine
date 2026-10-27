# The launcher

Routed behind [AGENTS.md](../AGENTS.md) for `launcher/`. Also read [app host](app.md) (what an app
is and the program that runs it). Public contract: [the manual](../launcher/docs/zengine.launcher.md).

- What may be started is a kind and a name in a catalog fixed at configure (`launcher/catalog.hpp`,
  `launcher/configured.hpp`): an app, a folder directly in the project's `apps/` -- a link out of
  it refused -- holding a plan the load plan's codec reads, started by the `zengine-app` beside the
  running program on that folder; or a demo, a setup the demo service lists, started by the
  configured interpreter running the configured `demo.py` on `<project>/demo-runs/<name>`. No
  message names a program, a path or an argument, and nothing a message says widens the catalog.
- In the package a stranger downloads (`ZENGINE_PACKAGE`) the catalog's paths are the package's
  own, relative to it and found above the running program (`configured::package_root`): its
  `demo.py` beside the tool packages and the setups there that build nothing against a development
  runtime. The interpreter is the one place not fixed at configure: a Python found on the PATH at
  each listing and start (`find_python`), never one a message names; with none, the setups are
  listed, the listing's note and a start's refusal say `kPythonNeeded`, and an app still starts.
- A start is detached and kept by nobody (`launcher/start.hpp`): on Windows a console and a process
  group of its own, no inherited handle, out of the job where the job allows; on POSIX a session of
  its own, its input empty, its output appended to a log, every other descriptor closed. The
  answer names the process's id: it started, not that it runs. What a started app does next on
  this platform is one sentence, `kStartedAppGoesOn` (`launcher/vocabulary.hpp`), which every
  start's notice says. The build runner's child, which dies with its holder, is not this.
- `zengine.launcher` takes `Launch` only from `zengine.view.builder`, `zengine.files`,
  `zengine.palette` and `zengine.first-run` (`kAskers`); each asks Workshop first for the hand that
  pressed, classes `build` and `write` together, so a guest starts nothing on a weaver's host. It
  asks `zengine.project` where the project is at its start, and refuses a start before the answer.
- Each start it answered `ok` is published to any, as `zengine.launcher`, beside its answer; a
  refusal is answered alone. Nothing else of a start is kept or said.
- A listing (`LaunchCatalogRequested`, from any office) is `launcher::listing`: the configured
  setups with the title and line each one's `setup.json` gives at configure, then each folder in
  `apps/` that `judge_launch` would start, at most `kMaxListed`, the rest counted. An app runs when
  its session file names a process the system says still runs the catalog's app host
  (`app_runs`, `runs_program`): asked at each listing, never remembered.
