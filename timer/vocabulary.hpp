// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_VOCABULARY_HPP
#define ZENGINE_TIMER_VOCABULARY_HPP

// The Timer package's message vocabulary: time, message-shaped. Games and packages neither read
// the OS clock nor sleep; a weave asks for time (`StartTimer`, `EnsureTimer`, ...) and it arrives
// as a message (`TimerFired`) from the service holding `zengine.timer`, the one place with a
// monotonic clock and a nap. Laws: docs/laws/timer-laws.md (TIMER-01..05); every shape and
// field: docs/reference/timer-protocol.md; succession: docs/reference/timer-continuity.md.
//
// A continuity decision is about one kind of state: intent (what a consumer wants, re-declared
// on its own activation), progress (the remaining time to the next firing -- the one thing a
// re-ask cannot rebuild, and what the handoff letter carries), or a binding's lifecycle (the
// consumer's, never carried here). Death is universal; inheritance is authored.

// Adding an accepted shape makes the service's crossing a replacement, not a reload: a reload
// requires exactly the accepted contract, so `zen.ReloadWeave` refuses it cleanly.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::timer {

/// Ask for time, delivered back to you: `TimerFired{id}` after `delay_ms`, once or every
/// `delay_ms` (`repeat`). `id` is your own name for it, scoped to you; asking again with the same
/// id replaces the schedule (an upsert, which is also how a cadence changes). Delays are
/// normalized (`timer.normalize_delay`): a repeat below 1 ms becomes 1, a negative one-shot fires
/// on the next beat. Sent by a root (no weave), there is no one to deliver to: dropped, counted.
struct StartTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    ZEN_SHAPE(StartTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat));
};

/// Ask for time delivered to a role: `TimerFired{id}` goes to whoever holds `role` at each
/// firing, so a swap's successor inherits the beat, and an unheld role refuses a delivery until it
/// is held. The upsert key is (role, id) across requesters: a successor re-asking replaces the
/// beat rather than doubling it. The standing table crosses a Timer replacement only through the
/// letter; otherwise a new service starts empty, and `TimerReady` gets consumers to re-ask.
struct StartRoleTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role;
    ZEN_SHAPE(StartRoleTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat),
              ZEN_FIELD(role));
};

/// Cancel by id: the sender's own (requester, id) timer, and any role timer with this id the
/// sender started. A role timer whose starter is gone is cancelled by a successor that first
/// re-asks (taking ownership).
struct CancelTimer {
    std::string id;
    ZEN_SHAPE(CancelTimer, 1, ZEN_FIELD(id));
};

/// Cancel every timer the sender started, role-addressed ones included: total and neutral, never
/// guessing which beats were meant as bequests. Being replaced, leave your role beats standing
/// (your successor inherits them; `zen.PrepareShutdown` is where to decide); retiring with no
/// heir, cancel them -- an unclaimed role beat keeps firing into an unheld role, and only a
/// current holder can take ownership and cancel it.
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

/// The service's availability notice, published once per accepted activation and never before
/// its continuity decision (TIMER-04): "re-establish the timers you require." A consumer loaded
/// before the Timer needs it -- its activation-time ask was rejected at the library seam, with no
/// refusal it could see -- and so does every consumer after the service is reloaded or replaced.
/// A raw re-ask never doubles a beat, but it re-anchors the schedule (a full delay from now); an
/// ordered re-ask preferring `preserve_remaining` keeps a matching schedule exactly as it was.
struct TimerReady {
    ZEN_SHAPE(TimerReady, 1);
};

/// The service's own beat: role-addressed (a loaded weave cannot address itself), and a claim of
/// ownership rather than a nudge (TIMER-01). It is acted on only when the service is activated,
/// its stamped sender is the chain's own, `activation_sender` and `activation_sequence` name the
/// current activation, and `serial` is the one expected next; anything else is ignored whole.
/// `activation_sender` is canonical decimal text, since a WeaveId is unsigned 64-bit and the
/// wire's Int is signed.
struct Drive {
    std::string activation_sender;      ///< canonical decimal of the activating sender's WeaveId
    std::int64_t activation_sequence = 0;
    std::int64_t serial = 0;
    ZEN_SHAPE(Drive, 2, ZEN_FIELD(activation_sender), ZEN_FIELD(activation_sequence),
              ZEN_FIELD(serial));
};

// ---- continuity: the order, and the receipt ---------------------------------
//
// request -> available menu -> resolved choice -> receipt: four words the Timer package
// understands about its own state, package-local on purpose and not a negotiation framework.

// Every shape here is relative: an absolute wall-clock alarm is a distinct future kind with its
// own clock and continuity answer, never a relative one-shot, which pauses across replacement.

