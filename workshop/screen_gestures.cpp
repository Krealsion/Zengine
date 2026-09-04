// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- reading past the ellipsis, the maker's gestures over
// one session, the size a hand asked for, the one resize affordance, where a pointer is, and what
// the OBJECTS panel can show -- compiled once into `zengine-workshop-logic` and linked by the
// host and every suite; the declarations, the constants and the constexpr functions stay in the
// header.
// Workshop law: agents/workshop/document.md (+11 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- Reading past the ellipsis -----------------------------------------------------------

namespace detail {

// WL-PTR-06 -- agents/workshop/pointer.md
std::size_t reveal_max_offset(const std::string& full, std::int64_t columns) {
    const std::size_t mark = std::char_traits<char>::length(kElided);
    if (columns <= 0 || static_cast<std::size_t>(columns) <= mark) {
        return 0;
    }
    const std::size_t room = static_cast<std::size_t>(columns) - mark;
    return full.size() > room ? full.size() - room : 0;
}

// WL-PTR-06 -- agents/workshop/pointer.md
std::int64_t reveal_offset_at_column(const std::string& full, std::int64_t columns,
                                     std::int64_t column) {
    const std::int64_t furthest = static_cast<std::int64_t>(reveal_max_offset(full, columns));
    if (furthest <= 0 || columns <= 1 || column <= 0) {
        return 0;
    }
    const std::int64_t last = columns - 1;
    return column >= last ? furthest : (furthest * column) / last;
}

// WL-PTR-04, WL-PTR-06 -- agents/workshop/pointer.md
std::string revealed_row(const std::string& full, std::int64_t columns,
                         std::int64_t offset) {
    if (columns <= 0) {
        return {};
    }
    const std::size_t furthest = reveal_max_offset(full, columns);
    if (offset <= 0 || furthest == 0) {
        return fit(full, columns);
    }
    const std::size_t want = static_cast<std::size_t>(offset);
    return std::string(kElided) +
           fit(full.substr(want < furthest ? want : furthest),
               columns - static_cast<std::int64_t>(std::char_traits<char>::length(kElided)));
}

// WL-PTR-04, WL-PTR-05, WL-PTR-08 -- agents/workshop/pointer.md
std::string reveal_shown(const Revealed& rev, std::int64_t place, std::size_t item,
                         const std::string& full, std::string rest,
                         std::int64_t columns) {
    if (rev.place != place || rev.item != item || rev.offset <= 0 || rev.text != full) {
        return rest;
    }
    return revealed_row(full, columns, rev.offset);
}

// WL-TERM-07 -- agents/workshop/terminal.md
std::vector<std::string> wrap(const std::string& text, std::int64_t width) {
    std::vector<std::string> rows;
    if (width <= 0) {
        return rows;
    }
    const std::size_t room = static_cast<std::size_t>(width);
    const std::size_t indent =
        width > kWrapIndent + 1 ? static_cast<std::size_t>(kWrapIndent) : 0;
    std::size_t at = 0;
    while (true) {
        const std::string lead(rows.empty() ? 0 : indent, ' ');
        const std::size_t take = room - lead.size();
        if (text.size() - at <= take) {
            rows.push_back(lead + text.substr(at));
            return rows;
        }
        // The last space at or before the first character that does not fit. Landing ON that
        // character means the whole run fits and the break is clean, which is why the search
        // starts there rather than one earlier.
        std::size_t cut = at + take;
        bool broke = false;
        for (std::size_t i = cut; i > at; --i) {
            if (text[i] == ' ') {
                cut = i;
                broke = true;
                break;
            }
        }
        rows.push_back(lead + text.substr(at, cut - at));
        at = cut;
        if (broke) {
            while (at < text.size() && text[at] == ' ') {
                ++at;
            }
        }
        if (at >= text.size()) {
            return rows;
        }
    }
}

std::int64_t step(std::int64_t v, std::int64_t by) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (by > 0) {
        return v > kMax - by ? v : v + by;
    }
    if (by < 0) {
        return v < kMin - by ? v : v + by;
    }
    return v;
}

