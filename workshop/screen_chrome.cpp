// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's pane chrome, the authored intent projected onto this screen, and placement spent
// on the pointer.
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
                            const SetupPane* authored, const Screen& sc,
                            const RuntimePane* preference, std::int64_t stack_y) {
    PaneProjection out;
    // The developer's answer is cell-lattice and enters the fine lattice exactly:
    // `placement_bounds` thinks in cells, and this multiply makes it the truth a maker's override
    // lays over, per axis.
    out.resolved = fine_of_cells(placement_bounds(where, slot, sc));
    const auto preferred = preferred_extent(preference, stack_capacity(sc));
    if (preferred.width) out.resolved.w = preferred.width;
    if (preferred.height) out.resolved.h = preferred.height;
    if (where == placement::kOverlayStack && stack_y >= 0) out.resolved.y = stack_y;
    if (where == placement::kSideRegion && preferred.width)
        out.resolved.x = sc.w * surface::kCellSubs - out.resolved.w;
    // THE UNIT IS ASKED FIRST AND FOR EVERY PLACEMENT. A refusal is WHOLE --
    // no rectangle, resolved or visible -- so every consumer that already reads an empty
    // rectangle as "nowhere" is right about a pixel-sized pane with no branch of its own.
    if (!pane_unit_projectable(authored)) {
        return PaneProjection{false, FineRect{}, FineRect{}};
    }
    // The maker's answer is spent wherever they gave one: an authored override lays over whatever
    // `placement_bounds` answered, per axis, for every place.
    if (authored != nullptr) {
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
    std::int64_t stack_y = kStackY * surface::kCellSubs;
    const auto capacity = stack_capacity(sc);
    for (const Panel& p : panels.open) {
        const SetupPane* authored = nullptr;
        for (const SetupPane& row : setup.panes) {
            const std::optional<std::int64_t> named = resolve_pane(row.ref, panels);
            if (named.has_value() && *named == p.kind) {
                authored = &row;
                break;
            }
        }
        // The desk may name the place, here and only here: a row saying `kRightColumn` names the
        // place no coordinate can, and it resolves through `placement_bounds` like any other;
        // authored extents still lay over it.
        std::int64_t where = placement_of(p.kind);
        if (authored != nullptr && authored->place.mode == pane_unit::kRightColumn) {
            where = placement::kSideRegion;
        }
        if (p.kind == kind) {
            const PaneProjection got = project_pane(where, slot, authored, sc,
                                                    panels.runtime.of_kind(kind), stack_y);
            return PanelBounds{true, where, got.visible, got.resolved, got.projected};
        }
        // A SLOT IS EARNED BY STANDING IN THE STACK AND SAYING NOTHING. A pane the desk placed
        // elsewhere is not in the stack to begin with, and one that named its own coordinates
        // does not queue for a rectangle it is not going to use.
        if (where == placement::kOverlayStack &&
            (authored == nullptr || authored->place.mode == pane_unit::kDefault)) {
            ++slot;
            const auto extent = preferred_extent(panels.runtime.of_kind(p.kind), capacity);
            stack_y += (extent.height ? extent.height : capacity.fallback_height) + capacity.gap;
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
    // Every test is the pointer's own grain against fine geometry (the aligned-span law), so a
    // pane answers on exactly the cells and pixels it paints.
    const std::vector<std::int64_t> order = effective_pane_order(setup, panels);
    for (std::size_t i = order.size(); i > 0; --i) {
        const std::int64_t kind = order[i - 1];
        if (bounds_of(panels, setup, kind, sc).rect.contains_at(at.sub.x, at.sub.y, at.grain)) {
            // `kind_name`, not `panel_kind(kind).name`: the total lookup answers a fallback
            // built-in row for any kind outside the compile-time catalog, misnaming an external
            // pane.
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
