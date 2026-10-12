// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TIMER_OS_CLOCK_HPP
#define ZENGINE_TIMER_OS_CLOCK_HPP

// The OS clock the shipped Timer is bound to (timer.cpp): monotonic milliseconds and the one nap
// in the running system. On Windows the read is the performance counter and the nap waits on a
// high-resolution waitable timer, so a beat lasts the period it names, not a scheduler tick.
// Timer law: timer/docs/timer-laws.md

#include <cstdint>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#else
#include <time.h>
#endif

namespace zengine::timer {

#if defined(_WIN32)
/// What the last nap asked of Windows: the relative due time, in 100 ns units, and whether it
/// waited on a high-resolution timer. A nap on `Sleep`, where Windows refused that timer, is kept
/// to the scheduler tick whatever it asks.
struct NapAsked {
    std::int64_t due_100ns = 0;
    bool high_resolution = false;
};
#endif

/// The real clock: monotonic milliseconds and a genuine nap. The beat's nap is what paces the
/// whole bus, and it sits behind a replaceable role rather than in a host.
class MonotonicClock {
public:
#if defined(_WIN32)
    MonotonicClock()
        : timer_(::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS)) {
        LARGE_INTEGER frequency{};
        ::QueryPerformanceFrequency(&frequency);
        ticks_per_second_ = frequency.QuadPart;
    }
    ~MonotonicClock() {
        if (timer_ != nullptr) {
            ::CloseHandle(timer_);
        }
    }
    MonotonicClock(MonotonicClock&& other) noexcept
        : timer_(std::exchange(other.timer_, nullptr)),
          ticks_per_second_(other.ticks_per_second_),
          asked_(other.asked_) {}
    MonotonicClock& operator=(MonotonicClock&& other) noexcept {
        if (this != &other) {
            if (timer_ != nullptr) {
                ::CloseHandle(timer_);
            }
            timer_ = std::exchange(other.timer_, nullptr);
            ticks_per_second_ = other.ticks_per_second_;
            asked_ = other.asked_;
        }
        return *this;
    }
    MonotonicClock(const MonotonicClock&) = delete;
    MonotonicClock& operator=(const MonotonicClock&) = delete;
#endif

    std::int64_t now_ms() {
#if defined(_WIN32)
        LARGE_INTEGER count{};
        ::QueryPerformanceCounter(&count);
        const std::int64_t ticks = count.QuadPart;
        return ticks / ticks_per_second_ * 1000 +
               ticks % ticks_per_second_ * 1000 / ticks_per_second_;
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
        LARGE_INTEGER due{};
        due.QuadPart = -ms * 10000; // relative, in 100 ns units
        if (timer_ != nullptr &&
            ::SetWaitableTimerEx(timer_, &due, 0, nullptr, nullptr, nullptr, 0) != FALSE &&
            ::WaitForSingleObject(timer_, INFINITE) == WAIT_OBJECT_0) {
            asked_ = NapAsked{ms * 10000, true};
            return;
        }
        ::Sleep(static_cast<DWORD>(ms));
        asked_ = NapAsked{ms * 10000, false};
#else
        timespec nap{};
        nap.tv_sec = static_cast<time_t>(ms / 1000);
        nap.tv_nsec = static_cast<long>((ms % 1000) * 1000000);
        ::nanosleep(&nap, nullptr);
#endif
    }

#if defined(_WIN32)
    NapAsked last_nap() const { return asked_; }

private:
    HANDLE timer_ = nullptr;
    std::int64_t ticks_per_second_ = 1;
    NapAsked asked_{};
#endif
};

} // namespace zengine::timer

#endif // ZENGINE_TIMER_OS_CLOCK_HPP
