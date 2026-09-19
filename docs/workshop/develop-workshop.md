# Develop Workshop from inside Workshop

**Walkthrough.** Change one of the panes Workshop ships — Attention, the Builder, the Editor and
the rest — while Workshop is running it: open the pane's code from the pane, change it, build the
pane's CMake target, and watch the same pane change with what it keeps intact. Read a failed build
without leaving Workshop, then keep a change or take it back.

This is the loop [edit a running pane](edit-a-running-pane.md) teaches, with Zengine's own source
as the project. That page makes a pane of *your* project from one source file; this one works on
the panes Zengine's CMake tree builds. The gestures are the same. What is new is the launch: one
Run in CLion opens the Workshop to work in, the same Run every time.

```
right-click a shipped pane → edit code → change it → Ctrl+s
    → Builder: b, with load after build on → the same pane shows the change, state kept
    → a build that fails: l reads what the compiler said, in the Builder
    → Shift+r runs the previous image again, or Shift+p makes this one what the next launch runs
```

## What you need

- **A Zengine checkout that is yours to edit.** The files the Editor opens and saves are that
  checkout's.
- **An installed Loom**, and a compiler and CMake that build Zengine with the SDL skin (the
  default) — what [Build it](../../README.md#build-it) asks for. The launch opens Workshop's window.
- **CLion**, or a shell. The launch is one run configuration in CLion and one executable in a
  shell; both run the same thing.

## Once: open the project and Run

In CLion, open the checkout as a CMake project and give its CMake profile the prefix the Loom is
installed in: `-DCMAKE_PREFIX_PATH=<prefix>` in the profile's CMake options. Reload the CMake
project. The run configuration list now holds **Develop Workshop**, beside the ones CLion makes for
each target: it is shared from the repository's `.run/` directory, and it names one target,
`zengine-workshop-develop`, and no path. Select it and press **Run**.

The first Run builds the host and every artifact its load plans name — minutes, once. Then the
launch says what it does in CLion's Run window, before Workshop's own lines:

```
zengine-workshop-develop - build tree: …/cmake-build-debug
zengine-workshop-develop - runtime: …/cmake-build-debug/workshop-runtime
-- zengine: development runtime made at …/cmake-build-debug/workshop-runtime
zengine-workshop-develop - project: …/cmake-build-debug/workshop-project (Workshop's project files)
zengine-workshop-develop - launching …/workshop-runtime/zengine-workshop --load-plan …/workshop-runtime/graphical-load-plan.json --recipes …/workshop-runtime/development-build-recipes.json
```

and Workshop's window opens with the development catalog in force: the Builder's header names
`development-build-recipes.json`. On Windows the program is `zengine-workshop.exe`, and the
compiler's runtime libraries come from the environment CLion gives the Run for the profile's
toolchain; Workshop's builds run in that environment too.

From a shell it is the same executable, run where the compiler runs (`deps` is the Loom prefix, as
in [Build it](../../README.md#build-it)):

```sh
cmake -S Zengine -B Zengine/build -DCMAKE_PREFIX_PATH="$PWD/deps"
cmake --build Zengine/build --target zengine-workshop-develop
Zengine/build/workshop/zengine-workshop-develop
```

## Every time after: Run

Press **Run** again. The launch finds the runtime it made, checks that it is this build tree's,
that the tree has built none of what it copied anew, and that every copy is still there, and opens
it in the same project directory, with every promotion made there:

```
-- zengine: reusing the development runtime at …/cmake-build-debug/workshop-runtime
```

Quit Workshop from Workshop, whose quit asks about unsaved work; CLion's Stop ends the launch
without asking, and leaves the Workshop it started open. CLion's **Debug** on this configuration
debugs the launch itself; to debug Workshop, attach to its process (`zengine-workshop`).

## One Run at a time, for one runtime

A runtime is one Workshop's: the images it loads, the panes you promote into it and the project
files beside it are one directory's, and two Workshops over one runtime would write each other's.
So a launch **claims** the runtime before it looks at anything or prepares anything, and holds the
claim until the Workshop it started has exited. A second Run over that runtime prepares nothing,
starts nothing, stops nothing, and says so:

```
zengine-workshop-develop - another launch holds the runtime …/workshop-runtime: it is preparing it, or the Workshop it started is still open. Use that Workshop, or quit it and launch again -- nothing was prepared, launched or stopped
```

The claim is the system's to keep: a named mutex on Windows, a lock file of yours under
`/run/user/<uid>` or `/tmp` on Linux, named after the runtime directory. The system lets go of it
when the launch ends, however it ends — so a crash, or CLion's Stop, leaves nothing held and
nothing for you to delete. (The Linux lock file stays where it was made; nothing reads it for an
answer, and it is not one to clean up.) Two runtimes are two claims, and one runtime spelled
another way — other case on Windows, through a link on Linux — is the one claim.

What the claim does not cover is a Workshop no launch holds: one started by hand from the runtime,
or one CLion's Stop left open. For those there is the other check — the runtime's host image is in
use — and a launch beside one of them refuses too, and stops nothing:

```
zengine-workshop-develop - …/workshop-runtime/zengine-workshop.exe is running, and no launch holds its runtime: a Workshop started from it some other way, or left open by a launch that was stopped. Quit it, then launch again -- nothing was prepared, launched or stopped
```

On Linux that check is the kernel's answer (`ETXTBSY`), and a kernel that lets a running program's
file be opened for writing does not give it.

## Where everything is

Four directories take part, and each has one job:

| directory | holds | written by |
|---|---|---|
| the **checkout** | the source you edit | you, through the Editor |
| the **build tree** — `cmake-build-debug`, or `Zengine/build` | everything CMake builds, each pane target in its own directory, the launch in `workshop/` | `b` in the Builder, CLion's build before a Run, and any `cmake --build` of yours |
| the **runtime** — `workshop-runtime` in the build tree | the Workshop that runs: the host, every artifact it loads, both load plans and both recipe catalogs, copied once | the launch makes it; a reload copies into it and a promotion writes it; no build does |
| the **project directory** — `workshop-project` in the build tree | Workshop's project files: setups, a pane you made, and any plan or recipes you author ([arguments](getting-started.md#arguments)) | Workshop, when you save |

Configuring writes two files beside the host, in the build tree's `workshop/` directory (under a
multi-config generator, in a directory per configuration, with a runtime per configuration too):

- `development-build-recipes.json`, the **development catalog**: one `cmake_target` recipe per
  shipped pane weave ([which](#which-panes)), each naming this build tree, the target, the
  directory the target builds into, and the pane's own source file as its **editing entry** — the
  file `edit code` opens. The entry is the source the build declared for that weave, never a file
  name worked out from the pane's title or the target's name.
- `development-runtime.cmake`, the script the launch runs to make the runtime or reuse it. You can
  run it yourself: `cmake [-DZEN_RUNTIME=<dir>] -P <build>/workshop/development-runtime.cmake`.

*Why a copy.* A Workshop running from the build tree's `workshop/` directory has loaded the very
files a build copies there. A build would then write over images the process is using — Windows
refuses the write, and Linux lets it change code under a running program — and it would change
what the next launch runs without anyone choosing that. Nothing a build does writes the runtime. A
pane's target builds into its own directory in the build tree; when a build is loaded, Workshop
copies the product into the runtime under a name no earlier load used and loads that copy. The
artifacts link Zengine and the Loom in rather than sharing them as libraries, so the build tree
holds no file the runtime loads.

*Why a launch.* It is outside Workshop: nothing in Workshop starts, stops, reloads or relaunches
the Workshop you are working in, and building the launch starts nothing. The catalog is its
choice, not the project's: run by hand, Workshop reads the project's `build-recipes.json` or the
shipped default exactly as it always does, and `u` in [Files](files.md) switches catalogs while it
runs. Without the SDL skin the launch refuses; the runtime script, then the runtime's
`zengine-workshop --recipes <runtime>/development-build-recipes.json` from a directory of your
own, gives a terminal Workshop the same setup.

## When the host changed: a new runtime

A runtime copies the host once. When the build tree has built the host, a service (a skin, an
input reader, the Timer, an operator provider), a load plan or a catalog anew — after a pull, or a
change to Workshop's own code — that runtime would still run what it copied, so the launch refuses
and names what changed:

```
zengine: …/workshop-runtime was made from this build tree at …, and the tree has built zengine-workshop.exe anew since, so it would still run the copies it took then -- nothing was copied, changed or launched.
```

Nothing is removed. To make a runtime from what the tree has built now, rename or move
`workshop-runtime` — its promotions and reloads stay in it — and Run again. To keep runtimes side
by side, give the launch another directory: `--runtime <dir>` in a copy of the run configuration's
program arguments, or on the command line. A rebuilt pane never makes a runtime stale; reloading
it is what the runtime is for.

## When a pane's messages change: a new runtime too

A reload replaces a running pane only with an image that answers to exactly the messages the
running one does and keeps the same state; anything else is refused at the reload, and the pane
goes on running the image it had ([a build that worked and a load that was refused](#a-build-that-fails)).
The launch cannot see this kind of change — a rebuilt pane never makes a runtime stale — so
Running the same runtime again runs the copy it took of the old pane.

Three changes of that kind are on this branch:

- **The desktop hears Loom's word that one of its asks never arrived** (`zen.DispatchRefused`),
  so a desktop built from this source is refused as a reload of one built before it.
- **The inventory the Pane Manager and Info read gained a field**, and a message shared by the
  host and a pane must come from one build: an Info or a desktop from the other side of the change
  loads, but every inventory sent to it is refused at its door, and its list reads
  `PANES (waiting)` for good. That the message kept its name and version promises nothing, and
  neither does a load that went ahead.
- **The desktop is usable by mouse**, and that is six more doors on its accept set — the press
  that names a picture, the wheel, the second button, a menu's answer, the toggle's answer and an
  edit's answer — so a desktop built from this source is refused as a reload of one built before
  it, and the Neovim editor's new right-button door does the same for it. A host from before this
  change never sends the desktop's new sentences; a desktop from before it never hears them.

For either, rebuild the whole tree — CLion's build, or a `cmake --build` of it — so that the host
and every pane come from the same source. Then rename or move `workshop-runtime` (its promotions
and reloads stay in it) and Run: the launch makes a new runtime from what the tree built, host
and panes together, and after that, a desktop rebuilt from its own source reloads in place again.
Nothing you authored lives in the runtime: your project files are in `workshop-project`, and your
keymap and last session are in your per-user folders ([where](getting-started.md#arguments)), so
the new runtime opens on them unchanged. Quit a Workshop before moving the runtime it runs from,
and delete nothing: the old runtime is the only copy of what you promoted into it.

## When the launch refuses

Every refusal copies nothing, starts nothing, and leaves the runtime as it was.

| it says | because | what to do |
|---|---|---|
| `… another launch holds the runtime …` | a Run has that runtime: preparing it, or its Workshop is open | use that Workshop, or quit it and Run |
| `… is running, and no launch holds its runtime` | a Workshop from that runtime is open that no Run holds — started by hand, or left by a Stop | quit it, then Run |
| `… could not claim the runtime … for this launch` | the system would not give the claim it names | read that reason; nothing was prepared |
| `… has built … anew since …` | the host, a service, a plan or a catalog was rebuilt | [a new runtime](#when-the-host-changed-a-new-runtime) |
| `… is incomplete: … is not there` | a file of the runtime was removed | a new runtime |
| `… configuration, not its … one`, or `… copied other files …` | the build tree was configured differently since | a new runtime |
| `… recorded too little to tell whether it is whole …` | an earlier Zengine made that runtime | a new runtime |
| `… made from the build tree …, not from …` | the directory is another build tree's runtime, or this tree moved | another directory |
| `… is not empty and is not a development runtime …` | `--runtime` names a directory of other files | an empty or new directory |
| `… is not there -- build the tree first` | the tree has not built what a runtime copies | build, then Run |
| `… staged no graphical load plan …` | the tree was configured with `-DZENGINE_SDL_SKIN=OFF` | configure with the SDL skin |

## The loop: point, change, build, reload

The example is **Attention**, which keeps one thing: the conditions you hid. If it says
`0 conditions`, there is nothing to hide and a reload only shows new text; any condition — a
preferences file Workshop could not read, say — gives you one. Press into Attention and `d` hides
it.

**1. Right-click Attention, and choose `edit code`.** The Editor opens `attention-pane/pane.cpp`
from your checkout, and the Builder chooses the recipe for you:

```
build recipe: zengine-attention-pane -> zengine-attention-pane -- the source of Attention is open in the Editor; save it, then build (load after build: off)
```

Nothing is built, and Attention keeps running with what it had.

**2. Change it, and save.** The heading Attention draws, `"ATTENTION -- "`, is written in
`say_view`, near the end of the file. The Editor has no search and no go-to-line: `Ctrl`+`End`
goes to the end, and the wheel rolls the view back a few lines a notch, leaving the caret where it
was. Press on the line to put the caret there, type — `"ATTENTION (mine) -- "`, say — and press
`Ctrl`+`s`. The build reads the saved file, never the Editor's buffer.

**3. Build, and reload.** Press into the Builder. The first time after launch, press
**`Shift`+`b`** — the notice says `load after build: on` — then **`b`**. A pane target builds in a
few seconds, and the realize row says:

```
realized, NOT DEFAULT (promote / revert) -- reloaded in place -- weave #21 keeps its id and its state
```

Attention shows the new heading, and the condition you hid is still hidden: the same weave, new
code, its state carried. The build wrote only the build tree. The image now running is a copy in
`workshop-runtime/zengine-attention-pane.reloads/`, and `workshop-runtime/zengine-attention-pane.so`
(`.dll` on Windows) — the file the next launch loads — is untouched. That is what `NOT DEFAULT`
says.

**4. Again.** Press into the Editor, change the line, `Ctrl`+`s`, press into the Builder, and `b`
alone: load after build stays on until Workshop quits, and the Builder pane keeps it across its
own reloads too.

## A build that fails

Make a mistake, save, and `b`. The Builder's rows say:

```
last     FAILED -- op #4, 1 out -- read output
realize  REFUSED -- the build failed, so nothing was offered to the project
```

and Attention keeps running the code it had. Your edit is still in the file, saved.

**Press `l` (*read output*).** The Builder shows that build's own lines, the way the build wrote
them, with a header saying which build they belong to:

```
output #4 zengine-attention-pane -- FAILED, exit 1 -- lines 1-5 of 8, 4 characters spelled in ASCII
[1/2] Building CXX object attention-pane/CMakeFiles/zengine-attention-pane.dir/pane.cpp.o
FAILED: attention-pane/CMakeFiles/zengine-attention-pane.dir/pane.cpp.o
```

and, further down, the line that names the file, the line and the column —
`…/attention-pane/pane.cpp:416:78: error: 'oops' was not declared in this scope`, with the path
written out in full. While the output is open:

| key | does |
|---|---|
| `↑` `↓`, the wheel | one line up or down; the wheel a few at a time |
| `Home` / `End` | the first line, or the last lines — and it follows new ones |
| `←` `→` | pan by half the pane's width, for the end of a long line |
| `[` / `]` | the output of the build before, or after, this one |
| `Escape` | close it; the Builder's rows come back |

Fix the mistake in the Editor, save, and `b`: the reload goes ahead as if nothing had happened.

What the reader shows, and what it does not:

- **It belongs to one build.** A newer build, another choice of recipe or a status about another
  build moves nothing it shows; `[` and `]` are how you change builds.
- **It is kept by the build tool, and bounded.** The tool keeps the output of its last few builds
  and, of a long one, the beginning and the end, with the lines between counted rather than kept;
  a line too long to keep is cut and counted. The header says so whenever it happened. The numbers
  are in `builder/vocabulary.hpp` (`kKeptOperations`, `kKeptHeadBytes`, `kKeptTailBytes`,
  `kMaxKeptLineBytes`). Nothing is written to a file, and a restart keeps nothing.
- **It is spelled in what the screen can draw.** A compiler's curly quotes and dashes are shown as
  their plain ASCII twins, any other character that cannot be drawn as `?`, and colour codes not at
  all — and the header counts them. A path, a line and column and the caret line under the source
  read exactly as written. On Windows, Ninja re-encodes the non-ASCII characters it passes on, so
  a compiler's quotes can arrive as other characters and read as `?`.

**A build that worked and a load that was refused are two answers.** Take one message out of the
`loom::Accept<…>` list near the top of `attention-pane/pane.cpp` — `PaneActionRequested`, say —
save, and build. It compiles, so the `last` row says `succeeded`; but the rebuilt pane answers to
different messages than the running one, so the realize row says the reload was refused and why,
and Attention keeps running the image it had, with the same weave. `l` on that build says it
succeeded. Put the message back, save, and build again. (A field added to what the pane keeps,
`AttentionPaneState` in `attention-pane/vocabulary.hpp`, is refused the same way.)

## Keep it, or take it back

- **`Shift`+`p` promotes.** The running image is written into
  `workshop-runtime/zengine-attention-pane.so`, so the next launch of this runtime runs it. What
  it wrote over is kept beside the reloads (`zengine-attention-pane-<n>-promoted-over`).
- **`Shift`+`r` reverts.** The image before the last reload runs again, in the same weave, state
  kept. After a promotion it still runs the code you had; when that is the promoted image, the
  realize row stops saying `NOT DEFAULT`.
- **Neither touches your checkout.** The saved source stays as it is — the Builder's notice says
  `saved source unchanged` — and the next `b` builds it. Take a source change back in the source,
  with your version control.
- **Neither touches anything but this runtime.** The build tree, the files `Zengine/build/workshop/`
  holds and any other runtime are exactly what they were.

A reload you never promoted is gone when Workshop quits. [Edit a running
pane](edit-a-running-pane.md#every-time-point-change-build-reload) walks through promote and revert
step by step; [the Builder](builder.md#load-after-build-and-reload-in-place) is their reference.

## Which panes

The development catalog holds a recipe for each pane weave Zengine ships:

| recipe | the panes it draws | editing entry |
|---|---|---|
| `zengine-attention-pane` | Attention | `attention-pane/pane.cpp` |
| `zengine-builder-pane` | Builder | `builder-pane/pane.cpp` |
| `zengine-desktop-pane` | the desktop: the Pane Manager, Hotkeys, the room's floor and the keys that work anywhere | `desktop-pane/pane.cpp` |
| `zengine-editor-pane` | Editor | `editor-pane/pane.cpp` |
| `zengine-neovim-editor` | Neovim (the Editor, when you switch to it) | `neovim-editor/pane.cpp` |
| `zengine-files` | Files | `files/files.cpp` |
| `zengine-info-pane` | Info | `info-pane/pane.cpp` |
| `zengine-terminal-pane` | Terminal | `terminal-pane/pane.cpp` |
| `zengine-introspection` | Loaded, Project, Powers | `introspection/introspection.cpp` |
| `zengine-composer` | Compose | `composer/composer.cpp` |

**Everything else is deliberately not in it.** The host, the skins, the input readers, the Timer
and the two operator providers are what every pane stands on, and Workshop's own panel (Layouts)
is part of the host — `edit code` on it says so. Changing any of those is an
ordinary rebuild, a new runtime and a relaunch.

**Whether a rebuilt pane reloads is decided at the reload**, by the same owner that decides it for
any project, one row at a time: same shapes only, and never an artifact that also supplies
operators. Each pane carries across what its state declares — Attention what you hid, the Builder
its chosen recipe and the load-after-build switch, the Editor its document, the desktop the row
its Pane Manager has chosen (a chosen row that left the list stays unchosen), Info its list's
choice the same way — and what it keeps anywhere else, it asks for again or starts afresh: the
desktop's half-typed pane name, its notices and the Hotkeys pane's scroll do not cross, and a
Pane Creator act the old image was still waiting on is answered to nobody. A change to what a
pane answers to is not a reload at all
([a new runtime too](#when-a-panes-messages-change-a-new-runtime-too)). The Builder can rebuild
and reload **itself**: the build and its output belong to the build tool rather than to the pane,
so the answer to the build it asked for reaches the reloaded pane, and an open output reader
simply closes.

**The entry is where reading starts, not the pane's files.** A pane is more than one source — its
vocabulary header, the packages it links. Files goes anywhere, so walk to the checkout and open
the rest from there; `m` marks it for the next time.

## When it does not go that way

| you see | because | what to do |
|---|---|---|
| `cannot read …/attention-pane/pane.cpp: No such file or directory -- Attention's code was not opened` | the checkout the catalog names is not there | put it back, or set up again (below) |
| `FAILED`, and `l` reads `CMake Error: The source directory "…" does not exist.` | the build tree's checkout moved or is gone | the same |
| `did not start … the configured CMake build tree … has no …` | the build tree is not where the catalog says | the same |
| the Editor refuses the file: `… holds a byte outside plain ASCII …` | the Editor edits plain ASCII only, and a shipped pane source is kept that way; a file you open from Files may not be | edit that file elsewhere |
| `recipe … is a cmake_target recipe with no editing entry` | a catalog whose recipe names no entry — every catalog written before entries existed | use the development catalog, or give the recipe an `entry` ([the Builder](builder.md#an-existing-cmake-target)) |
| the reload is refused with a changed shape | the rebuild changed what the pane keeps or answers to | put the shape back; replacing a shape is not something Workshop drives yet |

**A setup does not follow a move.** The development catalog names the checkout and the build tree
by absolute path, and a CMake build tree names its checkout too. Move either, configure again
and Run: a runtime made before the move is another tree's now, so move it aside or name another
directory, and the launch makes a new one.

**Two setups side by side stay apart.** Two checkouts, each with its own build tree and runtime,
build artifacts with the same names — both have a `zengine-attention-pane`. Each runtime's catalog
names its own build tree and checkout, each runtime's reloads and promotions are files in its own
directory, each launch opens its own tree's runtime in its own project directory, and the script
refuses to make one build tree's runtime in a directory that is already another's. The sharing
that remains is the kind you ask for: a runtime directory you name for two launches — one launch
at a time, since the first to claim it holds it — and a catalog you copy, which names the tree it
was written for.

## What this does not do

- **It is not an installed Workshop that develops itself.** A development runtime is a copy of one
  build tree, for working on Zengine; nothing here installs, packages or updates a Workshop, and
  a runtime that a changed host made stale is kept, not refreshed.
- **It does not launch from inside.** Workshop cannot start, stop, reload or relaunch the process
  it runs in; the launch is CLion's Run, or the executable in a shell.
- **It does not replace the host or a service.** Only the pane weaves above are rebuilt and
  reloaded from here.
- **It does not build, reload, promote or revert for you.** Each is a gesture, and each says what
  it did.
- **It does not search.** The Editor holds one file and has no find or go-to-line; the reach to a
  line far down a pane's source is the wheel.

The pieces, each in depth: [the Builder](builder.md) (recipes and their entries, load after build,
reload, promote, revert), [the Editor](editor.md), [Files](files.md), and the Builder package's
[reference](../reference/builder.md) for the build output a build keeps.
