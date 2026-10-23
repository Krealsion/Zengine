// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s desk read: the desk by its own numbers and words, each presented pane's words
// and parts where the weaver sees them, paged under the stamp a reading continues, the point a
// press on one character lands on, and the notice that the desk moved. A pane is read from the
// window that shows its area (`graphics::AreaShowAsked`), its asker answered once the window has.
// The arithmetic is desk_read.hpp's.
// Workshop law: agents/workshop/desk-read.md (+3 registers; agents/workshop.md routes)

#include "weave.hpp"
#include "decoded_cells.hpp"
#include "desk_read.hpp"
#include "fingerprint.hpp"
#include "reply_bytes.hpp"

#include <zen/serialize.hpp>

#include <algorithm>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace zengine::workshop {

using namespace desk_read;

namespace {

constexpr std::string_view kPageStale =
    "pane view stale: the pane no longer stands on the stamp this page continues -- its holder, "
    "incarnation, window or what it shows moved; read it again from the start";

constexpr std::string_view kPointStale =
    "pane point unavailable: what the pane shows moved; read it again";

/// The steady clock's milliseconds: how long a wait has waited, whatever clock a host reads its
/// interactions by.
std::int64_t wait_clock_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

template <class T>
void answer_now(loom::Mail& mail, const Said<T>& said) {
    std::visit([&](const auto& v) { (void)mail.answer(v); }, said);
}

template <class T>
void answer_later(loom::DeferredAnswer& due, loom::Mail& mail, const Said<T>& said) {
    std::visit([&](const auto& v) { (void)loom::answer_deferred(due, mail, v); }, said);
}

/// THE FINGERPRINT OF WHAT A WINDOW SAID AN AREA SHOWS: its words and parts, each with its place
/// and point, and whether its owner owes a drawing -- not its version, which moves with any area
/// that meets it.
std::int64_t shown_fingerprint(const graphics::AreaShown& s) {
    Fingerprint f;
    f.number(s.waiting ? 1 : 0);
    f.number(static_cast<std::int64_t>(s.words.size()));
    for (const graphics::AreaWord& w : s.words) {
        f.number(w.x);
        f.number(w.y);
        f.bytes(w.text);
    }
    f.number(static_cast<std::int64_t>(s.parts.size()));
    for (const graphics::AreaPart& p : s.parts) {
        f.bytes(p.name);
        for (const std::int64_t n : {std::int64_t{p.available}, p.x, p.y, p.w, p.h,
                                     std::int64_t{p.pressed}, p.px, p.py}) {
            f.number(n);
        }
    }
    return f.value();
}

/// WHAT A PANE SHOWS, AND WHERE: the fingerprint of what it shows folded with the place it shows
/// in, so a pane moved or resized reads as another picture even where it shows the same; none for a
/// picture no press is stamped with.
std::int64_t placed_fingerprint(std::int64_t shows, const DeskRect& at) {
    if (shows == 0) return 0;
    Fingerprint f;
    for (const std::int64_t n : {shows, at.x, at.y, at.w, at.h}) f.number(n);
    return f.value();
}

} // namespace

// ---- THE DESK, AS WORKSHOP HOLDS IT ---------------------------------------------------------------

// THE DESK, BY ITS OWN NUMBERS AND WORDS: every pane the desk names, in its order, with what
// Workshop resolves for it now; the menu; the band's notice and legend where they are drawn, with
// no point, since the band owns no pointer space; and the desk number, moved when anything said
// here moves. Nothing is read off a pane's picture.
// WL-READ-04 -- agents/workshop/desk-read.md
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
    if (session_.presented.open) presented_menu_reading(out.menu);
    const surface::SurfaceTextRegion band = band_region(session_, sc);
    const RegionFit fit =
        fit_region(band.x, band.y, band.w, band.h, sc.text_advance_px, sc.text_line_px);
    const GlyphGrid grid = glyph_grid(fit, session_.console_rows_above);
    for (std::size_t i = 0; i < band.rows.size(); ++i) {
        std::string text = without_trailing_blanks(band.rows[i].text);
        if (text.empty()) continue;
        PaneWord w = word_on(grid, static_cast<std::int64_t>(i), 0, std::move(text), out.space);
        no_point(w);
        w.word = static_cast<std::int64_t>(out.words.size());
        out.words.push_back(std::move(w));
    }
    const std::string bytes = loom::serialize(loom::to_value(out));
    if (bytes != desk_said_) {
        desk_said_ = bytes;
        ++desk_number_;
    }
    out.desk = desk_number_;
    return out;
}

