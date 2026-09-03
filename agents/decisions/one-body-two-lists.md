# One body, two lists

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [info-body](../workshop/info-body.md).

**Context.** A `SurfaceTextRegion` one cell tall holds (12 − 2·inset)/18 = zero rows of this
repository's face, so the property editor was drawn in cells and got a cell medium's mark rather
than the caret bar it was built for; a row could not simply be given two cells, because a
two-cell region covers the property beneath it (`8b023ca`, "Give the Inspector's property body
real rows, real bounds and a real window"). The OBJECTS list's capacity was a constant, and
three silences stood: a committed value ran off the canvas unmarked, `paint_info` had no bottom
bound, and a larger population said nothing (`0507a40`, "Give the Info panel's two lists one
bounded body and one row budget").

**Decision.** `info_body_place` is the whole Info body, resolved once — place, rows of the
active medium's type, sharing, value width, windows — and every consumer calls it. The body is
one region and a property row is one of its rows; `fit_region` answers with the text inset inside,
so nothing in Workshop multiplies a font metric. The vertical window is `list_window`, derived
every paint. The row maps are inverses and a press is never rounded to a cell. A resting value is
fitted and a live draft is windowed. A `SurfaceExtent` must not drop a live draft.
`share_body_rows` is max-min fair sharing. `OBJECTS` is the region's first prose row. An object
row is fitted whole and a press on it selects in command mode only.

**Alternatives considered.**
- *Two regions, one per list* — rejected: splitting the panel's cells needs `fit_region` read
  backwards, a second arithmetic beside the one function that turns a metric into a capacity;
  pinned by case `"HD-7: neither list paints through the other, at any extent"`.
- *A constant list capacity* — retired; measured at 78x25, 120x40, 240x80, 78x22, 80x38 and
  80x70 with twenty objects, character for character (`0507a40`).
- *A fixed 50/50 split* — a consequence, not a decision; pinned as properties over every budget
  from 0 to 200.
- *A scroll offset, a session field or a scroll gesture for the list* — none; the selection
  already decides what is visible (`e887452`, "Say what the screen is not showing").
- *Rounding a press to a Workshop cell* — rejected: an 18-pixel row against a 12-pixel cell
  names the wrong property for most of the body; pinned by case `"HD-8: the graphical press is
  not rounded to a Workshop cell"`.
- *Rebuilding rows on every `SurfaceExtent`* — repaired: a dragged window silently threw away a
  live draft; pinned by case `"HD-5: a surface extent does not take a maker's hands off a
  draft"`.
- *A press beginning an edit, or selecting during a live draft* — refused: changing objects
  rebuilds the rows a draft cannot survive.

**Consequences.** 25 cells of body is 16 rows of an 18-pixel face and 25 rows of a cell medium.
`completion_first_shown` is deliberately not the same function as `list_window`; it anchors to
the tail. `inspector_focus` exists though the editing row and the cursor are the same row today
by reachability. `component/text_box.hpp` was byte-identical through the change.

**Laws supported.** [WL-INFO-01](../workshop/info-body.md),
[WL-INFO-02](../workshop/info-body.md), [WL-INFO-03](../workshop/info-body.md),
[WL-INFO-04](../workshop/info-body.md), [WL-INFO-05](../workshop/info-body.md),
[WL-INFO-06](../workshop/info-body.md), [WL-INFO-07](../workshop/info-body.md),
[WL-INFO-08](../workshop/info-body.md), [WL-INFO-09](../workshop/info-body.md),
[WL-INFO-10](../workshop/info-body.md).
