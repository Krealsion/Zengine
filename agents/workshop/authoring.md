# Workshop law — authoring

Register `WL-AUTH`: the two authored files gain a writer, and it is the weaver's own act. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-AUTH-01 — The chooser authors one recipe row, as authored, through the one seam

LAW — `a` in Files enumerates once and asks what nothing can detect; the host appends ONE row as authored to the catalog's rows, saves atomically and installs it by `use_recipes`.

MEANS
- `.cpp` and `CMakeCache.txt` dirs are candidates, source trees are not; several configs ask one;
- the row goes to the catalog in force, else a project catalog in force next launch (WL-PROJ-15);
- a row the recipe law refuses is refused whole, in its words, and the file's bytes are the bytes.

DOES NOT MEAN
- that Workshop edits or removes a row, or reads a `CMakeLists.txt` — a text editor does;
- that the shipped catalog is written into — it is installation truth and stays so.

WHY — `agents/decisions/a-weaver-authors-the-two-files.md`

## WL-AUTH-02 — `load it` authors the minimum plan row, and a project plan is the plan in force

LAW — `o` asks a role, in a line inside the Builder pane's own room; the host appends through the executor first, then writes `<project>/workshop-plan.json`, the plan in force with no `--load-plan`.

MEANS
- the row is a stem and a weave with the typed role; an empty role or a named stem refuses first;
- the file is the plan as read plus the new row, through the codec, after the executor said yes;
- a frontier row whose product is on disk gets the button's own ask from `o`: no second key.

DOES NOT MEAN
- that a plan is edited, reordered or pruned — the one act is one appended row;
- that `o` builds from nothing or stages, or that the line is a keyboard mode of this HOST.

WHY — `agents/decisions/a-weaver-authors-the-two-files.md`

## WL-AUTH-03 — The executor has one append door, and the plan's law goes first

LAW — `append` runs the plan's law on the candidate, refuses Unstarted, Failed, Loading and Advancing in words, queues as `authored` behind a Waiting frontier, and in Complete performs the row.

MEANS
- in Complete the walk resumes from the new row by the ordinary steps until complete again;
- behind a frontier the row is authored, not looked at; a waiting row becomes the frontier;
- a refusal leaves the plan exactly what it was; the door writes no file.

DOES NOT MEAN
- that a row is removed, replaced or reordered through it — one door, one direction.

WHY — `agents/decisions/a-weaver-authors-the-two-files.md`

## Do not assume

- That the chooser finds recipes, reads a `CMakeLists.txt` or detects a build system — it lists
  what a place can at least TRY to build and asks for the rest (WL-AUTH-01).
- That a project catalog or a project plan is remembered anywhere but in the project — both are
  files at the captured root, read by the launch rule and nothing else (WL-AUTH-01, WL-AUTH-02).
- That the Terminal has a `pick` or `load` verb — it does not; both are keys in their panes.

## Proven by

### WL-AUTH-01
`files/vocabulary.hpp` kActionPickBuildable
`files/files.cpp` pick_buildable cache_is_multi_config chooser_choose record_field compose_recipe
field_at field_count
`workshop/pane_seam_vocabulary.hpp` RecipeAuthorRequested RecipeOutcome
`workshop/pane_doors.hpp` RecipesDoor
`workshop/weave.hpp` HostContext::RecipeDraft HostContext::author_recipe
`workshop/authoring.hpp` RecipeAuthor author_recipe
`workshop/recipe_persist.hpp` kProjectRecipesName
`tests/test_workshop_panes_files.cpp` "`a` opens a chooser inside the pane's own room" "the chooser
offers a source file and a configured tree, never a source tree" "a weaver authors a recipe row
in-pane, and the host writes it" "the authoring line takes raw keys, and Escape abandons it whole"
"deliberate keys in Files still choose, refuse a blank field, write one recipe and cancel, and
Return then opens the file" "an ordinary configured tree is authored in four fields, config empty"
"a tree with several configurations asks a fifth field, and keeps it"
`tests/test_workshop_files.cpp` "the recipes door spends this host's one writer and re-words
nothing" "a row authored while the shipped catalog is in force goes into a project catalog, which is
installed, and the shipped file keeps its bytes"
`tests/test_builder.cpp` (`--config` spent against a `cmake_target` recipe, unchanged by this law
— `builder/generate.hpp` `prepare`)

### WL-AUTH-02
`builder/builder-pane/vocabulary.hpp` kActionLoadIt kActionCommit kActionCancel
`builder/builder-pane/pane.cpp` begin_load_it open_role commit_role say_role
`workshop/builder_seam_vocabulary.hpp` PlanNamesRequested PlanNames PlanRowRequested PlanRowWritten
`workshop/pane_doors.hpp` PlanDoor
`workshop/weave.hpp` HostContext::PlanAppend HostContext::append_plan_row HostContext::plan_names
`workshop/authoring.hpp` PlanAuthor plan_names append_plan_row
`workshop/staging.hpp` product_of
`workshop/load_persist.hpp` kProjectLoadPlanName plan_in_force
`workshop/workshop.cpp` plan_path
`tests/test_workshop_panes_builder.cpp` "a weaver's authored override for a retired Workshop id
keeps working" "`o` asks for a role in the pane's own room, and authors it" "Escape abandons the
line, and nothing is written" "an artifact the plan already names is refused before the line" "a
refused row is said in the owner's own words" "a row whose product is built finishes with the
button's act" "`o` and the frontier action asked of a project office nobody holds are each refused
at dispatch, in words" "a role line's row refused at dispatch by the plan office is said, and
nothing is written or built"
`tests/test_workshop_load.cpp` "the minimum row is written as authored, the plan round-trips byte
for byte, and a duplicate stem is refused by the plan's own law" "the writer says where the frontier
row's product is, through the staging rule: present when the chosen recipe's file is on disk, absent
behind another frontier and absent when nothing is built"
`tests/test_workshop_files.cpp` "the project plan at the captured root is the plan in force when no
--load-plan is given"

### WL-AUTH-03
`workshop/load_execute.hpp` PlanExecutor::append PlanExecutor::Appended
`workshop/weave.hpp` HostContext::PlanAppend
`workshop/authoring.hpp` append_plan_row
`tests/test_workshop_load.cpp` "`append` in Complete performs the new row by the ordinary three
steps, and Complete means complete again" "`append` while Waiting queues the row behind the frontier
as `authored`" "`append` is refused mid-row and after a refusal, and a duplicate stem is refused"