// ---- A PANE'S READING -----------------------------------------------------------------------------

bool WorkshopWeave::capture_guest(loom::WeaveId who) const {
    if (!host_->guest_row) return false;
    const scope::GuestRowFacts row = host_->guest_row(who);
    return row.admitted && row.may(scope::kPowerCapture);
}

// THE STAMP A PANE OF `provider` STANDS ON, but for what it shows: its office's holder and that
// holder's incarnation, and the incarnation of the window that shows the desk.
PaneStamp WorkshopWeave::stamp_of(const std::string& provider, const std::string& pane) const {
    PaneStamp out{provider, pane, 0, 0, window_epoch_, 0};
    if (host_->role_holder) {
        const loom::WeaveId holder = host_->role_holder(provider);
        out.holder = static_cast<std::int64_t>(holder.value);
        if (holder.valid() && host_->incarnation_of) out.incarnation = host_->incarnation_of(holder);
    }
    return out;
}

// THE AREA A PANE DRAWS IN, when it draws in one Workshop placed; else none.
std::string WorkshopWeave::drawn_area(const PaneRef& ref) const {
    const RuntimePane* row = session_.panes.runtime.find(ref.provider, ref.pane);
    if (row == nullptr || placed_kinds_.count(row->kind) == 0) return {};
    return pane_window_area(session_.panes, *row);
}

// WHAT IS DRAWN OVER A PRESENTED PANE: arranging over all of it, an open menu, each pane in front
// of it, and the band where the pane stands outside the room.
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

// ONE PANE'S WHOLE READING: its stamp first, then, while what it shows is still in flight, that and
// no word; else its words and parts, a covered one not said, the cover named, and none given a
// point while a menu is open. Read from the window that shows its area, and from Workshop's own
// drawing for Layouts; a pane no window shows is no reading, and says why.
// WL-READ-02 -- agents/workshop/desk-read.md; WL-READ-07 -- agents/workshop/desk-read.md
std::string WorkshopWeave::pane_reading(const std::string& provider, const std::string& pane_key,
                                        PaneView& out) const {
    out = PaneView{};
    out.stamp = stamp_of(provider, pane_key);
    if (provider == kWorkshopProvider && pane_key == pane_key::kLayouts) return layouts_reading(out);
    const auto* pane = session_.panes.runtime.find(provider, pane_key);
    if (pane == nullptr || !session_.panes.has(pane->kind)) {
        return "pane view unavailable: closed or unknown";
    }
    if (placed_kinds_.count(pane->kind) != 0) return area_reading(*pane, out);
    return unshown_reading(*pane);
}

// A PANE THAT DRAWS IN AN AREA OF ITS OWN, AS THE WINDOW LAST SAID IT SHOWS: the window's words
// and parts in the desk's pixels, the stamp naming what the window said, in flight while the
// window waits for the pane's drawing, and Workshop's covers by its own order.
// WL-READ-08 -- agents/workshop/desk-read.md; WL-READ-05 -- agents/workshop/desk-read.md
std::string WorkshopWeave::area_reading(const RuntimePane& row, PaneView& out) const {
    const std::string area = pane_window_area(session_.panes, row);
    const auto placed = areas_placed_.find(area);
    if (placed == areas_placed_.end()) {
        return "pane view unavailable: the pane is not placed in a window";
    }
    const auto taken = area_readings_.find(area);
    if (taken == area_readings_.end() || taken->second.number == 0) {
        out.in_flight = true; // asked of the window, and not yet answered
        return {};
    }
    if (!taken->second.refused.empty()) {
        return "pane view unavailable: the window refused its reading: " + taken->second.refused;
    }
    const graphics::AreaShown& shown = taken->second.shown;
    const PixelRect rect{placed->second.x, placed->second.y, placed->second.w, placed->second.h};
    const std::int64_t advance = shown.advance > 0 ? shown.advance : graphics::kCellPx;
    const std::int64_t line = shown.line > 0 ? shown.line : graphics::kCellPx;
    out.stamp.epoch = shown.epoch;
    out.area = desk_rect(rect);
    out.stamp.fingerprint = placed_fingerprint(shown_fingerprint(shown), out.area);
    out.advance = advance;
    out.line = line;
    if (shown.waiting) {
        out.in_flight = true;
        return {};
    }
    std::vector<PaneWord> words;
    std::vector<PanePart> parts;
    shown_words_and_parts(shown, rect, words, parts);
    cover_reading(covers_of(row.kind, rect), rect, words, parts, out.covered);
    // NO POINT WHILE A MENU IS OPEN, nor while nobody holds the office a press there is told.
    const bool unheld = host_->role_holder && !host_->role_holder(row.provider).valid();
    if (session_.presented.open || session_.context.open || unheld) without_points(words, parts);
    out.total = static_cast<std::int64_t>(words.size() + parts.size());
    out.words = std::move(words);
    out.parts = std::move(parts);
    return {};
}

