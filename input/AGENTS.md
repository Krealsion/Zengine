# `input/` — Input: working on it

The router for `input/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `vocabulary.hpp` | the contract: moments, `scan::`, `mod::`, `space::`, sessions, `kInputRole` |
| `translate.hpp` | POSIX terminal and Win32 console events to moments, as pure code |
| `translate_sdl_core.hpp` | SDL events to moments, pure, for both readers of SDL's queue |
| `translate_sdl.hpp` | the SDL reader's events over it; the one file using the Surface vocabulary |
| `window_claim.hpp` | a window's claim on the terminal's input or SDL's queue, which a reader yields to |
| `input_weave.hpp` | the weave over an injected reader: publish, beat, sessions, motion, facets |
| `office_page.hpp` | `OfficePage`: the office manual both loadable weaves answer from; not installed |
| `terminal_reader.hpp` | the terminal and console readers, the platform's input edge |
| `input.cpp` | loadable weave `zengine-input` over the platform's reader |
| `input_sdl.cpp` | the SDL queue's reader unless an OS window claims it; loadable weave `zengine-input-sdl`, where SDL is |
| `CMakeLists.txt` | `zengine-input-vocabulary` (header-only, alias `zengine::input`), both weaves, each carrying the manual |
| `docs/zengine.input.md` | the manual of the office both weaves hold |
| `docs/input.md` | the package reference: the vocabulary, the backends, the weave over a reader |

## Law

No register of its own. Its law is its manual, [`zengine.input`](docs/zengine.input.md), and its
package reference, [The Input package](docs/input.md);
its backends' rules are in the Surface register, `agents/surface.md`, which `surface/` routes.

## Suites

- `input` — contract, both backends, the weave, beat and sessions: `tests/test_input.cpp`

The SDL reader's cases in `input` run under the `sdl` gate. Shared suites that run the real
Input weave: `workshop_guests` (`tests/test_workshop_guests.cpp`) and `workshop_probe`
(`tests/test_workshop_probe.cpp`); `workshop_panes` (`tests/test_workshop_panes_input.cpp`)
is Workshop's side, a press and a key reaching a pane.

## Host side

- Staging: `workshop/CMakeLists.txt` stages `zengine-input`, and `zengine-input-sdl` where built.
- Loading: `workshop/default-load-plan.json` loads `zengine-input` as `zengine.input`;
  `workshop/graphical-load-plan.json` loads `zengine-input-sdl` in that role.
- Guests: `workshop/guests.cpp` lets a guest with the `input` power send the session shapes to
  `kInputRole`; `workshop/guest_door.hpp` closes a gone guest's session on its behalf.
- Consumers: `workshop/weave_operation.cpp` accepts `AttributedInput` for operation authority;
  `workshop/keymap.hpp`, `workshop/pane_escape.hpp` and `workshop/screen.hpp` read the vocabulary.
- Beyond Workshop: `surface/skin_tui.hpp` turns terminal pointer reporting on;
  `cmake/ZengineSdl.cmake` shares one SDL with the window Skin; `cmake/ZengineInstall.cmake`
  installs the headers and `zengine-input`; `input/terminal_reader.hpp`'s readers are the
  terminal window's input edge too (`graphics/window/window.cpp`); the OS window translates SDL's
  queue with `input/translate_sdl_core.hpp` and claims it with `input/window_claim.hpp`
  (`graphics/window/window_sdl_backend.hpp`); and the snake's world consumes the keys it is given
  (`examples/snake/world.cpp`).

## Pages

- [`zengine.input`](docs/zengine.input.md), the manual
- [The Input package](docs/input.md)

## Practices

- [Input](../docs/contributing/best-practices.md#input-input)
