# A pane names its parts, and the names ride with its picture

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [pane controls](../workshop/pane-controls.md).

**Context.** An agent read a pane's words by message, each numbered by its place in one answer:
a redraw renumbered them, and a walk chose a Pane Manager row, a menu line or Info's `Height` by
what it said or where it fell. The panes already knew what each row and control meant -- a row
map records it for the press -- and nothing said it across the seam.

**Decision.** A pane names the parts a weaver acts on, from what each means: a run of a row's
columns (`PaneRowPart`, on `v4::PaneContent`, and a presented menu's lines on `v2::MenuShown`) or
a rectangle of its picture (`PaneCanvasPart`, on `v4::PaneCanvasContent`). The names travel with
the picture they name, judged with it -- a name once, on what the picture says -- and refused
whole with it. Workshop answers each beside its words, place and point (`PanePart`, on
`v3::PaneView` and `v2::DeskView`'s menu) and names its own menu's lines by their actions.

**Alternatives considered.**
- *A sentence of names beside the content* — argued: two messages could disagree about which
  picture a name is on, the race a caret beside its rows already has.
- *A name on each drawn row, label or run* — argued: a row holds several controls, and a part
  drawn as a box, a label and a run would need one of them chosen to carry its name.
- *Workshop naming what a pane draws* — argued: only the pane knows what a row means.

**Consequences.** An agent finds a part by its pane and its name, wherever a redraw put it. A
pane moving its rows onto the canvas names the same parts in its picture, and a name an agent
wrote still holds.

**Laws supported.** [WL-HAND-06](../workshop/pane-controls.md).
