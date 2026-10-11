# zengine.builder-pane -- the Builder pane

**Manual.** The pane you build with. It shows the recipes your project can build, builds the one you
choose, and hands what it built to the running project. It is a *weave*: a piece of Workshop that is
loaded, replaced and spoken to by messages, built as `zengine-builder-pane`. Other weaves reach it by
its *office*, the name it answers to, `zengine.builder-pane`. The builds themselves are the Builder
office's, [`zengine.builder`](zengine.builder.md). New to it? Start with
[Build your first pane](tutorials/build-your-first-pane/README.md).

## About

A *recipe* is an entry in your project's [recipe file](recipes.md) that says how one file is made:
from one `.cpp` file, or from a target in a CMake project someone already configured. The file it
makes is an *artifact*, named by its *stem* (`tally`, never `tally.dll`). The pane shows the recipe
you have chosen and the artifact it makes, how the last build went and what it said, and what the
running project made of the result. Building a file and loading it are two answers, and the pane
shows both: a build can succeed and its load still be refused.

The pane runs nothing itself. It asks the Builder office to build, and to offer a build to the
project; the project's realization owner, `zengine.realization`, decides whether to load it. It asks
the project which file a recipe names, the plan office to add a row to the project's plan, and the
opening office to open a source. It publishes a promotion or a revert for the realization owner. It
shows what those offices say.

## Use

