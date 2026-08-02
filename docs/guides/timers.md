# Using the Timer

Time in Zen is a service: the `zengine-timer` weave holds the `zengine.timer`
role, owns the monotonic clock and the one nap in the system, and delivers
beats to whoever asks. Your weave never sleeps, never polls, and never talks to
an id — always to the role.

## Order a timer

```cpp
using namespace zengine::timer;

// in a handler, with your Mail:
mail.send_to_role(kTimerRole, EnsureTimer{
    .id = "poll.jobs",                     // your name for it, scoped to you
    .delay_ms = 250,
    .repeat = true,
    .preferred = kPreserveRemaining,       // "preserve_remaining": keep progress
    .fallback  = kRestartDelay,            // "restart_delay" is acceptable too
});
```

`EnsureTimer` is idempotent and *ordered*: you state a preference for what
should happen to an existing schedule and a fallback you can live with; an
unavailable preference with no acceptable fallback **refuses** rather than
guessing. The receipt comes back to you as
`TimerResolution{id, resolved, reason}` — `resolved` is one of
`preserved_remaining / restarted_delay / dropped / refused`.

Beats arrive as `TimerFired{id}` (to you, or to a role if you ordered
`EnsureRoleTimer`). Cancel with `CancelTimer{id}`.

## The one rule that keeps schedules honest

When you hear **`TimerReady`**, re-place your orders. It is the service's
announcement that schedule questions may be asked (first boot, and again after
the service is replaced) — and it deliberately arrives only *after* the new
service has decided what it inherited, so re-asking cannot re-anchor a
schedule the handoff already preserved
([TIMER-04](../laws/timer-laws.md)).

```cpp
void on(const TimerReady&, loom::Mail& mail) { place_my_orders(mail); }
```

If your rhythm is fixed — "this weave beats every 50 ms" — skip all of this
plumbing and use [`TimedWeave`](timed-weaves.md), which re-places declared
orders for you at exactly the right moments.

## What survives a replacement

Remaining *durations* cross to a verified successor (never wall-clock
deadlines — [TIMER-03](../laws/timer-laws.md)); your `preferred/fallback`
choice is honored per timer, and the receipt tells you what actually happened.
Deeper: [timer-protocol](../reference/timer-protocol.md) ·
[timer-continuity](../reference/timer-continuity.md).
