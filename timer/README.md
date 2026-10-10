# The Timer

**The Timer gives a weave time as messages. Instead of sleeping or polling to wait, a weave asks
the Timer for a beat, once or on repeat, and a `TimerFired` message arrives when the beat is due.**

The Timer is one loadable weave, `zengine-timer`, holding the `zengine.timer` role; it owns the
beats a weave waits on. A weave whose rhythm is part of what it is declares
that rhythm once with `TimedWeave`, and it holds even when the Timer itself is replaced. A program
writes `#include "timer/vocabulary.hpp"` for the messages and `#include "timer/binding.hpp"` for
`TimedWeave`, and links `zengine::timer`.

## Pages

- [Timer protocol](docs/timer-protocol.md): the reference, every message and field and what the
  Timer does with each
- [Using the Timer](docs/timers.md): asking for a beat with the plain messages
- [TimedWeave](docs/timed-weaves.md): declaring a weave's rhythm once
- [Timer binding](docs/timer-binding.md): what `TimedWeave` adds to a weave, and where it stops
- [Timer continuity](docs/timer-continuity.md): what a waiting beat does when the Timer is replaced
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
| `timer.cpp` | the real clock, built as the `zengine-timer` weave |
| `CMakeLists.txt` | the build |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
