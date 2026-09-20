// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_CANVAS_TEXT_HPP
#define ZENGINE_WORKSHOP_PANE_CANVAS_TEXT_HPP

#include "pane_canvas_vocabulary.hpp"
#include "surface/pointing.hpp"
#include <algorithm>
#include <cstddef>
#include <limits>

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

inline constexpr CanvasTextMetrics canvas_text_metrics(const PaneCanvasRoom& room) noexcept {
    CanvasTextMetrics out;
    out.grain = room.grain > 0 ? room.grain : kPaneCanvasUnit;
    if (room.text_advance_px > 0 && room.text_line_px > 0) {
        out.advance = surface::subs_of_pixel(room.text_advance_px);
        out.line = surface::subs_of_pixel(room.text_line_px);
        out.inset = surface::subs_of_pixel(surface::kTextInsetPx);
        out.graphical = true;
    }
    return out;
}

struct CanvasTextLayout {
    PaneCanvasText text;
    CanvasTextBox bounds;
    std::size_t first_column = 0;
    surface::RegionFit fit;
    bool visible() const noexcept { return !bounds.empty(); }
};

// The complete padded region remains inside the clip. Clipping its authored bounds instead
// would change the text's origin/capacity. Here only whole display glyphs and whole rows go.
// The fallback's inserted caret is one display glyph, so cropping it cannot shift its suffix.
inline CanvasTextLayout clip_canvas_text(const PaneCanvasText& text, const CanvasTextBox& clip,
                                         const PaneCanvasRoom& room) {
    CanvasTextLayout out;
    if (clip.empty() || room.width <= 0 || room.height <= 0) return out;
    const auto m = canvas_text_metrics(room);
    const auto floor_at = [&](std::int64_t v) {
        const auto units = surface::floor_div_px(v, m.grain);
        const auto least = (std::numeric_limits<std::int64_t>::min)();
        return units < least / m.grain ? least : units * m.grain;
    };
    const auto ceil_at = [&](std::int64_t v) {
        const auto low = floor_at(v);
        return low == v ? low : surface::add_cells(low, m.grain);
    };
    const auto left = ceil_at((std::max)(std::int64_t{0}, clip.x));
    const auto top = ceil_at((std::max)(std::int64_t{0}, clip.y));
    const auto right = floor_at((std::min)(room.width, surface::add_cells(clip.x, clip.w)));
    const auto bottom = floor_at((std::min)(room.height, surface::add_cells(clip.y, clip.h)));
    if (right <= left || bottom <= top) return out;
    const auto pad = surface::mul_px(2, m.inset);
    const auto height = surface::add_cells(m.line, pad);
    const auto y = floor_at(text.y);
    if (y < top || y >= bottom || height > bottom - y) return out;
    const auto length = (std::min)(text.text.size(), kPaneCanvasMaxTextRunBytes);
    const bool has_caret = text.caret_col >= 0 &&
        text.caret_col <= static_cast<std::int64_t>(length);
    const bool inserted_caret = has_caret && !m.graphical;
    const auto caret = has_caret ? static_cast<std::size_t>(text.caret_col) : std::size_t{0};
    const auto display_length = length + (inserted_caret ? 1u : 0u);
    std::size_t first_display = 0;
    auto x = floor_at(text.x);
    while (x < left && first_display < display_length) {
        x = surface::add_cells(x, m.advance);
        ++first_display;
    }
    if (x < left || x >= right || pad >= right - x) return out;
    const auto capacity = (right - x - pad) / m.advance;
    if (capacity <= 0) return out;
    const auto take = (std::min)(display_length - first_display,
                                static_cast<std::size_t>(capacity));
    const auto end_display = first_display + take;
    const auto first = first_display - (inserted_caret && caret < first_display ? 1u : 0u);
    const auto end = end_display - (inserted_caret && caret < end_display ? 1u : 0u);
    const bool visible_caret = has_caret && (inserted_caret
        ? caret >= first_display && caret < end_display
        : caret >= first && caret <= end);
    if (take == 0 && !visible_caret) return out;
    out.text = PaneCanvasText{x, y, text.text.substr(first, end - first), text.role};
    out.text.caret_col = visible_caret ? static_cast<std::int64_t>(caret - first) : surface::kNoCaret;
    out.text.sel_begin_col = out.text.sel_end_col = surface::kNoSelection;
    if (text.sel_begin_col >= 0 && text.sel_end_col > text.sel_begin_col) {
        const auto begin = (std::max)(text.sel_begin_col, static_cast<std::int64_t>(first));
        const auto finish = (std::min)(text.sel_end_col, static_cast<std::int64_t>(end));
        if (finish > begin) {
            out.text.sel_begin_col = begin - static_cast<std::int64_t>(first);
            out.text.sel_end_col = finish - static_cast<std::int64_t>(first);
        }
    }
    const auto columns = static_cast<std::int64_t>((std::max)(std::size_t{1}, take));
    const auto width = surface::add_cells(surface::mul_px(columns, m.advance), pad);
    out.bounds = {x, y, width, height};
    out.first_column = first;
    out.fit = surface::fit_region_subs(x, y, width, height,
                                      room.text_advance_px, room.text_line_px);
    return out;
}

inline surface::SurfaceTextRegion canvas_text_region(const CanvasTextLayout& text,
                                                       std::int64_t offset_x = 0,
                                                       std::int64_t offset_y = 0) {
    surface::SurfaceTextRegion region;
    if (!text.visible()) return region;
    const auto x = surface::add_cells(text.bounds.x, offset_x);
    const auto y = surface::add_cells(text.bounds.y, offset_y);
    const auto split = [](std::int64_t value, std::int64_t& cells, std::int64_t& rem) {
        cells = surface::cell_of_subs(value);
        rem = value % kPaneCanvasUnit;
        if (rem < 0) rem += kPaneCanvasUnit;
    };
    split(x, region.x, region.sub_x);
    split(y, region.y, region.sub_y);
    split(text.bounds.w, region.w, region.sub_w);
    split(text.bounds.h, region.h, region.sub_h);
    region.ground = surface::kGroundBeneath;
    region.rows.push_back({text.text.text, text.text.role});
    if (text.text.caret_col >= 0) {
        region.caret_row = 0;
        region.caret_col = text.text.caret_col;
    }
    if (text.text.sel_begin_col >= 0 && text.text.sel_end_col > text.text.sel_begin_col) {
        region.sel_begin_row = region.sel_end_row = 0;
        region.sel_begin_col = text.text.sel_begin_col;
        region.sel_end_col = text.text.sel_end_col;
    }
    return region;
}

} // namespace zengine::workshop
#endif
