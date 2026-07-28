#ifndef ZENGINE_TIMER_VOCABULARY_HPP
#define ZENGINE_TIMER_VOCABULARY_HPP

// The Timer package's message vocabulary — time, message-shaped.
//
// The rule this package installs: games and packages do not read the OS clock
// and do not sleep. A weave that wants time ASKS for it (StartTimer /
// StartRoleTimer) and time ARRIVES like everything else does — as a message
// (TimerFired), delivered by the TimerService weave holding `zengine.timer`.
// The service is the one place in the running system that owns a monotonic
// clock and the one nap; everyone else just hears beats. There is no polling
// API in V1: time is a stream of events, not a state to be asked about.
//
// THE V1 CONTRACT (Vision's phase prompt) is four shapes: StartTimer v1,
// CancelTimer v1, CancelAllMyTimers v1, TimerFired v1 — spelled here exactly
// as locked; the timer suite pins the spellings by content-id.
//
// NAMED ADDITIONS (the contract proved insufficient here; recorded face-up
// per the report-back rule, never silently):
//   - StartRoleTimer v1 — the prompt's own "or to a role if the design makes
//     that cleaner" option, made a distinct shape so StartTimer stays exactly
//     as specified. A requester-addressed timer dies with its requester — and
//     a weave cannot observe another weave's death (the bus shows a sender no
//     outcomes, and no unload broadcast exists), so a heartbeat that must
//     SURVIVE its starter being swapped (the input pump, the skin pump) is
//     addressed to the ROLE: whoever holds the slot at each firing hears the
//     beat. That is also what heals a freshly swapped-in skin on a dead-quiet
//     bus: the beat belongs to the role, so the successor inherits it. (What
//     a standing timer does NOT survive is the TIMER SERVICE itself being
//     replaced — the table lives in the service. See Drive.)
//   - TimerReady v1 — the service's availability notice, published once per
//     ACCEPTED ACTIVATION. Since R2A-2 it is no longer the system's only
//     first breath (every weave gets its own `zen.Activated`); its remaining
//     job is the opposite load order and the service's own succession. See
//     TimerReady below.
//   - Drive v2 — the beat itself, and since R2A-2 a claim of ownership. A
//     weave runs only when a message arrives, so the service keeps itself
//     alive by re-sending Drive to its role at the end of every beat; inside
//     the beat it naps to the next deadline (the one sleep in the system) and
//     fires what came due. **The host does not wind the clock.** The service
//     seeds its own chain when the Loom's control door activates it, and each
//     valid beat seeds exactly its one successor. A Drive now carries the
//     activation it belongs to and its serial, so a stale, duplicated,
//     replayed, inherited or foreign one establishes nothing. See Drive below.

// CROSSING INTO THIS CONTRACT IS REPLACEMENT, NOT RELOAD — recorded so nobody
// reads the refusal as a regression. R2A-2 changed accepted-message contracts:
// the Timer now accepts `zen.Activated` and `Drive v2`, and the official
// consumers accept `zen.Activated`. The Loom enforces EXACT accepted-contract
// equality on reload-in-place (R2A-1), so `zen.ReloadWeave` from a pre-R2A-2
// artifact to one of these refuses cleanly with "accepted schema contract
// mismatch; reload refused". That is the substrate telling the truth: a weave
// whose doors changed is a REPLACEMENT (`zen.SwapWeave`), not a reload. Within
// this contract, reloading in place works and stays live — probe B pins exactly
// that.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::timer {

/// Ask for time, delivered back to YOU (the sender): TimerFired{id} arrives
/// at the requesting weave after delay_ms, once (repeat=false) or every
/// delay_ms (repeat=true). `id` is the caller's own name for the timer
/// ("snake.tick", "my.cooldown"); it is scoped to the requester, so two
/// weaves using the same id never collide. Asking again with the same id
/// REPLACES the schedule (an upsert — which is also how a cadence changes).
/// A repeating delay below 1ms is clamped to 1ms (a 0ms repeat would be a
/// hot spin wearing a timer's clothes); a negative one-shot delay fires on
/// the next beat. Sent by a root (no weave identity), there is no one to
/// deliver to: dropped, counted on the service's `dropped` poke counter.
struct StartTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    ZEN_SHAPE(StartTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat));
};

