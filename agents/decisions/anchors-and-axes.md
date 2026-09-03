# Anchors and axes

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [arrangement](../workshop/arrangement.md).

**Context.** Three measured defects, one per rule. A default pane resolving to 89 cells with
four on screen answered one rightward step by authoring five cells' worth, because the first
edit captured the visible rectangle (`2d0689a`). The first resize law said an edge names an axis
and a direction so a resize writes size and never place — and pulling the top edge moved the
bottom edge instead (`07eb620`). The repair judged a pane-window proposal as one indivisible
transaction, so a diagonal drag along the left wall froze (`0c6481d`, "Let independent axes
settle independently in pane arrangement").

**Decision.** A resize begins from the resolved window: `managed_bounds().resolved`, the
unclipped ask captured at the press, while `rect` owns painting, occupancy, coverage and the
handles. Every edge preserves its opposite anchor, and a corner the corner across from it.
Right and bottom pulls leave a default place reactive by not writing it; a left or top pull
authors place and size as one axis-local transaction through `author_pane_window`, the one
gesture door. Independent axes settle independently, refuse-never-clamp per axis, and an axis a
gesture did not change is no proposal and writes nothing.

**Alternatives considered.**
- *Resize writes size and never place* — reversed after measurement; pinned by case `"WUX-2:
  the reported top-edge defect is dead -- the bottom edge holds still"`.
- *Capturing the visible rectangle at the press* — rejected (`2d0689a`); the affordance stays on
  the visible boundary, and its delta applies to the resolved window.
- *One whole-window transaction* — rejected: a move blocked on one axis lost the other axis's
  legal motion; pinned by case `"WUX-2a: a move blocked at the left wall still follows the hand
  down"`.
- *Clamping a blocked axis* — refused; pinned by cases `"WUX-2a: a move past two walls at once
  writes nothing"` and `"WUX-2a: a refused nudge does not author a reactive place"`.
- *Refusing left-edge moves* because a left edge moving the place is two writes for one gesture
  — the earlier objection, answered by making each axis one door-judged transaction rather than
  by refusing the geometry a hand plainly means.

**Consequences.** A refused height can never leave a moved top edge behind; a refused
single-axis step cannot author a reactive place as a side effect. `author_pane_place` and
`author_pane_size` remain the value doors, atomic whole. `PaneWindowProposal` says per axis
which edge authors the place, and `managed_window_base` is the one spelling the keys, the
pointer and the axis fallbacks measure from — later reused by the coarse step and by the Pane
Manager's typed axes.

**Laws supported.** [WL-ARR-04](../workshop/arrangement.md),
[WL-ARR-05](../workshop/arrangement.md), [WL-ARR-06](../workshop/arrangement.md).
