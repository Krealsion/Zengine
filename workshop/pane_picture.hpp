// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_PICTURE_HPP
#define ZENGINE_WORKSHOP_PANE_PICTURE_HPP

// A canvas pane's picture as a value: its rectangles, labels, runs of text and the parts it names,
// in its area's own pixels, and the room it is composed for. A pane composes one and draws it in
// its area (`canvas_sketch`, pane_canvas_drawing.hpp); none of it crosses as a message. The pane
// owns what it shows and what a press on it means.

#include "graphics/pixels.hpp"
#include "graphics/role.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// One canvas cell: a label byte's advance, and the grain of a room whose device unit is the cell.
inline constexpr std::int64_t kPaneCanvasUnit = graphics::kCellPx;
/// The most a picture holds: what a window keeps of one bounds it again (`canvas_sketch`).
inline constexpr std::size_t kPaneCanvasMaxRects = 4096;
inline constexpr std::size_t kPaneCanvasMaxLabels = 2048;
inline constexpr std::size_t kPaneCanvasMaxLabelBytes = 4096;
inline constexpr std::size_t kPaneCanvasMaxTexts = 2048;
inline constexpr std::size_t kPaneCanvasMaxTextRunBytes = 4096;
inline constexpr std::size_t kPaneCanvasMaxTextBytes = 131072;

/// THE ROOM A PICTURE IS COMPOSED FOR: `width` by `height` pixels of the pane's area, its grain
/// one device unit of the window that shows it (one pixel where it shows every pixel,
/// `kPaneCanvasUnit` where its unit is the cell), and how that window sets text -- a character's
/// advance and a line's height, none where text is a cell.
struct CanvasRoom {
    std::int64_t width = 0, height = 0, grain = kPaneCanvasUnit;
    bool graphical = false;
    std::int64_t text_advance_px = 0, text_line_px = 0;
    bool operator==(const CanvasRoom&) const = default;
};

/// A rectangle filled in a role.
struct CanvasRect {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t role = graphics::role::kFill;
    bool operator==(const CanvasRect&) const = default;
};

/// A label: one canvas cell per printable ASCII byte, unscaled; offscreen glyphs go whole.
struct CanvasLabel {
    std::int64_t x = 0, y = 0;
    std::string text;
    std::int64_t role = graphics::role::kFill;
    bool operator==(const CanvasLabel&) const = default;
};

/// ONE LINE OF TEXT IN THE WINDOW'S MEASURE: `x`/`y` its padded region's origin (`padded`), or its
/// first character's cell, so unpadded runs a line apart stack as rows do; caret and selection are
/// byte columns of its text, the selection `[begin, end)`; a ground of its own under its
/// characters (`role::kNone`: the material beneath). Whole glyphs and rows go at the room's edge.
struct CanvasText {
    std::int64_t x = 0, y = 0;
    std::string text;
    std::int64_t role = graphics::role::kFill;
    std::int64_t caret_col = -1;
    std::int64_t sel_begin_col = -1, sel_end_col = -1;
    bool padded = true;
    std::int64_t background = graphics::role::kNone;
    bool operator==(const CanvasText&) const = default;
};

/// ONE PART OF A PICTURE: a rectangle of it -- an element, a control, a row of its text -- the
/// pane's name for it, kept across its redraws, and whether its operation applies now.
struct CanvasPart {
    std::string name;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    bool available = true;
    bool operator==(const CanvasPart&) const = default;
};

/// A WHOLE PICTURE, rects first and labels and runs above them, and the parts it names in the
/// order its pane reads a press: where parts hold one place, a press there reaches the later.
struct CanvasPicture {
    std::vector<CanvasRect> rects;
    std::vector<CanvasLabel> labels;
    std::vector<CanvasText> texts;
    std::vector<CanvasPart> parts;
    bool operator==(const CanvasPicture&) const = default;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_PICTURE_HPP
