# Workshop law — planes

Register `WL-FRONT`: the plane sequence, the three vertical regions, and the selection lift.
One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

Retired: WL-FRONT-01, WL-FRONT-02, WL-FRONT-03, WL-FRONT-06, WL-FRONT-07

## WL-FRONT-04 — `Panes::selected` is a press's memory

LAW — The selection is a press's memory: session-only, never persisted, none at start, resolved to a pane by one reader; it has five writers and no other.

MEANS
- the press line (in a window also `on(AreaPressed)`), `enter_arrange_pane` once admitted, a pane's reveal, Escape's fallthrough, a launch without the keys letting another's lift go;
- a refused Arrange leaves the selection exactly where it was.

WHY — `agents/decisions/the-selection-lift.md`

## WL-FRONT-05 — `effective_pane_order` is the one foreground order

LAW — The authored permutation with the selected pane lifted is the one answer, and every consumer meaning "in front right now" spends it; `presentation_order` is the authored base.

MEANS
- `paint_panes` ascending, `occupied_at` descending, `pane_is_covered`, the desk's pointer walk;
- persistence and `reset order` want the authored base, and nothing else may.

WHY — `agents/decisions/the-selection-lift.md`

## Do not assume

- That the band is at the bottom, or that canvas row 0 is empty — the selector and the setup's
  status are the first rows; the notice and the legend are the last.

## Proven by

### WL-FRONT-04
`workshop/panes.hpp` Panes::selected selected_pane kNoPaneKind
`workshop/weave_desktop.cpp` launch_pane
`workshop/weave_arrange.cpp` enter_arrange_pane
`workshop/weave_external.cpp` unselect_pane
`workshop/weave_seam.cpp` on(PaneRevealRequested) revealed
`tests/test_workshop_screen.cpp` "the selection lift never reaches the file, and no session starts
with one" "contextual Arrange lifts the pane it addressed, not the one in front" "every pane a
weaver can point at can be arranged, and the refusals are blind"
`workshop/weave_handlers.cpp` on(AreaPressed)

### WL-FRONT-05
`workshop/setup_reconcile.hpp` effective_pane_order presentation_order
`workshop/screen_compose.cpp` paint_panes
`workshop/screen_chrome.cpp` occupied_at
`workshop/screen_pane_state.cpp` pane_is_covered
`workshop/panes.hpp` selected_pane
`workshop/weave_arrange.cpp` arrange_press
`tests/test_workshop_screen.cpp` "selecting a pane lifts it, in the picture and under the hand at
once" "the arrangement desk's pointer takes what is visibly in front"
`tests/test_workshop_panes_window.cpp` "hit order is the exact reverse of paint order"
