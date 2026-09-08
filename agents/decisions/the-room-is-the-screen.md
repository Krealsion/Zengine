# The room is the screen, and the right column is a place

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [geometry](../workshop/geometry.md). It reverses the reservation half of
[the reserved column is nobody's to spend](the-reserved-column.md), and keeps that record's other
half: overlaps inside one owner's room, and a composition settled in cells before any metric.

**Context.** `screen_of` subtracted 28 columns and a two-cell gap from every screen's width
before anything asked for them, and called what was left the room. That subtraction was the
whole of what made the right column different from every other place: a pane standing there
could not be moved, sized or typed at, four separate code paths carried the same refusal
sentence, and a maker who took Info off the desk got thirty columns of nothing back. The arc
migrating Workshop's built-in panes into loadable weaves reached Info and stopped on it, because
a weave cannot be offered into a place the screen owns: `placement_of` answers `kOverlayStack`
for every runtime kind, the side region cannot be arranged into, and the catalog row that put
Info there is exactly the host furniture VD-19 is retiring. The reservation had to go before Info
could become a weave, and it is its own change rather than a clause of that one.

**Decision.** The room is the surface. `room_w` is the screen's width with nothing taken off it;
the right column is a PLACE at `w - kPanelCols`, resolving to the rectangle it always resolved
to, standing OVER the room rather than beside it. Every place is the maker's to author, so
`place_is_authorable` is gone rather than always-true, and the two refusals that read "is in the
reserved side column — the screen owns its place" are gone with it. A desk row may name the
column (`pane_unit::kRightColumn`, the word `right-column` in a setup file), because no row of
absolute coordinates can say "the right edge, the workspace's full height" on a screen whose
extent it does not know — and that sentence being sayable only by the screen was the reason the
column was the screen's. The shipped default desk says it, so a fresh session opens Info exactly
where it has always been and the strip beneath it is ordinary room.

**Alternatives considered.**
- *Leaving the reservation and giving Info a placement field on its offer* — refused by the
  founder in this arc's not-to-do list, and rightly: a pane telling the host where to put it is
  the host's furniture wearing the pane's name, and it would have made every weave's offer a
  negotiation about geometry.
- *Naming Info's office in host code as the right column's occupant* — refused for the same
  reason one step later: the host would still be the party that knows Info is furniture.
- *Absolute coordinates in the shipped desk* — measured impossible, not merely ugly. A place is
  two numbers on the fine lattice and a size is an amount; `28` and `39` are right for one
  screen and wrong for every other, and a desk shipped for everybody has no screen.
- *A `place_in` field on `SetupPane` beside the coordinates* — rejected as the same fact twice.
  `PanePlace::mode` already carries a named place: `kDefault` does not say what unit `x` and `y`
  are in, it says "put this where its place puts it". A second named answer belongs beside the
  first, and a separate field would have had to answer what a named place plus coordinates means.
- *Keeping the terminal pane's right edge at `w - kPanelCols`* — rejected: it is the reservation
  surviving under another name, in the one file that had just stopped making it. What it would
  have bought is real and is named under Consequences.

**Consequences.** The workspace is thirty columns wider at every extent, so every %-wide object
resolves against the bigger number — 60% of a 160-column surface is 96 cells where it was 78.
That is the move [the reserved column](the-reserved-column.md) refused when Info became
removable, and the reason it refused it does not apply here: it refused a room that changed with
which panes were open, so that hiding a list of names would resize a maker's material. This room
does not change with anything; it is the surface, at every moment, whatever stands on it. The
terminal overlay is eight cells wider at the minimum screen — it asked for 56 columns and was
given 48 because thirty of the surface's were spoken for. A stacked panel is wider too, and
gained the reachable columns to its right that the minimum screen never had: 48 of 48 left the
maker nothing, and 63 of 78 leaves fifteen. A panel now meets the right column at the smallest
screens, which is what an overlay is, and it is legible because a panel wears a boundary.

**And one cost this record does not pay for, stated rather than absorbed.** The terminal pane
reaches the screen's right edge, so with the shipped desk an open terminal covers the pane
standing there — HD-10's measured defect, returning. The reservation was doing two jobs, and
only one of them was "these columns are nobody's to spend"; the other was "the terminal cannot
silently erase what stands there", and every mechanism for that second job needs the screen to
know a pane is furniture, which is the knowledge this record removes. The overlap is pinned as
a measured fact (case `"HD-10: the terminal pane now covers the right column, measured"`) rather
than patched around, because the three candidate fixes — chrome on the terminal pane, a paint
order that puts the framed thing on top, or a ceiling that is this reservation under another
name — are three different products and the choice is the founder's. What is different from
HD-10 is that a maker can now move the pane out of the way, which under the reservation four
code paths refused to let them do.

**Laws supported.** [WL-GEO-02](../workshop/geometry.md), [WL-GEO-03](../workshop/geometry.md),
[WL-GEO-04](../workshop/geometry.md), [WL-PANE-01](../workshop/panes-and-windows.md),
[WL-PANE-08](../workshop/panes-and-windows.md).
