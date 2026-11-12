# Workshop law — the Terminal

Register `WL-TERM`: the Terminal pane, the seam across which it presents the participant Workshop
mounts. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md). The
participant, the door onto it, its completer and what it may say are Workshop's,
[`terminal-door.md`](terminal-door.md). The pane's own presentation — its rows, its list, its caret
and its keys — is the Terminal pane's: it draws them, its caret among them, as its own picture in
its area of the window, and the line it is typed into is [`text-box.md`](text-box.md). Its presses,
wheel and right press reach it there from the window ([`canvas.md`](canvas.md)).

## WL-TERM-01 — The Terminal is a pane, and nothing global reaches it

LAW — The Terminal is a loaded weave offered by `zengine.terminal`, arranged like any other pane; no key, chord or contextual row of this host's opens it or acts on it.

MEANS
- a weaver opens it from the Pane Manager and reaches its keys by pressing into it;
- its five action ids and gestures are the retired mode's, so an override moves with it;
- it wears a pane's boundary, so what it covers it covers legibly.

DOES NOT MEAN — that the participant moved. It is mounted by this host and reached by a pointer
this host holds (WL-TERM-02).

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-06 — The completion list is rows inside the pane, never a second region

LAW — The list is rows of the pane's own picture, above the input row and taking room from the transcript; it says which slice it shows, and never lies over the input line.

DOES NOT MEAN — that it still floats. It was a SECOND bounded region drawn on top of the
overlay's own; the pane draws its list and its line as rows of the one picture it sends, so
covering the transcript became taking rows from it. That is a change a weaver sees.

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-07 — Wrapping is a presentation act, and it is the pane's

LAW — A transcript entry takes as many of the pane's rows as its sentence needs; the core records it whole, so a length is a question about the pane and never about the grammar.

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-08 — The image that presents a participant cannot reach one

LAW — The pane's image, `zengine-terminal-pane`, includes no header declaring `loom::TerminalSession` or its transcript, links neither this host's logic nor Loom's terminal, and keeps no copy of the record.

DOES NOT MEAN — that the pane stands with nothing of the participant's beside it. The host's door,
its completer and the picture of the record are Workshop's (`workshop/terminal/`); the pane
includes none of them.

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-09 — A press in the pane is the pane's, and the Terminal asks for no sweep

LAW — A press on the input row places its caret; a candidate row selects it; a numbered transcript value row starts an authorized copy drag.

DOES NOT MEAN — that this pane sweeps a selection by pointer. Workshop owns copy-drag motion
and release. The Terminal still does not sweep input text by pointer; a second press in the same
word selects it, and shift with caret keys selects by keyboard.

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-10 — A refusal is budgeted with the rows, beside the line it is about

LAW — The pane budgets the input row first and a standing refusal second, before any row is composed; two rows is the smallest room that holds both, and one row is the line's.

MEANS
- the caret and the press mapping name rows the pane actually published, always;
- a row added after the budget was spent takes back the last one composed, which is the line.

DOES NOT MEAN — that a refusal is dropped when it does not fit. It is dropped only where there
is no row for it that is not the weaver's own line, and that room is one row.

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-11 — A completion answer applies to the line and caret it was asked about

LAW — An ask records the intent, line and caret it is about; an answer that comes back to another is neither shown nor accepted, and is asked again only when the intent standing then wants a list.

MEANS
- correlation says which question an answer is to, never that the question still stands;
- an edit, a caret move, a dismissal, a clear, a submit, a recall and its lock each end one;
- the selection still survives a recomputation of the same question (WL-TERM-05).

DOES NOT MEAN — that a weaver never sees the gap. The ask and its answer are one turn of the
host's drain; what a medium draws inside that turn is a different question, and unmeasured.

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## Proven by

### WL-TERM-01
`terminal/vocabulary.hpp` kTerminalPaneRole kTerminalPane kActionSubmit kActionBack kActionUp
kActionDown kActionComplete
`workshop/default-load-plan.json`
`tests/test_workshop_panes_terminal.cpp` "the Terminal is an ordinary arranged pane, offered by a
role" "the five keys are the pane's rows, on the built-in's own spellings" "nothing global opens
it, and no key acts on it from anywhere else" "a weaver presses in, types a line, and the
participant runs it"

### WL-TERM-06
`terminal/view.hpp` say_list first_shown
`tests/test_workshop_panes_terminal.cpp` "the list is rows INSIDE the pane, above the line it
belongs to"

### WL-TERM-07
`terminal/view.hpp` entry_line entry_wrapped wrap_record legend_text omission_text
`pane/text.hpp` wrap
`workshop/terminal/door.hpp` submit_terminal_line
`tests/test_workshop_panes_terminal.cpp` "a weaver presses in, types a line, and the participant
runs it"

### WL-TERM-08
`terminal/CMakeLists.txt`
`terminal/pane.cpp` TerminalPaneWeave
`tests/test_workshop_panes_terminal.cpp` "the image that presents a participant cannot reach one"

### WL-TERM-09
`pane/facets.hpp` PaneRowsFacet::on(AreaButtonChanged)
`terminal/pane.cpp` press
`terminal/view.hpp` caret
`workshop/terminal_seam_vocabulary.hpp` TerminalCompletionOffered
`tests/test_workshop_panes_terminal.cpp` "a press on the input row places the caret where the weaver
aimed" "the pane draws its caret in its own picture, in its area of the window"

### WL-TERM-10
`terminal/view.hpp` say caret kChromeRows
`pane/rows.hpp` RowsCaret
`tests/test_workshop_panes_terminal.cpp` "a refusal is said BESIDE the line it is about, never in
place of it" "in a room too small for both, the LINE is what survives"

### WL-TERM-11
`terminal/pane.cpp` on(TerminalCompletionOffered)
`terminal/history_and_completion.hpp` here moved offer_applies ask_completion selectable accept_candidate
`tests/test_workshop_panes_terminal.cpp` "a completion answer about a line that is gone is neither
shown nor taken" "an answer for a caret that has since moved does not reopen the list"

