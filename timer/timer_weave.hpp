// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_TIMER_WEAVE_HPP
#define ZENGINE_TIMER_TIMER_WEAVE_HPP

// The TimerService weave, over an injected Clock: anything with `std::int64_t now_ms()`
// (monotonic) and `void nap_ms(std::int64_t)` (block; <=0 lets the clock decline). timer.cpp's is
// the OS clock and the system's one nap; the suite's is virtual. The wire is
// docs/reference/timer-protocol.md and succession docs/reference/timer-continuity.md; what
// follows here is what those pages do not carry: the ordering this code depends on, its traps.
// Timer law: docs/laws/timer-laws.md

// The beat chain is authored from the activation (TIMER-01): accept it, decide what was
// inherited, publish TimerReady, seed Drive 0; each valid Drive naps to the soonest deadline (at
// most kBeatCapMs), fires what is due and seeds its one successor. A consumer's reply to a firing
// is queued behind the next Drive, so it waits out up to one nap. The activation cursor and the
// serial are plain members, never TimerState: a new incarnation begins unactivated, and a
// predecessor's queued Drive is inert.

// When the service goes: unloaded or swapped, its parked Drive fails on its dead sender
// (CapabilityDenied); reloaded, the same WeaveId's parked Drive is inert, and the new instance
// keeps TimerState's counters but not the table -- so it republishes TimerReady. For continuity
// a reload is a fresh service: no PrepareShutdown, no letter.

// Cleanup is told truthfully, not seen: a dead requester's timer fires into a clean NoSuchTarget
// (weave ids are never reused), a repeating one until cancelled or the service is replaced;
// role-addressed beats outlive their holders by construction; polite weaves cancel on their way
// out (`CancelAllMyTimers`).

#include "normalize.hpp"
#include "vocabulary.hpp"

#include "activation/activation.hpp"
#include "operator/catalog.hpp"

#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::timer {

/// May a chain advance from `serial`? At the last representable serial the chain stops rather
/// than wrap: a duplicated serial would be indistinguishable from a replay and fork time. Pinned
/// directly; the path through it is true by construction and unreachable by a test (2^63 beats).
inline constexpr bool can_advance_serial(std::int64_t serial) {
    return serial >= 0 && serial < std::numeric_limits<std::int64_t>::max();
}

/// Six honest counters, poke-inspectable: beats, firings, timers standing, asks dropped for
/// having nobody to answer, operations dropped from a full bootstrap hold, and entries restored
/// from a predecessor's letter at the last bootstrap (`inherited`: 0 means it started fresh).
struct TimerState {
    std::int64_t beats = 0;
    std::int64_t fired = 0;
    std::int64_t active = 0;
    std::int64_t dropped = 0;
    std::int64_t deferred_dropped = 0;
    std::int64_t inherited = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(TimerState, 2, ZEN_FIELD(beats), ZEN_FIELD(fired), ZEN_FIELD(active),
              ZEN_FIELD(dropped), ZEN_FIELD(deferred_dropped), ZEN_FIELD(inherited));
};

