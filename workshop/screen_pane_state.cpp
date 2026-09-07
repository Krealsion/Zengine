// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- the dynamic panels painted, what state one pane is in,
// a pane's geometry in the face's own language, and a surface sized by what it says -- compiled
// once into `zengine-workshop-logic` and linked by the host and every suite; the declarations,
// the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/panes-and-windows.md (+7 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- The dynamic panels, painted -------------------------------------------------------

// WL-CHROME-04 -- agents/workshop/chrome.md; WL-PANE-05 -- agents/workshop/panes-and-windows.md
// WL-CHROME-04 -- agents/workshop/chrome.md; WL-PANE-05 -- agents/workshop/panes-and-windows.md
void paint_panel_frame(surface::SurfaceLayer& layer, const FineRect& b,
                       std::int64_t role) {
    layer.rects.push_back(wire_rect_of(b, role));
}

// WL-CHROME-05 -- agents/workshop/chrome.md; WL-RGN-01 -- agents/workshop/regions.md
PanelProsePlace panel_prose_place(const FineRect& b, const Screen& sc) {
    PanelProsePlace p;
    const PaneInside inside = pane_inside(b, sc);
    p.inside = inside.rect;
    p.chrome_subs = inside.chrome_subs;
    if (inside.rect.w <= 0 || inside.rect.h <= 0) {
        return p;
    }
    p.fit = inside.fit;
    p.rows = p.fit.rows;
    p.columns = p.fit.columns;
    p.present = p.rows > 0 && p.columns > 0;
    return p;
}

// WL-RGN-01 -- agents/workshop/regions.md
surface::SurfaceTextRegion panel_prose_region(const PanelProsePlace& place) {
    surface::SurfaceTextRegion region;
    const surface::SurfaceRect wire = wire_rect_of(place.inside, surface::role::kFill);
    region.x = wire.x;
    region.y = wire.y;
    region.w = wire.w;
    region.h = wire.h;
    region.sub_x = wire.sub_x;
    region.sub_y = wire.sub_y;
    region.sub_w = wire.sub_w;
    region.sub_h = wire.sub_h;
    return region;
}

// ---- WHAT STATE ONE PANE IS IN -- the recovery invariant, as one word -----------------

const char* pane_state_word(std::int64_t state) {
    switch (state) {
    case pane_state::kUnresolved: return "unresolved";
    case pane_state::kRefused: return "refused";
    case pane_state::kWaiting: return "waiting";
    case pane_state::kOffRoom: return "off-room";
    case pane_state::kCovered: return "covered";
    case pane_state::kOpen: return "open";
    default: return "closed";
    }
}

// WL-PANE-10 -- agents/workshop/panes-and-windows.md
const char* pane_state_remedy(std::int64_t state) {
    switch (state) {
    case pane_state::kClosed: return "open it from the picker";
    case pane_state::kUnresolved: return "check the spelling, or the provider is not loaded";
    case pane_state::kRefused: return "reset its size, or open it on the other medium";
    case pane_state::kWaiting: return "make the window taller, or place it yourself";
    case pane_state::kOffRoom: return "reset its place";
    case pane_state::kCovered: return "raise it";
    default: return "";
    }
}

