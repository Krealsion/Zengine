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

## Constants

`kBeatCapMs = 10` (the longest nap; the beat granularity) ·
`kMaxHandoffEntries = 32` · `kPreparedClaimBeats = 8` (how long a prepared
successor waits for its claimed letter before honestly starting fresh —
derived and published). One nap exists in the whole system and the service
owns it.

## Tests

Zengine suite `timer` (78 cases): protocol, chains, continuity, prepared
crossing, real-clock pilot.
