// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s inspection seam: a pane an inspector names, the picture of its rows, and one
// write through the rows' owner.
// Workshop law: agents/workshop/info-body.md (agents/workshop.md routes)

#include "weave.hpp"
#include "screen_canvas.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <tuple>
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

// THE CENTER OF THE CELL SHOWING ONE CHARACTER, checked by resolving it to that row and column.
bool WorkshopWeave::cell_center(const VisibleBody& visible, std::int64_t row, std::int64_t column,
                                std::int64_t& x, std::int64_t& y, std::int64_t& space) const {
    const auto hit = cell_point(visible, row, column, x, y, space);
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
        // The row's third cell, or its last in a narrower body.
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
/// grid, and its middle byte's centre as the place a press names it -- or, with no byte to name,
/// its first cell, where a caret on an empty line stands.
PaneWord word_on(const GlyphGrid& g, std::int64_t row, std::int64_t column, std::string text,
                 std::int64_t space) {
    PaneWord w;
    const std::int64_t bytes = static_cast<std::int64_t>(text.size());
    const std::int64_t n = (std::max<std::int64_t>)(1, bytes);
    w.place = DeskRect{g.x + column * g.advance, g.y + row * g.line, n * g.advance, g.line};
    glyph_point(g, row, column + (bytes > 0 ? (bytes - 1) / 2 : 0), space, w.x, w.y);
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

/// What a column or a unit no part holds gives a press to.
constexpr std::size_t kNoPart = static_cast<std::size_t>(-1);

/// A PART NO PRESS REACHES ON ITS OWN HAS NO POINT: one in no space (`input::space::kUnknown`),
/// which no consumer reads, so a press made there blindly lands nowhere rather than on another.
void no_point(PanePart& out) {
    out.x = 0;
    out.y = 0;
    out.space = input::space::kUnknown;
}

/// THE PART A PRESS ON EACH COLUMN OF EACH ROW REACHES: of `parts`, by its index, the last listed
/// that holds the column, or `kNoPart` -- on each row, every column up to the last any part holds.
std::map<std::int64_t, std::vector<std::size_t>> row_owners(const std::vector<PaneRowPart>& parts) {
    std::map<std::int64_t, std::vector<std::size_t>> owners;
    for (const PaneRowPart& part : parts) {
        std::vector<std::size_t>& row = owners[part.row];
        const auto end = static_cast<std::size_t>((std::max<std::int64_t>)(0, part.column + part.columns));
        if (row.size() < end) row.resize(end, kNoPart);
    }
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const PaneRowPart& part = parts[i];
        std::vector<std::size_t>& row = owners[part.row];
        for (std::int64_t c = (std::max<std::int64_t>)(0, part.column); c < part.column + part.columns;
             ++c) {
            row[static_cast<std::size_t>(c)] = i;
        }
    }
    return owners;
}

/// ONE NAMED RUN OF A ROW WHERE A FIT DRAWS IT: the cells of `cells` that show the columns of
/// `parts[self]` in `shown`, the characters there, and its point: the middle of its own
/// characters, the ones `owner` gives it, else the middle of its own blank cells the body shows,
/// else none. False where no cell shows any of its columns.
bool row_part_on(const GlyphGrid& g, std::int64_t row, const std::string& shown,
                 std::int64_t cells, const std::vector<PaneRowPart>& parts,
                 const std::vector<std::size_t>& owner, std::size_t self, std::int64_t space,
                 PanePart& out) {
    const PaneRowPart& part = parts[self];
    const std::int64_t end = part.column + part.columns;
    const std::int64_t first = part.column;
    const std::int64_t last = (std::min)(end - 1, cells - 1);
    if (first > last) return false;
    const auto size = static_cast<std::int64_t>(shown.size());
    const auto from = static_cast<std::size_t>((std::min)(part.column, size));
    const auto to = static_cast<std::size_t>((std::min)(end, size));
    out.name = part.name;
    out.text = without_trailing_blanks(shown.substr(from, to - from));
    out.place = DeskRect{g.x + first * g.advance, g.y + row * g.line, (last - first + 1) * g.advance,
                         g.line};
    const std::int64_t characters_end = part.column + static_cast<std::int64_t>(out.text.size());
    std::vector<std::int64_t> characters, blanks;
    for (std::int64_t c = part.column; c <= last; ++c) {
        if (owner[static_cast<std::size_t>(c)] != self) continue;
        (c < characters_end ? characters : blanks).push_back(c);
    }
    const std::vector<std::int64_t>& own = !characters.empty() ? characters : blanks;
    if (own.empty()) {
        no_point(out);
        return true;
    }
    glyph_point(g, row, own[(own.size() - 1) / 2], space, out.x, out.y);
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

/// A unit of a medium, by its column and row.
using UnitPoint = std::pair<std::int64_t, std::int64_t>;

/// WHERE A PRESS REACHES EACH PART OF A PICTURE AS ITS OWN, in the medium's units: of `boxes`, in
/// the order the pane reads a press, a unit is the last one's that holds it. A part's point is its
/// centre unit where that is its own, else the middle of its widest stretch of its own on the row
/// nearest its centre that has one -- the upper of two as near, the leftmost of two as wide -- else
/// none. Each band the boxes' edges cut is painted once, from the last box down: the cost follows
/// the edges, no more than the body's units, never how the boxes nest or overlap.
std::vector<std::optional<UnitPoint>> own_points(const std::vector<UnitBox>& boxes) {
    std::vector<std::optional<UnitPoint>> out(boxes.size());
    std::vector<std::int64_t> xs, ys;
    for (const UnitBox& b : boxes) {
        if (b.empty()) continue;
        xs.insert(xs.end(), {b.x0, b.x1});
        ys.insert(ys.end(), {b.y0, b.y1});
    }
    for (std::vector<std::int64_t>* edges : {&xs, &ys}) {
        std::sort(edges->begin(), edges->end());
        edges->erase(std::unique(edges->begin(), edges->end()), edges->end());
    }
    if (xs.size() < 2) return out;
    const auto edge = [](const std::vector<std::int64_t>& edges, std::int64_t v) {
        return static_cast<std::size_t>(std::lower_bound(edges.begin(), edges.end(), v) - edges.begin());
    };
    // Each box's run of columns and of bands between the edges, its centre, and the best stretch
    // of its own found so far: its row's distance from the centre, the row, its width, its left end.
    struct Seen {
        std::size_t c0 = 0, c1 = 0, b0 = 0, b1 = 0;
        std::int64_t cx = 0, cy = 0;
        bool centre = false;
        std::int64_t far = -1, row = 0, width = 0, left = 0;
    };
    std::vector<Seen> seen(boxes.size());
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const UnitBox& b = boxes[i];
        if (b.empty()) continue;
        Seen& s = seen[i];
        s.c0 = edge(xs, b.x0);
        s.c1 = edge(xs, b.x1);
        s.b0 = edge(ys, b.y0);
        s.b1 = edge(ys, b.y1);
        s.cx = b.x0 + (b.x1 - b.x0 - 1) / 2;
        s.cy = b.y0 + (b.y1 - b.y0 - 1) / 2;
    }
    const std::size_t columns = xs.size() - 1;
    std::vector<std::size_t> owner(columns), next(columns + 1);
    // The first column at or after `k` this band has not painted.
    const auto unpainted = [&next](std::size_t k) {
        std::size_t root = k;
        while (next[root] != root) root = next[root];
        while (next[k] != root) {
            const std::size_t up = next[k];
            next[k] = root;
            k = up;
        }
        return root;
    };
    for (std::size_t band = 0; band + 1 < ys.size(); ++band) {
        std::fill(owner.begin(), owner.end(), kNoPart);
        for (std::size_t k = 0; k <= columns; ++k) next[k] = k;
        for (std::size_t i = boxes.size(); i-- > 0;) {
            const Seen& s = seen[i];
            if (boxes[i].empty() || band < s.b0 || band >= s.b1) continue;
            for (std::size_t k = unpainted(s.c0); k < s.c1; k = unpainted(k + 1)) {
                owner[k] = i;
                next[k] = k + 1;
            }
        }
        const std::int64_t top = ys[band], bottom = ys[band + 1] - 1;
        for (std::size_t k = 0; k < columns;) {
            std::size_t end = k + 1;
            while (end < columns && owner[end] == owner[k]) ++end;
            if (owner[k] != kNoPart) {
                Seen& s = seen[owner[k]];
                const std::int64_t left = xs[k], width = xs[end] - xs[k];
                s.centre = s.centre ||
                           (s.cy >= top && s.cy <= bottom && s.cx >= left && s.cx < left + width);
                const std::int64_t row = (std::clamp)(s.cy, top, bottom);
                const std::int64_t far = row > s.cy ? row - s.cy : s.cy - row;
                if (s.far < 0 || std::make_tuple(far, row, -width, left) <
                                     std::make_tuple(s.far, s.row, -s.width, s.left)) {
                    s.far = far;
                    s.row = row;
                    s.width = width;
                    s.left = left;
                }
            }
            k = end;
        }
    }
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const Seen& s = seen[i];
        if (s.centre) {
            out[i] = UnitPoint{s.cx, s.cy};
        } else if (s.far >= 0) {
            out[i] = UnitPoint{s.left + (s.width - 1) / 2, s.row};
        }
    }
    return out;
}

