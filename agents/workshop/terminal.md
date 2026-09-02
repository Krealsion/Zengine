# Workshop law — the Terminal

Register `WL-TERM`: the Terminal overlay as a mode, its pane, and its completion, written from
the tests. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md). The pane's
placement is [`geometry.md`](geometry.md) (WL-GEO-02) and its editable line is
[`text-box.md`](text-box.md).

## WL-TERM-01 — The Terminal is a modal overlay, toggled by one action

LAW — `workshop.terminal` (`^t`) opens the overlay and the same toggle closes it; its keystroke never becomes text, and while it is open the keys and the pointer belong to it.

MEANS
- a closed overlay leaves every ordinary Workshop gesture exactly as it was;
- the above-mode chords still work inside it: `^s` saves, `^c` copies.

PROVEN BY — `workshop/weave.hpp` `toggle_terminal`, `terminal_key`, `terminal_press`;
`workshop/screen.hpp` `TerminalPane`; `workshop/keymap.hpp` `workshop.terminal`, `kTerminal`;
`tests/test_workshop_screen.cpp` case `"the terminal toggle opens the overlay, and the same
toggle closes it"`, case `"the toggle's own keystroke never becomes text, in either direction"`,
case `"a closed overlay leaves every ordinary Workshop gesture exactly as it was"`, case `"while
the overlay is open the keys and the pointer belong to it"`, case `"^s still means save with the
overlay open, and ^c means copy there (TEXT-0)"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-02 — The pane presents an ordinary participant on Workshop's own bus

LAW — A typed line reaches the `loom::TerminalSession`'s own record; a typed send leaves through the participant's door, not as Workshop; the address grammar is Loom's; an ask waits for Loom's answer.

MEANS
- an address without its sigil sends nothing and records nothing.

PROVEN BY — `workshop/weave.hpp` `submit_terminal_line`, `terminal`; `workshop/complete.hpp`
`read_command_line`; `workshop/screen.hpp` `terminal_address`; `tests/test_workshop_screen.cpp`
case `"a typed line reaches the participant's own record, understood or not"`, case `"a typed send
leaves through the PARTICIPANT's door, on Workshop's own bus"`, case `"the address grammar the
pane reads is Loom's own, not a second one"`, case `"an ask waits, and LOOM's own answer settles
it on the screen"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-03 — The pane is one bounded region that fits entries, and says what it omits

LAW — The pane is one bounded region placed in cells; it fits entries rather than lines (`entries_that_fit`), and `terminal_omission` says what it is not showing in the two senses that differ.

MEANS
- `earlier` counts what the window left out; `dropped` counts what the record itself dropped;
- a medium that sets type reflows the pane, and the omission stays true;
- the snapshot outlives the participant it came from.

PROVEN BY — `workshop/screen.hpp` `TerminalPane`, `entries_that_fit`, `terminal_omission`,
`paint_terminal`, `kTerminalMinH`, `kTerminalChrome`, `kTerminalMinCols`, `terminal_cols`,
`terminal_rows`; `tests/test_workshop_screen.cpp` case `"the pane is published as ONE bounded
region, placed in cells"`, case `"a medium that sets real type reflows the pane, and the omission
stays true"`, case `"the pane says what it is not showing, in the two senses that differ"`, case
`"the pane's snapshot outlives the participant it came from"`; `tests/test_workshop_panels.cpp`
case `"a pane fits ENTRIES, not lines, and says what it could not show"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-04 — The completer reads the line's slot, and offers only what the submitter runs

LAW — The completer reads which slot the maker is standing in — verb, address, shape, version, arguments — and the verbs offered are exactly the verbs the submitter runs: `send` and `ask`.

MEANS
- an address offers three forms and never pretends to know the values;
- shapes are the catalog in the host's order; arguments offer field names, never values;
- a quoted token is left alone: the quote is not on the line the completer sees.

PROVEN BY — `workshop/complete.hpp` `read_command_line`, `LineSlot`, `TerminalVerb`,
`kTerminalVerbCount`, `complete_line`, `Completion`, `Candidate`, `kTerminalVerbs`, `said`,
`starts_with`, `named_already`; `tests/test_workshop_panels.cpp` case `"a half-typed line says
which part of it the maker is standing in"`, case `"the verbs a maker is offered are the verbs the
submitter runs"`, case `"an address offers the three forms and never pretends to know the
values"`, case `"arguments offer field NAMES, never values, and the heading is compose()'s
verdict"`, case `"a quoted token is left alone, because the quote is not on the line the completer
sees"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-05 — Browsing candidates authors nothing

