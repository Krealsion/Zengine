# Workshop law — the project

Register `WL-PROJ`: the project anchor and the recipe catalog. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md).

## WL-PROJ-01 — The project is the launch directory, captured once

LAW — The project is the launch directory, captured once by the host; it is not the install directory, and nothing derives it from that, from `--document`, from `--recipes` or from a prefix.

MEANS
- empty is the designed absence, said on the banner; there is no `--project`;
- two roads reach the absence (no reportable directory, or one this build cannot carry): one fact.

PROVEN BY — `workshop/weave.hpp` `HostContext::project_dir`, `HostContext::dir`;
`workshop/path_admission.hpp` `launch_project_dir`; `workshop/workshop.cpp` `launch_project_dir`;
`tests/test_workshop_files.cpp` case `"QR-12: the launch capture is the working directory, when it
can be said"`, case `"QR-12: a launch directory this Workshop cannot say is an absence, not an
exit"`, case `"SOURCE-0: zengine.project.anchor answers the owner's anchor, absence included"`.
WHY — `agents/decisions/project-is-several-mechanisms.md`

## WL-PROJ-02 — A relative source is the project's file, in the editor and in the build

LAW — Completion is the one place a host fact enters a recipe — the artifact directory and the workspace from the install, a relative source against the project — and it runs once per install.

MEANS
- completion never rewrites the recipe file; only a maker's own act appends a row (WL-AUTH-01);
- the falsifier: the project and the workspace both hold `src/example.cpp` with different bytes.

PROVEN BY — `workshop/recipe_persist.hpp` `WorkshopRecipe::artifact_dir`,
`WorkshopSingleSource::workspace`, `complete_recipes`; `workshop/recipes.hpp` `complete_recipes`,
`install_recipes`; `workshop/weave.hpp` `RecipeSource`, `HostContext::recipe_source`;
`workshop/persist.hpp` `resolved_against`;
`tests/test_workshop_files.cpp` case `"EDIT-1: a relative recipe source is the PROJECT's file, in
the editor and in the build"`, case `"EDIT-1: the project door names the file that recipe's build
would compile"`, case `"PROJ-1: a catalog's own directory is not a source base"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-03 — The completed catalog has one session owner, and every consumer reads it

LAW — One session owner holds the completed recipes, its views and the file they came from; holding a new catalog assigns into members it owns, so a replacement changes contents, never the bound objects.

MEANS
- `main` declares it above the bus, the Kernel and every weave; that order is the lifetime proof;
- the two build weaves take `const&` and refuse an rvalue; `recipe_source` asks at the gesture;
- the owner is not authorship: nothing completes host paths back into the recipe file.

DOES NOT MEAN
- that a running build re-aims when the catalog changes — it resolved its artifact at accept.

PROVEN BY — `workshop/recipes.hpp` `CurrentRecipes`, `RecipeView`, `CurrentRecipes::hold`;
`workshop/workshop.cpp` `BuildRunnerWeave`, `BuilderWeave`; `builder/weave.hpp`
`BuilderWeave::path_`, `BuilderWeave::before_`; `workshop/load_execute.hpp` `AwaitingBuild`;
`tests/test_workshop_files.cpp` case `"PROJ-0: the owner derives the tool's view from the recipes
it is holding"`, case `"PROJ-0: holding a new catalog replaces the contents, never the object"`,
case `"PROJ-0: the host's edit-source answer is asked of the owner, not of a copy"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-04 — `install_recipes` is the one seam that turns a file into the answer

LAW — Read, parse, complete, hold, every step on a candidate, so a refusal at any stage leaves all three answers as they were; the source path is a parameter of `hold()`, not a setter.

MEANS
- `main` wires `HostContext::use_recipes` over it and installs its own startup catalog through it;
- a valid empty catalog installs: a project with nothing to build is a project;
- completion is total and adds no third refusal kind beside the reader's and the parser's.

DOES NOT MEAN
- that a catalog has another road in — `install_recipes` is the one seam, at startup and live.

PROVEN BY — `workshop/recipes.hpp` `install_recipes`, `CurrentRecipes::hold`;
`workshop/weave.hpp` `HostContext::use_recipes`; `workshop/workshop.cpp` `install_recipes`,
`use_recipes`, `Arguments::recipes`; `workshop/recipe_persist.hpp` `from_text`;
`tests/test_workshop_files.cpp` case `"PROJ-0/PROJ-1: one completed catalog, installed through one
seam"`, case `"PROJ-1: installing a catalog moves its source, its rows and its views together"`,
case `"PROJ-1: a candidate that cannot be read installs nothing at all"`, case `"PROJ-1: a valid
EMPTY catalog is a replacement, not a failure"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-05 — A live catalog choice is one door, and the authoring one spends it

LAW — `u` in Project Files resolves a row to a path as activation does, refuses a directory and an uncarriable name, and hands it to `use_recipes`; every judgement about the bytes is the recipe owner's.

MEANS
- same-path is a reload and never a no-op: that is the application's whole live-refresh mechanism;
- a dirty Editor buffer is neither consumed nor auto-saved; no Builder pane need be loaded;
- a foreign catalog's relative `single_source` still names a file under the active project.

PROVEN BY — `workshop/pane_doors.hpp` `RecipesDoor`; `workshop/pane_seam_vocabulary.hpp`
`RecipeUseRequested`, `RecipeOutcome`; `workshop/weave.hpp` `HostContext::use_recipes`,
`RecipeSwap`;
`tests/test_workshop_files.cpp` case `"PROJ-1: a maker chooses a catalog in Files and every
consumer moves with it"`, case `"PROJ-1: selecting the catalog already in force is a reload, not a
no-op"`, case `"PROJ-1: the chooser needs no Builder pane loaded at all"`, case `"PROJ-2: an
external catalog is chosen live, and the project still owns relative sources"`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W49: recipes come from the saved file, never
from an unsaved Editor buffer"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-07 — Standing Builder intent survives by recipe identity, never by row position

