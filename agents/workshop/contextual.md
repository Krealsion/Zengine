# Workshop law — the contextual surface

Register `WL-CTX`: what can I do with this? One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-CTX-01 — Pointing names a subject for one request; selection is a state a maker entered

LAW — Opening captures a subject — a `PaneRef`, an object id, a layout position, or nothing; never a rectangle, row or handle — and changes no selection, candidate or focus; spend re-asks the owner.

MEANS
- right press: the pointed subject; `workshop.context` (`a`): the selected object, else the room;
- a ref outside the setup gets one truthful absence sentence, not a geometry refusal;
- `load_document()` drops a captured object subject — the one identity-aliasing door.

PROVEN BY — `workshop/context.hpp` `ContextMenu`, `context_subject`; `workshop/keymap.hpp`
`workshop.context`; `workshop/weave_save.cpp` `load_document`; `workshop/weave_pointer.cpp`
`spend_context_choice`, `open_context_at`, `open_context_ambient`; `workshop/screen_gestures.cpp`
`object_at`; `workshop/panel.hpp` `Panels::selected`; `tests/test_workshop_panels.cpp` case
`"CTX-0: a right press captures a subject and selects nothing"`, case `"CTX-0: a captured pane
that left the setup is refused truthfully"`, subcase `"the keyboard door opens on what command
mode can name"`; `tests/test_workshop_document.cpp` case `"CTX-0: replacing the document drops a
captured object subject"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-02 — Arrange is the one exception

LAW — Arrange binds the scope and selects the subject, and only after its explicit target passes admission (`enter_arrange_pane` → the target-taking `arrange_geometry_ready`).

DOES NOT MEAN
- that any other contextual action selects, binds or focuses — Arrange is the one exception.

PROVEN BY — `workshop/weave_arrange.cpp` `enter_arrange_pane`, `arrange_geometry_ready`;
`workshop/weave_pointer.cpp` `spend_context_choice`; `tests/test_workshop_panels.cpp` case
`"CTX-0/ARR-0: contextual Arrange admission precedes binding"`; `tests/test_workshop_screen.cpp`
case `"WUX-7: contextual Arrange lifts the pane it addressed, not the one in front"`, case
`"WUX-7: a refused Arrange leaves the selection exactly where it was"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-03 — The popup is local and its bounds are derived

LAW — The popup remembers only the press's canvas cell; its rectangle is re-derived at every paint and press from the current level's rows, through the one popup measurer, capped at a maximum width.

MEANS
- the keyboard entrance is `anchored == false` and opens at the overlay stack's corner;
- a group re-derives at the same anchor; a level taller than the room is cut, and says so;
- `popup_bounds_at`: one measurer, one chrome, one clamp in the overlay band; the hotkeys use it.

PROVEN BY — `workshop/context.hpp` `ContextMenu`, `ContextMenu::anchored`,
`ContextMenu::anchor_x`; `workshop/screen_attention.cpp` `context_bounds`, `context_entry_text`;
`workshop/screen_pane_state.cpp` `popup_bounds_at`; `workshop/screen.hpp` `kContextMaxCols`,
`chrome_outer_of`; `workshop/screen_hotkeys.cpp` `hotkeys_bounds`; `surface/region.hpp`
`region_cells_for`; `tests/test_workshop_screen.cpp` case `"ARR-0: the popup opens at the press's
own cell, and its extent is its content"`, case `"ARR-0: the popup shifts to stay usable inside
the room, at every boundary"`, case `"ARR-0: the keyboard entrance has no pointer and invents
none"`, case `"ARR-0: entering a group stays at the anchor, and the popup resizes to it"`.
WHY — `agents/decisions/content-sized-popups.md`

## WL-CTX-04 — The first row is an action

LAW — There is no title row and no hint row: painted row i is population row i, and the width is the widest action row, so the popup shrinks to its content.

PROVEN BY — `workshop/screen_attention.cpp` `context_press_at`, `paint_context`,
`context_row_text`, `context_entry_text`; `tests/test_workshop_screen.cpp` case `"WUX-5: the
contextual surface is its actions, and its width is theirs"`, case `"CTX-0: the contextual surface
is painted where it is hit"`.
WHY — `agents/decisions/content-sized-popups.md`

## WL-CTX-05 — `kContextCatalog` declares, and owns no power

LAW — The catalog declares rows — an action id, its subjects, a group — over the action catalog's ids, so a stale reference is a compile error; one population is what every consumer of the menu spends.

