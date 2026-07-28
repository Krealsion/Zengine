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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
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

/// Four honest counters, poke-inspectable like any state: beats lived,
/// firings delivered, timers currently standing, and asks dropped for having
/// no one to answer (a root-sent StartTimer has no requester to fire at).
struct TimerState {
    std::int64_t beats = 0;
    std::int64_t fired = 0;
    std::int64_t active = 0;
    std::int64_t dropped = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(TimerState, 1, ZEN_FIELD(beats), ZEN_FIELD(fired), ZEN_FIELD(active),
              ZEN_FIELD(dropped));
};

template <class Clock>
class TimerServiceT
    : public loom::WeaveBase<TimerServiceT<Clock>, TimerState,
                             loom::Accept<loom::Activated, Drive, StartTimer, StartRoleTimer,
                                          CancelTimer, CancelAllMyTimers>,
                             loom::Emit<TimerFired, TimerReady, Drive>> {
public:
    TimerServiceT() = default;
    explicit TimerServiceT(Clock clock) : clock_(std::move(clock)) {}

    /// The Loom's control door says this incarnation is committed and live.
    /// That is the whole of what it says — so this is where the service decides
    /// what to DO about it, which is: become available, and author one chain.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail.sender(), a.sequence)) {
            return; // invalid, duplicate or replayed: no notice, no chain, nothing
        }
        // A new activation owns a new chain, from serial 0.
        expected_serial_ = 0;
        // Tell the truth about the table rather than assuming: a fresh
        // incarnation's is empty, but activation is not state migration and has
        // no business clearing one that legitimately holds entries.
        this->state_.active = static_cast<std::int64_t>(entries_.size());
        mail.publish(TimerReady{});
        seed_chain(mail);
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
        std::int64_t now = clock_.now_ms();
        clock_.nap_ms(nap_until_next(now));
        now = clock_.now_ms();
        fire_due(now, mail);
        ++this->state_.beats;
        // Guard BEFORE the arithmetic, never after: a wrapped serial would
        // re-issue one this chain has already spent, and a duplicated serial is
        // indistinguishable from a replay. At the boundary the chain simply
        // ends — the beat it is in was real and did its work.
        if (!can_advance_serial(expected_serial_)) {
            return;
        }
        ++expected_serial_;
        // Seeded BY ROLE because a loaded weave cannot address itself; ownership
        // is carried by the key and the serial, not by the addressing.
        mail.send_to_role(kTimerRole,
                          Drive{activation_.sender_text(), activation_.sequence(),
                                expected_serial_});
    }

    void on(const StartTimer& s, loom::Mail& mail) {
        if (!mail.sender().valid()) {
            // No weave asked, so there is no one to fire at. Dropped, counted.
            ++this->state_.dropped;
            return;
        }
        upsert(s.id, /*role=*/"", mail.sender(), s.delay_ms, s.repeat);
    }

    void on(const StartRoleTimer& s, loom::Mail& mail) {
        if (s.role.empty()) {
            ++this->state_.dropped; // a role beat with no role is no ask at all
            return;
        }
        upsert(s.id, s.role, mail.sender(), s.delay_ms, s.repeat);
    }

    void on(const CancelTimer& c, loom::Mail& mail) { remove_mine(mail.sender(), &c.id); }

    void on(const CancelAllMyTimers&, loom::Mail& mail) { remove_mine(mail.sender(), nullptr); }

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

    /// The one write path for asks. Requester timers key by (requester, id);
    /// role timers by (role, id) ACROSS requesters — a successor replaces its
    /// predecessor's beat instead of doubling it, and takes cancel rights.
    void upsert(const std::string& id, const std::string& role, loom::WeaveId requester,
                std::int64_t delay_ms, bool repeat) {
        delay_ms = std::max<std::int64_t>(delay_ms, 0);
        if (repeat) {
            delay_ms = std::max<std::int64_t>(delay_ms, 1);
        }
        const std::int64_t due = clock_.now_ms() + delay_ms;
        for (Entry& e : entries_) {
            const bool same = role.empty() ? (e.role.empty() && e.requester == requester &&
                                              e.id == id)
                                           : (e.role == role && e.id == id);
            if (same) {
                e.requester = requester;
                e.delay_ms = delay_ms;
                e.repeat = repeat;
                e.next_due = due;
                return;
            }
        }
        entries_.push_back(Entry{id, role, requester, delay_ms, repeat, due, false});
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

    // Per-INCARNATION, never TimerState, and that is the design rather than an
    // omission: nothing about a chain may transplant through a reload, or a new
    // instance would inherit liveness it did not author.
    zengine::ActivationCursor activation_; ///< which activation this incarnation lives under
    std::int64_t expected_serial_ = 0;     ///< the one beat this chain will accept next
    loom::WeaveId chain_sender_{};         ///< learned from the seed; the id a beat must carry

    Clock clock_{};
    std::vector<Entry> entries_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_TIMER_WEAVE_HPP
