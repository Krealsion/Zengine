# Workshop law — the contextual surface

Register `WL-CTX`: what can I do with this? One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-CTX-01 — Pointing names a subject for one request; selection is a state a maker entered

LAW — Opening captures a subject — a `PaneRef`, a layout position, or nothing; never a rectangle, row or handle — and changes no selection, candidate or focus; spend re-asks the owner.

MEANS
- right press: the pointed subject; `workshop.context` (`a`): the room, all command mode names;
- a ref outside the setup gets one truthful absence sentence, not a geometry refusal;
- ⭐ an object id was a subject, and its load the aliasing door, until the object canvas retired.

PROVEN BY — `workshop/context.hpp` `ContextMenu`, `context_subject`; `workshop/keymap.hpp`
`workshop.context`; `workshop/weave_pointer.cpp` `spend_context_choice`, `open_context_at`,
`open_context_ambient`; `workshop/panel.hpp` `Panels::selected`;
`tests/test_workshop_panels.cpp` case `"CTX-0: a right press captures a subject and selects
nothing"`, case `"CTX-0: a captured pane that left the setup is refused truthfully"`, subcase
`"the keyboard door opens on what command mode can name: the room"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-02 — Arrange is the one exception

LAW — Arrange binds the scope and selects the subject, and only after its explicit target passes admission (`enter_arrange_pane` → the target-taking `arrange_geometry_ready`).

DOES NOT MEAN
- that any other contextual action selects, binds or focuses — Arrange is the one exception.

PROVEN BY — `workshop/weave_arrange.cpp` `enter_arrange_pane`, `arrange_geometry_ready`;
`workshop/weave_pointer.cpp` `spend_context_choice`; `tests/test_workshop_panels.cpp` case
`"CTX-0/ARR-0: contextual Arrange admission precedes binding"`; `tests/test_workshop_screen.cpp`
case `"WUX-7: contextual Arrange lifts the pane it addressed, not the one in front"`, case
`"WUX-7: every pane a maker can point at can be arranged, and the refusals are blind"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-03 — The popup is local and its bounds are derived

LAW — The popup remembers only the press's canvas cell; its rectangle is re-derived at every paint and press from the current level's rows, through the one popup measurer, capped at a maximum width.

MEANS
- the keyboard entrance is `anchored == false` and opens at the overlay stack's corner;
- a group re-derives at the same anchor; a level taller than the room is cut, and says so;
- `popup_bounds_at`: one measurer, one chrome, one clamp in the overlay band.

PROVEN BY — `workshop/context.hpp` `ContextMenu`, `ContextMenu::anchored`,
`ContextMenu::anchor_x`; `workshop/screen_attention.cpp` `context_bounds`, `context_entry_text`;
`workshop/screen_pane_state.cpp` `popup_bounds_at`; `workshop/screen.hpp` `kContextMaxCols`,
`chrome_outer_of`; `surface/region.hpp` `region_cells_for`; `tests/test_workshop_screen.cpp`
case `"ARR-0: the popup opens at the press's own cell, and its extent is its content"`, case
`"ARR-0: the popup shifts to stay usable inside the room, at every boundary"`, case `"ARR-0: the
keyboard entrance has no pointer and invents none"`, case `"ARR-0: entering a group stays at the
anchor, and the popup resizes to it"`.
WHY — `agents/decisions/content-sized-popups.md`

## WL-CTX-04 — The first row is an action

LAW — There is no title row and no hint row: painted row i is population row i, and the width is the widest action row, so the popup shrinks to its content.

PROVEN BY — `workshop/screen_attention.cpp` `context_press_at`, `paint_context`,
`context_row_text`, `context_entry_text`; `tests/test_workshop_screen.cpp` case `"WUX-5: the
contextual surface is its actions, and its width is theirs"`, case `"CTX-0: the contextual
surface is painted where it is hit"`.
WHY — `agents/decisions/content-sized-popups.md`

