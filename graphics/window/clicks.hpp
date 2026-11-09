// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_CLICKS_HPP
#define ZENGINE_GRAPHICS_WINDOW_CLICKS_HPP

// How a window counts the clicks of a hand's presses: by the double-click time the input role
// says (`zengine::input::PointerTimingSaid`), time its argument, each hand on a counter of its own.

#include "input/vocabulary.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>

namespace zengine::graphics::window {

/// A steady clock's milliseconds now: what a medium whose device keeps no clock counts by.
inline std::int64_t steady_now_ms() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/// A device's clock, where it keeps one (`now_ms()`), else a steady clock: what a window counts
/// the weaver's clicks by.
template <class Device>
std::int64_t device_now_ms(Device& device) {
    if constexpr (requires { device.now_ms(); }) {
        return device.now_ms();
    } else {
        return steady_now_ms();
    }
}

/// THE LAST PRESS OF ONE BUTTON, as it was counted: when it came, in a steady clock's
/// milliseconds that are never kept or sent, and the clicks it counted (0: none yet).
struct CountedPress {
    std::int64_t at_ms = 0;
    std::int64_t clicks = 0;
};

/// THE CLICKS A PRESS COUNTS: one more than the last press of its button when it comes within
/// `double_click_ms` after it, else 1 -- so 2 for a double click's second press, and on.
inline std::int64_t clicks_of(const CountedPress& last, std::int64_t now_ms,
                              std::int64_t double_click_ms) noexcept {
    if (last.clicks < 1 || now_ms < last.at_ms || now_ms - last.at_ms > double_click_ms) {
        return 1;
    }
    return last.clicks < std::numeric_limits<std::int64_t>::max() ? last.clicks + 1 : last.clicks;
}

/// ONE HAND'S PRESSES COUNTED, a button each (1 left, 2 middle, 3 right), by the time it holds.
class ClickCounter {
public:
    /// The time two presses are one double click by, held to what the input role takes.
    void time(std::int64_t double_click_ms) noexcept {
        double_click_ms_ = std::clamp(double_click_ms, zengine::input::kLeastDoubleClickMs,
                                      zengine::input::kMostDoubleClickMs);
    }
    std::int64_t time() const noexcept { return double_click_ms_; }

    /// A press of `button` at `now_ms`, counted and remembered; a button outside 1..3 counts 1.
    std::int64_t press(std::int64_t button, std::int64_t now_ms) noexcept {
        if (button < 1 || button > static_cast<std::int64_t>(last_.size())) {
            return 1;
        }
        CountedPress& last = last_[static_cast<std::size_t>(button - 1)];
        const std::int64_t clicks = clicks_of(last, now_ms, double_click_ms_);
        last = CountedPress{now_ms, clicks};
        return clicks;
    }

private:
    std::int64_t double_click_ms_ = zengine::input::kDefaultDoubleClickMs;
    std::array<CountedPress, 3> last_{};
};

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_CLICKS_HPP
