# Input

**Input turns the keyboard and the pointer into messages. The Input weave reads the terminal or
the Windows console and publishes each key press and release, each piece of typed text and each
pointer move, click and wheel turn; your weave reacts by accepting those messages.**

A key says which key changed and which modifiers were held; typed text is what the keyboard
layout actually produced. A program spells the messages with `#include "input/vocabulary.hpp"`
and links `zengine::input`.

## Manuals

- [Input](docs/zengine.input.md), `zengine.input`: the office the Input weave holds, what it
  publishes, how it reads beside each window, and the input session through which an agent or a
  test speaks for a person

## Pages

- [The Input package](docs/input.md): the vocabulary, with every message, what it carries and
  what each platform can vouch for, and the weave a reader is wrapped in

## What is in this folder

| | |
|---|---|
| `vocabulary.hpp` | the messages, key codes, modifiers and pointer spaces |
| `translate.hpp` | how terminal and console events become those messages |
| `translate_sdl_core.hpp` | how SDL window events become those messages, for the OS window |
| `input_weave.hpp` | the Input weave itself, over any reader |
| `terminal_reader.hpp` | the terminal and console readers |
| `window_claim.hpp` | a window's claim on the terminal, which the reader leaves to it, or SDL's queue |
| `office_page.hpp` | the office's manual, as the shipped weave answers from it |
| `input.cpp` | the terminal and console reader, built as the `zengine-input` weave |
| `CMakeLists.txt` | the build |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
