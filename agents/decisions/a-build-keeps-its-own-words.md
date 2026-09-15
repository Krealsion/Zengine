# A build keeps its own words

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [build-output](../workshop/build-output.md).

**Context.** A maker whose build failed could not read the compiler's reason inside Workshop.
The runner joined each look's lines with ` | ` and kept its last 2,048 characters; the tool's
status carried the last three observations; the Builder's `said` rows showed that string's
start, usually a command echo. A row carrying GCC's UTF-8 quotes would also have refused the
Builder's whole picture at `judge_content`. The walkthrough sent makers to rerun the command in a
terminal.

**Decision.** The runner says every byte, in order, in bounded messages. The Builder tool keeps,
per operation, the lines at both ends of the output with the middle counted, for its last few
operations, and answers one bounded page to whoever asks by operation number. The Builder pane's
`read output` is a mode bound to one operation: one row per line, cut and panned rather than
wrapped, under a header that names the operation, its outcome, its lines and what was spelled,
cut or not kept. Every row the pane publishes is spelled in characters a canvas draws.

**Alternatives considered.**
- *A longer `said` block* — argued: rows cannot recover bytes the runner had already dropped.
- *Opening the output in the Editor* — argued: the Editor's source is printable ASCII, and a
  compiler's quotes would refuse the open or loosen what a source document is.
- *A log file per build* — argued: a second file custody (where, who prunes, which process
  writes) for a question a bounded record in the tool answers.
- *The pane accumulating `BuildOutput` itself* — argued: a closed or reloaded pane would lose a
  build it did not watch, and the pane would become a second owner of the tool's facts.
- *Wrapping long lines* — argued: a Ninja command echo of a few kilobytes would fill a nine-row
  pane before the error line, and a caret line would wrap away from the column it points at.
- *Keeping only the tail* — argued: a compiler's first error leads a long failure.

**Consequences.** A maker reads the diagnostic, returns to the Editor and fixes the line; going
to that line is still the Editor's wheel. More `BuildOutput` messages cross the bus in a chatty
build, each bounded.

**Laws supported.** [WL-OUT-01](../workshop/build-output.md),
[WL-OUT-02](../workshop/build-output.md), [WL-OUT-03](../workshop/build-output.md),
[WL-OUT-04](../workshop/build-output.md).
