# Workshop law — arrangement

Register `WL-ARR`: gestures, the resize law, the two arranging scopes, the coarse step, and
Escape. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-ARR-01 — One press claims one gesture until release

LAW — `PaneGesture` holds an identity, an edge and the size at the press — no rectangle, no live position — so every motion proposes `base + (pointer - press)` and nothing crossed moves it.

MEANS
- crossing another pane, the Terminal or a reorder changes nothing about who is being moved;
- outside arrangement an addressed pane behind another claims no press and no address auto-raises.

PROVEN BY — `workshop/screen.hpp` `PaneGesture`, `Session::pane_drag`, `kPaneEdgeBandSubs`;
`workshop/screen_arrange.cpp` `pane_edge_at`; `workshop/weave_arrange.cpp` `take_pane_hold`,
`arrange_motion`; `tests/test_workshop_panes_window.cpp` case `"WIND-2: one press claims one
gesture, and crossing anything does not move it"`, case `"WIND-2: outside arrangement, an
addressed pane behind another clicks through nothing"`.
WHY — `agents/decisions/one-press-one-gesture.md`

## WL-ARR-02 — `end_held_gestures()` is the one release owner

LAW — Every branch that can see a release calls `end_held_gestures()`, because a gesture begun under one mode may be released under another; what to tell the maker is the caller's.

DOES NOT MEAN
- that a gesture may end anywhere else — `end_held_gestures()` is the one release owner.

PROVEN BY — `workshop/weave_pointer.cpp` `end_held_gestures`; `tests/test_workshop_screen.cpp`
case `"WIND-2a: a release ends a pane gesture whatever mode sees it"`.
WHY — `agents/decisions/one-press-one-gesture.md`

## WL-ARR-03 — `forget_removed_selection()` clears on membership, never on presentation

LAW — A pane that becomes waiting, refused, covered, off-room or unresolved stays addressed; a reference leaving the setup clears the address and its gesture, and closes the one-pane scope.

MEANS
- the desk stays open, its subject being the desk;
- it runs inside `apply_setup`, the one door membership changes through.

PROVEN BY — `workshop/weave_arrange.cpp` `forget_removed_selection`;
`workshop/weave_session.cpp` `apply_setup`; `workshop/screen.hpp` `PaneArrange`;
`tests/test_workshop_screen.cpp` case `"WIND-2a: a removed target leaves no stale selection,
submode or heading"`, case `"ARR-0: removing the pane being arranged ends the arrangement about
it"`; `tests/test_workshop_panes_window.cpp` case `"WIND-2: clearing the selected pane clears its
gesture safely"`.
WHY — `agents/decisions/one-press-one-gesture.md`

## WL-ARR-04 — A resize begins from the resolved window

LAW — A first edit measures from `managed_bounds().resolved`, the unclipped ask captured at the press; `rect` is the visible intersection and owns painting, occupancy, coverage and handles.

MEANS
- a default pane resolving to 89 cells with four on screen answers one rightward step by five.

PROVEN BY — `workshop/weave_arrange.cpp` `managed_bounds`, `managed_window_base`;
`workshop/screen.hpp` `PanelBounds`, `PanelBounds::rect`; `workshop/screen_gestures.cpp`
`pane_window_proposal`; `tests/test_workshop_screen.cpp` case `"WIND-2a: a clipped default resize
begins from the full resolved size"`.
WHY — `agents/decisions/anchors-and-axes.md`

## WL-ARR-05 — Every resize edge preserves its opposite anchor

LAW — The pulled edge follows the hand and the opposite edge holds still; a corner holds the corner across from it; right and bottom pulls leave a default place reactive by not writing it.

PROVEN BY — `workshop/screen_gestures.cpp` `pane_window_proposal`; `workshop/screen.hpp`
`pane_edge::kBottomRight`, `PaneWindowProposal`; `workshop/setup.hpp` `author_pane_window`;
`workshop/weave_arrange.cpp` `arrange_resize`; `tests/test_workshop_screen.cpp` case `"WUX-2:
every edge resizes pixel-fine and preserves its opposite anchor"`, case `"WUX-2: the reported
top-edge defect is dead -- the bottom edge holds still"`, case `"WUX-2: a right or bottom resize
leaves a default place reactive"`.
WHY — `agents/decisions/anchors-and-axes.md`

## WL-ARR-06 — Independent axes settle independently, refuse-never-clamp

LAW — A left or top pull authors place and size as one axis-local transaction, a gesture blocked on one axis still settles the other, and an unchanged axis is no proposal, so it writes nothing.

