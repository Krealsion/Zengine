// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENTS_GRID_HPP
#define ZENGINE_GRAPHICS_ELEMENTS_GRID_HPP

// A grid an owner draws into a Sketch: a board of cells of the owner's own type, each drawn at
// `<path>/c.<col>.<row>` with its face as the record there, and only while it holds something
// other than an empty `Cell{}`. A canvas sends only the cells a picture changed, so a step that
// changes two cells sends two; a cell emptied is removed.

#include "graphics/canvas.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::graphics {

/// What one drawn cell means to a reader: its box in the grid's space, its fill, and the glyph
/// drawn over it ("" for none).
struct GridCellFace {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    Paint fill;
    std::string glyph;
    ZEN_SHAPE(GridCellFace, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(fill), ZEN_FIELD(glyph));
};

/// A board of `cols` by `rows` cells, each `Cell{}` until set. `Cell` is default-constructible
/// and compares with `==`. A cell is `cell_w` by `cell_h` of the area's pixels, a terminal cell
/// each by default.
template <class Cell>
class Grid {
public:
    Grid(std::string path, std::int64_t cols, std::int64_t rows, std::int64_t cell_w = kCellPx,
         std::int64_t cell_h = kCellPx)
        : path_(std::move(path)), cols_(cols > 0 ? cols : 0), rows_(rows > 0 ? rows : 0),
          cell_w_(cell_w), cell_h_(cell_h),
          cells_(static_cast<std::size_t>(cols_ * rows_), Cell{}) {}

    void set_origin(std::int64_t x, std::int64_t y) {
        x_ = x;
        y_ = y;
    }
    void set_cell_size(std::int64_t w, std::int64_t h) {
        cell_w_ = w;
        cell_h_ = h;
    }
    /// Puts `value` in a cell; false, changing nothing, for a cell off the board.
    bool set(std::int64_t col, std::int64_t row, Cell value) {
        if (!in_bounds(col, row)) {
            return false;
        }
        cells_[index(col, row)] = std::move(value);
        return true;
    }
    bool clear(std::int64_t col, std::int64_t row) { return set(col, row, Cell{}); }
    void clear_all() { fill(Cell{}); }
    void fill(const Cell& value) { std::fill(cells_.begin(), cells_.end(), value); }
    /// The cell's value; `Cell{}` for a cell off the board.
    Cell at(std::int64_t col, std::int64_t row) const {
        return in_bounds(col, row) ? cells_[index(col, row)] : Cell{};
    }
    std::int64_t width() const { return cols_; }
    std::int64_t height() const { return rows_; }
    bool in_bounds(std::int64_t col, std::int64_t row) const {
        return col >= 0 && row >= 0 && col < cols_ && row < rows_;
    }
    const std::string& path() const { return path_; }
    std::string cell_path(std::int64_t col, std::int64_t row) const {
        return path_ + "/c." + std::to_string(col) + "." + std::to_string(row);
    }

    /// Draws every cell that is not empty as `face_of(col, row, cell)` faces it; a press on a cell
    /// asks `action_for(col, row, cell)` of the area's owner, and nothing when that is empty.
    template <class FaceOf, class ActionFor>
    void sketch(Sketch& s, FaceOf&& face_of, ActionFor&& action_for) const {
        s.group(path_, x_, y_, cols_ * cell_w_, rows_ * cell_h_);
        for (std::int64_t row = 0; row < rows_; ++row) {
            for (std::int64_t col = 0; col < cols_; ++col) {
                const Cell& cell = cells_[index(col, row)];
                if (cell == Cell{}) {
                    continue;
                }
                GridCellFace f = face_of(col, row, cell);
                f.x = col * cell_w_;
                f.y = row * cell_h_;
                f.w = cell_w_;
                f.h = cell_h_;
                const std::string p = cell_path(col, row);
                s.group(p, f.x, f.y, f.w, f.h);
                s.fill_rect(p + "/fill", 0, 0, f.w, f.h, f.fill);
                if (!f.glyph.empty()) {
                    s.text(p + "/glyph", 0, 0, f.glyph, by_role(role::kFill));
                }
                s.record(p, f, std::string(action_for(col, row, cell)));
            }
        }
    }
    /// Draws every cell that is not empty, with no action: a press on one sends nothing.
    template <class FaceOf>
    void sketch(Sketch& s, FaceOf&& face_of) const {
        sketch(s, std::forward<FaceOf>(face_of),
               [](std::int64_t, std::int64_t, const Cell&) { return std::string(); });
    }

private:
    std::size_t index(std::int64_t col, std::int64_t row) const {
        return static_cast<std::size_t>(row * cols_ + col);
    }

    std::string path_;
    std::int64_t cols_ = 0, rows_ = 0;
    std::int64_t cell_w_ = kCellPx, cell_h_ = kCellPx;
    std::int64_t x_ = 0, y_ = 0;
    std::vector<Cell> cells_;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_ELEMENTS_GRID_HPP
