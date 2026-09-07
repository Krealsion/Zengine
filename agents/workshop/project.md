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
`install_recipes`; `workshop/weave.hpp` `RecipeSource`; `workshop/persist.hpp` `resolved_against`;
`tests/test_workshop_files.cpp` case `"EDIT-1: a relative recipe source is the PROJECT's file, in
the editor and in the build"`, case `"EDIT-1: the editor opens the file that recipe's build would
compile"`, case `"PROJ-1: a catalog's own directory is not a source base"`.
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
- a dirty Editor buffer over that path is neither consumed nor auto-saved; no Builder needed;
- a foreign catalog's relative `single_source` still names a file under the active project.

PROVEN BY — `workshop/files_doors.hpp` `RecipesDoor`; `workshop/files_seam_vocabulary.hpp`
`RecipeUseRequested`, `RecipeOutcome`; `workshop/weave.hpp` `HostContext::use_recipes`,
`RecipeSwap`;
`tests/test_workshop_files.cpp` case `"PROJ-1: a maker chooses a catalog in Files and every
consumer moves with it"`, case `"PROJ-1: selecting the catalog already in force is a reload, not a
no-op"`, case `"PROJ-1: recipes come from the saved file, never from an unsaved editor buffer"`,
case `"PROJ-1: the chooser does not need the Builder panel to be open"`, case `"PROJ-2: an
external catalog is chosen live, and the project still owns relative sources"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-07 — Standing Builder intent survives by recipe identity, never by row position

LAW — `on(RecipeCatalog)` follows the chosen recipe by name to its new row, `picked` intact, and releases it when the identity is gone; no fallback to an index, stem or nearest name.

PROVEN BY — `workshop/weave.hpp` `RecipeCatalog`; `workshop/weave_pointer.cpp`
`on(RecipeCatalog)`; `workshop/panel.hpp` `BuilderPane::picked`; `tests/test_workshop_panels.cpp`
case `"PROJ-1: a reordered catalog moves the maker's choice to its recipe, not its row"`, case
`"PROJ-1: a choice whose recipe is gone is cleared, not handed to its neighbour"`, case `"PROJ-1:
an emptied catalog leaves no selection standing"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-09 — Which catalog is in force is the owner's answer, and nobody keeps a copy

LAW — The catalog in force is read back from the owner after every attempt and never recomposed from a candidate; the party that made the swap says so once, and no projection of it stands.

MEANS
- a refused swap answers with the catalog STILL running, which is the half a maker needs;
- the Builder panel named it while a session projection existed; that projection has left.

PROVEN BY — `workshop/workshop.cpp` `use_recipes`; `workshop/weave.hpp` `RecipeSwap`;
`files/files.hpp` `catalog_taken_words`, `catalog_refused_words`; `tests/test_files.cpp` case
`"PROJ-1: a refusal says what went wrong AND what is still running, in that order"`;
`tests/test_workshop_files.cpp` case `"PROJ-1: a live catalog choice is this session's and is
written nowhere"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-10 — A path is not a sentence

LAW — `detail::fit_path` measures a path a maker has to recognize: a root cue (`path_root_cue`, lexical), a mark where the middle went, the tail cut at a component boundary.

MEANS
- a sentence front-loads its meaning and a path back-loads it: they are cut at opposite ends;
- it changes no stored identity, and no pane widens to avoid a cut.

PROVEN BY — `workshop/screen_bindings.cpp` `fit_path`, `path_root_cue`;
`workshop/screen_layouts.cpp` `fit_path`; `tests/test_workshop_files.cpp` case
`"PROJ-2: fitting a path keeps the end that says which file it is"`;
`tests/test_workshop_screen.cpp` case `"WUX-7: four things must agree before a row is scrolled at
all"`.
WHY — `agents/decisions/a-path-is-not-a-sentence.md`

## WL-PROJ-11 — The Builder panel announces only what it watched

LAW — The panel's `awaiting` latch is set when it asks and released only at an outcome the build will not leave (`still_going`), so an arriving status is news exactly when this panel watched the build begin.

MEANS
- `heard` tells "the tool has not answered" from "the tool never built anything";
- a panel opened while a child is alive is told `running`, shows it, and announces nothing;
- `awaiting_realization` is the twin latch, held longer; `chosen` is bounded at use, not at write.

