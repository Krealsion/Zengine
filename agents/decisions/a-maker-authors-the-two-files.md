# A maker authors the two files, one row at a time, from inside the running Workshop

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [authoring](../workshop/authoring.md) and [files](../workshop/files.md).

**Context.** After the reload phase a maker could rebuild a loaded weave and see it enter the
running project with its state kept — for a weave the project already named. Naming one still
meant leaving Workshop twice: once to write a recipe row by hand into a catalog file, once to
write a plan row by hand into a plan file, and both formats are exact. The two files were
deliberately never written by Workshop: the recipe file because completion must not leak host
paths back into it (WL-PROJ-02), the plan because a durable plan must never be rewritten to agree
with a failed runtime. Both reasons still hold. Neither forbids a maker's own act.

**Decision.** Two gestures, two rows, one direction. In Files, `a` lists what the browsing
location can at least try to build — a `.cpp`, or a directory holding a `CMakeCache.txt` — asks
for the few things nothing can detect (a name, a stem, a package prefix and link targets for a
source; a target for a tree), and hands a DRAFT to the host. The host composes the row, checks it
by the recipe law, appends it AS AUTHORED to the catalog's rows AS WRITTEN, saves atomically and
installs the file through the one seam `u` already spends. When the catalog in force is the
shipped default, the row goes into `<project>/build-recipes.json`, seeded from the shipped rows so
nothing buildable disappears; the shipped file is never written. In the Builder, `o` asks a role
for the chosen recipe's artifact and hands stem and role to the host, which appends the minimum
row through a new executor door FIRST — so the running project refuses before any file changes —
and then writes `<project>/workshop-plan.json`. One launch rule makes that file the plan in force
when no `--load-plan` is given. Each act appends one row; nothing edits, reorders or removes one.

**Alternatives considered.**
- *Argued: a recipe editor in a pane* — refused: a second spelling of the recipe grammar, with a
  cursor over fields a text editor already edits better; the chooser asks only what a place
  cannot say.
- *Argued: detecting recipes from a `CMakeLists.txt` or a conventional filename* — refused, as it
  was in the recipe phase: a guess about a maker's intent written into a file that is their
  intent; pinned by case `"PICK-1: the chooser enumerates once, at the gesture: a .cpp and a
  configured tree are candidates, a source tree is not"`.
- *Argued: writing the new row into the shipped catalog when it is in force* — refused:
  installation truth is not a maker's file; pinned by case `"PICK-1: when the catalog in force is
  the shipped default, the chooser authors a PROJECT catalog and installs it"`.
- *Argued: writing the plan file first and letting the next launch pick it up* — refused: a row
  the running project would refuse (a duplicate, a row under a conversation) would be durable
  before it was true; the executor goes first, and a refusal writes nothing; pinned by case
  `"LOAD-IT: `append` is refused mid-row and after a refusal, and a duplicate stem is refused"`.
- *Argued: Terminal `pick` and `load` verbs* — withdrawn by the founder: the Files pane and the
  Builder are where the gestures belong, and the witness reads the oven's own painted state.
- *Argued: the project plan as a remembered `--load-plan`* — refused: a project's plan is a file
  in the project, found by one rule at launch, not a preference in a per-user folder.

**Consequences.** A maker can point at something buildable, build it, and load it without leaving
Workshop; the project grows `build-recipes.json` and `workshop-plan.json` at its root, and the
next launch from that root is made of what was loaded. A row once authored is edited or removed
in a text editor, as before. `plan_in_force` is the one place the launch default is spelled, and
the host may not spell a second. The Project pane may hide a long plan's new row behind
`... n more`; that is a presentation gap, not a plan fact.

**Laws supported.** [WL-AUTH-01](../workshop/authoring.md), [WL-AUTH-02](../workshop/authoring.md),
[WL-AUTH-03](../workshop/authoring.md), [WL-FILES-15](../workshop/files.md).
