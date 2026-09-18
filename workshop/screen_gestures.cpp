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

namespace detail {

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
                                   std::int64_t width, std::size_t rows, std::int64_t pane) {
    std::vector<std::string> out;
    if (rows == 0) {
        return out;
    }
    const std::int64_t legend = k.resolved_legend();
    if (legend == legend_mode::kHidden) {
        return out;
    }
    if (legend == legend_mode::kCompact) {
        // THE APPLICATION'S OWN ROWS, above every mode -- where a maker's launches are, the
        // key list among them; the host names none of them and reads them off the keymap.
        std::string row;
        for (const AppRow& app : k.app) {
            if (app.precedence != app_precedence::kAboveModes ||
                !k.app_row_active(app, ctx, pane)) {
                continue;
            }
            const std::string pair = gesture_text(app.gesture) + " " + app.label;
            row = row.empty() ? pair : row + " | " + pair;
        }
        if (!row.empty()) {
            out.push_back(detail::fit(row, width));
        }
        return out;
    }
    const std::vector<std::string> pairs = help_pairs(k, ctx, pane);
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

// ⭐ THE OBJECT CANVAS'S HANDS WERE HERE -- create and delete, `place`, `nudge`, `size_to`,
// `grow`, the size handle, take-hold and `drag_to` -- and retired with the canvas.

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

// WL-PTR-10 -- agents/workshop/pointer.md
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
