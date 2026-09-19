# Workshop law — the desktop

Register `WL-DESK`: the application's own default behaviour belongs to a participant, not to
the host. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md). How a
keystroke becomes an action at all is the keymap's law, in [`keyboard.md`](keyboard.md); what
the desktop's panes are told, and how the launcher holds its place, is
[`desktop-presenting.md`](desktop-presenting.md).

## WL-DESK-01 — The application's defaults are a weave, and the host keeps four things

LAW — One office, `zengine.desktop`, holds the application's default behaviour; the host keeps room, focus, realization and roots, and compiles none of what that office decides.

MEANS
- a Workshop whose desktop did not load has no launch bindings, key list, default Escape or floor;
- the weave is built, loaded, edited and reloaded by the means every pane weave is.

DOES NOT MEAN
- that the office is privileged: it opens nothing the inventory lacks and loads no artifact;
- that holding the office is speaking for it — a personal declaration registers nothing.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `kDesktopRole`;
`desktop-pane/vocabulary.hpp` `kDesktopRole`, `kLauncherPane`, `kLauncherName`,
`kLauncherSummary`, `kDesktopStem`, `DesktopState`; `desktop-pane/pane.cpp` `DesktopWeave`;
`tests/test_workshop_load.cpp` case `"the shipped default plan is a legal plan, and it is the
terminal arrangement"`; `tests/test_workshop_files.cpp` case `"the development catalog this tree
generated names every shipped pane by its own target, build directory and weave source, each one
the Editor opens, and nothing else"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-02 — A default row is asked where the host's own last word used to be said

LAW — A default-class row is requested exactly where the resolved context claimed the gesture for nothing and no pane took it; the host writes nothing until an answer echoes that ask.

MEANS
- the contexts are `default_row_context`'s, plus a pane whose holder had no door for the key;
- an answer echoing zero, another number, or arriving behind a later gesture, moves nothing;
- a pane's `PaneEscapeUnspent` is judged by the host and then asks the same row.

DOES NOT MEAN
- that a row is asked because a pane stayed quiet — silence settles nothing (WL-ARR-15);
- that the selection is the desktop's: the host performs it, the desktop decides when.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `DeselectRequested`;
`workshop/screen_arrange.cpp` `default_row_context`; `workshop/screen.hpp`
`default_row_context`; `workshop/weave.hpp` `request_app_action`, `on(DeselectRequested)`;
`workshop/weave_desktop.cpp` `request_app_action`, `on(DeselectRequested)`;
`workshop/weave_handlers.cpp` `on(KeyPressed)`; `workshop/weave_external.cpp`
`on(PaneEscapeUnspent)`; `tests/test_workshop_panes_actions.cpp` case `"WL-DESK-02: the host
asks the desktop for the default row, and only an answer that echoes the ask puts the selection
down"`; `tests/test_workshop_panes_input.cpp` case `"a pane that takes keys keeps Escape until
it says the Escape was unspent"`, case `"an answer to an Escape that is over cannot borrow the
next Escape's identity"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-03 — A launch opens or focuses; it never toggles and never loads

LAW — A launch names a pane by its durable keys; the host resolves it in `inventory_rows`, refuses one nobody holds now, seats a closed one through the trial seat, focuses it either way, and answers.

MEANS
- a pane already on the desk is selected and given the keys, and nothing is closed;
- an unknown name, an unoffered pane, a departed provider and no room are four refusals;
- a provider the run is still loading is refused as not here yet, never as something to build.

