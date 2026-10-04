// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_POINTING_HPP
#define ZENGINE_SURFACE_POINTING_HPP

// Where a reported pointer position lands on the canvas: one function per medium. A pointer
// arrives in the medium's own numbers (a terminal's cells, a window's pixels), and turning it
// into a canvas pixel needs how that medium lays a canvas out, which only its Skin knows -- so
// the numbers live here, and a consumer keeps only where its own work sits on the canvas. No
// registry and no backend dispatch: a consumer picks the function for the space its event was
// stamped with. They describe the shipped Skins; a second graphical layout needs its own.
// Reference: docs/reference/pointer-spaces.md.

#include "cells.hpp"
#include "region.hpp"
#include "vocabulary.hpp"

#include <cstdint>

namespace zengine::surface {

/// A position on the canvas, in canvas pixels or in cells as the function that made it says. It
/// may be outside the canvas: this is a projection, not a hit test, and "off the canvas" is an
/// answer.
struct CanvasPoint {
    std::int64_t x = 0;
    std::int64_t y = 0;

    friend bool operator==(const CanvasPoint&, const CanvasPoint&) = default;
};

/// The terminal row the terminal Skins put canvas row 0 on, 0-based: they write `\x1b[3;1H`,
/// because rows 1 and 2 are the status and score slots.
inline constexpr std::int64_t kTuiCanvasTopRow = 2;

/// A pointer position in the graphical Skin's window, as a canvas cell: the canvas starts at the
/// window's origin, one cell every `kCanvasCellPx` pixels.
inline constexpr CanvasPoint canvas_of_window_pixels(std::int64_t x, std::int64_t y) noexcept {
    return CanvasPoint{cell_of_pixel(x), cell_of_pixel(y)};
}

/// A pointer position in a terminal, as a canvas cell: a terminal cell is a canvas cell, offset
/// by `kTuiCanvasTopRow`. The subtraction saturates, because the number arrives from the wire.
inline constexpr CanvasPoint canvas_of_terminal_cells(std::int64_t x, std::int64_t y) noexcept {
    return CanvasPoint{x, add_cells(y, -kTuiCanvasTopRow)};
}

// ---- The same two projections in canvas pixels ------------------------------------------
//
// A pane gesture spends the pointer's own resolution, so each projection has a pixel twin. The
// grain beside each is that medium's device unit in canvas pixels, carried with the point
// because a hit test must quantize a rectangle exactly as the medium painted it.

/// The shipped graphical Skin's device unit, in canvas pixels: one window pixel.
inline constexpr std::int64_t kPixelGrainPx = 1;

/// A character medium's device unit, in canvas pixels: one canvas cell.
inline constexpr std::int64_t kCellGrainPx = kCanvasCellPx;

/// A pointer position in the graphical Skin's window, in canvas pixels: the canvas starts at the
/// window's origin and a window pixel is a canvas pixel, so a pane follows a hand pixel for pixel.
inline constexpr CanvasPoint canvas_px_of_window_pixels(std::int64_t x,
                                                        std::int64_t y) noexcept {
    return CanvasPoint{x, y};
}

/// A pointer position in a terminal, in canvas pixels: the cell's own corner. A terminal cannot
/// say anything finer than a cell, and its projection says so rather than inventing a pixel
/// nobody reported.
inline constexpr CanvasPoint canvas_px_of_terminal_cells(std::int64_t x,
                                                         std::int64_t y) noexcept {
    const CanvasPoint cell = canvas_of_terminal_cells(x, y);
    return CanvasPoint{px_of_cells(cell.x), px_of_cells(cell.y)};
}

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

/// Which prose column of a bounded text region a canvas pixel is on: subtract where the
/// region's text starts, floor by one character. It takes the region's `fit`, so the column is
/// resolved with the metric its rows were drawn with. A projection, not a hit test: a negative
/// column or one past `columns` means "not on this region's prose". Under a cell fit it is the
/// cell answer: the pixel's cell less the cell the region's edge floors to.
inline constexpr std::int64_t prose_column_of_pixel(std::int64_t px, const RegionFit& fit) noexcept {
    if (!fit.graphical()) {
        return sub_px(cell_of_pixel(px), cell_of_pixel(fit.view.x));
    }
    return floor_div_px(sub_px(px, add_cells(fit.view.x, fit.origin_x)), fit.advance_px);
}

/// WHICH PROSE ROW OF A BOUNDED TEXT REGION A CANVAS PIXEL IS ON. The other axis
/// of `prose_column_of_pixel`, same rules, same non-answers.
inline constexpr std::int64_t prose_row_of_pixel(std::int64_t py, const RegionFit& fit) noexcept {
    if (!fit.graphical()) {
        return sub_px(cell_of_pixel(py), cell_of_pixel(fit.view.y));
    }
    return floor_div_px(sub_px(py, add_cells(fit.view.y, fit.origin_y)), fit.line_px);
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_POINTING_HPP
