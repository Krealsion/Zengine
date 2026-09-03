# Workshop law — the project

Register `WL-PROJ`: the project anchor and the recipe catalog. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md).

## WL-PROJ-01 — The project is the launch directory, captured once

LAW — The project is the launch directory, captured once by the host; it is not the install directory, and nothing derives it from that, from `--document`, from `--recipes` or from a prefix.

MEANS
- empty is the designed absence, said on the banner; there is no `--project`;
- two roads reach the absence (no reportable directory, or one this build cannot carry): one fact.

PROVEN BY — `workshop/weave.hpp` `project_dir`, `dir`; `workshop/path_admission.hpp`
`launch_project_dir`; `workshop/workshop.cpp` `launch_project_dir`;
`tests/test_workshop_files.cpp` case `"QR-12: the launch capture is the working directory, when
it can be said"`, case `"QR-12: a launch directory this Workshop cannot say is an absence, not
an exit"`, case `"SOURCE-0: zengine.project.anchor answers the owner's anchor, absence
included"`.
WHY — `agents/decisions/project-is-several-mechanisms.md`

## WL-PROJ-02 — A relative source is the project's file, in the editor and in the build

LAW — Completion is the one place a host fact enters a recipe — the artifact directory and the workspace from the install, a relative source against the project — and it runs once per install.

MEANS
- the recipe file is never rewritten;
- the falsifier: the project and the workspace both hold `src/example.cpp` with different bytes.

PROVEN BY — `workshop/recipe_persist.hpp` `artifact_dir`, `workspace`, `complete_recipes`;
`workshop/recipes.hpp` `complete_recipes`, `install_recipes`; `workshop/weave.hpp` `RecipeSource`;
`workshop/persist.hpp` `resolved_against`; `tests/test_workshop_files.cpp` case `"EDIT-1: a
relative recipe source is the PROJECT's file, in the editor and in the build"`, case `"EDIT-1: the
editor opens the file that recipe's build would compile"`, case `"PROJ-1: a catalog's own
directory is not a source base"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-03 — The completed catalog has one session owner, and every consumer reads it

LAW — One session owner holds the completed recipes, its views and the file they came from; holding a new catalog assigns into members it owns, so a replacement changes contents, never the bound objects.

MEANS
- `main` declares it above the bus, the Kernel and every weave; that order is the lifetime proof;
- the two build weaves take `const&` and refuse an rvalue; `recipe_source` asks at the gesture;
- the owner is not authorship: nothing completes host paths back into the recipe file.

DOES NOT MEAN
- that a running build re-aims when the catalog changes — it resolved its artifact at accept.

PROVEN BY — `workshop/recipes.hpp` `CurrentRecipes`, `RecipeView`, `hold`;
`workshop/workshop.cpp` `BuildRunnerWeave`, `BuilderWeave`; `builder/weave.hpp` `path_`,
`before_`; `workshop/load_execute.hpp` `AwaitingBuild`; `tests/test_workshop_files.cpp` case
`"PROJ-0: the owner derives the tool's view from the recipes it is holding"`, case `"PROJ-0:
holding a new catalog replaces the contents, never the object"`, case `"PROJ-0: the host's
edit-source answer is asked of the owner, not of a copy"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-04 — `install_recipes` is the one seam that turns a file into the answer

LAW — Read, parse, complete, hold, every step on a candidate, so a refusal at any stage leaves all three answers as they were; the source path is a parameter of `hold()`, not a setter.

MEANS
- `main` wires `HostContext::use_recipes` over it and installs its own startup catalog through it;
- a valid empty catalog installs: a project with nothing to build is a project;
- completion is total and adds no third refusal kind beside the reader's and the parser's.

DOES NOT MEAN
- that a catalog has another road in — `install_recipes` is the one seam, at startup and live.

