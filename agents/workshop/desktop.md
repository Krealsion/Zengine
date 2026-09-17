# Workshop law — the desktop

Register `WL-DESK`: the application's own default behaviour belongs to a participant, not to
the host. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md). How a
keystroke becomes an action at all is the keymap's law, in [`keyboard.md`](keyboard.md).

## WL-DESK-01 — The application's defaults are a weave, and the host keeps four things

LAW — One office, `zengine.desktop`, holds the application's default behaviour; the host keeps room, focus, realization and roots, and compiles none of what that office decides.

MEANS
- a Workshop whose desktop did not load has no launch bindings, no default Escape, no floor;
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
`workshop/weave_handlers.cpp` `on(KeyPressed)`;
`workshop/weave_external.cpp` `on(PaneEscapeUnspent)`;
`tests/test_workshop_panes_actions.cpp` case `"WL-DESK-02: the host asks the desktop for the
default row, and only an answer that echoes the ask puts the selection down"`;
`tests/test_workshop_panes_input.cpp` case `"a pane that takes keys keeps Escape until it says
the Escape was unspent"`, case `"an answer to an Escape that is over cannot borrow the next
Escape's identity"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-03 — A launch opens or focuses; it never toggles and never loads

LAW — A launch names a pane by its two durable keys; the host resolves it in `inventory_rows`, seats a closed one through the picker's own trial, focuses it either way, and answers.

MEANS
- a pane already on the desk is selected and given the keys, and nothing is closed;
- an unknown name, an unoffered pane and a screen with no room are three refusals.

DOES NOT MEAN
- that asking confers anything: an unoffered pane loads no artifact and mounts nothing;
- that a launch changes what is inspected — choosing a subject is a different sentence.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `PaneLaunchRequested`, `PaneLaunchAnswered`;
`workshop/weave.hpp` `launch_pane`, `on(PaneLaunchRequested)`; `workshop/weave_desktop.cpp`
`launch_pane`, `on(PaneLaunchRequested)`; `workshop/setup.hpp` `inventory_rows`;
`desktop-pane/vocabulary.hpp` `kActionTerminal`, `kActionPanes`, `kActionLaunch`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-04 — The inventory is said out loud, and it is not a second inventory

LAW — The host publishes `inventory_rows`' answer whenever it changes, with authored participation, provider availability and room-to-seat as three separate facts.

MEANS
- a pane can be authored-open and unavailable, which is the state a maker needs explained;
- the publication is compared before it is sent, so an unchanged reading is silence.

DOES NOT MEAN
- that a presenter may act on the list: a launch still goes through the host's own door.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `InventoryPane`, `PaneInventory`;
`workshop/weave.hpp` `publish_inventory`; `workshop/weave_desktop.cpp` `publish_inventory`;
`workshop/setup.hpp` `inventory_rows`.
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

## WL-DESK-06 — A refused declaration is told to its declarer

LAW — Workshop refusing a pane's or an application's action declaration says the same sentence on the band and to the office that authored it; the fact is Workshop's, the recovery the provider's.

MEANS
- the re-join a keymap file forces is a rejection like any other, and is told the same way;
- the refusals are collected before any is sent, so a re-declaration cannot mutate the walk.

DOES NOT MEAN
- that silence is a refusal: a declarer told nothing was accepted;
- that anything is mandated, or that delivery is owed — the band still carries the refusal.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `ActionsRefused`; `workshop/weave.hpp`
`say_actions_refused`; `workshop/weave_desktop.cpp` `say_actions_refused`;
`workshop/weave_seam.cpp` `declare_pane_actions`, `rejoin_pane_rows`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-07 — An application row is one declaration in two precedence classes

LAW — The declaring office's rows join the one keymap under the one collision law; each row declares whether it is answered above the modes or last, and there are exactly two such places.

MEANS
- an above-the-modes row is asked before the keys cross to a pane, and meets every host row;
- a default row meets none of them, because it is asked only where they claimed nothing;
- a pane stands in for an above-the-modes row by naming its id, never by matching a gesture.

DOES NOT MEAN
- that a class this build cannot name is guessed — it is refused, by number;
- that a declaration is merged: a later one replaces the rows whole, or changes nothing.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `AppActionRow`, `AppActions`,
`AppActionRequested`, `app_precedence`; `workshop/keymap.hpp` `AppRow`, `kMaxAppActionRows`,
`Keymap::app`, `Keymap::app_action_for`, `Keymap::app_row_active`, `Keymap::app_row_of_id`,
`join_app_rows`, `join_pane_rows`; `workshop/weave.hpp` `on(AppActions)`;
`workshop/weave_desktop.cpp` `on(AppActions)`;
`tests/test_workshop_panes_actions.cpp` case `"WL-KEY-16: an application row is joined, is
requested above the modes, and reaches its declarer as the resolved id"`, case `"WL-KEY-16: the
collision law is precedence-aware, and a pane may stand in by name"`, case `"WL-KEY-16: a pane's
row and an above-the-modes application row collide unless the pane declares it stands in, in
both arrival orders"`.
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

## Do not assume

- That the desktop can take a gesture from a pane — a pane stands in for an above-the-modes
  row by naming it, and then the application row is not requestable there (WL-DESK-07).
- That `Ctrl+t` is the host's again — the id is `desktop.terminal`, in the weave's namespace,
  and a maker who authored an override for the retired `workshop.terminal` must move it.
