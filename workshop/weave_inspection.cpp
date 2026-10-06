// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s inspection seam: a pane an inspector names, the picture of its rows, and one
// write through the rows' owner.
// Workshop law: agents/workshop/info-body.md (agents/workshop.md routes)

#include "weave.hpp"
#include "screen_canvas.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace zengine::workshop {

// A PANE'S VISIBLE TEXT BODY, OR WHY THERE IS NONE: the one validation both the row reading and
// the point query spend, so a point is never offered for a pane its reading would refuse.
std::string WorkshopWeave::visible_text_body(const std::string& provider, const std::string& pane_key,
                                             VisibleBody& out) const {
    return visible_body(provider, pane_key, out, false);
}

// ...AND THE SAME FOR EITHER BODY: a canvas pane's picture too, where `canvas_too` asks for it.
std::string WorkshopWeave::visible_body(const std::string& provider, const std::string& pane_key,
                                        VisibleBody& out, bool canvas_too) const {
    const auto* pane = session_.panes.runtime.find(provider, pane_key);
    const auto sc = screen_of(session_);
    if (!pane || !session_.panes.has(pane->kind) || session_.arrange.open ||
        session_.context.open || session_.presented.open) {
        return "pane view unavailable: closed, unknown or covered by an interaction";
    }
    const auto* content = session_.panes.external_pane(pane->kind);
    const bool canvas = content && content->canvas.heard;
    if (canvas && !canvas_too) {
        return "pane view unavailable: the pane draws a picture, not text rows";
    }
    if (!content || !content->heard || content->awaiting || content->canvas.preview ||
        content->picture != content->stamp.aimed) {
        return canvas ? "pane view unavailable: no settled picture"
                      : "pane view unavailable: no settled text picture";
    }
    const auto bounds = bounds_of(session_.panes, session_.setup.active, pane->kind, sc);
    if (!bounds.open || bounds.rect.y < sc.room_y ||
        bounds.rect.y + bounds.rect.h > sc.notice_y) {
        return "pane view unavailable: pane extends outside the visible workspace";
    }
    bool above = false;
    for (const auto kind : effective_pane_order(session_.setup.active, session_.panes)) {
        if (kind == pane->kind) { above = true; continue; }
        if (!above) continue;
        const auto other = bounds_of(session_.panes, session_.setup.active, kind, sc);
        if (other.open && other.rect.x < bounds.rect.x + bounds.rect.w &&
            other.rect.x + other.rect.w > bounds.rect.x &&
            other.rect.y < bounds.rect.y + bounds.rect.h &&
            other.rect.y + other.rect.h > bounds.rect.y) {
            return "pane view unavailable: another pane overlaps it";
        }
    }
    out.kind = pane->kind;
    out.content = content;
    const std::int64_t titles = external_title_rows(session_.panes, pane->kind, session_.pane_titles);
    out.body = external_body_place(bounds.rect, sc, titles);
    if (!out.body.present) return "pane has no visible body";
    out.canvas = canvas;
    if (canvas) {
        // The body the painter draws the picture in, which the grant's room must still be.
        out.canvas_body = canvas_body_place(bounds.rect, sc, titles);
        const auto& c = content->canvas;
        if (out.canvas_body.empty() || out.canvas_body.x != c.x || out.canvas_body.y != c.y ||
            out.canvas_body.w != c.width || out.canvas_body.h != c.height) {
            return "pane view unavailable: no settled picture";
        }
    }
    return {};
}

// THE CENTER OF ONE CELL OF A BODY ROW, in the space input for this medium is read in, and what
// the press measurer resolves it to -- the inverse it measures.
ExternalPressAt WorkshopWeave::cell_point(const VisibleBody& visible, std::int64_t row,
                                          std::int64_t cell, std::int64_t& x, std::int64_t& y,
                                          std::int64_t& space) const {
    const auto sc = screen_of(session_);
    const auto& body = visible.body;
    if (sc.text_advance_px > 0 && sc.text_line_px > 0) {
        space = input::space::kPixels;
        if (body.fit.graphical()) {
            x = body.fit.view.x + body.fit.origin_x + cell*body.fit.advance_px + body.fit.advance_px/2;
            y = body.fit.view.y + body.fit.origin_y + (row+body.header_rows)*body.fit.line_px + body.fit.line_px/2;
        } else {
            x = body.region_x + surface::px_of_cells(cell) + surface::kCanvasCellPx/2;
            y = body.region_y + surface::px_of_cells(row+body.header_rows) + surface::kCanvasCellPx/2;
        }
    } else {
        space = input::space::kCells;
        x = surface::cell_of_pixel(body.region_x)+cell;
        y = surface::cell_of_pixel(body.region_y)+row+body.header_rows+surface::kTuiCanvasTopRow;
    }
    return external_press_at(session_.panes, session_.setup.active, sc, visible.kind,
                             session_.pane_titles, space, x, y);
}

