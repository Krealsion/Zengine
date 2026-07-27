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

**Default — the stranger's path** (`ZEN_LOOM_DEV=OFF`): `find_package(loom)` against an
installed, exported Loom, consumed exactly as a third party would. Two steps, because a
stranger cannot skip the install:

```sh
# in Zen/Loom — build and install the Loom
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
cmake --install build --prefix "$PWD/build/_install"

# in Zen/Zengine — consume it
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install"
cmake --build build -j"$(nproc)"
ctest --test-dir build
```

**The dev override — the sibling path** (`-DZEN_LOOM_DEV=ON`): `add_subdirectory(../Loom)`,
for editing the two trees together without an install round-trip:

```sh
# Linux / WSL
cmake -S . -B build-dev -DZEN_LOOM_DEV=ON
cmake --build build-dev -j"$(nproc)"
ctest --test-dir build-dev
```

On **Windows** (MinGW), the dev override is also the turnkey path, because it is the one that
brings a kernel: dev mode defaults `LOOM_ENABLE_WINDOWS_KERNEL=ON` — the Loom's explicit
development/demo backend (**no isolation**; the Loom prints its banner and
`Kernel::containment_note()` says so) — so the snake package and its suite build and run
natively. Pass `-DLOOM_ENABLE_WINDOWS_KERNEL=OFF` to decline. The MinGW runtime DLLs must be
on `PATH` (or beside the binaries) to run.

```powershell
# Windows / MinGW — one flag more than it used to be, on purpose
cmake -S . -B build-win -G Ninja -DZEN_LOOM_DEV=ON
cmake --build build-win
ctest --test-dir build-win
```

Both paths expose the **same target names** (`loom::core`, `loom::switchboard`,
`loom::kernel`) — the Loom's export sets `EXPORT_NAME` to match its in-tree aliases — so the
override is a genuine drop-in and the two paths cannot silently come to mean different things.

**Why the stranger's path is the default** (a decision, re-affirmed 2026-07-27): it is what
makes a mistaken dependency on an unexported target (the UI trio, the console, the TUI, the
bridge, the SDL skin — all Zengine-destined, each moving in its own port phase) fail on
*every* developer's machine, not only in the verification lanes. Dev mode reaches the whole
Loom build tree and is silent about that entire class of mistake. The default was ON for one
experiment's length; the trust gate found it had drifted there without a decision, so the
discipline is back where it cannot be forgotten — and dev mode stays exactly one flag away.

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

Zengine's green is six tests: the **smoke** (link the Loom's exported surface, drive a value
through the real gate, confirm the gate **refuses** a malformed candidate — the refusal is what
makes it a proof instead of a greeting), the **snake suite** (`tests/test_snake.cpp`) — the
Stage 2 vertical slice proven headless: the locked contract pinned by content-id, the simulation
and the v1→v2 migration as pure math, the three live-evolution moments driven end to end
through real `.so` weaves, the real kernel, and the real Weave Manager, and the phase's negative
space (a skinless game writes **zero bytes** to stdout, with a painted-bytes negative control) —
the **timer suite** (`tests/test_timer.cpp`): the Timer package's contract by content-id, every
schedule (one-shot, repeat, upsert, clamps, cancels, role succession, honest vacancy, the
dead-requester floor) over a fake clock through a real bus, the real `.so` re-winding itself on
the real clock, and the migration chains — the world ticking, the input weave polling, and the
skin servicing its medium with nobody pumping them — the **input suite**
(`tests/test_input.cpp`): the Input package's locked contract and SDL-scancode identity pinned
by content-id and literal value, both backends' translations as pure math on every lane, the
weave's publish path and its self-arranged beat through a real bus, and the keys-become-turns
chain through the real libraries — and the **surface suite** (`tests/test_surface.cpp`): the
Surface package's contract by content-id, the terminal skins as golden bytes, the SDL skin's
frame plan as pure math on every lane, the hello handshake (which now also asks for the skin's
beat) and the one-owner rule through the real kernel, the granted-operator speaking recipe, and
(where built) the SDL skin driven by the same intent under SDL's dummy video driver — and the
**trust-gate probes** (`tests/test_audit_probes.cpp`): a different KIND of suite, kept
deliberately. It pins what the substrate measurably does to a live beat chain when the timer
service itself is swapped, reloaded, double-wound, or joined late — *including where that is
unwanted*. Read its header before changing it: probe A asserts today's swap kills the chain,
and is expected to flip when the lifecycle question (R2) is answered.

## `timer/` — the Timer package

