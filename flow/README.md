# Flow

**Flow lets a weaver build a message-driven rule from the operators already on hand: the
messages it accepts, the state it keeps, the steps it runs and the messages it sends. A rule
runs without being compiled, can be changed while it runs and keep its state, and can be turned
into C++ that builds into a weave like any other.**

A weaver can work in it three ways: drawing a graph in Workshop's Flow pane, typing commands at
the standalone `zengine-flow` workbench, which needs no Workshop, or from a program of their own
that links the `zengine::flow` library. An installed header keeps its path from this repository,
for example `#include "flow/graph_edit.hpp"` or `#include "flow/flow-host/vocabulary.hpp"`.

## Pages

- [Author a running graph in Workshop](docs/flow.md): drawing, running and changing a graph in
  the Flow pane.
- [Make and run a Flow definition](docs/flow-guide.md): the standalone workbench, step by step.
- [Flow in an existing host](docs/flow-runtime.md): running Flow projects on a program's own bus
  and operators.
- [Flow authoring and native generation](docs/flow-reference.md): the reference for the library,
  its files and the C++ it generates.

## What is in this folder

| | |
|---|---|
| `flow-pane/` | the Flow pane, loaded into Workshop |
| `flow-host/` | runs a Flow project live on a host's own bus, as Workshop does |
| `main.cpp` | the standalone workbench |
| `shape.hpp`, `graph_edit.hpp`, `author.hpp` | message shapes, the graph and text commands |
| `project.hpp`, `workspace.hpp` | the saved project and the workspace a graph is edited in |
| `generate.hpp`, `native.hpp`, `compiled.hpp` | generating C++, and loading what it builds |
| `CMakeLists.txt` | the library and the workbench |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
