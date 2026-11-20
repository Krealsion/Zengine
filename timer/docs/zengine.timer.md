# zengine.timer -- the Timer

**Manual.** The Timer: the weave that gives other weaves time as messages, answering to the role
`zengine.timer` (a role is the name a weave answers to). A weave never sleeps or polls to wait: it
asks the Timer for a timer, once or on repeat, and `TimerFired` arrives when the timer comes due.
It is the loadable weave `zengine-timer`, which a load plan loads; it has no pane, and other
weaves reach it by the messages below. A weave whose rhythm is part of what it is declares that
rhythm once with [TimedWeave](timed-weaves.md) and never sends these messages itself.

## About

The Timer reads no clock. It tells the host's wait when its soonest timer is due, and the wait
wakes it then and sends it the time (`loom.host.Waited`). No message is queued to keep time. A
timer asked for while the Timer does not know the time starts at the next time the wait sends, so
a first ask costs one more wake. A host that does not wait on the Timer's role never wakes it
([the host's wait](timers.md#the-hosts-wait)). A repeating timer that comes due late fires once,
not once for every beat it missed.

A delay is normalized before it is scheduled, by the rule `timer.normalize_delay`
([`timer/normalize.hpp`](../normalize.hpp)): a repeating delay below 1 ms becomes 1, a one-shot
below 0 becomes 0 and is due at once, and every other delay stays as it is. The rule is an
operator: a Timer whose host offered it operators uses the host's rule, and one offered nothing
uses this repository's ([the operator host](../../operator/docs/operator-host.md)). An offer that
cannot serve the rule refuses the load, its reason on stderr after `zengine-timer: `.

The Timer keeps its timers and seven counters a poke of its state reads (`TimerState`):

- `wakes`: the wakes of the host's wait it took.
- `fired`: the timers it fired.
- `active`: the timers standing now.
- `dropped`: asks dropped because nobody could be answered.
- `deferred_dropped`: asks dropped because the hold was full while it decided what it inherited.
- `inherited`: the timers it began with, from a predecessor's letter or a reload in place; 0 for
  a fresh start.
- `gone`: timers dropped because the weave they fire at is gone. A role's timer stays for the
  role's next holder.

When the Timer is replaced, its timers cross to its successor as the time each has left, in a
letter, `TimerHandoff`, measured at the next time the wait sends, or at its last wake if the host
runs no wait for it: two seconds left then is two seconds left when it lands. A letter carries at most
`kMaxHandoffEntries` timers, the first in table order; the rest start again as they would if no
letter came. Reloaded in place, the Timer keeps its counters
and timers, as the time each had left at its last wake, and announces `TimerReady` again, so an
`EnsureTimer` preferring `preserve_remaining` finds its timer there. Unloaded, its timers end with it.

## Use

Send to the role, never to an id: `mail.send_to_role(zengine::timer::kTimerRole, ...)`. Ask with
[`EnsureTimer`](#ensuretimer), which answers with what it did, or [`StartTimer`](#starttimer),
which does not; each timer is named by an `id` you choose, and your ids are yours alone. When
[`TimerFired`](#follow) arrives, match its id against your own asks and ignore the rest, because
any weave may aim a beat at a role.

When you hear [`TimerReady`](#follow), ask for your timers again, preferring `preserve_remaining`,
so that a timer the letter carried keeps its time. The Timer sends it once each time it is
activated, and only after it has decided what it inherited. A weave loaded before the Timer needs
it: its first ask reached nobody, and nothing told it so. Asks that arrive while the Timer decides
are held, at most `kMaxDeferredOps` of them, and applied in the order they arrived once it has.

When your weave leaves for good, send [`CancelAllMyTimers`](#cancelallmytimers). When it is being
replaced, leave its role timers standing, so that its successor inherits them.

Who may send: any weave granted these shapes. Workshop grants every send to a weave its plan
loads.

## Commands

### `StartTimer`

`StartTimer` v1 asks for `TimerFired` back to you after a delay, once or on repeat. It is the
plain form, and it sends no receipt.

- **Takes:** `id`, your name for the timer; `delay_ms`, normalized as [About](#about) says;
  `repeat`, true to fire every `delay_ms`.
- **Answers:** nothing at once; `TimerFired{id}` to you when the timer comes due. Asking again
  with the same `id` replaces the timer and starts a full delay from now, which is how a weave
  changes its rhythm.
- **Refuses:** nothing in words. An ask that no weave sent has nobody to fire at, so it is
  dropped and counted in `dropped`. A timer whose weave is gone fires into the bus's refusal, and a
  repeating one keeps firing until it is cancelled or the Timer is replaced.
- **Example:** `mail.send_to_role(kTimerRole, StartTimer{"poll.jobs", 250, true});`

### `StartRoleTimer`

`StartRoleTimer` v1 asks for `TimerFired` to whoever holds a role at each firing, so that the
holder's successor inherits the beat.

- **Takes:** `id`, `delay_ms` and `repeat`, as [`StartTimer`](#starttimer); `role`, the role the
  beat goes to.
- **Answers:** `TimerFired{id}` to the role's holder at each firing. A role timer is keyed by its
  role and its id, whoever sent it, so a successor that asks again replaces the beat, and gains the
  right to cancel it, rather than doubling it.
- **Refuses:** an empty `role` is no ask: it is dropped and counted in `dropped`. A firing while
  nobody holds the role is refused by the bus, and the timer keeps standing.
- **Example:** a weave asks for its 10 ms beat at its own role, so that its successor inherits it:
  `StartRoleTimer{1, 10, true, "zengine.input"}`.

### `EnsureTimer`

`EnsureTimer` v1 asks for a timer back to you, says what should happen to a timer already standing
under that id, and answers with what it did.

- **Takes:** `id`, `delay_ms` and `repeat`, as [`StartTimer`](#starttimer); `preferred`, one of
  `preserve_remaining`, `restart_delay` or `drop`; `fallback`, one of those, or empty when no
  fallback is acceptable.
- **Answers:** `TimerResolution{id, resolved, reason}` to you. `resolved` is
  `preserved_remaining` (the standing timer kept its time), `restarted_delay` (the next firing is
  the delay from now), `dropped` (no timer is kept or made) or `refused`; `reason` says which
  preference was available and what was chosen, as `'preserve_remaining' was unavailable (no
  matching schedule to preserve); fell back to 'restart_delay': the next firing is the declared
  delay from now`. A timer can be kept only when one stands under the id with the same `repeat`
  and the same normalized delay.
- **Refuses:** `refused`, changing nothing: `unknown continuity preference 'later'; no schedule
  was created or changed`; `... and no fallback was acceptable; no schedule was created or
  changed`; `... and the declared fallback 'later' could not be used; ...`; and, while the Timer
  decides what it inherited and its hold is full, `the timer service is still restoring its
  schedule and its bounded hold of pending operations is full; no schedule was created or
  changed`. An order that no weave sent is dropped and counted.
- **Example:** `EnsureTimer{"poll.jobs", 250, true, "preserve_remaining", "restart_delay"}`, sent
  on every `TimerReady`.

### `EnsureRoleTimer`

`EnsureRoleTimer` v1 is the ordered form of [`StartRoleTimer`](#startroletimer): a role's beat,
with a preference and a receipt.

- **Takes:** as [`EnsureTimer`](#ensuretimer), and `role`.
- **Answers:** `TimerResolution` to the weave that asked, as [`EnsureTimer`](#ensuretimer); the
  beat goes to the role's holder.
- **Refuses:** `a role beat with no role is no ask at all; no schedule was created or changed`,
  and every refusal of [`EnsureTimer`](#ensuretimer).
- **Example:** what a [TimedWeave](timed-weaves.md)'s role binding sends at its activation.

### `CancelTimer`

`CancelTimer` v1 cancels your timer with this id, and any role timer with this id that you
started or took over by asking again.

- **Takes:** `id`.
- **Answers:** nothing. A firing already on its way still arrives; after it, nothing does.
- **Refuses:** nothing: an id you hold no timer under changes nothing. A role timer whose starter
  is gone is cancelled by a holder that first asks for it again.
- **Example:** `mail.send_to_role(kTimerRole, CancelTimer{"poll.jobs"});`

### `CancelAllMyTimers`

`CancelAllMyTimers` v1 cancels every timer you started, role timers included.

- **Takes:** nothing.
- **Answers:** nothing; no timer of yours fires again, except one already on its way.
- **Refuses:** nothing. It never guesses which beats you meant to leave to a successor: a role
  timer that nobody cancels keeps firing into its role after its holder is gone.
- **Example:** a weave retiring with no successor sends it on its way out.

### `PrepareTimerHandover`

`PrepareTimerHandover` v1 asks a candidate Timer, loaded sealed beside the live one, to get ready
to become the Timer.

- **Takes:** `transaction`, echoed back; `continuity`, either `inherit` (a letter is coming from
  the live Timer, so hold every ask until it lands) or `fresh` (carry nothing and wait for
  nothing).
- **Answers:** `TimerCandidatePrepared{transaction}` to the asker: space for a whole letter is
  reserved, and the candidate knows where its letter will come from. Once the candidate is
  admitted, the role moves to it, and the claim on the host's wait moves with the role; the sealed
  old Timer then writes its letter. The candidate, activated, asks the weave that prepared it for
  the letter by id, and waits at most `kPreparedClaimWakes` wakes before starting fresh; told
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
wait's clock, and `due` when the time the Timer claimed has come. The Timer starts counting every
timer that was waiting to learn the time, and fires every timer due by it. A `Waited` from any
weave changes nothing, and neither does one before the Timer is activated.

## Follow

Unasked, the Timer sends `TimerReady` v1 once per activation, after it has decided what it
inherited; `TimerFired` v1 to the weave or role a timer names, each time it comes due; and
`TimerResolution` v1 to the sender of an ordered ask. Activated, it asks Loom's manager role for
its predecessor's letter (`zen.ClaimBequest`) and waits `kBootstrapWakes` wakes for it, or starts
fresh at once when the bus refuses the ask (`zen.DispatchRefused`). When the bus refuses a
`TimerFired` because its weave is gone, it drops that weave's timers. Asked
`zen.PrepareShutdown`, it answers with a `zen.Bequest` holding `TimerHandoff` v1 once the next
wait sends it the time, or at once where no wait holds it; past `kMaxLettersHeld` waiting asks,
it answers `zen.Refused`. After every wake, and whenever what it waits for changed, it claims
`loom.host.WaitSet` v2 as its role: `until_ms` is its soonest timer's time, a time already past
while it must learn the time, or none when no timer stands.

## See also

- [Using the Timer](timers.md), asking for a timer with these messages.
- [TimedWeave](timed-weaves.md), a weave's rhythm declared once: what the binding adds, and where
  it stops.
- [The Timer's laws](timer-laws.md), and
  [why a letter carries remaining durations](timer-continuity-carries-remaining-duration.md).
- [Operator providers](../../operator/docs/operator-providers.md), how the Timer supplies
  `timer.normalize_delay`.
- [Loom's lifecycle](https://github.com/Krealsion/Loom/blob/main/docs/reference/lifecycle.md), the
  replacement the letter crosses.
