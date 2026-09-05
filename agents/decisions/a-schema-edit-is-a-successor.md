# A schema edit is a successor

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [succession](../maker/succession.md) and [weave](../maker/weave.md).

**Context.** A maker edits a live weave at an editor with no compiler in the loop. Two edits look
alike and are not: a new trigger body over the same state, and a new state shape. The Loom's
reload is shape-only by law — `swap_state` and `reload` admit only at the record's state schema,
and the kernel requires the identical schema — and the Loom's replacement is a prepared candidate
with an authored migration, never an inferred one. The research traced both to file and line and
found nothing in the substrate that needed to change.

**Decision.** A behaviour edit with the state schema unchanged is `swap_state`: the successor
revision's bodies mount beside the incumbent's, the live weave takes the definition, the old
bodies unmount, the incarnation bumps and `Revived` is announced under the same WeaveId. A schema
edit is a succession: a new definition with a new WeaveId, the role kept, the maker's state a
first-class Loom schema on both sides, and the conversion authored as data in the successor's
definition — mounted at registration as the conventional operator edge and spent by the
coordinator on the incumbent's final bytes. Reload stays shape-only; a stale state file meeting a
successor is refused by name this phase, and the edge it could take is named as the seam.

**Who may speak at the boundary.** Provenance, not shape — the Loom's own rule. Every maker weave
accepts the ceremony shapes, so the shape of a `Quiesce`, a `Resume` or an `Adopt` says nothing
about who sent it; the host arms the incumbent and the candidate for one coordinator and one token
through the objects it holds when the edit begins, and the weave honours those three shapes only
from that bus-stamped sender with that token — `Adopt` only while it is an unbound candidate,
bound again by Loom's attested activation. A stranger's is refused by name and the weave keeps
serving. The same rule namespaces a definition's emits under its own name, so a definition — pure
data — cannot speak the ceremony shapes to its siblings; accepts stay free.

**Alternatives considered.**
- *Converting in place under the same WeaveId* — tried against the substrate's own doors and
  refused there: `swap_state` admits only at the record's state schema, so a v2 value cannot
  enter a v1 record; pinned by case `"e: a definition whose state schema differs is refused as a
  behaviour edit, and the live weave is untouched"`.
- *A temporary migrator weave, as the handoff garden's* — argued and rejected: a migrator is a
  per-edit artifact with no C++ to hold it, since no maker shape exists in C++; the conversion
  as data in the successor keeps the five properties the garden names — inspectable, testable,
  versioned, refusable, attributable — with a catalog identity in place of a bus identity.
- *The candidate applying the conversion itself on receiving v1 bytes* — rejected: the
  candidate would then hold the predecessor's schema, and a refused conversion would reach a
  candidate before anyone knew; pinned by case `"f: a conversion the write refuses reaches no
  candidate -- the transaction aborts, the incumbent is resumed and is still the service"`.
- *Inferring the conversion from same-named fields, dropping the rest* — rejected: loss is
  authored, never inferred; a predecessor field neither copied nor named in `drops` refuses the
  plan, pinned by case `"f: the field-wise write refuses a target with no source, a source the
  schema lacks, a kind mismatch, two sources, a constant of a non-scalar kind, and a predecessor
  field neither copied nor dropped"`.
- *Trusting the shape at the ceremony doors* — tried against the suite and refused there: a
  stranger's `Quiesce` froze the weave and read its state, a stranger's `Resume` un-froze it
  mid-edit, and an `Adopt` on the live service rewrote its state if the bytes admitted; pinned by
  case `"before the merge: Quiesce and Resume are honoured only from the coordinator the host
  armed for this boundary; a stranger's are refused by name and the weave keeps serving"` and case
  `"before the merge: Adopt is honoured only by an unbound candidate and only from its
  coordinator; a bound weave refuses it by name and its state stands"`.
- *Converting a stale state file at load* — deferred, not rejected: the edge exists in the
  catalog once a successor is registered and the reader deliberately does not take it, pinned by
  case `"b: a state file of another version is refused by name at load, and nothing converts
  it"`.

**Consequences.** A schema edit costs a full prepared replacement — a coordinator, a seal, a
budget, one ask, a commit the host pumps — and a message in flight is handled, refused by name or
handled by the successor, never lost. The conversion's schema half is judged at admission where
the maker can see it, and its value half at the edge where the live bytes are. Every definition
edit is a new build identity; the grant is not re-floored here.

**Laws supported.** [MW-DEF-07](../maker/definition.md), [MW-DEF-08](../maker/definition.md),
[MW-SUCC-01](../maker/succession.md), [MW-SUCC-02](../maker/succession.md),
[MW-SUCC-03](../maker/succession.md), [MW-SUCC-04](../maker/succession.md),
[MW-SUCC-05](../maker/succession.md), [MW-SUCC-06](../maker/succession.md),
[MW-WEAVE-09](../maker/weave.md), [MW-WEAVE-10](../maker/weave.md).
