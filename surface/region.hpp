// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_REGION_HPP
#define ZENGINE_SURFACE_REGION_HPP

// A bounded region of a canvas, resolved: the one arithmetic a publisher and a medium both use
// to turn a region's pixel bounds and a medium's text metric into a viewport, a local
// origin and a capacity, so a publisher's "12 earlier" and the medium's drawing never disagree.
// A zero metric is the cell projection, a real answer: a terminal's text is a cell. Every
// product saturates, because a region's bounds are numbers a publisher chose. Reference:
// docs/reference/surface.md.

#include "cells.hpp"
#include "vocabulary.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace zengine::surface {

/// The breathing room inside a text region, in device pixels, on every side. Here and not in a
/// renderer: `fit_region` subtracts it from the columns it tells the publisher and adds it to
/// the origin it tells the medium, so it cannot be applied only once.
inline constexpr std::int64_t kTextInsetPx = 2;

/// How wide a caret bar is, in device pixels, in a medium that sets real type: part of where a
/// caret is, so a hit test, a plan and a renderer share it. A bar, never a block, where type is
/// set, because the caret is an insertion point between two characters; at an 8-pixel advance one
/// pixel reads as a stem and four as a block cursor. A medium whose character is a cell has no
/// place between two characters, and shows the cell the caret stands on inverted instead
/// (`ProjectedRow::caret`).
inline constexpr std::int64_t kCaretWidthPx = 2;

/// The largest cell coordinate that survives being multiplied into pixels. Not a
/// policy about how big a canvas may be — exactly how many cells a pixel number
/// can hold.
inline constexpr std::int64_t kMaxCellsInPixels =
    (std::numeric_limits<std::int64_t>::max)() / kCanvasCellPx;

/// A cell coordinate in canvas pixels, saturating at both ends.
inline constexpr std::int64_t px_of_cells(std::int64_t cells) noexcept {
    if (cells >= kMaxCellsInPixels) {
        return kMaxCellsInPixels * kCanvasCellPx;
    }
    if (cells <= -kMaxCellsInPixels) {
        return -kMaxCellsInPixels * kCanvasCellPx;
    }
    return cells * kCanvasCellPx;
}

/// `a - b` without leaving the number line — cells.hpp's `add_cells`, the other
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
    const std::int64_t q = v / kCanvasCellPx;
    return (v % kCanvasCellPx < 0) ? q - 1 : q;
}

/// A canvas pixel in a medium's device unit, for a medium that reported its own cell size
/// (`SurfaceExtent::cell_px`, device pixels per canvas cell): floored, so a caller spelling
/// geometry in that unit and a medium drawing it cannot disagree about a fractional edge.
/// `cell_px <= 0` means the device unit is the cell, and the answer is a cell count. Total over
/// every argument.
inline constexpr std::int64_t device_of_px(std::int64_t px, std::int64_t cell_px) noexcept {
    if (cell_px <= 0) {
        return cell_of_pixel(px);
    }
    const std::int64_t bound = (std::numeric_limits<std::int64_t>::max)() / cell_px;
    const std::int64_t s = px > bound ? bound : (px < -bound ? -bound : px);
    return floor_div_px(s * cell_px, kCanvasCellPx);
}

static_assert(device_of_px(kCanvasCellPx, kCanvasCellPx) == kCanvasCellPx,
              "the shipped window's device pixel IS the canvas pixel");
static_assert(device_of_px(kCanvasCellPx * 3, 0) == 3,
              "a medium whose device unit is the cell answers in cells");

/// Whether a canvas pixel is exactly sayable in that medium's device unit, or only its floor is:
/// what a readout needs to tell what a weaver chose from what a medium can show.
inline constexpr bool px_exact_in_device(std::int64_t px, std::int64_t cell_px) noexcept {
    if (cell_px <= 0) {
        return px % kCanvasCellPx == 0;
    }
    const std::int64_t bound = (std::numeric_limits<std::int64_t>::max)() / cell_px;
    if (px > bound || px < -bound) {
        return false;
    }
    return (px * cell_px) % kCanvasCellPx == 0;
}

