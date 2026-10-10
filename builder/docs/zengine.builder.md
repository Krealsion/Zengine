# zengine.builder -- the Builder

**Manual.** The Builder: the weave that builds, answering to the office `zengine.builder` (an office
is the name a weave answers to). It knows your project's recipes by name, asks its runner to run one,
follows the build without stopping the desk, and says how it went. Workshop hosts it; it is not a
file you load. Its pane is [`zengine.builder-pane`](zengine.builder-pane.md); another weave, or an
agent through the pane, reaches it by the messages below.

## About

The Builder turns a recipe's *name* into a finished build. Nothing that reaches it can spell a
command: a message names a recipe, and the [recipe file](recipes.md) you wrote says what that recipe
runs, always through CMake. A build *succeeded* when two things are true, checked in this order: the
build process exited zero, and the file the recipe names is there. A build that exits zero without
its file is `NO ARTIFACT`, said in those words. Asked to, the Builder offers a build that worked to
the running project, whose realization owner, `zengine.realization`, decides whether to load it; the
Builder shows that second answer beside the first, never merged into it.

The Builder is two weaves Workshop mounts: this office, which answers for recipes and outcomes, and
a runner, `zengine.build-runner`, the one weave in Workshop that starts a process. A build is an
ordinary child process of Workshop, exactly as privileged as Workshop, run with the CMake fixed when
Workshop was built. A build runs one at a time, and there is no cancel and no time limit: it runs
until it ends.

## Use