// WL-PANE-10 -- agents/workshop/panes-and-windows.md; WL-FRONT-05 -- agents/workshop/planes.md
bool pane_is_covered(const Panels& panels, const Setup& setup, const Screen& sc,
                     std::int64_t kind, const FineRect& mine) {
    if (mine.w <= 0 || mine.h <= 0) {
        return false; // nothing visible is OFF-ROOM, which is a different word
    }
    const std::vector<std::int64_t> order = effective_pane_order(setup, panels);
    std::size_t me = order.size();
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == kind) {
            me = i;
            break;
        }
    }
    if (me == order.size()) {
        return false;
    }
    std::vector<FineRect> ahead;
    for (std::size_t i = me + 1; i < order.size(); ++i) {
        const FineRect r = bounds_of(panels, setup, order[i], sc).rect;
        if (r.w > 0 && r.h > 0) {
            ahead.push_back(r);
        }
    }
    if (ahead.empty()) {
        return false;
    }
    // EXACT ON THE FINE LATTICE, BY EDGE COMPRESSION. The union of a handful of
    // rectangles is constant between their edges, so the question "is every sub-unit of
    // mine behind the union" needs one representative point per edge-bounded stripe —
    // never a walk of the lattice, which at this resolution would be forty-eight squared
    // points per cell of what used to be one. A pane peeking out by a single sub-unit
    // produces a stripe whose representative is visible, so a maker's sliver still means
    // `open` — one thing a maker can see is enough, exactly as it always was.
    std::vector<std::int64_t> xs{mine.x, surface::add_cells(mine.x, mine.w)};
    std::vector<std::int64_t> ys{mine.y, surface::add_cells(mine.y, mine.h)};
    for (const FineRect& r : ahead) {
        xs.push_back(r.x);
        xs.push_back(surface::add_cells(r.x, r.w));
        ys.push_back(r.y);
        ys.push_back(surface::add_cells(r.y, r.h));
    }
    std::sort(xs.begin(), xs.end());
    std::sort(ys.begin(), ys.end());
    const std::int64_t right = surface::add_cells(mine.x, mine.w);
    const std::int64_t bottom = surface::add_cells(mine.y, mine.h);
    for (std::size_t yi = 0; yi + 1 < ys.size(); ++yi) {
        const std::int64_t y = ys[yi];
        if (y < mine.y || y >= bottom || ys[yi + 1] == y) {
            continue;
        }
        for (std::size_t xi = 0; xi + 1 < xs.size(); ++xi) {
            const std::int64_t x = xs[xi];
            if (x < mine.x || x >= right || xs[xi + 1] == x) {
                continue;
            }
            bool hidden = false;
            for (const FineRect& r : ahead) {
                if (x >= r.x && x < surface::add_cells(r.x, r.w) && y >= r.y &&
                    y < surface::add_cells(r.y, r.h)) {
                    hidden = true;
                    break;
                }
            }
            if (!hidden) {
                return false; // one place a maker can see is enough
            }
        }
    }
    return true;
}

std::int64_t pane_state_of(const Panels& panels, const Setup& setup, const Screen& sc,
                           const CatalogRow& row) {
    if (!has_pane(setup, row.ref)) {
        return pane_state::kClosed;
    }
    if (row.kind == kNoPaneKind || !resolvable(row.ref, panels)) {
        return pane_state::kUnresolved;
    }
    // A UNIT OUTRANKS A WANT OF ROOM, and this is where that precedence is spent. A pane
    // with a pixel axis AND no tile left is refused rather than waiting: a taller window
    // would give it the tile and it still would not be presented, so telling the maker to
    // make the window taller would be a true sentence about the wrong problem.
    if (!pane_unit_projectable(pane_of(setup, row.ref))) {
        return pane_state::kRefused;
    }
    const PanelBounds where = bounds_of(panels, setup, row.kind, sc);
    if (!where.open) {
        // Named, resolved, projectable and not presented -- which is what `waiting` has
        // always meant here. `seat_panes` is the only thing that produces it and it is
        // medium-independent, which is why this branch does not consult one.
        return pane_state::kWaiting;
    }
    if (!where.projected) {
        return pane_state::kRefused;
    }
    if (where.rect.w <= 0 || where.rect.h <= 0) {
        return pane_state::kOffRoom;
    }
    if (pane_is_covered(panels, setup, sc, row.kind, where.rect)) {
        return pane_state::kCovered;
    }
    return pane_state::kOpen;
}

// WL-PED-01 -- agents/workshop/pane-manager.md
std::string picker_entry_text(const std::string& name, const char* state,
                              const std::string& tail) {
    return detail::pad(detail::fit(name, static_cast<std::int64_t>(kPickerNameCols)),
                       kPickerNameCols) +
           detail::pad(state, kPaneStateCols) + tail;
}

