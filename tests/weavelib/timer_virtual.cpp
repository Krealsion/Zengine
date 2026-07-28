// The TimerService, over a VIRTUAL clock — the suite's own loadable Timer.
//
// Identical to the shipped `zengine-timer` in every respect but one: the Clock.
// The scheduled behaviour, the protocol, the letter, the bootstrap and the beat
// chain are all `TimerServiceT` exactly as timer_weave.hpp defines them — this
// file only swaps out the thing that reads a clock and sleeps.
//
// WHY IT EXISTS. The R2B-0 vertical proof is a SEMANTIC claim about durations:
// a five-second one-shot with three seconds elapsed has two seconds left, and
// the successor fires it two seconds later and not five. Proving that on a wall
// clock would mean sleeping for five real seconds and hoping the machine was not
// busy — a slow, flaky test that proves less. Here virtual time advances only
// inside a beat's nap, by exactly the amount the service asked to sleep, so one
// beat is exactly `kBeatCapMs` of virtual time and every duration in the
// scenario is an exact integer nobody had to wait for.
//
// This is the same move the tier-2 FakeClock makes, carried across the library
// seam so the proof can run through the REAL kernel, the REAL Weave Manager, a
// REAL graceful swap, and REAL bequest storage — everything except the sleeping.

#include "timer/timer_weave.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>

#include <cstdint>

namespace {

/// Time that moves only when the service decides to wait for it.
///
/// `nap_ms` returns instantly and books the requested duration as elapsed. A nap
/// of zero or less is the service saying something is already due, and moves
/// nothing — exactly as the real clock declines to sleep in that case.
struct VirtualClock {
    std::int64_t now = 0;

    std::int64_t now_ms() { return now; }

    void nap_ms(std::int64_t ms) {
        if (ms > 0) {
            now += ms;
        }
    }
};

using VirtualTimerService = zengine::timer::TimerServiceT<VirtualClock>;

} // namespace

ZEN_EXPORT_WEAVE(VirtualTimerService)
