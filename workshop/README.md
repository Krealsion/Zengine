# Workshop

**Workshop is an interactive environment for making things with Zengine: a desk of panes, in a
terminal or a window, where a weaver (a person who writes and runs weaves) edits, builds, loads
and inspects a running program without leaving it.** It is optional: the
[C++ library](../docs/getting-started.md) works without it.

Almost every pane is a tool, a weave Workshop loads. The desk's own Pane Manager, Hotkeys pane
and menus are tools too, and you can change or replace them while Workshop runs.

## Pages

- [Getting started with Workshop](docs/getting-started.md): launch it and find your way around
- [Panes](docs/panes.md): open, close, move, resize and order panes
- [Hotkeys and the keymap](docs/hotkeys.md): what a key means, and how to change a binding
- [Setups and workspace continuity](docs/setups.md): how your desk is kept and comes back
- [Ready-to-use setups](docs/demo-setups.md): one command prepares a Workshop for a task
- [Choosing what a run is made of](docs/load-plans.md): use and write a load plan
- [Make a Workshop tool](docs/make-a-workshop-tool.md): write a pane of your own
- [Develop Workshop from inside Workshop](docs/develop-workshop.md): edit a shipped pane live
- [Current limitations](docs/limitations.md): what does not work yet
- [The authored load plan](docs/load-plan.md): the load plan file, in full
- [Workshop panes and setups — reference](docs/workshop-panes.md): the contracts behind panes
- [Pointer spaces — reference](docs/pointer-spaces.md): where a pointer position lands

## What is in this folder

| | |
|---|---|
| `desktop-pane/` | the Pane Manager and the Hotkeys pane: the desk's default behaviour |
| `menu-presenter/` | the tool that shows a pane's menu; you can replace it |
| `default-load-plan.json`, `graphical-load-plan.json` | the shipped load plans: terminal, windowed |
| `pane_vocabulary.hpp` and its helpers | the pane protocol a tool speaks, installed: `#include "workshop/pane_vocabulary.hpp"`, linking `zengine::pane` |
| `workshop.cpp` and the rest | the Workshop program |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
