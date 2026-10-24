// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's pane chrome, the authored intent projected onto this screen, and placement spent
// on the pointer.
// Workshop law: agents/workshop/panes-and-windows.md (+8 registers; agents/workshop.md routes)

#include "screen.hpp"

#include "ui/layout.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

// ---- THE CHROME A PANE WEARS, AND THE INTERIOR IT LEAVES --------------------------------

namespace detail {

PaneInside pane_inside_at(const PixelRect& outer, const Screen& sc,
                          std::int64_t chrome_px) {
    PaneInside p;
    p.chrome_px = chrome_px;
    p.rect = pane_interior(outer, chrome_px);
    if (p.rect.w <= 0 || p.rect.h <= 0) {
        return p;
    }
    p.fit = fit_region(p.rect.x, p.rect.y, p.rect.w, p.rect.h,
                                     sc.text_advance_px, sc.text_line_px);
    return p;
}

} // namespace detail

// WL-CHROME-01, WL-CHROME-03, WL-CHROME-04, WL-CHROME-07 -- agents/workshop/chrome.md
PaneInside pane_inside(const PixelRect& outer, const Screen& sc) {
    const std::int64_t fine = chrome_grain(sc);
    if (fine < kChromePx) {
        const PaneInside thin = detail::pane_inside_at(outer, sc, fine);
        if (thin.fit.graphical()) {
            return thin;
        }
    }
    const PaneInside cell = detail::pane_inside_at(outer, sc, kChromePx);
    if (cell.rect.w > 0 && cell.rect.h > 0) {
        return cell;
    }
    return detail::pane_inside_at(outer, sc, 0);
}

PixelRect pane_interior(const PixelRect& outer, const Screen& sc) {
    return pane_inside(outer, sc).rect;
}

// ---- AUTHORED INTENT, PROJECTED ONTO THIS SCREEN -------------------------------------
// The desk is solved by `ui::arrange`: its places are frames, each open pane a part standing in
// one, and the pane's rectangle is what the solve answers for it.

namespace {

// The desk's frames as parts of the solve. A pane's part takes `kFirstPanePart` plus its position
// among the open panes, so no pane shares an identity with a frame.
constexpr std::int64_t kRoomPart = 1;
constexpr std::int64_t kTopBandPart = 2;
constexpr std::int64_t kRightColumnPart = 3;
constexpr std::int64_t kStackPart = 4;
constexpr std::int64_t kFirstPanePart = 5;

ui::Size pixels_size(std::int64_t px) { return ui::Size{ui::kSizePixels, px}; }

ui::Place pixels_place(std::int64_t x, std::int64_t y) {
    return ui::Place{ui::kPlacePixels, x, y};
}

ui::Element frame_part(std::int64_t id, std::string label, std::int64_t context, ui::Place place,
                       ui::Size width, ui::Size height) {
    ui::Element e;
    e.id = id;
    e.label = std::move(label);
    e.context = context;
    e.place = place;
    e.width = width;
    e.height = height;
    return e;
}

/// The desk's four places on this screen, as frames: the room under the top band, where a
/// `pixels` place is measured from; the top band, whole; the right column, the room's top-right,
/// `workshop.side-columns` wide and the room's height; and the stack, the room's top-left, as wide
/// as the catalog's slot and the room's height, laying its panes down a cell apart and beginning
/// again at its top.
// WL-GEO-03 -- agents/workshop/geometry.md
// WL-PANE-01, WL-PANE-03 -- agents/workshop/panes-and-windows.md
void add_desk_frames(std::vector<ui::Element>& parts, std::vector<ui::Measure>& measures,
                     const Screen& sc) {
    const PixelRect column = placement_bounds(placement::kSideRegion, 0, sc);
    const PixelRect slot = placement_bounds(placement::kOverlayStack, 0, sc);
    parts.push_back(frame_part(kRoomPart, "room", ui::kRootContext, pixels_place(0, sc.room_y),
                               pixels_size(sc.room_w), pixels_size(sc.room_h)));
    parts.push_back(frame_part(kTopBandPart, "top band", ui::kRootContext, pixels_place(0, 0),
                               pixels_size(sc.w), pixels_size(sc.room_y)));
    parts.push_back(frame_part(kRightColumnPart, "right column", kRoomPart,
                               ui::Place{ui::kPlaceTopRight, 0, 0}, pixels_size(column.w),
                               ui::Size{ui::kSizeFill, std::nullopt}));
    ui::Element stack = frame_part(kStackPart, "stack", kRoomPart, ui::Place{}, pixels_size(slot.w),
                                   ui::Size{ui::kSizeFill, std::nullopt});
    stack.flow = ui::kFlowStack;
    stack.gap = graphics::px_of_cells(kStackGap);
    parts.push_back(stack);
    measures.resize(parts.size());
}

/// One pane's own measure: its first offered extent per axis, else its place's own extent, the
/// developer's answer a weaver's word lays over.
// WL-PANE-17 -- agents/workshop/panes-and-windows.md
ui::Measure pane_measure(std::int64_t where, const Screen& sc, const RuntimePane* preference) {
    const PixelRect own = placement_bounds(where, 0, sc);
    const auto preferred = preferred_extent(preference, stack_capacity(sc));
    return ui::Measure{preferred.width ? preferred.width : own.w,
                       preferred.height ? preferred.height : own.h};
}

/// One pane as a part: its sizes are the weaver's `pixels` where they gave one and `default` --
/// its measure -- where they did not; it stands in the room at its `pixels` place, or in its
/// place's frame: down the stack, in the top band, or in the right column at the offset that
/// puts its measured width against the column's right edge, which an authored width keeps.
// WL-PANE-01, WL-PANE-11 -- agents/workshop/panes-and-windows.md
ui::Element pane_part(std::int64_t id, std::int64_t where, const SetupPane* authored,
                      const ui::Measure& measure, const Screen& sc) {
    ui::Element e;
    e.id = id;
    if (authored != nullptr && authored->width.mode == pane_unit::kPixels) {
        e.width = pixels_size(authored->width.amount);
    }
    if (authored != nullptr && authored->height.mode == pane_unit::kPixels) {
        e.height = pixels_size(authored->height.amount);
    }
    if (authored != nullptr && authored->place.mode == pane_unit::kPixels) {
        e.context = kRoomPart;
        e.place = pixels_place(authored->place.x, authored->place.y);
    } else if (where == placement::kSideRegion) {
        e.context = kRightColumnPart;
        e.place = pixels_place(placement_bounds(where, 0, sc).w - measure.w.value_or(0), 0);
    } else if (where == placement::kTopBand) {
        e.context = kTopBandPart;
    } else {
        e.context = kStackPart;
    }
    return e;
}

PixelRect pixel_rect_of(const ui::Rect& r) { return PixelRect{r.x, r.y, r.w, r.h}; }

ui::Room room_of(const Screen& sc) { return ui::Room{sc.w, sc.h, chrome_grain(sc)}; }

} // namespace

