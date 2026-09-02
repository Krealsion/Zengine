# Half the surplus

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [panes-and-windows](../workshop/panes-and-windows.md).

**Context.** An overlay-stack slot was 48 by 9 cells at every extent, so a maker with a 200-column
surface gave a stacked panel — and the external pane inside it — exactly the room the 78x22
minimum gives it (`02d2603`, "Share the wider room with the maker").

**Decision.** `placement_bounds` resolves a slot's width to `kStackW + (room_w - kStackW)/2`,
the minimum plus half the room's surplus, floored, while its column, row, height and gap are
untouched. It is `screen_of`'s own half-share rule with `kStackW` as the base. Every cell a slot
gains is paint and pointer alike: the frame painter fills the whole rectangle, occupancy owns it,
and a press inside it is answered with the panel's sentence. An external pane's body is its slot
less its header rows, and the fitted room over it is granted to the provider whenever the body
changes.

**Alternatives considered.**
- *A full-width slot* — rejected: it leaves zero reachable workspace columns beside the panel,
  and the stack/terminal overlap would have been 3,033 cells where the half-share's worst case
  is 504 (`02d2603`).
- *Rounding the half up* — rejected: at 79 columns the surplus is exactly one, and rounding up
  spends it; floored, the odd column stays the maker's.
- *A threshold, a cap or a new constant* — rejected: one expression.
- *A width edit buying a slot* — refused: `stack_slots_that_fit` reads `y` and `h` only; pinned
  by case `"WIND-1: the minimum composition is byte-identical, and a width buys no slot"`.

**Consequences.** Reachable workspace columns beside a panel: 1, 9, 21, 61 and 281 at 79, 96,
120, 200 and 640 columns of surface. The minimum composition `{0,1,48,9}` is byte-identical.
The stack/terminal overlap grew from 432 to 504 worst-case cells and is bounded by
`kTerminalWantW`, because the pane's left edge moves right at the rate the slot's right edge
does. A provider's grant follows the widened body through one `fit_region` call — 8x48 at the
minimum, 8x109 at 200x60, 5x71 and 5x163 under the 8x18 face; a dragged edge, a widened room and
a hidden title all reach it by that one door. What a grant carries, and that an unchanged
capacity sends none, is the pane protocol's law.

**Laws supported.** [WL-PANE-04](../workshop/panes-and-windows.md),
[WL-PANE-05](../workshop/panes-and-windows.md), [WL-PANE-06](../workshop/panes-and-windows.md).
