// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_REST_HPP
#define ZENGINE_TIMER_REST_HPP

// The rest between turns: what a host does with the thread once a turn of its bus is over. The
// Timer never sleeps in a handler; at every beat that fired what was due it claims, as its office,
// how long the bus may rest (`TimerRest`). A host reads that claim after each turn and naps until
// it only while the Timer's beat is the one thing queued, so a reply to a firing, a key or an
// answer is delivered in the next turn, not after a nap. A host that never rests is served the
// same, only busier; a Timer that is not loaded claims nothing, and nothing rests.
// Timer law: timer/docs/timer-laws.md (TIMER-02)

#include "timer/vocabulary.hpp"

#include <zen/switchboard/switchboard.hpp>
#include <zen/weave/shape.hpp>

#include <algorithm>
#include <cstdint>

namespace zengine::timer {

/// THE HOST'S REST, over the host's own clock: anything with `now_ms()` and `nap_ms(ms)` reading
/// the same monotonic time the Timer reads (`MonotonicClock`, `timer/os_clock.hpp`, in a process).
template <class Clock>
class Rest {
public:
    explicit Rest(Clock& clock) : clock_(&clock) {}

    /// HOW LONG THIS HOST MAY WAIT BEFORE ITS NEXT TURN, read once after each turn: until the
    /// Timer's claimed deadline, at most `kBeatCapMs`, when its beat is all the bus holds and the
    /// claim is new since the last turn read (so the beat ran in this turn and its successor is
    /// that one queued delivery); 0 otherwise -- something else waits, the claim is a stale office
    /// holder's, or no Timer has claimed.
    std::int64_t quiet_ms(const loom::Switchboard& bus) {
        const loom::SenseReading read =
            bus.observe_office(kTimerRole, TimerRest::zen_name, TimerRest::zen_version);
        if (!read || !read.value.has_value() || read.by.office_claim_is_stale()) {
            return 0;
        }
        const bool fresh = !seen_ || read.by.revision != revision_;
        seen_ = true;
        revision_ = read.by.revision;
        if (!fresh || bus.pending() != 1) {
            return 0;
        }
        const TimerRest claimed = loom::from_value<TimerRest>(*read.value);
        return std::clamp<std::int64_t>(claimed.until_ms - clock_->now_ms(), 0, kBeatCapMs);
    }

    /// REST, IF THE BUS MAY: nap what `quiet_ms` allows. Returns what it napped, 0 for none.
    std::int64_t after_turn(const loom::Switchboard& bus) {
        const std::int64_t ms = quiet_ms(bus);
        if (ms > 0) {
            clock_->nap_ms(ms);
            ++rests_;
        }
        return ms;
    }

    /// How many times this host has rested: each one a wake when it ends.
    std::int64_t rests() const noexcept { return rests_; }

private:
    Clock* clock_;
    bool seen_ = false;
    std::uint64_t revision_ = 0;
    std::int64_t rests_ = 0;
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_REST_HPP
