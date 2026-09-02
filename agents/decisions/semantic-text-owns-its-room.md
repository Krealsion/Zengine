# Semantic text owns its room

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [regions](../workshop/regions.md).

**Context.** The graphical Workshop drew Info and the Terminal in a real face and everything
else in a 5x5-in-12px bitmap letterform. A canvas cell is twelve device pixels and the face's
line is eighteen, so a one-cell region holds zero rows; what can migrate is a run of rows
(`eb309a0`, "Set Workshop's ordinary prose in the medium's own type"). The workspace object's
name was the hard case: a region takes its rectangle, and the object's rectangle is authored
material (`1a3c775`, "Let semantic type sit on material somebody else owns"). Given the room to
the workspace's right edge, a six-cell object with a 32-byte name planned 564 px over 72 px of
material — nine characters legible on the body, twenty-three drawn on a `kMuted` backdrop in the
name's own `kMuted` ink (`50d748f`, the commit that bound a name to its material).

**Decision.** `panel_prose_place` + `panel_prose_region` is the one call for a panel whose
rectangle is its own. The Builder is a region composed by explicit priority, facts dropped whole.
The workspace object's name is a `kGroundBeneath` region over its own rectangle, bounded by the
material it names: `min(object width, workspace right edge − x)` by the object's height, each
floored at one cell, so a name that does not fit is marked rather than fading.

**Alternatives considered.**
- *An ordinary region over the object* — built and measured: it turns every object into an
  empty dark box, padding twelve cells of `#` into spaces.
- *Rows carrying the object's role as a ground* — built and measured: 12h − 4 = 18k has no
  integer solutions, so a 4/16/10-pixel band cycles that the strips cannot reach, and on a
  monochrome terminal the material becomes a blank rectangle.
- *A region ground that reconstitutes material* — built and refused: it would cut the name at
  the body's width or paint material across a workspace that has none (`1a3c775`).
- *A colour or role for the overrun* — rejected: nothing reads on both a `kFill` body and a
  `kMuted` backdrop, `kAccent` means the pointed thing, and a fifth role is refused; the answer
  is the bound (`50d748f`).
- *`paint_panel_row`, the cell-lattice row spelling* — gone with its last consumer (`git log
  -S'paint_panel_row'` → `5b79afa`).

**Consequences.** Fifty plan quads became seven; the cell projection is byte-for-byte what
`paint_panel_row` wrote (15847 bytes against 15847, zero hunks). A one-cell object shows its
name in cells with no `if (h < N)` written anywhere. `kMaxNameLen` went 32 → 64, the number
traced rather than inherited. The Builder's `said…` block wraps into exactly the rows that
survived its budget; a character medium's nine-row budget selects every fact.

**Laws supported.** [WL-RGN-01](../workshop/regions.md), [WL-RGN-02](../workshop/regions.md),
[WL-RGN-04](../workshop/regions.md), [WL-RGN-05](../workshop/regions.md).
