# Using the Timer

**How-to.** Ordering a timer from a weave with the raw protocol. For a rhythm that is part of
what your weave *is*, use [TimedWeave](timed-weaves.md) instead.

Time in Zen is a service: the `zengine-timer` weave holds the `zengine.timer`
role, owns the monotonic clock, and delivers beats to whoever asks; the host
rests between turns for as long as the Timer says the bus may
([`timer/rest.hpp`](../rest.hpp)). Your weave never sleeps, never polls, and
never talks to an id — always to the role.

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

If your rhythm is fixed — "this weave beats every 50 ms" — skip all of this
plumbing and use [`TimedWeave`](timed-weaves.md), which re-places declared
orders for you at exactly the right moments.

## What survives a replacement

Remaining *durations* cross to a verified successor (never wall-clock
deadlines — [TIMER-03](timer-laws.md)); your `preferred/fallback`
choice is honored per timer, and the receipt tells you what actually happened.
Deeper: [the Timer's manual](zengine.timer.md), every message it accepts and what it does when
it is replaced.

## What the cap bounds

A host rests between turns until what is due next, so no firing is late for the rest. With
nothing due it rests at most `kBeatCapMs` (`timer/vocabulary.hpp`), and that bound is a safety net,
not a source's lateness: a host that rests in Loom's one wait (`loom::host::Reactor`) wakes at once
for every source the wait holds -- a window, the Input role, a Neovim pane, the guest door each
claim what they wait on -- and a source the wait does not hold keeps a beat of its own
([`timer/held_beat.hpp`](../held_beat.hpp)). So the cap is the longest that anything neither waited
on nor due goes unseen, and the one turn a second an idle host keeps.

A suite's virtual clock that books each beat as elapsed may name a shorter step (`step_ms`), so
virtual seconds pass in many beats; a host's clock has none.
