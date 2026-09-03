# The Layouts pane

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [tab-run](../workshop/tab-run.md).

**Context.** The tab run first rode the band's status row (`102017a`), moved to the top of the
screen (`4868c6b`), and marked the live desk with two cells on its left while quoting every
name — `> "Home"  "Code"  "Art"`, a marker attached to nothing and three names dressed as tokens
(`dff7c6b`, "Let the selection hug the name it is about"). The first two rows were still painted
by the screen out of a rectangle nothing could name: no maker could move, cover, resize or take
the surface off a desk (`3bfc2fd`, "Let a maker move the layout tabs, because they are a pane
like the rest").

**Decision.** The run, the association and the workspace fact are the built-in `Layouts` pane in
the top band: a catalog row, a setup row, authored geometry, a front rank, ordinary paint,
occupancy, coverage, picker recovery and session persistence. The status is the active layout's
association in three sentences. The path is what elides. `+` is an action. `band_status` is the
one composition both consumers spend, against the pane's body. `>name<` brackets the live name,
one cell each side; every name is painted bare. The visible window is derived and stored
nowhere. A press on a painted tab is a press on the pane; a second press renames; a press takes
hold; a right press names the tab as a subject.

**Alternatives considered.**
- *Two marker cells on the left* — retired (`dff7c6b`).
- *Brackets on the live tab alone* — rejected: the row's right side would slide two cells on
  every switch, the footer's moving-target defect; the equality is the type's (`char`); pinned by
  case `"QR-15/SC-4: switching the live layout moves nothing to the right of it"`.
- *Quoting names in the run* — retired: the quotes paid for a delimiter the marker cells pay for
  now; the cost is stated where the law lives — a name spelled `Ops" UNSAVED | decoy` reads as
  status text to the eye, while the machine's boundary (the recorded span) is untouched.
- *`UNSAVED` as a status word* — retired: `none` does not mean unsaved (`2dc7626`).
- *Fitting the sentence after appending the path* — rejected: it cuts `modified` off the end;
  and taking the whole remainder for the path starved the unresolved count at the 78-column
  minimum, measured by the suite; pinned by case `"WUX-11/SC-24: the association's verdict
  survives the row's cut, at every width"`.
- *`+` as a durable pseudo-layout* — rejected: not in `layout_count`, unknown to the session.
- *The tab arm at the top of the pressed branch, above every layer* — removed with the
  conversion; pinned by case `"WUX-12/SC-4+SC-8: a tab press IS a press on the Layouts pane, and
  still switches"`.
- *Caching the dragged tab* — none: the press has just made it live, so the hand always carries
  `active_at`.

**Consequences.** `kSetupStatusCols` is derived from the words' own widths, and the status is
adjusted to the row's right edge where the row still fits. Narrowing the pane narrows the run,
and markers, reservation and `+` degrade by their own rules. Removing the pane strands nobody:
the keys still step the run and the picker brings it back. A version-5 session materializes the
row on the way in ([yesterday-belongs-to-a-conversion](yesterday-belongs-to-a-conversion.md)).
The live TUI witness found the saved marker eaten by the row's own cut mark at the 78-column
minimum with a full run, which the suite had missed by three tabs (`102017a`).

**Laws supported.** [WL-TAB-01](../workshop/tab-run.md), [WL-TAB-02](../workshop/tab-run.md),
[WL-TAB-03](../workshop/tab-run.md), [WL-TAB-04](../workshop/tab-run.md),
[WL-TAB-05](../workshop/tab-run.md), [WL-TAB-06](../workshop/tab-run.md),
[WL-TAB-07](../workshop/tab-run.md), [WL-TAB-08](../workshop/tab-run.md),
[WL-TAB-09](../workshop/tab-run.md), [WL-TAB-10](../workshop/tab-run.md),
[WL-TAB-11](../workshop/tab-run.md), [WL-TAB-12](../workshop/tab-run.md).
