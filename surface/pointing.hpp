// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_POINTING_HPP
#define ZENGINE_SURFACE_POINTING_HPP

// WHERE A REPORTED POINTER POSITION LANDS ON THE CANVAS — one function per
// medium, and nothing else.
//
// WHY THIS IS THE SURFACE PACKAGE'S AND NOT THE APPLICATION'S. A pointer is
// reported in the medium's own numbers: a terminal reports the terminal's cell
// grid, a window reports the window's pixels. Turning either into a CANVAS cell
// needs one fact, and it is a fact about how that medium lays a canvas out —
// which is precisely what a Skin decides and what nobody else can see:
//
//   the terminal Skins draw the canvas from terminal row 3 (skin_tui.hpp's
//   `\x1b[3;1H`), because rows 1 and 2 carry the SurfaceText slots
//   the SDL Skin draws the canvas from the window's origin at kCanvasCellPx
//   pixels per cell (skin_sdl_plan.hpp's `canvas_window_size` / `plan_canvas`),
//   because a window title carries the SurfaceText slots instead
//
// An application holding one Skin's layout number is correct only for as long as
// it has one medium: two pixels down the SDL window is canvas row 0, not canvas
// row -2. So the numbers live with the package that authors them, and a consumer
// keeps only the composition that is genuinely its own (where its workspace sits
// ON the canvas). The three spaces and who owns each boundary are
// docs/reference/pointer-spaces.md.
//
// WHAT IS DELIBERATELY NOT HERE. There is no registry, no transform graph, no
// presentation-context query and no dispatch on a backend identity. Two media
// exist; each has one named function; a consumer picks the one that matches the
// space its event was stamped with. The `space` vocabulary lives in the Input
// package and this header does not read it — the pairing of a coordinate space
// with a medium is the consumer's statement about the run it is in, and pushing
// it in here would be this package quietly deciding that kPixels can only ever
// mean this window.
//
// THE HONEST BOUND, stated because it is exactly the thing that will stop being
// true: `canvas_of_window_pixels` describes THE graphical Skin that exists
// today — the canvas at the window's origin, no margin, no scaling, one cell
// every kCanvasCellPx pixels. A second graphical Skin with a different layout
// would need its own function and a way for a consumer to tell them apart, which
// is the open seam docs/reference/pointer-spaces.md states so that the day it
// fires is recognisable.

#include "cells.hpp"
#include "vocabulary.hpp"

#include <cstdint>

namespace zengine::surface {

/// A position on the canvas, in canvas cells. It may be outside the canvas —
/// this is a projection, not a hit test, and "off the canvas" is an answer.
struct CanvasPoint {
    std::int64_t x = 0;
    std::int64_t y = 0;

    friend bool operator==(const CanvasPoint&, const CanvasPoint&) = default;
};

/// The terminal row the terminal Skins put canvas row 0 on, 0-based.
///
/// The Skins write `\x1b[3;1H` — terminal row 3, 1-based — because rows 1 and 2
/// are the status and score slots. Two rows, therefore, between where a
/// terminal counts from and where the canvas does.
inline constexpr std::int64_t kTuiCanvasTopRow = 2;

/// `v / kCanvasCellPx`, FLOORED — the pixel-to-cell rule, written once.
///
/// C++ integer division truncates toward zero, so a plain `/` would send pixel
/// -1 to cell 0: a pointer one pixel to the LEFT of the canvas would report as
/// being in the canvas's first column, and a click just outside the window
/// would select the object just inside it. Flooring is what makes the cell
/// boundaries evenly spaced across zero, which is what a maker's eye already
/// assumes.
///
/// Total over every std::int64_t. The divisor is a positive constant, so the
/// one overflowing division C++ has (INT64_MIN / -1) is unreachable here.
inline constexpr std::int64_t cell_of_pixel(std::int64_t v) noexcept {
    const std::int64_t q = v / kCanvasCellPx;
    return (v % kCanvasCellPx < 0) ? q - 1 : q;
}

/// A pointer position in the graphical Skin's window, as a canvas cell.
///
/// The window is exactly the canvas: `canvas_window_size` sizes it
/// `extent * kCanvasCellPx` with no margin and no scaling, and `plan_canvas`
/// draws cell (0,0) at pixel (0,0). So the whole transform is one floored
/// division per axis, and `kCanvasCellPx` is consulted rather than copied.
inline constexpr CanvasPoint canvas_of_window_pixels(std::int64_t x, std::int64_t y) noexcept {
    return CanvasPoint{cell_of_pixel(x), cell_of_pixel(y)};
}

/// A pointer position in a terminal, as a canvas cell.
///
/// A terminal cell IS a canvas cell — the only difference is where the canvas
/// starts, and that is `kTuiCanvasTopRow`. The subtraction saturates because
/// the number arrives from the wire: whichever weave holds the input role is a
/// weave like any other, and `INT64_MIN - 2` is undefined behaviour produced by
/// data. The saturated end is far outside any canvas, which already means
/// "nothing there".
inline constexpr CanvasPoint canvas_of_terminal_cells(std::int64_t x, std::int64_t y) noexcept {
    return CanvasPoint{x, add_cells(y, -kTuiCanvasTopRow)};
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_POINTING_HPP
