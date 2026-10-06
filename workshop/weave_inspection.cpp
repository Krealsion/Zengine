// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s inspection seam: a pane an inspector names, the picture of its rows, and one
// write through the rows' owner.
// Workshop law: agents/workshop/info-body.md (agents/workshop.md routes)

#include "weave.hpp"
#include "screen_canvas.hpp"

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

/// THE MENU ON THE SCREEN, read from the painter's own composition of it: Workshop's contextual
/// menu, or a pane's as its presenter showed it. At most one is open.
DeskMenu desk_menu(const Session& s, const Screen& sc) {
    DeskMenu menu;
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
    const GlyphGrid grid = glyph_grid(prose_place(bounds, sc).fit);
    const surface::SurfaceTextRegion& region = layer.texts.back();
    const std::int64_t space = input_space_of(sc);
    for (std::size_t i = 0; i < region.rows.size(); ++i) {
        std::string text = without_trailing_blanks(region.rows[i].text);
        const std::int64_t row = static_cast<std::int64_t>(i);
        PaneWord line = word_on(grid, row, 0, std::move(text), -1, space);
        line.word = row;
        menu.lines.push_back(std::move(line));
    }
    return menu;
}

} // namespace

// THE DESK, BY ITS OWN NUMBERS: every pane the desk names, in its order, with what Workshop
// resolves for it now; nothing is read off a picture.
// WL-GEO-13 -- agents/workshop/geometry.md
DeskView WorkshopWeave::desk_view() const {
    const Screen sc = screen_of(session_);
    const Setup& setup = session_.setup.active;
    const Panes& panes = session_.panes;
    DeskView out;
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

void WorkshopWeave::on(const DeskViewRequested&, loom::Mail& mail) {
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