std::int64_t minus(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (b < 0) {
        return a > kMax + b ? kMax : a - b;
    }
    return a < kMin + b ? kMin : a - b;
}

} // namespace detail

// WL-KEY-09 -- agents/workshop/keyboard.md; WL-RGN-03 -- agents/workshop/regions.md
std::vector<std::string> help_rows(const Keymap& k, KeyContext ctx,
                                   std::int64_t width, std::size_t rows) {
    std::vector<std::string> out;
    if (rows == 0) {
        return out;
    }
    const std::int64_t legend = k.resolved_legend();
    if (legend == legend_mode::kHidden) {
        return out;
    }
    if (legend == legend_mode::kCompact) {
        out.push_back(detail::fit(hotkey_text(k, Act::kHotkeys) + " hotkeys", width));
        return out;
    }
    const std::vector<std::string> pairs = help_pairs(k, ctx);
    std::string row;
    std::size_t taken = 0;
    for (const std::string& pair : pairs) {
        const std::string grown = row.empty() ? pair : row + " | " + pair;
        if (static_cast<std::int64_t>(grown.size()) <= width) {
            row = grown;
            ++taken;
            continue;
        }
        if (out.size() + 1 >= rows) {
            break; // this is the last row the legend was granted: the mark below says so
        }
        out.push_back(std::move(row));
        row.clear();
        if (static_cast<std::int64_t>(pair.size()) <= width) {
            row = pair;
            ++taken;
        }
    }
    // WHAT DID NOT FIT IS MARKED, NOT SWALLOWED: the next pair is written into the cut so
    // `detail::fit`'s mark says there was more -- a help row that silently loses its last
    // hints is the failure that mark exists to prevent, and the full list is one
    // keystroke away in every legend mode.
    if (taken < pairs.size()) {
        const std::string& next = pairs[taken];
        row = detail::fit(row.empty() ? next : row + " | " + next, width);
    }
    if (!row.empty()) {
        out.push_back(std::move(row));
    }
    return out;
}

PaneWindowProposal pane_window_proposal(std::int64_t edge, std::int64_t base_x,
                                        std::int64_t base_y, std::int64_t base_w,
                                        std::int64_t base_h, std::int64_t dx,
                                        std::int64_t dy) noexcept {
    PaneWindowProposal out{base_x, base_y, base_w, base_h, false, false};
    const bool wide = edge == pane_edge::kLeft || edge == pane_edge::kRight ||
                      edge == pane_edge::kTopLeft || edge == pane_edge::kTopRight ||
                      edge == pane_edge::kBottomLeft || edge == pane_edge::kBottomRight;
    const bool tall = edge == pane_edge::kTop || edge == pane_edge::kBottom ||
                      edge == pane_edge::kTopLeft || edge == pane_edge::kTopRight ||
                      edge == pane_edge::kBottomLeft || edge == pane_edge::kBottomRight;
    const bool leftwards = edge == pane_edge::kLeft || edge == pane_edge::kTopLeft ||
                           edge == pane_edge::kBottomLeft;
    const bool upwards = edge == pane_edge::kTop || edge == pane_edge::kTopLeft ||
                         edge == pane_edge::kTopRight;
    if (wide) {
        out.w = detail::step(base_w, leftwards ? detail::minus(0, dx) : dx);
        if (leftwards) {
            // The RIGHT edge is the anchor: base_x + base_w == x' + w', rearranged.
            out.x = detail::minus(detail::step(base_x, base_w), out.w);
            out.place_moved_x = true;
        }
    }
    if (tall) {
        out.h = detail::step(base_h, upwards ? detail::minus(0, dy) : dy);
        if (upwards) {
            // The BOTTOM edge is the anchor.
            out.y = detail::minus(detail::step(base_y, base_h), out.h);
            out.place_moved_y = true;
        }
    }
    return out;
}

// ---- The maker's gestures over one session ---------------------------------------------

