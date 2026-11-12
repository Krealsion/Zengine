# Workshop law — the Terminal's door

Register `WL-TERM`, continued from [`terminal.md`](terminal.md): the terminal PARTICIPANT Workshop
mounts and holds, the door onto it composed onto Workshop's weave (`workshop/terminal/door.hpp`),
its completer (`workshop/terminal/complete.hpp`), the picture of its record
(`workshop/terminal/transcript.hpp`) and what it may say. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). The pane that presents it is [`terminal.md`](terminal.md)'s.

## WL-TERM-02 — The participant stays this host's, and speaks as itself

LAW — `HostContext::terminal` is a non-owning pointer to the `loom::TerminalSession` this host mounted; a typed line reaches that participant's record and leaves through its door, as ITS identity.

MEANS
- the address grammar is Loom's own (`loom::tokenize`, `loom::parse_address`), never a second;
- `submit_terminal_line` is the one path in this process that speaks as that identity;
- no participant is a reading, not an absence: the door refuses, and the picture says so.

DOES NOT MEAN — that the participant could have migrated. Its handler sends nothing by
construction and it accepts only the three answer doors its host declared, so no message drives
it: the door is the only route inside the Loom fence. Nor that every hand reaches it: on a weaver's
host only the weaver's does (WL-GUEST-06); on a development host, the agent's own, a guest's line
leaves as the Terminal's too.

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-03 — The record crosses as a picture, said only when the reading changed

LAW — This host derives `TranscriptShown` on the repaint, compares it against the last utterance, and publishes it `to_any` as its own role only when it changed.

MEANS
- the picture is the WHOLE record (its owner bounds it); `dropped` is what that owner evicted;
- which rows a pane shows, and what is above and below them, are the PANE's (WL-TERM-14);
- silence when nothing changed terminates the seam: rows answer it, and a repaint follows.

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-04 — The completer reads the line's slot, and offers only what the submitter runs

LAW — The completer reads which slot the weaver is standing in — verb, address, shape, version, arguments — and the verbs offered are exactly the verbs the submitter runs: `send` and `ask`.

MEANS
- an address offers `*` and what the host read off the bus (WL-TERM-16), or the forms without;
- shapes are the catalog in the host's order; arguments offer field names, never values, in the shape's order or, by `workshop.completion-order`, required ones first;
- a quoted token is left alone: the quote is not on the line the completer sees.

DOES NOT MEAN — that it could cross as a picture. `compose` runs the real composition
ladder over the arguments already finished and resolves references against messages this
participant has
received, so the answer is a live fact and the pane asks for it (WL-TERM-05).

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-05 — What could be said next is an ask, and browsing authors nothing

LAW — A pane holding a line asks `TerminalCompletionRequested` and hears `TerminalCompletionOffered`; every call on that path is const, so browsing authors nothing.

MEANS
- the one path that authors is `TerminalActRequested`, and it is a different shape;
- `selected` is not sent: the pane owns it, and keeps it when slot and partial are unchanged.

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## WL-TERM-16 — Where a line can go is read off the bus at the ask, and kept by nobody

LAW — At the line's address, the host reads each weave on the bus that is not a sealed candidate -- id, roles now, accepted shapes, whether alive -- and hands that to the completer for one answer.

MEANS
- roles are listed by name, each role of a weave holding several among them, and weaves by id, each with what it is; the participant is marked;
- a weave loaded, removed or replaced shows on the next ask: nothing kept the last reading;
- it observes no traffic and holds nothing between asks: a reading, not a tap or a registry.

DOES NOT MEAN
- permission to send: a grant is per shape, and a listed weave may refuse or be refused;
- that it is there at send: a removed id is Loom's refusal, a role reaches its holder then.

WHY — `agents/decisions/destinations-are-read-off-the-bus.md`

## WL-TERM-17 — A transcript capture is content, acquired by the current actor

LAW — Capture reads an exact terminal instance and observation after current actor authorization; it never replays a line or transfers its old authority.

MEANS
- numbered wrapped rows retain observation identity; local prose is not a payload;
- evicted/unavailable or oversized values refuse; copies own their encoded schema and metadata;
- the receiver separately authorizes storage and later execution under the current actor.

WHY — `agents/decisions/transcript-values-are-reusable-content.md`

## WL-TERM-18 — The Terminal may ask the discovery door its two questions, and that is all

LAW — The host widens its Terminal with `let_terminal_find_powers`: it knows `FindPowers` (v1 and v2) and `DescribePower`, accepts their two answers, and may send them to `zengine.powers` alone.

MEANS
- the documented lines are typed against the host's own widening, as the switch's are;
- the door answers this participant's personal speech, refusals included, in the door's words;
- the same ask aimed at another role is `CapabilityDenied`: asking grants nothing more.

WHY — `agents/decisions/powers-are-found-through-one-door.md`

## WL-TERM-20 — What the Terminal may say is the host's one call, and none of it reaches a medium