PROVEN BY — `workshop/recipes.hpp` `install_recipes`, `hold`; `workshop/weave.hpp`
`use_recipes`; `workshop/workshop.cpp` `install_recipes`, `use_recipes`, `recipes`;
`workshop/recipe_persist.hpp` `from_text`; `tests/test_workshop_files.cpp` case `"PROJ-0/PROJ-1:
one completed catalog, installed through one seam"`, case `"PROJ-1: installing a catalog moves its
source, its rows and its views together"`, case `"PROJ-1: a candidate that cannot be read installs
nothing at all"`, case `"PROJ-1: a valid EMPTY catalog is a replacement, not a failure"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-05 — The first live chooser is `files.use-recipes`

LAW — `u` in Project Files resolves a row to a path as activation does, refuses a directory and an uncarriable name, and hands it to `use_recipes`; every judgement about the bytes is the recipe owner's.

MEANS
- same-path is a reload and never a no-op: that is the application's whole live-refresh mechanism;
- a dirty Editor buffer over that path is neither consumed nor auto-saved; no Builder needed;
- a foreign catalog's relative `single_source` still names a file under the active project.

PROVEN BY — `workshop/keymap.hpp` `files.use-recipes`; `workshop/weave.hpp` `files_use_recipes`,
`use_recipes`, `RecipeSwap`; `tests/test_workshop_files.cpp` case `"PROJ-1: a maker chooses a
catalog in Files and every consumer moves with it"`, case `"PROJ-1: selecting the catalog already
in force is a reload, not a no-op"`, case `"PROJ-1: recipes come from the saved file, never from
an unsaved editor buffer"`, case `"PROJ-1: the chooser does not need the Builder panel to be
open"`, case `"PROJ-2: an external catalog is chosen live, and the project still owns relative
sources"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-07 — Standing Builder intent survives by recipe identity, never by row position

LAW — `on(RecipeCatalog)` follows the chosen recipe by name to its new row, `picked` intact, and releases it when the identity is gone; no fallback to an index, stem or nearest name.

PROVEN BY — `workshop/weave.hpp` `RecipeCatalog`, `on(RecipeCatalog)`; `workshop/panel.hpp`
`picked`; `tests/test_workshop_panels.cpp` case `"PROJ-1: a reordered catalog moves the maker's
choice to its recipe, not its row"`, case `"PROJ-1: a choice whose recipe is gone is cleared, not
handed to its neighbour"`, case `"PROJ-1: an emptied catalog leaves no selection standing"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-09 — `Session::recipes_moved_to` is a projection, not an owner

LAW — Empty until a maker replaces a catalog, it is spent by the Builder on a row that exists exactly while the fact has moved, and holds the owner's own path read back, never recomposed.

MEANS
- it is on the `Session` and not on the Builder pane because `close_panel` forgets the pane whole.

PROVEN BY — `workshop/screen.hpp` `recipes_moved_to`; `workshop/weave.hpp` `RecipeSwap`;
`tests/test_workshop_document.cpp` case `"PROJ-1: the catalog row costs one `said` row, and only
where it is present"`; `tests/test_workshop_files.cpp` case `"PROJ-1: a live catalog choice is
this session's and is written nowhere"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## WL-PROJ-10 — A path is not a sentence

LAW — `detail::fit_path` measures the browser's location header and the Builder's catalog row: a root cue (`path_root_cue`, lexical), a mark where the middle went, the tail cut at a component boundary.

MEANS
- a sentence front-loads its meaning and a path back-loads it: they are cut at opposite ends;
- it changes no stored identity, and no pane widens to avoid a cut.

PROVEN BY — `workshop/screen.hpp` `fit_path`, `path_root_cue`, `files_header_prefix`;
`tests/test_workshop_files.cpp` case `"PROJ-2: fitting a path keeps the end that says which file
it is"`; `tests/test_workshop_screen.cpp` case `"WUX-7: four things must agree before a row is
scrolled at all"`.
WHY — `agents/decisions/a-path-is-not-a-sentence.md`

## WL-PROJ-11 — The Builder panel announces only what it watched

LAW — The panel's `awaiting` latch is set when it asks and released only at an outcome the build will not leave (`still_going`), so an arriving status is news exactly when this panel watched the build begin.

