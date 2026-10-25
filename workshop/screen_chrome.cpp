// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's pane chrome, the authored intent projected onto this screen, and placement spent
// on the pointer.
// Workshop law: agents/workshop/panes-and-windows.md (+8 registers; agents/workshop.md routes)

#include "screen.hpp"

#include "ui/layout.hpp"

#include <algorithm>
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
constexpr std::int64_t kStackPart = 3;
constexpr std::int64_t kFirstPanePart = 4;

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

/// The desk's three frames on this screen: the room under the top band, where every place word is
/// measured from; the top band, whole; and the stack, the room's top-left, as wide as the
/// catalog's slot and the room's height, laying its panes down a cell apart and beginning again at
/// its top.
// WL-GEO-03 -- agents/workshop/geometry.md
// WL-PANE-01, WL-PANE-03 -- agents/workshop/panes-and-windows.md
void add_desk_frames(std::vector<ui::Element>& parts, std::vector<ui::Measure>& measures,
                     const Screen& sc) {
    const PixelRect slot = placement_bounds(placement::kOverlayStack, 0, sc);
    parts.push_back(frame_part(kRoomPart, "room", ui::kRootContext, pixels_place(0, sc.room_y),
                               pixels_size(sc.room_w), pixels_size(sc.room_h)));
    parts.push_back(frame_part(kTopBandPart, "top band", ui::kRootContext, pixels_place(0, 0),
                               pixels_size(sc.w), pixels_size(sc.room_y)));
    ui::Element stack = frame_part(kStackPart, "stack", kRoomPart, ui::Place{}, pixels_size(slot.w),
                                   ui::Size{ui::kSizeFill, std::nullopt});
    stack.flow = ui::kFlowStack;
    stack.gap = graphics::px_of_cells(kStackGap);
    parts.push_back(stack);
    measures.resize(parts.size());
}

/// One pane's own measure, per axis: what its latest offer asks for where the axis says `fit`,
/// else its first offer's -- `default`'s, which a content change cannot move -- else its place's
/// own extent, the developer's answer a weaver's word lays over. A `fit` axis of a pane that
/// offered nothing stands at that default.
// WL-PANE-17 -- agents/workshop/panes-and-windows.md
ui::Measure pane_measure(std::int64_t where, const Screen& sc, const RuntimePane* preference,
                         const SetupPane* authored) {
    const PixelRect own = placement_bounds(where, 0, sc);
    const StackCapacity room = stack_capacity(sc);
    const PreferredExtent first = preferred_extent(preference, room);
    const PreferredExtent now = fit_extent(preference, room);
    const auto axis = [](std::int64_t latest, std::int64_t earliest, std::int64_t fallback,
                         bool fits) {
        if (fits && latest) {
            return latest;
        }
        return earliest ? earliest : fallback;
    };
    const bool fit_w = authored != nullptr && authored->width.mode == ui::kSizeFit;
    const bool fit_h = authored != nullptr && authored->height.mode == ui::kSizeFit;
    return ui::Measure{axis(now.width, first.width, own.w, fit_w),
                       axis(now.height, first.height, own.h, fit_h)};
}

/// One pane as a part: its sizes are the weaver's words, `default` -- its measure -- where they
/// said none; it stands in the room at its place word, or, for a `default` place, in its
/// catalog place's frame: down the stack, or in the top band.
// WL-PANE-01, WL-PANE-11 -- agents/workshop/panes-and-windows.md
ui::Element pane_part(std::int64_t id, std::int64_t where, const SetupPane* authored) {
    ui::Element e;
    e.id = id;
    if (authored != nullptr) {
        e.width = ui_size_of(authored->width);
        e.height = ui_size_of(authored->height);
    }
    const std::int64_t place = authored != nullptr ? authored->place.mode : pane_unit::kDefault;
    if (place != pane_unit::kDefault) {
        e.context = kRoomPart;
        e.place = ui_place_of(authored->place);
    } else if (where == placement::kTopBand) {
        e.context = kTopBandPart;
    } else {
        e.context = kStackPart;
    }
    return e;
}

PixelRect pixel_rect_of(const ui::Rect& r) { return PixelRect{r.x, r.y, r.w, r.h}; }

ui::Room room_of(const Screen& sc) { return ui::Room{sc.w, sc.h, chrome_grain(sc)}; }