/// The thinnest span a medium can show, in canvas pixels: one of its device units, which is what
/// a publisher drawing a boundary asks. A whole cell where the device unit is the cell. A
/// ceiling, so `device_of_px` of it is never zero, even on a cell size the canvas pixel does not
/// divide; a medium finer than the canvas pixel gets one pixel.
inline constexpr std::int64_t px_of_one_device(std::int64_t cell_px) noexcept {
    if (cell_px <= 0) {
        return kCanvasCellPx;
    }
    if (cell_px >= kCanvasCellPx) {
        return 1;
    }
    return (kCanvasCellPx + cell_px - 1) / cell_px;
}

static_assert(px_of_one_device(0) == kCanvasCellPx,
              "a medium whose device unit is the cell can show nothing thinner than one");
static_assert(px_of_one_device(kCanvasCellPx) == 1,
              "the shipped window's device unit is one canvas pixel, reached from its REPORT");
static_assert(device_of_px(px_of_one_device(7), 7) == 1 &&
                  device_of_px(px_of_one_device(7) - 1, 7) == 0,
              "and it is the SMALLEST span that does, on a cell size the pixel does not divide");

/// A region's outer rectangle in a graphical medium, in device pixels: the whole rectangle it
/// was granted, not what is visible (`clip_viewport`). A medium clips to it and draws in
/// coordinates local to `x`/`y`.
struct RegionViewport {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    constexpr bool empty() const noexcept { return w <= 0 || h <= 0; }

    friend bool operator==(const RegionViewport&, const RegionViewport&) = default;
};

/// A region resolved: its pixels, where its first character starts, and how much prose fits.
/// `columns`/`rows` are the capacity both sides use -- the publisher to decide what it shows and
/// what it says it omits, the medium to know it is never handed more. `origin_x`/`origin_y` are
/// local to `view`. `advance_px`/`line_px` are the metric this fit was resolved with, so a medium
/// places row `i` at `origin_y + i * line_px` with the numbers the capacity came from.
struct RegionFit {
    RegionViewport view{};
    std::int64_t origin_x = 0;
    std::int64_t origin_y = 0;
    std::int64_t columns = 0;
    std::int64_t rows = 0;
    std::int64_t advance_px = 0; ///< 0 = this fit is the cell projection
    std::int64_t line_px = 0;

    /// Is the interior expressed in the medium's own type, rather than in cells?
    constexpr bool graphical() const noexcept { return advance_px > 0 && line_px > 0; }

    friend bool operator==(const RegionFit&, const RegionFit&) = default;
};

/// The shared core of the resolution: a viewport and a cell capacity become a fit, so the
/// capacity a medium with a face and a medium without one tell a publisher cannot disagree about
/// how a metric turns pixels into prose, or about the fallback.
inline constexpr RegionFit resolve_region_fit(const RegionViewport& view,
                                              std::int64_t cell_columns, std::int64_t cell_rows,
                                              std::int64_t text_advance_px,
                                              std::int64_t text_line_px) noexcept {
    RegionFit f;
    f.view = view;
    if (text_advance_px > 0 && text_line_px > 0) {
        const std::int64_t inner_w = f.view.w - 2 * kTextInsetPx;
        const std::int64_t inner_h = f.view.h - 2 * kTextInsetPx;
        const std::int64_t columns = inner_w > 0 ? inner_w / text_advance_px : 0;
        const std::int64_t rows = inner_h > 0 ? inner_h / text_line_px : 0;
        if (columns > 0 && rows > 0) {
            f.advance_px = text_advance_px;
            f.line_px = text_line_px;
            f.origin_x = kTextInsetPx;
            f.origin_y = kTextInsetPx;
            f.columns = columns;
            f.rows = rows;
            return f;
        }
    }
    f.columns = cell_columns > 0 ? cell_columns : 0; // one cell per character
    f.rows = cell_rows > 0 ? cell_rows : 0;
    return f;
}

