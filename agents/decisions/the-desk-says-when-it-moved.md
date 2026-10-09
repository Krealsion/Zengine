# The desk says when it moved, and names each picture by what it shows

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [desk-read](../workshop/desk-read.md).

**Context.** An agent that read the desk learned of a change only by reading again, and a walk
re-read a pane five times a second while it waited. A stamp named a pane's picture by the pane's
own number, so a pane that numbered none -- a game drawing rows every tick -- stood on one stamp
while it moved, a list whose text changed under one number did too, and a canvas picture sent again
unchanged moved the stamp for nothing.

**Decision.** Workshop publishes `DeskStamps`, the desk number and every presented pane's stamp, at
the end of each delivery that moved either: the whole state, so a relay may keep only the newest
and the newest stands for each earlier one. A stamp names a picture by the fingerprint Workshop
takes of what the pane sent and holds, never its number, for every pane, a pane doing nothing for
it; a canvas picture no press is stamped with is named none. The numbers stay where a press needs
them. New versions carry it -- `PaneStamp` v2, `PaneView` v5 and its ask, `DeskRead` v2 -- and the
earlier ones answer as they did. The agent's `desk/watch` follows the notice, reads again only what
moved, and writes each change as the smaller of its changed lines and the lines as they stand.

**Alternatives considered.**
- *Changes published as occurrences* -- argued against: the producer would keep each followed pane's
  last lines to compute a change, a held copy beside the pane.
- *Whole readings published at each repaint* -- argued against: a copy of what a desk read answers on
  demand, sent whether anyone reads it or not.
- *Workshop numbering each admitted prose update itself* -- argued against: a picture sent again
  would still move the stamp, and the numbers are the pane's own, for its presses.
- *The fingerprint in the first `PaneStamp`'s `picture`* -- argued against: a published shape's
  meaning does not change in place; the new meaning takes new versions.
- *In flight judged by number in the fifth version* -- argued against: a picture sent again would read
  as in flight, and when it settled no notice would come, so a follower would keep "in flight".

**Consequences.** An agent stops polling: it is told, and reads only what moved. A stamp moves when
what a pane shows moves, a caret or a colour too, which may change no word of the reading.

**Laws supported.** [WL-READ-06](../workshop/desk-read.md),
[WL-READ-07](../workshop/desk-read.md).
