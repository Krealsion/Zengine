// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_SPRITES_HPP
#define ZENGINE_GRAPHICS_SPRITES_HPP

// Sprites gathered for one node: each a frame of one sheet in a box, later ones over earlier ones.
// Reference: graphics/docs/graphics.md, "Sheets, tiles, sprites and points".

#include "graphics/box.hpp"
#include "graphics/operands.hpp"
#include "graphics/sheet.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace zengine::graphics {

/// SPRITES FOR ONE NODE (`Sketch::sprites`), each a frame in a box of the node's place; a box past
/// what 16 bits hold is held at their end.
class SpriteBatch {
public:
    /// A sprite: `frame` in `box`.
    void add(const Box& box, FrameId frame) {
        for (const std::int64_t v : {box.x, box.y, box.w, box.h}) {
            put16(v);
        }
        bytes_.push_back(static_cast<std::uint8_t>(frame.place & 0xff));
        bytes_.push_back(static_cast<std::uint8_t>((frame.place >> 8) & 0xff));
    }
    std::size_t size() const { return bytes_.size() / kSpriteBytes; }
    void clear() { bytes_.clear(); }

    /// The sprites as a node draws them: from the sheet at `sheet`, placed from (x, y).
    SpritesArgs args(const std::string& sheet, std::int64_t x = 0, std::int64_t y = 0) const {
        return SpritesArgs{x, y, sheet, bytes_};
    }

private:
    void put16(std::int64_t v) {
        constexpr std::int64_t lo = (std::numeric_limits<std::int16_t>::min)();
        constexpr std::int64_t hi = (std::numeric_limits<std::int16_t>::max)();
        const auto held = static_cast<std::uint16_t>(static_cast<std::int16_t>(v < lo ? lo : v > hi ? hi : v));
        bytes_.push_back(static_cast<std::uint8_t>(held & 0xff));
        bytes_.push_back(static_cast<std::uint8_t>(held >> 8));
    }
    loom::Bytes bytes_;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_SPRITES_HPP
