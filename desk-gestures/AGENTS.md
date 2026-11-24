# `desk-gestures/` — The desk's gestures as a library: working on it

The router for `desk-gestures/`, a library: where its law, its suites, the host's side of it and
its practices are. It states no law. The repository's own rules are [the guide](../CONTRIBUTING.md).

## Parts

| | |
|---|---|
| `CMakeLists.txt` | `zengine-desk-gestures` (`zengine::desk-gestures`): a static library linking `zengine::graphics` and `zengine::input` |
| `geometry.hpp` | `PixelRect`, `CellRect`, `overlap`, `CanvasPoint`, `PointedAt`, `canvas_point_of`, `pane_edge`, `pane_edge_at`, `pane_edge_cell`, the snap reach's bounds, `saturating_step`, `saturating_minus` |
| `gestures.hpp`, `gestures.cpp` | `PaneHold`, `hold_at`, `sized_to`, `moved_to`, `pane_window_proposal`, `snap_pane_window`, `TabClickMemory`, `doubles_a_tab_click`, `LayoutTabPress`, `TabPressAsk`, `tab_press`, `tab_drag_to`, `NamingCommit`, `naming_commit`, `nudged`, `grown`, `arrange_step_to`, `Selectable`, `reading_order`, `cycle_from`, `Toward`, `neighbour`, `ListWindow`, `list_window` |
| `keys.hpp` | `KeyContext`, `KeyModes`, `keyboard_context`, `keyboard_context_beneath_menu`, `default_row_context` |
| `menus.hpp` | `context_cursor_bound`, `menu_cursor_moved`, `MenuBack`, `menu_back`, `cursor_on_group`, `MenuChoice`, `menu_choice`, `MenuPress`, `kMaxMenuPresses`, `kept_presses`, `menu_refusal_is_ours`, `WithdrawnMenu`, `withdrawn_refused` |

## Law

The rules these functions keep are Workshop's, in the registers
[workshop/AGENTS.md](../workshop/AGENTS.md) routes: geometry (where a pointer lands, an edge's
band), arrangement and its snap (a pull, a move, a nudge and a snap), pointer (a tab pressed
twice), keyboard (where the keys go), focus (the selection moved) and the pane's menu (its
custody).

## Suites

- `desk_gestures` — each transition as a value: `tests/test_desk_gestures.cpp`, its own executable
  linking only `zengine-desk-gestures`

Workshop's suites prove the wiring: `workshop_screen`, `workshop_panes_hand`, `workshop_host` and
the suites that press, drag, key and open menus through Workshop, which calls these functions.

## Host side

- `workshop/CMakeLists.txt` links it into `zengine-workshop-vocabulary`. `workshop/screen.hpp`
  names its geometry and gestures in Workshop's namespace, and gives `snap_pane_window` the limits
  a pane's row may author (`kPanePxMin`, `kMaxPanePixels`); `workshop/keymap.hpp` names
  `KeyContext`, and `workshop/context.hpp` the menu's presses and a withdrawn menu's record.
- `workshop/weave_arrange.cpp` takes a pane's hold (`take_pane_hold`), follows its motion
  (`arrange_motion`), and nudges, grows and steps by key; `workshop/weave_pointer.cpp` presses
  and drags the tabs and walks the menu; `workshop/weave_session.cpp` commits the name line;
  `workshop/weave_external.cpp` keeps the pane's menu in custody; `workshop/screen_arrange.cpp`
  gathers the routing chain's facts (`KeyModes`) and the selectable panes from the desk as laid
  out.

## Practices

- [The host](../docs/contributing/best-practices.md#the-host-workshop)
