# A window lands every hand

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [guests](../workshop/guests.md).

**Context.** The window is the one judge of the pointer: it lands each press on what it drew and
gives the moment to the owner of the area hearing it. A participant's press, injected by the input
office, reached Workshop instead, which landed it on its own picture and relayed it into the pane.

**Decision.** The window takes the input office's attributed moments (`input::AttributedInput`)
as moments of the hand they name, from that office alone, lands a pointer moment by its own judge
as it lands the platform's, and says the hand on every moment it gives (`AreaButtonChanged`,
`AreaPointerMoved`, `AreaWheelTurned`, `AreaPressed`, each v2). An area its arranger places for the
weaver's hand alone (`AreaPlaced` v3, `AreaIntroduced` v4) refuses an injected press or wheel,
and tells the arranger (`AreaHandRefused`), who says why. Workshop places the editor's, the
Terminal's and the Hotkeys pane's areas so on a weaver's host where a guest door listens, and reads
the hand on `AreaPressed`: a guest's press ends no guide mark, and moves the keys and the selection
as the weaver's does, so the keys it then types reach the pane it pressed. Keys stay as they are,
and a batch keeps its order: a key behind a pointer moment the window may not have landed waits
until the window answers an ask Workshop sends after it.

**Alternatives considered.**
- *Workshop judging a guest's moment and asking the window to land it* -- argued against: Workshop
  keeps taking the bare pointer moments and relays every injected one.
- *Agents pressing a part by name* -- argued against: no moment is injected, and a drag or a wheel
  then needs asks of its own.

**Consequences.** Four graphics shapes take a version and one is new; the window keeps a hand
beside each hold, so a guest's press never ends or takes the weaver's. Workshop leaves an injected
pointer moment to the window while one shows its desk. The window cannot tell a guest's hand from
another participant's, so an area placed for the weaver alone refuses every injected hand; with no
guest door, none is placed so. A press in a window is landed by what the window shows when it reads
it; where no window shows the desk, Workshop lands it against the picture it was read with.

**Laws supported.** [WL-GUEST-13](../workshop/guests.md).
