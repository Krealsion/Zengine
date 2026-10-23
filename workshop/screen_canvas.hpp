// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_SCREEN_CANVAS_HPP
#define ZENGINE_WORKSHOP_SCREEN_CANVAS_HPP
#include "screen.hpp"
#include <algorithm>

namespace zengine::workshop {
// WHERE A PANE'S AREA STANDS IN ITS FRAME: below the entire title region (including the face's
// text inset), its origin rounded to the active device grain.
inline PixelRect canvas_body_place(const PixelRect& pane_rect, const Screen& sc,
                                 std::int64_t header_rows) {
    const auto inside = pane_inside(pane_rect, sc);
    if (inside.rect.empty() || inside.fit.rows <= header_rows || inside.fit.columns <= 0) return {};
    const auto grain = chrome_grain(sc);
    const auto header = header_rows == 0 ? 0 : inside.fit.graphical()
        ? graphics::mul_px(graphics::add_cells(2 * inside.fit.origin_y,
                                             graphics::mul_px(header_rows, inside.fit.line_px)), grain)
        : graphics::px_of_cells(header_rows);
    const auto x = graphics::floor_div_px(inside.rect.x, grain) * grain;
    const auto y = graphics::floor_div_px(graphics::add_cells(inside.rect.y, header), grain) * grain;
    const auto right = graphics::floor_div_px(graphics::add_cells(inside.rect.x, inside.rect.w), grain) * grain;
    const auto bottom = graphics::floor_div_px(graphics::add_cells(inside.rect.y, inside.rect.h), grain) * grain;
    return right > x && bottom > y ? PixelRect{x, y, right - x, bottom - y} : PixelRect{};
}

} // namespace zengine::workshop
#endif