// EVERY POINT IN THE SPACE THE MEDIUM READS, as Workshop's own words say theirs: the window's
// pixel, or the terminal window's cell; a part's where the window's own judge lands a press on it.
void WorkshopWeave::shown_words_and_parts(const graphics::AreaShown& shown, const PixelRect& rect,
                                          std::vector<PaneWord>& words,
                                          std::vector<PanePart>& parts) const {
    const std::int64_t advance = shown.advance > 0 ? shown.advance : graphics::kCellPx;
    const std::int64_t line = shown.line > 0 ? shown.line : graphics::kCellPx;
    const std::int64_t space = input_space_of(screen_of(session_));
    const std::int64_t rows_above = session_.console_rows_above;
    for (std::size_t i = 0; i < shown.words.size(); ++i) {
        const graphics::AreaWord& said = shown.words[i];
        const auto bytes = static_cast<std::int64_t>(said.text.size());
        PaneWord w;
        w.word = static_cast<std::int64_t>(i);
        w.text = said.text;
        w.place = DeskRect{rect.x + said.x, rect.y + said.y, bytes * advance, line};
        glyph_point(GlyphGrid{w.place.x, w.place.y, advance, line, rows_above}, 0,
                    bytes > 0 ? (bytes - 1) / 2 : 0, space, w.x, w.y);
        w.space = space;
        words.push_back(std::move(w));
    }
    for (const graphics::AreaPart& said : shown.parts) {
        PanePart p;
        p.name = said.name;
        p.available = said.available;
        p.place = DeskRect{rect.x + said.x, rect.y + said.y, said.w, said.h};
        p.text = words_inside(words, p.place);
        if (said.pressed) {
            const std::int64_t px = rect.x + said.px, py = rect.y + said.py;
            p.x = space == input::space::kPixels ? px : graphics::cell_of_pixel(px);
            p.y = space == input::space::kPixels ? py : graphics::cell_of_pixel(py) + rows_above;
            p.space = space;
        } else {
            no_point(p);
        }
        parts.push_back(std::move(p));
    }
}

std::string WorkshopWeave::presented_area() const {
    if (!session_.presented.open || areas_placed_.count(session_.presented.area) == 0) {
        return {};
    }
    return session_.presented.area;
}

// A PANE'S MENU, READ FROM THE WINDOW: what the window last said its presenter's area shows, in the
// desk's pixels -- each line a word numbered by its line, each row's line a part named by the row's
// id, at the point the window lands a press on it. None while the window has said nothing, or the
// presenter owes it a drawing.
// WL-READ-08 -- agents/workshop/desk-read.md
void WorkshopWeave::presented_menu_reading(DeskMenu& menu) const {
    const std::string area = presented_area();
    const auto taken = area.empty() ? area_readings_.end() : area_readings_.find(area);
    if (taken == area_readings_.end() || taken->second.number == 0 ||
        !taken->second.refused.empty() || taken->second.shown.waiting) {
        return;
    }
    const graphics::AreaPlaced& placed = areas_placed_.at(area);
    const PixelRect rect{placed.x, placed.y, placed.w, placed.h};
    const graphics::AreaShown& shown = taken->second.shown;
    const std::int64_t line = shown.line > 0 ? shown.line : graphics::kCellPx;
    shown_words_and_parts(shown, rect, menu.lines, menu.parts);
    for (std::size_t i = 0; i < shown.words.size() && i < menu.lines.size(); ++i) {
        menu.lines[i].word = graphics::floor_div_px(shown.words[i].y, line);
    }
}

// A PANE NO WINDOW SHOWS: the frame's own words for why, where its office draws in no window,
// else that no window shows it.
// WL-READ-02 -- agents/workshop/desk-read.md
std::string WorkshopWeave::unshown_reading(const RuntimePane& row) const {
    const ExternalPane* pane = session_.panes.external_pane(row.kind);
    if (pane != nullptr && pane->held && !pane->in_window) {
        return "pane view unavailable: " + external_body_words(*pane, row.name);
    }
    if (pane != nullptr && !pane->held) {
        return "pane view unavailable: waiting for the provider -- no participant holds its office";
    }
    return "pane view unavailable: the pane is not placed in a window";
}

