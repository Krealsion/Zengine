#ifndef ZENGINE_TESTS_WEAVELIB_PROBE_VOCABULARY_HPP
#define ZENGINE_TESTS_WEAVELIB_PROBE_VOCABULARY_HPP

// The continuity probe's own small vocabulary — a suite fixture, not a package.
//
// The R2B-0 vertical proof has to watch a real binding, inside a real dynamic
// library, across a real graceful replacement of the Timer. A `.so` cannot be
// reached into from the test process, and the binding's lifecycle state and its
// last receipt are deliberately LOCAL to the consumer incarnation — so the probe
// is asked, by message, and answers with what it sees. That is the same door
// every other weave in this system offers, which is the point: nothing here
// needs a back channel.
//
// It lives under tests/ because it exists only to be measured. The Loom's own
// harness keeps its fixture weaves the same way.

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

/// The probe's whole visible truth: how many times its callback ran, what the
/// Timer last said it did about the binding (and why), and where the binding
/// stands in its own lifecycle.
struct ProbeReport {
    std::int64_t fires = 0;
    std::string resolved;  ///< the last TimerResolution's outcome ("" if none yet)
    std::string reason;    ///< and its self-contained why
    std::string lifecycle; ///< "waiting" | "spent" | "canceled"
    ZEN_SHAPE(ProbeReport, 1, ZEN_FIELD(fires), ZEN_FIELD(resolved), ZEN_FIELD(reason),
              ZEN_FIELD(lifecycle));
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
