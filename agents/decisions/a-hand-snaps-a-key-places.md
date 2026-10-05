# A hand snaps, a key places

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [arrangement](../workshop/arrangement.md).

**Context.** Arranging moved a pane by whole pixels under the hand, by a cell under the arrows
and four under the coarse keys, and to a value typed in Info, and nothing snapped: lining two
panes up was a matter of pixels by eye. The View Builder's canvas already snaps a place by hand
to the other elements' edges and its own (`view_builder::snap`), marks the edge met while held,
and sets every snap aside while Alt is held.

**Decision.** A hand's proposal snaps before the one gesture door (`snap_pane_window`): each
edge it moves comes to the nearest line within `kPaneSnapReachPx` -- the room's edges and every
other pane's on the screen -- that a pane's rules allow. The line met is kept in the gesture
(`PaneGesture::met_x`, `met_y`), marked across the room on the affordance plane a device unit
wide, and said in the status. Alt held sets it aside; the keys and a typed value place exactly.
Every motion snaps afresh from the press, so a snap leaves an ordinary place and size in whole
pixels: no anchor, no lock, nothing in a setup or a session.

**Alternatives considered.**
- *A reach of a whole cell* — argued: a terminal hand one cell from a line would be pulled the
  whole cell, so no pane could stand a cell from another by hand there.
- *The View Builder's reach of half a cell* — argued: a desk's panes are hundreds of pixels
  across, and a reach a little longer is still under a cell.
- *Sharing `view_builder::snap`* — argued: it rules a view's elements, a grid and a view's size;
  the desk has no grid and its own rules (a place never negative, an extent at least a cell).
- *Marking only the edges that met* — argued: a line across the room is the View Builder's mark,
  and it reads the same in a window and in a terminal.
- *Snapping the keys* — argued: a key's step is exact by its law (WL-ARR-08); near an edge one
  press would move a pane by another amount.

**Consequences.** A hand within reach of the room's edge comes to it rather than being refused,
so the cases pinning a wall's refusal move the hand beyond the reach; a pane standing on a line
stays on it until the hand passes the reach. No file, protocol or vocabulary changed.

**Laws supported.** [WL-ARR-17](../workshop/arrangement.md).
