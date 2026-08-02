# TimedWeave — a weave with an authored rhythm

If your weave's rhythm is part of what it *is* — "I pump every 50 ms", "I poll
every quarter second" — declare it once and let the binding layer keep it true
across activations and Timer-service replacements.

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

What the layer does for you: accepts `zen.Activated` / `TimerReady` /
`TimerFired`, trusts activations only when Loom attests them, and **re-places
your declared orders** at the two moments that matter — your activation, and
the Timer service announcing itself (including after it is replaced). Cancel a
binding any time with `tick_.cancel(mail)`.

## Activation work

Don't declare a raw `on(const loom::Activated&, ...)` — the compiler refuses
by name (it would replace the layer's reconciliation and silently cost every
declared timer). Domain work that belongs to your first breath goes here,
*after* your timers are already ordered:

```cpp
void on_timed_activation(const loom::Activated&, loom::Mail& mail) {
    mail.publish(ImAlive{});
}
```

## Know the boundary

The binding table is **authored, not dynamic**
([TIMER-05](../laws/timer-laws.md)): a binding created after the
reconciliation moments waits for the next `TimerReady` rather than
reconciling immediately. If your rhythm is runtime data — jobs with their own
cadences, user-configured alarms — speak the
[raw protocol](timers.md) directly and count your own beats; that is the
designed split, not a workaround.

Deeper: [timer-binding reference](../reference/timer-binding.md).
