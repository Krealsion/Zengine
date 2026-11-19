// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_BOX_HPP
#define ZENGINE_GRAPHICS_BOX_HPP

// A place and a size by name: a rectangle in an area's pixels that a drawer names once and passes
// where a call wants four numbers. Reference: graphics/docs/graphics.md, "Helpers".

#include <cstdint>

namespace zengine::graphics {

/// A PLACE AND A SIZE, in pixels: (x, y) its top left corner, w by h.
struct Box {
    std::int64_t x = 0, y = 0, w = 0, h = 0;

    std::int64_t right() const { return x + w; }
    std::int64_t bottom() const { return y + h; }
    /// Whether the point (px, py) lies inside: its left and top edges in, its right and bottom out.
    bool contains(std::int64_t px, std::int64_t py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    /// The box shrunk by `dx` on its left and right and `dy` on its top and bottom.
    Box inset(std::int64_t dx, std::int64_t dy) const {
        return {x + dx, y + dy, w - 2 * dx, h - 2 * dy};
    }
    /// A box of the same size to the right of this one, `gap` apart.
    Box beside(std::int64_t gap = 0) const { return {x + w + gap, y, w, h}; }
    /// A box of the same size below this one, `gap` apart.
    Box below(std::int64_t gap = 0) const { return {x, y + h + gap, w, h}; }
    /// The same box moved to (nx, ny).
    Box at(std::int64_t nx, std::int64_t ny) const { return {nx, ny, w, h}; }
    /// The same place at another size.
    Box sized(std::int64_t nw, std::int64_t nh) const { return {x, y, nw, nh}; }
    friend bool operator==(const Box&, const Box&) = default;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_BOX_HPP
