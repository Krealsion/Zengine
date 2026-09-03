// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_INTERACTION_TIME_HPP
#define ZENGINE_WORKSHOP_INTERACTION_TIME_HPP

// WHAT MONOTONIC TIME IS IT NOW, AND NOTHING ELSE.
// Workshop law: agents/workshop/pointer.md (+1 registers; agents/workshop.md routes)

#include <chrono>
#include <cstdint>

namespace zengine::workshop {

/// MILLISECONDS SINCE THE FIRST TIME ANYTHING IN THIS PROCESS ASKED.
// WL-PTR-01 -- agents/workshop/pointer.md
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
