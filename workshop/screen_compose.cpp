// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's composition: every presented pane back to front, one layer each, the bottom band as
// one published region, and the whole screen as one canvas of planes.
// Workshop law: agents/workshop/planes.md (+4 registers; agents/workshop.md routes)

#include "screen.hpp"
#include "workshop/pane_drawing.hpp" // pane_area: every pane's area, `<office>.<pane>`

#include <algorithm>
#include <iterator>
#include <utility>

namespace zengine::workshop {

// ---- THE COMPOSITION: every pane back to front, the bottom band, and the screen as planes ----

// WL-FRONT-01, WL-FRONT-05, WL-FRONT-07 -- agents/workshop/planes.md
void paint_panes(ScreenCanvas& c, const Session& s, const Screen& sc) {
    const Panes& panes = s.panes;
    const std::int64_t lifted = selected_pane(panes);
    for (const std::int64_t kind : effective_pane_order(s.setup.active, panes)) {
        const OpenPane p{kind};
        const PixelRect b = bounds_of(panes, s.setup.active, p.kind, sc).rect;
        if (b.w <= 0 || b.h <= 0) {
            continue;
        }
        const std::int64_t chrome = p.kind == lifted ? kPaneChromeSelected : kPaneChrome;
        detail::on_own_layer(c, [&](ScreenPlane& layer) {
            if (p.kind == pane_kind::kLayouts) {
                // The layout run, the setup association and the workspace fact: the rectangle is
                // `bounds_of`'s and the order `effective_pane_order`'s, so a pane in front is
                // drawn over it.
                paint_layouts(layer, s, b, sc, chrome);
            } else if (is_runtime_kind(p.kind)) {
                // One generic arm for every external pane: each is presented identically, a
                // header Workshop writes and a body the pane draws in its own area.
                paint_external(layer, panes, p.kind, b, sc, s.pane_titles, chrome);
            }
        });
    }
    detail::on_own_layer(c, [&](ScreenPlane& layer) {
        paint_pane_affordances(layer, s, sc);
    });
    detail::on_own_layer(c, [&](ScreenPlane& layer) {
        paint_selection_ring(layer, s, sc);
    });
    // The contextual-action surface, last in the band: over everything as the band's latest
    // gesture, and it takes the band's keys first for the same reason (`keyboard_context`).
    detail::on_own_layer(c, [&](ScreenPlane& layer) {
        paint_context(layer, s, sc);
    });
    // ...AND A PANE'S MENU'S FRAME, in the same position for the same reason: it is the surface the
    // weaver's next keys and presses go to. At most one of the two is open; the menu's lines are
    // its presenter's, drawn in an area of its own in a window.
    detail::on_own_layer(c, [&](ScreenPlane& layer) {
        paint_presented(layer, s, sc);
    });
}

namespace {

/// The selected pane's rectangle while command mode has the keys, or empty: what the selection's
/// ring is drawn around, on the canvas and in a window alike.
PixelRect selection_ring_rect(const Session& s, const Screen& sc) {
    if (keyboard_context(s) != KeyContext::kCommand) {
        return PixelRect{};
    }
    const std::int64_t kind = selected_pane(s.panes);
    if (kind == kNoPaneKind || !kind_takes_keyboard(kind)) {
        return PixelRect{};
    }
    return bounds_of(s.panes, s.setup.active, kind, sc).rect;
}

/// The selection's two rings around `b`, a device unit apart and just outside its edge, clipped to
/// the canvas: where an edge is finer than a cell, a mark on a corner cell would cover words.
std::vector<PixelRect> selection_rings(const PixelRect& b, const Screen& sc) {
    const std::int64_t grain = chrome_grain(sc);
    std::vector<PixelRect> out;
    for (std::int64_t ring = 1; ring <= 2; ++ring) {
        const std::int64_t x0 = (std::max)(std::int64_t{0}, b.x - ring * grain);
        const std::int64_t y0 = (std::max)(std::int64_t{0}, b.y - ring * grain);
        const std::int64_t x1 = (std::min)(sc.w, b.x + b.w + ring * grain);
        const std::int64_t y1 = (std::min)(sc.h, b.y + b.h + ring * grain);
        out.push_back(PixelRect{x0, y0, x1 - x0, y1 - y0});
    }
    return out;
}

} // namespace

// WL-FOCUS-12 -- agents/workshop/focus.md
void paint_selection_ring(ScreenPlane& layer, const Session& s, const Screen& sc) {
    const PixelRect b = selection_ring_rect(s, sc);
    if (b.empty()) {
        return;
    }
    // THE STYLE `workshop.focus.ring` NAMES: the accent's two rings, or one in the chrome's ink.
    const bool plain = s.selection_ring_plain;
    const std::int64_t role = plain ? kPaneChrome : graphics::role::kAccent;
    // WHERE AN EDGE IS FINER THAN A CELL, the rings, each as its four sides.
    const std::int64_t grain = chrome_grain(sc);
    if (grain < graphics::kCellPx) {
        std::vector<PixelRect> rings = selection_rings(b, sc);
        if (plain) {
            rings.resize(1);
        }
        for (const PixelRect& r : rings) {
            for (const PixelRect& side : {PixelRect{r.x, r.y, r.w, grain},
                                          PixelRect{r.x, r.y + r.h - grain, r.w, grain},
                                          PixelRect{r.x, r.y, grain, r.h},
                                          PixelRect{r.x + r.w - grain, r.y, grain, r.h}}) {
                layer.rects.push_back(wire_rect_of(side, role));
            }
        }
        return;
    }
    // ...WHERE TEXT IS A CELL, brackets on the frame's corner cells, which neither a role's fill nor
    // an arrangement handle draws.
    static constexpr std::pair<std::int64_t, const char*> kCorners[] = {
        {pane_edge::kTopLeft, "["},
        {pane_edge::kTopRight, "]"},
        {pane_edge::kBottomLeft, "["},
        {pane_edge::kBottomRight, "]"},
    };
    for (const auto& [edge, glyph] : kCorners) {
        const PixelRect at = pane_edge_cell(b, edge);
        layer.labels.push_back(ScreenLabel{at.x, at.y, glyph, role});
    }
}

ScreenRegion band_region(const Session& s, const Screen& sc) {
    const PixelRect b = band_bounds(sc);
    const RegionFit fit = band_fit(sc);
    ScreenRegion band;
    band.x = b.x;
    band.y = b.y;
    band.w = b.w;
    band.h = b.h;
    const std::int64_t budget = fit.rows;
    const std::int64_t columns = fit.columns;
    if (budget <= 0 || columns <= 0) {
        return band;
    }

    // The legend takes the band. While an external pane holds the keyboard and the legend is full,
    // the first legend row still says so: that sentence is keyboard-ownership truth, not a binding
    // list.
    const std::size_t legend_rows = static_cast<std::size_t>(budget);
    std::vector<std::string> legend;
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

    for (std::string& row : legend) {
        band.rows.push_back(TextRow{std::move(row), graphics::role::kMuted});
    }
    return band;
}

// WL-FRONT-01, WL-FRONT-07 -- agents/workshop/planes.md
// WL-ATTN-04 -- agents/workshop/attention.md
ScreenCanvas paint(const Session& s) {
    const Screen sc = screen_of(s);
    ScreenCanvas c;
    c.width = sc.w;
    c.height = sc.h;

    // NOTHING IS PAINTED BEHIND THE PANES: where no pane stands, each medium shows its own ground.
    // Every dynamic pane, Info included, each on a plane of its own in canonical front order. The
    // screen's own chrome is a later plane: a pane painted over the band would hide the keys the
    // weaver is about to press.
    paint_panes(c, s, sc);

    // The screen's own chrome over them, on its own plane: the bottom band, the keys' legend.
    // A region clears its whole rectangle, so a pane authored over the band is covered by it; the
    // band occupies no pointer space. The top band is an ordinary pane (`paint_panes`), and its
    // rows stay reserved (`kTopRows`), so the workspace does not move.
    detail::on_own_layer(c, [&](ScreenPlane& layer) {
        layer.texts.push_back(band_region(s, sc));
    });

    return c;
}

// ---- THE DESK IN A GRAPHICS WINDOW: each pane's own area, and Workshop's behind them ----------

std::string pane_window_area(const Panes&, const RuntimePane& row) {
    return pane_area(row.provider, row.pane);
}

bool desk_in_front(const Session& s) noexcept {
    return s.arrange.open || s.context.open || s.presented.open;
}

namespace {

/// The pixels `columns` characters stand on from `column` of a fitted region's row `row`: the
/// medium's type where it sets type, a cell a character where text is a cell.
PixelRect span_px(const RegionFit& f, std::int64_t row, std::int64_t column,
                  std::int64_t columns) {
    const std::int64_t advance = f.graphical() ? f.advance_px : graphics::kCellPx;
    const std::int64_t line = f.graphical() ? f.line_px : graphics::kCellPx;
    return PixelRect{f.view.x + f.origin_x + column * advance, f.view.y + f.origin_y + row * line,
                     columns * advance, line};
}

/// ONE REGION OF PROSE, drawn in the group `name` clipped to it: its ground unless it writes on
/// what lies beneath, each row's own ground, its rows, then its selection again in the accent role
/// and its caret as a bar, where the medium sets them.
void draw_region(graphics::Sketch& sk, const std::string& name,
                 const ScreenRegion& g, const Screen& sc) {
    if (g.w <= 0 || g.h <= 0) {
        return;
    }
    const RegionFit fit =
        fit_region(g.x, g.y, g.w, g.h, sc.text_advance_px, sc.text_line_px);
    sk.group(name, g.x, g.y, g.w, g.h);
    const auto local = [&](std::int64_t row, std::int64_t column, std::int64_t columns) {
        PixelRect at = span_px(fit, row, column, columns);
        at.x -= g.x;
        at.y -= g.y;
        return at;
    };
    if (g.ground != kGroundBeneath) {
        sk.fill_rect(name + "/ground", 0, 0, g.w, g.h, graphics::by_role(graphics::role::kGround));
    }
    for (std::size_t r = 0; r < g.rows.size(); ++r) {
        const TextRow& row = g.rows[r];
        const PixelRect at = local(static_cast<std::int64_t>(r), 0, 0);
        if (row.background != graphics::role::kNone) {
            sk.fill_rect(name + "/bg" + std::to_string(r), 0, at.y, g.w, at.h,
                         graphics::by_role(row.background));
        }
        sk.text(name + "/row" + std::to_string(r), at.x, at.y, row.text,
                graphics::by_role(row.role));
    }
    if (g.sel_begin_row != kNoSelection && g.sel_end_row != kNoSelection) {
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
                    graphics::by_role(graphics::role::kAccent));
        }
    }
    if (g.caret_row >= 0 && g.caret_row < static_cast<std::int64_t>(g.rows.size()) &&
        g.caret_col >= 0) {
        const PixelRect at = local(g.caret_row, g.caret_col, 1);
        sk.line(name + "/caret", at.x, at.y, at.x, at.y + at.h - 1,
                graphics::by_role(graphics::role::kAccent));
    }
}