// LAYOUTS, READ AS IT IS PAINTED: its rows as words, and its tabs and `+` as parts Workshop names
// itself -- `layout:<name>` and `layout:+` -- as it names its menu's lines. Its fingerprint is what
// it says, and the naming line's caret and selection.
// WL-READ-04 -- agents/workshop/desk-read.md
std::string WorkshopWeave::layouts_reading(PaneView& out) const {
    const std::int64_t kind = pane_kind::kLayouts;
    if (!session_.panes.has(kind)) return "pane view unavailable: closed or unknown";
    const Screen sc = screen_of(session_);
    const auto bounds = bounds_of(session_.panes, session_.setup.active, kind, sc);
    if (!bounds.open) return "pane view unavailable: the pane is not presented";
    surface::SurfaceLayer layer;
    paint_layouts(layer, session_, bounds.rect, sc, kPaneChrome);
    if (layer.texts.empty()) return "pane has no visible body";
    const ExternalBodyPlace place = external_body_place(bounds.rect, sc, 0);
    const GlyphGrid grid = glyph_grid(place.fit, session_.console_rows_above);
    const std::int64_t space = input_space_of(sc);
    const surface::SurfaceTextRegion& region = layer.texts.back();
    std::vector<PaneWord> words;
    for (std::size_t i = 0; i < region.rows.size(); ++i) {
        PaneWord w = word_on(grid, static_cast<std::int64_t>(i), 0,
                             without_trailing_blanks(region.rows[i].text), space);
        w.word = static_cast<std::int64_t>(words.size());
        words.push_back(std::move(w));
    }
    std::vector<PanePart> parts;
    if (!session_.setup.naming.open && !region.rows.empty()) {
        const BandStatus status = band_status(session_, place);
        const std::vector<std::string> names = layout_part_names(session_.setup);
        std::vector<RowPart> named;
        for (const LayoutTab& tab : status.tabs) {
            if (tab.at < names.size()) named.push_back(RowPart{names[tab.at], 0, tab.column, tab.columns});
        }
        if (status.create_columns > 0) {
            named.push_back(RowPart{"layout:+", 0, status.create_column, status.create_columns});
        }
        const std::map<std::int64_t, std::vector<std::size_t>> owners = row_owners(named);
        for (std::size_t i = 0; i < named.size(); ++i) {
            PanePart drawn;
            if (row_part_on(grid, 0, region.rows[0].text, place.columns, named, owners.at(0), i, space,
                            drawn)) {
                parts.push_back(std::move(drawn));
            }
        }
    }
    // ITS FINGERPRINT IS WHAT IT SHOWS: what it says, and the naming line's caret and selection.
    PaneView said;
    said.words = words;
    said.parts = parts;
    Fingerprint said_print;
    said_print.bytes(loom::serialize(loom::to_value(said)));
    for (const std::int64_t n : {region.caret_row, region.caret_col, region.sel_begin_row,
                                 region.sel_begin_col, region.sel_end_row, region.sel_end_col}) {
        said_print.number(n);
    }
    out.stamp.fingerprint = said_print.value();
    out.area = desk_rect(bounds.rect);
    out.advance = place.fit.graphical() ? place.fit.advance_px : graphics::kCellPx;
    out.line = place.fit.graphical() ? place.fit.line_px : graphics::kCellPx;
    cover_reading(covers_of(kind, bounds.rect), bounds.rect, words, parts, out.covered);
    if (session_.presented.open || session_.context.open) without_points(words, parts);
    out.total = static_cast<std::int64_t>(words.size() + parts.size());
    out.words = std::move(words);
    out.parts = std::move(parts);
    return {};
}

// ---- THE CONVERSATION WITH THE WINDOW --------------------------------------------------------------

