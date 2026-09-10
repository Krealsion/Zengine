# Workshop law — the Terminal

Register `WL-TERM`: the terminal PARTICIPANT this host mounts and holds, and the seam across
which a pane presents it. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). The pane's own presentation — its rows, its list, its caret
and its keys — is the Terminal pane's, not this host's; the caret it publishes is
[`panes-and-windows.md`](panes-and-windows.md) (WL-CARET) and the line it is typed into is
[`text-box.md`](text-box.md).

## WL-TERM-01 — The Terminal is a pane, and nothing global reaches it

LAW — The Terminal is a loaded weave offered by `zengine.terminal`, arranged like any other pane; no key, chord or contextual row of this host's opens it or acts on it.

MEANS
- a maker opens it from the picker and reaches its keys by pressing into it (VD-22);
- its five action ids and gestures are the retired mode's, so an override moves with it;
- it wears a pane's boundary, so what it covers it covers legibly.

DOES NOT MEAN — that the participant moved. It is mounted by this host and reached by a pointer
this host holds (WL-TERM-02).

PROVEN BY — `terminal-pane/vocabulary.hpp` `kTerminalPaneRole`, `kTerminalPane`,
`kActionSubmit`, `kActionBack`, `kActionUp`, `kActionDown`, `kActionComplete`;
`workshop/default-load-plan.json`; `tests/test_workshop_panes_terminal.cpp` case `"TERM-W1: the
Terminal is an ordinary arranged pane, offered by an office"`, case `"TERM-W2: the five keys are
the pane's rows, on the built-in's own spellings"`, case `"TERM-W3: nothing global opens it, and
no key acts on it from anywhere else"`, case `"TERM-W4: a maker presses in, types a line, and the
participant runs it"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-02 — The participant stays this host's, and speaks as itself

LAW — `HostContext::terminal` is a non-owning pointer to the `loom::TerminalSession` this host mounted; a typed line reaches that participant's record and leaves through its door, as ITS identity.

MEANS
- the address grammar is Loom's own (`loom::tokenize`, `loom::parse_address`), never a second;
- `submit_terminal_line` is the one path in this process that speaks as that identity;
- no participant is a reading, not an absence: the door refuses, and the picture says so.

DOES NOT MEAN — that the participant could have migrated. Its handler sends nothing by
construction and it accepts only the three answer doors its host declared, so no message drives
it: the door is the only route inside the Loom fence.

PROVEN BY — `workshop/weave_terminal.cpp` `submit_terminal_line`, `on(TerminalActRequested)`;
`workshop/weave.hpp` `HostContext::terminal`; `workshop/terminal_seam_vocabulary.hpp`
`TerminalActRequested`, `TerminalActed`, `kTerminalSubmitAct`; `workshop/screen_terminal.cpp`
`transcript_shown`; `tests/test_workshop_panes_terminal.cpp` case `"TERM-W5: a typed send leaves
through the PARTICIPANT's door, not the pane's"`, case `"TERM-W8: a Workshop with no participant
says so, and authors nothing"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-03 — The record crosses as a picture, said only when the reading changed

LAW — This host derives `TranscriptShown` on the repaint, compares it against the last utterance, and publishes it `to_any` as its own office only when it changed.

MEANS
- the picture is the WHOLE record (its owner bounds it); `dropped` is what that owner evicted;
- what a pane shows whole, and what each entry costs, are the PANE's: `earlier` is not sent;
- silence when nothing changed terminates the seam: rows answer it, and a repaint follows.

PROVEN BY — `workshop/screen_terminal.cpp` `transcript_shown`, `same_transcript`, `entry_kind`,
`entry_addressing`; `workshop/weave_terminal.cpp` `say_transcript`;
`workshop/terminal_seam_vocabulary.hpp` `TranscriptShown`, `ShownEntry`, `kEntryCommand`,
`kEntrySubmitted`, `kAddressWeave`; `tests/test_workshop_panes_terminal.cpp` case `"TERM-W6: the
record crosses as a picture, said only when the reading changed"`, case `"TERM-W9: the pane says
what it is not showing, in the two senses that differ"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-04 — The completer reads the line's slot, and offers only what the submitter runs

LAW — The completer reads which slot the maker is standing in — verb, address, shape, version, arguments — and the verbs offered are exactly the verbs the submitter runs: `send` and `ask`.

MEANS
- an address offers three forms and never pretends to know the values;
- shapes are the catalog in the host's order; arguments offer field names, never values;
- a quoted token is left alone: the quote is not on the line the completer sees.

DOES NOT MEAN — that it could cross as a picture. `compose` runs the real composition
ladder over the arguments already finished and resolves references against messages this
participant has
received, so the answer is a live fact and the pane asks for it (WL-TERM-05).

PROVEN BY — `workshop/complete.hpp` `read_command_line`, `LineSlot`, `TerminalVerb`,
`kTerminalVerbCount`, `complete_line`, `Completion`, `Candidate`, `kTerminalVerbs`,
`CommandLine::said`, `starts_with`, `named_already`; `workshop/weave_terminal.cpp`
`submit_terminal_line`, `slot_name`; `tests/test_workshop_panels.cpp` case `"a half-typed line
says which part of it the maker is standing in"`, case `"the verbs a maker is offered are the
verbs the submitter runs"`, case `"an address offers the three forms and never pretends to know
the values"`, case `"arguments offer field NAMES, never values, and the heading is compose()'s
verdict"`, case `"a quoted token is left alone, because the quote is not on the line the completer
sees"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-05 — What could be said next is an ask, and browsing authors nothing

