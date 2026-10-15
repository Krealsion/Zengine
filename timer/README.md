# The Timer

**The Timer gives a weave time as messages. Instead of sleeping or polling to wait, a weave asks
the Timer for a beat, once or on repeat, and a `TimerFired` message arrives when the beat is due.**

The Timer is one loadable weave, `zengine-timer`, holding the `zengine.timer` role; it owns the
beats a weave waits on. A weave whose rhythm is part of what it is declares
that rhythm once with `TimedWeave`, and it holds even when the Timer itself is replaced. A program
writes `#include "timer/vocabulary.hpp"` for the messages and `#include "timer/binding.hpp"` for
`TimedWeave`, and links `zengine::timer`.

## Pages

- [The Timer](docs/zengine.timer.md): the manual, every message the Timer accepts, what it does
  with each and what it says, and what a waiting beat does when the Timer is replaced
- [Using the Timer](docs/timers.md): asking for a beat with the plain messages
- [TimedWeave](docs/timed-weaves.md): declaring a weave's rhythm once
- [Timer binding](docs/timer-binding.md): what `TimedWeave` adds to a weave, and where it stops
- [Timer laws](docs/timer-laws.md): the numbered promises the Timer keeps
- [Remaining duration, not absolute
  time](docs/timer-continuity-carries-remaining-duration.md): why continuity works as it does

## What is in this folder

| | |
|---|---|
| `vocabulary.hpp` | the messages a weave sends to the Timer and receives from it |
| `binding.hpp` | `TimedWeave`, for a weave with a declared rhythm |
| `normalize.hpp` | the rule for what an authored delay becomes |
| `timer_weave.hpp` | the Timer weave itself, over any clock |
| `os_clock.hpp` | the real clock: monotonic milliseconds and the nap, a beat's own period on Windows too |
| `timer.cpp` | the Timer over the real clock, built as the `zengine-timer` weave |
| `CMakeLists.txt` | the build |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
