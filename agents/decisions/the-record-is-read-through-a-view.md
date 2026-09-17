# The record is read through a view

**Decision record.** One decision, its alternatives, and why this one. The law it supports is in
[terminal-pane](../workshop/terminal-pane.md).

**Context.** The Terminal pane showed the newest entries that fit whole, with one marker under
them counting "earlier" entries. An entry taller than the room its newer neighbours left was
never on screen, nothing above the newest entries could be reached, and the marker's unit was an
entry that the pane might be showing half of.

**Decision.** The pane wraps every entry of the record at its width and shows a view onto those
rows. The view follows the newest row until the maker reads away; then its top is anchored to an
entry, by `dropped` + index, and a wrapped row inside it. The row above the view counts rows
above and states evicted entries separately; while reading away, the row below counts rows below
and is the press back to the newest. Ctrl+Up and Ctrl+Down page, Ctrl+Home and Ctrl+End go to the
ends, the wheel reads three rows a notch, and a submit follows the newest again.

**Alternatives considered.**
- *Fitting the newest entries whole* -- replaced: a long entry could be clipped for good; pinned by
  case "a long entry is read whole by scrolling and no row of it is clipped for good".
- *Anchoring the view by row number* -- refused: new output, eviction and a re-wrap renumber rows;
  pinned by case "a resize re-wraps under the entry being read and the line and its caret stay
  usable".
- *PageUp and PageDown* -- not available: the input vocabulary names no page keys on any backend.
- *Counting entries in the markers* -- refused: a wrapped entry cut at the top is not a message.

**Consequences.** Following the newest output shows the end of a long entry; its start is above.
A reload starts following again. Ctrl+Home and Ctrl+End leave the line; Home and End still move
its caret.

**Laws supported.** [WL-TERM-14](../workshop/terminal-pane.md),
[WL-TERM-15](../workshop/terminal-pane.md).
