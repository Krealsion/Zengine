// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_SCREEN_PLANES_HPP
#define ZENGINE_WORKSHOP_SCREEN_PLANES_HPP

// Workshop's own composition of its desk: planes of rects, labels and bounded regions of rows, in
// canvas pixels, which `paint` makes and `draw_plane` turns into the drawing of Workshop's areas.
// Plain values, private to Workshop's screen: none crosses as a message. Reference:
// workshop/docs/workshop-panes.md.

#include "graphics/role.hpp"
#include "workshop/pane_rows.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// One filled rectangle, in canvas pixels. A plane's rects paint in list order, so a rect goes
/// behind another by coming earlier.
struct ScreenRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::int64_t role = graphics::role::kFill;
    bool operator==(const ScreenRect&) const = default;
};

/// One run of plain text anchored at a canvas pixel, drawn over its plane's rects.
struct ScreenLabel {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::string text;
    std::int64_t role = graphics::role::kFill;
    bool operator==(const ScreenLabel&) const = default;
};

/// Whose rectangle a region is. `kGroundOwn` (the default): the region clears its whole bounds
/// before a row is drawn. `kGroundBeneath`: the region writes on what its plane drew beneath it,
/// with no fill and no padding.
inline constexpr std::int64_t kGroundOwn = 0;
inline constexpr std::int64_t kGroundBeneath = 1;

/// A bounded region of rows, placed in canvas pixels like a rect and set in the screen's text
/// measure (`fit_region`, pane_text_fit.hpp). `caret_*` and `sel_*` are a row and a column into
/// `rows`; a selection runs from begin (inclusive) to end (exclusive) in reading order.
struct ScreenRegion {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::vector<TextRow> rows;
    std::int64_t caret_row = kNoCaret;
    std::int64_t caret_col = 0;
    std::int64_t ground = kGroundOwn;
    std::int64_t sel_begin_row = kNoSelection;
    std::int64_t sel_begin_col = 0;
    std::int64_t sel_end_row = kNoSelection;
    std::int64_t sel_end_col = 0;
    bool operator==(const ScreenRegion&) const = default;
};

/// One plane: its rects in list order, then its labels, then its regions, drawn whole before the
/// next plane goes over it.
struct ScreenPlane {
    std::vector<ScreenRect> rects;
    std::vector<ScreenLabel> labels;
    std::vector<ScreenRegion> texts;
    bool operator==(const ScreenPlane&) const = default;
};

/// The whole desk: its extent in canvas pixels and its planes, `layers[0]` back-most.
struct ScreenCanvas {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::vector<ScreenPlane> layers;
    bool operator==(const ScreenCanvas&) const = default;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_PLANES_HPP