/// The one resolution: a region's pixel bounds and a medium's text metric become a viewport, an
/// origin and a capacity, and every party asks this for all three. Total over every argument. A
/// non-positive advance or line is "text is a cell": the capacity is the cells the bounds cover
/// (each edge floored), with no inset. A region too small for one line of the medium's type
/// resolves the same way, so it is drawn in cells rather than vanishing (one cell tall holds no
/// row of an 18-pixel face).
inline constexpr RegionFit fit_region(std::int64_t x, std::int64_t y, std::int64_t w,
                                      std::int64_t h, std::int64_t text_advance_px,
                                      std::int64_t text_line_px) noexcept {
    const std::int64_t ww = w > 0 ? w : 0;
    const std::int64_t hh = h > 0 ? h : 0;
    const RegionViewport view{x, y, ww, hh};
    const std::int64_t cell_columns = cell_of_pixel(add_cells(x, ww)) - cell_of_pixel(x);
    const std::int64_t cell_rows = cell_of_pixel(add_cells(y, hh)) - cell_of_pixel(y);
    return resolve_region_fit(view, cell_columns, cell_rows, text_advance_px, text_line_px);
}

/// The same resolution from the shapes, so a region is fitted where it is painted.
inline constexpr RegionFit fit_region(const SurfaceTextRegion& r,
                                      const SurfaceExtent& metric) noexcept {
    return fit_region(r.x, r.y, r.w, r.h, metric.text_advance_px, metric.text_line_px);
}

/// A prose capacity, read backwards into canvas pixels.
struct RegionPixels {
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const RegionPixels&, const RegionPixels&) = default;
};

/// The one resolution read backwards: the smallest pixel extent, at a cell-aligned place, for
/// which `fit_region` answers at least `columns` by `rows`. A publisher sizing a region to its
/// content asks this rather than inverting the metric itself, which would be a second measurer
/// one inset away from a last row that does not fit. A "text is a cell" metric answers whole
/// cells.
inline constexpr RegionPixels region_px_for(std::int64_t columns, std::int64_t rows,
                                            std::int64_t text_advance_px,
                                            std::int64_t text_line_px) noexcept {
    RegionPixels out;
    if (columns <= 0 || rows <= 0) {
        return out;
    }
    if (text_advance_px > 0 && text_line_px > 0) {
        out.w = add_cells(mul_px(columns, text_advance_px), 2 * kTextInsetPx);
        out.h = add_cells(mul_px(rows, text_line_px), 2 * kTextInsetPx);
        return out;
    }
    out.w = px_of_cells(columns);
    out.h = px_of_cells(rows);
    return out;
}

// The inverse property, held at compile time for the shipped face's metric (8x18) and the
// cell projection: what this function answers is sufficient, and one pixel less is not.
static_assert(fit_region(0, 0, region_px_for(20, 5, 8, 18).w, region_px_for(20, 5, 8, 18).h,
                         8, 18)
                      .columns >= 20 &&
                  fit_region(0, 0, region_px_for(20, 5, 8, 18).w,
                             region_px_for(20, 5, 8, 18).h, 8, 18)
                          .rows >= 5,
              "the backward read must satisfy the forward one");
static_assert(fit_region(0, 0, region_px_for(20, 5, 8, 18).w - 1,
                         region_px_for(20, 5, 8, 18).h, 8, 18)
                      .columns < 20,
              "one pixel narrower no longer holds the asked columns");
static_assert(fit_region(0, 0, region_px_for(20, 5, 8, 18).w,
                         region_px_for(20, 5, 8, 18).h - 1, 8, 18)
                      .rows < 5,
              "one pixel shorter no longer holds the asked rows");
static_assert(region_px_for(20, 5, 0, 0) == RegionPixels{20 * kCanvasCellPx, 5 * kCanvasCellPx},
              "a metric with no type answers in whole cells, the fallback's own sentence");

/// The part of a viewport actually on the surface, in device pixels. Separate from the fit on
/// purpose: a window two pixels too small draws less, and does not change how much prose the
/// publisher was told fits.
inline constexpr RegionViewport clip_viewport(const RegionViewport& v, std::int64_t surface_w,
                                              std::int64_t surface_h) noexcept {
    RegionViewport out;
    out.x = v.x > 0 ? v.x : 0;
    out.y = v.y > 0 ? v.y : 0;
    // `add_cells` saturates pixels as well as cells: both numbers came off the wire.
    const std::int64_t right = add_cells(v.x, v.w);
    const std::int64_t bottom = add_cells(v.y, v.h);
    const std::int64_t clip_r = right < surface_w ? right : surface_w;
    const std::int64_t clip_b = bottom < surface_h ? bottom : surface_h;
    out.w = clip_r > out.x ? clip_r - out.x : 0;
    out.h = clip_b > out.y ? clip_b - out.y : 0;
    return out;
}

