# Desk gestures

**The desk's geometry and each of its gestures as a function: a rectangle on the canvas, where a
pointer landed and which edge of a pane it is on; and what a press, a motion and a snap do, from
what the hand holds to what it holds next and what it asks for. Nothing here holds state or
authority.**

A host keeps the wiring: it reads the event, calls the transition, stores what comes back and says
and sends what it asks. Workshop is that host for the desk. A program links the
`zengine::desk-gestures` target and spells a header from the repository root, for example
`#include "desk-gestures/gestures.hpp"`.

## What is in this folder

| | |
|---|---|
| `geometry.hpp` | `PixelRect`, `CellRect`, `CanvasPoint`, `PointedAt` and `canvas_point_of`; a pane's eight edges, their names and marks, and which one a pointer is on; the saturating arithmetic every gesture computes with |
| `gestures.hpp` | a pane taken by an edge or by its body (`hold_at`), sized (`sized_to`) or moved (`moved_to`), and snapped to the lines in reach (`snap_pane_window`); a press on a layout tab that doubles the one before; what a bounded list shows of its members |
| `gestures.cpp` | their bodies |
| `CMakeLists.txt` | the `zengine-desk-gestures` library target |

Working on it: [AGENTS.md](AGENTS.md).
