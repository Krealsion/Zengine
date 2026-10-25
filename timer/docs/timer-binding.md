# Timer binding (`TimedWeave`) — reference

**Reference.** The `TimedWeave` model, what it composes into a weave's manifest, and exactly
where its boundary is. To *use* it, start at [timed weaves](timed-weaves.md).

The composition layer over the raw protocol: a weave declares its rhythm once
and the layer keeps it true across activations and service replacements. Law:
[TIMER-05](timer-laws.md). Guide:
[timed-weaves](timed-weaves.md).

## Model

```cpp
class Pulse : public zengine::timer::TimedWeave<Pulse, PulseState,
                                                loom::Accept<...>, loom::Emit<...>> {
public:
    Pulse() : tick_(timers().repeat("pulse.tick", std::chrono::milliseconds(50),
                                    &Pulse::on_tick)) {}
    using TimedWeave::on;              // REQUIRED: keeps the inherited handlers reachable
    void on_tick(const TimerFired&, loom::Mail&);
private:
    Handle tick_;
};
```

Bindings are **authored at construction**, each a `Handle` (`timer/binding.hpp`):

| on `timers()` | delivers to |
|---|---|
| `repeat(id, delay, cb)` | this weave, at every beat |
| `once(id, delay, cb)` | this weave, once; fired, it is spent until restarted |
| `repeat_to_role(id, delay, role, cb)` | whoever holds `role` at each beat: the beat is the slot's |
| `once_to_role(id, delay, role, cb)` | whoever holds `role`, once |

| on a `Handle` | does |
|---|---|
| `cancel(mail)` | stop wanting it, and tell the service; a beat already on its way still arrives |
| `restart(mail)` | want it again and ask now: the one thing that arms a spent one-shot |
| `waiting()`, `spent()`, `canceled()` | where it stands, as this weave knows it; the Timer is not asked |
| `resolution()`, `resolution_reason()` | what the Timer last said it did about it, empty before a receipt |

The layer owns the plumbing:
it accepts `zen.Activated`, `TimerReady` and `TimerFired`, keeps an
[`ActivationCursor`](../../activation/docs/activation.md) (attested-only, per-operator lineage, replay-ignoring), and
**reconciles** the declared table — placing `EnsureTimer` orders — at exactly
two moments: an accepted activation, and `TimerReady`.

## The boundary (deliberate, priced by evidence)

The binding table is a declaration, **not a dynamic scheduling collection**
([TIMER-05](timer-laws.md)). A binding created after the
reconciliation moments waits for the next one — in practice the next
`TimerReady`, e.g. when the Timer service is replaced. A weave whose rhythm is
genuinely runtime data speaks the raw protocol directly and counts its own
beats; Night Lab's scheduler is the worked example
([evidence](https://github.com/Krealsion/Loom/blob/main/docs/evidence/night-lab.md)).

## The activation wall (compile-time)

A derived weave must not declare a raw
`on(const loom::Activated&, loom::Mail&)` — it would replace the layer's
reconciliation and silently cost every declared timer. The collision is a
**compile-time refusal** naming the alternative; author-domain activation work
goes in the optional hook, which runs *after* bindings are reconciled, inside
an already-accepted activation:

```cpp
void on_timed_activation(const loom::Activated&, loom::Mail&);
```

The hook cannot re-accept the activation (the cursor is exposed `const`), and
the missing-`using TimedWeave::on;` mistake is likewise a named compile-time
refusal.

## Replacement continuity

A `TimedWeave`'s orders are re-placed on the successor's `TimerReady` — that
is the reconciliation designed for the service being replaced beneath it. The
timers carry the weave's own ids, so `preserved_remaining` vs
`restarted_delay` receipts tell it exactly what survived
([the Timer's manual](zengine.timer.md#ensuretimer)).

## Tests

Zengine suite `timer` (binding cases) plus the four compile-negative CTest
targets (collision named, hook compiles, missing-`using` named, cursor bypass
refused) — each judged on its diagnostic, with a must-compile positive
control.
