# The activation cursor

**A small C++ helper a weave keeps to decide whether an arriving `zen.Activated` message is its own
to act on. The cursor accepts an activation only when the Loom vouched for it and, from the same
sender, it is newer than the last one accepted (another sender the Loom vouched for begins a new
lineage), so a weave does its once-per-activation work once, and a copy or a replay changes
nothing.**

It is header-only and needs no kernel. A program links `zengine::activation` and writes
`#include "activation/activation.hpp"`. A weave that uses the Timer binding has a cursor kept for
it ([the Timer binding](../timer/docs/timer-binding.md)).

## Pages

- [The activation cursor — reference](docs/activation.md): what `accept` checks, in what order and
  why, and how to carry an activation on a wire.

## What is in this folder

| | |
|---|---|
| `activation.hpp` | the cursor, `zengine::ActivationCursor` |
| `CMakeLists.txt` | its build target, `zengine::activation` |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
