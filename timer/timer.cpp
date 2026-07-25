// The TimerService weave library — time's only door to the OS.
//
// Everything scheduled lives in timer_weave.hpp (pinned by the suite over a
// fake clock); this file is just the real Clock — the monotonic read and the
// nap that used to live, hard-coded, in the playable host's loop. They moved
// HERE because this weave is the one participant whose whole purpose is time;
// for everyone else the clock is now a message away. Replace this library and
// the system keeps the same vocabulary with someone else's idea of time.

#include "timer_weave.hpp"
#include "vocabulary.hpp"

#include <zen/kernel/export.hpp>

#include <cstdint>

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
/// whole bus, exactly as the host's old nap_10ms() did, one layer lower and
/// behind a replaceable role.
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

using TimerService = zengine::timer::TimerServiceT<MonotonicClock>;

} // namespace

ZEN_EXPORT_WEAVE(TimerService)