/// ONE PLANE `paint` COMPOSES, drawn in the group `name`: its rects filled in their roles, its
/// labels, its regions, in that order. The rect `frame` is a backdrop a region covers but its
/// edge: drawn as a graphics window wears an edge, a ring in its role, on ground where `opaque`.
/// Each label stands on its own ground where `grounded`, a line of the window's text tall and an
/// advance of it a character: a plane over the panes covers only what it draws.
void draw_plane(graphics::Sketch& sk, const std::string& name, const ScreenPlane& layer,
                const Screen& sc, const PixelRect& frame = {}, bool opaque = true,
                bool grounded = false) {
    sk.group(name, 0, 0);
    for (std::size_t i = 0; i < layer.rects.size(); ++i) {
        const ScreenRect& r = layer.rects[i];
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
                         graphics::by_role(graphics::role::kGround));
        }
        sk.outline_rect(at, r.x, r.y, r.w, r.h, graphics::by_role(r.role));
    }
    const std::int64_t advance =
        sc.text_advance_px > 0 ? sc.text_advance_px : graphics::kCellPx;
    const std::int64_t line = sc.text_line_px > 0 ? sc.text_line_px : graphics::kCellPx;
    for (std::size_t i = 0; i < layer.labels.size(); ++i) {
        const ScreenLabel& l = layer.labels[i];
        const std::string at = name + "/label" + std::to_string(i);
        if (grounded && !l.text.empty()) {
            sk.fill_rect(at + "-ground", l.x, l.y,
                         static_cast<std::int64_t>(l.text.size()) * advance, line,
                         graphics::by_role(graphics::role::kGround));
        }
        sk.text(at, l.x, l.y, l.text, graphics::by_role(l.role));
    }
    for (std::size_t i = 0; i < layer.texts.size(); ++i) {
        draw_region(sk, name + "/text" + std::to_string(i), layer.texts[i], sc);
    }
}

