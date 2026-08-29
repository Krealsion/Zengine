# Workshop's Builder

**Reference, current state.** What the Builder pane can actually do today, said without
aspiration. The package underneath it is [the Builder package](../reference/builder.md).

## Two truths, and they are two files

Zengine keeps *how an artifact is made* and *how an artifact participates* apart, in two
authored files with two owners:

| file | says | read by |
|---|---|---|
| **build recipes** (`default-build-recipes.json`) | how an artifact can be **produced** | the Builder |
| **[load plan](load-plans.md)** (`default-load-plan.json`) | how an artifact **participates at runtime** | the realization owner |

Nothing joins them but the **artifact stem**. A recipe says which artifact it produces; a plan
row *is* an artifact. So "can this project build `zengine-oven`?" and "does this project run
`zengine-oven`?" are two questions with two answers, and neither file carries a copy of the
other's fields — no roles, mount modes or load order in a recipe, no compiler flags, source
lists or build trees in a plan.

Both files live **beside the executable**, and both can be replaced:

```
zengine-workshop --recipes <path> --load-plan <path>
```

An absent `--recipes` file is not an error — a project with nothing to build is an ordinary
project, and Workshop says so in its banner. A **malformed** one is refused out loud and
Workshop exits, because silently ignoring an authored file a maker got wrong is the quiet
wrong answer this repository keeps refusing.

## Using it

Open the pane with **`p`** → `Builder` → `Enter`. Then:

| key | does |
|---|---|
| **`c`** / **`Shift+c`** | move through the recipes this project holds (it wraps) |
| **`b`** | **build** the recipe you have chosen |
| **`Shift+b`** | **build and realize** it — build, and if that works, offer the result to the running project |
| **`f`** | **build and realize the frontier** — the one artifact the project is waiting on (below) |
| **`e`** | **open the chosen recipe's source** in [the source editor](editor.md) — `single_source` recipes only; a `cmake_target` recipe names no single source and refuses in those words |
| **`p`** | remove the pane |

The pane shows the chosen recipe and what it makes, where the last build got to (with its
operation number and how many times the runner has been heard about it), the exit status, what
was actually run, **the realization outcome on its own row**, and the last lines the build
said (three rows ordinarily, two while the project-frontier row below is present).

**A build outcome and a realization outcome are two answers and the pane shows two.** A build
that worked whose realization was refused is a completely different situation from a build that
failed, and both are ordinary.

## The project frontier

When realization is **waiting** on an artifact this project can build, the pane says so on a
row of its own — read live from the realization owner at every repaint, never from a copy:

```
project  waiting zengine-oven (oven, blocks 3)
```

That one row is the join a maker used to perform by hand across two panes: **which artifact**
the project stopped at, **which recipe produces it** (matched by artifact stem against the
catalog the pane already shows), and **how many authored rows** are stopped behind it. When
several recipes produce the artifact the row counts them (`2 recipes`), and when none does it
says `no recipe` — a frontier this project cannot produce is a different problem, and the row
will not guess. When nothing is waiting the row is simply absent: the Builder is an ordinary
Builder, and no "all good" is manufactured.

**`f` spends that row.** It selects the recipe that produces the frontier — visibly, so the
recipe row and the ask agree — and performs exactly what `Shift+b` performs: the same build,
the same offer, the same realization decision by the same owner. When **several** recipes
produce the frontier, `f` refuses to choose between them and names them; pick one with `c` and
press `f` again. When nothing is waiting, or nothing here produces the artifact, `f` says so
and asks for nothing.

`f` starts a build **only when pressed**. Encountering a buildable missing artifact never
starts a compiler on its own, and a plain `b` of the frontier's recipe still leaves the row
waiting until realization is explicitly asked for.

## Two kinds of recipe

### An existing CMake target

The artifact is already owned by a CMake project. The recipe names a **configured build tree**
and a target in it, and the action is the command you would type:

```
cmake --build <build tree> --target <target>
```

```json
{ "recipe": "skin-tui-block",
  "artifact": "zengine-skin-tui-block",
  "artifact_dir": "/path/to/build/surface",
  "cmake_target": [ { "build_dir": "/path/to/build",
                      "target": "zengine-skin-tui-block",
                      "config": "" } ],
  "single_source": [] }
```

It names a *configured* tree rather than a source tree on purpose: the project it builds has
already been configured by whoever owns it, with whatever policy they chose, and a Builder that
re-configured somebody else's tree would be deciding a policy that is not its to decide.
`config` is for a multi-config generator and is empty everywhere else.

### One source file

