# Develop Workshop from inside Workshop

**Walkthrough.** Change one of the panes Workshop ships — Attention, the Builder, the Editor and
the rest — while Workshop is running it: open the pane's code from the pane, change it, build the
pane's CMake target, and watch the same pane change with what it keeps intact. Read a failed build
without leaving Workshop, then keep a change or take it back.

This is the loop [edit a running pane](edit-a-running-pane.md) teaches, with Zengine's own source
as the project. That page makes a pane of *your* project from one source file; this one works on
the panes Zengine's CMake tree builds. The gestures are the same. What is new is the setup, and
the setup is most of this page:

```
right-click a shipped pane → edit code → change it → Ctrl+s
    → Builder: b, with load after build on → the same pane shows the change, state kept
    → a build that fails: l reads what the compiler said, in the Builder
    → Shift+r runs the previous image again, or Shift+p makes this one what the next launch runs
```

## What you need

- **A Zengine checkout that is yours to edit.** The files the Editor opens and saves are that
  checkout's.
- **An installed Loom**, and a compiler and CMake that build Zengine — exactly what
  [Build it](../../README.md#build-it) asks for.
- **A terminal** of about 200 by 56 cells, so Attention, the Builder and the Editor show together.

## Once: a checkout, a build tree, a runtime

Four directories take part, and each has one job:

| directory | holds | written by |
|---|---|---|
| the **checkout** | the source you edit | you, through the Editor |
| the **build tree** | everything CMake builds, each pane target in its own directory | `b` in the Builder, and any `cmake --build` of yours |
| the **runtime** | the Workshop that runs: the host, every artifact it loads, both load plans and both recipe catalogs, copied once | Workshop: a reload copies into it and a promotion writes it; no build does |
| the **project directory** | Workshop's project files — the document, setups, and any plan or recipes you author ([arguments](getting-started.md#arguments)) | Workshop, when you save |

From the directory that holds your checkout and the prefix the Loom is installed in (`deps`, as
in [Build it](../../README.md#build-it)):

```sh
cmake -S Zengine -B Zengine/build -DCMAKE_PREFIX_PATH="$PWD/deps"
cmake --build Zengine/build --target zengine-workshop
cmake -DZEN_RUNTIME="$PWD/workshop-runtime" -P Zengine/build/workshop/development-runtime.cmake
mkdir workshop-project
cd workshop-project
../workshop-runtime/zengine-workshop --recipes ../workshop-runtime/development-build-recipes.json
```

**1. Configure and build.** Any generator works, and `-DBUILD_TESTING=OFF` and
`-DZENGINE_SDL_SKIN=OFF` leave out what this loop does not use. Building `zengine-workshop` builds
the host and every artifact its load plans name. Configuring writes two more files beside the
host in `Zengine/build/workshop/` (under a multi-config generator, in a directory per
configuration):

- `development-build-recipes.json`, the **development catalog**: one `cmake_target` recipe per
  shipped pane weave ([which](#which-panes)), each naming this build tree, the target, the
  directory the target builds into, and the pane's own source file as its **editing entry** — the
  file `edit code` opens. The entry is the source the build declared for that weave, never a file
  name worked out from the pane's title or the target's name.
- `development-runtime.cmake`, the script that makes a runtime from this build tree.

**2. Make the runtime.** The script copies the host, the artifacts, both plans and both catalogs
into `ZEN_RUNTIME` — `Zengine/build/workshop-runtime` when you do not name one, or a directory per
configuration under a multi-config generator — and writes `zengine-development-runtime.txt` there,
naming the build tree and the checkout the runtime was made from.

*Why a copy.* A Workshop running from `Zengine/build/workshop/` has loaded the very files a build
copies there. A build would then write over images the process is using — Windows refuses the
write, and Linux lets it change code under a running program — and it would change what the next
launch runs without anyone choosing that. Nothing a build does writes the runtime. A pane's target
builds into its own directory in the build tree; when a build is loaded, Workshop copies the
product into the runtime under a name no earlier load used and loads that copy. The artifacts link
Zengine and the Loom in rather than sharing them as libraries, so the build tree holds no file the
runtime loads.

The runtime is made **once**. The script refuses, and copies nothing, when the directory is
already this build tree's runtime (to pick up what the tree has built since, quit that Workshop,
remove the directory and run the script again), when it is a runtime made from another build tree,
when it holds anything else, or when the tree has not built what it copies.

**3. Launch it** from a directory of its own, naming the development catalog. The catalog is
chosen only by you: without `--recipes`, Workshop reads the project's `build-recipes.json` or the
shipped default, exactly as it always does, and `u` in [Files](files.md) can switch catalogs while
it runs. The Builder's header names the catalog in force. Launching from the checkout works too,
but the project files you save then land among the source.

## Every time: point, change, build, reload

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
| `zengine-editor-pane` | Editor | `editor-pane/pane.cpp` |
| `zengine-files` | Files | `files/files.cpp` |
| `zengine-info-pane` | Info | `info-pane/pane.cpp` |
| `zengine-terminal-pane` | Terminal | `terminal-pane/pane.cpp` |
| `zengine-introspection` | Loaded, Project, Powers | `introspection/introspection.cpp` |
| `zengine-composer` | Compose | `composer/composer.cpp` |

**Everything else is deliberately not in it.** The host, the skins, the input readers, the Timer
and the two operator providers are what every pane stands on, and Workshop's own panels (Layouts,
Pane Manager) are part of the host — `edit code` on one says so. Changing any of those is an
ordinary rebuild, a new runtime and a relaunch.

**Whether a rebuilt pane reloads is decided at the reload**, by the same owner that decides it for
any project, one row at a time: same shapes only, and never an artifact that also supplies
operators. Each pane carries across what its state declares — Attention what you hid, the Builder
its chosen recipe and the load-after-build switch, the Editor its document — and what it keeps
anywhere else, it asks for again. The Builder can rebuild and reload **itself**: the build and its
output belong to the build tool rather than to the pane, so the answer to the build it asked for
reaches the reloaded pane, and an open output reader simply closes.

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
by absolute path, and a CMake build tree names its checkout too. Move either, and configure
again, build, and make a new runtime.

**Two setups side by side stay apart.** Two checkouts, each with its own build tree and runtime,
build artifacts with the same names — both have a `zengine-attention-pane`. Each runtime's catalog
names its own build tree and checkout, each runtime's reloads and promotions are files in its own
directory, and the script refuses to make one build tree's runtime in a directory that is already
another's. The sharing that remains is the kind you ask for: two Workshops launched from one
runtime directory share its images, and a catalog you copy names the tree it was written for.

## What this does not do

- **It is not an installed Workshop that develops itself.** A development runtime is a copy of one
  build tree, for working on Zengine; nothing here installs, packages or updates a Workshop.
- **It does not replace the host or a service.** Only the pane weaves above are rebuilt and
  reloaded from here.
- **It does not build, reload, promote or revert for you.** Each is a gesture, and each says what
  it did.
- **It does not search.** The Editor holds one file and has no find or go-to-line; the reach to a
  line far down a pane's source is the wheel.

The pieces, each in depth: [the Builder](builder.md) (recipes and their entries, load after build,
reload, promote, revert), [the Editor](editor.md), [Files](files.md), and the Builder package's
[reference](../reference/builder.md) for the build output a build keeps.
