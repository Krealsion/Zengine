# Using the Timer

**How-to.** Ordering a timer from a weave with the raw protocol. For a rhythm that is part of
what your weave *is*, use [TimedWeave](timed-weaves.md) instead.

Time in Zen is a service: the `zengine-timer` weave holds the `zengine.timer`
role and fires timers for whoever asks, at the times the host's one wait hands it
([the host's wait](#the-hosts-wait)). Your weave never sleeps, never polls, never
reads a clock, and never talks to an id — always to the role.

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
([TIMER-04](timer-laws.md)).

```cpp
void on(const TimerReady&, loom::Mail& mail) { place_my_orders(mail); }
```

If your rhythm is fixed — "this weave ticks every 50 ms" — skip all of this
plumbing and use [`TimedWeave`](timed-weaves.md), which re-places declared
orders for you at exactly the right moments.

## What survives a replacement

Remaining *durations* cross to a verified successor (never wall-clock
deadlines — [TIMER-03](timer-laws.md)); your `preferred/fallback`
choice is honored per timer, and the receipt tells you what actually happened.
Deeper: [the Timer's manual](zengine.timer.md), every message it accepts and what it does when
it is replaced.

## The host's wait

A host turns its bus and then rests in Loom's one wait (`loom::host::Reactor`), on every role it
trusts: the Timer's among them, with every source's -- a window, the Input role, a Neovim pane,
the guest door each claim what they wait on, and a time when they must look
([`timer/source_wait.hpp`](../source_wait.hpp)). The Timer claims the time its soonest timer is
due, and the wait wakes it then, handing it the time; with nothing standing it claims no time, and
an idle host rests until something is ready. Workshop and the app trust the Timer's role; a host
that does not wait on it -- one that runs no wait, or `loom-host`, which trusts no role -- never
wakes a Timer, so its timers never fire there.

A suite runs its host's loop on a clock that moves only when told (`loom::host::ManualClock`): a
wait that finds nothing ready moves it to the soonest time due, so a case says "until this time"
and every duration is exact.
