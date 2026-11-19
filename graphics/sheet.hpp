// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_SHEET_HPP
#define ZENGINE_GRAPHICS_SHEET_HPP

// A sheet made in code: its pixels, laid out in slots of one size, and the frames cut from them,
// each with the character a window of characters shows it as. Drawn as one node
// (`Sketch::sheet`); tiles and sprites name its frames by `FrameId`. Reference:
// graphics/docs/graphics.md, "Sheets, tiles, sprites and points".

#include "graphics/box.hpp"
#include "graphics/operands.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace zengine::graphics {

/// A FRAME AS TILES AND SPRITES NAME IT: its place among its sheet's frames, from 1; none is 0.
struct FrameId {
    std::int64_t place = 0;
    bool none() const { return place == 0; }
    friend bool operator==(const FrameId&, const FrameId&) = default;
};

/// One pixel's colour: red, green, blue and alpha.
struct Rgba {
    std::uint8_t r = 0, g = 0, b = 0, a = 0;
    friend bool operator==(const Rgba&, const Rgba&) = default;
};

/// A colour as 0xRRGGBBAA, as a pixel.
inline constexpr Rgba rgba_of(std::uint32_t rrggbbaa) {
    return Rgba{static_cast<std::uint8_t>(rrggbbaa >> 24), static_cast<std::uint8_t>(rrggbbaa >> 16),
                static_cast<std::uint8_t>(rrggbbaa >> 8), static_cast<std::uint8_t>(rrggbbaa)};
}

/// A SHEET IN THE MAKING: `across` by `down` slots of `slot_w` by `slot_h` pixels, every pixel
/// clear until it is painted, and the frames cut so far.
class Sheet {
public:
    Sheet(std::int64_t slot_w, std::int64_t slot_h, std::int64_t across, std::int64_t down)
        : slot_w_(slot_w), slot_h_(slot_h), across_(across), down_(down) {
        if (slot_w <= 0 || slot_h <= 0 || across <= 0 || down <= 0) {
            throw std::invalid_argument("zengine::graphics::Sheet: a sheet of no slots");
        }
        args_.pw = slot_w * across;
        args_.ph = slot_h * down;
        args_.rgba.assign(static_cast<std::size_t>(args_.pw * args_.ph * 4), 0);
    }

    std::int64_t slot_w() const { return slot_w_; }
    std::int64_t slot_h() const { return slot_h_; }
    std::int64_t slots() const { return across_ * down_; }
    /// The slot `i`'s box among the sheet's pixels, row by row from the top left.
    Box slot(std::int64_t i) const {
        if (i < 0 || i >= slots()) {
            throw std::out_of_range("zengine::graphics::Sheet: slot " + std::to_string(i) +
                                    " past the sheet's " + std::to_string(slots()));
        }
        return Box{(i % across_) * slot_w_, (i / across_) * slot_h_, slot_w_, slot_h_};
    }
    /// The next slot no frame has taken, which `frame` takes.
    Box next_slot() const { return slot(next_); }
    /// The next slot taken for frames `cut` from it, as several frames sharing one slot are.
    Box take_slot() { return slot(next_++); }

    /// A FRAME CUT FROM THE NEXT SLOT: its pixels, as `paint(box)` paints them there, and the
    /// character a window of characters shows it as, in `ink` on `ground`.
    template <class Paint>
    FrameId frame(Paint&& paint, std::string glyph, Color ink, Color ground = no_color()) {
        const Box box = slot(next_++);
        paint(box);
        return cut(box, std::move(glyph), std::move(ink), std::move(ground));
    }
    /// A frame of the pixels in `box`, which another frame may share.
    FrameId cut(const Box& box, std::string glyph, Color ink, Color ground = no_color()) {
        args_.frames.push_back(
            Frame{box.x, box.y, box.w, box.h, std::move(glyph), std::move(ink), std::move(ground)});
        return FrameId{static_cast<std::int64_t>(args_.frames.size())};
    }
    /// A frame with no pixels: its character, in `ink` on `ground`, in either window.
    FrameId character(std::string glyph, Color ink, Color ground = no_color()) {
        return cut(Box{}, std::move(glyph), std::move(ink), std::move(ground));
    }

    // ---- its pixels ------------------------------------------------------------------------------
    /// One pixel painted; one outside the sheet is not.
    void put(std::int64_t x, std::int64_t y, Rgba c) {
        if (x < 0 || y < 0 || x >= args_.pw || y >= args_.ph) {
            return;
        }
        const auto at = static_cast<std::size_t>((y * args_.pw + x) * 4);
        args_.rgba[at] = c.r;
        args_.rgba[at + 1] = c.g;
        args_.rgba[at + 2] = c.b;
        args_.rgba[at + 3] = c.a;
    }
    /// One pixel's colour; clear outside the sheet.
    Rgba at(std::int64_t x, std::int64_t y) const {
        if (x < 0 || y < 0 || x >= args_.pw || y >= args_.ph) {
            return {};
        }
        const auto at = static_cast<std::size_t>((y * args_.pw + x) * 4);
        return Rgba{args_.rgba[at], args_.rgba[at + 1], args_.rgba[at + 2], args_.rgba[at + 3]};
    }
    /// Every pixel of `box` painted.
    void fill(const Box& box, Rgba c) {
        for (std::int64_t y = box.y; y < box.bottom(); ++y) {
            for (std::int64_t x = box.x; x < box.right(); ++x) {
                put(x, y, c);
            }
        }
    }

    /// The sheet as a node draws it.
    const SheetArgs& args() const { return args_; }

private:
    std::int64_t slot_w_, slot_h_, across_, down_;
    std::int64_t next_ = 0;
    SheetArgs args_;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_SHEET_HPP