// WL-GEO-06 -- agents/workshop/geometry.md
// WL-PANE-01, WL-PANE-08, WL-PANE-11 -- agents/workshop/panes-and-windows.md
PaneProjection project_pane(std::int64_t where, std::size_t slot,
                            const SetupPane* authored, const Screen& sc,
                            const RuntimePane* preference, std::int64_t stack_y) {
    std::vector<ui::Element> parts;
    std::vector<ui::Measure> measures;
    add_desk_frames(parts, measures, sc);
    const ui::Measure measure = pane_measure(where, sc, preference);
    ui::Element pane = pane_part(kFirstPanePart, where, authored, measure, sc);
    // Projected alone, a pane in the stack stands at the slot it is given: its top `stack_y`, or
    // the slot's own where none is given.
    if (pane.context == kStackPart) {
        const std::int64_t top =
            stack_y >= 0 ? stack_y : placement_bounds(placement::kOverlayStack, slot, sc).y;
        pane.context = kRoomPart;
        pane.place = pixels_place(0, top - sc.room_y);
    }
    parts.push_back(pane);
    measures.push_back(measure);
    const ui::Scene scene = ui::arrange(parts, room_of(sc), measures);
    PaneProjection out;
    out.resolved = pixel_rect_of(scene.items.back().rect);
    out.visible = clip_to_canvas_px(out.resolved, sc);
    return out;
}

