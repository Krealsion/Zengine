# One geometry draws a thing and hits it

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [geometry](../workshop/geometry.md).

**Context.** The Terminal pane's editable line was placed by its painter and re-derived by the
press path, and the completion list's windowing existed twice — right until the first scroll,
which is to say wrong only when nobody was looking. The same shape had already cost a defect one
level down: a press on a cell the Builder visibly covered took hold of the object underneath it,
because a panel's bounds were pixels only and the press never asked. A second copy of a
placement is correct exactly as long as one person wrote both copies.

**Decision.** The geometry that draws a thing and the geometry that hits it are one resolved
geometry. `terminal_input_place` resolves the editable line once, and the painter, the caret and
the press all call it; `completion_first_shown` is the list's one windowing, lifted out of
`completion_rows`. Workshop has no `click_*_bounds()` beside a `paint_*_bounds()`: a press may
have its own inverse, but the inverse reads the painter's place.

**Alternatives considered.**
- *A press path with its own arithmetic* — rejected: the completion list's windowing was found
  duplicated when the pointer first entered the pane, and lifted (`b30ab5d`, "Put the pointer
  inside the Terminal pane"); pinned by case `"HD-3: the click reads the SAME window the rows
  were drawn with"`.
- *A painted-cell mask for what a hand meets* — rejected: what a maker can press would then
  depend on the length of a label; occupancy walks the same `bounds_of` the painter is handed
  (`c1a5e35`, "Let a panel occupy the space a maker can see it occupying"; `0fe05af`, "Let a
  panel look as occupied as it is").

**Consequences.** Hit geometry follows presentation geometry across a resize with no path of its
own, on both media. The rule outgrew the Terminal: `occupied_at` and every pane's local inverse
read the resolved bounds the painter was handed, and the last see-here/press-there surface (the
old top band) was converted into a pane rather than given a second inverse.

**Laws supported.** [WL-GEO-01](../workshop/geometry.md).
