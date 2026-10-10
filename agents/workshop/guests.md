# Workshop law — a guest's hand

Register `WL-GUEST`: who a guest is to this Workshop and what its hand may do beside its grant --
the guests file's version and host, the row that admitted each session, the action classes an
owner asks for, the places only the weaver's hand reaches on a weaver's host, each hand's own
gestures, and what a row's powers do not reach, said beside the row. The file's form and its powers
are public in [Drive Workshop from another host](../../external-host/docs/external-host.md). One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-GUEST-01 — The guests file names its version, and version 2 adds build and the host

LAW — A guests file names its version, absent being 1; a version this Workshop does not read, or a word of a later version than the file's, refuses it in words; version 2 adds `build` and the `host`.

MEANS
- `host` is `weaver`, the default, or `development`; a version-1 file is a weaver's host;
- `build` grants no shape: no guest is granted `BuildRequested`, a realization ask or a build door;
- the file is read once, at launch: a changed row applies at the next launch.

PROVEN BY — `workshop/guests.cpp` `read_guests_file`, `grant_for`; `workshop/guests.hpp`
`kGuestsFileVersion`, `kPowerBuild`, `kHostWeaver`, `kHostDevelopment`, `GuestsFile::version`,
`GuestsFile::host`; `tests/test_workshop_guests.cpp` case `"guests file: a file names its
version, version 2 adds build and the host, and a word its version does not know is refused"`,
case `"guests file: build grants no shape -- no guest is granted BuildRequested, a realization
ask or a build door"`.
WHY — `agents/decisions/a-guests-hand-is-judged-by-its-row-and-its-host.md`

## WL-GUEST-02 — Admission records the row it admitted, per connection

LAW — The admission policy records the row whose credential a connection presented, at admit and at defer, keyed by the connection; a session's row is read from that record, never found by name.

MEANS
- two rows of one name stay two rows: each session holds the row its own credential named;
- observation reads the same record;
- a session whose connection has gone keeps its row: an ask on its way is judged as that guest's.

PROVEN BY — `workshop/guests.hpp` `AdmittedRows`; `workshop/guests.cpp` `admission_of`,
`observation_of`; `workshop/guest_door.hpp` `GuestDoor::row_index`, `GuestDoor::facts`,
`mount_observation`; `tests/test_workshop_guests.cpp` case `"door: in a file with two rows of one
name, each session holds the row its own credential admitted"`.
WHY — `agents/decisions/a-guests-hand-is-judged-by-its-row-and-its-host.md`

## WL-GUEST-03 — An act of a class is approved against its hand's row and the host

LAW — An owner asks Workshop to approve an act's classes -- `build`, `write`, `open` -- for its gesture's hand once, at the first beat or the one learning its subject, and carries it to later sends.

MEANS
- a guest's `build` needs its row's `build`; on a weaver's host its `write` and its guests file are refused;
- a classed ask may name no send: the act the pane does itself, judged by its classes alone;
- the Builder's `e` learns its path at its second beat, and asks there under the first's gesture.

DOES NOT MEAN
- that a class is a grant: it is judged at the ask, and it lets no guest say a shape it lacks;
- that the weaver's hand, or a participant no guest door admitted, is narrowed by any class.

PROVEN BY — `workshop/actor_scope.hpp` `judge`, `judge_all`, `GuestRowFacts`, `HostFact`;
`workshop/pane_operation.hpp` `PaneOperationRequested`; `workshop/weave_operation.cpp`
`approve_gesture`, `judge_classes`, `on(PaneOperationRequested)`; `workshop/weave.hpp`
`HostContext::guest_row`, `HostContext::host_fact`; `tests/test_workshop_guests.cpp` case `"action
classes: a guest builds only with build, writes nothing and opens no guests file on a weaver's
host, and no class narrows a participant no door admitted"`;
`tests/test_workshop_panes_builder.cpp` case `"a guest without build is refused b, B, f and
load-built in words on either host, and the Builder asks for nothing"`, case `"a guest with build
builds the catalog in force, and on a weaver's host is refused the role line's commit before any
row is written"`, case `"the weaver's f, and the role line's commit then build, stand with a key
typed between the two beats"`; `tests/test_workshop_panes_files.cpp` case `"on a weaver's host a
guest is refused u, a mark and a written recipe before any write, and still lists names"`, case `"on
a weaver's host a guest opening the guests file through Files is refused, and another file
opens"`.
WHY — `agents/decisions/a-guests-hand-is-judged-by-its-row-and-its-host.md`

