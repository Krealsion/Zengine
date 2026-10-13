# snake — a worked example

**Reference / example.** A playable game whose parts are separate, independently replaceable
weaves where each earns it: one world holding the whole game, a window that shows it, a directory
that introduces the two, and a score weave loaded late. It is the best short answer to "what does
an application made of weaves actually look like?", and the `snake` suite proves it headless.

Source: [`examples/snake/`](world.cpp) — world, board, score, host.

- **World** (`snake-world-v1`/`-v2`, one source) is the whole game: its rules
  ([`logic.hpp`](logic.hpp)), its keys and its beat. It holds `SnakeWorldState` -- the game, its
  bindings (whether its keys steer and which four, its pace), and where it presented its board --
  and publishes its news, `SnakeVisual`, `FoodEaten` and `SnakeDied`, for anyone. It draws its frame
  (the score, a death notice, a new-game button) as nodes into the terminal window that shows its
  area, and its board as an element of the game's own: said once, then each step as a change.
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
  the host's own build, chooses the skin -- and speaks the status line in an area of its own. The
  window gives every key to the world and the operator alike.

Run it from the build tree, in a terminal: keys are `wasd` or the arrows to steer; `1` chooses the
other skin, classic or block (painter code the window opens mid-game; nothing is drawn again on the
bus); `2` loads the score weave; `3` grows the world (graceful v1→v2 migration); `4` says the SDL
window waits on a later stage; `r` reloads the world in place; `n` starts a new game through the
poke-reset door; `l` lists; `q` quits. Pressing `[ new game ]` with the pointer starts one too,
sent to the board as a change.

How the drawing works is [the Graphics package](../../graphics/docs/graphics.md). This package is
the **hosting consumer** that pulled `loom::kernel` onto the Loom's export surface. The snake
targets gate on `if(TARGET loom::kernel)`, so a Windows Loom install still configures -- the package
simply skips.
