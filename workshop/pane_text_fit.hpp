// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_TEXT_FIT_HPP
#define ZENGINE_WORKSHOP_PANE_TEXT_FIT_HPP

// How much prose a bounded region holds: a region's pixel bounds and a medium's text metric
// resolved into a viewport, a local origin and a capacity, the one arithmetic a drawer and the
// medium it draws on both use, so a drawer's "12 earlier" and the drawing never disagree. A zero
// metric is the cell projection, a real answer: a terminal's text is a cell. And which columns of
// a row a region's selection covers. Reference: workshop/docs/workshop-panes.md.

#include "graphics/pixels.hpp"

#include <cstdint>

namespace zengine::workshop {

/// The breathing room inside a text region, in device pixels, on every side. Here and not in a
/// renderer: `fit_region` subtracts it from the columns it tells the publisher and adds it to
/// the origin it tells the medium, so it cannot be applied only once.
inline constexpr std::int64_t kTextInsetPx = 2;

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
    const std::int64_t cell_columns = graphics::cell_of_pixel(graphics::add_cells(x, ww)) - graphics::cell_of_pixel(x);
    const std::int64_t cell_rows = graphics::cell_of_pixel(graphics::add_cells(y, hh)) - graphics::cell_of_pixel(y);
    return resolve_region_fit(view, cell_columns, cell_rows, text_advance_px, text_line_px);
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
        out.w = graphics::add_cells(graphics::mul_px(columns, text_advance_px), 2 * kTextInsetPx);
        out.h = graphics::add_cells(graphics::mul_px(rows, text_line_px), 2 * kTextInsetPx);
        return out;
    }
    out.w = graphics::px_of_cells(columns);
    out.h = graphics::px_of_cells(rows);
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
static_assert(region_px_for(20, 5, 0, 0) == RegionPixels{20 * graphics::kCellPx, 5 * graphics::kCellPx},
              "a metric with no type answers in whole cells, the fallback's own sentence");

/// Which columns of one row a selection covers -- a text region's, or a pane's rows' (`RowsCaret`):
/// whatever says `sel_begin_row`, `sel_begin_col`, `sel_end_row` and `sel_end_col` -- written once
/// for every medium (two media computing it apart would highlight different characters). The
/// begin row is covered
/// from `sel_begin_col` to its end, the end row up to `sel_end_col`, rows between whole, all
/// clamped into `[0, row_len]`. A range that is absent, empty or out of order answers the empty
/// span for every row.
struct RowSpan {
    std::int64_t begin = 0;
    std::int64_t end = 0; ///< exclusive; `end > begin` is what "selected here" means
    constexpr bool present() const noexcept { return end > begin; }
};

template <class Range>
inline constexpr RowSpan selection_span_of_row(const Range& r, std::int64_t row,
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

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_TEXT_FIT_HPP
