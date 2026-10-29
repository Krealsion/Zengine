// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_GESTURE_HPP
#define ZENGINE_FLOW_PANE_GESTURE_HPP
// Two presses Flow reads as one gesture: a double click is Flow's own interpretation, time its
// argument (agents/decisions/time-is-an-argument.md), on the interval Workshop's takes.
#include <cstdint>
#include <string>

namespace zengine::flow_pane {

/// How close in time two presses are one double click: Workshop's `kDoubleClickMs`, which
/// `tests/test_workshop_screen.cpp` keeps this equal to, so one gesture means one thing.
inline constexpr std::int64_t kDoublePressMs = 400;

/// WHAT THE LAST PRESS NAMED, so the next can complete a double click: what it landed on, where,
/// and when, in a steady clock's milliseconds that are never kept or sent.
struct ClickMemory {
    bool armed = false;
    std::string target;
    std::int64_t x = 0, y = 0, at_ms = 0;
};

/// IS THIS PRESS THE SECOND OF A DOUBLE CLICK? Pure and total: the last press armed, on the same
/// target, within `slack` of where it landed either way, and within `kDoublePressMs` after it.
inline bool doubles(const ClickMemory& prior, const std::string& target, std::int64_t x,
                    std::int64_t y, std::int64_t slack, std::int64_t now_ms) noexcept {
    if (!prior.armed || prior.target.empty() || prior.target != target) return false;
    const auto close_by = [slack](std::int64_t a, std::int64_t b) { return a - b <= slack && b - a <= slack; };
    if (!close_by(x, prior.x) || !close_by(y, prior.y)) return false;
    return now_ms >= prior.at_ms && now_ms - prior.at_ms <= kDoublePressMs;
}

} // namespace zengine::flow_pane
#endif
