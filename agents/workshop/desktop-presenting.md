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
- a chosen pane that leaves is said; Return and `x` wait for a choice -- across a reload too;
- every cut in the list is counted on a row of its own.

DOES NOT MEAN
- that the launcher owns the inventory: it keeps the host's last reading and edits none of it.
- that both keys empty is a lost choice: it is a cursor never given a pane, which takes its row.

PROVEN BY — `desktop-pane/vocabulary.hpp` `DesktopState`, `DesktopState::cursor_office`,
`DesktopState::cursor_pane`; `desktop-pane/pane.cpp` `find_cursor`, `write_choice`,
`launch_cursor`, `close_cursor`; `component/held_choice.hpp` `HeldChoice`;
`component/list_window.hpp` `ListWindow`, `cursor_window`;
`tests/test_workshop_panes_actions.cpp` case `"the launcher keeps the row it will open in view,
and its feedback on a row of its own"`, case `"the launcher's cursor is an identity: rows moving
under it do not retarget Return, and a row that left the list is said, not replaced"`, case `"a
choice whose row left stays unchosen across a desktop replacement and the publications after it:
Return and x reach no neighbour, and a row chosen then is obeyed"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-14 — Both panes are usable by mouse, and a press names the picture it was aimed at

LAW — A composition is numbered by what its rows mean; a press names the picture the medium held when read, acting only on the current one; the wheel walks the cursor; a right press or `m` offers a menu.

MEANS
- the Manager: the mark shows or hides, the name chooses, a second marked-name press is Return;
- its menu: open/focus/close, `manage...` (the host's menu on that pane), `inspect in Info`;
- the Hotkeys pane: a press chooses a binding and its menu edits it (WL-KEY-17); others hand back.

DOES NOT MEAN
- that an older picture's press acts, or a repaint moving no row renumbers; a subject swap does;
- that the medium's own latency after it handled a canvas is seen: P-WORK-25's residue, open.

PROVEN BY — `workshop/pane_vocabulary.hpp` `v3::PaneContent`, `v3::PanePressed`;
`workshop/vocabulary.hpp` `PictureFence`; `workshop/panel.hpp` `ExternalPane::picture`,
`ExternalPane::stamp`, `ExternalPane::forget_pictures`, `PictureStamp`; `workshop/weave_seam.cpp`
`admit_content`, `on(v3::PaneContent)`, `fence_pictures`, `on(PictureFence)`;
`workshop/weave_external.cpp` `external_press`; `desktop-pane/pane.cpp`
`launcher_press`, `keys_press`, `offer_launcher_row`, `offer_keys_row`, `launcher_chose`,
`keys_chose`, `LauncherMeaning`, `LauncherMeaning::ref`, `KeysMeaning`, `KeysMeaning::ref`,
`launcher_ref`, `keys_line_ref`, `kMovedSentence`, `take_notches`;
`component/row_map.hpp` `RowMap`, `solid_columns`; `component/columns.hpp` `layout_columns`,
`table_line`; `tests/test_workshop_panes_desktop.cpp` case `"WL-DESK-14: content queued ahead
of a raw press cannot retarget the row the hand aimed at -- the press is stamped with the picture
the medium had, and refused as moved"`, case `"WL-DESK-14: a same-length inventory
swap changes the picture, so a press stamped with the old number opens nothing -- the meaning
carries the subject, not just the slot"`, case `"WL-DESK-14: a press on a row's mark
shows or hides that pane; a press on its name only moves the marker"`, case `"WL-DESK-14: a
deliberate second press on the marked name, with the keys already here, opens it -- or focuses
and lifts it when it is open and covered"`, case `"WL-DESK-14: the wheel walks the marker one
row per notch, and a press names the picture it was aimed at -- a press queued behind a change
of the list is refused, never resolved against the moved rows"`, case `"WL-DESK-14: a right
press on a row offers its menu -- open, manage and inspect -- and `manage...` opens the host's
own pane menu on THAT pane; the menu key offers the marked row's"`, case `"WL-DESK-14: the Pane
Manager renders within every budget the screen grants -- down to a heading alone -- and never
says an omission marker it did not seat"`, case `"WL-KEY-17: the table has coherent columns, a
visible cursor the wheel and the keys walk, a press that chooses a row, and a heading press that
is handed back; a small room keeps the notice"`.
WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-DESK-11 — The effective keymap is said out loud, and presenters print only it

LAW — The host publishes every binding in force, grouped by where it is answered, when it changes and to an arriving asker; the floor and the Hotkeys pane print keys from it and from nothing else.

MEANS
- a moved row is printed where it moved, a disabled one as having no key, `*` marking the maker's;
- the key list is the desktop's Hotkeys pane, launched by `desktop.hotkeys`; the host paints none.

DOES NOT MEAN
- that a presenter rebinds anything: a key moves through the host's edit door or the file.

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

