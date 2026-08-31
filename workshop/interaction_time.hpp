// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_INTERACTION_TIME_HPP
#define ZENGINE_WORKSHOP_INTERACTION_TIME_HPP

// WHAT MONOTONIC TIME IS IT NOW, AND NOTHING ELSE (WUX-7).
//
// One gesture in this application is genuinely temporal: two presses are one double-click
// only if the second followed the first closely enough. `input::PointerButton` carries no
// click count and no timestamp -- deliberately, on a wire two backends spell differently --
// so the question "how long ago" has to be answered here, and this header is the whole of
// the answer.
//
// WHAT IT IS NOT, and the absences are the design:
//
//   NOT A TIMER SERVICE.  Nothing is scheduled, nothing fires, nothing is cancelled and
//                         nothing waits. The Timer package exists one floor over and this
//                         is emphatically not a small copy of it -- a reading has no
//                         lifetime to manage, so it needs no owner, no handle and no
//                         reload contract.
//   NOT A CLOCK.          `steady_clock` is monotonic and has no calendar. It cannot say
//                         what day it is, it is never persisted, it crosses no wire and it
//                         is not comparable between two runs of this process.
//   NOT A POLICY.         How long a double-click may take is `kDoubleClickMs`, beside its
//                         one consumer's own state (`ClickMemory`, workshop/screen.hpp);
//                         what an elapsed number of milliseconds MEANS is never decided
//                         here.
//   NOT A SCHEDULE FOR    A presentation that advances on its own would need a repaint with
//   PRESENTATION.         no event behind it, and this application publishes a canvas only
//                         when something happened. Nothing here manufactures one.
//
// THE HOST MAY ANSWER IT INSTEAD (`HostContext::interaction_now`, workshop/weave.hpp), the
// seam `HostContext::frontier` already has: a reading, wired by whoever owns the process,
// spent at the gesture and stored nowhere. A suite hands over a stepped one and the
// interval becomes a thing a case can falsify rather than a thing a case must outrun.

#include <chrono>
#include <cstdint>

namespace zengine::workshop {

/// MILLISECONDS SINCE THE FIRST TIME ANYTHING IN THIS PROCESS ASKED.
///
/// The origin is arbitrary and that is the point: only DIFFERENCES are ever spent, so the
/// zero is wherever this function was first called and no caller may care. It is
/// `steady_clock` rather than `system_clock` because a maker's hand is not affected by the
/// machine's clock being set backwards, and a double-click that stopped qualifying because
/// an NTP daemon stepped the calendar would be the least explicable defect this application
/// could have.
inline std::int64_t interaction_now_ms() noexcept {
    static const std::chrono::steady_clock::time_point origin =
        std::chrono::steady_clock::now();
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - origin)
            .count());
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_INTERACTION_TIME_HPP
