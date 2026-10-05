# The stack begins again at its top

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [panes-and-windows](../workshop/panes-and-windows.md).

**Context.** The stack seated reactive panes down one column and rationed it by the room's
height: a pane the column had no height for waited, unseen, and a launch, a reveal or a managed
open was refused (`no room for ... make the window taller`) even with the room beside the column
empty. A view run from the View Builder beside the Pane Manager was refused so (`5901b39`).

**Decision.** No room rations the stack. `bounds_of` walks the column as it always did, and a
pane that would pass the floor begins the column again at its top; one taller than the room
stands at the top. `seat_panes` is resolution alone, so every resolved row is seated and no door
refuses for room. A launched row is the newest, front-most by its rank, and selected, so it lands
in front, in sight. No free space is searched for and nothing is packed.

**Alternatives considered.**
- *Rationing by the column's height* — tried: a launch beside an empty part of the room was
  refused, in a window and in a terminal (`5901b39`).
- *A second column beside the first* — argued: the stack is wider than half the narrowest room,
  so the room's right edge would cut a second column there, and a second column runs out too.
- *A launch authoring a place* — argued: a launch would write a fact the weaver never authored,
  and the pane would leave the stack for good.
- *Each pass offset so the pane beneath shows* — argued: a second rule, off the room's edge; the
  Pane Manager and the arranging keys reach a covered pane.

**Consequences.** `waiting` is no state: the inventory says open, closed or gone, and a shrink
closes no pane but lays the same panes out again, one over another where the column ran out. The
order of a restore, the room and then the desk, no longer changes seating. One pass of the
column's fallback slots (`stack_slots_that_fit`) is still the room the desk's claim carries.

**Laws supported.** [WL-PANE-03](../workshop/panes-and-windows.md).