/// What a requester would like to happen to an existing schedule, spelled as Text so a stranger
/// or a tap can read it. An unknown spelling is refused, never guessed.
inline constexpr const char* kPreserveRemaining = "preserve_remaining";
inline constexpr const char* kRestartDelay = "restart_delay";
inline constexpr const char* kDrop = "drop";

/// The four things that can actually have happened. A receipt states the
/// RESULT, never merely success.
inline constexpr const char* kResolutionPreserved = "preserved_remaining";
inline constexpr const char* kResolutionRestarted = "restarted_delay";
inline constexpr const char* kResolutionDropped = "dropped";
inline constexpr const char* kResolutionRefused = "refused";

/// The menu, as C++ says it. Three CHOICES — refusal is deliberately not among
/// them, because refusal is what HAPPENS when a choice cannot be honoured, not
/// something a caller can ask for.
enum class Continuity { PreserveRemaining, RestartDelay, Drop };

inline const char* spelling_of(Continuity c) {
    switch (c) {
    case Continuity::PreserveRemaining:
        return kPreserveRemaining;
    case Continuity::RestartDelay:
        return kRestartDelay;
    case Continuity::Drop:
        return kDrop;
    }
    return ""; // unreachable; an unspellable choice must not become a silent one
}

/// Read a spelling off the wire. Nothing is guessed: an unknown word is an
/// empty answer, and the service refuses the order rather than picking for the
/// caller.
inline std::optional<Continuity> continuity_from(std::string_view text) {
    if (text == kPreserveRemaining) {
        return Continuity::PreserveRemaining;
    }
    if (text == kRestartDelay) {
        return Continuity::RestartDelay;
    }
    if (text == kDrop) {
        return Continuity::Drop;
    }
    return std::nullopt;
}

/// What a requester would like, and what it will settle for. The default -- prefer preserving
/// the remaining time, accept restarting -- gives a graceful replacement real continuity and
/// lets a first load, a hard replacement and a reload start cleanly. No fallback makes the
/// preference required: unavailable, the order is refused and nothing changes.
struct ContinuityOrder {
    Continuity preferred = Continuity::PreserveRemaining;
    std::optional<Continuity> fallback = Continuity::RestartDelay;
};

/// The fallback as it travels: empty Text means "none acceptable".
inline std::string fallback_spelling(const ContinuityOrder& order) {
    return order.fallback ? std::string(spelling_of(*order.fallback)) : std::string();
}

/// Ask for time with a preference about an existing schedule, delivered back to you and answered
/// with a `TimerResolution`. An empty `fallback` means none is acceptable: an unavailable
/// preference refuses the order and nothing changes. Refusal is an outcome, never a choice. A
/// shape of its own because `StartTimer` v1 means "upsert, re-anchored from now", and a frozen
/// shape's meaning never changes; raw and ordered are two public promises.
struct EnsureTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string preferred;
    std::string fallback; ///< empty = none acceptable; an unavailable preference refuses
    ZEN_SHAPE(EnsureTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat),
              ZEN_FIELD(preferred), ZEN_FIELD(fallback));
};

/// The role-addressed twin: the ordered form of StartRoleTimer.
struct EnsureRoleTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role;
    std::string preferred;
    std::string fallback;
    ZEN_SHAPE(EnsureRoleTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat),
              ZEN_FIELD(role), ZEN_FIELD(preferred), ZEN_FIELD(fallback));
};

/// What the Timer did about one order, sent to the stamped requester (a role beat's receipt
/// belongs to whoever ordered it). `reason` is self-contained prose: which preference, whether
/// it was available, which choice. An ordinary message, so a tap sees both halves of an order.
struct TimerResolution {
    std::string id;
    std::string resolved; ///< one of the four kResolution* spellings
    std::string reason;
    ZEN_SHAPE(TimerResolution, 1, ZEN_FIELD(id), ZEN_FIELD(resolved), ZEN_FIELD(reason));
};

// ---- the letter: what the Timer offers its successor ------------------------
//
// The Loom supplies the replacement moment and carries the envelope; the contents are this
// package's. Entries are described, never serialized, and the successor re-admits every byte
// through the gate. Not carried: callbacks (the consumer's), spent or cancelled one-shots, and
// absolute due times.

/// One active schedule, described. `remaining_ms` rather than a deadline, because the
/// successor's clock is another epoch (TIMER-03) -- so replacement downtime is paused: two
/// seconds left when the letter was written is two seconds left when restored, and no absolute
/// deadline is kept. `requester` is canonical decimal text, lossless: firings and cancels match it.
struct TimerHandoffEntry {
    std::string requester; ///< canonical decimal of the requesting weave's id
    std::string id;
    std::string role; ///< empty = requester-addressed
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::int64_t remaining_ms = 0; ///< to the next firing; 0 = due or overdue
    ZEN_SHAPE(TimerHandoffEntry, 1, ZEN_FIELD(requester), ZEN_FIELD(id), ZEN_FIELD(role),
              ZEN_FIELD(delay_ms), ZEN_FIELD(repeat), ZEN_FIELD(remaining_ms));
};

