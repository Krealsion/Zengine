// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENTS_TEXT_FACE_HPP
#define ZENGINE_GRAPHICS_ELEMENTS_TEXT_FACE_HPP

// The text pieces as a window that sets type draws them: a text box's outline, its text, caret and
// selection, and a text area scrolled freely with its bar. Stateless: every face is drawn from the
// value its owner keeps (`component/text_box.hpp`, `component/text_area.hpp`) and keeps nothing,
// and the geometry a face is drawn with is the one its owner reads a press with
// (`text_area_geometry`), so a press lands where the face showed. A medium of characters spells the
// same values in cells instead (`component::text_box_cells`, `component::TextArea::cells`).
// Reference: graphics/docs/graphics.md, "Drawing the text pieces".

#include "component/text_area.hpp"
#include "component/text_box.hpp"
#include "graphics/canvas.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zengine::graphics {

/// THE BAR OF A TEXT AREA, in a window's pixels: the room kept on the area's right whether or not
/// the text overflows, so nothing reflows when the bar appears, and pressed across all of it; the
/// width it is drawn; and the shortest its thumb is drawn.
inline constexpr std::int64_t kTextAreaBarRoom = 16;
inline constexpr std::int64_t kTextAreaBarDrawn = 8;
inline constexpr std::int64_t kTextAreaThumbLeast = 16;

/// A BOX'S OUTLINE: one pixel in `role::kEdge` round `w` by `h` at `x`, `y`.
inline void text_box_outline(Sketch& sketch, const std::string& node, std::int64_t x,
                             std::int64_t y, std::int64_t w, std::int64_t h) {
    sketch.outline_rect(node, x, y, w, h, by_role(role::kEdge));
}

/// A TEXT BOX DRAWN FROM ITS CELLS (`component::text_box_cells`) at `x`, `y` in `text`'s measure,
/// one line tall: its outline where it is seen as a box, its slice in `ink`, the selected columns
/// again in the accent role and the caret a bar before the column it sits before -- the caret
/// and the selection only while `keys` are in the box. A node under `node` each.
inline void text_box_face(Sketch& sketch, const std::string& node,
                          const component::TextBoxCells& cells, std::int64_t x, std::int64_t y,
                          const TextMeasure& text, Paint ink, bool keys) {
    const std::int64_t w = cells.columns * text.advance;
    if (cells.bordered) {
        text_box_outline(sketch, node + ".edge", x, y, w, text.line);
    }
    const std::int64_t at = x + cells.text_begin * text.advance;
    const auto take = static_cast<std::size_t>(std::max<std::int64_t>(0, cells.text_columns));
    const std::string slice = cells.text.substr(static_cast<std::size_t>(cells.text_begin), take);
    sketch.text(node + ".text", at, y, slice, ink);
    if (!keys) {
        return;
    }
    if (cells.selected()) {
        const auto from = static_cast<std::size_t>(cells.selection_begin);
        const auto n = static_cast<std::size_t>(cells.selection_end - cells.selection_begin);
        sketch.text(node + ".sel", x + cells.selection_begin * text.advance, y,
                    cells.text.substr(from, n), by_role(role::kAccent));
    }
    const std::int64_t cx = x + cells.caret * text.advance;
    sketch.line(node + ".caret", cx, y, cx, y + text.line - 1, by_role(role::kAccent));
}

/// WHERE A TEXT AREA STANDS IN A WINDOW `w` BY `h` PIXELS in `text`'s measure: the columns it wraps
/// at (the room less the bar's), its lines, the window's height and its scroll in
/// `TextArea::kUnitsPerLine`, the first line shown and how many pixels of it are scrolled past, and
/// the bar's thumb. A face draws from it and its owner reads a press with it.
struct TextAreaGeometry {
    std::int64_t w = 0, h = 0;
    TextMeasure text;
    std::int64_t columns = 0;
    std::vector<component::TextArea::Line> lines;
    std::int64_t shown = 0, offset = 0, furthest = 0;
    std::size_t first = 0;
    std::int64_t past = 0;
    std::int64_t thumb_y = 0, thumb_h = 0;
    /// The text is longer than the window: the thumb is drawn and the bar scrolls.
    bool overflows() const noexcept { return furthest > 0; }
    /// The bar's room begins at this pixel of the area.
    std::int64_t bar_x() const noexcept { return w - kTextAreaBarRoom; }
};

inline TextAreaGeometry text_area_geometry(const component::TextArea& area, std::int64_t w,
                                           std::int64_t h, const TextMeasure& text) {
    TextAreaGeometry g;
    g.w = w;
    g.h = h;
    g.text = text;
    if (w <= 0 || h <= 0 || text.advance <= 0 || text.line <= 0) {
        return g;
    }
    constexpr std::int64_t unit = component::TextArea::kUnitsPerLine;
    g.columns = std::max<std::int64_t>(1, (w - kTextAreaBarRoom) / text.advance);
    g.lines = area.lines(g.columns);
    g.shown = h * unit / text.line;
    g.furthest = component::TextArea::furthest(g.lines, g.shown);
    g.offset = area.offset(g.lines, g.shown);
    g.first = static_cast<std::size_t>(g.offset / unit);
    g.past = (g.offset % unit) * text.line / unit;
    if (g.overflows()) {
        const std::int64_t whole = static_cast<std::int64_t>(g.lines.size()) * unit;
        g.thumb_h = std::clamp<std::int64_t>(h * g.shown / whole, std::min(kTextAreaThumbLeast, h), h);
        g.thumb_y = (h - g.thumb_h) * g.offset / g.furthest;
    }
    return g;
}

