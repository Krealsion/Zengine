# Content-sized popups

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [contextual](../workshop/contextual.md) and [keyboard](../workshop/keyboard.md).

**Context.** A four-action context menu cost six rows and thirty-eight columns because two of
them announced that a menu of actions contains actions, and contextual help opened at the
window's corner however far the hands were from it (`65cf9f1`). The full hotkey view took the
overlay column — floor to ceiling with nothing selected — for a list 36 characters wide
(`d1bf7f4`, "Let the hotkey view fit what it says").

**Decision.** `ContextMenu` captures the opening press's canvas cell — the gesture's place, not
the subject's — and `context_bounds` re-derives the rectangle at every paint and press from the
level's rows: `popup_bounds_at` is one measurer (`region_cells_for`, the text measurer's
inverse), one chrome (`chrome_outer_of`) and one clamp inside the band the overlay stack
respects, capped at `kContextMaxCols`. The first row is an action: painted row i is population
row i. The hotkey view anchors at the selected pane's visible outer top-left, is sized from
`hotkeys_rows` through the same measurer, is keys-modal, and owns no pointer space.

**Alternatives considered.**
- *A title row and a hint row on the context menu* — removed: they were the width floor, and
  the hint was a second cheat sheet for rows the band's legend already says (`git log
  -S'kContextHeadingRows'` → `65cf9f1`).
- *A fixed width, a floor-to-ceiling column, a `kPickerRows` floor for the hotkey view* —
  retired; measured on the shipped face at 123x83 cells, 837x885 px before and 322x897 after for
  the same 48 rows; on a 100x60 terminal 57x49 before and 36x48 after (`d1bf7f4`).
- *The room-under-the-anchor law* — replaced by content owning the extent; the old anchoring
  case had let a 48-row list on a 44-row screen "prove" the anchor by filling the band.
- *Anchoring at the subject rather than the gesture* — rejected; the keyboard entrance is
  `anchored == false` and opens at the overlay stack's corner, a deterministic placement rather
  than an invented pointer.

**Consequences.** A level taller than the room keeps the room's height and `list_window` says
what was cut; entering a group re-derives at the same anchor. Every label in `Order` and `Reset`
was checked to say what it does without the title. Nothing is stored; `occupied_at` never reads
the view and `screen_of` cannot see it. `attention_bounds` deliberately did not follow.

**Laws supported.** [WL-CTX-03](../workshop/contextual.md),
[WL-CTX-04](../workshop/contextual.md), [WL-KEY-10](../workshop/keyboard.md),
[WL-KEY-11](../workshop/keyboard.md).