LAW — The Builder pane holds the chosen recipe as a NAME, so a republished catalog moves the choice with it and an identity that is gone releases it; no fallback to an index, stem or nearest name.

MEANS
- the name is the pane's durable state, so a reload keeps a choice an index would have lost;
- `picked` stays the pane's own: how a selection was made is not changed by a reordering.

PROVEN BY — `builder-pane/vocabulary.hpp` `BuilderPaneState::chosen`; `builder-pane/pane.cpp`
`named_row`, `cursor_row`; `tests/test_workshop_panes_builder.cpp`
case `"BLD-WEAVE: PROJ-1 -- the choice follows its RECIPE to a new row, not its index"`, case
`"BLD-WEAVE: PROJ-1 -- a choice whose recipe is gone is released, not inherited"`, case
`"BLD-WEAVE: an empty catalog is said plainly, and `b` asks for nothing"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-09 — Which catalog is in force is the owner's answer, and nobody keeps a copy

LAW — The catalog in force is read back from the owner after every attempt and never recomposed from a candidate; the party that made the swap says so once, and no projection of it stands.

MEANS
- a refused swap answers with the catalog STILL running, which is the half a maker needs;
- the Builder pane names it again, from `RecipeCatalog` v2's `source`, on the owner's word;

PROVEN BY — `workshop/workshop.cpp` `use_recipes`; `workshop/weave.hpp` `RecipeSwap`;
`files/files.hpp` `catalog_taken_words`, `catalog_refused_words`; `builder/vocabulary.hpp`
`RecipeCatalog`; `tests/test_files.cpp` case
`"PROJ-1: a refusal says what went wrong AND what is still running, in that order"`;
`tests/test_workshop_files.cpp` case `"PROJ-1: a live catalog choice is this session's and is
written nowhere"`; `tests/test_workshop_panes_builder.cpp` case `"BLD-WEAVE: P-WORK-20 -- the
pane names the catalog in force, from RecipeCatalog v2"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-10 — A path is not a sentence