// WL-PANE-03, WL-PANE-07, WL-PANE-09 -- agents/workshop/panes-and-windows.md
PaneBounds bounds_of(const Panes& panes, const Setup& setup, std::int64_t kind,
                      const Screen& sc) {
    // Each desk row resolved once, so building the desk costs one pass over it, not one per open
    // pane: a desk of many panes is asked this for every pane on every repaint.
    std::vector<std::pair<std::int64_t, const SetupPane*>> named_rows;
    named_rows.reserve(setup.panes.size());
    for (const SetupPane& row : setup.panes) {
        if (const std::optional<std::int64_t> named = resolve_pane(row.ref, panes)) {
            named_rows.emplace_back(*named, &row);
        }
    }
    std::vector<ui::Element> parts;
    std::vector<ui::Measure> measures;
    parts.reserve(4 + panes.open.size());
    measures.reserve(4 + panes.open.size());
    add_desk_frames(parts, measures, sc);
    std::optional<std::size_t> asked; // the first open pane of this kind
    std::int64_t asked_where = placement_of(kind);
    for (std::size_t i = 0; i < panes.open.size(); ++i) {
        const OpenPane& p = panes.open[i];
        const SetupPane* authored = nullptr;
        for (const auto& [named, row] : named_rows) {
            if (named == p.kind) {
                authored = row;
                break;
            }
        }
        // The desk may name the place, here and only here: a row saying `kRightColumn` names the
        // place no coordinate can; authored extents still lay over it.
        std::int64_t where = placement_of(p.kind);
        if (authored != nullptr && authored->place.mode == pane_unit::kRightColumn) {
            where = placement::kSideRegion;
        }
        const ui::Measure measure = pane_measure(where, sc, panes.runtime.of_kind(p.kind));
        if (!asked && p.kind == kind) {
            asked = parts.size();
            asked_where = where;
        }
        parts.push_back(pane_part(kFirstPanePart + static_cast<std::int64_t>(i), where, authored,
                                  measure, sc));
        measures.push_back(measure);
    }
    if (!asked) {
        return PaneBounds{false, placement_of(kind), PixelRect{}, PixelRect{}};
    }
    // Every part stands in a frame that reaches the root, so the scene holds each, in order.
    const ui::Scene scene = ui::arrange(parts, room_of(sc), measures);
    const PixelRect resolved = pixel_rect_of(scene.items[*asked].rect);
    return PaneBounds{true, asked_where, clip_to_canvas_px(resolved, sc), resolved};
}

// ---- PLACEMENT SPENT ON THE POINTER: a place a weaver can see is a place a hand meets ------

PointedAt canvas_point_of(std::int64_t space, std::int64_t x, std::int64_t y) noexcept {
    if (space == input::space::kCells) {
        return PointedAt{true, CanvasPoint{x, y},
                         CanvasPoint{graphics::px_of_cells(x), graphics::px_of_cells(y)},
                         graphics::kCellGrainPx};
    }
    if (space == input::space::kPixels) {
        return PointedAt{true, CanvasPoint{graphics::cell_of_pixel(x), graphics::cell_of_pixel(y)},
                         CanvasPoint{x, y}, graphics::kPixelGrainPx};
    }
    return PointedAt{};
}

// WL-FRONT-02, WL-FRONT-05 -- agents/workshop/planes.md
// WL-PRESS-04, WL-PRESS-05 -- agents/workshop/press-chain.md
// WL-FOCUS-03 -- agents/workshop/focus.md
// WL-PANE-05 -- agents/workshop/panes-and-windows.md
// WL-TAB-09 -- agents/workshop/tab-run.md
Occupancy occupied_at(const Panes& panes, const Setup& setup, const Screen& sc,
                      const PointedAt& at) {
    // Every test is the pointer's own grain against fine geometry (the aligned-span law), so a
    // pane answers on exactly the cells and pixels it paints.
    const std::vector<std::int64_t> order = effective_pane_order(setup, panes);
    for (std::size_t i = order.size(); i > 0; --i) {
        const std::int64_t kind = order[i - 1];
        if (bounds_of(panes, setup, kind, sc).rect.contains_at(at.px.x, at.px.y, at.grain)) {
            // `kind_name`, not `builtin_pane(kind).name`: the total lookup answers a fallback
            // built-in row for any kind outside the compile-time catalog, misnaming an external
            // pane.
            return Occupancy{true, kind_name(panes, kind), kind};
        }
    }
    return Occupancy{};
}

// WL-FRONT-02, WL-FRONT-05 -- agents/workshop/planes.md
// WL-PRESS-04, WL-PRESS-05 -- agents/workshop/press-chain.md
// WL-FOCUS-03 -- agents/workshop/focus.md
// WL-PANE-05 -- agents/workshop/panes-and-windows.md
// WL-TAB-09 -- agents/workshop/tab-run.md
Occupancy occupied_at(const Panes& panes, const Setup& setup, const Screen& sc,
                      std::int64_t cx, std::int64_t cy) {
    return occupied_at(panes, setup, sc,
                       PointedAt{true, CanvasPoint{cx, cy},
                                 CanvasPoint{graphics::px_of_cells(cx),
                                                      graphics::px_of_cells(cy)},
                                 graphics::kCellGrainPx});
}

} // namespace zengine::workshop
