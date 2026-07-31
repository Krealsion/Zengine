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
//
// R2B-3c builds THREE artifacts from this one source, and they are three real
// separate libraries rather than one loaded twice:
//
//   zengine-timer-virtual       the incumbent (v1)
//   zengine-timer-virtual-v2    the prepared candidate — the SAME service code,
//                               because the artifact that prepares must be the
//                               artifact that goes live
//   zengine-timer-declines      the deliberately broken candidate
//                               (ZENGINE_TIMER_DECLINES): a real artifact that
//                               loads, validates, seals and is asked, and then
//                               refuses to become the Timer
//
// The brokenness lives HERE and not in the package. A `#ifdef` in `timer_weave.hpp`
// would put a test-only branch inside shipped service code; a fixture weave that
// declares the same preparation door and answers it the other way proves the same
// thing — a candidate that COULD have gone live and does not — without the
// package knowing a broken build exists.

#include "timer/timer_weave.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

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

#if defined(ZENGINE_TIMER_DECLINES)

namespace timer = zengine::timer;

/// The deliberately broken candidate.
///
/// It is broken in the ONE way that is hardest to catch and most worth proving:
/// not by failing to load, not by crashing, but by passing every artifact-level
/// check the substrate makes — it opens, its manifest reconstructs, its state
/// gates, it is sealed, and it is asked — and then declining, authentically,
/// with the same one answer right a readiness would have spent.
///
/// It declares the production Timer contract it would have served, so nothing
/// about the refusal is "it was never a plausible Timer". What it must never do
/// is disturb the incumbent, and it has no way to: a sealed candidate speaks
/// only to its coordinator, and the incumbent is never told a transaction
/// exists.
struct BrokenState {
    std::int64_t asked = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(BrokenState, 1, ZEN_FIELD(asked));
};

class DecliningTimerCandidate
    : public loom::WeaveBase<
          DecliningTimerCandidate, BrokenState,
          loom::Accept<loom::Activated, timer::Drive, timer::StartTimer, timer::StartRoleTimer,
                       timer::EnsureTimer, timer::EnsureRoleTimer, timer::CancelTimer,
                       timer::CancelAllMyTimers, loom::PrepareShutdown, loom::Bequest,
                       loom::Refused, timer::PrepareTimerHandover>,
          loom::Emit<timer::TimerFired, timer::TimerReady, timer::Drive, timer::TimerResolution,
                     loom::Bequest, loom::ClaimBequest, timer::TimerCandidatePrepared,
                     timer::TimerCandidateDeclined>> {
public:
    void on(const timer::PrepareTimerHandover& p, loom::Mail& mail) {
        ++state_.asked;
        mail.answer(timer::TimerCandidateDeclined{
            p.transaction, "this build cannot become the Timer and says so rather than "
                           "discovering it after the role has moved"});
    }
    // Every other door is declared and does nothing: a candidate that silently
    // ignored the production contract would pass an isolation proof by being
    // inert rather than by being sealed.
    void on(const loom::Activated&, loom::Mail&) {}
    void on(const timer::Drive&, loom::Mail&) {}
    void on(const timer::StartTimer&, loom::Mail&) {}
    void on(const timer::StartRoleTimer&, loom::Mail&) {}
    void on(const timer::EnsureTimer&, loom::Mail&) {}
    void on(const timer::EnsureRoleTimer&, loom::Mail&) {}
    void on(const timer::CancelTimer&, loom::Mail&) {}
    void on(const timer::CancelAllMyTimers&, loom::Mail&) {}
    void on(const loom::PrepareShutdown&, loom::Mail&) {}
    void on(const loom::Bequest&, loom::Mail&) {}
    void on(const loom::Refused&, loom::Mail&) {}
};

#endif

} // namespace

#if defined(ZENGINE_TIMER_DECLINES)
ZEN_EXPORT_WEAVE(DecliningTimerCandidate)
#else
ZEN_EXPORT_WEAVE(VirtualTimerService)
#endif
