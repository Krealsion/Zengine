# Timer continuity carries remaining duration, not absolute time

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the
behaviour it decided is [timer continuity](../reference/timer-continuity.md).

**Context.** When the Timer service is replaced, standing schedules must cross
to the successor. What number crosses?

**Decision.** The handoff letter converts every active entry to a **remaining
duration** (`remaining_ms`) at one clock read. `remaining_ms` is never a due
time, and the letter is written by a service that fires, cancels and advances
nothing while describing itself.

**Alternatives considered.**
- *Absolute due timestamps* — rejected: the successor's monotonic clock has no
  shared epoch with the predecessor's, and any replacement downtime would
  silently *pause* deadlines — exactly the behavior a deadline must not have.
- *Re-anchoring on arrival* ("restart every delay") — rejected as the default:
  it converts "two seconds left" into "full delay from now"; it survives only
  as the requester's explicit `restart_delay` preference.
- *Wall-clock times* — rejected: wall clocks jump; alarms that want them are a
  different vocabulary with its own continuity answer, named future work.

**Consequences.** The letter is meaningful on any clock; the moving-clock
proof ("the incumbent advanced one more second mid-preparation, the letter
carries 1000 not 2000") is assertable exactly; requesters choose per timer
(`preserve_remaining` / `restart_delay` / `drop`) and receive a receipt naming
what actually happened.

**Laws supported.** [TIMER-03](../laws/timer-laws.md),
[TIMER-04](../laws/timer-laws.md).

**Evidence / history.** R2B-0 and R2B-3c in
[Loom's history](https://github.com/Krealsion/Loom/blob/main/docs/history/README.md); the keystone and
moving-clock cases in `tests/test_timer.cpp`.
