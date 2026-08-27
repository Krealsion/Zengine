# Current limitations

**Reference.** What does not work yet, in one place, so you can plan around it instead of
discovering it. Everything here was read out of source, not inferred from architecture.

This page describes version **0.1.0**. It is a pre-release; interfaces change without
deprecation cycles.

## The library itself

### The installable package does not cover everything Zengine builds

`find_package(zengine)` exports eight capability targets and installs the loadable artifacts
they need — see [using Zengine from another
project](../getting-started.md#using-zengine-from-another-project). Four things are
deliberately outside it, and each is a real limit rather than an oversight:

- **The SDL-backed skin and input reader are not installed.** When no SDL3 is present the
  build fetches one and links it out of its own build tree, so installing those two artifacts
  without it would ship images that cannot load. A consumer who wants a window today builds
  Zengine from source.
- **Workshop is not installed.** Its executable compiles the path of the build tree that
  produced it into itself for the Builder panel, so an installed copy would carry a stranger's
  directory layout. Workshop is launched from a build tree.
- **The external-pane vocabularies are not exported**, for the reason below: there is no way
  for an externally-built pane to arrive in a Workshop run.
- **The Timer's own service headers are not installed** (`timer/normalize.hpp`,
  `timer/timer_weave.hpp`), so writing a *replacement* Timer service outside this repository
  is not a supported path. Using the Timer is; being one is not.

### A declared Timer binding's delay is fixed at declaration

`TimedWeave`'s declared bindings cover *authored rhythms*. There is no way to change a
binding's delay, and a binding created at run time waits for the next `TimerReady` rather than
reconciling immediately. A delay that is **data** — from a message, a person, a job — must use
the raw Timer protocol, which means writing the accept and emit sets, the id filter and the
receipt handling by hand.

This is the designed split ([TIMER-05](../laws/timer-laws.md)) and it is stated in the guides.
It is listed here because it is the first thing most Timer authors meet, and the convenient
layer does not cover it.

### A sender cannot observe its own send's fate

