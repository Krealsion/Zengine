# Two bands

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [planes](../workshop/planes.md) and [regions](../workshop/regions.md).

**Context.** Canvas row 0 carried four one-cell voices, each structurally unable to hold a row
of a real face; the band's budget composition retired the row, moved its facts into the foot and
left the cell empty rather than growing the workspace (`5b79afa`, "Compose the screen's chrome
against the budget its medium actually fits"). The layout run then sat in the footer, on the
status row that had named the arrangement all along, because a selector at the top seemed to
need either a one-cell row (zero face rows) or a sixth reserved row (which resizes the
workspace every share resolves against). A constraint stated as a prohibition hid the answer
that a budget would have shown (`4868c6b`, "Put the layout selector on the first row, where a
selector belongs"). Finally the top band painted in front of every pane while answering presses
on the tabs alone: a pane dragged under it was visually erased, still met the hand, and still
classified `open` (`3bfc2fd`).

**Decision.** Three regions tile the screen exactly: `kTopRows` (2) reserved, with the Layouts
pane standing on them by default; the body; the last `kBottomRows` (4) as the foot band.
`kTopRows + kBottomRows == 6` is asserted and `room_h` is byte-identical to what it was. The
foot band owns its whole rectangle (`kGroundOwn`), is in front of the panes, and occupies no
pointer space; `band_region` composes the notice first and `budget - 1` legend rows after. The
top rows belong to a pane.

**Alternatives considered.**
- *A sixth reserved row* — refused: it would resize the workspace; both phases were forbidden
  to spend it.
- *A one-cell top band* — refused: zero rows of a real face and bitmap glyphs, the exact defect
  the shared row was retired over.
- *The selector in the footer* — superseded (`4868c6b`).
- *The top band as screen chrome with tab-only presses* — removed; pinned by case
  `"WUX-12/SC-5+SC-7: a pane in front of the Layouts pane takes the press"`.
- *Converting the foot beside the top* — not done: the foot is where the tool speaks, and a
  panel backdrop drawn over it would erase the notice that just told a maker what happened; the
  utterance channel's reachability keeps it the screen's.

**Consequences.** The Info column is `room_h` cells rather than one more; the overlay stack is
one row closer to the Terminal pane, so their bounded overlap is two rows at the minimum screen
where it was one, and one row at the two heights above where it was none. A character medium
reads six rows of content where it read five and a gap. At a budget of one the band keeps the
notice while there is one; the notice is cut with a mark on screen and kept whole in the session.

**Laws supported.** [WL-FRONT-02](../workshop/planes.md), [WL-FRONT-03](../workshop/planes.md),
[WL-RGN-03](../workshop/regions.md).