// THE CENTER OF THE CELL SHOWING ONE CHARACTER -- past a caret a fit in cells draws as a glyph of
// its own -- checked by resolving it to that row and column.
bool WorkshopWeave::cell_center(const VisibleBody& visible, std::int64_t row, std::int64_t column,
                                std::int64_t& x, std::int64_t& y, std::int64_t& space) const {
    const std::int64_t cell =
        drawn_column(column, external_caret_glyph(visible.content, visible.body.fit, row));
    const auto hit = cell_point(visible, row, cell, x, y, space);
    return hit.named && hit.row == row && hit.column == column;
}

void WorkshopWeave::on(const PaneViewRequested& asked, loom::Mail& mail) {
    VisibleBody visible;
    if (const auto why = visible_text_body(asked.provider, asked.pane, visible); !why.empty()) {
        (void)mail.answer(loom::Refused{why}); return;
    }
    const auto& body = visible.body;
    const auto* content = visible.content;
    PaneView reply{asked.provider, asked.pane, content->stamp.aimed, {}};
    for (std::int64_t row = 0; row < body.rows && row < static_cast<std::int64_t>(content->shown.size()); ++row) {
        PaneViewRow out;
        out.row = row;
        // The row's third cell, or its last in a narrower body: a cell of the body whatever a
        // caret's glyph stands before it.
        const auto hit = cell_point(visible, row, std::min<std::int64_t>(2, body.columns - 1),
                                    out.x, out.y, out.space);
        if (!hit.named || hit.row != row) {
            (void)mail.answer(loom::Refused{"pane has no addressable row center"}); return;
        }
        out.text = detail::fit(content->shown[static_cast<std::size_t>(row)].text, body.columns);
        reply.rows.push_back(std::move(out));
    }
    (void)mail.answer(reply);
}

void WorkshopWeave::on(const PanePointRequested& asked, loom::Mail& mail) {
    VisibleBody visible;
    if (const auto why = visible_text_body(asked.provider, asked.pane, visible); !why.empty()) {
        (void)mail.answer(loom::Refused{why}); return;
    }
    if (asked.picture != visible.content->stamp.aimed) {
        (void)mail.answer(loom::Refused{"pane point unavailable: the pane's picture moved; read it again"}); return;
    }
    if (asked.row < 0 || asked.column < 0 || asked.row >= visible.body.rows || asked.column >= visible.body.columns ||
        asked.row >= static_cast<std::int64_t>(visible.content->shown.size())) {
        (void)mail.answer(loom::Refused{"pane point unavailable: outside the pane's visible text"}); return;
    }
    PanePoint reply{asked.provider, asked.pane, asked.picture, asked.row, asked.column, 0, 0, 0};
    if (!cell_center(visible, asked.row, asked.column, reply.x, reply.y, reply.space)) {
        (void)mail.answer(loom::Refused{"pane point unavailable: that cell is not addressable"}); return;
    }
    (void)mail.answer(reply);
}

// ---- THE DESK, AS WORKSHOP HOLDS IT -----------------------------------------------------------

