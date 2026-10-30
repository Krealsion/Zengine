// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_ROWS_HPP
#define ZENGINE_WORKSHOP_PANE_ROWS_HPP

// A pane's rows of text as values: what a row says and in which roles, the runs of it the pane
// names as its parts, where its caret and selection stand, and the cell of the rows a point falls
// on. A pane composes them and draws them in its own area (`rows_drawing`, pane_drawing.hpp); none
// of them crosses as a message. Reference: workshop/docs/workshop-panes.md, "The pane facets".

#include "component/text_box.hpp"
#include "graphics/elements/text_face.hpp"
#include "graphics/role.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// No caret, and no selection: negative, as `graphics::role::kNone` is, so neither is a row.
inline constexpr std::int64_t kNoCaret = -1;
inline constexpr std::int64_t kNoSelection = -1;
/// No fold mark on a row.
inline constexpr std::int64_t kNoMark = -1;

/// A TEXT BOX STANDING ON A ROW: its first column, its whole width, and what it shows there
/// (`component::text_box_view`, measured for `graphics::text_box_room(columns)`). The row carries the
/// box, never its look: a room of cells shows it `[text___]` and a window that sets type its outline
/// (`graphics/elements/text_face.hpp`). None where `column` is negative.
struct RowBox {
    std::int64_t column = kNoMark;
    std::int64_t columns = 0;
    component::TextBoxView view{};
    bool present() const noexcept { return column >= 0 && columns > 0; }
    bool operator==(const RowBox&) const = default;
};

/// A box `columns` wide at `column` of its row, showing `box` through the window it keeps round
/// its caret -- a column short of the room, so a caret after the text stands on the box's blank
/// room and never on its closing edge.
inline RowBox row_box(component::TextBox& box, std::int64_t column, std::int64_t columns) {
    const std::int64_t room = graphics::text_box_room(columns);
    box.keep_caret_visible(room > 1 ? room - 1 : room);
    return RowBox{column, columns,
                  component::text_box_view(static_cast<const component::TextBox&>(box), room)};
}
/// ...and of a box its owner reads only.
inline RowBox row_box(const component::TextBox& box, std::int64_t column, std::int64_t columns) {
    return RowBox{column, columns,
                  component::text_box_view(box, graphics::text_box_room(columns))};
}

/// ONE ROW OF TEXT: plain text, the role its characters are drawn in, and a ground beneath them
/// (`role::kNone`, the default, shows whatever the area shows there). Plain ASCII: a character is
/// an advance of the window's text, and a byte past it would split. `mark` is the column of a
/// section's fold mark, `+` or `-` (`component::fold_mark`): a medium of characters draws the
/// character, and a window that sets type the icon table's `right` or `down` in its place. `box` is
/// a text box standing on the row, over whatever the text says in its columns: each room shows its
/// own face of it (`row_text_shown`).
struct TextRow {
    std::string text;
    std::int64_t role = graphics::role::kFill;
    std::int64_t background = graphics::role::kNone;
    std::int64_t mark = kNoMark;
    RowBox box{};
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

/// THE CARET AND SELECTION OF A ROW'S BOX on row `row`, in the rows' lattice: inside the box's
/// inset, where both faces put its text.
inline RowsCaret box_caret(const RowBox& box, std::int64_t row) {
    RowsCaret caret;
    const std::int64_t at = box.column + graphics::kTextBoxInset;
    caret.row = row;
    caret.column = at + box.view.caret;
    if (box.view.selected()) {
        caret.sel_begin_row = row;
        caret.sel_end_row = row;
        caret.sel_begin_col = at + box.view.selection_begin;
        caret.sel_end_col = at + box.view.selection_end;
    }
    return caret;
}

/// A ROW'S TEXT AS A ROOM SHOWS IT: its text, with its box's face written over the box's columns --
/// `[text___]` in a room of cells, the box's text inside blank margins where the window sets type
/// (its outline is drawn beside, `text_box_outline`).
inline std::string row_text_shown(const TextRow& row, bool cells) {
    if (!row.box.present()) {
        return row.text;
    }
    std::string out = row.text;
    const auto at = static_cast<std::size_t>(row.box.column);
    const auto n = static_cast<std::size_t>(row.box.columns);
    if (out.size() < at + n) {
        out.resize(at + n, ' ');
    }
    out.replace(at, n, cells ? graphics::text_box_spelling(row.box.view, row.box.columns)
                             : graphics::text_box_lettering(row.box.view, row.box.columns));
    return out;
}

/// A PANE'S ROWS AS COMPOSED: the rows, and the parts of them it names in the order it reads a
/// press.
struct ComposedRows {
    std::vector<TextRow> rows;
    std::vector<RowPart> parts;
    /// The pane's number for what the rows mean (`component::RowMap::picture`), 0 for none.
    std::int64_t number = 0;
};

/// THE CELL OF THE ROWS A PLACE STANDS ON: its row and column, floored, and whether the rows show
/// it. Unclamped, so a drag's place above or below the rows is a row before the first or past the
/// last.
struct RowCell {
    std::int64_t row = 0, column = 0;
    bool shown = false;
    bool operator==(const RowCell&) const = default;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_ROWS_HPP
