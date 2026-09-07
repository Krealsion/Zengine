// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- which revealable row the pointer is on, the wheel a
// cursor-windowed list spends, and the arrangement's affordance rings -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
//
// THE PROJECT BROWSER USED TO BE PRESENTED HERE and is a weave now (`Zengine/files/`). What
// stayed is what was never only its: `spend_wheel`, which the Editor, the Pane Manager and
// the picker all spend; the Info panel's reveal and the one dispatch every motion asks; and
// the rings arrangement paints over every pane.
// Workshop law: agents/workshop/pointer.md (+5 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- THE PROJECT BROWSER, PRESENTED -----------------------------------------------------

// WL-EDIT-10 -- agents/workshop/editor.md
std::int64_t spend_wheel(double& accum, double dy, std::int64_t rows_per_notch) {
    accum += dy * static_cast<double>(rows_per_notch);
    const std::int64_t rows = static_cast<std::int64_t>(accum);
    accum -= static_cast<double>(rows);
    return rows;
}

// ---- Which revealable row the pointer is on ----------------------------------------------

RevealAt info_reveal_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                        std::int64_t x, std::int64_t y) {
    const InfoBodyAt where = info_body_at(d, s, space, x, y);
    if (!where.present || where.at.column < 0 || where.at.column >= where.body.columns) {
        return RevealAt{};
    }
    const std::size_t object = object_at_prose_row(where.body, where.at.row);
    if (object != kNoObject && object < d.elements.size()) {
        const ui::Element& e = d.elements[object];
        const std::string full = object_row_full(e, e.id == s.selected);
        return RevealAt{true,
                        reveal_place::kInfoObject,
                        object,
                        full,
                        detail::fit(full, where.body.columns),
                        where.body.columns,
                        where.at.column};
    }
    const std::size_t property = property_at_prose_row(where.body, where.at.row);
    if (property == kNoProperty || property >= s.rows.size()) {
        return RevealAt{};
    }
    const Row& row = s.rows[property];
    if (row.editing()) {
        return RevealAt{}; // a draft owns its own window; see `paint_info`
    }
    const bool here = property == s.cursor;
    return RevealAt{true,
                    reveal_place::kInfoProperty,
                    property,
                    property_row_full(row, here),
                    property_row_text(row, here, where.body.value_columns),
                    where.body.columns,
                    where.at.column};
}

RevealAt reveal_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                   std::int64_t x, std::int64_t y) {
    const Screen sc = screen_of(s);
    const PointedAt at = canvas_point_of(space, x, y);
    if (!at.understood) {
        return RevealAt{};
    }
    const Occupancy here = occupied_at(s.panels, s.setup.active, sc, at);
    if (!here.occupied) {
        return RevealAt{};
    }
    if (here.kind == panel::kInfo) {
        return info_reveal_at(d, s, space, x, y);
    }
    return RevealAt{};
}

Revealed reveal_for(const WorkshopDoc& d, const Session& s, std::int64_t space,
                    std::int64_t x, std::int64_t y) {
    const RevealAt at = reveal_at(d, s, space, x, y);
    if (!at.clipped()) {
        return Revealed{};
    }
    return Revealed{at.place, at.item, at.text,
                    detail::reveal_offset_at_column(at.text, at.columns, at.column)};
}

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
        // EVERY PANE WHOSE PLACE IS THE MAKER'S TO AUTHOR WEARS HANDLES. This
        // named the overlay stack while the stack was the only such place, which made the
        // ring a list rather than the rule it is; `place_is_authorable` is the same
        // exclusion the arrangement admission already spoke -- the side column is the
        // screen's, and a pane whose geometry no gesture can change must not advertise
        // eight grips that all refuse.
        if (!kind.has_value() || !place_is_authorable(placement_of(*kind))) {
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
            // THE WIRE SPELLING, cells plus remainders (`wire_rect_of`'s decomposition):
            // a label's x/y ARE canvas cells, and the fine-lattice construction that handed
            // them raw sub-units put every mark off the canvas -- rings that hit
            // correctly and painted nowhere, the exact see/grab split one geometry forbids.
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
