# snake — a worked example

**Reference / example.** A playable game whose parts are separate, independently replaceable
weaves where each earns it: one world holding the whole game, a window that shows it, a directory
that introduces the two, and a score weave loaded late. It is the best short answer to "what does
an application made of weaves actually look like?", and the `snake` suite proves it headless.

Source: [`examples/snake/`](world.cpp) — world, board, score, host.

- **World** (`snake-world-v1`/`-v2`, one source) is the whole game: its rules
  ([`logic.hpp`](logic.hpp)), its keys and its beat. It holds `SnakeWorldState` -- the game and
  its bindings (whether its keys steer and which four, its pace) -- and publishes its news,
  `SnakeVisual`, `FoodEaten` and `SnakeDied`, for anyone. It draws its frame (the score, a death
  notice, a new-game button) as nodes into the terminal window that shows its area, and its board
  as an element of the game's own: said once, then each step as a change. The new-game button is
  a `gfx::Button` ([`graphics/elements/button.hpp`](../../graphics/elements/button.hpp)), so beside
  its nodes the window holds a `ButtonFace` record at `new-game`, naming the action a press asks
  of the world. Its state names no window: the window's room says whether it still holds the
  board.
  Both versions converse (`zen.PrepareShutdown` → a letter); v2 is additionally an heir -- it
  claims by role on first wake and folds a v1 inheritance through `migrate()`.
- **Its doors stay open.** `SnakeTick` and `SnakeTurn`, sent by role to whoever holds
  `snake.world`, drive and steer the world from anyone. That open door, not an adapter weave, is
  what a pilot, a replay or a stepper needs: a pilot writes the world's `keys` false and steers by
  `SnakeTurn`; a replay writes `beat_ms` 0 and feeds `SnakeTick`; a faster game writes `beat_ms`
  and needs no rebuild. Nobody writing the field means two drivers at once.
- **Board** (`snake-board`) is the board's code, opened by the window as it opens a skin
  ([`board.hpp`](board.hpp), [`board.cpp`](board.cpp)): an outline and a node a filled cell, the
  head in the accent, the body in the fill, the food in the alert. It draws what it is told; the
  rules stay the world's, and a press on the board reaches the world.
- **Score** (`snake-score`) is the late-load demonstration and nothing more: it counts the
  `FoodEaten` it witnesses -- loaded after two meals it reads 0 while the world reads 2 -- and draws
  its tally in an area of its own.
- **Host** (`zengine-snake`) owns the boot list and nothing else: the directory, the terminal
  window, the world and the Timer service, whose loading is what starts time. Its operator weave
  is the window's arranger -- it places the areas, offers the two skins and the board's code from
  the host's own build, chooses the skin -- and speaks the status line, with a row of
  `gfx::Button`s beneath it, in an area of its own. The window gives every key to the world and
  the operator alike, and a press on an operator button to the operator, which makes the same
  gesture as the key the button names. Every command the operator sends is an ask in its
  `loom::AskBook` (`<zen/weave/ask_book.hpp>`), which is bounded: one more than it tracks is
  refused out loud. An answer settles an ask of the manager when it carries that ask's
  correlation and the manager's stamp, and the reset door's ask, sent by role, only when Loom
  attests it as the answer.

Run it from the build tree, in a terminal: keys are `wasd` or the arrows to steer; `1` chooses the
other skin, classic or block (painter code the window opens mid-game; nothing is drawn again on the
bus); `2` loads the score weave; `3` grows the world (graceful v1→v2 migration); `4` asks for an OS
window, and says no window weave draws one: the snake's window is the terminal; `r` reloads the
world in place; `n` starts a new game through the poke-reset door; `l` lists; `q` quits. The
buttons beneath the status line, `[1 skin]` to `[q quit]`, do what their keys do. Pressing
`[ new game ]` with the pointer starts one too, sent to the board as a change.

How the drawing works is [the Graphics package](../../graphics/docs/graphics.md). This package is
the **hosting consumer** that pulled `loom::kernel` onto the Loom's export surface. The snake
targets gate on `if(TARGET loom::kernel)`, so a Windows Loom install still configures -- the package
simply skips.
