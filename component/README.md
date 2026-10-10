# Component

**The pieces a pane (one panel of a tool's window) is built from: an editable line of text, a
window onto a long list, a record of what each drawn row means, a choice that stays put while
its list changes, a strip of labelled controls, table columns, and paths for things that move.
Each piece keeps its own state and knows nothing about the screen that shows it.**

It is header-only and depends on nothing. A program links the `zengine::component` target and
spells a header from the repository root, for example `#include "component/text_box.hpp"`.

## Pages

- [The Component package](docs/component.md): the reference — each piece, what it owns, and what
  it leaves to the program using it.

## What is in this folder

| | |
|---|---|
| `text_box.hpp` | a one-line text box: caret, selection, copy and paste, and undo |
| `list_window.hpp` | which part of a list fits the space a pane has, and how much is left out |
| `row_map.hpp` | what each row and column span a pane drew means, so a press lands on it |
| `held_choice.hpp` | a chosen row remembered by identity while the list around it changes |
| `control_strip.hpp` | a row of labelled controls packed into the width available |
| `columns.hpp` | a table's columns, laid out once so every row lines up |
| `motion.hpp` | straight and curved paths, and progress over elapsed time |
| `CMakeLists.txt` | the `zengine-component` library target |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
