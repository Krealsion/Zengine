// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESK_GESTURES_GEOMETRY_HPP
#define ZENGINE_DESK_GESTURES_GEOMETRY_HPP

// The desk's geometry as values: a rectangle on the canvas in pixels or in whole cells, where a
// pointer landed, and which of a rectangle's eight affordances it is on. Functions of their
// arguments, holding nothing: a host composes a desk from them, and a test asks them without one.

#include "graphics/pixels.hpp" // the cell lattice: `kCellPx`, `px_span_contains`, `add_cells`

#include <algorithm>
#include <cstdint>
#include <limits>

namespace zengine::desk_gestures {

/// `v + by` without leaving the number line: a proposal is computed from values a hand reported,
/// so the step saturates and the result meets the ordinary refusal instead of wrapping.
constexpr std::int64_t saturating_step(std::int64_t v, std::int64_t by) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (by > 0) {
        return v > kMax - by ? v : v + by;
    }
    if (by < 0) {
        return v < kMin - by ? v : v + by;
    }
    return v;
}

/// `a - b` without leaving the number line, for the same reason: it saturates at either end.
constexpr std::int64_t saturating_minus(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (b < 0) {
        return a > kMax + b ? kMax : a - b;
    }
    return a < kMin + b ? kMin : a - b;
}

/// A rectangle on the canvas, in canvas pixels.
struct PixelRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const PixelRect&, const PixelRect&) = default;

    constexpr bool empty() const noexcept { return w <= 0 || h <= 0; }

    /// DOES A POINTER AT THIS PIXEL, REPORTED AT THIS GRAIN, LAND ON THIS RECTANGLE.
    // WL-GEO-07 -- agents/workshop/geometry.md
    constexpr bool contains_at(std::int64_t sx, std::int64_t sy,
                               std::int64_t grain) const noexcept {
        return graphics::px_span_contains(x, w, sx, grain) &&
               graphics::px_span_contains(y, h, sy, grain);
    }
};

/// A rectangle of whole cells: what a terminal shows of a pixel rectangle, and what a cell-counted
/// layout says before it becomes pixels. Distinct from the pixel rectangle.
struct CellRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const CellRect&, const CellRect&) = default;

    /// Whether the cell is inside. Total over every value: an empty or inverted rectangle
    /// contains nothing, and the far edge is compared in unsigned arithmetic, so a rectangle whose
    /// right edge is past the int64 range answers without signed overflow.
    constexpr bool contains(std::int64_t cx, std::int64_t cy) const noexcept {
        if (w <= 0 || h <= 0 || cx < x || cy < y) {
            return false;
        }
        using U = std::uint64_t;
        return (static_cast<U>(cx) - static_cast<U>(x)) < static_cast<U>(w) &&
               (static_cast<U>(cy) - static_cast<U>(y)) < static_cast<U>(h);
    }
};

/// The canvas pixels both rectangles hold, or an empty rectangle when they share none: two
/// rectangles that only touch along an edge share no pixel.
constexpr PixelRect overlap(const PixelRect& a, const PixelRect& b) noexcept {
    const std::int64_t x0 = (std::max)(a.x, b.x), y0 = (std::max)(a.y, b.y);
    const std::int64_t x1 = (std::min)(a.x + a.w, b.x + b.w), y1 = (std::min)(a.y + a.h, b.y + b.h);
    return x1 > x0 && y1 > y0 ? PixelRect{x0, y0, x1 - x0, y1 - y0} : PixelRect{};
}

/// A position on the canvas, in canvas pixels or in cells as the function that made it says. It
/// may be outside the canvas: a projection, not a hit test.
struct CanvasPoint {
    std::int64_t x = 0;
    std::int64_t y = 0;
    friend bool operator==(const CanvasPoint&, const CanvasPoint&) = default;
};

/// The canvas pixel a reported pointer position lands on, in pixels or a window's cells, the canvas
/// starting at the window's origin, and the cell that pixel is in -- or nothing, for a space this
/// application cannot place.
// WL-GEO-07 -- agents/workshop/geometry.md
struct PointedAt {
    bool understood = false;
    CanvasPoint cell;
    /// The same moment in canvas pixels, and the grain the reporting medium can honestly
    /// distinguish (one window pixel or one terminal cell).
    // WL-GEO-07 -- agents/workshop/geometry.md
    CanvasPoint px;
    std::int64_t grain = graphics::kCellGrainPx;
};

/// Where a pointer reported in `space` (`input::space`) at `x`, `y` lands.
PointedAt canvas_point_of(std::int64_t space, std::int64_t x, std::int64_t y) noexcept;