## WL-CTX-05 — `kContextCatalog` declares, and owns no power

LAW — The catalog declares rows — an action id, its subjects, a group — over the action catalog's ids, so a stale reference is a compile error; one population is what every consumer of the menu spends.

MEANS
- a pane's: `arrange`, `Order >`, `Reset >`, `edit code`, `remove`;
- a tab's: `layout.rename`, `layout.duplicate`, `Order >`, `layout.remove`; the room's: no target;
- groups are their names, and an empty group cannot exist.

PROVEN BY — `workshop/context.hpp` `kContextCatalog`, `context_actions_resolve`,
`context_population`, `kOnPane`, `kOnLayout`, `kOnRoot`, `context_same_id`;
`workshop/keymap.hpp` `kActionCatalog`; `tests/test_workshop_panels.cpp` case `"CTX-0: the
declared populations are the researched ones, keyed by id"`; `tests/test_workshop_document.cpp`
case `"CTX-0: the shipped catalog stays admissible with the new rows"`;
`tests/test_workshop_screen.cpp` case `"CTX-0: an open group paints its own rows and its own way
out"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-06 — A row may teach its shortcut, and only a truthful one

LAW — A row shows its effective gesture exactly when its action owns a binding active in the context the maker returns to; a row whose action is unbound never annotates.

MEANS
- a layout row only when the captured tab is the active one, else row and key act on two subjects;
- a mode beneath that swallows bare keys (a pane holding them) suppresses a command row's key.

PROVEN BY — `workshop/screen_attention.cpp` `context_annotation`, `context_row_text`;
`workshop/screen_arrange.cpp` `keyboard_context_beneath_menu`; `workshop/keymap.hpp`
`active_in`, `is_bound`; `tests/test_workshop_screen.cpp` case `"ARR-0: shortcut annotations
teach only truthful surrounding bindings"`; `tests/test_workshop_panels.cpp` case
`"WUX-11/SC-2+SC-5: a tab's context menu acts on THAT tab"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-07 — Spending is one seam per subject kind, and paint is not policy

LAW — One seam per subject kind — `spend_pane_action`, the position-taking layout doors, the room's zero-target owners — and the owner refuses at spend; paint is not policy.

MEANS
- no owner predicate runs on the paint path: the menu renders an identity, not an existence;
- ⭐ an object's seam, `delete_object_at`, retired with the object canvas and its draft guard.

PROVEN BY — `workshop/weave_arrange.cpp` `spend_pane_action`; `workshop/weave_session.cpp`
`open_layout_rename`, `duplicate_layout`, `shift_layout`, `drop_layout`;
`workshop/weave_pointer.cpp` `spend_context_choice`; `tests/test_workshop_panels.cpp` case
`"CTX-0: a contextual action acts on the pointed pane, not the selection"`, case `"CTX-0: a
contextual remove removes the pointed pane"`, case `"WUX-11/SC-4: Move Left and Move Right
reorder from the tab that was pointed at"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-08 — The surface is a mode with first refusal, and a pass-back is the host's fallback

LAW — `KeyContext::kContext` tops the modes beneath it: inside, navigate or choose (`context_press_at`); outside, dismissal, consumed whole; a pass-back opens the host's rows for that pane, once, at its cell.

MEANS
- a further right press re-targets, offering a pane under it its press first;
- a pass-back is a continuation (WL-PRESS-06): stale, zero, spent or foreign moves nothing.

DOES NOT MEAN
- that a pane's rows reach this surface unasked: only by its request (WL-CTX-09);
- that a doorless body opens the host's menu: it is empty; the chrome and the Manager reach it.

