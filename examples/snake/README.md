# snake — a worked example

**Reference / example.** A playable game whose parts are separate, independently replaceable
weaves where each earns it: one world holding the whole game, a window that shows it, a directory
that introduces the two, a score weave loaded late, and a pilot that plays the game. It is the
best short answer to "what does an application made of weaves actually look like?", and the
`snake` suite proves it headless.

Source: [`examples/snake/`](world.cpp) — world, board, score, pilot, host.

- **World** (`snake-world-v1`/`-v2`, one source) is the whole game: its rules
  ([`logic.hpp`](logic.hpp)), its keys, its beat and its pause. It holds `SnakeWorldState` -- the
  game, its bindings (whether its keys steer and which four, its pace) and its pause (whether it
  is paused, the steps this game has taken, and the step it pauses itself after) -- and publishes
  its news,
  `SnakeVisual`, `FoodEaten` and `SnakeDied`, for anyone. It draws its frame (the score, a death
  notice, a new-game button and a pause button) as nodes into the TUI window that shows its
  area, and its board as an element of the game's own: said once, then each step as a change. Each
  button is a `gfx::Button` ([`graphics/elements/button.hpp`](../../graphics/elements/button.hpp)),
  so beside its nodes the window holds a `ButtonFace` record at `new-game` and at `pause`, naming
  the action a press asks of the world. Its state names no window: the window's room says whether
  it still holds the board.
- **Its pause** is the world's, whoever steers: `p` or the pause button pauses it and resumes it,
  and a paused world takes no step -- from its beat or a `SnakeTick` -- and no turn until it
  resumes. Its beat rests while it is paused, however the pause was written, and a resumed world
  beats again at the pace it held. Writing `pause_at` asks it to pause itself after that step of
  the game, once: with the pilot flying from the first step, the same game held still on the same
  step every run.
  Both versions converse (`zen.PrepareShutdown` → a letter); v2 is additionally an heir -- it
  claims by role on first wake and folds a v1 inheritance through `migrate()`.
- **Its doors stay open.** `SnakeTick` and `SnakeTurn`, sent by role to whoever holds
  `snake.world`, drive and steer the world from anyone. That open door, not an adapter weave, is
  what a pilot, a replay or a stepper needs: a pilot writes the world's `keys` false and steers by
  `SnakeTurn`; a replay writes `beat_ms` 0 and feeds `SnakeTick`; a faster game writes `beat_ms`
  and needs no rebuild. Nobody writing the field means two drivers at once.
- **Board** (`snake-board`) is the board's code, opened by the window as element code
  ([`board.hpp`](board.hpp), [`board.cpp`](board.cpp)): an outline and a node a filled cell, the
  head in the accent, the body in the fill, the food in the alert. It draws what it is told; the
  rules stay the world's, and a press on the board reaches the world.
- **Score** (`snake-score`) is the late-load demonstration and nothing more: it counts the
  `FoodEaten` it witnesses -- loaded after two meals it reads 0 while the world reads 2 -- and draws
  its tally in an area of its own.
- **Pilot** (`snake-pilot`, role `snake.pilot`) plays the game live, for an agent or a demo: it
  hears each `SnakeVisual` and answers it with one `SnakeTurn`, steering as an old game AI does --
  the shortest way to the food when its own tail is still in reach once it has eaten, else toward
  its own tail ([`pilot.hpp`](pilot.hpp), pure, so the suite flies a mirror of it in lockstep).
  It never steps the world: the world's beat is the game's time. Activated, it takes the wheel,
  writing the world's `keys` false; Loom tells no weave it is unloaded, so whoever lands it gives
  the keys back -- the operator does, once the kernel's door says the pilot has gone.
- **Host** (`zengine-snake`) owns the boot list and nothing else: the directory, the terminal
  window, the world, the pilot when it flies from birth, and the Timer service, whose loading is
  what starts time. Its operator weave is the window's arranger -- it offers two of the shipped
  palettes, night and block, and the board's code from the host's own build, chooses each area's
  palette and places it -- and speaks the status line, which says who steers, with a row of
  `gfx::Button`s beneath it, in an area of its own. The window gives every key to the world and
  the operator alike, and a press on an operator button to the operator, which makes the same
  gesture as the key the button names. Every command the operator sends is an ask in its
  `loom::AskBook` (`<zen/weave/ask_book.hpp>`), which is bounded: one more than it tracks is
  refused out loud. An answer settles an ask of the manager, or of the kernel's door that lands
  the pilot, when it carries that ask's correlation and its stamp, and an ask of the world's
  poke doors, sent by role, only when Loom attests it as the answer.

Run it from the build tree, in a terminal: keys are `wasd` or the arrows to steer and `p` to pause;
`1` chooses the other skin, the night or the block palette (the window paints every area again
mid-game; nothing is drawn again on the bus); `2` loads the score weave; `3` grows the world
(graceful v1→v2 migration); `4` asks for a GUI window, and says no window weave draws one: the
snake's window is the terminal; `5` flies the pilot, and lands it again; `r` reloads the world in
place; `n` starts a new game through the poke-reset door, which gives the world its defaults, so
while the pilot flies the operator writes the keys off again; `l` lists; `q` quits. The buttons
beneath the status line, `[1 skin]` to `[q quit]`, do what their keys do. Pressing `[ new game ]`
with the pointer starts one too, sent to the board as a change, and `[ pause ]` pauses as `p`
does.

`zengine-snake --pilot` flies the pilot from birth, before time starts, so it plays the first game
from its first step; `--pause-at <step>` has the world pause itself after that step of its first
game. Together they are the same game every run, held still on the same step: what a picture of a
game in play is taken from.

How the drawing works is [the Graphics package](../../graphics/docs/graphics.md). This package is
the **hosting consumer** that pulled `loom::kernel` onto the Loom's export surface. The snake
targets gate on `if(TARGET loom::kernel)`, so a Windows Loom install still configures -- the package
simply skips.
