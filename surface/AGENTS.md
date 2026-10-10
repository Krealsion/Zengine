# `surface/` — Surface and the skins: working on it

The router for `surface/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `vocabulary.hpp` | messages: canvas, text, extent, placement, clipboard, capture; `kSkinRole` |
| `region.hpp`, `cells.hpp` | the shared arithmetic: `fit_region`, cell projection, selections |
| `pointing.hpp` | a pointer on the canvas: [pointer spaces](../workshop/docs/pointer-spaces.md) |
| `terminal_size.hpp` | the one place a terminal's size is asked of the operating system |
| `skin.hpp` | `SkinT`, the Skin weave shell over an injected Medium |
| `skin_tui.hpp` | the terminal medium: `ClassicStyle`, `BlockStyle`, `TuiMedium`, `TuiTerminal` |
| `skin_tui_classic.cpp` | loadable weave `zengine-skin-tui-classic` |
| `skin_tui_block.cpp` | loadable weave `zengine-skin-tui-block` |
| `skin_sdl.cpp` | loadable weave `zengine-skin-sdl`, where SDL is built |
| `skin_sdl_plan.hpp`, `skin_sdl_text.hpp`, `skin_sdl_glyphs.hpp` | its plan; real, bitmap faces |
| `fonts/` | JetBrains Mono, embedded in the SDL Skin; `PROVENANCE.md`, `OFL.txt` |
| `CMakeLists.txt` | `zengine-surface-vocabulary` (header-only, `zengine::surface`), the weaves |

## Law

- [surface](../agents/surface.md) — surface and media

## Suites

- `surface` — the contract, golden terminal bytes, the SDL plan, hello and one owner, the canvas:
  `tests/test_surface.cpp`

The SDL Skin's cases in `surface` run under the `sdl` gate. Other suites load the real Skins:
`snake` (`tests/test_snake.cpp`) and `timer` (`tests/test_timer.cpp`) the terminal pair, and
`input` (`tests/test_input.cpp`) the SDL Skin beside the SDL reader.
`tests/package/public_surface.cpp` includes the installed headers as a consumer would.

## Host side

- Staging: `workshop/CMakeLists.txt` stages `zengine-skin-tui-classic`, and `zengine-skin-sdl`
  where built; `examples/snake/CMakeLists.txt` builds all three beside the snake host.
- Loading: `workshop/default-load-plan.json` loads `zengine-skin-tui-classic` as `zengine.skin`;
  `workshop/graphical-load-plan.json` loads `zengine-skin-sdl` in that role.
- Grants: `workshop/grant.cpp` (canvas, text, clipboard, placement), `workshop/workshop.cpp`
  (the terminal's `SurfaceText`), `workshop/guests.cpp` (capture, for the `capture` power).
- Consumers: `workshop/screen.hpp` paints the screen as one `SurfaceCanvas`;
  `workshop/weave_seam.cpp` asks the Skin for paste text; `workshop/weave_session.cpp` offers
  back a remembered placement.
- Beyond Workshop: `cmake/ZengineSdl.cmake` shares one SDL with `input/input_sdl.cpp`;
  `cmake/EmbedBinary.cmake` embeds the font; `cmake/ZengineInstall.cmake` installs the headers
  and the terminal pair.

## Pages

- [The Surface package](docs/surface.md)

## Practices

- [Surface](../docs/contributing/best-practices.md#surface-surface)
