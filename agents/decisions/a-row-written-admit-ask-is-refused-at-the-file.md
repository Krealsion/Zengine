# A row written admit ask is refused when the file is read

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [guests](../workshop/guests.md).

**Context.** A guests file's row may say `"admit": "ask"`. The admission policy then defers the
connection, and `GuestDoor::decide` is what would admit or refuse it. Nothing in the host calls
`decide`: a suite does. A guest whose row asks is never admitted and never refused. Its connection
stays until it leaves. No box asks the weaver.

**Decision.** The file reader refuses a row written `"admit": "ask"`, in words naming the row, and
Workshop does not listen. A row that omits `admit`, or writes `now`, is admitted as that row when
its credential is presented. A credential no row presents is refused in the same words as before.

**Alternatives considered.**
- *Argued: Workshop asks the weaver in a box naming the guest and the row's grant, and the answer
  reaches `decide`* — no such box is built, and building one would be a new surface.
- *Argued: the policy refuses the connection at once, and the file still accepts `ask`* — the
  connection would be decided, but the file would still name a decision nobody takes.

**Consequences.** A file that says `"admit": "ask"` stops Workshop before anything is mounted, as
any other refusal of the file does. The door can still report `awaiting-decision`; this host's
file never produces it.

**Laws supported.** [WL-GUEST-14](../workshop/guests.md).
