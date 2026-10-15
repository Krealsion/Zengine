# Graphics

**Graphics is how a participant draws into a window. It draws nodes -- the one primitive, an
operator and its operands -- or says an element once and sends only its changes; the window that
shows its area keeps the picture, paints it with the area's skin, and writes only what changed.**

A program spells the messages with `#include "graphics/vocabulary.hpp"` and draws with
`graphics/drawing.hpp` or `graphics/canvas.hpp`, linking `zengine::graphics`. The snake
(`examples/snake/`) is the worked example: its frame drawn as nodes, its board an element of the
game's own.

## Pages

- [`zengine.window.terminal`](docs/zengine.window.terminal.md), the terminal window's manual: what
  it keeps and shows, what an owner sends it and what it refuses, where a key and a press go, the
  libraries it opens, and what a reload or a replacement costs
- [`zengine.graphics`](docs/zengine.graphics.md), the directory's manual: what the arranger and a
  window tell it, and what it says to each window
- [The Graphics package](docs/graphics.md): drawing into a window -- who says what, the node and the
  element record, the seven operators, drawing with a canvas, a drawing weave, an element of your
  own, and the two seams

## What is in this folder

| | |
|---|---|
| `vocabulary.hpp` | the messages: the node, the element record, drawing, elements, the directory, a window's report |
| `ops.hpp` | the seven operators and their operands, and a `Paint` |
| `implied.hpp` | a value's body without its envelope, where an operator or a type implies its shape |
| `canvas.hpp` | what an owner draws with: a `Sketch`, a `Canvas`, `difference` |
| `drawing.hpp` | a drawing weave: the room's handshake and the showing, so a drawer writes its picture |
| `painting.hpp` | what crosses the two seams: a paint task and its result, an element's event and its outcome |
| `painter_abi.h`, `painter_export.hpp`, `term_paint.hpp` | the skin seam, its export, and the painter for a medium of characters |
| `element_abi.h`, `element_export.hpp` | the element seam and its export |
| `window/` | the terminal window, `zengine-window-terminal`: its store, its picture, its seams, its weave |
| `directory.cpp` | the directory, `zengine-graphics` |
| `paint_classic.cpp`, `paint_block.cpp` | the classic and block skins, `zengine-paint-classic` and `zengine-paint-block` |
| `CMakeLists.txt` | how it is built |
| `docs/` | the two weaves' manuals and this package's page |

Working on it: [AGENTS.md](AGENTS.md).
