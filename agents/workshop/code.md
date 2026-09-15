# Workshop law — a pane's code

Register `WL-CODE`: a running pane followed to the authored source of its code, the maker handed
on to the build, and Workshop's own panes made a project the same way. One law per heading; cite
by ID. Router: [`../workshop.md`](../workshop.md). The opening is [`opening.md`](opening.md); the
pointed subject is [`contextual.md`](contextual.md); the recipes and the Builder's choice are
[`project.md`](project.md); what a build said is [`build-output.md`](build-output.md).

## WL-CODE-01 — What stands behind an office's code is three owners' answer, read at the ask

LAW — The host reads the office's holder from the bus, the realized row with that WeaveId, and every recipe producing its artifact, at the ask; it chooses nothing, opens nothing and stores nothing.

MEANS
- an authored role that equals the office joins nothing: only the holder's minted WeaveId does;
- nobody holding, no realized row and no recipe are three absences, each its own sentence;
- a `cmake_target` recipe carries its editing entry or no source; the reload refusal rides beside.

DOES NOT MEAN
- that the answer lists what a build compiles: a source or an entry is where reading starts.

PROVEN BY — `workshop/provenance.hpp` `code_source_of`; `workshop/weave.hpp`
`HostContext::CodeSource`, `HostContext::code_source`; `workshop/load_execute.hpp`
`reload_refusal`, `ResolvedArtifact`; `workshop/workshop.cpp` `code_source`;
`tests/test_workshop_panes_code.cpp` case `"the code behind an office is found by the weave
holding it, never by a plan row's authored role"`.
WHY — `agents/decisions/a-pane-reaches-its-code-through-the-host.md`

## WL-CODE-02 — Edit Code spends the pointed pane, and opens exactly one recipe's source

LAW — Through the pane seam, Edit Code asks the host about the captured pane's office and asks the opening office to open only exactly one recipe's single source or editing entry; any other answer is words.

MEANS
- the pane seam re-asks the setup first; neither the selection nor the keyboard is read or moved;
- several recipes are named and left to the Builder's choice; a target with no entry names none;
- a pane made from data, Workshop's own, and a weave no plan row realized each say what they are.

DOES NOT MEAN
- that a key can spend it: command mode names no pane, so the bound action says where it lives.

PROVEN BY — `workshop/weave_code.cpp` `edit_code`, `code_refusal`; `workshop/context.hpp`
`kContextCatalog`; `workshop/keymap.hpp` `kEditCode`, `kActionCatalog`;
`workshop/weave_arrange.cpp` `spend_pane_action`; `workshop/weave_pointer.cpp`
`spend_context_choice`; `workshop/weave_terminal.cpp` `command`;
`tests/test_workshop_panes_code.cpp` case `"Edit Code opens the pointed pane's one source
through the opening office, and the Builder follows its recipe"`, case `"Edit Code acts on the
pane that was pointed at, not the selection, and a later selection redirects nothing"`, case `"a
pointed pane that left the setup before Edit Code was chosen is refused, and nothing is asked"`,
case `"code that cannot be named is said in words -- no recipe, several, a CMake target, a weave
the plan did not load -- and nothing is opened"`, case `"Edit Code bound to a key in command
mode names no pane, says where the gesture lives, and opens nothing"`, case `"Edit Code opens a
CMake target recipe's editing entry through the opening office, and the Builder follows that
recipe"`.
WHY — `agents/decisions/a-pane-reaches-its-code-through-the-host.md`

## WL-CODE-03 — The open's answer settles the one held ask, and the Builder hears unchanged code

LAW — Only the held ask's own answer or Loom's refusal of its attempt settles Edit Code's open; an accepted answer re-asks the host, publishing `PaneSourceOpened` once only if the code is unchanged.

MEANS
- a refused or superseded answer is said in the owner's words, and nothing is published;
- code that changed while opening leaves the file open, tells the Builder nothing, and says so;
- silence stays pending: no turn count, timeout or retry decides it; a newer spend replaces it.

DOES NOT MEAN
- that `PaneSourceOpened` is an order: it builds, arms, reloads and promotes nothing.

PROVEN BY — `workshop/weave_code.cpp` `on(SourceOpened)`, `on(DispatchRefused)`, `same_code`;
`workshop/weave.hpp` `WorkshopWeave::CodeOpen`, `WorkshopWeave::code_open_`;
`workshop/pane_seam_vocabulary.hpp` `PaneSourceOpened`; `tests/test_workshop_panes_code.cpp`
case `"a dirty Editor refuses the pane's source, the refusal is said, and the Builder is not
moved"`, case `"an open no opening office could take is refused at dispatch by that attempt,
and forged answers settle nothing"`, case `"an open that is never answered stays pending
however many turns pass, and a late answer settles it"`, case `"code that changed while its
source was opening leaves the file open and tells the Builder nothing"`.
WHY — `agents/decisions/a-pane-reaches-its-code-through-the-host.md`

## WL-CODE-04 — The Builder pane follows Workshop's reading by recipe name, and nothing more

LAW — Hearing `PaneSourceOpened` as Workshop's office, the Builder pane chooses its recipe and says so with its own load after build; no pick follows, no build is sent, and other offices are ignored.

MEANS
- a pick of another recipe stops standing, as the choice left it; a pick of this recipe stands;
- a catalog the pane heard that lacks the recipe is a disagreement said, and nothing is chosen;
- a catalog not yet heard takes the name, which its arrival keeps or releases (WL-PROJ-07).

DOES NOT MEAN
- that the next build reloads the pane: eligibility is the owner's, and Workshop's notice says it;
- that a finished build's button now loads this recipe: it loads the build that finished.

