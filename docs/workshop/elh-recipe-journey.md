# Author and check a recipe through an ELH

Use this after [starting an admitted Workshop session](external-host.md#3-from-a-loom-session-journeys-as-python-tools).
It composes the shipped `workshop/inspect-capture` and `workshop/verify-recipe` tools. The
result is a new recipe written through Files, with captures and semantic checks retained as
named ELH runs. No direct recipe write substitutes for the authoring action.

## Prepare a disposable project

You need CMake, Ninja and a C++ compiler on PATH. From the Zengine checkout, choose a new
absolute directory for `<project>` and configure the small shipped fixture:

```sh
cmake -S examples/workshop-recipe-fixture -B <project>/build -G "Ninja Multi-Config"
```

The source fixture declares `demo_target`. This step configures a real multi-config tree;
building the library is not required for this authoring check. `build/CMakeCache.txt` must
contain `CMAKE_CONFIGURATION_TYPES`, including `Debug` and `Release`.

Launch your task-owned Workshop with `<project>` as its working directory, the appropriate
load plan and its own guests file. Use `--isolated`. Keep the staged runtime separate from the
build tree you edit, as [the setup guide explains](external-host.md#3-from-a-loom-session-journeys-as-python-tools).
Use an explicit `--recipes <project>/build-recipes.json` with this empty initial catalog:

```json
{"zen":1,"schema":"WorkshopRecipeFile","version":2,"fields":{"format":"zengine-build-recipes","format_version":"2","recipes":[]}}
```

This is fixture preparation before the test, not the resulting authored recipe. Save its bytes
or hash so an unsuccessful write can be checked against it. Start the ELH against this
Workshop and require the named `workshop/connections` check to pass before input.

## Reach the form and establish its state

Every example below needs its own unused run name in this host lifetime. Keep the host running
between commands. `--wait 30` is the client's wait budget, not a promise that pending means failure.
Read `loom-session show <session> <name>` after a wait expires before deciding what happened.

```sh
loom-session run <session> workshop/inspect-capture --name open-manager --input chord=ctrl+p --wait 30
```

Read that run's `after.bmp` (graphical) or `after.cells.txt` (TUI). Select Files in Pane Manager
with separate arrow runs and Enter, then focus the Files pane. Derive clicks from the current
picture: pixels use `x,y`, cells use `x,yc`. Pane positions and menu coordinates are not universal.
If Pane Manager reports no room, enlarge the task window and retry opening Files. Input
settlement does not turn that application refusal into success. An open Files pane should show
the fixture directory before you continue.

Files must be browsing `<project>`, the **parent** of `build`. Use its parent-directory action
if needed. Press `a`, select `build/ (configured tree)` in the chooser, then Enter, each as a
settled run. **`u` adopts an existing recipe catalog; it does not open authoring.** With the
fixture above there is one configured-tree candidate. Confirm the five-field form before typing.

The fields, indexed from zero, are recipe name, CMake target, artifact stem, optional artifact
directory and required configuration. Arrows commit the active line and move fields. Return
on the last field writes; use an arrow when you mean to leave that field without writing.

## Traverse the menu and replace a field

Give field 0 the name `elh-demo`:

```sh
loom-session run <session> workshop/inspect-capture --name recipe-name --input clear=true --input text=elh-demo --input chord=down --wait 30
```

Read the picture and click Files' `[menu]` control alone:

```sh
loom-session run <session> workshop/inspect-capture --name field-menu --input click=<x,y> --input chord= --wait 30
```

Wait for that run to finish and confirm the menu is visible. In separate runs, use Down/Up to
select `type the configuration`, then Enter. Confirm configuration is now active. A key appended
to the opening click can supersede its gesture; F12 is not a neutral way to force a repaint.

From configuration (field 4), two Up presses reach artifact stem (field 2). Type a long value
with `clear=true`, `text=very_long_artifact_stem_before_replacement`, `chord=down`. Go Up to
return to that populated field, then Left with `repeat=19` to place the caret inside it.
Use `changed=false` for caret-only motion if necessary; a changing image is not the field oracle.

Replace it with `clear=true`, `text=demo_target`, `chord=down`. Go Up again and read the
artifact-stem field: it must contain exactly `demo_target`, without fragments of the old value.
One further Up reaches CMake target; replace that with `demo_target`, committing with Down.
Keep artifact directory blank. Use separate navigation runs to reach configuration again.

The helper's clear operation selects all, erases, optionally types text, then sends the chord.
Without `clear`, its order is chord first, then optional text. Establish the active field
before using either route; input acceptance alone does not prove which pane consumed it.

## Refuse an empty required field, then write successfully

On configuration, set `Debug` with `clear=true`, `text=Debug`, `chord=up`; go Down again and
confirm it is populated. Empty it with `clear=true`, `text=` and `chord=up`; go Down again
and confirm it is empty. Press Enter. Files must name the missing configuration and say that
nothing was written. Compare the catalog bytes/hash with the initial fixture: unchanged.

Now fill configuration with `Debug` and commit with Enter. Check the authored notice. Read
the new catalog through the checker, naming every expected field:

```sh
loom-session run <session> workshop/verify-recipe --name recipe-good --input project=<project> --input recipe=elh-demo --input artifact=demo_target --input artifact_dir_blank=true --input cmake_target=demo_target --input cmake_build_dir=<project>/build --input cmake_config=Debug --input cmake_entry_blank=true --wait 30
loom-session run <session> workshop/verify-recipe --name recipe-wrong --input project=<project> --input recipe=elh-demo --input cmake_config=Release --wait 30
```

Use the absolute build path Files actually reported, with its displayed separators. The first
check must pass; the second must fail with the configuration mismatch. This checker observes
local filesystem content. Pair its result with the preceding settled write run, initial
catalog and captures to support the story of who changed it.

## Leave and return

Keep the ELH running; each new CLI invocation is already a fresh client. Recover the write,
checks and selected message history by their recorded names:

```sh
loom-session status <session>
loom-session show <session> <write-run>
loom-session show <session> recipe-good
loom-session crossings <session> <write-run>
```

Compare the host lifetime with the one you recorded before leaving. Keep the meaningful
crossings, including far session/author/attempt, before configured retention can evict them.
`loom-session stop` ends the host; it is not client detachment.

For an engineering proof, retain a compact ordinary run output identifying the source candidate
(including uncommitted changes if any), dependency/build/staged runtime, executable and actual
loaded modules or an equivalent verifiable staging chain, project and fixture, decisive run
names, outcomes and evidence paths. Recover that output through `show` as well. A supplied DLL
path whose timestamp precedes a process start does not alone establish that the process loaded it.
Concrete machine paths belong in the proof artifact, not this reusable guide.

Repeat on a supported TUI when shared behavior is in scope; record which platform each run
actually used. Neither ELH injection nor a TUI capture establishes physical-device behavior.
