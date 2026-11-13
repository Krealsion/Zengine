# Timer laws (TIMER)

**Invariants.** The numbered truths the Timer package keeps, each with the evidence that pins
it. Cite these by number.

Zengine owns these truths. Reference:
[the Timer's manual](zengine.timer.md) ·
[timer-binding](timer-binding.md). Loom-tier laws they build on:
[`Loom docs/laws/`](https://github.com/Krealsion/Loom/blob/main/docs/laws/README.md).

## TIMER-01 — One claim of time per activated incarnation

LAW — Every successfully activated Timer incarnation keeps time by one claim, as
its role, of the time its soonest timer is due, and fires only on a time the
host's own wait hands it. A wake from any participant, a duplicate or replayed
activation, or one before activation, moves nothing.

MEANS
- the claim rides the ROLE, so moving the role ends the incumbent's wakes
  naturally: the wait reads the role's claim, which is the successor's now;
- nothing of the Timer's time is on the bus between wakes, so a replacement
  has nothing in flight to refuse or inherit;
- a new incarnation begins unactivated; a wake meant for a predecessor finds an
  inert weave.

DOES NOT MEAN
- that the host's word is authentication of anything but the time — it is the
  word with no sender, which only the host can say (all in-process code is
  trusted today).

PROVEN BY — `timer/timer_weave.hpp` (activation cursor, `after_delivery`'s
claim); suite `timer` (the host's word, the claim at replacement); suite
`audit_probes`.

## TIMER-02 — The host owes the service its wait, and nothing else

LAW — The host never winds the clock and the service reads none. The host
turns the bus and rests in its one wait, trusting the Timer's role; the wait
wakes the service when the time it claimed comes, handing it that time, and
the service fires what is due by it. No handler sleeps or reads a clock, and
nothing is queued to keep time.

MEANS
- load order decides nothing — every package arranges its own timers on its
  own activation (or on `TimerReady`);
- there is no host beat and no host schedule: what the host rests for is the
  service's claim, never a time of its own;
- a reply to a firing, a key or an answer is delivered in the next turn, not
  after a rest: the host rests only with its bus empty;
- a host that does not wait on the Timer's role wakes it never.

DOES NOT MEAN
- that the pump is the Timer's — pumping the bus is running the world; the
  wait between turns is merely where the world breathes.

PROVEN BY — suite `timer` (the host's loop on a clock that moves only when
told; a reply in the next turn; no Timer, no time); the snake host winds
nothing and waits on the Timer's role.

## TIMER-03 — Continuity carries remaining duration, never due times

LAW — A Timer handoff letter converts every active entry's absolute deadline
into a **remaining duration** at one clock read. `remaining_ms` is never a due
time.

MEANS
- the letter is meaningful on the successor's clock, whatever epoch it runs;
- writing the letter fires nothing, cancels nothing, advances nothing — being
  asked to describe yourself is not an event in a schedule's life.

DOES NOT MEAN
- that deadlines are "preserved" as instants — a moving clock cannot cross as
  timestamps; that is the point.

PROVEN BY — `TimerHandoffEntry.remaining_ms`; suite `timer` (moving-clock
one-second proof; the two-seconds-remaining keystone).

## TIMER-04 — TimerReady follows the continuity decision

LAW — A successor may not announce `TimerReady` before it has decided what it
inherited (restored a letter, or honestly started fresh).

MEANS
- `TimerReady` is what makes consumers re-ask; announcing early would make
  them re-anchor the very schedules the letter carried ("two seconds left"
  silently becoming "five seconds from now");
- operations arriving during the decision are held and replayed after it, in
  order.

DOES NOT MEAN
- that readiness (the *replacement* transaction's `Ready`) and `TimerReady`
  are the same word — one is the bus's verdict about a candidate, the other is
  this package's service announcement.

PROVEN BY — suite `timer` (no-TimerReady-before-restoration, hold-and-replay
cases).

## TIMER-05 — The binding table is authored, not dynamic

LAW — `TimedWeave` bindings are declared at construction and reconciled at
known moments: the weave's activation and `TimerReady`. It is not a general
dynamic scheduling collection.

MEANS
- a binding created after those moments waits for the next reconciliation
  (in practice: the next `TimerReady` — e.g. when the Timer service is
  replaced) rather than reconciling immediately;
- a weave whose rhythm is genuinely data-driven speaks the raw protocol
  (`EnsureTimer` etc.) and counts its own beats.

DOES NOT MEAN
- that this is a defect — the boundary is the design (authored rhythm), found
  and priced by a real application (Night Lab's scheduler).

PROVEN BY — `timer/binding.hpp` (reconciliation moments); suite `timer`;
evidence: [Loom docs/evidence/night-lab.md](https://github.com/Krealsion/Loom/blob/main/docs/evidence/night-lab.md).