void paint_picker(surface::SurfaceLayer& layer, const Panels& panels, const Setup& setup,
                  const Screen& sc, const Keymap& keymap) {
    const PanelPicker& picker = panels.picker;
    if (!picker.open) {
        return;
    }
    const FineRect b = picker_bounds(sc);
    paint_panel_frame(layer, b, kTransientChrome);
    // THE PICKER IS ONE BOUNDED REGION OF PROSE, and the budget it spends is the
    // ACTIVE medium's row count rather than the slot's cell count. The two are the same
    // number in a character medium and they are not in one that sets real type -- nine cells
    // of slot is nine rows of a terminal and five rows of an 18-pixel face -- which is the
    // same pair of honest projections the Info panel's body has had.
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a slot with no room for a row says nothing rather than lying about the room
    }
    surface::SurfaceTextRegion region = panel_prose_region(place);
    const auto say = [&region, &place](const std::string& text, std::int64_t role) {
        region.rows.push_back(
            surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    say("+ PANEL -- " + hotkey_text(keymap, Act::kPickerUp) + "/" +
            hotkey_text(keymap, Act::kPickerDown) + ", " +
            hotkey_text(keymap, Act::kPickerChoose) + " opens or removes",
        surface::role::kAccent);
    // THE POPULATION IS THE COMBINED CATALOG AND THE BUDGET IS THE SLOT'S.
    // Before this the list was `kPanelKinds` long and the picker's height was a
    // constant derived from it, which is a catalog census standing in for a
    // capacity -- it was right for exactly as long as no catalog could outgrow
    // the box, and a runtime offer is precisely a catalog that can. So the rows
    // under the heading are `list_window`'s to spend: the OBJECTS list's own
    // function, its own three rules and its own wording (`omitted_text`), which
    // is the second consumer the rule was established with and the fourth
    // overall. There is no second scrolling algorithm here and the picker did not
    // get taller.
    //
    // AND THE POPULATION IS THE SHARED INVENTORY -- the catalog UNION every
    // reference the setup names -- so a pane a maker authored and this build cannot resolve
    // has a row here too, and can be removed with the gesture that removes any other.
    const std::vector<CatalogRow> rows = inventory_rows(setup, panels);
    const std::size_t budget =
        place.rows > 1 ? static_cast<std::size_t>(place.rows - 1) : 0;
    const ListWindow win = list_window(rows.size(), picker.cursor, budget);
    if (win.before > 0) {
        say("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
    }
    for (std::size_t i = win.first; i < win.first + win.count; ++i) {
        const bool here = i == picker.cursor;
        say(std::string(here ? "> " : "  ") +
                picker_entry_text(rows[i].name,
                                  pane_state_word(pane_state_of(panels, setup, sc, rows[i])),
                                  rows[i].summary),
            here ? surface::role::kAccent : surface::role::kFill);
    }
    if (win.after > 0) {
        say("  " + omitted_text(win.after, "more"), surface::role::kMuted);
    }
    // THE REST OF THE SLOT IS THE REGION'S OWN EMPTINESS, and nobody writes it.
    // A region owns what is inside its bounds, so its cell projection already pads every row
    // it was not given -- the spaces that erase the panel underneath in a character medium are
    // `project_one_text_region`'s, and the graphical medium clears the same rectangle once
    // rather than a row at a time. What used to be a loop padding out to `b.h` is now the
    // primitive's contract, which is why this painter no longer has one. See kPickerRows for
    // why the whole slot is covered at all.
    layer.texts.push_back(std::move(region));
}

// ---- SAYING A PANE'S GEOMETRY IN THE FACE'S OWN LANGUAGE ------------------------------

const char* geometry_unit(std::int64_t cell_px) {
    return cell_px > 0 ? "px" : "cells";
}

// WL-GEO-09, WL-GEO-10 -- agents/workshop/geometry.md
GeometrySpelling geometry_spelling(std::int64_t subs, std::int64_t cell_px) {
    return GeometrySpelling{std::to_string(surface::device_of_subs(subs, cell_px)),
                            surface::subs_exact_in_device(subs, cell_px)};
}

std::string geometry_amount_text(std::int64_t subs, std::int64_t cell_px,
                                 bool& any_projected) {
    const GeometrySpelling spelled = geometry_spelling(subs, cell_px);
    if (spelled.exact) {
        return spelled.amount;
    }
    any_projected = true;
    return std::string(kProjectedMark) + spelled.amount;
}

FaceAmount parse_face_amount(std::string_view text, std::int64_t cell_px) {
    FaceAmount out;
    const auto trim = [](std::string_view v) {
        while (!v.empty() && v.front() == ' ') {
            v.remove_prefix(1);
        }
        while (!v.empty() && v.back() == ' ') {
            v.remove_suffix(1);
        }
        return v;
    };
    std::string_view body = trim(text);
    const std::string_view unit = geometry_unit(cell_px);
    const std::string_view other = cell_px > 0 ? "cells" : "px";
    if (body.size() > other.size() &&
        body.substr(body.size() - other.size()) == other) {
        out.refusal = "this face reads " + std::string(unit) + ", not " + std::string(other);
        return out;
    }
    if (body.size() > unit.size() && body.substr(body.size() - unit.size()) == unit) {
        body = trim(body.substr(0, body.size() - unit.size()));
    }
    const std::optional<std::int64_t> amount = TextForm<std::int64_t>::parse(body);
    if (!amount) {
        out.refusal = "not a whole number of " + std::string(unit) + " (`-` resets it)";
        return out;
    }
    out.accepted = true;
    out.subs = subs_of_device_amount(*amount, cell_px);
    return out;
}

std::string fine_rect_text(const FineRect& r, std::int64_t cell_px) {
    bool projected = false;
    std::string text = "@" + geometry_amount_text(r.x, cell_px, projected) + "," +
                       geometry_amount_text(r.y, cell_px, projected) + " " +
                       geometry_amount_text(r.w, cell_px, projected) + "x" +
                       geometry_amount_text(r.h, cell_px, projected) + " " +
                       geometry_unit(cell_px);
    if (projected) {
        text += kProjectedNote;
    }
    return text;
}

// WL-GEO-09 -- agents/workshop/geometry.md
std::string pane_window_text(const SetupPane* row, std::int64_t cell_px) {
    if (row == nullptr) {
        return "--";
    }
    bool projected = false;
    const auto axis = [cell_px, &projected](const PaneSize& s) -> std::string {
        if (s.mode == pane_unit::kSubcells) {
            return geometry_amount_text(s.amount, cell_px, projected);
        }
        if (s.mode == pane_unit::kPixels) {
            return std::to_string(s.amount) + "px";
        }
        return std::string("-");
    };
    std::string text;
    if (row->place.mode == pane_unit::kSubcells) {
        text += "@" + geometry_amount_text(row->place.x, cell_px, projected) + "," +
                geometry_amount_text(row->place.y, cell_px, projected) + " ";
    }
    text += axis(row->width) + "x" + axis(row->height);
    // THE UNIT IS SAID ONCE, AND ONLY WHERE A NUMBER IN IT WAS PRINTED. A row default
    // on every axis has said nothing measurable, and appending `cells` to `-x-` would
    // be naming the unit of a number that is not there.
    if (row->place.mode == pane_unit::kSubcells || row->width.mode == pane_unit::kSubcells ||
        row->height.mode == pane_unit::kSubcells) {
        text += " " + std::string(geometry_unit(cell_px));
    }
    text += " f" + std::to_string(row->front);
    if (projected) {
        text += kProjectedNote;
    }
    return text;
}

// WL-GEO-12 -- agents/workshop/geometry.md
bool pane_window_partly_default(const SetupPane* row) {
    if (row == nullptr) {
        return false;
    }
    return row->place.mode == pane_unit::kDefault || row->width.mode == pane_unit::kDefault ||
           row->height.mode == pane_unit::kDefault;
}

// ---- A SURFACE SIZED BY WHAT IT SAYS, PLACED ---------------------------------------------

// WL-CTX-03 -- agents/workshop/contextual.md; WL-KEY-10 -- agents/workshop/keyboard.md
FineRect popup_bounds_at(std::int64_t want_cols, std::int64_t want_rows,
                         std::int64_t x, std::int64_t y, const Screen& sc) {
    const surface::RegionCells cells =
        surface::region_cells_for(want_cols, want_rows, sc.text_advance_px, sc.text_line_px);
    const ui::Rect outer = chrome_outer_of(0, 0, cells.w, cells.h);
    const std::int64_t floor_y = kWorkspaceY + sc.room_h;
    const std::int64_t w = outer.w > sc.w ? sc.w : outer.w;
    const std::int64_t room_rows = floor_y - kStackY;
    const std::int64_t h = outer.h > room_rows ? room_rows : outer.h;
    if (x + w > sc.w) {
        x = sc.w - w;
    }
    if (x < 0) {
        x = 0;
    }
    if (y + h > floor_y) {
        y = floor_y - h;
    }
    if (y < kStackY) {
        y = kStackY;
    }
    return fine_of_cells(ui::Rect{x, y, w, h});
}

} // namespace zengine::workshop
