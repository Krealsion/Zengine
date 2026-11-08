# `graphics/` — Graphics: working on it

The router for `graphics/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `vocabulary.hpp` | messages: `Node`, `ElementRecord`, `Drawn`, `ElementPut`, `ElementChange`, the pointer in an area (`AreaButtonChanged` ... `AreaPointerLeft`), the directory's and the palettes (`PaletteOffered`, `PaletteChosen`), `WindowShown`; `kGraphicsRole` |
| `color.hpp` | a colour a draw carries, `Color`: an exact value (`rgb`, `rgba`, beside the pool colour it is `like`), a pool name (`PoolColor`) or a slot (`SlotColor`) |
| `pool.json`, `themes/*.json` | the pool's colours and their defaults, and the three shipped themes, generated at configure time into `graphics/colors.hpp` (`colors::accent` ...) and `graphics/themes.hpp` (`themes::night()` ...) by `cmake/ZengineNames.cmake` |
| `pixels.hpp` | saturating pixel arithmetic: `add_cells`, `sub_px`, `mul_px`, `floor_div_px`, `px_of_cells`, `cell_of_pixel`, `device_of_px`, `px_span_contains` and the grains |
| `ops.hpp`, `implied.hpp` | the seven operators' operands and the colours each carries (`colors_of`), the pool colour a colour shows as (`pool_color_of`), refused in words when they do not read (`operands_refused`); a body without its envelope |
| `canvas.hpp` | `Sketch`, `Canvas`, `difference`: an owner's drawing, sent as what changed |
| `elements.hpp`, `elements/*.hpp` | the parts an owner draws into a `Sketch`: `Button`, `Checkbox`, `List`, `Grid`, `Icon`, `Image`, `ScrollView`, `TextBox`; each its nodes and its `*Face` record, the action the owner names riding the record; and the text pieces' stateless faces over `component/`'s values (`text_face.hpp`) |
| `drawing.hpp` | `DrawingWeave`: a drawer's authoring base, the room's handshake and the showing |
| `painting.hpp` | `PaintTask`/`PaintResult`, a node handed its colours resolved (`PaintInk`), `ElementEvent`/`ElementOutcome`/`ElementDescribed` |
| `term_paint.hpp`, `light_paint.hpp` | the window's own painters: for characters, and for light |
| `element_abi.h`, `element_export.hpp` | the element seam (`zengine_element`), `ZENGINE_EXPORT_ELEMENT` |
| `window/store.hpp`, `window/seams.hpp`, `window/palette.hpp`, `window/window.hpp` | the window's areas and judge; the element code it opens; the palettes it holds and a name turned into a colour (`resolve`); `Window<Medium>`, the window over its medium |
| `window/compose.hpp`, `window/terminal_io.hpp`, `window/window_terminal.hpp` | the terminal's painted cells, composition and landing; the terminal's claim, size, ink (`sgr_for`: a colour's word, else its nearest) and copy sequence; `TerminalMedium`, `WindowT<Terminal>` |
| `window/window_sdl.hpp` | the medium of light over a pixel backend: `LightMedium`, `SdlWindowT<Backend>`; the desktop's arithmetic, and the BMP a picture is read back as |
| `window/window_sdl_backend.hpp` | the OS window's backend through SDL3, `ProcessSdlBackend` |
| `window/window.cpp`, `window/window_sdl.cpp` | loadable weaves `zengine-window-terminal` and, where the build has SDL, `zengine-window-sdl` |
| `directory.cpp` | loadable weave `zengine-graphics` |
| `CMakeLists.txt` | `zengine-graphics-vocabulary` (header-only, `zengine::graphics`) with its generated names and palettes, the weaves |

## Law

- [graphics](../agents/graphics.md) — the window, its colours, its seam and the directory

## Suites

- `graphics` — the vocabulary, the canvas, colours by name, both painters and every operator, the
  light painter's packing and landing, and a tripwire that no source here includes Surface; the
  window, the directory, the palettes and the snake's board through the real kernel: said once, one
  change a tick shown once, a palette chosen or offered anew painting again with nothing drawn on
  the bus, a slot, the window's own palette, a terminal's nearest ink, operands of the first
  versions refused, each participant replaced or reloaded, the room naming what the
  window holds, a drawing weave shown once a beat, keys and their releases, every pointer moment
  given to the area hearing the pointer it lands in, a press held for its area until its release
  or a hold's end, the arranger told where presses land, an area deaf to the pointer letting it
  through, every bound refused in words,
  and each role's manual answering for every shape it accepts; every part in `elements/` drawn,
  changed and painted, and pressed through the window, its action asked or nothing sent; the
  Attention and Connections panes Workshop loads drawn, pressed and reloaded in a window, and a
  pane's refused drawing followed by a whole one; the OS window over the suite's own pixel
  backend, every operator painted, pressed, typed into, resized, asked to close and started again
  from what it kept; both windows' clipboard, place on the desktop and last picture read back, and
  the desktop's arithmetic and the BMP; and, where the build has SDL, its weave under SDL's dummy
  driver emptying SDL's queue, claiming it while loaded, keeping its one OS window across a reload,
  and holding one again once replaced, and its own backend centring, growing and placing its window,
  copying to SDL's clipboard and reading its pixels back; `tests/test_graphics.cpp`,
  `tests/graphics_rig.hpp`
- `graphics_change_outside_list_refused` — an element's change outside its code's list does not
  compile (`tests/compile_negative/drawing_walls.cpp`, case 1)
- `graphics_own_after_delivery_refused` — a drawing weave whose drawer replaces the
  end-of-delivery hook does not compile (case 2)
- `graphics_undeclared_color_refused` — a pool colour nobody declared does not compile (case 4)
- `graphics_color_role_refused` — a colour named by a role does not compile (case 5)
- `graphics_drawer_compiles` — the walls' positive control, a drawer naming a declared colour that
  builds (case 3)

The snake suite (`snake`, `tests/test_snake.cpp`) chooses a palette mid-game and reads the window's
rows; it shares the rig.

## Host side

- Staging and loading: `examples/snake/CMakeLists.txt` builds the window, the directory and the
  board's code beside the snake's host; `examples/snake/play.cpp` loads the directory and the
  window, and its operator weave is the arranger, offering two of the shipped palettes.
  `workshop/CMakeLists.txt` stages the window and the directory beside `zengine-workshop` (and the
  OS window where the build has SDL), whose weave arranges when they are loaded and offers the three
  shipped palettes, and `cmake/ZengineInstall.cmake` installs them as artifacts, all but the OS
  window, whose SDL the install does not own, with the generator and the pool's and themes' JSON.
- Grants: the snake host grants its operator `AreaPlaced`, `PaletteOffered`, `PaletteChosen`,
  `ElementCodeOffered` and `RoomAsked` to `kGraphicsRole`, and `Drawn` to the window's role.
- Beyond the snake: the terminal window reads its terminal with `input/terminal_reader.hpp` and
  claims its output with `window/terminal_io.hpp`; the OS window reads SDL's queue with
  `input/translate_sdl_core.hpp` over the SDL `cmake/ZengineSdl.cmake` acquires; each claims its
  input edge with `input/window_claim.hpp`; both open element code with `operator/image.hpp`. `cmake/ZengineInstall.cmake` installs the headers `zengine::graphics`
  carries.

## Pages

- [`zengine.window.terminal`](docs/zengine.window.terminal.md), the terminal window's manual,
  compiled in
- [`zengine.window.sdl`](docs/zengine.window.sdl.md), the OS window's manual, compiled in
- [`zengine.graphics`](docs/zengine.graphics.md), the directory's manual, compiled in
- [The Graphics package](docs/graphics.md), drawing into a window

## Practices

- An element's code draws what it is told; a game's or a pane's rules stay with its owner.
- A part in `elements/` draws the state its owner set and asks for no action of its own: a press
  on it reaches the owner as the action the owner named when it drew the part, or, in an area
  placed to hear the pointer, as the press itself.
- A value a page quotes is marked and registered in `tests/code_values.txt`.
