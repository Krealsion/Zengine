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
// HOW THE SERVICE RUNS — the beat chain. Dispatch on this bus is
// single-threaded FIFO and a weave runs only when a message arrives, so the
// service keeps itself alive: each Drive beat is
//
//   announce (first beat only) -> nap to the soonest deadline (capped at
//   kBeatCapMs) -> fire everything due -> re-send Drive to self.
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
// The host's whole obligation is the first Drive (the wind) — after that,
// pumping the bus IS running the world, and the pump breathes at this weave's
// pace because the nap lives inside the beat.
//
// WHEN THE SERVICE GOES AWAY (measured — the trust-gate audit of 2026-07-26,
// probes A/B in tests/test_audit_probes.cpp; the summary lives on Drive in
// vocabulary.hpp):
//   - unloaded or SWAPPED: the in-flight Drive dies with it, and the chain
//     dies too. The mechanism is precise and worth stating exactly, because
//     the obvious guess is wrong: a gated send is authorized by looking its
//     SENDER up at delivery time, so the parked re-wind fails on a dead
//     sender (CapabilityDenied) — NOT on a vacant target. On a swap the
//     successor already holds the role by the time that Drive is delivered;
//     it is simply never allowed to arrive. So the successor gets no first
//     message, never announces, and never beats: time stops when the clock is
//     gone, and stays stopped until something winds it again.
//   - RELOADED: the chain lives. Reload rebinds the new library behind the
//     same adapter and WeaveId, so the sender lookup keeps succeeding and the
//     beat rides straight through. Note what a reload does and does not carry:
//     it constructs a NEW instance and transplants only the ZEN_SHAPE state
//     (TimerState) through the gate, so `beats`/`fired` continue while
//     `entries_` and `announced_` below start fresh. That is why a reloaded
//     service re-announces — and why it must: the standing timers are gone
//     with the old instance, and the hello is what gets them re-asked.
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

#include <zen/weave.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::timer {

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
                             loom::Accept<Drive, StartTimer, StartRoleTimer, CancelTimer,
                                          CancelAllMyTimers>,
                             loom::Emit<TimerFired, TimerReady, Drive>> {
public:
    TimerServiceT() = default;
    explicit TimerServiceT(Clock clock) : clock_(std::move(clock)) {}

    void on(const Drive&, loom::Mail& mail) {
        announce_once(mail);
        std::int64_t now = clock_.now_ms();
        clock_.nap_ms(nap_until_next(now));
        now = clock_.now_ms();
        fire_due(now, mail);
        ++this->state_.beats;
        // Re-wind BY ROLE, not by id: a loaded weave does not know its own
        // bus-side id (self-sends die at delivery), and the role is the more
        // honest address anyway — the beat belongs to "whoever provides time",
        // not to one incarnation of it. A service without its role does not
        // beat; the role is the contract, not a convenience.
        //
        // What role addressing does NOT do is carry the chain across a SWAP:
        // this send is authorized against its SENDER at delivery, so once this
        // incarnation is unregistered the parked Drive is refused and the
        // chain ends there. Reload keeps it (same WeaveId); swap does not.
        // See the header's "WHEN THE SERVICE GOES AWAY".
        mail.send_to_role(kTimerRole, Drive{});
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

    /// One hello per INCARNATION (the skin's `announced_` stance): a plain
    /// member, never state — so a fresh incarnation announces again and the
    /// standing-beat owners re-ask, which is what refills the table a new
    /// instance starts empty (every ask is an upsert; re-asking is idempotent).
    /// Measured: that cascade runs on a RELOAD, which re-announces on the beat
    /// it rides through. It does not run on a swap — a swapped-in service
    /// never gets a first message to announce on, so there is nothing to
    /// re-ask into. Announcing is necessary for succession here; it is not
    /// sufficient, and the missing half is liveness (see the header).
    void announce_once(loom::Mail& mail) {
        if (announced_) {
            return;
        }
        announced_ = true;
        mail.publish(TimerReady{});
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

    bool announced_ = false;
    Clock clock_{};
    std::vector<Entry> entries_;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_TIMER_WEAVE_HPP
