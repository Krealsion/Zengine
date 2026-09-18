# An undelivered ask is released

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [info-body](../workshop/info-body.md).

**Context.** Info discarded the `loom::Ticket` its document asks returned and did not accept
Loom's `zen.DispatchRefused`, while it sends one commit at a time. Measured at `b42fe35` with
the loaded pane: Workshop's weave killed between a commit's send and its delivery refused the
commit `TargetUnavailable`, the pane was told nothing, and with Workshop revived the next Return
said `commit not sent -- an earlier commit is still unanswered`. The draft could not be
committed again in that run.

**Decision.** Each document ask keeps its ticket. A ticket that is not valid means nothing was
queued: the record is released at once and the pane says the commit was not submitted. A valid
one is matched later against `zen.DispatchRefused` only when `mail.dispatch_refused()` is true,
then by attempt, correlation, shape, version and office; that record alone is released, and the
account — not delivered, nothing written, Loom's reason — replaces only that commit's own
sentence. No draft is closed, because nothing was written, so the next Return is a fresh commit.
Delivered silence is not a refusal and still holds the commit outstanding. A select, a create
or a delete is accounted the same way against its own record.

**Alternatives considered.**
- *Tried: discarding the ticket* — the refused commit held the one-outstanding gate for the rest
  of the run; pinned by case `"an Info commit Loom refuses at dispatch is released: the draft and
  its text stand, the next Return is written, and a cancel's promise is replaced"`.
- *Argued: a timeout or a retry* — silence proves no fate, and a retry is a second write.
- *Argued: matching the attempt alone* — a sequence is a number anyone can say; the correlation,
  shape and office are what this pane asked, and provenance is what makes it Loom's.
- *Argued: closing the draft on a refusal* — the maker's unwritten text would go with it.
- *Argued: accounting every pane send* — the rows, the declaration and the clipboard hold no
  record a refusal could release, and are outside this decision.

**Consequences.** Every Info send Loom refuses at dispatch now reaches the pane as a notice, and
one that names no document ask is ignored. A suite stages these refusals by killing Workshop's
weave or taking it off the bus between a send and its delivery, because no gesture can.

**Since.** The desktop's Pane Creator holds one act at a time and met the same failure: a make
the host's admission denied at dispatch held its slot, so every later make, save and discard said
it was not sent. It keeps its act's ticket on these terms, paste included, and a delivered act
still waits ([WL-MAKER-14](../workshop/maker-pane.md)).

**Laws supported.** [WL-INFO-13](../workshop/info-body.md).
