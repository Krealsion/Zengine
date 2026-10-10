# `activation/` — The activation cursor: working on it

The router for `activation/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `activation.hpp` | the whole library, header-only: `zengine::ActivationCursor` |
| `CMakeLists.txt` | `zengine-activation`, INTERFACE, alias `zengine::activation`; not a weave |
| `docs/activation.md` | the reference |

It links `loom::core` and `loom::switchboard` only, so it builds with or without a kernel. The
top-level `CMakeLists.txt` adds it first.

## Law

No register of its own under `agents/`. Its law is its reference,
[the activation cursor](docs/activation.md), which rests on the Loom's lifecycle laws (LIFE-01,
LIFE-03, LIFE-04). The Timer's activation law, TIMER-01, is in
[timer laws](../timer/docs/timer-laws.md).

## Suites

No CTest entry of its own; the entries that exercise it are the Timer's (`timer/AGENTS.md`).
Its cases in shared suites: `timer` (`tests/test_timer.cpp`: forged, unactivated, duplicate,
replayed, newer and second-operator activations, through the Timer and its binding) and
`timer_cursor_bypass_refused` (`tests/compile_negative/activation_collision.cpp`, case 4). The
suites activate a weave as the Loom's control door does, through `tests/lifecycle_door.hpp`.
Outside CTest, the installed-package witness (`tests/package/run.cmake`) compiles it from
`tests/package/public_surface.cpp`.

## Host side

None: it is a library, and no Workshop file hosts it. The activations it reads are attested by
the Loom's control door. Its consumers: the Timer (`timer/binding.hpp`, `timer/timer_weave.hpp`)
and nearly every weave and pane, Workshop's own included (`workshop/desktop-pane/pane.cpp`,
`workshop/menu-presenter/presenter.cpp`). `cmake/ZengineInstall.cmake` installs its header.

## Pages

- [The activation cursor — reference](docs/activation.md)

## Practices

- [The Timer and
  activation](../docs/contributing/best-practices.md#the-timer-and-activation-timer-activation)
