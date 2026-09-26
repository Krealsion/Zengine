// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WEAVELIB_PROBE_VOCABULARY_HPP
#define ZENGINE_TESTS_WEAVELIB_PROBE_VOCABULARY_HPP

// The continuity probe's own vocabulary, a suite fixture. The binding's lifecycle and its last
// receipt are local to the consumer in a real dynamic library, which a test process cannot reach
// into, so the probe is asked by message and answers with what it sees: the door every weave
// offers, and no back channel.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::probe {

/// The timer id the probe declares. One id, so a firing is unambiguous.
inline constexpr const char* kProbeTimerId = "probe.oneshot";

/// "Tell me where you stand." Answered with ProbeReport.
struct AskProbe {
    ZEN_SHAPE(AskProbe, 1);
};

/// The probe's whole visible truth: how often its callback ran, what the Timer last said it did
/// about the binding and why, where the binding stands, and how often its own activation hook ran.
/// That last shows the hook ran on this consumer's activation, after its bindings reconciled, and
/// not again when a Timer replacement republishes `TimerReady` to every consumer: only a counter
/// can say it did not re-run.
struct ProbeReport {
    std::int64_t fires = 0;
    std::string resolved;  ///< the last TimerResolution's outcome ("" if none yet)
    std::string reason;    ///< and its self-contained why
    std::string lifecycle; ///< "waiting" | "spent" | "canceled"
    std::int64_t activations = 0; ///< how many times on_timed_activation ran
    ZEN_SHAPE(ProbeReport, 2, ZEN_FIELD(fires), ZEN_FIELD(resolved), ZEN_FIELD(reason),
              ZEN_FIELD(lifecycle), ZEN_FIELD(activations));
};

/// "Arm it again." The deliberate restart a spent one-shot needs.
struct RestartProbe {
    ZEN_SHAPE(RestartProbe, 1);
};

/// "Stop wanting it." Both halves, through the handle.
struct CancelProbe {
    ZEN_SHAPE(CancelProbe, 1);
};

} // namespace zengine::probe

#endif // ZENGINE_TESTS_WEAVELIB_PROBE_VOCABULARY_HPP
