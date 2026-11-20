// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_VOCABULARY_HPP
#define ZENGINE_TIMER_VOCABULARY_HPP

// The messages a weave exchanges with the Timer. A weave never reads the operating system's clock
// and never sleeps: it asks for time (`StartTimer`, `EnsureTimer` and the rest), and the time
// arrives as a message (`TimerFired`) from the weave that holds the role `zengine.timer`. What the
// Timer does with each message, and what it sends back, is in its manual,
// `timer/docs/zengine.timer.md`, and the rules that span files are the Timer's laws,
// `timer/docs/timer-laws.md`.
//
// When the Timer is replaced, each thing a weave knows about a timer is one of three kinds. Its
// intent, what it wants, is the weave's to say again on its own activation. Its progress, the
// time left before the next firing, is the one thing a second ask cannot rebuild, so it is what
// the old Timer's letter carries. The binding's state, such as whether a one-shot is spent, is
// the weave's and never crosses here. Every timer ends with the Timer that holds it unless the
// letter carries it on.

// Adding a message the Timer accepts turns the next change of Timer into a replacement rather than
// a reload: a reload requires exactly the contract the old Timer accepted, so `zen.ReloadWeave`
// refuses it cleanly.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::timer {

/// Asks for `TimerFired{id}` back to the sender after `delay_ms`, once or every `delay_ms`
/// (`repeat`). `id` is the sender's own name for the timer, and another weave's ids never meet it.
/// Asking again with the same id replaces the timer, which is also how a weave changes its rhythm.
/// The delay is normalized first (`timer.normalize_delay`): a repeating delay below 1 ms becomes 1,
/// and a negative one-shot fires at the next wake. An ask that no weave sent, such as one from the
/// host, has nobody to fire at, so it is dropped and counted.
struct StartTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    ZEN_SHAPE(StartTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat));
};

/// Asks for `TimerFired{id}` to whoever holds `role` at each firing, so that a holder's successor
/// inherits the beat; while nobody holds the role, the bus refuses each firing. A role timer is
/// keyed by its role and its id, whoever asked, so a successor that asks again replaces the beat
/// rather than doubling it. A new Timer starts with no timers unless the old one's letter carries
/// them, and its `TimerReady` is what makes every weave ask again.
struct StartRoleTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role;
    ZEN_SHAPE(StartRoleTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat),
              ZEN_FIELD(role));
};

/// Cancels the sender's own timer with this id, and any role timer with this id that the sender
/// started. A role timer whose starter is gone can be cancelled by a successor that first asks for
/// it again, which makes the timer the successor's.
struct CancelTimer {
    std::string id;
    ZEN_SHAPE(CancelTimer, 1, ZEN_FIELD(id));
};

/// Cancels every timer the sender started, role timers included. It never guesses which beats the
/// sender meant to leave to a successor. A weave being replaced leaves its role timers standing,
/// so that its successor inherits them, and decides that when it hears `zen.PrepareShutdown`. A
/// weave retiring with no successor cancels them: a role timer nobody cancels keeps firing into a
/// role nobody holds, and only a current holder can take it over and cancel it.
struct CancelAllMyTimers {
    ZEN_SHAPE(CancelAllMyTimers, 1);
};

/// A timer came due. `id` is the one its asker chose. A weave that hears this matches the id
/// against its own asks and ignores the rest, because any weave may aim a beat at a role, and an
/// id the weave never asked for is not a command to it.
struct TimerFired {
    std::string id;
    ZEN_SHAPE(TimerFired, 1, ZEN_FIELD(id));
};

/// The Timer's notice that it is serving: published once per accepted activation, and never before
/// it has decided what it inherited (TIMER-04). It asks every weave to ask again for the timers it
/// needs. A weave loaded before the Timer needs it, because its first ask was refused where the
/// libraries meet and no refusal reached it; so does every weave after the Timer is reloaded or
/// replaced. A plain second ask never doubles a beat, but it starts the delay again from now; an
/// ordered ask that prefers `preserve_remaining` keeps a matching timer exactly as it was.
struct TimerReady {
    ZEN_SHAPE(TimerReady, 1);
};

// ---- an ordered ask, and its receipt -----------------------------------------
//
// An ordered ask names what the asker would like to happen to a timer already standing, the Timer
// sees which of those is possible, chooses, and answers with a receipt. These words belong to the
// Timer alone, on purpose: they are not a general way for weaves to negotiate.

// Every delay here is relative. An alarm at a time of day would be a message of its own, with its
// own clock and its own answer to replacement, and never a one-shot, whose delay pauses while the
// Timer is replaced.