/// A PART NAMED BY ITS ACTION: the characters `row` shows at those columns drawn again over them,
/// in its role, recorded with `action`.
void draw_part(WorkshopWindowDrawing& out, const std::string& group, const std::string& action,
               const RegionFit& fit, std::int64_t r, std::int64_t column,
               std::int64_t columns, const TextRow& row) {
    if (columns <= 0 || column < 0) {
        return;
    }
    std::string words = column < static_cast<std::int64_t>(row.text.size())
                            ? row.text.substr(static_cast<std::size_t>(column),
                                              static_cast<std::size_t>(columns))
                            : std::string();
    words.resize(static_cast<std::size_t>(columns), ' ');
    const PixelRect at = span_px(fit, r, column, columns);
    const std::string node = group + "/part" + std::to_string(out.sketch.records().size());
    out.sketch.text(node, at.x, at.y, words, graphics::by_role(row.role));
    out.sketch.action(node, action);
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

/// ONE PANE, `kind`, drawn in a group under `desk` as `paint_panes` paints it, over `b`: its edge,
/// its header and the words its body says, on ground; the header's presses, and the Layouts row's
/// tabs, as their parts.
void draw_pane(WorkshopWindowDrawing& out, const Session& s, std::int64_t kind,
               const PixelRect& b, const Screen& sc, const std::string& desk) {
    const std::int64_t chrome = kind == selected_pane(s.panes) ? kPaneChromeSelected : kPaneChrome;
    ScreenPlane layer;
    if (kind == pane_kind::kLayouts) {
        const std::string group = desk + "/layouts";
        paint_layouts(layer, s, b, sc, chrome);
        draw_plane(out.sketch, group, layer, sc, b);
        const std::int64_t tab_row = band_tab_row(s, sc);
        if (tab_row != kNoBandRow) {
            const ExternalBodyPlace place = layouts_body(s, sc);
            const BandStatus band = band_status(s, place);
            const TextRow row{band.text, graphics::role::kMuted};
            for (const LayoutTab& tab : band.tabs) {
                draw_part(out, group, "layout:" + std::to_string(tab.at), place.fit, tab_row,
                          tab.column, tab.columns, row);
            }
            draw_part(out, group, "layout:new", place.fit, tab_row, band.create_column,
                      band.create_columns, row);
        }
        return;
    }
    const RuntimePane* row = s.panes.runtime.of_kind(kind);
    if (!is_runtime_kind(kind) || row == nullptr) {
        return;
    }
    const std::string group = desk + "/pane" + std::to_string(kind);
    paint_external(layer, s.panes, kind, b, sc, s.pane_titles, chrome);
    draw_plane(out.sketch, group, layer, sc, b);
    const ExternalBodyPlace body =
        external_body_place(b, sc, external_title_rows(s.panes, kind, s.pane_titles));
    const ExternalPane* pane = s.panes.external_pane(kind);
    const bool titled = body.header_rows > 0 && !layer.texts.empty() &&
                        layer.texts.back().x == body.region_x &&
                        layer.texts.back().y == body.region_y && !layer.texts.back().rows.empty() &&
                        (pane == nullptr || !pane->title_waits);
    if (!titled) {
        return;
    }
    // THE HEADER'S PRESSES: the title puts the pane in front, and the `?` and the gear, where the
    // row wears them, are pressed as their own (`chrome_mark_at`).
    const TextRow& header = layer.texts.back().rows.front();
    const bool marks = pane != nullptr && pane->manual && body.columns >= kChromeMarksMinColumns;
    const std::int64_t marks_cols =
        static_cast<std::int64_t>(std::char_traits<char>::length(kChromeMarks));
    draw_part(out, group, "focus:" + std::to_string(kind), body.fit, 0, 0,
              marks ? body.columns - marks_cols - 1 : body.columns, header);
    if (marks) {
        draw_part(out, group, "settings:" + std::to_string(kind), body.fit, 0,
                  body.columns - marks_cols, 1, header);
        draw_part(out, group, "manual:" + std::to_string(kind), body.fit, 0, body.columns - 1, 1,
                  header);
    }
}

} // namespace

