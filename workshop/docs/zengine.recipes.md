# zengine.recipes -- the recipes office

**Manual.** Workshop's office that changes which build recipes this project means: it can make
another recipe file the one in force, or add a recipe to it. It is a door inside Workshop,
answering to the office `zengine.recipes`; it is not a file you load. The Files pane reaches it when
you choose a recipe file or write a recipe.

## About

A *recipe* says how one file is made, and the recipe file in force is the list the Builder builds
from ([the recipe file](../../builder/docs/recipes.md)). Every judgement about a file's bytes and a
recipe's fields is the recipe owner's inside Workshop: this office passes on what was asked and
answers with what came of it, in the owner's own words, never reworded. Where the project is belongs
to [`zengine.project`](zengine.project.md); which artifacts run, to
[`zengine.plan`](zengine.plan.md).

## Use

Ask as an office: an ask sent as nobody is counted and answered by no one. Both commands answer
`RecipeOutcome` v1 to the asker alone: `accepted`, or `refusal`, the owner's words for why not; then
`path`, the recipe file in force after the attempt, and how many `recipes` it holds. A refused act
leaves the file in force exactly as it was.

## Commands

### `RecipeUseRequested`

`RecipeUseRequested` v1 makes the recipe file at a path the one in force for this run.

- **Takes:** `path`, the file to use.
- **Answers:** `RecipeOutcome`: the file read, judged whole, and installed as the one in force.
- **Refuses:** `this host cannot change recipe catalogs`; the owner's refusal of the file, as a file
  that cannot be read or a recipe the recipe law refuses, naming it.
- **Example:** the Files pane's `use as recipes` on a `build-recipes.json`.

### `RecipeAuthorRequested`

`RecipeAuthorRequested` v2 adds one recipe from what you typed, written into the project's recipe
file and installed.

- **Takes:** `id` and `artifact`; for a one-file recipe `source`, `packages` and `links`; for a
  CMake target `build_dir`, `target`, `artifact_dir`, and `config` when the tree has several; and
  `tree`, which of the two kinds this is.
- **Answers:** `RecipeOutcome`. The row goes into the recipe file in force when it is your own;
  otherwise into `build-recipes.json` in the project, which starts from the shipped rows when it is
  new. The file is saved whole, then installed.
- **Refuses:** `this host cannot author recipes`; `this run began nowhere -- a recipe needs a
  project to be written into`; `the catalog in force could not be read back: <why>`; the recipe
  law's words for the row or the file; `written to <path>, but this host cannot install it`.
- **Example:** the Files pane's `write recipe`, `{id: "tally", artifact: "tally", source:
  "tally.cpp", tree: false}`.

## Files

Reads and writes the recipe file in force, or the project's `build-recipes.json`; the format is
[the recipe file](../../builder/docs/recipes.md)'s.

## See also

- [`zengine.project`](zengine.project.md) and [`zengine.plan`](zengine.plan.md).
- [`zengine.builder`](../../builder/docs/zengine.builder.md), which builds from the file in force.
