# Zengine

Zen's **default set** — the engine-shaped weaves most projects will want, built on the Loom.

Documentation router: **[docs/README.md](docs/README.md)** (Timer guides,
reference, laws, decisions). Substrate documentation lives in
[../Loom/docs/](../Loom/docs/README.md); agents start at
[AGENTS.md](AGENTS.md).

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
cmake -DZEN_BUILD_DIR=build -P tests/verify.cmake

# the sanitizer lane — the same suites, instrumented (ASan + UBSan)
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install" -DZENGINE_SANITIZE=ON
cmake --build build-san -j"$(nproc)"
cmake -DZEN_BUILD_DIR=build-san -P tests/verify.cmake
```

**The dev override — the sibling path** (`-DZEN_LOOM_DEV=ON`): `add_subdirectory(../Loom)`,
for editing the two trees together without an install round-trip:

```sh
# Linux / WSL
cmake -S . -B build-dev -DZEN_LOOM_DEV=ON
cmake --build build-dev -j"$(nproc)"
cmake -DZEN_BUILD_DIR=build-dev -P tests/verify.cmake
```

On **Windows** — under **MinGW-w64 and MSVC alike** — the dev override is also the turnkey
path, because it is the one that brings a kernel: dev mode defaults
`LOOM_ENABLE_WINDOWS_KERNEL=ON` — the Loom's explicit development/demo backend (**no
isolation**; the Loom prints its banner and `Kernel::containment_note()` says so) — so the
snake package and its suite build and run natively. Pass
`-DLOOM_ENABLE_WINDOWS_KERNEL=OFF` to decline. With MinGW the runtime DLLs must be on
`PATH` (or beside the binaries) to run.

```powershell
# Windows / MinGW — one flag more than it used to be, on purpose
cmake -S . -B build-win -G Ninja -DZEN_LOOM_DEV=ON
cmake --build build-win
cmake -DZEN_BUILD_DIR=build-win -P tests/verify.cmake
```

```powershell
# Windows / MSVC — from a Developer PowerShell (or after vcvars64.bat)
cmake -S . -B build-msvc -G Ninja -DZEN_LOOM_DEV=OFF "-DCMAKE_PREFIX_PATH=<loom prefix>"
cmake --build build-msvc
cmake -DZEN_BUILD_DIR=build-msvc -P tests/verify.cmake
```

Zengine adds **no MSVC-specific flag of its own**, and that is the point: Loom's public
macro surface needs `/Zc:preprocessor`, and `loom::core` carries it as an interface
requirement, so a consumer inherits it by linking. A compatibility flag copied into this
repo would mean the package had stopped carrying its own law. Tested on MSVC 19.50 (Visual
Studio 2026) x64; clang-cl and ARM64 are unverified, and the Linux-only sandbox is
unaffected by any of this.

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

Zengine's green is seven tests: the **smoke** (link the Loom's exported surface, drive a value
through the real gate, confirm the gate **refuses** a malformed candidate — the refusal is what
makes it a proof instead of a greeting), the **snake suite** (`tests/test_snake.cpp`) — the
Stage 2 vertical slice proven headless: the locked contract pinned by content-id, the simulation
and the v1→v2 migration as pure math, the three live-evolution moments driven end to end
through real `.so` weaves, the real kernel, and the real Weave Manager, and the phase's negative
space (a skinless game writes **zero bytes** to stdout, with a painted-bytes negative control) —
the **timer suite** (`tests/test_timer.cpp`): the Timer package's contract by content-id, every
schedule (one-shot, repeat, upsert, clamps, cancels, role succession, honest vacancy, the
dead-requester floor) over a fake clock through a real bus, the activation law (a chain authored
from an activation; premature, duplicate, foreign, stale and replayed beats establishing
nothing), the real `.so` authoring and re-seeding its chain on the real clock, the load-order
matrix, the migration chains — the world ticking, the input weave polling, and the skin
servicing its medium with nobody pumping them — and (R2B-0) the **continuity lane**: the letter
and the order model pinned as units, then proven end to end through real libraries, the real
kernel, the real steward and a real graceful replacement, over a *virtual* clock so that "a
five-second one-shot had two seconds remaining, and the successor fired it two seconds later"
is an exact integer nobody had to sleep for — the **input suite**
(`tests/test_input.cpp`): the Input package's locked contract and SDL-scancode identity pinned
by content-id and literal value, both backends' translations as pure math on every lane, the
weave's publish path and its self-arranged beat through a real bus, and the keys-become-turns
chain through the real libraries — and the **surface suite** (`tests/test_surface.cpp`): the
Surface package's contract by content-id, the terminal skins as golden bytes, the SDL skin's
frame plan as pure math on every lane, the hello handshake (which now also asks for the skin's
beat) and the one-owner rule through the real kernel, the granted-operator speaking recipe, the
general canvas as golden bytes (roles, paint order, labels over rects, clipping, the unknown-role
fallback, and a canvas claiming the surface exactly as a board does), and (where built) the SDL
skin driven by the same intent under SDL's dummy video driver — the **workshop suite**
(`tests/test_workshop.cpp`): the authored shapes by content-id, identity-is-not-the-name, the
typed property connection (a live read through the semantic surface, a commit that writes
through it, and the two ways a commit can fail told apart with the property untouched by both),
the one-call-per-property reuse pin, authored-versus-resolved as two facts only one of which
moves, hit testing against the real authored objects, the maker's own gestures (a fresh identity
that is neither the label nor the index and is never handed out twice, the post-delete selection
rule, an empty document reached by deleting and left by creating, a nudge and a drag authoring
placement through the one operation a typed edit also goes through, and a refused move writing
neither coordinate), and whole screens asserted as `SurfaceCanvas` values including a live draft
and a refusal — and the
**trust-gate probes** (`tests/test_audit_probes.cpp`): a different KIND of suite, kept
deliberately. It pins what the substrate measurably does to a live beat chain when the timer
service itself is swapped, reloaded, double-wound, or joined late — *including where that was
unwanted*. Read its header before changing it. All four probes were flipped by R2A-2 into
witnesses of the earned promise while keeping every measured half: probe A still asserts that a
swap kills the incumbent's parked beat (`CapabilityDenied`, sender-death), and now also that the
activated successor authors a new chain. It deliberately measures the **hard** path; since
R2B-0 the service converses about its own succession, so the graceful path — and the continuity
it buys — is the timer suite's.

### The sanitizer lane — the other half of the evidence (W-3a)

`-DZENGINE_SANITIZE=ON` builds Zengine's own targets with **AddressSanitizer +
UndefinedBehaviorSanitizer** (`-fsanitize=address,undefined -fno-omit-frame-pointer
-fno-sanitize-recover=all`), and `tests/verify.cmake` then runs the *same* population under
them. CI runs it on every push and pull request; there is nothing to remember.

**Two lanes, two questions, and neither substitutes for the other.** The ordinary verifier asks
whether the population this repository meant to run existed, ran and passed. It cannot ask
whether the code that passed did so while reading freed memory or overflowing a signed integer,
because a wrong answer is not the failure mode — *no answer changing at all* is:

| | ordinary lane | sanitizer lane |
|---|---|---|
| a `Placed` bound into a temporary `Scene` | **passes** | ASan `heap-use-after-free` |
| `resolve_extent` without its overflow guard | **passes** | UBSan `signed integer overflow` |

Both rows are measured, and both are real history rather than invented hazards. W-2 shipped the
first one in committed test code; W-3 found signed-overflow UB in `ui::Rect::contains` — shared
`ui/` code, on the ordinary Workshop press path. Each was caught by a hand-built sanitizer tree
that existed for one phase and was then thrown away, and each had been called green by the
ordinary lane.

The instrumentation reaches the targets **this repository authors**, not the Loom it consumes: a
stranger-path Loom arrives already compiled, and the Loom runs this same lane over itself next
door. ASan's allocator is process-wide either way, so a Loom allocation freed and then read by
Zengine code is still caught; a fault entirely inside Loom's compiled objects is Loom's lane's
job. Both defects above were in Zengine's own code.

The lane runs the **full** population, the SDL skin included — the verifier prints `gates
active: always;sdl`, and every entry the ordinary lane declares is here at the same floor. No
floor is lowered and no gate is turned off to buy the instrumentation. The entries and their
floors are in [tests/test_population.txt](tests/test_population.txt); this paragraph used to
name a couple of them and both had gone stale. A new target lists `zengine-sanitize` beside
`zengine-warnings`; leaving it off does not fail the build, it just quietly leaves that target
out of the witness.

## `timer/` — the Timer package

Time as a service: the `zengine-timer` weave owns the monotonic clock and the
one nap in the system, holds the `zengine.timer` role, authors one beat chain
per activation, and carries schedules across its own replacement as remaining
durations. The package documentation lives in `docs/`:

- **use it** — [docs/guides/timers.md](docs/guides/timers.md) ·
  [docs/guides/timed-weaves.md](docs/guides/timed-weaves.md)
- **exact semantics** — [docs/reference/timer-protocol.md](docs/reference/timer-protocol.md) ·
  [docs/reference/timer-continuity.md](docs/reference/timer-continuity.md) ·
  [docs/reference/timer-binding.md](docs/reference/timer-binding.md)
- **invariants** — [docs/laws/timer-laws.md](docs/laws/timer-laws.md) (TIMER-01..05)
- **why durations, not deadlines** —
  [docs/decisions/timer-continuity-carries-remaining-duration.md](docs/decisions/timer-continuity-carries-remaining-duration.md)

The pre-consolidation package manuscript (~415 lines of design narrative) is
frozen unabridged at
[docs/history/pre-r2c/README.md](docs/history/pre-r2c/README.md).

## `input/` — the Input package

The floor games sit on: exactly one Input weave (`zengine-input`, holding the `zengine.input`
role) is the sole producer of the input shapes and the only code that talks to the platform.
Consumers only accept; there is no polling API.

**The law (W-4): Input reports coherent MOMENTS; applications interpret GESTURES.** A moment
carries everything the backend already knew when one thing happened, so no consumer has to
reconstruct a fact the platform had already stated. A drag, a resize or a selection is
application meaning and is not spoken here at any version.

| shape | what it preserves |
|---|---|
| `KeyPressed` / `KeyReleased` v2 | which key changed state, plus the **modifiers held at that transition**. SDL scancodes are the wire identity; `name` is convenience, never authority. |
| `TextEntered` v1 | **what the user actually typed**, UTF-8, as the platform's own keyboard layout produced it. The only truthful route to a character — nobody computes `Shift+5 -> %`. |
| `PointerMoved` v1 | position, delta, coordinate **space**, modifiers. |
| `PointerButton` v1 | button, transition, **the position it happened at**, space, modifiers. |
| `PointerWheel` v1 | notches, position, space, modifiers. |

Positions are int64 and carry a `space` (`kCells` on both current backends, `kPixels` declared)
so a terminal cell can never be mistaken for an SDL pixel. Editing controls are keys, never text:
Backspace, Enter and Escape arrive as transitions, and what they *mean* is the application's.

Backends today are the ones snake and Workshop run on. The **POSIX terminal** parses raw-mode
bytes with a *stateful, incremental* parser — an OS read boundary is not an event boundary, so a
mouse report split across reads is rejoined rather than translated into the keystrokes its bytes
happen to spell; a lone `ESC` is held until an empty poll resolves it as the Escape key. Pointer
reports are SGR (`ESC [ < b ; x ; y M/m`), 1-based and translated to the 0-based contract. The
**Win32 console** reads `INPUT_RECORD`s: `uChar.UnicodeChar` is the text, `dwControlKeyState` the
modifiers, `dwMousePosition` the position — all present on the record and all now preserved. An
SDL **Reader** (the window's own input, including its close box) is the named follow-on.

**Who turns the terminal's pointer on:** the **Skin**, because terminal modes are output and the
output stream is already claimed and released on the Skin's own lifetime (`surface/skin_tui.hpp`).
Input never writes a byte to the terminal; it parses SGR reports whenever they arrive, and they
only arrive because a Skin asked. The two packages need no coordination surface between them.

The weave arranges its own execution: on the TimerService's hello it asks for a repeating
role-addressed beat (`zengine.input.pump`, 10ms — the package owns its own pace) and polls on
each firing; `PumpInput` stays as the same hands on direct request, for suites and timer-less
hosts.

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
the painter being replaced mid-game).

`SurfaceCanvas{width, height, rects, labels}` is the **general** canvas: an extent in cells,
filled `SurfaceRect`s in painter's (list) order, and `SurfaceLabel` text runs over them. Each
element carries a semantic **role** — `kFill`/`kAccent`/`kMuted`/`kAlert` — never a colour, so
the terminal media pick an SGR *and a glyph* per role (colour alone would be a lie on a
monochrome terminal) while the SDL medium picks RGB, from one unchanged publisher. Cells, not
pixels: a cell is the coarsest unit a terminal can address, so a canvas lands somewhere real in
every medium. It is a *drawing* vocabulary and pointedly not a layout one — no parent/child, no
anchors, no percentages; whoever publishes has already decided where things go. A skin treats a
canvas exactly as a board (same hello, same first-frame flag, same `frames` counter — it is the
same act), an unknown role paints as `kFill` rather than vanishing, and elements outside the
extent are the skin's to clip. **Both media draw labels**: a terminal already owns a font, and
the SDL medium carries its own 6×6 bitmap face (`surface/skin_sdl_glyphs.hpp`), so a canvas
whose meaning lives in its labels is readable in a window as well as in a terminal. That face is
deliberately debug-grade and covers printable ASCII 0x20–0x7E and nothing else; any other byte —
a control character, or any byte of a multi-byte UTF-8 sequence — renders as a visible unknown
box and is never dropped, because a character that silently disappeared would be the
labels-vanish defect again at character granularity. `SurfaceText` — the named *slot*
lines, which are a different shape — still lands in the window's *title*, because the canvas
occupies the whole window.

`SurfaceExtent{width, height}` is the one fact that travels the *other* way — a medium
answering how much room it has, in canvas cells. Every other shape here is intent flowing
publisher → skin; this is the only one flowing skin → publisher, and it exists because
"how many cells is there room for" is a fact **only the medium holds**. The active skin
publishes it when the answer CHANGES and at no other time (its own 10ms beat is what notices
a person dragging a window edge), and a medium with no answer — every terminal skin, and a
window skin before its window exists — publishes **nothing** rather than publishing zeroes:
"I have no opinion" and "there is no room" are different sentences. It is an *offer*: a
publisher that ignores it keeps publishing whatever extent it likes and the skin clips, which
is the contract `SurfaceCanvas` already states.

The Workshop package is the live consumer that pulled the canvas in. `SnakeVisual`
remains the V1 payload the skins also accept directly — that named coupling is **not** dissolved:
re-expressing snake's proven frames through the canvas is its own evidence-carrying move, and a
general shape existing is not permission to migrate a proven one through it.

Three skins ship: **`zengine-skin-tui-classic`** and **`zengine-skin-tui-block`** (the old
snake drawers' looks, now living where drawing lives — the terminal medium is one header,
golden-byte tested), and **`zengine-skin-sdl`** — a real window, same intent, zero
medium-specific fields added anywhere (the agnosticism proof). The SDL skin is the only
target that sees SDL: it fetches a **pinned static SDL3** where none is installed
(checksum in the build; `-DZENGINE_SDL_SKIN=OFF` declines), plans every frame as pure math
(`skin_sdl_plan.hpp`, pinned on every lane), and degrades gracefully with no display — the
suite drives it under SDL's dummy driver, and the window title carries the text slots.

The SDL window **is an ear as well as a surface** (G-1): it was created not-focusable while
the terminal was the game's only ear, and the flag came off when the SDL Reader made the
window able to hear. It keeps itself answering its OS: a skin's own activation asks the Timer
package for the `zengine.skin.pump` role beat (10ms), and the beat calls `SDL_PumpEvents` —
which gathers OS input INTO the process-global queue and removes nothing, so the queue still
has exactly one owner and it is still the Input reader. That servicing happens even when the
world publishes nothing (a dead world starves a frame-driven pump; the OS calls the result
"not responding"). Role-addressed is the load-bearing half: the beat belongs to the SLOT, so a
swapped-in skin inherits it without asking. Terminal media no-op the beat, exactly as they
no-op'd the old host-sent pump; `PumpSurface` stays as the same hands on direct request, for
suites and timer-less hosts.

**The window is the person's to resize** (G-2), under one rule with no per-shape special
case: *a window never shows less than the picture asks for, and is otherwise the person's*. It
is created at the size its first picture asks for, that size becomes its **minimum**, it
carries `SDL_WINDOW_RESIZABLE`, and after that it is grown only by a picture that genuinely
does not fit — which is how a snake board that grew mid-run still comes up whole, while a
canvas publisher that heard `SurfaceExtent` never moves it at all. The alternative is two
parties resizing each other: a canvas rounds down to whole cells, so a medium that sized the
window to the canvas would nibble the window a few pixels smaller every time somebody dragged
it. The extent the skin reports is measured from the renderer's own output size, never
remembered, because a person dragging an edge changes that number and no message says so.

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
  package keeps no clock, never sleeps, and pumps nobody. It contributes nothing to time
  either: it queues the boot list and `pump()` IS the game. Loading the timer service is what
  starts the clock — the control door activates it and it authors its own chain — so there is
  no wind, and no boot-pump-then-wind ceremony to order correctly. The loop ends when the
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

## `ui/` — the authored/resolved vocabulary

W-1's slice, and it owns exactly **one** distinction: what a maker *authored* is not what a
viewport *makes of it*. Two headers keep the halves apart, and the second is only ever an
observation of the first.

```text
ui/vocabulary.hpp   Extent{mode, amount}     an authored width or height, one property
                    kRootContext             the identity that is not one: "the root"
                    Element{id, label,       one authored element -- identity, label,
                            context,         WHOSE FRAME its values are read in (W-6)...
                            x, y,            ...authored placement in that frame...
                            width, height}   ...and two authored extents
                    authored_only_v<T>       the compile-time fence, as a question about
                                             ANY type, so an application can ask it too
                    ById / walk_context      finding and following a relationship, with
                                             no viewport and no number in sight

ui/layout.hpp       Viewport{cells_w, cells_h}   what the ROOT frame is made of
                    Rect / Placed{id, rect}      the resolved observation
                    Scene{viewport, items}       authored order == paint order
                    root_frame / resolve_in      authored shape + context = resolved shape
                    resolve_extent / resolve     the ONE place intent becomes geometry
                    frame_in                     the frame an element was read in
                    hit / placed_for             what is under this cell -> the AUTHORED id
```

Four things are structural rather than promised:

- **Resolution needs a context.** `resolve()` takes a viewport, so "how big is this?" is not an
  answerable question about an element alone.
- **A context is a FRAME, and the root is one of them.** W-6's whole addition: the origin an
  element's `x`/`y` are counted from and the span its shares are shares *of*, as one value
  (`Rect`). Before it, that frame was two hard-coded assumptions in one statement of `resolve` —
  origin `0,0`, span the whole viewport. An element now names an *identity* whose resolved
  rectangle supplies it, or says nothing and gets the root's. `resolve` orders its own work by
  dependency, iteratively and on the heap (no depth ceiling, no recursion), and emits its items
  in **document** order, because document order is paint/hit/list order and must not silently
  become dependency order. A chain that cannot reach the root — a cycle, or a source nothing
  carries — is **not placed at all**: an absence, never a guess at the root.
- **The result is a separate value, cached nowhere.** The authored side has no field able to
  hold a resolved rectangle, and the fence makes adding one a compile error — proven *firing*
  by `ui_authored_extent_required` and `ui_resolved_geometry_refused` (a bare `int64_t width`,
  and a resolved `w`/`h` cached beside honest extents), with `ui_authored_element_compiles` as
  the positive control.
- **The resolved side has no wire form.** `Rect`/`Placed`/`Scene`/`Viewport` are deliberately
  not `ZEN_SHAPE`s (asserted against `loom::Shape`), so an observation cannot be serialized,
  poked, or published as though it were content. The authored side *is* content and travels as
  ordinary shapes.

`resolve_extent` is **total for every value the type can hold**, not merely for validated ones:
authored content is a shape, so it arrives from the wire and from a poke as well as from a
checked edit. An out-of-range share is clamped and an absurd span divides before it multiplies
(`span * amount` on unvalidated `int64` is signed overflow — undefined behaviour produced by
data). *What* a legal extent is stays with whoever accepts one; see Workshop's `check_extent`.

What it is **not**: no widget kinds, no stacks, no relational arrangement, and **still no
parent/child** — W-6 changed what that absence means rather than ending it. An element says what
its values are *measured against*; it says nothing about containment, ownership, clipping,
painting or lifetime, and a dependent's rectangle may extend well past its source's with nothing
trimming it. "Put B inside A" is something an application could build on this; Workshop builds
exactly one policy over it (a source with dependents is not deletable) and keeps that policy in
its own document law. Also no colour, no z, and nothing about
painting. `SurfaceCanvas` is the drawing vocabulary; a resolved scene is what you paint *from*.
The Loom's `loom::Widget` + `px_layout` is the *other* model — intent plus **relationship**,
resolved by a renderer — and W-1 measured that it is neither relocatable (the Loom console,
TUI and bridge consume it in-tree) nor able to express an authored placement or an absolute
extent. The two are not competitors and neither replaces the other.

No kernel, no weave, no bus: this package is vocabulary and arithmetic, so it exists on every
configuration.

## `builder/` — a name, a command, and the line between them

The first Zengine package whose subject is an **effect** rather than a picture: building
something means starting an operating-system process, with a real exit status, and nothing
here had ever done that. So the package is arranged as a split, and the split is what it is
for.

- **The tool** (`weave.hpp`) is ordinary. It holds one target **name**, remembers what
  happened to it, and publishes that as `BuildStatus` for any presentation to read. Two grant
  rules: order the runner, and say what it knows. It holds no command and cannot spell one.
- **The runner** (`runner.hpp`) holds the host's catalog of recipes — an absolute program, an
  absolute working directory, an argument vector — and is the only weave in the program that
  starts a process. One grant rule: report a `BuildOutcome` to whoever holds the Builder
  office. A `RunBuild` naming something outside the catalog is refused by name, and nothing
  runs.
- **The wire cannot spell a command.** `BuildRequested` and `RunBuild` carry exactly one field
  and it is a target name; there is no shape here whose field is a program, an argument list,
  a directory or a shell line. "The panel sent a command" is not a sentence this vocabulary
  can express, which is a property of the types rather than of a check.
- **`run.hpp` is not a shell.** It takes a program and an argument vector and runs them —
  `fork`/`exec` with a pipe on POSIX, `CreateProcess` with a pipe on Windows. `popen()` would
  have been four lines and would also have been a general shell capability; a later phase that
  genuinely needs one can add it and argue for it, rather than finding it already here having
  arrived as a side effect of a button.
- **What the split buys is reviewability, not containment.** An in-process weave shares the
  host's address space, so a grant bounds what a weave may *say* and never what it may *touch*
  (Loom `docs/reference/capabilities.md`) — any code compiled into the binary could call the
  same platform functions. What this arrangement gives is one place where process authority
  lives, one grant to read, and one refusal to test. Calling it containment would be the
  overclaim these phases exist to refuse.
- **It blocks.** The runner builds inside its own handler, so the tool that asked for the
  build freezes for the duration. That cost is real, it is measured rather than hidden, and it
  is what a queue-and-poll phase would exist to answer.

No kernel and no loadable weave: both weaves are mounted in-process by whichever host wants
them, exactly as Workshop's own weave is. The suite is `tests/test_builder.cpp`, and it starts
real child processes — every recipe it runs is `cmake -E ...`, CMake's own portable shim, so it
needs no shell and no assumption about what else is installed.

## `workshop/` — the maker-facing surface

A person opens Workshop, **makes** an ordinary authored rectangle, selects it, **moves** it,
inspects a real property, changes it, watches an invalid change refuse, **deletes** it — down to
an empty document and back — **saves** it, closes the program, and **opens it again** to the same
objects with the same identities. It is an ordinary Zengine application holding no privilege
snake does not — keys from the Input weave, time from the Timer service, painting by
*publishing* a `SurfaceCanvas` to whichever skin holds `zengine.skin`. It touches no terminal
and no window itself.

- **The authored object** is a real `zengine::ui::Element` in the weave's own state
  (`vocabulary.hpp`): gated, schema-carrying, `ZEN_EXPOSE`d. There is no shadow model — the
  element a maker selects *is* the one the canvas is painted from and the inspector reads
  through. `id` is the identity and `label` is only a label, so two elements may share a name
  and stay distinct. W-1 moved the *type* out to the `ui/` package; the object is no less
  Workshop's state for being spelled in a shared vocabulary, and there is no alias or wrapper
  left behind to suggest otherwise.
- **Width and height are authored as a `ui::Extent`** — a mode plus an amount, `12` cells or
  `70%` of the workspace — which is **one** property presented as one row even though it is two
  stored fields. The **resolved** cell count is a separate, read-only row: narrowing the
  workspace moves `Resolved` and never touches `Width`.
- **Workshop keeps no geometry.** Since W-1 the painted rectangle, the inspector's `Resolved`
  reading and the answer to a click are all derived from one `ui::Scene` (`workspace_scene()`),
  so they cannot come to disagree. W-0 had three call sites doing their own extent arithmetic
  and agreeing only because one person wrote all three. What *is* still Workshop's: the screen's
  furniture (where the object list sits beside the workspace) and every refusal — `check_extent`
  is this document's policy, not the vocabulary's law.
- **The property connection** (`property.hpp`) is the phase's one new abstraction and is
  deliberately Workshop-local: `Property<T>` holds a typed read and a typed write over the
  document's *semantic* operations (`document.hpp`, where every setter can refuse), never a
  member address; `TextForm<T>` is the text conversion written **once per type**, so `Width` and
  `Height` share every line of it; `Row` is a type-erased inspector line carrying an editor
  **draft** that the property never sees until commit. No reflection, no registry, no
  persistence implication.
- **A commit has three outcomes, not two:** accepted, *unparseable* (`banana` is not an extent),
  and *refused* (`500%` is an extent the setter rejects, with its own reason). A maker fixes the
  first by retyping and the second by wanting something else, so they are never collapsed. A
  failed commit leaves the property untouched and keeps the draft, marked as a draft.
- **There is exactly one operation that writes a position** (`doc::move`) and exactly one that
  writes a size (`doc::resize`); `set_x`/`set_y` and `set_width`/`set_height` are those
  operations holding one half still. So a typed `X` in the inspector and a drag on the canvas are
  not two write paths that validate alike; they are one path reached two ways — the only version
  of "shared policy" that a change to one cannot separate from the other. Both are **atomic**:
  both halves are checked before either is written, so a diagonal gesture never half-succeeds
  *and* reports a refusal.
- **A hand STOPS at a boundary; a written value is REFUSED.** Those are different acts and get
  different answers. A drag or a resize key that reaches past what exists is clamped to the
  boundary — in the *gesture* layer (`screen.hpp`), expressed in the document's own limits — and
  the notice says which wall was met (`#1 is now 1% x 6 -- stopped at the smallest size`). A
  value a maker *typed* is refused outright, the authored state is untouched and the draft
  survives (`Width: at least 1 cell`). The document's operations never clamp, so nothing is ever
  silently corrected; in this tool the alert ink means exactly one thing: **nothing was written**.
- **Resizing preserves the authored mode, and the rounding rule is forced.** Pull a `60%`-wide
  object's corner out to 34 cells and Workshop writes **`71%`**, not `34`. Converting the share to
  cells would destroy the only part of the value that survives the next workspace change. The
  projection (`extent_from_drag`, Workshop-local) is *not* an inverse of `resolve_extent` — that
  function floors and clamps, so it has none — it **authors a new value** that this viewport
  resolves to what the hand asked for, taking the smallest share that reaches it. That "smallest"
  is not taste: the resolver floors, so `nearest` would send 28 cells to 58%, which resolves to
  27, and merely grabbing an edge would shrink the object. And the rule is expressed by *asking
  the resolver* over its hundred candidates rather than re-deriving its arithmetic, so the two
  cannot drift and the projection inherits its totality over hostile values for free.
- **The maker's gestures live in `screen.hpp`, not in the weave.** `create`, `delete_selected`,
  `nudge`, `grow`, `take_hold`/`drag_to`/`end_drag` and `size_handle` are pure-ish functions over
  document + session; `workshop.cpp` binds keys and pointer events to them and reaches the
  document through nothing else. A gesture whose only witness is a keystroke is a gesture no
  suite can pin.
- **The resize handle is Workshop's furniture, not authored content.** It is the `+` on the
  selection ring's bottom-right corner, derived from the selected object's resolved `Placed::rect`
  every time it is wanted, with no identity, no persistence and no place in `zengine::ui`. It is
  drawn with an ordinary `SurfaceLabel`, so no role and no package shape had to be added. Its
  priority over the body on a press is Workshop's own — `ui::hit` still answers only about
  authored elements. When it would fall outside the workspace it is not shown *and* not grabbable:
  what a maker cannot see, a maker cannot grab.
- **Identity survives creation and deletion.** The mint is still `WorkshopDoc::next_id`, it never
  rewinds, and a new object's default label is deliberately the same word the others carry — so
  making one teaches "the name is not the identity" at the moment it is cheapest to learn. The
  post-delete selection rule is stated once and tested: the object that took the deleted one's
  place, else the new last, else **none**.
- **One object can be measured against another** (W-6), and it is one editable property. The
  inspector's `Context` row reads `root` or `#1`; authoring it is the same enter/retype/enter a
  width takes, and it cost one `Row::edit` line plus one `TextForm`. It is **not** labelled
  `Parent`: nothing here is a parent, and the familiar word would promise ownership, clipping and
  cascade-delete that the document does not do. `doc::set_context` is the third instance of the
  one-act pattern `move` and `resize` established — it judges the relationship *and* re-judges the
  coordinates in the proposed frame, because rewiring can make an already-written coordinate
  illegal, and a refused rewire writes neither half. It **does not compensate**: an object rewired
  into a source visibly moves, because a maker who changed what a number is measured from changed
  what the number means, and silently rewriting `x`/`y` would author facts they never touched.
- **`the workspace starts at 0` turned out to be a law about the workspace.** Its own stated
  reason said so — "the workspace has no cells there". A coordinate in another element's frame is
  an *offset*, so `-1` there is ordinary and lands on a cell that exists. `check_coord` now takes
  the context it is judging in; the root's guard is exactly as strong as it was, and it is stated
  as being the root's rather than being every coordinate's by accident.
- **Direct manipulation projects through the context, using the resolver's own answer.** A hand
  speaks workspace cells, so `place` clamps in *global* cells — a dependent dragged left stops at
  the workspace edge where a maker can see it stop — and then subtracts the frame's origin
  (`ui::frame_in`) to get the authored offset, which may be negative. W-2 concluded that a
  position "relative to something else" could not be dragged because the resolver is not
  invertible; W-6 measured that as true of *extents* and false of *placement* — a share floors and
  clamps, a sum does not, so a position composes by addition and inverts exactly. Resizing needed
  **no** new path at all: `extent_from_drag` was always asking "which share of this span reaches
  the hand", and the span is now the context's rather than always the workspace's.
- **A source something measures against is not deletable**, and the refusal names the dependents
  (`#2 and #3 take context from #1 -- change or delete them first`). Cascade-deleting would treat
  dependency as ownership; re-rooting the dependents would rewrite their coordinates' *meaning*
  while leaving the numbers alone. Both are silent semantic edits, so neither happens: rewire,
  then delete, as two authored acts. That policy is **Workshop's**, in Workshop's document law —
  not a property of `ui::Element::context`.
- **The screen** (`screen.hpp`) is a pure function from document + session to `SurfaceCanvas`, so
  the suite asserts whole screens as values. Session facts — selection, workspace extent, drafts,
  a drag in flight — live outside the authored state on purpose. An empty document *says* it is
  empty rather than going blank, because a maker can now reach that state by deleting their own
  work.
- **The screen's extent is runtime, and a bigger surface is a bigger workspace** (G-2). Until
  then it was one pair of constants, because a canvas publisher had no way to learn how much
  room its medium had; `surface::SurfaceExtent` now says so, Workshop takes it, and `Screen` /
  `screen_of` derive every other number from it in one place. Where the extra room goes is
  this application's composition and nothing more general: the **workspace** takes the extra
  columns and rows (`]` reaches the new ceiling), the **panel column** keeps its width and
  moves with the right edge, the bottom band keeps its shape against the bottom edge, and the
  terminal overlay takes **half** of whatever the surface gained. The panel's width and the
  inspector's rows are decisions about how much of each thing is worth showing, not shares of
  a screen, and turning them into shares would be a layout policy nothing here has evidence
  for. The extent is **clamped** at both ends — the minimum is the 78×22 composition, and the
  maximum is arithmetic rather than taste: an extent arrives off the bus as a `ZEN_SHAPE`
  whose fields are whatever the sender put in them, and the terminal rasterizer allocates
  `w × h` cells from whatever canvas it is handed. Nothing authored moves: a cell coordinate
  is in the same cell on both screens and a share resolves to more cells, which is the
  authored/resolved discipline meeting a window edge.
- **The terminal projection keeps the minimum, and that is its own policy rather than a
  stub.** A terminal skin owns no drawable whose size is its to read — it writes into a
  `Sink` that may be a string, a pipe or a console — so it declines the question, publishes
  no extent, and a terminal Workshop paints exactly the screen it painted before G-2. A
  terminal too small shows less of it, which is what a terminal has always done to output too
  wide for it. Nothing here mimics the window medium's resize semantics for symmetry.
- **The host** (`workshop.cpp`) owns the boot list. Its `BootWeave` holds the Manager grant and
  **hears the answers**: a root `bus.send` of `zen.LoadWeave` has no asker, so the Manager's
  relay forwards nothing and the load silently never happens
  (`loom::forward_for`) — found by running it, and the reason boot answers have an addressee.

- **The document survives the process, and only the document does** (W-5). `Ctrl+S` writes the
  authored objects to one file and `Ctrl+O` reads them back; `--document <path>` says which file,
  and the status line names it and says `saved` or `UNSAVED`. What is written is exactly
  `WorkshopDoc` — identity, name, authored place, both halves of each extent, the object **order**,
  and `next_id` — and nothing else. The selection, the workspace extent, an editor draft and a
  drag in flight are *session*; the resolved scene, every `Rect`, the resolved size and the size
  handle are *derived* and rebuilt. The sharpest consequence: save under a 48-cell workspace and
  load under a 36-cell one, and the authored `61%` is byte-identical while `Resolved` reads
  `29 x 6` before and `21 x 6` after. **`next_id` is in the file** because it cannot be
  reconstructed: `max(surviving ids) + 1` would recycle the identity of an object deleted before
  the save, so a maker who made `#3`, deleted it and came back would find their next object
  wearing a dead one's number.
- **The file is JSON, written through the Loom's own compat codec** (`workshop/persist.hpp`), not
  a parser written here — so a document and a message are refused by the *same* gate, the
  materialization budget already bounds a hostile file, and W-5 added no dependency. The file has
  its own three small shapes rather than serialising the weave's state, so renaming a member
  cannot silently change a maker's file, and an extent mode is the **word** `cells` or `percent`
  rather than the integer it is in memory. Output is deterministic, so `save → load → save` is
  byte-identical and a document is diffable. Loading is a **transaction** (`persist::load_into` →
  `doc::restore`): the candidate is judged whole — format, then version, then the document law —
  and a refusal leaves the live document, the selection, the drag and any draft exactly as they
  were. Unknown fields are **rejected**, not dropped; an unsupported `format_version` fails
  closed, naming the number. There is one version and deliberately no migration machinery.
  W-6 added one field — `context`, written as the **identity** it is (`0` is the root, and a
  `static_assert` pins that 0 can never become an identity an object carries). Whether `#4` exists,
  and whether following it comes back around, is **not** checked there: that is the document's law,
  asked once, in `doc::check_document`, so an interactive rewire and a loaded file are refused in
  the same words. The relationship is persisted; its *result* is not — no frame, no global
  position, no cell count, no traversal order. Adding the field changed the written shape, so a
  W-5-era document no longer admits: refused, closed, by the Loom's gate. That is stated rather
  than papered over — Workshop is pre-release and its own only consumer, `format_version` is
  **not yet a public compatibility boundary**, no migration layer or legacy reader was built, and
  the number was not bumped for ceremony.
- **Saving never touches the document and never destroys the last good one.** Serialization takes
  a `const&` and normalises nothing (`60%` is not written as the 28 cells it currently resolves
  to). The writer serialises the complete candidate, writes a sibling `.saving` file, checks it,
  and only then renames it over the destination — so a detected write failure leaves the previous
  save byte-identical. No crash durability is claimed: nothing calls `fsync`.
- **A load re-establishes the session rather than preserving it.** The drag is cancelled, the
  inspector is rebuilt (so drafts end), and the selection is set by the same rule that opens a
  fresh Workshop — the first object, or none — because keeping the old id would silently alias
  whatever new object happened to carry that number. A `Ctrl+S` while a row is being edited is
  **refused**, naming the row: the alternatives were writing the old value while a new one is on
  screen, or auto-committing a value the maker never confirmed.

Keys: `n` makes an object, `d` deletes the selected one, `hjkl` moves it a cell at a time,
`Shift+hjkl` resizes it, `tab` cycles objects, `up`/`down` walks inspector rows, `enter` edits,
`esc` cancels, `[`/`]` resizes the workspace, `Ctrl+S` saves, `Ctrl+O` opens, `q` quits.
`Ctrl+S` is byte 0x13 — XOFF — and reaches the application only because the Input weave's
terminal reader clears `IXON` when it takes raw mode; the modifier is then *measured*, not
inferred. The move gesture is `hjkl` and not the arrows because the arrows already step the rows and
Workshop has no focus concept that would let one pair of keys mean two things — inventing one to
free the arrows would be a focus system built to serve a keybinding. **Resize is `Shift+hjkl`**:
one gesture family spelled two ways, rather than two families competing for free keys. W-3 could
not say that — the wire had no modifiers, so a second directional gesture cost four more literal
keys (`,` `.` `-` `=`), and those four bindings are gone.

**Press-drag-release is the real gesture** — on the body to move, on the `+` handle to resize —
and the keyboard is the fallback; both end at `doc::move` and `doc::resize`. Since W-4 that is
live on the **canonical POSIX/TUI lane** as well as the Win32 console: the Skin asks the terminal
to report its pointer, Input parses the reports incrementally, and `PointerButton` carries the
position the press happened at — so Workshop grabs from the press itself rather than from the
last motion event, which W-2 measured could be arbitrarily stale. Text is likewise the platform's
own: `%` and capital letters are ordinary `TextEntered`, Workshop maps no key to any character,
and the `70p` workaround the extent parser existed to accept is no longer needed to type `70%`.

**The terminal overlay wraps** (G-2). `Shift+Space` opens a pane on the ordinary
`loom::TerminalSession` the host mounted, anchored to the screen's bottom-right corner. A
transcript entry is rendered whole and then spends as many of the pane's rows as its sentence
needs (`detail::wrap`, continuation rows indented two cells); nothing upstream is shortened to
suit a pane, exactly as `Session::notice` keeps its whole sentence and `detail::fit` bounds
only what is shown. Before this, an entry was fitted into one 56-cell row — so the pane's own
syntax notice, the one thing a maker can ask it, arrived as its first fifty-three characters
and `...`. The pane fits **entries**, not lines: `entries_that_fit` is the one place that
arithmetic lives and both the snapshot and the painter call it, so the omission marker cannot
come to lie about rows spent on wrapping. Discoverability is unchanged and deliberately so —
any line the pane does not recognise (including `help`) answers with the grammar, and the pane
still speaks two verbs.

### Dynamic panels: Builder and Info (BLD-0, PNL-0, PNL-1)

> A weave may provide a tool; a **panel** is its presentation.

`[+ panel]` on the screen's title row (`p`) opens a small picker over the catalog of panel
kinds Workshop knows how to present (`panel.hpp`). The catalog is a constant array of
Workshop's own — a panel that is not in it cannot be opened, because the picker is the only
door — and it holds two kinds, chosen to be unalike:

| kind | presents | behind it |
|---|---|---|
| `Builder` | one known build target and its last outcome | a weave holding `zengine.builder` |
| `Info` | the `OBJECTS` list and the `PROPERTIES` inspector | nothing — the document and the session |

`panel == weave` is deliberately **not** an architectural rule, and `Info` is what pays for
that sentence rather than asserting it: opening it sends no message, asks no office and needs
no weave mounted anywhere, and it has no per-panel state for a close to destroy. A Workshop
hosting no tools at all opens it and it works.

**The picker owns panel presence** (PNL-0). One door, both directions:

```text
closed panel  ->  select  ->  open
open panel    ->  select  ->  remove
```

so the picker lists each kind as `open` or `closed` beside its name — a toggle whose current
state is invisible is a gesture a maker has to guess at. BLD-0 spelled removal `x`, which was
unambiguous while one kind existed; a second kind would have made that key choose a panel, and
choosing means either a per-panel binding or a focused panel. Both are frameworks this Workshop
has declined, so presence moved wholly to the picker and `x` is an unbound key again.

**`Info` is open at boot**, and until PNL-0 those two columns were not a panel at all: `paint`
drew them unconditionally, and the only way to not have them was to edit `paint`. What the
migration moved is where they are painted from; what a maker sees at boot is byte-identical.

- **The panel is not the tool.** The Builder panel holds a *copy* of the last `BuildStatus` the
  Builder tool published, and closing the panel destroys the copy and nothing else. Reopening
  it sends `builder::StatusRequested` and shows the tool's own answer — including `asks N
  ever`, the tool's running count, which comes back as 3 rather than as 1 and is the number a
  panel that owned the state could not produce.
- **Workshop gained two sentences and no powers.** Its grant adds `StatusRequested` and
  `BuildRequested`, both scoped *to the Builder office*. It cannot reach the runner, and the
  only build it can ask for is the one the tool has already named — a panel that has not heard
  from its tool cannot ask for anything, and says so.
- **There are two places, they are named, and there is no layout policy** (PNL-1). A kind
  DECLARES its place in the catalog — `placement::kOverlayStack` or `placement::kSideRegion` —
  and one function turns a place plus a screen into the rectangle that panel occupies:

  ```text
  panel kind  ->  placement intent (panel.hpp)  ->  placement_bounds()  ->  the painter is
                                                                           handed that rect
  ```

  The **overlay stack** is anchored to the canvas's top-left, exactly the workspace's width at
  the minimum screen, stacked downwards — the terminal overlay's mechanism pointed at the other
  corner — and it covers the top of the material a maker is building. The **side region** is
  the fixed right-hand column `Info` has always been; it holds exactly one panel, and a second
  kind declaring it is a compile-time refusal rather than two panels painting over each other.
  A slot is earned by being *placed in the stack*, so an `Info` ahead of a `Builder` in the open
  list never pushes it down a slot it does not occupy. `bounds_of(panels, kind, screen)` is the
  one path to an open panel's bounds — a closed one answers with an empty rectangle rather than
  with the place it would have had. Before PNL-1 each painter carried its own column and the
  two places existed only as agreement between them; what a third kind costs now is a catalog
  row and a painter, neither of which is geometry. Docking, tabs, saved layouts, dragging,
  resizing and focus are all still absent, and what using two unalike panels felt like is the
  evidence for whichever of them gets built.
- **Removing `Info` leaves its 28 columns empty, deliberately.** Giving them to the workspace
  would not be a tidier layout — the workspace's extent is what a share resolves against, so
  every `%`-wide object on screen would change size because a maker hid a list of names. A
  panel's presence must not be visible in the picture of the document.
- **No focus framework.** Four modes, in priority: the terminal overlay, the panel picker, an
  open inspector draft, then command mode. `p` and `b` were unbound keys and `b` still does
  nothing with no Builder panel open. The inspector's own keys (`up`, `down`, Return) belong
  to `Info`: with it removed they say so instead of driving rows nobody can see, which would
  otherwise open a draft that no screen shows and that `^s` would then refuse to save over.
- **A synchronous build has no in-flight frame.** Pressing `b` paints one frame — `asked --
  waiting for it to finish` — because Workshop's own repaint is queued ahead of the order that
  reaches the runner; after that the runner blocks the pump that would carry the next one. What
  a maker sees is the truth about what this mechanism can do, not a spinner over a freeze.
- **Announcing and learning are different.** A status that arrives for a build this panel
  asked for is announced on the notice line; one that merely arrives — the answer to a reopen —
  is shown in the panel's rows and never announced. The first live run got that wrong out loud,
  saying `built zengine-snake -- exit 0` about a build that had finished minutes earlier.

The target is `zengine-snake`, and the recipe is the target's own: `${CMAKE_COMMAND} --build
<this build tree> --target zengine-snake`, both baked at configure time. It is deliberately not
one of the weaves this running Workshop has loaded — building one of those would overwrite a
shared library the process has mapped — and `zengine-workshop` rebuilding itself is the same
hazard aimed at the host binary, which is Build+Load's problem and not this phase's.

Workshop's weave lives in `workshop/weave.hpp`, not in the host's translation unit — so the
suite mounts it on a real bus and walks `input message -> gesture -> semantic operation` end to
end (W-4, closing P16). It is mounted **in-process**: nothing asks to unload it, so the
reloadable-weave machinery would be ceremony bought with nothing. The weaves it *loads* are
other packages'. The host gates on `if(TARGET loom::kernel)` like snake's.

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

## License

Zengine is licensed under MPL-2.0.
See [LICENSING.md](LICENSING.md) for the plain-language boundary
and [LICENSE](LICENSE) for the legal terms. The standing principle —
the Loom is everyone's, Zengine is the default set, your weaves are
yours — is exactly what the license implements: what you create with
Zen is yours.
