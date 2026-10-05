// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's composition: every presented pane back to front, one layer each, the bottom band as
// one published region, and the whole screen as one canvas of planes.
// Workshop law: agents/workshop/planes.md (+4 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- THE COMPOSITION: every pane back to front, the bottom band, and the screen as planes ----

// WL-FRONT-01, WL-FRONT-05, WL-FRONT-07 -- agents/workshop/planes.md
void paint_panes(surface::SurfaceCanvas& c, const Session& s, const Screen& sc) {
    const Panes& panes = s.panes;
    const std::int64_t lifted = selected_pane(panes);
    for (const std::int64_t kind : effective_pane_order(s.setup.active, panes)) {
        const OpenPane p{kind};
        const PixelRect b = bounds_of(panes, s.setup.active, p.kind, sc).rect;
        if (b.w <= 0 || b.h <= 0) {
            continue;
        }
        const std::int64_t chrome = p.kind == lifted ? kPaneChromeSelected : kPaneChrome;
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            if (p.kind == pane_kind::kLayouts) {
                // The layout run, the setup association and the workspace fact: the rectangle is
                // `bounds_of`'s and the order `effective_pane_order`'s, so a pane in front is
                // drawn over it.
                paint_layouts(layer, s, b, sc, chrome);
            } else if (is_runtime_kind(p.kind)) {
                // One generic arm for every external pane: each is presented identically, a
                // header Workshop writes and a region the provider fills.
                paint_external(layer, panes, p.kind, b, sc, s.pane_titles, chrome);
            }
        });
    }
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_pane_affordances(layer, s, sc);
    });
    // The contextual-action surface, last in the band: over everything as the band's latest
    // gesture, and it takes the band's keys first for the same reason (`keyboard_context`).
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_context(layer, s, sc);
    });
    // ...AND A PANE'S MENU, AS ITS PRESENTER SHOWED IT, in the same position for the same reason:
    // it is the surface the weaver's next keys and presses go to. At most one of the two is open.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_presented(layer, s, sc);
    });
}

surface::SurfaceTextRegion band_region(const Session& s, const Screen& sc) {
    const PixelRect b = band_bounds(sc);
    const surface::RegionFit fit = band_fit(sc);
    surface::SurfaceTextRegion band;
    band.x = b.x;
    band.y = b.y;
    band.w = b.w;
    band.h = b.h;
    const std::int64_t budget = fit.rows;
    const std::int64_t columns = fit.columns;
    if (budget <= 0 || columns <= 0) {
        return band;
    }

    const std::string notice = s.notice.empty() ? std::string() : detail::fit(s.notice, columns);
    const std::int64_t notice_role =
        s.notice_is_bad ? surface::role::kAlert : surface::role::kFill;

    // The legend takes what the notice leaves. While an external pane holds the keyboard and the
    // legend is full, the first legend row still says so: that sentence is keyboard-ownership
    // truth, not a binding list.
    const std::size_t legend_rows =
        budget >= 2 ? static_cast<std::size_t>(budget - 1) : 0;
    std::vector<std::string> legend;
    if (legend_rows > 0) {
        const KeyContext ctx = keyboard_context(s);
        const std::int64_t typing = keyboard_pane(s.panes);
        // The sentence names where an ordinary key goes, which is not always the keyboard pane:
        // under a mode the pane stays the candidate while every ordinary key is the mode's.
        // `typing_pane` is what a press's `keys_went_here` reads, so the band and the seam agree.
        const std::int64_t typed = typing_pane(s);
        const RuntimePane* typed_into =
            typed == kNoPaneKind ? nullptr : s.panes.runtime.of_kind(typed);
        // (The host's Pane Manager was the last built-in that took the keys and had a sentence of
        // its own here; the source editor had one before it. Both are panes now, named above.)
        std::string said;
        if (typed_into != nullptr && s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "typing goes to " + typed_into->name + " @" + typed_into->provider +
                   " -- press elsewhere for Workshop's keys";
        }
        if (!said.empty()) {
            if (legend_rows == 1) {
                const std::int64_t rest =
                    columns - static_cast<std::int64_t>(said.size()) - 3;
                const std::vector<std::string> pairs =
                    help_rows(s.keymap, ctx, rest, 1, typing);
                legend.push_back(detail::fit(
                    pairs.empty() ? said : said + " | " + pairs.front(), columns));
            } else {
                legend.push_back(detail::fit(said, columns));
                // THE PANE'S OWN DECLARED ROWS COME FIRST IN WHAT FOLLOWS (WL-KEY-15),
                // then the chorded survivors: `help_pairs` orders them so.
                const std::vector<std::string> pairs =
                    help_rows(s.keymap, ctx, columns, legend_rows - 1, typing);
                for (const std::string& row : pairs) {
                    legend.push_back(row);
                }
            }
        } else {
            legend = help_rows(s.keymap, ctx, columns, legend_rows, typing);
        }
    }

    const auto push = [&band](std::string text, std::int64_t role) {
        band.rows.push_back(surface::SurfaceTextRow{std::move(text), role});
    };
    if (budget >= 2) {
        push(notice, notice_role);
        for (std::string& row : legend) {
            push(std::move(row), surface::role::kMuted);
        }
    } else if (!notice.empty()) {
        // One row: the tool's own voice while it has something to say.
        push(notice, notice_role);
    } else {
        const std::vector<std::string> pairs =
            help_rows(s.keymap, keyboard_context(s), columns, 1, keyboard_pane(s.panes));
        if (!pairs.empty()) {
            push(pairs.front(), surface::role::kMuted);
        }
    }
    return band;
}

// WL-FRONT-01, WL-FRONT-07 -- agents/workshop/planes.md
// WL-ATTN-04 -- agents/workshop/attention.md
surface::SurfaceCanvas paint(const Session& s) {
    const Screen sc = screen_of(s);
    surface::SurfaceCanvas c;
    c.width = sc.w;
    c.height = sc.h;

    // NOTHING IS PAINTED BEHIND THE PANES: where no pane stands, each medium shows its own ground.
    // Every dynamic pane, Info included, each on a plane of its own in canonical front order. The
    // screen's own chrome is a later plane: a pane painted over the band would erase the notice
    // that just told the weaver what happened.
    paint_panes(c, s, sc);

    // The screen's own chrome over them, on its own plane: the bottom band, where the tool speaks.
    // A region clears its whole rectangle, so a pane authored over the band is covered by it; the
    // band occupies no pointer space. The top band is an ordinary pane (`paint_panes`), and its
    // rows stay reserved (`kTopRows`), so the workspace does not move.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        layer.texts.push_back(band_region(s, sc));
    });

    return c;
}

} // namespace zengine::workshop