## WL-GUEST-04 — A door past the dispatch asks about its sender

LAW — An owner sent a classed act a guest's grant names, directly past Workshop's dispatch, asks `ActorScopeRequested` about its sender before it acts, and acts only on `ActorScopeJudged`'s allowance.

MEANS
- the sender is judged as a gesture's hand is: its admitted row, the file's host;
- the toolbox save sent to the Inventory pane asks it; a refusal answers the sender in words;
- a send a pane relays as its own office is judged at its approval, as the classes its shape is.

PROVEN BY — `workshop/pane_operation.hpp` `ActorScopeRequested`, `ActorScopeJudged`;
`workshop/weave_operation.cpp` `on(ActorScopeRequested)`, `approve_gesture`;
`workshop/actor_scope.hpp` `classes_of_send`, `judge_send`; `inventory/inventory-pane/toolbox.hpp`
`Toolbox::begin_sent_save`; `tests/test_workshop_panes_guests.cpp` case `"on a weaver's host a
guest's toolbox save sent to the Inventory pane is refused through ActorScopeRequested, and no file
is written"`, case `"on a weaver's host a guest's Submit of a toolbox save in the Composer is
refused, and no file is written"`; `tests/test_workshop_guests.cpp` case `"action classes: a send
on a guest's behalf is the classes its shape is -- a toolbox save and a quit write, and an open
shows no path"`.
WHY — `agents/decisions/a-guests-hand-is-judged-by-its-row-and-its-host.md`

## WL-GUEST-05 — Each hand keeps its own gestures

LAW — The weaver's hand and each guest's keep their own latest gesture and their own record of each gesture sent a pane; a record is current while it is its hand's latest, so one hand never stales another's.

MEANS
- an approval is judged for the hand whose record it spends, never the latest hand on the desk;
- an ask naming no send spends its act's record while unspent, whatever its hand did since;
- the swallow, the app row asked and a menu's choosing act, and its number, are each their hand's own.

DOES NOT MEAN
- that the keys, the selection or a held press are per hand: one desk holds one of each.

PROVEN BY — `workshop/weave.hpp` `WorkshopWeave::Hand`, `hand_of`, `count_gesture`, `keep_act`,
`drop_kept`; `workshop/weave_operation.cpp` `hand_of`, `find_hand`, `latest_of`, `keep_act`,
`drop_kept`, `count_gesture`, `hand_of_act`, `approve_gesture`, `accept_carry`;
`workshop/weave_external.cpp` `forward_menu_input`; `tests/test_workshop_panes_builder.cpp` case
`"the weaver's b stands when a guest's injected key lands before the Builder's approval"`, case
`"the weaver's b stands when the weaver's own next key lands before the Builder's approval"`, case
`"a guest's choice in a menu the weaver opened is judged for the guest, whatever the guest pressed
since"`; `tests/test_workshop_panes_files.cpp` case `"the weaver's u stands when a guest's
files.open reaches Files before Workshop answers it"`; `tests/test_workshop_panes_button.cpp` case
`"a release forwarded to an open menu carries its own press's number when another hand acts between,
and chooses the row its press armed"`.
WHY — `agents/decisions/a-guests-hand-is-judged-by-its-row-and-its-host.md`

## WL-GUEST-06 — On a weaver's host the editor, the Terminal, typing and carrying answer only the weaver

LAW — On a weaver's host a guest's keys, text, presses, wheels and drops toward the editor office, the Terminal or the Hotkeys pane, its typed text anywhere and any carry it would begin are refused in words.

