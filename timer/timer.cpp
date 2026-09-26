// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The TimerService weave library: time's only door to the OS. Scheduling lives in
// timer_weave.hpp (pinned over a fake clock); this is the real Clock -- the monotonic read and
// the nap -- here because this weave's whole purpose is time and no host winds it (TIMER-02). It
// also decides which operator truth this instance spends: its constructor takes the host's offer
// inside `create()`, the only window a scoped offer leaves open (docs/reference/operator-host.md).
// Timer law: docs/laws/timer-laws.md

#include "normalize.hpp"
#include "timer_weave.hpp"
#include "vocabulary.hpp"

#include "operator/host.hpp"
#include "operator/provider.hpp"

#include <zen/kernel/export.hpp>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <time.h>
#endif

namespace {

/// The real clock: monotonic milliseconds and a genuine nap. The one place
/// in the running system that sleeps — the beat's nap is what paces the
/// whole bus, and it sits behind a replaceable role rather than in a host.
struct MonotonicClock {
    std::int64_t now_ms() {
#if defined(_WIN32)
        return static_cast<std::int64_t>(::GetTickCount64());
#else
        timespec ts{};
        ::clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
    }

    void nap_ms(std::int64_t ms) {
        if (ms <= 0) {
            return; // something is already due: fire now, sleep later
        }
#if defined(_WIN32)
        ::Sleep(static_cast<DWORD>(ms));
#else
        timespec nap{};
        nap.tv_sec = static_cast<time_t>(ms / 1000);
        nap.tv_nsec = static_cast<long>((ms % 1000) * 1000000);
        ::nanosleep(&nap, nullptr);
#endif
    }
};

/// The shipped Timer: the service over the real clock, spending whatever authority this load was
/// offered -- a constructor and nothing else, since `ZEN_EXPORT_WEAVE` builds with `new S()`. It
/// may refuse to exist: an offer that cannot serve `timer.normalize_delay` at this Timer's
/// signature throws, the reason goes to stderr (a null from `create()` carries none), and the
/// Kernel refuses the load.
class TimerService : public zengine::timer::TimerServiceT<MonotonicClock> {
public:
    TimerService()
        : zengine::timer::TimerServiceT<MonotonicClock>(MonotonicClock{}, offered_authority()) {}

private:
    static zengine::timer::DelayAuthority offered_authority() {
        try {
            return zengine::timer::DelayAuthority(zengine::op::OperatorHost::offered());
        } catch (const std::exception& e) {
            std::fprintf(stderr, "zengine-timer: %s\n", e.what());
            std::fflush(stderr);
            throw;
        }
    }
};

} // namespace

ZEN_EXPORT_WEAVE(TimerService)

/// This image can receive an operator host. Optional: a host that offers nothing loads this
/// library as it always did and gets the vocabulary this repository authors.
ZENGINE_OPERATOR_CONSUMER();

/// ...and this image supplies a power, `timer.normalize_delay`. Three independent relationships,
/// one artifact: `zen_weave_abi` (it can be the Timer), `zengine_operator_provider` (it supplies
/// the rule) and `zengine_operator_consumer` (it can spend a host's truth). A host mounts the
/// provider first and then offers its resolution to the instance it creates, so the contribution
/// exists before the instance that needs it. The rule's primitives come from whoever supplies
/// them, so another provider for either changes what this Timer schedules without a rebuild.
ZENGINE_OPERATOR_PROVIDER("zengine.timer", zengine::timer::provider_contributions)