template <class Clock>
class TimerServiceT
    : public loom::WeaveBase<
          TimerServiceT<Clock>, TimerState,
          loom::Accept<loom::Activated, Drive, StartTimer, StartRoleTimer, EnsureTimer,
                       EnsureRoleTimer, CancelTimer, CancelAllMyTimers, loom::PrepareShutdown,
                       loom::Bequest, loom::Refused, PrepareTimerHandover>,
          loom::Emit<TimerFired, TimerReady, Drive, TimerResolution, loom::Bequest,
                     loom::ClaimBequest, TimerCandidatePrepared, TimerCandidateDeclined>> {
public:
    TimerServiceT() = default;
    explicit TimerServiceT(Clock clock) : clock_(std::move(clock)) {}

    /// With another catalog carrying the same identity: the Timer consumes the delay rule and
    /// does not own it, so the suite can replace a primitive beneath it and watch execution and an
    /// independent reader move together.
    TimerServiceT(Clock clock, op::Catalog operators)
        : clock_(std::move(clock)), semantics_(std::move(operators)) {}

    /// ...or with the authority it resolves the rule through: a loaded artifact builds one from
    /// what its host offered (`DelayAuthority`). The only door to a host-backed Timer; no setter.
    TimerServiceT(Clock clock, DelayAuthority semantics)
        : clock_(std::move(clock)), semantics_(std::move(semantics)) {}

    /// The rule this Timer runs, for anyone who wants to evaluate it without
    /// scheduling anything. LOCAL-FALLBACK only: a host-backed Timer carries no
    /// catalog at all, and this throws rather than inventing one.
    const op::Catalog& operators() const { return semantics_.operators(); }

    /// Whether this Timer resolves through a host's catalog or its own. A
    /// diagnostic and nothing more — what a Timer SCHEDULES is where its
    /// authority is proven, and this answer changes nothing about it.
    bool host_backed() const noexcept { return semantics_.host_backed(); }

    /// The Loom's control door says this incarnation is committed and live.
    /// That is the whole of what it says — so this is where the service decides
    /// what to DO about it, which is: find out what it inherited, and only then
    /// become available (TIMER-04).
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            // Unattested, forged, duplicate or replayed: no notice, no chain,
            // nothing. A first breath is Loom's to grant, not a shape's to claim.
            return;
        }
        // A new activation owns a new chain, from serial 0.
        expected_serial_ = 0;
        // Tell the truth about the table rather than assuming: a fresh
        // incarnation's is empty, but activation is not state migration and has
        // no business clearing one that legitimately holds entries.
        this->state_.active = static_cast<std::int64_t>(entries_.size());
        // No TimerReady until the decision (TIMER-04): a consumer re-asking before restoration
        // would re-anchor the very schedule the letter carried.
        bootstrap_ = Bootstrap::Awaiting;
        bootstrap_beats_ = 0;
        this->state_.inherited = 0;
        // WHERE THIS INCARNATION LOOKS FOR ITS LETTER, decided by what it was
        // TOLD before it was admitted and never by what it can see. Anything
        // already held stays held in every mode: an operation that arrived
        // before the activation was still sent after the fork point, and it
        // deserves to land after the inheritance rather than under it.
        switch (startup_) {
        case Startup::PreparedRestoration:
            // A prepared candidate knows exactly whose letter it is waiting for
            // — it read the preparer off its ask's bus-stamped sender — so it
            // asks that weave by id rather than asking a role who is standing
            // there. The answer is still trusted only on Loom's attestation.
            claim_open_ = true;
            claim_budget_ = kPreparedClaimBeats;
            mail.send(preparer_, loom::ClaimBequest{kTimerRole}, kClaimCorrelation);
            seed_chain(mail);
            return;
        case Startup::Fresh:
            // Told plainly that nothing is being carried. No claim goes out at
            // all, so there is no answer to wait for and nothing a late letter
            // could reopen: decide now, replay what was held, and announce.
            claim_open_ = false;
            claim_budget_ = 0;
            resolve_bootstrap(mail, /*letter=*/nullptr);
            seed_chain(mail);
            return;
        case Startup::GracefulClaim:
            // Two beats, derived from dispatch order, never tuned: this handler enqueues the claim
            // and Drive 0, and the steward's answer is enqueued when it hears the claim -- after
            // Drive 0, before Drive 1. One beat would decide fresh with the letter in flight; three
            // would spend a turn that carries no news. With no steward at all, beat 1 arrives
            // unanswered and the service starts fresh.
            claim_open_ = true;
            claim_budget_ = kBootstrapBeats;
            mail.send_to_role(loom::kManagerRole, loom::ClaimBequest{kTimerRole},
                              kClaimCorrelation);
            seed_chain(mail);
            return;
        }
    }

    /// "Be ready to become the Timer", through a sealed candidate's coordinator-only door: the
    /// seal is the Loom's, so the sender recorded as preparer is a coordinator because the bus
    /// said so. Every fallible step happens here while the incumbent is fully live -- the plan
    /// checked, the mode chosen, a full letter's capacity reserved, so restoration never grows a
    /// container (copying entry strings may still allocate). A live incarnation refuses the ask,
    /// and a second ask is answered as the mistake it is.
    void on(const PrepareTimerHandover& p, loom::Mail& mail) {
        if (activation_.activated()) {
            decline(mail, p, "this Timer is already live under an accepted activation; a "
                             "serving service is not a candidate");
            return;
        }
        if (prepared_ != Preparation::None) {
            decline(mail, p, "this incarnation has already answered a preparation ask; one "
                             "transaction opens one preparation conversation");
            return;
        }
        const bool inherit = p.continuity == kInheritFromIncumbent;
        if (!inherit && p.continuity != kStartFresh) {
            decline(mail, p,
                    "unknown continuity plan '" + p.continuity +
                        "'; this Timer prepares for '" + kInheritFromIncumbent + "' or '" +
                        kStartFresh + "' and guesses at neither");
            return;
        }
        // Reserved so neither the table, the restore buffer nor the hold grows after "ready": a
        // letter never exceeds kMaxHandoffEntries.
        entries_.reserve(kMaxHandoffEntries);
        restoring_.reserve(kMaxHandoffEntries);
        deferred_.reserve(kMaxDeferredOps);
        startup_ = inherit ? Startup::PreparedRestoration : Startup::Fresh;
        preparer_ = mail.sender();
        prepared_ = Preparation::Accepted;
        mail.answer(TimerCandidatePrepared{p.transaction});
    }

    void on(const Drive& d, loom::Mail& mail) {
        if (!owns_beat(d, mail)) {
            return; // stale, duplicate, replayed, foreign or premature: nothing at all
        }
        if (!chain_sender_.valid()) {
            // The seed came home: the id stamped on it is this incarnation's own, the only
            // self-knowledge a loaded weave has, and every later beat must carry it.
            chain_sender_ = mail.sender();
        }
        if (bootstrap_ == Bootstrap::Awaiting) {
            // A bootstrap beat spends a queue turn and nothing else: no nap, no
            // firing, no schedule touched. It is a real beat of the one chain
            // and is counted as one.
            ++this->state_.beats;
            if (++bootstrap_beats_ >= claim_budget_) {
                resolve_bootstrap(mail, /*letter=*/nullptr); // nobody answered: fresh
            }
            advance_chain(mail);
            return;
        }
        std::int64_t now = clock_.now_ms();
        clock_.nap_ms(nap_until_next(now));
        now = clock_.now_ms();
        fire_due(now, mail);
        ++this->state_.beats;
        advance_chain(mail);
    }

    void on(const StartTimer& s, loom::Mail& mail) {
        schedule(mail, Op{Op::Kind::Start, mail.sender(), s.id, s.delay_ms, s.repeat, {}, {}, {}});
    }

    void on(const StartRoleTimer& s, loom::Mail& mail) {
        schedule(mail, Op{Op::Kind::StartRole, mail.sender(), s.id, s.delay_ms, s.repeat, s.role,
                          {}, {}});
    }

    /// The ordered form: a preference, a fallback, and a receipt.
    void on(const EnsureTimer& e, loom::Mail& mail) {
        schedule(mail, Op{Op::Kind::Ensure, mail.sender(), e.id, e.delay_ms, e.repeat, {},
                          e.preferred, e.fallback});
    }

    void on(const EnsureRoleTimer& e, loom::Mail& mail) {
        schedule(mail, Op{Op::Kind::EnsureRole, mail.sender(), e.id, e.delay_ms, e.repeat, e.role,
                          e.preferred, e.fallback});
    }

    void on(const CancelTimer& c, loom::Mail& mail) {
        schedule(mail, Op{Op::Kind::Cancel, mail.sender(), c.id, 0, false, {}, {}, {}});
    }

    void on(const CancelAllMyTimers&, loom::Mail& mail) {
        schedule(mail, Op{Op::Kind::CancelAll, mail.sender(), {}, 0, false, {}, {}, {}});
    }

    /// "You are being replaced; say what your heir should know": how far each schedule has got,
    /// the one thing a re-ask cannot rebuild. One clock read for the whole letter; nothing fires,
    /// cancels or advances. Answered to the stamped sender, with its correlation.
    void on(const loom::PrepareShutdown&, loom::Mail& mail) {
        const std::int64_t now = clock_.now_ms();
        TimerHandoff handoff;
        for (const Entry& e : entries_) {
            if (handoff.entries.size() >= kMaxHandoffEntries) {
                break; // the published bound, from the writing side
            }
            handoff.entries.push_back(TimerHandoffEntry{std::to_string(e.requester.value), e.id,
                                                        e.role, e.delay_ms, e.repeat,
                                                        remaining_from(e.next_due, now)});
        }
        loom::Bequest letter;
        letter.role = kTimerRole;
        letter.items.push_back(loom::bequeath_item(handoff));
        mail.send(mail.sender(), letter, mail.correlation());
    }

    /// The steward's answer to our claim: a letter.
    void on(const loom::Bequest& letter, loom::Mail& mail) {
        if (!answers_our_claim(mail)) {
            return; // unsolicited, stale, or arriving after the decision was made
        }
        resolve_bootstrap(mail, &letter);
    }

    /// The steward's other answer: "no bequest is held for you." A real answer,
    /// and the fastest honest way to a fresh start.
    void on(const loom::Refused&, loom::Mail& mail) {
        if (!answers_our_claim(mail)) {
            return;
        }
        resolve_bootstrap(mail, /*letter=*/nullptr);
    }

    Clock& clock() { return clock_; }

