# Workshop law — what the desktop's panes present

Register `WL-DESK`, the presenting half: what a presenter of the host's readings is told and
when, and how the launcher holds a weaver's place in a list that moves. The owner's own laws —
the defaults weave, precedence, launching, closing — are in
[`desktop.md`](desktop.md), and the ids are one series. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md).

## WL-DESK-09 — A presenter that arrives is answered the inventory as it is now

LAW — A presenter asking as an office, or a guest session holding `capture`, is answered the inventory reading of that moment, alone; an offer clears the record, so the next is said to all.

MEANS
- a desktop reloaded while nothing changes shows the list at once, not at an unrelated change;
- the answer reaches the incarnation that asked, so a successor asks for itself.

DOES NOT MEAN
- that the answer is a second inventory: it is `inventory_reading`, the publication's own value.

WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-10 — The launcher's cursor is a pane's identity, and its window keeps it in view

LAW — The launcher holds the row a weaver is on by its two durable keys, shows it through a window that follows it, and reserves its feedback rows before the list is laid out.

MEANS
- a row moving under the cursor moves the marker with it, and Return opens what is marked;
- a chosen pane that leaves is said; Return and `x` wait for a choice -- across a reload too;
- every cut in the list is counted on a row of its own.

DOES NOT MEAN
- that the launcher owns the inventory: it keeps the host's last reading and edits none of it.
- that both keys empty is a lost choice: it is a cursor never given a pane, which takes its row.

WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-14 — Both panes are usable by mouse, and a press is read against the rows it landed on

LAW — A composition is numbered by what its rows mean; a press in its area is read against the rows drawn there, acting only on their meaning; the wheel walks the cursor; a right press or `m` offers a menu.

