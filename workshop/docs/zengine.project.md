# zengine.project -- the project office

**Manual.** Workshop's read-only office about the project this run is: where it began, where the
Files pane keeps its places, what the running project waits on, whether its plan names an artifact,
and which file a recipe was written from. It is a door inside Workshop, answering to the office
`zengine.project`; it is not a file you load. A pane reaches it by the messages below.

## About

Each answer is derived when it is asked, from the owner of that fact in Workshop: the launch, the
load plan the realization owner performs, the recipe file in force. The office keeps nothing
between asks and runs nobody's code, so asking it changes nothing. Changing the project goes to
the offices that act: [`zengine.recipes`](zengine.recipes.md) for recipes and
[`zengine.plan`](zengine.plan.md) for the plan.

## Use

Ask as an office: a weave that sends as one of the offices it holds is answered, to it alone; an ask
sent as nobody (personal speech, or a host's root) is counted and answered by no one. The Files
pane asks where the project is; the Builder pane asks the rest.

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

## Files

The office reads no file itself. The places file it names is the Files pane's, kept as
`workshop-marks.json` in Workshop's state folder unless `--marks <path>` names another, and none in
an isolated run.

## See also

- [`zengine.recipes`](zengine.recipes.md) and [`zengine.plan`](zengine.plan.md), the offices that
  change the project.
- [Choosing what a run is made of](load-plans.md) and [the authored load plan](load-plan.md).