// ONE AREA ASKED OF THE WINDOW THAT SHOWS THE DESK, since the version Workshop last took of it, under
// a number the waits count by; 0 when it cannot be asked: no window shows the desk, the book is full,
// or the send was refused.
std::uint64_t WorkshopWeave::ask_area(loom::Mail& mail, const std::string& area) {
    if (!window_.shown()) return 0;
    const std::string office = std::string(graphics::kWindowOfficePrefix) + window_name_;
    const loom::AskOpened opened = area_asks_.open_to_role(
        office, graphics::AreaShowAsked::zen_name, graphics::AreaShowAsked::zen_version);
    if (!opened) return 0;
    const auto taken = area_readings_.find(area);
    const std::int64_t since = taken != area_readings_.end() && taken->second.number != 0 &&
                                       taken->second.refused.empty()
                                   ? taken->second.shown.version
                                   : 0;
    const loom::Ticket sent = mail.as_role(kWorkshopProvider)
                                  .send_to_role(office, graphics::AreaShowAsked{area, since},
                                                opened.correlation);
    if (!sent.valid()) {
        (void)area_asks_.forget(opened.id);
        return 0;
    }
    area_asked_.push_back(AreaAsk{opened.id, area, ++area_ask_number_});
    return area_ask_number_;
}

// WHAT A WINDOW SAID AN AREA SHOWS, taken: kept as the area's reading, then each wait it completes
// answered, a mark on it judged again, and the desk's notice said if it moved.
// WL-READ-08 -- agents/workshop/desk-read.md
void WorkshopWeave::on(const graphics::AreaShown& shown, loom::Mail& mail) {
    if (!mail.answers_ask()) return;
    const std::optional<loom::PendingAsk> settled = area_asks_.settle(mail.correlation(), mail.sender());
    if (!settled) return;
    if (take_key_fence(settled->id, &shown, mail)) return;
    take_area_answer(mail, settled->id, &shown, {});
}

// ...OR WHY IT WOULD NOT SAY: kept as the area's refusal, which its waits answer.
void WorkshopWeave::on(const loom::Refused& refused, loom::Mail& mail) {
    if (!mail.answers_ask()) return;
    const std::optional<loom::PendingAsk> settled = area_asks_.settle(mail.correlation(), mail.sender());
    if (!settled) return;
    if (take_key_fence(settled->id, nullptr, mail)) return;
    take_area_answer(mail, settled->id, nullptr, refused.reason);
}

void WorkshopWeave::take_area_answer(loom::Mail& mail, std::uint64_t ask,
                                     const graphics::AreaShown* shown, const std::string& refused) {
    const auto it = std::find_if(area_asked_.begin(), area_asked_.end(),
                                 [&](const AreaAsk& a) { return a.id == ask; });
    if (it == area_asked_.end()) return;
    const AreaAsk asked = *it;
    area_asked_.erase(it);
    AreaReading& reading = area_readings_[asked.area];
    const std::int64_t before = reading.number != 0 && reading.refused.empty()
                                    ? shown_fingerprint(reading.shown)
                                    : 0;
    const bool had = reading.number != 0;
    if (shown != nullptr && shown->same && had && reading.refused.empty()) {
        reading.shown.version = shown->version;
        reading.shown.epoch = shown->epoch;
        reading.shown.waiting = shown->waiting;
    } else if (shown != nullptr) {
        reading.shown = *shown;
        reading.refused.clear();
    } else {
        reading.refused = refused;
    }
    reading.number = (std::max)(reading.number, asked.number);
    const std::int64_t after = reading.refused.empty() ? shown_fingerprint(reading.shown) : 0;
    // WHAT A PANE'S MENU SHOWS IS THE DESK'S OWN WORD, so its number moves with it.
    if ((!had || before != after) && asked.area == presented_area()) desk_owed_ = true;
    finish_desk_waits(mail);
    if (!had || before != after) {
        if (!guide_marks_.empty()) repaint(mail);
        publish_desk_stamps(mail);
    }
}

