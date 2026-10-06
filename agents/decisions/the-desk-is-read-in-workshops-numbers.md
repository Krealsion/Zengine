# The desk is read in Workshop's own numbers

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [geometry](../workshop/geometry.md).

**Context.** An agent read a pane's rows and one cell's point by message, and nothing else: a
pane's place and size, its state and rank, which pane held the keys, whether arranging was open
and which menu was on the screen were said in words on the band or shown only in pictures, so a
witness counted pixels and a walk chose a row by where it fell. A canvas pane's words -- the View
Builder's `[Label]`, a Flow node's name -- were refused as a picture, not text rows.

**Decision.** Workshop answers the desk (`DeskViewRequested`, answered by `DeskView`) from the
numbers it owns: `bounds_of`'s resolved and visible rectangles, `pane_state_of`'s word, the rank
in the effective order, the selection and the keys, the arranging flag, and a menu's lines from
the painter's own composition of it. A pane's words (`PaneView` version 2) are a text pane's rows
or a canvas pane's labels and runs, each placed as the painter clips it, with the point a press
names it by; `PanePoint` version 2 resolves one character of one word. Every place is the
canvas's, in its pixels: a terminal draws it on the cells it floors to. The first versions are
served as they were.

**Alternatives considered.**
- *Reading the places off the composed picture* — tried, as the proof rather than the answer:
  the desk suite reads each place back through the terminal's rasterizer and the window's plan
  (`rasterize_canvas`, `plan_canvas`) and finds the word there. As the answer it would have made
  a picture the owner of a number Workshop owns.
- *A door of its own for a canvas pane's words* — argued: one question, what a pane says and
  where, would take two doors, and a pane moving from rows to a picture would move doors.
- *Places in the input space, as points are* — argued: a place is the canvas's geometry, which
  a terminal floors; cells would lose a window's pixel, and a console's rows above the canvas
  would be folded into a place.
- *A menu's lines through the pane view, by the pane that asked* — argued: Workshop's own menu
  has no pane, and a menu covers the pane whose words are refused while it is open.

**Consequences.** A walk checks a size by its number and presses a canvas word or a menu line by
what it says (`workshop/act`'s `desk`, `menu`, `click` and `control`). Workshop names nothing
inside a pane: a word's number is its place in one answer, and a pane naming its own parts is
the pane's.

**Laws supported.** [WL-GEO-13](../workshop/geometry.md).
