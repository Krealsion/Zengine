# The Timer

**The Timer gives a weave time as messages. A weave never sleeps, polls or reads a clock to wait:
it asks the Timer for a timer, once or on repeat, and a `TimerFired` message arrives when the
timer comes due.**

The Timer is one loadable weave, `zengine-timer`, which answers to the role `zengine.timer`. It
keeps every timer a weave waits on, and it keeps them across its own replacement: a timer with
two seconds left when one Timer gives way to another still has two seconds left after. A weave
whose rhythm is part of what it is, such as one that moves every 50 ms, declares that rhythm once
with `TimedWeave`, and the rhythm holds even while the Timer is replaced. A program includes
`timer/vocabulary.hpp` for the messages and `timer/binding.hpp` for `TimedWeave`, and links
`zengine::timer`. Words such as weave, role and the host's wait are in [the
glossary](../docs/glossary.md).

The Timer reads no clock either. The program that hosts it turns its message bus and then rests
in Loom's one wait, and that wait wakes the Timer when its soonest timer is due and tells it the
time. A program that runs no such wait never wakes the Timer, so its timers never fire there.

## Pages

- [Using the Timer](docs/timers.md): asking for a timer with the plain messages, and the one rule
  that keeps a timer's time across a replacement. Start here.
- [TimedWeave](docs/timed-weaves.md): declaring a weave's rhythm once, what the binding does for
  you, and where it stops.
- [The Timer's manual](docs/zengine.timer.md): every message the Timer accepts, what it does with
  each, and what it sends back.
- [The Timer's laws](docs/timer-laws.md): the numbered rules the Timer keeps across files.
- [Why a replacement carries remaining durations](docs/timer-continuity-carries-remaining-duration.md):
  the decision, and the ways it was not made.

## Parts

| file | what it holds |
|---|---|
| `vocabulary.hpp` | the messages a weave sends the Timer and receives from it, and the role `kTimerRole`; installed |
| `binding.hpp` | `TimedWeave`, `TimerBindings` and `TimerHandle`, for a weave with a declared rhythm; installed |
| `normalize.hpp` | `timer.normalize_delay`, the rule for the delay a Timer schedules, and `DelayAuthority`, which decides whose copy of the rule a Timer uses |
| `timer_weave.hpp` | `TimerWeave`, the Timer itself |
| `source_wait.hpp` | `SourceWait`, for any weave that waits on the outside world through the host's one wait; installed |
| `timer.cpp` | the loadable weave `zengine-timer`, which also supplies `timer.normalize_delay` to a host |
| `CMakeLists.txt` | `zengine-timer-vocabulary`, a header-only target aliased `zengine::timer`, and the weave `zengine-timer`, built only where the installed Loom exports `loom::kernel` |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`timer/` adds to it.

**Law.** The rules that span files are [the Timer's laws](docs/timer-laws.md), TIMER-01 to
TIMER-05, and a comment that relies on one names it in a sentence. A rule about one declaration
is in that declaration's comment. The delay rule is an operator, and the operator package's
rules over it are in [`agents/operators.md`](../agents/operators.md). The walls that refuse a
wrong `TimedWeave` at compile time are in
[`agents/verification/walls.md`](../agents/verification/walls.md).

**The manual is compiled in.** `zengine-timer`, and every target that builds `TimerWeave`, carries
`docs/zengine.timer.md` and answers a shape's section of it to anyone who asks
(`ShapeDocumentRequested`). An edit to the manual is therefore a build: the `timer` suite holds
its sections to the messages the Timer accepts, and `manual` holds its form.

**Suites.** Each runs through the official lane, `tests/verify.cmake`.

| suite | what it proves | where |
|---|---|---|
| `timer` | the manual against the accepted messages; every schedule, on a clock that moves only when a case moves it; the activation law; the claim on the host's wait and the wake; the timers of a weave that is gone; the host resting only with its bus empty; the real library through the real kernel; load order; a window and Input as sources of the wait; continuity across reload and replacement | `tests/test_timer.cpp`, `tests/weavelib/timer_virtual.cpp`, `tests/weavelib/oneshot_probe.cpp` |
| `audit_probes` | what Loom does to a running Timer's time when the Timer is swapped, reloaded, woken twice or joined late | `tests/test_audit_probes.cpp` |
| `timer_raw_activation_refused` | a `TimedWeave` that declares its own `on(const loom::Activated&, loom::Mail&)` does not compile | `tests/compile_negative/activation_collision.cpp`, case 1 |
| `timer_missing_using_refused` | a `TimedWeave` with its own `on` handlers and no `using TimedWeave::on;` does not compile | the same file, case 3 |
| `timer_activation_hook_compiles` | the control: a `TimedWeave` with `on_timed_activation` compiles | the same file, case 2 |
| `timer_cursor_bypass_refused` | accepting the activation again inside the hook does not compile | the same file, case 4 |
| `timer_missing_using_no_binding_refused` | case 3's mistake in a weave that declares no binding | the same file, case 5 |

The compile cases' targets are in `tests/compile_negative/CMakeLists.txt`. Two shared suites hold
Timer cases too: `operator` (`tests/test_operator_canonical.cpp`: the Timer and a loaded stranger
use one catalog) and `workshop_load` (`tests/test_workshop_load.cpp`: the load plan's
`zengine-timer`). Outside CTest, the package witness builds `tests/package/oven.cpp` against
`zengine::timer`.

**The host's side.** These programs turn the bus and rest in Loom's one wait with the Timer's
role among the roles they trust, which is what makes timers fire: Workshop
(`workshop/workshop.cpp`), the app (`app/main.cpp`), the snake example
(`examples/snake/play.cpp`) and the package's `tests/package/kitchen_host.cpp`. Elsewhere:

- `workshop/CMakeLists.txt` stages `zengine-timer` beside Workshop, and
  `workshop/default-load-plan.json` loads it twice, as an operator provider and as the
  `zengine.timer` weave ([the load plan](../workshop/docs/load-plan.md)).
- `workshop/workshop.cpp` grants the build runner `EnsureTimer` and `CancelTimer` to the Timer's
  role.
- `TimedWeave`s outside this folder: `workshop/editor_switch.hpp`, `workshop/guest_door.hpp`, and
  weaves in `input/`, `builder/`, `flow/flow-host/`, `graphics/window/` and `examples/snake/`.
- `cmake/ZengineInstall.cmake` installs the headers and the weave.

**Practice.** [The Timer and
activation](../docs/contributing/best-practices.md#the-timer-and-activation-timer-activation).