/// THE EIGHT MANIPULATION AFFORDANCES of a rectangle, and there is not a ninth.
namespace pane_edge {
inline constexpr std::int64_t kLeft = 0;
inline constexpr std::int64_t kRight = 1;
inline constexpr std::int64_t kTop = 2;
inline constexpr std::int64_t kBottom = 3;
inline constexpr std::int64_t kTopLeft = 4;
inline constexpr std::int64_t kTopRight = 5;
inline constexpr std::int64_t kBottomLeft = 6;
inline constexpr std::int64_t kBottomRight = 7;
inline constexpr std::int64_t kCount = 8;
} // namespace pane_edge

/// NO EDGE. Negative, for `no_color()`'s reason: every edge is a non-negative index into a
/// table, so an absence cannot collide with one.
inline constexpr std::int64_t kNoPaneEdge = -1;

/// The edge a weaver reads, and the mark they read it BY.
inline constexpr const char* pane_edge_name(std::int64_t edge) noexcept {
    switch (edge) {
    case pane_edge::kLeft: return "left";
    case pane_edge::kRight: return "right";
    case pane_edge::kTop: return "top";
    case pane_edge::kBottom: return "bottom";
    case pane_edge::kTopLeft: return "top-left";
    case pane_edge::kTopRight: return "top-right";
    case pane_edge::kBottomLeft: return "bottom-left";
    case pane_edge::kBottomRight: return "bottom-right";
    default: return "none";
    }
}

/// Plain ASCII, because the desk's canvas is plain ASCII by contract and a glyph a medium cannot
/// draw is a mark a weaver cannot read.
inline constexpr const char* pane_edge_mark(std::int64_t edge) noexcept {
    switch (edge) {
    case pane_edge::kLeft: return "<";
    case pane_edge::kRight: return ">";
    case pane_edge::kTop: return "^";
    case pane_edge::kBottom: return "v";
    case pane_edge::kTopLeft: return "<^";
    case pane_edge::kTopRight: return "^>";
    case pane_edge::kBottomLeft: return "<v";
    case pane_edge::kBottomRight: return "v>";
    default: return "-";
    }
}

/// HOW DEEP AN EDGE'S GRAB BAND REACHES INTO THE PANE, in canvas pixels: one cell --
/// exactly the ring the affordances have always occupied.
inline constexpr std::int64_t kPaneEdgeBandPx = graphics::kCellPx;

/// HOW NEAR A LINE AN EDGE THE HAND MOVES COMES TO MEET IT, in canvas pixels: under a cell, so a
/// terminal hand a cell away is not pulled across it, and at least half a cell, so a terminal hand
/// at the nearest cell meets a line that falls inside it. The reach is `workshop.snap-reach`, from
/// `kMinSnapReachPx` to `kMaxSnapReachPx`; this is its default.
inline constexpr std::int64_t kPaneSnapReachPx = 8;
inline constexpr std::int64_t kMinSnapReachPx = 6;
inline constexpr std::int64_t kMaxSnapReachPx = 11;

static_assert(kMaxSnapReachPx < graphics::kCellPx &&
                  2 * kMinSnapReachPx >= graphics::kCellPx &&
                  kMinSnapReachPx <= kPaneSnapReachPx && kPaneSnapReachPx <= kMaxSnapReachPx,
              "every reach the setting takes is under a cell, so a terminal hand is never pulled a "
              "whole cell, and at least half one, so every line falls within reach of a terminal "
              "hand's cell");

/// THE ONE CELL-SIZED MARK AN AFFORDANCE IS DRAWN ON -- at the pane's own edges.
PixelRect pane_edge_cell(const PixelRect& r, std::int64_t edge) noexcept;

/// What "take hold here" looks like: one character, used by none of the medium's role glyphs
/// (`.` room, `#` body, `*` ring, `!` alert).
inline constexpr const char* kHandleGlyph = "+";

/// One character for the cell an affordance is drawn on: `pane_edge_mark`'s two-character spelling
/// is prose. The corners share `kHandleGlyph`; their positions tell them apart.
inline constexpr const char* pane_edge_glyph(std::int64_t edge) noexcept {
    switch (edge) {
    case pane_edge::kLeft: return "<";
    case pane_edge::kRight: return ">";
    case pane_edge::kTop: return "^";
    case pane_edge::kBottom: return "v";
    default: return kHandleGlyph;
    }
}

/// WHICH AFFORDANCE OF THIS RECTANGLE A POINTER IS ON, or `kNoPaneEdge` -- at the
/// pointer's own grain.
std::int64_t pane_edge_at(const PixelRect& r, std::int64_t sx, std::int64_t sy,
                          std::int64_t grain) noexcept;

} // namespace zengine::desk_gestures

#endif // ZENGINE_DESK_GESTURES_GEOMETRY_HPP
