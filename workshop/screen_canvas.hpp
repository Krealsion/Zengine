// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_SCREEN_CANVAS_HPP
#define ZENGINE_WORKSHOP_SCREEN_CANVAS_HPP
#include "screen.hpp"
#include "pane_canvas.hpp"
#include "pane_canvas_text.hpp"
#include <algorithm>

namespace zengine::workshop {
// One geometry in both directions. The canvas begins below the entire title region (including
// the face's text inset); its origin is rounded to the active device grain, like its painting.
inline PixelRect canvas_body_place(const PixelRect& pane_rect, const Screen& sc,
                                 std::int64_t header_rows) {
    const auto inside = pane_inside(pane_rect, sc);
    if (inside.rect.empty() || inside.fit.rows <= header_rows || inside.fit.columns <= 0) return {};
    const auto grain = chrome_grain(sc);
    const auto header = header_rows == 0 ? 0 : inside.fit.graphical()
        ? surface::mul_px(surface::add_cells(2 * inside.fit.origin_y,
                                             surface::mul_px(header_rows, inside.fit.line_px)), grain)
        : surface::px_of_cells(header_rows);
    const auto x = surface::floor_div_px(inside.rect.x, grain) * grain;
    const auto y = surface::floor_div_px(surface::add_cells(inside.rect.y, header), grain) * grain;
    const auto right = surface::floor_div_px(surface::add_cells(inside.rect.x, inside.rect.w), grain) * grain;
    const auto bottom = surface::floor_div_px(surface::add_cells(inside.rect.y, inside.rect.h), grain) * grain;
    return right > x && bottom > y ? PixelRect{x, y, right - x, bottom - y} : PixelRect{};
}

/// WHERE A PANE'S PICTURE STANDS: the room it was granted while a held press keeps that room from
/// the title its keys bring (`ExternalPane::Canvas::title_waits`), else the body its place and
/// title rows give it. The painter draws the picture there and inspection reads it there, as the
/// pointer answers in the room granted, so all three agree on one room.
inline PixelRect shown_canvas_place(const ExternalPane::Canvas& c, const PixelRect& pane_rect,
                                    const Screen& sc, std::int64_t header_rows) {
    if (c.title_waits) return PixelRect{c.x, c.y, c.width, c.height};
    return canvas_body_place(pane_rect, sc, header_rows);
}

inline PixelRect canvas_clip_rect(const PaneCanvasRect& r, std::int64_t width,
                                std::int64_t height) {
    const auto left = (std::max)(std::int64_t{0}, r.x);
    const auto top = (std::max)(std::int64_t{0}, r.y);
    const auto right = (std::min)(width, surface::add_cells(r.x, r.w));
    const auto bottom = (std::min)(height, surface::add_cells(r.y, r.h));
    return r.w > 0 && r.h > 0 && right > left && bottom > top
        ? PixelRect{left, top, right - left, bottom - top} : PixelRect{};
}

inline void paint_pane_canvas(surface::SurfaceLayer& layer, const PixelRect& body,
                              const v5::PaneCanvasContent& content,
                              std::int64_t text_advance_px = 0, std::int64_t text_line_px = 0,
                              std::int64_t grain = kPaneCanvasUnit) {
    if (body.empty()) return;
    for (const auto& rect : content.rects) {
        auto clipped = canvas_clip_rect(rect, body.w, body.h);
        if (clipped.empty()) continue;
        clipped.x = surface::add_cells(body.x, clipped.x);
        clipped.y = surface::add_cells(body.y, clipped.y);
        layer.rects.push_back(wire_rect_of(clipped, rect.role));
    }
    for (const auto& label : content.labels) {
        if (label.y < 0 || label.y > body.h - kPaneCanvasUnit) continue;
        // At most the admitted bounded bytes; saturating coordinates cannot overflow a step.
        std::size_t first = 0;
        auto x = label.x;
        while (first < label.text.size() && x < 0) {
            x = surface::add_cells(x, kPaneCanvasUnit);
            ++first;
        }
        if (x > body.w - kPaneCanvasUnit || first == label.text.size()) continue;
        const auto room = (body.w - x) / kPaneCanvasUnit;
        const auto count = (std::min)(label.text.size() - first, static_cast<std::size_t>(room));
        if (count == 0) continue;
        layer.labels.push_back(surface::SurfaceLabel{surface::add_cells(body.x, x),
                                                     surface::add_cells(body.y, label.y),
                                                     label.text.substr(first, count), label.role});
    }
    const PaneCanvasRoom room{content.pane, content.grant, body.w, body.h, grain,
                             grain < kPaneCanvasUnit, text_advance_px, text_line_px};
    for (const auto& text : content.texts) {
        const auto placed = clip_canvas_run(text, {0, 0, body.w, body.h}, room);
        if (placed.visible()) layer.texts.push_back(canvas_text_region(placed, body.x, body.y));
    }
}

/// An earlier picture, painted as the current form says it.
inline void paint_pane_canvas(surface::SurfaceLayer& layer, const PixelRect& body,
                              const PaneCanvasContent& content,
                              std::int64_t text_advance_px = 0, std::int64_t text_line_px = 0,
                              std::int64_t grain = kPaneCanvasUnit) {
    paint_pane_canvas(layer, body, canvas_content_of(content), text_advance_px, text_line_px, grain);
}
} // namespace zengine::workshop
#endif