/// The desk solved: open pane `i` is the scene's item `first + i`.
struct SolvedDesk {
    ui::Scene scene;
    std::size_t first = 0;
};

/// THE DESK AS ONE SOLVE: its frames, then every open pane a part, in the order the panes are open.
SolvedDesk solve_desk(const Panes& panes, const Setup& setup, const Screen& sc) {
    // Each desk row resolved once, so the solve costs one pass over the desk, not one per open pane.
    std::vector<std::pair<std::int64_t, const SetupPane*>> named_rows;
    named_rows.reserve(setup.panes.size());
    for (const SetupPane& row : setup.panes) {
        if (const std::optional<std::int64_t> named = resolve_pane(row.ref, panes)) {
            named_rows.emplace_back(*named, &row);
        }
    }
    std::vector<ui::Element> parts;
    std::vector<ui::Measure> measures;
    parts.reserve(3 + panes.open.size());
    measures.reserve(3 + panes.open.size());
    add_desk_frames(parts, measures, sc);
    const std::size_t first = parts.size();
    for (std::size_t i = 0; i < panes.open.size(); ++i) {
        const OpenPane& p = panes.open[i];
        const SetupPane* authored = nullptr;
        for (const auto& [named, row] : named_rows) {
            if (named == p.kind) {
                authored = row;
                break;
            }
        }
        const std::int64_t where = placement_of(p.kind);
        measures.push_back(pane_measure(where, sc, panes.runtime.of_kind(p.kind), authored));
        parts.push_back(pane_part(kFirstPanePart + static_cast<std::int64_t>(i), where, authored));
    }
    // Every part stands in a frame that reaches the root, so the scene holds each, in order.
    return SolvedDesk{ui::arrange(parts, room_of(sc), measures), first};
}

PaneBounds open_bounds(std::int64_t kind, const ui::Rect& solved, const Screen& sc) {
    const PixelRect resolved = pixel_rect_of(solved);
    return PaneBounds{true, placement_of(kind), clip_to_canvas_px(resolved, sc), resolved};
}

PaneBounds closed_bounds(std::int64_t kind) {
    return PaneBounds{false, placement_of(kind), PixelRect{}, PixelRect{}};
}

} // namespace

// WL-GEO-06 -- agents/workshop/geometry.md
// WL-PANE-01, WL-PANE-08, WL-PANE-11 -- agents/workshop/panes-and-windows.md
PaneProjection project_pane(std::int64_t where, std::size_t slot,
                            const SetupPane* authored, const Screen& sc,
                            const RuntimePane* preference, std::int64_t stack_y) {
    std::vector<ui::Element> parts;
    std::vector<ui::Measure> measures;
    add_desk_frames(parts, measures, sc);
    const ui::Measure measure = pane_measure(where, sc, preference, authored);
    ui::Element pane = pane_part(kFirstPanePart, where, authored);
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
    for (std::size_t i = 0; i < panes.open.size(); ++i) {
        if (panes.open[i].kind == kind) { // the first open pane of this kind
            const SolvedDesk desk = solve_desk(panes, setup, sc);
            return open_bounds(kind, desk.scene.items[desk.first + i].rect, sc);
        }
    }
    return closed_bounds(kind);
}

DeskBounds desk_bounds(const Panes& panes, const Setup& setup, const Screen& sc) {
    DeskBounds out;
    if (panes.open.empty()) return out;
    const SolvedDesk desk = solve_desk(panes, setup, sc);
    for (std::size_t i = 0; i < panes.open.size(); ++i) {
        const std::int64_t kind = panes.open[i].kind;
        const auto seen = std::find_if(out.laid.begin(), out.laid.end(),
                                       [&](const auto& laid) { return laid.first == kind; });
        if (seen != out.laid.end()) continue; // the first open pane of a kind answers for it
        out.laid.emplace_back(kind, open_bounds(kind, desk.scene.items[desk.first + i].rect, sc));
    }
    return out;
}

PaneBounds DeskBounds::of(std::int64_t kind) const {
    const auto found = std::find_if(laid.begin(), laid.end(),
                                    [&](const auto& one) { return one.first == kind; });
    return found != laid.end() ? found->second : closed_bounds(kind);
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
