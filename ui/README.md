# UI

**UI keeps where a person put something on screen apart from where it lands on a particular
screen. A program stores each element's position and size as they were authored, such as "4 cells
in, half the width", then resolves them against a screen's size to get the rectangles to draw and
to learn which element is under a given cell.**

A cell is one square of the grid a screen is drawn in. An element can be measured from another
element instead of from the whole screen, so moving one moves everything measured from it.
Resolution gives an answer for any values, and its result is never stored back on what was
authored. It is header-only: a program links `zengine::ui` and spells
`#include "ui/vocabulary.hpp"` for the authored side and `#include "ui/layout.hpp"` for the
resolved side.

## Pages

- [The UI package — authored and resolved](docs/ui.md): the reference — elements, sizes and
  frames, how they resolve, what is under a cell, and the compile-time check that keeps a
  resolved number out of an authored type

## What is in this folder

| | |
|---|---|
| `vocabulary.hpp` | what a person authors: an element's identity, label, frame, position and size |
| `layout.hpp` | what a screen makes of it: rectangles, a resolved scene, and what is under a cell |
| `CMakeLists.txt` | the `zengine-ui-vocabulary` library target, exported as `zengine::ui` |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