namespace {

/// THE SPACE A POINT ON THIS SCREEN IS READ IN: a window's pixels where the medium sets type, a
/// terminal's cells where text is a cell -- the space `cell_center` answers in.
std::int64_t input_space_of(const Screen& sc) {
    return sc.text_advance_px > 0 && sc.text_line_px > 0 ? input::space::kPixels
                                                         : input::space::kCells;
}

DeskRect desk_rect(const PixelRect& r) {
    return r.w > 0 && r.h > 0 ? DeskRect{r.x, r.y, r.w, r.h} : DeskRect{};
}

/// WHERE A FIT DRAWS ITS GLYPHS: the first glyph's corner, and one glyph's advance and line, in
/// canvas pixels -- the face's where the fit sets type, one cell each where text is a cell.
struct GlyphGrid {
    std::int64_t x = 0, y = 0;
    std::int64_t advance = surface::kCanvasCellPx, line = surface::kCanvasCellPx;
};

GlyphGrid glyph_grid(const surface::RegionFit& fit) {
    if (fit.graphical()) {
        return GlyphGrid{fit.view.x + fit.origin_x, fit.view.y + fit.origin_y, fit.advance_px,
                         fit.line_px};
    }
    return GlyphGrid{fit.view.x, fit.view.y, surface::kCanvasCellPx, surface::kCanvasCellPx};
}

/// The centre of one glyph, in the input space: a window's pixel, or the terminal cell a medium
/// whose unit is the cell draws it on (floored, as it draws) on the console row it reads.
void glyph_point(const GlyphGrid& g, std::int64_t row, std::int64_t column, std::int64_t space,
                 std::int64_t& x, std::int64_t& y) {
    if (space == input::space::kPixels) {
        x = g.x + column * g.advance + g.advance / 2;
        y = g.y + row * g.line + g.line / 2;
        return;
    }
    x = surface::cell_of_pixel(g.x + column * g.advance);
    y = surface::cell_of_pixel(g.y + row * g.line) + surface::kTuiCanvasTopRow;
}

/// ONE RUN OF GLYPHS AS A WORD: `text` drawn from `column` of `row` over its bytes' cells of the
/// grid and the caret glyph a terminal draws into it at `caret`, and its middle byte's centre as
/// the place a press names it -- or, with no byte to name, its first cell, the caret's own.
PaneWord word_on(const GlyphGrid& g, std::int64_t row, std::int64_t column, std::string text,
                 std::int64_t caret, std::int64_t space) {
    PaneWord w;
    const std::int64_t bytes = static_cast<std::int64_t>(text.size());
    const std::int64_t n = (std::max<std::int64_t>)(1, bytes + (caret >= 0 ? 1 : 0));
    w.place = DeskRect{g.x + column * g.advance, g.y + row * g.line, n * g.advance, g.line};
    glyph_point(g, row, column + (bytes > 0 ? drawn_column((bytes - 1) / 2, caret) : 0), space,
                w.x, w.y);
    w.space = space;
    w.text = std::move(text);
    return w;
}

std::string without_trailing_blanks(std::string text) {
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return text;
}

/// Whether `inner` lies inside `outer` and is not all of it: a part a press inside it names first.
bool inside_part(const PaneRowPart& inner, const PaneRowPart& outer) {
    return inner.row == outer.row && inner.column >= outer.column && inner.columns < outer.columns &&
           inner.column + inner.columns <= outer.column + outer.columns;
}

bool inside_part(const PaneCanvasPart& inner, const PaneCanvasPart& outer) {
    return inner.x >= outer.x && inner.y >= outer.y &&
           surface::add_cells(inner.x, inner.w) <= surface::add_cells(outer.x, outer.w) &&
           surface::add_cells(inner.y, inner.h) <= surface::add_cells(outer.y, outer.h) &&
           (inner.w < outer.w || inner.h < outer.h);
}

/// ONE NAMED RUN OF A ROW WHERE A FIT DRAWS IT: the cells of `cells` that show its columns of
/// `shown` -- past a caret glyph at `caret`, whose own cell stands for the caret's column -- the
/// characters there, and the point of its middle own one -- a character no part of `parts` inside
/// it holds -- or of its first cell where it has none. False where no cell shows any of its columns.
bool row_part_on(const GlyphGrid& g, std::int64_t row, const std::string& shown,
                 std::int64_t caret, std::int64_t cells, const PaneRowPart& part,
                 const std::vector<PaneRowPart>& parts, std::int64_t space, PanePart& out) {
    const std::int64_t end = part.column + part.columns;
    const std::int64_t first = part.column + (caret >= 0 && part.column > caret ? 1 : 0);
    const std::int64_t last =
        (std::min)(end - 1 + (caret >= 0 && end - 1 >= caret ? 1 : 0), cells - 1);
    if (first > last) return false;
    const auto size = static_cast<std::int64_t>(shown.size());
    const auto from = static_cast<std::size_t>((std::min)(part.column, size));
    const auto to = static_cast<std::size_t>((std::min)(end, size));
    out.name = part.name;
    out.text = without_trailing_blanks(shown.substr(from, to - from));
    out.place = DeskRect{g.x + first * g.advance, g.y + row * g.line, (last - first + 1) * g.advance,
                         g.line};
    const auto bytes = static_cast<std::int64_t>(out.text.size());
    std::vector<char> held(static_cast<std::size_t>(bytes), 0);
    for (const PaneRowPart& inner : parts) {
        if (!inside_part(inner, part)) continue;
        for (std::int64_t c = inner.column; c < inner.column + inner.columns && c < part.column + bytes;
             ++c) {
            held[static_cast<std::size_t>(c - part.column)] = 1;
        }
    }
    std::vector<std::int64_t> own;
    for (std::int64_t c = 0; c < bytes; ++c) {
        if (!held[static_cast<std::size_t>(c)]) own.push_back(part.column + c);
    }
    const std::int64_t at = !own.empty() ? drawn_column(own[(own.size() - 1) / 2], caret) : first;
    glyph_point(g, row, at, space, out.x, out.y);
    out.space = space;
    return true;
}

/// The words lying wholly inside a place, said in their order and joined by a space.
std::string words_inside(const std::vector<PaneWord>& words, const DeskRect& place) {
    std::string out;
    for (const PaneWord& w : words) {
        if (w.text.empty() || w.place.x < place.x || w.place.y < place.y ||
            w.place.x + w.place.w > place.x + place.w || w.place.y + w.place.h > place.y + place.h) {
            continue;
        }
        out += out.empty() ? w.text : " " + w.text;
    }
    return out;
}

/// A canvas rectangle in a medium's own units -- pixels in a window, cells in a terminal -- as the
/// medium shows [begin, end) on them: [floor(begin / unit), floor(end / unit)) on each axis.
struct UnitBox {
    std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool empty() const { return x1 <= x0 || y1 <= y0; }
};

/// A part's local rectangle on the body, cut to it, in the medium's units.
UnitBox units_of(const PixelRect& body, const PaneCanvasPart& part, std::int64_t unit) {
    const std::int64_t left = surface::add_cells(body.x, part.x);
    const std::int64_t top = surface::add_cells(body.y, part.y);
    return UnitBox{surface::floor_div_px((std::max)(body.x, left), unit),
                   surface::floor_div_px((std::max)(body.y, top), unit),
                   surface::floor_div_px((std::min)(body.x + body.w, surface::add_cells(left, part.w)), unit),
                   surface::floor_div_px((std::min)(body.y + body.h, surface::add_cells(top, part.h)), unit)};
}

/// The middle of the widest stretch of [lo, hi) that no cut covers, or nothing where cuts cover all.
std::optional<std::int64_t> middle_of_widest(std::int64_t lo, std::int64_t hi,
                                             std::vector<std::pair<std::int64_t, std::int64_t>> cuts) {
    std::sort(cuts.begin(), cuts.end());
    std::optional<std::int64_t> best;
    std::int64_t widest = 0;
    std::int64_t at = lo;
    const auto keep = [&](std::int64_t from, std::int64_t to) {
        if (to - from > widest) {
            widest = to - from;
            best = from + (to - from - 1) / 2;
        }
    };
    for (const auto& [from, to] : cuts) {
        if (from > at) keep(at, (std::min)(from, hi));
        at = (std::max)(at, to);
    }
    if (at < hi) keep(at, hi);
    return best;
}

/// WHERE A PRESS NAMES A PART AND NOTHING INSIDE IT: its centre unit, or, where a part inside it
/// covers that, the middle of the widest uncovered stretch of the first line to have one -- its
/// centre row, top row, bottom row, then centre, left and right columns. Else its centre.
std::pair<std::int64_t, std::int64_t> own_point(const UnitBox& box, const std::vector<UnitBox>& held) {
    const std::int64_t cx = box.x0 + (box.x1 - box.x0 - 1) / 2;
    const std::int64_t cy = box.y0 + (box.y1 - box.y0 - 1) / 2;
    if (std::none_of(held.begin(), held.end(), [cx, cy](const UnitBox& b) {
            return cx >= b.x0 && cx < b.x1 && cy >= b.y0 && cy < b.y1;
        })) {
        return {cx, cy};
    }
    for (const std::int64_t y : {cy, box.y0, box.y1 - 1}) {
        std::vector<std::pair<std::int64_t, std::int64_t>> cuts;
        for (const UnitBox& b : held) {
            if (y >= b.y0 && y < b.y1) cuts.emplace_back(b.x0, b.x1);
        }
        if (const auto x = middle_of_widest(box.x0, box.x1, std::move(cuts))) return {*x, y};
    }
    for (const std::int64_t x : {cx, box.x0, box.x1 - 1}) {
        std::vector<std::pair<std::int64_t, std::int64_t>> cuts;
        for (const UnitBox& b : held) {
            if (x >= b.x0 && x < b.x1) cuts.emplace_back(b.y0, b.y1);
        }
        if (const auto y = middle_of_widest(box.y0, box.y1, std::move(cuts))) return {x, *y};
    }
    return {cx, cy};
}

/// ONE NAMED PART OF A PICTURE WHERE ITS BODY SHOWS IT: the part's rectangle cut to the body, the
/// words inside it, and the point a press names it by and no part of `parts` inside it -- a pixel
/// in a window, the cell the medium paints there in a terminal. False where the medium shows none.
bool canvas_part_on(const PixelRect& body, const PaneCanvasPart& part,
                    const std::vector<PaneCanvasPart>& parts, const std::vector<PaneWord>& words,
                    std::int64_t space, PanePart& out) {
    const std::int64_t unit = space == input::space::kPixels ? 1 : surface::kCanvasCellPx;
    const UnitBox box = units_of(body, part, unit);
    const UnitBox px = units_of(body, part, 1);
    if (box.empty() || px.empty()) return false;
    std::vector<UnitBox> held;
    for (const PaneCanvasPart& inner : parts) {
        if (!inside_part(inner, part)) continue;
        const UnitBox b = units_of(body, inner, unit);
        if (!b.empty()) held.push_back(b);
    }
    out.name = part.name;
    out.place = DeskRect{px.x0, px.y0, px.x1 - px.x0, px.y1 - px.y0};
    out.text = words_inside(words, out.place);
    const auto [x, y] = own_point(box, held);
    out.x = x;
    out.y = space == input::space::kPixels ? y : y + surface::kTuiCanvasTopRow;
    out.space = space;
    return true;
}

/// THE MENU ON THE SCREEN, read from the painter's own composition of it: Workshop's contextual
/// menu, or a pane's as its presenter showed it, and the lines each names. At most one is open.
v2::DeskMenu desk_menu(const Session& s, const Screen& sc) {
    v2::DeskMenu menu;
    surface::SurfaceLayer layer;
    PixelRect bounds;
    if (s.presented.open) {
        menu.office = s.presented.office;
        menu.pane = s.presented.pane;
        menu.picture = s.presented.picture;
        bounds = presented_bounds(s, sc);
        paint_presented(layer, s, sc);
    } else if (s.context.open) {
        menu.office = kWorkshopProvider;
        bounds = context_bounds(s, sc);
        paint_context(layer, s, sc);
    } else {
        return menu;
    }
    menu.open = true;
    menu.place = desk_rect(bounds);
    if (layer.texts.empty()) {
        return menu; // granted and not shown yet: no line is drawn, so none is said
    }
    const ProsePlace place = prose_place(bounds, sc);
    const GlyphGrid grid = glyph_grid(place.fit);
    const surface::SurfaceTextRegion& region = layer.texts.back();
    const std::int64_t space = input_space_of(sc);
    for (std::size_t i = 0; i < region.rows.size(); ++i) {
        std::string text = without_trailing_blanks(region.rows[i].text);
        const std::int64_t row = static_cast<std::int64_t>(i);
        PaneWord line = word_on(grid, row, 0, std::move(text), -1, space);
        line.word = row;
        menu.lines.push_back(std::move(line));
    }
    // THE LINES EACH MENU NAMES: a pane's, as its presenter named them; Workshop's own, by the
    // action or group each line shows.
    std::vector<PaneRowPart> named = s.presented.open ? s.presented.parts : std::vector<PaneRowPart>{};
    if (!s.presented.open) {
        const std::vector<std::string> names = context_line_names(s, sc);
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (!names[i].empty()) {
                named.push_back(PaneRowPart{names[i], static_cast<std::int64_t>(i), 0, place.columns});
            }
        }
    }
    for (const PaneRowPart& part : named) {
        if (part.row < 0 || part.row >= static_cast<std::int64_t>(region.rows.size())) continue;
        PanePart drawn;
        if (row_part_on(grid, part.row, region.rows[static_cast<std::size_t>(part.row)].text, -1,
                        place.columns, part, named, space, drawn)) {
            menu.parts.push_back(std::move(drawn));
        }
    }
    return menu;
}

} // namespace

