# A quit that cannot be asked is refused

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [session](../workshop/session.md).

**Context.** Workshop's quit publishes `PaneQuitRequested` as its office and waits for as many
answers as Loom had accepters. A participant Loom will not deliver to — held behind a claim its
image could not apply, dead, gone, no longer taking the question — runs no handler and can never
answer. Measured with the real Editor image held: the quit waited, every later gesture was held,
the picker could not open, and a reload of the Editor replayed no question, so the close box went
on saying `quitting -- waiting for 1 pane(s) to answer`. Loom's `zen.DispatchRefused` names a
refused attempt for a directed or role send only: `Switchboard::fanout` captures no refusal
recipient, and Loom lists publication aggregation among the capabilities it does not provide.

**Decision.** The refusal is read where Loom records it. The host mounts a `QuitDeliveryWatch` on
its own tap beside Workshop: a `Refused` event whose sender stamp is Workshop and whose shape is
the quit question is copied — the ask's correlation, the participant, its office then, the
reason — into `HostContext::undelivered_quits`, and Workshop is woken by a root delivery that
carries nothing. Workshop takes the whole book at every wake-up and refuses the quit in flight on
an entry for its own ask, through the cleanup an answered refusal uses. A delivered question
nobody answers still waits.

**Alternatives considered.**
- *Accepting `zen.DispatchRefused` in Workshop* — refused from source: the notice is never built
  for a publication's envelopes, so Workshop would hear nothing.
- *Asking each participant with a directed send* — refused: the host would enumerate accepters
  with a second copy of fanout's rule, and the protocol's one publication would change shape.
- *Carrying the fact in the wake-up* — refused: a payload, a role spelling or a correlation is
  speech a stranger can choose, and a stranger can choose the live ask's number; the book is host
  memory that only the watch writes.
- *Filtering the watch on the authored office as well* — refused: only the office's holder can
  author as it, so the sender stamp already says it, and a guard no state can reach is ceremony.
- *Waiting for the other answers before refusing* — refused: one known refusal already makes the
  quit impossible, and an owed answer would hold the maker's keys for nothing.
- *A deadline, a forced exit, or a reload that replays the question* — outside the decision: a
  silent participant stays pending, and nothing is synthesized for one.

**Consequences.** Every composition that mounts Workshop in its office mounts the watch — the
host and both rigs. The refusal names the office, the weave and Loom's reason; gestures replay;
nothing is saved. Coverage is fanout's own, so a participant that shows no pane is asked and
counted. A delivery made to a handler that then failed is not a refusal. The watch's entry is
best-effort: an allocation failure loses it rather than replace the evidence the delivery
produced, and that quit waits as it did before anything watched.

**Laws supported.** [WL-SESSION-19](../workshop/session.md).
