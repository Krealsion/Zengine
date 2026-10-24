// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_SCREEN_CELLS_HPP
#define ZENGINE_TESTS_SCREEN_CELLS_HPP

// Workshop's planes (`paint`, workshop/screen_planes.hpp) read as cells, for the screen suites:
// each plane in list order, its rects, then its labels, then its regions, a later write covering
// an earlier one at the cell grain. A region's rows stand one cell row apart from its top, cut at
// the cells it covers and padded across them where it owns its ground. What a window shows is read
// from the window; this reads the composition Workshop draws from.

#include "graphics/pixels.hpp"
#include "graphics/role.hpp"
#include "workshop/pane_text_fit.hpp"
#include "workshop/screen_planes.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::testing::screen_cells {

namespace ws = zengine::workshop;
namespace gfx = zengine::graphics;

/// How much of a region is materialized, per axis: a bound on a case's own numbers.
inline constexpr std::int64_t kMaxCells = 16384;

/// One region row as cells: the label it writes, the ground beneath it, the selected columns and
/// the column its caret stands on (-1 none).
struct CellRow {
    ws::ScreenLabel label;
    std::int64_t background = gfx::role::kNone;
    std::int64_t ground = ws::kGroundOwn;
    std::int64_t sel_begin = 0, sel_end = 0;
    std::int64_t caret = -1;
};

/// A region's rows as cells: row `i` one cell below row 0, cut at the covered cells' width,
/// dropped past their height, padded across the width unless the region writes on the ground
/// beneath it; a caret stands on the cell of the character it sits before, past a full row's end
/// on its last cell, and at a beneath row's end on one blank cell.
inline void region_rows(const ws::ScreenRegion& r, std::vector<CellRow>& out) {
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    const std::int64_t covered_w =
        gfx::cell_of_pixel(gfx::add_cells(r.x, r.w)) - gfx::cell_of_pixel(r.x);
    const std::int64_t covered_h =
        gfx::cell_of_pixel(gfx::add_cells(r.y, r.h)) - gfx::cell_of_pixel(r.y);
    const auto width = static_cast<std::size_t>(std::min(covered_w, kMaxCells));
    const std::int64_t lines = std::min(covered_h, kMaxCells);
    for (std::int64_t i = 0; i < lines; ++i) {
        const auto at = static_cast<std::size_t>(i);
        const bool said = at < r.rows.size();
        std::string text = said ? r.rows[at].text : std::string();
        const std::int64_t role = said ? r.rows[at].role : gfx::role::kFill;
        const std::int64_t back = said ? r.rows[at].background : gfx::role::kNone;
        ws::RowSpan span = ws::selection_span_of_row(r, i, static_cast<std::int64_t>(text.size()));
        const bool has_caret = r.caret_row == i && r.caret_col >= 0 &&
                               r.caret_col <= static_cast<std::int64_t>(text.size()) &&
                               r.caret_col <= static_cast<std::int64_t>(width) && width > 0;
        const bool takes_the_cells = r.ground != ws::kGroundBeneath || back != gfx::role::kNone;
        if (has_caret && static_cast<std::size_t>(r.caret_col) == text.size() && text.size() < width) {
            text += ' ';
        }
        if (text.size() > width) {
            text.resize(width);
        } else if (takes_the_cells) {
            text.append(width - text.size(), ' ');
        } else if (text.empty()) {
            continue;
        }
        const auto last = static_cast<std::int64_t>(text.size()) - 1;
        const std::int64_t caret = has_caret ? std::min(r.caret_col, last) : -1;
        span.begin = std::min(span.begin, static_cast<std::int64_t>(text.size()));
        span.end = std::min(span.end, static_cast<std::int64_t>(text.size()));
        out.push_back(CellRow{ws::ScreenLabel{r.x, gfx::add_cells(r.y, gfx::px_of_cells(i)),
                                              std::move(text), role},
                              back, r.ground, span.begin, span.end, caret});
    }
}

/// Every region of one plane as cells, in the plane's order.
inline std::vector<CellRow> region_rows(const ws::ScreenPlane& plane) {
    std::vector<CellRow> out;
    for (const ws::ScreenRegion& r : plane.texts) {
        region_rows(r, out);
    }
    return out;
}

/// The glyph a role's rect fills a cell with, so a picture read as text still tells inks apart.
inline char glyph_of(std::int64_t role) noexcept {
    switch (role) {
    case gfx::role::kAccent: return '*';
    case gfx::role::kMuted: return '.';
    case gfx::role::kAlert: return '!';
    case gfx::role::kGround: return ' ';
    case gfx::role::kMediumGround: return ' ';
    case gfx::role::kGuide: return '+';
    default: return '#';
    }
}

/// The whole canvas as rows of cells: `height` rows of `width` glyphs, the planes in list order.
inline std::vector<std::string> rows_of(const ws::ScreenCanvas& c) {
    const std::int64_t w = c.width > 0 ? gfx::cell_of_pixel(c.width) : 0;
    const std::int64_t h = c.height > 0 ? gfx::cell_of_pixel(c.height) : 0;
    std::vector<std::string> rows(static_cast<std::size_t>(std::max<std::int64_t>(h, 0)),
                                  std::string(static_cast<std::size_t>(std::max<std::int64_t>(w, 0)), ' '));
    const auto put = [&](std::int64_t x, std::int64_t y, char g) {
        if (x >= 0 && y >= 0 && x < w && y < h) {
            rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = g;
        }
    };
    const auto write = [&](const ws::ScreenLabel& l) {
        const std::int64_t x0 = gfx::cell_of_pixel(l.x);
        const std::int64_t y0 = gfx::cell_of_pixel(l.y);
        for (std::size_t i = 0; i < l.text.size(); ++i) {
            put(gfx::add_cells(x0, static_cast<std::int64_t>(i)), y0, l.text[i]);
        }
    };
    for (const ws::ScreenPlane& plane : c.layers) {
        for (const ws::ScreenRect& r : plane.rects) {
            if (r.w <= 0 || r.h <= 0) {
                continue;
            }
            const std::int64_t x0 = std::max<std::int64_t>(gfx::cell_of_pixel(r.x), 0);
            const std::int64_t y0 = std::max<std::int64_t>(gfx::cell_of_pixel(r.y), 0);
            const std::int64_t x1 = std::min(gfx::cell_of_pixel(gfx::add_cells(r.x, r.w)), w);
            const std::int64_t y1 = std::min(gfx::cell_of_pixel(gfx::add_cells(r.y, r.h)), h);
            for (std::int64_t y = y0; y < y1; ++y) {
                for (std::int64_t x = x0; x < x1; ++x) {
                    put(x, y, glyph_of(r.role));
                }
            }
        }
        for (const ws::ScreenLabel& l : plane.labels) {
            write(l);
        }
        for (const CellRow& row : region_rows(plane)) {
            write(row.label);
        }
    }
    return rows;
}

} // namespace zengine::testing::screen_cells

#endif // ZENGINE_TESTS_SCREEN_CELLS_HPP
