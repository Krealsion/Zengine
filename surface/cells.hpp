// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_CELLS_HPP
#define ZENGINE_SURFACE_CELLS_HPP

// Arithmetic on canvas cell coordinates, for the Skins that rasterize them. Every coordinate in
// a canvas is a number a publisher chose, so these are total: a sum that cannot leave the number
// line, and a span clipped to the canvas before any loop or pixel multiply runs over it.
// Reference: docs/reference/surface.md.

#include <cstdint>
#include <limits>

namespace zengine::surface {

/// `a + b` in cells, saturating: `INT64_MAX + 1` would be undefined behaviour produced by data,
/// and the saturated ends are far outside any canvas, which already means "clipped".
inline constexpr std::int64_t add_cells(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (b > 0) {
        return a > kMax - b ? kMax : a + b;
    }
    if (b < 0) {
        return a < kMin - b ? kMin : a + b;
    }
    return a;
}

/// Half-open `[begin, end)` in cells: one axis of something published, clipped to
/// an extent. `end <= begin` means nothing of it is on the canvas.
struct CellSpan {
    std::int64_t begin = 0;
    std::int64_t end = 0;

    constexpr bool empty() const noexcept { return end <= begin; }
    constexpr std::int64_t count() const noexcept { return empty() ? 0 : end - begin; }

    friend bool operator==(const CellSpan&, const CellSpan&) = default;
};

/// The part of `[origin, origin + length)` on `[0, extent)`, with `0 <= begin <= end <= extent`:
/// both ends clamped, so an empty span sits at the extent rather than off the number line. Clip
/// before iterating: a loop that drops off-canvas cells works in proportion to what the
/// publisher said (a 4x2 canvas with a rect 100,000,000 cells wide took 75 ms for 38 bytes).
inline constexpr CellSpan clip_span(std::int64_t origin, std::int64_t length,
                                    std::int64_t extent) noexcept {
    const std::int64_t end = add_cells(origin, length);
    CellSpan s;
    s.begin = origin > 0 ? origin : 0;
    if (s.begin > extent) {
        s.begin = extent;
    }
    s.end = end < extent ? end : extent;
    if (s.end < s.begin) {
        s.end = s.begin;
    }
    return s;
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_CELLS_HPP
