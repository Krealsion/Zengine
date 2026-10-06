// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_CANVAS_ROWS_HPP
#define ZENGINE_WORKSHOP_PANE_CANVAS_ROWS_HPP

// A pane's rows of text, drawn as its own canvas picture: the lattice its room's text stands on,
// its rows set there as unpadded runs with their grounds, its caret and selection, the parts it
// names as the rectangles they cover, and the inverse a pointer's place is read through. The pane
// owns the picture -- what each row says, which parts it names and what a press means; this is
// the arithmetic every pane drawing rows would otherwise repeat. Reference:
// docs/reference/workshop-panes.md, "Rows on the lattice".

#include "pane_canvas_text.hpp"
#include "pane_canvas_vocabulary.hpp"
#include "pane_vocabulary.hpp"
#include "surface/region.hpp"
#include "surface/vocabulary.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHERE A ROOM'S TEXT STANDS, in local canvas pixels: the first row's first character's cell,
/// one character's advance and one row's line, and how many columns and rows the room holds --
/// the columns a prose body of its size holds, and the rows of one under the pane's title, whose
/// insets the title shares (with no title, a row more where the room has it). A cell each from
/// the room's corner in a medium whose character is a cell; where type is set, the medium's inset
/// in from the left, rows from the top, and the inset kept free at the right for a caret after a
/// full row's last character.
struct CanvasRows {
    std::int64_t x = 0, y = 0;
    std::int64_t advance = kPaneCanvasUnit, line = kPaneCanvasUnit;
    std::int64_t columns = 0, rows = 0;
    std::int64_t width = 0, height = 0; ///< the room's

    constexpr bool empty() const noexcept { return columns <= 0 || rows <= 0; }
    /// Where row `row` stands.
    constexpr std::int64_t row_y(std::int64_t row) const noexcept {
        return surface::add_cells(y, surface::mul_px(row, line));
    }
    /// Where column `column` stands.
    constexpr std::int64_t column_x(std::int64_t column) const noexcept {
        return surface::add_cells(x, surface::mul_px(column, advance));
    }
};

inline CanvasRows canvas_rows(const PaneCanvasRoom& room) {
    CanvasRows out;
    if (room.width <= 0 || room.height <= 0) return out;
    const CanvasTextMetrics m = canvas_text_metrics(room);
    out.x = m.inset;
    out.advance = m.advance;
    out.line = m.line;
    out.width = room.width;
    out.height = room.height;
    const std::int64_t across = room.width - surface::mul_px(2, m.inset);
    out.columns = across > 0 ? across / m.advance : 0;
    out.rows = room.height / m.line;
    return out;
}

/// THE CELL OF THE LATTICE A LOCAL PLACE STANDS ON: its row and column, floored, and whether the
/// room shows it. Unclamped, so a drag's place above or below the rows is a row before the first
/// or past the last, as a prose drag's is.
struct RowCell {
    std::int64_t row = 0, column = 0;
    bool shown = false;
};

inline RowCell row_cell_at(const CanvasRows& rows, std::int64_t x, std::int64_t y) {
    RowCell out;
    if (rows.advance <= 0 || rows.line <= 0) return out;
    out.row = surface::floor_div_px(surface::sub_px(y, rows.y), rows.line);
    out.column = surface::floor_div_px(surface::sub_px(x, rows.x), rows.advance);
    out.shown = out.row >= 0 && out.row < rows.rows && out.column >= 0 && out.column < rows.columns;
    return out;
}

/// WHERE A CARET AND A SELECTION STAND IN A PANE'S ROWS, as `v2::PaneCaret` says them beside
/// prose rows: the row and the column the caret sits before, and a range in reading order, its
/// end exclusive. `surface::kNoCaret` and `surface::kNoSelection` say none.
struct RowsCaret {
    std::int64_t row = surface::kNoCaret, column = 0;
    std::int64_t sel_begin_row = surface::kNoSelection, sel_begin_col = 0;
    std::int64_t sel_end_row = surface::kNoSelection, sel_end_col = 0;
};

