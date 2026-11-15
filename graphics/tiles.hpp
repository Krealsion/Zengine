// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_TILES_HPP
#define ZENGINE_GRAPHICS_TILES_HPP

// A world whose places are tiles: a tile's size as the window that shows it gives it, a tile's box
// and the tile a press lands in, and a grid of tiles drawn as one node from a sheet's frames.
// Reference: graphics/docs/graphics.md, "Sheets, tiles, sprites and points".

#include "graphics/box.hpp"
#include "graphics/operands.hpp"
#include "graphics/pixels.hpp"
#include "graphics/sheet.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace zengine::graphics {

/// A TILE'S SIDE IN A WINDOW, in pixels: `want` floored to whole units of what the window shows
/// (`px_of_one_device` of its `cell_px`), at least one unit -- `want` itself in a window that shows
/// every pixel, a cell in a window of characters for anything under two cells.
inline constexpr std::int64_t tile_px(std::int64_t want, std::int64_t cell_px) noexcept {
    const std::int64_t unit = px_of_one_device(cell_px);
    const std::int64_t units = want / unit;
    return (units < 1 ? 1 : units) * unit;
}

static_assert(tile_px(16, kCellPx) == 16, "a window that shows every pixel shows a tile whole");
static_assert(tile_px(16, 0) == kCellPx, "a window of characters shows a tile as a cell");

/// WHERE A WORLD OF TILES STANDS IN AN AREA: its top left corner, and a tile's side, both in
/// pixels. A place in the world is a tile's column and row, whole or between tiles.
struct TileSpace {
    std::int64_t x = 0, y = 0;
    std::int64_t tile = kCellPx;

    /// A place in the world (columns and rows, fractions of a tile between them), in pixels.
    std::int64_t px_x(double col) const {
        return x + static_cast<std::int64_t>(std::floor(col * static_cast<double>(tile)));
    }
    std::int64_t px_y(double row) const {
        return y + static_cast<std::int64_t>(std::floor(row * static_cast<double>(tile)));
    }
    /// The box of `cols` by `rows` tiles whose top left tile is (col, row).
    Box box(double col, double row, double cols = 1, double rows = 1) const {
        const std::int64_t x0 = px_x(col), y0 = px_y(row);
        return Box{x0, y0, px_x(col + cols) - x0, px_y(row + rows) - y0};
    }
    /// The box a world of `cols` by `rows` tiles fills.
    Box whole(std::int64_t cols, std::int64_t rows) const {
        return Box{x, y, cols * tile, rows * tile};
    }
    /// The tile a point of the area lands in, in a world of `cols` by `rows` tiles; none outside.
    std::optional<std::pair<std::int64_t, std::int64_t>> tile_at(std::int64_t px, std::int64_t py,
                                                                  std::int64_t cols,
                                                                  std::int64_t rows) const {
        if (tile <= 0) {
            return std::nullopt;
        }
        const std::int64_t c = floor_div_px(px - x, tile), r = floor_div_px(py - y, tile);
        if (c < 0 || r < 0 || c >= cols || r >= rows) {
            return std::nullopt;
        }
        return std::make_pair(c, r);
    }
};

/// A GRID OF TILES, each a frame of one sheet or none, drawn as one node (`Sketch::tiles`).
class TileGrid {
public:
    TileGrid(std::int64_t cols, std::int64_t rows)
        : cols_(cols < 0 ? 0 : cols), rows_(rows < 0 ? 0 : rows),
          cells_(static_cast<std::size_t>(cols_ * rows_) * kTileBytes, 0) {}

    std::int64_t cols() const { return cols_; }
    std::int64_t rows() const { return rows_; }
    /// The tile at (col, row) set to show `frame`; one outside the grid is not.
    void set(std::int64_t col, std::int64_t row, FrameId frame) {
        if (col < 0 || row < 0 || col >= cols_ || row >= rows_) {
            return;
        }
        const auto at = static_cast<std::size_t>(row * cols_ + col) * kTileBytes;
        cells_[at] = static_cast<std::uint8_t>(frame.place & 0xff);
        cells_[at + 1] = static_cast<std::uint8_t>((frame.place >> 8) & 0xff);
    }
    /// The frame the tile at (col, row) shows; none outside the grid.
    FrameId at(std::int64_t col, std::int64_t row) const {
        if (col < 0 || row < 0 || col >= cols_ || row >= rows_) {
            return {};
        }
        return FrameId{read_u16(cells_, static_cast<std::size_t>(row * cols_ + col) * kTileBytes)};
    }
    /// Every tile set to show `frame`.
    void fill(FrameId frame) {
        for (std::int64_t r = 0; r < rows_; ++r) {
            for (std::int64_t c = 0; c < cols_; ++c) {
                set(c, r, frame);
            }
        }
    }

    /// The grid as a node draws it: from the sheet at `sheet`, in `space`'s place and tile.
    TilesArgs args(const std::string& sheet, const TileSpace& space) const {
        return TilesArgs{space.x, space.y, cols_, rows_, space.tile, space.tile, sheet, cells_};
    }

private:
    std::int64_t cols_, rows_;
    loom::Bytes cells_;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_TILES_HPP
