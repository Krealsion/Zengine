// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's arrangement rings.
// Workshop law: agents/workshop/arrangement.md (+2 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// WL-ARR-09 -- agents/workshop/arrangement.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
// WL-FRONT-01 -- agents/workshop/planes.md
void paint_pane_affordances(surface::SurfaceLayer& layer, const Session& s,
                            const Screen& sc) {
    if (!s.arrange.open) {
        return;
    }
    const auto ring = [&](const PaneRef& ref, bool emphasized) {
        const std::optional<std::int64_t> kind = resolve_pane(ref, s.panels);
        // Every pane this build can resolve wears handles; only a reference that resolves to no
        // kind stops a ring.
        if (!kind.has_value()) {
            return;
        }
        const PanelBounds where = bounds_of(s.panels, s.setup.active, *kind, sc);
        if (!where.open || where.rect.w <= 0 || where.rect.h <= 0) {
            return;
        }
        const bool held = s.pane_drag.active && s.pane_drag.sizing && s.pane_drag.pane == ref;
        for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
            const FineRect at = pane_edge_cell(where.rect, edge);
            const bool chosen = held ? s.pane_drag.edge == edge : emphasized;
            // The wire spelling, cells plus remainders (`wire_rect_of`): a label's x/y are canvas
            // cells, so raw sub-units would paint every mark off the canvas.
            const std::int64_t cx = surface::cell_of_subs(at.x);
            const std::int64_t cy = surface::cell_of_subs(at.y);
            layer.labels.push_back(surface::SurfaceLabel{
                cx, cy, std::string(pane_edge_glyph(edge)),
                chosen ? surface::role::kAccent : surface::role::kMuted,
                at.x - surface::subs_of_cells(cx), at.y - surface::subs_of_cells(cy)});
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
