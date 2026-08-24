# Timer continuity — reference

**Reference.** What a standing schedule does when the Timer service itself is replaced.

How a schedule survives the service being replaced. Laws:
[TIMER-01..05](../laws/timer-laws.md). Substrate ceremonies:
[Loom lifecycle](https://github.com/Krealsion/Loom/blob/main/docs/reference/lifecycle.md) and
[Loom prepared-replacement](https://github.com/Krealsion/Loom/blob/main/docs/reference/prepared-replacement.md).

## The invariant everything serves

> The incumbent owns time until the replacement boundary. The successor crosses
> that boundary with the incumbent's **final** schedule progress — not an
> earlier approximation. A failed attempt does not reset, duplicate or orphan
> the incumbent's clock.

Progress crosses as **remaining durations** ([TIMER-03](../laws/timer-laws.md))
in a `TimerHandoff` letter (≤ 32 entries), written at one clock read by a
service that fires/cancels/advances nothing while describing itself.

## Startup modes — declared, never inferred

A fresh incarnation is told which world it enters
(`Startup{GracefulClaim | PreparedRestoration | Fresh}`):

- **GracefulClaim** (default) — the heir claims the letter **by role**
  (`zen.ClaimBequest` to the steward) because it cannot know its predecessor's
  id. The legacy ceremony's window applies.
- **PreparedRestoration** — a prepared candidate claims **by id** from the
  preparer it read off its ask's stamped sender, waiting at most
  `kPreparedClaimBeats = 8` beats before honestly starting fresh (a bounded
  promise; the consumer is refused rather than held forever).
- **Fresh** — asks nobody, inherits nothing, says so.

In every mode: `TimerReady` only **after** the continuity decision
([TIMER-04](../laws/timer-laws.md)); operations arriving mid-decision are held
and replayed in order; restoration reserves bounded capacity so it does not
grow mid-restore (copying entry strings may still allocate).

## The prepared path: the boundary IS the admission

The moving-state problem (any snapshot at time T is stale by commit at T+k)
dissolves because two facts are the substrate's:

1. the beat chain rides the **role** — the instant admission moves the role,
   the incumbent's parked beat resolves to the successor, which refuses a
   different activation's key: the incumbent's clock stops advancing, and
   nothing parked it;
2. admission **seals** the incumbent in the same dispatch — its table is
   frozen, reachable only by its coordinator.

So the letter is written **after** admission, by a service already incapable
of changing, through the unchanged `zen.PrepareShutdown → TimerHandoff`
exchange — one interpretation of progress, not two. The incumbent is **never
told** anything beforehand; abort is correct by construction because there is
nothing to release. The candidate's own activation necessarily precedes its
prepared claim ([Loom PR-08](https://github.com/Krealsion/Loom/blob/main/docs/laws/replacement-laws.md)).

Failure direction: every pre-admission failure leaves the incumbent's chain
and schedule untouched — no reset, no fork, no orphan; a promised letter that
never arrives degrades at the published bound with the consumer honestly
refused.

## The graceful path

The Weave Manager's swap ceremony: `PrepareShutdown` asks the outgoing holder
to describe itself; the letter travels as a `zen.Bequest`; the heir claims by
role. Preserves authored work; verifies nothing about the successor; has an
observable window. Both paths interpret the same letter shapes — one
vocabulary, two ceremonies.

## Tests

Zengine suite `timer`: the two-seconds keystone, the moving-clock one-second
proof, queue-boundary operations, five failure routes, forged/refused letters,
the real-clock pilot on the shipped artifact.
