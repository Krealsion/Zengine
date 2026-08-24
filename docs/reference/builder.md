# The Builder package

**Reference.** Starting a real operating-system process from a named recipe, and following it
without blocking the bus. For what Workshop's Builder pane can actually do today, see
[Workshop's Builder](../workshop/builder.md).

Source: [`builder/vocabulary.hpp`](../../builder/vocabulary.hpp) ·
[`builder/weave.hpp`](../../builder/weave.hpp) ·
[`builder/runner.hpp`](../../builder/runner.hpp) ·
[`builder/run.hpp`](../../builder/run.hpp).

The first Zengine package whose subject is an **effect** rather than a picture: building
something means starting an operating-system process, with a real exit status, and nothing
here had ever done that. So the package is arranged as a split, and the split is what it is
for.

- **The tool** (`weave.hpp`) is ordinary. It holds one target **name**, follows how its build
  is going, and publishes that as `BuildStatus` for any presentation to read. Two grant rules:
  order the runner, and say what it knows. It holds no command and cannot spell one, holds no
  timer, and never asks anything whether it is done yet.
- **The runner** (`runner.hpp`) holds the host's catalog of recipes — an absolute program, an
  absolute working directory, an argument vector — and is the only weave in the program that
  starts a process. Its reach is the four observations it may report to whoever holds the
  Builder office (`BuildStarted`, `BuildOutput`, `BuildFinished`, `BuildNotStarted`) plus two
  sentences to the Timer: ask for a beat, and give it back. A `RunBuild` naming something
  outside the catalog is refused by name, and nothing runs.
- **The wire cannot spell a command.** `BuildRequested` and `RunBuild` carry exactly one field
  and it is a target name; there is no shape here whose field is a program, an argument list,
  a directory or a shell line. "The panel sent a command" is not a sentence this vocabulary
  can express, which is a property of the types rather than of a check.
- **`run.hpp` is not a shell.** It takes a program and an argument vector and runs them —
  `fork`/`exec` with a pipe on POSIX, `CreateProcess` with a pipe on Windows. `popen()` would
  have been four lines and would also have been a general shell capability; a later phase that
  genuinely needs one can add it and argue for it, rather than finding it already here having
  arrived as a side effect of a button.
- **What the split buys is reviewability, not containment.** An in-process weave shares the
  host's address space, so a grant bounds what a weave may *say* and never what it may *touch*
  (Loom `docs/reference/capabilities.md`) — any code compiled into the binary could call the
  same platform functions. What this arrangement gives is one place where process authority
  lives, one grant to read, and one refusal to test. Calling it containment would be the
  overclaim these phases exist to refuse.
## The build outlives the turn that asked for it

The package's one process verb used to be *run, wait, result*, and it blocked: the runner
built inside its own handler, on the bus the Workshop was pumping, so the whole application
stopped until the child exited. That was replaced with **custody**, and nothing else. There is no
scheduler, no thread, no queue, no coroutine and no async runtime; every line of the Builder
still runs inside an ordinary handler, on the ordinary Loom execution thread, with an ordinary
`Mail`.

- **`start_recipe` hands back a `RunningRecipe`** (`run.hpp`) — move-only custody of one child
  process and the read end of its pipe, released exactly once by its destructor. It does not
  need the stack frame that made it, which is the whole property: a `RunningRecipe` in the
  runner's member vector is a build that outlives the handler that started it.
- **`look()` is bounded and never blocks.** It drains at most `kMaxLookBytes` from a
  non-blocking pipe (`O_NONBLOCK` / `PeekNamedPipe`) and reports what is newly true. A slow
  child does not make checking it slow; a chatty one does not make checking it long.
- **The ending is reported only after the output has ended.** `look()` will not say *ended*
  while the pipe is still open, even when the child has already exited — a build's last words
  are written before it exits and read after it, and an ending that raced the drain would
  throw away exactly the lines that say what went wrong.
- **The runner owns the unfinished work**, because it is the participant that possesses the
  process capability and therefore the only one that can honestly say it saw a child exit. Not
  Workshop, not the panel, not the Kernel, not an ambient registry.
- **It polls its own handles on an ordinary beat, and polling stops there.** The runner is a
  `TimedWeave`: it asks the Timer for a repeating beat when it takes custody of something and
  cancels the beat when it has nothing left to watch, so an idle Workshop carries no Builder
  traffic at all. What leaves the runner is only newly observed *facts* — nothing upstream ever
  asks *is it done yet?*, and there is no shape in this vocabulary with which it could.
- **Three moments, three shapes.** `BuildStarted` and `BuildFinished` are two facts because
  they happen at two times, and `BuildNotStarted` is a third because "there is no compiler" and
  "the compiler said no" are different problems. A single `BuildOutcome` shape was truthful
  only while both facts became true in the same instant, which stopped being so as soon as
  builds outlived the handler that started them.
- **An operation identity is a payload field, minted by the runner**, and it is meaningful
  *within one live runner incarnation*. That limit is written down rather than discovered:
  this runner is mounted natively, so `Kernel::query_role` answers `holder == 0` for its office
  and `unload_role` declines it — there is no path by which a successor could exist to inherit
  a number. The day it becomes a loadable weave is the day the counter needs a surviving
  high-water mark.
- **A holder that dies takes its work with it, and says nothing.** Destroying the runner
  terminates and reaps every held child: no orphan, no zombie, and no false completion. *The
  holder stopped holding* is not *the external operation stopped*, and a destructor has no
  `Mail` with which to author either.

No kernel and no loadable weave: both weaves are mounted in-process by whichever host wants
them, exactly as Workshop's own weave is. The suite is `tests/test_builder.cpp`, and it starts
real child processes — every recipe it runs is `cmake -E ...` or `cmake -P tests/slow_build.cmake`,
CMake's own portable shim and this repository's own deliberately slow script, so it needs no
shell and no assumption about what else is installed. The suite carries the regression canary
for all of the above: the blocking runner, rebuilt over the same platform code, measures
carrying **zero** unrelated deliveries between a build's start and its end — which is exactly
what custody exists to prevent.