DOES NOT MEAN
- that asking confers anything: an unoffered pane loads no artifact and mounts nothing;
- that a launch changes what is inspected — choosing a subject is a different sentence.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `PaneLaunchRequested`,
`PaneLaunchAnswered`; `workshop/weave.hpp` `launch_pane`, `on(PaneLaunchRequested)`,
`provider_present`; `workshop/weave_desktop.cpp` `launch_pane`, `on(PaneLaunchRequested)`,
`provider_present`; `workshop/setup.hpp` `inventory_rows`; `desktop-pane/vocabulary.hpp`
`kActionTerminal`, `kActionPanes`, `kActionLaunch`; `tests/test_workshop_panes_actions.cpp` case
`"a pane whose provider left is unavailable in the launcher and refused at launch, while its
identity and the desk row naming it stay"`, case `"a pane the run is still loading is pending,
not unavailable: the launcher marks it `[load]`, the floor names nothing to build, and a launch
says it is not here yet"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-04 — The inventory is said out loud, and it is not a second inventory

LAW — The host publishes `inventory_rows`' answer whenever it changes, with authored participation, provider presence now, a provider still to come and room-to-seat as separate facts.

MEANS
- a pane can be authored-open and unavailable, or still owed by the run: two states, one verdict;
- presence is asked of the office's current holder at every reading, never read off a past offer;
- the publication is compared before it is sent, so an unchanged reading is silence.

DOES NOT MEAN
- that a presenter may act on the list: a launch still goes through the host's own door;
- that presence is health: a holder that accepts a room may never answer it.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `InventoryPane`, `PaneInventory`;
`workshop/weave.hpp` `publish_inventory`, `inventory_reading`, `provider_present`,
`HostContext::office_pending`; `workshop/weave_desktop.cpp` `publish_inventory`,
`inventory_reading`, `provider_present`; `workshop/load_execute.hpp` `office_pending`;
`workshop/setup.hpp` `inventory_rows`; `tests/test_workshop_panes_actions.cpp` case `"a pane
whose provider left is unavailable in the launcher and refused at launch, while its identity and
the desk row naming it stay"`, case `"a pane the run is still loading is pending, not
unavailable: the launcher marks it `[load]`, the floor names nothing to build, and a launch says
it is not here yet"`; `tests/test_workshop_load.cpp` case `"an office is still to come while a
plan row loading it has not settled, and is owed nothing once every such row resolved or was
stepped over"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-05 — The room's floor is the desktop's words and the host's wall

LAW — The floor carries the rows the host paints in the workspace behind every pane: retained whole, refused whole past its bound, composed by nobody here, empty until the desktop speaks.

MEANS
- the floor takes no input and is granted no room: a press there means what it always meant;
- an empty floor is the honest picture of a Workshop whose desktop never loaded.

DOES NOT MEAN
- that the floor is a pane: it is not seated, ordered, covered, arranged or removable.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `DesktopFace`, `kMaxBackdropRows`;
`workshop/screen.hpp` `Session::backdrop`; `workshop/weave.hpp` `on(DesktopFace)`;
`workshop/weave_desktop.cpp` `on(DesktopFace)`; `workshop/screen_compose.cpp` `paint`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-06 — A declaration is answered with its verdict, and withdrawn by its number

LAW — Workshop answers each declaration it judges with its verdict, numbers an accepted one, and names that number when it later leaves the keymap; the fact is Workshop's, the recovery the provider's.

MEANS
- the verdict is Loom's answer: it echoes the declaration's correlation, to its incarnation only;
- a keymap file that displaces a pane's or the application's declaration withdraws it whole;
- only a holder whose office accepts the shape is told; the band says a refusal either way.

DOES NOT MEAN
- that silence is a verdict: an answer can be refused at the gate or dropped with its asker;
- that anything is mandated: nothing is retried, and no draft, mode or binding is chosen for it.

PROVEN BY — `workshop/pane_vocabulary.hpp` `ActionsJudged`, `ActionsWithdrawn`;
`workshop/weave.hpp` `answer_declaration`, `say_withdrawn`, `rejoin_app_rows`;
`workshop/weave_desktop.cpp` `answer_declaration`, `say_withdrawn`, `rejoin_app_rows`;
`workshop/weave_seam.cpp` `declare_pane_actions`, `rejoin_pane_rows`; `workshop/panel.hpp`
`RuntimePane::declaration`; `desktop-pane/pane.cpp` `DesktopWeave`;
`tests/test_workshop_panes_actions.cpp` case `"a verdict answers the declaration it judges: a
refused attempt is named by its own number after a later one was accepted, and an accepted one
is given Workshop's"`, case `"a declaration the keymap file displaces is withdrawn by the number
its verdict gave it, and a pane that reads verdicts learns both"`, case `"a withdrawal naming a
predecessor's declaration does not reach the successor's rows: the reloaded desktop shows only
its own verdict"`, case `"the shipped desktop shows Workshop's verdict on its own declaration,
and only for the attempt it is waiting on"`, case `"the keymap file wins: a pane whose rows its
bindings collide with is refused in words, in both orders"`.
WHY — `agents/decisions/a-verdict-answers-its-declaration.md`

## WL-DESK-07 — An application row is one declaration in two precedence classes

LAW — The declaring office's rows join the one keymap under the one collision law; each row declares whether it is answered above the modes or last, and there are exactly two such places.

MEANS
- an above-the-modes row is asked before any pane, meets every host row, and takes no text key;
- a default row meets none of them, because it is asked only where they claimed nothing;
- a pane stands in for an above-the-modes row by naming its id, never by matching a gesture.

