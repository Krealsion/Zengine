# A guest's act waits for its answers

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [guests](../workshop/guests.md).

**Context.** Workshop judges a guest's act by the row its guest door admitted the guest with and by
what Loom says the guest may do now. Both were read through calls its host handed it, which no
loaded Workshop is handed; each now has an owner that answers by message, the guest door
(`GuestRowOfRequested`) and Loom's authority reader (`zen.AuthorityAsked`), and an answer comes a
turn after the act.

**Decision.** Each act Workshop judges for a participant that is not the weaver's own hand -- a
press, a key, a carry, a desk read, a guide mark, a pane's ask on its behalf -- is held with every
later act of the same participant, in the order they came, while its row and then its authority
are asked once for that act; it runs again as it came, with its sender, provenance and answer
right, and the answers are in scope for that run alone. The weaver's own acts never wait. The hold
is bounded and an act past it is refused in words. With no door to answer, the participant is no
guest; with no reader to answer, the act is not permitted. The hold is its own part of the weave
(`workshop/guest_acts.hpp`), with its own doors.

**Alternatives considered.**
- *The host's two calls kept* — argued against: a loaded Workshop is handed no host.
- *The guest door claims each admitted row as a Sense, read with nothing waiting* — argued against:
  the row is the door's to say at each act, and a claim is a copy its reader reads later.
- *The answers kept per guest between acts* — argued against: a row or an authority kept is a
  second copy, stale the moment the door or the bus changes.
- *A role speaking for itself taken as no guest* — tried: a guest's session may speak as a role,
  and its quit asked so went unjudged (`test_workshop_panes_guests.cpp`).

**Consequences.** A guest's act lands a turn or two later than before, which a person does not
see. The window gives a pane a guest's press at once, so what that pane's role says next waits
behind Workshop's word of the press (`zengine::With`'s facets may hold a delivery before its
handler). A held act whose sender was replaced meanwhile lands on nobody. Where no guest door
listens, nobody is a guest, and an act that would ask the reader nothing runs at once.

**Laws supported.** [WL-GUEST-15](../workshop/guests.md).