/// Ask for time, delivered to a ROLE: TimerFired{id} goes to whoever holds
/// `role` at each firing. The beat outlives any particular holder of THAT
/// role — a swap's successor inherits it without asking — and an unheld role
/// refuses the delivery cleanly (the beat waits for the next holder; it is
/// the slot's pulse, not a weave's). It also outlives its own starter: the
/// entry stays, and only cancel rights are stranded (see CancelTimer).
/// Upsert key is (role, id), ACROSS requesters: a successor re-asking
/// replaces its predecessor's schedule instead of doubling the beat. Same
/// clamps as StartTimer.
///
/// The one succession it does NOT survive is the TimerService's own: the
/// standing-timer table is the service instance's private state, not
/// gate-carried state, so a replaced or reloaded service starts with an
/// empty table. Since R2A-2 that heals the same way for BOTH reload and
/// swap: the new incarnation is activated, publishes TimerReady, and
/// standing consumers refill the table by re-asking (see Drive).
struct StartRoleTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role;
    ZEN_SHAPE(StartRoleTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat),
              ZEN_FIELD(role));
};

/// Cancel by id: removes the sender's own (requester, id) timer, and any
/// role timer with this id that the sender itself started. V1 edge, honest:
/// a role timer whose starter is gone is cancellable only by a successor
/// first re-asking (upsert takes ownership) and then cancelling.
struct CancelTimer {
    std::string id;
    ZEN_SHAPE(CancelTimer, 1, ZEN_FIELD(id));
};

/// The convenience for a weave that is going away politely: every timer the
/// sender started — requester-addressed and role-addressed alike — dies.
///
/// TOTAL AND NEUTRAL, CHOSEN (decided 2026-07-27, and pinned as chosen in the
/// suite so the choice cannot erode into an accident). It would be easy to
/// make this shape spare role timers on the theory that a role beat was
/// "meant" to outlive its starter. It deliberately does not: succession here
/// is AUTHORED, never system-guessed, and a shape that guessed would be
/// deciding, on the weave's behalf, which of its beats were bequests. The
/// mechanism stays a plain "everything I started"; the policy lives with the
/// author, taught here:
///
///   - being REPLACED? Leave your role beats standing. They are addressed to
///     the slot, not to you, and your successor inherits them without asking
///     — cancelling them would make the swap a gap in the pulse for no
///     reason. `zen.PrepareShutdown` is exactly the signal that a replacement
///     is coming, so it is the right place to make this decision knowingly.
///   - RETIRING with no heir? Cancel. Nothing is coming to claim the beat.
///
/// The honest consequence of the retiring-weave case done wrong: an unclaimed
/// role beat is a LEAKED TIMER. It keeps firing into an unheld role, each
/// delivery a clean refusal (the same bounded floor a dead requester's timer
/// rides, with the consumer obligation covering anyone who does hold the role
/// later). Sad, benign — and NOT collectable, which is the part worth being
/// clear-eyed about: a role beat is never provably garbage, because being
/// reachable by a future holder is the whole point of it. Only a current
/// holder can declare one unwanted, by re-asking to take ownership and then
/// cancelling.
///
/// Forward: when the steward speaks about shutdown (the lifecycle session,
/// R2), its notice must distinguish REPLACEMENT from RETIREMENT — that is
/// what turns this convention from a taught default into a mechanically
/// informed one.
struct CancelAllMyTimers {
    ZEN_SHAPE(CancelAllMyTimers, 1);
};

/// A timer came due. `id` is the one the requester chose; the consumer
/// obligation applies — match it against YOUR OWN asks and ignore the rest
/// (a role can be aimed at by anyone; an unknown id is data, not a command).
struct TimerFired {
    std::string id;
    ZEN_SHAPE(TimerFired, 1, ZEN_FIELD(id));
};

/// The service's availability notice, published once per ACCEPTED ACTIVATION:
///
///   "The Timer service has accepted an activation and is available;
///    re-establish the timers you require."
///
/// It is no longer the only first breath available to a consumer, and that
/// changes what it is FOR. Since R2A-2 a consumer arranges its own time on its
/// OWN `zen.Activated`, so this shape's remaining job is the opposite load
/// order and the service's own succession:
///   - consumer loaded AFTER the Timer — its own activation makes it ask; it
///     needs nothing from here;
///   - consumer loaded BEFORE the Timer — its activation-time ask went nowhere,
///     and this is what tells it to try again. WHERE it went is worth being
///     exact about: a loaded weave's send crosses the library seam as bytes and
///     the host resolves the claimed schema against the bus registry BEFORE
///     routing, so with no service present nobody accepts StartTimer /
///     StartRoleTimer, the shape is unregistered, and the send is rejected AT
///     THE SEAM — earlier than role resolution, with no envelope and no refusal
///     event. The asker cannot tell, which is exactly why this notice exists;
///   - Timer reloaded or swapped — the new incarnation's private schedule table
///     is empty, and this is what gets standing consumers to refill it.
///
/// RE-ASKING IS CARDINALITY-IDEMPOTENT, NOT TIMING-NEUTRAL, and the difference
/// is worth the extra words. The upsert keys guarantee a re-ask never produces
/// a second entry or a doubled beat. They do NOT make it free: a re-ask
/// REPLACES the schedule and RE-ANCHORS it, so the next firing is a full delay
/// from now rather than from the original ask, and a timer reconciled mid-cycle
/// loses the remainder of that cycle. That is the right trade for a service
/// that may just have come back with an empty table — and it is a real cost,
/// not a no-op.
///
/// Published on the ACTIVATION, not on the first beat — a consumer should not
/// have to wait a nap to learn the service exists. A duplicate or non-newer
/// activation republishes nothing.
///
/// HISTORICAL: before R2A-2 this was published on the service's first beat and
/// was the system's only first breath, which made a weave loaded after the wind
/// permanently deaf (measured: probe D — a late `snake-clock` whose tick count
/// stayed 0 forever). That is fixed at the source now: the latecomer gets its
/// own activation.
struct TimerReady {
    ZEN_SHAPE(TimerReady, 1);
};

