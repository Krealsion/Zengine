# The Editor

**The Editor is the Workshop pane where a weaver (a person who writes and runs weaves) opens one
source file, edits it, saves exactly what they meant to save, and builds and loads the result
without leaving Workshop. Neovim can take the Editor's place, and a weaver can switch between the
two while working, taking unsaved edits, the caret and the selection along.**

Both Editors take part in Workshop's drag and drop: they carry out a copy of a selection or of a
file's place, to Inventory or another pane, and take text, commands and file places in. Nothing
dropped on an Editor is run. A program of your own can make and recognize that material with
`#include "editor/source-transfer/vocabulary.hpp"` (CMake target `zengine::source-transfer`), and
start, ask after and stop the Neovim-backed Editor with
`#include "editor/neovim-editor/vocabulary.hpp"` (`zengine::neovim`).

## Pages

- [The source editor](docs/editor.md): open a file, edit it, save it, carry text in and out, and
  what the Editor does with every byte
- [Neovim in Workshop](docs/neovim.md): edit in Neovim, choose its configuration, switch while
  you work, and run it from a Loom with no Workshop
- [Editor switch](docs/editor-switch.md): the reference for switching: the choices a load plan
  offers, the messages a weaver sends, and what crosses with the document
- [Source material](docs/source-transfer.md): the reference for the text, commands and file
  places the Editors carry to and from Inventory

## What is in this folder

| | |
|---|---|
| `editor-pane/` | the standard Editor, a weave that Workshop loads, and the document it holds |
| `neovim-editor/` | the Neovim-backed Editor, a weave too, and the requests it answers |
| `neovim/` | how a Neovim is started, spoken to and shown in a pane |
| `source-transfer/` | the text, commands and file places the Editors carry, and how each is read |
| `docs/` | these pages and their screenshots |

Working on it: [AGENTS.md](AGENTS.md).
