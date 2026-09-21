# A pane draws its own controls

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law
it supports is in [pane-controls](../workshop/pane-controls.md).

**Context.** Files and the Builder were reachable by hand only as far as a press could carry
them. Files' press selected a row and a second press on it opened the row; every other
operation it had — up a directory, look again, use as recipes, pick buildable, mark — was a
bare letter with nothing on the screen saying so, and its two modes (the buildable chooser and
the authoring line) took presses and did nothing at all, so a maker who entered one with a
mouse could not leave it with one. The Builder accepted no press whatever: `c` walked a
catalog a maker could not see, and nine verbs lived entirely in the bottom band's legend.

**Decision.** A pane that wants a hand draws a strip of labelled controls under its rows, and
every control is the same operation its key already had — one `perform`, reached by a key, by
a control, or by a row of the pane's own menu. `component/control_strip.hpp` packs the faces;
`component::RowMap` records where each one landed and numbers the composition; the pane
answers `v3::PanePressed` and `PaneButton` against that number. Availability is drawn on the
face (`[label]` against `(label)`) and checked again by the operation, which refuses in its
own words. What the width or the room cannot seat is counted and reachable through `[menu]`,
which is the first control every strip declares.

**Alternatives considered.**
- *Right-click only* — rejected: a maker discovers the second button last, and the founder's
  rule is that a useful operation is visible. The menu carries everything; the strip is what
  says so without being asked.
- *A control whose meaning carries its subject* — tried and withdrawn (measured): naming the
  selection inside a control's `RowMap` meaning moved the picture on every selection, and the
  picture fence then refused the second press of an ordinary double-click. Controls act on
  what the pane shows as chosen, and only the three whose subject is NOT the choice name it.
- *Hiding unavailable controls* — rejected: a control that vanishes teaches nothing, and a
  maker who aims at one is owed the operation's own reason. They are drawn in round brackets
  and they answer.
- *A widget framework* — declined. Two consumers earned one packer; nothing here knows what a
  pane means, and no other pane was migrated for symmetry.

**Consequences.** Files' centred listing window was replaced by `component::cursor_window`,
which moves by the least it can: that is what makes a double-click land on the row it was
aimed at, and with the picture fence it repairs the queued-press defect P-WORK-25 reproduced
(aimed `entry-03`, ended on `entry-04`). The authoring walk shows all four fields and any of
them can be stood on again, so a mistyped first field no longer costs the draft. Workshop
sends a primary press under a correlation, so a pane may answer a click on its own `[menu]`
by offering rows — judged on the same three terms the secondary press and the declared key
already met. A marker row is now said only where the window reserved one, because saying it
anyway overran the budget and pushed the strip out of a short pane.

**Laws supported.** [WL-HAND-01](../workshop/pane-controls.md),
[WL-HAND-02](../workshop/pane-controls.md), [WL-HAND-03](../workshop/pane-controls.md),
[WL-HAND-04](../workshop/pane-controls.md), [WL-HAND-05](../workshop/pane-controls.md).
