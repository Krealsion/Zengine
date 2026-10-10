# Input

**Input turns the keyboard and the pointer into messages. One Input weave reads the terminal, the
Windows console or an SDL window and publishes each key press and release, each piece of typed
text and each pointer move, click and wheel turn; your weave reacts by accepting those messages.**

A key says which key changed and which modifiers were held; typed text is what the keyboard
layout actually produced. A program spells the messages with `#include "input/vocabulary.hpp"`
and links `zengine::input`.

## Pages

- [The Input package](docs/input.md): the reference, with every message, what it carries and
  which backend produces it, plus input sessions and timed pointer motion

## What is in this folder

| | |
|---|---|
| `vocabulary.hpp` | the messages, key codes, modifiers and pointer spaces |
| `translate.hpp` | how terminal and console events become those messages |
| `translate_sdl.hpp` | how SDL window events become those messages |
| `input_weave.hpp` | the Input weave itself, over any reader |
| `input.cpp` | the terminal and console reader, built as the `zengine-input` weave |
| `input_sdl.cpp` | the window reader, built as `zengine-input-sdl` where SDL is available |
| `CMakeLists.txt` | the build |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
