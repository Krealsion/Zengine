# The whole pixel

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws
it supports are in [geometry](../workshop/geometry.md).

**Context.** Pane geometry lived on a lattice of 1/48 cell
([the-fine-lattice](../../docs/history/decisions/the-fine-lattice.md)): fine enough for a window
pixel, at four sub-units to it, and finer than any medium here can say.
Three of every four values a weaver could hold were invisible on every face, the `pixels` unit
was declared and refused, and the screen's fixed parts were whole cells that held fewer rows of
the shipped face than they were sized for. The window's picture is the one a weaver sees whole;
a terminal's is that picture floored to its cells.

**Decision.** Workshop's unit is the whole canvas pixel, twelve to the cell (`kCanvasCellPx`).
Everything Workshop lays out, resolves, hit-tests and says is whole pixels, as `PixelRect`; a cell
value enters by one multiply (`pixels_of_cells`) and leaves only by `cells_covered`. The picture
crosses to each Skin in pixels: the window draws it 1:1, a terminal floors every span by the one
quantization law and reports its room as its cells times twelve, so its geometry is derived and
never written back. The screen's two bands are fitted to the text they hold (`band_px_for`): the
Layouts pane's rows and the foot's rows in the face's own metric, inside one device unit of
chrome, and whole cells where a face sets no type. A pane authored in `pixels` is presented at
exactly its pixels on every medium; there is no refused state.

**Alternatives considered.**
- *Keeping the 1/48 lattice* — rejected: a value no medium can say is a value a weaver cannot
  see or grab, and a one-pixel drag already moved four of them; pinned by case `"a one-pixel drag
  moves a pane by exactly one pixel of lattice"`.
- *Bands of whole cells* — rejected: two cells of the shipped window are 24 pixels, one 18-pixel
  row of type and no boundary; pinned by case `"the screen's furniture cannot see a pane, open or
  closed"`.
- *A terminal laying out its own geometry* — rejected: two authorities for one desk, and a visit
  would author; pinned by case `"the TUI projects a pane onto its covered cells and rewrites
  nothing"`.
- *Rounding an old fine value to the nearest pixel* — rejected: an old desk opens where it was
  painted, which is its edges floored, not rounded; pinned by case `"pixel geometry survives the
  setup file without losing a pixel"`.
- *A place measured from the canvas* — tried: an old desk's panes at y 24 lay 18 pixels under
  the window's band, every shipped walk failed at its first step until the shipped desks were
  lowered for the window, and one desk was arranged differently in each medium; pinned by case
  `"an authored place is measured from the room's top-left, not an offset from the default"`.

**A place is measured from the room.** With the bands fitted, the room's top is not the same
pixel on every face: 42 in the shipped window, 24 in a terminal. A place measured from the room's
top-left, as a default place already was, stands directly under the band on every face. An older
file's places were measured from the canvas while the room began two cells down, so each reads two
cells less.

**Consequences.** A one-pixel drag moves a pane one pixel, and the file keeps every one. An old
desk's extents open to the pixel they painted and its places where they stood in their room; a
v3 pixel axis under one cell is raised to one cell, and a place above the old room's top lands at
the room's top. No pane is placed over the top band: the Layouts pane stands there by default, and
resetting its place is how it returns. Canvas panes that ask for the old shapes are answered in
them, four sub-units to a pixel. A terminal reads the same desk in cells, marked `~` where a pixel
is not a whole cell.

**Laws supported.** [WL-GEO-05](../workshop/geometry.md), [WL-GEO-06](../workshop/geometry.md),
[WL-GEO-07](../workshop/geometry.md), [WL-PANE-11](../workshop/panes-and-windows.md).