A send that goes nowhere — no holder for the role, a grant that does not permit it, a shape
this process has never declared — produces no error your handler can read. The refusal is real
and is visible to a **host-installed observer**. This is a recorded Loom seam, not a Zengine
choice; the practical consequence is that "it loads and nothing happens" is the failure a
newcomer hits first, and diagnosing it needs a tap. See
[getting started](../getting-started.md#when-it-does-not-work).

### Loading the Timer service makes the process permanently non-quiescent

The service seeds its one successor beat inside every beat's own handler, so from the moment it
is loaded the bus always has something queued — whether or not any schedule is outstanding. The
consequence for a host is that **"wait until the bus goes quiet" is never a termination
condition once the Timer is loaded**: `loom::Switchboard::drain_until_idle()` on such a process
does not return, exactly as its name and contract say. An ordinary host loop asks for
`pump_pending()` and gets control back every turn; snake and Workshop want the drain, because
the bus *is* their whole program and `stop()` is their exit.

## Workshop

### The document is not restored at launch

The desk is, and the window size is — both automatically, from the last session. The
**document** is not: every session still begins with `Ctrl`+`o`.

What makes that cost more than one keypress: a fresh Workshop seeds two example objects, so
forgetting `Ctrl`+`o` gives you a plausible document that is not yours rather than an obviously
empty one. Detail in [workspace continuity](setups.md#workspace-continuity).

### The window comes back the size you left it, not the place

The last session restores the **size** of the Workshop window, in canvas cells, to the nearest
whole cell. It does not restore where the window was on the screen, and it does not restore a
maximized window as maximized — it restores whatever size it had.

This is a seam rather than an omission. Workshop does not own its window: whichever Skin holds
`zengine.skin` does, across a C ABI, and the only thing that Skin publishes about it is how many
canvas cells it has (`surface::SurfaceExtent`). There is no message in either direction that
carries a screen position or a maximized state, so persisting them would mean a new
publisher-to-medium protocol rather than a new field in a file.

### The session is written on an orderly close, and only then

`q`, `Ctrl`+`c` and the window's close box all reach the same door and all write it. A Workshop
that is **killed** loses the session it was in; the previous session file is untouched and is
what the next launch reads. There is no autosave, no background writer and no crash recovery,
and none of those is claimed anywhere else either.

### Panes are 9 rows tall by default and a bigger terminal does not change that

Width scales with the surface; height does not. A larger pane is authored in management mode
(`w` → `s` → arrows) at **one cell per keypress**, with no coarse step, no drag-to-size and no
"fill the room". It persists correctly once authored. The mode is advertised by the band's
legend and the hotkey view; its sub-keys are not announced once you are in it. Detail in
[pane geometry](panes.md#pane-geometry).

Panes are also drawn **over** the material you are building. There is no docking, no tiling and
no reflow.

### Builder builds what an authored file says, and no more

What can be built is a **recipe catalog beside the binary**, and a maker edits it in a text
editor: there is no recipe editor in Workshop, no picker beyond stepping through the list with
`c`, and no way to add a recipe at run time. Two recipe kinds exist — an existing CMake target,
and one `.cpp` that Zengine wraps in a generated CMake project — and there is deliberately no
third: no arbitrary shell recipe, no multi-source recipe, no globbed source list, no dependency
solver.

A successful build **can** enter the running project, but only where the project already
authored participation for that artifact and this run left the row waiting. There is no hot
reload — an artifact that is already live is refused in words, and a rebuilt file has not
changed the image that is running — and no automatic build-on-missing: a maker presses a key.
Detail in [Builder](builder.md).

### There is no text editor

**Workshop cannot open, edit or save arbitrary project text.** Source-traced: the only file
I/O in the Workshop package is the document JSON, the setup JSON, the load-plan JSON and the
optional recorder dump. There is no file browser, no buffer, no editor pane, and no external
editor integration.

What text editing *does* exist is three single-line editors, each over the same component: an
inspector property draft, the setup-name line, and the terminal overlay's command line. Each
has a caret, character-safe edits and a horizontal window; none is a file.

| question | answer |
|---|---|
| Can Workshop edit project source today? | **no** |
| Can it save source? | **no** |
| Can it open arbitrary project text? | **no** |
| Is an external editor the only realistic path? | **yes** |

The maker loop is `edit → build → load → observe`. Workshop today has **observe** (the
introspection panes and the terminal overlay), a fixed-target **build**, and neither **edit**
nor **load**. Those two are the missing links, and *load* is the one that also blocks Builder
from being useful.

### Lifecycle

| operation | today |
|---|---|
| load weaves and mount providers at startup, from an authored plan | **yes** |
| overlay a provider contribution over an existing one, reversibly | **yes** — `"mode": "overlay"` in the plan |
| unmount a provider | **only** as a failed record's own rollback, and at teardown |
| roll back one artifact's partial record | **yes** — one artifact is the atomic unit |
| roll back a whole plan | **no** — earlier artifacts stay; you are told which artifact stopped it and what still stands |
| unload a weave at run time | **no** from Workshop |
| reload or replace a weave in place | **no** from Workshop |
| any run-time change to what is loaded | **no** |

**Loading is initial and restart intent.** The plan executor has no unload, reload or remount
path — that is written down in its own source, not inferred.

**What realizes a project is now a live thing rather than a call**, and that changes what is
*missing* rather than what is possible. Workshop begins the project and returns to its ordinary
loop; each row settles when its own load answer arrives, and the owner of that work is still
there afterwards, holding the authored plan, a cursor and what each row produced. Nothing new is
offered to a maker by that on its own — there is still no unload, no reload, and no run-time
change to what is loaded. What it removes is the reason those were impossible to *reach*: the
object that would have to perform them used to have returned before the host loop started.

**Reversible provider overlay is not artifact hot reload**, and the two must not be read as one.
Overlay and unmount are reversible *within the host's operator catalog*: unmounting an overlay
reveals what was underneath, unchanged and unrebuilt. That is a real, proven capability about
**contributions**. It says nothing about replacing a loaded **artifact** while the system runs.
The Loom's `Kernel::reload_from` exists and has open provider-custody caveats; nothing in
Workshop reaches it.

### No cross-pane interaction

You cannot drag a semantic object from one pane into another. There is no drag-and-drop between
panes, no shared selection across panes, and no protocol by which one pane could hand an object
to another. An external pane is read-only prose and receives no input at all.

The ownership map a future cross-pane gesture would have to cross is recorded in
[the architecture notes](../architecture/README.md#cross-pane-interaction).

### Panes have no installation story

A pane arrives because an artifact was in the load plan and the weave offered one. There is no
discovery, no plugin directory, no versioning of pane offers, and no way for a maker to install
somebody else's pane other than by editing a plan file and having the artifact on disk.

## Platforms

| | state |
|---|---|
| Linux / WSL, GCC 11+ | fully supported; the canonical lane |
| Linux, Clang | builds; not a routine lane |
| Windows, MSVC 19.50 (VS 2026) x64 | the portable subset builds and the suites run. clang-cl and ARM64 are unverified |
| Windows, MinGW-w64 | builds; runtime DLLs must be on `PATH` or beside the binaries |
| macOS | never built. Unclassified, not "unsupported" |
| the Loom's OS sandbox | **Linux only.** On Windows the kernel exists as an explicit development/demo backend with **no isolation**, and says so at every surface |

Detail in [supported toolchains](../contributing/supported-toolchains.md).

## What is deliberately not here

These are absences by decision, not gaps, and they are not going to arrive because a toolkit is
expected to have them:

- **No widget set.** The component package has one component, earned by working consumers
  (four of them, since TEXT-0) needing the same machinery. There is no Button, List, Dropdown,
  ScrollView, focus tree, tab order or theme.
- **No layout system.** The drawing vocabulary has no parent/child, anchors or percentages —
  whoever publishes has already decided where things go. The `ui` package resolves an authored
  extent against a frame and states no containment.
- **No shell.** The Builder's process runner takes a program and an argument vector.
  `popen()` would have been shorter and would also have been a general shell capability.
- **No dependency solver.** A load plan's order is authored policy; a person wrote the rows in
  the order they must happen.
- **Containment is not claimed.** An in-process weave shares the host's address space. A grant
  bounds what a weave may *say*, never what it may *touch*. `Kernel::containment_note()` says
  so at run time; read it literally.