// THE DESK, BY ITS OWN NUMBERS: every pane the desk names, in its order, with what Workshop
// resolves for it now; nothing is read off a picture.
// WL-GEO-13 -- agents/workshop/geometry.md
v2::DeskView WorkshopWeave::desk_view() const {
    const Screen sc = screen_of(session_);
    const Setup& setup = session_.setup.active;
    const Panes& panes = session_.panes;
    v2::DeskView out;
    out.width = sc.w;
    out.height = sc.h;
    out.cell_px = sc.cell_px;
    out.space = input_space_of(sc);
    out.room = DeskRect{0, sc.room_y, sc.room_w, sc.room_h};
    const std::vector<CatalogRow> rows = inventory_rows(setup, panes);
    const std::vector<std::int64_t> order = effective_pane_order(setup, panes);
    const std::int64_t selected = selected_pane(panes);
    const std::int64_t keys = keyboard_pane();
    for (const SetupPane& authored : setup.panes) {
        CatalogRow row{kNoPaneKind, authored.ref, authored.ref.pane, std::string()};
        for (const CatalogRow& known : rows) {
            if (known.ref == authored.ref) {
                row = known;
                break;
            }
        }
        DeskPane pane;
        pane.provider = authored.ref.provider;
        pane.pane = authored.ref.pane;
        pane.name = row.name;
        pane.state = pane_state_word(pane_state_of(panes, setup, sc, row));
        if (row.kind != kNoPaneKind && panes.has(row.kind)) {
            const PaneBounds where = bounds_of(panes, setup, row.kind, sc);
            pane.resolved = desk_rect(where.resolved);
            pane.visible = desk_rect(where.rect);
            for (std::size_t i = 0; i < order.size(); ++i) {
                if (order[i] == row.kind) {
                    pane.front = static_cast<std::int64_t>(order.size() - 1 - i);
                }
            }
            pane.selected = row.kind == selected;
            pane.keys = row.kind == keys;
        }
        out.panes.push_back(std::move(pane));
    }
    out.arranging = session_.arrange.open;
    out.menu = desk_menu(session_, sc);
    return out;
}