PROVEN BY — `workshop/panel.hpp` `BuilderPane`, `BuilderPane::heard`, `BuilderPane::awaiting`,
`BuilderPane::awaiting_realization`, `BuilderPane::chosen`; `workshop/weave_pointer.cpp`
`on(BuildStatus)`; `builder/vocabulary.hpp` `still_going`; `tests/test_workshop_panels.cpp` case
`"a panel opened mid-build is TOLD it is running, and announces nothing"`, case `"a running build
is on the panel, with its operation and its output count"`, case `"BLD-1: a build outcome and a
realization outcome are TWO rows and TWO notices"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-12 — The tool's status is kept only while a panel presents it

LAW — A `BuildStatus` with no Builder panel open is not remembered; closing the panel destroys its copy and reaches no tool, and reopening asks again and is answered with the tool's own running total.

MEANS
- a copy kept against a later panel makes a presentation a second owner of somebody else's facts.

PROVEN BY — `workshop/weave_pointer.cpp` `on(BuildStatus)`; `workshop/panel.hpp` `close_panel`;
`tests/test_workshop_panels.cpp` case `"closing forgets the panel's copy; the TOOL keeps its own
count"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-13 — A build is asked for by the tool's name, with the realize intention beside it

LAW — Workshop holds no target, recipe or command: `build_now` names the row under the maker's cursor, refuses in words with no answer or no recipes yet, and says `realize` from the armed toggle.

MEANS
- with no Builder panel open the key is unbound; a panel that has not heard cannot ask;
- everything after the send belongs to the tool, the runner and the realization owner.

PROVEN BY — `workshop/weave_arrange.cpp` `build_now`; `tests/test_workshop_panels.cpp` case
`"Build asks for the name the TOOL gave, and asks for nothing without one"`, case `"a panel that
has not heard from its tool cannot ask for a build"`, case `"BLD-1: `b` builds the recipe the
maker chose, not the one last built"`, case `"BLD-1: armed by `Shift+b`, `b` is BUILD & REALIZE,
and the second intention crosses the seam"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-14 — The frontier build is one comparison, and never chooses for the maker

LAW — `f` compares the live frontier artifact with each catalog row's, once, and sends `build_now` with the realize intention; several producers refuse unless the maker's standing pick is one of them.

MEANS
- the catalog's order is nobody's intent: the refusal names the candidates; the pick is `c`'s;
- `picked` tells an explicit pick from `chosen`'s default of 0, an index and not a choice;
- there is no second build path, no direct load and no new sentence on the bus.

PROVEN BY — `workshop/weave_arrange.cpp` `build_frontier`; `workshop/panel.hpp`
`BuilderPane::picked`; `tests/test_workshop_panels.cpp` case `"BLD-2: `f` builds and realizes the
ONE recipe that produces the frontier"`, case `"BLD-2: several recipes produce the frontier -- `f`
never chooses for the maker"`.
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
`PlanExecutor::revert`, `reload_refusal_words`; `workshop/weave.hpp` `build_realize`,
`promote_image`, `revert_image`; `workshop/weave_arrange.cpp` `build_realize`, `promote_image`,
`revert_image`; `workshop/panel.hpp` `BuilderPane::arm`; `tests/test_workshop_files.cpp` case
`"RELOAD-1: a single-source recipe's product lands in its workspace, never on the loaded path"`;
`tests/test_workshop_load.cpp` case `"RELOAD-1: a live weave-only row reloads in place -- same
WeaveId, state kept, Ack settles it"`, case `"RELOAD-1: promote writes the running image into the
plan's file, sibling then rename, and revert reloads the image before the last reload"`;
`tests/test_workshop_panels.cpp` case `"RELOAD-2: `B` before a build is a toggle: armed, `b` asks
to build AND load"`, case `"RELOAD-2: after a plain build that succeeded and nothing armed, `B` is
a button that loads the built artifact now"`.
WHY — `agents/decisions/a-reload-lands-off-the-loaded-path.md`

## Do not assume

- That a running build follows a replaced catalog — it does not, and that has no witness yet
  (WL-PROJ-03).

