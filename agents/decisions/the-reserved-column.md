# The reserved column is nobody's to spend

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [geometry](../workshop/geometry.md).

**Context.** The Terminal pane's right edge was the screen's, so at every extent it stood in the
28 columns `screen_of` had already subtracted for the side region, and the Info panel published
its properties and its footer into cells a later region then cleared to the canvas colour. The
panel read as stopped rather than covered, and a maker could not tell rows omitted from rows
hidden from rows destroyed (`58c88cf`, "Stop the terminal pane spending the column reserved
beside it"). The answer was already in the file: `screen_of` subtracts the side column for the
workspace three lines above where it placed the pane, and the overlay stack had been asserted
inside that number since the places were declared; the pane was the one placement older than the
discipline. Underneath lies an older refusal: the columns Info vacates stay vacant, because giving
them to the workspace would change what every share resolves to.

**Decision.** The reservation is the screen's, not any pane's. `screen_of` subtracts the side
column and the top rows whether or not a pane stands in them, and `room_w`/`room_h` are what
every share resolves against. The Terminal pane is anchored to the room's corner — it wants
`kTerminalWantW + (w - kScreenMinW)/2`, the room is the ceiling, and `terminal_x + terminal_w ==
room_w`. Overlaps inside one owner's room stay; no presentation reaches into a column or row the
screen reserved for another. The composition is settled in canvas cells before any text metric
is consulted; a metric only changes how much prose fits inside a placement it did not choose.

**Alternatives considered.**
- *Forbidding overlap generally* — rejected: it would forbid the three intentional overlaps
  (the completion list over the transcript, the picker over a slot, the pane over the workspace
  and the band); pinned by case `"HD-10: what the pane DOES cover is unchanged, and is on
  purpose"`.
- *Asserting only that the pane shares no cell with the side region* — measured insufficient: a
  pane whose right edge sits exactly on `panel_x` shares no cell and is still wrong, and that
  mutation passed the cell count while both edge assertions reddened. Both edges are asserted,
  at `kMinScreen` in the type system and over eleven extents times three metrics in the suite.
- *A second reservation tying the pane's height to `kStackRows`*, to remove the pane's overlap
  with the overlay stack — rejected: it would be a reservation `screen_of` does not make; the
  overlap is bounded (504 shared cells at the measured worst) and pinned as a known fact.
- *Giving Info's vacated columns to the workspace* — refused when Info became removable
  (`bdcc710`, "Let a maker put the properties away and get them back"): every %-wide object
  would change size because a maker hid a list of names.

**Consequences.** At 78–80 columns the pane gets the room; want and room agree from 94 columns
up; the price is eight cells of pane at the minimum extent, where its standing statement is elided
and marked below 81 columns. Hide Info and the column stays empty; move or remove the Layouts pane
and its rows stay empty and `room_h` is the number it was — the same rule applied at the other
edge when the top rows became a pane. The nine cases for this pushed the old single test binary
past COFF's section limit under MSVC.

Before the pane's right edge became the workspace's, the cost was measured at every extent this
composition lays out: the pane covered the full 28-column side region and between 8 and 37 of its
rows, so the Info panel published its lists and its footer and a later region erased them in the
same frame.

**Laws supported.** [WL-GEO-02](../workshop/geometry.md), [WL-GEO-03](../workshop/geometry.md),
[WL-GEO-04](../workshop/geometry.md), [WL-GEO-05](../workshop/geometry.md).
