# Zengine cheat sheet

Dense operational reference. Everything here is checked against the source it describes.
Prose and reasoning live in [docs/](docs/README.md); this page is for looking things up.

Where something is genuinely awkward today it says so in a **⚠ friction** note rather than
showing syntax that does not exist.

---

## Build

```sh
# the Loom, installed (Zengine consumes it as a package, not a subdirectory)
cmake -S Loom -B Loom/build -DCMAKE_BUILD_TYPE=Debug
cmake --build Loom/build -j
cmake --install Loom/build --prefix Loom/build/_install

# Zengine against it
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install"
cmake --build build -j

# the official verification lane (NOT a bare ctest -- see below)
cmake -DZEN_BUILD_DIR=build -P tests/verify.cmake
```

| I want | flag |
|---|---|
| sibling Loom source instead of an installed one | `-DZEN_LOOM_DEV=ON` |
| no SDL window skin (no SDL3 fetch) | `-DZENGINE_SDL_SKIN=OFF` |
| ASan + UBSan over the same suites | `-DZENGINE_SANITIZE=ON` (use build dir `build-san`) |
| extra CTest flags through the lane | `-DZEN_CTEST_ARGS=-V` |
| the Windows kernel backend (dev/demo only, **no isolation**) | `-DLOOM_ENABLE_WINDOWS_KERNEL=ON` (implied by `ZEN_LOOM_DEV=ON` on Windows) |

**Windows / MSVC** — from a Developer PowerShell:

```powershell
cmake -S . -B build-msvc -G Ninja -DZEN_LOOM_DEV=OFF "-DCMAKE_PREFIX_PATH=<loom prefix>"
cmake --build build-msvc
```

Zengine adds no MSVC-specific flag: `loom::core` carries `/Zc:preprocessor` as an interface
requirement, so linking inherits it.

**Why `verify.cmake` and not `ctest`.** A bare `ctest` runs the tests but cannot tell you
whether the population that ran is the population this repository meant to run — a deleted
registration or a filter matching nothing both exit 0. `verify.cmake` checks the inventory in
[`tests/test_population.txt`](tests/test_population.txt) first. Quote the lane, not the ctest.

Per-repo green: Zengine's lane never re-runs the Loom's suite. Say which repository's green
you proved.

---

## Zengine C++

### Define a weave

```cpp
#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

struct MyState {
    std::int64_t n = 0;
    ZEN_EXPOSE();                                  // makes the state pokeable/inspectable
    ZEN_SHAPE(MyState, 1, ZEN_FIELD(n));           // name, version, fields
};

class MyWeave : public loom::WeaveBase<MyWeave, MyState,
                                       loom::Accept<Order>,     // what may arrive
                                       loom::Emit<Report>> {    // what may leave
public:
    void on(const Order& o, loom::Mail& mail) { mail.publish(Report{o.n}); }
};

ZEN_EXPORT_WEAVE(MyWeave)                          // only for a loadable .so/.dll
```

- `Accept<>` and `Emit<>` **are** the manifest. Loom refuses a shape you did not declare in
  either direction; there is no wildcard.
- `state_` is the protected member `WeaveBase` gives you. It is the only state a weave has.
- One handler per accepted shape, always `void on(const Shape&, loom::Mail&)`.

### Declare a shape

```cpp
struct Order {
    std::string what;
    std::int64_t n = 0;
    ZEN_SHAPE(Order, 1, ZEN_FIELD(what), ZEN_FIELD(n));
};
```

Version is the second argument and is part of the wire identity: `Order` v1 and v2 are
different shapes to the gate. Bump it when fields change.

### Send a message

| I want to reach… | write |
|---|---|
| everyone who accepts it | `mail.publish(Report{...})` |
| one specific weave | `mail.send(id, Report{...})` |
| **whoever holds a role** (survives replacement) | `mail.send_to_role("my.role", Report{...})` |
| as an office I currently hold | `mail.office_send(role, target, ...)` |

Prefer `send_to_role` whenever the recipient is a *slot* rather than a specific instance —
that is what makes the recipient replaceable while your code keeps working.

⚠ **friction — send fate is not observable to the sender.** A send that goes nowhere (no
holder for the role, grant refused, shape unknown to the host) returns no error your handler
can read. The refusal is real and visible on a host-installed observer, not to you. This is a
recorded Loom seam; see *Debugging* below.

