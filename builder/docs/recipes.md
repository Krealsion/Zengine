# The recipe file

**Reference.** What a build recipe holds, where the file it lives in comes from, where a build's
artifact lands, and how a build is judged. The commands that build are in the manuals:
[the Builder pane](zengine.builder-pane.md) and [the Builder](zengine.builder.md).

## Two files, joined by a stem

Zengine keeps *how an artifact is made* and *how an artifact takes part* apart, in two files:

| file | says | read by |
|---|---|---|
| **build recipes** (`build-recipes.json`) | how an artifact can be **produced** | the Builder |
| **[load plan](../../workshop/docs/load-plans.md)** (`workshop-plan.json`) | how an artifact **takes part** in the running project | the realization owner |

Nothing joins them but the artifact's **stem**. A recipe says which artifact it produces; a plan
row *is* an artifact. No role, mode or load order is in a recipe, and no compiler, source or build
tree is in a plan.

At launch Workshop reads `--recipes <path>` when it is given; otherwise `build-recipes.json` in the
directory it was launched in, when there is one; otherwise the shipped `default-build-recipes.json`
beside Workshop. A recipe file that is there and refused stops Workshop with its reason. With no
`--recipes` and no file found, Workshop says it can build nothing and starts.

The file is a `zengine-build-recipes` document, version 2. A version-1 file is read whole, with no
editing entry on any CMake target; the first recipe added to it from Files writes the whole file as
version 2. A later version is refused by its number, and a version-2 row with no `entry` is refused
rather than read as "no entry".

## Two kinds of recipe

Every recipe has a name (`recipe`), the stem of the artifact it makes (`artifact`) and where that
file lands (`artifact_dir`), and exactly one of two mechanisms. A recipe with neither, or both, is
refused.

### An existing CMake target

The artifact is already owned by a CMake project. The recipe names a **configured** build tree and
a target in it, and the build is the command you would type: `cmake --build <tree> --target
<target>`, with `--config <config>` when the recipe names one.

```json
{ "recipe": "skin-tui-block",
  "artifact": "zengine-skin-tui-block",
  "artifact_dir": "/path/to/build/examples/snake",
  "cmake_target": [ { "build_dir": "/path/to/build",
                      "target": "zengine-skin-tui-block",
                      "config": "",
                      "entry": "" } ],
  "single_source": [] }
```

- The tree is configured by whoever owns it; the Builder never configures it.
- `config` is for a multi-config generator (Visual Studio, Xcode, Ninja Multi-Config), and empty
  under a single-config one such as plain Ninja.
- `artifact_dir` is where the target's file actually lands, which is not always the folder of the
  package that declares it. A build that works and ends `NO ARTIFACT` means this field is wrong.
  Empty means beside Workshop.
