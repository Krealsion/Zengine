// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's composition: every presented pane back to front, one layer each, the bottom band as
// one published region, and the whole screen as one canvas of planes.
// Workshop law: agents/workshop/planes.md (+4 registers; agents/workshop.md routes)

#include "screen.hpp"

#include <algorithm>
#include <iterator>

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
                    help_rows(s.keymap, s.band_keys, ctx, rest, 1, typing);
                legend.push_back(detail::fit(
                    pairs.empty() ? said : said + " | " + pairs.front(), columns));
            } else {
                legend.push_back(detail::fit(said, columns));
                // THE PANE'S OWN DECLARED ROWS COME FIRST IN WHAT FOLLOWS (WL-KEY-15),
                // then the chorded survivors: `help_pairs` orders them so.
                const std::vector<std::string> pairs =
                    help_rows(s.keymap, s.band_keys, ctx, columns, legend_rows - 1, typing);
                for (const std::string& row : pairs) {
                    legend.push_back(row);
                }
            }
        } else {
            legend = help_rows(s.keymap, s.band_keys, ctx, columns, legend_rows, typing);
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
            help_rows(s.keymap, s.band_keys, keyboard_context(s), columns, 1,
                      keyboard_pane(s.panes));
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

// ---- THE DESK IN A GRAPHICS WINDOW: each pane's own area, and Workshop's behind them ----------

std::string pane_window_area(const Panes& panes, const RuntimePane& row) {
    // The offices that seat each of their panes in an area of its own, whatever they offer now.
    static constexpr const char* kSeatingOffices[] = {"zengine.desktop", "zengine.introspection",
                                                     "zengine.info", "zengine.inventory"};
    bool seats = std::any_of(std::begin(kSeatingOffices), std::end(kSeatingOffices),
                             [&](const char* office) { return row.provider == office; });
    seats = seats || std::count_if(panes.runtime.entries.begin(), panes.runtime.entries.end(),
                                   [&](const RuntimePane& r) {
                                       return r.provider == row.provider;
                                   }) > 1;
    return seats ? row.provider + "." + row.pane : row.provider;
}

bool desk_in_front(const Session& s) noexcept {
    return s.arrange.open || s.context.open || s.presented.open;
}

namespace {

/// The pixels `columns` characters stand on from `column` of a fitted region's row `row`: the
/// medium's type where it sets type, a cell a character where text is a cell.
PixelRect span_px(const surface::RegionFit& f, std::int64_t row, std::int64_t column,
                  std::int64_t columns) {
    const std::int64_t advance = f.graphical() ? f.advance_px : surface::kCanvasCellPx;
    const std::int64_t line = f.graphical() ? f.line_px : surface::kCanvasCellPx;
    return PixelRect{f.view.x + f.origin_x + column * advance, f.view.y + f.origin_y + row * line,
                     columns * advance, line};
}

/// ONE REGION OF PROSE, drawn in the group `name` clipped to it: its ground unless it writes on
/// what lies beneath, each row's own ground, its rows, then its selection again in the accent role
/// and its caret as a bar, where the medium sets them.
void draw_region(graphics::Sketch& sk, const std::string& name,
                 const surface::SurfaceTextRegion& g, const Screen& sc) {
    if (g.w <= 0 || g.h <= 0) {
        return;
    }
    const surface::RegionFit fit =
        surface::fit_region(g.x, g.y, g.w, g.h, sc.text_advance_px, sc.text_line_px);
    sk.group(name, g.x, g.y, g.w, g.h);
    const auto local = [&](std::int64_t row, std::int64_t column, std::int64_t columns) {
        PixelRect at = span_px(fit, row, column, columns);
        at.x -= g.x;
        at.y -= g.y;
        return at;
    };
    if (g.ground != surface::kGroundBeneath) {
        sk.fill_rect(name + "/ground", 0, 0, g.w, g.h, graphics::by_role(surface::role::kGround));
    }
    for (std::size_t r = 0; r < g.rows.size(); ++r) {
        const surface::SurfaceTextRow& row = g.rows[r];
        const PixelRect at = local(static_cast<std::int64_t>(r), 0, 0);
        if (row.background != surface::role::kNone) {
            sk.fill_rect(name + "/bg" + std::to_string(r), 0, at.y, g.w, at.h,
                         graphics::by_role(row.background));
        }
        sk.text(name + "/row" + std::to_string(r), at.x, at.y, row.text,
                graphics::by_role(row.role));
    }
    if (g.sel_begin_row != surface::kNoSelection && g.sel_end_row != surface::kNoSelection) {
        for (std::int64_t r = (std::max)(std::int64_t{0}, g.sel_begin_row);
             r <= g.sel_end_row && r < static_cast<std::int64_t>(g.rows.size()); ++r) {
            const std::string& text = g.rows[static_cast<std::size_t>(r)].text;
            const std::int64_t first = r == g.sel_begin_row ? g.sel_begin_col : 0;
            const std::int64_t last = (std::min)(static_cast<std::int64_t>(text.size()),
                                                 r == g.sel_end_row ? g.sel_end_col
                                                                    : std::int64_t{1} << 30);
            if (first < 0 || last <= first) {
                continue;
            }
            const PixelRect at = local(r, first, last - first);
            sk.text(name + "/sel" + std::to_string(r), at.x, at.y,
                    text.substr(static_cast<std::size_t>(first),
                                static_cast<std::size_t>(last - first)),
                    graphics::by_role(surface::role::kAccent));
        }
    }
    if (g.caret_row >= 0 && g.caret_row < static_cast<std::int64_t>(g.rows.size()) &&
        g.caret_col >= 0) {
        const PixelRect at = local(g.caret_row, g.caret_col, 1);
        sk.line(name + "/caret", at.x, at.y, at.x, at.y + at.h - 1,
                graphics::by_role(surface::role::kAccent));
    }
}

/// ONE PLANE `paint` COMPOSES, drawn in the group `name`: its rects filled in their roles, its
/// labels, its regions, in that order. The rect `frame` is a backdrop a region covers but its
/// edge: drawn as a graphics window wears an edge, a ring in its role, on ground where `opaque`.
void draw_plane(graphics::Sketch& sk, const std::string& name, const surface::SurfaceLayer& layer,
                const Screen& sc, const PixelRect& frame = {}, bool opaque = true) {
    sk.group(name, 0, 0);
    for (std::size_t i = 0; i < layer.rects.size(); ++i) {
        const surface::SurfaceRect& r = layer.rects[i];
        const std::string at = name + "/rect" + std::to_string(i);
        if (r.w <= 0 || r.h <= 0) {
            continue;
        }
        if (PixelRect{r.x, r.y, r.w, r.h} != frame) {
            sk.fill_rect(at, r.x, r.y, r.w, r.h, graphics::by_role(r.role));
            continue;
        }
        if (opaque) {
            sk.fill_rect(at + "-ground", r.x, r.y, r.w, r.h,
                         graphics::by_role(surface::role::kGround));
        }
        sk.outline_rect(at, r.x, r.y, r.w, r.h, graphics::by_role(r.role));
    }
    for (std::size_t i = 0; i < layer.labels.size(); ++i) {
        const surface::SurfaceLabel& l = layer.labels[i];
        sk.text(name + "/label" + std::to_string(i), l.x, l.y, l.text,
                graphics::by_role(l.role));
    }
    for (std::size_t i = 0; i < layer.texts.size(); ++i) {
        draw_region(sk, name + "/text" + std::to_string(i), layer.texts[i], sc);
    }
}

/// A PART A PRESS ASKS FOR BY NAME: the characters `row` shows at those columns drawn again over
/// them, in its role, recorded with `action`, and where they stand kept in `out`.
void draw_part(WorkshopWindowDrawing& out, const std::string& group, const std::string& action,
               const surface::RegionFit& fit, std::int64_t r, std::int64_t column,
               std::int64_t columns, const surface::SurfaceTextRow& row) {
    if (columns <= 0 || column < 0) {
        return;
    }
    std::string words = column < static_cast<std::int64_t>(row.text.size())
                            ? row.text.substr(static_cast<std::size_t>(column),
                                              static_cast<std::size_t>(columns))
                            : std::string();
    words.resize(static_cast<std::size_t>(columns), ' ');
    const PixelRect at = span_px(fit, r, column, columns);
    const std::string node = group + "/part" + std::to_string(out.parts.size());
    out.sketch.text(node, at.x, at.y, words, graphics::by_role(row.role));
    out.sketch.action(node, action);
    out.parts[action] = at;
}

/// A drawing's root renamed for its structure: the paths in their order, hashed. A node a change
/// adds goes after its siblings in the window, so a drawing that gains, loses or reorders a node
/// is a new root, the old one removed whole, and a change in place keeps the root.
graphics::Sketch rooted(const graphics::Sketch& built, const std::string& stem) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const graphics::Node& n : built.nodes()) {
        for (const char ch : n.path + "\n") {
            hash = (hash ^ static_cast<unsigned char>(ch)) * 1099511628211ULL;
        }
    }
    static constexpr char kHex[] = "0123456789abcdef";
    std::string root = stem + "-";
    for (int shift = 60; shift >= 0; shift -= 4) {
        root += kHex[(hash >> shift) & 0xF];
    }
    const auto renamed = [&](const std::string& path) {
        return path == stem ? root : root + path.substr(stem.size());
    };
    graphics::Sketch out;
    for (graphics::Node n : built.nodes()) {
        n.path = renamed(n.path);
        out.put(std::move(n));
    }
    for (graphics::ElementRecord r : built.records()) {
        r.path = renamed(r.path);
        out.say(std::move(r));
    }
    return out;
}

} // namespace

