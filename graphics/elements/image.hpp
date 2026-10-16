// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENTS_IMAGE_HPP
#define ZENGINE_GRAPHICS_ELEMENTS_IMAGE_HPP

// An image an owner draws into a Sketch: a rectangle of a sheet of pixels the owner holds inline,
// chosen by its uv corners and shown in a box with the `image` operator, tinted when the tint is
// an exact colour. With no pixels it is drawn as a muted frame and the words of its source, which
// are its record's too, for a reader and for a medium that shows no pixels.

#include "graphics/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace zengine::graphics {

/// What a drawn image means to a reader: its box in its parent's space, the words that name its
/// source, the rectangle of the sheet it shows (0 to 1 across and down), and its tint.
struct ImageFace {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::string source;
    double u0 = 0.0, v0 = 0.0, u1 = 1.0, v1 = 1.0;
    Paint tint;
    ZEN_SHAPE(ImageFace, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(source), ZEN_FIELD(u0), ZEN_FIELD(v0), ZEN_FIELD(u1), ZEN_FIELD(v1),
              ZEN_FIELD(tint));
};

/// A sheet of `pw` by `ph` pixels, four bytes each (red, green, blue, alpha), and the rectangle of
/// it shown: the whole sheet by default, one sprite of it by `set_uv`. The pixels the slice
/// selects travel as one node's operands, so a slice larger than the window's operand bound is
/// refused there, in words.
class Image {
public:
    /// The pixels a uv rectangle selects, tinted: their width, their height and their bytes.
    struct Slice {
        std::int64_t pw = 0, ph = 0;
        loom::Bytes rgba;
    };

    explicit Image(std::string path, std::string source = {})
        : path_(std::move(path)), source_(std::move(source)) {}

    void set_source(std::string source) { source_ = std::move(source); }
    void set_bounds(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) {
        x_ = x;
        y_ = y;
        w_ = w;
        h_ = h;
    }
    /// The rectangle shown, each corner held between 0 and 1; one whose right or bottom edge is
    /// not past its left or top selects no pixels.
    void set_uv(double u0, double v0, double u1, double v1) {
        u0_ = std::clamp(u0, 0.0, 1.0);
        v0_ = std::clamp(v0, 0.0, 1.0);
        u1_ = std::clamp(u1, 0.0, 1.0);
        v1_ = std::clamp(v1, 0.0, 1.0);
    }
    /// A tint with an exact colour multiplies each pixel's red, green and blue by its own; one
    /// without (`no_paint()`, the default, or a role alone) leaves the pixels as they are.
    void set_tint(Paint tint) { tint_ = tint; }
    /// The sheet; false, keeping no pixels, when `rgba` is not `pw` by `ph` by four bytes.
    bool set_pixels(std::int64_t pw, std::int64_t ph, loom::Bytes rgba) {
        if (pw <= 0 || ph <= 0 || rgba.size() != static_cast<std::size_t>(pw * ph * 4)) {
            pw_ = ph_ = 0;
            rgba_.clear();
            return false;
        }
        pw_ = pw;
        ph_ = ph;
        rgba_ = std::move(rgba);
        return true;
    }

    const std::string& path() const { return path_; }
    const std::string& source() const { return source_; }
    const Paint& tint() const { return tint_; }
    bool has_pixels() const { return !rgba_.empty(); }

    /// The pixels the uv rectangle selects from the sheet, tinted.
    Slice slice() const {
        const auto edge = [](double at, std::int64_t extent, bool up) {
            const double px = at * static_cast<double>(extent);
            return static_cast<std::int64_t>(up ? std::ceil(px) : std::floor(px));
        };
        const std::int64_t x0 = edge(u0_, pw_, false), x1 = edge(u1_, pw_, true);
        const std::int64_t y0 = edge(v0_, ph_, false), y1 = edge(v1_, ph_, true);
        Slice out;
        if (x1 <= x0 || y1 <= y0) {
            return out;
        }
        out.pw = x1 - x0;
        out.ph = y1 - y0;
        out.rgba.reserve(static_cast<std::size_t>(out.pw * out.ph * 4));
        for (std::int64_t y = y0; y < y1; ++y) {
            const auto from = rgba_.begin() + static_cast<std::ptrdiff_t>((y * pw_ + x0) * 4);
            out.rgba.insert(out.rgba.end(), from, from + static_cast<std::ptrdiff_t>(out.pw * 4));
        }
        if (tint_.rgb >= 0) {
            const std::int64_t by[3] = {(tint_.rgb >> 16) & 0xff, (tint_.rgb >> 8) & 0xff,
                                        tint_.rgb & 0xff};
            for (std::size_t i = 0; i < out.rgba.size(); ++i) {
                if (i % 4 != 3) {
                    out.rgba[i] = static_cast<std::uint8_t>(
                        static_cast<std::int64_t>(out.rgba[i]) * by[i % 4] / 255);
                }
            }
        }
        return out;
    }

    /// The record as the state stands.
    ImageFace face() const {
        return ImageFace{x_, y_, w_, h_, source_, u0_, v0_, u1_, v1_, tint_};
    }

    /// Draws the image at its path; a press on it asks `action` of the area's owner, and nothing
    /// when `action` is empty.
    void sketch(Sketch& s, const std::string& action = {}) const {
        s.group(path_, x_, y_, w_, h_);
        Slice sl = slice();
        if (sl.rgba.empty()) {
            s.outline_rect(path_ + "/frame", 0, 0, w_, h_, by_role(role::kMuted));
            s.text(path_ + "/words", 0, 0, source_, by_role(role::kMuted));
        } else {
            s.image(path_ + "/pixels", 0, 0, w_, h_, sl.pw, sl.ph, std::move(sl.rgba));
        }
        s.record(path_, face(), action);
    }

private:
    std::string path_;
    std::string source_;
    std::int64_t x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    double u0_ = 0.0, v0_ = 0.0, u1_ = 1.0, v1_ = 1.0;
    Paint tint_;
    std::int64_t pw_ = 0, ph_ = 0;
    loom::Bytes rgba_;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_ELEMENTS_IMAGE_HPP