MEANS
- `heard` tells "the tool has not answered" from "the tool never built anything";
- a panel opened while a child is alive is told `running`, shows it, and announces nothing;
- `awaiting_realization` is the twin latch, held longer; `chosen` is bounded at use, not at write.

PROVEN BY — `workshop/panel.hpp` `BuilderPane`, `BuilderPane::heard`, `BuilderPane::awaiting`,
`BuilderPane::awaiting_realization`, `BuilderPane::chosen`; `workshop/weave.hpp`
`on(BuildStatus)`; `builder/vocabulary.hpp` `still_going`; `tests/test_workshop_panels.cpp` case
`"a panel opened mid-build is TOLD it is running, and announces nothing"`, case `"a running build
is on the panel, with its operation and its output count"`, case `"BLD-1: a build outcome and a
realization outcome are TWO rows and TWO notices"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-12 — The tool's status is kept only while a panel presents it

LAW — A `BuildStatus` with no Builder panel open is not remembered; closing the panel destroys its copy and reaches no tool, and reopening asks again and is answered with the tool's own running total.

MEANS
- a copy kept against a later panel makes a presentation a second owner of somebody else's facts.

PROVEN BY — `workshop/weave.hpp` `on(BuildStatus)`; `workshop/panel.hpp` `close_panel`;
`tests/test_workshop_panels.cpp` case `"closing forgets the panel's copy; the TOOL keeps its own
count"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-13 — A build is asked for by the tool's name, with the realize intention beside it

LAW — Workshop holds no target, recipe or command: `build_now` names the row under the maker's cursor, refuses in words with no answer or no recipes yet, and says `realize` in the same sentence.

MEANS
- with no Builder panel open the key is unbound; a panel that has not heard cannot ask;
- everything after the send belongs to the tool, the runner and the realization owner.

PROVEN BY — `workshop/weave.hpp` `build_now`; `tests/test_workshop_panels.cpp` case `"Build asks
for the name the TOOL gave, and asks for nothing without one"`, case `"a panel that has not heard
from its tool cannot ask for a build"`, case `"BLD-1: `b` builds the recipe the maker chose, not
the one last built"`, case `"BLD-1: `Shift+b` is BUILD & REALIZE, and the second intention crosses
the seam"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-14 — The frontier build is one comparison, and never chooses for the maker

LAW — `f` compares the live frontier artifact with each catalog row's, once, and sends `build_now` with the realize intention; several producers refuse unless the maker's standing pick is one of them.

MEANS
- the catalog's order is nobody's intent: the refusal names the candidates; the pick is `c`'s;
- `picked` tells an explicit pick from `chosen`'s default of 0, an index and not a choice;
- there is no second build path, no direct load and no new sentence on the bus.

PROVEN BY — `workshop/weave.hpp` `build_frontier`; `workshop/panel.hpp` `BuilderPane::picked`;
`tests/test_workshop_panels.cpp` case `"BLD-2: `f` builds and realizes the ONE recipe that
produces the frontier"`, case `"BLD-2: several recipes produce the frontier -- `f` never chooses
for the maker"`.
WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-PROJ-15 — The shipped catalog is staged beside the executable

LAW — The recipe catalog Workshop ships is staged beside the executable under `kDefaultRecipesName`; `--recipes` names a different one, and there is no registry, no picker and no search path.

PROVEN BY — `workshop/recipe_persist.hpp` `kDefaultRecipesName`; `workshop/workshop.cpp`
`exe_dir`, `Arguments::recipes`; `workshop/CMakeLists.txt` `zengine_workshop_dir`;
`tests/test_workshop_files.cpp` case `"the shipped catalog is staged beside the executable, under
the name the launch resolves"`, case `"the launch resolves the shipped catalog beside the
executable, and --recipes is the only other road"`.
WHY — `agents/decisions/one-completion-one-owner.md`

## Do not assume

- That a running build follows a replaced catalog — it does not, and that has no witness yet
  (WL-PROJ-03).

