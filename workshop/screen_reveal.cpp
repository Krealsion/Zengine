// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's arrangement rings.
// Workshop law: agents/workshop/arrangement.md (+2 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// WL-ARR-09 -- agents/workshop/arrangement.md
// WL-ARR-17 -- agents/workshop/arrangement-snap.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
// WL-FRONT-01 -- agents/workshop/planes.md
void paint_pane_affordances(surface::SurfaceLayer& layer, const Session& s,
                            const Screen& sc) {
    if (!s.arrange.open) {
        return;
    }
    // THE LINES A HELD SNAP MET, across the room, one device unit wide and inside it: over every
    // pane, beneath the rings, gone with the gesture.
    if (s.pane_drag.active) {
        const std::int64_t grain = chrome_grain(sc);
        const auto inside = [grain](std::int64_t line, std::int64_t extent) {
            return line < 0 ? std::int64_t{0} : (line > extent - grain ? extent - grain : line);
        };
        if (s.pane_drag.met_x.has_value()) {
            layer.rects.push_back(wire_rect_of(
                PixelRect{inside(*s.pane_drag.met_x, sc.room_w), sc.room_y, grain, sc.room_h},
                surface::role::kAccent));
        }
        if (s.pane_drag.met_y.has_value()) {
            layer.rects.push_back(wire_rect_of(
                PixelRect{0, surface::add_cells(sc.room_y, inside(*s.pane_drag.met_y, sc.room_h)),
                          sc.room_w, grain},
                surface::role::kAccent));
        }
    }
    const auto ring = [&](const PaneRef& ref, bool emphasized) {
        const std::optional<std::int64_t> kind = resolve_pane(ref, s.panes);
        // Every pane this build can resolve wears handles; only a reference that resolves to no
        // kind stops a ring.
        if (!kind.has_value()) {
            return;
        }
        const PaneBounds where = bounds_of(s.panes, s.setup.active, *kind, sc);
        if (!where.open || where.rect.w <= 0 || where.rect.h <= 0) {
            return;
        }
        const bool held = s.pane_drag.active && s.pane_drag.sizing && s.pane_drag.pane == ref;
        for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
            const PixelRect at = pane_edge_cell(where.rect, edge);
            const bool chosen = held ? s.pane_drag.edge == edge : emphasized;
            layer.labels.push_back(surface::SurfaceLabel{
                at.x, at.y, std::string(pane_edge_glyph(edge)),
                chosen ? surface::role::kAccent : surface::role::kMuted});
        }
    };
    if (!s.arrange.desk) {
        if (s.arrange.addressed()) {
            ring(s.arrange.pane, true);
        }
        return;
    }
    for (const SetupPane& row : s.setup.active.panes) {
        ring(row.ref, s.arrange.addressed() && row.ref == s.arrange.pane);
    }
}

} // namespace zengine::workshop