// WL-DOC-10 -- agents/workshop/document.md
std::int64_t create(WorkshopDoc& d, Session& s) {
    const std::int64_t id = doc::add_default(d);
    if (id == 0) {
        return 0;
    }
    s.selected = id;
    refocus(d, s);
    return id;
}

// WL-DOC-10 -- agents/workshop/document.md; WL-CTX-07 -- agents/workshop/contextual.md
Written delete_selected(WorkshopDoc& d, Session& s) {
    const std::int64_t id = s.selected;
    const std::size_t at = position_of(d, id);
    const Written removed = doc::remove(d, id);
    if (!removed.accepted) {
        return removed;
    }
    if (d.elements.empty()) {
        s.selected = 0;
    } else {
        s.selected = (at < d.elements.size() ? d.elements[at] : d.elements.back()).id;
    }
    refocus(d, s);
    return Written::ok();
}

// WL-DOC-06, WL-DOC-08 -- agents/workshop/document.md
Handled place(WorkshopDoc& d, const ui::Scene& scene, std::int64_t id, std::int64_t gx,
              std::int64_t gy) {
    const ui::Element* e = doc::find(d, id);
    if (e == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    Handled done;
    if (gx < doc::kFirstCell) {
        gx = doc::kFirstCell;
        done.boundary = kAtWorkspaceStart;
    }
    if (gy < doc::kFirstCell) {
        gy = doc::kFirstCell;
        done.boundary = kAtWorkspaceStart;
    }
    const ui::Rect frame = ui::frame_in(scene, *e);
    done.written = doc::move(d, id, detail::minus(gx, frame.x), detail::minus(gy, frame.y));
    return done;
}

// WL-DOC-06 -- agents/workshop/document.md
Handled nudge(WorkshopDoc& d, Session& s, std::int64_t ddx, std::int64_t ddy) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, s.selected);
    if (placed == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    return place(d, scene, s.selected, detail::step(placed->rect.x, ddx),
                 detail::step(placed->rect.y, ddy));
}

// ---- The size a hand asked for, as an authored extent ----------------------------------

// WL-DOC-07 -- agents/workshop/document.md
ui::Extent extent_from_drag(const ui::Extent& current, std::int64_t want,
                            std::int64_t span, std::string& boundary) {
    if (current.mode == ui::kExtentPercent) {
        const std::int64_t least = ui::resolve_extent(ui::Extent{ui::kExtentPercent, 1}, span);
        const std::int64_t most = ui::resolve_extent(ui::Extent{ui::kExtentPercent, 100}, span);
        if (want < least) {
            want = least;
            boundary = kAtSmallest;
        } else if (want > most) {
            want = most;
            // The wall a share meets at the far end is not the workspace being a
            // wall -- placement has no such limit and a cells extent has none
            // either. It is the vocabulary: a share OF something cannot be more
            // than the whole of it, so 100% is where this mode stops.
            boundary = kAtWholeContext;
        }
        if (ui::resolve_extent(current, span) == want) {
            return current; // this share already says exactly that: do not re-author it
        }
        for (std::int64_t pct = 1; pct <= 100; ++pct) {
            const ui::Extent candidate{ui::kExtentPercent, pct};
            if (ui::resolve_extent(candidate, span) >= want) {
                return candidate;
            }
        }
        return ui::Extent{ui::kExtentPercent, 100};
    }
    // Cells, and anything a poke wrote that is neither: an absolute size, whose
    // limits are the document's and have nothing to do with the workspace. An
    // object may be authored WIDER than the workspace for the same reason one may
    // be positioned past its right edge -- the canvas clips, and a maker who did
    // that has not made a mistake.
    if (want < ui::kMinCells) {
        want = ui::kMinCells;
        boundary = kAtSmallest;
    } else if (want > doc::kMaxCells) {
        want = doc::kMaxCells;
        boundary = kAtLargest;
    }
    return ui::Extent{ui::kExtentCells, want};
}

