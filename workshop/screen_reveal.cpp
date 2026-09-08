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

// ⭐ READING PAST AN ELLIPSIS LEFT WITH THE INFO PANEL, AND IT IS A LOSS RATHER THAN A MOVE.
// `info_reveal_at`, `reveal_at`, `reveal_for`, `RevealAt`, `Revealed`, `reveal_place`,
// `Session::reveal`, `revealed_row` and `reveal_shown` were one feature: a pointer resting on a
// truncated OBJECTS or PROPERTIES row scrolled that row under the hand so a maker could read
// the rest of a long name or value without editing it. Every one of them was Info's -- the
// `reveal_at` walk answered nothing for any other pane -- and the feature needs the row's
// UNFITTED text, which is the pane's now: what crosses the seam is rows already cut to the room
// the pane was granted, so this host has nothing left to scroll.
//
// AND IT IS NOT REPLACED, DELIBERATELY. The pane protocol has no hover -- press, key, text and
// wheel are the four inbound sentences -- and adding one so that this host could keep a feature
// is exactly the host-mapped route VD-22 refuses. A pane that wants it can scroll its own rows
// under its own keys, which is a pane's business and not a protocol's.

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
        // EVERY PANE THIS BUILD CAN RESOLVE WEARS HANDLES, and that is now every open pane.
        // The ring named the overlay stack while the stack was the only movable place, then
        // asked `place_is_authorable` to exclude the reserved column -- a pane whose geometry
        // no gesture could change must not advertise eight grips that all refuse. No pane is
        // in that position any more, so the only thing that can still stop a ring is a
        // reference this build cannot resolve to a kind at all.
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
