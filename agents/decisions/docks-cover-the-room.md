# Docks cover the room

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [setup-file](../workshop/setup-file.md), [layouts](../workshop/layouts.md),
[geometry](../workshop/geometry.md) and [chrome](../workshop/chrome.md).

**Context.** A pane stood in the room at its place word and nowhere else; a fold had no home, a
hide took the row off the layout and kept its place for the run alone, and docking was refused
with no record behind the refusal. A weaver wants a pane put away to come back where it was, a pane
shut to its title, and a pane kept against an edge -- a popup needed for a moment -- that moves
nothing they placed.

**Decision.** Fold and dock are typed fields of a setup's pane row: a fold `open`, `shut` or
`hidden`, a dock an edge or `none`. A hidden row is kept whole, and a show puts it back where it
was, in this run or the next. A dock keeps a strip the whole length of its edge, open or shut, so
the room is narrower by it whatever the dock does; its open pane covers the room and moves no
pane. An edge shows one open pane, the others as tags on its strip.

**Alternatives considered.**
- *Fold and dock as Workshop's words in each row's settings* -- argued: no format would move, but
  a fold is the desk's own fact, which the solve and the reconcile read and the file judges once;
  a pane's settings are the pane's.
- *A dock taking room* -- argued: each open, shut or switch would move what stands beside the
  dock; only an authored anchor moves a pane, and none exists.
- *A strip only while the dock is shut* -- argued: opening it would narrow the room, and the panes
  against that edge would move at every open.

**Consequences.** Docking a pane moves the panes against its edge once, by the strip; opening and
shutting it moves none. Removing a pane from the setup is the one way to forget its place.

**Laws supported.** [WL-SETUP-13](../workshop/setup-file.md), [WL-LAYOUT-14](../workshop/layouts.md),
[WL-GEO-14](../workshop/geometry.md), [WL-CHROME-11](../workshop/chrome.md),
[WL-CHROME-12](../workshop/chrome.md).