DOES NOT MEAN
- that a class this build cannot name is guessed — it is refused, by number;
- that a declaration is merged: a later one replaces the rows whole, or changes nothing.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `AppActionRow`, `AppActions`,
`AppActionRequested`, `app_precedence`; `workshop/keymap.hpp` `AppRow`, `kMaxAppActionRows`,
`Keymap::app`, `Keymap::app_action_for`, `Keymap::app_row_active`, `Keymap::app_row_of_id`,
`join_app_rows`, `join_pane_rows`; `workshop/weave.hpp` `on(AppActions)`;
`workshop/weave_desktop.cpp` `on(AppActions)`; `tests/test_workshop_panes_actions.cpp` case
`"WL-KEY-16: an application row is joined, is requested above the modes, and reaches its
declarer as the resolved id"`, case `"WL-KEY-16: the collision law is precedence-aware, and a
pane may stand in by name"`, case `"an application row answered above every mode cannot take a
bare printable or a chord the text box owns, whoever wrote it"`, case `"WL-KEY-16: a pane's row
and an above-the-modes application row collide unless the pane declares it stands in, in both
arrival orders"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-08 — A maker may move an application default, or disable it

LAW — An authored row moves an application row by id, and the gesture `none` leaves it declared, listed and nameable with no key at all; nothing compiled in answers for a disabled row.

MEANS
- the file's rows are preserved unjudged until their declarer arrives, then applied at the join;
- a disabled row collides with nothing, because it answers to no key.

DOES NOT MEAN
- that disabling deletes the row, or that the host keeps a copy of the behaviour behind it.

PROVEN BY — `workshop/keymap.hpp` `parse_gesture`, `kNoGesture`, `is_bound`;
`tests/test_workshop_panes_actions.cpp` case `"WL-KEY-16: a maker's authored row moves an
application row, and `none` disables it"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-12 — A close takes a pane off the desk and unloads nothing

LAW — A close names a pane by its durable keys; the host takes its row off the live desk through the setup's door and reseats, refusing one that is not there; its provider and all it holds stay.

MEANS
- a launch finds it as it was: an Editor's unsaved source, a Terminal's history;
- a close never opens anything, and the launcher's Return never closes anything (`x` does).

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `PaneCloseRequested`, `PaneCloseAnswered`;
`workshop/weave.hpp` `close_pane`, `on(PaneCloseRequested)`; `workshop/weave_desktop.cpp`
`close_pane`, `on(PaneCloseRequested)`; `desktop-pane/vocabulary.hpp` `kActionClose`;
`tests/test_workshop_panes_actions.cpp` case `"WL-DESK-12: a close takes a pane off the desk and
leaves its provider holding; a close of a pane that is not there is refused and opens nothing"`,
case `"the shipped desktop's x closes the row its marker holds, and Return opens it again: the
mark says which, and the provider never left"`; `tests/test_workshop_panes_editor.cpp` case
`"the Pane Manager's close takes the Editor off the desk and unloads nothing: a launch finds its
unsaved source exactly as it was"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-13 — A toggle is judged by the host against the desk as it is

LAW — A toggle names a pane by its keys; the host judges it against the desk when handled (on it: close; off it: open and focus) and answers which; a presenter's own reading never judges.

MEANS
- the shipped desktop's `desktop.panes` (`Ctrl+p`) is this toggle on its own Pane Manager;
- two toggles queued before any reading reaches the presenter end where they began.

DOES NOT MEAN
- that a launch toggles: the terminal and hotkeys rows still open or focus (WL-DESK-03).

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `PaneToggleRequested`,
`PaneToggleAnswered`; `workshop/weave.hpp` `on(PaneToggleRequested)`;
`workshop/weave_desktop.cpp` `on(PaneToggleRequested)`, `launch_pane`, `close_pane`;
`desktop-pane/vocabulary.hpp` `kActionPanes`; `tests/test_workshop_panes_desktop.cpp` case
`"WL-DESK-13: Ctrl+P is a strict visibility toggle judged by the host -- open becomes closed,
closed becomes open and focused, whatever the desktop last heard"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## Do not assume

- That the desktop can take a gesture from a pane — a pane stands in for an above-the-modes
  row by naming it, and then the application row is not requestable there (WL-DESK-07).
- That `Ctrl+t` is the host's again — the id is `desktop.terminal`, in the weave's namespace;
  an override authored for the retired `workshop.terminal` is read as it, and the load says so.
