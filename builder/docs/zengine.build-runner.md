# zengine.build-runner -- the build runner

**Manual.** The build runner: the weave that starts and holds a build's process, answering to the
role `zengine.build-runner` (a role is the name a weave answers to). It runs a recipe the
[Builder](zengine.builder.md) orders, follows the process without stopping the desk, and tells the
Builder what it sees. Workshop mounts it; it is not a file you load, and no pane speaks to it.

## About

The runner is the weave in Workshop that holds a build's command and starts its process; the
Neovim editor, when it holds the editor's role, starts `nvim` too
([Neovim](../../editor/docs/neovim.md)), and the launcher starts an app or a demo that outlives
Workshop ([`zengine.launcher`](../../launcher/docs/zengine.launcher.md)). The runner is `BuildRunnerWeave`, in
[`builder/runner.hpp`](../runner.hpp), mounted by Workshop's host itself. It reads two things the
host owns and never keeps a copy of either: the host's recipe catalog, the recipes the
[recipe file](recipes.md) gives, and the one CMake program Workshop was configured with
(`ZENGINE_BUILDER_CMAKE`; none in the package a stranger downloads, where every order is
refused in words and Workshop's banner says it builds nothing). Neither is in its state, so no message and no poke can name a program
for it to run: an order names a recipe, and the catalog says what that recipe runs.

Asked to run a recipe, the runner turns it into one CMake command (writing a one-file recipe's
small project first), starts the process and returns at once, holding the process and the answer
it owes under an operation number that only goes up while the runner lives. It tells the Builder the build started, then
looks at what it holds on its own beat, `zengine.builder.look`, every `kLookBeatMs`
([`builder/vocabulary.hpp`](../vocabulary.hpp)). It asks the Timer for that beat only while it holds
a process, and cancels it once the last one ends, so an idle Workshop carries no build traffic.

Each look is brief and never waits: whole lines of new output go to the Builder in order, in pieces
of at most `kMaxOutputChars` bytes, and a process that ended is answered once and let go. Four counters
are its state, which a poke reads (`RunnerState`): `ran`, the recipes it tried to start; `refused`,
the orders it turned down; `live`, the operations it holds now; and `looks`, its passes so far.
Destroyed, the runner ends and reaps every process it holds, and Loom tells each asker its end.

## Use

In Workshop the Builder, `zengine.builder`, is the one weave granted [`RunBuild`](#runbuild) to this
role, and the runner reports to it alone and answers it; nothing else may order a build. A suite, a diagnostic,
or a host with no Timer may send [`LookAtBuilds`](#lookatbuilds) to have it look now instead of
waiting for its beat. Any weave granted `ShapeDocumentRequested` to this role may ask what a shape
it accepts does, and is answered from this page.

## Commands

### `RunBuild`

`RunBuild` v3 asks the runner to carry out the recipe the catalog holds under a name, for one of
its products. It is an ask: the build's result is its answer.

- **Takes:** `recipe`, the name of a recipe in the host's catalog; `artifact`, the stem of the
  product to build, empty for the recipe's first.
- **Answers:** keeps the answer, starts the recipe's process, counts it in `ran` and `live`, sends
  `BuildStarted` `{op, recipe, command}` to `zengine.builder`, and asks for its beat if it is not
  already beating. When the process ends it answers `BuildFinished` `{op, recipe, status}`, or
  `BuildNotStarted` under its number when the process never became the program.
- **Refuses:** with `BuildNotStarted` as its answer, `op` 0, and nothing run: for a name the
  catalog does not hold, `` no recipe here is called `oven` ``, counted in `refused`; for a recipe
  that cannot become a command, such as a one-file recipe whose project cannot be written, a
  product it does not make, or a boxed product on a host that names no wasi-sdk, the reason it
  gives, counted in `refused`; with as many builds held as Loom lets one weave keep answers
  (`kMaxDeferredPerWeave`), `the build runner already owes <n> builds`, counted in `refused`; for a
  process that could not be started, the command it tried and the reason, counted in `ran`. An
  order with nobody to answer, a root's, runs nothing and is counted in `refused`.
- **Example:** from the Builder, `mail.ask_role(zengine::builder::kBuildRunnerRole, zengine::builder::RunBuild{"tally"}, ask);`

### `LookAtBuilds`

`LookAtBuilds` v1 asks the runner to look at everything it holds now, as its beat would.

- **Takes:** nothing.
- **Answers:** one look, counted in `looks`: each held build's new whole lines as `BuildOutput`, to
  `zengine.builder`, and for one that ended, its order's answer, as [`RunBuild`](#runbuild) says;
  that build is then let go, `live` falls, and once nothing is held the beat is cancelled.
- **Refuses:** nothing; it cannot start a build, only say now what the next beat would.
- **Example:** `mail.send_to_role(zengine::builder::kBuildRunnerRole, zengine::builder::LookAtBuilds{});`

## Hears

### `TimerFired`

`TimerFired` v1, from `zengine.timer`, the Timer: the runner's beat, `zengine.builder.look`. On each,
it looks at what it holds, as [`LookAtBuilds`](#lookatbuilds) does. Shape:
[`timer/vocabulary.hpp`](../../timer/vocabulary.hpp).

### `TimerReady`

`TimerReady` v1, from `zengine.timer`, the Timer: a Timer is there, so a runner holding a build asks for its beat
again. Shape: [`timer/vocabulary.hpp`](../../timer/vocabulary.hpp).

### `TimerResolution`

`TimerResolution` v1, from `zengine.timer`, the Timer: what it made of the runner's ask for its beat. Shape:
[`timer/vocabulary.hpp`](../../timer/vocabulary.hpp).

## Follow

The runner speaks only to `zengine.builder`, and only of what it saw: `BuildStarted` v2 when a
process began and `BuildOutput` v3 with each build's next bytes in order, and as an order's answer
`BuildFinished` v2 when a process ended or `BuildNotStarted` v2 when none ran. An operation's number
joins all four.
What a weaver follows is the Builder's own account, [its Follow](zengine.builder.md#follow).

## See also

- [`zengine.builder`](zengine.builder.md), the role that orders builds and keeps their account.
- [`zengine.builder-pane`](zengine.builder-pane.md), the pane.
- [The recipe file](recipes.md).
- [`builder/vocabulary.hpp`](../vocabulary.hpp), the shapes.