LAW — A pane holding a line asks `TerminalCompletionRequested` and hears `TerminalCompletionOffered`; every call on that path is const, so browsing authors nothing.

MEANS
- the one path that authors is `TerminalActRequested`, and it is a different shape;
- `selected` is not sent: the pane owns it, and keeps it when slot and partial are unchanged.

PROVEN BY — `workshop/weave_terminal.cpp` `on(TerminalCompletionRequested)`;
`workshop/terminal_seam_vocabulary.hpp` `TerminalCompletionRequested`,
`TerminalCompletionOffered`, `ShownCandidate`, `kSlotVerb`, `kSlotArguments`;
`tests/test_workshop_panes_terminal.cpp` case `"TERM-W13: what could be said next is an ASK, and
browsing authors nothing"`, case `"TERM-W15: the selection survives a recomputation and not a
change of question"`, case `"TERM-W16: accepting a candidate edits the line, and the grammar's
separators hold"`, case `"TERM-W17: completion follows the END of the line, and says so when it
cannot"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-06 — The completion list is rows inside the pane, never a second region

LAW — The list is rows of the pane's own `PaneContent`, above the input row and taking room from the transcript; it says which slice it shows, and never lies over the input line.

DOES NOT MEAN — that it still floats. It was a SECOND bounded region drawn on top of the
overlay's own; a pane publishes one list of rows and Workshop assembles one region from it, so
covering the
transcript became taking rows from it. That is a change a maker sees.

PROVEN BY — `terminal-pane/pane.cpp` `say_list`, `first_shown`;
`tests/test_workshop_panes_terminal.cpp` case `"TERM-W14: the list is rows INSIDE the pane, above
the line it belongs to"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-07 — Wrapping is a presentation act, and it is the pane's

LAW — A transcript entry takes as many of the pane's rows as its sentence needs; the core records it whole, so a length is a question about the pane and never about the grammar.

PROVEN BY — `terminal-pane/pane.cpp` `entry_line`, `entry_wrapped`, `entries_that_fit`,
`legend_text`, `omission_text`; `workshop/pane_text.hpp` `wrap`;
`workshop/weave_terminal.cpp` `submit_terminal_line`; `tests/test_workshop_panes_terminal.cpp`
case `"TERM-W4: a maker presses in, types a line, and the participant runs it"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-08 — The image that presents a participant cannot reach one

LAW — `terminal-pane/` includes no header declaring `loom::TerminalSession` or its transcript, links neither this host's logic nor Loom's terminal, and keeps no copy of the record.

PROVEN BY — `terminal-pane/CMakeLists.txt`; `terminal-pane/pane.cpp` `TerminalPaneWeave`;
`tests/test_workshop_panes_terminal.cpp` case `"TERM-W7: the image that presents a participant
cannot reach one"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-09 — A press in the pane is the pane's, and there is no sweep

LAW — A press on the input row places the caret through the same window the row was drawn with; a press on a candidate row chooses it; a press elsewhere in the pane changes nothing.

DOES NOT MEAN — that a selection can be swept by pointer. A pane is sent a press and is sent no
motion and no release, so the drag the overlay performed across its own line is GONE: a second
press in the same word selects it, and shift with the caret keys sweeps by keyboard.

PROVEN BY — `terminal-pane/pane.cpp` `on(PanePressed)`, `say_caret`;
`workshop/terminal_seam_vocabulary.hpp` `TerminalCompletionOffered`;
`tests/test_workshop_panes_terminal.cpp` case `"TERM-W12: a press on the input row places the
caret where the maker aimed"`, case `"TERM-W10: the pane publishes a caret, and Workshop draws it
into the region"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-10 — A refusal is budgeted with the rows, beside the line it is about

LAW — The pane budgets the input row first and a standing refusal second, before any row is composed; two rows is the smallest room that holds both, and one row is the line's.

MEANS
- the caret and the press mapping name rows the pane actually published, always;
- a row added after the budget was spent takes back the last one composed, which is the line.

DOES NOT MEAN — that a refusal is dropped when it does not fit. It is dropped only where there
is no row for it that is not the maker's own line, and that room is one row.

PROVEN BY — `terminal-pane/pane.cpp` `say`, `say_caret`, `kChromeRows`;
`workshop/panel.hpp` `ExternalPane::caret_row`; `tests/test_workshop_panes_terminal.cpp` case
`"TERM-W19: a refusal is said BESIDE the line it is about, never in place of it"`, case
`"TERM-W19b: in a room too small for both, the LINE is what survives"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-11 — A completion answer applies to the line and caret it was asked about

LAW — An ask records the line and the caret it is about; an answer that comes back to a different line or caret is neither shown nor accepted, and the question is put again for the line that is there.

MEANS
- correlation says which question an answer is to, never that the question still stands;
- Escape, a submit, a caret leaving the end and a further keystroke each end one;
- the selection still survives a recomputation of the same question (WL-TERM-05).

DOES NOT MEAN — that a maker never sees the gap. The ask and its answer are one turn of the
host's drain; what a medium draws inside that turn is a different question, and unmeasured.

PROVEN BY — `terminal-pane/pane.cpp` `here`, `offer_applies`, `ask_completion`,
`on(TerminalCompletionOffered)`, `selectable`, `accept_candidate`;
`tests/test_workshop_panes_terminal.cpp` case `"TERM-W20: a completion answer about a line that
is gone is neither shown nor taken"`, case `"TERM-W20b: an answer for a caret that has since
moved does not reopen the list"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`