Open it from the Pane Manager, <!-- key desktop.panes -->`ctrl+p`<!-- /key -->. Its keys act only
while it holds your keys: press into it first. The ordinary path: choose a recipe
([`builder.recipe`](#builderrecipe), [`builder.recipes`](#builderrecipes)), build it
([`builder.build`](#builderbuild)), then load what was built
([`builder.load-built`](#builderload-built)), or turn on load-after-build first
([`builder.arm`](#builderarm)) so each build that works is loaded. A recipe the project does not run
yet is put into its plan with [`builder.load`](#builderload).

Each command is also a control in the strip below the rows, and a line in the pane's menu. A control
that cannot act now is drawn in round brackets, `(promote the loaded image)`, and still answers,
with its reason.

Every command that builds, loads, promotes, reverts, writes the plan or opens a file asks Workshop
first. A refusal names what was not done and why, as `` `tally` was not built -- <the reason> ``: a
guest without the `build` power is refused this way before anything runs. The reason is Workshop's
own, `8 acts are already waiting on Workshop`, `nothing was queued to <office>`, or `the ask could
not reach <office> (<the bus's reason>)`. Until the Builder office has said what it builds, a
command says `the Builder has not said what it builds yet` and does nothing.

## Commands

### `builder.build`

Builds the recipe you have chosen. With load-after-build on, the build that works is also offered to
the running project.

- **Takes:** the chosen recipe; the load-after-build switch ([`builder.arm`](#builderarm)). Default
  key <!-- key builder.build -->`b`<!-- /key -->; the control `[build]`.
- **Answers:** sends [`BuildRequested`](zengine.builder.md#buildrequested). The notice says
  `` asked the Builder for `tally` -- Workshop stays live while it builds `` (with load-after-build
  on, `` ... for `tally` and to realize it -- ... ``), and the `last` row `running -- op #1, 12 out`.
  When the build ends the notice is its outcome, `` succeeded `tally` -> tally -- built tally ``,
  and for a build offered to the project, then the project's answer: `realize: realized, NOT
  DEFAULT (promote makes it the file a restart loads) -- <the owner's words>`, or `realize: REFUSED
  -- <the owner's words>`.
- **Refuses:** `this project has no build recipes -- nothing was asked for`; `` `tally` was not built
  -- it is not in the recipes this pane last heard ``; and the refusals every act shares ([Use](#use)).
  While another build runs, the Builder takes no second one: its `said` row says so, and the notice
  ends with the build that was running.
- **Example:** choose `tally`, press <!-- key builder.build -->`b`<!-- /key -->.

### `builder.build-realize`

One key with two meanings. When a build has finished, nothing loaded it and load-after-build is off,
it loads that build, as [`builder.load-built`](#builderload-built); otherwise it turns
load-after-build on or off, as [`builder.arm`](#builderarm).

- **Takes:** the build finished and unloaded, or the switch. Default key
  <!-- key builder.build-realize -->`shift+b`<!-- /key -->; no control of its own.
- **Answers:** as [`builder.load-built`](#builderload-built) when a build waits to be loaded; as
  [`builder.arm`](#builderarm) otherwise.
- **Refuses:** only as a load: `` `tally` was not loaded -- it is not what is built and waiting now
  `` when another build finished between your aim and your press, and the refusals every act shares.
  It never refuses as the switch: it changes meaning instead.
- **Example:** after a plain build that worked, press
  <!-- key builder.build-realize -->`shift+b`<!-- /key --> to load it.

### `builder.promote`

Makes the image running now the file the next launch loads. After a reload in place the file a
restart loads still holds the old code, and the `realize` row says `NOT DEFAULT (promote / revert)`;
this writes the running image over it, keeping the bytes it wrote over for a revert.

- **Takes:** the artifact standing: the one the last realization is about, realized or refused, not
  the chosen recipe. Default key <!-- key builder.promote -->`shift+p`<!-- /key -->; the control
  `[promote tally]`.
- **Answers:** publishes `PromoteArtifact`. The notice says `` asked to promote `tally` -- the file a
  restart loads takes the running image ``, and the `realize` row then shows the owner's answer:
  `promoted: the next launch runs the image weave #25 is running now`, or its refusal.
- **Refuses:** `nothing this Builder realized is standing -- nothing to promote`; `` `tally` was not
  promoted -- it is not what is standing now ``; the owner's refusals on the `realize` row, such as
  `artifact 'tally' already runs from the file a restart loads; there is nothing to promote`.
- **Example:** after a reload that worked, press <!-- key builder.promote -->`shift+p`<!-- /key -->.

### `builder.revert`

Runs the image from before the last reload again, by another reload in place: the same weave, its
state kept, your saved source unchanged.

- **Takes:** the artifact standing. Default key <!-- key builder.revert -->`shift+r`<!-- /key -->; the
  control `[revert tally]`.
- **Answers:** publishes `RevertArtifact`. The notice says `` asked to revert `tally`: the previous
  image runs, state kept; saved source unchanged ``, and the `realize` row shows the owner's answer
  when the reload settles: `reverted: weave #25 runs the image before the last reload again, and
  keeps its id and its state`.
- **Refuses:** `nothing this Builder realized is standing -- nothing to revert`; `` `tally` was not
  reverted -- it is not what is standing now ``; the owner's refusals on the `realize` row, such as
  `artifact 'tally' has no previous image to revert to: it has not been reloaded in this run, or a
  promotion wrote over the image before its last reload`.
- **Example:** press <!-- key builder.revert -->`shift+r`<!-- /key --> to take a reload back.

### `builder.load`

Puts the chosen recipe's artifact into the project, under a role you type, so the project runs it.
The project's *plan* says what runs and as what; this adds one row to it.

- **Takes:** the chosen recipe's artifact, then the role, typed on a line the pane opens
  ([commands on the role line](#commands-on-the-role-line)). Default key
  <!-- key builder.load -->`o`<!-- /key -->; the control `[add to the load plan...]`.
- **Answers:** asks the project whether its plan already names the artifact; when it does not, the
  role line opens, `role for tally> `. What the line's commit then does is
  [`authoring.commit`](#authoringcommit).
- **Refuses:** `` `tally` is already in this project's plan -- build-and-load it instead ``; `this
  project has no build recipes -- nothing to load`; `` `tally`: nothing was added to the plan -- ``
  and the project office's reason when the ask could not reach it.
- **Example:** choose `tally`, press <!-- key builder.load -->`o`<!-- /key -->, type `example.tally`,
  press <!-- key authoring.commit -->`return`<!-- /key -->.

### `builder.recipe`

Chooses the next recipe in the recipe file, wrapping at the end. Choosing builds nothing. Your choice
follows its recipe's name when the recipe file changes, and is cleared when the recipe is gone.

- **Takes:** the recipe file in force. Default key <!-- key builder.recipe -->`c`<!-- /key -->; no
  control and no menu line.
- **Answers:** the `recipe` row moves; the notice says `build recipe: tally -> tally`.
- **Refuses:** `this project has no build recipes to choose between`.
- **Example:** press <!-- key builder.recipe -->`c`<!-- /key --> until the `recipe` row names `tally`.

### `builder.recipe-back`

Chooses the previous recipe, wrapping at the start. Otherwise as [`builder.recipe`](#builderrecipe).

- **Takes:** the recipe file in force. Default key
  <!-- key builder.recipe-back -->`shift+c`<!-- /key -->.
- **Answers:** the `recipe` row moves back one; the notice says `build recipe: tally -> tally`.
- **Refuses:** as [`builder.recipe`](#builderrecipe).
- **Example:** press <!-- key builder.recipe-back -->`shift+c`<!-- /key -->.

### `builder.frontier`

Builds and loads the one artifact the project is waiting on. When the project reaches a row of its
plan whose artifact is not built, and a recipe can build it, the project waits there, at its
*frontier*, and the `project` row says so: `waiting tally (tally, blocks 0)`.

- **Takes:** what the project is waiting on, read when you press; the recipe that makes it. Default
  key <!-- key builder.frontier -->`f`<!-- /key -->; the control `[build what is waited on]`.
- **Answers:** chooses the recipe that makes the waited-on artifact, visibly, and sends
  [`BuildRequested`](zengine.builder.md#buildrequested) with the load aboard; the notice says
  `` asked the Builder for `tally` and to realize it -- Workshop stays live while it builds ``, then
  the build's outcome and the project's answer, as [`builder.build`](#builderbuild). It does not
  turn load-after-build on.
- **Refuses:** `this project is not waiting on any artifact -- nothing was asked for`; `` no authored
  recipe produces `tally` -- nothing was asked for ``; when several recipes make it, it names them
  and asks you to choose one first, with [`builder.recipe`](#builderrecipe) or the list.
- **Example:** after [`authoring.commit`](#authoringcommit) put an unbuilt `tally` in the plan,
  press <!-- key builder.frontier -->`f`<!-- /key -->.

### `builder.edit-source`

Opens the chosen recipe's source in the Editor: a one-file recipe's `.cpp`, or a CMake recipe's
*editing entry*, the one file a reader of that artifact starts at.

- **Takes:** the chosen recipe; in the recipe list, the row under the cursor, without changing your
  choice. Default key <!-- key builder.edit-source -->`e`<!-- /key -->; the controls `[edit source]`
  and, in the list, `[edit this recipe's source]`.
- **Answers:** asks the project which file the recipe names, then asks the opening office to open it;
  the Editor shows it, and the pane says nothing when it worked.
- **Refuses:** `` `skin-tui-block` is a cmake_target recipe -- it names no source file or editing
  entry to open ``; `` this project's recipes do not hold `tally` -- nothing was opened ``; `this host
  resolves no recipe sources -- nothing was opened`; `` `tally`: the source was not opened -- `` and
  the opening office's words when it cannot open the file now.
- **Example:** press <!-- key builder.edit-source -->`e`<!-- /key -->; the Editor opens `tally.cpp`.

### `builder.output`

Reads what a build said, whole: the pane becomes a reader bound to that one build, until you close
it. While it reads, no build command is reachable.

- **Takes:** the build the `last` row names. Default key <!-- key builder.output -->`l`<!-- /key -->;
  the control `[read output #1]`.
- **Answers:** asks the Builder office for a page of that build's lines
  ([`BuildOutputRequested`](zengine.builder.md#buildoutputrequested)). The header names the build,
  how it ended and which lines show, `output #1 tally -- FAILED, exit 1 -- lines 1-40 of 212`, and
  each row is one line as the build wrote it, spelled in ASCII.
- **Refuses:** `no build has started in this Workshop -- there is no output to read`; `` output #1:
  the page was not read -- `` and the reason when the ask could not reach the Builder office.
- **Example:** after a build says `FAILED`, press <!-- key builder.output -->`l`<!-- /key --> and read
  up from the end for the compiler's reason.

### `builder.recipes`

Opens the recipe file as a list, so a recipe can be chosen by pointing. The list's cursor is not your
choice: moving it changes nothing until you choose.

- **Takes:** the recipe file in force. Default key <!-- key builder.recipes -->`return`<!-- /key -->;
  the control `[choose a recipe...]`, or a press on the `recipe` row while the pane holds your keys.
- **Answers:** the notice `choose a recipe -- Return takes it, Escape leaves the choice as it was`,
  and the list under the heading `choose a recipe -- 2 recipes in <the recipe file's path>`, with
  `(chosen)` beside your choice ([commands in the recipe list](#commands-in-the-recipe-list)).
- **Refuses:** `this project has no build recipes -- there is nothing to choose between`.
- **Example:** press <!-- key builder.recipes -->`return`<!-- /key -->, move to `tally`, press
  <!-- key builder.recipe-choose -->`return`<!-- /key --> again.

### `builder.menu`

Opens this pane's menu: every command the pane can do in its present mode, each line spelled with
what it would act on (`` build `tally` ``, `` load the built `tally` now ``).

- **Takes:** the mode the pane is in. Default key <!-- key builder.menu -->`shift+m`<!-- /key -->,
  except on the role line, where a capital letter is typed; the control `[menu]`, always first; a
  right press on a row.
- **Answers:** the menu, presented by Workshop's menu presenter. A line that names an artifact or a
  build acts only on that one.
- **Refuses:** a line whose subject moved meanwhile, `` `tally` is not what is here now -- aim again
  ``; a line from a menu about something else, `that menu was about something else -- nothing was
  done`.
- **Example:** press <!-- key builder.menu -->`shift+m`<!-- /key --> and choose `` build `tally` ``.

### `builder.arm`

Turns load-after-build on or off. While it is on, every build that works is offered to the running
project. It stays where you leave it until Workshop quits, and through a reload of the pane.

- **Takes:** the switch. No default key; the control `[turn load-after-build on]` or `[turn
  load-after-build off]`.
- **Answers:** `load after build: on -- the next build is offered to the running project when it
  works`, or `load after build: off -- the next build is a plain build`; while nothing is offered yet,
  the `realize` row reads `[x] load after build`.
- **Refuses:** `` `tally` is built and waiting -- load it, or build again to arm the next one ``,
  while a finished build waits to be loaded.
- **Example:** press the control `[turn load-after-build on]`, then
  <!-- key builder.build -->`b`<!-- /key -->.

### `builder.load-built`

Loads the build that finished and that nothing loaded, even if you have chosen another recipe since:
it asks for that build's recipe again with the load aboard, and the build tool confirms it is up to
date.

- **Takes:** the finished, unloaded build the `realize` row names. No default key; the control
  `[load built tally]`.
- **Answers:** sends [`BuildRequested`](zengine.builder.md#buildrequested) for that build's recipe
  with the load aboard; the notice says `` loading the built `tally` now -- Workshop stays live while
  the incremental build confirms it ``, then the outcome, `already up to date`, and the project's
  answer, as [`builder.build`](#builderbuild).
- **Refuses:** `nothing built is waiting to be loaded`; `load after build is already on -- the next
  build is offered`; `` `tally` was not loaded -- it is not what is built and waiting now `` when
  another build finished between your aim and your press; and the refusals every act shares.
- **Example:** press the control `[load built tally]`.

## Commands in the output reader

These replace the pane's commands while [`builder.output`](#builderoutput) is open; none of them
reaches a build. [`builder.menu`](#buildermenu) stays.

### `builder.output-up`

One line up.

- **Takes:** the reader. Default key <!-- key builder.output-up -->`up`<!-- /key -->; the wheel moves
  three lines a notch; the control `[up]`.
- **Answers:** the page moves; moving up into lines the Builder did not keep lands on the line before
  them.
- **Refuses:** nothing; at the first line nothing moves.
- **Example:** <!-- key builder.output-up -->`up`<!-- /key -->.

### `builder.output-down`

One line down.

- **Takes:** the reader. Default key <!-- key builder.output-down -->`down`<!-- /key -->; the control
  `[down]`.
- **Answers:** the page moves; moving down into lines the Builder did not keep lands after them.
- **Refuses:** nothing; at the last line nothing moves.
- **Example:** <!-- key builder.output-down -->`down`<!-- /key -->.

### `builder.output-first`

The first line.

- **Takes:** the reader. Default key <!-- key builder.output-first -->`home`<!-- /key -->; the control
  `[first line]`.
- **Answers:** the page from line 1.
- **Refuses:** nothing.
- **Example:** <!-- key builder.output-first -->`home`<!-- /key -->.

### `builder.output-last`

The last lines; the reader follows new ones while the build is still writing.

- **Takes:** the reader. Default key <!-- key builder.output-last -->`end`<!-- /key -->; the control
  `[last lines]`.
- **Answers:** the page that ends at the last line.
- **Refuses:** nothing.
- **Example:** <!-- key builder.output-last -->`end`<!-- /key -->.

### `builder.output-left`

Pans left by half the pane's width, for a long line.

- **Takes:** the reader. Default key <!-- key builder.output-left -->`left`<!-- /key -->; the control
  `[pan left]`.
- **Answers:** the rows shift, and the header says from which column.
- **Refuses:** nothing; at the first column nothing moves.
- **Example:** <!-- key builder.output-left -->`left`<!-- /key -->.

### `builder.output-right`

Pans right by half the pane's width.

- **Takes:** the reader. Default key <!-- key builder.output-right -->`right`<!-- /key -->; the
  control `[pan right]`.
- **Answers:** the rows shift.
- **Refuses:** nothing.
- **Example:** <!-- key builder.output-right -->`right`<!-- /key -->.

### `builder.output-older`

Reads the build before this one, among the builds the Builder still keeps (its last few).

- **Takes:** the reader. Default key <!-- key builder.output-older -->`[`<!-- /key -->; the control
  `[older build]`.
- **Answers:** the reader binds to that build.
- **Refuses:** `no older build's output is kept`.
- **Example:** <!-- key builder.output-older -->`[`<!-- /key -->.

### `builder.output-newer`

Reads the build after this one.

- **Takes:** the reader. Default key <!-- key builder.output-newer -->`]`<!-- /key -->; the control
  `[newer build]`.
- **Answers:** the reader binds to that build.
- **Refuses:** `no newer build's output is kept`.
- **Example:** <!-- key builder.output-newer -->`]`<!-- /key -->.

### `builder.output-close`

Closes the reader; the build rows come back.

- **Takes:** the reader. Default key <!-- key builder.output-close -->`escape`<!-- /key -->; the
  control `[close output]`.
- **Answers:** the pane's own commands again.
- **Refuses:** nothing.
- **Example:** <!-- key builder.output-close -->`escape`<!-- /key -->.

## Commands in the recipe list

These replace the pane's commands while [`builder.recipes`](#builderrecipes) is open;
[`builder.edit-source`](#builderedit-source) and [`builder.menu`](#buildermenu) stay.

### `builder.recipes-up`

Moves the list's cursor up one row; your choice does not move.

- **Takes:** the list. Default key <!-- key builder.recipes-up -->`up`<!-- /key -->; a first press on
  a row, or the wheel.
- **Answers:** the cursor moves; it stops at the first row.
- **Refuses:** nothing.
- **Example:** <!-- key builder.recipes-up -->`up`<!-- /key -->.

### `builder.recipes-down`

Moves the list's cursor down one row.

- **Takes:** the list. Default key <!-- key builder.recipes-down -->`down`<!-- /key -->.
- **Answers:** the cursor moves; it stops at the last row.
- **Refuses:** nothing.
- **Example:** <!-- key builder.recipes-down -->`down`<!-- /key -->.

### `builder.recipe-choose`

Makes the row under the cursor your chosen recipe, and closes the list.

- **Takes:** the cursor's row. Default key <!-- key builder.recipe-choose -->`return`<!-- /key -->;
  the control `[choose this recipe]`, or a second press on the cursor's row.
- **Answers:** `build recipe: tally -> tally`.
- **Refuses:** `that recipe is not in the catalog any more -- nothing was chosen`.
- **Example:** <!-- key builder.recipe-choose -->`return`<!-- /key -->.

### `builder.recipes-close`

Closes the list and leaves your choice as it was.

- **Takes:** the list. Default key <!-- key builder.recipes-close -->`escape`<!-- /key -->; the
  control `[close the list]`.
- **Answers:** `the list is closed -- the choice is unchanged`.
- **Refuses:** nothing.
- **Example:** <!-- key builder.recipes-close -->`escape`<!-- /key -->.

## Commands on the role line

While [`builder.load`](#builderload)'s line is open, every other key is a character for it.

### `authoring.commit`

Adds the row `{ artifact, role }` to the running project, then writes it to the project's plan file
(`workshop-plan.json` in the directory Workshop was launched in). When the new row is what the
project waits on and the artifact is already built, it loads it now.

- **Takes:** the role typed. Default key <!-- key authoring.commit -->`return`<!-- /key -->; the
  control `[load it with this role]`.
- **Answers:** the project's answer and where the row was written: `` loaded `tally` as example.tally
  -- pending -- the project is waiting on it; nothing is built yet -- the frontier action builds and
  loads it ``, then [`builder.frontier`](#builderfrontier) builds it. When it loads a built artifact,
  the notice then follows that build and its load, as [`builder.build`](#builderbuild).
- **Refuses:** `a weave declaration needs a role -- nothing was loaded`; the project's refusals, as
  `not loaded: realization is not between rows: it is still performing the authored plan`. A row
  the project refuses is not written.
- **Example:** type `example.tally`, press <!-- key authoring.commit -->`return`<!-- /key -->.

### `authoring.cancel`

Closes the line; nothing is loaded and nothing is written.

- **Takes:** the line. Default key <!-- key authoring.cancel -->`escape`<!-- /key -->; the control
  `[cancel -- load nothing]`.
- **Answers:** `nothing was loaded and nothing was written`.
- **Refuses:** nothing.
- **Example:** <!-- key authoring.cancel -->`escape`<!-- /key -->.

## Follow

The pane's rows follow the Builder office's `BuildStatus`, and the `project` row the project's
frontier. An observer follows the same owners instead of reading the pane: `BuildAsked` and
`BuildStatus` from `zengine.builder` ([Follow](zengine.builder.md#follow)), and `RealizationAsked`,
`ArtifactRealized` and `ArtifactPromoted` from `zengine.realization`. The rows that move are `last`,
`realize` and `said`.

## Shows

### `recipe`

The chosen recipe, the artifact it makes and its place in the file: `tally -> tally  (1/2)`; `(this
project has no build recipes)` when there are none.

### `project`

Only while the project waits: what on, which recipe makes it, and how many rows wait behind it,
`waiting tally (tally, blocks 0)`.

### `last`

How the last build is going or went, its operation number and how many pieces of output arrived,
`running -- op #1, 12 out`; after `FAILED`, `NO ARTIFACT` or `did not start` it ends `-- read output`.

### `exit`

The last exit status when a build succeeded or failed, `--` otherwise, and how many asks the Builder
has taken, `asks 3 ever`.

### `ran`

The command the build ran; `(nothing has run yet)` before the first.

### `realize`

Load-after-build, `[x] load after build`; a finished build waiting to be loaded; or what the project
made of the last offer, `realized -- op #2, NOT DEFAULT (promote / revert) -- <the owner's words>`.

### `said`

The last lines the build said; [`builder.output`](#builderoutput) reads all of them.

## Files

The pane reads no file. The recipe file it shows is the Builder office's
([Files](zengine.builder.md#files)); [`authoring.commit`](#authoringcommit) writes the plan through
the project's plan office. A built artifact is kept beside Workshop, as `<stem>.so` or `<stem>.dll`,
with each reload's copy under `<stem>.reloads/`; [`builder.load`](#builderload) loads one an earlier
run built.

## See also

- [`zengine.builder`](zengine.builder.md), the office that builds.
- [The recipe file](recipes.md): what a recipe holds, and where its artifact lands.
- [Build your first pane](tutorials/build-your-first-pane/README.md), and
  [edit a running pane](edit-a-running-pane.md).