MEANS
- the Manager: the mark shows or hides, the name chooses, a second marked-name press is Return;
- its menu: show/focus/hide, `manage <pane> >` (the host's menu on that pane), `inspect in Info`;
- the Hotkeys pane: a press chooses a binding and its menu edits it (WL-KEY-17); others hand back.

DOES NOT MEAN
- that a repaint moving no row renumbers (a subject swap does), or that display time is seen;
- that a press the window landed before the list moved is refused as moved: which drawing a window press names is a decision held open, its witness held as an expected failure.

WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-11 — The effective keymap is said out loud, and presenters print only it

LAW — The host publishes every binding in force, grouped by where it is answered, when it changes and to an arriving asker; the Hotkeys pane prints keys from it and from nothing else.

MEANS
- a moved row prints where it moved, a disabled one as having no key, `*` marking the weaver's;
- each binding says its address -- office, pane, id -- and what a press causes, and each pane its modes (WL-CAUSE-01);
- the key list is the desktop's Hotkeys pane, launched by `desktop.hotkeys`; the host paints none.

DOES NOT MEAN
- that a presenter rebinds anything: a key moves through the host's edit door or the file.

WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-17 — The Manual shows a pane's manual with the keys in force, and keeps it only while open

LAW — The desktop's Manual asks the pane a `?` or the manual key names for its whole manual and shows `About`, `Use` and each command beside its keys now; its words go when its pane leaves the desk.

MEANS
- a marker's place in the words is the build's data; its key is a row in force, else the weaver's file, else the page's default;
- keys are spelled as the Hotkeys view spells them; only the answer to its latest ask is shown;
- the manual key is above every mode: a pane keeps the keys it did not declare, so a chord.

DOES NOT MEAN
- that Workshop holds or relays a manual: the desktop asks the pane's office, and nothing keeps it.

WHY — `agents/decisions/a-panes-manual-is-shown-by-the-application.md`

## Proven by

### WL-DESK-09
`workshop/desktop_seam_vocabulary.hpp` PaneInventoryRequested
`workshop/weave.hpp` on(PaneInventoryRequested) inventory_published_
`workshop/weave_desktop.cpp` on(PaneInventoryRequested)
`workshop/weave_seam.cpp` on(PaneOffered)
`workshop/desktop-pane/desktop_common.hpp` from_workshop
`workshop/desktop-pane/pane.cpp` DesktopWeave
`tests/test_workshop_panes_actions.cpp` "a desktop reloaded in place is not left waiting: its new
image asks for the inventory and shows it, though nothing about the inventory changed" "an arriving
presenter is answered the inventory as it is now, to itself alone, and an offer makes the next
reading be said again"

### WL-DESK-10
`workshop/desktop-pane/vocabulary.hpp` DesktopState DesktopState::cursor_office
DesktopState::cursor_pane
`workshop/desktop-pane/launcher_view.hpp` find_cursor write_choice launch_cursor close_cursor
`component/held_choice.hpp` HeldChoice
`component/list_window.hpp` ListWindow cursor_window
`tests/test_workshop_panes_actions.cpp` "the launcher keeps the row it will open in view, and its
feedback on a row of its own" "the launcher's cursor is an identity: rows moving under it do not
retarget Return, and a row that left the list is said, not replaced" "a choice whose row left stays
unchosen across a desktop replacement and the publications after it: Return and x reach no
neighbour, and a row chosen then is obeyed"

### WL-DESK-14
`graphics/vocabulary.hpp` AreaButtonChanged AreaWheelTurned
`workshop/pane_drawing.hpp` rows_drawing window_cell WindowDrawing
`workshop/desktop-pane/desktop_common.hpp` PanePress take_notches
`workshop/desktop-pane/launcher_view.hpp` launcher_press publish_launcher offer_launcher_row
launcher_chose LauncherMeaning LauncherMeaning::ref launcher_ref second_button wheel
`workshop/desktop-pane/hotkeys_view.hpp` keys_press publish_keys offer_keys_row keys_chose
KeysMeaning KeysMeaning::ref keys_line_ref second_button wheel
`workshop/desktop-pane/pane.cpp` second_button wheel on_window_press
`component/row_map.hpp` RowMap RowMap::settle solid_columns
`component/columns.hpp` layout_columns table_line
`tests/test_workshop_panes_desktop.cpp` "WL-DESK-14: a press queued behind a change of the list is
refused, never resolved against the moved rows" "WL-DESK-14: a press on a row's mark shows or hides that pane; a press on its name only moves the
marker" "WL-DESK-14: a deliberate second press on the marked name, with the keys already here, opens
it -- or focuses and lifts it when it is open and covered" "WL-DESK-14: the wheel walks the marker
one row per notch, and a press names the picture it was aimed at -- a press queued behind a change
of the list is refused, never resolved against the moved rows" "WL-DESK-14: a right press on a row
offers its menu -- show, manage and inspect -- and `manage <pane> >` opens the host's own pane menu
on THAT pane; the menu key offers the marked row's" "WL-DESK-14: the Pane Manager renders within
every budget the screen grants -- down to a heading alone -- and never says an omission marker it
did not seat" "WL-KEY-17: the table has coherent columns, a visible cursor the wheel and the keys
walk, a press that chooses a row, and a heading press that is handed back; a small room keeps the
notice"

### WL-DESK-11
`workshop/desktop_seam_vocabulary.hpp` ShownBinding ShownModes KeymapShown KeymapRequested
`workshop/screen.hpp` keymap_shown
`workshop/screen_hotkeys.cpp` keymap_shown keyboard_context_name
`workshop/weave.hpp` publish_keymap on(KeymapRequested)
`workshop/weave_desktop.cpp` publish_keymap on(KeymapRequested)
`workshop/desktop-pane/pane.cpp` DesktopWeave
`workshop/desktop-pane/hotkeys_view.hpp` HotkeysView keymap_said say_keys
`workshop/desktop-pane/vocabulary.hpp` kHotkeysPane kActionHotkeys
`tests/test_workshop_panes_actions.cpp` "the band and the Hotkeys pane teach the application's keys
as they are in force: a moved row where it moved, a disabled one as having no key"
`tests/test_workshop_document.cpp` "the effective keymap lists every place a key is answered, and
marks the text box's keys as nobody's to move"
`tests/test_workshop_panes_canvas.cpp` "a weaver keys a described view's button by naming its id in
their keymap file: the key says the button's intent, the keymap says what it causes, and its part is
the action's"

### WL-DESK-17
`workshop/desktop-pane/manual_reader.hpp` keys_of with_keys manual_lines
`workshop/desktop-pane/vocabulary.hpp` kManualPane kActionManual
`workshop/desktop-pane/pane.cpp` DesktopWeave
`workshop/desktop-pane/manual_view.hpp` ManualView show_manual document_shown inventory_said
say_manual
`workshop/desktop_seam_vocabulary.hpp` AppActionRequested KeymapShown
`workshop/pane_document.hpp` PaneDocumentKey
`cmake/ZengineManual.cmake` zengine_manual_marks
`tests/test_workshop_panes_builder.cpp` "WL-DESK-17: the Builder's `?` shows About, Use and every
command it declares in every mode, each beside the key it answers to, the census's default where
nothing moved it" "WL-DESK-17: a key the weaver moved in a keymap file shows in place of the marked
default, beside the heading and in the words, for a command in force and one in another mode"
"WL-DESK-17: the manual key asks for the manual of the pane holding the keys; the Manual keeps no
words once closed, and a pane reloaded in place shows its new words at the next `?`"
