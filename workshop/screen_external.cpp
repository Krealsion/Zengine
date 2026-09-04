// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- an external pane's body -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/focus.md (+4 registers; agents/workshop.md routes)

#include "screen.hpp"

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
    // WORKSHOP'S HEADER IS THE REGION'S FIRST ROW, so the provenance line and the
    // provider's sentences are the same kind of text in whatever face this medium owns. It is
    // fitted to the region's own columns, which is what marks its cut. the row
    // exists exactly when the resolution reserved one: hidden titles return it to the
    // provider, and the pane holding the keyboard keeps its title -- and with it the `> `
    // mark -- whatever the preference says (`external_title_rows`).
    if (body.header_rows > 0) {
        region.rows.push_back(surface::SurfaceTextRow{
            detail::fit(external_header(*row, keyboard_pane(panels) == kind), body.columns),
            surface::role::kAccent});
    }
    // A PANE WITH ROOM FOR THE HEADER AND NOTHING ELSE STILL SAYS WHOSE IT IS. `present` is
    // the question "was a body granted", and it is asked AFTER the header is written rather
    // than before it -- a rectangle showing a maker nothing at all is the worse of the two
    // answers, and it is what this painter gave for one frame when the header stopped being
    // a cell row of its own.
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
    if (!pane->refusal.empty()) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(pane->refusal, body.columns),
                                                      surface::role::kAlert});
    } else if (!pane->heard) {
        region.rows.push_back(surface::SurfaceTextRow{
            detail::fit(kExternalWaiting, body.columns), surface::role::kMuted});
    } else {
        region.rows.insert(region.rows.end(), pane->shown.begin(), pane->shown.end());
    }
    layer.texts.push_back(std::move(region));
}

} // namespace zengine::workshop