// WL-DOC-07 -- agents/workshop/document.md
Handled size_to(WorkshopDoc& d, const Session& s, std::int64_t id, std::int64_t want_w,
                std::int64_t want_h) {
    const ui::Element* e = doc::find(d, id);
    if (e == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    const ui::Rect frame = ui::frame_in(workspace_scene(d, s), *e);
    Handled done;
    const ui::Extent w = extent_from_drag(e->width, want_w, frame.w, done.boundary);
    const ui::Extent h = extent_from_drag(e->height, want_h, frame.h, done.boundary);
    done.written = doc::resize(d, id, w, h);
    return done;
}

// WL-DOC-07 -- agents/workshop/document.md
Handled grow(WorkshopDoc& d, Session& s, std::int64_t dw, std::int64_t dh) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, s.selected);
    if (placed == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    return size_to(d, s, s.selected, detail::step(placed->rect.w, dw),
                   detail::step(placed->rect.h, dh));
}

// ---- The one resize affordance ---------------------------------------------------------

Handle size_handle(const WorkshopDoc& d, const Session& s) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, s.selected);
    if (placed == nullptr) {
        return Handle{};
    }
    // Asked without performing the addition: `rect.x + rect.w` is not
    // representable for every rect a poked extent can produce, and an overflow
    // here would put the grip somewhere the object is not.
    const std::int64_t back = detail::minus(0, placed->rect.x);
    const std::int64_t room = detail::minus(s.workspace_w, placed->rect.x);
    const std::int64_t up = detail::minus(0, placed->rect.y);
    const std::int64_t down = detail::minus(s.workspace_h, placed->rect.y);
    if (placed->rect.w < back || placed->rect.w >= room || placed->rect.h < up ||
        placed->rect.h >= down) {
        return Handle{};
    }
    return Handle{true, s.selected, placed->rect.x + placed->rect.w,
                  placed->rect.y + placed->rect.h};
}

// WL-DOC-09 -- agents/workshop/document.md
std::int64_t begin_drag(const WorkshopDoc& d, Session& s, std::int64_t cx,
                        std::int64_t cy) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* under = ui::hit(scene, cx, cy);
    if (under == nullptr) {
        s.drag = Drag{};
        return 0;
    }
    s.drag = Drag{true, false, under->id, detail::minus(cx, under->rect.x),
                  detail::minus(cy, under->rect.y)};
    return under->id;
}

// WL-PANE-05 -- agents/workshop/panes-and-windows.md; WL-PRESS-04 -- agents/workshop/press-chain.md
std::int64_t take_hold(WorkshopDoc& d, Session& s, std::int64_t cx, std::int64_t cy) {
    const Handle handle = size_handle(d, s);
    if (handle.shown && handle.x == cx && handle.y == cy) {
        s.drag = Drag{true, true, handle.id, 0, 0};
        return handle.id;
    }
    return begin_drag(d, s, cx, cy);
}

// WL-CTX-01 -- agents/workshop/contextual.md
std::int64_t object_at(const WorkshopDoc& d, const Session& s, std::int64_t cx,
                       std::int64_t cy) {
    // The scene must outlive the answer read from it -- `hit` returns a pointer into it
    // (`begin_drag`'s own spelling).
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* under = ui::hit(scene, cx, cy);
    return under == nullptr ? 0 : under->id;
}

// WL-DOC-09 -- agents/workshop/document.md
Handled drag_to(WorkshopDoc& d, const Session& s, std::int64_t cx, std::int64_t cy) {
    if (!s.drag.active) {
        return Handled::of(Written::no("nothing is being dragged"));
    }
    const ui::Scene scene = workspace_scene(d, s);
    if (s.drag.resizing) {
        // The object's RESOLVED left/top edge, because the pointer and the
        // handle are both in workspace cells. Reading the authored `e->x` would
        // ask for a size measured from the wrong corner the moment the object
        // had a context -- the two are the same number only at the root.
        const ui::Placed* placed = ui::placed_for(scene, s.drag.id);
        if (placed == nullptr) {
            return Handled::of(Written::no("no such object"));
        }
        return size_to(d, s, s.drag.id, detail::minus(cx, placed->rect.x),
                       detail::minus(cy, placed->rect.y));
    }
    return place(d, scene, s.drag.id, detail::minus(cx, s.drag.grab_dx),
                 detail::minus(cy, s.drag.grab_dy));
}