WorkshopWindowDrawing workshop_window_drawing(const Session& s,
                                              const std::set<std::int64_t>& placed,
                                              const std::vector<surface::SurfaceLayer>& overlays) {
    const Screen sc = screen_of(s);
    // AN OVERLAY STANDS OVER THE PANES' OWN AREAS, so the desk comes in front with it.
    const auto drawn = [](const surface::SurfaceLayer& l) {
        return !l.rects.empty() || !l.labels.empty() || !l.texts.empty();
    };
    const bool front = desk_in_front(s) || std::any_of(overlays.begin(), overlays.end(), drawn);
    const std::string desk = "desk";
    WorkshopWindowDrawing out;
    out.sketch.group(desk, 0, 0);
    // EVERY PANE BACK TO FRONT, as `paint_panes` paints them: a pane whose office draws its body in
    // an area of its own wears its edge, its header and what stands in for a body not yet heard,
    // over ground while the desk stands behind the areas, so a pane in front covers the one behind.
    const std::int64_t lifted = selected_pane(s.panes);
    for (const std::int64_t kind : effective_pane_order(s.setup.active, s.panes)) {
        const PixelRect b = bounds_of(s.panes, s.setup.active, kind, sc).rect;
        if (b.w <= 0 || b.h <= 0) {
            continue;
        }
        const std::int64_t chrome = kind == lifted ? kPaneChromeSelected : kPaneChrome;
        surface::SurfaceLayer layer;
        if (kind == pane_kind::kLayouts) {
            const std::string group = desk + "/layouts";
            paint_layouts(layer, s, b, sc, chrome);
            draw_plane(out.sketch, group, layer, sc, b);
            const std::int64_t tab_row = band_tab_row(s, sc);
            if (tab_row != kNoBandRow) {
                const ExternalBodyPlace place = layouts_body(s, sc);
                const BandStatus band = band_status(s, place);
                const surface::SurfaceTextRow row{band.text, surface::role::kMuted};
                for (const LayoutTab& tab : band.tabs) {
                    draw_part(out, group, "layout:" + std::to_string(tab.at), place.fit, tab_row,
                              tab.column, tab.columns, row);
                }
                draw_part(out, group, "layout:new", place.fit, tab_row, band.create_column,
                          band.create_columns, row);
            }
            continue;
        }
        const RuntimePane* row = s.panes.runtime.of_kind(kind);
        if (!is_runtime_kind(kind) || row == nullptr) {
            continue;
        }
        const bool own_area = placed.count(kind) != 0;
        const std::string group = desk + "/pane" + std::to_string(kind);
        paint_external(layer, s.panes, kind, b, sc, s.pane_titles, chrome, !own_area);
        draw_plane(out.sketch, group, layer, sc, b, !own_area || !front);
        const ExternalBodyPlace body =
            external_body_place(b, sc, external_title_rows(s.panes, kind, s.pane_titles));
        const bool titled = body.header_rows > 0 && !layer.texts.empty() &&
                            layer.texts.back().x == body.region_x &&
                            layer.texts.back().y == body.region_y &&
                            !layer.texts.back().rows.empty();
        if (!titled) {
            continue;
        }
        // THE HEADER'S PRESSES: the title puts the pane in front, and the `?` and the gear, where
        // the row wears them, are pressed as their own (`chrome_mark_at`).
        const surface::SurfaceTextRow& header = layer.texts.back().rows.front();
        const ExternalPane* pane = s.panes.external_pane(kind);
        const bool marks = pane != nullptr && pane->manual &&
                           body.columns >= kChromeMarksMinColumns;
        const std::int64_t marks_cols =
            static_cast<std::int64_t>(std::char_traits<char>::length(kChromeMarks));
        draw_part(out, group, "focus:" + std::to_string(kind), body.fit, 0, 0,
                  marks ? body.columns - marks_cols - 1 : body.columns, header);
        if (marks) {
            draw_part(out, group, "settings:" + std::to_string(kind), body.fit, 0,
                      body.columns - marks_cols, 1, header);
            draw_part(out, group, "manual:" + std::to_string(kind), body.fit, 0,
                      body.columns - 1, 1, header);
        }
    }
    // THE ARRANGEMENT'S RINGS, THEN A MENU, THEN THE BAND, THEN THE OVERLAYS, each over everything
    // before it.
    {
        surface::SurfaceLayer layer;
        paint_pane_affordances(layer, s, sc);
        draw_plane(out.sketch, desk + "/rings", layer, sc);
    }
    if (s.context.open) {
        surface::SurfaceLayer layer;
        paint_context(layer, s, sc);
        const std::string group = desk + "/context";
        draw_plane(out.sketch, group, layer, sc, context_bounds(s, sc));
        const ProsePlace place = prose_place(context_bounds(s, sc), sc);
        const std::vector<std::string> names = context_line_names(s, sc);
        for (std::size_t i = 0; i < names.size() && !layer.texts.empty() &&
                                i < layer.texts.back().rows.size();
             ++i) {
            if (!names[i].empty()) {
                draw_part(out, group, "menu:" + std::to_string(i), place.fit,
                          static_cast<std::int64_t>(i), 0, place.columns,
                          layer.texts.back().rows[i]);
            }
        }
    }
    if (s.presented.open) {
        surface::SurfaceLayer layer;
        paint_presented(layer, s, sc);
        const std::string group = desk + "/presented";
        const PixelRect b = presented_bounds(s, sc);
        draw_plane(out.sketch, group, layer, sc, b);
        const ProsePlace place = prose_place(b, sc);
        const std::size_t lines = layer.texts.empty() ? 0 : layer.texts.back().rows.size();
        for (std::size_t i = 0; i < lines; ++i) {
            draw_part(out, group, "menu:" + std::to_string(i), place.fit,
                      static_cast<std::int64_t>(i), 0, place.columns, layer.texts.back().rows[i]);
        }
    }
    {
        surface::SurfaceLayer layer;
        layer.texts.push_back(band_region(s, sc));
        draw_plane(out.sketch, desk + "/band", layer, sc);
    }
    for (std::size_t i = 0; i < overlays.size(); ++i) {
        if (drawn(overlays[i])) {
            draw_plane(out.sketch, desk + "/overlay" + std::to_string(i), overlays[i], sc);
        }
    }
    out.sketch = rooted(out.sketch, desk);
    return out;
}

} // namespace zengine::workshop
