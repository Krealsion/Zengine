# Workshop law — what a build said

Register `WL-OUT`: a build's output carried whole from the process that wrote it, kept by the
operation that said it, and read in the Builder pane on rows a canvas can draw. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md). The build conversation itself is
[`../realization.md`](../realization.md); the Builder's other rows are [`project.md`](project.md);
the pane a build's source is reached from is [`code.md`](code.md).

Retired: WL-OUT-01, WL-OUT-02, WL-OUT-04

## WL-OUT-03 — A pane shows the words it carries as written, and counts what it does not show

LAW — `printable` shows every character as written, a tab or any other control character as one space and a terminal escape sequence as nothing, counting each; the Builder pane shows every row it says so.

MEANS
- a compiler's quotes and a path past plain ASCII read on the row as the compiler wrote them;
- a line, a column and a caret line keep their columns, since a control character still takes one;
- what the tool keeps is never changed: only a row a pane shows is.

DOES NOT MEAN
- that the Editor admits such text: a source document stays printable ASCII.

WHY — `agents/decisions/a-build-keeps-its-own-words.md`

## Do not assume

- That a build's words are complete once kept: both ends are, and the middle of a long build is
  counted, not kept.
- That a row is the bytes the compiler wrote: it leaves out its escape sequences and shows its
  control characters as spaces, and the header says how many it did not show (WL-OUT-03).
- That a compiler's non-ASCII words arrive as it wrote them on every platform: on Windows Ninja
  re-encodes the output it passes on, and the kept bytes are what reached the pipe.

## Proven by

### WL-OUT-03
`pane/text.hpp` printable
`builder/builder-pane/pane.cpp` say
`builder/builder-pane/view.hpp` publish
`builder/builder-pane/output_reader.hpp` say_output
`tests/test_workshop_panes_output.cpp` "a compiler's non-ASCII words in a build's last lines leave
the Builder's picture standing" "read output shows a failed build's own lines on rows Workshop
takes, as written, the control characters it did not show said"
