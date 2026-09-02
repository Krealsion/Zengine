# One completion, one owner

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [project](../workshop/project.md).

**Context.** A relative authored `single_source` meant two files: the editor and the runner's
exists-preflight resolved it against the process working directory while the generated project
embedded it verbatim for CMake to resolve against the workspace (`5a302ae`). The completed
catalog then had no owner — the runner kept a vector of recipes, the Builder a vector of views,
the host's edit-source answer a closure per recipe — so replacing it would have meant
re-synchronising every consumer by hand, and the first one forgotten would answer from the old
project in a way no test could see (`fc9cb62`, "One Workshop, one set of recipes it currently
means"). `--recipes` chose the catalog a session was stuck with (`dc0501f`, "Choose the recipes
this Workshop means, while it is running").

**Decision.** `complete_recipes(recipes, host_dir, project_dir)` is the one place any host fact
enters a recipe, called once. `CurrentRecipes` is the one session owner, declared above the
HostContext, the bus, the Kernel and every weave — the declaration order is the lifetime proof
— and `hold()` assigns into members it owns, so a replacement changes contents and never the
bound objects. `install_recipes` is the one seam that turns a file into the answer: read, parse,
complete, hold, every step on a candidate. `files.use-recipes` is the first live chooser. Standing
Builder intent survives by recipe identity. `Session::recipes_moved_to` is a projection holding
the owner's own absolute path.

**Alternatives considered.**
- *Three resolutions of one spelling* — the defect; the falsifier is case `"EDIT-1: a relative
  recipe source is the PROJECT's file, in the editor and in the build"`, two bases holding
  `src/example.cpp` with different bytes.
- *A copy per consumer* — replaced; the two build weaves refuse an rvalue outright, because a
  temporary catalog is a dangling one; pinned by case `"PROJ-0: holding a new catalog replaces
  the contents, never the object"`.
- *A setter for the source path* — rejected: a parameter of `hold()`, so "the path moved and the
  rows did not" has no spelling.
- *`--recipes` as a private path to the owner* — rejected: `main` wires `use_recipes` and
  installs its own startup catalog through it, so there is no second completion policy.
- *Completing relative sources against the catalog's own directory* — rejected; pinned by case
  `"PROJ-1: a catalog's own directory is not a source base"`.
- *Same-path as a no-op* — rejected: the reload is the application's whole live-refresh
  mechanism, with no watcher, timer or poll.
- *Falling back to the old index, the artifact stem or a nearest name when a catalog is
  replaced* — rejected: a replacement may invalidate a choice and may not reinterpret one;
  pinned by case `"PROJ-1: a choice whose recipe is gone is cleared, not handed to its
  neighbour"`.
- *A project-relative spelling for `recipes_moved_to`* — replaced: unambiguous only while the
  browser could not leave the project (`0cf8a94`).

**Consequences.** The recipe file is never rewritten and the owner is not authorship. A valid
empty catalog installs; completion is total and adds no third refusal kind. A foreign catalog's
relative source still names a file under the active project. An in-flight build keeps the
artifact it was ordered for and is never re-aimed — a claim with no witness yet. The Builder's
catalog row costs one `said` row, only where the fact has moved.

**Laws supported.** [WL-PROJ-02](../workshop/project.md), [WL-PROJ-03](../workshop/project.md),
[WL-PROJ-04](../workshop/project.md), [WL-PROJ-05](../workshop/project.md),
[WL-PROJ-07](../workshop/project.md), [WL-PROJ-09](../workshop/project.md).