// THE WAITS WHOSE AREAS HAVE ALL BEEN SAID SINCE THEY BEGAN, answered now from what was said; a wait
// past its bound, or one whose window changed, refused in words.
// WL-READ-08 -- agents/workshop/desk-read.md
void WorkshopWeave::finish_desk_waits(loom::Mail& mail) {
    const std::int64_t now = wait_clock_ms();
    for (std::size_t i = 0; i < desk_waits_.size();) {
        DeskWait& w = desk_waits_[i];
        bool ready = true;
        for (const auto& [area, number] : w.needs) {
            const auto taken = area_readings_.find(area);
            if (taken == area_readings_.end() || taken->second.number < number) ready = false;
        }
        const bool stale = w.epoch != window_epoch_ || now - w.since >= kDeskWaitMs;
        if (!ready && !stale) {
            ++i;
            continue;
        }
        DeskWait done = std::move(w);
        desk_waits_.erase(desk_waits_.begin() + static_cast<std::ptrdiff_t>(i));
        if (!ready) {
            const std::string why = done.epoch != window_epoch_
                                        ? "the window that shows the desk started again"
                                        : "the window that shows the desk did not answer in time";
            (void)loom::answer_deferred(done.answer, mail,
                                        loom::Refused{"desk read unavailable: " + why + "; read again"});
            continue;
        }
        switch (done.kind) {
        case DeskWait::Kind::kDeskRead:
            answer_later(done.answer, mail, desk_read_now(mail, done.asker));
            publish_desk_stamps(mail);
            break;
        case DeskWait::Kind::kPaneView:
            answer_later(done.answer, mail, pane_page(mail, done.asker, done.view));
            break;
        case DeskWait::Kind::kPanePoint:
            answer_later(done.answer, mail, pane_point(mail, done.asker, done.point));
            break;
        case DeskWait::Kind::kGuide:
            guide_requested(done.guide, done.asker, mail, &done.answer, true);
            break;
        }
    }
}

// A REQUEST THAT WAITS ON THE WINDOW: its answer deferred and each area it reads asked anew, or it is
// refused at once -- past the waits Workshop holds, past what the delivery can defer, or with no
// window to ask.
bool WorkshopWeave::wait_on_areas(loom::Mail& mail, DeskWait wait, const std::vector<std::string>& areas) {
    if (desk_waits_.size() >= kMaxDeskWaits) {
        (void)mail.answer(loom::Refused{"desk read refused: " + std::to_string(kMaxDeskWaits) +
                                        " reads wait on the window already; read again"});
        return false;
    }
    wait.answer = mail.defer_answer();
    if (!wait.answer.valid()) {
        (void)mail.answer(loom::Refused{"desk read refused: this delivery cannot defer its answer"});
        return false;
    }
    for (const std::string& area : areas) {
        const std::uint64_t number = ask_area(mail, area);
        if (number == 0) {
            (void)loom::answer_deferred(
                wait.answer, mail,
                loom::Refused{"desk read unavailable: the window that shows the desk cannot be asked "
                              "what area '" + area + "' shows"});
            return false;
        }
        wait.needs.emplace_back(area, number);
    }
    wait.since = wait_clock_ms();
    wait.epoch = window_epoch_;
    desk_waits_.push_back(std::move(wait));
    return true;
}

// AT A DELIVERY'S END THAT OWED A LOOK: every presented pane's area asked since what Workshop took
// of it, one ask an area at a time, so a pane that drew in its area moves the notice; and every wait
// past its bound refused.
// WL-READ-06 -- agents/workshop/desk-read.md
void WorkshopWeave::look_at_areas(loom::Mail& mail) {
    finish_desk_waits(mail);
    std::vector<std::string> areas;
    for (const PaneRef& ref : presented_refs()) {
        const std::string area = drawn_area(ref);
        if (!area.empty()) areas.push_back(area);
    }
    if (const std::string menu = presented_area(); !menu.empty()) areas.push_back(menu);
    for (const std::string& area : areas) {
        const bool open = std::any_of(area_asked_.begin(), area_asked_.end(),
                                      [&](const AreaAsk& a) { return a.area == area; });
        if (!open) (void)ask_area(mail, area);
    }
}

// THE WINDOW THE DESK IS SHOWN IN STARTED AGAIN, OR ANOTHER TOOK IT: what it said of every area is
// forgotten, and every wait on it refused.
void WorkshopWeave::forget_area_readings(loom::Mail& mail) {
    area_readings_.clear();
    for (const AreaAsk& a : area_asked_) (void)area_asks_.forget(a.id);
    area_asked_.clear();
    finish_desk_waits(mail);
    // ...AND THE KEYS WAITING ON IT GO: what it held before it started again is landed or gone.
    if (key_fence_ != 0) {
        (void)area_asks_.forget(key_fence_);
        key_fence_ = 0;
    }
    fence_since_ = 0;
    fence_covers_ = held_seq_;
    release_held_input(mail);
}

// ---- THE DESK, SAID WHOLE --------------------------------------------------------------------------

