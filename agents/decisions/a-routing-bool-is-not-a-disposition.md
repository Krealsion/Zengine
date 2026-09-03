# A routing bool is not a disposition

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [press-chain](../workshop/press-chain.md).

**Context.** `info_press` answered "the caret MOVED" where its caller asked "did you CONSUME this
press". The two agree for exactly as long as every press landing on a live draft also moves it —
so a maker pressing where the caret already was fell through the property editor, the controls and
the object list, and the panel wrote "Info is here -- nothing under it can be taken hold of" over
a notice they were still reading (`d797a4c`, "Make a press say whether it was consumed, not
whether it changed"). Later, five geometry questions ran above the occupancy walk — two top-band
arms and Info's three internal arms — so a pane authored over the side column and ranked in
front of Info still lost presses to Info (`3bfc2fd`).

**Decision.** The handlers under `if (b.pressed)` answer one routing question: true is
consumed, stop; false is not mine, carry on. A consumed press need not change anything. A
deliberate `false` is a decision. `info_body_at` is the resolve-and-locate preamble, owned once
and resolved once per press beside the canvas point. Nothing asks a geometry question above
occupancy: modes first, then pane occupancy over `effective_pane_order`, then the resolved pane's
own local inverse, then the workspace. `band_tab_at` is the Layouts pane's local inverse. A
secondary press is state-local first refusal.

**Alternatives considered.**
- *A three-valued disposition, a target enum, an interaction package* — rejected: the failure
  was a name, not a missing type, and a plausible shape would have hidden it; the richer answers
  already exist on semantic paths (`Written`, `Handled`, `Commit`, `Availability`, `Occupancy`).
- *Consuming the already-selected object row for symmetry* — rejected: naming the bit is what
  makes the deliberate no legible; pinned by case `"QR-2: a press on the ALREADY selected object
  row is deliberately not consumed"`.
- *Unifying `terminal_press`'s bool with the chain* — rejected: its bool is "a repaint is owed",
  consumption there was decided by the mode, and its `false` means the opposite of the chain's.
- *Resolving the body in each handler* — replaced by one resolution, sound because every
  handler changes nothing on the paths where it declines.
- *Pane-internal arms above the walk* — removed; pinned by case `"WUX-12/SC-6: a pane in front
  of an Info control takes the point"`.
- *A global Back action or a `right_click_back` keymap row* — refused (`bf35754`): each state's
  own local reading, and a future state may claim the secondary press for something else.

**Consequences.** Consumed and not-consumed are told apart by where, never by what changed. A
new pane-internal gesture belongs in the resolved-owner arm, never above the walk. One consumed
gesture performs one transition: the press that closes an arrangement scope never also opens
the menu, and its release is dropped on the ordinary path.

**Laws supported.** [WL-PRESS-01](../workshop/press-chain.md),
[WL-PRESS-02](../workshop/press-chain.md), [WL-PRESS-03](../workshop/press-chain.md),
[WL-PRESS-04](../workshop/press-chain.md), [WL-PRESS-05](../workshop/press-chain.md),
[WL-PRESS-06](../workshop/press-chain.md).