LAW — The host mounts its Terminal with `let_terminal_speak`: the discovery door's find, its ordinary verb, the editor switch's questions and a pane's settings, each to its own role alone.

MEANS
- no rule reaches a window, a medium or everyone: being a terminal confers nothing else;
- `Drawn` is known, so the completer describes it, and no rule sends it;
- a case reads the grant through the call the host makes, so the two cannot differ.

DOES NOT MEAN — that a line could draw had a rule allowed it: a line cannot type its lists, so
the participant never composes one.

WHY — `agents/decisions/the-terminal-is-a-participant.md`

## Proven by

### WL-TERM-02
`workshop/terminal/door.hpp` submit_terminal_line on(TerminalActRequested)
`workshop/weave.hpp` HostContext::terminal
`workshop/terminal_seam_vocabulary.hpp` TerminalActRequested TerminalActed kTerminalSubmitAct
`workshop/terminal/transcript.hpp` transcript_shown
`tests/test_workshop_panes_terminal.cpp` "a typed send leaves through the PARTICIPANT's door, not
the pane's" "a Workshop with no participant says so, and authors nothing"

### WL-TERM-03
`workshop/terminal/transcript.hpp` transcript_shown same_transcript entry_kind entry_addressing
`workshop/terminal/door.hpp` say_transcript
`workshop/terminal_seam_vocabulary.hpp` TranscriptShown ShownEntry kEntryCommand kEntrySubmitted
kAddressWeave
`tests/test_workshop_panes_terminal.cpp` "the record crosses as a picture, said only when the
reading changed" "the pane says what it is not showing, in the two senses that differ"

### WL-TERM-04
`workshop/terminal/complete.hpp` read_command_line LineSlot TerminalVerb kTerminalVerbCount complete_line
Completion Candidate kTerminalVerbs CommandLine::said starts_with named_already CompletionFieldOrder
`workshop/terminal/door.hpp` submit_terminal_line slot_name
`workshop/screen.hpp` Session::completion_required_first
`tests/test_workshop_panes_terminal.cpp` "`workshop.completion-order` puts a shape's required fields
first in the Terminal's list, so accepting the first offer names what the message needs"
`tests/test_workshop_host.cpp` "arguments offer a shape's fields in its order, or its required
fields first and its optional ones after, each in its order"
"a half-typed line says which part of it the weaver is standing in"
"the verbs a weaver is offered are the verbs the submitter runs" "an address offers the three forms
and never pretends to know the values" "arguments offer field NAMES, never values, and the heading
is compose()'s verdict" "a quoted token is left alone, because the quote is not on the line the
completer sees"

### WL-TERM-05
`workshop/terminal/door.hpp` on(TerminalCompletionRequested)
`workshop/terminal_seam_vocabulary.hpp` TerminalCompletionRequested TerminalCompletionOffered
ShownCandidate kSlotVerb kSlotArguments
`tests/test_workshop_panes_terminal.cpp` "what could be said next is an ASK, and browsing authors
nothing" "the selection survives a recomputation and not a change of question" "accepting a
candidate edits the line, and the grammar's separators hold" "completion follows the END of the
line, and says so when it cannot"

### WL-TERM-16
`workshop/terminal/complete.hpp` Destination destination_detail complete_line CandidateKind
`workshop/terminal/door.hpp` bus_destinations on(TerminalCompletionRequested)
`workshop/weave.hpp` HostContext::destinations
`tests/test_workshop_host.cpp` "with the bus read the address offers everyone then each role held
now then each weave registered now with what it is" "a destination holding several roles is offered
as each of them, in role order"
`tests/test_workshop_panes_terminal.cpp` "the address lists the roles and weaves on the bus now
with what each is and every ask reads the bus again" "an id chosen from the list and gone before the
line is sent leaves the participant's refusal and reaches no other weave" "a role chosen from the
list reaches whoever holds it at delivery and a vacated role is refused not retargeted"

### WL-TERM-17
`workshop/weave_operation.cpp` on(TerminalValueRequested) authorize_pane_operation
`terminal/pane.cpp` acquire
`tests/test_workshop_inventory_info.cpp` "terminal capture: primary drag stores exact authored
content and names the new copy" "terminal capture: retrieval and Inventory storage need separate
current actor authority"

### WL-TERM-18
`workshop/powers_door.hpp` let_terminal_find_powers PowersDoor
`workshop/powers_vocabulary.hpp` allow_finding_powers find_powers_schema find_powers_v1_schema
`workshop/terminal_speech.hpp` let_terminal_find_powers
`tests/test_workshop_panes_powers.cpp` "the documented discovery lines, typed through Workshop's own
door, answer what the door derives" "the Terminal may ask the discovery door its two questions, and
nothing more"

### WL-TERM-20
`workshop/terminal_speech.hpp` let_terminal_speak
`workshop/workshop.cpp` let_terminal_speak
`tests/test_workshop_panes_powers.cpp` "the host's Terminal finds powers as its ordinary verb, speaks
only to the roles that widen it, and draws in no window"
