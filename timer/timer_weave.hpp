// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_TIMER_WEAVE_HPP
#define ZENGINE_TIMER_TIMER_WEAVE_HPP

// The Timer weave. It reads no clock and never sleeps. It claims, as its role, the time its
// soonest timer is due (`loom::host::WaitSet::until_ms`), and the host's one wait (Loom's
// `zen/host/reactor.hpp`) tells it when that time has come by sending a `loom::host::Waited` that
// carries the wait's clock, `now_ms`; the Timer fires on that time. The messages themselves are
// described in the manual, `timer/docs/zengine.timer.md`. These comments say what the page does
// not: the order of events this code depends on, and the mistakes it guards against.

// The Timer learns the time and never reads it (TIMER-02). A timer starts counting at the next
// time the wait sends, so a timer asked for while no time is known is claimed as a time already
// past, and the next wait sends the time: the first ask costs one more wake. No message is queued
// to keep time. Between a claim and its wake the bus holds nothing for the Timer, so a bus with no
// other work empties. Only the host's own `Waited`, which has no sender, is time; one from any
// weave changes nothing (TIMER-01).

// When the Timer goes away: unloaded or replaced, its claim ends with its role. Reloaded in place,
// the new incarnation keeps `TimerState`, which holds the counters and the table as the time each
// timer had left (`standing`), measured from the last time the wait sent, and it publishes
// `TimerReady` again. A reload writes no letter; only a replacement's `PrepareShutdown` asks for
// one.

// The Timer learns that a weave is gone from the bus. A timer fired at a weave that no longer
// exists comes back as the bus's refusal (`zen.DispatchRefused`, NoSuchTarget), and every timer
// that weave started is dropped then. A role timer outlives each holder of the role by design, so
// a weave that is leaving for good cancels its own on the way out (`CancelAllMyTimers`).

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

