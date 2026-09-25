// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's external pane body.
// Workshop law: agents/workshop/focus.md (+4 registers; agents/workshop.md routes)

#include "screen.hpp"
#include "screen_canvas.hpp"

namespace zengine::workshop {

// ---- AN EXTERNAL PANE'S BODY: one header row of Workshop's, and a region ---------------

// WL-FOCUS-11 -- agents/workshop/focus.md
std::int64_t external_title_rows(const Panels& panels, std::int64_t kind,
                                 bool titles_shown) noexcept {
    return (titles_shown || keyboard_pane(panels) == kind) ? kExternalHeaderRows : 0;
}

// WL-CHROME-05 -- agents/workshop/chrome.md
// WL-EDIT-12 -- agents/workshop/editor.md
// WL-PANE-06 -- agents/workshop/panes-and-windows.md
ExternalBodyPlace external_body_place(const FineRect& panel, const Screen& sc,
                                      std::int64_t header_rows) {
    ExternalBodyPlace p;
    const PaneInside inside = pane_inside(panel, sc);
    const FineRect inner = inside.rect;
    if (inner.w <= 0 || inner.h <= 0) {
        return p;
    }
    const surface::SurfaceRect wire = wire_rect_of(inner, surface::role::kFill);
    p.region_x = wire.x;
    p.region_y = wire.y;
    p.region_w = wire.w;
    p.region_h = wire.h;
    p.region_sub_x = wire.sub_x;
    p.region_sub_y = wire.sub_y;
    p.region_sub_w = wire.sub_w;
    p.region_sub_h = wire.sub_h;
    p.fit = inside.fit;
    p.header_rows = header_rows;
    p.rows = p.fit.rows > header_rows ? p.fit.rows - header_rows : 0;
    p.columns = p.fit.columns;
    p.present = p.rows > 0 && p.columns > 0;
    return p;
}

// WL-PRESS-04 -- agents/workshop/press-chain.md
ExternalPressAt external_press_at(const Panels& panels, const Setup& setup,
                                  const Screen& sc, std::int64_t kind, bool titles,
                                  std::int64_t space, std::int64_t x, std::int64_t y) {
    const PanelBounds where = bounds_of(panels, setup, kind, sc);
    if (!where.open) {
        return ExternalPressAt{};
    }
    const ExternalBodyPlace body =
        external_body_place(where.rect, sc, external_title_rows(panels, kind, titles));
    if (!body.present) {
        return ExternalPressAt{};
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    if (!at.understood) {
        return ExternalPressAt{};
    }
    const std::int64_t row = at.row - body.header_rows;
    if (row < 0 || row >= body.rows || at.column < 0 || at.column >= body.columns) {
        return ExternalPressAt{};
    }
    return ExternalPressAt{true, row, at.column};
}

// WL-EDIT-12 -- agents/workshop/editor.md; WL-FOCUS-10 -- agents/workshop/focus.md
std::string external_header(const RuntimePane& row, bool typing) {
    return std::string(typing ? kTypingHere : kTypingElsewhere) + row.name + " @" +
           row.provider;
}

// WL-FOCUS-10 -- agents/workshop/focus.md; WL-PANE-06 -- agents/workshop/panes-and-windows.md
void paint_external(surface::SurfaceLayer& layer, const Panels& panels, std::int64_t kind,
                    const FineRect& b, const Screen& sc, bool titles,
                    std::int64_t chrome) {
    paint_panel_frame(layer, b, chrome);
    const RuntimePane* row = panels.runtime.of_kind(kind);
    if (row == nullptr) {
        return; // an open kind with no catalog row cannot happen; drawing a lie could
    }
    const ExternalBodyPlace body =
        external_body_place(b, sc, external_title_rows(panels, kind, titles));
    if (body.fit.rows <= 0 || body.fit.columns <= 0) {
        return; // no room for one row of this medium's type: say nothing at all
    }
    surface::SurfaceTextRegion region;
    region.x = body.region_x;
    region.y = body.region_y;
    region.w = body.region_w;
    region.h = body.region_h;
    region.sub_x = body.region_sub_x;
    region.sub_y = body.region_sub_y;
    region.sub_w = body.region_sub_w;
    region.sub_h = body.region_sub_h;
    // Workshop's header is the region's first row, fitted to its columns, and exists exactly when
    // the resolution reserved one: hidden titles return it to the provider, but the keyboard pane
    // keeps its title and its `> ` mark (`external_title_rows`).
    if (body.header_rows > 0) {
        region.rows.push_back(surface::SurfaceTextRow{
            detail::fit(external_header(*row, keyboard_pane(panels) == kind), body.columns),
            surface::role::kAccent});
    }
    // A pane with room for the header and nothing else still says whose it is: `present` is asked
    // after the header is written.
    if (!body.present) {
        if (!region.rows.empty()) {
            layer.texts.push_back(std::move(region));
        }
        return; // no room under the heading: the heading, and no invented room
    }
    const ExternalPane* pane = panels.external_pane(kind);
    if (pane == nullptr) {
        if (!region.rows.empty()) {
            layer.texts.push_back(std::move(region));
        }
        return;
    }
    if (pane->canvas.grant > 0 && (pane->canvas.heard || pane->canvas.preview)) {
        const FineRect canvas = canvas_body_place(b, sc, body.header_rows);
        paint_pane_canvas(layer, canvas, pane->canvas.content, sc.text_advance_px,
                          sc.text_line_px, chrome_grain(sc));
        if (pane->canvas.preview) {
            if (!region.rows.empty()) {
                region.rows[0].text = detail::fit("(updating) " + region.rows[0].text, body.columns);
            } else {
                PaneCanvasContent notice;
                notice.texts.push_back({0, 0, "(updating)", surface::role::kAlert});
                paint_pane_canvas(layer, canvas, notice, sc.text_advance_px,
                                  sc.text_line_px, chrome_grain(sc));
            }
        }
        if (!region.rows.empty()) {
            const auto inside = pane_inside(b, sc).rect;
            const auto header = wire_rect_of(FineRect{inside.x, inside.y, inside.w,
                                                       surface::sub_px(canvas.y, inside.y)},
                                              surface::role::kFill);
            region.h = header.h;
            region.sub_h = header.sub_h;
            layer.texts.push_back(std::move(region));
        }
        return;
    }
    if (!pane->refusal.empty()) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(pane->refusal, body.columns),
                                                      surface::role::kAlert});
    } else if (!pane->heard) {
        region.rows.push_back(surface::SurfaceTextRow{
            detail::fit(kExternalWaiting, body.columns), surface::role::kMuted});
    } else {
        region.rows.insert(region.rows.end(), pane->shown.begin(), pane->shown.end());
        // The pane's own caret, merged here and nowhere else: the header is the region's first
        // row, so the offset added is the one `external_press_row` subtracts. Admission already
        // judged it (`judge_caret`).
        if (pane->caret_row != surface::kNoCaret) {
            region.caret_row = pane->caret_row + body.header_rows;
            region.caret_col = pane->caret_col;
        }
        if (pane->sel_begin_row != surface::kNoSelection) {
            region.sel_begin_row = pane->sel_begin_row + body.header_rows;
            region.sel_begin_col = pane->sel_begin_col;
            region.sel_end_row = pane->sel_end_row + body.header_rows;
            region.sel_end_col = pane->sel_end_col;
        }
    }
    layer.texts.push_back(std::move(region));
}

} // namespace zengine::workshop
