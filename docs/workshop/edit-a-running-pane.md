# Edit a running pane

**Walkthrough.** Point at a pane that is running, open the code that draws it, change it, and
watch the same pane change without restarting Workshop — then keep the change or take it back.
The example is **Tally** ([`examples/tally-pane/tally.cpp`](../../examples/tally-pane/tally.cpp)):
one source file, a pane that shows a count, and a count that survives a reload.

The loop, once it is set up:

```
right-click the pane → edit code → change it → Ctrl+s
    → Builder: Shift+b, b → the same pane shows the change, count kept
    → Shift+r takes it back, or Shift+p makes it what the next launch runs
```

Each arrow is a gesture you make. Opening code builds nothing, saving builds nothing, a build
reloads only because you armed it, and nothing is kept for the next launch until you promote.

## What you need

- **Workshop, built** from a Zengine build tree ([getting started](getting-started.md)).
- **Zengine and the Loom installed into a prefix** from that same build, the way
  [using Zengine from another project](../getting-started.md#using-zengine-from-another-project)
  shows (`cmake --install … --prefix "$PWD/deps"` for both). A one-file recipe is compiled as an
  ordinary project that finds the installed packages and nothing else, so the prefix is not
  optional — and it has to be the build Workshop runs from, or the pane speaks a protocol
  Workshop does not.
- **A terminal** to launch Workshop from, and to read a compiler's whole answer when a build fails
  ([below](#a-build-that-fails)).
- **Room for four panes**: a terminal of about 200 by 56 cells shows Files, the Builder, Tally and
  the Editor together. On a smaller one, Workshop says which pane is waiting for room.

## Once: make Tally a pane of your project

Workshop's project is the directory you launch it in. Make one, and put the example's source in
it — a copy, so the file you edit is yours:

```sh
mkdir tally-project
cp Zengine/examples/tally-pane/tally.cpp tally-project/
cd tally-project
../Zengine/build/workshop/zengine-workshop
```

**1. Author its recipe, in Files.** `p`, choose `Files`, press into it, put the cursor on
`tally.cpp` and press **`a`** (*pick buildable*). `Return` chooses `tally.cpp`; then answer one
line at a time, `Return` after each:

| asked | answer |
|---|---|
| recipe name | `tally` (suggested) |
| artifact stem | `tally` (suggested) |
| package prefix | the prefix you installed into — both, comma-separated, if Zengine and the Loom are in two |
| link targets | `zengine::pane,zengine::activation,zengine::input,loom::switchboard` |

The link targets are written at the top of `tally.cpp`. Files answers `authored recipe `tally` ->
tally in …/build-recipes.json`.

**2. Put it in the project, and build it.** Press anywhere on the workspace to give Workshop the
keys back, `p`, choose `Builder`, and press into it. `c` steps the recipe row to
`tally -> tally`. Press **`o`** (*load it*) and type the role **`example.tally`**, then `Return`.
The role must be the office `tally.cpp` speaks as (`kOffice` in the source): a weave can speak
only as the office its plan row gives it, and a pane whose source names another office never
appears in the picker.

Nothing is built yet, so the Builder says the project is waiting on `tally`. Press **`f`**: it
builds the one recipe that produces it and loads the result. The first build configures a small
generated project and takes a few seconds; then the realize row says
`realized -- weave #… as example.tally`.

**3. Open it.** Give Workshop the keys back, `p`, choose `Tally`. It says `Tally: 0`. Press into
it; each `Space` adds one.

Your project now holds `build-recipes.json` and `workshop-plan.json`, and launching Workshop from
this directory again loads Tally with the rest — open it from the picker.

## Every time: point, change, build, reload

**1. Right-click Tally, and choose `edit code`.** The Editor opens `tally.cpp` with your keys in
it, and the notice says what the code is and what comes next:

```
opened the source of Tally -- recipe `tally` builds tally; save, then build it in the Builder
with load after build, and Tally reloads in place
```

The Builder has chosen the `tally` recipe for you and says so on its first row. Nothing was
built, nothing was selected, and Tally is still running with its count.

**2. Change it, and save.** The rows Tally says are in `rows_for`, at the top of the file under
`CHANGE THIS FIRST`. Change `"Tally: "` to `"Count: "` and press `Ctrl`+`s`. The Editor's status
row says `saved`. The build reads the saved file, never the buffer.

**3. Build, and reload.** Press into the Builder, press **`Shift`+`b`** — the notice says
`load after build: on` — then **`b`**. When the build finishes the realize row says:

```
realized, NOT DEFAULT (promote / revert) -- reloaded in place -- weave #25 keeps its id and its state
```

and Tally says `Count: 3`: the new code, the same weave, the count you had. The rebuilt image was
copied beside Workshop (`tally.reloads/tally-1…`) and loaded from there, so the file the plan
names — the one a restart loads — is untouched. That is what `NOT DEFAULT` means.

**4. Keep it, or take it back.**

- **`Shift`+`r` reverts.** The image before the last reload runs again, in the same weave, count
  kept: Tally says `Tally: 3`. **Your saved source is not reverted** — `tally.cpp` still says
  `Count`, the Builder's notice says `saved source unchanged`, and the next `b` builds it again.
  To go back in the source too, change the file back and build.
- **`Shift`+`p` promotes.** The running image is written into the file the plan names, so the
  next launch runs it; the realize row says `promoted: the next launch runs the image weave #…
  is running now`. A count lives in the running weave, so a fresh launch starts again at
  `Count: 0` — with the new text.
- **`Shift`+`r` after `Shift`+`p` still takes it back.** Promote kept the bytes it wrote over
  (`tally.reloads/tally-<n>-promoted-over…`), so `Tally: 3` runs again and the realize row says
  `NOT DEFAULT` again: the next launch runs what you promoted. `Shift`+`r` once more runs it.

Revert while you are trying things, and promote once you want to keep one. A reload you never
promoted is gone at the next launch, and the realize row keeps saying `NOT DEFAULT` so that is
not a surprise.

## When it does not go that way

### A build that fails

Make a typo, save, and `b`. The Builder says `FAILED` on its `last` row and
`REFUSED -- the build failed, so nothing was offered to the project` on its realize row, and
Tally keeps running the code it had. Your edit is still in the Editor, saved.

The Builder's rows show the beginning of what the build said, and a compiler's reason is further
down its output. Grow the Builder (right-click it, `arrange`, `=`) until its `ran` row shows,
and run that command in a terminal to read every line — the error names the file, the line and
the column. Fix it, save, and `b` again: the reload goes ahead as if nothing had happened.

### A change to what the pane keeps

Add a field to `TallyState` and build with load after build armed. The build works, and the
reload is refused before anything is replaced:

```
the rebuilt 'tally' changed a shape but kept its name and version, and this process already
holds the old meaning, so the running weave was left as it is. Put the shape back to reload in
place; a changed shape needs a new version and a prepared replacement
```

Tally keeps running. Put the field back, save, and build again. Replacing a running weave with a
differently-shaped one is a prepared replacement with a migration you write, which Workshop does
not drive yet ([limitations](limitations.md)).

### `edit code` that cannot open anything

`edit code` opens a file only when it can say which one. Otherwise it says why, and opens
nothing:

| the notice starts | because | what to do |
|---|---|---|
| `no build recipe produces …, the artifact behind …` | no recipe in the catalog makes the artifact that pane runs from — every pane Zengine itself ships, before you write one | author a recipe for its source with `a` in Files |
| `2 recipes build …` | several recipes make that artifact, and choosing between them is yours | choose one in the Builder with `c`, then `e` there opens its source |
| `recipe … is a cmake_target recipe` | that recipe builds an existing CMake target, which names a build tree, not one file | open the file you mean from Files |
| `… is part of Workshop itself` | the pane is one Workshop draws | there is no separate code to open |
| `… is drawn by … which this project's plan did not load` | the pane's weave did not come from this project's load plan, so no artifact is known for it | load it through the plan |

`edit code` also meets the Editor's own rules: if the Editor holds unsaved changes to another
file, the open is refused in those words, and nothing moves until you save or discard them.

## What this does not do

- **It does not build, reload or promote for you.** Each is a gesture, and each says what it did.
- **It opens one file.** A single-source recipe names exactly one source; the headers it includes
  are yours to find, and a pane built by a CMake target has no one file to open.
- **Workshop's own panes are not reachable this way yet.** Files, the Builder and the rest are
  built by Zengine's own CMake tree, and a recipe for a CMake target names no source.
- **It does not search.** The Editor holds one file and has no find or go-to-line; the example
  keeps the part to change at the top so the first loop is short.

The pieces, each in depth: [the Builder](builder.md) (recipes, load after build, reload, promote,
revert), [the Editor](editor.md) (opening, saving, what can refuse an open), the
[context menu](panes.md#the-context-menu--what-can-i-do-with-this), and
[making a Workshop tool](../guides/make-a-workshop-tool.md) for a pane of your own.
