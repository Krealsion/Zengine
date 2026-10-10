# Surface

**Surface is how a weave shows things. A weave publishes what it wants drawn — rectangles, labels
and regions of text on a canvas, or a named line of text — and a Skin, a replaceable weave that
owns the screen, paints it in a terminal or in a real window.**

Each element names a role, such as accent or alert, rather than a colour, so one picture reads in
a monochrome terminal and in a window alike. A program spells the messages with
`#include "surface/vocabulary.hpp"` and links `zengine::surface`. An installed Zengine also
carries `surface/region.hpp`, `surface/cells.hpp`, `surface/pointing.hpp` and
`surface/terminal_size.hpp`, and the two terminal Skins.

## Pages

- [The Surface package](docs/surface.md): the reference, with every message, which kind of text
  to choose, the drawing order, what each Skin does with them, captures and the clipboard

## What is in this folder

| | |
|---|---|
| `vocabulary.hpp` | the messages: what a weave publishes to be drawn, and what the Skin reports back |
| `region.hpp`, `cells.hpp` | how much text a region holds, and the arithmetic every Skin shares |
| `pointing.hpp` | where a pointer lands on the canvas ([Pointer spaces](../workshop/docs/pointer-spaces.md)) |
| `terminal_size.hpp` | how big the terminal is |
| `skin.hpp` | what every Skin has in common |
| `skin_tui.hpp`, `skin_tui_classic.cpp`, `skin_tui_block.cpp` | the two terminal Skins, `zengine-skin-tui-classic` and `zengine-skin-tui-block` |
| `skin_sdl.cpp`, `skin_sdl_plan.hpp`, `skin_sdl_text.hpp`, `skin_sdl_glyphs.hpp` | the window Skin, `zengine-skin-sdl`, built where SDL is available |
| `fonts/` | the typeface the window Skin carries, with its licence and where it came from |
| `CMakeLists.txt` | how it is built |
| `docs/` | these pages and their screenshots |

Working on it: [AGENTS.md](AGENTS.md).
