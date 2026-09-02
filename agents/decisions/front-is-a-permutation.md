# Front is a permutation

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [setup-file](../workshop/setup-file.md) and [planes](../workshop/planes.md).

**Context.** The setup needed a durable front order once a maker could author the window
(`3ecaedd`, the commit that authored the window). The order then had nowhere honest to go on
the canvas: it held three root lists, so painter's order was global across kinds — every rect,
then every label, then every text region. Place the Builder over the Info column and send it to
front, and `occupied_at` answered Builder while the terminal drew Info's prose in the same cell
(`2d0689a`, the commit that made pane front order visible).

**Decision.** `front` is a canonical rank: a permutation of 0..n-1 over all authored rows,
unresolved ones included, so reset writes bytes identical to a setup that was never reordered.
`panels.open` is never reordered — `seat_panes` walks the setup list, a reactive slot is counted
over that same list, and no ordering operation writes what either reads, which is the whole of
"raising a pane cannot move it". The canvas is an ordered list of planes, and Workshop's
publication order is the whole depth story: the workspace, one plane per pane in
`effective_pane_order` ascending, the affordances, the picker and overlays, the foot band, the
Terminal.

**Alternatives considered.**
- *`max + 1` as a front counter* — rejected: an operation trace; alternating front(A)/front(B)
  produces the same two orders forever while the integers grow, so a legal gesture would
  eventually fail on a setup for which a bounded spelling always existed; pinned by case
  `"WIND-2: 10,000 alternating ordering operations stay inside 0..n-1"`.
- *A secondary key for ties* — unnecessary: a permutation has no tie.
- *Ranking seated rows only* — rejected: the presented order is the permutation restricted to
  what was seated, and a restriction of a total order is a total order, so an absent pane keeps
  its exact place for free.
- *Three root lists per primitive kind* — measured wrong in both directions with only one of
  them wrong, which is why a case that reversed a vector had passed (`2d0689a`); replaced by
  `SurfaceLayer`, a position in a vector with no identity, z, opacity or clipping tree.

**Consequences.** Ordering changes paint order and nothing else; hit order is the exact reverse
of paint order; a provider's text cannot bury the picker that recovers it; a gapped or
duplicated rank is refused. The plane list raised the surface floors from 59 to 66 and 5 to 6,
and workshop's from 473 to 482 with zero slack.

**Laws supported.** [WL-PANE-07](../workshop/panes-and-windows.md),
[WL-FRONT-01](../workshop/planes.md), [WL-SETUP-07](../workshop/setup-file.md).
