// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_CELLS_HPP
#define ZENGINE_SURFACE_CELLS_HPP

// Arithmetic on canvas cell coordinates, for the Skins that rasterize them.
//
// A `SurfaceCanvas` is a ZEN_SHAPE: every coordinate in one arrives from the wire
// or from a poke, so a Skin's clipping arithmetic runs on values a publisher
// chose and not on values an application computed. That is the input domain this
// header exists for, and it is the P11 shape stated once instead of learned again
// per Skin — a helper shared between media inherits the widest domain of any of
// them, so it is written total rather than written for the values Workshop
// happens to send.
//
// Two Skins rasterize a canvas (the terminal's `canvas_body`, the SDL plan's
// `plan_canvas`) and both need the same two facts: a sum that cannot leave the
// number line, and a span already clipped to the canvas so neither the loop nor
// the pixel multiply that follows it can run away. G-0 found both defects live in
// the terminal Skin, so the answer is one rule in one place rather than two
// copies that agree today.

#include <cstdint>
#include <limits>

namespace zengine::surface {

/// `a + b` in cells, without leaving the number line.
///
/// `INT64_MAX + 1` is undefined behaviour produced by data. The saturated ends are
/// far outside any canvas extent, which already means "clipped", so saturation
/// costs nothing a picture could notice.
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

/// The part of `[origin, origin + length)` that lies on `[0, extent)`.
///
/// `extent` is a clipped canvas extent, so it is never negative, and the answer
/// always satisfies `0 <= begin <= end <= extent`. Both ends are clamped, not just
/// the far one: a span that starts past the extent comes back as an empty span AT
/// the extent rather than one beginning somewhere off the number line, so a caller
/// cannot pick up a nonsense `begin` from a span it correctly treated as empty.
///
/// CLIPPING BEFORE ITERATING, not inside the loop, and the difference is not
/// style. A rasterizer that visits every cell of a published rectangle and drops
/// the ones off the canvas does work proportional to what the PUBLISHER said
/// rather than to what the canvas can show: a 4x2 canvas carrying one rect
/// 100,000,000 cells wide took 75 ms to produce 38 bytes, and the same shape at
/// the top of the number line does not finish. Clipped first, the work is bounded
/// by the canvas — which is the only bound a Skin actually owns.
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