std::string pane_frame_area(const Panes& panes, std::int64_t kind) {
    const RuntimePane* row = is_runtime_kind(kind) ? panes.runtime.of_kind(kind) : nullptr;
    const PaneRef ref = row != nullptr ? PaneRef{row->provider, row->pane} : pane_ref_of(kind);
    return std::string(kWorkshopProvider) + ".frame." + pane_area(ref.provider, ref.pane);
}

WorkshopWindowDrawing workshop_window_drawing(const Session& s) {
    const Screen sc = screen_of(s);
    const std::string desk = "desk";
    WorkshopWindowDrawing out;
    out.sketch.group(desk, 0, 0);
    // THE ARRANGEMENT'S RINGS, THEN A MENU, THEN THE BAND, each over everything before it.
    {
        ScreenPlane layer;
        paint_pane_affordances(layer, s, sc);
        draw_plane(out.sketch, desk + "/rings", layer, sc);
    }
    // ...AND THE KEYBOARD'S SELECTION, where command mode has one: a window's edges are a device
    // unit wide, so it wears the rings.
    if (const PixelRect b = selection_ring_rect(s, sc); !b.empty()) {
        const std::string group = desk + "/selection";
        out.sketch.group(group, 0, 0);
        std::vector<PixelRect> rings = selection_rings(b, sc);
        if (s.selection_ring_plain) {
            rings.resize(1); // `workshop.focus.ring` plain: one ring, in the chrome's ink
        }
        for (std::size_t i = 0; i < rings.size(); ++i) {
            out.sketch.outline_rect(group + "/ring" + std::to_string(i), rings[i].x, rings[i].y,
                                    rings[i].w, rings[i].h,
                                    graphics::by_role(s.selection_ring_plain
                                                          ? kPaneChrome
                                                          : graphics::role::kAccent));
        }
    }
    if (s.context.open) {
        ScreenPlane layer;
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
        // A PANE'S MENU: its frame here, its lines in the presenter's own area inside it.
        ScreenPlane layer;
        paint_presented(layer, s, sc);
        draw_plane(out.sketch, desk + "/presented", layer, sc, presented_bounds(s, sc));
    }
    {
        ScreenPlane layer;
        layer.texts.push_back(band_region(s, sc));
        draw_plane(out.sketch, desk + "/band", layer, sc);
    }
    out.sketch = rooted(out.sketch, desk);
    return out;
}

