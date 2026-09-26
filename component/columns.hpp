// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_COLUMNS_HPP
#define ZENGINE_COMPONENT_COLUMNS_HPP

// A table's columns, laid out once for every row: widths chosen from what the population wants,
// within the room the pane was granted, so every row's cells begin at the same column and a
// press on a cell is answered by its column. It owns the layout arithmetic only; cutting
// (`fit`) and padding (`pad`) are the consumer's (`workshop/pane_text.hpp`), and the widths are
// spent twice -- to write the line and to record the spans (`RowMap`): one layout draws and hits.
// Reference: docs/reference/component.md.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::component {

/// ONE COLUMN'S ASK: the width its widest cell wants, and the least it may be cut to. A
/// column that may not be cut at all says `min == want`.
struct Column {
    std::size_t want = 0;
    std::size_t min = 1;
};

/// THE WIDTHS THE COLUMNS GET INSIDE `budget`, with `gap` blank columns between neighbours.
///
/// Every column gets what it wants when the room allows. When it does not, columns are cut
/// from the LAST to the first, each down to its `min`, until the row fits; a table that still
/// does not fit at every minimum is returned at those minimums and the consumer's `fit` cuts
/// the line, marking the cut, exactly as it cuts any row. TOTAL over any budget.
inline std::vector<std::size_t> layout_columns(const std::vector<Column>& columns,
                                               std::size_t budget, std::size_t gap = 1) {
    std::vector<std::size_t> widths;
    widths.reserve(columns.size());
    std::size_t total = 0;
    for (const Column& c : columns) {
        const std::size_t w = c.want < c.min ? c.min : c.want;
        widths.push_back(w);
        total += w;
    }
    if (columns.size() > 1) {
        total += gap * (columns.size() - 1);
    }
    for (std::size_t i = columns.size(); i > 0 && total > budget; --i) {
        const std::size_t idx = i - 1;
        const std::size_t floor = columns[idx].min;
        const std::size_t excess = total - budget;
        const std::size_t give = widths[idx] > floor ? widths[idx] - floor : 0;
        const std::size_t cut = give < excess ? give : excess;
        widths[idx] -= cut;
        total -= cut;
    }
    return widths;
}

/// WHERE EACH COLUMN BEGINS, given the widths: the offsets a span is recorded at.
inline std::vector<std::size_t> column_offsets(const std::vector<std::size_t>& widths,
                                               std::size_t gap = 1) {
    std::vector<std::size_t> offsets;
    offsets.reserve(widths.size());
    std::size_t at = 0;
    for (std::size_t i = 0; i < widths.size(); ++i) {
        offsets.push_back(at);
        at += widths[i] + gap;
    }
    return offsets;
}

/// ONE ROW OF THE TABLE: each cell cut to its width by the consumer's `fit` and padded to it by
/// the consumer's `pad`, joined by `gap` spaces. A cell wider than its column is cut with the
/// consumer's mark, never silently. `fit(text, width)` and `pad(text, width)` are
/// `workshop/pane_text.hpp`'s signatures.
template <class Fit, class Pad>
std::string table_line(const std::vector<std::string>& cells,
                       const std::vector<std::size_t>& widths, Fit fit, Pad pad,
                       std::size_t gap = 1) {
    std::string out;
    for (std::size_t i = 0; i < cells.size() && i < widths.size(); ++i) {
        if (i > 0) {
            out.append(gap, ' ');
        }
        const bool last = i + 1 == cells.size() || i + 1 == widths.size();
        std::string cell = fit(cells[i], static_cast<std::int64_t>(widths[i]));
        out += last ? cell : pad(std::move(cell), widths[i]);
    }
    return out;
}

/// THE WIDTH THE WIDEST CELL OF ONE COLUMN WANTS -- what a consumer asks of its population
/// before laying out, so the columns are coherent across every row and not per row.
template <class Population, class CellOf>
std::size_t widest(const Population& population, CellOf cell_of) {
    std::size_t w = 0;
    for (const auto& member : population) {
        const std::size_t here = cell_of(member).size();
        w = here > w ? here : w;
    }
    return w;
}

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_COLUMNS_HPP
