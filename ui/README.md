# UI

**UI keeps where a person put something on screen apart from where it lands on a particular
screen. A program stores each part's place and size as they were authored, in words such as "half
the width", "its own size", "what is left" or "the bottom-right corner, 10 pixels in", then solves
them for a screen's size to get the rectangles to draw, in whole pixels, and to learn which part
is under a given pixel.**

A part can stand in another part instead of in the whole screen, so moving one moves everything
standing in it, and a part can lay the parts standing in it down a column. Solving gives an answer
for any values, a terminal solving the same words at the size of its cells; its result is never
stored back on what was authored, and what no screen could solve -- a size that depends on itself
-- is found from the words alone. It is header-only: a program links `zengine::ui` and spells
`#include "ui/vocabulary.hpp"` for the authored side and `#include "ui/layout.hpp"` for the
solved side.

## Pages

- [The UI package — authored and solved](docs/ui.md): the reference — parts, their sizes and
  places in words, frames and columns, how they solve, what is under a pixel, what is wrong with
  the words alone, and the compile-time check that keeps a solved number out of an authored type

## What is in this folder

| | |
|---|---|
| `vocabulary.hpp` | what a person authors: a part's identity, label, frame, place and sizes, and what is wrong with them |
| `layout.hpp` | what a screen makes of it: the solve, rectangles, a solved scene, and what is under a pixel |
| `CMakeLists.txt` | the `zengine-ui-vocabulary` library target, exported as `zengine::ui` |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
