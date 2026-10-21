// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_PIXELS_HPP
#define ZENGINE_GRAPHICS_PIXELS_HPP

// Arithmetic on canvas pixels and the cells a character medium floors them to: sums, products and
// floors that cannot leave the number line, a pixel in a medium's device unit, and the grain a
// hit test floors by. Every coordinate is a number a publisher chose, so each is total.
// Reference: graphics/docs/graphics.md.

#include "graphics/vocabulary.hpp" // `kCellPx`

#include <cstdint>
#include <limits>

namespace zengine::graphics {

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

/// The largest cell coordinate that survives being multiplied into pixels. Not a
/// policy about how big a canvas may be — exactly how many cells a pixel number
/// can hold.
inline constexpr std::int64_t kMaxCellsInPixels =
    (std::numeric_limits<std::int64_t>::max)() / kCellPx;

/// A cell coordinate in canvas pixels, saturating at both ends.
inline constexpr std::int64_t px_of_cells(std::int64_t cells) noexcept {
    if (cells >= kMaxCellsInPixels) {
        return kMaxCellsInPixels * kCellPx;
    }
    if (cells <= -kMaxCellsInPixels) {
        return -kMaxCellsInPixels * kCellPx;
    }
    return cells * kCellPx;
}

/// `a - b` without leaving the number line — `add_cells`, the other
/// way round. It is its own function rather than `add_cells(a, -b)` because
/// negating `INT64_MIN` is itself the undefined behaviour being avoided.
inline constexpr std::int64_t sub_px(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (b < 0) {
        return a > kMax + b ? kMax : a - b;
    }
    if (b > 0) {
        return a < kMin + b ? kMin : a - b;
    }
    return a;
}

/// `a * b` for non-negative operands, saturating: `text_line_px` arrives on the bus, so a row's
/// pixel offset multiplies by a number a publisher chose. A non-positive operand answers zero.
inline constexpr std::int64_t mul_px(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    if (a <= 0 || b <= 0) {
        return 0;
    }
    return a > kMax / b ? kMax : a * b;
}

/// `v / d` floored, for a positive divisor: `/` truncates toward zero, which would put pixel -1
/// in column 0.
inline constexpr std::int64_t floor_div_px(std::int64_t v, std::int64_t d) noexcept {
    if (d <= 0) {
        return 0;
    }
    const std::int64_t q = v / d;
    return (v % d < 0) ? q - 1 : q;
}

// ---- The pixel lattice and the cell a character medium floors it to -------------------------
//
// A canvas coordinate is a whole canvas pixel. A medium whose device unit is the cell shows the
// span [L, R) on cells [floor(L/12), floor(R/12)) and hit-tests by the same floor, so the first
// cell it paints is the first that answers a hand.

/// The cell a canvas pixel lands in: FLOORED, because `/` truncates toward zero and would put
/// pixel -1 in cell 0, so a click just outside the canvas would select what is just inside. A
/// span [L, R) covers the cells [cell_of_pixel(L), cell_of_pixel(R)). Total over every value.
inline constexpr std::int64_t cell_of_pixel(std::int64_t v) noexcept {
    const std::int64_t q = v / kCellPx;
    return (v % kCellPx < 0) ? q - 1 : q;
}

/// A canvas pixel in a medium's device unit, for a medium that reported its own cell size
/// (`WindowOffered::cell_px`, device pixels per canvas cell): floored, so a caller spelling
/// geometry in that unit and a medium drawing it cannot disagree about a fractional edge.
/// `cell_px <= 0` means the device unit is the cell, and the answer is a cell count. Total over
/// every argument.
inline constexpr std::int64_t device_of_px(std::int64_t px, std::int64_t cell_px) noexcept {
    if (cell_px <= 0) {
        return cell_of_pixel(px);
    }
    const std::int64_t bound = (std::numeric_limits<std::int64_t>::max)() / cell_px;
    const std::int64_t s = px > bound ? bound : (px < -bound ? -bound : px);
    return floor_div_px(s * cell_px, kCellPx);
}

static_assert(device_of_px(kCellPx, kCellPx) == kCellPx,
              "the shipped window's device pixel IS the canvas pixel");
static_assert(device_of_px(kCellPx * 3, 0) == 3,
              "a medium whose device unit is the cell answers in cells");

/// Whether a canvas pixel is exactly sayable in that medium's device unit, or only its floor is:
/// what a readout needs to tell what a weaver chose from what a medium can show.
inline constexpr bool px_exact_in_device(std::int64_t px, std::int64_t cell_px) noexcept {
    if (cell_px <= 0) {
        return px % kCellPx == 0;
    }
    const std::int64_t bound = (std::numeric_limits<std::int64_t>::max)() / cell_px;
    if (px > bound || px < -bound) {
        return false;
    }
    return (px * cell_px) % kCellPx == 0;
}

/// The thinnest span a medium can show, in canvas pixels: one of its device units, which is what
/// a publisher drawing a boundary asks. A whole cell where the device unit is the cell. A
/// ceiling, so `device_of_px` of it is never zero, even on a cell size the canvas pixel does not
/// divide; a medium finer than the canvas pixel gets one pixel.
inline constexpr std::int64_t px_of_one_device(std::int64_t cell_px) noexcept {
    if (cell_px <= 0) {
        return kCellPx;
    }
    if (cell_px >= kCellPx) {
        return 1;
    }
    return (kCellPx + cell_px - 1) / cell_px;
}

static_assert(px_of_one_device(0) == kCellPx,
              "a medium whose device unit is the cell can show nothing thinner than one");
static_assert(px_of_one_device(kCellPx) == 1,
              "the shipped window's device unit is one canvas pixel, reached from its REPORT");
static_assert(device_of_px(px_of_one_device(7), 7) == 1 &&
                  device_of_px(px_of_one_device(7) - 1, 7) == 0,
              "and it is the SMALLEST span that does, on a cell size the pixel does not divide");

/// A window's device unit, in canvas pixels: one window pixel.
inline constexpr std::int64_t kPixelGrainPx = 1;

/// A character medium's device unit, in canvas pixels: one canvas cell.
inline constexpr std::int64_t kCellGrainPx = kCellPx;

/// Does a press at this device unit land on this span? The paint rule read backwards: a medium
/// of grain `g` shows [begin, begin + extent) on units [floor(begin/g), floor(end/g)), so both
/// sides are floored by the same grain before comparing. Comparing the raw pixel is off by up to
/// one device unit at an edge inside a unit -- an edge a hand passes through.
inline constexpr bool px_span_contains(std::int64_t begin, std::int64_t extent,
                                       std::int64_t press_px, std::int64_t grain) noexcept {
    if (extent <= 0 || grain <= 0) {
        return false;
    }
    const std::int64_t unit = floor_div_px(press_px, grain);
    return unit >= floor_div_px(begin, grain) &&
           unit < floor_div_px(add_cells(begin, extent), grain);
}

/// A canvas pixel floored to a medium's device unit: the pixel itself in a window, the corner of
/// its cell in a terminal. A unit that is not positive is the cell.
inline constexpr std::int64_t floor_to_grain(std::int64_t px, std::int64_t grain) noexcept {
    const std::int64_t unit = grain > 0 ? grain : kCellPx;
    return mul_px(floor_div_px(px, unit), unit);
}

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_PIXELS_HPP
