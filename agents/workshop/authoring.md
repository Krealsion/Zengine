# Workshop law — authoring

Register `WL-AUTH`: the two authored files gain a writer, and it is the maker's own act. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-AUTH-01 — The chooser authors one recipe row, as authored, through the one seam

LAW — `a` in Files enumerates once and asks what nothing can detect; the host appends ONE row as authored to the catalog's rows, saves atomically and installs it by `use_recipes`.

MEANS
- a `.cpp` and a directory holding `CMakeCache.txt` are candidates; a source tree is not;
- the row goes to the catalog in force, else a project catalog in force next launch (WL-PROJ-15);
- a row the recipe law refuses is refused whole, in its words, and the file's bytes are the bytes.

DOES NOT MEAN
- that Workshop edits or removes a row, or reads a `CMakeLists.txt` — a text editor does;
- that the shipped catalog is written into — it is installation truth and stays so.

PROVEN BY — `files/vocabulary.hpp` `kActionPickBuildable`; `files/files.cpp` `pick_buildable`,
`chooser_choose`, `authoring_commit`; `workshop/files_seam_vocabulary.hpp`
`RecipeAuthorRequested`, `RecipeOutcome`; `workshop/files_doors.hpp` `RecipesDoor`;
`workshop/weave.hpp` `HostContext::RecipeDraft`, `HostContext::author_recipe`;
`workshop/authoring.hpp` `RecipeAuthor`, `author_recipe`; `workshop/screen.hpp`
`paint_authoring`;
`workshop/recipe_persist.hpp` `kProjectRecipesName`; `tests/test_workshop_panes_files.cpp` case
`"FILES-WEAVE: `a` opens a chooser inside the pane's own room"`, case `"FILES-WEAVE: a maker
authors a recipe row in-pane, and the host writes it"`, case `"FILES-WEAVE: the authoring line
takes raw keys, and Escape abandons it whole"`; `tests/test_workshop_files.cpp` case `"PANE-DOOR:
the recipes door spends this host's one writer and re-words nothing"`.
WHY — `agents/decisions/a-maker-authors-the-two-files.md`

## WL-AUTH-02 — `load it` authors the minimum plan row, and a project plan is the plan in force

LAW — `o` asks a role for the Builder's chosen artifact; the host appends through the executor first, then writes `<project>/workshop-plan.json`, which is the plan in force when no `--load-plan` is given.

MEANS
- the row is a stem and a weave with the typed role; an empty role or a named stem refuses first;
- the file is the plan as read plus the new row, through the codec, after the executor said yes;
- a frontier row whose product is on disk gets the button's own ask from `o`: no second key.

DOES NOT MEAN
- that a plan is edited, reordered or pruned — the one act is one appended row;
- that `o` builds from nothing or stages — with no product the row is the frontier, for `f`.

PROVEN BY — `workshop/keymap.hpp` `builder.load`, `KeyContext::kAuthoring`;
`workshop/screen.hpp` `AuthoringPrompt`, `Session::authoring`;
`workshop/screen_pane_state.cpp` `paint_authoring`;
`workshop/weave.hpp` `HostContext::PlanAppend`, `HostContext::append_plan_row`,
`HostContext::plan_names`, `load_it`; `workshop/weave_recipes.cpp` `load_it`,
`authoring_commit`; `workshop/authoring.hpp` `PlanAuthor`, `plan_names`, `append_plan_row`;
`workshop/staging.hpp` `product_of`; `workshop/load_persist.hpp` `kProjectLoadPlanName`,
`plan_in_force`; `workshop/workshop.cpp` `plan_path`; `tests/test_workshop_panels.cpp` case
`"LOAD-IT: `o` asks for a role, refuses an empty one in the plan's words, and authors nothing
until it has one"`, case `"LOAD-IT: `o` on an artifact the plan already names refuses and points
at `B`"`, case `"`o` on a recipe whose product is already built finishes the load: the button's
own ask with the second intention aboard, and the row's sentence says so"`, case `"`o` with
nothing built yet leaves the row pending and asks the Builder nothing, and names the frontier
key; a row that is not the frontier says only what the project said"`;
`tests/test_workshop_load.cpp` case `"LOAD-IT: the minimum row is written as authored, the plan
round-trips byte for byte, and a duplicate stem is refused by the plan's own law"`, case `"the
writer says where the frontier row's product is, through the staging rule: present when the
chosen recipe's file is on disk, absent behind another frontier and absent when nothing is
built"`; `tests/test_workshop_files.cpp` case `"LOAD-IT: the project plan at the captured root
is the plan in force when no --load-plan is given"`.
WHY — `agents/decisions/a-maker-authors-the-two-files.md`

## WL-AUTH-03 — The executor has one append door, and the plan's law goes first

LAW — `append` runs the plan's law on the candidate, refuses Unstarted, Failed, Loading and Advancing in words, queues as `authored` behind a Waiting frontier, and in Complete performs the row.

MEANS
- in Complete the walk resumes from the new row by the ordinary steps until complete again;
- behind a frontier the row is authored, not looked at; a waiting row becomes the frontier;
- a refusal leaves the plan exactly what it was; the door writes no file.

DOES NOT MEAN
- that a row is removed, replaced or reordered through it — one door, one direction.

PROVEN BY — `workshop/load_execute.hpp` `PlanExecutor::append`, `PlanExecutor::Appended`;
`workshop/weave.hpp` `HostContext::PlanAppend`; `workshop/authoring.hpp` `append_plan_row`;
`tests/test_workshop_load.cpp` case `"LOAD-IT: `append` in Complete performs the new row by the
ordinary three steps, and Complete means complete again"`, case `"LOAD-IT: `append` while Waiting
queues the row behind the frontier as `authored`"`, case `"LOAD-IT: `append` is refused mid-row
and after a refusal, and a duplicate stem is refused"`.
WHY — `agents/decisions/a-maker-authors-the-two-files.md`

## Do not assume

- That the chooser finds recipes, reads a `CMakeLists.txt` or detects a build system — it lists
  what a place can at least TRY to build and asks for the rest (WL-AUTH-01).
- That a project catalog or a project plan is remembered anywhere but in the project — both are
  files at the captured root, read by the launch rule and nothing else (WL-AUTH-01, WL-AUTH-02).
- That the Terminal has a `pick` or `load` verb — it does not; both are keys in their panes.
