# `desk-gestures/` — The desk's gestures as a library: working on it

The router for `desk-gestures/`, a library: where its law, its suites, the host's side of it and
its practices are. It states no law. The repository's own rules are [the guide](../CONTRIBUTING.md).

## Parts

| | |
|---|---|
| `CMakeLists.txt` | `zengine-desk-gestures` (`zengine::desk-gestures`): a static library linking `zengine::graphics` and `zengine::input` |
| `geometry.hpp` | `PixelRect`, `CellRect`, `CanvasPoint`, `PointedAt`, `canvas_point_of`, `pane_edge`, `pane_edge_at`, `pane_edge_cell`, the snap reach's bounds, `saturating_step`, `saturating_minus` |
| `gestures.hpp`, `gestures.cpp` | `PaneHold`, `hold_at`, `sized_to`, `moved_to`, `pane_window_proposal`, `snap_pane_window`, `TabClickMemory`, `doubles_a_tab_click`, `ListWindow`, `list_window` |

## Law

The rules these functions keep are Workshop's, in the registers
[workshop/AGENTS.md](../workshop/AGENTS.md) routes: geometry (where a pointer lands, an edge's
band), arrangement and its snap (a pull, a move and a snap), and pointer (a tab pressed twice).

## Suites

- `desk_gestures` — each transition as a value: `tests/test_desk_gestures.cpp`, its own executable
  linking only `zengine-desk-gestures`

Workshop's suites prove the wiring: `workshop_screen` and `workshop_panes_hand` press, drag and
release a pane through Workshop, which calls these functions.

## Host side

- `workshop/CMakeLists.txt` links it into `zengine-workshop-vocabulary`; `workshop/screen.hpp`
  names its types and functions in Workshop's namespace, and gives `snap_pane_window` the limits a
  pane's row may author (`kPanePxMin`, `kMaxPanePixels`).
- `workshop/weave_arrange.cpp` takes a pane's hold (`take_pane_hold`) and follows its motion
  (`arrange_motion`) through `hold_at`, `sized_to` and `moved_to`.

## Practices

- [The host](../docs/contributing/best-practices.md#the-host-workshop)
