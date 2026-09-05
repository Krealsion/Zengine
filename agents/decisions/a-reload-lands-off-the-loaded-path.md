# A reload lands off the loaded path

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [project](../workshop/project.md).

**Context.** A maker edits a weave the running Workshop has loaded, rebuilds it, and wants the
rebuilt code in the running project with the weave's state kept. The Loom already owns that:
`Kernel::reload_from` swaps the code behind the same `WeaveId` and carries the state across,
behind the Manager's `zen.ReloadWeave`. What stood in the way was a file. A recipe with an empty
`artifact_dir` wrote exactly the file the plan had loaded, and a loaded file is mapped by the
process: Windows refused the link on it, and Linux let the linker change code under the running
program (both measured, in the research this record closes).

**Decision.** A rebuilt product never lands on the loaded file. A single-source recipe builds
into its own workspace, under `out/`; a CMake target builds where its project puts it; and the
HOST copies the product to where it will be opened from — the plan's file for a first
realization, a per-operation path beside the host for a reload — through one rule both hosts
wire into the executor, which spells no path. The executor's already-resolved arm opens one
`zen.ReloadWeave` conversation over the copy, bracketed by the operator offer, settled by the
booter. The plan's file is untouched by a reload, so a quit runs the old code next launch: the
row says `NOT DEFAULT`, and two explicit acts resolve it — promote (the image's bytes into the
plan's file, sibling then rename) and revert (a reload of the image before the last one). Load
after build is one action in two states, a toggle read by `b` and a button that re-sends the
finished build's own ask.

**Alternatives considered.**
- *Tried: relinking the loaded file in place* — the lane measured a refused link on Windows and a
  silently divergent image on Linux; pinned by the rebuild-of-a-live-stem section of
  `tests/build/run.cmake`.
- *Argued: copying the last image into place at an orderly quit* — refused by the founder: a
  killed Workshop would restart on an image nobody chose; promote is the maker's own act.
- *Argued: a pointer file every load path reads* — refused: a second spelling of which file a
  stem means, in a file no reader owned.
- *Argued: a `RealizeLast` shape for the button* — refused: the finished build's own ask, re-sent,
  is the same sentence and adds no grant; pinned by case `"RELOAD-2: after a plain build that
  succeeded and nothing armed, `B` is a button that loads the built artifact now"`.
- *Argued: reloading a provider+weave artifact's weave half* — refused: the catalog would keep
  the old image; pinned by case `"RELOAD-1: a provider+weave row is refused in words: reloading
  its weave would leave the catalog on the old image"`.

**Consequences.** A plain build no longer changes the file a restart loads; the product reaches it
when the maker loads it. `<stem>.reloads/` grows one file per reload and nothing prunes it. A
promotion that wrote over the previous image leaves nothing honest to revert to, and revert says
so. A changed shape is refused by the kernel before the incumbent is touched, and the road on is
a prepared replacement with an authored migration, which Workshop does not host yet.

**Laws supported.** [WL-PROJ-16](../workshop/project.md).
