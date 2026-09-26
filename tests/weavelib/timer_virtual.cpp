// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Timer service over a virtual clock: the shipped `TimerServiceT` in every respect but the
// clock, whose nap books the requested duration as elapsed. Continuity is a claim about durations
// (TIMER-03), so each beat is exactly `kBeatCapMs` of virtual time and every duration an exact
// integer nobody waits for, through the real kernel, swap and bequest. Three libraries from this
// source: the incumbent, the prepared candidate (the same code, since what prepares must be what
// goes live), and a candidate that declines, broken here rather than by a branch in the package.

#include "timer/normalize.hpp"
#include "timer/timer_weave.hpp"
#include "timer/vocabulary.hpp"

#include "operator/host.hpp"

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

/// The suite's Timer, built as the shipped one is built over `MonotonicClock`: the same class but
/// for the clock, so the canonicality witnesses run on virtual time.
class VirtualTimerService : public zengine::timer::TimerServiceT<VirtualClock> {
public:
    VirtualTimerService()
        : zengine::timer::TimerServiceT<VirtualClock>(
              VirtualClock{}, zengine::timer::DelayAuthority(zengine::op::OperatorHost::offered())) {
    }
};

#if defined(ZENGINE_TIMER_DECLINES)

namespace timer = zengine::timer;

/// The deliberately broken candidate, broken the way hardest to catch: it passes every
/// artifact-level check -- opens, reconstructs its manifest, gates its state, is sealed and is
/// asked -- and then declines, authentically, with the one answer right a readiness would spend.
/// It declares the production Timer contract, so the refusal is not "never a plausible Timer",
/// and it cannot disturb the incumbent: a sealed candidate speaks only to its coordinator.
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

/// All three artifacts built from this source declare the consumer surface, for
/// the reason the shipped Timer does: a host that offers nothing loads them
/// exactly as it always did. The declining candidate carries it too and does
/// nothing with it, which is correct -- it is a Timer-shaped artifact whose only
/// deliberate difference is that it refuses the ROLE.
ZENGINE_OPERATOR_CONSUMER();
