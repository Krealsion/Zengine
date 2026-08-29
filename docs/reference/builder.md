# The Builder package

**Reference.** Starting a real operating-system process from a named recipe, and following it
without blocking the bus. For what Workshop's Builder pane can actually do today, see
[Workshop's Builder](../workshop/builder.md).

Source: [`builder/vocabulary.hpp`](../../builder/vocabulary.hpp) ·
[`builder/recipe.hpp`](../../builder/recipe.hpp) ·
[`builder/weave.hpp`](../../builder/weave.hpp) ·
[`builder/generate.hpp`](../../builder/generate.hpp) ·
[`builder/runner.hpp`](../../builder/runner.hpp) ·
[`builder/run.hpp`](../../builder/run.hpp).

The first Zengine package whose subject is an **effect** rather than a picture: building
something means starting an operating-system process, with a real exit status, and nothing
here had ever done that. So the package is arranged as a split, and the split is what it is
for.

- **The tool** (`weave.hpp`) is ordinary. It answers for recipe **names** and the **artifact**
  each produces, follows how the build of one is going, and publishes that as `BuildStatus` for any
  presentation to read. Three grant rules: order the runner, say what it knows, and say that an
  artifact somebody asked to have realized is now on disk. It holds no command and cannot spell
  one, holds no build tree, no source path, no package prefix and no timer, and never asks
  anything whether it is done yet.
- **The runner** (`runner.hpp`) reads the host's catalog of **authored recipes** and is the only
  weave in the program that turns one into a process. Its reach is the four observations it may
  report to whoever holds the Builder office (`BuildStarted`, `BuildOutput`, `BuildFinished`,
  `BuildNotStarted`) plus two sentences to the Timer: ask for a beat, and give it back. A
  `RunBuild` naming something outside the catalog is refused by name, and nothing runs.
- **Neither of them owns the catalog.** The host that composed the process holds one completed
  recipe catalog for as long as it runs -- and can replace what it holds, which is how Workshop
  lets a maker choose another catalog file without restarting -- and both weaves read it: the
  runner the whole recipe, the tool the reduced view. Neither keeps a copy that could go on
  answering for a catalog the host has moved past, which is what makes "the recipe this build
  runs is the recipe this Workshop means" a property of the arrangement rather than of two
  parties agreeing. A build already in flight is the one thing a replacement does not reach:
  the tool resolved the file that operation is about when the ask was accepted, and reports the
  ending against that.
- **The wire cannot spell a command.** `BuildRequested` and `RunBuild` carry a recipe *name*;
  there is no shape here whose field is a program, an argument list, a directory or a shell
  line — and no recipe *input* travels either, so the widest thing anything on this bus can say
  is "build the recipe called X". "The panel sent a command" is not a sentence this vocabulary
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
## A recipe is authored knowledge, and it can name no program

`recipe.hpp` is what a maker may write down about how one artifact is produced: an
**identity**, the **artifact stem** it is expected to produce, where that artifact lands, and
**one** of two mechanisms with the inputs that mechanism needs. A recipe with neither describes
nothing and a recipe with both describes two builds under one name; both are refused rather
than resolved by precedence, because a precedence rule is a thing a maker has to remember and a
refusal is a thing they are told.

- **`CMakeTargetRecipe`** — a configured build tree and a target in it. The action is
  `cmake --build <tree> --target <target>`, with `--config` when the recipe authors one.
- **`SingleSourceRecipe`** — one `.cpp`, some package prefixes, some exported target names to
  link, optionally a build tree to borrow a toolchain from, and where to generate.

**There is no `command` kind and there must not be one.** A recipe carrying a program and an
argument vector would make every recipe *file* an arbitrary-execution document — a different
authority from the one this package has always had, where the catalog of runnable things is
written by the party that composed the process. `generate.hpp::prepare` is the only thing that
turns a recipe into a `BuildCommand`, and it always puts the host's own CMake in `program`.

The **stem** is the load plan's stem, under the same five rules and checked in both places: a
rule enforced in one execution-authority document and trusted in the other is a rule with a
door in it. That shared spelling is also the entire cross-reference between the two files —
there is no `recipe:` field on a plan row and no role, mode or order in a recipe.

`recipe.hpp` returns refusals as `std::string` (empty means accepted) rather than reaching for
`workshop::Written`: `builder/` is *below* `workshop/` in this tree's dependency order, and
borrowing that type to save one bool would invert the edge. The one place the two meet is
`workshop/recipe_persist.hpp`, which wraps these sentences in `Written` at the file boundary.

## Zengine writes a CMake project; it does not drive a compiler

`generate.hpp` turns a single-source recipe into two small files in a durable workspace — a
`CMakeLists.txt` and a `zengine-build.cmake` driver — and then into **one** `cmake -P` process.

```text
one .cpp -> a generated project -> find_package(zengine CONFIG) -> cmake configure + build
```

- **The whole build is one process.** Configure and build are two CMake invocations; the shapes
  available for that were a two-step sequence in the runner (which turns the one process
  custodian into a small workflow engine) and a two-step state machine in the tool (which gives
  a semantic owner a cursor over somebody else's procedure). Both were refused. The driver is a
  **generated CMake script** run as `cmake -P` — the precedent is `tests/slow_build.cmake` and
  `tests/package/run.cmake`, and the reason is theirs: a `-P` script needs no shell, no
  `/bin/sh`, no `.bat` and no assumption about what else is installed, on either platform. What
  it buys is that one operation, one identity and one ending describe a whole build.
- **It is not an arbitrary script and cannot become one.** No maker authors a line of it: every
  line is written from typed fields that have already been through `check_recipe`, and the only
  maker-supplied material in it is quoted strings that cannot contain a quote, a newline or a
  NUL. `$` and `\` are escaped on the way in, which is what keeps a Windows path a path.
- **The two failures are told apart by the thing that can tell them apart.** The driver is the
  only party that sees both exit codes, so it says *CMake configure FAILED* or *compile or link
  FAILED*, in the output a maker reads, and names the generated project in both — a build that
  failed leaves its project on disk and nothing deletes it.
- **The toolchain is borrowed, never guessed.** `load_cache()` reads the generator, its
  platform, its toolset, its make program, its C++ compiler and its build type out of a
  configured build tree the recipe names. No cache parser is written in C++ and the policy is
  legible in a file. An empty `toolchain_from` means "let CMake choose for this machine", which
  is said to be a default rather than a decision. On a Ninja + `cl.exe` configuration the
  Visual Studio environment comes from the parent process, inherited: Zengine does not set it,
  invent it or look for it.
- **CMake owns where the artifact lands**, including the import library a Windows link
  produces and the per-configuration variants a multi-config generator would append a directory
  to. Nothing in Zengine spells `.so`, `.dll` or a Debug postfix.

## A process exiting zero is not an artifact

The runner owns process custody and **only** process custody: it can honestly say a child
exited and with what status. Whether the file a recipe named is on disk is a different question
about a different subject, and the tool — which holds recipe-to-artifact knowledge — is the one
that looks.

```text
BuildFinished, status 0, artifact present   -> succeeded
BuildFinished, status 0, artifact ABSENT    -> kNoArtifact, and it says so
BuildFinished, status non-zero              -> FAILED, and the artifact is never consulted
```

The order closes the stale-artifact trap without a timestamp heuristic: a failed build leaves
whatever was at the destination — often a perfectly good artifact from an earlier build — and
the exit status is checked first, so a previous success can never satisfy the current
operation. The stamp the tool takes when a build *starts* is therefore not a correctness
mechanism; it is how a maker is told whether their build actually relinked anything.

There is **no scanning**: no directory enumeration, no newest-file rule, no "there is one DLL
so it must be mine".

## The build-to-realization seam

Two shapes, and one of them the Builder does not say.

- **`OfferArtifact`** — said by the tool, and **only when the maker asked for realization**. It
  is a **command**, not an observation, and the name says so: a plain build produces a file too
  and says so in `BuildStatus`, so *"the artifact is there"* is equally true on a path where
  nothing at all is published. What this shape carries is a maker's *intent* that the project
  take the result, justified by the facts about the build that ride along with it. A fact whose
  truth depends on whether somebody wanted to act on it is not a fact.
- **`ArtifactRealized`** — said by the participant that speaks for the realization owner
  ([the load plan's](load-plan.md) `PlanBooter`), heard by the tool, and folded into the picture
  it publishes so one presentation can show a maker both halves of what they asked for.

**It is an offer and not an order**, which is why it is not called `RealizeArtifact`. The
Builder cannot load anything and holds no realization authority: what an offer is worth is
decided by `PlanExecutor::realize`, whose eligibility rules are all about the **authored plan**
— the owner must be between rows, the artifact must not already be resolved, the plan must name
it, and it must be the row realization is currently waiting on. A later authored row may be
**built** now; it participates when the rows in front of it have. The path the message carries
is not used: the owner resolves a stem with the host's own rule, so a message naming a path
cannot redirect a load. The dangerous grant in a Zengine host is still exactly one, and it is
still the plan booter's.

An already-loaded artifact is **refused in words**. BLD-1 does not unload, reload, replace or
migrate anything, and a second load of a live artifact would be pretending otherwise.

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
them, exactly as Workshop's own weave is.

## What is not here

- **No arbitrary shell recipe**, in any file, message or field.
- **No multi-source recipe**, no globbed source list, no dependency graph, no solver, no
  parallel artifact scheduler and no generic task runtime.
- **No hot reload**: no unload, no replacement, no state migration, no rollback of a live
  artifact.
- **No automatic build-on-missing.** Every build begins with a maker saying `BuildRequested`.
- **No cancel and no timeout.** A running operation is simply running; how long is too long is
  an observer's judgement about itself, and no observer here makes one.
- **No containment.** A build is an ordinary child process, exactly as privileged as the host
  that started it.

## How it is measured

The suite is `tests/test_builder.cpp`. It drives a **real configured CMake tree**
(`tests/buildfixture/`, `LANGUAGES NONE`, configured once at build time) through the ordinary
production path — recipe → `prepare()` → `cmake --build --target` → a child process — because a
recipe cannot name a command and neither can a case. It also reads both generated files **as
text**, which is the only way a claim about what a single-source build does not reach into can
be checked on every lane rather than on the one that happens to have a compiler configured.

The suite carries the regression canary for custody: the blocking runner, rebuilt over the same
platform code, measures carrying **zero** unrelated deliveries between a build's start and its
end. And a source tripwire reads `builder/weave.hpp` and `builder/runner.hpp` with the prose
stripped, refusing nine scheduler verbs and eight async nouns in each — because a property that
is true because of what code does *not* contain cannot be proved by running the code.

The **real compile** is a lane and not a ctest entry: `tests/build/run.cmake` installs Zengine
into a scratch prefix, writes a one-file weave outside both repositories (under a directory
with a space in its name), builds it through the real Builder in a real running host, realizes
it, and then breaks the prefix two ways to prove the canaries can fire.

```bash
cmake -DZEN_BUILD_DIR=build -DZEN_WORK=/tmp/zengine-build-witness -P tests/build/run.cmake
```
