// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENTS_SCROLL_VIEW_HPP
#define ZENGINE_GRAPHICS_ELEMENTS_SCROLL_VIEW_HPP

// A vertical scroll view an owner draws into a Sketch: a box that clips what is drawn in it, a
// content group moved up by the scroll offset, and a bar down its right edge, a track and a thumb
// whose size and place say how much of the content shows and where. The owner draws its content
// under `content_path()`, in the content's own space; the bar's face is the record at `<path>/bar`,
// so its action is the bar's alone and a press on the content reaches the content's own records.

#include "graphics/canvas.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace zengine::graphics {

/// What a drawn scroll bar means to a reader: its box in the view's space, the content's height,
/// the view's height and its scroll offset, the thumb's place and size along the track, and the
/// paints of the track and the thumb.
struct ScrollBarFace {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t content_size = 0;
    std::int64_t viewport_size = 0;
    std::int64_t scroll_offset = 0;
    std::int64_t thumb_offset = 0;
    std::int64_t thumb_size = 0;
    Paint track_fill;
    Paint thumb_fill;
    ZEN_SHAPE(ScrollBarFace, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(content_size), ZEN_FIELD(viewport_size), ZEN_FIELD(scroll_offset),
              ZEN_FIELD(thumb_offset), ZEN_FIELD(thumb_size), ZEN_FIELD(track_fill),
              ZEN_FIELD(thumb_fill));
};

/// A view `h` pixels high onto content `content_size` high, scrolled between 0 and `max_scroll`.
/// The bar is `kCellPx` wide; the content's width is what the bar leaves.
class ScrollView {
public:
    explicit ScrollView(std::string path) : path_(std::move(path)) {}

    void set_bounds(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) {
        x_ = x;
        y_ = y;
        w_ = w;
        h_ = std::max<std::int64_t>(0, h);
        scroll_to(scroll_);
    }
    void set_content_size(std::int64_t content_h) {
        content_ = std::max<std::int64_t>(0, content_h);
        scroll_to(scroll_);
    }
    void scroll_to(std::int64_t offset) {
        scroll_ = std::clamp<std::int64_t>(offset, 0, max_scroll());
    }
    void scroll_by(std::int64_t delta) { scroll_to(scroll_ + delta); }

    const std::string& path() const { return path_; }
    std::string content_path() const { return path_ + "/content"; }
    std::int64_t max_scroll() const { return std::max<std::int64_t>(0, content_ - h_); }
    std::int64_t scroll_offset() const { return scroll_; }
    std::int64_t viewport_size() const { return h_; }
    std::int64_t content_size() const { return content_; }
    std::int64_t content_width() const { return std::max<std::int64_t>(0, w_ - kCellPx); }

    /// The items of height `item_h` the view shows, at least in part: the first, and one past the
    /// last, of the content's items.
    std::pair<std::int64_t, std::int64_t> visible_range(std::int64_t item_h) const {
        if (item_h <= 0) {
            return {0, 0};
        }
        const std::int64_t items = (content_ + item_h - 1) / item_h;
        const std::int64_t first = std::min(scroll_ / item_h, items);
        const std::int64_t end = std::min((scroll_ + h_ + item_h - 1) / item_h, items);
        return {first, std::max(first, end)};
    }
    /// The thumb's size along the track: the part of the content that shows, never less than one
    /// `kCellPx` (nor more than the track), so a medium of characters always shows it.
    std::int64_t thumb_size() const {
        if (content_ <= h_) {
            return h_;
        }
        return std::min(h_, std::max<std::int64_t>(kCellPx, h_ * h_ / content_));
    }
    /// The thumb's place along the track: as far down the track's free length as the scroll is
    /// down the content's.
    std::int64_t thumb_offset() const {
        const std::int64_t room = max_scroll();
        return room == 0 ? 0 : (h_ - thumb_size()) * scroll_ / room;
    }

    /// The bar's record as the state stands.
    ScrollBarFace face() const {
        ScrollBarFace f;
        f.x = content_width();
        f.w = w_ - f.x;
        f.h = h_;
        f.content_size = content_;
        f.viewport_size = h_;
        f.scroll_offset = scroll_;
        f.thumb_offset = thumb_offset();
        f.thumb_size = thumb_size();
        f.track_fill = by_role(role::kMuted);
        f.thumb_fill = by_role(role::kFill);
        return f;
    }

    /// Draws the box, the content group the owner draws under, and the bar; a press on the bar
    /// asks `action` of the area's owner, and nothing when `action` is empty.
    void sketch(Sketch& s, const std::string& action = {}) const {
        const ScrollBarFace f = face();
        const std::string bar = path_ + "/bar";
        s.group(path_, x_, y_, w_, h_);
        s.group(content_path(), 0, -scroll_);
        s.group(bar, f.x, f.y, f.w, f.h);
        s.fill_rect(bar + "/track", 0, 0, f.w, f.h, f.track_fill);
        s.fill_rect(bar + "/thumb", 0, f.thumb_offset, f.w, f.thumb_size, f.thumb_fill);
        s.record(bar, f, action);
    }

private:
    std::string path_;
    std::int64_t x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    std::int64_t content_ = 0;
    std::int64_t scroll_ = 0;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_ELEMENTS_SCROLL_VIEW_HPP
