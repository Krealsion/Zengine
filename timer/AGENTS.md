# `timer/` — The Timer: working on it

The router for `timer/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `vocabulary.hpp` | the messages, and the role `kTimerRole` (`zengine.timer`); installed |
| `binding.hpp` | `TimedWeave`, `TimerBindings`, `TimerHandle`: a declared rhythm; installed |
| `normalize.hpp` | `timer.normalize_delay`, the delay rule as an operator; `DelayAuthority` |
| `timer_weave.hpp` | `TimerWeave`, the service: it claims its soonest time on the host's one wait and fires on the time it is handed |
| `source_wait.hpp` | `SourceWait`: what a source claims of the host's one wait, with the time it must look by, and the time it was handed; installed |
| `timer.cpp` | exports `TimerWeave` and the `zengine.timer` provider |
| `CMakeLists.txt` | `zengine-timer-vocabulary` and the `zengine-timer` weave |
| `docs/` | the pages below |

`zengine-timer-vocabulary`: INTERFACE, alias `zengine::timer`.
`zengine-timer`: the loadable weave, built only where the Loom exports `loom::kernel`; it links
activation and operator.

## Law

No register of its own under `agents/`. Its law is [Timer laws (TIMER)](docs/timer-laws.md),
TIMER-01..05. The delay rule's owner is in `agents/operators.md`, which `operator/AGENTS.md`
routes; its compile walls are in `agents/verification/walls.md`, which the root routes.

## Suites

- `audit_probes` — what the substrate does to a live Timer's time when the Timer is swapped,
  reloaded, stimulated twice or joined late (probes A-D); `tests/test_audit_probes.cpp`
- `timer` — the contract by content-id, the role's manual, every schedule under the host's one
  wait on a clock that moves only when told, the activation law, the claim and the wake, a gone
  weave's timers, the host resting only with its bus empty, the real library through the real
  kernel, load order, the window and input as sources of the wait, continuity;
  `tests/test_timer.cpp`, `tests/weavelib/timer_virtual.cpp`, `tests/weavelib/oneshot_probe.cpp`
- `timer_activation_hook_compiles` — the control: a `TimedWeave` using `on_timed_activation`
  builds; `tests/compile_negative/activation_collision.cpp`, case 2
- `timer_cursor_bypass_refused` — accepting the activation again inside the hook does not
  compile; same source, case 4
- `timer_missing_using_no_binding_refused` — case 3's defect in a weave declaring no binding;
  same source, case 5
- `timer_missing_using_refused` — own `on` handlers without `using TimedWeave::on;` do not
  compile; same source, case 3
- `timer_raw_activation_refused` — a raw `on(const loom::Activated&, loom::Mail&)` does not
  compile; same source, case 1

The compile cases' targets: `tests/compile_negative/CMakeLists.txt`. Its cases in shared suites:
`operator` (`tests/test_operator_canonical.cpp`: the Timer and a loaded stranger spend one
catalog) and `workshop_load` (`tests/test_workshop_load.cpp`: the plan's `zengine-timer`).
Outside CTest, the package witness builds `tests/package/oven.cpp` against `zengine::timer`.

## Host side

- Staging: `workshop/CMakeLists.txt` stages `zengine-timer` beside the host.
- Waiting: `workshop/workshop.cpp`, `app/main.cpp` and `examples/snake/play.cpp` turn the bus and
  rest in Loom's one wait, trusting the Timer's role; so does the package's
  `tests/package/kitchen_host.cpp`.
- Loading: `workshop/default-load-plan.json` loads `zengine-timer` as a provider and as the `zengine.timer` weave (`workshop/docs/load-plan.md`).
- Grants: `workshop/workshop.cpp` grants the build runner `EnsureTimer` and `CancelTimer` to
  `kTimerRole`; `workshop/editor_switch.hpp` and `workshop/guest_door.hpp` are `TimedWeave`s.
- No seam vocabulary. `cmake/ZengineInstall.cmake` installs the headers and the weave;
  `input/`, `builder/`, `flow/flow-host/`, `graphics/window/` and `examples/snake/` (the world)
  hold `TimedWeave`s too.

## Pages

- [zengine.timer -- the Timer](docs/zengine.timer.md), the role's manual: carried by
  `zengine-timer` (`MANUAL`) and by every target that builds `TimerServiceT`, answered through
  `ShapeDocumentRequested`; its witness is the `timer` suite's role case
- [TimedWeave — a weave with an authored rhythm](docs/timed-weaves.md)
- [Timer binding (`TimedWeave`) — reference](docs/timer-binding.md)
- [Timer continuity carries remaining duration, not absolute
  time](docs/timer-continuity-carries-remaining-duration.md)
- [Timer laws (TIMER)](docs/timer-laws.md)
- [Using the Timer](docs/timers.md)

## Practices

- [The Timer and
  activation](../docs/contributing/best-practices.md#the-timer-and-activation-timer-activation)
