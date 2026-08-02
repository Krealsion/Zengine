# Zengine documentation

Zengine documents only what it owns: the packages. Substrate truth — messaging,
lifecycle, replacement, capabilities — lives in
[Loom's docs](../../Loom/docs/README.md); machine collaborators start at
[Loom's CONTEXT.md](../../Loom/docs/CONTEXT.md).

**Using time:** [guides/timers.md](guides/timers.md) (order a timer, receipts,
the `TimerReady` rule) · [guides/timed-weaves.md](guides/timed-weaves.md)
(a weave with an authored rhythm).

**Exact Timer semantics:**
[reference/timer-protocol.md](reference/timer-protocol.md) ·
[reference/timer-continuity.md](reference/timer-continuity.md) ·
[reference/timer-binding.md](reference/timer-binding.md).

**Invariants:** [laws/timer-laws.md](laws/timer-laws.md) (TIMER-01..05).

**Why:** [decisions/](decisions/timer-continuity-carries-remaining-duration.md) ·
the phase story is in [Loom's history](../../Loom/docs/history/README.md).

**The other packages** (Input, Surface, the snake panel) are documented in the
repo [README](../README.md) beside their sources — they are smaller truths and
their sections are their reference. The pre-consolidation package manuscript is
frozen at [history/pre-r2c/README.md](history/pre-r2c/README.md).
