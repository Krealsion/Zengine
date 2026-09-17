# History is the participant's record, walked by the pane

**Decision record.** One decision, its alternatives, and why this one. The law it supports is in
[terminal-pane](../workshop/terminal-pane.md).

**Context.** A maker repeats and adjusts commands. The founder's rule: Up and Down browse command
history when no command is being composed, and keep browsing while a recalled command is shown;
Enter and Tab lock the recalled line in and do nothing else on that press; any other key returns
the arrows to completion. The participant already records each line it is asked to run as a
`command` entry, before parsing it, and the pane is already shown that record.

**Decision.** The pane walks the `command` entries of the picture it holds, newest first, a run of
one command as one step, each named by `dropped` + index so eviction does not move a position.
A recall keeps the whole box it started from and gives it back past the newest or on Escape.
Enter and Tab end a recall and are spent. Any key the line takes ends it and then does its own
work once. Every act that changes what a completion is about starts a new intent, and an answer
is taken only by the intent that asked for it.

**Alternatives considered.**
- *A log of submitted lines kept by the pane* -- refused: a second copy of the participant's
  record, lost on reload, and blind to lines another presentation ran.
- *Enter running a recalled line* -- refused by the founder: the press that locks does nothing
  else; pinned by case "on an empty line Up Up Enter leaves the older command ready to edit and
  runs nothing until the next Enter runs it".
- *Correlating a completion answer by line and caret alone* -- replaced: a cleared-and-retyped or
  recalled line can have the same bytes and a different intent; pinned by case "a completion
  answer asked before a recall is neither shown on the recalled line nor taken by its lock though
  the bytes match".
- *History across launches* -- not chosen: the participant and its record belong to the run.

**Consequences.** History is as long as the record keeps, 256 entries of every kind. A reload
while a line is recalled keeps the draft from before the recall. A key the line does not take
ends no recall.

**Laws supported.** [WL-TERM-12](../workshop/terminal-pane.md),
[WL-TERM-13](../workshop/terminal-pane.md).