// The first version says the same desk, its menu without the lines it names.
void WorkshopWeave::on(const DeskViewRequested&, loom::Mail& mail) {
    const v2::DeskView now = desk_view();
    const v2::DeskMenu& m = now.menu;
    (void)mail.answer(DeskView{now.width, now.height, now.cell_px, now.space, now.room, now.panes,
                               now.arranging,
                               DeskMenu{m.open, m.office, m.pane, m.picture, m.place, m.lines}});
}

void WorkshopWeave::on(const v2::DeskViewRequested&, loom::Mail& mail) {
    (void)mail.answer(desk_view());
}

// ---- A PANE'S WORDS, TEXT OR CANVAS ALIKE --------------------------------------------------------

namespace {

/// ONE CANVAS LABEL AS THE PAINTER DRAWS IT (`paint_pane_canvas`): the bytes left of the body
/// dropped whole, the row cut at its right edge, a label outside its rows not drawn at all.
bool drawn_label(const PaneCanvasLabel& label, const PixelRect& body, std::string& text,
                 std::int64_t& x) {
    if (label.y < 0 || label.y > body.h - kPaneCanvasUnit) return false;
    std::size_t first = 0;
    x = label.x;
    while (first < label.text.size() && x < 0) {
        x = surface::add_cells(x, kPaneCanvasUnit);
        ++first;
    }
    if (x > body.w - kPaneCanvasUnit || first == label.text.size()) return false;
    const auto room = static_cast<std::size_t>((body.w - x) / kPaneCanvasUnit);
    const auto count = (std::min)(label.text.size() - first, room);
    if (count == 0) return false;
    text = label.text.substr(first, count);
    return true;
}

} // namespace

