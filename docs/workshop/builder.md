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

Both files live **beside the executable**, and both can be named at launch:

```
zengine-workshop --recipes <path> --load-plan <path>
```

An absent `--recipes` file is not an error — a project with nothing to build is an ordinary
project, and Workshop says so in its banner. A **malformed** one is refused out loud and
Workshop exits, because silently ignoring an authored file a maker got wrong is the quiet
wrong answer this repository keeps refusing.

## Choosing a recipe catalog while Workshop is running

`--recipes` and the shipped default choose the catalog this session **starts** with. They are
not the only catalog the process can ever use: while Workshop is running you can point at any
recipe file the [Files](files.md) pane can reach and make it the current one, with no restart.

Press into Files, put the cursor on the file, and press **`u`** (*use as recipes*). It is an
ordinary action like every other — your keymap file can move it, and the hotkey view lists it
under the browser's own keys.

What happens next is one transaction:

```
read the file  →  parse it  →  complete it against this project  →  make it current
```

Every step happens on a **candidate**. Nothing about the session changes until the whole
candidate is ready, so:

- **If it works**, the recipes this Workshop means are that file's. The Builder pane moves to
  the new catalog straight away — it is not closed, rebuilt or reopened — and every other place
  that asks what this project can build gets the same new answer. The notice names the file and
  how many recipes it holds.
- **If it does not**, the catalog you were already using is still the catalog you are using.
  Nothing is half-replaced: the same recipes, the same Builder rows, the same builds. The
  notice says what was wrong **and** that the previous catalog is still active.

Three more things are worth knowing:

- **Choosing the file that is already current re-reads it.** That is deliberate: it is how you
  pick up an edit you just saved. Nothing watches the filesystem, nothing polls and nothing
  reloads on its own — your press is the event.
- **The file on disk is the input.** If that same catalog is open with unsaved edits in [the
  source editor](editor.md), the *saved* file is what is read. Nothing is auto-saved, and an
  unsaved draft never becomes build procedure. Save first, then press `u` again.
- **Your selection follows the recipe, not the row.** If the recipe you had chosen still exists
  in the new catalog, it stays chosen wherever it has moved to. If it is gone, the choice is
  cleared rather than handed to whatever now occupies that row.

Choosing a catalog changes **build intent and nothing else**. It does not move your project —
relative sources still mean the directory you launched Workshop in, and the catalog file's own
directory never becomes a source base — and it does not touch your document, your setup, or
what is loaded.

The choice lasts for the session. Nothing is remembered: the next launch picks its catalog from
`--recipes` or the default exactly as this one did.

Once you have changed catalogs, the Builder pane carries a `catalog` row naming the one in
force, so the banner's answer being out of date is not something you have to keep in your head.

## Using it

Open the pane with **`p`** → `Builder` → `Enter`. Then:

| key | does |
|---|---|
| **`c`** / **`Shift+c`** | move through the recipes this project holds (it wraps) |
| **`b`** | **build** the recipe you have chosen |
| **`Shift+b`** | **load after build** — one action in two states. Before or during a build it is a *toggle*: armed, the next `b` builds **and** loads the result into the running project. When an artifact is built, nothing was asked about loading it and nothing is armed, it is a *button*: press it and the built artifact is loaded now |
| **`Shift+p`** | **promote** the running image — make it the file a restart loads (after a reload in place; below) |
| **`Shift+r`** | **revert** — run the image before the last reload again, state kept (below) |
| **`f`** | **build and realize the frontier** — the one artifact the project is waiting on (below) |
| **`e`** | **open the chosen recipe's source** in [the source editor](editor.md) — `single_source` recipes only; a `cmake_target` recipe names no single source and refuses in those words. The [Files](files.md) pane opens any project file through the same door |
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

- `source` may be absolute, as above, or **relative to the project** — the directory you
  launched Workshop in, printed on the startup banner. It is resolved once, when the catalog
  is read, so the editor, the check that the file exists, and the generated project that
  compiles it all name the same file. That stays true of a catalog you choose later: it is
  completed against the same project, wherever the file itself happens to live. Your recipe
  file is never rewritten: what you wrote stays what you wrote.
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
host's one rule, exactly as a load plan's stem is. `artifact_dir` is where that file lands. For a
single-source recipe, empty means the recipe's **workspace**, under `out/` — CMake is told to put
it there, and never on the file the running project has loaded, because that file is mapped by
the process (Windows refuses a writer on it; Linux lets a writer change code under the program).
For a CMake-target recipe, empty means the host's own artifact directory: that project puts its
file where it puts it. An explicit `artifact_dir` is honoured as written for both kinds, and the
hazard is then yours.

So a plain build no longer changes the file a restart loads. The product reaches that file when
you **load** it — a first realization copies it into place; a reload in place copies it to a
per-operation path beside the host and opens that, and **promote** is what writes it into the
plan's file (below).

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

