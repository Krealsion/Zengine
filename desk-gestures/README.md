# Desk gestures

**The desk's geometry and each of its gestures as a function: a rectangle on the canvas, where a
pointer landed and which edge of a pane it is on; and what a press, a motion, a key and a snap do,
from what the hand holds to what it holds next and what it asks for. Nothing here holds state or
authority.**

A host keeps the wiring: it reads the event, calls the transition, stores what comes back and says
and sends what it asks. Workshop is that host for the desk. A program links the
`zengine::desk-gestures` target and spells a header from the repository root, for example
`#include "desk-gestures/gestures.hpp"`.

## What is in this folder

| | |
|---|---|
| `geometry.hpp` | `PixelRect`, `CellRect`, the pixels two rectangles share (`overlap`), `CanvasPoint`, `PointedAt` and `canvas_point_of`; a pane's eight edges, their names and marks, and which one a pointer is on; the saturating arithmetic every gesture computes with |
| `gestures.hpp` | a pane taken by an edge or by its body (`hold_at`), sized (`sized_to`) or moved (`moved_to`), and snapped to the lines in reach (`snap_pane_window`); a press on the layout tabs (`tab_press`), a double among them, and a held tab carried (`tab_drag_to`); a name line's commit (`naming_commit`); the keyboard's nudge, grow and step (`nudged`, `grown`, `arrange_step_to`); the selection walked in reading order and moved in a direction (`reading_order`, `cycle_from`, `neighbour`); what a bounded list shows of its members |
| `keys.hpp` | where the keys go: `KeyContext`, the facts the routing chain reads (`KeyModes`), and the chain (`keyboard_context`) |
| `menus.hpp` | a menu walked and chosen (`menu_cursor_moved`, `menu_back`, `cursor_on_group`, `menu_choice`); a pane's menu in custody: the presses it keeps, whose refusal is whose, and which withdrawn menu a refusal is about |
| `gestures.cpp` | the bodies of `geometry.hpp`'s and `gestures.hpp`'s functions; `keys.hpp` and `menus.hpp` are whole in their headers |
| `CMakeLists.txt` | the `zengine-desk-gestures` library target |

Working on it: [AGENTS.md](AGENTS.md).