/// How much of a region the cell projection materializes, per axis: a bound, not a policy. `w`
/// and `h` are a publisher's numbers, and a region a hundred million cells wide would allocate
/// that much per row for a canvas that shows a few hundred; no display is sixteen thousand
/// cells across.
inline constexpr std::int64_t kMaxProjectedWidth = 16384;
inline constexpr std::int64_t kMaxProjectedRows = 16384;

/// Which columns of one row a region's selection covers, written once for every medium (two
/// media computing it apart would highlight different characters). The begin row is covered
/// from `sel_begin_col` to its end, the end row up to `sel_end_col`, rows between whole, all
/// clamped into `[0, row_len]`. A range that is absent, empty or out of order answers the empty
/// span for every row.
struct RowSpan {
    std::int64_t begin = 0;
    std::int64_t end = 0; ///< exclusive; `end > begin` is what "selected here" means
    constexpr bool present() const noexcept { return end > begin; }
};

inline constexpr RowSpan selection_span_of_row(const SurfaceTextRegion& r, std::int64_t row,
                                               std::int64_t row_len) noexcept {
    if (r.sel_begin_row < 0 || r.sel_end_row < r.sel_begin_row ||
        (r.sel_begin_row == r.sel_end_row && r.sel_end_col <= r.sel_begin_col)) {
        return RowSpan{}; // no selection, or a range no publisher could mean
    }
    if (row < r.sel_begin_row || row > r.sel_end_row || row_len <= 0) {
        return RowSpan{};
    }
    std::int64_t begin = row == r.sel_begin_row ? r.sel_begin_col : 0;
    std::int64_t end = row == r.sel_end_row ? r.sel_end_col : row_len;
    if (begin < 0) {
        begin = 0;
    }
    if (end > row_len) {
        end = row_len;
    }
    return begin < end ? RowSpan{begin, end} : RowSpan{};
}

/// One projected row: the label a cell medium draws, and the ground it draws it on. The ground
/// sits beside the label because `SurfaceLabel` is a wire shape with no background, and the
/// pairing belongs to this projection alone.
struct ProjectedRow {
    SurfaceLabel label;
    std::int64_t background = role::kNone; ///< role::kNone: whatever is underneath
    /// ...and the region's own answer: a medium resolves the row's ground if it named one, else
    /// the canvas ground if the region took its rectangle, else nothing.
    std::int64_t ground = kGroundOwn;
    /// The selected columns, in the label's own bytes (cut applied): a consumer highlights
    /// `text[i]` for `sel_begin <= i < sel_end`. Empty is the absence.
    std::int64_t sel_begin = 0;
    std::int64_t sel_end = 0;
    /// The byte of `label.text` the region's caret stands on, which a medium shows inverted;
    /// -1 is none. It inserts nothing, so no character moves for a caret.
    std::int64_t caret = -1;
};

