// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_GESTURE_HPP
#define ZENGINE_FLOW_PANE_GESTURE_HPP
// Two presses Flow reads as one gesture: the window counts a press's clicks by the input role's
// double-click time, and Flow reads the count with the place, never timing a press itself.
#include <cstdint>
#include <string>

namespace zengine::flow_pane {

/// WHAT THE LAST PRESS NAMED, so the next can complete a double click: what it landed on, and where.
struct ClickMemory {
    bool armed = false;
    std::string target;
    std::int64_t x = 0, y = 0;
};

/// IS THIS PRESS THE SECOND OF A DOUBLE CLICK? Pure and total: the window counted it 2 or more
/// (`clicks`), the last press armed, on the same target, within `slack` of where it landed
/// either way.
inline bool doubles(const ClickMemory& prior, const std::string& target, std::int64_t x,
                    std::int64_t y, std::int64_t slack, std::int64_t clicks) noexcept {
    if (clicks < 2 || !prior.armed || prior.target.empty() || prior.target != target) return false;
    const auto close_by = [slack](std::int64_t a, std::int64_t b) { return a - b <= slack && b - a <= slack; };
    return close_by(x, prior.x) && close_by(y, prior.y);
}

} // namespace zengine::flow_pane
#endif
