# TimedWeave: a weave with a declared rhythm

**How-to and reference.** A weave whose rhythm is part of what it is declares that rhythm once,
and the binding keeps it true. When a delay is data the weave learns at run time, it sends [the
plain messages](timers.md) instead.

If your weave moves every 50 ms or polls every quarter second, that rhythm is part of what it
is. Declare it once in the constructor and let `TimedWeave` keep it true: when the weave is
activated, and again whenever the Timer is replaced.

```cpp
#include "timer/binding.hpp"
using zengine::timer::TimedWeave;
using zengine::timer::TimerFired;

struct PulseState {
    std::int64_t beats = 0;
    ZEN_SHAPE(PulseState, 1, ZEN_FIELD(beats));
};

class Pulse : public TimedWeave<Pulse, PulseState,
                                loom::Accept<>, loom::Emit<>> {
public:
    Pulse() : tick_(timers().repeat("pulse.tick",
                                    std::chrono::milliseconds(50),
                                    &Pulse::on_tick)) {}

    using TimedWeave::on;   // required: keeps the inherited handlers reachable

    void on_tick(const TimerFired&, loom::Mail&) { ++state_.beats; }

private:
    Handle tick_;
};
```

## What the binding does for you

`TimedWeave` accepts `zen.Activated`, `TimerReady`, `TimerFired` and `TimerResolution` for you, and
adds them, with the messages it sends the Timer, to your weave's declared contract. It trusts an
activation only when Loom attests it. It asks the Timer for every declared timer at two moments:
when your weave's activation is accepted, and when the Timer announces itself with `TimerReady`,
which it does again after it is replaced. Each ask is an `EnsureTimer` that prefers to keep the
time a standing timer has left, so a timer the old Timer passed on keeps its time, and the receipt
says what happened. A firing reaches the callback you declared under its id, and nothing else.

The factories on `timers()` each return a `Handle`:

| on `timers()` | the beat goes to |
|---|---|
| `repeat(id, delay, cb)` | this weave, at every beat |
| `once(id, delay, cb)` | this weave, once; after it fires it stays spent until you restart it |
| `repeat_to_role(id, delay, role, cb)` | whoever holds `role` at each beat, so a successor in that role inherits it |
| `once_to_role(id, delay, role, cb)` | whoever holds `role`, once |

| on a `Handle` | what it does |
|---|---|
| `cancel(mail)` | stops wanting the timer and tells the Timer; a beat already on its way still arrives |
| `restart(mail)` | wants it again and asks now; it is the only thing that arms a spent one-shot |
| `waiting()`, `spent()`, `canceled()` | where it stands, as your weave knows it; the Timer is not asked |
| `resolution()`, `resolution_reason()` | what the Timer last said it did about it, empty before its first receipt |

## Work at activation

Do not declare your own `on(const loom::Activated&, loom::Mail&)`. It would replace the binding's
handler, and none of your timers would ever be asked for, so the compiler refuses it and names the
alternative. Work that belongs to your weave's first moment goes in the optional hook, which runs
after your timers have been asked for, and only for an activation your weave accepted:

```cpp
void on_timed_activation(const loom::Activated&, loom::Mail& mail) {
    mail.publish(ImAlive{});
}
```

The hook cannot accept the activation a second time, because the binding gives it only a read-only
view of the activation's state. Forgetting `using TimedWeave::on;` is refused by name at compile
time too.

## Where it stops

The declared timers are a fixed table, not a collection to add to at run time
([TIMER-05](timer-laws.md#timer-05--the-binding-table-is-authored-not-dynamic)). A binding
declared after the two moments above waits for the next `TimerReady`, which may not come until the
Timer is replaced. If your rhythm is data, such as jobs with their own cadences or alarms a person
sets, send [the plain messages](timers.md) and count your own beats. That is the intended split,
found by a real scheduler that needed it
([Night Lab's evidence](https://github.com/Krealsion/Loom/blob/main/docs/evidence/night-lab.md)).

## Across a replacement

Your timers carry your own ids, so after a replacement each receipt tells you what survived:
`preserved_remaining` when the timer kept its time, `restarted_delay` when it starts its delay
again ([`EnsureTimer` in the manual](zengine.timer.md#ensuretimer)).

The binding's cases are in the `timer` suite, and the four compile-time refusals and their control
are CTest entries of their own ([the folder's suites](../README.md#working-on-it)).
