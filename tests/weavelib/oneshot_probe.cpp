// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The continuity probe — a real consumer of the timer binding, in a real
// dynamic library, whose whole job is to be watched across a Timer succession.
//
// One source, two libraries (the Loom weavelib pattern), differing only in the
// ORDER the binding carries:
//
//   (default)                    zengine-probe-oneshot
//                                  prefer preserve_remaining, accept restart_delay
//                                  — the binding API's own default
//   PROBE_REQUIRE_PRESERVATION   zengine-probe-required
//                                  prefer preserve_remaining, accept NOTHING
//                                  — the required-preservation order, whose
//                                    unavailability must refuse rather than
//                                    quietly do something else
//
// Everything else is deliberately as ordinary as a consumer gets: one declared
// one-shot, one callback, and no Timer protocol written by hand anywhere. That
// ordinariness is half the proof — continuity is something the PACKAGE
// authored, not something this weave had to know about.

#include "probe_vocabulary.hpp"

#include "timer/binding.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <chrono>
#include <cstdint>
#include <string>

namespace {

namespace timer = zengine::timer;
using namespace zengine::probe;

/// How long the declared one-shot waits. Five seconds — long enough that
/// "restarted from the full delay" and "resumed with two seconds left" could
/// never be mistaken for one another.
constexpr std::int64_t kProbeDelayMs = 5000;

struct ProbeState {
    std::int64_t fires = 0;
    /// v2 (R2B-3c): how many times this consumer's own activation hook ran. In
    /// the state as well as in the report, so it can be read through the poke
    /// door of a weave that is mid-conversation.
    std::int64_t activations = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ProbeState, 2, ZEN_FIELD(fires), ZEN_FIELD(activations));
};

timer::ContinuityOrder probe_order() {
    timer::ContinuityOrder order;
    order.preferred = timer::Continuity::PreserveRemaining;
#if defined(PROBE_REQUIRE_PRESERVATION)
    order.fallback = std::nullopt; // required: nothing else will do
#else
    order.fallback = timer::Continuity::RestartDelay;
#endif
    return order;
}

class OneShotProbe
    : public timer::TimedWeave<OneShotProbe, ProbeState,
                               loom::Accept<AskProbe, RestartProbe, CancelProbe>,
                               loom::Emit<ProbeReport>> {
public:
    /// The whole declaration. Declaring sends nothing — there is no Mail during
    /// construction and there may be no Timer in the process at all; the binding
    /// is reconciled on this incarnation's activation and on every TimerReady.
    OneShotProbe()
        : shot_(timers().once(kProbeTimerId, std::chrono::milliseconds(kProbeDelayMs),
                              &OneShotProbe::on_fire, probe_order())) {}

    /// The one line of ceremony: a derived `on` hides every base `on`.
    using TimedWeave::on;

    /// THE AUTHOR'S OWN ACTIVATION WORK, extended rather than replacing the
    /// binding's (R2A-3's wall, R2B-3c's use of it). It runs after every desired
    /// timer was reconciled, only for an activation the cursor accepted, and —
    /// the part this phase watches — NOT when some other weave's replacement
    /// republishes `TimerReady`.
    void on_timed_activation(const loom::Activated&, loom::Mail&) { ++state_.activations; }

    void on(const AskProbe&, loom::Mail& mail) {
        mail.send(mail.sender(), ProbeReport{state_.fires, shot_.resolution(),
                                             shot_.resolution_reason(), lifecycle(),
                                             state_.activations});
    }

    void on(const RestartProbe&, loom::Mail& mail) { shot_.restart(mail); }
    void on(const CancelProbe&, loom::Mail& mail) { shot_.cancel(mail); }

private:
    /// The firing. Counted, and nothing else — the interesting question is not
    /// what this does but WHEN it runs and how many times.
    void on_fire(const timer::TimerFired&, loom::Mail&) { ++state_.fires; }

    std::string lifecycle() const {
        switch (shot_.state()) {
        case timer::BindingState::Waiting:
            return "waiting";
        case timer::BindingState::Spent:
            return "spent";
        case timer::BindingState::Canceled:
            return "canceled";
        }
        return "unknown";
    }

    Handle shot_;
};

} // namespace

ZEN_EXPORT_WEAVE(OneShotProbe)
