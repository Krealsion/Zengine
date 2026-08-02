# Timer laws (TIMER)

Zengine owns these truths. Reference:
[timer-protocol](../reference/timer-protocol.md) ·
[timer-continuity](../reference/timer-continuity.md) ·
[timer-binding](../reference/timer-binding.md). Loom-tier laws they build on:
[`Loom docs/laws/`](../../../Loom/docs/laws/README.md).

## TIMER-01 — One beat chain per activated incarnation

LAW — Every successfully activated Timer incarnation establishes exactly one
beat chain, authored from its own activation. Stale, duplicate, replayed,
inherited or foreign Drives cannot establish another.

MEANS
- the chain rides the ROLE (`send_to_role`), so moving the role ends the
  incumbent's chain naturally — its parked beat resolves to the successor,
  which refuses a different activation's key;
- a new incarnation begins unactivated; a predecessor's queued Drive finds an
  inert weave.

DOES NOT MEAN
- that beat recognition is authentication — it establishes which sender owns
  the chain, not that the sender is trustworthy (all in-process code is
  trusted today).

PROVEN BY — `timer/timer_weave.hpp` (activation cursor + serial); suite
`timer` (chain cases, replacement-boundary chain end).

## TIMER-02 — The host owes the service nothing

LAW — The host never winds the clock. Loading the service is what starts time:
the control door activates the fresh incarnation and the service seeds its own
first beat.

MEANS
- load order decides nothing — every package arranges its own timers on its
  own activation (or on `TimerReady`);
- there is no host beat, no host loop, no host sleep.

DOES NOT MEAN
- that the pump is the Timer's — pumping the bus is running the world; the nap
  inside the beat is merely where the world breathes.

PROVEN BY — suite `timer` (no-wind cases); the snake host contributes nothing
to time.

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
evidence: [Loom docs/evidence/night-lab.md](../../../Loom/docs/evidence/night-lab.md).