MEANS
- a pane's level: `arrange`, `Order >`, `Reset >`, `remove`; an object's: `object.delete`;
- a tab's: `layout.rename`, `layout.duplicate`, `Order >`, `layout.remove`; the room's: no target;
- groups are their names, and an empty group cannot exist.

PROVEN BY — `workshop/context.hpp` `kContextCatalog`, `context_actions_resolve`,
`context_population`, `kOnPane`, `kOnObject`, `kOnLayout`, `kOnRoot`, `context_same_id`;
`workshop/keymap.hpp` `kActionCatalog`; `tests/test_workshop_panels.cpp` case `"CTX-0: the
declared populations are the researched ones, keyed by id"`; `tests/test_workshop_document.cpp`
case `"CTX-0: the shipped catalog stays admissible with the new rows"`;
`tests/test_workshop_screen.cpp` case `"CTX-0: an open group paints its own rows and its own way
out"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-06 — A row may teach its shortcut, and only a truthful one

LAW — A row shows its effective gesture exactly when its action owns a binding active in the context the maker returns to; a row whose action is unbound never annotates.

MEANS
- `object.delete` is taught exactly when the captured object is the selection;
- a layout row only when the captured tab is the active one, else row and key act on two subjects.

PROVEN BY — `workshop/screen_attention.cpp` `context_annotation`, `context_row_text`;
`workshop/screen_arrange.cpp` `keyboard_context_beneath_menu`; `workshop/keymap.hpp` `active_in`,
`is_bound`; `tests/test_workshop_screen.cpp` case `"ARR-0: shortcut annotations teach only
truthful surrounding bindings"`, case `"ARR-0: object.delete teaches its key exactly when the
subject IS the selection"`; `tests/test_workshop_panels.cpp` case `"WUX-11/SC-2+SC-5: a tab's
context menu acts on THAT tab"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-07 — Spending is one seam per subject kind, and paint is not policy

LAW — One seam per subject kind — `spend_pane_action`, `delete_object_at`, the position-taking layout doors, the room's zero-target owners — and the owner refuses at spend; paint is not policy.

MEANS
- `delete_object_at` reuses `delete_selected` for the selection; a live draft holds it back;
- no owner predicate runs on the paint path: the menu renders an identity, not an existence.

PROVEN BY — `workshop/weave_arrange.cpp` `spend_pane_action`; `workshop/weave_save.cpp`
`delete_object_at`, `finish_draft_first`, `context_delete_object`; `workshop/weave_session.cpp`
`open_layout_rename`, `duplicate_layout`, `shift_layout`, `drop_layout`;
`workshop/weave_pointer.cpp` `spend_context_choice`; `workshop/screen_gestures.cpp`
`delete_selected`; `tests/test_workshop_panels.cpp` case `"CTX-0: a contextual action acts on the
pointed pane, not the selection"`, case `"CTX-0: a contextual remove removes the pointed pane"`,
case `"WUX-11/SC-4: Move Left and Move Right reorder from the tab that was pointed at"`;
`tests/test_workshop_document.cpp` case `"CTX-0: contextually deleting the selected object uses
the existing repair"`, case `"CTX-0: a live draft holds a contextual deletion back"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-08 — The surface is a mode with first refusal

LAW — `KeyContext::kContext` tops the picker band and the pointer branch consumes every press while open: inside, navigate or choose through `context_press_at`; outside, dismissal, consumed whole.

MEANS
- a further right press re-targets rather than toggling.

DOES NOT MEAN
- that anything crosses the provider seam — no second-button `PanePressed`, no provider rows.

PROVEN BY — `workshop/keymap.hpp` `KeyContext::kContext`; `workshop/screen_arrange.cpp`
`keyboard_context`; `workshop/screen_attention.cpp` `context_press_at`; `workshop/screen.hpp`
`ContextPressAt`; `workshop/weave_pointer.cpp` `spend_context_choice`, `choose_context_row`,
`context_press`; `tests/test_workshop_panels.cpp` case `"CTX-0: input spent on the open surface
does not leak through it"`, case `"CTX-0: navigation backtracks cleanly and every way out
closes"`; `tests/test_workshop_panes_window.cpp` case `"CTX-0: a right press over a provider's
pane crosses the seam not at all"`, case `"CTX-0: input spent on the open surface reaches no
provider"`.
WHY — `agents/decisions/pointing-is-not-selection.md`
