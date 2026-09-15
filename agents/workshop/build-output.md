# Workshop law — what a build said

Register `WL-OUT`: a build's output carried whole from the process that wrote it, kept by the
operation that said it, and read in the Builder pane on rows a canvas can draw. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md). The build conversation itself is
[`../realization.md`](../realization.md); the Builder's other rows are [`project.md`](project.md);
the pane a build's source is reached from is [`code.md`](code.md).

## WL-OUT-01 — The runner says every byte a build writes, in order

LAW — Each look's ready bytes leave the runner whole and in order, in as many `BuildOutput` messages of at most `kMaxOutputChars` as they need; nothing is joined, cut or counted away.

MEANS
- a message ends at a line break where one fits; a longer line continues in the next one;
- a look is still bounded by `kMaxLookBytes`: what it leaves stays in the pipe for the next look;
- a status's `detail` is the last lines that say something, joined with ` | `; blanks spend none.

DOES NOT MEAN
- that the bus keeps a build's output: the tool keeps it, bounded (WL-OUT-02);
- that the bytes are a compiler's: they are the build's, as any tool it runs re-encoded them.

PROVEN BY — `builder/runner.hpp` `output_piece`, `BuildRunnerWeave::look_at_held`;
`builder/vocabulary.hpp` `kMaxOutputChars`, `BuildOutput`, `tail_lines`;
`tests/test_builder.cpp` case `"the runner says every byte a build writes, in order, in pieces no
bigger than a message"`, case `"the tail a panel is shown keeps whole lines, and keeps them
APART"`.
WHY — `agents/decisions/a-build-keeps-its-own-words.md`

## WL-OUT-02 — The tool keeps what its last operations said, and answers a page by operation

LAW — `BuilderWeave` keeps both ends of each of its last `kKeptOperations` operations' lines, the middle counted, and answers `BuildOutputRequested` with one bounded page of that operation or `kept` false.

MEANS
- a kept line is its bytes, a CR the break's; a line past `kMaxKeptLineBytes` is cut and counted;
- a page never crosses the lines not kept: asked inside them it begins after; `from` 0 ends last;
- an operation let go is `kept` false with the numbers kept, and never another operation's lines.

DOES NOT MEAN
- that output is a log: it is written to no file, kept across no restart and searched by nothing.

PROVEN BY — `builder/weave.hpp` `KeptOutput`, `page_of`, `on(BuildOutputRequested)`,
`BuilderWeave::kept_`; `builder/vocabulary.hpp` `BuildOutputRequested`, `BuildOutputSaid`,
`kKeptOperations`, `kKeptHeadBytes`, `kKeptTailBytes`, `kMaxKeptLineBytes`,
`kMaxOutputPageLines`, `kMaxOutputPageBytes`; `workshop/workshop.cpp` `BuildOutputSaid`;
`tests/test_builder.cpp` case `"a failed build's own lines are kept by its operation, and a page
reads them as the build wrote them"`, case `"a kept record holds both ends of a long output,
numbers the gap, and never pages across it"`, case `"the tool keeps the last few operations'
output, and an operation it let go is said, not replaced"`.
WHY — `agents/decisions/a-build-keeps-its-own-words.md`

## WL-OUT-03 — A pane spells the words it carries in what a canvas draws, and counts them

LAW — `ascii_spelling` gives compilers' UTF-8 punctuation its ASCII twin, any other undrawable character `?` and an escape sequence nothing, counting each; the Builder pane spells every row it says.

MEANS
- a picture carrying a compiler's quotes is one Workshop takes, not one `judge_content` refuses;
- printable ASCII passes untouched, so a path, a line and column and a caret line read as written;
- what the tool keeps is never spelled: only a row a pane shows is.

DOES NOT MEAN
- that the Editor admits such text: a source document stays printable ASCII.

PROVEN BY — `workshop/pane_text.hpp` `ascii_spelling`; `builder-pane/pane.cpp` `say`, `publish`,
`say_output`; `tests/test_workshop_panes_output.cpp` case `"a compiler's non-ASCII words in a
build's last lines leave the Builder's picture standing"`, case `"read output shows a failed
build's own lines on rows Workshop takes, spelled in ASCII and said to be"`.
WHY — `agents/decisions/a-build-keeps-its-own-words.md`

## WL-OUT-04 — The Builder's reader is bound to one operation until it is closed

LAW — `read output` binds a mode to the operation the tool's picture names: a header saying how it ended, which lines and what was spelled, cut or not kept, then one row per line, cut and panned.

MEANS
- a newer build, a status about another operation or a new choice moves nothing it shows;
- `[` and `]` step to the older or newer operation kept; Escape gives back the build rows;
- a reloaded Builder pane is not reading, and the tool answers again when the reader reopens.

DOES NOT MEAN
- that a refused realization is a failed build: the reader says the build succeeded;
- that the reader builds, opens a file or moves a caret: it reads.

PROVEN BY — `builder-pane/pane.cpp` `open_output`, `say_output`, `read_output`, `scroll`,
`ask_page`; `builder-pane/vocabulary.hpp` `kActionOutput`, `kActionOutputClose`;
`tests/test_workshop_panes_output.cpp` case `"read output shows a failed build's own lines on rows
Workshop takes, spelled in ASCII and said to be"`, case `"the reader stays bound to its build: a
newer build and a new status do not move it, and it steps between the builds kept"`, case `"a
build the tool no longer keeps is said, and no other build's lines are shown under its
number"`, case `"a build that worked and a realization that was refused read as two answers, and
the reader says the build succeeded"`.
WHY — `agents/decisions/a-build-keeps-its-own-words.md`

## Do not assume

- That a build's words are complete once kept: both ends are, and the middle of a long build is
  counted, not kept (WL-OUT-02).
- That a spelled row is the bytes the compiler wrote: it is what a canvas can draw of them, and
  the header says how many characters were spelled (WL-OUT-03).
- That a compiler's non-ASCII words arrive as it wrote them on every platform: on Windows Ninja
  re-encodes the output it passes on, and the kept bytes are what reached the pipe (WL-OUT-01).