Ask it to build with [`BuildRequested`](#buildrequested), then follow its account:
[`BuildAsked`](#follow) says what became of your ask, and [`BuildStatus`](#follow) how the build goes.
To come back to a build later, ask [`BuildStatusRequested`](#buildstatusrequested); to read
everything a build said, ask [`BuildOutputRequested`](#buildoutputrequested) page by page.

Who may send: a weave the project loads (its plan admits every artifact it loads); the Builder pane is
one. An agent builds through the pane, under its guest row's `build` power, and a guest row that
observes `BuildStatus` may ask `BuildStatusRequested`; no guest is granted `BuildRequested` itself.
Workshop's Terminal is granted none of these shapes.

## Commands

### `BuildRequested`

`BuildRequested` v2 asks the Builder to build a recipe, and if you ask, to offer the result to the
running project.

- **Takes:** `recipe`, the name the recipe file gives it; `realize`, true to offer a build that worked
  to the running project.
- **Answers:** publishes `BuildAsked`, `taken` with the number the ask became, or a `refusal`; then
  `BuildStatus` as the build moves: `asked`, `running`, then `succeeded`, `FAILED`, `NO ARTIFACT` or
  `did not start`. A success's detail is `built tally`, or `already up to date: tally`. With
  `realize`, a success is offered to the project (`OfferArtifact`) and `realization` reads
  `offered`; the owner's answer then sets `realized` or `REFUSED`, with its words: the row the
  project waits on is loaded, a running one is reloaded in place, and an artifact its plan does not
  name is refused ([loading what you built](../../workshop/docs/load-plans.md#when-a-built-artifact-is-loaded-again)).
  A build that did not work offers nothing, and says so: `the build failed, so nothing was offered
  to the project`.
- **Refuses:** in `BuildAsked.refusal`: `this Builder holds no recipes at all`; `` this Builder holds
  no recipe called `oven` (it holds 3) ``, published with the outcome `unknown recipe` when no build
  runs; `a build was already asked for and has not started yet`; `a build is already running:
  operation #4`. A build that cannot start ends `did not start`, its reason in `BuildStatus.detail`.
- **Example:** from a weave the project loads, holding its office,
  `mail.as_role("example.tally").send_to_role("zengine.builder", zengine::builder::BuildRequested{"tally", true});`,
  as the pane's [`builder.build`](zengine.builder-pane.md#builderbuild) does.

### `StatusRequested`

`StatusRequested` v1 asks the Builder to say everything it shows, to everyone: a presentation that
just opened learns its rows this way.

- **Takes:** nothing.
- **Answers:** publishes `RecipeCatalog` (every recipe and its artifact, and the recipe file in
  force), then `BuildStatus`.
- **Refuses:** nothing.
- **Example:** the Builder pane sends it each time Workshop gives it room.

### `BuildStatusRequested`

`BuildStatusRequested` v1 asks the Builder where it stands, answered to the asker alone: the starting
point for an observer that comes back to a build.

- **Takes:** nothing.
- **Answers:** `BuildStatus`, to the asker alone; nothing is published and no count moves.
- **Refuses:** nothing.
- **Example:** subscribe to `BuildStatus` first, then ask; join the answer with every publication that
  arrived with it, since a build and its realization only move forward.

### `BuildOutputRequested`

`BuildOutputRequested` v1 asks for a page of what one build said, by its operation number.

- **Takes:** `op`, the build's operation; `from`, the first line wanted, counted from 1, or 0 for the
  page that ends at the last line; `lines`, how many the reader has room for (a page holds at most a
  few dozen).
- **Answers:** `BuildOutputSaid` v1, to the asker alone: whole lines as the build wrote them,
  numbered, with how the build ended, the lines not kept and the bytes cut from long lines counted,
  and which operations the Builder still keeps.
- **Refuses:** `kept` false, with only `op` and `ops` filled, for a build it never ran or no longer
  keeps; it never answers with another build's lines.
- **Example:** the pane's reader asks `{op: 3, from: 0, lines: 40}` for the last forty lines.

## Hears

### `BuildStarted`

`BuildStarted` v2, from the runner: a build of the recipe it follows has started, with its operation
number and the command it ran.

### `BuildOutput`

`BuildOutput` v3, from the runner: the next bytes the build wrote, in order and whole. The Builder
keeps the first and the last part of each of its last few builds' output, in memory only, and counts
the rest.

### `BuildFinished`

`BuildFinished` v2, from the runner: the build's process ended, with its exit status, after its
output ended.

### `BuildNotStarted`

`BuildNotStarted` v2, from the runner: no process ran, and why (no CMake, no build tree, no source
file).

### `ArtifactRealized`

`ArtifactRealized` v3, from the realization owner, `zengine.realization`: what the project made of an
offered build, or of a revert.

### `ArtifactPromoted`

`ArtifactPromoted` v2, from the realization owner: whether a promotion wrote the running image into
the file a restart loads.

## Follow

The Builder publishes its own account: `BuildAsked` v1 once for every `BuildRequested`, and
`BuildStatus` v4 on every change, both from `zengine.builder`, and a guest row may observe either. To
follow one press to its end: subscribe first; the `BuildAsked` your request caused is your ask; its
build has ended when `BuildStatus.outcome` is no longer `asked` or `running` for the `builds` your ask
became; its realization when `realization` is `realized` or `REFUSED`. `RecipeCatalog` v2 is
published on `StatusRequested`, and `OfferArtifact` v1 when a build asked to be realized succeeds.

## Files

- **The recipe file.** At launch Workshop reads `--recipes <path>` when given; otherwise
  `build-recipes.json` in the directory it was launched in, when there; otherwise the shipped
  `default-build-recipes.json` beside it. A recipe file that is there and refused stops Workshop with
  its reason. Files can choose another file while Workshop runs, and add a recipe to one. What a
  recipe holds is [the recipe file](recipes.md)'s.
- **A one-file build** writes a small CMake project into its workspace (empty: beside Workshop, under
  `build-workspace/<recipe>`) and its file into the workspace's `out/`. Nothing is deleted: a failed
  build's project stays, and the build's last line names it, `zengine build: compile or link FAILED
  (exit 1)`.
- **A CMake target build** runs `cmake --build <tree> --target <target>` in a tree someone already
  configured, and never configures it.
- **The journal.** Launched with `--log <path>`, Workshop keeps every build that finished or did not
  start, and what the project made of each offer, whole: the one place a long refusal is kept.

## See also

- [`zengine.builder-pane`](zengine.builder-pane.md), the pane.
- [The recipe file](recipes.md).
- [Build your first pane](tutorials/build-your-first-pane/README.md).