// WHAT A VISIBLE BODY SHOWS, word by word: a text pane's rows under its header, a canvas pane's
// labels and then its text runs, each where the medium draws it. A word the body does not draw
// is not said, so a word's number is its place in this list and nowhere else. `glyphs` takes
// where each word's glyphs stand, which a point inside it is measured by.
// WL-GEO-13 -- agents/workshop/geometry.md
std::vector<PaneWord> WorkshopWeave::visible_words(const VisibleBody& visible,
                                                   std::vector<WordGlyphs>* glyphs) const {
    const Screen sc = screen_of(session_);
    const std::int64_t space = input_space_of(sc);
    std::vector<PaneWord> out;
    const auto keep = [&](const GlyphGrid& grid, std::string text, std::int64_t row,
                          std::int64_t caret) {
        PaneWord w = word_on(grid, row, 0, std::move(text), caret, space);
        w.word = static_cast<std::int64_t>(out.size());
        out.push_back(std::move(w));
        if (glyphs != nullptr) glyphs->push_back(WordGlyphs{grid.advance, caret});
    };
    const auto* content = visible.content;
    if (!visible.canvas) {
        const auto& body = visible.body;
        const GlyphGrid grid = glyph_grid(body.fit);
        for (std::int64_t row = 0;
             row < body.rows && row < static_cast<std::int64_t>(content->shown.size()); ++row) {
            // A terminal inserts the caret as a glyph of its own and then cuts the row to the
            // body's columns, so a row with the caret shows one character fewer, and the text
            // after the glyph stands a cell on.
            const std::int64_t glyph = external_caret_glyph(content, body.fit, row);
            const std::int64_t caret = glyph < body.columns ? glyph : -1;
            const std::int64_t room = caret >= 0 ? body.columns - 1 : body.columns;
            std::string text = without_trailing_blanks(
                content->shown[static_cast<std::size_t>(row)].text.substr(
                    0, static_cast<std::size_t>(room)));
            const bool in_word = caret >= 0 && caret <= static_cast<std::int64_t>(text.size());
            keep(grid, std::move(text), row + body.header_rows, in_word ? caret : -1);
        }
        return out;
    }
    const PixelRect& body = visible.canvas_body;
    const auto& picture = content->canvas.content;
    for (const PaneCanvasLabel& label : picture.labels) {
        std::string text;
        std::int64_t x = 0;
        if (!drawn_label(label, body, text, x)) continue;
        const GlyphGrid grid{surface::add_cells(body.x, x), surface::add_cells(body.y, label.y),
                             kPaneCanvasUnit, kPaneCanvasUnit};
        keep(grid, std::move(text), 0, -1);
    }
    const PaneCanvasRoom room{picture.pane, picture.grant, body.w, body.h, content->canvas.grain,
                              content->canvas.graphical, content->canvas.text_advance_px,
                              content->canvas.text_line_px};
    for (const PaneCanvasText& run : picture.texts) {
        const auto placed = clip_canvas_text(run, {0, 0, body.w, body.h}, room);
        if (!placed.visible()) continue;
        const surface::SurfaceTextRegion region = canvas_text_region(placed, body.x, body.y);
        const surface::RegionFit fit = surface::fit_region(region.x, region.y, region.w, region.h,
                                                           sc.text_advance_px, sc.text_line_px);
        std::string text = placed.text.text;
        const bool caret = !fit.graphical() && region.caret_row == 0 && region.caret_col >= 0 &&
                           region.caret_col <= static_cast<std::int64_t>(text.size());
        keep(glyph_grid(fit), std::move(text), 0, caret ? region.caret_col : -1);
    }
    return out;
}

