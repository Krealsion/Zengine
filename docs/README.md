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

**Across packages:**
[reference/pointer-spaces.md](reference/pointer-spaces.md) — where a reported
pointer position lands, and which of Input, Surface and the consuming
application owns each step. It is here rather than in any one package's source
because no single package can state it.

**The other packages** (Input, Surface, UI, Workshop, the snake panel) are
documented in the repo [README](../README.md) beside their sources — they are
smaller truths and their sections are their reference. A source comment may cite
a README section by anchor; `doc_links` checks those the same way it checks
these pages. The pre-consolidation package manuscript is frozen at
[history/pre-r2c/README.md](history/pre-r2c/README.md).