MEANS
- by office, whichever implementation holds it; a refused press moves no keys and no selection;
- no buffer, field or line holds text a guest typed, or an item it carried, for the weaver to commit as the weaver's own;
- on a development host a guest's hand reaches them as the weaver's does.

PROVEN BY — `workshop/weave_operation.cpp` `refused_toward`, `drop_carry`, `accept_carry`;
`workshop/actor_scope.hpp` `refuse_toward`, `refuse_text`, `refuse_carry`; `workshop/weave_external.cpp` `external_key`;
`workshop/weave_pointer.cpp` `on(TextEntered)`, `on(PointerButton)`, `on(PointerWheel)`;
`workshop/pane_seam_vocabulary.hpp` `kEditorRole`, `kTerminalRole`, `kHotkeysPane`;
`tests/test_workshop_panes_guests.cpp` case `"on a weaver's host a guest's keys, text, save and
presses toward the Editor are refused at the dispatch, and the file is unchanged"`, case `"on a
weaver's host a guest's keys and presses toward the Terminal are refused, and no line is sent and
no editor switch asked"`, case `"on a weaver's host a guest's text toward Info, the Composer,
Layouts' naming line and a Flow dialog field is refused, and the weaver's commit carries none of
it"`, case `"on a development host the same guest types into the Editor and saves, types a Terminal
line, and toggles titles"`, case `"on a weaver's host a guest's carried item rests in no pane: its carry
is refused in words before it begins, so nothing it drops on Info or the Composer lands, and the
weaver's own carry does"`, case `"on a development host the same guest's carried item lands on Info and
on the Composer as a draft"`; `tests/test_workshop_editor_transfers.cpp` case `"on a weaver's host a
guest's wheel and value drop toward the Editor are refused, and the Editor hears neither"`.
WHY — `agents/decisions/a-guests-hand-is-judged-by-its-row-and-its-host.md`

## WL-GUEST-07 — A guest learns its own row's powers and no other's

LAW — The door's inventory at version 2 says each row's powers, the file's host and version and the row's losses: every row's to this host's own participants, and to a guest only its own row's.

MEANS
- a guest still hears every connection as version 1 says it, with no other row's powers;
- no guest observes the version-2 inventory through the relay, whatever its row lists.

PROVEN BY — `workshop/guest_seam_vocabulary.hpp` `GuestConnection`, `GuestConnections`;
`workshop/guest_door.hpp` `on(GuestConnectionsRequested)`, `inventory_v2`, `guest_session`;
`workshop/guests.cpp` `observation_of`; `tests/test_workshop_guests.cpp` case `"door: version 2's
inventory tells a guest its own row's powers and losses alone, and this host every row's"`.
WHY — `agents/decisions/a-guests-hand-is-judged-by-its-row-and-its-host.md`

## WL-GUEST-08 — What a row's powers do not reach is said beside it

LAW — Each row's version and losses -- what its powers do not reach on this host -- are said at launch, on Attention and in Connections; a development host stands as a condition.

MEANS
- the losses are one function's: no `build`, and on a weaver's host every refusal above;
- conditions are keyed by the row's place in the file, never its name.

PROVEN BY — `workshop/guests.cpp` `losses_of`; `workshop/attention.hpp` `development_host`,
`guest_row_losses`; `external-host/connections-pane/pane.cpp` `version_text`; `tests/test_workshop_guests.cpp`
case `"guests file: a row's losses are what its powers do not reach on its file's host"`;
`tests/test_workshop_host.cpp` case `"a development host and each row's losses stand as
conditions on Attention"`; `tests/test_workshop_panes_attention.cpp` case `"the Connections pane
says a row's version and its losses beneath its connection"`.
WHY — `agents/decisions/a-guests-hand-is-judged-by-its-row-and-its-host.md`

## WL-GUEST-09 — Workshop's own writes and openings judge the hand that asked

LAW — Workshop's own `s`, `t` and quit are class `write`, and Edit Code class `open` of the source it found, judged for the hand that asked; a guest's quit by message is judged for its sender.

