# Timer continuity carries remaining duration, not absolute time

**Decision record.** One decision, the other ways it could have gone, and why this one. What the
Timer does today is in [the Timer's manual](zengine.timer.md#about).

**The question.** When the Timer is replaced, its standing timers must cross to its successor.
What number crosses for each?

**The decision.** The letter the old Timer writes turns every standing timer into the time it has
left, `remaining_ms`, measured at one reading of the clock. `remaining_ms` is never a deadline, and
the Timer writing the letter fires, cancels and moves nothing while it describes itself.

**The other ways.**

- *Deadlines as timestamps.* Rejected: the successor's clock does not count from the same moment
  as its predecessor's, and any time the replacement took would silently pause every deadline,
  which is the one thing a deadline must not do.
- *Starting every delay again when it lands.* Rejected as the default: it turns "two seconds left"
  into "the full delay from now". It survives as a choice a weave makes for itself, `restart_delay`.
- *Times of day.* Rejected: a wall clock jumps. An alarm that wants a time of day is a different
  kind of message, with its own answer to replacement, and is not built yet.

**What follows.** The letter means the same on any clock. A clock that moves while the letter is
written can be tested exactly: when the old Timer's clock moves one more second while it prepares,
the letter carries 1000 and not 2000. A weave chooses timer by timer (`preserve_remaining`,
`restart_delay` or `drop`) and receives a receipt that says what actually happened.

**The laws it supports.** [TIMER-03](timer-laws.md) and [TIMER-04](timer-laws.md).

**Evidence.** The work on continuity and handover is in [Loom's
history](https://github.com/Krealsion/Loom/blob/main/docs/history/README.md), and the cases are in
`tests/test_timer.cpp`.
