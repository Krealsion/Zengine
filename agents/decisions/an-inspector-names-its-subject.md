# An inspector names its subject

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [info-body](../workshop/info-body.md).

**Context.** Info inspected the prototype object document, which is retiring with its canvas.
What a maker needs to read and adjust is a pane: its identity, where the desk places it, what the
screen made of that. The host already had exactly those rows — the Pane Manager's
(`pane_editor_rows`), every setter an existing door (WL-PED-05) — but they were painted by a host
panel, over a subject that panel alone could choose.

**Decision.** Info names a pane with one ask (`InspectPaneRequested`) and the host records it
(`Session::inspected`), because the rows are closures over the live session and cannot cross.
Nothing else writes it: not the selection, not the keys, not Escape; Info may name itself. The
host builds the Pane Manager's rows over it without the Pane Manager's own keys
(`pane_subject_rows`), names what they address — this pane, this desk put live
(`SetupState::put_live`), this row layout — and publishes the picture compare-gated, answering
an arriving inspector. A commit returns that name; the host judges it first, writes through the
row's own setter, reseats, and says what it wrote to which pane. The draft, its text and every
answer's accounting are Info's, carried from the object inspector unchanged.

**Alternatives considered.**
- *Info holds the subject and asks for rows when it needs them* — rejected: a pane's window
  moves under the inspector with no gesture into it (a resize, a drag, a desk switch), and only a
  publication keeps the picture true; a host cannot publish rows for a subject it does not know.
- *The subject follows the selection or the keys* — rejected: reading a pane would require
  selecting it, and pressing into Info to type would make Info its own subject.
- *Keeping the name across a desk switch* — rejected: the same pane's Width on another desk is
  another property, and a draft typed for one would be written into the other.
- *A general property or component editor* — out of scope: the rows are the ones the Pane
  Manager already showed; a new kind of row is a new decision.
- *One subject per inspecting office* — not needed with one inspector; recorded as the limit.

**Consequences.** One subject slot: an office that asks moves it. A restore or a layout switch
abandons a live draft and says so; a commit queued behind one is refused with nothing written.
A write the setter refuses (a unit the face does not read, a pane with no room) is the owner's
sentence, and the draft stands. A replaced Info loses its draft and is told nothing about a
commit its predecessor sent (Loom ANS-03); the write, if taken, is in the rows.

**Laws supported.** [WL-INFO-14](../workshop/info-body.md),
[WL-INFO-15](../workshop/info-body.md).
