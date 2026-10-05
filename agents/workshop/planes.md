# Workshop law — planes

Register `WL-FRONT`: the plane sequence, the three vertical regions, and the selection lift.
One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-FRONT-01 — The plane sequence is the layout of the screen

LAW — The canvas is published in one depth order: one plane per pane in `effective_pane_order` ascending, the affordances, the overlays, the foot band; nothing is painted beneath the panes.

MEANS
- an overlapping pane is painted where it is hit, in both front orders;
- a provider's text cannot bury the contextual surface drawn over it;
- a room cell no pane covers carries no mark: each medium shows its own ground there.

PROVEN BY — `workshop/screen_compose.cpp` `paint`, `paint_panes`, `band_region`;
`workshop/screen_reveal.cpp` `paint_pane_affordances`; `workshop/screen.hpp` `on_own_layer`;
`workshop/setup.hpp` `effective_pane_order`; `tests/test_workshop_screen.cpp` case `"an
overlapping pane is painted where it is hit, in both front orders"`, case `"a visible pane
occupies the pointer space it covers"`, case `"nothing is painted behind the panes: where no pane
stands, the room is the medium's own ground"`; `tests/test_workshop_panes_seam.cpp`
case `"an external pane's own text cannot bury the surface that recovers it"`.
WHY — `agents/decisions/front-is-a-permutation.md`

## WL-FRONT-02 — The foot band is in front of the panes and owns no pointer space

LAW — The foot band owns its whole rectangle (`kGroundOwn`) and occupies no pointer space; the top rows belong to a pane, so nothing paints in front of a pane while hitting nothing but tabs.

MEANS
- a band is where the tool speaks: a pane authored over it is covered by it;
- a pane in front of the Layouts pane takes the press, and the Layouts pane reads `covered`.

PROVEN BY — `workshop/screen_compose.cpp` `band_region`; `workshop/screen.hpp` `band_bounds`;
`workshop/screen_chrome.cpp` `occupied_at`; `surface/vocabulary.hpp` `kGroundOwn`;
`tests/test_workshop_screen.cpp` case `"a pane in front of the Layouts pane takes the press"`;
`tests/test_workshop_document.cpp` case `"two bands compose their budgets, and
the selector is row 0"`.
WHY — `agents/decisions/two-bands.md`

## WL-FRONT-03 — Three regions tile the screen exactly

LAW — Three regions tile the screen exactly: the top band is reserved (the Layouts pane's default), the body follows, and the bottom band is the foot; each band is fitted to its rows (`WL-GEO-05`).

MEANS
- `room_h` never moved: chrome that moves must not resize a weaver's document;
- the reserved rows are two because the Layouts pane's identity and tabs are two rows;
- slots, the side region, the overlay column and occupancy all begin at `Screen::room_y`.

PROVEN BY — `workshop/screen.hpp` `kTopRows`, `kBottomRows`, `Screen::room_y`, `Screen::room_h`,
`band_bounds`, `top_band_bounds`; `tests/test_workshop_screen.cpp` case `"the layout selector is
the first Workshop row, on both media"`, case `"the move re-homed reserved rows and
did not add one"`, case `"every owner of the body agrees about
where it begins"`.
WHY — `agents/decisions/two-bands.md`

## WL-FRONT-04 — `Panes::selected` is a press's memory

LAW — The selection is a press's memory: session-only, never persisted, none at start, resolved to a pane by one reader; it has four writers and no other.

MEANS
- the press line, `enter_arrange_pane` after admission, a pane's reveal, Escape's fallthrough;
- a refused Arrange leaves the selection exactly where it was.

PROVEN BY — `workshop/panes.hpp` `Panes::selected`, `selected_pane`, `kNoPaneKind`;
`workshop/weave_arrange.cpp` `enter_arrange_pane`; `workshop/weave_external.cpp` `unselect_pane`;
`workshop/weave_seam.cpp` `on(PaneRevealRequested)`; `tests/test_workshop_screen.cpp` case
`"the selection lift never reaches the file, and no session starts with one"`, case
`"contextual Arrange lifts the pane it addressed, not the one in front"`, case `"every pane a
weaver can point
at can be arranged, and the refusals are blind"`.
WHY — `agents/decisions/the-selection-lift.md`

## WL-FRONT-05 — `effective_pane_order` is the one foreground order

LAW — The authored permutation with the selected pane lifted is the one answer, and every consumer meaning "in front right now" spends it; `presentation_order` is the authored base.

MEANS
- `paint_panes` ascending, `occupied_at` descending, `pane_is_covered`, the desk's pointer walk;
- persistence and `reset order` want the authored base, and nothing else may.

PROVEN BY — `workshop/setup.hpp` `effective_pane_order`, `presentation_order`;
`workshop/screen_compose.cpp` `paint_panes`; `workshop/screen_chrome.cpp` `occupied_at`;
`workshop/screen_pane_state.cpp` `pane_is_covered`; `workshop/panes.hpp` `selected_pane`;
`workshop/weave_arrange.cpp` `arrange_press`; `tests/test_workshop_screen.cpp` case `"selecting a
pane lifts it, in the picture and under the hand at once"`, case `"the arrangement
desk's pointer takes what is visibly in front"`;
`tests/test_workshop_panes_window.cpp` case `"hit order is the exact reverse of paint order"`.
WHY — `agents/decisions/the-selection-lift.md`

## WL-FRONT-06 — The lift is a rotation and never a write

LAW — No rank is read differently or written, `panes.open` is untouched, nothing reaches a file, and a selection that is not seated lifts nothing; `manage.front` is the permanent statement.

PROVEN BY — `workshop/setup.hpp` `effective_pane_order`, `presentation_order`;
`workshop/keymap.hpp` `manage.front`; `tests/test_workshop_screen.cpp` case `"the selection lift
never reaches the file, and no session starts with one"`;
`tests/test_workshop_panes_window.cpp` case `"ordering changes paint order and NOTHING else"`.
WHY — `agents/decisions/the-selection-lift.md`

## WL-FRONT-07 — The transient planes stay above the panes

LAW — The lift orders the ordinary pane planes among themselves and reaches no further, so a selected pane is never drawn over the menu a weaver just opened on it.

PROVEN BY — `workshop/screen_attention.cpp` `paint_context`; `workshop/screen_compose.cpp`
`paint_panes`, `paint`; `tests/test_workshop_screen.cpp` case `"a transient surface stays over
the pane it covers, selected or not"`.
WHY — `agents/decisions/the-selection-lift.md`

## Do not assume

- That the band is at the bottom, or that canvas row 0 is empty — the selector and the setup's
  status are the first rows; the notice and the legend are the last (WL-FRONT-03).
