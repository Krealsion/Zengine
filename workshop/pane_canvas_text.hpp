// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_CANVAS_TEXT_HPP
#define ZENGINE_WORKSHOP_PANE_CANVAS_TEXT_HPP

#include "pane_picture.hpp"
#include "pane_text_fit.hpp"
#include "graphics/pixels.hpp"
#include "graphics/role.hpp"
#include <algorithm>
#include <cstddef>
#include <limits>

// How a run of a canvas picture's text stands in its room: the window's measure for it, and the run
// clipped to whole glyphs and rows. Reference: workshop/docs/workshop-panes.md.

namespace zengine::workshop {

struct CanvasTextBox {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    constexpr bool empty() const noexcept { return w <= 0 || h <= 0; }
};

struct CanvasTextMetrics {
    std::int64_t advance = kPaneCanvasUnit, line = kPaneCanvasUnit, inset = 0;
    std::int64_t grain = kPaneCanvasUnit;
    bool graphical = false;
};

inline constexpr CanvasTextMetrics canvas_text_metrics(const CanvasRoom& room) noexcept {
    CanvasTextMetrics out;
    out.grain = room.grain > 0 ? room.grain : kPaneCanvasUnit;
    if (room.text_advance_px > 0 && room.text_line_px > 0) {
        out.advance = room.text_advance_px;
        out.line = room.text_line_px;
        out.inset = kTextInsetPx;
        out.graphical = true;
    }
    return out;
}

struct CanvasTextLayout {
    CanvasText text;
    /// The ground a run names under its characters (`CanvasText::background`).
    std::int64_t background = graphics::role::kNone;
    CanvasTextBox bounds;
    std::size_t first_column = 0;
    RegionFit fit;
    bool visible() const noexcept { return !bounds.empty(); }
};

// The complete padded region remains inside the clip. Clipping its authored bounds instead
// would change the text's origin/capacity. Here only whole glyphs and whole rows go. A caret
// moves no glyph, and adds no cell: where a character is a cell, one after the run's last
// character stands on that character's cell, so a run gives it the cell after with a blank there.
namespace detail {

// The one clip, for a run whose padded region may reach `slack` canvas pixels past the clip and
// the room above and below, and `slack_left` to its left: 0 for a padded run, the medium's inset
// for an unpadded one moved back to its region's origin. Never to its right, where a caret after
// the run's last character stands, so a caret stays inside the clip as every glyph does.
inline CanvasTextLayout clip_canvas_text_by(const CanvasText& text, const CanvasTextBox& clip,
                                            const CanvasRoom& room, std::int64_t slack,
                                            std::int64_t slack_left) {
    CanvasTextLayout out;
    if (clip.empty() || room.width <= 0 || room.height <= 0) return out;
    const auto m = canvas_text_metrics(room);
    const auto floor_at = [&](std::int64_t v) {
        const auto units = graphics::floor_div_px(v, m.grain);
        const auto least = (std::numeric_limits<std::int64_t>::min)();
        return units < least / m.grain ? least : units * m.grain;
    };
    const auto ceil_at = [&](std::int64_t v) {
        const auto low = floor_at(v);
        return low == v ? low : graphics::add_cells(low, m.grain);
    };
    const auto left = ceil_at(graphics::sub_px((std::max)(std::int64_t{0}, clip.x), slack_left));
    const auto top = ceil_at(graphics::sub_px((std::max)(std::int64_t{0}, clip.y), slack));
    const auto right = floor_at((std::min)(room.width, graphics::add_cells(clip.x, clip.w)));
    const auto bottom = floor_at(graphics::add_cells(
        (std::min)(room.height, graphics::add_cells(clip.y, clip.h)), slack));
    if (right <= left || bottom <= top) return out;
    const auto pad = graphics::mul_px(2, m.inset);
    const auto height = graphics::add_cells(m.line, pad);
    const auto y = floor_at(text.y);
    if (y < top || y >= bottom || height > bottom - y) return out;
    const auto length = (std::min)(text.text.size(), kPaneCanvasMaxTextRunBytes);
    const bool has_caret = text.caret_col >= 0 &&
        text.caret_col <= static_cast<std::int64_t>(length);
    const auto caret = has_caret ? static_cast<std::size_t>(text.caret_col) : std::size_t{0};
    std::size_t first = 0;
    auto x = floor_at(text.x);
    while (x < left && first < length) {
        x = graphics::add_cells(x, m.advance);
        ++first;
    }
    if (x < left || x >= right || pad >= right - x) return out;
    const auto capacity = (right - x - pad) / m.advance;
    if (capacity <= 0) return out;
    const auto take = (std::min)(length - first, static_cast<std::size_t>(capacity));
    const auto end = first + take;
    const bool visible_caret = has_caret && caret >= first && caret <= end;
    if (take == 0 && !visible_caret) return out;
    out.text = CanvasText{x, y, text.text.substr(first, end - first), text.role};
    out.text.caret_col = visible_caret ? static_cast<std::int64_t>(caret - first) : -1;
    out.text.sel_begin_col = out.text.sel_end_col = -1;
    if (text.sel_begin_col >= 0 && text.sel_end_col > text.sel_begin_col) {
        const auto begin = (std::max)(text.sel_begin_col, static_cast<std::int64_t>(first));
        const auto finish = (std::min)(text.sel_end_col, static_cast<std::int64_t>(end));
        if (finish > begin) {
            out.text.sel_begin_col = begin - static_cast<std::int64_t>(first);
            out.text.sel_end_col = finish - static_cast<std::int64_t>(first);
        }
    }
    const auto columns = static_cast<std::int64_t>((std::max)(std::size_t{1}, take));
    const auto width = graphics::add_cells(graphics::mul_px(columns, m.advance), pad);
    out.bounds = {x, y, width, height};
    out.first_column = first;
    out.fit = fit_region(x, y, width, height,
                                      room.text_advance_px, room.text_line_px);
    return out;
}

} // namespace detail

inline CanvasTextLayout clip_canvas_text(const CanvasText& text, const CanvasTextBox& clip,
                                         const CanvasRoom& room) {
    return detail::clip_canvas_text_by(text, clip, room, 0, 0);
}

} // namespace zengine::workshop
#endif