- `entry` is the **editing entry**: the one source file a reader of this artifact starts at, which
  [`builder.edit-source`](zengine.builder-pane.md#builderedit-source) opens. Empty means the recipe
  names no file, and opening its source is refused in words. A relative entry means the project
  directory.

### One source file

You write one `.cpp` and no CMakeLists. Zengine writes a small CMake project around it, and CMake
compiles and links it.

```json
{ "recipe": "oven",
  "artifact": "zengine-oven",
  "artifact_dir": "",
  "cmake_target": [],
  "single_source": [ { "source": "/home/me/My Weaves/oven.cpp",
                       "packages": [ "/opt/zengine", "/opt/loom" ],
                       "links": [ "zengine::timer", "loom::switchboard" ],
                       "toolchain_from": "/home/me/project/build",
                       "workspace": "" } ] }
```

- `source` is absolute, or relative to the project: the directory Workshop was launched in, named
  on its startup banner. It is resolved once, when the file is read.
- `packages` is `CMAKE_PREFIX_PATH`. The written project says `find_package(zengine CONFIG
  REQUIRED)`, an ordinary consumer of the installed package, so the prefix must hold one: it must be
  the install of the Zengine that Workshop runs from.
- `links` is a list of exported target names, `zengine::timer`, `loom::switchboard`; a `-l`, a path
  or a library file is refused by name.
- `toolchain_from` is a configured build tree whose toolchain this borrows (generator, platform,
  toolset, make program, C++ compiler and build type, read with CMake's `load_cache()`); empty lets
  CMake choose for this machine.
- `workspace` is where the written project goes; empty means `build-workspace/<recipe>` beside
  Workshop. It is kept: a failed build leaves its project there, and its last line names it,
  `zengine build: CMake configure FAILED (exit 1)` or `zengine build: compile or link FAILED (exit
  1)`.

Zengine names no compiler, no flag, no output suffix and no library: every one of those is CMake's.

## Where the artifact lands, and how a build is judged

`artifact` is a **stem**, `zengine-oven`, never `zengine-oven.so`, spelled to a file by one rule:
`<stem>.so` on Linux and macOS, `<stem>.dll` on Windows, in the artifact directory and nowhere else.

**Your own CMake project has to produce exactly that name.** CMake's default for `add_library(oven
SHARED ...)` on Linux is `liboven.so`, so the build exits zero and the Builder says `NO ARTIFACT`. In
a CMake-target recipe, set it yourself:

```cmake
set_target_properties(oven PROPERTIES PREFIX "" OUTPUT_NAME "oven")
```

A one-file recipe needs nothing: the project written around your `.cpp` sets both.

A one-file build's artifact lands in its workspace's `out/`, never on the file the running project
has loaded. Loading it is a separate step: the first load copies it into place beside Workshop, a
reload in place copies it to `<stem>.reloads/` and opens that copy, and only a promotion writes the
file a restart loads ([loading what you built](../../workshop/docs/load-plans.md#when-a-built-artifact-is-loaded-again)).

A build **succeeded** when two things are true, checked in this order: the build process exited zero,
and the file the recipe names is there. A failed build is a failure whatever sits at the destination,
so a file an earlier build left is never read as this build's. A build that exits zero with its file
absent is `NO ARTIFACT`. Nothing scans a folder for something new: a file the recipe did not name
counts for nothing.

## Choosing another recipe file while Workshop runs

In [Files](../../files/docs/files.md), put the cursor on a recipe file and press `u` (*use as
recipes*). The file is read, checked and completed against the project as one step, and only then
made the recipe file in force:

- **If it works**, the Builder pane moves to it at once, and the notice names the file and how many
  recipes it holds. Your chosen recipe follows its name into the new file, and is cleared when the
  recipe is gone.
- **If it does not**, the file you were using is still in force, whole, and the notice says what was
  wrong and that the previous file is still active.
- **Choosing the file already in force reads it again**: that is how an edit you saved is picked up.
  Nothing watches the disk.
- **The saved file is read**, never an unsaved draft in the Editor.
- A relative source still means the directory Workshop was launched in. A build already running ends
  against the file it was asked from. The choice lasts until Workshop quits.

## Adding a recipe from Files

In Files, put the cursor in a folder and press `a` (*pick buildable*). A chooser lists what that
folder can try to build: every `.cpp` file, and every folder holding a `CMakeCache.txt`, a configured
CMake tree. Choose one, then answer the few things nothing can detect, one line at a time: for a
source, the recipe's name, the artifact's stem, the package prefixes and the link targets; for a
tree, the name, the CMake target, the stem, an optional artifact folder and, only when the tree holds
several configurations, which one. `Return` takes a line and `Escape` cancels the whole, writing
nothing.

The row is appended to the recipe file in force when you named it (with `--recipes` or `u`), or when
`a` made it. When the shipped default is in force, the row goes into `build-recipes.json` in your
project, seeded with the shipped rows, and that file becomes the one in force; the shipped file is
never written. A row the recipe rules refuse, such as a name already used, is refused whole in their
words, and the file is left exactly as it was. There is no recipe editor: a row is changed or removed
in a text editor.

## What a recipe cannot do

- **Name a program.** No field anywhere, in a file, a message or a pane, names a program, an
  argument, a folder to run in or a shell line. A recipe names inputs to CMake.
- **Build several sources**, glob a list, or resolve dependencies: one source file, or one target in
  a project that already has a CMakeLists.
- **Build on its own.** Nothing starts a build because a file is missing, and nothing finds recipe
  files: you name the file, at launch or with `u`, or `a` makes the project's.
- **Contain a build.** A build is an ordinary child process of Workshop, exactly as privileged as
  Workshop.
