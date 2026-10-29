# Enter takes the candidate the weaver moved to

**Decision record.** One decision, its alternatives, and why this one. The law it supports is in
[terminal-pane](../workshop/terminal-pane.md).

**Context.** The completion list puts its cursor on its first row, Up and Down move it, and Tab
takes what it is on. Enter ran the line as typed wherever the cursor stood, so a weaver who moved to
a candidate and pressed Enter, as a chosen row is taken in most lists, ran a half-typed line. The
founder's rule: Enter is the completion once the weaver has moved the list's cursor to a row, and
Shift+Enter enters the line early.

**Decision.** The pane keeps whether the weaver moved the cursor: Up, Down and a press on a
candidate's row move it; whatever puts the cursor back on the first row -- a new question, a
recall, a submit -- and a take and a dismissal leave it unmoved. Enter takes the candidate through
Tab's own take while it is moved, and runs the line otherwise; Shift+Enter, `terminal.submit-early`,
runs the line as typed. While the cursor is moved the list's heading says so, and the completer's
verdict names no key.

**Alternatives considered.**
- *Enter takes the first row too* -- refused: the line as typed would then need Shift every time a
  list stands open, which typing opens.
- *Shift+Enter running a recalled line* -- refused: the founder refused one press running a
  recalled line; pinned by case "Shift+Enter on a recalled line locks it in and runs nothing, and
  the next Shift+Enter runs it".
- *No key for a POSIX terminal* -- refused: Shift on Return never arrives there (`posix_gap`), and
  every shipped action has a key a POSIX terminal delivers; `ctrl+e` is the one Compose's submit
  names there.

**Consequences.** A recomputation of the same list keeps the cursor moved, as it keeps the cursor.
A key moved in the weaver's file moves the meaning, not the words the heading says.

**Laws supported.** [WL-TERM-21](../workshop/terminal-pane.md).
