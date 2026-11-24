# Input

**Input turns the keyboard and the pointer into messages. The Input weave reads the terminal or
the Windows console and publishes each key press and release, each piece of typed text and each
pointer move, click and wheel turn; your weave reacts by accepting those messages.**

A key says which key changed and which modifiers were held; typed text is what the keyboard
layout actually produced. A test or an agent can speak for a person through an input session the
role opens, and its moments are published as the same messages. A program spells the messages with
`#include "input/vocabulary.hpp"` and links `zengine::input`.

## Manuals

- [Input](docs/zengine.input.md), `zengine.input`: the role the Input weave holds, what it
  publishes, how it reads beside each window, and the input session through which an agent or a
  test speaks for a person

## Pages

- [The Input package](docs/input.md): the vocabulary, with every message, what it carries and
  what each platform can vouch for, the backends' rules, and the weave a reader is wrapped in

## Parts

| file | what it holds |
|---|---|
| `vocabulary.hpp` | the contract: the moments (keys, text, pointer), the key codes (`scan::`), the modifiers (`mod::`), the pointer spaces (`space::`), the input sessions and `kInputRole`; installed |
| `gesture_grammar.hpp` | `Gesture` and its grammar: how a key is written and read back, as the keymap and a theme's keys spell it |
| `translate.hpp` | POSIX terminal and Win32 console events turned into moments, as pure code |
| `translate_sdl_core.hpp` | SDL events turned into moments, as pure code, for the GUI window, which reads SDL's queue |
| `window_claim.hpp` | a window's claim on the terminal's input, or on SDL's queue: the reader yields the terminal to either |
| `input_weave.hpp` | the Input weave over any reader it is given: publishing, the beat, the sessions, motion and its facets |
| `terminal_reader.hpp` | the terminal and console readers, the platform's input edge |
| `role_page.hpp` | `RolePage`, the role's manual both loadable weaves answer from; not installed |
| `input.cpp` | the loadable weave `zengine-input`, over the platform's reader |
| `double_click_setting.hpp` | the double-click time the role holds, and how the layouts' owner keeps it |
| `CMakeLists.txt` | `zengine-input-vocabulary` (header-only, aliased `zengine::input`) and the weave, which carries the manual |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`input/` adds to it.

**Law.** The folder has no law page of its own. Its contract is its manual,
[`zengine.input`](docs/zengine.input.md), and its package reference,
[The Input package](docs/input.md), which holds its backends' rules too. How a key is spelled, in
`gesture_grammar.hpp`, keeps Workshop's keymap laws, in
[the key-spelling register](../agents/workshop/key-spelling.md) and
[the keyboard register](../agents/workshop/keyboard.md), which Workshop's router routes; each
comment names the law it keeps.

**The manual is compiled in.** `zengine-input` carries `docs/zengine.input.md` and answers it when
asked, so an edit to the manual is a build: the `input` suite and the manual witnesses hold it to
the messages the role accepts, and the `manual` check holds its form.

**Suites.** Each runs through the official lane, `tests/verify.cmake`.

| suite | what it proves | where |
|---|---|---|
| `input` | the contract, both backends, the weave, the beat and the sessions; the SDL translation's cases run where SDL is built | `tests/test_input.cpp` |

Shared suites that run the real Input weave: `workshop_guests` (`tests/test_workshop_guests.cpp`)
and `workshop_probe` (`tests/test_workshop_probe.cpp`). `workshop_panes_hand`
(`tests/test_workshop_panes_input.cpp`) is Workshop's side: a press and a key reaching a pane.

**The host's side.**

- Staging and loading: `workshop/CMakeLists.txt` stages `zengine-input`, and
  `workshop/default-load-plan.json` loads it as `zengine.input` beside either window. A plan
  naming the retired `zengine-input-sdl` is refused by name (`realization/owner.hpp`).
- Guests: `workshop/guests.cpp` lets a guest with the `input` power send the session shapes to
  `kInputRole`, and `workshop/guest_door.hpp` closes a departed guest's session on its behalf.
- Users: `workshop/weave_operation.cpp` accepts `AttributedInput` for an operation's authority, and
  `workshop/keymap.hpp`, `pane/escape.hpp` and `workshop/screen.hpp` read the vocabulary.
- Beyond Workshop: `graphics/window/terminal_io.hpp` turns terminal pointer reporting on; the
  readers in `terminal_reader.hpp` are the TUI window's input edge too
  (`graphics/window/window.cpp`); the GUI window translates SDL's queue with
  `translate_sdl_core.hpp` and claims it with `window_claim.hpp`
  (`graphics/window/window_sdl_backend.hpp`); and the snake's world takes the keys it is given
  (`examples/snake/world.cpp`). `cmake/ZengineInstall.cmake` installs the headers and
  `zengine-input`.

**Practice.** [Input](../docs/contributing/best-practices.md#input-input).
