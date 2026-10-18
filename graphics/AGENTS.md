# `graphics/` — Graphics: working on it

The router for `graphics/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `vocabulary.hpp` | messages: `Node`, `ElementRecord`, `Drawn`, `ElementPut`, `ElementChange`, the directory's, `WindowShown`; `kGraphicsRole` |
| `role.hpp` | the theme's roles, `role::kFill` ... `role::kNone`, which Surface spells too |
| `ops.hpp`, `implied.hpp` | the seven operators' operands, a `Paint`; a body without its envelope |
| `canvas.hpp` | `Sketch`, `Canvas`, `difference`: an owner's drawing, sent as what changed |
| `elements.hpp`, `elements/*.hpp` | the parts an owner draws into a `Sketch`: `Button`, `Checkbox`, `List`, `Grid`, `Icon`, `Image`, `ScrollView`, `TextBox`; each its nodes and its `*Face` record, the action the owner names riding the record |
| `drawing.hpp` | `DrawingWeave`: a drawer's authoring base, the room's handshake and the showing |
| `painting.hpp` | `PaintTask`/`PaintResult`, `ElementEvent`/`ElementOutcome`/`ElementDescribed` |
| `painter_abi.h`, `painter_export.hpp`, `term_paint.hpp`, `light_paint.hpp` | the skin seam (`zengine_painter`), `ZENGINE_EXPORT_PAINTER` and `ZENGINE_EXPORT_LIGHT_PAINTER`, the character painter and the painter for light |
| `element_abi.h`, `element_export.hpp` | the element seam (`zengine_element`), `ZENGINE_EXPORT_ELEMENT` |
| `window/store.hpp`, `window/seams.hpp`, `window/window.hpp` | the window's areas and judge; the libraries it opens; `Window<Medium>`, the window over its medium |
| `window/compose.hpp`, `window/terminal_io.hpp`, `window/window_terminal.hpp` | the terminal's painted cells, composition and landing; the terminal's claim, size and ink; `TerminalMedium`, `WindowT<Terminal>` |
| `window/window_sdl.hpp` | the medium of light over a pixel backend: `LightMedium`, `SdlWindowT<Backend>` |
| `window/window.cpp`, `window/window_sdl.cpp` | loadable weaves `zengine-window-terminal` and, where the build has SDL, `zengine-window-sdl` |
| `directory.cpp` | loadable weave `zengine-graphics` |
| `paint_classic.cpp`, `paint_block.cpp`, `paint_day.cpp`, `paint_night.cpp` | painter libraries `zengine-paint-classic`, `zengine-paint-block` (characters), `zengine-paint-day`, `zengine-paint-night` (light) |
| `CMakeLists.txt` | `zengine-graphics-vocabulary` (header-only, `zengine::graphics`), the weaves, the skins |

## Law

- [graphics](../agents/graphics.md) — the window, its seams and the directory

## Suites

- `graphics` — the vocabulary, the canvas, both painters and every operator, the light painter's
  packing and landing, and a tripwire that no source here includes Surface; the window, the
  directory, the skins and the snake's board through the real kernel: said once, one change a
  tick shown once, a skin chosen, each participant replaced or reloaded, the room naming what the
  window holds, a drawing weave shown once a beat, presses and keys, every bound refused in words,
  and each office's manual answering for every shape it accepts; every part in `elements/` drawn,
  changed and painted, and pressed through the window, its action asked or nothing sent; the
  Attention and Connections panes Workshop loads drawn, pressed and reloaded in a window, and a
  pane's refused drawing followed by a whole one; the OS window over the suite's own pixel
  backend, every operator painted, pressed, typed into, resized and started again from what it
  kept, and, where the build has SDL, its weave under SDL's dummy driver keeping its one OS window
  across a reload, and holding one again once replaced; `tests/test_graphics.cpp`, `tests/graphics_rig.hpp`
- `graphics_change_outside_list_refused` — an element's change outside its code's list does not
  compile (`tests/compile_negative/drawing_walls.cpp`, case 1)
- `graphics_own_after_delivery_refused` — a drawing weave whose drawer replaces the
  end-of-delivery hook does not compile (case 2)
- `graphics_drawer_compiles` — the two walls' positive control, a drawer that builds (case 3)

The snake suite (`snake`, `tests/test_snake.cpp`) chooses a skin mid-game and reads the window's
rows; it shares the rig.

## Host side

- Staging and loading: `examples/snake/CMakeLists.txt` builds the window, the directory, the skins
  and the board's code beside the snake's host; `examples/snake/play.cpp` loads the directory and
  the window, and its operator weave is the arranger. `workshop/CMakeLists.txt` stages the window,
  the directory and the skins beside `zengine-workshop` (and the OS window where the build has
  SDL), whose weave arranges when they are loaded, and `cmake/ZengineInstall.cmake` installs them
  as artifacts, all but the OS window, whose SDL the install does not own.
- Grants: the snake host grants its operator `AreaPlaced`, `SkinOffered`, `SkinChosen`,
  `ElementCodeOffered` and `RoomAsked` to `kGraphicsRole`, and `Drawn` to the window's office.
- Beyond the snake: the terminal window reads its terminal with `input/terminal_reader.hpp` and
  claims its output with `window/terminal_io.hpp`; the OS window reads SDL's queue with
  `input/translate_sdl.hpp` and shares SDL with the SDL Skin (`cmake/ZengineSdl.cmake`); both open
  libraries with `operator/image.hpp`. `cmake/ZengineInstall.cmake` installs the headers
  `zengine::graphics` carries.

## Pages

- [`zengine.window.terminal`](docs/zengine.window.terminal.md), the terminal window's manual,
  compiled in
- [`zengine.window.sdl`](docs/zengine.window.sdl.md), the OS window's manual, compiled in
- [`zengine.graphics`](docs/zengine.graphics.md), the directory's manual, compiled in
- [The Graphics package](docs/graphics.md), drawing into a window

## Practices

- An element's code draws what it is told; a game's or a pane's rules stay with its owner.
- A part in `elements/` draws the state its owner set and asks for no action of its own: a press
  on it reaches the owner only as the action the owner named when it drew the part.
- A value a page quotes is marked and registered in `tests/code_values.txt`.
