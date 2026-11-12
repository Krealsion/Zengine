// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_TIMER_WEAVE_HPP
#define ZENGINE_TIMER_TIMER_WEAVE_HPP

// The TimerService weave. It reads no clock and sleeps nowhere: it claims, as its role, the time
// its soonest timer is due (`loom::host::WaitSet::until_ms`), and the host's one wait (Loom's
// `zen/host/reactor.hpp`) tells it when that time came in a `loom::host::Waited` carrying the
// wait's clock, `now_ms`, which it fires on. The wire is the manual, timer/docs/zengine.timer.md;
// what follows is what that page does not carry: the ordering this code depends on, its traps.
// Timer law: timer/docs/timer-laws.md

// Time is learned, never read (TIMER-02): an ask is anchored at the next time the wait hands over,
// so a timer asked while none is known is claimed as a time already past and the next wait says
// the time -- one more wake an arm. Nothing is queued to keep time: between a claim and its wake
// the bus holds nothing for the Timer, and a bus whose only business is the Timer drains. Only the
// host's own `Waited` (no sender) is time; one from any participant is nothing (TIMER-01).

// When the service goes: unloaded or swapped, its claim goes stale with its role; reloaded, the
// new instance keeps TimerState -- its counters, and the table as remaining durations
// (`standing`), measured from the last time it was handed -- and republishes TimerReady. A reload
// writes no letter: PrepareShutdown is a replacement's.

// Cleanup is told: a timer fired at a weave that is gone comes back as the bus's refusal
// (`zen.DispatchRefused`, NoSuchTarget), and every timer that weave started is dropped then.
// Role-addressed timers outlive their holders by construction; polite weaves cancel on their way
// out (`CancelAllMyTimers`).

#include "normalize.hpp"
#include "source_wait.hpp"
#include "vocabulary.hpp"

#include "activation/activation.hpp"
#include "manual/vocabulary.hpp"
#include "manuals/zengine.timer.hpp" // this role's manual, compiled in from its page
#include "operator/catalog.hpp"

#include <zen/host/wait_set.hpp>
#include <zen/weave.hpp>
#include <zen/weave/dispatch_refusal.hpp>
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

/// Seven honest counters, poke-inspectable: the wait's wakes it took, firings, timers standing,
/// asks dropped for having nobody to answer, operations dropped from a full bootstrap hold, entries
/// this incarnation began with (`inherited`: restored from a predecessor's letter or carried
/// through a reload in place; 0 means it started fresh), and timers dropped because the weave they
/// fire at is gone (`gone`). `standing` is the table as a reload carries it: filled only in a
/// snapshot and read only at revival, never the live table.
struct TimerState {
    std::int64_t wakes = 0;
    std::int64_t fired = 0;
    std::int64_t active = 0;
    std::int64_t dropped = 0;
    std::int64_t deferred_dropped = 0;
    std::int64_t inherited = 0;
    std::int64_t gone = 0;
    std::vector<TimerHandoffEntry> standing;
    ZEN_EXPOSE();
    ZEN_SHAPE(TimerState, 4, ZEN_FIELD(wakes), ZEN_FIELD(fired), ZEN_FIELD(active),
              ZEN_FIELD(dropped), ZEN_FIELD(deferred_dropped), ZEN_FIELD(inherited),
              ZEN_FIELD(gone), ZEN_FIELD(standing));
};

