// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- the chrome a pane wears, the authored intent projected
// onto this screen, and placement spent on the pointer -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/panes-and-windows.md (+8 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- THE CHROME A PANE WEARS, AND THE INTERIOR IT LEAVES --------------------------------

namespace detail {

PaneInside pane_inside_at(const FineRect& outer, const Screen& sc,
                          std::int64_t chrome_subs) {
    PaneInside p;
    p.chrome_subs = chrome_subs;
    p.rect = pane_interior(outer, chrome_subs);
    if (p.rect.w <= 0 || p.rect.h <= 0) {
        return p;
    }
    p.fit = surface::fit_region_subs(p.rect.x, p.rect.y, p.rect.w, p.rect.h,
                                     sc.text_advance_px, sc.text_line_px);
    return p;
}

} // namespace detail

// WL-CHROME-01, WL-CHROME-03, WL-CHROME-04, WL-CHROME-07 -- agents/workshop/chrome.md
// WL-MAKER-05 -- agents/workshop/maker-pane.md
PaneInside pane_inside(const FineRect& outer, const Screen& sc) {
    const std::int64_t fine = chrome_grain(sc);
    if (fine < kChromeSubs) {
        const PaneInside thin = detail::pane_inside_at(outer, sc, fine);
        if (thin.fit.graphical()) {
            return thin;
        }
    }
    const PaneInside cell = detail::pane_inside_at(outer, sc, kChromeSubs);
    if (cell.rect.w > 0 && cell.rect.h > 0) {
        return cell;
    }
    return detail::pane_inside_at(outer, sc, 0);
}

FineRect pane_interior(const FineRect& outer, const Screen& sc) {
    return pane_inside(outer, sc).rect;
}

// ---- AUTHORED INTENT, PROJECTED ONTO THIS SCREEN -------------------------------------

// WL-SETUP-06 -- agents/workshop/setup-file.md
bool pane_unit_projectable(const SetupPane* authored) noexcept {
    if (authored == nullptr) {
        return true;
    }
    return authored->width.mode != pane_unit::kPixels &&
           authored->height.mode != pane_unit::kPixels;
}

// WL-GEO-06 -- agents/workshop/geometry.md
// WL-PANE-01, WL-PANE-08, WL-PANE-11 -- agents/workshop/panes-and-windows.md
PaneProjection project_pane(std::int64_t where, std::size_t slot,
                            const SetupPane* authored, const Screen& sc) {
    PaneProjection out;
    // THE DEVELOPER'S ANSWER IS CELL-LATTICE AND ENTERS THE FINE LATTICE EXACTLY
    //: `placement_bounds` keeps thinking in the screen's own cells, and the
    // multiply here is where its rectangle becomes arrangement truth a maker's
    // override lays over, per axis, in the same sub-units the override carries.
    out.resolved = fine_of_cells(placement_bounds(where, slot, sc));
    // THE UNIT IS ASKED FIRST AND FOR EVERY PLACEMENT. A refusal is WHOLE --
    // no rectangle, resolved or visible -- so every consumer that already reads an empty
    // rectangle as "nowhere" is right about a pixel-sized pane with no branch of its own.
    if (!pane_unit_projectable(authored)) {
        return PaneProjection{false, FineRect{}, FineRect{}};
    }
    // THE MAKER'S ANSWER IS SPENT WHEREVER THE PLACE IS THEIRS TO AUTHOR. This
    // used to name the overlay stack, which was the same set said as a list while the stack
    // was the only movable place -- and a list is what a fourth place would be added to by
    // somebody who remembered. `place_is_authorable` (panel.hpp) is the exclusion itself:
    // the side region is the SCREEN's and everything else takes an override. Nothing about
    // the overlay stack changed; the top band joined it.
    if (place_is_authorable(where) && authored != nullptr) {
        if (authored->place.mode == pane_unit::kSubcells) {
            out.resolved.x = authored->place.x;
            out.resolved.y = authored->place.y;
        }
        if (authored->width.mode == pane_unit::kSubcells) {
            out.resolved.w = authored->width.amount;
        }
        if (authored->height.mode == pane_unit::kSubcells) {
            out.resolved.h = authored->height.amount;
        }
    }
    out.visible = clip_to_canvas_fine(out.resolved, sc);
    return out;
}

