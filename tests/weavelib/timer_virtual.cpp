// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The suites' Timer libraries: the shipped `TimerWeave`, built as timer.cpp builds it, so a suite
// that runs its host's wait on a clock that moves only when told (`loom::host::ManualClock`) holds
// every duration exact through the real kernel, swap and bequest (TIMER-03). Three libraries from
// this source: the incumbent, the prepared candidate (the same code, since what prepares must be
// what goes live), and a candidate that declines, broken here rather than by a branch in the
// package.

#include "timer/normalize.hpp"
#include "timer/timer_weave.hpp"
#include "timer/vocabulary.hpp"

#include "operator/host.hpp"

#include <zen/host/wait_set.hpp>
#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/dispatch_refusal.hpp>

#include <cstdint>

namespace {

/// The suite's Timer, built as the shipped one is: the same class, spending what its host offers,
/// handed its time by the suite's host's wait.
class VirtualTimerService : public zengine::timer::TimerWeave {
public:
    VirtualTimerService()
        : zengine::timer::TimerWeave(
              zengine::timer::DelayAuthority(zengine::op::OperatorHost::offered())) {}
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
          loom::Accept<loom::Activated, loom::host::Waited, timer::StartTimer,
                       timer::StartRoleTimer, timer::EnsureTimer, timer::EnsureRoleTimer,
                       timer::CancelTimer, timer::CancelAllMyTimers, loom::PrepareShutdown,
                       loom::Bequest, loom::Refused, loom::DispatchRefused,
                       timer::PrepareTimerHandover, zengine::manual::ShapeDocumentRequested>,
          loom::Emit<timer::TimerFired, timer::TimerReady, timer::TimerResolution,
                     loom::Bequest, loom::ClaimBequest, timer::TimerCandidatePrepared,
                     timer::TimerCandidateDeclined, zengine::manual::ShapeDocumentShown>,
          loom::Claims<loom::host::WaitSet>> {
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
    void on(const loom::host::Waited&, loom::Mail&) {}
    void on(const loom::DispatchRefused&, loom::Mail&) {}
    void on(const timer::StartTimer&, loom::Mail&) {}
    void on(const timer::StartRoleTimer&, loom::Mail&) {}
    void on(const timer::EnsureTimer&, loom::Mail&) {}
    void on(const timer::EnsureRoleTimer&, loom::Mail&) {}
    void on(const timer::CancelTimer&, loom::Mail&) {}
    void on(const timer::CancelAllMyTimers&, loom::Mail&) {}
    void on(const loom::PrepareShutdown&, loom::Mail&) {}
    void on(const loom::Bequest&, loom::Mail&) {}
    void on(const loom::Refused&, loom::Mail&) {}
    void on(const zengine::manual::ShapeDocumentRequested&, loom::Mail&) {}
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
