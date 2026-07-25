# Zengine

Zen's **default set** — the engine-shaped weaves most projects will want, built on the Loom.

## The three tiers

**The Loom is everyone's, Zengine is the default set, your weaves are yours.**

The Loom is the substrate: values, schemas, the gate, the switchboard. Zengine is one
opinionated set of weaves built on it — a set you can take, replace piece by piece, or ignore
entirely. Your own weaves sit alongside Zengine's as peers, not as plugins into it.

This repository is the middle tier, and it is **separate from the Loom on purpose**. Zengine
consumes the Loom exactly as a stranger would, so every rough edge in the Loom's public surface
hits the house before it hits a guest. The dependency arrow is structurally un-invertible: the
Loom's build cannot see Zengine.

## How this consumes the Loom

**Default — the sibling path** (`ZEN_LOOM_DEV=ON`): `add_subdirectory(../Loom)`. The
working-tree default, and the turnkey path everywhere:

```sh
# Linux / WSL
cmake -S . -B build-dev
cmake --build build-dev -j"$(nproc)"
ctest --test-dir build-dev
```

On **Windows** (MinGW), the same configure is all it takes: dev mode defaults
`LOOM_ENABLE_WINDOWS_KERNEL=ON` — the Loom's explicit development/demo backend (**no
isolation**; the Loom prints its banner and `Kernel::containment_note()` says so) — so the
snake package and its suite build and run natively. Pass
`-DLOOM_ENABLE_WINDOWS_KERNEL=OFF` to decline. The MinGW runtime DLLs must be on `PATH`
(or beside the binaries) to run.

**The stranger's path** (`-DZEN_LOOM_DEV=OFF`): `find_package(loom)` against an installed,
exported Loom, consumed exactly as a third party would:

```sh
# in Zen/Loom — build and install the Loom
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
cmake --install build --prefix "$PWD/build/_install"

# in Zen/Zengine — consume it
cmake -S . -B build -DZEN_LOOM_DEV=OFF -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install"
cmake --build build -j"$(nproc)"
ctest --test-dir build
```

Both paths expose the **same target names** (`loom::core`, `loom::switchboard`,
`loom::kernel`) — the Loom's export sets `EXPORT_NAME` to match its in-tree aliases — so the
override is a genuine drop-in and the two paths cannot silently come to mean different things.

The stranger's path is no longer the default, but it remains the **proof lane**: the WSL
verification lanes (`build`, `build-san`) are configured with it deliberately, so a mistaken
dependency on an unexported target (the UI trio, the console, the TUI, the bridge, the SDL
skin — all Zengine-destined, each moving in its own port phase) still surfaces here, in the
house, before it hits a guest. Dev mode reaches the whole Loom build tree and is silent about
that class of mistake — which is exactly why the proof lanes stay on the other path.

## `reference/` — the read-only quarry

`reference/` holds the **V1 Zen engine**: a quarry to read, not a codebase to port. Nothing in
it is built by this repo. Ports are read-and-rewrite, phase by phase, landing in their proper
home from birth — never a lift-and-shift.

*Provenance:* a plain file import of the V1 repo's **working tree** (taken 2026-07-18, including
changes uncommitted there at the time), not a history-carrying subtree split. Its git history
stays in that original working copy (`G:\dev\BloodRush\Zen`) and is deliberately not carried
here — note its `origin` points at the *same* GitHub project as the Loom's, which is a further
reason the import drops `.git` rather than nesting a second repo under `reference/`. Dropped at
import: the 206 MB Python virtualenv, the derived `all_code.txt` concatenation, and
editor/agent cruft. No CMake `CompilerId` junk existed to drop.

## Test discipline

**Per-repo green.** Zengine's lane runs Zengine's tests against its pinned/installed Loom and
**does not re-run the Loom's suite** — a dependency's proof rides its version. Every report-back
states *which repo's green was proven*; "green" must never silently mean "green in one of two."

*Honest today-note:* the Loom is still under active development, so during Loom phases its
delegated-scope suite still runs there, per phase. The don't-re-prove economy arrives as the
Loom stabilizes; the structure is ready for it now.

Zengine's green is four tests: the **smoke** (link the Loom's exported surface, drive a value
through the real gate, confirm the gate **refuses** a malformed candidate — the refusal is what
makes it a proof instead of a greeting), the **snake suite** (`tests/test_snake.cpp`) — the
Stage 2 vertical slice proven headless: the locked contract pinned by content-id, the simulation
and the v1→v2 migration as pure math, the three live-evolution moments driven end to end
through real `.so` weaves, the real kernel, and the real Weave Manager, and the phase's negative
space (a skinless game writes **zero bytes** to stdout, with a painted-bytes negative control) —
the **input suite** (`tests/test_input.cpp`): the Input package's locked contract and
SDL-scancode identity pinned by content-id and literal value, both backends' translations as
pure math on every lane, the weave's publish path through a real bus, and the keys-become-turns
chain through the real libraries — and the **surface suite** (`tests/test_surface.cpp`): the
Surface package's contract by content-id, the terminal skins as golden bytes, the SDL skin's
frame plan as pure math on every lane, the hello handshake and the one-owner rule through the
real kernel, the granted-operator speaking recipe, and (where built) the SDL skin driven by the
same intent under SDL's dummy video driver.

