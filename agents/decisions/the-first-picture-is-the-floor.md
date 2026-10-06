# The first picture is the floor

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [session restore](../workshop/session-restore.md).

**Context.** A medium that has been told nothing has only a run's first picture to size itself
from, and the SDL medium makes that size the window's minimum, once, at creation
(`SDL_SetWindowMinimumSize`). Seeding the remembered extent before the first canvas came up at
the right size and left a weaver unable ever to shrink their own window — measured on Windows
against a real window that refused every drag below its restored size (`fba0dc2`). Position and
maximized state were not in the vocabulary until the placement pair closed that omission
(`6790547`); then the medium re-maximized before the restored room arrived, and a weaver
unmaximized onto 78x22 (`209c7da`, "Maximize a restored window over the room it is meant to come
back to").

**Decision.** `on(SurfaceReady)` repaints at the minimum extent and then takes the session back;
the room, then the desk into it. The viewport is `{width, height}` in canvas pixels, one level
above the desk, and a viewport outside the screen's band is declined, never clamped. The desktop
placement is remembered opaque and judged by the medium (`placement_within`). The saved viewport
is the normal window's. A maximized restore repositions, re-grows through the canvas
conversation, then re-maximizes, one beat after the picture that supplies the room.

**Alternatives considered.**
- *Seeding the remembered extent first* — measured against a real window (`fba0dc2`); pinned by
  case `"the FIRST picture of a run is the floor, and the room is the second"`.
- *Applying the desk before the viewport* — measured red while the room's height rationed the
  stack (`fba0dc2`); since no room rations it
  ([the stack begins again](the-stack-begins-again.md)), the order changes no seating.
- *Clamping an out-of-band viewport* — rejected: clamping 100000 to 640 still opens a window
  nobody chose on a display Workshop cannot see.
- *Persisting `SurfaceExtent`, or pixels* — rejected: cells are what cross the Skin seam; the
  message is free to grow a field, and its text metric would be a stale claim about a font.
- *Workshop interpreting desktop coordinates* — refused: the medium validates against the
  displays that exist now, and a terminal retains the remembered value rather than erasing it.
- *Maximizing the moment the offer arrives* — measured wrong by handle on a real desktop: 9 of
  14 checks, 11 of 14 with the one-beat delay alone, 14 of 14 as shipped (`209c7da`).

**Consequences.** A restored window is the weaver's size floored to whole cells, at most
`kCanvasCellPx − 1` pixels short on each axis. The remembered room is an ordinary later picture,
a want rather than a floor. A maximized close writes the room the weaver chose with `maximized`
beside it, and a flag merely restored never gates a placement-less run's tracking.

**Laws supported.** [WL-SESSION-07](../workshop/session-restore.md),
[WL-SESSION-08](../workshop/session-restore.md), [WL-SESSION-09](../workshop/session-restore.md),
[WL-SESSION-11](../workshop/session-restore.md).
