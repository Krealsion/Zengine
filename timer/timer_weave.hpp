// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_TIMER_WEAVE_HPP
#define ZENGINE_TIMER_TIMER_WEAVE_HPP

// The TimerService weave, over an injected Clock (the Input package's Reader
// move, pointed at time). A Clock is anything with:
//
//   std::int64_t now_ms();       // monotonic milliseconds
//   void nap_ms(std::int64_t);   // block for that long (<=0: the clock decides,
//                                //  the real one just declines to sleep)
//
// The real one (timer.cpp) owns the OS's monotonic clock and the one nap the
// whole system is allowed; the suite's fake advances a virtual now and lets
// the test count beats, so every schedule below is pinned without a single
// wall-clock wait.
//
// HOW THE SERVICE RUNS — the beat chain, AUTHORED FROM ITS ACTIVATION.
//
// The law (R2A-2): every successfully activated incarnation establishes exactly
// ONE beat chain. A new activation owns a new chain; stale, duplicate, replayed,
// inherited or foreign Drives cannot establish another.
//
//   zen.Activated -> accept a new activation lineage
//                 -> publish TimerReady (the service is available)
//                 -> seed Drive serial 0
//   each valid Drive -> nap, fire, and seed exactly its one successor
//
// The host does not wind the clock and owes the service nothing, ever. Loading
// the service is what starts time, because the Loom's control door activates a
// freshly committed incarnation and the service authors its chain from that.
//
// A NEW INCARNATION BEGINS UNACTIVATED, and that is load-bearing rather than
// incidental: the activation cursor and the expected serial are plain members,
// never TimerState, so nothing about a chain transplants through a reload. A
// predecessor's queued Drive that reaches the new instance first finds a weave
// that is not activated at all, and is inert.
//
// Dispatch on this bus is single-threaded FIFO and a weave runs only when a
// message arrives, so the service keeps itself alive: each Drive beat is
//
//   nap to the soonest deadline (capped at kBeatCapMs) -> fire everything due
//   -> seed the one successor Drive.
//
// The nap comes BEFORE the firing so a firing is delivered the moment it is
// due, not one nap later; the re-send comes last, which buys exactly one
// thing — the beat's own firings are delivered before the next beat begins.
// It does NOT buy what an earlier version of this comment claimed. Dispatch
// is FIFO, so anything a CONSUMER says in response to a firing is enqueued
// BEHIND the next Drive that was already sitting there: the response is
// handled only after that beat's nap. A reply to a firing therefore waits out
// up to one nap (kBeatCapMs when nothing is due sooner) before anyone hears
// it. That is a known V1 property, not a bug being hidden — the fix is
// delivery on the idle/deadline side of the nap rather than behind it, and
// that is a deferred Loom question, not designed here.
//
// Pumping the bus IS running the world, and the pump breathes at this weave's
// pace because the nap lives inside the beat.
//
// KNOWING ITS OWN BEAT — how a Drive is recognised as ours. A loaded weave has
// no usable `self_`: nothing ever calls `zen_set_self` on the instance inside a
// `.so` (the kernel sets it on the host-side adapter), so this service cannot
// simply compare a stamped sender against its own id. It LEARNS that id instead,
// from the first Drive of a chain it authored, and requires every later beat to
// carry it.
//
// That is sound rather than merely convenient, and the reason is the bus's own
// ordering guarantee: dispatch is single-threaded, FIFO and non-reentrant. The
// activation key does not exist for anyone until the activation is DELIVERED
// here, and this handler enqueues its seed before any other weave can run — so
// the first Drive bearing the current key is necessarily our own. Anything a
// third party queued earlier arrives before the activation (when we are not
// activated, or still on the old key) and is ignored; anything it queues later
// is behind our seed and fails the serial check.
//
// It is NOT authentication, and the distinction is worth keeping sharp: it
// establishes which sender owns this chain, not that that sender is trustworthy.
// Every weave in this process is trusted code today.
//
// WHEN THE SERVICE GOES AWAY. The substrate behaviour here is unchanged and
// still measured (the trust-gate audit of 2026-07-26, probes A/B) — what changed
// in R2A-2 is that its consequences are no longer the whole story:
//   - unloaded or SWAPPED: the in-flight Drive still dies with its sender. The
//     mechanism is precise and the obvious guess is wrong: a gated send is
//     authorized by looking its SENDER up at delivery, so the parked beat fails
//     on a dead sender (CapabilityDenied) — NOT on a vacant target; on a swap
//     the successor already holds the role by then. The old chain therefore ends
//     honestly. What is new is that the successor does not need it: it is
//     activated, and authors a chain of its own.
//   - RELOADED: the same WeaveId survives, so the predecessor's parked Drive is
//     still deliverable — and it is now INERT anyway, because the new instance
//     begins unactivated and the old serial is not one it expects. Liveness
//     comes from the reload's own activation, not from an inherited beat.
//     Reload constructs a NEW instance and transplants only the ZEN_SHAPE state
//     (TimerState) through the gate, so `beats`/`fired` continue while
//     `entries_`, the activation cursor and the expected serial start fresh —
//     which is exactly why the new activation republishes TimerReady: the
//     standing timers went with the old instance and the notice is what gets
//     them re-asked.
//
// ---------------------------------------------------------------------------
// R2B-0 — WHAT SURVIVES, AND WHO DECIDED. Death is universal; inheritance is
// AUTHORED. The Loom hands this weave the replacement moment (zen.PrepareShutdown)
// and carries the envelope (zen.Bequest / zen.ClaimBequest, weave/lifecycle.hpp);
// everything about WHAT crosses is this package's own decision, said in this
// package's own words.
//
// THE PREDECESSOR, asked to write. It reads its clock ONCE, converts every
// active entry's absolute due time into a REMAINING DURATION, and hands that
// over as one bequest item. It fires nothing, cancels nothing and advances
// nothing: being asked to write a letter is not an event in a schedule's life,
// and a predecessor that mutated itself while describing itself would be
// describing something else.
//
// THE SUCCESSOR, deciding what it inherited. Ordering is the whole of it:
//
//     zen.Activated -> accept a lineage
//                   -> ask the steward, by role, for a letter (ClaimBequest)
//                   -> seed Drive serial 0                 [BOOTSTRAP]
//     bootstrap beat 0 -> nothing is known yet; seed the next beat
//     the answer (Bequest | Refused) -> restore or start fresh
//                   -> replay whatever arrived while we were deciding
//                   -> publish TimerReady
//                   -> the chain continues as ordinary beats
//     bootstrap beat 1 with no answer -> there was no steward; start fresh,
//                   the same way, and continue
//
// TimerReady MAY NOT BE PUBLISHED BEFORE THAT DECISION, and the reason is
// mechanical rather than aesthetic: TimerReady is what makes every standing
// consumer re-ask, and a consumer that re-asks before restoration finds nothing
// to preserve and re-anchors its schedule — silently converting "two seconds
// left" into "five seconds from now". Announcing early does not merely look
// untidy; it destroys the very progress the letter carried.
//
// WHY THE BOOTSTRAP IS EXACTLY TWO BEATS, derived from the bus's own ordering
// rather than tuned. Dispatch is single-threaded FIFO and every send enqueues at
// the TAIL, so a graceful replacement's queue reads (Q1 first):
//
//   Q1 zen.Activated  -> successor      (the door sends this BEFORE it answers
//   Q2 zen.Result     -> steward         the operator, so the activation is
//   Q3 ClaimBequest   -> steward         already queued when "loaded" is heard)
//   Q4 Drive serial 0 -> successor      | Q3/Q4 are enqueued by Q1's handler
//   Q5 Bequest        -> successor      | Q5 by Q3's; the steward learned the
//   Q6 Drive serial 1 -> successor      | heir's id at Q2, one turn earlier
//
// The claim's answer therefore lands at Q5 — AFTER the first beat and BEFORE the
// second. One beat would resolve fresh while the letter was still in flight;
// three would cost a queue turn that can never carry news. Two is the count the
// ordering produces, and it is a count of QUEUE TURNS, not milliseconds: there
// is no wall-clock timeout, no spin, and no permanent dependency on a steward
// existing at all (a direct control-door load with no Manager simply reaches
// beat 1 unanswered and starts fresh). A bootstrap beat naps for nothing and
// fires nothing — it exists to spend a turn — but it IS a beat of the one chain
// and is counted as one.
//
// ONCE RESOLVED, ALWAYS RESOLVED. A late, duplicated or forged letter arriving
// after the decision changes nothing. The consumer obligation here is
// correlation plus one-shot; the stamped-sender half is honestly WAIVED for the
// same reason snake's heir waives it — an heir reaches the steward BY ROLE
// precisely because it cannot know the steward's id, so it cannot pre-bind the
// answer's sender. In-process peers are trusted-by-declaration at this tier
// (B1 ground); that is named here, not hidden.
//
// WHAT RELOAD DOES *NOT* DO, said before anyone assumes otherwise. Reload-in-
// place does not run the graceful ceremony at all — no PrepareShutdown, no
// letter — and the schedule table is deliberately not part of TimerState, so a
// reload starts with an empty table exactly as a replacement does. For
// continuity purposes RELOAD IS A FRESH SERVICE: the default order falls back to
// restart, a required preservation refuses, and nothing here claims otherwise.
// Moving schedule progress into reload-transplanted state is a possible future
// design and is recorded as one, never as an accidental promise.
//
// ---------------------------------------------------------------------------
// R2B-3c — CROSSING A PREPARED REPLACEMENT. The moving-state problem, and where
// this package put the boundary.
//
// A graceful replacement is one ceremony a few queue turns long, and R2B-0's
// letter describes the schedule at the instant the incumbent is asked. A
// PREPARED replacement is deliberately not like that: the candidate is loaded,
// contract-checked and readied while the incumbent stays COMPLETELY LIVE, so
// between "the candidate could serve" and "the candidate does serve" the
// incumbent's clock advances, timers fire, repeats re-arm, and consumers start
// and cancel schedules. Any snapshot taken during preparation is stale before it
// is used, and a successor restoring one would be lying about where time was.
//
// THE BOUNDARY IS THE ADMISSION ITSELF, and the reason is that it is the only
// instant that is simultaneously the last moment the incumbent owns anything and
// the first moment the candidate owns everything. Two facts make it work, and
// NEITHER of them is a Timer mechanism — both are the substrate's:
//
//   1. THE BEAT CHAIN RIDES THE ROLE. Every Drive is `send_to_role(kTimerRole)`,
//      resolved at delivery. The instant admission moves the role, the
//      incumbent's parked beat resolves to the CANDIDATE, which refuses it (a
//      different activation key), and the incumbent never beats again. Its clock
//      stops advancing, nothing more fires, and no old TimerReady or TimerFired
//      can leak as new production — not because anything parked it, but because
//      a chain addressed to a slot ends when the slot moves.
//   2. THE INCUMBENT IS SEALED FOR RETIREMENT IN THE SAME BREATH. Ordinary sends
//      to it become NoSuchTarget and role traffic goes elsewhere, so the only
//      weave that can still reach it is the coordinator. Its schedule table is
//      therefore FROZEN at the boundary — structurally, by the Loom.
//
// So the letter is written AFTER the admission, by a service that has already
// been made incapable of changing, and it describes exactly the boundary state:
// every operation ordered before the admission was applied by the incumbent
// (FIFO), and every operation ordered after it reaches the candidate. Nothing is
// lost, nothing applies twice, and the exchange that produces the letter is the
// unchanged R2B-0 one — `zen.PrepareShutdown` -> `TimerHandoff` — so this
// package still has exactly ONE interpretation of schedule progress.
//
// THE INCUMBENT IS NEVER TOLD, and that is the whole abort story. No preparation
// message reaches it, no state is parked, nothing is reserved on its behalf. A
// candidate that dies, refuses, or exhausts its budget therefore cannot reset,
// duplicate or orphan the incumbent's clock: a failed attempt leaves untouched a
// service it never touched. There is no "resume" to get wrong.
//
// WHAT THE CANDIDATE DOES, and it is a THIRD startup mode, declared and never
// inferred (`Startup`):
//
//   Activated -> claim by ID from the PREPARER (not by role from the steward)
//             -> seed Drive serial 0                    [PREPARED BOOTSTRAP]
//   ...beats spend turns, holding every operation...
//   the answer (Bequest) -> restore -> replay held ops -> publish TimerReady
//   kPreparedClaimBeats with no answer -> the promise was not kept; start fresh,
//             replay everything held, and let a required preservation be
//             REFUSED rather than quietly restarted
//
// The claim goes to a KNOWN ID because the candidate learned its preparer from
// the ask's bus-stamped sender, which makes this strictly stronger than the
// graceful claim: there, the heir must reach the steward BY ROLE because it
// cannot know the steward's id, and the answer is trusted purely on Loom's
// attestation. Here the ask itself names its recipient, and the attestation
// confirms it.
//
// WHAT READINESS MEANS HERE, said before anyone reads more into it. It means
// every FALLIBLE step is complete and the bounded capacity a full letter could
// ever need is reserved — not that the schedule is already restored, which is
// impossible before the boundary exists. What is left after readiness is
// bounded (`kMaxHandoffEntries`), deterministic (the same `adopt` every other
// path uses), and incapable of making a committed Timer unavailable: if the
// letter never comes the service starts fresh and says so, rather than holding
// forever.
//
// SAID EXACTLY, because "preallocated" is easy to over-read: what preparation
// reserves is the CAPACITY of the table, the restore buffer and the hold, so
// none of them has to grow when the letter lands. It does not make restoration
// allocation-free — an entry's id and role are strings, and copying them
// allocates. The claim is that restoration cannot fail for want of room this
// weave could have arranged in advance, not that it touches no allocator.
//
// WHAT THE PREPARATION DOOR'S AUTHORITY ACTUALLY IS, named rather than implied.
// It is THE SEAL, and the seal is the Loom's: a sealed candidate can be reached
// only by the coordinator preparing it, so the `mail.sender()` this weave writes
// down as its preparer is a coordinator BECAUSE THE BUS SAID SO. This weave
// cannot ask whether it is sealed, and so cannot verify that itself. Refusing an
// ask once an activation has been accepted is what closes the ordinary road: a
// freshly loaded Timer's `zen.Activated` is enqueued by the control door inside
// the delivery that registered it, so nothing a third party sends afterwards can
// arrive first, and its WeaveId did not exist to be addressed before that. What
// remains is a host that REGISTERS a Timer and never activates it — there is no
// such path in this tree — and the day a coordinator is itself an untrusted
// loaded weave. The harm if it were reachable is exactly the one R2B-0 already
// named for a forged bequest: a letter names the requesters future firings are
// addressed to. Same B1-tier trusted-in-process ground, same real answer (a
// Loom-tier authenticated claim), and no wider than before — the prepared claim
// is addressed to a KNOWN id, where the graceful one must ask a role.
//
// CLEANUP, honestly. "Cancel a dead requester's timers" wants the service to
// SEE death, and a weave cannot: the bus shows a sender no delivery outcomes
// and broadcasts no unloads. So V1 tells the truth instead of pretending:
//   - a requester-addressed timer whose weave is gone fires into a clean
//     NoSuchTarget refusal (weave ids are never reused, so it can never hit a
//     stranger); a repeating one keeps doing so until cancelled or the
//     service is replaced — bounded noise, pinned in the suite, and the
//     demo's standing beats don't take this path at all;
//   - the beats that must survive replacement are ROLE-addressed, where
//     "requester death" is a non-event by construction;
//   - polite weaves cancel on their way out (CancelAllMyTimers).
// The day the steward speaks about lifecycle (a Manager unload notice), the
// service accepts one more shape and this note shortens.

