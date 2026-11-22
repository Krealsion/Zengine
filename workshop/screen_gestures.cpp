// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The screen's gestures and measures: wrapping, the band's help rows, the size a hand asked for,
// where a pointer is, and a bounded list's window.
// Workshop law: agents/workshop/document.md (+11 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {
using pane::RegionFit;

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
    return desk_gestures::saturating_step(v, by);
}

std::int64_t minus(std::int64_t a, std::int64_t b) noexcept {
    return desk_gestures::saturating_minus(a, b);
}

} // namespace detail

std::vector<std::string> help_rows(const Keymap& k, KeySpelling spelling, KeyContext ctx,
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
        // THE APPLICATION'S OWN ROWS, above every mode -- where a weaver's launches are, the
        // key list among them; the host names none of them and reads them off the keymap.
        std::string row;
        for (const AppRow& app : k.app) {
            if (app.precedence != app_precedence::kAboveModes ||
                !k.app_row_active(app, ctx, pane)) {
                continue;
            }
            const std::string pair = spell_gesture(app.gesture, spelling) + " " + app.label;
            row = row.empty() ? pair : row + " | " + pair;
        }
        if (!row.empty()) {
            out.push_back(detail::fit(row, width));
        }
        return out;
    }
    const std::vector<std::string> pairs = help_pairs(k, spelling, ctx, pane);
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

PaneWindowAxes pane_window_axes(const PaneWindowProposal& want, const PixelRect& base,
                                const Screen& sc) {
    PaneWindowAxes out = pane_window_at(base, sc);
    if (want.place_moved_x && want.x != base.x) {
        out.horizontal.position = want.x;
    }
    if (want.w != base.w) {
        out.horizontal.extent = PaneSize{pane_unit::kPixels, want.w};
    }
    if (want.place_moved_y && want.y != base.y) {
        out.vertical.position = want.y;
    }
    if (want.h != base.h) {
        out.vertical.extent = PaneSize{pane_unit::kPixels, want.h};
    }
    return out;
}

// ---- A hand's proposal, snapped ---------------------------------------------------------

PaneSnapLines pane_snap_lines(const Panes& panes, const Setup& setup, const Screen& sc,
                              std::int64_t held) {
    PaneSnapLines out;
    out.xs = {0, sc.room_w};
    out.ys = {0, sc.room_h};
    for (const OpenPane& p : panes.open) {
        if (p.kind == held) {
            continue;
        }
        const PixelRect r = room_of_canvas(bounds_of(panes, setup, p.kind, sc).rect, sc);
        if (r.empty()) {
            continue;
        }
        out.xs.push_back(r.x);
        out.xs.push_back(graphics::add_cells(r.x, r.w));
        out.ys.push_back(r.y);
        out.ys.push_back(graphics::add_cells(r.y, r.h));
    }
    return out;
}

// ---- Where a pointer is, in a region's prose -------------------------------------------

// WL-GEO-01 -- agents/workshop/geometry.md
ProseAt prose_at(std::int64_t space, std::int64_t x, std::int64_t y,
                 const RegionFit& fit) noexcept {
    // The press is read in the unit of the medium that said it: a window's pixel, a terminal's
    // cell -- the unit text set in cells was drawn from.
    CanvasPoint at;
    std::int64_t grain = graphics::kCellPx;
    if (space == input::space::kPixels) {
        at = CanvasPoint{x, y};
        grain = 1;
    } else if (space == input::space::kCells) {
        at = CanvasPoint{graphics::px_of_cells(x), graphics::px_of_cells(y)};
    } else {
        return ProseAt{};
    }
    return ProseAt{true, prose_column_of_pixel(at.x, fit, grain),
                   prose_row_of_pixel(at.y, fit, grain)};
}

} // namespace zengine::workshop
