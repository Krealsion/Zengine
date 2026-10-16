# zengine.build-runner -- the build runner

**Manual.** The build runner: the weave that starts and holds a build's process, answering to the
office `zengine.build-runner` (an office is the name a weave answers to). It runs a recipe the
[Builder](zengine.builder.md) orders, follows the process without stopping the desk, and tells the
Builder what it sees. Workshop mounts it; it is not a file you load, and no pane speaks to it.

## About

The runner is the one weave in Workshop that holds a command and starts a child process:
`BuildRunnerWeave`, in [`builder/runner.hpp`](../runner.hpp), mounted by Workshop's host itself. It
reads two things the host owns and never keeps a copy of either: the host's recipe catalog, the
recipes the [recipe file](recipes.md) gives, and the one CMake program Workshop was configured with
(`ZENGINE_BUILDER_CMAKE`). Neither is in its state, so no message and no poke can name a program
for it to run: an order names a recipe, and the catalog says what that recipe runs.

Ordered to run a recipe, the runner turns it into one CMake command (writing a one-file recipe's
small project first), starts the process and returns at once, holding the process under an
operation number that only goes up while the runner lives. It tells the Builder the build started,
then looks at what it holds on its own beat, `zengine.builder.look`, every `kLookBeatMs`
([`builder/vocabulary.hpp`](../vocabulary.hpp)). It asks the Timer for that beat only while it holds
a process, and cancels it once the last one ends, so an idle Workshop carries no build traffic.

Each look is brief and never waits: whole lines of new output go to the Builder in order, in pieces
of at most `kMaxOutputChars` bytes, and a process that ended is said once and let go. Four counters
are its state, which a poke reads (`RunnerState`): `ran`, the recipes it tried to start; `refused`,
the orders it turned down; `live`, the operations it holds now; and `looks`, its passes so far.
When the runner is destroyed it ends and reaps every process it holds, and says nothing.

## Use

In Workshop the Builder, `zengine.builder`, is the one weave granted [`RunBuild`](#runbuild) to this
office, and the runner reports to it alone; nothing else may order a build. A suite, a diagnostic,
or a host with no Timer may send [`LookAtBuilds`](#lookatbuilds) to have it look now instead of
waiting for its beat. Any weave granted `ShapeDocumentRequested` to this office may ask what a shape
it accepts does, and is answered from this page.

## Commands

### `RunBuild`

`RunBuild` v2 orders the runner to carry out the recipe the catalog holds under a name.

- **Takes:** `recipe`, the name of a recipe in the host's catalog.
- **Answers:** starts the recipe's process, counts it in `ran` and `live`, sends `BuildStarted`
  `{op, recipe, command}` to `zengine.builder`, and asks for its beat if it is not already beating.
- **Refuses:** with `BuildNotStarted` to `zengine.builder`, `op` 0, and nothing run: for a name the
  catalog does not hold, `` no recipe here is called `oven` ``, counted in `refused`; for a recipe
  that cannot become a command, such as a one-file recipe whose project cannot be written, the
  reason it gives, counted in `refused`; for a process that could not be started, the command it
  tried and the reason, counted in `ran`.
- **Example:** from the Builder, `mail.send_to_role(zengine::builder::kBuildRunnerRole, zengine::builder::RunBuild{"tally"});`

### `LookAtBuilds`

`LookAtBuilds` v1 asks the runner to look at everything it holds now, as its beat would.

- **Takes:** nothing.
- **Answers:** one look, counted in `looks`: each held build's new whole lines as `BuildOutput`, and
  for one that ended, `BuildFinished` `{op, recipe, status}`, or `BuildNotStarted` under its number
  when the process never became the program; that build is then let go, `live` falls, and once
  nothing is held the beat is cancelled. All of it goes to `zengine.builder`.
- **Refuses:** nothing; it cannot start a build, only say now what the next beat would.
- **Example:** `mail.send_to_role(zengine::builder::kBuildRunnerRole, zengine::builder::LookAtBuilds{});`

## Hears

### `TimerFired`

`TimerFired` v1, from the Timer, `zengine.timer`: the runner's beat, `zengine.builder.look`. On each,
it looks at what it holds, as [`LookAtBuilds`](#lookatbuilds) does. Shape:
[`timer/vocabulary.hpp`](../../timer/vocabulary.hpp).

### `TimerReady`

`TimerReady` v1, from the Timer: a Timer is there, so a runner holding a build asks for its beat
again. Shape: [`timer/vocabulary.hpp`](../../timer/vocabulary.hpp).

### `TimerResolution`

`TimerResolution` v1, from the Timer: what it made of the runner's ask for its beat. Shape:
[`timer/vocabulary.hpp`](../../timer/vocabulary.hpp).

## Follow

The runner speaks only to `zengine.builder`, and only of what it saw: `BuildStarted` v2 when a
process began, `BuildOutput` v3 with each build's next bytes in order, `BuildFinished` v2 when a
process ended, and `BuildNotStarted` v2 when none ran. An operation's number joins all four.
What a weaver follows is the Builder's own account, [its Follow](zengine.builder.md#follow).

## See also

- [`zengine.builder`](zengine.builder.md), the office that orders builds and keeps their account.
- [`zengine.builder-pane`](zengine.builder-pane.md), the pane.
- [The recipe file](recipes.md).
- [`builder/vocabulary.hpp`](../vocabulary.hpp), the shapes.