// A PAGE OF ONE PANE'S READING, under the stamp it continues: a stamp that is no longer the pane's
// is refused as stale, and the reader reads it again from the start; a picture in flight is said so
// at any page.
// WL-READ-02 -- agents/workshop/desk-read.md
Said<PaneView> WorkshopWeave::pane_page(loom::Mail& mail, loom::WeaveId asker,
                                        const PaneViewRequested& asked) const {
    if (const std::string withheld = read_withheld(mail, asker, asked.provider, asked.pane);
        !withheld.empty()) {
        return loom::Refused{"pane view withheld: " + withheld};
    }
    PaneView reading;
    if (const std::string why = pane_reading(asked.provider, asked.pane, reading); !why.empty()) {
        return loom::Refused{why};
    }
    // A STAMP NAMES ITS HOLDER, so an ask naming none reads the pane as it stands.
    const bool continues = asked.from > 0 || asked.stamp.holder != 0;
    if (continues && !same_stamp(asked.stamp, reading.stamp)) return loom::Refused{std::string(kPageStale)};
    if (const std::string why = page_refused(asked.from, reading); !why.empty()) return loom::Refused{why};
    if (reading.in_flight) return in_flight_page(reading, asked.from);
    const Page<PaneView> page = page_of(reading, asked.from, kDecodedCellBudget, kReplyByteBudget);
    if (const std::string why = page_alone_refused(page, asked.from, reading.total); !why.empty()) {
        return loom::Refused{why};
    }
    return page.view;
}

// WL-READ-02 -- agents/workshop/desk-read.md
void WorkshopWeave::on(const PaneViewRequested& asked, loom::Mail& mail) {
    const std::string area = drawn_area(PaneRef{asked.provider, asked.pane});
    if (area.empty() || !read_withheld(mail, mail.sender(), asked.provider, asked.pane).empty()) {
        answer_now(mail, pane_page(mail, mail.sender(), asked));
        return;
    }
    DeskWait wait;
    wait.kind = DeskWait::Kind::kPaneView;
    wait.asker = mail.sender();
    wait.view = asked;
    (void)wait_on_areas(mail, std::move(wait), {area});
}

// WHERE ONE CHARACTER OF ONE WORD IS NOW: the centre of its character in the reading the asker read,
// while the pane still shows what that reading said; a word or a character the reading does not
// hold, or one no press reaches, is refused.
Said<PanePoint> WorkshopWeave::pane_point(loom::Mail& mail, loom::WeaveId asker,
                                          const PanePointRequested& asked) const {
    if (const std::string withheld = read_withheld(mail, asker, asked.provider, asked.pane);
        !withheld.empty()) {
        return loom::Refused{"pane view withheld: " + withheld};
    }
    PaneView reading;
    if (const std::string why = pane_reading(asked.provider, asked.pane, reading); !why.empty()) {
        return loom::Refused{why};
    }
    if (reading.in_flight || reading.stamp.fingerprint != asked.fingerprint) {
        return loom::Refused{std::string(kPointStale)};
    }
    const auto word = std::find_if(reading.words.begin(), reading.words.end(),
                                   [&](const PaneWord& w) { return w.word == asked.word; });
    if (word == reading.words.end() || asked.column < 0 ||
        asked.column >= static_cast<std::int64_t>(word->text.size())) {
        return loom::Refused{"pane point unavailable: outside the pane's visible words"};
    }
    if (word->space == input::space::kUnknown) {
        return loom::Refused{"pane point unavailable: no press reaches that word now"};
    }
    PanePoint reply{asked.provider, asked.pane, asked.fingerprint, asked.word, asked.column, 0, 0,
                    word->space};
    const std::int64_t bytes = static_cast<std::int64_t>(word->text.size());
    const std::int64_t advance = bytes > 0 ? word->place.w / bytes : 0;
    if (advance <= 0) return loom::Refused{"pane point unavailable: that character is not addressable"};
    glyph_point(GlyphGrid{word->place.x, word->place.y, advance, word->place.h,
                          session_.console_rows_above},
                0, asked.column, word->space, reply.x, reply.y);
    return reply;
}

