# The activation cursor

**A small C++ helper a weave keeps to decide whether an arriving `zen.Activated` message is its own
to act on. The cursor accepts an activation only when Loom vouched for it and, from the same
sender, it is newer than the last one accepted (another sender Loom vouched for begins a new
lineage), so a weave does its once-per-activation work once, and a copy or a replay changes
nothing.**

It is header-only and needs no kernel. A program links `zengine::activation` and writes
`#include "activation/activation.hpp"`. A weave that uses the Timer binding has a cursor kept for
it ([the Timer binding](../timer/docs/timed-weaves.md)). The same header holds two smaller helpers
a weave is built with: `zengine::With`, which composes facets onto a weave, and the value door,
which lets a schema built by hand stand in a weave's accept list. Words such as weave and incarnation
are in [the glossary](../docs/glossary.md).

## Pages

- [The activation cursor — reference](docs/activation.md): what `accept` checks, in what order and
  why, how to carry an activation on a wire, and how a weave accepts a schema built by hand.

## Parts

| file | what it holds |
|---|---|
| `activation.hpp` | the whole library: the cursor, `zengine::ActivationCursor`; `zengine::With`, which composes facets onto a weave; the value door (`zengine::ValueDoor`, `zengine::RawValueDoor`, `ZENGINE_VALUE_DOOR`); installed |
| `CMakeLists.txt` | `zengine-activation`, a header-only target aliased `zengine::activation`. It links `loom::core` and `loom::switchboard` only, so it builds with or without a kernel, and the top-level `CMakeLists.txt` adds it first |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`activation/` adds to it.

**Law.** The folder has no law page of its own. What an activation means, and who may attest one,
are Loom's [lifecycle laws](https://github.com/Krealsion/Loom/blob/main/docs/laws/lifecycle-laws.md)
(LIFE-01, LIFE-03, LIFE-04), and the cursor's own rules are in its comments and in
[the reference](docs/activation.md). The Timer's activation law, TIMER-01, is on
[the Timer's law page](../timer/docs/timer-laws.md).

**Suites.** The folder has no CTest entry of its own. The cursor is proven through the Timer, whose
front page claims those entries ([the Timer's suites](../timer/README.md#working-on-it)): the
`timer` suite (`tests/test_timer.cpp`) drives forged, unactivated, duplicate, replayed, newer and
second-initiator activations through the Timer and its binding, and the compile case
`timer_cursor_bypass_refused` (`tests/compile_negative/activation_collision.cpp`, case 4) refuses a
binding that accepts the activation a second time. The suites activate a weave as Loom's control
door does, through `tests/lifecycle_door.hpp`. A value door is proven where it is accepted:
Compose's discovery in `workshop_panes_hand` (`tests/test_workshop_panes_input.cpp`), and the
discovery door in `workshop_panes_terminal` (`tests/test_workshop_panes_powers.cpp`) and
`workshop_load`. Outside CTest, the installed-package witness (`tests/package/run.cmake`) compiles
the header from `tests/package/public_surface.cpp`. Each runs through the official lane,
`tests/verify.cmake`.

**The host's side.** No host holds the cursor: it is a library, and the activations it reads are
attested by Loom's control door. Its users are the Timer (`timer/binding.hpp`,
`timer/timer_weave.hpp`) and nearly every weave and pane, Workshop's own included
(`workshop/desktop-pane/pane.cpp`, `workshop/menu-presenter/presenter.cpp`). The shipped value doors
are the Compose pane's (`composer/composer.cpp`) and the host's discovery door
(`workshop/powers_door.hpp`). `cmake/ZengineInstall.cmake` installs the header.

**Practice.** [The Timer and
activation](../docs/contributing/best-practices.md#the-timer-and-activation-timer-activation).