void WorkshopWeave::on(const v2::PaneViewRequested& asked, loom::Mail& mail) {
    VisibleBody visible;
    if (const auto why = visible_body(asked.provider, asked.pane, visible, true); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    (void)mail.answer(v2::PaneView{asked.provider, asked.pane, visible.content->stamp.aimed,
                                   visible.canvas, visible_words(visible)});
}

// THE PARTS A VISIBLE BODY'S PANE NAMES, each where the medium draws it, as the pane named it: a
// part the body does not show is not said.
// WL-HAND-06 -- agents/workshop/pane-controls.md
std::vector<PanePart> WorkshopWeave::visible_parts(const VisibleBody& visible,
                                                   const std::vector<PaneWord>& words) const {
    const std::int64_t space = input_space_of(screen_of(session_));
    std::vector<PanePart> out;
    const auto* content = visible.content;
    if (visible.canvas) {
        for (const PaneCanvasPart& part : content->canvas.parts) {
            PanePart drawn;
            if (canvas_part_on(visible.canvas_body, part, content->canvas.parts, words, space, drawn)) {
                out.push_back(std::move(drawn));
            }
        }
        return out;
    }
    const auto& body = visible.body;
    const GlyphGrid grid = glyph_grid(body.fit);
    for (const PaneRowPart& part : content->parts) {
        if (part.row >= body.rows || part.row >= static_cast<std::int64_t>(content->shown.size())) {
            continue;
        }
        // The row as `visible_words` reads it: a caret's glyph inserted, then the row cut.
        const std::int64_t glyph = external_caret_glyph(content, body.fit, part.row);
        const std::int64_t caret = glyph < body.columns ? glyph : -1;
        const std::int64_t room = caret >= 0 ? body.columns - 1 : body.columns;
        const std::string shown = content->shown[static_cast<std::size_t>(part.row)].text.substr(
            0, static_cast<std::size_t>(room));
        PanePart drawn;
        if (row_part_on(grid, part.row + body.header_rows, shown, caret, body.columns, part,
                        content->parts, space, drawn)) {
            out.push_back(std::move(drawn));
        }
    }
    return out;
}

void WorkshopWeave::on(const v3::PaneViewRequested& asked, loom::Mail& mail) {
    VisibleBody visible;
    if (const auto why = visible_body(asked.provider, asked.pane, visible, true); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    std::vector<PaneWord> words = visible_words(visible);
    std::vector<PanePart> parts = visible_parts(visible, words);
    (void)mail.answer(v3::PaneView{asked.provider, asked.pane, visible.content->stamp.aimed,
                                   visible.canvas, std::move(words), std::move(parts)});
}

// WHERE ONE CHARACTER OF ONE WORD IS NOW, measured as the word was and checked by resolving it: a
// text row through the press measurer, a canvas word against the body its press lands in.
void WorkshopWeave::on(const v2::PanePointRequested& asked, loom::Mail& mail) {
    VisibleBody visible;
    if (const auto why = visible_body(asked.provider, asked.pane, visible, true); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    if (asked.picture != visible.content->stamp.aimed) {
        (void)mail.answer(loom::Refused{"pane point unavailable: the pane's picture moved; read it again"});
        return;
    }
    std::vector<WordGlyphs> glyphs;
    const std::vector<PaneWord> words = visible_words(visible, &glyphs);
    if (asked.word < 0 || asked.word >= static_cast<std::int64_t>(words.size()) ||
        asked.column < 0 ||
        asked.column >= static_cast<std::int64_t>(words[static_cast<std::size_t>(asked.word)].text.size())) {
        (void)mail.answer(loom::Refused{"pane point unavailable: outside the pane's visible words"});
        return;
    }
    v2::PanePoint reply{asked.provider, asked.pane, asked.picture, asked.word, asked.column, 0, 0, 0};
    bool resolved = false;
    if (!visible.canvas) {
        resolved = cell_center(visible, asked.word, asked.column, reply.x, reply.y, reply.space);
    } else {
        const auto at_word = static_cast<std::size_t>(asked.word);
        const DeskRect& place = words[at_word].place;
        reply.space = input_space_of(screen_of(session_));
        const WordGlyphs& drawn = glyphs[at_word];
        glyph_point(GlyphGrid{place.x, place.y, drawn.advance, place.h}, 0,
                    drawn_column(asked.column, drawn.caret), reply.space, reply.x, reply.y);
        const PointedAt at = canvas_point_of(reply.space, reply.x, reply.y);
        resolved = at.understood &&
                   visible.canvas_body.contains_at(at.px.x, at.px.y, at.grain) &&
                   PixelRect{place.x, place.y, place.w, place.h}.contains_at(at.px.x, at.px.y, at.grain);
    }
    if (!resolved) {
        (void)mail.answer(loom::Refused{"pane point unavailable: that character is not addressable"});
        return;
    }
    (void)mail.answer(reply);
}

// ---- A PANE AS AN INSPECTOR'S SUBJECT (the Info pane's) -------------------------------------

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::refresh_inspected() {
    InspectedPane& in = session_.inspected;
    if (!in.addressed()) {
        return;
    }
    // THE TWO THINGS A NAME STANDS FOR: this pane (the door keeps it) and this desk. A value
    // moving is neither -- the rows read fresh -- and neither is a provider arriving or leaving:
    // its rows say so, and a write it made impossible is refused by the owner in its own words.
    if (in.name != 0 && in.desk == session_.setup.put_live) {
        return;
    }
    in.rows = pane_subject_rows(session_, in.ref);
    in.desk = session_.setup.put_live;
    in.name = ++in.minted;
}

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::on(const InspectPaneRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return; // an office asks; personal speech is answered by nobody
    }
    const PaneRef ref{asked.office, asked.pane};
    bool known = false;
    for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panes)) {
        if (row.ref == ref) {
            known = true;
            break;
        }
    }
    if (ref.provider.empty() || !known) {
        (void)mail.answer(PaneSubjectActed{
            false, ref_text(ref) +
                       " is in neither this build's vocabulary nor this desk -- nothing to inspect"});
        return;
    }
    InspectedPane& in = session_.inspected;
    if (!(in.ref == ref)) {
        in.ref = ref;
        in.name = 0; // another pane: named afresh below, so no draft typed for the last one lands
    }
    refresh_inspected();
    (void)mail.answer(PaneSubjectActed{true, std::string()});
    repaint(mail);
}

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::on(const PaneSubjectRequested&, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return;
    }
    refresh_inspected();
    (void)mail.answer(pane_subject_shown(session_));
}

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::publish_pane_subject(loom::Mail& mail) {
    // NOTHING IS SAID ABOUT A SUBJECT NOBODY NAMED. An inspector that arrives asks and is answered
    // "none" (`on(PaneSubjectRequested)`); until one names a pane, this host has no sentence here.
    if (!session_.inspected.addressed()) {
        return;
    }
    refresh_inspected();
    PaneSubjectShown said = pane_subject_shown(session_);
    if (subject_published_ && same_pane_subject(said, subject_said_)) {
        return; // no news is silence, and silence is what makes this seam terminate
    }
    subject_said_ = said;
    subject_published_ = true;
    (void)mail.as_role(kWorkshopProvider).publish(std::move(said));
}