LAW — Browsing sends no traffic, opens no ask and writes no transcript entry; accepting a candidate edits the line with the grammar's separators right, and an untouched line asks nothing.

MEANS
- the completion keys are unbound in this mode: Tab opens, Up and Down move, Up at the top stays.

PROVEN BY — `workshop/weave.hpp` `accept_completion`, `move_completion`,
`completion_selectable`; `workshop/screen.hpp` `completion_rows`, `completion_first_shown`,
`kCompletionMinRows`, `dismissed`, `asked`; `tests/test_workshop_panels.cpp` case `"browsing
candidates authors NOTHING -- no traffic, no ask, no transcript entry"`, case `"accepting a
candidate edits the line, and the grammar's separators stay right"`, case `"the completion keys
were unbound in this mode, and the ones that were not still work"`, case `"an untouched line asks
nothing, so the answer to the last command stays readable"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-06 — The completion list is a bounded region inside the pane

LAW — The list never lies over the input line; it says which slice of the vocabulary it shows, and it covers transcript rows without changing what the pane omits.

MEANS
- the list clears the input line under a real metric too, where a row is not a cell;
- the terminal medium projects the list honestly, ground and all.

PROVEN BY — `workshop/screen.hpp` `completion_rows`, `completion_first_shown`,
`CompletionPlace`; `tests/test_workshop_panels.cpp` case `"the list is a bounded region inside the
pane, and never over the input line"`, case `"the list clears the input line under a real metric
too, where a row is not a cell"`, case `"the list says which slice of a long vocabulary it is
showing"`, case `"the list covers transcript rows and changes nothing about what the pane omits"`,
case `"the terminal medium projects the list honestly, ground and all"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-07 — Wrapping is a presentation act

LAW — A sentence takes as many rows as it needs, and the pane states its whole grammar wrapped with nothing elided.

PROVEN BY — `workshop/screen.hpp` `paint_terminal`, `wrap`, `terminal_line`, `terminal_legend`,
`terminal_wrapped`; `tests/test_workshop_panels.cpp` case `"wrapping is a presentation act: as
many rows as the sentence needs"`, case `"the pane states its whole grammar, wrapped, with nothing
elided"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-08 — A fresh skin clears nothing, and no participant is said plainly

LAW — A fresh skin's hello does not clear the presentation context, and a Workshop with no participant says so and authors nothing; `open` is the mode and `attached` the participant.

PROVEN BY — `workshop/weave.hpp` `refresh_terminal`, `attached`; `workshop/screen.hpp`
`TerminalPane`; `tests/test_workshop_screen.cpp` case `"a fresh skin's hello does NOT clear the
presentation context -- measured, not blessed"`, case `"a Workshop with no participant says so
and authors nothing"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-09 — A press in the pane is the pane's

LAW — A press on the input row places the caret where the maker aimed, a press in the pane never reaches the workspace, and both media answer the caret in their own type.

MEANS
- opening the pane mid-drag does not strand the gesture;
- clicking a completion row selects it, and Tab accepts what was clicked;
- a terminal medium's press reaches the same local hit model.

PROVEN BY — `workshop/weave.hpp` `terminal_press`; `workshop/screen.hpp` `terminal_input_place`,
`kTerminalPromptCols`, `kTerminalCaretCols`, `TerminalInputPlace`, `terminal_input_hit`;
`tests/test_workshop_screen.cpp` case `"HD-3: a press on the input row places the caret where the
maker aimed"`, case `"HD-3: a press in the pane never reaches the workspace underneath it"`, case
`"HD-3: opening the pane mid-drag does not strand the gesture"`, case `"HD-3: clicking a
completion row selects it, and Tab accepts what was clicked"`, case `"HD-3: the pane publishes a
caret, and both media answer it in their own type"`.
WHY — `agents/decisions/the-terminal-is-a-participant.md`
