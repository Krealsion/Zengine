# A verdict answers its declaration

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [desktop](../workshop/desktop.md).

**Context.** A provider must be able to learn that Workshop refused its action declaration, so it
can recover gracefully under its own policy (BL-WORK-04). The first attempt sent a refusal to the
declaring office with the pane and a sentence. An independent review reproduced why that is not
enough: a provider that declared twice (attempt 101 refused, 102 accepted) received a refusal
naming no attempt, so it could not tell which declaration was judged; a refusal addressed to an
office reaches whoever holds it by then, so a successor after a reload could read its
predecessor's verdict as its own; and the claim that silence meant acceptance was false, because
a refusal whose delivery fails is also silence.

**Decision.** Workshop answers each declaration it judges with `ActionsJudged`, as Loom's answer
to that delivery: the answer carries the declaration's own correlation and reaches only the
incarnation that sent it (Loom ANS-03, ANS-05). An accepted declaration is given a number Workshop
mints once, and that number is told only in the answer. When a later re-join — a keymap file, or
application rows declared afterwards — takes a gesture away, the whole declaration leaves the
keymap and `ActionsWithdrawn` names that number, as ordinary speech to the office. Only a holder
whose office accepts the shape is told. The band says every refusal either way. Nothing is
mandated or retried.

**Alternatives considered.**
- *An office-addressed refusal with a pane and a sentence* — tried, and it was this seam's first
  shape: an independent probe declared under correlations 101 and 102 and received the refusal
  with correlation 0. The same sequence is now the case `"a verdict answers the declaration it
  judges: a refused attempt is named by its own number after a later one was accepted, and an
  accepted one is given Workshop's"`.
- *The declaration's correlation as the only identity* — argued against: it names a direct
  refusal well, but a withdrawal is ordinary speech that follows the office, and a successor
  restarting its own count could hold the same number as its predecessor.
- *A deferred answer held per declaration, spent at withdrawal* — argued against: it would reach
  exactly the declaring incarnation, but deferred answers are a Loom-wide pool of 64 (ANS-02),
  and a held right per pane would spend it for as long as the pane lives.
- *Keeping a displaced declaration to re-join later* — tried until this change: a later re-join
  could bring it back into force without its declarer being told, after it had been told the
  rows were gone. It is withdrawn now, and the provider may declare again.
- *Reading acceptance from silence* — refused: an answer can be refused at the gate, dropped with
  the incarnation that asked, or never sent to a provider built before the shape.

**Consequences.** A provider built against the protocol before these two shapes declares and
dispatches exactly as before and is told nothing. One that accepts them knows which of its
declarations is in force and which is not, and a successor ignores numbers it was never given.

**Laws supported.** [WL-DESK-06](../workshop/desktop.md).
