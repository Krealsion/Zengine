# The maker weave

**A weave (a part of a program that keeps a state and talks in messages over the Loom's bus)
built from a weaver's definition, which is data rather than a C++ class: the state it keeps, the
messages it accepts and sends, and the rules that update the state when a message arrives. A
program registers one while it runs, can change its rules live, and can change the shape of its
state with a written conversion that carries the old state across.**

It is header-only. A program links the `zengine::maker` target and spells a header from the
repository root, for example `#include "maker/weave.hpp"`. [Flow](../flow/README.md) is where a
weaver draws or types a definition; it builds on this package.

## Pages

- [The maker weave — a weave from a definition](docs/maker-weave.md): the reference — the
  definition and state files, what a rule does when a message arrives, and the two ways a running
  definition is changed.

## What is in this folder

| | |
|---|---|
| `definition.hpp` | the definition and the state as data, and the checks a definition must pass |
| `weave.hpp` | runs a definition as a weave, and swaps in new rules over the same state |
| `runtime.hpp` | what a maker weave does with each message it receives |
| `succession.hpp` | replaces a running weave whose state changes shape, converting the old state |
| `vocabulary.hpp` | the messages that replacement is carried out in |
| `write.hpp` | writes one value's fields into another, for a sent message or a conversion |
| `files.hpp` | reads and writes the definition and state files without risking the last good copy |
| `CMakeLists.txt` | the `zengine-maker` library target |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