PROVEN BY — `workshop/keymap.hpp` `KeyContext::kContext`; `workshop/screen_arrange.cpp`
`keyboard_context`; `workshop/screen_attention.cpp` `context_press_at`; `workshop/screen.hpp`
`ContextPressAt`; `workshop/weave_pointer.cpp` `spend_context_choice`, `choose_context_row`,
`context_press`; `workshop/weave_external.cpp` `on(PanePassRequested)`; `workshop/pane_menu.hpp`
`pass_back`; `tests/test_workshop_panels.cpp` case `"CTX-0: input spent on the open surface does
not leak through it"`, case `"CTX-0: navigation backtracks cleanly and every way out closes"`;
`tests/test_workshop_panes_window.cpp` case `"CTX-0: a right press over a provider's pane
crosses the seam not at all"`, case `"CTX-0: input spent on the open surface reaches no
provider"`; `tests/test_workshop_panes_button.cpp` case `"WL-CTX-08: a pass-back after a clean
click opens the host's menu for that pane, once"`, case `"WL-CTX-08: a pass-back on the press's
own turn opens the menu, and the release still reaches the pane under it"`, case `"WL-CTX-08: a
stale correlation, a zero one, and a pass-back from an office that did not offer the pane all
move nothing"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-09 — A pane's menu is judged where it opens and handed to the presenter

LAW — A pane's menu request is judged where it would open and granted to the presenter's office; it moves no keys or selection, and the host answers only asks it refused or no presenter can.

MEANS
- a press continuation on a pass-back's terms; a key's while that keystroke is the latest act;
- one menu at a time: a newer one, a right press, the host's menu, the pane leaving withdraw it;
- the host keeps custody and place -- forwards acts, draws lines in the room -- and no row.

DOES NOT MEAN
- that the host performs a pane's operation, or that a row grants one: the requester acts;
- that anything is restored after: a choice may continue to `manage...` or the keyboard, once.

PROVEN BY — `workshop/pane_vocabulary.hpp` `PaneMenuRow`, `PaneMenuRequested`,
`PaneMenuAnswered`, `PaneManageRequested`, `PaneKeyboardRequested`, `kPresenterRole`;
`workshop/presenter_vocabulary.hpp` `MenuGranted`, `MenuShown`, `MenuInput`, `MenuClosed`,
`MenuWithdrawn`; `workshop/context.hpp` `PresentedMenu`; `workshop/weave.hpp` `grant_menu`,
`withdraw_menu`, `end_menu_unanswered`, `cell_of_body_place`, `ChoiceAnswered`, `action_sent_`;
`workshop/weave_external.cpp` `on(PaneMenuRequested)`, `grant_menu`, `withdraw_menu`,
`end_menu_unanswered`, `about_open_menu`, `forward_menu_input`, `menu_key`, `menu_button`,
`on(MenuShown)`, `on(MenuClosed)`, `on(PresenterReady)`, `on(PaneManageRequested)`,
`on(PaneKeyboardRequested)`, `cell_of_body_place`; `workshop/weave_pointer.cpp` `context_key`;
`workshop/screen.hpp` `presented_room`, `presented_bounds`, `paint_presented`, `PresentedPressAt`,
`presented_press_at`; `workshop/screen_attention.cpp` `presented_room`, `presented_bounds`,
`paint_presented`, `presented_press_at`; `tests/test_workshop_panes_desktop.cpp` case
`"WL-CTX-09: a printable menu shortcut and the text its own key produced are one gesture, so the
menu opens"`; `tests/test_workshop_panes_button.cpp` case `"WL-CTX-09: a menu requested on the
press's own turn is granted to the presenter and opens beside the press with the pane's rows,
moves no keys and no selection, and Return returns the first row -- answered by the presenter"`,
case `"WL-CTX-09: the keyboard works the menu -- Down then Return chooses the second row; Escape
answers it unchosen; the release under it still reaches the pane"`, case `"WL-CTX-09: an outside
press dismisses the menu, is spent on dismissing, and reaches nothing beneath it"`, case
`"WL-CTX-09: a press on a presented row chooses it"`, case `"WL-CTX-09: a late request is refused
where the menu opens -- a newer primary press elsewhere keeps the keys it took; the review's
second integration finding"`, case `"WL-CTX-09: an offer echoing no gesture and one from an office
that never offered the pane are refused or dropped by the host; an empty offer is the presenter's
to refuse, and it spends the gesture"`, case `"WL-CTX-09: a newer menu replaces an open one, which
its presenter answers unchosen in the host's words; one menu at a time"`, case `"WL-CTX-09: a pane
that leaves the desk while its menu is open has it withdrawn, answered unchosen; a pane that
consumes the press opens nothing"`, case `"WL-CTX-09: a menu opened by a declared key continues
that keystroke -- eligible on its turn, refused after a newer key, and anchored in the pane's own
body"`, case `"WL-CTX-09: a chosen row may continue into the host's own pane menu on a subject the
pane names -- once, while the choice is the maker's latest act, and never for a pane the inventory
lacks"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-10 — The presenter owns a menu's showing and lifetime; replacing it is ordinary

LAW — The presenter's office shows a granted menu, reads its acts, ends it and answers once; any image may hold it, and a reload keeping `HeldMenu` hands an open menu to the new image.

MEANS
- the shipped presenter chooses on the press; the numbered example on a digit or the release;
- a requester takes a choice only from this office, under its own ask's number, once (`Asked`);
- a presenter that leaves, or a holder not carrying the menu, ends it answered by the host.

DOES NOT MEAN
- that a presenter reads a subject or holds authority: the requester judges and acts;
- that every later participant must hand over or cancel alike: it is these consumers' policy.

PROVEN BY — `workshop/presenter_vocabulary.hpp` `HeldMenu`, `PresenterReady`, `MenuClosed`,
`kMaxMenuLines`; `menu-presenter/presenter.cpp` `MenuPresenter`, `refusal_of`;
`examples/numbered-presenter/presenter.cpp` `NumberedPresenter`, `digit_row`, `line_for`;
`workshop/pane_menu.hpp` `Asked`, `Asked::take`, `Offer`; `desktop-pane/pane.cpp`
`launcher_asked_`, `keys_asked_`; `workshop/weave_external.cpp` `on(PresenterReady)`;
`workshop/weave_code.cpp` `on(DispatchRefused)`; `workshop/weave_seam.cpp` `on(PaneOffered)`;
`tests/test_workshop_panes_button.cpp` case `"WL-CTX-09: a menu with more rows than the room is
windowed by the presenter, and every row is still reachable"`, case `"WL-CTX-10: a requester's
record of its ask settles on the presenter's answer or the host's refusal -- once, and on nothing
another office says under its number"`; `tests/test_workshop_panes_desktop.cpp` case
`"WL-CTX-10: a reloaded desktop cancels its predecessor's menu -- withdrawn when the successor
offers its pane again, and a choice about it acts on nothing"`, case `"WL-CTX-10: the desktop acts
only on the presenter's answer to an ask of this image's own -- not the host's choice, not another
subject, not another number, not a predecessor's, and not twice"`, case `"WL-CTX-10: an ordinary
replacement presenter holds the office -- the Pane Manager and Hotkeys keep their own operations
while it presents their menus its way: numbered, a digit chooses, a release chooses"`, case
`"WL-CTX-10: the replacement presenter presents the Pane Manager's menu too, and a digit opens the
row -- through the desktop's own launch"`, case `"WL-CTX-10: the presenter reloaded in place by
another image while a menu is open hands the menu over -- shown again the new way with its cursor,
answered under the same number, and the Hotkeys edit completes"`, case `"WL-CTX-10: a presenter
that leaves ends its menu, answered by the host; with none in the office a menu is refused in
words and the host's own menu still opens; one loaded afresh does not carry the old menu, which
ends answered"`.
WHY — `agents/decisions/a-menu-is-presented-by-a-participant.md`
