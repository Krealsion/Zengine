# One press claims one gesture

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [arrangement](../workshop/arrangement.md).

**Context.** A gesture begun on the workspace owns the pointer until its release (`c1a5e35`,
"Let a panel occupy the space a maker can see it occupying"). Two defects said what a release
owner is for: opening the Terminal pane mid-drag swallowed the release and stranded
`drag.active` with the button up, after which a bare motion moved an object nobody was holding
(`b30ab5d`); and a pane gesture released under the Terminal stayed live and followed the pointer
afterwards (`2d0689a`). Sweeping a text selection then needed a record of its own (`235141b`,
"Make TextBox feel like a text box").

**Decision.** `PaneGesture` holds an identity, an edge and the size at the press — no rectangle,
no live position — so every motion proposes `base + (pointer - press)` and nothing crossed moves
it. `end_held_gestures()` is the one release owner: every branch that can see a release calls
it, and what to tell the maker is the caller's. `forget_removed_selection()` clears on
membership, never on presentation, inside `apply_setup`. `Session::text_drag` holds which
editable line a press began sweeping and nothing else; every motion re-resolves the current
geometry through the press's own functions and hands the component a column.

**Alternatives considered.**
- *Accumulating motion deltas* — rejected: crossing another pane, the Terminal, or a reorder
  would then change who is being moved; pinned by case `"WIND-2: one press claims one gesture,
  and crossing anything does not move it"`.
- *Ending only one gesture kind per mode* — rejected: a gesture begun under one mode is released
  under another, and ending one kind leaves another alive with the button up (`2d0689a`).
- *A capture framework* — rejected: three records and one function, four with the tab drag.
- *Clearing the selection on a presentation state* — rejected: a waiting, refused, covered,
  off-room or unresolved pane is still a pane the setup names and still reachable by stepping;
  pinned by case `"WIND-2a: a removed target leaves no stale selection, submode or heading"`.
- *Re-testing the row mid-drag* — rejected: a hand that wanders off the line keeps sweeping it
  by column, which is what keeps the selection stable.
- *Occluding motion at a panel's edge* — rejected: stopping a drag there would clamp the
  document, a panel's presence becoming visible in what a maker may author (`c1a5e35`).

**Consequences.** Outside arrangement an addressed pane behind another claims no press and no
address auto-raises. A reference leaving the setup clears the address and its gesture, closes
the one-pane scope silently (the removing operation's sentence is on the notice), and leaves the
desk scope open. A drag left of the slice steps one character per motion; a text drag begins
only on paths that consume the press; release keeps the selection.

**Laws supported.** [WL-ARR-01](../workshop/arrangement.md),
[WL-ARR-02](../workshop/arrangement.md), [WL-ARR-03](../workshop/arrangement.md),
[WL-TEXT-14](../workshop/text-box.md).