/// What an asker would like to happen to a timer already standing, written as text so that any
/// weave, or a tool watching the bus, can read it. A word the Timer does not know is refused,
/// never guessed at.
inline constexpr const char* kPreserveRemaining = "preserve_remaining";
inline constexpr const char* kRestartDelay = "restart_delay";
inline constexpr const char* kDrop = "drop";

/// The four things a receipt can report. A receipt says what happened, never only that the ask
/// succeeded.
inline constexpr const char* kResolutionPreserved = "preserved_remaining";
inline constexpr const char* kResolutionRestarted = "restarted_delay";
inline constexpr const char* kResolutionDropped = "dropped";
inline constexpr const char* kResolutionRefused = "refused";

/// The three choices an asker can make, in C++. Refusal is not among them: it is what happens when
/// no choice can be honoured, not something an asker can ask for.
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
    return ""; // Unreachable; an empty word is refused, so a choice with no name is never acted on.
}

/// Reads a choice from its word as it travels. Nothing is guessed: an unknown word gives no
/// answer, and the Timer then refuses the ask rather than choosing for the asker.
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

/// What an asker would like, and what it will settle for. The default prefers keeping the time
/// left and accepts starting again, so a graceful replacement keeps every timer's time, and a
/// first load, a hard replacement and a reload start cleanly. With no fallback the preference is
/// required: when it cannot be met, the ask is refused and nothing changes.
struct ContinuityOrder {
    Continuity preferred = Continuity::PreserveRemaining;
    std::optional<Continuity> fallback = Continuity::RestartDelay;
};

/// The fallback's word as it travels; an empty word means that no fallback is acceptable.
inline std::string fallback_spelling(const ContinuityOrder& order) {
    return order.fallback ? std::string(spelling_of(*order.fallback)) : std::string();
}

/// Asks for a timer back to the sender, says what should happen to a timer already standing under
/// the id, and is answered with a `TimerResolution`. An empty `fallback` means that none is
/// acceptable, so a preference that cannot be met refuses the ask and nothing changes. It is a
/// message of its own, rather than a new version of `StartTimer`, because `StartTimer` v1 means
/// "replace, and start the delay from now", and a published message's meaning never changes.
struct EnsureTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string preferred;
    std::string fallback; ///< empty when no fallback is acceptable; then an unmet preference refuses
    ZEN_SHAPE(EnsureTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat),
              ZEN_FIELD(preferred), ZEN_FIELD(fallback));
};

/// The ordered form of `StartRoleTimer`: a role's beat, with a preference and a receipt.
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

/// What the Timer did about one ordered ask, sent to the weave the bus named as its sender; a role
/// beat's receipt goes to whoever asked for it, not to the role. `reason` is a sentence that stands
/// alone: which preference was asked, whether it could be met, and what was chosen. It is an
/// ordinary message, so a tool watching the bus sees both the ask and its answer.
struct TimerResolution {
    std::string id;
    std::string resolved; ///< one of the four kResolution* words
    std::string reason;
    ZEN_SHAPE(TimerResolution, 1, ZEN_FIELD(id), ZEN_FIELD(resolved), ZEN_FIELD(reason));
};

// ---- the letter: what a Timer passes to its successor ------------------------
//
// Loom chooses the moment of replacement and carries the envelope; what the letter says is the
// Timer's. Each timer is described as data, never copied as memory, and the successor checks every
// byte again before it trusts it. The letter does not carry callbacks, which are the weaves' own,
// nor spent or cancelled one-shots, nor any deadline.

/// One standing timer, described. It carries `remaining_ms` rather than a deadline because the
/// successor's clock counts from another moment (TIMER-03), so the time a replacement takes is
/// paused: two seconds left when the letter was written is two seconds left when it lands.
/// `requester` is the asking weave's id written in decimal, exactly, because firings and cancels
/// are matched against it.
struct TimerHandoffEntry {
    std::string requester; ///< the asking weave's id, in decimal with no sign or padding
    std::string id;
    std::string role; ///< empty for a timer that fires back to its requester
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::int64_t remaining_ms = 0; ///< time left before the next firing; 0 when due or overdue
    ZEN_SHAPE(TimerHandoffEntry, 1, ZEN_FIELD(requester), ZEN_FIELD(id), ZEN_FIELD(role),
              ZEN_FIELD(delay_ms), ZEN_FIELD(repeat), ZEN_FIELD(remaining_ms));
};

/// The whole letter: one item of Loom's bequest, one message, and a bounded list. A letter of
/// another version is not this message, so `claim_item<TimerHandoff>` returns nothing for it.
struct TimerHandoff {
    std::vector<TimerHandoffEntry> entries;
    ZEN_SHAPE(TimerHandoff, 1, ZEN_FIELD(entries));
};