Time, message-shaped. Games and packages do not read the OS clock and do not sleep: a weave
that wants time ASKS — `StartTimer{id, delay_ms, repeat}` (fired back to the asker) or
`StartRoleTimer{…, role}` (fired to whoever holds the role at each firing — the beat that
survives its starter being swapped) — and time ARRIVES as `TimerFired{id}`, delivered by the
**TimerService** (`zengine-timer`, holding `zengine.timer`). `CancelTimer`/`CancelAllMyTimers`
cancel; re-asking with the same id **replaces** the schedule (an upsert — also how a cadence
changes, and how a successor takes over a role beat instead of doubling it).

The service is the one place in the running system that owns a monotonic clock and the one
nap. It runs on its own **beat chain**: the host sends the first `Drive` (the wind, once, at
boot — after the boot list has actually loaded), and the service re-sends Drive to its own
role forever after — nap to the soonest deadline (capped at 10ms), fire what came due, re-wind.
Pumping the bus IS running the world, paced by the one legal sleep. On its first beat it
publishes `TimerReady` (the hello): weaves loaded before the wind hear it and ask for their
beats — which is how anything gets standing execution in a system whose host pumps nobody.

Honest V1 edges, pinned in the suite: the service cannot SEE a requester die (a weave gets no
delivery outcomes and the bus broadcasts no unloads), so a dead requester's directed timer
fires into clean `NoSuchTarget` refusals until cancelled or the service is replaced — weave
ids are never reused, so it can never hit a stranger; the standing heartbeats that must
survive replacement are role-addressed instead, where requester death is a non-event.

**The service's own succession, measured** (the trust-gate audit, 2026-07-26; pinned in
`tests/test_audit_probes.cpp`): `zen.ReloadWeave` of the timer service **keeps** the beat chain
— reload rebinds behind the same WeaveId, so the in-flight `Drive` still finds its sender at
delivery, and the fresh incarnation re-announces so every standing timer gets re-asked.
`zen.SwapWeave` of it **kills** the chain: the incumbent's parked re-wind is refused
`CapabilityDenied` once the incumbent is unregistered (sender-death, not role vacancy), and the
successor never gets a first message to announce on. Only a fresh wind re-lights it. Two other
measured edges: a second wind is a permanent second chain (conserved, never coalesced — the
cost is a halved per-chain cadence, not a faster clock), and a time-hungry weave loaded *after*
the wind never hears the spent hello and stays deaf. Re-lighting liveness after a swap, and a
birth notice for latecomers, are the open lifecycle questions (R2), not solved here.

## `input/` — the Input package

The floor games sit on: exactly one Input weave (`zengine-input`, holding the `zengine.input`
role) is the sole producer of the five locked shapes — `KeyPressed`/`KeyReleased` (SDL scancodes
as the wire identity of a key; `name` is convenience, never authority), `MouseButton`,
`MouseMoved`, `MouseWheel` — and the only code that talks to the platform. Consumers only
accept; there is no polling API. Backends today are the ones snake runs on: the POSIX terminal
(raw mode; strokes synthesize press+release) and the Win32 console (real key transitions, mouse
records); an SDL **Reader** (the window's own input, including its close box) is the named
follow-on now that the Surface package gives it a window to read. The weave arranges its own
execution: on the TimerService's hello it asks for a repeating role-addressed beat
(`zengine.input.pump`, 10ms — the package owns its own pace now) and polls on each firing;
`PumpInput` stays as the same hands on direct request, for suites and timer-less hosts.

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
Reader phase makes the window an ear too), and it keeps itself answering its OS: a skin's
first breath asks the Timer package for the `zengine.skin.pump` role beat (10ms), and the
beat services the window's event queue even when the world publishes nothing (a dead world
starves a frame-driven pump; the OS calls the result "not responding"). Role-addressed is the
load-bearing half: the beat belongs to the SLOT, so a swapped-in skin inherits it without
asking. Terminal media no-op the beat, exactly as they no-op'd the old host-sent pump;
`PumpSurface` stays as the same hands on direct request, for suites and timer-less hosts.

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
- **Clock** (`snake-clock`) is snake's time binding — the controls move, pointed at time: it
  asks the Timer package for the 120ms repeating beat (`snake.tick`, the cadence the host
  used to hard-code, now living where the pace belongs) and turns each `TimerFired` into
  `SnakeTick`, sent to the world **by role** so time survives the world being swapped. The
  world never learns where ticks come from; only the source of time moved.
- **Host** (`zengine-snake`) owns the boot list — nothing else. It reads no keys (the Input
  weave produces them), owns no screen (the skin claims it at load), and since the Timer
  package keeps no clock, never sleeps, and pumps nobody: it boots the cast, winds the clock
  with one `Drive`, and then `pump()` IS the game — the loop ends when the operator's quit
  key stops the bus (or, honestly, when the bus goes quiet because no clock is deployed).
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
