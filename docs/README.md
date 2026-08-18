# Zengine documentation

Zengine documents only what it owns: the packages. Substrate truth — messaging,
lifecycle, replacement, capabilities — lives in
[Loom's docs](../../Loom/docs/README.md); machine collaborators start at
[Loom's CONTEXT.md](../../Loom/docs/CONTEXT.md).

**Making a Workshop tool:**
[guides/make-a-workshop-tool.md](guides/make-a-workshop-tool.md) — the two authoring paths and
which one you are on. A **compiled-in panel** is source-contributor work: identity, the room
Workshop grants it, publishing rows into it, one operation reached by a pointer and a hotkey,
when a component is worth holding, and where each kind of state belongs. An **office-authored
external pane** is the bounded read-only provider protocol: four shapes, a prose budget, no
input, and no plugin/installation story yet. The exact wire shapes are
[`workshop/pane_vocabulary.hpp`](../workshop/pane_vocabulary.hpp) and the reference account is
the repo README's [A weave may offer a pane](../README.md#a-weave-may-offer-a-pane-wp-0);
[`tests/weavelib/workshop_hello.cpp`](../tests/weavelib/workshop_hello.cpp) is its smallest
complete witness, and a test fixture rather than a product plugin. For built-in examples read
`Info` and `Builder` in [`workshop/screen.hpp`](../workshop/screen.hpp), and for a shipped
external one read [reference/introspection.md](reference/introspection.md) beside
[`introspection/`](../introspection/loaded.hpp).

**Seeing what is running:** [reference/introspection.md](reference/introspection.md) — the
`Loaded` pane. What it shows, where each fact's authority lives, what it deliberately does not
show, why it is a snapshot rather than a feed, what happens when its provider disappears, and the
exact authority it holds. It is also the first tool that reaches Workshop entirely through the
external pane protocol, so it is worth reading beside the guide above.

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