/// The most timers a letter carries, one number for both sides. An old Timer writes at most this
/// many, in table order; a new Timer refuses a whole letter that holds more, which an honest old
/// Timer cannot write. So a Timer with more standing timers than this passes on the first this
/// many and no others: the weaves that asked for the rest find nothing to keep when they ask again,
/// and their fallbacks apply as after a hard replacement.
inline constexpr std::size_t kMaxHandoffEntries = 32;

/// How many asks the Timer holds while it decides what it inherited. They are applied in the order
/// they arrived once it has decided, so a fresh ask wins over a timer the letter carried. An ask
/// past this bound is counted (`deferred_dropped`), and an ordered one is refused with a receipt.
inline constexpr std::size_t kMaxDeferredOps = 32;

/// How many asks for its letter (`zen.PrepareShutdown`) the Timer holds until the host's wait sends
/// it the time to write them at. One more than this is refused, in words.
inline constexpr std::size_t kMaxLettersHeld = 4;

/// The correlation number on the one claim for a letter a Timer makes each activation. It labels
/// the conversation, is public, and is no secret: what makes an answer trustworthy is Loom's word
/// that it answers this ask (`Mail::answers_ask()`).
inline constexpr std::uint64_t kClaimCorrelation = 0x71E5;

/// How many wakes of the host's wait a new Timer spends waiting for the answer to its claim before
/// it decides it inherited nothing. It is a count of turns, never of milliseconds, taken from the
/// order of delivery: the claim goes out at activation, the steward's answer is queued in the next
/// turn, and the wait's second wake is queued behind it (timer_weave.hpp). When no steward stands,
/// the bus refuses the claim, and that refusal decides at once.
inline constexpr std::int64_t kBootstrapWakes = 2;

// ---- a prepared replacement: getting a candidate ready -----------------------
//
// A prepared candidate is made ready while the old Timer keeps serving, so it cannot be given the
// timers then: they would be out of date by the time it took over. The line is the moment Loom
// admits the candidate. The claim on the host's wait belongs to the role, the role moves, and the
// sealed old Timer writes its letter after that, through the same `zen.PrepareShutdown` and
// `TimerHandoff` that a graceful replacement uses.

/// What a candidate is asked to be ready for: `inherit` (a letter is coming, so hold every ask
/// until it lands) or `fresh` (carry nothing, and wait for nothing). It is always told, never
/// worked out from what it can see.
inline constexpr const char* kInheritFromIncumbent = "inherit";
inline constexpr const char* kStartFresh = "fresh";

/// The coordinator's request that a sealed candidate get ready. `transaction` is echoed back for
/// anyone reading the bus and carries no authority: Loom's word that an answer answers this ask is
/// what the coordinator trusts.
struct PrepareTimerHandover {
    std::int64_t transaction = 0;
    std::string continuity; ///< kInheritFromIncumbent or kStartFresh
    ZEN_SHAPE(PrepareTimerHandover, 1, ZEN_FIELD(transaction), ZEN_FIELD(continuity));
};

/// The candidate's answer that every step that could fail is done: it is loaded and checked, space
/// for a whole letter is reserved, and it knows how it starts and where its letter comes from. It
/// does not mean the timers are restored, which cannot happen before the candidate takes over; what
/// is left is bounded, always the same, and cannot fail for want of space.
struct TimerCandidatePrepared {
    std::int64_t transaction = 0;
    ZEN_SHAPE(TimerCandidatePrepared, 1, ZEN_FIELD(transaction));
};

/// The candidate's answer that it will not become the Timer, and why. It is a real outcome: the
/// candidate is discarded, and the old Timer, which is never told, keeps serving.
struct TimerCandidateDeclined {
    std::int64_t transaction = 0;
    std::string reason;
    ZEN_SHAPE(TimerCandidateDeclined, 1, ZEN_FIELD(transaction), ZEN_FIELD(reason));
};

/// How many wakes of the host's wait a prepared Timer spends waiting for the letter it was promised
/// before it starts fresh. It must be more than the coordinator's round trip after admission, which
/// is a turn each for the old Timer's ask for its letter, the letter to the coordinator, and the
/// claim's answer. It must also be bounded, or a coordinator that is alive and silent would leave
/// an admitted Timer holding every ask forever; a coordinator that is gone shows as the bus's
/// refusal of the claim. Eight is a margin to spare.
inline constexpr std::int64_t kPreparedClaimWakes = 8;

/// The role the Timer holds: the address of whoever provides time, which outlives any one Timer
/// that is swapped in.
inline constexpr const char* kTimerRole = "zengine.timer";

} // namespace zengine::timer

#endif // ZENGINE_TIMER_VOCABULARY_HPP
