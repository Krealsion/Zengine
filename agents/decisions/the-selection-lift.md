# The selection lift

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [planes](../workshop/planes.md).

**Context.** The pane a maker was using could sit behind another, with no way to bring it
forward for the duration of the work that did not also rewrite the authored order (`65cf9f1`,
"Give the desk edges, and let the pane you are using come forward"). `manage.front` already
meant "and I mean this permanently".

**Decision.** `Panels::selected` is a press's memory — session-only, never persisted, none at
start, resolved to a pane by `selected_pane`, with four writers: the press line,
`enter_arrange_pane` after admission, `open_source`, and Escape's fallthrough.
`effective_pane_order` is the authored permutation with the selected pane lifted, the one answer
every consumer meaning "in front right now" spends; `presentation_order` remains the authored
base for persistence and `reset order`. The lift is a rotation and never a write. The transient
planes stay above the panes.

**Alternatives considered.**
- *Writing `front` on selection or arrangement* — rejected: no rank moves and nothing reaches a
  file; pinned by case `"WUX-5: the selection lift never reaches the file, and no session starts
  with one"`.
- *Testing occupancy twice for the selection and the keyboard candidate* — rejected: the
  candidate derives from the selection through declared candidacy
  ([the-keys-go-where-last-pressed](the-keys-go-where-last-pressed.md)).
- *A clearing path for an unseated selection* — unnecessary: a selection that is not seated
  lifts nothing, `bounds_of`'s discipline.
- *A lift that reaches the transient planes* — rejected: a selected pane must never be drawn
  over the menu a maker just opened on it; pinned by case `"WUX-5: a transient surface stays
  over the pane it covers, selected or not"`.

**Consequences.** `paint_panels` ascending, `occupied_at` descending, `pane_is_covered` and the
desk's pointer walk cannot disagree, because there is nothing to disagree about. A refused
Arrange leaves the selection exactly where it was. Nothing that means "in front right now" may
call `presentation_order`.

**Laws supported.** [WL-FRONT-04](../workshop/planes.md), [WL-FRONT-05](../workshop/planes.md),
[WL-FRONT-06](../workshop/planes.md), [WL-FRONT-07](../workshop/planes.md).
