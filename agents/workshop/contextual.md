# Workshop law — the contextual surface

Register `WL-CTX`: what can I do with this? One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-CTX-01 — Pointing names a subject for one request; selection is a state a weaver entered

LAW — Opening captures a subject — a `PaneRef`, a layout position, or nothing; never a rectangle, row or handle — and changes no selection, candidate or focus; spend re-asks the owner.

MEANS
- right press: the pointed subject; `workshop.context` (`a`): the room, all command mode names;
- a ref outside the setup gets one truthful absence sentence, not a geometry refusal;
- ⭐ an object id was a subject, and its load the aliasing door, until the object canvas retired.

WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-02 — Arrange is the one exception

LAW — Arrange binds the scope and selects the subject, and only after its explicit target passes admission (`enter_arrange_pane` → the target-taking `arrange_geometry_ready`).

DOES NOT MEAN
- that any other contextual action selects, binds or focuses — Arrange is the one exception.

WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-03 — The popup is local and its bounds are derived

LAW — The popup remembers only the press's canvas point; its rectangle is re-derived at every paint and press from the current level's rows, through the one popup measurer, capped at a maximum width.

MEANS
- the keyboard entrance is `anchored == false` and opens at the overlay stack's corner;
- a group re-derives at the same anchor; a level taller than the room is cut, and says so;
- `popup_bounds_at`: one measurer, one chrome, one clamp in the overlay band.

WHY — `agents/decisions/content-sized-popups.md`

## WL-CTX-04 — The first row is an action

LAW — There is no title row and no hint row: painted row i is population row i, and the width is the widest action row, so the popup shrinks to its content.

MEANS
- a group's row ends in the one submenu mark, as a pane's row that opens a menu does.

WHY — `agents/decisions/content-sized-popups.md`

## WL-CTX-05 — `kContextCatalog` declares, and owns no power

LAW — The catalog declares rows — an action id, its subjects, a group — over the action catalog's ids, so a stale reference is a compile error; one population is what every consumer of the menu spends.

MEANS
- a pane's: `arrange`, `Order >`, `Reset >`, `edit code`, `hide pane`;
- a tab's: `layout.rename`, `layout.duplicate`, `Order >`, `layout.remove`; the room's: no target;
- groups are their names, and an empty group cannot exist.

WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-06 — A row may teach its shortcut, and only a truthful one

LAW — A row shows its effective gesture exactly when its action owns a binding active in the context the weaver returns to; a row whose action is unbound never annotates.

MEANS
- a layout row only when the captured tab is the active one, else row and key act on two subjects;
- a mode beneath that swallows bare keys (a pane holding them) suppresses a command row's key.

WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-07 — Spending is one seam per subject kind, and paint is not policy

LAW — One seam per subject kind — `spend_pane_action`, the position-taking layout doors, the room's zero-target owners — and the owner refuses at spend; paint is not policy.

MEANS
- no owner predicate runs on the paint path: the menu renders an identity, not an existence;
- ⭐ an object's seam, `delete_object_at`, retired with the object canvas and its draft guard.

WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-08 — The surface is a mode with first refusal, and a pass-back is the host's fallback

LAW — `KeyContext::kContext` tops the modes beneath it: inside, navigate or choose (`context_press_at`); outside, dismissal, consumed whole; a pass-back opens the host's rows for that pane, once, at its cell.

MEANS
- a further right press re-targets, offering a pane under it its press first -- one a graphics window shows in its own area is sent nothing;
- a pass-back is a continuation (WL-PRESS-06): stale, zero, spent or foreign moves nothing; in a graphics window it cites the window's number;
- a right press in a doorless body opens the same rows at the press: a body press is not lost.

DOES NOT MEAN
- that a pane's rows reach this surface unasked: only by its request (WL-CTX-09);
- that a row is spent by another owner: the host spends these, a pane its own (WL-CTX-09).

WHY — `agents/decisions/pointing-is-not-selection.md`

## Proven by

### WL-CTX-01
`workshop/context.hpp` ContextMenu context_subject
`workshop/keymap.hpp` workshop.context
`workshop/weave_pointer.cpp` spend_context_choice open_context_at open_context_ambient
`workshop/panes.hpp` Panes::selected
`tests/test_workshop_host.cpp` "a right press captures a subject and selects nothing" "a captured
pane that left the setup is refused truthfully" "the keyboard door opens on what command mode can
name: the room"

