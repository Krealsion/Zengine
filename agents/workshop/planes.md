# Workshop law — planes

Register `WL-FRONT`: the plane sequence, the three vertical regions, and the selection lift.
One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-FRONT-01 — The plane sequence is the layout of the screen

LAW — The canvas is published in one depth order: the workspace, one plane per pane in `effective_pane_order` ascending, the affordances, the picker and overlays, the foot band, the Terminal.

MEANS
- an overlapping pane is painted where it is hit, in both front orders;
- a provider's text cannot bury the picker that recovers it;
- the Terminal overlay outranks panels for the pointer too, and by a wider rule.

PROVEN BY — `workshop/screen_compose.cpp` `paint`, `paint_panels`, `band_region`;
`workshop/screen_browser.cpp` `paint_pane_affordances`; `workshop/screen.hpp` `on_own_layer`;
`workshop/setup.hpp` `effective_pane_order`; `tests/test_workshop_screen.cpp` case `"WIND-2a: an
overlapping pane is painted where it is hit, in both front orders"`, case `"the terminal overlay
outranks panels for the pointer too, and by a wider rule"`; `tests/test_workshop_panes_seam.cpp`
case `"WIND-2a: an external pane's own text cannot bury the surface that recovers it"`.
WHY — `agents/decisions/front-is-a-permutation.md`

## WL-FRONT-02 — The foot band is in front of the panes and owns no pointer space

LAW — The foot band owns its whole rectangle (`kGroundOwn`) and occupies no pointer space; the top rows belong to a pane, so nothing paints in front of a pane while hitting nothing but tabs.

MEANS
- a band is where the tool speaks: a pane authored over it is covered by it;
- a pane in front of the Layouts pane takes the press, and the Layouts pane reads `covered`.

PROVEN BY — `workshop/screen_compose.cpp` `band_region`; `workshop/screen.hpp` `band_bounds`;
`workshop/screen_chrome.cpp` `occupied_at`; `surface/vocabulary.hpp` `kGroundOwn`;
`tests/test_workshop_screen.cpp` case `"WUX-12/SC-5+SC-7: a pane in front of the Layouts pane
takes the press"`; `tests/test_workshop_document.cpp` case `"QR-14/SC-2+SC-7: two bands compose
their budgets, and the selector is row 0"`.
WHY — `agents/decisions/two-bands.md`

## WL-FRONT-03 — Three regions tile the screen exactly

LAW — Three regions tile the screen exactly: the first rows are reserved (the Layouts pane's default), the body follows, and the last rows are the foot; the reserved and foot rows together are six, asserted.

MEANS
- `room_h` never moved: chrome that moves must not resize a maker's document;
- the reserved rows are two because one cell holds zero rows of a real face;
- slots, the side region, the overlay column and occupancy all begin at `kWorkspaceY`.

PROVEN BY — `workshop/screen.hpp` `kTopRows`, `kBottomRows`, `kWorkspaceY`, `Screen::room_h`,
`band_bounds`, `top_band_bounds`; `tests/test_workshop_screen.cpp` case `"QR-14/SC-2: the layout
selector is the first Workshop row, on both media"`, case `"QR-14/SC-2: the move re-homed
reserved rows and did not add one"`, case `"QR-14/SC-6: every owner of the body agrees about
where it begins"`.
WHY — `agents/decisions/two-bands.md`

## WL-FRONT-04 — `Panels::selected` is a press's memory

LAW — The selection is a press's memory: session-only, never persisted, none at start, resolved to a pane by one reader; it has four writers and no other.

MEANS
- the press line, `enter_arrange_pane` after admission, `open_source`, Escape's fallthrough;
- a refused Arrange leaves the selection exactly where it was.

PROVEN BY — `workshop/panel.hpp` `Panels::selected`, `selected_pane`, `kNoPaneKind`;
`workshop/weave_arrange.cpp` `enter_arrange_pane`; `workshop/weave_external.cpp` `unselect_pane`;
`workshop/weave_pane_editor.cpp` `open_source`; `tests/test_workshop_screen.cpp` case `"WUX-5: the
selection lift never reaches the file, and no session starts with one"`, case `"WUX-7: contextual
Arrange lifts the pane it addressed, not the one in front"`, case `"WUX-7: a refused Arrange
leaves the selection exactly where it was"`.
WHY — `agents/decisions/the-selection-lift.md`

## WL-FRONT-05 — `effective_pane_order` is the one foreground order

LAW — The authored permutation with the selected pane lifted is the one answer, and every consumer meaning "in front right now" spends it; `presentation_order` is the authored base.

MEANS
- `paint_panels` ascending, `occupied_at` descending, `pane_is_covered`, the desk's pointer walk;
- persistence and `reset order` want the authored base, and nothing else may.

PROVEN BY — `workshop/setup.hpp` `effective_pane_order`, `presentation_order`;
`workshop/screen_compose.cpp` `paint_panels`; `workshop/screen_chrome.cpp` `occupied_at`;
`workshop/screen_pane_state.cpp` `pane_is_covered`; `workshop/panel.hpp` `selected_pane`;
`workshop/weave_arrange.cpp` `arrange_press`; `tests/test_workshop_screen.cpp` case `"WUX-5:
selecting a pane lifts it, in the picture and under the hand at once"`, case `"WUX-5/WUX-7: the
arrangement desk's pointer takes what is visibly in front"`;
`tests/test_workshop_panes_window.cpp` case `"WIND-2: hit order is the exact reverse of paint
order"`.
WHY — `agents/decisions/the-selection-lift.md`

## WL-FRONT-06 — The lift is a rotation and never a write

LAW — No rank is read differently or written, `panels.open` is untouched, nothing reaches a file, and a selection that is not seated lifts nothing; `manage.front` is the permanent statement.

PROVEN BY — `workshop/setup.hpp` `effective_pane_order`, `presentation_order`;
`workshop/keymap.hpp` `manage.front`; `tests/test_workshop_screen.cpp` case `"WUX-5: the selection
lift never reaches the file, and no session starts with one"`;
`tests/test_workshop_panes_window.cpp` case `"WIND-2: ordering changes paint order and NOTHING
else"`.
WHY — `agents/decisions/the-selection-lift.md`

## WL-FRONT-07 — The transient planes stay above the panes

LAW — The lift orders the ordinary pane planes among themselves and reaches no further, so a selected pane is never drawn over the menu a maker just opened on it.

PROVEN BY — `workshop/screen_attention.cpp` `paint_context`; `workshop/screen_compose.cpp`
`paint_panels`, `paint`; `tests/test_workshop_screen.cpp` case `"WUX-5: a transient surface stays
over the pane it covers, selected or not"`.
WHY — `agents/decisions/the-selection-lift.md`

## Do not assume

- That the band is at the bottom, or that canvas row 0 is empty — the selector and the setup's
  status are the first rows; the notice and the legend are the last (WL-FRONT-03).
