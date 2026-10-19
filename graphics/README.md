# Graphics

**Graphics is how a participant draws into a window. It draws nodes -- the one primitive, an
operator and its operands -- or says an element once and sends only its changes; the window that
shows its area keeps the picture, paints it with the area's skin, and writes only what changed.**

A program spells the messages with `#include "graphics/vocabulary.hpp"` and draws with
`graphics/drawing.hpp` or `graphics/canvas.hpp`, linking `zengine::graphics`. The snake
(`examples/snake/`) is the worked example: its frame drawn as nodes, its board an element of the
game's own.

The common parts of a user interface come ready to draw, with `#include "graphics/elements.hpp"`:
a button, a checkbox, a list, a grid, an icon, an image, a scroll view and a text box. Each is a
small class the owner keeps and changes, and draws into its `Sketch`: its nodes at its path, and
its face (`ButtonFace`, `ListRowFace`, ...) as the record there, so a reader reads what the part is
and shows. A press on a part asks the owner for the action the owner named when it drew the part,
and sends nothing when it named none or the part is disabled. A part is drawn by its owner, unlike
an element the window presents from the owner's data (`element_export.hpp`).

## Pages

- [`zengine.window.terminal`](docs/zengine.window.terminal.md), the terminal window's manual: what
  it keeps and shows, what an owner sends it and what it refuses, where a key and a press go, the
  libraries it opens, and what a reload or a replacement costs
- [`zengine.window.sdl`](docs/zengine.window.sdl.md), the OS window's manual: the same window on
  the desktop, drawn in light through SDL3, and how it keeps its window across a reload
- [`zengine.graphics`](docs/zengine.graphics.md), the directory's manual: what the arranger and a
  window tell it, and what it says to each window
- [The Graphics package](docs/graphics.md): drawing into a window -- who says what, the node and the
  element record, the seven operators, drawing with a canvas, a drawing weave, an element of your
  own, the two seams, and what a window answers to C++ in its own process

## What is in this folder

| | |
|---|---|
| `vocabulary.hpp` | the messages: the node, the element record, drawing, elements, the directory, a window's report |
| `role.hpp` | the theme's roles: what a colour means, in every medium |
| `ops.hpp` | the seven operators and their operands, and a `Paint` |
| `implied.hpp` | a value's body without its envelope, where an operator or a type implies its shape |
| `canvas.hpp` | what an owner draws with: a `Sketch`, a `Canvas`, `difference` |
| `elements.hpp` | every part below, in one include |
| `elements/button.hpp`, `elements/checkbox.hpp` | a `Button` and a `Checkbox`, each with an action while enabled |
| `elements/list.hpp`, `elements/grid.hpp` | a `List` of the owner's rows, one chosen, a window of them shown; a `Grid` of the owner's cells, only the filled ones drawn; an action a row or a cell |
| `elements/icon.hpp`, `elements/image.hpp` | an `Icon` drawn by name from its table of outlines; an `Image`, a rectangle of a sheet of pixels, tinted |
| `elements/scroll_view.hpp`, `elements/text_box.hpp` | a `ScrollView`, a clipped box the owner draws in, and its bar; a one-line `TextBox` with its cursor |
| `drawing.hpp` | a drawing weave: the room's handshake and the showing, so a drawer writes its picture |
| `painting.hpp` | what crosses the two seams: a paint task and its result, an element's event and its outcome |
| `painter_abi.h`, `painter_export.hpp`, `term_paint.hpp`, `light_paint.hpp` | the skin seam, its export, and the painters for a medium of characters and of light |
| `element_abi.h`, `element_export.hpp` | the element seam and its export |
| `window/` | the windows: their store, their seams and the window over a medium; the terminal window, `zengine-window-terminal`, with the terminal's claim; the OS window, `zengine-window-sdl` |
| `directory.cpp` | the directory, `zengine-graphics` |
| `paint_classic.cpp`, `paint_block.cpp` | the classic and block skins, `zengine-paint-classic` and `zengine-paint-block`, for characters |
| `paint_day.cpp`, `paint_night.cpp` | the day and night skins, `zengine-paint-day` and `zengine-paint-night`, for light |
| `CMakeLists.txt` | how it is built |
| `docs/` | the weaves' manuals and this package's page |

Working on it: [AGENTS.md](AGENTS.md).
