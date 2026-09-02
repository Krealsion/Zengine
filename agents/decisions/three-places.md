# Three places, and only one of them is the screen's

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [panes-and-windows](../workshop/panes-and-windows.md).

**Context.** Workshop had two places and no way to say so: the Builder's painter wrote its rows
at the stack's column, Info's wrote its labels at `Screen::panel_x`, and a counter in the
painting loop named a kind to decide which panels earned a slot (`977f269`, "Give a panel a
place instead of a painter that knows a column"). A movable Info was already refused: `room_w`
is what every share resolves against. When the top band became a pane there was a third place,
and three consumers each said `== kOverlayStack` — the same set written as a list somebody has
to extend (`3bfc2fd`, "Let a maker move the layout tabs, because they are a pane like the
rest").

**Decision.** A kind declares one of three places in the catalog, and `placement_bounds` turns
a place plus a screen into the rectangle. The side region is the screen's — place-fixed, no
override; the overlay stack and the top band are the maker's, a developer default an authored
row lays over per axis. `place_is_authorable(where)` is that exclusion in one sentence and every
consumer asks it. `kinds_placed_in` pins the side region and the top band at one kind each, at
compile time. A band-anchored or authored pane spends no reactive slot and cannot wait for one.
The host clips and never rewrites. Seven states, one classifier, one precedence. The picker
keeps presence and arrangement never touches it.

**Alternatives considered.**
- *Each painter reading the screen* — retired: `paint_info` has no `Screen` in it any more
  (`977f269`).
- *Spelling the exclusion at each consumer* — replaced by one predicate when the top band became
  authorable (`git log -S'place_is_authorable'` → `3bfc2fd`).
- *Containment by one pane as "covered"* — rejected: two panes that each cover half of a third
  leave nothing showing; pinned by case `"WIND-2: coverage is the UNION of what is in front, not
  containment by one pane"`.
- *Rewriting an off-room ask to fit the canvas* — rejected: `bounds_of` clips for drawing, hit
  and capacity and never rewrites; pinned by case `"WIND-2: a partly off-room pane is clipped,
  and its intent is not rewritten"`.
- *Letting arrangement toggle participation* — rejected; pinned by case `"ARR-0: participation
  stays the picker's; arrangement does not add or offer"`.

**Consequences.** `waiting` means only that the reactive default ran out of tiles; a pane with a
pixel axis and no tile left is `refused`, because a taller window would not help; one visible cell
is `open`; the state column is eleven cells because `unresolved` is ten bytes. A side-region row's
authored geometry is retained in the file, never spent, and arrangement names the reservation it
hit. `kinds_placed_in` has no runtime witness — its pins are compile-time — and an unresolved
picker row carries `kNoPaneKind` so nothing can present it as the Builder.

**Laws supported.** [WL-PANE-01](../workshop/panes-and-windows.md),
[WL-PANE-03](../workshop/panes-and-windows.md), [WL-PANE-08](../workshop/panes-and-windows.md),
[WL-PANE-09](../workshop/panes-and-windows.md), [WL-PANE-10](../workshop/panes-and-windows.md),
[WL-PANE-12](../workshop/panes-and-windows.md).