/// ONE NAMED PART OF A PICTURE WHERE ITS BODY SHOWS IT: its rectangle cut to the body, the words
/// inside it, and its point in `box`'s units (`own_points`) -- a pixel in a window, the cell the
/// medium paints there in a terminal -- or none. False where the medium shows none of it.
bool canvas_part_on(const PixelRect& body, const PaneCanvasPart& part, const UnitBox& box,
                    const std::optional<UnitPoint>& point, const std::vector<PaneWord>& words,
                    std::int64_t space, PanePart& out) {
    const UnitBox px = units_of(body, part, 1);
    if (box.empty() || px.empty()) return false;
    out.name = part.name;
    out.place = DeskRect{px.x0, px.y0, px.x1 - px.x0, px.y1 - px.y0};
    out.text = words_inside(words, out.place);
    if (!point) {
        no_point(out);
        return true;
    }
    out.x = point->first;
    out.y = space == input::space::kPixels ? point->second : point->second + surface::kTuiCanvasTopRow;
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
        PaneWord line = word_on(grid, row, 0, std::move(text), space);
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
    const std::map<std::int64_t, std::vector<std::size_t>> owners = row_owners(named);
    for (std::size_t i = 0; i < named.size(); ++i) {
        const PaneRowPart& part = named[i];
        if (part.name.empty() || part.row < 0 ||
            part.row >= static_cast<std::int64_t>(region.rows.size())) {
            continue;
        }
        PanePart drawn;
        if (row_part_on(grid, part.row, region.rows[static_cast<std::size_t>(part.row)].text,
                        place.columns, named, owners.at(part.row), i, space, drawn)) {
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
    const auto keep = [&](const GlyphGrid& grid, std::string text, std::int64_t row) {
        PaneWord w = word_on(grid, row, 0, std::move(text), space);
        w.word = static_cast<std::int64_t>(out.size());
        out.push_back(std::move(w));
        if (glyphs != nullptr) glyphs->push_back(WordGlyphs{grid.advance});
    };
    const auto* content = visible.content;
    if (!visible.canvas) {
        const auto& body = visible.body;
        const GlyphGrid grid = glyph_grid(body.fit);
        for (std::int64_t row = 0;
             row < body.rows && row < static_cast<std::int64_t>(content->shown.size()); ++row) {
            std::string text = without_trailing_blanks(
                content->shown[static_cast<std::size_t>(row)].text.substr(
                    0, static_cast<std::size_t>(body.columns)));
            keep(grid, std::move(text), row + body.header_rows);
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
        keep(grid, std::move(text), 0);
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
        keep(glyph_grid(fit), placed.text.text, 0);
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
// part the body does not show is not said, and neither is a place the pane names nothing.
// WL-HAND-06 -- agents/workshop/pane-parts.md
std::vector<PanePart> WorkshopWeave::visible_parts(const VisibleBody& visible,
                                                   const std::vector<PaneWord>& words) const {
    const std::int64_t space = input_space_of(screen_of(session_));
    std::vector<PanePart> out;
    const auto* content = visible.content;
    if (visible.canvas) {
        const std::vector<PaneCanvasPart>& parts = content->canvas.parts;
        const std::int64_t unit = space == input::space::kPixels ? 1 : surface::kCanvasCellPx;
        std::vector<UnitBox> boxes;
        boxes.reserve(parts.size());
        for (const PaneCanvasPart& part : parts) {
            boxes.push_back(units_of(visible.canvas_body, part, unit));
        }
        const std::vector<std::optional<UnitPoint>> points = own_points(boxes);
        for (std::size_t i = 0; i < parts.size(); ++i) {
            PanePart drawn;
            if (!parts[i].name.empty() && canvas_part_on(visible.canvas_body, parts[i], boxes[i],
                                                         points[i], words, space, drawn)) {
                out.push_back(std::move(drawn));
            }
        }
        return out;
    }
    const auto& body = visible.body;
    const GlyphGrid grid = glyph_grid(body.fit);
    const std::map<std::int64_t, std::vector<std::size_t>> owners = row_owners(content->parts);
    for (std::size_t i = 0; i < content->parts.size(); ++i) {
        const PaneRowPart& part = content->parts[i];
        if (part.name.empty() || part.row >= body.rows ||
            part.row >= static_cast<std::int64_t>(content->shown.size())) {
            continue;
        }
        // The row as `visible_words` reads it: cut to the body's columns.
        const std::string shown = content->shown[static_cast<std::size_t>(part.row)].text.substr(
            0, static_cast<std::size_t>(body.columns));
        PanePart drawn;
        if (row_part_on(grid, part.row + body.header_rows, shown, body.columns,
                        content->parts, owners.at(part.row), i, space, drawn)) {
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
        glyph_point(GlyphGrid{place.x, place.y, drawn.advance, place.h}, 0, asked.column,
                    reply.space, reply.x, reply.y);
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