/// A region's rows as canvas labels, the cell projection every cell medium shares: row `i` one
/// cell below row 0, cut at the covered cells' width, dropped past their height. Every cell row
/// gets a label padded to the full width, because a region is an overlay and an unwritten row shows
/// its emptiness. Under `kGroundBeneath` a row is cut but not padded, unless it named its own
/// ground, and an empty row is not produced unless a caret stands on it. A caret stands on the
/// cell of the character it sits before, and past a full row's end on the row's last cell (this
/// projection does not scroll); a caret at a beneath row's end gets one blank cell to stand on.
inline void project_one_text_region(const SurfaceTextRegion& r, std::vector<ProjectedRow>& out) {
    if (r.w <= 0 || r.h <= 0) {
        return; // a region with no bounds shows nothing, and says nothing about it
    }
    // The capacity is the covered cells: a right edge that crosses a cell boundary earns that
    // cell, so the cut, the padding and a pane's backdrop agree to the cell.
    const std::int64_t covered_w = cell_of_pixel(add_cells(r.x, r.w)) - cell_of_pixel(r.x);
    const std::int64_t covered_h = cell_of_pixel(add_cells(r.y, r.h)) - cell_of_pixel(r.y);
    const std::size_t width = static_cast<std::size_t>(
        covered_w < static_cast<std::int64_t>(kMaxProjectedWidth) ? covered_w
                                                                  : kMaxProjectedWidth);
    const std::int64_t lines =
        covered_h < static_cast<std::int64_t>(kMaxProjectedRows) ? covered_h
                                                                 : kMaxProjectedRows;
    for (std::int64_t i = 0; i < lines; ++i) {
        const std::size_t at = static_cast<std::size_t>(i);
        const bool said = at < r.rows.size();
        std::string text = said ? r.rows[at].text : std::string();
        const std::int64_t role = said ? r.rows[at].role : role::kFill;
        const std::int64_t back = said ? r.rows[at].background : role::kNone;
        RowSpan span = selection_span_of_row(r, i, static_cast<std::int64_t>(text.size()));
        // A caret further right than just past the last cell shown stands nowhere shown.
        const bool has_caret = r.caret_row == i && r.caret_col >= 0 &&
            r.caret_col <= static_cast<std::int64_t>(text.size()) &&
            r.caret_col <= static_cast<std::int64_t>(width) && width > 0;
        // `!= kGroundBeneath`, never `== kGroundOwn`: an unknown ground reads as OWNING its
        // room, so a number nobody chose cannot stop a region padding. See vocabulary.hpp.
        const bool takes_the_cells = r.ground != kGroundBeneath || back != role::kNone;
        if (has_caret && static_cast<std::size_t>(r.caret_col) == text.size() &&
            text.size() < width) {
            text += ' '; // a cell to stand on, past the last character
        }
        if (text.size() > width) {
            text.resize(width); // cut on a byte boundary: one cell per byte, as ever
        } else if (takes_the_cells) {
            text.append(width - text.size(), ' ');
        } else if (text.empty()) {
            continue; // nothing to write and no cells to claim: not a row at all
        }
        const std::int64_t last = static_cast<std::int64_t>(text.size()) - 1;
        const std::int64_t caret = has_caret ? (r.caret_col < last ? r.caret_col : last) : -1;
        if (span.begin > static_cast<std::int64_t>(text.size())) {
            span.begin = static_cast<std::int64_t>(text.size());
        }
        if (span.end > static_cast<std::int64_t>(text.size())) {
            span.end = static_cast<std::int64_t>(text.size()); // the cut cuts highlights too
        }
        // Row `i` is `i` cells below row 0: a character medium floors the anchor to its cell,
        // and the bitmap face draws it where it is.
        out.push_back(ProjectedRow{SurfaceLabel{r.x, add_cells(r.y, px_of_cells(i)),
                                                std::move(text), role},
                                   back, r.ground, span.begin, span.end, caret});
    }
}

/// Every region of one layer as cells: a character medium's whole answer for the layer. There
/// is deliberately no overload taking a `SurfaceCanvas`: a canvas-wide projection would flatten
/// every plane into one run, the global order the canvas stopped having.
inline std::vector<ProjectedRow> project_text_regions(const SurfaceLayer& l) {
    std::vector<ProjectedRow> out;
    for (const SurfaceTextRegion& r : l.texts) {
        project_one_text_region(r, out);
    }
    return out;
}

/// The regions of one layer this medium cannot set in its own type, as cells: those whose
/// bounds `fit_region` says hold no row of the face (`plan_layer_regions` draws the rest). With
/// a zero metric that is every region, byte for byte the overload above.
inline std::vector<ProjectedRow> project_text_regions(const SurfaceLayer& l,
                                                     const SurfaceExtent& metric) {
    std::vector<ProjectedRow> out;
    for (const SurfaceTextRegion& r : l.texts) {
        if (fit_region(r, metric).graphical()) {
            continue; // this medium sets this one in type: plan_layer_regions has it
        }
        project_one_text_region(r, out);
    }
    return out;
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_REGION_HPP
