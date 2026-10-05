# The fine lattice

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supported is in [geometry](../../../agents/workshop/geometry.md).

**Superseded.** The lattice became the whole pixel: [the-whole-pixel](../../../agents/decisions/the-whole-pixel.md). What
follows is the decision as it was made.

**Context.** Pane placement and extent were whole canvas cells, so the finest movement a pointer
can honestly report on the shipped face — one pixel — could not be authored, while the other
constraint stood unchanged: no monitor pixel may become authored truth, because the setup is
medium-independent and a whole-cell publisher's bytes must keep meaning what they always meant
(`07eb620`, "Give pane arrangement a sub-cell lattice, and every resize edge its anchor").

**Decision.** The canvas geometry carries a sub-cell remainder of 1/48 cell (`kCellSubs`).
Workshop resolves panes in sub-units end to end as `FineRect`, a compiler-distinct lattice from
`ui::Rect`; conversion is only `fine_of_cells`/`cells_covered`, and a cell value enters the
lattice by one multiply, at `project_pane`. One quantization law serves every consumer: a
presenter of device grain g shows a fine span [L, R) on device units [floor(L/g), floor(R/g))
and hit-tests by the same floor, so the first painted device unit answers the hand and the one
before it does not. The TUI quantizes at its own projection and never writes back.

**Alternatives considered.**
- *Authoring in device pixels* — rejected: no medium here publishes a trustworthy per-axis
  device-pixel scale for a canvas cell, so the `pixels` unit is declared and refused at
  projection instead ([setup-format-v3](../../../agents/decisions/setup-format-v3.md)).
- *One rectangle type for both lattices* — rejected: a pane rectangle passed through
  `workspace_cell_x/y`, the document's whole-cell conversion, would be a silent mix of grains;
  the distinct type makes that a compile error, and the round trip was pinned with it; the
  type survives as `PixelRect`.
- *Rounding the projection to the nearest device unit* — rejected: flooring both edges is what
  makes "what you see is what you can grab" an identity rather than an intention; pinned by case
  `"one quantization law -- a span lands on device units by flooring both edges"`.

**Consequences.** A one-pixel drag moves a pane by exactly one pixel of lattice and the file
keeps every sub-unit. Screen furniture, the document and every placement default stay whole
cells. Exact-cell values render byte-identically on the TUI, and any number of frames rewrites
nothing. The setup format moved to version 3 with a retained version-2 reader that scales by
48, and introspection says a fine value as an exact reduced fraction — the spelling later
refined per face in [the-face-reports-the-unit](../../../agents/decisions/the-face-reports-the-unit.md).

**Laws supported.** [WL-GEO-06](../../../agents/workshop/geometry.md) and
[WL-GEO-07](../../../agents/workshop/geometry.md) until the whole pixel; they stand on
[the-whole-pixel](../../../agents/decisions/the-whole-pixel.md) now.