### Ask and answer

Loom's request/answer pair is `zen.Ask` / `zen.Answer` with a correlation the asker chooses.
Zengine's own services use plain messages plus a role rather than ask/answer; the Timer's
`TimerResolution` receipt is the pattern to copy — a fact sent back, keyed by the id you
chose. See [Loom's messaging reference](https://github.com/Krealsion/Loom/blob/main/docs/reference/messaging.md).

### Roles and offices

```cpp
inline constexpr const char* kMyRole = "my.service";     // by convention: dotted, lowercase
```

A role is a *slot* one weave holds at a time. Bind it at load (`LoadWeave{stem, path, role}`)
or when a host mounts natively (`bus.register_weave(std::move(w), grant, office)`). Loom
refuses a second loadable weave into a held singleton role.

---

## Timer

The Timer service holds the role `zengine.timer` (`zengine::timer::kTimerRole`) and owns the
only clock and the only sleep in the process.

### An authored rhythm — `TimedWeave`

Use this when the cadence is part of what your weave *is*.

```cpp
#include "timer/binding.hpp"

class Pulse : public zengine::timer::TimedWeave<Pulse, PulseState,
                                                loom::Accept<>, loom::Emit<>> {
public:
    Pulse() : tick_(timers().repeat("pulse.tick", std::chrono::milliseconds(50),
                                    &Pulse::on_tick)) {}

    using TimedWeave::on;        // REQUIRED if you declare any on() of your own

    void on_tick(const zengine::timer::TimerFired&, loom::Mail&) { ++state_.beats; }

private:
    Handle tick_;
};
```

| factory on `timers()` | delivers to |
|---|---|
| `repeat(id, delay, cb)` | this weave's identity |
| `once(id, delay, cb)` | this weave's identity, once |
| `repeat_to_role(id, delay, role, cb)` | whoever holds `role` at each firing |
| `once_to_role(id, delay, role, cb)` | whoever holds `role`, once |

The layer accepts `zen.Activated` / `TimerReady` / `TimerFired` / `TimerResolution` and emits
`EnsureTimer` / `EnsureRoleTimer` / `CancelTimer` on your behalf, and re-places your declared
orders on your activation and whenever the Timer service (re)appears.

| on a `Handle` | does |
|---|---|
| `h.cancel(mail)` | stop wanting it, and tell the service |
| `h.restart(mail)` | re-arm a spent one-shot (the only thing that does) |
| `h.waiting()` / `h.spent()` / `h.canceled()` | local state |
| `h.resolution()` / `h.resolution_reason()` | what the service said it did |

Forget `using TimedWeave::on;` and the build fails naming the fix. Declaring your own
`on(const loom::Activated&, ...)` is refused by name — use `on_timed_activation` instead.

### A runtime delay — the raw protocol

Use this when the delay is *data* (it arrived in a message, a user typed it, a job carries it).

```cpp
#include "timer/vocabulary.hpp"
namespace timer = zengine::timer;

void on(const BakeOrder& o, loom::Mail& mail) {
    mail.send_to_role(timer::kTimerRole, timer::StartTimer{"oven.bake", o.minutes, false});
}
void on(const timer::TimerFired& f, loom::Mail& mail) { /* f.id */ }
```

Declare `Accept<timer::TimerFired, ...>` and `Emit<timer::StartTimer, ...>` yourself.

⚠ **friction — a declared binding's delay is fixed at declaration.** There is no
`h.restart(mail, new_delay)` and no way to add a binding at run time and have it take effect
before the next `TimerReady`. A message-driven delay must use the raw protocol, which means
writing the accept/emit sets, the id filter and the receipt handling by hand. This is the
designed split ([TIMER-05](docs/laws/timer-laws.md)), not an oversight — but it is the split
an author meets first.

### The vocabulary

| shape | direction | meaning |
|---|---|---|
| `StartTimer{id, delay_ms, repeat}` | → service | place a schedule (replaces & re-anchors an existing id) |
| `EnsureTimer{id, delay_ms, repeat, preferred, fallback}` | → service | place it, saying what to do with one that already exists |
| `StartRoleTimer` / `EnsureRoleTimer{..., role}` | → service | the beat belongs to the *slot* |
| `CancelTimer{id}` | → service | remove what **this sender** started |
| `CancelAllMyTimers{}` | → service | all of this sender's |
| `TimerFired{id}` | ← service | it fired |
| `TimerReady{}` | ← service | the service exists; re-place what you want |
| `TimerResolution{id, resolved, reason}` | ← service | the receipt for an `Ensure*` |

Continuity spellings for `preferred`/`fallback`: `kPreserveRemaining`, `kRestartDelay`,
`kDrop`. Receipts: `kResolutionPreserved`, `kResolutionRestarted`, `kResolutionDropped`,
`kResolutionRefused`.

Deeper: [timers guide](docs/guides/timers.md) · [timed weaves](docs/guides/timed-weaves.md) ·
[protocol](docs/reference/timer-protocol.md) · [continuity](docs/reference/timer-continuity.md) ·
[binding layer](docs/reference/timer-binding.md) · [laws](docs/laws/timer-laws.md).

---

## Input

One weave (`zengine-input`, role `zengine.input`) is the sole producer. Consumers only accept
— there is no polling API.

```cpp
#include "input/vocabulary.hpp"
namespace input = zengine::input;

void on(const input::KeyPressed& k, loom::Mail&) {
    const bool ctrl = (k.modifiers & input::mod::kCtrl) != 0;   // modifiers is a bitmask
    if (k.scancode == input::scan::kQ && ctrl) { /* ... */ }
}
void on(const input::TextEntered& t, loom::Mail&) { /* t.text is UTF-8 as typed */ }
```

| shape | carries |
|---|---|
| `KeyPressed` / `KeyReleased` v2 | `scancode` (SDL scancode is the identity), `name` (convenience), `modifiers` at the transition |
| `TextEntered` v1 | `text` — what the layout actually produced, UTF-8 |
| `PointerMoved` v1 | position, delta, `space`, modifiers |
| `PointerButton` v1 | button, transition, position, `space`, modifiers |
| `PointerWheel` v1 | notches, position, `space`, modifiers |

Positions are `int64` and carry a `space` (`kCells` on both current backends, `kPixels`
declared). Backspace, Enter and Escape are **keys**, never text.

Full reference: [docs/reference/input.md](docs/reference/input.md) ·
[pointer spaces](docs/reference/pointer-spaces.md).

---

## Drawing

Publish intent; a **skin** (holding role `zengine.skin`) paints it. Your code names no colour
and touches no terminal.

```cpp
#include "surface/vocabulary.hpp"
namespace surface = zengine::surface;

surface::SurfaceCanvas canvas;
canvas.width = 78;
canvas.height = 22;
surface::SurfaceLayer layer;
layer.rects.push_back(surface::SurfaceRect{0, 0, 20, 3, surface::role::kAccent});
layer.labels.push_back(surface::SurfaceLabel{1, 1, "hello"});
canvas.layers.push_back(std::move(layer));
mail.publish(canvas);
```

| shape | for |
|---|---|
| `SurfaceText{slot, text}` | one plain line for a named slot ("status", "score") |
| `SurfaceCanvas{width, height, layers}` | the general drawing: cells, ordered planes |
| `SurfaceLayer{rects, labels, texts}` | one complete plane, painter's order within it |
| `SurfaceRect{x, y, w, h, role}` | a filled rectangle |
| `SurfaceLabel{x, y, text}` | one cell per byte, in every medium |
| `SurfaceTextRegion{x, y, w, h, rows, caret_row, caret_col, ground}` | bounded prose; a graphical medium sets it in real type |
| `SurfaceReady{}` | ← the active skin's hello; re-publish your current lines on it |
| `SurfaceExtent{width, height, text_advance_px, text_line_px}` | ← how much room the medium has |

**Roles, never colours:** `kFill`, `kAccent`, `kMuted`, `kAlert`. An unknown role paints as
`kFill`. Depth is two levels and that is all of it: within a plane, rects → labels → regions;
between planes, `layers[0]` is back-most.

**Choosing a text shape** — the question is *is the rectangle mine?*

| the rectangle is… | use |
|---|---|
| mine | `SurfaceTextRegion` with `ground = kGroundOwn` (default) |
| somebody else's, and I write **on** it | `SurfaceTextRegion` with `ground = kGroundBeneath` |
| not a rectangle — this **cell** is the meaning | `SurfaceLabel` |

**Asking how much fits:** call `surface::fit_region(region, extent)` (from
`surface/region.hpp`) with the last `SurfaceExtent` the skin published — the same function the
medium calls, so publisher and medium cannot disagree. It answers a `RegionFit` carrying
`rows`, `columns` and `graphical()`. A metric of `text_advance_px == 0 && text_line_px == 0`
means "text is a cell" — the honest answer for every terminal, and for a window before (or
without) a font.

Full reference: [docs/reference/surface.md](docs/reference/surface.md).

---

## Layout

```cpp
#include "ui/vocabulary.hpp"
#include "ui/layout.hpp"
namespace ui = zengine::ui;

ui::Element e;
e.id = 7; e.label = "panel";
e.x = 2; e.y = 1;
e.width  = ui::Extent{ui::kExtentCells,   12};   // 12 cells
e.height = ui::Extent{ui::kExtentPercent, 70};   // 70% of its frame
e.context = ui::kRootContext;                    // whose frame x/y/w/h are read in

const ui::Scene scene = ui::resolve({e}, ui::Viewport{78, 22});
const ui::Placed* under_cursor = ui::hit(scene, cx, cy);          // -> authored id
const ui::Placed* mine        = ui::placed_for(scene, e.id);
```

| call | answers |
|---|---|
| `resolve(elements, viewport)` | a `Scene`: the viewport plus one `Placed{id, rect}` per element |
| `resolve_extent(extent, span)` | one authored extent against one span — total for every value the type can hold |
| `root_frame(viewport)` / `resolve_in(element, frame)` | the two halves `resolve` composes |
| `hit(scene, cx, cy)` | what is under this cell, as the **authored** id |
| `placed_for(scene, id)` | the resolved rectangle for one authored id |

- Resolution **needs a context**: "how big is this?" is not answerable about an element alone.
- The result is a **separate value**, cached nowhere — the authored type has no field able to
  hold a rectangle, and adding one is a compile error.
- `resolve()` emits items in **document** order (= paint/hit/list order), not dependency order.
- A chain that cannot reach the root (a cycle, a missing source) is **not placed at all**.

No parent/child, no widget kinds, no colour, no z. Full reference:
[docs/reference/ui.md](docs/reference/ui.md).

---

## Workshop

```sh
build/workshop/zengine-workshop                              # terminal, needs >= 78x22
build/workshop/zengine-workshop --load-plan workshop/graphical-load-plan.json   # SDL window
```

| argument | default | is |
|---|---|---|
| `--document <path>` | `workshop.json` beside the binary | the authored objects |
| `--setup <path>` | `workshop-setup.json` | the pane arrangement |
| `--load-plan <path>` | `default-load-plan.json` beside the binary | which artifacts run |
| `--log <path>` | none | durable journal, appended as things happen |
| `--dump <path>` | none | what the volatile recorder still held at exit |

### Keys

**Command mode** (the default)

| key | does |
|---|---|
| `n` / `d` | create / delete an object |
| `Tab` | select the next object |
| `↑` `↓` | move the inspector cursor |
| `Enter` | edit the selected property |
| `h` `j` `k` `l` | move the selected object by one cell |
| `Shift`+`h` `j` `k` `l` | resize the selected object |
| `[` `]` | narrow / widen the workspace by 4 cells |
| `p` | open the pane picker |
| `w` | open pane management |
| `s` | name and save the current setup |
| `r` | restore the setup from its file |
| `b` | build the Builder's target |
| `Ctrl`+`s` / `Ctrl`+`o` | save / open the document |
| `Shift`+`Space` | open the terminal overlay |
| `Ctrl`+`c`, `q` | quit |

**Pane picker** (`p`) — `↑` `↓` choose, `Enter` opens or removes, `Esc` or `p` cancels.

**Pane management** (`w`)

| key | does |
|---|---|
| `Tab` `↑` `↓` | choose a pane |
| `m` | move mode — arrows nudge one cell, `Esc` back |
| `s` | size mode — `Tab` picks the edge, arrows grow one cell, `Esc` back |
| `f` `b` | send to front / back |
| `r` `l` | raise / lower one step |
| `0` | reset — then `p` place, `w` width, `h` height, `o` order |
| `Esc` | close management |

### Panes

Built-in: **Builder**, **Info**. Loaded through the default plan: **Loaded**, **Project**,
**Powers** (from `zengine-introspection`), and the **Composer**.

| pane | shows |
|---|---|
| `Loaded` | what the **Kernel** has: artifacts and incarnations actually resident |
| `Project` | what the **load plan** asked for, paired with what resolved |
| `Powers` | which artifact supplies which operator in the host's catalog |

`Loaded` and `Project` deliberately disagree when the plan asked for something that did not
resolve — that difference is the information. Reference:
[docs/reference/introspection.md](docs/reference/introspection.md).

⚠ **friction — panes are 9 rows tall by default** and a bigger terminal does not grant more.
A larger pane is authored in management mode (`w` → `s` → arrows), one cell per keypress, and
persists in the setup file. See [pane geometry](docs/workshop/panes.md#pane-geometry).

⚠ **friction — nothing is restored at launch.** The document and the setup both persist, and
both need a keypress (`Ctrl`+`o`, `r`) to come back. Both keys are advertised on screen, so this
is a missing default rather than a missing mechanism — but a fresh Workshop *seeds two example
objects*, so forgetting `Ctrl`+`o` looks like a state rather than an omission. See
[workspace continuity](docs/workshop/setups.md#workspace-continuity).

**On-screen hints** (so you need this page less): the title row carries `[+ panel]  p` and
`[window]  w` plus `shift+space terminal`; the setup line carries `s name/save  r restore`; the
bottom line carries the object gestures. Management mode's own sub-keys are the ones not
announced.

---

## Load plans

One durable JSON file naming which artifacts participate and how, executed in **authored
order** (there is no dependency solver). One record per artifact, with two optional surfaces.

```json
{ "artifact": "zengine-timer",
  "provider": [ { "mode": "normal" } ],
  "weave":    [ { "role": "zengine.timer" } ] }
```

| record shape | means |
|---|---|
| `provider` set, `weave` empty | mount this artifact's operator contributions only |
| `provider` empty, `weave` set | load it as a weave into that role only |
| both set | mount the contribution, then load the weave (that order is law, not policy) |
| `"mode": "overlay"` | contribute over powers already in the catalog, reversibly |

A record that mounts a provider and then fails to load its weave **rolls back its own mount**
and stops the plan. Earlier artifacts are not rolled back; the host is told which artifact
stopped it and what still stands.

Shipped plans: [`workshop/default-load-plan.json`](workshop/default-load-plan.json) (terminal)
and [`workshop/graphical-load-plan.json`](workshop/graphical-load-plan.json) (SDL window).
Reference: [docs/reference/load-plan.md](docs/reference/load-plan.md).

---

## Building things (the Builder package)

```cpp
#include "builder/vocabulary.hpp"
namespace builder = zengine::builder;
mail.send_to_role(builder::kBuildRunnerRole, builder::RunBuild{"my-target"});
```

| shape | direction | means |
|---|---|---|
| `RunBuild{target}` | → runner | build this catalog entry (anything else is refused by name) |
| `BuildStarted` | ← runner | a child process exists |
| `BuildOutput` | ← runner | newly drained output |
| `BuildFinished` | ← runner | it ended, with an exit status |
| `BuildNotStarted` | ← runner | there is no compiler / it could not start |
| `BuildStatus` | ← tool | what the Builder currently knows, for any presentation |

The wire **cannot spell a command**: no shape here has a field that is a program, an argument
list or a directory. The runner holds the catalog; the host writes it.

⚠ **friction — Workshop's catalog holds exactly one recipe**, fixed at configure time
(`zengine-snake`, built in Zengine's own build tree). A maker cannot choose a target or build
their own project. See [Workshop's Builder](docs/workshop/builder.md).

---

## Lifecycle

| operation | available today |
|---|---|
| load a weave into a role | **yes** — `loom::LoadWeave{stem, path, role}` to the Weave Manager |
| mount a provider's operator contributions | **yes** — at plan execution |
| overlay a contribution over an existing one, reversibly | **yes** — `"mode": "overlay"` |
| unmount a provider | **only** as a failed record's own rollback, and at teardown |
| unload a weave at run time | **no** from Workshop; `loom::UnloadWeave` exists at the Loom |
| reload / replace in place | **no** from Workshop; `Kernel::reload_from` has provider-custody caveats |
| roll back a whole plan | **no** — one artifact is the atomic unit |

Loading is **initial and restart intent**. Do not read reversible provider overlay as
provider-aware artifact hot reload; they are different things.

---

## Debugging

### Categories of failure, and where each appears

| symptom | first thing to check |
|---|---|
| `library create() returned null` on load | your weave's constructor threw — a duplicate binding id, an empty id, an empty role on a `*_to_role` binding |
| `open failed: … cannot open shared object file` | the artifact is not at the path the plan/host spelled |
| the weave loads and **nothing happens** | no holder for the role you send to; or the shape is unknown to the host process |
| `RefusalReason::SeamUnresolved` | a loaded weave emitted a shape **nothing in this process ever declared** — usually the service you are talking to is not loaded |
| `RefusalReason::CapabilityDenied` | the shape is not in your `Emit<>`, or not permitted to that target |
| `RefusalReason::NotAccepted` | it is not in the target's `Accept<>` |
| `RefusalReason::NoSuchTarget` | that `WeaveId` is not registered |
| a weave's timers never fire | you declared your own `on(const loom::Activated&, ...)`, or forgot `using TimedWeave::on;` — both are now compile errors |
| the program hangs at startup | `bus.pump()` drains to **quiescence**, and a live Timer beat chain never quiesces. Use `bus.pump_pending()` for a bounded turn |

### Seeing refusals

A sender cannot observe its own send's fate. A host can, with an observer:

```cpp
bus.add_observer([](const loom::BusEvent& e) {
    if (e.kind == loom::EventKind::Refused) {
        std::fprintf(stderr, "refused %s -> %s : %s\n", e.schema_name.c_str(),
                     e.addressed_role.c_str(), e.refusal.message().c_str());
    }
});
```

`loom::name_of(RefusalReason)` gives the reason's name; `Refusal::message()` gives text. In
Workshop, `--log <path>` writes a durable journal and `--dump <path>` writes what the volatile
recorder still held at exit.

### What a host says about itself

`Kernel::containment_note()` returns, verbatim, what this host actually isolates. An
in-process Zengine host prints *"in-process; trusted; no OS sandbox"* — read it as literal.

---

## Vocabulary

| term | means |
|---|---|
| **Loom** | the substrate: values, schemas, the gate, the switchboard, the Kernel |
| **Zengine** | this repository — the default set of weaves built on the Loom |
| **Workshop** | the interactive maker environment built with Zengine |
| **weave** | one participant: a state struct, an accept list, an emit list, handlers |
| **shape** | a `ZEN_SHAPE` type — a schema-carrying message or state, identified by (name, version) |
| **message** | one delivery of a shape from a sender to a target |
| **role** | a named slot one weave holds at a time; addressing it survives replacement |
| **office** | a role a weave may deliberately *speak as*, stamped on the envelope |
| **grant** | what a weave may say, and to whom. Never what it may touch |
| **artifact** | one loadable image (`.so`/`.dll`) that may export a weave, a provider, or both |
| **provider** | an artifact that supplies operator definitions to a host's catalog |
| **operator** | a typed, reusable rule a host can evaluate |
| **power** | one operator in a host's catalog, as introspection names it |
| **load plan** | the authored file naming which artifacts participate and how |
| **skin** | the replaceable weave that claims a medium and paints published intent |
| **pane** | one region of Workshop's screen, built-in or offered by a weave |
| **setup** | the persisted pane arrangement, with a name |
| **document** | the persisted authored objects a maker is working on |

**`Sense`** is the Loom's read-side surface — a deliberate immutable claim of the latest
observation a participant published, gated by the reader's `Claims<...>`. It is a Loom concept;
no Zengine package defines or extends it.

Terms that are **not** synonyms, and must not be flattened:

| these differ | because |
|---|---|
| **role** vs **office** | a role is a slot you *hold*; an office is a role you deliberately *speak as*, stamped on the envelope. Holding a role attaches nothing on its own |
| **artifact** vs **weave** | an artifact is a loadable image; a weave is a participant. One artifact may export a weave surface, a provider surface, both, or be asked for one and get one |
| **operator** vs **power** | the same thing from two sides: an operator is the definition, a power is one entry in a host's catalog as introspection names it |
| **setup** vs **document** | the room you work in, versus the thing you are making |
| **pane** vs **panel** | a pane is any region; *panel* is the compiled-in kind. A maker needs neither word |
| **authored** vs **resolved** | what a person wrote, versus what a viewport made of it. Held apart by a compile-time fence |
| **Zen** | the umbrella name for Loom + Zengine + Workshop together. It is not a component, a namespace, or a directory you need |
