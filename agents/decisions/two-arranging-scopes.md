# Two arranging scopes

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [arrangement](../workshop/arrangement.md).

**Context.** Pane management was a selector with submodes — select a pane, then a Move step, a
Size step and an edge-picking step — and a roster panel painted the state. When the contextual
surface arrived, moving and resizing a pane were plainly one maker intent (`bf35754`, "Context
opens beside the hand, and arranging is the scope a maker chose").

**Decision.** `PaneArrange{open, desk, pane, resetting}` replaces the selector. The one-pane
scope is bound to exactly its pane, admission before binding through `arrange_geometry_ready`:
its body moves it, its ring sizes it, and a press anywhere else is consumed with the sentence
naming the state. The desk scope opens with no pane addressed and every arrangeable pane answers
the pointer directly, topmost first; a press takes hold and makes that pane the keyboard's
target. The arranging keys are one vocabulary in both scopes. Arranging a pane is choosing it:
`enter_arrange_pane` writes `Panels::selected` after admission and nothing else, and the state's
visible statement is the rings, the legend and `arrange_status()` carrying the pane's state word.

**Alternatives considered.**
- *The selector with submodes* — retired; `manage.move`, `manage.size` and `manage.edge` are
  retired ids whose authored rows are kept verbatim as unknown (`git log -S'manage.move'` →
  `bf35754`).
- *A roster panel as the state's statement* — retired in the same commit; the rings are the
  statement and an invisible pane is recoverable by ear.
- *A selection prerequisite for the desk scope* — retired: a press is its own targeting.
- *A separate arrangement z-order, or writing `front` when arranging* — rejected: it spends the
  selection fact; pinned by case `"WUX-7: contextual Arrange lifts the pane it addressed, not
  the one in front"`.
- *Selecting before admission* — rejected: a refusal must leave the maker where they were;
  pinned by case `"WUX-7: a refused Arrange leaves the selection exactly where it was"`.

**Consequences.** `manage.arrange` (Return, on the desk) was earned by the desk's narrowing;
`manage.previous` rides `shift+tab` because the POSIX backend reads `ESC [ Z`. A hand and a key
author the same setup values; escape unwinds one level and rolls nothing back. The affordance
marks were put back on the canvas — they had been constructed in the wrong wire currency and
painted nowhere.

**Laws supported.** [WL-ARR-07](../workshop/arrangement.md),
[WL-ARR-08](../workshop/arrangement.md), [WL-ARR-09](../workshop/arrangement.md).