## Load after build, and reload in place

`Shift+b` armed, then `b` — or the button, after a plain build — asks for the result to be handed
to the running project. A row that is **waiting** is realized: the product is copied into the
plan's file and loaded, exactly as it always was. A row that is **already live** is now
**reloaded in place**:

- the host copies the rebuilt product to a per-operation path beside itself
  (`<stem>.reloads/<stem>-<n>`), off the file the process has mapped;
- the realization owner asks the Weave Manager for `zen.ReloadWeave` over that copy, bracketed
  by the same operator offer a load carries;
- the Loom swaps the code behind the **same `WeaveId`** and carries the weave's **state** across.
  Nothing else in the process learns anything: the role, the routing and every other
  participant are untouched.

The Builder's realize row then says *reloaded in place — weave #N keeps its id and its state*,
and the same weave answers the same messages, with the new code.

**Two things a reload leaves you.** The file the plan resolves the stem to is exactly what it
was, so a quit now runs the old code next launch — the row says *NOT DEFAULT* rather than leave
that to be discovered. **`Shift+p` promotes**: the running image's bytes are written into the
plan's file (a sibling, then a rename, so a refused write leaves nothing half-written), and the
row says *default*. **`Shift+r` reverts**: the image before the last reload runs again, through
the same reload — same id, state kept. A promotion that wrote over the previous image leaves
nothing honest to revert to, and revert says so.

**Same shapes only.** A reload carries state and keeps routing, so it is refused — before the
running weave is touched — when the rebuilt weave keeps a different **state** or answers to
different **messages**. The refusal says *that* the shape changed and *what* changed:

> the rebuilt 'zengine-oven' keeps a different STATE than the running one; a same-shape reload
> cannot carry the state across, so the running weave was left as it is. Replacing it is a
> prepared replacement with an authored migration (Loom: state schema version mismatch; reload
> refused)

The road on from there is the Loom's prepared replacement with a migration you author, which
Workshop does not host yet. Two more refusals are Workshop's own: an artifact that also
**supplies operators** to the catalog is not reloaded (its weave would run new code while the
catalog kept the old image; unmount-and-remount is not built), and a provider-only row has no
weave to reload.

A row is *waiting* when this run reached it, the artifact was not on disk, and some authored
recipe can produce it. That is the case a project has on its first run: the plan says how the
artifact participates, the artifact has not been built yet, and Workshop starts anyway and says
which rows are waiting. An artifact that is missing and that **nothing here can build** still
refuses the project by name, exactly as it did before. And a plain `b` of a waiting row's recipe
leaves the row waiting until you load it: nothing is realized because a file appeared.

## What it deliberately does not do

- **No arbitrary shell recipes.** There is no field anywhere — in a file, in a message, or on a
  panel — that names a program, an argument, a working directory or a shell line. A recipe
  names *inputs* to a mechanism, and the mechanism is CMake either way. "The panel sent a
  command" is not a sentence the vocabulary can express.
- **No multi-source recipe**, no globbed source list, no dependency graph and no solver. One
  source file, or one target in a project that already has a CMakeLists.
- **No replacement and no migration.** A reload in place is for a rebuilt weave whose shapes
  did not change. Nothing unloads, replaces or migrates a weave, and a changed shape is refused
  before the running weave is touched.
- **No automatic build-on-missing.** Nothing starts a build because a file is absent. A maker
  presses a key.
- **No recipe discovery.** Nothing searches for recipe files, adopts a conventional filename,
  reads a `CMakeLists.txt`, detects a build system or writes a recipe for you. The catalog is a
  file you named — at launch, or by pointing at it.
- **No rewriting of a build that is already running.** Changing catalogs while a build is in
  flight does not cancel it, restart it or re-aim it: that operation finishes from the facts it
  started with, and its result is reported as being about the recipe that actually started it.
  The next build you ask for uses the new catalog.
- **No containment.** A build is an ordinary child process of Workshop, exactly as privileged
  as Workshop is. That is the honest description; implying a boundary that is not there is the
  one thing these pages will not do.

## What the pane still asks of a maker

- A recipe file is edited in a text editor, not in Workshop. There is no recipe editor and no
  way to add a single recipe at run time — what you can change at run time is *which whole
  catalog file* is in force. (A `single_source` recipe's **source** opens in Workshop's own
  editor with `e`; any other project file — a recipe catalog included — opens from the
  [Files](files.md) pane, see [the source editor](editor.md).)
- A single-source recipe names its package prefixes by hand. Nothing discovers where a Zengine
  package is installed.
- A rebuilt weave whose **shape** changed does not enter the running project: the refusal
  names the change, and the road on is a prepared replacement you author. A reloaded image is
  not the file a restart loads until you promote it, and the row says so.

See [limitations](limitations.md).
