# Using the Timer

**How-to.** Asking the Timer for a timer with the plain messages. For a rhythm that is part of what
your weave is, use [TimedWeave](timed-weaves.md) instead: it sends these messages for you.

Time in Zen is a service. The weave `zengine-timer` answers to the role `zengine.timer` and fires
timers for any weave that asks, at the times the host's wait sends it ([the host's
wait](#the-hosts-wait)). Your weave never sleeps, never polls and never reads a clock, and it
always sends to the role, never to the Timer's id, because the weave behind the role can be
replaced.

## Ask for a timer

```cpp
using namespace zengine::timer;

// In a handler, with your Mail:
mail.send_to_role(kTimerRole, EnsureTimer{
    .id = "poll.jobs",                     // your own name for it; another weave's ids are apart
    .delay_ms = 250,
    .repeat = true,
    .preferred = kPreserveRemaining,       // keep the time a standing timer has left
    .fallback  = kRestartDelay,            // or else start the delay again
});
```

Asking again under the same id changes the timer you already have rather than adding a second.
With `EnsureTimer` you also say what should happen to a timer already standing under that id: a
preference, and a fallback you can accept when the preference cannot be met. If neither can be
met, the Timer refuses and changes nothing, rather than guessing. Its answer comes back to you as
`TimerResolution{id, resolved, reason}`, where `resolved` is `preserved_remaining`,
`restarted_delay`, `dropped` or `refused`, and `reason` says why in a sentence.

Each beat arrives as `TimerFired{id}`: to you, or to whoever holds a role if you asked with
`EnsureRoleTimer`. Cancel a timer with `CancelTimer{id}`.

## The one rule that keeps a timer's time

When you hear `TimerReady`, ask for your timers again. The Timer sends it when it starts and again
after it is replaced, and it sends it only after it has decided what it inherited from the Timer
before it ([TIMER-04](timer-laws.md#timer-04--a-new-timer-says-timerready-only-after-it-has-decided-what-it-inherited)).
So asking again, preferring `preserve_remaining`, finds the timer the old Timer passed on and keeps
its time.

```cpp
void on(const TimerReady&, loom::Mail& mail) { ask_for_my_timers(mail); }
```

If your rhythm is fixed, such as a weave that moves every 50 ms, skip all of this and use
[`TimedWeave`](timed-weaves.md), which asks again for you at the right moments.

## What survives a replacement

The time each timer has left crosses to the new Timer, never a deadline
([TIMER-03](timer-laws.md#timer-03--a-replacement-carries-the-time-each-timer-has-left-never-when-it-is-due)).
Your preference and fallback are honoured timer by timer, and the receipt tells you what
happened. Every message the Timer accepts, and what it does when it is replaced, is in [the
Timer's manual](zengine.timer.md).

## The host's wait

A host turns its bus and then rests in Loom's one wait (`loom::host::Reactor`), on every role it
trusts. The Timer's role is one of them, beside every source that reads the outside world: a
window, the Input role, a Neovim pane and the guest door each claim what they wait on, and the time
by which they must look again ([`timer/source_wait.hpp`](../source_wait.hpp)). The Timer claims the
time its soonest timer is due, and the wait wakes it then and sends it the time. With no timer
standing it claims no time, and a host with nothing to do rests until something is ready.
Workshop and the app trust the Timer's role. A host that does not wait on it, either because it
runs no wait or because it is `loom-host`, which trusts no role, never wakes a Timer, so timers
never fire there.

A suite runs its host's loop on a clock that moves only when the case moves it
(`loom::host::ManualClock`). A wait that finds nothing ready moves that clock to the soonest time
due, so a case can say "run until this time" and every duration is exact.