## `input/` — the Input package

The floor games sit on: exactly one Input weave (`zengine-input`, holding the `zengine.input`
role) is the sole producer of the five locked shapes — `KeyPressed`/`KeyReleased` (SDL scancodes
as the wire identity of a key; `name` is convenience, never authority), `MouseButton`,
`MouseMoved`, `MouseWheel` — and the only code that talks to the platform. Consumers only
accept; there is no polling API. Backends today are the ones snake runs on: the POSIX terminal
(raw mode; strokes synthesize press+release) and the Win32 console (real key transitions, mouse
records); an SDL **Reader** (the window's own input, including its close box) is the named
follow-on now that the Surface package gives it a window to read. `PumpInput` is the drive
message — the host loop opens the weave's hands each lap until timers exist.

## `surface/` — the Surface package

Visual intent in, output out. No game, world, or panel weave talks to the terminal, a window,
or a renderer: they **publish** intent, and a **Skin** — a replaceable loadable weave holding
the singleton `zengine.skin` role — claims the actual surface and paints. Claiming is RAII
(the constructor takes the medium, the destructor gives it back; a swap is release-then-claim
because the Manager delivers the unload first), and ownership is enforced ground: loading a
second skin into the held role is a clean refusal.

The vocabulary is deliberately tiny: `SurfaceText{slot, text}` (a line of **plain** text for a
named slot — "status", "score"; styling is the skin's business) and `SurfaceReady` (the active
skin's hello, published once per incarnation on its first message; text publishers re-publish
their current line on hearing it, so a fresh painter starts complete — the tally line survives
the painter being replaced mid-game). `SnakeVisual` is the V1 canvas payload, accepted by the
skins directly — a named coupling; the general canvas vocabulary is a later phase.

Three skins ship: **`zengine-skin-tui-classic`** and **`zengine-skin-tui-block`** (the old
snake drawers' looks, now living where drawing lives — the terminal medium is one header,
golden-byte tested), and **`zengine-skin-sdl`** — a real window, same intent, zero
medium-specific fields added anywhere (the agnosticism proof). The SDL skin is the only
target that sees SDL: it fetches a **pinned static SDL3** where none is installed
(checksum in the build; `-DZENGINE_SDL_SKIN=OFF` declines), plans every frame as pure math
(`skin_sdl_plan.hpp`, pinned on every lane), and degrades gracefully with no display — the
suite drives it under SDL's dummy driver, and the window title carries the text slots.

The SDL window is **output-only in V1, structurally**: it is created not-focusable (a window
that cannot hear must not take the keys — the terminal stays the game's one ear until the SDL
Reader phase makes the window an ear too), and it is kept answering its OS through
`PumpSurface`, the PumpInput-precedent drive message the host root-sends by role each lap —
a window medium must service its event queue even when the world publishes nothing (a dead
world starves a frame-driven pump; the OS calls the result "not responding"). Terminal media
no-op the pump; the shape retires with PumpInput the day timers arrive.

## `snake/` — the first game panel

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
- **Host** (`zengine-snake`) owns the clock and the boot list — nothing else. It reads no
  keys (the Input weave produces them) and owns no screen (the skin claims it at load); its
  status line is published intent like everything else, spoken by the granted operator weave
  that also sends every lifecycle command through the Weave Manager. Run it under WSL from
  the build tree; keys: wasd steer, `1` swap the TUI skin, `2` load score, `3` grow the world
  (graceful v1→v2 migration), `4` swap to the SDL skin (where deployed), `r` reload in place,
  `n` new game, `l` list, `q` quit.

This package is the **hosting consumer** that pulled `loom::kernel` onto the Loom's export
surface, and whose nested shapes surfaced (and pulled the completion of) the manifest's
documented-but-unbuilt `referenced` section (`zen.Manifest` v3). The snake targets gate on
`if(TARGET loom::kernel)`, so a Windows Loom install still configures — the package simply
skips.

## Working in this tree

Zengine and the Loom live side by side under a shared `Zen/` root:

```
Zen/
  Loom/        the substrate — everyone's
  Zengine/     this repo — the default set
  playground/  Josh's own weaves
```

Assistant sessions are launched from the **`Zen/` root**, never from inside a sub-repo: the
memory graph is keyed to that path, so launching from `Zen/Zengine/` silently cold-starts
without it. Run git per-repo (`git -C Loom …`, `git -C Zengine …`).