// WL-INFO-15 -- agents/workshop/info-body.md
void WorkshopWeave::on(const PaneCommitRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return;
    }
    const auto answer = [&mail](bool accepted, std::string refusal) {
        (void)mail.answer(PaneSubjectActed{accepted, std::move(refusal)});
    };
    // THE NAME FIRST, BEFORE ANY ROW IS READ OR ANY SETTER REACHED -- and judged against the
    // facts as they are now, so a desk put live since the picture was drawn is caught here
    // even though no repaint has named it yet.
    refresh_inspected();
    InspectedPane& in = session_.inspected;
    if (in.name == 0 || asked.subject != in.name) {
        answer(false, kPaneCommitSubjectGone);
        return;
    }
    if (asked.row < 0 || static_cast<std::size_t>(asked.row) >= in.rows.size()) {
        answer(false, "that row is not in this pane's properties any more");
        return;
    }
    Row& row = in.rows[static_cast<std::size_t>(asked.row)];
    if (!row.editable()) {
        answer(false, row.label() + " is not authored -- it is what the screen makes of the "
                                    "authored value");
        return;
    }
    const Commit result = row.commit_text(asked.text);
    if (result != Commit::Accepted) {
        // THE OWNER'S OWN WORDS: an amount the face does not read, a pane that is not in this
        // desk, a definition that is not open -- each worded by its setter.
        answer(false, row.label() + ": " + row.refusal());
        return;
    }
    // SAID WITH WHAT WAS WRITTEN, TO WHICH PANE, read before the reseat below touches anything.
    const std::string written = "committed " + row.label() + " of " +
                                pane_subject_shown(session_).name + " = " + row.value();
    // A PLACEMENT WRITE IS RECONCILED as the hand's is (`arrange_place`): an authored place
    // takes the pane out of the stack, and `apply_setup` is the one door that opens or closes a
    // pane -- for a place, with no room rationing the stack, it opens and closes nothing.
    apply_setup(mail);
    say(written, false);
    answer(true, std::string());
    repaint(mail);
}

} // namespace zengine::workshop