class TimerWeave
    : public zengine::With<manual::ManualFacet<manual::pages::zengine_timer::kManual>>::Weave<
          TimerWeave, TimerState,
          loom::Accept<loom::Activated, loom::host::Waited, StartTimer, StartRoleTimer,
                       EnsureTimer, EnsureRoleTimer, CancelTimer, CancelAllMyTimers,
                       loom::PrepareShutdown, loom::Bequest, loom::Refused, loom::DispatchRefused,
                       PrepareTimerHandover>,
          loom::Emit<TimerFired, TimerReady, TimerResolution, loom::Bequest, loom::ClaimBequest,
                     TimerCandidatePrepared, TimerCandidateDeclined>,
          loom::WeaveBase, loom::Claims<loom::host::WaitSet>> {
public:
    TimerWeave() = default;

    /// With another catalog carrying the same identity: the Timer consumes the delay rule and
    /// does not own it, so the suite can replace a primitive beneath it and watch execution and an
    /// independent reader move together.
    explicit TimerWeave(op::Catalog operators) : semantics_(std::move(operators)) {}

    /// ...or with the authority it resolves the rule through: a loaded artifact builds one from
    /// what its host offered (`DelayAuthority`). The only door to a host-backed Timer; no setter.
    explicit TimerWeave(DelayAuthority semantics) : semantics_(std::move(semantics)) {}

    /// The rule this Timer runs, for anyone who wants to evaluate it without scheduling anything.
    /// LOCAL-FALLBACK only: a host-backed Timer carries no catalog at all, and this throws rather
    /// than inventing one.
    const op::Catalog& operators() const { return semantics_.operators(); }

    /// Whether this Timer resolves through a host's catalog or its own. A diagnostic and nothing
    /// more -- what a Timer SCHEDULES is where its authority is proven.
    bool host_backed() const noexcept { return semantics_.host_backed(); }

    /// The Loom's control door says this incarnation is committed and live. That is the whole of
    /// what it says -- so this is where the service decides what to DO about it, which is: find
    /// out what it inherited, and only then become available (TIMER-04).
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            // Unattested, forged, duplicate or replayed: no notice, no claim, nothing. A first
            // breath is Loom's to grant, not a shape's to claim.
            return;
        }
        // Tell the truth about the table rather than assuming: activation is not state migration
        // and has no business clearing one that legitimately holds entries.
        this->state_.active = static_cast<std::int64_t>(entries_.size());
        // No TimerReady until the decision (TIMER-04): a consumer re-asking before restoration
        // would re-anchor the very schedule the letter carried.
        bootstrap_ = Bootstrap::Awaiting;
        bootstrap_wakes_ = 0;
        this->state_.inherited = revived_;
        // WHERE THIS INCARNATION LOOKS FOR ITS LETTER, decided by what it was TOLD before it was
        // admitted and never by what it can see. Anything already held stays held in every mode.
        switch (startup_) {
        case Startup::PreparedRestoration:
            // A prepared candidate knows whose letter it waits for -- it read the preparer off its
            // ask's bus-stamped sender -- so it asks that weave by id, trusting only Loom's word.
            claim_open_ = true;
            claim_budget_ = kPreparedClaimWakes;
            mail.send(preparer_, loom::ClaimBequest{kTimerRole}, kClaimCorrelation);
            return;
        case Startup::Fresh:
            // Told plainly that nothing is being carried: no claim goes out, so there is nothing
            // a late letter could reopen. Decide now, replay what was held, and announce.
            claim_open_ = false;
            claim_budget_ = 0;
            resolve_bootstrap(mail, /*letter=*/nullptr);
            return;
        case Startup::GracefulClaim:
            // Ask the steward by role. Its answer -- a letter or a refusal -- decides, and so does
            // the bus's refusal of the claim when no steward stands; the wakes are the bound.
            claim_open_ = true;
            claim_budget_ = kBootstrapWakes;
            mail.send_to_role(loom::kManagerRole, loom::ClaimBequest{kTimerRole},
                              kClaimCorrelation);
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

    /// WHAT THE HOST'S WAIT SAID: the time, `now_ms`, on the wait's clock. Only the host's own
    /// word is time (it has no sender); a `Waited` from a participant is nothing. Every one the
    /// host sends carries the time, so each anchors what waited for one and fires what is due.
    void on(const loom::host::Waited& w, loom::Mail& mail) {
        if (mail.sender().valid() || !activation_.activated()) {
            return;
        }
        if (w.due) {
            ++this->state_.wakes;
            woke_ = true;
        }
        handed(w.now_ms);
        if (bootstrap_ == Bootstrap::Awaiting) {
            // A wake spent waiting for the claim's answer is counted and fires nothing.
            if (w.due && claim_open_ && ++bootstrap_wakes_ >= claim_budget_) {
                resolve_bootstrap(mail, /*letter=*/nullptr); // nobody answered: fresh
            }
            return;
        }
        anchor_waiting();
        fire_due(mail);
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
    /// the one thing a re-ask cannot rebuild, measured from the last time this Timer was handed.
    /// Nothing fires, cancels or advances. Answered to the stamped sender, with its correlation.
    void on(const loom::PrepareShutdown&, loom::Mail& mail) {
        TimerHandoff handoff;
        // The published bound, from the writing side.
        handoff.entries = describe_table(kMaxHandoffEntries);
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

    /// The steward's other answer: "no bequest is held for you." A real answer, and the fastest
    /// honest way to a fresh start.
    void on(const loom::Refused&, loom::Mail& mail) {
        if (!answers_our_claim(mail)) {
            return;
        }
        resolve_bootstrap(mail, /*letter=*/nullptr);
    }

    /// WHAT THE BUS REFUSED OF THIS TIMER'S SENDS, in Loom's own word (`dispatch_refused`): the
    /// claim, when nobody stands to answer it -- a fresh start; a firing at a weave that is gone --
    /// every timer that weave started dropped, since none of them can ever be delivered.
    void on(const loom::DispatchRefused& r, loom::Mail& mail) {
        if (!mail.dispatch_refused()) {
            return; // ordinary speech wearing the shape, not the bus's word
        }
        if (r.shape == loom::ClaimBequest::zen_name) {
            if (claim_open_ && bootstrap_ == Bootstrap::Awaiting) {
                resolve_bootstrap(mail, /*letter=*/nullptr);
            }
            return;
        }
        if (r.shape != TimerFired::zen_name || r.reason != kNoSuchTarget) {
            return;
        }
        const loom::WeaveId gone = r.addressed_weave();
        if (!gone.valid()) {
            return; // a role timer's holder came and went; the role keeps its timers
        }
        const std::size_t before = entries_.size();
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const Entry& e) { return e.requester == gone; }),
                       entries_.end());
        this->state_.gone += static_cast<std::int64_t>(before - entries_.size());
        this->state_.active = static_cast<std::int64_t>(entries_.size());
    }

    /// AFTER EVERY DELIVERY: what this Timer waits for, claimed as its role whenever it changed or
    /// a wake was taken -- the wait holds a time due once per claim, so a source claims anew after
    /// each wake, unchanged or not.
    void after_delivery(loom::Mail& mail) {
        if (!activation_.activated()) {
            return;
        }
        const std::optional<std::int64_t> until = wanted_until();
        if (claimed_ && !woke_ && until == claimed_until_) {
            return;
        }
        loom::host::WaitSet set;
        set.until_ms = until;
        (void)mail.as_role(kTimerRole).claim(set);
        claimed_ = true;
        claimed_until_ = until;
        woke_ = false;
    }

    /// The state a reload carries: the counters, and every standing timer as the letter describes
    /// one -- its remaining duration from the last time handed, since a deadline on this wait's
    /// clock means nothing on the next instance's.
    loom::Value snapshot() const override {
        TimerState carried = this->state_;
        carried.standing = describe_table(entries_.size());
        return loom::to_value(carried);
    }

    /// Revival installs what the snapshot carried, the table whole or not at all, as a letter is
    /// adopted; the next activation counts it as `inherited`.
    void revive(const loom::Value& v) override {
        TimerState revived = loom::from_value<TimerState>(v);
        std::vector<TimerHandoffEntry> standing;
        standing.swap(revived.standing);
        this->state_ = std::move(revived);
        entries_.clear();
        revived_ = static_cast<std::int64_t>(table_from(standing, entries_));
        this->state_.active = static_cast<std::int64_t>(entries_.size());
    }

