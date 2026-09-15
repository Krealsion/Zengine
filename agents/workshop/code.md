# Workshop law — a pane's code

Register `WL-CODE`: a running pane followed to the authored source of its code, and the maker
handed on to the build. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). The opening is [`opening.md`](opening.md); the pointed subject
is [`contextual.md`](contextual.md); the recipes and the Builder's choice are
[`project.md`](project.md).

## WL-CODE-01 — What stands behind an office's code is three owners' answer, read at the ask

LAW — The host reads the office's holder from the bus, the realized row with that WeaveId, and every recipe producing its artifact, at the ask; it chooses nothing, opens nothing and stores nothing.

MEANS
- an authored role that equals the office joins nothing: only the holder's minted WeaveId does;
- nobody holding, no realized row and no recipe are three absences, each its own sentence;
- a `cmake_target` recipe carries no source, and the owner's reload refusal rides beside the rows.

DOES NOT MEAN
- that the answer lists what a build compiles: a single source is an editing entry point.

PROVEN BY — `workshop/provenance.hpp` `code_source_of`; `workshop/weave.hpp`
`HostContext::CodeSource`, `HostContext::code_source`; `workshop/load_execute.hpp`
`reload_refusal`, `ResolvedArtifact`; `workshop/workshop.cpp` `code_source`;
`tests/test_workshop_panes_code.cpp` case `"the code behind an office is found by the weave
holding it, never by a plan row's authored role"`.
WHY — `agents/decisions/a-pane-reaches-its-code-through-the-host.md`

## WL-CODE-02 — Edit Code spends the pointed pane, and opens exactly one recipe's single source

LAW — Through the pane seam, Edit Code asks the host about the captured pane's office and asks the opening office to open only exactly one recipe's single source; any other answer is words.

MEANS
- the pane seam re-asks the setup first; neither the selection nor the keyboard is read or moved;
- several recipes are named and left to the Builder's choice; a CMake target names no file;
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
mode names no pane, says where the gesture lives, and opens nothing"`.
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

## Do not assume

- That Workshop's own panes reach their code: this repository's CMake tree builds them, and a
  `cmake_target` recipe names no single source to open (WL-CODE-02).
- That a pane is discovered because its protocol is installed: it arrives by a load-plan row a
  maker authors, and the office its source speaks as must be the role that row gives it.