void end_drag(Session& s) { s.drag = Drag{}; }

// ---- Where a pointer is, in workspace cells --------------------------------------------

ProseAt prose_at(std::int64_t space, std::int64_t x, std::int64_t y,
                 std::int64_t region_x, std::int64_t region_y,
                 const surface::RegionFit& fit) noexcept {
    if (space == input::space::kPixels) {
        return ProseAt{true, surface::prose_column_of_pixel(x, region_x, fit),
                       surface::prose_row_of_pixel(y, region_y, fit)};
    }
    if (space == input::space::kCells) {
        const surface::CanvasPoint at = surface::canvas_of_terminal_cells(x, y);
        return ProseAt{true, surface::sub_px(at.x, region_x), surface::sub_px(at.y, region_y)};
    }
    return ProseAt{};
}

// WL-GEO-06 -- agents/workshop/geometry.md
std::int64_t workspace_cell_x(std::int64_t canvas_x) noexcept {
    return detail::minus(canvas_x, kWorkspaceX);
}

std::int64_t workspace_cell_y(std::int64_t canvas_y) noexcept {
    return detail::minus(canvas_y, kWorkspaceY);
}

// ---- What the OBJECTS panel can show, and what it must SAY it cannot ---------------------

// WL-EDIT-10 -- agents/workshop/editor.md
// WL-INFO-03 -- agents/workshop/info-body.md
// WL-TAB-08 -- agents/workshop/tab-run.md
ListWindow list_window(std::size_t total, std::size_t selected_at, std::size_t rows) {
    ListWindow w;
    if (total == 0 || rows == 0) {
        w.after = total; // no room at all: everything there is, is missing
        return w;
    }
    if (total <= rows) {
        w.count = total; // rule 1 -- and this is the only case a small document takes
        return w;
    }
    if (rows < 3) {
        // Too few lines to seat one object between two markers, so no window can
        // obey rules 2 and 3 together. It spends what it has on the omission,
        // because the one thing this panel may not do is drop objects quietly.
        //
        // IT WAS UNREACHABLE AT `kListRows = 5` AND IT IS REACHABLE NOW. A share of
        // one or two rows is what a short panel gives a list whose population wants more,
        // so a body of three or four prose rows lands here -- and what a maker then reads is
        // `... 20 more` where the names would be, which is the honest answer: this place
        // cannot show you an object AND tell you what it is hiding, so it tells you.
        w.after = total;
        return w;
    }
    if (selected_at >= total) {
        selected_at = 0; // nothing selected, or a selection that outlived its object
    }
    // One marker's worth of room. Both single-marker windows are this wide, and
    // both leave a non-empty count because `total > rows`.
    const std::size_t one_marker = rows - 1;
    if (selected_at < one_marker) {
        w.count = one_marker;
        w.after = total - w.count;
        return w;
    }
    const std::size_t tail = total - one_marker;
    if (selected_at >= tail) {
        w.first = tail;
        w.count = one_marker;
        w.before = tail;
        return w;
    }
    // The selection is far enough from both ends that both walls are real, so
    // both are said. `first` is the earliest run that reaches the selection,
    // which is at least 2 here (selected_at >= rows - 1 and count == rows - 2),
    // so neither subtraction can leave the number line.
    w.count = rows - 2;
    w.first = selected_at + 1 - w.count;
    w.before = w.first;
    w.after = total - w.first - w.count;
    return w;
}

// WL-INFO-03 -- agents/workshop/info-body.md
std::string omitted_text(std::size_t how_many, const char* which) {
    return "... " + std::to_string(how_many) + " " + which;
}

} // namespace zengine::workshop