// WL-PANE-03, WL-PANE-07, WL-PANE-09 -- agents/workshop/panes-and-windows.md
// WL-MAKER-05 -- agents/workshop/maker-pane.md
PanelBounds bounds_of(const Panels& panels, const Setup& setup, std::int64_t kind,
                      const Screen& sc) {
    std::size_t slot = 0;
    for (const Panel& p : panels.open) {
        const std::int64_t where = placement_of(p.kind);
        const SetupPane* authored = nullptr;
        for (const SetupPane& row : setup.panes) {
            const std::optional<std::int64_t> named = resolve_pane(row.ref, panels);
            if (named.has_value() && *named == p.kind) {
                authored = &row;
                break;
            }
        }
        if (p.kind == kind) {
            const PaneProjection got = project_pane(where, slot, authored, sc);
            return PanelBounds{true, where, got.visible, got.resolved, got.projected};
        }
        if (where == placement::kOverlayStack &&
            (authored == nullptr || authored->place.mode == pane_unit::kDefault)) {
            ++slot;
        }
    }
    return PanelBounds{false, placement_of(kind), FineRect{}, FineRect{}, true};
}

// ---- PLACEMENT SPENT ON THE POINTER: a place a maker can see is a place a hand meets ------

PointedAt canvas_point_of(std::int64_t space, std::int64_t x, std::int64_t y) noexcept {
    if (space == input::space::kCells) {
        return PointedAt{true, surface::canvas_of_terminal_cells(x, y),
                         surface::canvas_subs_of_terminal_cells(x, y),
                         surface::kCellGrainSubs};
    }
    if (space == input::space::kPixels) {
        return PointedAt{true, surface::canvas_of_window_pixels(x, y),
                         surface::canvas_subs_of_window_pixels(x, y),
                         surface::kPixelGrainSubs};
    }
    return PointedAt{};
}

// WL-FRONT-02, WL-FRONT-05 -- agents/workshop/planes.md
// WL-PRESS-04, WL-PRESS-05 -- agents/workshop/press-chain.md
// WL-FOCUS-03 -- agents/workshop/focus.md
// WL-PANE-05 -- agents/workshop/panes-and-windows.md
// WL-TAB-09 -- agents/workshop/tab-run.md
Occupancy occupied_at(const Panels& panels, const Setup& setup, const Screen& sc,
                      const PointedAt& at) {
    // EVERY TEST BELOW IS THE POINTER'S OWN GRAIN AGAINST FINE GEOMETRY — the
    // aligned-span law, so the cells and pixels a pane paints are exactly the ones on
    // which it answers. For whole-cell rectangles this is the cell containment this
    // walk has always performed.
    if (panels.picker.open && picker_bounds(sc).contains_at(at.sub.x, at.sub.y, at.grain)) {
        return Occupancy{true, kPickerName, kNoKind};
    }
    const std::vector<std::int64_t> order = effective_pane_order(setup, panels);
    for (std::size_t i = order.size(); i > 0; --i) {
        const std::int64_t kind = order[i - 1];
        if (bounds_of(panels, setup, kind, sc).rect.contains_at(at.sub.x, at.sub.y, at.grain)) {
            // `kind_name` AND NOT `panel_kind(kind).name`. The total lookup answers
            // `Builder` for anything outside the compile-time catalog, so an external pane
            // would tell a maker their hand was on the build tool -- the same lie
            // `resolve_pane` is fallible to prevent, arriving through the pointer instead
            // of through a file. Built-ins are unchanged: `kind_name` reads the same row.
            return Occupancy{true, kind_name(panels, kind), kind};
        }
    }
    return Occupancy{};
}

// WL-FRONT-02, WL-FRONT-05 -- agents/workshop/planes.md
// WL-PRESS-04, WL-PRESS-05 -- agents/workshop/press-chain.md
// WL-FOCUS-03 -- agents/workshop/focus.md
// WL-PANE-05 -- agents/workshop/panes-and-windows.md
// WL-TAB-09 -- agents/workshop/tab-run.md
Occupancy occupied_at(const Panels& panels, const Setup& setup, const Screen& sc,
                      std::int64_t cx, std::int64_t cy) {
    return occupied_at(panels, setup, sc,
                       PointedAt{true, surface::CanvasPoint{cx, cy},
                                 surface::CanvasPoint{surface::subs_of_cells(cx),
                                                      surface::subs_of_cells(cy)},
                                 surface::kCellGrainSubs});
}

} // namespace zengine::workshop