/// A PANE'S ROWS AS ITS PICTURE in `room`, numbered `picture`: the room's ground beneath them, one
/// unpadded run for each row the lattice holds, cut to its columns -- its role, its ground blank
/// to the row's end where it names one, and the caret and the selection that stand in it -- and
/// each of `parts`, in their order, the rectangle its row and columns cover. A row's blanks after
/// its last character go, unless a ground, the caret or the selection stands on them, and a caret
/// after the last character is given the blank after it where the row has one; a row with
/// nothing left is drawn as nothing. A part on a row the room does not hold is left out. The
/// picture stands for this room, and the pane sends it as itself.
inline v5::PaneCanvasContent rows_picture(const PaneCanvasRoom& room, std::int64_t picture,
                                          const std::vector<surface::SurfaceTextRow>& rows,
                                          const std::vector<PaneRowPart>& parts = {},
                                          const RowsCaret& caret = {}) {
    v5::PaneCanvasContent out;
    out.pane = room.pane;
    out.grant = room.grant;
    out.picture = picture;
    if (room.width <= 0 || room.height <= 0) return out;
    out.rects.push_back(PaneCanvasRect{0, 0, room.width, room.height, surface::role::kGround});
    const CanvasRows lattice = canvas_rows(room);
    if (lattice.empty()) return out;
    surface::SurfaceTextRegion selected; // the selection's per-row arithmetic is surface's
    selected.sel_begin_row = caret.sel_begin_row;
    selected.sel_begin_col = caret.sel_begin_col;
    selected.sel_end_row = caret.sel_end_row;
    selected.sel_end_col = caret.sel_end_col;
    const auto count = (std::min)(lattice.rows, static_cast<std::int64_t>(rows.size()));
    for (std::int64_t r = 0; r < count; ++r) {
        const surface::SurfaceTextRow& row = rows[static_cast<std::size_t>(r)];
        std::string text = row.text.substr(0, static_cast<std::size_t>(lattice.columns));
        const auto length = static_cast<std::int64_t>(text.size());
        const surface::RowSpan span = surface::selection_span_of_row(selected, r, length);
        const bool has_caret = caret.row == r && caret.column >= 0 && caret.column <= length;
        std::size_t keep = text.find_last_not_of(' ');
        keep = keep == std::string::npos ? 0 : keep + 1;
        if (span.present()) keep = (std::max)(keep, static_cast<std::size_t>(span.end));
        if (has_caret) keep = (std::max)(keep, static_cast<std::size_t>(caret.column) + 1);
        keep = (std::min)(keep, static_cast<std::size_t>(lattice.columns));
        text.resize(row.background != surface::role::kNone
                        ? static_cast<std::size_t>(lattice.columns)
                        : keep,
                    ' ');
        if (text.empty() && !has_caret) continue;
        out.texts.push_back(v2::PaneCanvasText{
            lattice.x, lattice.row_y(r), std::move(text), row.role,
            has_caret ? caret.column : surface::kNoCaret,
            span.present() ? span.begin : surface::kNoSelection,
            span.present() ? span.end : surface::kNoSelection, false, row.background});
    }
    for (const PaneRowPart& part : parts) {
        if (part.row < 0 || part.row >= lattice.rows || part.columns <= 0) continue;
        out.parts.push_back(PaneCanvasPart{part.name, lattice.column_x(part.column),
                                           lattice.row_y(part.row),
                                           surface::mul_px(part.columns, lattice.advance),
                                           lattice.line});
    }
    return out;
}

/// WHICH PICTURE A PRESS WAS AIMED AT, by what its rows meant. The canvas numbers every picture a
/// pane sends afresh within its room's grant; a pane that numbers its composition by what its
/// rows mean (`component::RowMap`) keeps one number while that stands still. A press is current
/// when the picture it names was drawn, in the grant it names, under the meaning the pane holds
/// now -- so a repaint that moves no row keeps a press aimed at the picture before it.
class CanvasPictures {
public:
    /// The number of the next picture for `room`, drawn under the meaning numbered `meaning`.
    std::int64_t next(const PaneCanvasRoom& room, std::int64_t meaning) {
        if (room.grant != grant_) {
            grant_ = room.grant;
            last_ = 0;
            since_ = 0;
        }
        ++last_;
        if (since_ == 0 || meaning != meaning_) {
            since_ = last_;
            meaning_ = meaning;
        }
        return last_;
    }
    /// Whether `picture` of `grant` was drawn under the meaning of the last picture numbered.
    bool current(std::int64_t grant, std::int64_t picture) const noexcept {
        return grant_ > 0 && grant == grant_ && since_ > 0 && picture >= since_ && picture <= last_;
    }

private:
    std::int64_t grant_ = 0, last_ = 0, since_ = 0, meaning_ = 0;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_CANVAS_ROWS_HPP
