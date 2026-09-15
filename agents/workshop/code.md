# Workshop law — a pane's code

Register `WL-CODE`: a running pane followed to the authored source of its code, the maker handed
on to the build, and Workshop's own panes made a project the same way, with the launch that starts
the Workshop working on them. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). The opening is [`opening.md`](opening.md); the pointed subject
is [`contextual.md`](contextual.md); the recipes and the Builder's choice are
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

LAW — The runtime script copies the host, its staged artifacts, both plans and both catalogs into a runtime its manifest binds to one tree and configuration, reuses it while current, and writes over nothing.

MEANS
- a pane builds into its package's directory, never the host's: no build writes an image it maps;
- a runtime of another tree or configuration, an incomplete one and a busy directory are refused;
- a host, service, plan or catalog built anew makes it stale; a rebuilt or promoted pane does not.

DOES NOT MEAN
- that a runtime updates itself: a stale one is kept as it is, and a new one is made beside it;
- that a reuse reads what is in the runtime's copies: the digests it checks are the tree's files.

PROVEN BY — `workshop/CMakeLists.txt` `zengine_development_copies`,
`zengine_development_replaceable`, `zengine_development_runtime`; `workshop/prepare-runtime.cmake`
`zengine_manifest`, `zengine_fixed_names`, `zengine_changed`, `zengine_missing`;
`tests/test_workshop_files.cpp` case `"a development runtime is made whole into an absent
directory, then reused while what it copied is current: a promoted pane and a rebuilt one keep it,
and nothing is copied again"`, case `"a development runtime that is stale, incomplete, or made for
another configuration or another set of copies is refused and left exactly as it is"`, case `"the
development runtime script this tree generated refuses another tree's runtime, an earlier script's
runtime and a non-empty directory, and copies nothing"`, case `"the development catalog this tree
generated names every shipped pane by its own target, build directory and weave source, each one
the Editor opens, and nothing else"`.
UNWITNESSED — an ordinary build leaving a runtime's files untouched is measured by a live
witness on Linux and on Windows and pinned by no case.
WHY — `agents/decisions/workshop-develops-itself-from-a-copy.md`

## WL-CODE-08 — The development launch holds its runtime and starts its copy, or nothing

LAW — `zengine-workshop-develop` claims its runtime while it prepares and runs it, has the runtime script make or reuse it, then starts that runtime's host with its plan, catalog and project, or nothing.

MEANS
- the claim comes before it looks or prepares, and is let go when the host has exited: one launch;
- a launch that cannot have it prepares, starts and stops nothing; a host none holds is refused;
- the build tree's host is never started, and the shared configuration names only the target.

DOES NOT MEAN
- that Workshop can launch, reload, edit or relaunch the process hosting it: that stays outside;
- that a host started without a launch is coordinated: the in-use check alone refuses beside it.

PROVEN BY — `workshop/develop.hpp` `launch`, `choose`, `runtime_command`, `host_command`,
`image_in_use`, `claim_runtime`, `runtime_claim_name`, `real_world`; `workshop/develop.cpp`
`main`; `builder/run.hpp` `run_forwarding`; `workshop/CMakeLists.txt` `zengine_development_plan`;
`tests/test_workshop_files.cpp` case `"the development launch claims its runtime, prepares it
through the runtime script, then starts only that runtime's host, with its graphical plan and
development catalog, in a project directory of its own, and lets the claim go only after that host
exits"`, case `"a development launch that is refused starts nothing and holds nothing after: no
graphical plan, a runtime another launch holds or that could not be claimed, a runtime whose host
is running, a runtime script that refused or never ran, a project path that is no directory, an
argument it does not know"`, case `"two launches of one runtime that overlap before its host
starts: exactly one prepares and starts it, and the other refuses without preparing, whether the
runtime was absent or already made"`, case `"a launch holds its runtime until the Workshop it
started has exited: a launch meanwhile is refused and prepares nothing, and a launch after the
exit prepares and starts it again"`, case `"a launch whose preparation fails, or whose Workshop
does not start, lets its runtime go as it ends: the next launch prepares it and starts it"`, case
`"a launch that cannot make its runtime's claim prepares nothing and launches nothing, and says
why"`, case `"a launch that dies holding its runtime lets it go with its process, and a Workshop a
dead launch left open is refused by the in-use check and stopped by nobody"`, case `"a Workshop
started from a runtime without a launch holds no claim, and while it runs a launch is refused by
the in-use check and stops nothing"`, case `"launches of two runtimes do not wait on each other,
and one runtime spelled another way is still the runtime its launch holds"`, case `"a runtime's
claim is named for its directory however it is spelled, made yet or not, and two runtimes are two
claims"`, case `"an image a running program holds reads as in use and a file nobody holds does
not, and the project directory is made when absent and refused when it is a file"`.
UNWITNESSED — that CLion loads the shared configuration and runs it with its toolchain's
environment is measured by a live witness on Windows and pinned by no case.
WHY — `agents/decisions/the-development-launch-stands-outside-workshop.md`

## Do not assume

- That any catalog opens Workshop's own panes: the shipped default names none of their entries;
  the development catalog a configured tree generates does (WL-CODE-06).
- That an ordinary build beside a runtime is pinned — it is measured by a witness, not a case
  (WL-CODE-07, UNWITNESSED).
- That the IDE's side of the launch is pinned — CLion loading the shared configuration and its
  toolchain environment reaching the host are measured by a witness, not a case (WL-CODE-08,
  UNWITNESSED).
- That looking for a running host is what keeps two launches apart: the claim does, and the look
  is for a host no launch holds — one started another way, or left by a stopped launch
  (WL-CODE-08).
- That a reused runtime has been found undamaged: the digests are read from the build tree's
  files, and the runtime is looked at for the names it copied (WL-CODE-07).
- That a pane is discovered because its protocol is installed: it arrives by a load-plan row a
  maker authors, and the office its source speaks as must be the role that row gives it.