MEANS
- `author_pane_window` is the one gesture door; a refused height never leaves a moved top edge;
- a refused single-axis step cannot author a reactive place as a side effect.

DOES NOT MEAN
- that a wall clamps an axis — a blocked axis is refused whole and keeps its authored value.

PROVEN BY — `workshop/setup.hpp` `author_pane_window`, `check_pane_place_coord`,
`PaneAxisProposal`; `workshop/screen_gestures.cpp` `pane_window_proposal`; `workshop/screen.hpp`
`PaneWindowProposal`; `workshop/weave_arrange.cpp` `arrange_place`, `arrange_resize`;
`tests/test_workshop_screen.cpp` case `"WUX-2: a refused anchored resize writes neither the place
nor the size"`, case `"WUX-2a: a move blocked at the left wall still follows the hand down"`, case
`"WUX-2a: a move past two walls at once writes nothing"`, case `"WUX-2a: a refused nudge does not
author a reactive place"`, case `"WUX-2a: a corner resize blocked on one axis still resizes the
other"`.
WHY — `agents/decisions/anchors-and-axes.md`

## WL-ARR-07 — Arrangement is two scopes and one vocabulary

LAW — The one-pane scope is bound to exactly its pane — admission before binding through `arrange_geometry_ready` — and the desk opens with no pane addressed, every pane answering the pointer.

MEANS
- `PaneArrange{open, desk, pane, resetting}` replaced the selector with submodes;
- in the one-pane scope a press elsewhere is consumed with the sentence naming the state;
- on the desk a press takes hold and makes that pane the keyboard's target, topmost first.

PROVEN BY — `workshop/screen.hpp` `PaneArrange`, `Session::arrange`;
`workshop/weave_arrange.cpp` `enter_arrange_pane`, `take_pane_hold`, `arrange_geometry_ready`,
`open_arrange_desk`, `arrange_press`; `tests/test_workshop_panes_window.cpp` case `"ARR-0: the
one-pane scope is bound -- another pane cannot be drawn into it"`, case `"ARR-0: the desk
manipulates panes directly, and a press is its own targeting"`; `tests/test_workshop_panels.cpp`
case `"CTX-0/ARR-0: contextual Arrange admission precedes binding"`.
WHY — `agents/decisions/two-arranging-scopes.md`

## WL-ARR-08 — The arranging keys are one vocabulary in both scopes

LAW — Arrows place one cell, shift+arrows pull the extent one cell anchored at the place, `=`/`-` the coarse step, `f`/`b`/`r`/`l` order, `d` remove, `0` the reset prompt, esc leave.

MEANS
- the desk adds `tab`/`shift+tab` over `arrangeable()` — every setup row — and Return;
- a hand and a key author the same setup values; escape unwinds one level and rolls nothing back.

PROVEN BY — `workshop/keymap.hpp` `kActionCatalog`, `KeyContext::kArrangePane`,
`KeyContext::kArrangeDesk`, `KeyContext::kArrangeReset`; `workshop/weave_arrange.cpp`
`arrange_key`, `arrangeable`, `arrange_step`, `arrange_nudge`, `arrange_grow`;
`tests/test_workshop_panes_window.cpp` case `"WIND-2: the keyboard alone reaches every window
operation"`, case `"WIND-2: a hand and a key author the same setup values"`, case `"WIND-2: escape
unwinds one level and rolls nothing back"`.
WHY — `agents/decisions/two-arranging-scopes.md`

## WL-ARR-09 — Arranging a pane is choosing it, and the rings are its statement

LAW — `enter_arrange_pane` writes `Panels::selected` from the addressed reference after admission and nothing else; the state's visible statement is the rings, the legend and the notice.

MEANS
- rings: accent in the one-pane scope; over the desk muted, with accent on the target;
- `arrange_status()` carries the pane's state word, so an invisible pane is recoverable by ear.

PROVEN BY — `workshop/weave_arrange.cpp` `enter_arrange_pane`, `arrange_status`;
`workshop/screen_browser.cpp` `paint_pane_affordances`; `workshop/screen.hpp` `pane_edge_name`;
`workshop/screen_arrange.cpp` `pane_edge_cell`; `tests/test_workshop_screen.cpp` case `"ARR-0: the
arrangement's visible statement is the ring on the pane itself"`, case `"WUX-7: contextual Arrange
lifts the pane it addressed, not the one in front"`; `tests/test_workshop_panes_window.cpp` case
`"ARR-0: stepping names the pane, its state and its authored window in words"`.
WHY — `agents/decisions/two-arranging-scopes.md`