PROVEN BY — `builder-pane/pane.cpp` `on(PaneSourceOpened)`, `finish_frontier_build`,
`build_realize`; `builder-pane/vocabulary.hpp` `BuilderPaneState::chosen`,
`BuilderPaneState::arm`; `tests/test_workshop_panes_code.cpp` case `"the Builder pane follows an
opened pane source only when Workshop's office said so"`, case `"Edit Code opens the pointed
pane's one source through the opening office, and the Builder follows its recipe"`, case `"the
Builder's choice from Edit Code is not a pick between producers: the frontier action still
asks"`, case `"a pick of another recipe does not follow Edit Code's choice: the frontier action
still asks between producers"`, case `"a pick of the recipe Edit Code chose still stands: the
frontier action builds it without another pick"`, case `"the Builder's words after Edit Code
promise no reload: an owner's refusal stands alone, and an eligible pane still reads how to build
it"`, case `"load after build stays as the maker set it across Edit Code: the Builder says which,
and the next loop's b alone offers its build"`, case `"a finished build left unloaded keeps its
button through Edit Code: it loads that build's own recipe, and b first makes it the chosen
recipe's"`.
WHY — `agents/decisions/a-pane-reaches-its-code-through-the-host.md`

## WL-CODE-05 — A CMake target may name where editing starts, and an old catalog names nowhere

LAW — `CMakeTargetRecipe::entry` names the source a reader of its artifact begins at; format 2 writes it, empty is none, a relative one is the project's, and a version-1 catalog reads whole with none.

MEANS
- one rule reads a recipe's file for both doors: its single source, else its entry;
- a version is answered from the envelope's claim before a row is judged; any other is refused;
- an entry is a recipe path: a quote or a control byte refuses the catalog whole.

DOES NOT MEAN
- that an entry lists a target's sources or changes a build: the build is still `--target`.

PROVEN BY — `builder/recipe.hpp` `CMakeTargetRecipe::entry`, `check_recipe`;
`workshop/recipe_persist.hpp` `kFormatVersion`, `v1`, `from_text`, `complete_recipes`,
`WorkshopCMakeTarget`; `workshop/provenance.hpp` `recipe_source_of`, `code_source_of`;
`tests/test_workshop_files.cpp` case `"a version-1 catalog a maker already has reads whole: every
row, and no CMake target with an entry"`, case `"a catalog is refused by a version this Workshop
does not read, and a version-2 row without its entry is refused by admission"`, case `"an editing
entry is written, read back, checked as a recipe path, and completed against the project like a
source"`; `tests/test_workshop_panes_code.cpp` case `"the Builder's e opens a CMake target
recipe's editing entry by the rule Edit Code reads, and one with none says so"`.
WHY — `agents/decisions/an-entry-is-where-reading-starts.md`

## WL-CODE-06 — Workshop's own panes are a catalog their configured tree generates

LAW — Configuring writes `development-build-recipes.json` beside the host: a `cmake_target` row per listed shipped pane weave, naming its target, its build directory, and its recorded weave source as entry.

MEANS
- which weaves are panes is a list in build configuration; no host code names a pane or a path;
- the entry is the source `zengine_weave` was handed, never a guess, and the Editor can open it;
- nothing selects the catalog: a maker names it, and the launch's catalog rule is unchanged.

DOES NOT MEAN
- that a listed pane reloads: eligibility is the realization owner's, row by row, at the reload.

PROVEN BY — `CMakeLists.txt` `zengine_weave`; `workshop/CMakeLists.txt`
`zengine_workshop_pane_weaves`, `zengine_development_rows`; `tests/test_workshop_files.cpp` case
`"the development catalog this tree generated names every shipped pane by its own target, build
directory and weave source, each one the Editor opens, and nothing else"`.
WHY — `agents/decisions/workshop-develops-itself-from-a-copy.md`

## WL-CODE-07 — The Workshop that edits itself runs from a copy no build writes

LAW — `development-runtime.cmake` copies the host, its staged artifacts, both plans and both catalogs into a runtime its manifest binds to one build tree, and into nothing that is already something.

MEANS
- a pane builds into its package's directory, never the host's: no build writes an image it maps;
- another tree's runtime, a runtime already made and a non-empty directory are refused, uncopied;
- a reload stages into the runtime and a promotion writes the runtime's file, not the build tree.

DOES NOT MEAN
- that the runtime is an install: the package ships no Workshop, and a changed host is a new copy.

PROVEN BY — `workshop/CMakeLists.txt` `zengine_development_copies`,
`zengine_development_runtime`; `tests/test_workshop_files.cpp` case `"the development runtime is
made once, into nothing that is already something: another tree's runtime, this tree's own and a
non-empty directory are refused, and nothing is copied"`, case `"the development catalog this
tree generated names every shipped pane by its own target, build directory and weave source, each
one the Editor opens, and nothing else"`.
UNWITNESSED — a successful copy, and an ordinary build leaving it untouched, are measured by a
live witness on Linux and on Windows and pinned by no case.
WHY — `agents/decisions/workshop-develops-itself-from-a-copy.md`

## Do not assume

- That any catalog opens Workshop's own panes: the shipped default names none of their entries;
  the development catalog a configured tree generates does (WL-CODE-06).
- That a successful runtime copy and an ordinary build beside it are pinned — they are measured
  by a witness, not a case (WL-CODE-07, UNWITNESSED).
- That a pane is discovered because its protocol is installed: it arrives by a load-plan row a
  maker authors, and the office its source speaks as must be the role that row gives it.