/// The service's own beat — and, since R2A-2, a CLAIM OF OWNERSHIP rather than
/// a bare nudge. A beat now carries the activation it belongs to and its place
/// in that chain, so the service can tell its own next breath from everything
/// else that might arrive wearing the same shape.
///
/// THE LAW THIS ENFORCES:
///
///   Every successfully activated Timer incarnation establishes exactly ONE
///   beat chain. A new activation owns a new chain; stale, duplicate,
///   replayed, inherited, or foreign Drives cannot establish another.
///
/// A Drive is acted on only when ALL of these hold — anything else is ignored
/// completely (no nap, no firing, no beat count, no re-wind):
///   - the service is activated at all (a fresh incarnation is not, which is
///     what makes a predecessor's queued Drive inert even if it arrives first);
///   - its bus-stamped sender is the service's own chain sender;
///   - `activation_sender` + `activation_sequence` name the activation the
///     service is currently living under;
///   - `serial` is exactly the one expected next.
///
/// `activation_sender` is TEXT, and deliberately: a `WeaveId` is unsigned
/// 64-bit while the wire's `Int` is signed, so an Int field would silently
/// narrow the top half of the range. Canonical decimal Text is lossless and is
/// already the house spelling for a WeaveId on the wire — the kernel's control
/// door answers a load with `zen.Result{std::to_string(id.value)}` and the
/// Weave Manager parses it back.
///
/// It stays ROLE-addressed (a loaded weave cannot address itself — see the
/// service header), but role addressing is no longer what establishes
/// ownership. The activation key, the serial, and the stamped sender are.
///
/// v2: the three fields joined the shape. `Drive v1` was empty — it carried no
/// question, no answer and no authority, which was elegant and was also exactly
/// the problem: an empty beat is indistinguishable from any other empty beat,
/// so a second one seeded a permanent second chain and a predecessor's parked
/// beat could drive a successor. The version bump is the immutable-published-
/// schema rule paid honestly: `(Drive, 1)` meant "an anonymous nudge" and still
/// does, forever.
///
/// HISTORICAL, and kept because the audit record depends on it: before R2A-2
/// the HOST sent the first Drive (the wind) and liveness was accidental —
/// `zen.ReloadWeave` happened to preserve the chain because the WeaveId
/// survived, `zen.SwapWeave` killed it because the parked beat's sender was
/// gone (CapabilityDenied at delivery, sender-death rather than role vacancy),
/// a stray second Drive seeded a permanent conserved second chain, and a
/// consumer loaded after the hello was permanently deaf. All four were measured
/// (the trust-gate audit of 2026-07-26; probes A–D). R2A-2 replaced the
/// mechanism rather than patching it: **there is no host wind**, and the
/// substrate behaviour those probes measured has not changed — the old parked
/// beat is still refused CapabilityDenied on a swap. What changed is that the
/// successor is ACTIVATED, and authors a new chain of its own.
struct Drive {
    std::string activation_sender;      ///< canonical decimal of the activating sender's WeaveId
    std::int64_t activation_sequence = 0;
    std::int64_t serial = 0;
    ZEN_SHAPE(Drive, 2, ZEN_FIELD(activation_sender), ZEN_FIELD(activation_sequence),
              ZEN_FIELD(serial));
};

/// The role slot the TimerService holds: the address "whoever provides
/// time", which outlives any particular implementation being swapped in.
inline constexpr const char* kTimerRole = "zengine.timer";

/// The beat cap: the longest the service will nap when nothing is due
/// sooner, which is also the worst-case lateness of a firing and the arrival
/// bound on a StartTimer being considered. 10ms — the responsiveness the old
/// host loop's nap gave the whole system, now owned by the one weave allowed
/// to sleep.
inline constexpr std::int64_t kBeatCapMs = 10;

} // namespace zengine::timer

#endif // ZENGINE_TIMER_VOCABULARY_HPP