### WL-CTX-02
`workshop/weave_arrange.cpp` enter_arrange_pane arrange_geometry_ready
`workshop/weave_pointer.cpp` spend_context_choice
`tests/test_workshop_host.cpp` "contextual Arrange admission precedes binding"
`tests/test_workshop_screen.cpp` "contextual Arrange lifts the pane it addressed, not the one in
front" "every pane a weaver can point at can be arranged, and the refusals are blind"

### WL-CTX-03
`workshop/context.hpp` ContextMenu ContextMenu::anchored ContextMenu::anchor_x
`workshop/screen_attention.cpp` context_bounds context_entry_text
`workshop/screen_pane_state.cpp` popup_bounds_at
`workshop/screen.hpp` kContextMaxCols chrome_outer_of
`workshop/pane_text_fit.hpp` region_px_for
`tests/test_workshop_screen.cpp` "the popup opens at the press's own cell, and its extent is its
content" "the popup shifts to stay usable inside the room, at every boundary" "the keyboard entrance
has no pointer and invents none" "entering a group stays at the anchor, and the popup resizes to it"

### WL-CTX-04
`workshop/screen_attention.cpp` context_press_at paint_context context_row_text context_entry_text
`workshop/pane_vocabulary.hpp` kSubmenuMark
`workshop/pane_menu.hpp` Offer::submenu
`tests/test_workshop_screen.cpp` "the contextual surface is its actions, and its width is theirs"
"the contextual surface is painted where it is hit"

### WL-CTX-05
`workshop/context.hpp` kContextCatalog context_actions_resolve context_population kOnPane kOnLayout
kOnRoot context_same_id
`workshop/keymap.hpp` kActionCatalog
`tests/test_workshop_host.cpp` "the declared populations are the researched ones, keyed by id"
`tests/test_workshop_document.cpp` "the shipped catalog stays admissible with the new rows"
`tests/test_workshop_screen.cpp` "an open group paints its own rows and its own way out"

### WL-CTX-06
`workshop/screen_attention.cpp` context_annotation context_row_text
`workshop/screen_arrange.cpp` keyboard_context_beneath_menu
`workshop/keymap.hpp` active_in is_bound
`tests/test_workshop_screen.cpp` "shortcut annotations teach only truthful surrounding bindings"
`tests/test_workshop_host.cpp` "a tab's context menu acts on THAT tab"

### WL-CTX-07
`workshop/weave_arrange.cpp` spend_pane_action
`workshop/weave_session.cpp` open_layout_rename duplicate_layout shift_layout drop_layout
`workshop/weave_pointer.cpp` spend_context_choice
`tests/test_workshop_host.cpp` "a contextual action acts on the pointed pane, not the selection" "a
contextual remove removes the pointed pane" "Move Left and Move Right reorder from the tab that was
pointed at"

### WL-CTX-08
`workshop/keymap.hpp` KeyContext::kContext
`workshop/screen_arrange.cpp` keyboard_context
`workshop/screen_attention.cpp` context_press_at
`workshop/screen.hpp` ContextPressAt
`workshop/weave_pointer.cpp` spend_context_choice choose_context_row context_press
`workshop/weave_external.cpp` on(PanePassRequested)
`workshop/pane_menu.hpp` pass_back
`tests/test_workshop_host.cpp` "input spent on the open surface does not leak through it"
"navigation backtracks cleanly and every way out closes"
`tests/test_workshop_panes_window.cpp` "a right press over a provider's pane crosses the seam not at
all" "input spent on the open surface reaches no provider"
`tests/test_workshop_panes_button.cpp` "WL-PRESS-06: a right press in a doorless pane's body opens
the host's pane menu at the press, sends the provider nothing and takes no keys; its chrome opens
the same menu" "WL-CTX-08: a further right press while the host's menu is open asks again -- a pane
with the door is offered it first, and a doorless body opens the host's menu where it landed"
"WL-CTX-08: a pass-back after a clean click opens the host's menu for that pane, once" "WL-CTX-08: a
pass-back on the press's own turn opens the menu, and the release still reaches the pane under it"
"WL-CTX-08: a stale correlation, a zero one, and a pass-back from an office that did not offer the
pane all move nothing"
`workshop/weave_handlers.cpp` on(AreaPressed)
`tests/test_workshop_desk.cpp` "a press in a pane's area of the window is the pane's, and under the window's number Workshop
tells it where the keys were, hands its press back, opens its menu and carries what it drags out
or picks up" "Workshop's own area of the window answers each button as itself: a press on the room says
so, a right press asks its menu about the room or a tab, re-asking it while one is open, a press
on the gear anchors there, and a tab carried along the run reorders it"