LAW — `detail::fit_path` measures a path a maker has to recognize: a root cue (`path_root_cue`, lexical), a mark where the middle went, the tail cut at a component boundary.

MEANS
- a sentence front-loads its meaning and a path back-loads it: they are cut at opposite ends;
- it changes no stored identity, and no pane widens to avoid a cut.

PROVEN BY — `workshop/screen_bindings.cpp` `fit_path`, `path_root_cue`;
`workshop/screen_layouts.cpp` `fit_path`; `tests/test_workshop_files.cpp` case
`"PROJ-2: fitting a path keeps the end that says which file it is"`.
WHY — `agents/decisions/a-path-is-not-a-sentence.md`

## WL-PROJ-11 — The Builder pane announces only what it watched

LAW — The pane's `awaiting` latch is set when it asks and released only at an outcome the build will not leave (`still_going`), so an arriving status is news exactly when this pane watched the build begin.

MEANS
- `heard` tells "the tool has not answered" from "the tool never built anything";
- a pane granted room while a child is alive is told `running`, shows it, and announces nothing;
- `awaiting_realization` is the twin latch, held longer, and none of them is durable state.

PROVEN BY — `builder-pane/pane.cpp` `on(builder::BuildStatus)`, `build_words`, `realize_words`;
`builder-pane/vocabulary.hpp` `BuilderPaneState`; `builder/vocabulary.hpp` `still_going`;
`tests/test_workshop_panes_builder.cpp` case
`"BLD-WEAVE: the pane asks the tool what it is on its own room grant, and shows it"`, case
`"BLD-WEAVE: RELOAD-2 -- after a plain build that worked, `B` is the button"`, case
`"BLD-WEAVE: closing the pane forgets its copy; the TOOL keeps its own count"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-12 — The tool's status is kept only while a pane presents it

LAW — The Builder pane's picture of the tool is a member, never durable state; a removed pane is granted no room, and the next room grant asks again and hears the tool's own running total.

MEANS
- a copy kept against a later opening makes a presentation a second owner of somebody's facts;
- closing reaches no tool: it retracts no offer, sends no unload and changes nothing.

PROVEN BY — `builder-pane/vocabulary.hpp` `BuilderPaneState`; `builder-pane/pane.cpp`
`on(PaneRoom)`, `ask_status`; `tests/test_workshop_panes_builder.cpp` case
`"BLD-WEAVE: closing the pane forgets its copy; the TOOL keeps its own count"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-13 — A build is asked for by the tool's name, with the realize intention beside it

LAW — The Builder pane holds no target, recipe or command: `build_now` names the row the maker chose, refuses in words with no answer or no recipes yet, and says `realize` from the armed toggle.

MEANS
- the row is the PANE's and acts only while it holds the keyboard: elsewhere it reaches nobody;
- everything after the send belongs to the tool, the runner and the realization owner.

PROVEN BY — `builder-pane/pane.cpp` `build_now`, `send_build`, `has_recipe`;
`builder-pane/vocabulary.hpp` `kActionBuild`; `tests/test_workshop_panes_builder.cpp` case
`"BLD-WEAVE: `b` builds only after the maker has pressed into the pane"`, case
`"BLD-WEAVE: `b` builds the recipe the maker chose, by name"`, case
`"BLD-WEAVE: an empty catalog is said plainly, and `b` asks for nothing"`, case
`"BLD-WEAVE: RELOAD-2 -- `B` before a build is the toggle, and `b` reads it"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-14 — The frontier build is one comparison, and never chooses for the maker

LAW — `f` ASKS the host for the frontier, compares that artifact with each catalog row's once, and sends `build_now` with the realize intention; several producers refuse unless a standing pick is one.

MEANS
- the catalog's order is nobody's intent: the refusal names the candidates; the pick is `c`'s;
- `picked` tells an explicit pick from the catalog's own first row, which is nobody's choice;
- there is no second build path, no direct load and no new sentence on the bus.

PROVEN BY — `builder-pane/pane.cpp` `begin_frontier_build`, `finish_frontier_build`;
`workshop/pane_doors.hpp` `ProjectDoor`; `workshop/builder_seam_vocabulary.hpp`
`ProjectFrontierRequested`, `ProjectFrontierSaid`; `tests/test_workshop_panes_builder.cpp` case
`"BLD-WEAVE: BLD-2 -- the frontier row comes from the host's read-only door"`, case
`"BLD-WEAVE: BLD-2 -- `f` builds and realizes the one recipe that makes the frontier"`, case
`"BLD-WEAVE: BLD-2 -- `f` refuses in words, and never chooses between recipes"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-15 — The shipped catalog is staged beside the executable

