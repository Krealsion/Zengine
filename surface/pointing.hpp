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
// Reference: workshop/docs/pointer-spaces.md.

#include "graphics/pixels.hpp"

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
/// window's origin, one cell every `kCellPx` pixels.
inline constexpr CanvasPoint canvas_of_window_pixels(std::int64_t x, std::int64_t y) noexcept {
    return CanvasPoint{graphics::cell_of_pixel(x), graphics::cell_of_pixel(y)};
}

/// A pointer position in a terminal, as a canvas cell: a terminal cell is a canvas cell, offset
/// by the rows the medium spends above its canvas -- `kTuiCanvasTopRow` for a terminal Skin, none
/// for a terminal window. The subtraction saturates, because the number arrives from the wire.
inline constexpr CanvasPoint canvas_of_terminal_cells(
    std::int64_t x, std::int64_t y, std::int64_t rows_above = kTuiCanvasTopRow) noexcept {
    return CanvasPoint{x, graphics::add_cells(y, -rows_above)};
}

// ---- The same two projections in canvas pixels ------------------------------------------
//
// A pane gesture spends the pointer's own resolution, so each projection has a pixel twin. The
// medium's grain (`graphics::kPixelGrainPx`, `graphics::kCellGrainPx`) is carried with the point
// because a hit test must quantize a rectangle exactly as the medium painted it.

/// A pointer position in the graphical Skin's window, in canvas pixels: the canvas starts at the
/// window's origin and a window pixel is a canvas pixel, so a pane follows a hand pixel for pixel.
inline constexpr CanvasPoint canvas_px_of_window_pixels(std::int64_t x,
                                                        std::int64_t y) noexcept {
    return CanvasPoint{x, y};
}

/// A pointer position in a terminal, in canvas pixels: the cell's own corner. A terminal cannot
/// say anything finer than a cell, and its projection says so rather than inventing a pixel
/// nobody reported.
inline constexpr CanvasPoint canvas_px_of_terminal_cells(
    std::int64_t x, std::int64_t y, std::int64_t rows_above = kTuiCanvasTopRow) noexcept {
    const CanvasPoint cell = canvas_of_terminal_cells(x, y, rows_above);
    return CanvasPoint{graphics::px_of_cells(cell.x), graphics::px_of_cells(cell.y)};
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_POINTING_HPP
