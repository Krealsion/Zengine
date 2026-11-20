# The Timer's laws (TIMER)

**Laws.** The numbered rules the Timer keeps, each a rule that more than one file must keep
true. A comment or a page that relies on one names it by its number. A rule about one
declaration is in that declaration's comment, not here. The messages themselves are in [the
Timer's manual](zengine.timer.md), and the Loom laws these build on are
[Loom's](https://github.com/Krealsion/Loom/blob/main/docs/laws/README.md).

## TIMER-01 — A Timer keeps time by one claim, and only the host tells it the time

Each Timer that Loom has activated keeps time by one claim on the host's wait, made as its role:
the time its soonest timer is due. It fires only on a time that the host's own wait sends it in a
`loom::host::Waited` message. A `Waited` that any weave sent, an activation that is a duplicate or
a replay, and anything that arrives before activation change nothing.

What this means:

- The claim belongs to the role, so when the role moves to a new Timer the old one stops being
  woken: the wait reads the role's claim, which is now the new Timer's.
- Between two wakes nothing of the Timer's time is on the bus, so a replacement finds no message
  in flight to refuse or to inherit.
- A new Timer starts unactivated, so a wake meant for the Timer before it finds a Timer that does
  nothing with it.

What it does not mean: that a message with no sender proves anything but the time. Only the host
can send one, and all code in one process is trusted today.

Proved by `TimerWeave` in `timer/timer_weave.hpp`, by the `timer` suite (a `Waited` from a weave,
and the claim across a replacement) and by the `audit_probes` suite.

## TIMER-02 — The host owes the Timer its wait, and nothing else

The host never moves the clock forward and the Timer reads none. The host turns the bus and then
rests in its one wait, with the Timer's role among the roles it trusts. The wait wakes the Timer
when the time it claimed comes and sends it that time, and the Timer fires every timer due by it.
No handler sleeps or reads a clock, and no message is queued to keep time.

What this means:

- Load order decides nothing: every weave asks for its own timers on its own activation, or when
  it hears `TimerReady`.
- The host keeps no beat and no schedule of its own. What it rests for is the Timer's claim.
- A reply to a firing, a key or an answer is delivered in the next turn, not after a rest: the
  host rests only when its bus is empty.
- A host that does not wait on the Timer's role never wakes it, so no timer fires there.

What it does not mean: that turning the bus belongs to the Timer. Turning the bus runs every
weave; the wait between turns is only where the program rests.

Proved by the `timer` suite (the host's loop on a clock that moves only when a case moves it; a
reply in the next turn; no Timer, no time) and by the snake host, which moves no clock and waits on
the Timer's role.

## TIMER-03 — A replacement carries the time each timer has left, never when it is due

When a Timer is replaced, the letter it writes for its successor turns every standing timer's
deadline into the time it has left, `remaining_ms`, measured at one reading of the clock.
`remaining_ms` is never a time of day or a deadline.

What this means:

- The letter means the same on the successor's clock, whatever that clock counts from.
- Writing the letter fires, cancels and moves nothing: being asked to describe a schedule is not an
  event in its life.
- The clock reading is the wait's. A Timer the wait holds writes its letter when the next wait
  sends it the time; one the wait does not hold writes it from its last wake, the only time it has.

What it does not mean: that deadlines cross as instants. A clock that keeps moving cannot carry an
instant to another clock, and that is why the letter carries durations ([the
decision](timer-continuity-carries-remaining-duration.md)).

Proved by `TimerHandoffEntry::remaining_ms` and by the `timer` suite (a clock that moves one
second while the letter is prepared, so the letter carries 1000 and not 2000; a timer with two
seconds left that still has two seconds left after the replacement).

## TIMER-04 — A new Timer says `TimerReady` only after it has decided what it inherited

A Timer that succeeds another does not announce `TimerReady` until it has decided what it
inherited: the letter it restored, or a fresh start.

What this means:

- `TimerReady` is what makes every weave ask for its timers again. Said too early, it would make
  them restart the very timers the letter carried, and "two seconds left" would become "the full
  delay from now".
- Asks that arrive while the Timer decides are held, and applied in the order they arrived once it
  has decided.

What it does not mean: that `TimerReady` and the `Ready` of Loom's replacement are one thing. Loom
says `Ready` of a candidate; `TimerReady` is the Timer's own notice that it is serving.

Proved by the `timer` suite (no `TimerReady` before the letter is restored, and the cases that hold
asks and apply them after).

## TIMER-05 — The binding table is authored, not dynamic

The timers a `TimedWeave` declares are declared in its constructor, and the binding asks for them
at two known moments: the weave's activation, and `TimerReady`. The table is not a collection a
weave adds to at run time.

What this means:

- A binding declared after those moments waits for the next one, which in practice is the next
  `TimerReady`, as when the Timer is replaced.
- A weave whose rhythm is data sends the plain messages (`EnsureTimer` and the rest) and counts its
  own beats.

What it does not mean: that this is a defect. The fixed table is the design, and a real
application's scheduler is what found and priced it
([Night Lab's evidence](https://github.com/Krealsion/Loom/blob/main/docs/evidence/night-lab.md)).

Proved by `TimerBindings` in `timer/binding.hpp` and by the `timer` suite's binding cases.
