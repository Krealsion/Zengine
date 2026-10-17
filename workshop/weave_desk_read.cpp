// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s desk read: what is on the screen, said by message -- the desk by its own
// numbers, each presented pane's words and parts where the medium draws them, a reading paged to
// one reply under the stamp it continues, the point a press on one character or cell lands on, and
// the notice that the desk moved. The arithmetic each answer spends is desk_read.hpp's.
// Workshop law: agents/workshop/desk-read.md (+3 registers; agents/workshop.md routes)

#include "weave.hpp"
#include "decoded_cells.hpp"
#include "desk_read.hpp"
#include "reply_bytes.hpp"
#include "screen_canvas.hpp"
#include "pane_canvas_rows.hpp"

#include <zen/serialize.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

using namespace desk_read;

namespace {

/// A CANVAS PICTURE THAT TAKES NO PRESS (`canvas_takes_press`) -- the one a managed opening showed,
/// numbered none, or one its office's holder no longer holds -- gives no point: a press is taken
/// from its pane's own next picture (WL-OPEN-10), so a point is read again.
constexpr std::string_view kTakesNoPress =
    "pane point unavailable: the pane's picture moved; read it again -- the one shown takes no press "
    "until its pane draws its own";

constexpr std::string_view kPageStale =
    "pane view stale: the pane no longer stands on the stamp this page continues -- its holder, "
    "incarnation, room or picture moved; read it again from the start";

} // namespace

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
        // Where the painter draws the picture, which the grant's room must still be.
        out.canvas_body = shown_canvas_place(content->canvas, bounds.rect, sc, titles);
        const auto& c = content->canvas;
        if (out.canvas_body.empty() || out.canvas_body.x != c.x || out.canvas_body.y != c.y ||
            out.canvas_body.w != c.width || out.canvas_body.h != c.height) {
            return "pane view unavailable: no settled picture";
        }
        // Workshop's mark over a refused picture, where no title row carries it, covers what the
        // picture says there: the view is refused, as for a pane another covers.
        if (!refused_mark_cover(*content, out.canvas_body, sc, titles > 0 && !c.title_waits).empty()) {
            return "pane view unavailable: Workshop's mark over its refused picture covers part of it";
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
    if (const std::string withheld = read_withheld(mail, mail.sender(), asked.provider, asked.pane);
        !withheld.empty()) {
        (void)mail.answer(loom::Refused{"pane view withheld: " + withheld});
        return;
    }
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
    if (const std::string withheld = read_withheld(mail, mail.sender(), asked.provider, asked.pane);
        !withheld.empty()) {
        (void)mail.answer(loom::Refused{"pane view withheld: " + withheld});
        return;
    }
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
    // THE DESK LAID OUT ONCE, back to front: a pane's place, its state and what covers it are
    // judged from these bounds as `pane_state_of` judges them, never laying the desk out again.
    std::vector<PaneBounds> laid;
    for (const std::int64_t kind : order) laid.push_back(bounds_of(panes, setup, kind, sc));
    const auto at_of = [&](std::int64_t kind) {
        return static_cast<std::size_t>(std::find(order.begin(), order.end(), kind) - order.begin());
    };
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
        const bool presented = row.kind != kNoPaneKind && panes.has(row.kind);
        const std::size_t at = presented ? at_of(row.kind) : order.size();
        const PaneBounds where = at < order.size() ? laid[at]
                                 : presented        ? bounds_of(panes, setup, row.kind, sc)
                                                    : PaneBounds{};
        std::int64_t state = pane_state_of(panes, setup, sc, row, false, presented ? &where : nullptr);
        if (state == pane_state::kOpen && at < order.size()) {
            std::vector<PixelRect> ahead;
            for (std::size_t i = at + 1; i < order.size(); ++i) {
                if (laid[i].rect.w > 0 && laid[i].rect.h > 0) ahead.push_back(laid[i].rect);
            }
            if (rects_cover(where.rect, ahead)) state = pane_state::kCovered;
        }
        pane.state = pane_state_word(state);
        if (presented) {
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

// WHAT A VISIBLE BODY SHOWS, word by word: a text pane's rows under its header, a canvas pane's
// labels and then its text runs, each where the medium draws it. A word the body does not draw
// is not said, so a word's number is its place in this list and nowhere else. `glyphs` takes
// where each word's glyphs stand, which a point inside it is measured by. A picture that takes no
// press says each word with its place and no point.
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
    for (const v2::PaneCanvasText& run : picture.texts) {
        const auto placed = clip_canvas_run(run, {0, 0, body.w, body.h}, room);
        if (!placed.visible()) continue;
        const surface::SurfaceTextRegion region = canvas_text_region(placed, body.x, body.y);
        const surface::RegionFit fit = surface::fit_region(region.x, region.y, region.w, region.h,
                                                           sc.text_advance_px, sc.text_line_px);
        // A run's word is its characters, without the blanks after the last, as a row's is: a
        // run may be blank to the row's end to carry a ground there, as a row is padded.
        std::string text = without_trailing_blanks(placed.text.text);
        if (text.empty() && placed.text.caret_col < 0) continue;
        keep(glyph_grid(fit), std::move(text), 0);
    }
    if (!canvas_takes_press(visible.kind)) {
        for (PaneWord& w : out) no_point(w);
    }
    return out;
}

void WorkshopWeave::on(const v2::PaneViewRequested& asked, loom::Mail& mail) {
    if (const std::string withheld = read_withheld(mail, mail.sender(), asked.provider, asked.pane);
        !withheld.empty()) {
        (void)mail.answer(loom::Refused{"pane view withheld: " + withheld});
        return;
    }
    VisibleBody visible;
    if (const auto why = visible_body(asked.provider, asked.pane, visible, true); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    (void)mail.answer(v2::PaneView{asked.provider, asked.pane, visible.content->stamp.aimed,
                                   visible.canvas, visible_words(visible)});
}

// THE PARTS A VISIBLE BODY'S PANE NAMES, each where the medium draws it, as the pane named it: a
// part the body does not show is not said, and neither is a place the pane names nothing. A
// picture that takes no press says each part with its place and no point.
// WL-HAND-06 -- agents/workshop/pane-parts.md
std::vector<v2::PanePart> WorkshopWeave::visible_parts(const VisibleBody& visible,
                                                       const std::vector<PaneWord>& words) const {
    const std::int64_t space = input_space_of(screen_of(session_));
    std::vector<v2::PanePart> out;
    const auto* content = visible.content;
    if (visible.canvas) {
        const std::vector<v2::PaneCanvasPart>& parts = content->canvas.content.parts;
        const std::int64_t unit = space == input::space::kPixels ? 1 : surface::kCanvasCellPx;
        std::vector<UnitBox> boxes;
        boxes.reserve(parts.size());
        for (const v2::PaneCanvasPart& part : parts) {
            boxes.push_back(units_of(visible.canvas_body, part, unit));
        }
        const std::vector<std::optional<UnitPoint>> points = own_points(boxes);
        const bool pressed = canvas_takes_press(visible.kind);
        for (std::size_t i = 0; i < parts.size(); ++i) {
            v2::PanePart drawn;
            if (!parts[i].name.empty() && canvas_part_on(visible.canvas_body, parts[i], boxes[i],
                                                         points[i], words, space, drawn)) {
                if (!pressed) no_point(drawn);
                out.push_back(std::move(drawn));
            }
        }
        return out;
    }
    const auto& body = visible.body;
    const GlyphGrid grid = glyph_grid(body.fit);
    const std::map<std::int64_t, std::vector<std::size_t>> owners = row_owners(content->parts);
    for (std::size_t i = 0; i < content->parts.size(); ++i) {
        const v2::PaneRowPart& part = content->parts[i];
        if (part.name.empty() || part.row >= body.rows ||
            part.row >= static_cast<std::int64_t>(content->shown.size())) {
            continue;
        }
        // The row as `visible_words` reads it: cut to the body's columns.
        const std::string shown = content->shown[static_cast<std::size_t>(part.row)].text.substr(
            0, static_cast<std::size_t>(body.columns));
        v2::PanePart drawn;
        if (row_part_on(grid, part.row + body.header_rows, shown, body.columns,
                        content->parts, owners.at(part.row), i, space, drawn)) {
            out.push_back(std::move(drawn));
        }
    }
    return out;
}

void WorkshopWeave::on(const v3::PaneViewRequested& asked, loom::Mail& mail) {
    if (const std::string withheld = read_withheld(mail, mail.sender(), asked.provider, asked.pane);
        !withheld.empty()) {
        (void)mail.answer(loom::Refused{"pane view withheld: " + withheld});
        return;
    }
    VisibleBody visible;
    if (const auto why = visible_body(asked.provider, asked.pane, visible, true); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    std::vector<PaneWord> words = visible_words(visible);
    std::vector<PanePart> parts = first_parts(visible_parts(visible, words));
    (void)mail.answer(v3::PaneView{asked.provider, asked.pane, visible.content->stamp.aimed,
                                   visible.canvas, std::move(words), std::move(parts)});
}

// ---- THE DESK, SAID WHOLE --------------------------------------------------------------------

bool WorkshopWeave::capture_guest(loom::WeaveId who) const {
    if (!host_->guest_row) return false;
    const scope::GuestRowFacts row = host_->guest_row(who);
    return row.admitted && row.may(scope::kPowerCapture);
}

PaneStamp WorkshopWeave::stamp_of(const std::string& provider, const std::string& pane,
                                  std::int64_t grant, std::int64_t picture) const {
    PaneStamp out{provider, pane, 0, 0, grant, picture};
    if (host_->role_holder) {
        const loom::WeaveId holder = host_->role_holder(provider);
        out.holder = static_cast<std::int64_t>(holder.value);
        if (holder.valid() && host_->incarnation_of) out.incarnation = host_->incarnation_of(holder);
    }
    return out;
}

// WHAT IS DRAWN OVER A PRESENTED PANE: arranging over all of it, an open menu, each pane in front
// of it, and the band where the pane stands outside the room. A canvas pane's refused mark is its
// own reading's to add.
// WL-READ-05 -- agents/workshop/desk-read.md
std::vector<WorkshopWeave::Cover> WorkshopWeave::covers_of(std::int64_t kind,
                                                           const PixelRect& rect) const {
    const Screen sc = screen_of(session_);
    std::vector<Cover> out;
    if (session_.arrange.open) out.push_back(Cover{"arranging", rect});
    if (session_.presented.open) out.push_back(Cover{"menu", presented_bounds(session_, sc)});
    else if (session_.context.open) out.push_back(Cover{"menu", context_bounds(session_, sc)});
    const std::vector<CatalogRow> rows = inventory_rows(session_.setup.active, session_.panes);
    bool above = false;
    for (const auto other : effective_pane_order(session_.setup.active, session_.panes)) {
        if (other == kind) {
            above = true;
            continue;
        }
        if (!above) continue;
        const auto bounds = bounds_of(session_.panes, session_.setup.active, other, sc);
        if (!bounds.open) continue;
        std::string name;
        for (const CatalogRow& row : rows) {
            if (row.kind == other) name = row.name;
        }
        out.push_back(Cover{name, bounds.rect});
    }
    // Layouts is drawn in the top band, its own place there; the foot band is drawn over it as
    // over every pane.
    if (kind != pane_kind::kLayouts && rect.y < sc.room_y) {
        out.push_back(Cover{"band", PixelRect{rect.x, rect.y, rect.w, sc.room_y - rect.y}});
    }
    if (rect.y + rect.h > sc.notice_y) {
        out.push_back(Cover{"band", PixelRect{rect.x, sc.notice_y, rect.w, rect.y + rect.h - sc.notice_y}});
    }
    return out;
}

// ONE PANE'S WHOLE READING: its stamp first, then, while its newest picture is still in flight --
// what it shows not yet what the stamp names, or a room out that no picture has answered -- that
// and no word; else its words and parts, a covered one not said, the cover named, and none given a
// point while a menu is open. The fourth version judges in flight by numbers (`by_numbers`).
// WL-READ-02 -- agents/workshop/desk-read.md; WL-READ-07 -- agents/workshop/desk-read.md
std::string WorkshopWeave::pane_reading(const std::string& provider, const std::string& pane_key,
                                        v6::PaneView& out, bool by_numbers) const {
    out = v6::PaneView{};
    out.provider = provider;
    out.pane = pane_key;
    if (provider == kWorkshopProvider && pane_key == pane_key::kLayouts) return layouts_reading(out);
    const auto* pane = session_.panes.runtime.find(provider, pane_key);
    if (pane == nullptr || !session_.panes.has(pane->kind)) {
        return "pane view unavailable: closed or unknown";
    }
    const auto* content = session_.panes.external_pane(pane->kind);
    if (content == nullptr || (!content->heard && !content->awaiting)) {
        return "pane view unavailable: the pane has drawn nothing yet";
    }
    out.canvas = content->canvas.heard;
    const PaneStamp stamp = stamp_of(provider, pane_key, out.canvas ? content->canvas.grant : 0,
                                     content->stamp.aimed);
    out.picture = stamp.picture;
    out.holder = stamp.holder;
    out.incarnation = stamp.incarnation;
    out.grant = stamp.grant;
    out.fingerprint = stamp_fingerprint(*content);
    const Screen sc = screen_of(session_);
    const auto bounds = bounds_of(session_.panes, session_.setup.active, pane->kind, sc);
    if (!bounds.open) return "pane view unavailable: the pane is not presented";
    const std::int64_t titles = external_title_rows(session_.panes, pane->kind, session_.pane_titles);
    VisibleBody visible;
    visible.kind = pane->kind;
    visible.content = content;
    visible.body = external_body_place(bounds.rect, sc, titles);
    visible.canvas = out.canvas;
    if (!visible.body.present) return "pane has no visible body";
    // A REFUSED UPDATE IS SAID IN WORDS, as the pane paints it, where nothing of it stands or the
    // picture refused is the one a new room asked for; a refusal from before the new room leaves
    // the picture it carries as a preview, a picture in flight.
    if (!content->heard && !content->refusal.empty() &&
        (!content->canvas.preview || content->refused_grant == content->canvas.grant)) {
        return "pane view unavailable: Workshop refused the pane's last update and kept none of it";
    }
    // A ROOM OUT THAT NO HOLDER CAN ANSWER is not in flight: the pane waits for its provider.
    if (!content->heard && host_->role_holder && !host_->role_holder(provider).valid()) {
        return "pane view unavailable: waiting for the provider -- no participant holds its office";
    }
    // IN FLIGHT WHILE WHAT IS SHOWN IS NOT YET WHAT THE STAMP NAMES: a picture sent again unchanged
    // under its next number is read at once. The fourth version judges by numbers, as it always has.
    if (content->awaiting || content->canvas.preview ||
        (by_numbers ? content->picture != content->stamp.aimed
                    : picture_fingerprint(*content) != content->stamp.aimed_fingerprint)) {
        out.in_flight = true;
        return {};
    }
    std::vector<Cover> covers = covers_of(pane->kind, bounds.rect);
    if (out.canvas) {
        const auto& c = content->canvas;
        visible.canvas_body = shown_canvas_place(c, bounds.rect, sc, titles);
        if (visible.canvas_body.empty() || visible.canvas_body.x != c.x ||
            visible.canvas_body.y != c.y || visible.canvas_body.w != c.width ||
            visible.canvas_body.h != c.height) {
            out.in_flight = true; // a new room, its picture not drawn yet
            return {};
        }
        const PixelRect mark =
            refused_mark_cover(*content, visible.canvas_body, sc, titles > 0 && !c.title_waits);
        if (!mark.empty()) covers.push_back(Cover{"refused mark", mark});
    }
    std::vector<PaneWord> words = visible_words(visible);
    std::vector<v2::PanePart> parts = visible_parts(visible, words);
    cover_reading(covers, bounds.rect, words, parts, out.covered);
    if (session_.presented.open || session_.context.open) without_points(words, parts);
    out.total = static_cast<std::int64_t>(words.size() + parts.size());
    out.words = std::move(words);
    out.parts = std::move(parts);
    return {};
}

// LAYOUTS, READ AS IT IS PAINTED: its rows as words, and its tabs and `+` as parts Workshop names
// itself -- `layout:<name>` and `layout:+` -- as it names its menu's lines. Its picture moves when
// what it says moves.
// WL-READ-04 -- agents/workshop/desk-read.md
std::string WorkshopWeave::layouts_reading(v6::PaneView& out) const {
    const std::int64_t kind = pane_kind::kLayouts;
    if (!session_.panes.has(kind)) return "pane view unavailable: closed or unknown";
    const Screen sc = screen_of(session_);
    const auto bounds = bounds_of(session_.panes, session_.setup.active, kind, sc);
    if (!bounds.open) return "pane view unavailable: the pane is not presented";
    surface::SurfaceLayer layer;
    paint_layouts(layer, session_, bounds.rect, sc, kPaneChrome);
    if (layer.texts.empty()) return "pane has no visible body";
    const ExternalBodyPlace place = external_body_place(bounds.rect, sc, 0);
    const GlyphGrid grid = glyph_grid(place.fit);
    const std::int64_t space = input_space_of(sc);
    const surface::SurfaceTextRegion& region = layer.texts.back();
    std::vector<PaneWord> words;
    for (std::size_t i = 0; i < region.rows.size(); ++i) {
        PaneWord w = word_on(grid, static_cast<std::int64_t>(i), 0,
                             without_trailing_blanks(region.rows[i].text), space);
        w.word = static_cast<std::int64_t>(words.size());
        words.push_back(std::move(w));
    }
    std::vector<v2::PanePart> parts;
    if (!session_.setup.naming.open && !region.rows.empty()) {
        const BandStatus status = band_status(session_, place);
        const std::vector<std::string> names = layout_part_names(session_.setup);
        std::vector<PaneRowPart> named;
        for (const LayoutTab& tab : status.tabs) {
            if (tab.at < names.size()) named.push_back(PaneRowPart{names[tab.at], 0, tab.column, tab.columns});
        }
        if (status.create_columns > 0) {
            named.push_back(PaneRowPart{"layout:+", 0, status.create_column, status.create_columns});
        }
        const std::map<std::int64_t, std::vector<std::size_t>> owners = row_owners(named);
        for (std::size_t i = 0; i < named.size(); ++i) {
            v2::PanePart drawn;
            if (row_part_on(grid, 0, region.rows[0].text, place.columns, named, owners.at(0), i, space,
                            drawn)) {
                parts.push_back(std::move(drawn));
            }
        }
    }
    // THE PICTURE IS WHAT IT SAYS: a number that moves when its words or parts are said differently.
    v3::PaneView said{out.provider, out.pane, 0, false, words, first_parts(parts)};
    const std::string bytes = loom::serialize(loom::to_value(said));
    if (bytes != layouts_said_) {
        layouts_said_ = bytes;
        ++layouts_picture_;
    }
    const PaneStamp stamp = stamp_of(out.provider, out.pane, 0, layouts_picture_);
    out.picture = stamp.picture;
    out.holder = stamp.holder;
    out.incarnation = stamp.incarnation;
    out.grant = 0;
    // ...ITS FINGERPRINT WHAT IT SHOWS: what it says, and the naming line's caret and selection.
    Fingerprint said_print;
    said_print.bytes(bytes);
    for (const std::int64_t n : {region.caret_row, region.caret_col, region.sel_begin_row,
                                 region.sel_begin_col, region.sel_end_row, region.sel_end_col}) {
        said_print.number(n);
    }
    out.fingerprint = said_print.value();
    out.canvas = false;
    cover_reading(covers_of(kind, bounds.rect), bounds.rect, words, parts, out.covered);
    if (session_.presented.open || session_.context.open) without_points(words, parts);
    out.total = static_cast<std::int64_t>(words.size() + parts.size());
    out.words = std::move(words);
    out.parts = std::move(parts);
    return {};
}

// THE DESK WITH WORKSHOP'S OWN WORDS: the band's notice and legend where they are drawn, with no
// point, since the band owns no pointer space; the status slot as the medium is handed it; and
// the desk number, moved when anything said here moves.
// WL-READ-04 -- agents/workshop/desk-read.md
v3::DeskView WorkshopWeave::desk_view_v3() const {
    const v2::DeskView v2 = desk_view();
    v3::DeskView out;
    out.width = v2.width;
    out.height = v2.height;
    out.cell_px = v2.cell_px;
    out.space = v2.space;
    out.room = v2.room;
    out.panes = v2.panes;
    out.arranging = v2.arranging;
    out.menu = v2.menu;
    const Screen sc = screen_of(session_);
    const surface::SurfaceTextRegion band = band_region(session_, sc);
    const surface::RegionFit fit =
        surface::fit_region(band.x, band.y, band.w, band.h, sc.text_advance_px, sc.text_line_px);
    const GlyphGrid grid = glyph_grid(fit);
    for (std::size_t i = 0; i < band.rows.size(); ++i) {
        std::string text = without_trailing_blanks(band.rows[i].text);
        if (text.empty()) continue;
        PaneWord w = word_on(grid, static_cast<std::int64_t>(i), 0, std::move(text), out.space);
        no_point(w);
        w.word = static_cast<std::int64_t>(out.words.size());
        out.words.push_back(std::move(w));
    }
    out.slots.push_back(surface::SurfaceText{surface::kSlotStatus, status_line()});
    const std::string bytes = loom::serialize(loom::to_value(out));
    if (bytes != desk_said_) {
        desk_said_ = bytes;
        ++desk_number_;
    }
    out.desk = desk_number_;
    return out;
}

// A PAGE OF ONE PANE'S READING, under the stamp it continues: a stamp that is no longer the pane's
// is refused as stale, whatever its picture number, and the reader reads it again from the start;
// a picture in flight is said so at any page.
// WL-READ-02 -- agents/workshop/desk-read.md
void WorkshopWeave::on(const v4::PaneViewRequested& asked, loom::Mail& mail) {
    if (const std::string withheld = read_withheld(mail, mail.sender(), asked.provider, asked.pane);
        !withheld.empty()) {
        (void)mail.answer(loom::Refused{"pane view withheld: " + withheld});
        return;
    }
    v6::PaneView whole;
    if (const std::string why = pane_reading(asked.provider, asked.pane, whole, true); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    // A STAMP NAMES ITS HOLDER, so an ask naming none reads the pane as it stands.
    const bool continues = asked.from > 0 || asked.stamp.holder != 0;
    if (continues && !same_stamp(asked.stamp, whole)) {
        (void)mail.answer(loom::Refused{std::string(kPageStale)});
        return;
    }
    if (const std::string why = page_refused(asked.from, whole); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    // FILLED AS ITS OWN VERSION COUNTS, so a page holds what one decoded value and one reply of it
    // hold.
    const v4::PaneView fourth = fourth_version(whole);
    if (fourth.in_flight) {
        v4::PaneView page = fourth;
        page.from = asked.from;
        (void)mail.answer(page);
        return;
    }
    const Page<v4::PaneView> page = page_of(fourth, asked.from, kDecodedCellBudget, kReplyByteBudget);
    if (const std::string why = page_alone_refused(page, asked.from, fourth.total); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    (void)mail.answer(page.view);
}

// ...AND UNDER A STAMP NAMING THE PICTURE BY ITS FINGERPRINT: stale only when its holder,
// incarnation, room or what the pane shows moved; `fill` says the reading in the version asked.
// WL-READ-02 -- agents/workshop/desk-read.md
template <class View, class Fill>
void WorkshopWeave::answer_page(const std::string& provider, const std::string& pane,
                                std::int64_t from, const v2::PaneStamp& stamp, Fill fill,
                                loom::Mail& mail) const {
    if (const std::string withheld = read_withheld(mail, mail.sender(), provider, pane);
        !withheld.empty()) {
        (void)mail.answer(loom::Refused{"pane view withheld: " + withheld});
        return;
    }
    v6::PaneView reading;
    if (const std::string why = pane_reading(provider, pane, reading); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    const bool continues = from > 0 || stamp.holder != 0;
    if (continues && !same_stamp(stamp, fingerprint_stamp_from(reading))) {
        (void)mail.answer(loom::Refused{std::string(kPageStale)});
        return;
    }
    const View whole = fill(reading);
    if (const std::string why = page_refused(from, whole); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    if (whole.in_flight) {
        (void)mail.answer(in_flight_page(whole, from));
        return;
    }
    const Page<View> page = page_of(whole, from, kDecodedCellBudget, kReplyByteBudget);
    if (const std::string why = page_alone_refused(page, from, whole.total); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    (void)mail.answer(page.view);
}

// WL-READ-02 -- agents/workshop/desk-read.md
void WorkshopWeave::on(const v5::PaneViewRequested& asked, loom::Mail& mail) {
    answer_page<v5::PaneView>(asked.provider, asked.pane, asked.from, asked.stamp,
                              [](const v6::PaneView& v) { return fifth_version(v); }, mail);
}

// ...AND WITH EACH PART'S AVAILABILITY, as its pane said it.
// WL-READ-02 -- agents/workshop/desk-read.md; WL-CAUSE-03 -- agents/workshop/action-causes.md
void WorkshopWeave::on(const v6::PaneViewRequested& asked, loom::Mail& mail) {
    answer_page<v6::PaneView>(asked.provider, asked.pane, asked.from, asked.stamp,
                              [](const v6::PaneView& v) { return v; }, mail);
}

// AT MOST A FEW DESK READS A SECOND, EACH ASKER: every read composes the whole desk, so past the
// rate the ask is refused in words, and the asker reads again when its oldest read is a second old.
// WL-READ-03 -- agents/workshop/desk-read.md
std::string WorkshopWeave::take_desk_read(loom::WeaveId asker) {
    const std::int64_t now = interaction_now();
    for (auto it = desk_reads_.begin(); it != desk_reads_.end();) {
        std::vector<std::int64_t>& times = it->second;
        times.erase(std::remove_if(times.begin(), times.end(),
                                   [now](std::int64_t t) { return now - t >= 1000; }),
                    times.end());
        it = times.empty() ? desk_reads_.erase(it) : std::next(it);
    }
    std::vector<std::int64_t>& mine = desk_reads_[asker.value];
    if (static_cast<std::int64_t>(mine.size()) >= kDeskReadsPerSecond) {
        const std::int64_t wait = 1000 - (now - mine.front());
        return "desk read refused: at most " + std::to_string(kDeskReadsPerSecond) +
               " a second, each asker -- read again in " + std::to_string(wait) + " ms";
    }
    mine.push_back(now);
    return {};
}

// EVERY PRESENTED PANE, FRONT TO BACK, by office and pane: the order a desk read and a notice say
// their stamps in.
std::vector<PaneRef> WorkshopWeave::presented_refs() const {
    const Screen sc = screen_of(session_);
    std::vector<std::int64_t> order = effective_pane_order(session_.setup.active, session_.panes);
    std::reverse(order.begin(), order.end()); // front to back
    std::vector<PaneRef> out;
    for (const std::int64_t kind : order) {
        if (!bounds_of(session_.panes, session_.setup.active, kind, sc).open) continue;
        if (is_runtime_kind(kind)) {
            if (const RuntimePane* row = session_.panes.runtime.of_kind(kind)) {
                out.push_back(PaneRef{row->provider, row->pane});
            }
        } else if (kind == pane_kind::kLayouts) {
            out.push_back(PaneRef{kWorkshopProvider, pane_key::kLayouts});
        }
    }
    return out;
}

// THE STAMP A PANE STANDS ON NOW, ITS PICTURE BY FINGERPRINT: its holder and incarnation, the room
// its picture was drawn for, and what the picture aimed at shows -- the one stamp a desk read, a
// page and a notice all say.
// WL-READ-07 -- agents/workshop/desk-read.md
v2::PaneStamp WorkshopWeave::stamp_now(const PaneRef& ref) const {
    if (ref.provider == kWorkshopProvider && ref.pane == pane_key::kLayouts) {
        v6::PaneView layouts;
        if (pane_reading(ref.provider, ref.pane, layouts).empty()) {
            return fingerprint_stamp_from(layouts);
        }
        const PaneStamp none = stamp_of(ref.provider, ref.pane, 0, 0);
        return v2::PaneStamp{ref.provider, ref.pane, none.holder, none.incarnation, 0, 0};
    }
    const RuntimePane* row = session_.panes.runtime.find(ref.provider, ref.pane);
    const ExternalPane* content = row == nullptr ? nullptr : session_.panes.external_pane(row->kind);
    const bool canvas = content != nullptr && content->canvas.heard;
    const PaneStamp of = stamp_of(ref.provider, ref.pane, canvas ? content->canvas.grant : 0, 0);
    return v2::PaneStamp{ref.provider, ref.pane, of.holder, of.incarnation, of.grant,
                         content == nullptr ? 0 : stamp_fingerprint(*content)};
}

// THE DESK IN ONE TURN: the desk, every presented pane's stamp front to back, and as many of those
// panes' readings, in that order, as one decoded value and one reply's bytes hold; a stamp past the
// last reading names a pane its reader asks alone. Nothing is held between asks.
// WL-READ-01 -- agents/workshop/desk-read.md
void WorkshopWeave::on(const DeskReadRequested&, loom::Mail& mail) {
    if (const std::string why = take_desk_read(mail.sender()); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    DeskRead out;
    out.desk = desk_view_v3();
    if (reads_by_pane(mail.sender())) withhold_desk(out.desk);
    std::vector<std::optional<v4::PaneView>> readings;
    for (const PaneRef& ref : presented_refs()) {
        v6::PaneView reading;
        if (!read_withheld(mail, mail.sender(), ref.provider, ref.pane).empty()) {
            const v2::PaneStamp w = withheld_stamp(ref);
            out.stamps.push_back(PaneStamp{w.provider, w.pane, w.holder, 0, 0, 0});
            readings.emplace_back(std::nullopt);
        } else if (pane_reading(ref.provider, ref.pane, reading, true).empty()) {
            out.stamps.push_back(stamp_from(reading));
            readings.emplace_back(fourth_version(reading));
        } else {
            out.stamps.push_back(stamp_of(ref.provider, ref.pane, 0, 0));
            readings.emplace_back(std::nullopt);
        }
    }
    const loom::Value shell = loom::to_value(out);
    std::int64_t cells = decoded_cells(shell);
    std::int64_t bytes = reply_bytes(shell);
    for (const std::optional<v4::PaneView>& reading : readings) {
        if (!reading) continue;
        // A READING IS CARRIED WHOLE OR NOT AT ALL: whole when its first page, in the room the rest
        // leave beside its own cell and comma, holds every item.
        const Page<v4::PaneView> whole = page_of(*reading, 0, kDecodedCellBudget - cells - 1,
                                                 kReplyByteBudget - bytes - 1);
        if (items_of(whole.view) < reading->total) break;
        cells += 1 + whole.cells;
        bytes += 1 + whole.bytes;
        out.panes.push_back(*reading);
    }
    (void)mail.answer(out);
    publish_desk_stamps(mail, &out.desk);
}

// ...AND ITS STAMPS BY FINGERPRINT, the ones a notice of the same desk says.
// WL-READ-01 -- agents/workshop/desk-read.md
void WorkshopWeave::on(const v2::DeskReadRequested&, loom::Mail& mail) {
    if (const std::string why = take_desk_read(mail.sender()); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    v2::DeskRead out;
    out.desk = desk_view_v3();
    if (reads_by_pane(mail.sender())) withhold_desk(out.desk);
    std::vector<std::optional<v5::PaneView>> readings;
    for (const PaneRef& ref : presented_refs()) {
        // A PANE THE ASKER MAY NOT READ is said by its place, name and holder alone.
        if (!read_withheld(mail, mail.sender(), ref.provider, ref.pane).empty()) {
            out.stamps.push_back(withheld_stamp(ref));
            readings.emplace_back(std::nullopt);
            continue;
        }
        out.stamps.push_back(stamp_now(ref));
        v6::PaneView reading;
        if (pane_reading(ref.provider, ref.pane, reading).empty()) readings.emplace_back(fifth_version(reading));
        else readings.emplace_back(std::nullopt);
    }
    const loom::Value shell = loom::to_value(out);
    std::int64_t cells = decoded_cells(shell);
    std::int64_t bytes = reply_bytes(shell);
    for (const std::optional<v5::PaneView>& reading : readings) {
        if (!reading) continue;
        // ...EACH READING WHOLE OR NOT AT ALL, as the first version carries its own.
        const Page<v5::PaneView> whole = page_of(*reading, 0, kDecodedCellBudget - cells - 1,
                                                 kReplyByteBudget - bytes - 1);
        if (items_of(whole.view) < reading->total) break;
        cells += 1 + whole.cells;
        bytes += 1 + whole.bytes;
        out.panes.push_back(*reading);
    }
    (void)mail.answer(out);
    publish_desk_stamps(mail, &out.desk);
}

// THE PICTURE A PRESS ON A PANE IS STAMPED WITH NOW, by its own number: what a reading by number
// reads, and 0 where none is aimed at yet or the pane is Workshop's own.
// WL-READ-06 -- agents/workshop/desk-read.md
std::int64_t WorkshopWeave::aimed_now(const PaneRef& ref) const {
    const RuntimePane* row = session_.panes.runtime.find(ref.provider, ref.pane);
    const ExternalPane* content =
        row == nullptr ? nullptr : session_.panes.external_pane(row->kind);
    return content == nullptr ? 0 : content->stamp.aimed;
}

// THE DESK MOVED: its number, every presented pane's stamp and the picture each is aimed at,
// published as the office when any differs from what was last published, so the desk number never
// moves without a notice and an unchanged desk sends none; the first version, naming no aimed
// picture, only when its number or a stamp moved.
// WL-READ-06 -- agents/workshop/desk-read.md
void WorkshopWeave::publish_desk_stamps(loom::Mail& mail, const v3::DeskView* known) {
    v2::DeskStamps now;
    now.desk = known != nullptr ? known->desk : desk_owed_ ? desk_view_v3().desk : desk_number_;
    desk_owed_ = false;
    DeskStamps numbered;
    for (const PaneRef& ref : presented_refs()) {
        now.panes.push_back(v2::StampedPane{stamp_now(ref), aimed_now(ref)});
        numbered.panes.push_back(now.panes.back().stamp);
    }
    numbered.desk = now.desk;
    if (stamps_published_ && now.desk == stamps_said_.desk &&
        same_panes(now.panes, stamps_said_.panes)) {
        return;
    }
    if (!numbered_published_ || now.desk != numbered_said_.desk ||
        !same_stamps(numbered.panes, numbered_said_.panes)) {
        if (mail.as_role(kWorkshopProvider).publish(numbered).authored) {
            numbered_said_ = std::move(numbered);
            numbered_published_ = true;
        }
    }
    if (!mail.as_role(kWorkshopProvider).publish(now).authored) return;
    stamps_said_ = std::move(now);
    stamps_published_ = true;
}

// WHERE ONE CHARACTER OF ONE WORD IS NOW, measured as the word was and checked by resolving it: a
// text row through the press measurer, a canvas word against the body its press lands in.
void WorkshopWeave::on(const v2::PanePointRequested& asked, loom::Mail& mail) {
    if (const std::string withheld = read_withheld(mail, mail.sender(), asked.provider, asked.pane);
        !withheld.empty()) {
        (void)mail.answer(loom::Refused{"pane view withheld: " + withheld});
        return;
    }
    VisibleBody visible;
    if (const auto why = visible_body(asked.provider, asked.pane, visible, true); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    if (asked.picture != visible.content->stamp.aimed) {
        (void)mail.answer(loom::Refused{"pane point unavailable: the pane's picture moved; read it again"});
        return;
    }
    if (visible.canvas && !canvas_takes_press(visible.kind)) {
        (void)mail.answer(loom::Refused{std::string(kTakesNoPress)});
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

// WHERE ONE CELL OF A PANE'S TEXT LATTICE IS NOW: a text pane's painted cell, as the first version
// answers, or a canvas pane's cell of the lattice its room's text stands on, wherever the picture
// draws or leaves it blank -- checked by reading the point back to that cell of the room it lands in.
void WorkshopWeave::on(const v3::PanePointRequested& asked, loom::Mail& mail) {
    if (const std::string withheld = read_withheld(mail, mail.sender(), asked.provider, asked.pane);
        !withheld.empty()) {
        (void)mail.answer(loom::Refused{"pane view withheld: " + withheld});
        return;
    }
    VisibleBody visible;
    if (const auto why = visible_body(asked.provider, asked.pane, visible, true); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    if (asked.picture != visible.content->stamp.aimed) {
        (void)mail.answer(loom::Refused{"pane point unavailable: the pane's picture moved; read it again"});
        return;
    }
    if (visible.canvas && !canvas_takes_press(visible.kind)) {
        (void)mail.answer(loom::Refused{std::string(kTakesNoPress)});
        return;
    }
    PanePoint reply{asked.provider, asked.pane, asked.picture, asked.row, asked.column, 0, 0, 0};
    if (!visible.canvas) {
        if (asked.row < 0 || asked.column < 0 || asked.row >= visible.body.rows ||
            asked.column >= visible.body.columns ||
            asked.row >= static_cast<std::int64_t>(visible.content->shown.size())) {
            (void)mail.answer(loom::Refused{"pane point unavailable: outside the pane's visible text"});
            return;
        }
        if (!cell_center(visible, asked.row, asked.column, reply.x, reply.y, reply.space)) {
            (void)mail.answer(loom::Refused{"pane point unavailable: that cell is not addressable"});
            return;
        }
        (void)mail.answer(reply);
        return;
    }
    const PixelRect& body = visible.canvas_body;
    const ExternalPane::Canvas& c = visible.content->canvas;
    const CanvasRows lattice = canvas_rows(PaneCanvasRoom{asked.pane, c.grant, body.w, body.h, c.grain,
                                                          c.graphical, c.text_advance_px,
                                                          c.text_line_px});
    if (asked.row < 0 || asked.column < 0 || asked.row >= lattice.rows ||
        asked.column >= lattice.columns) {
        (void)mail.answer(loom::Refused{"pane point unavailable: outside the pane's text lattice"});
        return;
    }
    reply.space = input_space_of(screen_of(session_));
    glyph_point(GlyphGrid{surface::add_cells(body.x, lattice.x), surface::add_cells(body.y, lattice.y),
                          lattice.advance, lattice.line},
                asked.row, asked.column, reply.space, reply.x, reply.y);
    const PointedAt at = canvas_point_of(reply.space, reply.x, reply.y);
    const RowCell cell = row_cell_at(lattice, surface::sub_px(at.px.x, body.x), surface::sub_px(at.px.y, body.y));
    if (!at.understood || !body.contains_at(at.px.x, at.px.y, at.grain) || !cell.shown ||
        cell.row != asked.row || cell.column != asked.column) {
        (void)mail.answer(loom::Refused{"pane point unavailable: that cell is not addressable"});
        return;
    }
    (void)mail.answer(reply);
}

} // namespace zengine::workshop