#include "vocabulary.hpp"

#include "activation/activation.hpp"

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

/// May a chain advance from `serial` to its successor?
///
/// A serial is a finite signed integer, so a chain has a last representable
/// beat. At that beat the chain STOPS rather than wrapping or re-issuing one it
/// has already spent — a duplicated serial would be indistinguishable from a
/// replay and would fork time, which is precisely what this phase exists to
/// prevent.
///
/// PROOF LEVEL, stated honestly: this predicate is pinned DIRECTLY, and the
/// behavioural path through it is **true by construction, not reachable by any
/// test** — arriving at the boundary would take 2^63 beats. The serial is a
/// per-incarnation plain member by design (nothing about a chain transplants),
/// so unlike the Loom's activation sequence there is no revival path a test
/// could use to place a chain near its end. Extracting the guard is what makes
/// the boundary assertable at all.
inline constexpr bool can_advance_serial(std::int64_t serial) {
    return serial >= 0 && serial < std::numeric_limits<std::int64_t>::max();
}

/// Six honest counters, poke-inspectable like any state: beats lived, firings
/// delivered, timers currently standing, asks dropped for having no one to
/// answer (a root-sent StartTimer has no requester to fire at), operations
/// dropped for arriving during bootstrap with the hold already full, and
/// entries actually restored from a predecessor's letter.
///
/// `inherited` is the one number that says what crossed death, and it is here
/// rather than in a log because "what actually survived" is a question a
/// console, a suite, or a curious operator must be able to ask a running
/// service. It counts entries adopted at the LAST bootstrap, not a running
/// total: a service that started fresh reads 0 and means it.
///
/// v2: `deferred_dropped` and `inherited` joined the shape. `zen.TimerState` v1
/// meant the four counters and still does, forever — the immutable-published-
/// schema rule, paid as usual. (Crossing from a v1 artifact to this one is
/// REPLACEMENT, not reload, for this reason and for the accept-set change; see
/// the vocabulary header.)
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

    /// The Loom's control door says this incarnation is committed and live.
    /// That is the whole of what it says — so this is where the service decides
    /// what to DO about it, which since R2B-0 is: find out what it inherited,
    /// and only then become available.
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
        // The bootstrap opens here and closes at the continuity decision. No
        // TimerReady yet — see the header: announcing before restoring is what
        // makes a consumer re-anchor the very schedule the letter carried.
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
            claim_open_ = true;
            claim_budget_ = kBootstrapBeats;
            mail.send_to_role(loom::kManagerRole, loom::ClaimBequest{kTimerRole},
                              kClaimCorrelation);
            seed_chain(mail);
            return;
        }
    }

    /// "Be ready to become the Timer." The preparation ask, which only ever
    /// arrives through the coordinator-only door of a SEALED candidate.
    ///
    /// Everything fallible about becoming this role happens here, while the
    /// incumbent is still completely live and nothing in the world can be
    /// disturbed by a refusal: the plan is validated, the startup mode is
    /// chosen, the preparer is remembered, and the bounded capacity a full
    /// letter could ever need is RESERVED — so restoration after admission does
    /// not have to GROW any container. It is not allocation-free: copying an
    /// entry's id and role strings may still allocate. The claim is about the
    /// bounded, fallible part being paid while a refusal is still harmless.
    ///
    /// TWO REFUSALS THAT ARE NOT ABOUT THE PLAN, and both are about identity
    /// rather than content:
    ///   - a LIVE incarnation is not a candidate. Once an activation has been
    ///     accepted this weave is somebody's Timer, and a stray preparation ask
    ///     must not be able to re-point its bootstrap or reserve on its behalf.
    ///   - ONE ASK, ONE ANSWER. A transaction has exactly one preparation
    ///     conversation; a second ask to the same incarnation is answered as the
    ///     mistake it is rather than silently re-preparing.
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
        // The reservation, and it is what makes readiness honest: a letter can
        // never carry more than kMaxHandoffEntries, so a table with room for
        // that many never has to GROW when the letter finally lands. The hold is
        // reserved for the same reason — everything between admission and
        // restoration is held, and running out of room there would be a fallible
        // step happening after readiness was claimed. It does not make
        // restoration allocation-free; copying an entry's id and role does
        // allocate. See the header.
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
            // The seed came home. Whatever the bus stamped on it is this
            // incarnation's own id — the only self-knowledge available to a
            // loaded weave — and every later beat must carry it.
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

    /// "You are being replaced. Say what you want your heir to know."
    ///
    /// This service says the one thing a successor cannot reconstruct by being
    /// asked again: HOW FAR EACH SCHEDULE HAS GOT. Intent comes back on its own
    /// (every consumer re-declares what it wants); progress does not.
    ///
    /// The clock is read ONCE, so every entry in one letter is described
    /// against one instant — two reads would let a slow letter drift against
    /// itself. Nothing is fired, cancelled or advanced: being asked to describe
    /// a schedule is not an event in that schedule's life. The answer goes to
    /// the STAMPED SENDER (the steward that asked) echoing the correlation;
    /// PrepareShutdown arrives via send, so reply_to is deliberately unset.
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

    /// ONE internal spelling for every schedule operation, whatever shape
    /// carried it and whenever it is performed.
    ///
    /// That single representation is the point rather than a convenience: a
    /// held-and-replayed operation goes through EXACTLY the code a live one
    /// does, so "an operation that waited out the bootstrap means the same
    /// thing" is structural instead of a claim two code paths have to keep
    /// agreeing on.
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

    /// Where this incarnation is in deciding what it inherited. It begins
    /// AWAITING at construction — not at activation — so an operation that
    /// arrives before the activation is delivered is held too. Such an
    /// operation was still sent after the fork point, and the phase's rule is
    /// that a fresh request beats inherited state for the same key; applying it
    /// early would let the letter overwrite it.
    enum class Bootstrap { Awaiting, Resolved };

    /// WHERE THIS INCARNATION EXPECTS ITS PAST TO COME FROM, and it is a
    /// DECLARED fact rather than one inferred from what happens to arrive.
    ///
    ///   GracefulClaim       the default and the unchanged one: ask the steward,
    ///                       by role, and start fresh if nobody answers. Every
    ///                       ordinary load, hard swap and reload takes this.
    ///   PreparedRestoration a coordinator told this candidate, before it was
    ///                       admitted, that a letter is coming from the service
    ///                       it replaces — and which weave will hand it over.
    ///   Fresh               a coordinator told this candidate that nothing is
    ///                       being carried. It waits for no letter and refuses a
    ///                       late one, so "prepared" never silently means
    ///                       "restoring".
    ///
    /// Inferring the middle one from a nonempty table would collapse three
    /// different promises into one code path, and the day they diverged nobody
    /// could say which had run.
    enum class Startup { GracefulClaim, PreparedRestoration, Fresh };

    /// Whether this incarnation has answered a preparation ask, and how. One ask
    /// gets one answer; a second is answered as the mistake it is.
    enum class Preparation { None, Accepted, Declined };

    /// Is this beat the one this chain is waiting for? All four terms, and any
    /// one of them failing means the Drive is ignored entirely.
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
        // The chain's sender, learned from its own seed (see the header). Until
        // it is learned, the seed itself is the only Drive that can pass the
        // three checks above, and FIFO dispatch makes that seed necessarily ours.
        //
        // PROOF LEVEL, measured and stated rather than assumed: this term is
        // DEFENSE IN DEPTH and is **true by construction, not pinned**. Removing
        // it was mutated and the suite stayed green — deliberately reported
        // rather than papered over. The reason is instructive: the serial is a
        // single counter, so an honoured foreign beat does not FORK the chain,
        // it DISPLACES the real one (the genuine next beat then fails the serial
        // check and is dropped). Chain count, beat count and virtual time all
        // read identically, so no instrument here can tell the two apart. The
        // three checks above are what make the chain single and correct; this
        // one closes a seizure vector that is currently benign, and it stays
        // because it is cheap, correct, and the thing that would matter first if
        // a beat ever carried authority.
        return !chain_sender_.valid() || mail.sender() == chain_sender_;
    }

    /// Send the chain's first beat and adopt the identity it comes back with.
    void seed_chain(loom::Mail& mail) {
        mail.send_to_role(kTimerRole,
                          Drive{activation_.sender_text(), activation_.sequence(), 0});
    }

    /// Seed exactly this beat's one successor.
    ///
    /// Guard BEFORE the arithmetic, never after: a wrapped serial would re-issue
    /// one this chain has already spent, and a duplicated serial is
    /// indistinguishable from a replay. At the boundary the chain simply ends —
    /// the beat it is in was real and did its work.
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

    /// Does this answer OUR claim? Three terms, and R2B-1 added the first —
    /// which is the one that turns "probably the steward" into "the steward".
    ///
    ///   1. LOOM'S WORD. `answers_ask()` is a delivery fact the bus sets on the
    ///      one authorized answer to a request this incarnation actually sent.
    ///      Only the weave that received our ClaimBequest can produce it, and
    ///      only once. This is what the heir could not have before: it reaches
    ///      the steward BY ROLE precisely because it cannot know the steward's
    ///      id, so it could not pre-bind the answer's sender — and a shape plus
    ///      a public correlation is exactly what any weave holding the same
    ///      grant can also produce. For THIS letter the gap was load-bearing: a
    ///      forged handoff names the identities future firings are addressed to.
    ///   2. OUR CONVERSATION. The correlation Loom copied from our own claim.
    ///   3. ONE-SHOT. `claim_open_` closes at the decision, so a late or
    ///      duplicated answer — even a genuine one — cannot reopen or replace a
    ///      resolved bootstrap.
    bool answers_our_claim(const loom::Mail& mail) const {
        return claim_open_ && mail.answers_ask() && mail.correlation() == kClaimCorrelation;
    }

    /// Say no, authentically — spending the same one answer right a readiness
    /// would have spent, so a coordinator hears a verdict rather than a silence
    /// it has to time out. The reason is self-contained: a stranger reading it
    /// off the wire must be able to tell what was refused and why.
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

    /// Read the letter. Every item is re-admitted through the real gate before a
    /// field is touched (loom::claim_item), so an item that is malformed,
    /// truncated, or simply somebody else's shape is a clean nothing rather than
    /// a misread — and a handoff written to a DIFFERENT VERSION of the shape is
    /// exactly that case, answered by the one validator instead of by a label
    /// this weave chose to trust.
    void restore_from(const loom::Bequest& letter) {
        for (const loom::Bytes& item : letter.items) {
            if (const std::optional<TimerHandoff> handoff = loom::claim_item<TimerHandoff>(item)) {
                adopt(*handoff);
                return; // one Timer handoff per letter; the rest is not ours
            }
        }
    }

    /// A LETTER IS ADOPTED WHOLE OR NOT AT ALL, and that is the single explicit
    /// rule this side of the gap runs on.
    ///
    /// Over the published bound, or carrying one entry whose requester is not a
    /// lossless decimal weave id, and nothing is taken. The reasoning is the
    /// gate's own: an honest predecessor cannot produce either, so such a letter
    /// is untrusted input rather than a large truth — and adopting the half of
    /// an untrusted letter that happens to parse is worse than starting fresh,
    /// because it produces a schedule nobody authored.
    void adopt(const TimerHandoff& handoff) {
        if (handoff.entries.size() > kMaxHandoffEntries) {
            return;
        }
        const std::int64_t now = clock_.now_ms();
        // A MEMBER BUFFER, NOT A LOCAL, and only because of prepared
        // replacement: a candidate reserves it during preparation, so the one
        // step that happens AFTER it answered "ready" does not have to grow it.
        // Not allocation-free — copying each entry's id and role still may
        // allocate; what is reserved is the container capacity. Every other path
        // reaches this with an empty unreserved buffer and behaves exactly as it
        // did with a local — the bound is the same either way.
        restoring_.clear();
        for (const TimerHandoffEntry& t : handoff.entries) {
            const std::optional<loom::WeaveId> who = parse_weave_id(t.requester);
            if (!who) {
                restoring_.clear();
                return; // malformed: adopt nothing
            }
            const std::int64_t remaining = std::max<std::int64_t>(t.remaining_ms, 0);
            restoring_.push_back(Entry{t.id, t.role, *who, clamp_delay(t.delay_ms, t.repeat),
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

    /// Hold an operation while the continuity decision is pending. Returns true
    /// iff the caller must stop here (held, or refused for overflow).
    ///
    /// Overflow is VISIBLE both ways it can be: counted on `deferred_dropped`
    /// for anyone inspecting the service, and answered with a `refused` receipt
    /// for an ORDERED request, which by definition has somewhere to hear one.
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

    /// The order model, resolved: request -> available menu -> chosen -> receipt.
    ///
    /// MATCHING, DEFINED ONCE AND PINNED. A standing entry matches an order when
    /// it has the same UPSERT KEY — (requester, id) for the requester form,
    /// (role, id) for the role form — AND the same schedule meaning: the same
    /// repeat mode and the same clamped delay. The key is what makes it the same
    /// timer; the meaning is what makes preserving it honest. An order that
    /// changes the addressing mode has a different key by construction and so
    /// finds nothing, and an order that changes the delay or the repeat mode
    /// finds the entry but not a match — both resolve as UNAVAILABLE and go to
    /// the fallback, because calling either of them "preserved" would be
    /// describing a schedule nobody asked for.
    void apply_ensure(loom::Mail& mail, const Op& op) {
        if (!op.sender.valid()) {
            // An order with no stamped requester has nowhere to send its
            // receipt, and an unreported resolution is the exact thing the
            // ordered form exists to prevent. The raw shapes remain the
            // fire-and-forget door for a caller that truly wants no answer.
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
        const std::int64_t delay = clamp_delay(op.delay_ms, op.repeat);
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

    /// The receipt, to the stamped requester. Correlation 0: a resolution is an
    /// ANSWER to an order, and the ordered shapes are sent, not forwarded, so
    /// there is no asker's correlation to echo. The consumer obligation covers
    /// it — a binding matches on the timer id it declared.
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

    /// THE UPSERT KEY, in one place. Requester timers key by (requester, id);
    /// role timers by (role, id) ACROSS requesters — a successor replaces its
    /// predecessor's beat instead of doubling it, and takes cancel rights.
    ///
    /// Extracted so that "matching" means exactly one thing: the ordered form's
    /// availability question and the raw form's replace-or-insert question are
    /// the same question, asked once.
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
        const std::int64_t delay = clamp_delay(delay_ms, repeat);
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

    // ---- small total arithmetic ---------------------------------------------

    /// A repeating delay below 1ms is a hot spin wearing a timer's clothes; a
    /// negative delay fires on the next beat.
    static std::int64_t clamp_delay(std::int64_t delay_ms, bool repeat) {
        delay_ms = std::max<std::int64_t>(delay_ms, 0);
        return repeat ? std::max<std::int64_t>(delay_ms, 1) : delay_ms;
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

    /// How long until this deadline, WITHOUT UNDERFLOW. A due or overdue entry
    /// transfers with zero remaining — it is due, and the successor should treat
    /// it as due rather than inherit a negative number that means nothing. The
    /// subtraction is done in unsigned arithmetic (defined, modular) so that even
    /// an absurd clock cannot produce undefined behaviour, and the result is
    /// saturated into the signed range the wire carries.
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

    // Per-INCARNATION, never TimerState, and that is the design rather than an
    // omission: nothing about a chain may transplant through a reload, or a new
    // instance would inherit liveness it did not author. The same is true of the
    // bootstrap: a new incarnation begins not knowing what it inherited, and
    // finds out by asking.
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
    std::vector<Entry> entries_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_TIMER_WEAVE_HPP
