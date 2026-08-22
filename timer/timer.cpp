// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The TimerService weave library — time's only door to the OS.
//
// Everything scheduled lives in timer_weave.hpp (pinned by the suite over a
// fake clock); this file is just the real Clock — the monotonic read and the
// nap. They live HERE because this weave is the one participant whose whole
// purpose is time, and no host winds it (TIMER-02, docs/laws/timer-laws.md);
// for everyone else the clock is a message away. Replace this library and the
// system keeps the same vocabulary with someone else's idea of time.
//
// ...AND, SINCE CAT-0, THE ONE OTHER THING AN ARTIFACT DECIDES: which operator
// truth this instance spends. `ZENGINE_OPERATOR_CONSUMER()` at the bottom is
// what makes this image able to RECEIVE a host's operator surface at all, and
// `TimerService`'s constructor is what takes the offer — inside `create()`, the
// first moment the instance exists, which is the only window a scoped offer
// leaves open (OPH-0). A host that offers nothing gets exactly the Timer it got
// before this line existed. See `timer/normalize.hpp` for the two states and
// docs/reference/operator-host.md for the seam.

#include "normalize.hpp"
#include "timer_weave.hpp"
#include "vocabulary.hpp"

#include "operator/host.hpp"

#include <zen/kernel/export.hpp>

#include <cstdint>
#include <cstdio>
#include <exception>

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

/// THE SHIPPED TIMER: the service over the real clock, spending whatever
/// semantic authority this load was offered.
///
/// IT IS A CONSTRUCTOR AND NOTHING ELSE. No state, no override, no second
/// behaviour — `ZEN_EXPORT_WEAVE` builds its weave with `new S()`, and taking an
/// offer needs an argument, so the smallest honest way to say "this artifact's
/// Timer takes the offer" is a default constructor that passes one down. Every
/// door the ABI calls is `TimerServiceT`'s, unchanged and uninterposed.
///
/// AND IT MAY REFUSE TO EXIST. If a host offered an operator surface that cannot
/// serve `timer.normalize_delay` at the signature this Timer was authored
/// against, `DelayAuthority` throws — and the alternative is the one thing CAT-0
/// forbids, a Timer that quietly schedules by its own arithmetic while a host
/// believes it owns the rule. The sentence goes to stderr because `create()`'s
/// contract is a null pointer and a null pointer cannot carry a reason; the
/// throw then travels one frame into `do_create`, which returns that null, and
/// the Kernel refuses the load. The two facts reach a reader together.
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

/// THIS IMAGE CAN RECEIVE AN OPERATOR HOST (OPH-0's one line, CAT-0's use of it).
///
/// It is OPTIONAL for a consumer and it stays optional here: a Loom or Zengine
/// host that never offers anything loads this library exactly as it always did,
/// resolves no extra symbol beyond the one lookup that fails, and gets a Timer
/// with the vocabulary this repository authors. Nothing about an ordinary
/// Timer's behaviour, ABI, dependencies or lifetime moves because this line is
/// here.
ZENGINE_OPERATOR_CONSUMER();