WorkshopFrameDrawing workshop_frame_drawing(const Session& s, std::int64_t kind) {
    const Screen sc = screen_of(s);
    WorkshopFrameDrawing out;
    const PaneBounds where = bounds_of(s.panes, s.setup.active, kind, sc);
    if (!where.open || where.rect.w <= 0 || where.rect.h <= 0) {
        return out;
    }
    // DRAWN AS THE DESK DRAWS IT, in the desk's pixels, under a group that carries them to the
    // area's own corner.
    out.at = where.rect;
    const std::string frame = "frame";
    WorkshopWindowDrawing drawn;
    drawn.sketch.group(frame, -out.at.x, -out.at.y);
    draw_pane(drawn, s, kind, out.at, sc, frame);
    out.sketch = rooted(drawn.sketch, frame);
    return out;
}

graphics::Sketch workshop_overlay_drawing(const Session& s,
                                          const std::vector<ScreenPlane>& overlays) {
    const Screen sc = screen_of(s);
    const std::string marks = "marks";
    graphics::Sketch out;
    out.group(marks, 0, 0);
    for (std::size_t i = 0; i < overlays.size(); ++i) {
        if (!overlays[i].rects.empty() || !overlays[i].labels.empty() ||
            !overlays[i].texts.empty()) {
            draw_plane(out, marks + "/plane" + std::to_string(i), overlays[i], sc, {}, true,
                       true);
        }
    }
    return rooted(out, marks);
}

} // namespace zengine::workshop
