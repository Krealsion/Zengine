# The keyboard reaches a pane, and leaves any

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [focus](../workshop/focus.md).

**Context.** Giving a pane the keys took a press, and a pane that keeps Escape in a mode of its own
kept the keys until one: no keyboard path into a pane, and a trap out of some (WCAG 2.1.1, 2.1.2).

**Decision.** In command mode a selection moves without the keys: `focus.next` and
`focus.previous` walk the panes in reading order, wrapping; the arrows take the nearest pane past
the selected one's edge, past its centre where panes overlap, scored by the gap along the direction
plus twice the misalignment across it, a tie the front-most. The selected pane wears bracketed
corners where text is a cell, and two rings outside its edge where an edge is finer, since a corner
cell there would stand on its words. Only `focus.enter` points the keys, and it sends no press.
`focus.leave` is the one host row of the global class: it closes any mode and gives Workshop the
keys, the pane left still selected. Its default chord is `ctrl+q`.

**Alternatives tried.**
- *`ctrl+g` for `focus.leave`* — refused: Info's `info.value.grab` and Flow's `ask-generate`
  declare it, and a global row on it refuses both panes' rows. `ctrl+q` was the one control letter
  free on both media; the Hotkeys guide's example moved off it.

**Alternatives argued.**
- *Selection by Tab in front order* — refused: the front moves with each selection, so the walk
  would reorder itself; reading order is the geometry the weaver sees.
- *`focus.leave` a row a pane may own* — refused: a trap is what it exists to break.
- *No default chord* — refused: the trap would stay open for a weaver who has not bound one.

**Consequences.** A weaver's keymap file that binds `ctrl+q` elsewhere meets the collision law,
naming `focus.leave`. Return, Tab and the arrows in command mode are the selection's.

**Laws supported.** [WL-FOCUS-12](../workshop/focus.md), [WL-FOCUS-13](../workshop/focus.md).
