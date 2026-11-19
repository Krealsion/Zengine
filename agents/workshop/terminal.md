# Workshop law — the Terminal

Register `WL-TERM`: the Terminal pane, the seam across which it presents the participant Workshop
mounts. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md). The
participant, the door onto it, its completer and what it may say are Workshop's,
[`terminal-door.md`](terminal-door.md). The pane's own presentation — its rows, its list, its caret
and its keys — is the Terminal pane's: it draws them, its caret among them, as its own picture in
its area of the window, and the line it is typed into is [`text-box.md`](text-box.md). Its presses,
wheel and right press reach it there from the window ([`canvas.md`](canvas.md)).

Retired: WL-TERM-06, WL-TERM-07, WL-TERM-09, WL-TERM-10

## WL-TERM-01 — The Terminal is a pane, and nothing global reaches it

LAW — The Terminal is a loaded weave offered by `zengine.terminal`, arranged like any other pane; no key, chord or contextual row of this host's opens it or acts on it.

MEANS
- a weaver opens it from the Pane Manager and reaches its keys by pressing into it;
- its five action ids and gestures are the retired mode's, so an override moves with it;
- it wears a pane's boundary, so what it covers it covers legibly.

DOES NOT MEAN — that the participant moved. It is mounted by this host and reached by a pointer
this host holds (WL-TERM-02).

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-08 — The image that presents a participant cannot reach one

LAW — The pane's image, `zengine-terminal-pane`, includes no header declaring `loom::TerminalSession` or its transcript, links neither this host's logic nor Loom's terminal, and keeps no copy of the record.

DOES NOT MEAN — that the pane stands with nothing of the participant's beside it. The host's door,
its completer and the picture of the record are Workshop's (`workshop/terminal/`); the pane
includes none of them.

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

### WL-TERM-08
`terminal/CMakeLists.txt`
`terminal/pane.cpp` TerminalPaneWeave
`tests/test_workshop_panes_terminal.cpp` "the image that presents a participant cannot reach one"

### WL-TERM-11
`terminal/pane.cpp` on(TerminalCompletionOffered)
`terminal/history_and_completion.hpp` here moved offer_applies ask_completion selectable accept_candidate
`tests/test_workshop_panes_terminal.cpp` "a completion answer about a line that is gone is neither
shown nor taken" "an answer for a caret that has since moved does not reopen the list"