## WL-ARR-10 — The coarse step is the fine step with a bigger delta

LAW — The coarse step resizes the addressed pane by the coarse count on both axes through the ordinary resize seam: one proposal at the bottom-right edge, into the one gesture door, and no second owner.

MEANS
- it meets the identical per-axis settlement a shifted arrow meets, and there is no second owner;
- it cannot move the pane it resizes, moves no other pane, and measures no content.

PROVEN BY — `workshop/weave_arrange.cpp` `arrange_grow`; `workshop/screen.hpp`
`kCoarseStepCells`, `pane_edge::kBottomRight`; `workshop/screen_gestures.cpp`
`pane_window_proposal`; `workshop/setup.hpp` `author_pane_window`;
`tests/test_workshop_panes_window.cpp` case `"WUX-6/SC-5+SC-7: the coarse step is the resize seam
with a bigger delta"`, case `"WUX-6/SC-5: a coarse shrink meets the same per-axis refusal a fine
one does"`.
WHY — `agents/decisions/why-the-coarse-step-is-four.md`

## WL-ARR-11 — Four is pinned, not chosen

LAW — A `static_assert` holds `kStackRows + kCoarseStepCells - 2*kChromeCells - 1 >= 8`, so one grow gives a default stack pane's body the eight rows the Compose form needs.

PROVEN BY — `workshop/screen.hpp` `kCoarseStepCells`, `kStackRows`, `kChromeCells`;
`tests/test_workshop_panes_input.cpp` case `"WUX-6/SC-7: one coarse grow gives the DEFAULT
Compose pane a usable form"`.
WHY — `agents/decisions/why-the-coarse-step-is-four.md`

## WL-ARR-12 — The coarse step is ordinary action vocabulary

LAW — `manage.grow`/`manage.shrink` are catalog rows declared in both scopes, so one override moves both; the legend and the hotkey view say them, and no pane's chrome paints them.

PROVEN BY — `workshop/keymap.hpp` `manage.grow`, `manage.shrink`;
`tests/test_workshop_panes_window.cpp` case `"WUX-6/SC-7: the coarse step is ordinary action
vocabulary, not pane chrome"`; `tests/test_workshop_screen.cpp` case `"WUX-5: no ordinary pane
spends a row teaching a key the keymap already owns"`.
WHY — `agents/decisions/why-the-coarse-step-is-four.md`

## WL-ARR-13 — Escape is back, not cancel, and its last meaning puts the pane down

LAW — After every mode, overlay and draft has answered, a bare Escape where a list or nothing holds the keys sheds `Panels::selected` and the keyboard candidate, moving nothing else.

MEANS
- every immediate-commit gesture is reversible only by its inverse; there is no undo;
- a desk whose panes cover every usable cell still reaches selection = none;
- it is not a keymap action: a recovery gesture must not be authorable into a lockout.

PROVEN BY — `workshop/weave_external.cpp` `unselect_pane`; `workshop/screen_arrange.cpp`
`escape_may_shed_selection`; `tests/test_workshop_panels.cpp` case `"QR-18/SC-1+SC-3: Escape
clears the ordinary selection last, and the Pane Editor's subject stands"`, case `"QR-18/SC-2:
every more-specific Escape meaning answers first, and deselection waits"`, case `"QR-18/SC-4: a
desk with no unoccupied cell still reaches selection = none"`.
WHY — `agents/decisions/escape-is-back.md`

## WL-ARR-14 — A place a maker types into keeps Escape

LAW — The source editor's Escape is a pinned no-op, and a focused external pane has already been sent the key; the way out of either is a press on a pane that takes no text, then Escape.

PROVEN BY — `workshop/weave_external.cpp` `unselect_pane`; `workshop/screen_arrange.cpp`
`escape_may_shed_selection`, `keyboard_context`; `tests/test_workshop_panes_input.cpp` case
`"QR-18/SC-1+SC-2: a focused external pane keeps Escape; a press on a pane that takes no text,
then Escape, puts the selection down"`; `tests/test_workshop_editor.cpp` case `"EDIT-0: Escape
means nothing in the editor -- no mode closes, no text moves"`.
WHY — `agents/decisions/escape-is-back.md`

## Do not assume

- That Escape with a pane selected does nothing, or closes the pane — it puts the selection
  down, last, and closes, moves, ranks and writes nothing (WL-ARR-13).
