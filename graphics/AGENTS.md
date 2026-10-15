# `graphics/` — Graphics: working on it

The router for `graphics/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `vocabulary.hpp` | messages: `Node`, `ElementRecord`, `Drawn`, `ElementPut`, `ElementChange`, the directory's, `WindowShown`; `kGraphicsRole` |
| `ops.hpp`, `implied.hpp` | the seven operators' operands, a `Paint`; a body without its envelope |
| `canvas.hpp` | `Sketch`, `Canvas`, `difference`: an owner's drawing, sent as what changed |
| `drawing.hpp` | `DrawingWeave`: a drawer's authoring base, the room's handshake and the showing |
| `painting.hpp` | `PaintTask`/`PaintResult`, `ElementEvent`/`ElementOutcome`/`ElementDescribed` |
| `painter_abi.h`, `painter_export.hpp`, `term_paint.hpp` | the skin seam (`zengine_painter`), `ZENGINE_EXPORT_PAINTER`, the character painter |
| `element_abi.h`, `element_export.hpp` | the element seam (`zengine_element`), `ZENGINE_EXPORT_ELEMENT` |
| `window/store.hpp`, `window/compose.hpp`, `window/seams.hpp`, `window/window.hpp` | the window's areas and judge; its painted cells, composition and landing; the libraries it opens; `WindowT<Terminal>` |
| `window/window.cpp` | loadable weave `zengine-window-terminal` |
| `directory.cpp` | loadable weave `zengine-graphics` |
| `paint_classic.cpp`, `paint_block.cpp` | painter libraries `zengine-paint-classic`, `zengine-paint-block` |
| `CMakeLists.txt` | `zengine-graphics-vocabulary` (header-only, `zengine::graphics`), the weaves, the skins |

## Law

- [graphics](../agents/graphics.md) — the window, its seams and the directory

## Suites

- `graphics` — the vocabulary, the canvas, the painter and every operator; the window, the
  directory, the skins and the snake's board through the real kernel: said once, one change a
  tick shown once, a skin chosen, each participant replaced or reloaded, the room naming what the
  window holds, a drawing weave shown once a beat, presses and keys, every bound refused in words;
  `tests/test_graphics.cpp`, `tests/graphics_rig.hpp`
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
  the window, and its operator weave is the arranger.
- Grants: the snake host grants its operator `AreaPlaced`, `SkinOffered`, `SkinChosen`,
  `ElementCodeOffered` and `RoomAsked` to `kGraphicsRole`, and `Drawn` to the window's office.
- Beyond the snake: the window reads its terminal with `input/terminal_reader.hpp` and claims its
  output with `surface/tui_terminal.hpp`; it opens libraries with `operator/image.hpp`. Nothing
  installs this package's headers yet.

## Pages

- [The Graphics package](docs/graphics.md)

## Practices

- An element's code draws what it is told; a game's or a pane's rules stay with its owner.
- A value a page quotes is marked and registered in `tests/code_values.txt`.
