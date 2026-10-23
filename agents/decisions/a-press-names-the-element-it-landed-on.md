# A press names the element it landed on

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [the press chain](../workshop/press-chain.md).

**Context.** A pane draws in its own area, and the window lands each press on what it drew there.
The pane reads the press when it is delivered, and by then it may have drawn again: a wheel turned
in the same beat, a list that moved, a legend that hid. Read against the newest rows, a press could
act on whatever slid under the hand; refused whenever the picture moved, a press would fail on
every caret blink, hover or recolouring.

**Decision.** The window says, with each button moment, what it drew under the hand when the
moment landed: the area that showed it, the part named there, and that area's drawing as its
drawer numbered it (`AreaButtonChanged::under`, `part`, `drawing`; `Drawn::number`). A pane acts on
the element the press landed on, over that element's whole face, and refuses it, in its own words,
only where a different element or none stands there in its newest drawing. The same element is the
part a pane names, by the name it keeps across its redraws; where it names none, what the row means,
by the pane's number for its composition. A drop carries the window's word from the release.

**Alternatives considered.**
- *One area a picture* -- argued against: an area a drawing multiplies the window's areas, and a
  press is still read after the next drawing lands.
- *A fence through the window* -- argued against: a press waits on a drawing, and two offices'
  messages must arrive in one order.
- *A drawing's number alone* -- argued against: a press refused whenever the picture moved, a
  colour or a caret included.

**Consequences.** A press needs no wait and no order between offices: each fact carries its own
number, and a late one is read by it. A pane names what a press may act on, or numbers what its rows
mean; one that does neither reads a press against its newest drawing.

**Laws supported.** [WL-PRESS-07](../workshop/press-chain.md).