LAW — The shipped catalog sits beside the executable under `kDefaultRecipesName`; `--recipes` wins, else `<project>/build-recipes.json` when present, else the shipped; no registry, picker or search path.

MEANS
- `recipes_in_force` is `plan_in_force`'s twin, decided by the host's one probe, never a default;
- an absent shipped default is "nothing to build"; an absent named file is a refusal.

PROVEN BY — `workshop/recipe_persist.hpp` `kDefaultRecipesName`, `recipes_in_force`;
`workshop/workshop.cpp` `exe_dir`, `Arguments::recipes`; `workshop/CMakeLists.txt`
`zengine_workshop_dir`; `tests/test_workshop_files.cpp` case `"the shipped catalog is staged
beside the executable, under the name the launch resolves"`, case `"the launch resolves the
catalog by one rule: --recipes, else the project catalog at the root, else the shipped default
beside the executable"`, case `"the project catalog at the captured root is the catalog in
force when no --recipes is given"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-16 — A live weave-only row reloads in place from an image off the loaded path

LAW — A single-source build lands in its workspace; the host stages the product to the plan's file (a first load) or off it (a reload); `B` is one action in two states; promote and revert are the owner's.

MEANS
- `complete_recipes` completes an empty single-source `artifact_dir` to `<workspace>/out`;
- the toggle arms `b`; the button re-sends the finished build's own recipe with `realize`;
- `StageArtifact` and `PromoteImage` are the host's; a promotion keeps what it writes over.

DOES NOT MEAN
- that a changed shape is replaced or migrated — the kernel refuses it and the words say so;
- that a provider+weave row is reloaded — refused until unmount-and-remount exists.

PROVEN BY — `workshop/recipe_persist.hpp` `complete_recipes`; `workshop/staging.hpp` `stage`,
`promote`; `workshop/load_execute.hpp` `PlanExecutor::reload`, `PlanExecutor::promote`,
`PlanExecutor::revert`, `reload_refusal_words`; `builder-pane/pane.cpp` `build_realize`,
`promote_image`, `revert_image`; `builder-pane/vocabulary.hpp` `BuilderPaneState::arm`;
`tests/test_workshop_files.cpp` case
`"RELOAD-1: a single-source recipe's product lands in its workspace, never on the loaded path"`;
`tests/test_workshop_load.cpp` case `"RELOAD-1: a live weave-only row reloads in place -- same
WeaveId, state kept, Ack settles it"`, case `"RELOAD-1: promote writes the running image into the
plan's file, sibling then rename, and revert reloads the image before the last reload"`;
`tests/test_workshop_panes_builder.cpp` case
`"BLD-WEAVE: RELOAD-2 -- `B` before a build is the toggle, and `b` reads it"`, case
`"BLD-WEAVE: RELOAD-2 -- after a plain build that worked, `B` is the button"`, case
`"BLD-WEAVE: RELOAD-2 -- `P` and `R` are one offer each, about the built artifact"`.
WHY — `agents/decisions/a-reload-lands-off-the-loaded-path.md`

## Do not assume

- That a running build follows a replaced catalog — it does not, and that has no witness yet
  (WL-PROJ-03).