private:
    /// Loom's name for a delivery whose addressed weave does not exist (`name_of(NoSuchTarget)`).
    static constexpr const char* kNoSuchTarget = "NoSuchTarget";

    /// One standing timer. `role` empty = requester-addressed. `next_due` is a reading of the
    /// wait's clock, or none while the timer waits for the next time handed to be anchored at,
    /// `first_in` after it.
    struct Entry {
        std::string id;
        std::string role;
        loom::WeaveId requester{};
        std::int64_t delay_ms = 0;
        bool repeat = false;
        std::optional<std::int64_t> next_due;
        std::int64_t first_in = 0;
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

    /// Whether this incarnation has answered a preparation ask, and how. One ask gets one
    /// answer; a second is answered as the mistake it is.
    enum class Preparation { None, Accepted, Declined };

    /// The time this Timer waits for, as its claim says it: a time already past while it must
    /// learn the time (an entry waiting to be anchored, or a bootstrap counting wakes), else its
    /// soonest deadline, else none.
    std::optional<std::int64_t> wanted_until() const {
        if (bootstrap_ == Bootstrap::Awaiting) {
            return claim_open_ ? std::optional<std::int64_t>(kTimePast) : std::nullopt;
        }
        std::optional<std::int64_t> soonest;
        for (const Entry& e : entries_) {
            if (!e.next_due) {
                return kTimePast;
            }
            if (!soonest || *e.next_due < *soonest) {
                soonest = e.next_due;
            }
        }
        return soonest;
    }

    /// A reading of the wait's clock, kept as the latest time this Timer knows; the clock never
    /// goes back, and a stale reading moves nothing.
    void handed(std::int64_t now) {
        if (!now_ || now > *now_) {
            now_ = now;
        }
    }

    /// Every timer that waited for a time, due `first_in` after the time just handed.
    void anchor_waiting() {
        if (!now_) {
            return;
        }
        for (Entry& e : entries_) {
            if (!e.next_due) {
                e.next_due = add_clamped(*now_, e.first_in);
            }
        }
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

    /// The decision, and the only place it is made. Restore (or don't), then apply everything
    /// that arrived while we were deciding, and only then say the service is available.
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
        // A member buffer, reserved at preparation, so the step after "ready" does not grow it.
        restoring_.clear();
        this->state_.inherited = static_cast<std::int64_t>(table_from(handoff.entries, restoring_));
        for (Entry& e : restoring_) {
            install(std::move(e));
        }
        restoring_.clear();
    }

    /// The table as letter records, in table order, at most `bound` of them: what is left of each
    /// timer from the last time handed, and a timer still waiting for one with all of it left.
    std::vector<TimerHandoffEntry> describe_table(std::size_t bound) const {
        std::vector<TimerHandoffEntry> out;
        for (const Entry& e : entries_) {
            if (out.size() >= bound) {
                break;
            }
            const std::int64_t left = e.next_due && now_ ? remaining_from(*e.next_due, *now_)
                                                         : std::max<std::int64_t>(e.first_in, 0);
            out.push_back(TimerHandoffEntry{std::to_string(e.requester.value), e.id, e.role,
                                            e.delay_ms, e.repeat, left});
        }
        return out;
    }

    /// Letter records as timers due their remaining time after the next time handed, appended to
    /// `out`: all of them, or none when one requester is not a lossless decimal weave id. Returns
    /// how many were appended.
    std::size_t table_from(const std::vector<TimerHandoffEntry>& records,
                           std::vector<Entry>& out) const {
        const std::size_t first = out.size();
        for (const TimerHandoffEntry& t : records) {
            const std::optional<loom::WeaveId> who = parse_weave_id(t.requester);
            if (!who) {
                out.resize(first);
                return 0; // malformed: adopt nothing
            }
            Entry e{t.id, t.role, *who, effective_delay(t.delay_ms, t.repeat), t.repeat,
                    std::nullopt, std::max<std::int64_t>(t.remaining_ms, 0), false};
            out.push_back(std::move(e));
        }
        return out.size() - first;
    }

    /// Put a restored entry in the table under its own key -- replacing rather than doubling,
    /// exactly as an upsert would.
    void install(Entry e) {
        if (Entry* existing = find_entry(e.id, e.role, e.requester)) {
            *existing = std::move(e);
            return;
        }
        entries_.push_back(std::move(e));
    }

    /// Every schedule operation, whatever shape carried it, enters here: hold it if the
    /// continuity decision is still pending, otherwise perform it.
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

    /// Everything that waited, in ARRIVAL ORDER, after the inheritance and before the
    /// availability notice. Arrival order is what makes a later request beat an earlier one for
    /// the same key, and running it after the restore is what makes any request beat inherited
    /// state.
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
            // Ownership moves exactly as an upsert moves it (a successor re-asking takes cancel
            // rights); the ONE thing preservation does not do is re-anchor the schedule.
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

    /// Fire every anchored timer due by the time last handed, in table order.
    void fire_due(loom::Mail& mail) {
        if (!now_) {
            return;
        }
        const std::int64_t now = *now_;
        // Indexed loop on purpose: firing can grow the queue, never entries_, and an upsert
        // arriving mid-fire is impossible (single-threaded dispatch).
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            Entry& e = entries_[i];
            if (!e.next_due || *e.next_due > now) {
                continue;
            }
            if (e.role.empty()) {
                mail.send(e.requester, TimerFired{e.id});
            } else {
                mail.send_to_role(e.role, TimerFired{e.id});
            }
            ++this->state_.fired;
            if (e.repeat) {
                // Hold the lattice when merely late; never burst to catch up (one firing a wake a
                // timer -- a stalled host gets one tick on resume, not a flood of stale ones).
                *e.next_due = add_clamped(*e.next_due, e.delay_ms);
                if (*e.next_due <= now) {
                    e.next_due = add_clamped(now, e.delay_ms);
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

    /// The one write path for asks: due `delay` after the next time the wait hands over.
    void upsert(const std::string& id, const std::string& role, loom::WeaveId requester,
                std::int64_t delay_ms, bool repeat) {
        const std::int64_t delay = effective_delay(delay_ms, repeat);
        if (Entry* e = find_entry(id, role, requester)) {
            e->requester = requester;
            e->delay_ms = delay;
            e->repeat = repeat;
            e->next_due.reset();
            e->first_in = delay;
            return;
        }
        entries_.push_back(Entry{id, role, requester, delay, repeat, std::nullopt, delay, false});
        this->state_.active = static_cast<std::int64_t>(entries_.size());
    }

    /// Cancellation: everything `who` started (id == nullptr), or its timers with exactly that id.
    /// Matching is by recorded requester -- the bus stamps senders, so this is identity, not claim.
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

    /// now + duration, saturating rather than wrapping. A wrapped deadline would read as
    /// permanently overdue and fire forever.
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

    /// A weave id, off the wire, losslessly or not at all. Canonical decimal only: no sign, no
    /// whitespace, no trailing characters.
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

    // Per-incarnation, never TimerState: nothing about a claim, a time or a bootstrap may
    // transplant through a reload, or a new instance would inherit liveness it did not author.
    zengine::ActivationCursor activation_; ///< which activation this incarnation lives under
    std::optional<std::int64_t> now_;      ///< the latest time the wait handed over
    bool claimed_ = false;                 ///< a WaitSet has been claimed since activation
    std::optional<std::int64_t> claimed_until_; ///< the time that claim named
    bool woke_ = false;                         ///< a wake was taken since the last claim
    Bootstrap bootstrap_ = Bootstrap::Awaiting; ///< open from construction; closed by the decision
    std::int64_t bootstrap_wakes_ = 0;          ///< wakes spent waiting for the claim's answer
    std::int64_t claim_budget_ = kBootstrapWakes; ///< how many that mode allows before it gives up
    bool claim_open_ = false;                     ///< a claim is outstanding (opens at activation)
    std::vector<Op> deferred_;                    ///< held while the decision is pending
    /// The prepared-replacement half, all per-incarnation for the same reason everything else
    /// here is: nothing about a preparation may transplant.
    Startup startup_ = Startup::GracefulClaim; ///< told, before admission, never inferred
    Preparation prepared_ = Preparation::None; ///< one ask, one answer
    loom::WeaveId preparer_{};                 ///< read off the ask; whose letter to claim
    std::vector<Entry> restoring_;             ///< adopt()'s buffer; reserved at preparation
    std::int64_t revived_ = 0;                 ///< entries the last revival installed

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