/// THE PLACE IN THE TEXT A POINT OF THE AREA NAMES, `x`, `y` in its pixels: the line under it and
/// the column, its first above the text and its last's end below it. None on the bar's room or
/// where there is no text.
inline std::optional<std::size_t> text_area_place_at(const component::TextArea& area,
                                                     const TextAreaGeometry& g, std::int64_t x,
                                                     std::int64_t y) {
    if (g.lines.empty() || x >= g.bar_x()) {
        return std::nullopt;
    }
    const std::int64_t down = std::max<std::int64_t>(0, y) + g.past;
    std::size_t line = g.first + static_cast<std::size_t>(down / g.text.line);
    if (line >= g.lines.size()) {
        return g.lines.back().end;
    }
    const std::int64_t column = std::max<std::int64_t>(0, x) / g.text.advance;
    return area.place_in_line(g.lines[line], column);
}

/// WHETHER A POINT OF THE AREA IS ON ITS BAR: the bar's whole room, the whole height.
inline bool text_area_on_bar(const TextAreaGeometry& g, std::int64_t x, std::int64_t y) {
    return x >= g.bar_x() && x < g.w && y >= 0 && y < g.h;
}

/// WHERE A PRESS OR A DRAG ON THE BAR AT `y` PUTS THE SCROLL, in units: the thumb's middle under
/// the pointer, held inside the text.
inline std::int64_t text_area_bar_scroll(const TextAreaGeometry& g, std::int64_t y) {
    const std::int64_t travel = g.h - g.thumb_h;
    if (!g.overflows() || travel <= 0) {
        return 0;
    }
    const std::int64_t top = std::clamp<std::int64_t>(y - g.thumb_h / 2, 0, travel);
    return top * g.furthest / travel;
}

/// A TEXT AREA DRAWN `w` BY `h` AT `x`, `y`, under `node`: its lines from the scroll's, a line part
/// shown at the top and foot where the scroll stands between two, each in the ink `ink_of` gives
/// its paragraph's look, the selection on a muted ground behind its text, and the bar's thumb in
/// the bar's room while the text overflows. The text is a group clipped to the area less the bar,
/// recorded with `action` so a press on it reaches the owner; the bar's room is recorded with
/// `bar_action`. The owner reads a press with `text_area_geometry` and the functions beside it.
template <class InkOf>
void text_area_face(Sketch& sketch, const std::string& node, const component::TextArea& area,
                    const TextAreaGeometry& g, std::int64_t x, std::int64_t y,
                    const InkOf& ink_of, const std::string& action = {},
                    const std::string& bar_action = {}) {
    if (g.w <= 0 || g.h <= 0) {
        return;
    }
    const std::string body = node + ".text";
    sketch.group(body, x, y, std::max<std::int64_t>(0, g.bar_x()), g.h);
    sketch.fill_rect(body + "/ground", 0, 0, std::max<std::int64_t>(0, g.bar_x()), g.h,
                     by_role(role::kGround));
    if (!action.empty()) {
        sketch.action(body, action);
    }
    const component::TextPlaces& sel = area.places();
    const std::string& text = area.text();
    for (std::size_t i = g.first; i < g.lines.size(); ++i) {
        const std::int64_t ly = static_cast<std::int64_t>(i - g.first) * g.text.line - g.past;
        if (ly >= g.h) {
            break;
        }
        const component::TextArea::Line& line = g.lines[i];
        const std::string name = body + "/l" + std::to_string(i);
        const std::int64_t lx = line.indent * g.text.advance;
        if (sel.has_selection()) {
            const std::size_t b = std::max(sel.begin(), line.begin);
            const std::size_t e = std::min(sel.end(), line.end);
            if (e > b) {
                sketch.fill_rect(name + "s",
                                 lx + static_cast<std::int64_t>(b - line.begin) * g.text.advance,
                                 ly, static_cast<std::int64_t>(e - b) * g.text.advance, g.text.line,
                                 by_role(role::kMuted));
            }
        }
        sketch.text(name, lx, ly, text.substr(line.begin, line.end - line.begin),
                    ink_of(area.look(line.paragraph)));
    }
    const std::string bar = node + ".bar";
    sketch.group(bar, x + g.bar_x(), y, kTextAreaBarRoom, g.h);
    sketch.fill_rect(bar + "/room", 0, 0, kTextAreaBarRoom, g.h, by_role(role::kGround));
    if (!bar_action.empty()) {
        sketch.action(bar, bar_action);
    }
    if (g.overflows()) {
        sketch.fill_rect(bar + "/thumb", (kTextAreaBarRoom - kTextAreaBarDrawn) / 2, g.thumb_y,
                         kTextAreaBarDrawn, g.thumb_h, by_role(role::kMuted));
    }
}

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_ELEMENTS_TEXT_FACE_HPP