void WorkshopWeave::on(const PanePointRequested& asked, loom::Mail& mail) {
    const std::string area = drawn_area(PaneRef{asked.provider, asked.pane});
    if (area.empty() || !read_withheld(mail, mail.sender(), asked.provider, asked.pane).empty()) {
        answer_now(mail, pane_point(mail, mail.sender(), asked));
        return;
    }
    DeskWait wait;
    wait.kind = DeskWait::Kind::kPanePoint;
    wait.asker = mail.sender();
    wait.point = asked;
    (void)wait_on_areas(mail, std::move(wait), {area});
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

// THE STAMP A PANE STANDS ON NOW: its holder and incarnation, the window's incarnation, and the
// fingerprint of what it shows as last said -- the one stamp a desk read, a page and a notice say.
// A pane whose reading is refused still names what it shows where that is known, as a refused
// picture's room shows the refusal.
// WL-READ-07 -- agents/workshop/desk-read.md
PaneStamp WorkshopWeave::stamp_now(const PaneRef& ref) const {
    PaneView reading;
    (void)pane_reading(ref.provider, ref.pane, reading);
    return reading.stamp;
}

// THE DESK IN ONE TURN: the desk, every presented pane's stamp front to back, and as many of those
// panes' readings, in that order, as one decoded value and one reply's bytes hold; a stamp past the
// last reading names a pane its reader asks alone. A pane the asker may not read is said by its
// place, name and holder alone. Nothing is held between asks.
// WL-READ-01 -- agents/workshop/desk-read.md
Said<DeskRead> WorkshopWeave::desk_read_now(loom::Mail& mail, loom::WeaveId asker) const {
    DeskRead out;
    out.desk = desk_view();
    if (reads_by_pane(asker)) withhold_desk(out.desk);
    std::vector<std::optional<PaneView>> readings;
    for (const PaneRef& ref : presented_refs()) {
        if (!read_withheld(mail, asker, ref.provider, ref.pane).empty()) {
            out.stamps.push_back(withheld_stamp(ref));
            readings.emplace_back(std::nullopt);
            continue;
        }
        PaneView reading;
        const bool said = pane_reading(ref.provider, ref.pane, reading).empty();
        out.stamps.push_back(reading.stamp);
        if (said) {
            readings.emplace_back(std::move(reading));
        } else {
            readings.emplace_back(std::nullopt);
        }
    }
    const loom::Value shell = loom::to_value(out);
    std::int64_t cells = decoded_cells(shell);
    std::int64_t bytes = reply_bytes(shell);
    for (const std::optional<PaneView>& reading : readings) {
        if (!reading) continue;
        // A READING IS CARRIED WHOLE OR NOT AT ALL: whole when its first page, in the room the rest
        // leave beside its own cell and comma, holds every item.
        const Page<PaneView> whole = page_of(*reading, 0, kDecodedCellBudget - cells - 1,
                                             kReplyByteBudget - bytes - 1);
        if (items_of(whole.view) < reading->total) break;
        cells += 1 + whole.cells;
        bytes += 1 + whole.bytes;
        out.panes.push_back(*reading);
    }
    return out;
}

// WL-READ-01 -- agents/workshop/desk-read.md
void WorkshopWeave::on(const DeskReadRequested&, loom::Mail& mail) {
    if (const std::string why = take_desk_read(mail.sender()); !why.empty()) {
        (void)mail.answer(loom::Refused{why});
        return;
    }
    std::vector<std::string> areas;
    for (const PaneRef& ref : presented_refs()) {
        const std::string area = drawn_area(ref);
        if (!area.empty() && read_withheld(mail, mail.sender(), ref.provider, ref.pane).empty()) {
            areas.push_back(area);
        }
    }
    // ...AND A PANE'S MENU, which its presenter draws in an area of its own.
    if (const std::string menu = presented_area(); !menu.empty()) areas.push_back(menu);
    if (areas.empty()) {
        answer_now(mail, desk_read_now(mail, mail.sender()));
        publish_desk_stamps(mail);
        return;
    }
    DeskWait wait;
    wait.kind = DeskWait::Kind::kDeskRead;
    wait.asker = mail.sender();
    (void)wait_on_areas(mail, std::move(wait), areas);
}

// THE DESK MOVED: its number and every presented pane's stamp front to back, published as the office
// when any differs from what was last published, so the desk number never moves without a notice
// and an unchanged desk sends none.
// WL-READ-06 -- agents/workshop/desk-read.md
void WorkshopWeave::publish_desk_stamps(loom::Mail& mail, const DeskView* known) {
    DeskStamps now;
    now.desk = known != nullptr ? known->desk : desk_owed_ ? desk_view().desk : desk_number_;
    desk_owed_ = false;
    for (const PaneRef& ref : presented_refs()) {
        now.panes.push_back(stamp_now(ref));
    }
    if (stamps_published_ && now.desk == stamps_said_.desk && same_stamps(now.panes, stamps_said_.panes)) {
        return;
    }
    if (!mail.as_role(kWorkshopProvider).publish(now).authored) return;
    stamps_said_ = std::move(now);
    stamps_published_ = true;
}

} // namespace zengine::workshop