/// The Timer's state, which an inspection can read: seven counters, each labelled where a maker
/// reads them, in the About section of the Timer's manual (timer/docs/zengine.timer.md), and
/// `standing`, the table as a reload carries it. `standing` is filled only in a snapshot and read
/// only on revival, and it is never the table the Timer runs on.
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
                     loom::Refused, TimerCandidatePrepared, TimerCandidateDeclined>,
          loom::WeaveBase, loom::Claims<loom::host::WaitSet>> {
public:
    TimerWeave() = default;

    /// A Timer with another catalog that holds the same delay rule. The Timer uses the rule and
    /// does not own it, so a suite can replace a primitive under it and see the Timer's schedule and
    /// a separate reader of the rule change together.
    explicit TimerWeave(op::Catalog operators) : semantics_(std::move(operators)) {}

    /// A Timer that looks up the delay rule through `semantics`, which a loaded library builds from
    /// what its host offered (`DelayAuthority`). This constructor is the only way to make a Timer
    /// backed by a host; there is no setter.
    explicit TimerWeave(DelayAuthority semantics) : semantics_(std::move(semantics)) {}

    /// The catalog holding the rule this Timer runs, for anyone who wants to run the rule without
    /// scheduling anything. Only a local Timer has one: a host-backed Timer carries no catalog, and
    /// this throws rather than making one up.
    const op::Catalog& operators() const { return semantics_.operators(); }

    /// Whether this Timer looks up the rule in a host's catalog or its own. It is for diagnosis
    /// only: which rule a Timer runs is proved by what it schedules.
    bool host_backed() const noexcept { return semantics_.host_backed(); }

    /// Loom's activation: this incarnation is committed and live, and that is all it means. So this
    /// is where the Timer decides what to do about it: find out what it inherited, and only then
    /// start serving (TIMER-04).
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            // Not attested, forged, a duplicate or a replay: no notice, no claim, nothing. Only
            // Loom can activate a weave; sending the message does not.
            return;
        }
        // Count the table as it is rather than assume it is empty: activation does not move state,
        // and must not clear a table that rightly holds timers, such as one a reload carried.
        this->state_.active = static_cast<std::int64_t>(entries_.size());
        // No `TimerReady` until the decision is made (TIMER-04): a weave that asked again before
        // the letter was restored would start again the very timer the letter carried.
        bootstrap_ = Bootstrap::Awaiting;
        bootstrap_wakes_ = 0;
        this->state_.inherited = revived_;
        // Where this incarnation looks for its letter is decided by what it was told before it was
        // admitted, never by what it can see. Any ask already held stays held in every case.
        switch (startup_) {
        case Startup::PreparedRestoration:
            // A prepared candidate knows whose letter it waits for, because it took the preparer
            // from the sender the bus named on the request. So it asks that weave by id, and trusts
            // only Loom's word that the answer answers it.
            claim_open_ = true;
            claim_budget_ = kPreparedClaimWakes;
            mail.send(preparer_, loom::ClaimBequest{kTimerRole}, kClaimCorrelation);
            return;
        case Startup::Fresh:
            // Told that nothing is carried, it sends no claim, so a late letter has nothing to
            // answer. It decides now, applies what was held, and announces itself.
            claim_open_ = false;
            claim_budget_ = 0;
            resolve_bootstrap(mail, /*letter=*/nullptr);
            return;
        case Startup::GracefulClaim:
            // Ask the steward, Loom's keeper of bequests, by its role. Its answer, a letter or a
            // refusal, decides; so does the bus's refusal of the claim when no steward stands. The
            // count of wakes bounds the wait.
            claim_open_ = true;
            claim_budget_ = kBootstrapWakes;
            mail.send_to_role(loom::kManagerRole, loom::ClaimBequest{kTimerRole},
                              kClaimCorrelation);
            return;
        }
    }

    /// A coordinator's request that this sealed candidate get ready to become the Timer. Loom seals
    /// the candidate so that only a coordinator can send this, so the sender recorded as the
    /// preparer is a coordinator on the bus's word. Every step that can fail happens here, while
    /// the old Timer is still fully live: the plan is checked, the start is chosen, and space for a
    /// whole letter is reserved, so restoring never grows a container (copying a timer's strings
    /// may still allocate). A live Timer refuses the request, and a second request is answered as
    /// the mistake it is.
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
        // Reserved now, so that neither the table, the restore buffer nor the hold grows after the
        // candidate has answered that it is ready; a letter never holds more than kMaxHandoffEntries.
        entries_.reserve(kMaxHandoffEntries);
        restoring_.reserve(kMaxHandoffEntries);
        deferred_.reserve(kMaxDeferredOps);
        startup_ = inherit ? Startup::PreparedRestoration : Startup::Fresh;
        preparer_ = mail.sender();
        prepared_ = Preparation::Accepted;
        mail.answer(TimerCandidatePrepared{p.transaction});
    }

    /// The host's wait has sent the time, `now_ms`, on the wait's clock. Only the host's own
    /// `Waited`, which has no sender, is time; one from any weave changes nothing (TIMER-01). Every
    /// `Waited` the host sends carries the time, so each one starts the timers that were waiting to
    /// learn it and fires every timer that is due.
    void on(const loom::host::Waited& w, loom::Mail& mail) {
        if (mail.sender().valid() || !activation_.activated()) {
            return;
        }
        waited_on_ = w.held;
        if (w.due) {
            ++this->state_.wakes;
            woke_ = true;
        }
        handed(w.now_ms);
        if (bootstrap_ == Bootstrap::Awaiting) {
            // A wake spent waiting for the claim's answer is counted, and it fires nothing.
            if (w.due && claim_open_ && ++bootstrap_wakes_ >= claim_budget_) {
                resolve_bootstrap(mail, /*letter=*/nullptr); // Nobody answered, so start fresh.
            }
            write_letters(mail);
            return;
        }
        anchor_waiting();
        fire_due(mail);
        write_letters(mail);
    }

    void on(const StartTimer& s, loom::Mail& mail) {
        schedule(mail, Op{Op::Kind::Start, mail.sender(), s.id, s.delay_ms, s.repeat, {}, {}, {}});
    }

    void on(const StartRoleTimer& s, loom::Mail& mail) {
        schedule(mail, Op{Op::Kind::StartRole, mail.sender(), s.id, s.delay_ms, s.repeat, s.role,
                          {}, {}});
    }

    /// The ordered ask: a preference, a fallback and a receipt.
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

    /// Loom's request, when this Timer is being replaced, for the letter its successor needs: the
    /// time each timer has left. The letter is written at the time the next wait sends, for which
    /// a time already past is claimed, after the timers due then have fired. It is written at once,
    /// from the last wake, where no wait holds this Timer or its role has left it, as when a
    /// prepared replacement asks after it has committed. Being asked fires, cancels and moves
    /// nothing. The letter goes to the sender the bus named, under the ask's correlation; with
    /// `kMaxLettersHeld` asks already waiting, a further ask is refused in words.
    void on(const loom::PrepareShutdown&, loom::Mail& mail) {
        const LetterAsk asked{mail.sender(), mail.correlation()};
        if (!activation_.activated() || !waited_on_) {
            write_letter(mail, asked);
            return;
        }
        if (letters_due_.size() >= kMaxLettersHeld) {
            mail.send(asked.to,
                      loom::Refused{"this Timer already holds " + std::to_string(kMaxLettersHeld) +
                                    " asks for its letter until the host's wait hands it the "
                                    "time; ask again once one is answered"},
                      asked.correlation);
            return;
        }
        letters_due_.push_back(asked);
        if (!claim_wait(mail)) {
            write_letters(mail); // The role is not this Timer's, so no wait will send it a time.
        }
    }

    /// The steward's answer to this Timer's claim: a letter.
    void on(const loom::Bequest& letter, loom::Mail& mail) {
        if (!answers_our_claim(mail)) {
            return; // Not asked for, out of date, or arriving after the decision was made.
        }
        resolve_bootstrap(mail, &letter);
    }

    /// The steward's other answer: no letter is held for this Timer. It is a real answer, and the
    /// quickest true way to a fresh start.
    void on(const loom::Refused&, loom::Mail& mail) {
        if (!answers_our_claim(mail)) {
            return;
        }
        resolve_bootstrap(mail, /*letter=*/nullptr);
    }

    /// A message this Timer sent that the bus refused, on Loom's own word (`dispatch_refused`).
    /// When it is the claim, nobody stands to answer it, so the Timer starts fresh. When it is a
    /// firing at a weave that is gone, every timer that weave started is dropped, because none of
    /// them can ever be delivered.
    void on(const loom::DispatchRefused& r, loom::Mail& mail) {
        if (!mail.dispatch_refused()) {
            return; // A weave sent this message itself; it is not the bus's refusal.
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
            return; // A role's holder came and went; the role keeps its timers.
        }
        const std::size_t before = entries_.size();
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const Entry& e) { return e.requester == gone; }),
                       entries_.end());
        this->state_.gone += static_cast<std::int64_t>(before - entries_.size());
        this->state_.active = static_cast<std::int64_t>(entries_.size());
    }

    /// Runs after every message this Timer handles. It claims, as its role, the time it waits for
    /// whenever that time changed or a wake was taken. The wait honours a claimed time once, so a
    /// source must claim again after each wake, whether its time changed or not.
    void after_delivery(loom::Mail& mail) {
        if (!activation_.activated()) {
            return;
        }
        if (claimed_ && !woke_ && wanted_until() == claimed_until_) {
            return;
        }
        if (!claim_wait(mail) && !letters_due_.empty()) {
            write_letters(mail); // The role has left this Timer, so no wait will send it a time.
        }
    }

    /// The state a reload carries: the counters, and every standing timer as the letter describes
    /// one, by the time it had left at the last time the wait sent. A deadline on this incarnation's
    /// clock would mean nothing to the next incarnation.
    loom::Value snapshot() const override {
        TimerState carried = this->state_;
        carried.standing = describe_table(entries_.size());
        return loom::to_value(carried);
    }

    /// Restores what the snapshot carried: the whole table or none of it, as a letter is taken.
    /// The next activation counts the restored timers as `inherited`.
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
    /// Loom's word for a delivery to a weave that does not exist (`name_of(NoSuchTarget)`).
    static constexpr const char* kNoSuchTarget = "NoSuchTarget";

    /// One standing timer. An empty `role` means the timer fires back to its requester. `next_due`
    /// is a time on the wait's clock, or none while the timer waits to learn the time: it is then
    /// due `first_in` after the next time the wait sends.
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

    /// Every ask to change the table, in one form, so that an ask held and applied later runs
    /// exactly the code an ask applied at once does.
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

    /// An ask for the letter: whom it is written to, and the correlation it answers under.
    struct LetterAsk {
        loom::WeaveId to{};
        std::uint64_t correlation = 0;
    };

    /// Where this incarnation is in deciding what it inherited. It is awaiting from construction, not
    /// from activation, so an ask that arrives before the activation is held too: it was sent after
    /// the old Timer's letter was taken, and a fresh ask wins over an inherited timer under the
    /// same key.
    enum class Bootstrap { Awaiting, Resolved };

    /// Where this incarnation expects its letter to come from, as it was told, never as it guesses.
    /// `GracefulClaim`: ask the steward by role, and start fresh if nobody answers; this is every
    /// ordinary load, swap and reload. `PreparedRestoration`: a coordinator's request named a
    /// letter coming, and from whom. `Fresh`: told that nothing is carried, it waits for no letter and
    /// takes no late one.
    enum class Startup { GracefulClaim, PreparedRestoration, Fresh };

    /// Whether this incarnation has answered a request to get ready, and how. One request gets one
    /// answer, and a second is answered as the mistake it is.
    enum class Preparation { None, Accepted, Declined };

    /// The time this Timer waits for, as its claim states it. It is a time already past while the
    /// Timer must learn the time: a letter waits to be written, a timer waits to start counting, or
    /// a new Timer is counting wakes for its claim's answer. Otherwise it is the soonest deadline,
    /// or none.
    std::optional<std::int64_t> wanted_until() const {
        if (!letters_due_.empty()) {
            return kTimePast; // A letter waits for the time the next wait sends.
        }
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

    /// Keeps a time the wait sent as the latest time this Timer knows. The wait's clock never goes
    /// back, so an older time changes nothing.
    void handed(std::int64_t now) {
        if (!now_ || now > *now_) {
            now_ = now;
        }
    }

    /// Starts every timer that was waiting to learn the time: each is due `first_in` after the time
    /// the wait just sent.
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

    // ---- deciding what this incarnation inherited -------------------------------

    /// Whether this message answers this Timer's claim. `answers_ask()` comes first: it is Loom's
    /// word, which only the weave that received the claim can earn, and only once. The new Timer
    /// asks by role, so it cannot know the steward's id in advance, and a forged letter would
    /// decide whom future firings reach. Then the correlation must match, and the claim must still
    /// be open: `claim_open_` closes when the decision is made, so a late answer, even a true one,
    /// cannot reopen it.
    bool answers_our_claim(const loom::Mail& mail) const {
        return claim_open_ && mail.answers_ask() && mail.correlation() == kClaimCorrelation;
    }

    /// Says no, as the true answer to the request, using the one answer a readiness would have
    /// used. The coordinator hears a decision rather than waiting out a silence, and the reason is
    /// a sentence that stands alone.
    void decline(loom::Mail& mail, const PrepareTimerHandover& p, std::string why) {
        prepared_ = Preparation::Declined;
        mail.answer(TimerCandidateDeclined{p.transaction, std::move(why)});
    }

    /// The decision of what this Timer inherited, and the only place it is made. Restore the
    /// letter, if there is one; then apply every ask that arrived while the Timer was deciding; and
    /// only then announce that it is serving.
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

    /// Reads the letter. Each item is checked again by Loom (`loom::claim_item`) before any field
    /// is read, so a malformed item, or a letter of another version, gives nothing at all.
    void restore_from(const loom::Bequest& letter) {
        for (const loom::Bytes& item : letter.items) {
            if (const std::optional<TimerHandoff> handoff = loom::claim_item<TimerHandoff>(item)) {
                adopt(*handoff);
                return; // A letter holds one Timer handoff; its other items are not the Timer's.
            }
        }
    }

    /// Takes a letter whole or not at all. A letter over the bound, or with one requester that is
    /// not a weave id written exactly in decimal, gives nothing: an honest predecessor writes
    /// neither, and half of an untrusted letter is a schedule nobody asked for.
    void adopt(const TimerHandoff& handoff) {
        if (handoff.entries.size() > kMaxHandoffEntries) {
            return;
        }
        // A member buffer, reserved when the candidate got ready, so this step does not grow it.
        restoring_.clear();
        this->state_.inherited = static_cast<std::int64_t>(table_from(handoff.entries, restoring_));
        for (Entry& e : restoring_) {
            install(std::move(e));
        }
        restoring_.clear();
    }

    /// The table as the letter describes it, in table order and at most `bound` timers: the time
    /// each has left at the last time the wait sent, and all of its delay for a timer still
    /// waiting to learn the time.
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

    /// Claims, as this Timer's role, the time it waits for, and records the claim either way.
    /// Returns false when the claim is refused because the role is not this Timer's.
    bool claim_wait(loom::Mail& mail) {
        const std::optional<std::int64_t> until = wanted_until();
        loom::host::WaitSet set;
        set.until_ms = until;
        const bool accepted = static_cast<bool>(mail.as_role(kTimerRole).claim(set));
        claimed_ = true;
        claimed_until_ = until;
        woke_ = false;
        return accepted;
    }

    /// Writes the letter to whoever asked for it, under the ask's correlation: the table as it
    /// stands at the time the wait last sent.
    void write_letter(loom::Mail& mail, const LetterAsk& asked) const {
        TimerHandoff handoff;
        // The writer keeps to the same published bound the reader checks.
        handoff.entries = describe_table(kMaxHandoffEntries);
        loom::Bequest letter;
        letter.role = kTimerRole;
        letter.items.push_back(loom::bequeath_item(handoff));
        mail.send(asked.to, letter, asked.correlation);
    }

    /// Writes every letter that was waiting for the time, now that the wait has sent it, in the
    /// order they were asked for.
    void write_letters(loom::Mail& mail) {
        for (const LetterAsk& asked : letters_due_) {
            write_letter(mail, asked);
        }
        letters_due_.clear();
    }

    /// Turns the letter's timers into table entries, each due its remaining time after the next
    /// time the wait sends, and appends them to `out`: all of them, or none when one requester is
    /// not a weave id written exactly in decimal. Returns how many were appended.
    std::size_t table_from(const std::vector<TimerHandoffEntry>& records,
                           std::vector<Entry>& out) const {
        const std::size_t first = out.size();
        for (const TimerHandoffEntry& t : records) {
            const std::optional<loom::WeaveId> who = parse_weave_id(t.requester);
            if (!who) {
                out.resize(first);
                return 0; // A malformed requester: take nothing.
            }
            Entry e{t.id, t.role, *who, effective_delay(t.delay_ms, t.repeat), t.repeat,
                    std::nullopt, std::max<std::int64_t>(t.remaining_ms, 0), false};
            out.push_back(std::move(e));
        }
        return out.size() - first;
    }

    /// Puts a restored timer in the table under its own key, replacing one already there rather
    /// than doubling it, exactly as a second ask would.
    void install(Entry e) {
        if (Entry* existing = find_entry(e.id, e.role, e.requester)) {
            *existing = std::move(e);
            return;
        }
        entries_.push_back(std::move(e));
    }

    /// Every ask to change the table enters here, whatever message carried it: it is held while
    /// the Timer still decides what it inherited, and applied otherwise.
    void schedule(loom::Mail& mail, Op op) {
        if (defer(mail, op)) {
            return;
        }
        apply(mail, op);
    }

    /// Holds an ask while the decision is pending, and returns true when the caller must stop. An
    /// ask past the hold's bound is counted (`deferred_dropped`), and an ordered one is refused
    /// with a receipt.
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

    /// Applies every held ask, in the order they arrived, after the letter is restored and before
    /// `TimerReady`. The order of arrival is what lets a later ask win over an earlier one under
    /// the same key, and applying them after the restore is what lets any ask win over a timer the
    /// letter carried.
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

    // ---- applying an ask ----------------------------------------------------

    void apply(loom::Mail& mail, const Op& op) {
        switch (op.kind) {
        case Op::Kind::Start:
            if (!op.sender.valid()) {
                // No weave asked, so there is nobody to fire at: the ask is dropped and counted.
                ++this->state_.dropped;
                return;
            }
            upsert(op.id, /*role=*/"", op.sender, op.delay_ms, op.repeat);
            return;
        case Op::Kind::StartRole:
            if (op.role.empty()) {
                ++this->state_.dropped; // A role beat with no role is no ask at all.
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

    /// An ordered ask: the asker's preference, what is possible, the choice, and the receipt. A
    /// standing timer matches when it has the same key and means the same: the same repeat and the
    /// same normalized delay. A changed way of addressing the beat finds no timer at all, and a
    /// changed delay or repeat finds the timer but no match. Either way keeping the time is not
    /// possible, and the fallback applies, because "kept" would describe a timer nobody asked for.
    void apply_ensure(loom::Mail& mail, const Op& op) {
        if (!op.sender.valid()) {
            // An ordered ask with no sender has nowhere to send its receipt; a sender that wants no
            // receipt uses the plain messages.
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
            // The timer becomes the asker's, exactly as a plain second ask makes it, so a successor
            // that asks again gains the right to cancel it. The one thing keeping the time does not
            // do is start the delay again.
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

    /// Sends the receipt to the sender the bus named, with correlation 0. An ordered ask is sent as
    /// a message, not as a question, so it has no correlation to echo, and a binding matches the
    /// receipt by the timer's id.
    void answer_order(loom::Mail& mail, const Op& op, const char* resolved, std::string reason) {
        mail.send(op.sender, TimerResolution{op.id, resolved, std::move(reason)});
    }

    /// Fires every counting timer due by the time the wait last sent, in table order.
    void fire_due(loom::Mail& mail) {
        if (!now_) {
            return;
        }
        const std::int64_t now = *now_;
        // An indexed loop, on purpose: firing adds to the bus's queue but never to `entries_`, and
        // no ask can arrive in the middle of this loop, because dispatch runs on one thread.
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
                // A timer that is late by less than its delay keeps its rhythm: its next beat is
                // one delay after the beat it was due. One later than that starts again a delay
                // from now. Either way it never fires twice to catch up, so a host that stalled
                // gets one beat when it resumes, not a flood of stale ones.
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

    /// The key a timer is found by, in one place: the requester and the id for a timer that fires
    /// back to its requester, and the role and the id, whoever asked, for a role timer, so that a
    /// successor replaces its predecessor's beat and gains the right to cancel it. An ordered
    /// ask's question "is there a timer to keep?" and a plain ask's "replace or add?" are the same
    /// question, asked here.
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

    /// The one way an ask writes a timer: it is due `delay` after the next time the wait sends.
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

    /// Cancels every timer `who` started when `id` is null, or its timers with exactly that id. It
    /// matches by the recorded requester: the bus names every sender, so this is who the weave is,
    /// not a name it gives itself.
    void remove_mine(loom::WeaveId who, const std::string* id) {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const Entry& e) {
                                          return e.requester == who &&
                                                 (id == nullptr || e.id == *id);
                                      }),
                       entries_.end());
        this->state_.active = static_cast<std::int64_t>(entries_.size());
    }

    // ---- the arithmetic of time --------------------------------------------
    //
    // The delay rule belongs to `timer.normalize_delay`, and only the call to it is here. Capping a
    // deadline and measuring what is left of one are facts about a clock, so they stay the Timer's.

    /// The delay this Timer schedules for the delay a weave asked for, from `timer.normalize_delay`
    /// (normalize.hpp) and never written out again here. Every path that normalizes a delay, a
    /// plain ask, the comparison an ordered ask makes, and a restored letter, comes through here,
    /// so no timer can be scheduled by any rule but the one this incarnation was built with.
    std::int64_t effective_delay(std::int64_t delay_ms, bool repeat) const {
        return semantics_.effective_delay(delay_ms, repeat);
    }

    /// `now` plus `duration`, capped at the largest value rather than wrapping. A deadline that
    /// wrapped would read as always overdue, and its timer would fire forever.
    static std::int64_t add_clamped(std::int64_t now, std::int64_t duration) {
        constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
        if (duration > 0 && now > kMax - duration) {
            return kMax;
        }
        return now + duration;
    }

    /// The time left until a deadline, with no overflow. A timer that is due or overdue has zero
    /// left, and the subtraction is done unsigned, where it is defined, and capped to the range a
    /// letter can carry.
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

    /// Reads a weave id from a letter, exactly or not at all. Only plain decimal is accepted: no
    /// sign, no spaces, and nothing after the digits.
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

    // These belong to this incarnation and are never part of `TimerState`. Nothing about a claim, a
    // time or a decision in progress may cross a reload, or a new incarnation would believe it was
    // live in ways it never earned.
    zengine::ActivationCursor activation_; ///< the activation this incarnation lives under
    std::optional<std::int64_t> now_;      ///< the latest time the wait sent
    bool claimed_ = false;                 ///< a WaitSet has been claimed since activation
    std::optional<std::int64_t> claimed_until_; ///< the time that claim named
    bool woke_ = false;                         ///< a wake was taken since the last claim
    bool waited_on_ = false; ///< whether the last `Waited` reported this claim held
    std::vector<LetterAsk> letters_due_; ///< asks for the letter that wait for the time, at
                                         ///< most `kMaxLettersHeld`
    Bootstrap bootstrap_ = Bootstrap::Awaiting; ///< open from construction, closed by the decision
    std::int64_t bootstrap_wakes_ = 0;          ///< wakes spent waiting for the claim's answer
    std::int64_t claim_budget_ = kBootstrapWakes; ///< how many wakes this start allows before it gives up
    bool claim_open_ = false;                     ///< a claim is unanswered; it opens at activation
    std::vector<Op> deferred_;                    ///< held while the decision is pending
    /// What a prepared replacement needs. All of it is this incarnation's own, for the same reason as
    /// the rest: nothing about getting ready may cross a reload.
    Startup startup_ = Startup::GracefulClaim; ///< told before admission, never guessed
    Preparation prepared_ = Preparation::None; ///< one request, one answer
    loom::WeaveId preparer_{};                 ///< the request's sender; whose letter to claim
    std::vector<Entry> restoring_;             ///< adopt()'s buffer, reserved when getting ready
    std::int64_t revived_ = 0;                 ///< timers the last revival restored

    /// Whose copy of the delay rule this Timer runs: the catalog of a host that offered one, for
    /// the Timer's whole life, or the catalog this repository builds. Both look up
    /// `timer.normalize_delay` at every call and keep no earlier answer, so the Timer's schedule
    /// and a preview of the rule cannot disagree. A default-constructed one is local, as for a host
    /// that knows nothing of operators.
    DelayAuthority semantics_;
    std::vector<Entry> entries_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_TIMER_WEAVE_HPP