You write **one `.cpp`** and no CMakeLists at all. Zengine generates a tiny CMake project
around it, and CMake compiles and links it.

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

- `packages` is `CMAKE_PREFIX_PATH`. The generated project says `find_package(zengine CONFIG
  REQUIRED)` and nothing else, so it is an **ordinary external consumer** of the installed
  package — the same thing any other project is. If the prefix does not carry a Zengine
  package the configure fails, and it must: a fallback that reached into a source tree would
  make every green here meaningless.
- `links` is a list of **exported target names**, not a link line. `zengine::timer`,
  `loom::switchboard`. A `-l`, a path or a library file is refused by name.
- `toolchain_from` is a **configured build tree whose toolchain this borrows** — its generator,
  platform, toolset, make program, C++ compiler and build type, read with CMake's own
  `load_cache()`. Leaving it empty means "let CMake choose for this machine", which is right
  where there is one compiler and is said to be a default rather than a decision.
- `workspace` is where the generated project is written. Empty means beside the host's
  artifacts. It is a **durable directory and never a temporary**: a build that failed leaves
  its project on disk, and every refusal names the path, because that is where you go to read
  why.

**Zengine does not drive a compiler.** Nothing in it names `g++`, `clang++` or `cl.exe`, chooses
an ABI flag, discovers a link library, invents an output suffix or knows what a Debug postfix
is. Every one of those is CMake's.

## Where the artifact lands, and how success is decided

`artifact` is a **stem** — `zengine-oven`, never `zengine-oven.so` — spelled to a file by the
host's one rule, exactly as a load plan's stem is. `artifact_dir` is where that file lands, and
empty means the host's own artifact directory. For a single-source recipe CMake is told to put
it there; for a CMake-target recipe it is where that project already puts it.

A build is a **success** when two things are true, checked in this order:

1. the build process exited zero, **and**
2. the file the recipe names is there.

The order is the whole of the stale-artifact guarantee: a failed build is a failure whatever is
sitting at the destination, so an artifact left by an earlier success can never be read as this
build's product. And a build that exits zero with its artifact absent is **neither** — it is
`NO ARTIFACT`, said in those words, because a green build whose product is missing has told you
something true about a process and something false about your project.

There is **no scanning**. Nothing looks in a directory for something new, or newest, or
plausible: the recipe says what it produces, and a file it did not name counts for nothing.

## Build & realize

`Shift+b` asks for the result to be handed to the running project. It is performed only when
**all** of these hold:

- you asked for it;
- the build succeeded and the artifact is there;
- the project's **load plan already names that artifact** — the participation is the plan's,
  never the recipe's;
- the row is **waiting** rather than already part of the running project.

A row is *waiting* when this run reached it, the artifact was not on disk, and some authored
recipe can produce it. That is the case a project has on its first run: the plan says how the
artifact participates, the artifact has not been built yet, and Workshop starts anyway and says
which rows are waiting. An artifact that is missing and that **nothing here can build** still
refuses the project by name, exactly as it did before.

An artifact that is **already loaded** is refused, in words:

> artifact 'zengine-oven' is already part of this running project. BLD-1 does not unload,
> reload or replace a live artifact, so a rebuilt file has NOT changed the image that is
> running — restart to pick it up.

## What it deliberately does not do

- **No arbitrary shell recipes.** There is no field anywhere — in a file, in a message, or on a
  panel — that names a program, an argument, a working directory or a shell line. A recipe
  names *inputs* to a mechanism, and the mechanism is CMake either way. "The panel sent a
  command" is not a sentence the vocabulary can express.
- **No multi-source recipe**, no globbed source list, no dependency graph and no solver. One
  source file, or one target in a project that already has a CMakeLists.
- **No hot reload.** Nothing unloads, reloads, replaces or migrates anything. A rebuilt file
  has not changed the image that is running.
- **No automatic build-on-missing.** Nothing starts a build because a file is absent. A maker
  presses a key.
- **No containment.** A build is an ordinary child process of Workshop, exactly as privileged
  as Workshop is. That is the honest description; implying a boundary that is not there is the
  one thing these pages will not do.

## What the pane still asks of a maker

- A recipe file is edited in a text editor, not in Workshop. There is no recipe editor, no
  picker beyond `c`, and no way to add a recipe at run time. (A `single_source` recipe's
  **source** is the one file Workshop itself can edit — press `e`; see
  [the source editor](editor.md).)
- A single-source recipe names its package prefixes by hand. Nothing discovers where a Zengine
  package is installed.
- A rebuilt artifact that is already live needs a restart to take effect, and the refusal says
  so rather than pretending otherwise.

See [limitations](limitations.md).