private:
    /// One standing timer. `role` empty = requester-addressed. `next_due` is
    /// in the service clock's own epoch.
    struct Entry {
        std::string id;
        std::string role;
        loom::WeaveId requester{};
        std::int64_t delay_ms = 0;
        bool repeat = false;
        std::int64_t next_due = 0;
        bool spent = false;
    };

    /// One internal spelling for every schedule operation, so a held-and-replayed operation runs
    /// exactly the code a live one does.
    struct Op {
        enum class Kind { Start, StartRole, Ensure, EnsureRole, Cancel, CancelAll };
        Kind kind = Kind::Start;
        loom::WeaveId sender{};
        std::string id;
        std::int64_t delay_ms = 0;
        bool repeat = false;
        std::string role;
        std::string preferred;
        std::string fallback;
    };

    /// Where this incarnation is in deciding what it inherited: awaiting from construction, not
    /// activation, so an operation arriving before the activation is held too -- it was sent
    /// after the fork point, and a fresh request beats inherited state for the same key.
    enum class Bootstrap { Awaiting, Resolved };

    /// Where this incarnation expects its past from, declared rather than inferred:
    /// `GracefulClaim` (ask the steward by role; start fresh if nobody answers -- every ordinary
    /// load, swap and reload), `PreparedRestoration` (a coordinator said a letter is coming, and
    /// from whom), `Fresh` (told nothing is carried; waits for no letter and refuses a late one).
    enum class Startup { GracefulClaim, PreparedRestoration, Fresh };

    /// Whether this incarnation has answered a preparation ask, and how. One ask
    /// gets one answer; a second is answered as the mistake it is.
    enum class Preparation { None, Accepted, Declined };

    /// Is this beat the one this chain is waiting for? Activated, the current activation's key,
    /// the expected serial and the chain's own sender -- any term failing ignores the Drive. A
    /// loaded weave has no usable `self_`, so the sender is learned from the chain's own seed:
    /// FIFO, non-reentrant dispatch makes the first Drive bearing a fresh key necessarily ours.
    /// That establishes which sender owns the chain, not that it is trustworthy.
    bool owns_beat(const Drive& d, const loom::Mail& mail) const {
        if (!activation_.activated()) {
            return false; // premature: nothing has told this incarnation it is live
        }
        if (!activation_.matches(d.activation_sender, d.activation_sequence)) {
            return false; // a different activation's beat — inherited, stale, or forged
        }
        if (d.serial != expected_serial_) {
            return false; // an old serial (replayed) or a future one (fabricated)
        }
        // Until the sender is learned, the seed is the only Drive that can pass the checks above.
        // Defense in depth, true by construction and not pinned: a mutation removing this term
        // stayed green, because the single serial makes an honoured foreign beat displace the
        // chain rather than fork it. Cheap, and first to matter if a beat ever carried authority.
        return !chain_sender_.valid() || mail.sender() == chain_sender_;
    }

    /// Send the chain's first beat and adopt the identity it comes back with.
    void seed_chain(loom::Mail& mail) {
        mail.send_to_role(kTimerRole,
                          Drive{activation_.sender_text(), activation_.sequence(), 0});
    }

    /// Seed exactly this beat's one successor, guarding before the arithmetic: a wrapped serial
    /// would re-issue a spent one, indistinguishable from a replay. At the boundary the chain ends.
    void advance_chain(loom::Mail& mail) {
        if (!can_advance_serial(expected_serial_)) {
            return;
        }
        ++expected_serial_;
        // Seeded BY ROLE because a loaded weave cannot address itself; ownership
        // is carried by the key and the serial, not by the addressing.
        mail.send_to_role(kTimerRole, Drive{activation_.sender_text(), activation_.sequence(),
                                            expected_serial_});
    }

    // ---- the bootstrap: deciding what this incarnation inherited -------------

    /// Does this answer our claim? `answers_ask()` first -- Loom's word, which only the weave that
    /// received our claim can produce, once; the heir asks by role and cannot pre-bind the
    /// steward's id, and a forged letter would name whom future firings reach. Then our
    /// correlation, then one-shot: `claim_open_` closes at the decision, so a late answer, even a
    /// genuine one, cannot reopen it.
    bool answers_our_claim(const loom::Mail& mail) const {
        return claim_open_ && mail.answers_ask() && mail.correlation() == kClaimCorrelation;
    }

    /// Say no, authentically, spending the one answer a readiness would have: a coordinator hears
    /// a verdict rather than a silence to time out, and the reason stands on its own.
    void decline(loom::Mail& mail, const PrepareTimerHandover& p, std::string why) {
        prepared_ = Preparation::Declined;
        mail.answer(TimerCandidateDeclined{p.transaction, std::move(why)});
    }

    /// The decision, and the only place it is made. Restore (or don't), then
    /// apply everything that arrived while we were deciding, and only then say
    /// the service is available.
    void resolve_bootstrap(loom::Mail& mail, const loom::Bequest* letter) {
        bootstrap_ = Bootstrap::Resolved;
        claim_open_ = false;
        if (letter != nullptr) {
            restore_from(*letter);
        }
        replay_deferred(mail);
        this->state_.active = static_cast<std::int64_t>(entries_.size());
        mail.publish(TimerReady{});
    }

    /// Read the letter: each item is re-admitted through the gate (`loom::claim_item`) before a
    /// field is touched, so a malformed item or another version's handoff is a clean nothing.
    void restore_from(const loom::Bequest& letter) {
        for (const loom::Bytes& item : letter.items) {
            if (const std::optional<TimerHandoff> handoff = loom::claim_item<TimerHandoff>(item)) {
                adopt(*handoff);
                return; // one Timer handoff per letter; the rest is not ours
            }
        }
    }

    /// A letter is adopted whole or not at all: over the bound, or with one requester that is not
    /// a lossless decimal weave id, nothing is taken -- an honest predecessor writes neither, and
    /// half an untrusted letter is a schedule nobody authored.
    void adopt(const TimerHandoff& handoff) {
        if (handoff.entries.size() > kMaxHandoffEntries) {
            return;
        }
        const std::int64_t now = clock_.now_ms();
        // A member buffer, reserved at preparation, so the step after "ready" does not grow it.
        restoring_.clear();
        for (const TimerHandoffEntry& t : handoff.entries) {
            const std::optional<loom::WeaveId> who = parse_weave_id(t.requester);
            if (!who) {
                restoring_.clear();
                return; // malformed: adopt nothing
            }
            const std::int64_t remaining = std::max<std::int64_t>(t.remaining_ms, 0);
            restoring_.push_back(Entry{t.id, t.role, *who, effective_delay(t.delay_ms, t.repeat),
                                       t.repeat, add_clamped(now, remaining), false});
        }
        this->state_.inherited = static_cast<std::int64_t>(restoring_.size());
        for (Entry& e : restoring_) {
            install(std::move(e));
        }
        restoring_.clear();
    }

    /// Put a restored entry in the table under its own key — replacing rather
    /// than doubling, exactly as an upsert would.
    void install(Entry e) {
        if (Entry* existing = find_entry(e.id, e.role, e.requester)) {
            *existing = std::move(e);
            return;
        }
        entries_.push_back(std::move(e));
    }

    /// Every schedule operation, whatever shape carried it, enters here: hold it
    /// if the continuity decision is still pending, otherwise perform it.
    void schedule(loom::Mail& mail, Op op) {
        if (defer(mail, op)) {
            return;
        }
        apply(mail, op);
    }

    /// Hold an operation while the decision is pending; true when the caller must stop. Overflow
    /// is counted (`deferred_dropped`) and, for an ordered request, refused with a receipt.
    bool defer(loom::Mail& mail, const Op& op) {
        if (bootstrap_ == Bootstrap::Resolved) {
            return false;
        }
        if (deferred_.size() >= kMaxDeferredOps) {
            ++this->state_.deferred_dropped;
            if (ordered(op.kind) && op.sender.valid()) {
                answer_order(mail, op, kResolutionRefused,
                             "the timer service is still restoring its schedule and its bounded "
                             "hold of pending operations is full; no schedule was created or "
                             "changed");
            }
            return true;
        }
        deferred_.push_back(op);
        return true;
    }

    /// Everything that waited, in ARRIVAL ORDER, after the inheritance and
    /// before the availability notice. Arrival order is what makes a later
    /// request beat an earlier one for the same key, and running it after the
    /// restore is what makes any request beat inherited state.
    void replay_deferred(loom::Mail& mail) {
        std::vector<Op> ops;
        ops.swap(deferred_);
        for (const Op& op : ops) {
            apply(mail, op);
        }
    }

    static bool ordered(typename Op::Kind k) {
        return k == Op::Kind::Ensure || k == Op::Kind::EnsureRole;
    }

    // ---- performing an operation --------------------------------------------

    void apply(loom::Mail& mail, const Op& op) {
        switch (op.kind) {
        case Op::Kind::Start:
            if (!op.sender.valid()) {
                // No weave asked, so there is no one to fire at. Dropped, counted.
                ++this->state_.dropped;
                return;
            }
            upsert(op.id, /*role=*/"", op.sender, op.delay_ms, op.repeat);
            return;
        case Op::Kind::StartRole:
            if (op.role.empty()) {
                ++this->state_.dropped; // a role beat with no role is no ask at all
                return;
            }
            upsert(op.id, op.role, op.sender, op.delay_ms, op.repeat);
            return;
        case Op::Kind::Ensure:
        case Op::Kind::EnsureRole:
            apply_ensure(mail, op);
            return;
        case Op::Kind::Cancel:
            remove_mine(op.sender, &op.id);
            return;
        case Op::Kind::CancelAll:
            remove_mine(op.sender, nullptr);
            return;
        }
    }

    /// The order model: request -> available menu -> chosen -> receipt. A standing entry matches
    /// when it has the same upsert key and the same meaning -- repeat mode and normalized delay.
    /// A changed addressing mode finds nothing; a changed delay or mode finds the entry but no
    /// match: both are unavailable and go to the fallback, since "preserved" would describe a
    /// schedule nobody asked for.
    void apply_ensure(loom::Mail& mail, const Op& op) {
        if (!op.sender.valid()) {
            // An order with no stamped requester has nowhere to send its receipt; the raw shapes
            // are the fire-and-forget door.
            ++this->state_.dropped;
            return;
        }
        if (op.kind == Op::Kind::EnsureRole && op.role.empty()) {
            answer_order(mail, op, kResolutionRefused,
                         "a role beat with no role is no ask at all; no schedule was created or "
                         "changed");
            return;
        }
        const std::optional<Continuity> preferred = continuity_from(op.preferred);
        if (!preferred) {
            answer_order(mail, op, kResolutionRefused,
                         "unknown continuity preference '" + op.preferred +
                             "'; no schedule was created or changed");
            return;
        }
        const std::int64_t delay = effective_delay(op.delay_ms, op.repeat);
        Entry* existing = find_entry(op.id, op.role, op.sender);
        const bool can_preserve =
            existing != nullptr && existing->repeat == op.repeat && existing->delay_ms == delay;

        Continuity choice = *preferred;
        std::string why = "'" + op.preferred + "' was available: ";
        if (choice == Continuity::PreserveRemaining && !can_preserve) {
            const std::string unavailable =
                existing == nullptr
                    ? "'preserve_remaining' was unavailable (no matching schedule to preserve)"
                    : "'preserve_remaining' was unavailable (the declared schedule differs from "
                      "the standing one)";
            if (op.fallback.empty()) {
                answer_order(mail, op, kResolutionRefused,
                             unavailable + " and no fallback was acceptable; no schedule was "
                                           "created or changed");
                return;
            }
            const std::optional<Continuity> fallback = continuity_from(op.fallback);
            if (!fallback || (*fallback == Continuity::PreserveRemaining && !can_preserve)) {
                answer_order(mail, op, kResolutionRefused,
                             unavailable + " and the declared fallback '" + op.fallback +
                                 "' could not be used; no schedule was created or changed");
                return;
            }
            choice = *fallback;
            why = unavailable + "; fell back to '" + op.fallback + "': ";
        }
        switch (choice) {
        case Continuity::PreserveRemaining:
            // Ownership moves exactly as an upsert moves it (a successor
            // re-asking takes cancel rights); the ONE thing preservation does
            // not do is re-anchor the schedule.
            existing->requester = op.sender;
            answer_order(mail, op, kResolutionPreserved,
                         why + "the standing schedule kept its remaining time and was not "
                               "re-anchored");
            return;
        case Continuity::RestartDelay:
            upsert(op.id, op.role, op.sender, delay, op.repeat);
            answer_order(mail, op, kResolutionRestarted,
                         why + "the next firing is the declared delay from now");
            return;
        case Continuity::Drop:
            erase_entry(op.id, op.role, op.sender);
            answer_order(mail, op, kResolutionDropped,
                         why + "no schedule is kept or created for this id");
            return;
        }
    }

    /// The receipt, to the stamped requester, with correlation 0: the ordered shapes are sent, not
    /// forwarded, so there is no asker's correlation to echo; a binding matches on the timer id.
    void answer_order(loom::Mail& mail, const Op& op, const char* resolved, std::string reason) {
        mail.send(op.sender, TimerResolution{op.id, resolved, std::move(reason)});
    }

    std::int64_t nap_until_next(std::int64_t now) const {
        std::int64_t nap = kBeatCapMs;
        for (const Entry& e : entries_) {
            nap = std::min(nap, e.next_due - now);
        }
        return std::max<std::int64_t>(nap, 0);
    }

    void fire_due(std::int64_t now, loom::Mail& mail) {
        // Indexed loop on purpose: firing can grow the queue, never entries_,
        // but an upsert arriving mid-beat is impossible anyway (single-threaded
        // dispatch) — the index just keeps firing order the insertion order.
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            Entry& e = entries_[i];
            if (e.next_due > now) {
                continue;
            }
            if (e.role.empty()) {
                mail.send(e.requester, TimerFired{e.id});
            } else {
                mail.send_to_role(e.role, TimerFired{e.id});
            }
            ++this->state_.fired;
            if (e.repeat) {
                // Hold the lattice when merely late; never burst to catch up
                // (one firing per beat per timer — a stalled host gets one
                // tick on resume, not a flood of stale ones).
                e.next_due += e.delay_ms;
                if (e.next_due <= now) {
                    e.next_due = now + e.delay_ms;
                }
            } else {
                e.spent = true;
            }
        }
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [](const Entry& e) { return e.spent; }),
                       entries_.end());
        this->state_.active = static_cast<std::int64_t>(entries_.size());
    }

    /// The upsert key, in one place: (requester, id) for requester timers, (role, id) across
    /// requesters for role timers -- a successor replaces its predecessor's beat and takes cancel
    /// rights. The ordered availability and the raw replace-or-insert are one question.
    Entry* find_entry(const std::string& id, const std::string& role, loom::WeaveId requester) {
        for (Entry& e : entries_) {
            const bool same = role.empty()
                                  ? (e.role.empty() && e.requester == requester && e.id == id)
                                  : (e.role == role && e.id == id);
            if (same) {
                return &e;
            }
        }
        return nullptr;
    }

    void erase_entry(const std::string& id, const std::string& role, loom::WeaveId requester) {
        Entry* e = find_entry(id, role, requester);
        if (e == nullptr) {
            return;
        }
        entries_.erase(entries_.begin() + (e - entries_.data()));
        this->state_.active = static_cast<std::int64_t>(entries_.size());
    }

    /// The one write path for asks.
    void upsert(const std::string& id, const std::string& role, loom::WeaveId requester,
                std::int64_t delay_ms, bool repeat) {
        const std::int64_t delay = effective_delay(delay_ms, repeat);
        const std::int64_t due = add_clamped(clock_.now_ms(), delay);
        if (Entry* e = find_entry(id, role, requester)) {
            e->requester = requester;
            e->delay_ms = delay;
            e->repeat = repeat;
            e->next_due = due;
            return;
        }
        entries_.push_back(Entry{id, role, requester, delay, repeat, due, false});
        this->state_.active = static_cast<std::int64_t>(entries_.size());
    }

    /// Cancellation: everything `who` started (id == nullptr), or its timers
    /// with exactly that id. Matching is by recorded requester — the bus
    /// stamps senders, so this is identity, not claim.
    void remove_mine(loom::WeaveId who, const std::string* id) {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const Entry& e) {
                                          return e.requester == who &&
                                                 (id == nullptr || e.id == *id);
                                      }),
                       entries_.end());
        this->state_.active = static_cast<std::int64_t>(entries_.size());
    }

    // ---- the numbers this service works in ----------------------------------
    //
    // The delay rule is `timer.normalize_delay`'s and only its call is here; saturating a
    // deadline and measuring what is left of one are facts about a clock, and stay the Timer's.

    /// What this Timer makes of an authored delay, from `timer.normalize_delay` (normalize.hpp),
    /// never re-encoded. Every path that normalizes -- `upsert`, the ordered availability
    /// comparison, `adopt` -- comes through here, so no schedule can reach another truth than the
    /// one this instance was built with.
    std::int64_t effective_delay(std::int64_t delay_ms, bool repeat) const {
        return semantics_.effective_delay(delay_ms, repeat);
    }

    /// now + duration, saturating rather than wrapping. A wrapped deadline would
    /// read as permanently overdue and fire forever.
    static std::int64_t add_clamped(std::int64_t now, std::int64_t duration) {
        constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
        if (duration > 0 && now > kMax - duration) {
            return kMax;
        }
        return now + duration;
    }

    /// How long until this deadline, without underflow: a due or overdue entry transfers with
    /// zero remaining, and the subtraction is unsigned (defined), saturated into the wire's range.
    static std::int64_t remaining_from(std::int64_t next_due, std::int64_t now) {
        if (next_due <= now) {
            return 0;
        }
        const std::uint64_t diff =
            static_cast<std::uint64_t>(next_due) - static_cast<std::uint64_t>(now);
        constexpr std::uint64_t kMax =
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        return static_cast<std::int64_t>(diff > kMax ? kMax : diff);
    }

    /// A weave id, off the wire, losslessly or not at all. Canonical decimal
    /// only: no sign, no whitespace, no trailing characters.
    static std::optional<loom::WeaveId> parse_weave_id(const std::string& text) {
        if (text.empty()) {
            return std::nullopt;
        }
        std::uint64_t value = 0;
        const char* first = text.data();
        const char* last = first + text.size();
        const std::from_chars_result r = std::from_chars(first, last, value);
        if (r.ec != std::errc{} || r.ptr != last) {
            return std::nullopt;
        }
        return loom::WeaveId{value};
    }

    // Per-incarnation, never TimerState: nothing about a chain or a bootstrap may transplant
    // through a reload, or a new instance would inherit liveness it did not author.
    zengine::ActivationCursor activation_; ///< which activation this incarnation lives under
    std::int64_t expected_serial_ = 0;     ///< the one beat this chain will accept next
    loom::WeaveId chain_sender_{};         ///< learned from the seed; the id a beat must carry
    Bootstrap bootstrap_ = Bootstrap::Awaiting; ///< open from construction; closed by the decision
    std::int64_t bootstrap_beats_ = 0;          ///< beats spent waiting for the claim's answer
    std::int64_t claim_budget_ = kBootstrapBeats; ///< how many that mode allows before it gives up
    bool claim_open_ = false;                   ///< a claim is outstanding (opens at activation)
    std::vector<Op> deferred_;                  ///< held while the decision is pending
    /// The prepared-replacement half, all per-incarnation for the same reason
    /// everything else here is: nothing about a preparation may transplant.
    Startup startup_ = Startup::GracefulClaim; ///< told, before admission, never inferred
    Preparation prepared_ = Preparation::None; ///< one ask, one answer
    loom::WeaveId preparer_{};                 ///< read off the ask; whose letter to claim
    std::vector<Entry> restoring_;             ///< adopt()'s buffer; reserved at preparation

    Clock clock_{};
    /// Which semantic truth this service executes: the operator surface of a host that offered
    /// one, for this Timer's whole life, or the vocabulary this repository authors. Both resolve
    /// `timer.normalize_delay` at every spend and hold no resolution, so execution and a preview
    /// cannot disagree. Default-constructed means local fallback: a host that never heard of
    /// operators.
    DelayAuthority semantics_;
    std::vector<Entry> entries_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_TIMER_WEAVE_HPP