MEANS
- on a weaver's host a guest's setup save, titles toggle and quit write nothing, said in words.

PROVEN BY — `workshop/weave_session.cpp` `save_setup`; `workshop/weave_terminal.cpp` `command`;
`workshop/weave_run.cpp` `quit`, `on(WorkshopQuitRequested)`; `workshop/weave_code.cpp`
`edit_code`; `tests/test_workshop_panes_guests.cpp` case `"on a weaver's host a guest's s, t and
quit are refused, and nothing is written"`; `tests/test_workshop_panes_code.cpp` case `"Edit Code
asks class open for the guest's hand: a recipe whose source is the guests file is refused on a
weaver's host"`.
WHY — `agents/decisions/a-guests-hand-is-judged-by-its-row-and-its-host.md`

## WL-GUEST-10 — A session reads its own row, and nothing of another

LAW — The guest door answers an admitted session's `GuestRowDescribedRequested` with the row it recorded for that session -- its name, powers, observe list and the file's host -- and nothing of any other row.

MEANS
- every row may ask, whatever its powers: what a row grants is the asker's own to read;
- a session the door never admitted is refused in words.

PROVEN BY — `workshop/guest_seam_vocabulary.hpp` `GuestRowDescribedRequested`, `GuestRowDescribed`,
`GuestObserve`; `workshop/guest_door.hpp` `on(GuestRowDescribedRequested)`, `described`;
`workshop/guests.cpp` `grant_for`; `tests/test_workshop_guests.cpp` case `"door: a session asks its
own row and hears its name, powers, observe list and the file's host, and nothing of another
row"`.
WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## WL-GUEST-11 — A row observes and reads only what its powers read, and a widening is said

LAW — A row without `capture` is refused, in words, every observed publication carrying the desk's words or picture, and every row `ClipboardCopy`; what `capture` reads past its first reach is said.

MEANS
- the desk's words and picture are `SurfaceCanvas`, `SurfaceText`, `TranscriptShown`, `PaneSubjectShown`, `PaneInventory`, `KeymapShown` and `StandingConditions`, from any producer;
- `capture` reads the desk said whole, the inventory and the keymap; it is said at launch and on Attention;
- `DeskStamps`, the notice that the desk moved, says no word but follows them, so it needs `capture` too.

DOES NOT MEAN
- that `ClipboardCopy` is anyone's yet: it needs `clipboard`, a power no file of this version grants.

PROVEN BY — `workshop/guests.cpp` `observation_of`, `carries_the_desk`, `follows_the_desk`, `gains_of`, `grant_for`;
`workshop/attention.hpp` `guest_row_gains`; `tests/test_workshop_guests.cpp` case `"observation: a
row without capture is refused the desk's words and pictures, every row is refused ClipboardCopy,
and a capture row may observe StandingConditions"`, case `"door: in a file with two rows of one
name, the session admitted under the row without capture is refused a capture read and a capture
observation"`, case `"guests file: capture's widening is said as a gain"`;
`tests/test_workshop_host.cpp` case `"a development host and each row's losses stand as conditions
on Attention"`; `tests/test_workshop_guests.cpp` case `"observation: an observe row naming DeskStamps
without capture is refused in words, and a capture row listing it is told the notices, the newest
standing for those its window held back"`.
WHY — `agents/decisions/the-desk-is-said-whole-on-the-agents-host.md`

## Do not assume

- That a guest's hand is narrowed everywhere: every other control a key or a press reaches (a
  layout's removal, the desktop's launch and close, the clipboard) answers a guest as the weaver.
- That the sweep of writes is closed: a write a shipped act makes and no class names is a defect,
  and the View Builder's run record is the pane's own keeping, written whatever hand acts.
- That a guest's motion is refused: its drag still moves a press the weaver holds.
- That every classed act sent straight to its owner asks about its sender: `FlowEdit` and
  `ViewEdit` write, and no guest's grant names them, so they ask nothing.
