# zengine.project -- the project office

**Manual.** Workshop's office about the project this run is. It answers where the project began,
where the Files pane keeps its places, what the running project waits on, whether its plan names an
artifact, and which file a recipe was written from; and it changes the project: which recipe file is
in force, a recipe added to it, and a row added to the load plan. It is a door inside Workshop,
answering to the office `zengine.project`; it is not a file you load. A pane reaches it by the
messages below.

## About

Each reading is derived when it is asked, from the owner of that fact in Workshop: the launch, the
load plan the realization owner performs, the recipe file in force. The office keeps nothing between
asks, and a reading runs nobody's code, so asking one changes nothing.

Each act goes to the one owner of what it changes. A *recipe* says how one file is made, and the
recipe file in force is the list the Builder builds from
([the recipe file](../../builder/docs/recipes.md)); every judgement about a file's bytes and a
recipe's fields is the recipe owner's. The *load plan* says which artifacts a project runs and as
what ([choosing what a run is made of](load-plans.md)); a row added to it goes first to the running
project, whose realization owner, [`zengine.realization`](zengine.realization.md), takes it or
refuses it, and only a row it took is written to the file. The office passes on what was asked
and answers with what came of it, in the owner's own words, never reworded; a refused act leaves
the project exactly as it was.

## Use

Ask as an office: a weave that sends as one of the offices it holds is answered, to it alone; an ask
sent as nobody (personal speech, or a host's root) is counted and answered by no one. The Files
pane asks where the project is and uses or writes recipes; the Builder pane asks the rest and adds
plan rows.

Both recipe commands answer `RecipeOutcome` v1: `accepted`, or `refusal`, the owner's words for why
not; then `path`, the recipe file in force after the attempt, and how many `recipes` it holds.

## Commands

### `ProjectRootRequested`

`ProjectRootRequested` v1 asks where this run began and where its places live.

- **Takes:** nothing.
- **Answers:** `ProjectRoot` v1, to the asker alone: `project_dir`, the directory Workshop was
  launched in (empty for a run that began nowhere), and `marks_path`, the file the Files pane keeps
  its places in (empty for none).
- **Refuses:** an ask not sent as an office has no answer.
- **Example:** the Files pane asks it each time Workshop gives it room.

### `ProjectFrontierRequested`

`ProjectFrontierRequested` v1 asks what the running project is stopped on: the row of its plan
whose artifact is not built, which the project waits at.

- **Takes:** nothing.
- **Answers:** `ProjectFrontierSaid` v1, to the asker alone: `waiting`, the `artifact` it waits on
  (empty when it waits on nothing) and how many authored rows wait behind it (`blocked`). A host
  that performs no plan answers `waiting` false.
- **Refuses:** an ask not sent as an office has no answer.
- **Example:** the Builder pane asks it on every `BuildStatus` and shows `waiting tally (tally,
  blocks 0)`.

### `PlanNamesRequested`

`PlanNamesRequested` v1 asks whether the plan in force already names an artifact.

- **Takes:** `stem`, the artifact's name (`tally`, never `tally.dll`).
- **Answers:** `PlanNames` v1, to the asker alone: the `stem` asked about and `named`, true when a
  row of the plan in force loads it.
- **Refuses:** an ask not sent as an office has no answer.
- **Example:** `{stem: "tally"}`, before the Builder pane opens its role line.

### `RecipeSourceRequested`

`RecipeSourceRequested` v1 asks which file a recipe was written from, by the recipe's name: the one
`.cpp` of a one-file recipe, or a CMake recipe's editing entry.

- **Takes:** `recipe`, the name the recipe file gives it.
- **Answers:** `RecipeSourceSaid` v1, to the asker alone: the `recipe` asked about and, when
  `accepted`, the one absolute `source`. Opening it is the Editor's, asked of
  [`zengine.opening`](zengine.opening.md).
- **Refuses:** in `refusal`: `this host resolves no recipe sources -- nothing was opened`; ``this
  project's recipes do not hold `tally` -- nothing was opened``; `` `skin-tui-block` is a
  cmake_target recipe -- it names no source file or editing entry to open ``.
- **Example:** the Builder pane's `edit source` asks `{recipe: "tally"}`.

### `RecipeUseRequested`

`RecipeUseRequested` v1 makes the recipe file at a path the one in force for this run.

- **Takes:** `path`, the file to use.
- **Answers:** `RecipeOutcome` v1, to the asker alone: the file read, judged whole, and installed as
  the one in force.
- **Refuses:** `this host cannot change recipe catalogs`; the owner's refusal of the file, as a file
  that cannot be read or a recipe the recipe law refuses, naming it.
- **Example:** the Files pane's `use as recipes` on a `build-recipes.json`.

### `RecipeAuthorRequested`

`RecipeAuthorRequested` v2 adds one recipe from what you typed, written into the project's recipe
file and installed.

- **Takes:** `id` and `artifact`; for a one-file recipe `source`, `packages` and `links`; for a
  CMake target `build_dir`, `target`, `artifact_dir`, and `config` when the tree has several; and
  `tree`, which of the two kinds this is.
- **Answers:** `RecipeOutcome` v1, to the asker alone. The row goes into the recipe file in force
  when it is your own; otherwise into `build-recipes.json` in the project, which starts from the
  shipped rows when it is new. The file is saved whole, then installed.
- **Refuses:** `this host cannot author recipes`; `this run began nowhere -- a recipe needs a
  project to be written into`; `the catalog in force could not be read back: <why>`; the recipe
  law's words for the row or the file; `written to <path>, but this host cannot install it`.
- **Example:** the Files pane's `write recipe`, `{id: "tally", artifact: "tally", source:
  "tally.cpp", tree: false}`.

### `PlanRowRequested`

`PlanRowRequested` v1 adds the row `{ artifact, role }` to the running project, then to its plan
file.

- **Takes:** `stem`, the artifact; `role`, the office it is loaded as; `recipe`, the recipe that
  makes it, so the answer can say whether that is built.
- **Answers:** `PlanRowWritten` v1, to the asker alone: `accepted`; `detail`, what the running
  project made of the row; `frontier`, true when the new row is what the project now waits on;
  `product`, where the recipe's file is when the row is the frontier and the file is already built;
  `path`, the plan file written.
- **Refuses:** in `refusal`: `this host cannot author plan rows`; the plan law's words for the row
  or the plan; `this run began nowhere -- a plan row needs a project to be written into`; `this host
  has no realization owner to append to`; the running project's refusal, as `realization is not
  between rows: it is still performing the authored plan`. A refused row is not written.
- **Example:** the Builder pane's role line, `{stem: "tally", role: "example.tally", recipe:
  "tally"}`.

## Files

The places file it names is the Files pane's, kept as `workshop-marks.json` in Workshop's state
folder unless `--marks <path>` names another, and none in an isolated run; the office never reads
it. The recipe acts read and write the recipe file in force, or the project's `build-recipes.json`,
in [the recipe file](../../builder/docs/recipes.md)'s format. A plan row is written to
`workshop-plan.json` in the directory Workshop was launched in, in
[the authored load plan](load-plan.md)'s format.

## See also

- [`zengine.realization`](zengine.realization.md), which performs the plan.
- [`zengine.builder`](../../builder/docs/zengine.builder.md), which builds from the recipe file in
  force.
- [Choosing what a run is made of](load-plans.md) and [the authored load plan](load-plan.md).
