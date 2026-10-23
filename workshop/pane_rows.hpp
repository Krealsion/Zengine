// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_ROWS_HPP
#define ZENGINE_WORKSHOP_PANE_ROWS_HPP

// A pane's rows of text as values: what a row says and in which roles, the runs of it the pane
// names as its parts, where its caret and selection stand, and the cell of the rows a point falls
// on. A pane composes them and draws them in its own area (`rows_drawing`, pane_drawing.hpp); none
// of them crosses as a message. Reference: workshop/docs/workshop-panes.md, "The pane facets".

#include "graphics/role.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// No caret, and no selection: negative, as `graphics::role::kNone` is, so neither is a row.
inline constexpr std::int64_t kNoCaret = -1;
inline constexpr std::int64_t kNoSelection = -1;

/// ONE ROW OF TEXT: plain text, the role its characters are drawn in, and a ground beneath them
/// (`role::kNone`, the default, shows whatever the area shows there). Plain ASCII: a character is
/// an advance of the window's text, and a byte past it would split.
struct TextRow {
    std::string text;
    std::int64_t role = graphics::role::kFill;
    std::int64_t background = graphics::role::kNone;
    bool operator==(const TextRow&) const = default;
};

/// ONE PART OF A PANE'S ROWS: `columns` columns of row `row` from `column` -- a row entire, or a
/// control inside one -- the pane's name for it, and whether its operation applies now, which a
/// pane draws `[label]` when true and `(label)` when false. A hint, as the drawing is: the pane
/// judges the press again when it arrives. A part that performs nothing, a row, says true.
struct RowPart {
    std::string name;
    std::int64_t row = 0;
    std::int64_t column = 0;
    std::int64_t columns = 0;
    bool available = true;
    bool operator==(const RowPart&) const = default;
};

/// WHERE A CARET AND A SELECTION STAND IN A PANE'S ROWS: the row and the column the caret sits
/// before, and a range in reading order, its end exclusive. `kNoCaret` and `kNoSelection` say none.
struct RowsCaret {
    std::int64_t row = kNoCaret, column = 0;
    std::int64_t sel_begin_row = kNoSelection, sel_begin_col = 0;
    std::int64_t sel_end_row = kNoSelection, sel_end_col = 0;
    bool operator==(const RowsCaret&) const = default;
};

/// A PANE'S ROWS AS COMPOSED: the rows, and the parts of them it names in the order it reads a
/// press.
struct ComposedRows {
    std::vector<TextRow> rows;
    std::vector<RowPart> parts;
};

/// THE CELL OF THE ROWS A PLACE STANDS ON: its row and column, floored, and whether the rows show
/// it. Unclamped, so a drag's place above or below the rows is a row before the first or past the
/// last.
struct RowCell {
    std::int64_t row = 0, column = 0;
    bool shown = false;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_ROWS_HPP
