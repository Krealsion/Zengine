# `input/` — Input: working on it

The router for `input/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `vocabulary.hpp` | the contract: moments, `scan::`, `mod::`, `space::`, sessions, `kInputRole` |
| `translate.hpp` | POSIX terminal and Win32 console events to moments, as pure code |
| `translate_sdl.hpp` | SDL events to moments, pure; the one file using the Surface vocabulary |
| `input_weave.hpp` | the weave over an injected reader: publish, own beat, sessions, timed motion |
| `input.cpp` | the terminal and console reader; loadable weave `zengine-input` |
| `input_sdl.cpp` | the SDL queue's one reader; loadable weave `zengine-input-sdl`, where SDL is |
| `CMakeLists.txt` | `zengine-input-vocabulary` (header-only, alias `zengine::input`), both weaves |
| `docs/input.md` | the reference page |

## Law

No register of its own. Its law is its reference page, [The Input package](docs/input.md);
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
  installs the headers and `zengine-input`; `examples/snake/controls.cpp` consumes the keys.

## Pages

- [The Input package](docs/input.md)

## Practices

- [Input](../docs/contributing/best-practices.md#input-input)
