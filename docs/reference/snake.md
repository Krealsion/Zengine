# snake — a worked example

**Reference / example.** A playable game whose parts are genuinely separate, independently
replaceable weaves. It is the best short answer to "what does an application made of weaves
actually look like?", and it is what the `zengine-snake-tests` suite proves headless.

Source: [`snake/`](../../snake/world.cpp) — world, score, controls, clock, host.

The Stage 2 vertical slice: a playable snake whose parts are genuinely separate weaves.
Since the Surface migration, **snake contains no drawing code at all** — the suite pins a
skinless game at zero stdout bytes.

- **World** (`snake-world-v1`/`-v2`, one source) owns the simulation and holds
  `SnakeWorldState`; it publishes `SnakeVisual`/`FoodEaten`/`SnakeDied` and never knows its
  consumers. Both versions converse (`zen.PrepareShutdown` → a letter); v2 is additionally an
  heir — it claims by role on first wake and folds a v1 inheritance through `migrate()`.
  Untouched by the Surface migration — the whole ceremony reduction was consumer-side.
- **Score** (`snake-score`) accepts only `FoodEaten` and counts what it *witnesses* — loaded
  late into a live game, its count honestly differs from the world's. It publishes its tally
  as `SurfaceText` (and re-publishes on a skin's hello) instead of painting a row.
- **Controls** (`snake-controls`) is snake's input binding: it accepts the Input package's
  `KeyPressed` and turns WASD/arrows into `SnakeTurn`, sent to the world **by role** so
  steering survives the world being swapped mid-game. The binding is a weave, so it is
  replaceable like everything else (a remap, an AI pilot, a replay feeder).
- **Clock** (`snake-clock`) is snake's time binding — the controls move, pointed at time: it
  asks the Timer package for the 120ms repeating beat (`snake.tick`, the cadence the host
  used to hard-code, now living where the pace belongs) and turns each `TimerFired` into
  `SnakeTick`, sent to the world **by role** so time survives the world being swapped. The
  world never learns where ticks come from; only the source of time moved.
- **Host** (`zengine-snake`) owns the boot list — nothing else. It reads no keys (the Input
  weave produces them), owns no screen (the skin claims it at load), and since the Timer
  package keeps no clock, never sleeps, and pumps nobody. It contributes nothing to time
  either: it queues the boot list and `drain_until_idle()` IS the game — this host has nothing
  of its own to do between turns, so "run until the world stops" is exactly its program.
  Loading the timer service is what starts the clock — the control door activates it and it
  authors its own chain — so there is no wind, and no boot-drain-then-wind ceremony to order
  correctly. The loop ends when the
  operator's quit key stops the bus (or, honestly, when the bus goes quiet because no clock is
  deployed, or because activation could not establish time).
  Its status line is published intent like everything else, spoken by the granted operator
  weave that also sends every lifecycle command through the Weave Manager. Run it under WSL
  from the build tree; keys: wasd steer, `1` swap the TUI skin, `2` load score, `3` grow the
  world (graceful v1→v2 migration), `4` swap to the SDL skin (where deployed), `r` reload in
  place, `n` new game, `l` list, `q` quit.

This package is the **hosting consumer** that pulled `loom::kernel` onto the Loom's export
surface, and whose nested shapes surfaced (and pulled the completion of) the manifest's
documented-but-unbuilt `referenced` section (`zen.Manifest` v3). The snake targets gate on
`if(TARGET loom::kernel)`, so a Windows Loom install still configures — the package simply
skips.