/// The whole letter: one bequest item, one shape, a bounded list. A letter of another version is
/// not this shape, and `claim_item<TimerHandoff>` returns a clean nothing.
struct TimerHandoff {
    std::vector<TimerHandoffEntry> entries;
    ZEN_SHAPE(TimerHandoff, 1, ZEN_FIELD(entries));
};

/// The letter's bound, one number for both sides: a predecessor writes at most this many entries
/// in table order; a successor refuses a letter claiming more, whole (an honest predecessor
/// cannot write one). So a service standing more timers than this offers continuity to the first
/// this many and none to the rest: their consumers' ordered re-asks find nothing to preserve and
/// fall back as after a hard replacement.
inline constexpr std::size_t kMaxHandoffEntries = 32;

/// How many schedule operations the service holds while deciding what it inherited: replayed in
/// arrival order after restoration, so a fresh request beats inherited state. Overflow is counted
/// (`deferred_dropped`), and an ordered request is refused with a receipt.
inline constexpr std::size_t kMaxDeferredOps = 32;

/// The correlation on the service's one claim per activation: a conversation label, public and
/// no secret. What makes an answer trustworthy is Loom's attestation (`Mail::answers_ask()`).
inline constexpr std::uint64_t kClaimCorrelation = 0x71E5;

/// The bootstrap: how many of its own beats a fresh incarnation waits for its claim's answer
/// before deciding it inherited nothing. A count of queue turns, never milliseconds, derived from
/// the dispatch order (timer_weave.hpp traces it).
inline constexpr std::int64_t kBootstrapBeats = 2;

// ---- prepared replacement: the preparation conversation ---------------------
//
// A prepared candidate is readied while the incumbent keeps serving, so it cannot be handed a
// schedule then: it would be stale by admission. The boundary is the admission itself -- the
// beat chain rides the role, the role moves, and the sealed incumbent writes the letter
// afterwards, through the same `zen.PrepareShutdown` -> `TimerHandoff` the graceful path uses.

/// What a candidate is asked to be ready for: `inherit` (a letter is coming; hold everything
/// until it lands) or `fresh` (carry nothing, and do not wait). Declared, never inferred.
inline constexpr const char* kInheritFromIncumbent = "inherit";
inline constexpr const char* kStartFresh = "fresh";

/// The coordinator's preparation ask to a sealed candidate. `transaction` is echoed for a reader
/// of the wire and is not authority: Loom's attestation that the answer answers this ask is.
struct PrepareTimerHandover {
    std::int64_t transaction = 0;
    std::string continuity; ///< kInheritFromIncumbent | kStartFresh
    ZEN_SHAPE(PrepareTimerHandover, 1, ZEN_FIELD(transaction), ZEN_FIELD(continuity));
};

/// "Every fallible step is done": loaded, validated, capacity for a full letter reserved, the
/// startup mode and the letter's source known. Not "the schedule is restored" -- it cannot be
/// before the boundary; what is left is bounded, deterministic, and cannot fail for want of room.
struct TimerCandidatePrepared {
    std::int64_t transaction = 0;
    ZEN_SHAPE(TimerCandidatePrepared, 1, ZEN_FIELD(transaction));
};

/// "I will not become the Timer, and here is why": a real outcome. The candidate is discarded,
/// and the incumbent, never told, keeps serving.
struct TimerCandidateDeclined {
    std::int64_t transaction = 0;
    std::string reason;
    ZEN_SHAPE(TimerCandidateDeclined, 1, ZEN_FIELD(transaction), ZEN_FIELD(reason));
};

/// How many beats a prepared incarnation waits for the promised letter before starting fresh. It
/// must exceed the coordinator's round trip after admission -- the letter lands after beat 2
/// (activation; claim and beat 0; the incumbent's letter ask; beat 1; the letter to the preparer;
/// beat 2; the claim's answer) -- and must be bounded, or a dead preparer would leave an admitted
/// Timer holding every operation forever. Eight is headroom; a count of queue turns.
inline constexpr std::int64_t kPreparedClaimBeats = 8;

/// The role slot the TimerService holds: the address "whoever provides
/// time", which outlives any particular implementation being swapped in.
inline constexpr const char* kTimerRole = "zengine.timer";

/// The beat cap: the longest the service naps when nothing is due sooner, and so the worst-case
/// lateness of a firing. The one nap in the system is the service's.
inline constexpr std::int64_t kBeatCapMs = 10;

} // namespace zengine::timer

#endif // ZENGINE_TIMER_VOCABULARY_HPP
