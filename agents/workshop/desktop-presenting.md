# Workshop law — what the desktop's panes present

Register `WL-DESK`, the presenting half: what a presenter of the host's readings is told and
when, and how the launcher holds a maker's place in a list that moves. The owner's own laws —
the defaults weave, precedence, launching, closing, the floor — are in
[`desktop.md`](desktop.md), and the ids are one series. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md).

## WL-DESK-09 — A presenter that arrives is answered the inventory as it is now

LAW — A presenter that asks as an office is answered the inventory reading of that moment, alone; an offer clears the publication's record, so the next reading is said to everyone.

MEANS
- a desktop reloaded while nothing changes shows the list at once, not at an unrelated change;
- the answer reaches the incarnation that asked, so a successor asks for itself.

DOES NOT MEAN
- that the answer is a second inventory: it is `inventory_reading`, the publication's own value.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `PaneInventoryRequested`;
`workshop/weave.hpp` `on(PaneInventoryRequested)`, `inventory_published_`;
`workshop/weave_desktop.cpp` `on(PaneInventoryRequested)`; `workshop/weave_seam.cpp`
`on(PaneOffered)`; `desktop-pane/pane.cpp` `DesktopWeave`, `from_workshop`;
`tests/test_workshop_panes_actions.cpp` case `"a desktop reloaded in place is not left waiting:
its new image asks for the inventory and shows it, though nothing about the inventory changed"`,
case `"an arriving presenter is answered the inventory as it is now, to itself alone, and an
offer makes the next reading be said again"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-10 — The launcher's cursor is a pane's identity, and its window keeps it in view

LAW — The launcher holds the row a maker is on by its two durable keys, shows it through a window that follows it, and reserves its feedback rows before the list is laid out.

MEANS
- a row moving under the cursor moves the marker with it, and Return opens what is marked;
- a marked pane that leaves the list is said, and Return waits for a choice, not a neighbour;
- every cut in the list is counted on a row of its own.

DOES NOT MEAN
- that the launcher owns the inventory: it keeps the host's last reading and edits none of it.

PROVEN BY — `desktop-pane/vocabulary.hpp` `DesktopState`, `DesktopState::cursor_office`,
`DesktopState::cursor_pane`; `desktop-pane/pane.cpp` `window_for`, `ListWindow`,
`find_cursor`, `launch_cursor`; `tests/test_workshop_panes_actions.cpp` case `"the launcher
keeps the row it will open in view, and its feedback on a row of its own"`, case `"the
launcher's cursor is an identity: rows moving under it do not retarget Return, and a row that
left the list is said, not replaced"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-11 — The effective keymap is said out loud, and presenters print only it

LAW — The host publishes every binding in force, grouped by where it is answered, when it changes and to an arriving asker; the floor and the Hotkeys pane print keys from it and from nothing else.

MEANS
- a moved row is printed where it moved, a disabled one as having no key, `*` marking the maker's;
- the key list is the desktop's Hotkeys pane, launched by `desktop.hotkeys`; the host paints none.

DOES NOT MEAN
- that a presenter rebinds anything: a key moves in the keymap file, which is read at launch.

PROVEN BY — `workshop/desktop_seam_vocabulary.hpp` `ShownBinding`, `KeymapShown`,
`KeymapRequested`; `workshop/screen.hpp` `keymap_shown`; `workshop/screen_hotkeys.cpp`
`keymap_shown`, `keyboard_context_name`; `workshop/weave.hpp` `publish_keymap`,
`on(KeymapRequested)`; `workshop/weave_desktop.cpp` `publish_keymap`, `on(KeymapRequested)`;
`desktop-pane/pane.cpp` `DesktopWeave`; `desktop-pane/vocabulary.hpp` `kHotkeysPane`,
`kActionHotkeys`; `tests/test_workshop_panes_actions.cpp` case `"the floor and the Hotkeys pane
teach the application's keys as they are in force: a moved row where it moved, a disabled one as
having no key"`; `tests/test_workshop_document.cpp` case `"KEY-0: the effective keymap lists
every place a key is answered, and marks the text box's keys as nobody's to move"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

