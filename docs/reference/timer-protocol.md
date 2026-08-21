# Timer protocol — reference

The Timer package's wire vocabulary (`timer/vocabulary.hpp`), all v1 shapes
except `Drive` (v2), all content-id pinned. Laws:
[TIMER-01..05](../laws/timer-laws.md). The service holds role
`zengine.timer`; every consumer speaks to the role, never to an id — the
package's continuity promise is about the role.

## Consumer surface

| Shape | Fields | Meaning |
|---|---|---|
| `EnsureTimer` | `id, delay_ms, repeat, preferred, fallback` | the ORDERED form: create-or-continue with an authored preference (`preferred`) and an acceptable fallback; an unavailable preference with no acceptable fallback **refuses** — refusal is an outcome, not a menu choice |
| `EnsureRoleTimer` | + `role` | the same, firing to a role instead of the requester |
| `StartTimer` / `StartRoleTimer` | `id, delay_ms, repeat[, role]` | the RAW form: plain create; both vocabularies stay public (two different promises) |
| `CancelTimer` | `id` | cancel one |
| `CancelAllMyTimers` | — | cancel every timer the stamped sender owns |
| `TimerFired` | `id` | the delivery a due timer produces (to requester or role) |
| `TimerReady` | — | the service announcement: schedule questions may be re-asked ([TIMER-04](../laws/timer-laws.md)) |
| `TimerResolution` | `id, resolved, reason` | the receipt, sent to the stamped requester: `resolved` is one of `preserved_remaining` / `restarted_delay` / `dropped` / `refused`; `reason` is self-contained prose |

Timer identity is per-requester (`id` is the asker's name for it); the
requester crosses the wire as lossless decimal text where it must.

## Service internals on the wire

| Shape | Meaning |
|---|---|
| `Drive` v2 `{activation_sender, activation_sequence, serial}` | the beat chain: role-addressed, self-seeded, one chain per activation ([TIMER-01](../laws/timer-laws.md)); a beat carries its activation's key and a serial the service expects |
| `zen.PrepareShutdown` | (Loom shape) "describe yourself": fires nothing, cancels nothing, advances nothing — an exact clock read |
| `zen.Bequest` / `zen.ClaimBequest` | (Loom shapes) the letter's envelope, graceful path |
| `TimerHandoff{entries}` / `TimerHandoffEntry{requester, id, role, delay_ms, repeat, remaining_ms}` | the letter body: **remaining durations, never due times** ([TIMER-03](../laws/timer-laws.md)); ≤ `kMaxHandoffEntries = 32` |
| `PrepareTimerHandover{transaction, continuity}` | the prepared-replacement ask: `continuity` = `kInheritFromIncumbent` / `kStartFresh`, declared never inferred |
| `TimerCandidatePrepared{transaction}` / `TimerCandidateDeclined{transaction, reason}` | the candidate's authenticated answer (the `transaction` field is wire legibility; the bus's envelope identity is what authenticates) |

## What the Timer makes of a delay

An authored `delay_ms` is not necessarily the delay that is scheduled. The
transform is named, and it is the same one everywhere:

```text
timer.normalize_delay(delay_ms : Int, repeat : Bool) -> effective_delay : Int

    floor_zero = math.max(delay_ms, 0)
    floor_one  = math.max(floor_zero, 1)
    effective  = logic.select_int(repeat, floor_one, floor_zero)
```

| `delay_ms` | `repeat = false` | `repeat = true` |
|---|---|---|
| `-500` | `0` | `1` |
| `-1` | `0` | `1` |
| `0` | `0` | `1` |
| `1` | `1` | `1` |
| `2` | `2` | `2` |
| anything larger | unchanged | unchanged |

A repeating delay below 1 ms is a hot spin wearing a timer's clothes; a
negative delay fires on the next beat. The floor is the only thing the rule
moves — everything at or above it is returned untouched, in both modes.

**It is an operator, not an implementation detail.** `timer.normalize_delay` is
a composition over two published primitives ([`operator/`](../../operator/catalog.hpp),
authored in [`timer/normalize.hpp`](../../timer/normalize.hpp)), evaluated by one
evaluator. `StartTimer`, `StartRoleTimer`, the `EnsureTimer` / `EnsureRoleTimer`
availability comparison, and the adoption of a predecessor's letter all obtain
their number from it, so there is no second copy of the rule to disagree with —
and a tool outside the Timer can evaluate the same operator, by identity, without
compiling against the service. Since OPH-0 that reaches past the process: a
weave the host **dynamically loaded** can ask for this rule by name and spend it
synchronously, without a catalog of its own and without a message per evaluation
— see [operator-host.md](operator-host.md).

**What follows from that, and is worth knowing before reading a receipt:** an
order is matched against the standing schedule on the NORMALIZED delay, so
`EnsureTimer{delay_ms = -500, repeat = true}` against a standing 1 ms repeating
beat is the SAME schedule and answers `preserved_remaining`. That is agreement,
not a lie — but it does mean a receipt describes the schedule the Timer
understood, not the number a maker typed.

## Constants

`kBeatCapMs = 10` (the longest nap; the beat granularity) ·
`kMaxHandoffEntries = 32` · `kPreparedClaimBeats = 8` (how long a prepared
successor waits for its claimed letter before honestly starting fresh —
derived and published). One nap exists in the whole system and the service
owns it.

## Tests

Zengine suite `timer`: protocol, chains, continuity, prepared crossing,
real-clock pilot, and the delay rule — the matrix above read off a real letter,
and the witnesses that the running service and an independent reader spend one
definition rather than two that agree. Suite `operator` holds the substrate's
own contract. Their case floors are in
[`tests/test_population.txt`](../../tests/test_population.txt), which is the
contract; a count repeated here would be a second answer that nothing keeps
current.
