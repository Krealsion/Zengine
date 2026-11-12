# zengine.timer -- the Timer

**Manual.** The Timer: the weave that gives other weaves time as messages, answering to the role
`zengine.timer` (a role is the name a weave answers to). A weave never sleeps or polls to wait:
it asks the Timer for a time, once or on repeat, and `TimerFired` arrives when that time comes. It
is the loadable weave `zengine-timer`, which a plan loads; it offers no pane, and other weaves
reach it by the messages below. A weave whose rhythm is part of what it is declares that rhythm
once with [TimedWeave](timed-weaves.md) and never sends these itself.

## About

The Timer reads no clock: it claims the time its soonest timer is due, and the host's one wait
wakes it then with the time (Hears, Follow). Nothing is queued to keep time. An ask is anchored at
the next time handed over: one more wake an arm. A host that does not wait on its role never wakes
it ([the host's wait](timers.md#the-hosts-wait)). A late repeating timer fires once.

A delay is normalized before it is scheduled, by the rule `timer.normalize_delay`
([`timer/normalize.hpp`](../normalize.hpp)): a repeating delay below 1 ms becomes 1, a one-shot
below 0 becomes 0 and is due at once, and every other delay is left as it is. The rule is
an operator: a Timer whose host offered it an operator surface when it was made spends the host's
rule, and one offered nothing spends the rule this repository authors
([the operator host](../../operator/docs/operator-host.md)). An offer that cannot serve the rule
refuses the load, its reason on stderr after `zengine-timer: `.

What it keeps is its table of standing timers, and seven counters a poke reads (`TimerState`):
`wakes` (the wait's wakes it took), `fired`, `active`, `dropped` (asks with nobody to answer),
`deferred_dropped` (asks lost from a full hold), `inherited` (timers it began with, from a
predecessor or a reload, 0 for a fresh start) and `gone` (timers dropped because the weave they
fire at is gone; a role's stay for its next holder).

When the Timer is replaced, its standing timers cross to its successor as remaining durations in
a letter, `TimerHandoff`, measured from the last time the wait handed it: two seconds left then
is two seconds left when it lands, the time since that wake paused. A letter carries at most
`kMaxHandoffEntries` timers, the first in table order; the rest start as after a hard replacement.
Reloaded in place, it keeps its counters and every standing timer, carried in its state as
remaining durations, and announces `TimerReady` again: an ask to preserve finds its timer there.
Unloaded, its timers end with it.

## Use

Speak to the role, never to an id: `mail.send_to_role(zengine::timer::kTimerRole, ...)`. Ask
with [`EnsureTimer`](#ensuretimer), which answers what it did, or [`StartTimer`](#starttimer),
which does not; each timer is named by an `id` you choose, scoped to you. When
[`TimerFired`](#follow) arrives, match its id against your own asks and ignore the rest: anyone
may aim a beat at a role.

When you hear [`TimerReady`](#follow), ask for your timers again, preferring `preserve_remaining`
so a timer the letter carried keeps its time. The Timer says it once each time it is activated,
and only after it has decided what it inherited. A weave loaded before the Timer needs it: its
first ask reached nobody, and nothing told it so. Asks that arrive while the Timer decides are
held, at most `kMaxDeferredOps` of them, and applied in arrival order once it has.

Leaving for good, send [`CancelAllMyTimers`](#cancelallmytimers). Being replaced, leave your role
timers standing: your successor inherits them.

Who may send: any weave granted these shapes. Workshop grants every send to a weave its plan
loads.

## Commands

### `StartTimer`

`StartTimer` v1 asks for `TimerFired` back to you after a delay, once or on repeat: the raw form,
with no receipt.

- **Takes:** `id`, your name for it; `delay_ms`, normalized as [About](#about) says; `repeat`,
  true to fire every `delay_ms`.
- **Answers:** nothing at once; `TimerFired{id}` to you when it comes due. Asking again with the
  same `id` replaces the timer, a full delay from now: that is how a cadence changes.
- **Refuses:** nothing it says. An ask no weave sent has nobody to fire at, and is dropped and
  counted in `dropped`. A timer whose weave is gone fires into the bus's refusal, a repeating one
  until it is cancelled or the Timer is replaced.
- **Example:** `mail.send_to_role(kTimerRole, StartTimer{"poll.jobs", 250, true});`

### `StartRoleTimer`

`StartRoleTimer` v1 asks for `TimerFired` to whoever holds a role at each firing, so a holder's
successor inherits the beat.

- **Takes:** `id`, `delay_ms` and `repeat`, as [`StartTimer`](#starttimer); `role`, the role
  the beat goes to.
- **Answers:** `TimerFired{id}` to the role's holder at each firing. A role timer is keyed by its
  role and id whoever sent it, so a successor asking again replaces the beat, and takes the right
  to cancel it, rather than doubling it.
- **Refuses:** an empty `role` is no ask: dropped, and counted in `dropped`. A firing while nobody
  holds the role is refused by the bus, and the timer keeps standing.
- **Example:** a weave asks for its 10 ms beat at its own role, so its successor inherits it,
  `StartRoleTimer{1, 10, true, "zengine.input"}`.

### `EnsureTimer`

`EnsureTimer` v1 asks for time back to you with a preference about the timer already standing
under that id, and answers what it did.

- **Takes:** `id`, `delay_ms` and `repeat`, as [`StartTimer`](#starttimer); `preferred`, one of
  `preserve_remaining`, `restart_delay` or `drop`; `fallback`, one of those, or empty when none
  is acceptable.
- **Answers:** `TimerResolution{id, resolved, reason}` to you. `resolved` is
  `preserved_remaining` (the standing timer kept its time), `restarted_delay` (the next firing is
  the delay from now), `dropped` (no timer is kept or made) or `refused`; `reason` says which
  preference was available and what was chosen, as `'preserve_remaining' was unavailable (no
  matching schedule to preserve); fell back to 'restart_delay': the next firing is the declared
  delay from now`. A timer can be preserved only when one stands under the id with the same
  `repeat` and the same normalized delay.
- **Refuses:** `refused`, changing nothing: `unknown continuity preference 'later'; no schedule
  was created or changed`; `... and no fallback was acceptable; no schedule was created or
  changed`; `... and the declared fallback 'later' could not be used; ...`; and while it decides
  what it inherited, with its hold full, `the timer service is still restoring its schedule and
  its bounded hold of pending operations is full; no schedule was created or changed`. An order
  no weave sent is dropped and counted.
- **Example:** `EnsureTimer{"poll.jobs", 250, true, "preserve_remaining", "restart_delay"}`, sent
  on every `TimerReady`.

### `EnsureRoleTimer`

`EnsureRoleTimer` v1 is the ordered form of [`StartRoleTimer`](#startroletimer): a role's beat,
with a preference and a receipt.

- **Takes:** as [`EnsureTimer`](#ensuretimer), and `role`.
- **Answers:** `TimerResolution` to the weave that ordered it, as [`EnsureTimer`](#ensuretimer);
  the beat goes to the role's holder.
- **Refuses:** `a role beat with no role is no ask at all; no schedule was created or changed`,
  and every refusal of [`EnsureTimer`](#ensuretimer).
- **Example:** what a [TimedWeave](timed-weaves.md)'s role binding sends at its activation.

### `CancelTimer`

`CancelTimer` v1 cancels your timer with this id, and any role timer with this id you started or
took over by asking again.

- **Takes:** `id`.
- **Answers:** nothing. A firing already on its way still arrives; after it, nothing.
- **Refuses:** nothing: an id you hold no timer under changes nothing. A role timer whose starter
  is gone is cancelled by a holder that first asks for it again.
- **Example:** `mail.send_to_role(kTimerRole, CancelTimer{"poll.jobs"});`

### `CancelAllMyTimers`

`CancelAllMyTimers` v1 cancels every timer you started, role timers included.

- **Takes:** nothing.
- **Answers:** nothing; no timer of yours fires again but one already on its way.
- **Refuses:** nothing. It never guesses which beats you meant to leave to an heir: a role timer
  nobody cancels keeps firing into its role after its holder is gone.
- **Example:** a weave retiring with no successor sends it on its way out.

### `PrepareTimerHandover`

`PrepareTimerHandover` v1 asks a candidate Timer, loaded sealed beside the live one, to be ready
to become the Timer.

- **Takes:** `transaction`, echoed back; `continuity`, `inherit` (a letter is coming from the
  live Timer: hold every ask until it lands) or `fresh` (carry nothing and wait for nothing).
- **Answers:** `TimerCandidatePrepared{transaction}` to the asker: room for a whole letter is
  reserved and the candidate knows where its letter comes from. Once admitted the role moves, and
  its claim on the host's wait with it, and the sealed incumbent writes its letter. The candidate,
  activated, asks the preparer for it by id and waits at most `kPreparedClaimWakes` wakes before
  starting fresh; told
  `fresh`, it asks nobody. A failed attempt leaves the live Timer and its timers as they were.
- **Refuses:** `TimerCandidateDeclined{transaction, reason}`: `this Timer is already live under
  an accepted activation; a serving service is not a candidate`; `this incarnation has already
  answered a preparation ask; one transaction opens one preparation conversation`; `unknown
  continuity plan 'later'; this Timer prepares for 'inherit' or 'fresh' and guesses at neither`.
- **Example:** Loom's prepared replacement sends it through a sealed candidate's coordinator
  door ([Loom's prepared
  replacement](https://github.com/Krealsion/Loom/blob/main/docs/reference/prepared-replacement.md)).

## Hears

### `loom.host.Waited`

`loom.host.Waited` v2, from the host's one wait, with no sender: the time, `now_ms`, on the
wait's clock, and `due` when the time the Timer claimed came. The Timer anchors what waited for a
time at it and fires what is due by it. One from any participant is nothing, and so is one before
the Timer is activated.


## Follow

The Timer says, unasked: `TimerReady` v1, once per activation after it has decided what it
inherited; `TimerFired` v1, to the weave or role a timer names, each time it comes due;
`TimerResolution` v1, to the sender of an ordered ask. Activated, it claims its predecessor's
letter (`zen.ClaimBequest` to the Loom's manager role) and waits `kBootstrapWakes` wakes for it,
or starts fresh at once when the bus refuses the claim (`zen.DispatchRefused`); a `TimerFired`
the bus refuses for a weave that is gone drops that weave's timers. Asked `zen.PrepareShutdown`,
it answers with a `zen.Bequest` holding `TimerHandoff` v1. After every wake, and whenever what it
waits for changed, it claims `loom.host.WaitSet` v2 as the role: `until_ms`, its soonest timer's
time, a time already past while it must learn the time, none when nothing stands.

## See also

- [Using the Timer](timers.md), ordering a timer with these messages.
- [TimedWeave](timed-weaves.md), a weave's rhythm declared once, and
  [the Timer binding](timer-binding.md), what it adds and where it stops.
- [Timer laws (TIMER)](timer-laws.md), and
  [why a letter carries remaining durations](timer-continuity-carries-remaining-duration.md).
- [Operator providers](../../operator/docs/operator-providers.md), how the Timer supplies
  `timer.normalize_delay`.
- [Loom's lifecycle](https://github.com/Krealsion/Loom/blob/main/docs/reference/lifecycle.md), the
  replacement the letter crosses.
