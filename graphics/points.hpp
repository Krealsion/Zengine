// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_POINTS_HPP
#define ZENGINE_GRAPHICS_POINTS_HPP

// Points gathered for one node -- particles, sparks, dust: squares of one size, each in a colour
// of its own, later ones over earlier ones. Reference: graphics/docs/graphics.md, "Sheets, tiles,
// sprites and points".

#include "graphics/operands.hpp"
#include "graphics/vocabulary.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace zengine::graphics {

/// POINTS FOR ONE NODE (`Sketch::points`), `size` pixels across, each named by its colour, the
/// colours said once each; a colour past `kMaxPointColors` is not taken, and a place past what 16
/// bits hold is held at their end.
class PointBatch {
public:
    explicit PointBatch(std::int64_t size = 1) : size_(size) {}

    /// A point at (x, y) in `color`. False, and no point, for a colour past the bound.
    bool add(std::int64_t x, std::int64_t y, const Color& color) {
        auto it = std::find(colors_.begin(), colors_.end(), color);
        if (it == colors_.end()) {
            if (colors_.size() >= kMaxPointColors) {
                return false;
            }
            colors_.push_back(color);
            it = colors_.end() - 1;
        }
        put16(x);
        put16(y);
        bytes_.push_back(static_cast<std::uint8_t>((it - colors_.begin()) + 1));
        return true;
    }
    std::size_t size() const { return bytes_.size() / kPointBytes; }
    void clear() {
        bytes_.clear();
        colors_.clear();
    }

    /// The points as a node draws them, placed from (x, y).
    PointsArgs args(std::int64_t x = 0, std::int64_t y = 0) const {
        return PointsArgs{x, y, size_, colors_, bytes_};
    }

private:
    void put16(std::int64_t v) {
        constexpr std::int64_t lo = (std::numeric_limits<std::int16_t>::min)();
        constexpr std::int64_t hi = (std::numeric_limits<std::int16_t>::max)();
        const auto held = static_cast<std::uint16_t>(static_cast<std::int16_t>(v < lo ? lo : v > hi ? hi : v));
        bytes_.push_back(static_cast<std::uint8_t>(held & 0xff));
        bytes_.push_back(static_cast<std::uint8_t>(held >> 8));
    }
    std::int64_t size_;
    std::vector<Color> colors_;
    loom::Bytes bytes_;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_POINTS_HPP
