# The app host

Routed behind [AGENTS.md](../AGENTS.md) for `app/`. Also read [realization](realization.md) (the
plan's executor), [view](view.md) and [flow host](flow-host.md) (the two hosts it composes) and
[graphics](graphics.md) (the window and its arranger). Public contract:
[running an app on its own](../app/docs/app.md).

- `zengine-app` holds what a weave cannot and nothing else: the bus, one operator catalog, the
  Kernel and its control door, the plan's booter and executor, the Flow host and the view host,
  the place door (`zengine.app.host`), the close, and the console. The window, the directory, the
  Timer and the app's own weave are rows of its plan. It never holds `zengine.workshop`.
- The decisions are Loom's authority store, `app-authority.json` in the app's folder, its lock held
  for the run, so one host runs an app at a time; there is no decision store beside it, and nothing
  is admitted by default. The console speaks Loom's `authority` command over it.
- The Kernel asks the store at `Open`, and the plan's executor asks the same policy before each
  row, a provider's row included: a row with no standing decision, or whose file is not the build
  decided, waits (`PlanExecutor`'s barrier), and nothing of it is opened or mounted. After each
  console line the waiting row is asked again and realized on a yes.
- A plan row, loaded from the file the plan names, speaks with the plan's posture, as Workshop's
  rows do, and says so at the start; anything else loaded gets the store's baseline and speaks only
  what the console allows. Loom's warden adopts each load inside the control door, so `authority
  allow`, `show` and `revoke` reach a running row.
- The app's window is the first window that offers itself. Its `SurfaceCloseRequested`, said as its
  office, ends the run with exit 0; a close any other office says, or one said with no office,
  ends nothing. `quit` at the console ends it too.
- `zengine-app-weave` reads `app.json` from the folder the place door says, runs each flow then
  each view as its office, and on the directory's `GraphicsReady` and the window's `WindowOffered`
  offers the skin by the host's artifact directory, places each view's area hearing the window's
  keys, and chooses the skin. A file `app.json` names is a plain name in the app's folder.
- An app's folder is written by `app/folder.hpp` alone (the View Builder's Export app calls it):
  everything judged first -- the view and the flow read back through their own gates, `app.json`
  through `app/files.hpp`, the name a plain folder name, the program present -- then the plan
  through the load plan's codec, `app.json`, the two files and the two launchers, each written
  whole. It writes no decision and starts nothing: `app-authority.json`, `app-session.json` and
  every other file in the folder are never touched, and no artifact is copied there.
- `zengine-app` stands beside Workshop's program: installed, both in the package's `bin`; in a
  build, Workshop's directory stages it and `zengine-app-weave` (`workshop/CMakeLists.txt`). What
  Workshop writes or starts for an app names the program beside the one running
  (`app/program.hpp`).
