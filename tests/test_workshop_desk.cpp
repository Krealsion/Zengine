// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop desk suite -- the desk and the words on it, read by message: every pane on the
// desk in Workshop's own numbers, and each run of words where the medium draws it.

#include "doctest.h"
#include "workshop_support.hpp"
#include "pane_window.hpp"
#include "timeline.hpp"
#include "attention/vocabulary.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/screen_planes.hpp"
#include "workshop/desk_read.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/screen_canvas.hpp"
#include "view/view-builder/picture.hpp"
#include "view/view.hpp"

#include <zen/host/grant_wiring.hpp>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr const char* kAlphaOffice = "zengine.test.desk-alpha";
constexpr const char* kBetaOffice = "zengine.test.desk-beta";

struct DeskAskerState {
    ZEN_SHAPE(DeskAskerState, 1);
};

/// WHO ASKS: an ordinary weave granted the desk's door, keeping every answer and refusal.
class DeskAsker
    : public loom::WeaveBase<DeskAsker, DeskAskerState,
                             loom::Accept<SeatDo, DeskRead, PaneView, PanePoint, loom::Refused>,
                             loom::Emit<DeskReadRequested, PaneViewRequested, PanePointRequested>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<DeskRead> reads;
    std::vector<PaneView> pages;
    std::vector<PanePoint> points;
    std::vector<std::string> refusals;
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(m);
    }
    void on(const DeskRead& d, loom::Mail&) { reads.push_back(d); }
    void on(const PaneView& v, loom::Mail&) { pages.push_back(v); }
    void on(const PanePoint& p, loom::Mail&) { points.push_back(p); }
    void on(const loom::Refused& r, loom::Mail&) { refusals.push_back(r.reason); }
};

/// A RIG WITH TWO TEXT PANES ON A FRESH DESK, and the asker beside them.
struct DeskRig {
    PaneRig r;
    ProviderSeat* alpha = nullptr;
    ProviderSeat* beta = nullptr;
    std::int64_t alpha_kind = kNoPaneKind;
    std::int64_t beta_kind = kNoPaneKind;
    DeskAsker* asker = nullptr;
    loom::WeaveId asker_id{};

    DeskRig() {
        r.mount_workshop();
        r.ready();
        r.extent(150, 60);
        alpha = r.mount_provider(kAlphaOffice);
        beta = r.mount_provider(kBetaOffice);
        alpha_kind = seat_pane_open(r, alpha, kAlphaOffice, "alpha");
        beta_kind = seat_pane_open(r, beta, kBetaOffice, "beta");
        REQUIRE(alpha_kind != kNoPaneKind);
        REQUIRE(beta_kind != kNoPaneKind);
        auto made = std::make_unique<DeskAsker>();
        asker = made.get();
        loom::Grant grant;
        for (const auto& shape : {loom::schema_of<DeskReadRequested>(),
                                  loom::schema_of<PaneViewRequested>(),
                                  loom::schema_of<PanePointRequested>()}) {
            grant.allow_to_role(shape->name(), shape->version(), kWorkshopProvider);
        }
        asker_id = r.bus.register_weave(std::move(made), std::move(grant));
        asker->zen_set_self(asker_id);
    }

    void ask(std::function<void(loom::Mail&)> what) {
        asker->next = std::move(what);
        (void)r.bus.send(asker_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    }

    /// The desk said whole, or the refusal's reason.
    std::string read(DeskRead& out) {
        asker->refusals.clear();
        const std::size_t before = asker->reads.size();
        ask([](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, DeskReadRequested{}); });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->reads.size() == before + 1);
        out = asker->reads.back();
        return std::string();
    }

    /// The desk, as a desk read says it.
    DeskView desk() {
        DeskRead whole;
        const std::string why = read(whole);
        REQUIRE(why.empty());
        return whole.desk;
    }

    /// One page of a pane's reading, or the refusal's reason.
    std::string page(const PaneViewRequested& asked, PaneView& out) {
        asker->refusals.clear();
        const std::size_t before = asker->pages.size();
        ask([&](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, asked); });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->pages.size() == before + 1);
        out = asker->pages.back();
        return std::string();
    }

    /// A pane's words and named parts as it stands, or the refusal's reason when Workshop will not
    /// say them.
    std::string parts(const std::string& provider, const std::string& pane, PaneView& out) {
        return page(PaneViewRequested{provider, pane, 0, {}}, out);
    }
    std::string words(const std::string& provider, const std::string& pane, PaneView& out) {
        return parts(provider, pane, out);
    }

    /// Where one character of one word is, or the refusal's reason.
    std::string point(const PanePointRequested& asked, PanePoint& out) {
        asker->refusals.clear();
        const std::size_t before = asker->points.size();
        ask([&](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, asked); });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->points.size() == before + 1);
        out = asker->points.back();
        return std::string();
    }

    /// A primary click where a word says to press, in the space it says.
    /// A press and its release at a point a reading names, through the window showing the desk.
    void click(std::int64_t x, std::int64_t y, std::int64_t space) {
        for (const bool down : {true, false}) r.hand_button(x, y, space, down);
    }
};

const DeskPane* pane_named(const DeskView& d, const std::string& provider, const std::string& pane) {
    for (const DeskPane& p : d.panes) {
        if (p.provider == provider && p.pane == pane) {
            return &p;
        }
    }
    return nullptr;
}

bool same(const DeskRect& a, const PixelRect& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

} // namespace

TEST_CASE("the desk says every pane on it, in the desk's order, with its state, rank and place in Workshop's own numbers") {
    DeskRig d;
    const Session& s = d.r.session();
    const Screen sc = screen_of(s);
    const DeskView desk = d.desk();

    // THE CANVAS AND THE MEDIUM: a terminal reports no device pixel, and its points are cells.
    CHECK(desk.width == sc.w);
    CHECK(desk.height == sc.h);
    CHECK(desk.cell_px == 0);
    CHECK(desk.space == input::space::kCells);
    CHECK(desk.room.y == sc.room_y);
    CHECK(desk.room.h == sc.room_h);
    CHECK_FALSE(desk.arranging);
    CHECK_FALSE(desk.menu.open);

    // EVERY ROW THE DESK NAMES, IN ITS ORDER: a fresh desk's two, then the two opened here.
    REQUIRE(desk.panes.size() == s.setup.active.panes.size());
    for (std::size_t i = 0; i < desk.panes.size(); ++i) {
        CAPTURE(i);
        CHECK(desk.panes[i].provider == s.setup.active.panes[i].ref.provider);
        CHECK(desk.panes[i].pane == s.setup.active.panes[i].ref.pane);
    }

    // AN OPEN PANE: its rectangles are `bounds_of`'s, its rank its place in the effective order.
    const std::vector<std::int64_t> order = effective_pane_order(s.setup.active, s.panes);
    for (const auto& [office, key, kind] :
         {std::tuple{kAlphaOffice, "alpha", d.alpha_kind}, std::tuple{kBetaOffice, "beta", d.beta_kind}}) {
        CAPTURE(key);
        const DeskPane* p = pane_named(desk, office, key);
        REQUIRE(p != nullptr);
        CHECK(p->name == "Seat");
        CHECK(p->state == "open");
        const PaneBounds where = bounds_of(s.panes, s.setup.active, kind, sc);
        CHECK(same(p->visible, where.rect));
        CHECK(same(p->resolved, where.resolved));
        CHECK(p->visible.w > 0);
        CHECK(p->front == static_cast<std::int64_t>(order.size()) - 1 - index_of(order, kind));
    }

    // A ROW THIS BUILD CANNOT PRESENT: named, unresolved, unranked, and placed nowhere.
    const DeskPane* info = pane_named(desk, kInfoPaneProvider, kInfoPaneKey);
    REQUIRE(info != nullptr);
    CHECK(info->state == "unresolved");
    CHECK(info->front == -1);
    CHECK(info->visible.w == 0);
    CHECK(info->resolved.w == 0);
    CHECK_FALSE(info->selected);
    CHECK_FALSE(info->keys);
}

TEST_CASE("the desk says which pane is selected and holds the keys, and a pane placed off the screen is off the room with no visible part") {
    DeskRig d;
    // A PRESS INTO ALPHA'S BODY selects it, lifts it to the front, and points the keys at it.
    press_body(d.r, d.alpha_kind);
    DeskView desk = d.desk();
    const DeskPane* alpha = pane_named(desk, kAlphaOffice, "alpha");
    const DeskPane* beta = pane_named(desk, kBetaOffice, "beta");
    REQUIRE(alpha != nullptr);
    REQUIRE(beta != nullptr);
    CHECK(alpha->selected);
    CHECK(alpha->keys);
    CHECK(alpha->front == 0);
    CHECK_FALSE(beta->selected);
    CHECK_FALSE(beta->keys);
    CHECK(beta->front > 0);

    // ...AND BETA, THEN: the answer follows the desk, not the order the panes were opened in.
    press_body(d.r, d.beta_kind);
    desk = d.desk();
    CHECK(pane_named(desk, kBetaOffice, "beta")->keys);
    CHECK(pane_named(desk, kBetaOffice, "beta")->front == 0);
    CHECK_FALSE(pane_named(desk, kAlphaOffice, "alpha")->keys);

    // A PLACE PAST THE CANVAS'S RIGHT EDGE: the intent is kept and said; nothing of it is visible.
    const Screen sc = screen_of(d.r.session());
    REQUIRE(place_at_canvas(d.r, PaneRef{kAlphaOffice, "alpha"}, sc.w + 240, sc.room_y + 24)
                .accepted);
    d.r.extent(150, 60);
    desk = d.desk();
    alpha = pane_named(desk, kAlphaOffice, "alpha");
    REQUIRE(alpha != nullptr);
    CHECK(alpha->state == "off-room");
    CHECK(alpha->resolved.x == sc.w + 240);
    CHECK(alpha->resolved.w > 0);
    CHECK(alpha->visible.w == 0);
    CHECK(alpha->visible.h == 0);
}

TEST_CASE("the desk says arranging is open, and the menu on the screen with each line where it is drawn") {
    DeskRig d;
    // ARRANGING, by the desk's own key from command mode.
    d.r.key(input::scan::kW);
    REQUIRE(d.r.session().arrange.open);
    CHECK(d.desk().arranging);
    d.r.key(input::scan::kEscape);
    REQUIRE_FALSE(d.r.session().arrange.open);
    CHECK_FALSE(d.desk().arranging);

    // WORKSHOP'S OWN MENU, opened on the room by its key.
    d.r.key(input::scan::kA);
    REQUIRE(d.r.session().context.open);
    const DeskView desk = d.desk();
    REQUIRE(desk.menu.open);
    CHECK(desk.menu.office == kWorkshopProvider);
    CHECK(same(desk.menu.place, context_bounds(d.r.session(), screen_of(d.r.session()))));
    REQUIRE_FALSE(desk.menu.lines.empty());
    // THE LINES ARE THE PAINTER'S: the region the composition drew, row for row.
    ScreenPlane layer;
    paint_context(layer, d.r.session(), screen_of(d.r.session()));
    REQUIRE_FALSE(layer.texts.empty());
    const auto& region = layer.texts.back();
    REQUIRE(desk.menu.lines.size() == region.rows.size());
    for (std::size_t i = 0; i < region.rows.size(); ++i) {
        CAPTURE(i);
        CHECK(region.rows[i].text.rfind(desk.menu.lines[i].text, 0) == 0);
        CHECK(desk.menu.lines[i].word == static_cast<std::int64_t>(i));
        CHECK(desk.menu.lines[i].space == input::space::kCells);
    }
    CHECK(desk.menu.lines[0].text.rfind("> ", 0) == 0);
}

namespace {

constexpr const char* kCanvasOffice = "zengine.test.desk-canvas";
constexpr const char* kCanvasPane = "sketch";

/// ONE POINTER MOMENT THE WINDOW GAVE THE SKETCH'S AREA: its phase (`canvas_pointer`), the
/// button, the point in the area's pixels, and the window's number for the press.
struct SketchPointer {
    std::int64_t phase = canvas_pointer::kPress;
    std::int64_t button = 0;
    std::int64_t x = 0, y = 0;
    std::uint64_t gesture = 0;
};

/// A PROVIDER THAT DRAWS A PICTURE in the area of its own office in the window that shows it, and
/// records every room that window gives it, every drawing it refuses and every pointer moment it
/// hands the area.
class SketchSeat : public loom::WeaveBase<SketchSeat, SeatState,
    loom::Accept<PaneCatalogRequested, gfx::AreaRoom, gfx::DrawnRefused, gfx::AreaButtonChanged,
                 gfx::AreaPointerMoved, gfx::AreaWheelTurned, gfx::AreaHoldEnded, PaneCanvasHover,
                 SeatDo>,
    loom::Emit<v3::PaneOffered, gfx::Drawn, PanePassRequested, gfx::RoomAsked>> {
public:
    explicit SketchSeat(std::string office = kCanvasOffice, std::string pane = kCanvasPane)
        : office_(office), window_(office, pane_area(office, pane)) {}
    std::vector<gfx::AreaRoom> rooms;
    std::vector<SketchPointer> pointers;
    std::vector<gfx::DrawnRefused> rejected;
    int drawn = 0;
    /// Whether a third-button press is handed back for Workshop's own pane menu.
    bool pass_right = false;
    std::function<void(SketchSeat&, loom::Mail&)> next;
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const gfx::AreaRoom& r, loom::Mail& m) {
        if (window_.room(r, m)) rooms.push_back(r);
    }
    void on(const gfx::DrawnRefused& r, loom::Mail& m) {
        if (window_.refused(r, m)) rejected.push_back(r);
    }
    void on(const gfx::AreaButtonChanged& b, loom::Mail& m) {
        take(window_.pointer(b, m, 0, 0));
        if (pass_right && b.pressed && b.button == 3) {
            (void)pane_menu::pass_back(m, office_, kCanvasPane);
        }
    }
    void on(const gfx::AreaPointerMoved& e, loom::Mail& m) { take(window_.pointer(e, m, 0, 0)); }
    void on(const gfx::AreaWheelTurned& e, loom::Mail& m) { take(window_.pointer(e, m, 0, 0)); }
    void on(const gfx::AreaHoldEnded& e, loom::Mail& m) { take(window_.pointer(e, m)); }
    void on(const PaneCanvasHover&, loom::Mail&) {}
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(*this, m);
    }
    /// THE ROOM THE WINDOW LAST GAVE THE AREA, as a picture is composed for it.
    CanvasRoom room() const {
        REQUIRE_FALSE(rooms.empty());
        const gfx::AreaRoom& r = rooms.back();
        return canvas_window_room(r.w, r.h, r.cell_px, r.measure());
    }
    /// DRAW `picture` in the area, for the room the window last gave it.
    void draw(loom::Mail& m, const CanvasPicture& picture) {
        const CanvasSketch said = canvas_sketch(picture, room());
        REQUIRE_MESSAGE(said.over.empty(), said.over);
        ++drawn;
        window_.draw(said.sketch, m);
    }

private:
    void take(const std::optional<WindowPointer>& p) {
        if (p) pointers.push_back(SketchPointer{p->phase, p->button, p->x, p->y, p->press});
    }
    std::string office_;
    WindowDrawing window_;
};

/// What a canvas seat may say: its offer, its drawing, and a press handed back.
loom::Grant sketch_grant() {
    loom::Grant grant;
    grant.allow_to_any(v3::PaneOffered::zen_name, v3::PaneOffered::zen_version);
    grant.allow_to_any(gfx::Drawn::zen_name, gfx::Drawn::zen_version);
    grant.allow_to_any(PanePassRequested::zen_name, PanePassRequested::zen_version);
    grant.allow_to_role(gfx::RoomAsked::zen_name, gfx::RoomAsked::zen_version, gfx::kGraphicsRole);
    return grant;
}

/// THE DESK RIG WITH A CANVAS PANE BESIDE ITS TWO TEXT PANES, drawing two labels and two runs of
/// measured text -- one with a caret -- in its area.
struct SketchRig : DeskRig {
    SketchSeat* sketch = nullptr;
    loom::WeaveId sketch_id{};
    std::int64_t sketch_kind = kNoPaneKind;
    std::int64_t number = 0;

    SketchRig() {
        r.host.role_holder = [this](std::string_view office) { return r.bus.role_holder(office); };
        auto made = std::make_unique<SketchSeat>();
        sketch = made.get();
        sketch_id = r.bus.register_weave(std::move(made), sketch_grant(), std::string(kCanvasOffice));
        sketch->zen_set_self(sketch_id);
        drive([](SketchSeat&, loom::Mail& m) {
            (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider,
                v3::PaneOffered{kCanvasPane, "Sketch", "a local picture", 40 * kPaneCanvasUnit,
                                12 * kPaneCanvasUnit, 0});
        });
        r.pick(PaneRef{kCanvasOffice, kCanvasPane});
        const auto* row = r.session().panes.runtime.find(kCanvasOffice, kCanvasPane);
        REQUIRE(row != nullptr);
        sketch_kind = row->kind;
        draw();
    }
    void drive(std::function<void(SketchSeat&, loom::Mail&)> f) {
        sketch->next = std::move(f);
        (void)r.bus.send(sketch_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    }
    /// The picture for the room the window last gave the area.
    CanvasPicture picture() {
        const CanvasRoom room = sketch->room();
        const auto line = room.text_line_px > 0 ? room.text_line_px : kPaneCanvasUnit;
        ++number;
        CanvasPicture p;
        p.rects.push_back(CanvasRect{0, 0, 10 * kPaneCanvasUnit, kPaneCanvasUnit, gfx::role::kMuted});
        p.labels.push_back(CanvasLabel{kPaneCanvasUnit, 0, "node one", gfx::role::kFill});
        p.labels.push_back(CanvasLabel{2 * kPaneCanvasUnit, 2 * kPaneCanvasUnit, "[Label]",
                                       gfx::role::kAccent});
        p.texts.push_back(CanvasText{0, 4 * kPaneCanvasUnit, "measured words", gfx::role::kFill});
        CanvasText typed{0, 4 * kPaneCanvasUnit + 2 * line, "typed here", gfx::role::kFill};
        typed.caret_col = 3;
        p.texts.push_back(typed);
        return p;
    }
    /// The picture, drawn in the area.
    void draw() {
        const CanvasPicture p = picture();
        const int before = sketch->drawn;
        drive([p](SketchSeat& s, loom::Mail& m) { s.draw(m, p); });
        REQUIRE(sketch->drawn == before + 1);
    }
    /// ...and the same picture naming `parts`.
    void draw_named(std::vector<CanvasPart> parts) {
        CanvasPicture p = picture();
        p.parts = std::move(parts);
        drive([p](SketchSeat& s, loom::Mail& m) { s.draw(m, p); });
    }
    /// ...or any picture.
    void draw_picture(const CanvasPicture& p) {
        drive([p](SketchSeat& s, loom::Mail& m) { s.draw(m, p); });
    }
    /// The medium changes: a new room, and the picture drawn again for it.
    void medium(bool window) {
        if (window) {
            r.extent_on_window(150, 60);
        } else {
            r.extent(150, 60);
        }
        draw();
    }
};

/// THE TEXT PANES' ROWS, said again for the room the window just gave their areas.
void say_rows(DeskRig& d) {
    d.r.drive(d.alpha, [](ProviderSeat& s, loom::Mail& m) {
        s.say(m, SeatRows{"alpha", {TextRow{"first row", gfx::role::kFill},
                                    TextRow{"press [Save] here", gfx::role::kFill}}, {}});
    });
    d.r.drive(d.beta, [](ProviderSeat& s, loom::Mail& m) {
        s.say(m, SeatRows{"beta", {TextRow{"beta row", gfx::role::kFill}}, {}});
    });
}

/// WHETHER TWO READINGS OF A PANE PLACE ITS AREA THE SAME: the room the window gives it.
bool same_place(const ExternalPane& a, const ExternalPane& b) {
    return a.placed_x == b.placed_x && a.placed_y == b.placed_y && a.placed_w == b.placed_w &&
           a.placed_h == b.placed_h;
}

/// WHAT THE WINDOW SHOWS AS A WORD AT `place`: the characters across it, the blanks after the
/// last one left off.
std::string shown_word(PaneRig& r, const DeskRect& place);

/// WHAT THE WINDOW SHOWS ACROSS `place`, on its first line: the characters a weaver reads there.
std::string shown_across(PaneRig& r, const DeskRect& place) {
    const DeskShown shown = r.shown();
    const std::int64_t columns = shown.advance > 0 ? place.w / shown.advance : 0;
    std::string out = shown.text_at_px(place.x, place.y, columns);
    out.resize(static_cast<std::size_t>(columns), ' ');
    return out;
}

std::string shown_word(PaneRig& r, const DeskRect& place) {
    std::string out = shown_across(r, place);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

/// The character the window shows at a point in the terminal window's cells.
char shown_glyph(PaneRig& r, std::int64_t x, std::int64_t y) {
    const DeskShown shown = r.shown();
    const std::string at = shown.text_at_px(x * shown.advance, y * shown.line, 1);
    return at.empty() ? ' ' : at[0];
}

/// WHETHER THE WINDOW SHOWS THE SKETCH'S FIRST LABEL where its area stands at `pane`'s place.
bool painted_at(PaneRig& r, const ExternalPane& pane) {
    return r.shown().text_at_px(pane.placed_x + kPaneCanvasUnit, pane.placed_y, 8) == "node one";
}

/// Where a press in the area lands, in the desk's pixels: the place, less the area's corner.
bool inside_locally(const SketchPointer& e, const DeskRect& place, const ExternalPane& pane) {
    const auto x = e.x + pane.placed_x;
    const auto y = e.y + pane.placed_y;
    return x >= place.x && x < place.x + place.w && y >= place.y && y < place.y + place.h;
}

} // namespace

// A GRID OF GLYPHS NAMES ITS CORNER, ITS ADVANCE AND ITS LINE EVERY TIME it is made: no default
// stands in for them, so no grid reads a point off a measure it was not given.
TEST_CASE("a glyph grid is made with its corner, advance and line named, from no default") {
    using I = std::int64_t;
    using zengine::workshop::desk_read::GlyphGrid;
    CHECK_FALSE(std::is_default_constructible_v<GlyphGrid>);
    CHECK_FALSE(std::is_constructible_v<GlyphGrid, I, I, I>);
    CHECK(std::is_constructible_v<GlyphGrid, I, I, I, I>);
    const GlyphGrid in_window{0, 0, 12, 12};
    CHECK(in_window.line == 12);
}

TEST_CASE("a text pane's words are its rows, each with its place and size, and a press at a word's point or a character's lands on that row and column") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        say_rows(d);
        PaneView view;
        REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
        // ITS STAMP NAMES WHAT IT SHOWS: read again unchanged, the same fingerprint.
        CHECK(view.stamp.fingerprint != 0);
        {
            PaneView again;
            REQUIRE(d.words(kAlphaOffice, "alpha", again).empty());
            CHECK(again.stamp.fingerprint == view.stamp.fingerprint);
        }
        REQUIRE(view.words.size() == 2);
        CHECK(view.words[0].text == "first row");
        CHECK(view.words[1].text == "press [Save] here");
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        const std::int64_t advance = window ? body.fit.advance_px : gfx::kCellPx;
        const std::int64_t line = window ? body.fit.line_px : gfx::kCellPx;
        for (const PaneWord& w : view.words) {
            CAPTURE(w.text);
            CHECK(w.place.w == static_cast<std::int64_t>(w.text.size()) * advance);
            CHECK(w.place.h == line);
            CHECK(w.space == (window ? input::space::kPixels : input::space::kCells));
        }
        CHECK(view.words[1].place.y - view.words[0].place.y == line);
        CHECK(view.words[0].place.x == view.words[1].place.x);

        // EACH WORD'S OWN POINT IS A PRESS ON ITS ROW.
        for (const PaneWord& w : view.words) {
            d.alpha->presses.clear();
            d.click(w.x, w.y, w.space);
            REQUIRE(d.alpha->presses.size() == 1);
            CHECK(d.alpha->presses[0].row == w.word);
        }
        // ...AND A CHARACTER'S POINT, ASKED BY ITS COLUMN, IS A PRESS ON THAT COLUMN.
        const std::int64_t save = static_cast<std::int64_t>(view.words[1].text.find("[Save]")) + 1;
        PanePoint at;
        REQUIRE(d.point(PanePointRequested{kAlphaOffice, "alpha", view.stamp.fingerprint, 1, save}, at).empty());
        CHECK(at.space == view.words[1].space);
        d.alpha->presses.clear();
        d.click(at.x, at.y, at.space);
        REQUIRE(d.alpha->presses.size() == 1);
        CHECK(d.alpha->presses[0].row == 1);
        CHECK(d.alpha->presses[0].column == save);

        // A READING THE CALLER DID NOT READ, A WORD OR A COLUMN THERE IS NOT: refused, by name.
        CHECK(d.point(PanePointRequested{kAlphaOffice, "alpha", view.stamp.fingerprint + 1, 1, 0}, at)
                  .find("shows moved") != std::string::npos);
        CHECK(d.point(PanePointRequested{kAlphaOffice, "alpha", view.stamp.fingerprint, 2, 0}, at)
                  .find("outside") != std::string::npos);
        CHECK(d.point(PanePointRequested{kAlphaOffice, "alpha", view.stamp.fingerprint, 0, 9}, at)
                  .find("outside") != std::string::npos);
    }
}

TEST_CASE("a canvas pane's words are its labels and text runs, each where it is drawn, and a press at a word's point lands inside it in the pane's own canvas") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        PaneView view;
        REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
        REQUIRE(view.words.size() == 4);
        CHECK(view.words[0].text == "node one");
        CHECK(view.words[1].text == "[Label]");
        CHECK(view.words[2].text == "measured words");
        CHECK(view.words[3].text == "typed here");
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        // A LABEL IS DRAWN AS THE WINDOW SETS TEXT, from its own corner in the body: a cell a byte
        // in cells, the face's advance a byte where the window sets type.
        const std::int64_t label_advance = window ? d.r.session().text_advance_px : kPaneCanvasUnit;
        CHECK(view.words[1].place.x == pane.placed_x + 2 * kPaneCanvasUnit);
        CHECK(view.words[1].place.y == pane.placed_y + 2 * kPaneCanvasUnit);
        CHECK(view.words[1].place.w == 7 * label_advance);
        // A RUN OF MEASURED TEXT IS THE FACE'S, where the medium sets type: one cell a byte where
        // it does not, the caret taking none of its own in either.
        if (window) {
            CHECK(view.words[2].place.w == 14 * d.r.session().text_advance_px);
            CHECK(view.words[3].place.w == 10 * d.r.session().text_advance_px);
        } else {
            CHECK(view.words[2].place.w == 14 * kPaneCanvasUnit);
            CHECK(view.words[3].place.w == 10 * kPaneCanvasUnit);
        }
        for (const PaneWord& w : view.words) {
            CAPTURE(w.text);
            d.sketch->pointers.clear();
            d.click(w.x, w.y, w.space);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            const SketchPointer& press = d.sketch->pointers.front();
            CHECK(press.phase == canvas_pointer::kPress);
            CHECK(inside_locally(press, w.place, pane));
        }
        // ONE CHARACTER OF A CANVAS WORD, by its column: inside that character's own cell.
        PanePoint at;
        REQUIRE(d.point(PanePointRequested{kCanvasOffice, kCanvasPane, view.stamp.fingerprint, 1, 1}, at).empty());
        d.sketch->pointers.clear();
        d.click(at.x, at.y, at.space);
        REQUIRE_FALSE(d.sketch->pointers.empty());
        const DeskRect l = view.words[1].place;
        CHECK(inside_locally(d.sketch->pointers.front(),
                             DeskRect{l.x + kPaneCanvasUnit, l.y, kPaneCanvasUnit, l.h}, pane));
        // ...AND ONE AFTER THE CARET, which moves no character: `d`, the run's fifth byte, stands
        // in its fifth cell in a terminal and its fifth advance in a window.
        REQUIRE(d.point(PanePointRequested{kCanvasOffice, kCanvasPane, view.stamp.fingerprint, 3, 4}, at).empty());
        const DeskRect typed = view.words[3].place;
        if (window) {
            const auto advance = d.r.session().text_advance_px;
            CHECK(at.x == typed.x + 4 * advance + advance / 2);
        } else {
            CHECK(at.x == gfx::cell_of_pixel(typed.x) + 4);
        }
    }
}

TEST_CASE("a canvas pane a held press gave the keys, its title waiting with pane titles hidden, is read "
          "where its picture is painted, and a point there lands in the room the press kept") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        press_outside(d.r, d.sketch_kind); // the keys are Workshop's...
        d.r.key(input::scan::kT);          // ...and the titles hidden
        d.r.text("t");
        REQUIRE_FALSE(d.r.session().pane_titles);
        REQUIRE(external_title_rows(d.r.session().panes, d.sketch_kind, false) == 0);
        d.draw(); // in the room the hidden title gave back
        PaneView before;
        REQUIRE(d.words(kCanvasOffice, kCanvasPane, before).empty());
        REQUIRE(before.words.size() == 4);
        const ExternalPane kept = *d.r.session().panes.external_pane(d.sketch_kind);
        // A PRESS HELD ON THE PANE takes the keys, and with them the title the pane wears while
        // it has them (WL-FOCUS-11); the room that title takes waits for the press.
        const PaneWord& pressed = before.words[2];
        const std::size_t rooms_before = d.sketch->rooms.size();
        d.r.hand_button(pressed.x, pressed.y, pressed.space, true);
        REQUIRE(keyboard_pane(d.r.session().panes) == d.sketch_kind);
        REQUIRE(external_title_rows(d.r.session().panes, d.sketch_kind, false) == 1);
        const ExternalPane& held = *d.r.session().panes.external_pane(d.sketch_kind);
        REQUIRE(held.placed_y == kept.placed_y);
        REQUIRE(held.placed_h == kept.placed_h);
        // ...and no room is given its area while the press is held.
        CHECK(d.sketch->rooms.size() == rooms_before);
        // READ WHERE IT IS PAINTED, before the pane draws again, as it may never: the words stand
        // in the room the press kept, where the painter draws the picture.
        const auto read = [&](PaneView& view) {
            const std::string refused = d.words(kCanvasOffice, kCanvasPane, view);
            REQUIRE_MESSAGE(refused.empty(), refused);
            REQUIRE(view.words.size() == 4);
            CHECK(view.words[1].place.x == kept.placed_x + 2 * kPaneCanvasUnit);
            CHECK(view.words[1].place.y == kept.placed_y + 2 * kPaneCanvasUnit);
        };
        PaneView unredrawn;
        read(unredrawn);
        CHECK(unredrawn.stamp.fingerprint == before.stamp.fingerprint);
        CHECK(painted_at(d.r, kept));
        // ...AND ONCE IT DRAWS AGAIN in the room it still holds, that picture read in it.
        d.draw();
        REQUIRE(held.placed_y == kept.placed_y);
        PaneView during;
        read(during);
        CHECK(painted_at(d.r, kept));
        // ...under no title: the title waits for the press, over the row the picture still holds.
        const RuntimePane* row = d.r.session().panes.runtime.of_kind(d.sketch_kind);
        REQUIRE(row != nullptr);
        const std::string title = external_header(*row, true);
        const auto titled = [&] {
            for (const ScreenPlane& layer : paint(d.r.session()).layers)
                for (const ScreenRegion& region : layer.texts)
                    for (const TextRow& line : region.rows)
                        if (line.text.rfind(title, 0) == 0) return true;
            return false;
        };
        CHECK_FALSE(titled());
        // ...AND A POINT THERE: the release at it reaches the pane inside that word, in the room
        // the press was aimed at.
        PanePoint at;
        const std::string unpointed =
            d.point(PanePointRequested{kCanvasOffice, kCanvasPane, during.stamp.fingerprint, 1, 1}, at);
        REQUIRE_MESSAGE(unpointed.empty(), unpointed);
        d.sketch->pointers.clear();
        d.r.hand_button(at.x, at.y, at.space, false);
        REQUIRE_FALSE(d.sketch->pointers.empty());
        const SketchPointer& released = d.sketch->pointers.front();
        CHECK(released.phase == canvas_pointer::kRelease);
        const DeskRect l = during.words[1].place;
        CHECK(inside_locally(released, DeskRect{l.x + kPaneCanvasUnit, l.y, kPaneCanvasUnit, l.h},
                             kept));
        // THE PRESS ENDED: the area is placed under the title, and the window shows the picture there
        // at once -- where the pointer answers now -- and the title is drawn over the row it left.
        const ExternalPane& after = *d.r.session().panes.external_pane(d.sketch_kind);
        CHECK_FALSE(after.title_waits);
        CHECK(after.placed_y > kept.placed_y);
        CHECK(painted_at(d.r, after));
        // ...its area given one room, a title row lower.
        REQUIRE(d.sketch->rooms.size() == rooms_before + 1);
        CHECK(d.sketch->rooms.back().h < kept.placed_h);
        d.draw();
        CHECK(titled());
    }
}

TEST_CASE("a held press keeps a canvas pane's room only from its title row: a pane moved while the press "
          "is held is granted its new room at once, and the press is lost") {
    SketchRig d;
    const auto author = [&](std::int64_t y, std::int64_t h) {
        stage_desk(d.r, [&](Setup& staged) {
            for (auto& p : staged.panes) {
                if (p.ref.provider != kCanvasOffice || p.ref.pane != kCanvasPane) continue;
                p.place = {pane_unit::kPixels, 4 * gfx::kCellPx, y * gfx::kCellPx};
                p.width = {pane_unit::kPixels, 60 * gfx::kCellPx};
                p.height = {pane_unit::kPixels, h * gfx::kCellPx};
            }
        });
        d.r.extent(149, 60); // a same-size extent reseats nothing: the desk re-seats every pane
        d.r.extent(150, 60);
    };
    author(20, 16);
    d.draw();
    PaneView view;
    REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
    const ExternalPane before = *d.r.session().panes.external_pane(d.sketch_kind);
    // A SECONDARY PRESS HELD ON THE PANE: its hold is the canvas's, and the keys stay where they are.
    const PaneWord& w = view.words[2];
    d.sketch->pointers.clear();
    d.r.hand_button(w.x, w.y, w.space, true, 3);
    REQUIRE_FALSE(d.sketch->pointers.empty());
    REQUIRE(d.sketch->pointers.back().phase == canvas_pointer::kPress);
    // ...AND THE PANE'S TOP EDGE MOVED two rows down, the same width ending where it did: no title
    // row made that change, so the room is not kept for the press.
    author(22, 14);
    const ExternalPane& moved = *d.r.session().panes.external_pane(d.sketch_kind);
    CHECK_FALSE(same_place(moved, before));
    CHECK_FALSE(moved.title_waits);
    CHECK(moved.placed_y > before.placed_y);
    CHECK(moved.placed_y + moved.placed_h == before.placed_y + before.placed_h);
    CHECK(d.sketch->pointers.back().phase == canvas_pointer::kPress);
    d.r.hand_button(w.x, w.y, w.space, false, 3);
    CHECK(d.sketch->pointers.back().phase == canvas_pointer::kRelease);
}

TEST_CASE("a canvas pane whose title waits for a held press, moved while it waits, is granted its new room "
          "at once, and the press stays the pane's until its release") {
    SketchRig d;
    press_outside(d.r, d.sketch_kind); // the keys are Workshop's...
    d.r.key(input::scan::kT);          // ...and the titles hidden
    d.r.text("t");
    REQUIRE_FALSE(d.r.session().pane_titles);
    const auto author = [&](std::int64_t y, std::int64_t h) {
        stage_desk(d.r, [&](Setup& staged) {
            for (auto& p : staged.panes) {
                if (p.ref.provider != kCanvasOffice || p.ref.pane != kCanvasPane) continue;
                p.place = {pane_unit::kPixels, 4 * gfx::kCellPx, y * gfx::kCellPx};
                p.width = {pane_unit::kPixels, 60 * gfx::kCellPx};
                p.height = {pane_unit::kPixels, h * gfx::kCellPx};
            }
        });
        d.r.extent(149, 60); // a same-size extent reseats nothing: the desk re-seats every pane
        d.r.extent(150, 60);
    };
    author(20, 16);
    d.draw();
    PaneView view;
    REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
    const ExternalPane before = *d.r.session().panes.external_pane(d.sketch_kind);
    // A PRIMARY PRESS HELD ON THE PANE takes the keys, and the room their title row takes waits.
    const PaneWord& w = view.words[2];
    d.sketch->pointers.clear();
    d.r.hand_button(w.x, w.y, w.space, true);
    REQUIRE(keyboard_pane(d.r.session().panes) == d.sketch_kind);
    REQUIRE(d.r.session().panes.external_pane(d.sketch_kind)->title_waits);
    REQUIRE(same_place(*d.r.session().panes.external_pane(d.sketch_kind), before));
    // ...AND THE PANE MOVED two rows down while its title waits: no room is kept for the press.
    author(22, 14);
    const ExternalPane& moved = *d.r.session().panes.external_pane(d.sketch_kind);
    CHECK_FALSE(same_place(moved, before));
    CHECK_FALSE(moved.title_waits);
    CHECK(moved.placed_y > before.placed_y);
    CHECK(d.sketch->pointers.back().phase == canvas_pointer::kPress);
    d.r.hand_button(w.x, w.y, w.space, false);
    CHECK(d.sketch->pointers.back().phase == canvas_pointer::kRelease);
}

TEST_CASE("a pane moved one title row's height while a press is held, its title unchanged, is granted its "
          "new room at once, and the press stays the pane's until its release, in a window and in a "
          "terminal, titled or not") {
    for (const bool window : {false, true}) {
        for (const bool hidden : {false, true}) {
            CAPTURE(window);
            CAPTURE(hidden);
            SketchRig d;
            if (hidden) {
                press_outside(d.r, d.sketch_kind); // the keys are Workshop's...
                d.r.key(input::scan::kT);          // ...and the titles hidden
                d.r.text("t");
                REQUIRE_FALSE(d.r.session().pane_titles);
            }
            const auto author = [&](std::int64_t y, std::int64_t h) {
                stage_desk(d.r, [&](Setup& staged) {
                    for (auto& p : staged.panes) {
                        if (p.ref.provider != kCanvasOffice || p.ref.pane != kCanvasPane) continue;
                        p.place = {pane_unit::kPixels, 4 * gfx::kCellPx, y};
                        p.width = {pane_unit::kPixels, 60 * gfx::kCellPx};
                        p.height = {pane_unit::kPixels, h};
                    }
                });
                // a same-size extent reseats nothing: the desk re-seats every pane
                if (window) {
                    d.r.extent_on_window(149, 60);
                    d.r.extent_on_window(150, 60);
                } else {
                    d.r.extent(149, 60);
                    d.r.extent(150, 60);
                }
            };
            const std::int64_t top = 20 * gfx::kCellPx, tall = 16 * gfx::kCellPx;
            author(top, tall);
            d.draw();
            const std::int64_t titles = hidden ? 0 : kExternalHeaderRows;
            REQUIRE(external_title_rows(d.r.session().panes, d.sketch_kind, d.r.session().pane_titles) ==
                    titles);
            // ONE TITLE ROW'S HEIGHT in this medium: what a body loses to the title row.
            const Screen sc = screen_of(d.r.session());
            const PaneBounds at = bounds_of(d.r.session().panes, d.r.session().setup.active,
                                            d.sketch_kind, sc);
            const std::int64_t step =
                canvas_body_place(at.rect, sc, kExternalHeaderRows).y - canvas_body_place(at.rect, sc, 0).y;
            REQUIRE(step > 0);
            PaneView view;
            REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
            const ExternalPane before = *d.r.session().panes.external_pane(d.sketch_kind);
            // A SECONDARY PRESS HELD ON THE PANE: the keys, and so its title rows, stay as they are.
            const PaneWord& w = view.words[2];
            d.sketch->pointers.clear();
            d.r.hand_button(w.x, w.y, w.space, true, 3);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            REQUIRE(d.sketch->pointers.back().phase == canvas_pointer::kPress);
            // ...AND ITS TOP EDGE MOVED one title row's height, its bottom where it was: down under a
            // title it keeps, up with none -- each the body it would have with the other title count,
            // and no title row's change.
            if (hidden) {
                author(top - step, tall + step);
            } else {
                author(top + step, tall - step);
            }
            REQUIRE(external_title_rows(d.r.session().panes, d.sketch_kind, d.r.session().pane_titles) ==
                    titles);
            const ExternalPane& moved = *d.r.session().panes.external_pane(d.sketch_kind);
            CHECK_FALSE(same_place(moved, before));
            CHECK_FALSE(moved.title_waits);
            CHECK(moved.placed_y == before.placed_y + (hidden ? -step : step));
            CHECK(moved.placed_y + moved.placed_h == before.placed_y + before.placed_h);
            CHECK(d.sketch->pointers.back().phase == canvas_pointer::kPress);
            d.r.hand_button(w.x, w.y, w.space, false, 3);
            CHECK(d.sketch->pointers.back().phase == canvas_pointer::kRelease);
        }
    }
}

TEST_CASE("a held press on the canvas pane that has the keys keeps its room while launches take its title "
          "row away and give it back, and its title is drawn again once it has the keys again") {
    SketchRig d;
    press_outside(d.r, d.sketch_kind); // the keys are Workshop's...
    d.r.key(input::scan::kT);          // ...and the titles hidden
    d.r.text("t");
    REQUIRE_FALSE(d.r.session().pane_titles);
    d.draw();
    // THE KEYS COME TO THE PANE by a click, and its title with them, in the room that title makes.
    PaneView view;
    REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
    const PaneWord& w = view.words[2];
    d.click(w.x, w.y, w.space);
    REQUIRE(keyboard_pane(d.r.session().panes) == d.sketch_kind);
    d.draw();
    REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
    const ExternalPane titled_room = *d.r.session().panes.external_pane(d.sketch_kind);
    const RuntimePane* row = d.r.session().panes.runtime.of_kind(d.sketch_kind);
    REQUIRE(row != nullptr);
    const std::string title = external_header(*row, true);
    const auto titled = [&] {
        for (const ScreenPlane& layer : paint(d.r.session()).layers)
            for (const ScreenRegion& region : layer.texts)
                for (const TextRow& line : region.rows)
                    if (line.text.rfind(title, 0) == 0) return true;
        return false;
    };
    REQUIRE(titled());
    // A PRESS HELD ON IT, and a launch of another pane taking the keys -- and the title row --
    // away while it is held: the pane keeps the room the press was aimed at, untitled.
    const PaneWord& held_at = view.words[2];
    d.r.hand_button(held_at.x, held_at.y, held_at.space, true);
    (void)hand_launch(d.r, PaneRef{kAlphaOffice, "alpha"});
    REQUIRE(keyboard_pane(d.r.session().panes) == d.alpha_kind);
    const ExternalPane& now = *d.r.session().panes.external_pane(d.sketch_kind);
    CHECK(same_place(now, titled_room));
    CHECK(now.title_waits);
    CHECK_FALSE(titled());
    // ...AND A LAUNCH OF THE PANE ITSELF giving them back, still held: its room and its title
    // agree again, so it waits for nothing and its title is drawn.
    (void)hand_launch(d.r, PaneRef{kCanvasOffice, kCanvasPane});
    REQUIRE(keyboard_pane(d.r.session().panes) == d.sketch_kind);
    CHECK(same_place(now, titled_room));
    CHECK_FALSE(now.title_waits);
    CHECK(titled());
    d.r.hand_button(held_at.x, held_at.y, held_at.space, false);
    CHECK(same_place(*d.r.session().panes.external_pane(d.sketch_kind), titled_room));
    CHECK(titled());
}

TEST_CASE("a held press a menu takes ends its canvas pane's wait in that same repaint: the room under the "
          "title and its rows are granted at once") {
    SketchRig d;
    press_outside(d.r, d.sketch_kind); // the keys are Workshop's...
    d.r.key(input::scan::kT);          // ...and the titles hidden
    d.r.text("t");
    REQUIRE_FALSE(d.r.session().pane_titles);
    d.draw();
    PaneView view;
    REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
    const ExternalPane kept = *d.r.session().panes.external_pane(d.sketch_kind);
    // A PRESS HELD ON THE PANE gives it the keys, and the room their title row makes waits for it.
    const PaneWord& w = view.words[2];
    d.r.hand_button(w.x, w.y, w.space, true);
    REQUIRE(keyboard_pane(d.r.session().panes) == d.sketch_kind);
    REQUIRE(d.r.session().panes.external_pane(d.sketch_kind)->title_waits);
    // ...AND A RIGHT PRESS ON THE ROOM BELOW IT, the first still held: the room's menu opens and
    // takes the hold, and in that repaint the wait is over -- no press holds the room.
    const CellRect pane_rect = cells_covered(external_pane_rect(d.r.session(), d.sketch_kind));
    d.r.hand_moment(input::PointerButton{3, true, pane_rect.x + 1,
                                                    pane_rect.y + pane_rect.h + 1,
                                                    input::space::kCells, input::mod::kNone});
    REQUIRE(d.r.session().context.open);
    const ExternalPane& now = *d.r.session().panes.external_pane(d.sketch_kind);
    CHECK_FALSE(now.title_waits);
    CHECK_FALSE(same_place(now, kept));
    CHECK(now.placed_y > kept.placed_y);
    CHECK(now.placed_h < kept.placed_h);
    REQUIRE_FALSE(d.sketch->pointers.empty());
    CHECK(d.sketch->pointers.back().phase == canvas_pointer::kLost);
}

TEST_CASE("a pane's words are covered while a menu is open or arranging, no point a press would land on, and the desk still answers") {
    DeskRig d;
    say_rows(d);
    PaneView view;
    REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
    const auto covered_by = [](const PaneView& v, const std::string& what) {
        return std::find(v.covered.by.begin(), v.covered.by.end(), what) != v.covered.by.end();
    };
    d.r.key(input::scan::kA);
    REQUIRE(d.r.session().context.open);
    PaneView under_menu;
    REQUIRE(d.words(kAlphaOffice, "alpha", under_menu).empty());
    CHECK(covered_by(under_menu, "menu"));
    for (const PaneWord& w : under_menu.words) {
        CAPTURE(w.text);
        CHECK(w.space == input::space::kUnknown);
    }
    CHECK(d.desk().menu.open);
    d.r.key(input::scan::kEscape);
    d.r.key(input::scan::kW);
    REQUIRE(d.r.session().arrange.open);
    PaneView arranged;
    REQUIRE(d.words(kAlphaOffice, "alpha", arranged).empty());
    CHECK(covered_by(arranged, "arranging"));
    CHECK(arranged.words.empty());
    CHECK(d.desk().arranging);
    // A PANE NOBODY OFFERED is no pane at all.
    CHECK(d.words("zengine.test.nobody", "none", view).find("unknown") != std::string::npos);
}

namespace {

/// A pane's frame where the window draws it: one rectangle of Workshop's planes exactly the
/// place, in the chrome's voice, which the OS window hands its device as one quad exactly there.
bool frame_drawn(PaneRig& r, const ScreenCanvas& c, const DeskRect& p, bool window) {
    for (const ScreenPlane& layer : c.layers) {
        for (const ScreenRect& rect : layer.rects) {
            if (rect.x != p.x || rect.y != p.y || rect.w != p.w || rect.h != p.h ||
                (rect.role != kPaneChrome && rect.role != kPaneChromeSelected)) {
                continue;
            }
            if (!window) {
                return true;
            }
            for (const zengine::testing::rig_windows::Quad& q : r.windows.quads()) {
                if (q.x == p.x && q.y == p.y && q.w == p.w && q.h == p.h) return true;
            }
            return false;
        }
    }
    return false;
}

} // namespace

TEST_CASE("every place the desk and the words give is where the medium draws it, in a window and in a terminal") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        say_rows(d);
        const Session& s = d.r.session();
        // EVERY ANSWER, read from the bus as an agent reads it.
        std::vector<PaneWord> words;
        for (const auto& [office, pane] : {std::pair{kAlphaOffice, "alpha"}, std::pair{kBetaOffice, "beta"},
                                           std::pair{kCanvasOffice, kCanvasPane}}) {
            PaneView view;
            REQUIRE(d.words(office, pane, view).empty());
            REQUIRE_FALSE(view.words.empty());
            words.insert(words.end(), view.words.begin(), view.words.end());
        }
        const DeskView desk = d.desk();

        // THE DESK WORKSHOP COMPOSED, which the window draws.
        const ScreenCanvas canvas = paint(d.r.session());

        for (const PaneWord& w : words) {
            CAPTURE(w.text);
            // A PANE'S WORDS ARE WHAT THE WINDOW SHOWS ACROSS THEIR PLACE.
            CHECK(shown_word(d.r, w.place) == w.text);
            if (!window) {
                CHECK(w.place.h == gfx::kCellPx);
                // ...and its point is one of those cells, on the console row the terminal reads.
                CHECK(w.x >= gfx::cell_of_pixel(w.place.x));
                CHECK(w.x < gfx::cell_of_pixel(w.place.x + w.place.w));
                CHECK(w.y == gfx::cell_of_pixel(w.place.y));
            }
        }
        // EVERY OPEN PANE'S FRAME, at the place the desk gives.
        std::size_t framed = 0;
        for (const DeskPane& p : desk.panes) {
            if (p.state != "open") continue;
            CAPTURE(p.pane);
            CHECK(frame_drawn(d.r, canvas, p.visible, window));
            ++framed;
        }
        CHECK(framed == 4);

        // AND A MENU'S LINES, where its popup draws them.
        d.r.key(input::scan::kA);
        REQUIRE(s.context.open);
        const DeskView with_menu = d.desk();
        REQUIRE(with_menu.menu.open);
        REQUIRE_FALSE(with_menu.menu.lines.empty());
        for (const PaneWord& line : with_menu.menu.lines) {
            CAPTURE(line.text);
            CHECK(shown_word(d.r, line.place) == line.text);
            if (!window) {
                CHECK(line.place.h == gfx::kCellPx); // one row of the terminal
            }
        }
        d.r.key(input::scan::kEscape);
    }
}

namespace {


/// Where a body's row of glyphs ends, in canvas pixels: its columns from the first glyph's corner.
std::int64_t row_right_edge(const ExternalBodyPlace& body) {
    const RegionFit& fit = body.fit;
    return fit.graphical() ? fit.view.x + fit.origin_x + body.columns * fit.advance_px
                           : fit.view.x + body.columns * gfx::kCellPx;
}

} // namespace

TEST_CASE("a character's point is the cell showing it, a caret moving none, and a press on a cell reaches the pane as the column of the character it shows") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        const std::string text = "abcdef";
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say(m, SeatRows{"alpha", {TextRow{text, gfx::role::kFill}}, {}});
            s.caret(m, "alpha", RowsCaret{0, 2}); // before the `c`
        });
        for (std::int64_t column = 0; column < static_cast<std::int64_t>(text.size()); ++column) {
            CAPTURE(column);
            PaneView view;
            REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
            REQUIRE(view.words.size() == 1);
            REQUIRE(view.words[0].text == text);
            PanePoint at;
            REQUIRE(d.point(PanePointRequested{kAlphaOffice, "alpha", view.stamp.fingerprint, 0, column}, at)
                        .empty());
            if (window) {
                // A window draws the caret as a bar between two characters, which moves none.
                const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
                CHECK(at.x >= view.words[0].place.x + column * body.fit.advance_px);
                CHECK(at.x < view.words[0].place.x + (column + 1) * body.fit.advance_px);
            } else {
                // THE TERMINAL WINDOW'S OWN CELLS: the cell the point names shows that character.
                CHECK(shown_glyph(d.r, at.x, at.y) == text[static_cast<std::size_t>(column)]);
            }
            // ...and a press there reaches the pane as that column.
            d.alpha->presses.clear();
            d.click(at.x, at.y, at.space);
            REQUIRE(d.alpha->presses.size() == 1);
            CHECK(d.alpha->presses[0].row == 0);
            CHECK(d.alpha->presses[0].column == column);
        }
        if (!window) {
            // THE CARET STANDS BEFORE THE CHARACTER IT SITS BEFORE, on its cell, and a press there
            // is that character's column, the caret's own.
            PaneView view;
            REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
            const std::int64_t row = gfx::cell_of_pixel(view.words[0].place.y);
            const std::int64_t x0 = gfx::cell_of_pixel(view.words[0].place.x);
            const std::int64_t caret = x0 + 2;
            REQUIRE(shown_glyph(d.r, caret, row) == 'c');
            CHECK(held_caret(*d.r.session().panes.external_pane(d.alpha_kind)).column == 2);
            d.alpha->presses.clear();
            d.click(caret, row, input::space::kCells);
            REQUIRE(d.alpha->presses.size() == 1);
            CHECK(d.alpha->presses[0].column == 2);
        }
    }
}

TEST_CASE("a row as wide as its pane with a caret in it says every character, at a place inside the pane, the caret past its end on its last cell") {
    for (const auto& medium : {std::pair{false, false}, std::pair{false, true}, std::pair{true, false},
                               std::pair{true, true}}) {
        const bool window = medium.first;
        const bool at_end = medium.second; // the caret past the row's last character
        CAPTURE(window);
        CAPTURE(at_end);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        REQUIRE(body.present);
        std::string full;
        for (std::int64_t i = 0; i < body.columns; ++i) {
            full += static_cast<char>('a' + i % 26);
        }
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say(m, SeatRows{"alpha", {TextRow{full, gfx::role::kFill}}, {}});
            s.caret(m, "alpha", RowsCaret{0, at_end ? body.columns : 3});
        });
        PaneView view;
        REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
        REQUIRE(view.words.size() == 1);
        const PaneWord& w = view.words[0];
        // A caret moves no character in either medium, so every character of the row shows; a
        // terminal shows the caret past the last one on the last cell, which still shows it.
        const std::int64_t shown = body.columns;
        CHECK(w.text == full);
        CHECK(w.place.x + w.place.w <= row_right_edge(body));
        if (!window) {
            CHECK(shown_across(d.r, w.place) == w.text);
            CHECK(w.place.h == gfx::kCellPx);
            // ...the caret on the last cell when it stands past the last character: a caret
            // stands on a drawn cell, and past the room there is none.
            const RowsCaret caret = held_caret(*d.r.session().panes.external_pane(d.alpha_kind));
            CHECK(caret.column == (at_end ? body.columns - 1 : 3));
        }
        // THE LAST CHARACTER SHOWN HAS A POINT, AND A PRESS THERE IS ITS COLUMN...
        PanePoint at;
        REQUIRE(d.point(PanePointRequested{kAlphaOffice, "alpha", view.stamp.fingerprint, 0, shown - 1}, at)
                    .empty());
        d.alpha->presses.clear();
        d.click(at.x, at.y, at.space);
        REQUIRE(d.alpha->presses.size() == 1);
        CHECK(d.alpha->presses[0].column == shown - 1);
        // ...AND A COLUMN PAST THE BODY HAS NONE.
        CHECK(d.point(PanePointRequested{kAlphaOffice, "alpha", view.stamp.fingerprint, 0, shown}, at)
                  .find("outside") != std::string::npos);
    }
}

TEST_CASE("a row with nothing on it but the caret says no word, and a press on the caret's own cell reaches the pane, in a text row and in a canvas field at the body's right edge") {
    // THE WINDOW'S READING IS ITS RUNS OF TEXT: a caret on an empty row is a mark the window draws,
    // no character, so the reading says no word there -- and the cell is still the pane's.
    {
        DeskRig d;
        d.r.drive(d.alpha, [](ProviderSeat& s, loom::Mail& m) {
            s.say(m, SeatRows{"alpha", {TextRow{"", gfx::role::kFill},
                                        TextRow{"below", gfx::role::kFill}}, {}});
            s.caret(m, "alpha", RowsCaret{0, 0});
        });
        PaneView view;
        REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
        REQUIRE(view.words.size() == 1);
        CHECK(view.words[0].text == "below");
        CHECK(held_caret(*d.r.session().panes.external_pane(d.alpha_kind)).row == 0);
        CHECK(held_caret(*d.r.session().panes.external_pane(d.alpha_kind)).column == 0);
        d.alpha->presses.clear();
        press_pane(d.r, d.alpha_kind, 0, 0);
        REQUIRE(d.alpha->presses.size() == 1);
        CHECK(d.alpha->presses[0].row == 0);
        CHECK(d.alpha->presses[0].column == 0);
    }
    // AN EMPTY FIELD IN A CANVAS PANE'S LAST CELL: no word, and a press there reaches the pane.
    {
        SketchRig d;
        const CanvasRoom room = d.sketch->room();
        CanvasPicture p;
        CanvasText field{room.width - kPaneCanvasUnit, 2 * kPaneCanvasUnit, "", gfx::role::kFill};
        field.caret_col = 0;
        p.texts.push_back(field);
        d.draw_picture(p);
        PaneView view;
        REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
        CHECK(view.words.empty());
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        const DeskRect cell{pane.placed_x + field.x, pane.placed_y + field.y, gfx::kCellPx,
                            gfx::kCellPx};
        d.sketch->pointers.clear();
        d.click(gfx::cell_of_pixel(cell.x), gfx::cell_of_pixel(cell.y), input::space::kCells);
        REQUIRE_FALSE(d.sketch->pointers.empty());
        CHECK(inside_locally(d.sketch->pointers.front(), cell, pane));
    }
}

namespace {

const PanePart* part_named(const std::vector<PanePart>& parts, const std::string& name) {
    for (const PanePart& p : parts) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

std::vector<std::string> names_of(const std::vector<PanePart>& parts) {
    std::vector<std::string> out;
    for (const PanePart& p : parts) out.push_back(p.name);
    std::sort(out.begin(), out.end());
    return out;
}

/// One named row of a text pane, and the columns a press on it must land in.
struct NamedRun {
    const char* name;
    std::int64_t row, from, to;
};

/// A press at a part's point reaches the text pane on its row, at a column inside it.
void press_lands(DeskRig& d, const PanePart& part, const NamedRun& run) {
    CAPTURE(part.name);
    d.alpha->presses.clear();
    d.click(part.x, part.y, part.space);
    REQUIRE(d.alpha->presses.size() == 1);
    CHECK(d.alpha->presses[0].row == run.row);
    CHECK(d.alpha->presses[0].column >= run.from);
    CHECK(d.alpha->presses[0].column < run.to);
}

} // namespace

TEST_CASE("a part whose one cell holds the caret at the body's right edge has a point there, and a press on it reaches the pane") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        REQUIRE(body.present);
        std::string full;
        for (std::int64_t i = 0; i < body.columns; ++i) {
            full += static_cast<char>('a' + i % 26);
        }
        const std::int64_t edge = body.columns - 1;
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say(m, SeatRows{"alpha", {TextRow{full, gfx::role::kFill}}, {RowPart{"control:edge", 0, edge, 1}}});
            s.caret(m, "alpha", RowsCaret{0, edge});
        });
        PaneView view;
        REQUIRE(d.parts(kAlphaOffice, "alpha", view).empty());
        const PanePart* part = part_named(view.parts, "control:edge");
        REQUIRE(part != nullptr);
        CHECK(part->text == full.substr(static_cast<std::size_t>(edge)));
        REQUIRE(part->space == (window ? input::space::kPixels : input::space::kCells));
        CHECK(held_caret(*d.r.session().panes.external_pane(d.alpha_kind)).column == edge);
        d.alpha->presses.clear();
        d.click(part->x, part->y, part->space);
        REQUIRE(d.alpha->presses.size() == 1);
        CHECK(d.alpha->presses[0].row == 0);
        CHECK(d.alpha->presses[0].column == edge);
    }
}

TEST_CASE("a text pane's named parts are said under the pane's own names over the cells that show them, and a press at a part's point lands on it, in a window and in a terminal") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        REQUIRE(body.columns >= 20);
        // The caret before `ess`, which moves no character in either medium.
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say(m, SeatRows{"alpha",
                                           {TextRow{"first row", gfx::role::kFill},
                                            TextRow{"press [Save] here", gfx::role::kFill},
                                            TextRow{"", gfx::role::kFill}},
                                           {RowPart{"row:first", 0, 0, body.columns},
                                            RowPart{"control:save", 1, 6, 6},
                                            RowPart{"field:empty", 2, 0, 4}}});
            s.caret(m, "alpha", RowsCaret{1, 2});
        });
        PaneView view;
        REQUIRE(d.parts(kAlphaOffice, "alpha", view).empty());
        // THE WINDOW'S WORDS ARE ITS RUNS OF TEXT: the empty row says none.
        REQUIRE(view.words.size() == 2);
        REQUIRE(view.parts.size() == 3);
        const PanePart* first = part_named(view.parts, "row:first");
        const PanePart* save = part_named(view.parts, "control:save");
        const PanePart* empty = part_named(view.parts, "field:empty");
        REQUIRE(first != nullptr);
        REQUIRE(save != nullptr);
        REQUIRE(empty != nullptr);
        CHECK(first->text == "first row");
        CHECK(save->text == "[Save]");
        CHECK(empty->text.empty());
        const std::int64_t advance = window ? body.fit.advance_px : gfx::kCellPx;
        const std::int64_t line = window ? body.fit.line_px : gfx::kCellPx;
        const std::int64_t left = view.words[0].place.x;
        // A ROW ENTIRE COVERS THE BODY'S COLUMNS, A CONTROL ITS OWN, and an empty field its own,
        // though nothing is drawn in it.
        CHECK(first->place.x == left);
        CHECK(first->place.w == body.columns * advance);
        CHECK(first->place.y == view.words[0].place.y);
        CHECK(save->place.x == left + 6 * advance);
        CHECK(save->place.w == 6 * advance);
        CHECK(save->place.y == view.words[1].place.y);
        CHECK(save->place.h == line);
        CHECK(empty->place.x == left);
        CHECK(empty->place.w == 4 * advance);
        for (const PanePart& p : view.parts) {
            CHECK(p.space == (window ? input::space::kPixels : input::space::kCells));
        }
        // EACH PART'S POINT IS A PRESS ON ITS ROW, at a column inside it.
        press_lands(d, *first, NamedRun{"row:first", 0, 0, body.columns});
        press_lands(d, *save, NamedRun{"control:save", 1, 6, 12});
        press_lands(d, *empty, NamedRun{"field:empty", 2, 0, 4});
        // ...AND THE WORDS ARE SAID BESIDE THEM.
        PaneView words;
        REQUIRE(d.words(kAlphaOffice, "alpha", words).empty());
        CHECK(words.words.size() == 2);
    }
}

TEST_CASE("a part's point is a place of its own: beside a control a row holds, on a row's blank cell where its text is a control's, and none for a row with no place of its own") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        REQUIRE(body.columns >= 20);
        // THE ROW'S MIDDLE CHARACTER IS THE MARK'S: of `> [open] Files`'s fourteen, the seventh
        // stands inside `[open]`. The second row is two controls and nothing of its own; the
        // third's every character is `[Go]`'s, and the blank cells after it are the row's.
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say(m, SeatRows{"alpha",
                                           {TextRow{"> [open] Files", gfx::role::kFill},
                                            TextRow{"[one][two]", gfx::role::kFill},
                                            TextRow{"[Go]", gfx::role::kFill}},
                                           {RowPart{"pane:files", 0, 0, body.columns},
                                            RowPart{"mark:files", 0, 2, 6},
                                            RowPart{"row:both", 1, 0, 10},
                                            RowPart{"control:one", 1, 0, 5},
                                            RowPart{"control:two", 1, 5, 5},
                                            RowPart{"row:go", 2, 0, body.columns},
                                            RowPart{"control:go", 2, 0, 4}}});
        });
        PaneView view;
        REQUIRE(d.parts(kAlphaOffice, "alpha", view).empty());
        REQUIRE(view.parts.size() == 6);
        const PanePart* row = part_named(view.parts, "pane:files");
        REQUIRE(row != nullptr);
        CHECK(row->text == "> [open] Files");
        d.alpha->presses.clear();
        d.click(row->x, row->y, row->space);
        REQUIRE(d.alpha->presses.size() == 1);
        CHECK(d.alpha->presses[0].row == 0);
        // ON `F`: of the eight characters beside the mark, the middle one.
        CHECK(d.alpha->presses[0].column == 9);
        press_lands(d, *part_named(view.parts, "mark:files"), NamedRun{"mark:files", 0, 2, 8});
        press_lands(d, *part_named(view.parts, "control:one"), NamedRun{"control:one", 1, 0, 5});
        press_lands(d, *part_named(view.parts, "control:two"), NamedRun{"control:two", 1, 5, 10});
        press_lands(d, *part_named(view.parts, "control:go"), NamedRun{"control:go", 2, 0, 4});
        // A PART WHOSE EVERY CHARACTER IS ANOTHER'S is pressed on a blank cell of its own...
        press_lands(d, *part_named(view.parts, "row:go"), NamedRun{"row:go", 2, 4, body.columns});
        CHECK(d.alpha->presses[0].column == 4 + (body.columns - 5) / 2);
        // ...AND ONE WITH NO PLACE OF ITS OWN IS NOT SAID: the window draws nothing of it, so no
        // press reaches it, and none is given another part's.
        CHECK(part_named(view.parts, "row:both") == nullptr);
    }
}

TEST_CASE("a row map's parts are named by what each span means, and a name nothing, one the judge would refuse or one taken is left unnamed") {
    component::RowMap<std::string> map;
    map.begin();
    REQUIRE(map.span(0, 2, 4, 20, "control"));
    map.row(0, "first");
    map.row(1, "");
    map.row(2, "first");
    map.row(3, std::string(kMaxPanePartNameLen + 1, 'x'));
    map.row(4, "tab\there");
    (void)map.settle();
    const std::vector<RowPart> parts =
        row_parts(map, 20, [](const std::string& meaning) { return meaning; });
    // EVERY SPAN IS A PLACE, listed as a press reads them: on a row the row entire first and its
    // runs after it, so the one `at` answers is the last that holds a place.
    REQUIRE(parts.size() == 6);
    CHECK(parts[0].name == "first");
    CHECK(parts[0].row == 0);
    CHECK(parts[0].column == 0);
    CHECK(parts[0].columns == 20);
    CHECK(parts[1].name == "control");
    CHECK(parts[1].row == 0);
    CHECK(parts[1].column == 2);
    CHECK(parts[1].columns == 4);
    for (std::size_t i = 2; i < parts.size(); ++i) {
        CAPTURE(i);
        CHECK(parts[i].name.empty());
        CHECK(parts[i].row == static_cast<std::int64_t>(i) - 1);
        CHECK(parts[i].columns == 20);
    }
    CHECK(row_parts_problem(parts, 5, 20).empty());
    // ...AND A LIST GATHERED BY HAND IS HELD TO THE SAME RULE: every place kept, and past the
    // bound the earliest go, the ones every later place is over.
    PartNames<CanvasPart> picture;
    CHECK(picture.add(CanvasPart{"node", 0, 0, 12, 12}));
    CHECK_FALSE(picture.add(CanvasPart{"node", 12, 0, 12, 12}));
    CHECK_FALSE(picture.add(CanvasPart{"", 24, 0, 12, 12}));
    std::size_t named = 0;
    for (std::size_t i = 3; i <= kMaxPaneParts; ++i) {
        named += picture.add(CanvasPart{"n" + std::to_string(i), 0, 0, 1, 1}) ? 1u : 0u;
    }
    CHECK(named == kMaxPaneParts - 2);
    const std::vector<CanvasPart> kept = picture.take();
    REQUIRE(kept.size() == kMaxPaneParts);
    CHECK(kept[0].name.empty());
    CHECK(kept[0].x == 12);
    CHECK(kept[1].name.empty());
    CHECK(kept[1].x == 24);
    CHECK(kept.back().name == "n" + std::to_string(kMaxPaneParts));
    CHECK(canvas_parts_problem(kept).empty());
}

TEST_CASE("a part keeps its name across its pane's redraws, and is said and pressed where the redraw put it") {
    DeskRig d;
    const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
    const auto draw = [&](std::vector<std::string> rows, std::vector<RowPart> parts) {
        SeatRows c{"alpha", {}, std::move(parts)};
        for (std::string& text : rows) {
            c.rows.push_back(TextRow{std::move(text), gfx::role::kFill});
        }
        d.r.drive(d.alpha, [c](ProviderSeat& s, loom::Mail& m) { s.say(m, c); });
    };
    draw({"alpha", "[Save]"},
         {RowPart{"row:alpha", 0, 0, body.columns}, RowPart{"control:save", 1, 0, 6}});
    PaneView before;
    REQUIRE(d.parts(kAlphaOffice, "alpha", before).empty());
    const PanePart* was = part_named(before.parts, "control:save");
    REQUIRE(was != nullptr);
    // THE REDRAW: a row arrives above, and the control moves right along its own row.
    draw({"a notice", "alpha", "x [Save]"},
         {RowPart{"row:alpha", 1, 0, body.columns}, RowPart{"control:save", 2, 2, 6}});
    PaneView after;
    REQUIRE(d.parts(kAlphaOffice, "alpha", after).empty());
    CHECK(names_of(after.parts) == names_of(before.parts));
    const PanePart* now = part_named(after.parts, "control:save");
    REQUIRE(now != nullptr);
    CHECK(now->text == "[Save]");
    CHECK(now->place.y > was->place.y);
    CHECK(now->place.x > was->place.x);
    CHECK(part_named(after.parts, "row:alpha")->text == "alpha");
    press_lands(d, *now, NamedRun{"control:save", 2, 2, 8});
}

TEST_CASE("a canvas pane's named parts are said where its body shows them, with the words inside them, and a press at a part's point lands inside it in the pane's own canvas, in a window and in a terminal") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        const CanvasRoom room = d.sketch->room();
        const std::int64_t u = kPaneCanvasUnit;
        // THE TYPED RUN'S OWN BOX, as a provider lays it out (`clip_canvas_text`).
        const CanvasPicture picture = d.picture();
        const CanvasTextLayout typed = clip_canvas_text(picture.texts[1], {0, 0, room.width, room.height}, room);
        REQUIRE(typed.visible());
        d.draw_named({CanvasPart{"tool:label", 2 * u, 2 * u, 7 * u, u},
                      CanvasPart{"node", 0, 0, 10 * u, u},
                      CanvasPart{"field:typed", typed.bounds.x, typed.bounds.y, typed.bounds.w,
                                     typed.bounds.h},
                      CanvasPart{"edge", room.width - u / 2, 6 * u, u, u},
                      CanvasPart{"gone", room.width + u, 0, u, u}});
        REQUIRE(d.sketch->rejected.empty());
        PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        // A PART THE ROOM DOES NOT SHOW IS NOT SAID; one it cuts is said as far as it shows.
        CHECK(names_of(view.parts) == std::vector<std::string>{"edge", "field:typed", "node", "tool:label"});
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        const PanePart* label = part_named(view.parts, "tool:label");
        REQUIRE(label != nullptr);
        CHECK(label->text == "[Label]");
        CHECK(label->place.x == pane.placed_x + 2 * u);
        CHECK(label->place.y == pane.placed_y + 2 * u);
        CHECK(label->place.w == 7 * u);
        CHECK(label->place.h == u);
        CHECK(part_named(view.parts, "node")->text == "node one");
        CHECK(part_named(view.parts, "field:typed")->text == "typed here");
        const PanePart* edge = part_named(view.parts, "edge");
        REQUIRE(edge != nullptr);
        CHECK(edge->text.empty());
        // ...as far as the medium shows it: half a cell in the OS window, the cell a terminal
        // paints it on.
        CHECK(edge->place.w == (window ? u / 2 : u));
        CHECK(edge->place.x + edge->place.w == pane.placed_x + pane.placed_w);
        for (const PanePart& p : view.parts) {
            CAPTURE(p.name);
            d.sketch->pointers.clear();
            d.click(p.x, p.y, p.space);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            CHECK(d.sketch->pointers.front().phase == canvas_pointer::kPress);
            if (window) {
                CHECK(inside_locally(d.sketch->pointers.front(), p.place, pane));
            } else {
                // A terminal names a cell it paints the part on, by that cell's corner.
                const auto& e = d.sketch->pointers.front();
                const std::int64_t x = e.x + pane.placed_x;
                const std::int64_t y = e.y + pane.placed_y;
                CHECK(x + u > p.place.x);
                CHECK(x < p.place.x + p.place.w);
                CHECK(y + u > p.place.y);
                CHECK(y < p.place.y + p.place.h);
            }
        }
    }
}

TEST_CASE("a canvas part's point is its own: a part with another over its centre is pressed where nothing inside it is, and a part a terminal paints on no cell is not said there") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        const std::int64_t u = kPaneCanvasUnit;
        const std::vector<CanvasPart> parts{CanvasPart{"canvas", 0, 6 * u, 12 * u, 5 * u},
                                                CanvasPart{"element:a", 4 * u, 7 * u, 4 * u, 3 * u},
                                                CanvasPart{"handle:a", 7 * u, 9 * u, u, u},
                                                CanvasPart{"tiny", u / 4, 0, u / 2, u / 2}};
        d.draw_named(parts);
        REQUIRE(d.sketch->rejected.empty());
        PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        // A PART SMALLER THAN A CELL AND INSIDE ONE paints on no cell of a terminal, so it is not
        // said there; a window shows it.
        CHECK(names_of(view.parts) == (window ? std::vector<std::string>{"canvas", "element:a", "handle:a", "tiny"}
                                              : std::vector<std::string>{"canvas", "element:a", "handle:a"}));
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        const std::int64_t grain = window ? gfx::kPixelGrainPx : gfx::kCellGrainPx;
        const auto lands = [&](const SketchPointer& e, const CanvasPart& part) {
            return gfx::px_span_contains(pane.placed_x + part.x, part.w, pane.placed_x + e.x, grain) &&
                   gfx::px_span_contains(pane.placed_y + part.y, part.h, pane.placed_y + e.y, grain);
        };
        // EACH PART'S POINT IS A PRESS ON IT, by the paint rule read backwards, and on no part
        // inside it: `canvas` beside `element:a`, `element:a` beside its handle.
        for (const CanvasPart& part : parts) {
            const PanePart* said = part_named(view.parts, part.name);
            if (said == nullptr) continue;
            CAPTURE(part.name);
            d.sketch->pointers.clear();
            d.click(said->x, said->y, said->space);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            const SketchPointer& e = d.sketch->pointers.front();
            CHECK(lands(e, part));
            for (const CanvasPart& inner : parts) {
                if (&inner != &part && inner.x >= part.x && inner.y >= part.y &&
                    inner.x + inner.w <= part.x + part.w && inner.y + inner.h <= part.y + part.h) {
                    CAPTURE(inner.name);
                    CHECK_FALSE(lands(e, inner));
                }
            }
        }
    }
}

namespace {

/// THE PART A PRESS REACHES AS A PANE READS ITS PARTS: the last listed that holds the place, by
/// the paint rule read backwards -- every shipped canvas pane's `hit` -- or nullptr.
const CanvasPart* reached(const std::vector<CanvasPart>& parts, const SketchPointer& e,
                              std::int64_t grain) {
    for (auto at = parts.rbegin(); at != parts.rend(); ++at) {
        if (gfx::px_span_contains(at->x, at->w, e.x, grain) &&
            gfx::px_span_contains(at->y, at->h, e.y, grain)) {
            return &*at;
        }
    }
    return nullptr;
}

/// A picture a shipped canvas pane drew, drawn as the sketch pane's, for the room last given.
void draw_as_sketch(SketchRig& d, const CanvasPicture& named) { d.draw_picture(named); }

/// A VIEW'S PICTURE NAMING ITS ELEMENTS over the places a press on each lands: a button whose
/// intent the view declares `control:<id>`, every other element `element:<id>`.
CanvasPicture named_view(const zengine::view::Description& d, const zengine::view::Picture& p) {
    CanvasPicture out = p.content;
    for (const zengine::view::Hit& hit : p.hits) {
        const bool control = d.intent(hit.element) != nullptr;
        out.parts.push_back(CanvasPart{(control ? "control:" : "element:") + hit.element, hit.x,
                                       hit.y, hit.w, hit.h});
    }
    return out;
}

} // namespace

TEST_CASE("a part under another is pressed where a press reaches it as its pane reads a press: the later of two parts takes the place they share, and so does a place the pane names nothing, in a window and in a terminal") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        const std::int64_t u = kPaneCanvasUnit;
        const std::int64_t grain = window ? gfx::kPixelGrainPx : gfx::kCellGrainPx;
        const auto every_point_reaches = [&](const std::vector<CanvasPart>& parts,
                                             const std::vector<std::string>& said) {
            d.sketch->rejected.clear();
            d.draw_named(parts);
            CHECK(d.sketch->rejected.empty());
            if (!d.sketch->rejected.empty()) {
                CAPTURE(d.sketch->rejected.front().reason);
                return;
            }
            PaneView view;
            REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
            CHECK(names_of(view.parts) == said);
            for (const PanePart& part : view.parts) {
                CAPTURE(part.name);
                d.sketch->pointers.clear();
                d.click(part.x, part.y, part.space);
                REQUIRE_FALSE(d.sketch->pointers.empty());
                const CanvasPart* got = reached(parts, d.sketch->pointers.front(), grain);
                REQUIRE(got != nullptr);
                CHECK(got->name == part.name);
            }
        };
        // `second` IS LISTED AFTER `first` AND LIES OVER ITS CENTRE: a press there is `second`'s.
        every_point_reaches({CanvasPart{"first", 2 * u, 2 * u, 8 * u, 2 * u},
                             CanvasPart{"second", 5 * u, 2 * u, 8 * u, 2 * u}},
                            {"first", "second"});
        // A PLACE THE PANE NAMES NOTHING lies over `third`'s centre, is said nowhere, and is still
        // the place a press there reaches.
        every_point_reaches({CanvasPart{"third", 2 * u, 6 * u, 10 * u, 2 * u},
                             CanvasPart{"", 5 * u, 6 * u, 2 * u, 2 * u}},
                            {"third"});
    }
}

TEST_CASE("two overlapping buttons in a running view are each pressed where the view gives that button the press") {
    namespace view = zengine::view;
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        // `second` is drawn after `first` and over its centre, in pixels and in the cells a
        // terminal floors them to.
        view::Description v;
        v.name = "two.buttons";
        v.width = 240;
        v.height = 48;
        v.elements = {{"first", view::Kind::button, "First", 12, 12, 96, 24, ""},
                      {"second", view::Kind::button, "Second", 48, 12, 96, 24, ""}};
        REQUIRE(view::problem(v).empty());
        const view::Picture drawn = view::picture(v, {}, view::Presentation{}, d.sketch->room());
        draw_as_sketch(d, named_view(v, drawn));
        REQUIRE(d.sketch->rejected.empty());
        PaneView said;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, said).empty());
        for (const std::string id : {"first", "second"}) {
            CAPTURE(id);
            const PanePart* part = part_named(said.parts, "element:" + id);
            REQUIRE(part != nullptr);
            d.sketch->pointers.clear();
            d.click(part->x, part->y, part->space);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            const view::Hit* hit = drawn.hit(d.sketch->pointers.front().x, d.sketch->pointers.front().y);
            REQUIRE(hit != nullptr);
            CHECK(hit->element == id);
        }
    }
}

TEST_CASE("the View Builder's two overlapping elements are each pressed where the builder gives that element the press, in a window and in a terminal") {
    namespace vb = zengine::view_builder;
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        // `second` is placed after `first`, over its centre and reaching below it: where the
        // design area cuts both at its edge, it still lies over `first` and not inside it.
        vb::Model m;
        for (const std::vector<std::string>& edit :
             std::vector<std::vector<std::string>>{
                 {"new", "two.buttons", "discard"},
                 {"add", "button"}, {"element", "0", "first", "First", "12", "12", "96", "24"},
                 {"add", "button"}, {"element", "1", "second", "Second", "48", "12", "96", "36"}}) {
            m.command(edit.front(), std::vector<std::string>(edit.begin() + 1, edit.end()));
        }
        REQUIRE(m.description.elements.size() == 2);
        vb::Presentation shown;
        const vb::Picture drawn = vb::picture(m, shown, d.sketch->room());
        draw_as_sketch(d, vb::named(drawn, m.description));
        REQUIRE(d.sketch->rejected.empty());
        PaneView said;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, said).empty());
        for (const std::size_t at : {std::size_t{0}, std::size_t{1}}) {
            const std::string& id = m.description.elements[at].id;
            CAPTURE(id);
            const PanePart* part = part_named(said.parts, "element:" + id);
            REQUIRE(part != nullptr);
            d.sketch->pointers.clear();
            d.click(part->x, part->y, part->space);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            const vb::Hit* hit = drawn.hit(d.sketch->pointers.front().x, d.sketch->pointers.front().y);
            REQUIRE(hit != nullptr);
            CHECK(hit->action == "element");
            CHECK(hit->args == std::vector<std::string>{std::to_string(at)});
        }
    }
}

TEST_CASE("a canvas part's own place is sought over all of it, and a part with none has no point, never another's") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        const std::int64_t u = kPaneCanvasUnit;
        const std::int64_t grain = window ? gfx::kPixelGrainPx : gfx::kCellGrainPx;
        // `lined` has places it names nothing over its centre, top and bottom rows and its centre,
        // left and right columns, and its own places between them; `covered` lies under two parts;
        // `centred` has one over its left end, beside its centre.
        const std::vector<CanvasPart> parts{CanvasPart{"lined", 0, 0, 10 * u, 10 * u},
                                                CanvasPart{"", 0, 0, 10 * u, u},
                                                CanvasPart{"", 0, 9 * u, 10 * u, u},
                                                CanvasPart{"", 0, 4 * u, 10 * u, 2 * u},
                                                CanvasPart{"", 0, 0, u, 10 * u},
                                                CanvasPart{"", 9 * u, 0, u, 10 * u},
                                                CanvasPart{"", 4 * u, 0, 2 * u, 10 * u},
                                                CanvasPart{"covered", 12 * u, 0, 4 * u, 2 * u},
                                                CanvasPart{"cover:left", 12 * u, 0, 2 * u, 2 * u},
                                                CanvasPart{"cover:right", 14 * u, 0, 2 * u, 2 * u},
                                                CanvasPart{"centred", 18 * u, 0, 6 * u, 2 * u},
                                                CanvasPart{"", 18 * u, 0, u, 2 * u}};
        d.draw_named(parts);
        REQUIRE(d.sketch->rejected.empty());
        PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        CHECK(names_of(view.parts) ==
              std::vector<std::string>{"centred", "cover:left", "cover:right", "covered", "lined"});
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        // A UNIT OF THE BODY, as the medium's own numbers say it.
        const auto unit_at = [&](std::int64_t px, std::int64_t py) {
            return window ? std::pair{pane.placed_x + px, pane.placed_y + py}
                          : std::pair{gfx::cell_of_pixel(pane.placed_x) + px / u,
                                      gfx::cell_of_pixel(pane.placed_y) + py / u};
        };
        for (const PanePart& part : view.parts) {
            CAPTURE(part.name);
            // `lined` AND `centred` ARE POINTED WHERE THEY ARE THEIR OWN -- the press below reaches
            // each -- never on a place over them that names nothing.
            CHECK((part.space != input::space::kUnknown || part.name == "covered"));
            (void)unit_at;
            if (part.name == "covered") {
                // SAID WITH ITS PLACE AND NO POINT: no press reaches it, and none is given another's.
                CHECK(part.place.x == pane.placed_x + 12 * u);
                CHECK(part.place.w == 4 * u);
                CHECK(part.space == input::space::kUnknown);
                CHECK(part.x == 0);
                CHECK(part.y == 0);
                continue;
            }
            d.sketch->pointers.clear();
            d.click(part.x, part.y, part.space);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            const CanvasPart* got = reached(parts, d.sketch->pointers.front(), grain);
            REQUIRE(got != nullptr);
            CHECK(got->name == part.name);
        }
    }
}

TEST_CASE("a canvas picture its office's holder no longer holds says its words and parts with no point, "
          "gives none, and a press where one was reaches nothing") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        d.draw_named({CanvasPart{"node", kPaneCanvasUnit, 0, 8 * kPaneCanvasUnit, kPaneCanvasUnit}});
        REQUIRE(d.sketch->rejected.empty());
        PaneView held;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, held).empty());
        REQUIRE(held.parts.size() == 1);
        REQUIRE(held.parts[0].space != input::space::kUnknown);
        // THE OFFICE HELD BY NO ONE NOW, the picture standing in the window until the desk places
        // it afresh: its holder gone from the bus, kept here to say what reached it.
        auto gone = d.r.bus.unregister_weave(d.sketch_id);
        REQUIRE(gone);
        PaneView now;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, now).empty());
        CHECK(now.stamp.fingerprint == held.stamp.fingerprint);
        REQUIRE(now.words.size() == held.words.size());
        for (std::size_t i = 0; i < now.words.size(); ++i) {
            CAPTURE(now.words[i].text);
            CHECK(now.words[i].text == held.words[i].text);
            CHECK(now.words[i].place.x == held.words[i].place.x);
            CHECK(now.words[i].place.w == held.words[i].place.w);
            CHECK(now.words[i].x == 0);
            CHECK(now.words[i].y == 0);
            CHECK(now.words[i].space == input::space::kUnknown);
        }
        REQUIRE(now.parts.size() == 1);
        CHECK(now.parts[0].name == "node");
        CHECK(now.parts[0].place.x == held.parts[0].place.x);
        CHECK(now.parts[0].place.w == held.parts[0].place.w);
        CHECK(now.parts[0].x == 0);
        CHECK(now.parts[0].y == 0);
        CHECK(now.parts[0].space == input::space::kUnknown);
        // THE POINT DOOR GIVES NONE...
        PanePoint character;
        CHECK(d.point(PanePointRequested{kCanvasOffice, kCanvasPane, now.stamp.fingerprint, 0, 0}, character)
                  .find("no press reaches that word now") != std::string::npos);
        // ...AS A PRESS WHERE THE PART'S POINT WAS REACHES NOTHING.
        d.sketch->pointers.clear();
        d.click(held.parts[0].x, held.parts[0].y, held.parts[0].space);
        CHECK(d.sketch->pointers.empty());
    }
}

TEST_CASE("a picture of as many parts as it may name gives each a point a press there gives it, or none") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        const std::int64_t u = kPaneCanvasUnit;
        const std::int64_t grain = window ? gfx::kPixelGrainPx : gfx::kCellGrainPx;
        const CanvasRoom room = d.sketch->room();
        // SQUARES NESTED IN RUNS, EACH INSIDE THE LAST AND A LATER RUN OVER AN EARLIER, THEN STRIPS
        // AND BOXES STREWN OVER THEM, some past the room's edge and some smaller than a cell,
        // every one named -- as many as a window keeps pressed parts in one area.
        std::vector<CanvasPart> parts;
        const std::size_t most = (std::min)(kMaxPaneParts, gfx::kMaxElementsPerArea);
        for (std::int64_t i = 0; parts.size() < most; ++i) {
            const std::string name = "p" + std::to_string(i);
            if (i < 680) {
                const std::int64_t inset = i % 68;
                parts.push_back(CanvasPart{name, inset, inset, room.width - 2 * inset,
                                               room.height - 2 * inset});
            } else if (i % 2 == 0) {
                parts.push_back(CanvasPart{name, (i * 37) % room.width - u, (i * 11) % room.height,
                                               (i * 13) % (3 * u) + 1, u / 4 + i % 5});
            } else {
                parts.push_back(CanvasPart{name, (i * 53) % room.width, (i * 29) % room.height - u,
                                               u / 2 + (i * 7) % (2 * u), (i * 3) % (2 * u) + 1});
            }
        }
        // OVER ONE RECT AND NO CARET, so each part repaints one node and asks the only records.
        CanvasPicture over;
        over.rects.push_back(CanvasRect{0, 0, room.width, room.height, gfx::role::kMuted});
        over.parts = parts;
        d.draw_picture(over);
        REQUIRE(d.sketch->rejected.empty());
        PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        // WHICH PART EACH UNIT OF THE BODY GIVES A PRESS TO, as every shipped canvas pane reads one.
        const std::int64_t across = room.width / grain, down = room.height / grain;
        std::vector<std::size_t> owner(static_cast<std::size_t>(across * down), parts.size());
        std::vector<char> owns(parts.size(), 0);
        for (std::int64_t y = 0; y < down; ++y) {
            for (std::int64_t x = 0; x < across; ++x) {
                for (std::size_t i = parts.size(); i-- > 0;) {
                    if (gfx::px_span_contains(parts[i].x, parts[i].w, x * grain, grain) &&
                        gfx::px_span_contains(parts[i].y, parts[i].h, y * grain, grain)) {
                        owner[static_cast<std::size_t>(y * across + x)] = i;
                        owns[i] = 1;
                        break;
                    }
                }
            }
        }
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        std::size_t pointed = 0, none = 0;
        for (const PanePart& part : view.parts) {
            CAPTURE(part.name);
            const auto at = static_cast<std::size_t>(std::stoll(part.name.substr(1)));
            if (part.space == input::space::kUnknown) {
                CHECK_FALSE(owns[at]);
                ++none;
                continue;
            }
            CHECK(owns[at]);
            ++pointed;
            const PointedAt pressed = canvas_point_of(part.space, part.x, part.y);
            REQUIRE(pressed.understood);
            const std::int64_t x = gfx::floor_div_px(pressed.px.x - pane.placed_x, grain);
            const std::int64_t y = gfx::floor_div_px(pressed.px.y - pane.placed_y, grain);
            REQUIRE(x >= 0);
            REQUIRE(x < across);
            REQUIRE(y >= 0);
            REQUIRE(y < down);
            CHECK(owner[static_cast<std::size_t>(y * across + x)] == at);
        }
        CHECK(pointed > 0);
        CHECK(none > 0);
        MESSAGE("parts said " << view.parts.size() << ", with a point " << pointed << ", with none " << none);
    }
}

TEST_CASE("a row part is pressed where its row map gives it the press, not on a narrower run over it nor an unnamed one") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        REQUIRE(body.columns >= 20);
        // `field:narrow` is narrower than `field:wide` and lies over its middle without lying
        // inside it; the run in `one two three`'s middle means nothing the pane names.
        component::RowMap<std::string> map;
        map.begin();
        map.row(0, "row:runs");
        REQUIRE(map.span(0, 0, 12, 16, "field:wide"));
        REQUIRE(map.span(0, 4, 10, 16, "field:narrow"));
        map.row(1, "row:one");
        REQUIRE(map.span(1, 4, 3, 13, ""));
        (void)map.settle();
        const std::vector<RowPart> parts =
            row_parts(map, body.columns, [](const std::string& meaning) { return meaning; });
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say(m, SeatRows{"alpha",
                                           {TextRow{"0123456789abcdef", gfx::role::kFill},
                                            TextRow{"one two three", gfx::role::kFill}},
                                           parts});
        });
        PaneView view;
        REQUIRE(d.parts(kAlphaOffice, "alpha", view).empty());
        CHECK(names_of(view.parts) ==
              std::vector<std::string>{"field:narrow", "field:wide", "row:one", "row:runs"});
        for (const PanePart& part : view.parts) {
            CAPTURE(part.name);
            d.alpha->presses.clear();
            d.click(part.x, part.y, part.space);
            REQUIRE(d.alpha->presses.size() == 1);
            const std::string* meaning = map.at(d.alpha->presses[0].row, d.alpha->presses[0].column);
            REQUIRE(meaning != nullptr);
            CHECK(*meaning == part.name);
        }
    }
}

TEST_CASE("the desk names the lines of Workshop's own menu by the action or group each shows, over the line it is drawn on") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        d.r.key(input::scan::kA);
        REQUIRE(d.r.session().context.open);
        const Session& s = d.r.session();
        const Screen sc = screen_of(s);
        const DeskView desk = d.desk();
        REQUIRE(desk.menu.open);
        const std::vector<std::string> names = context_line_names(s, sc);
        REQUIRE(names.size() == desk.menu.lines.size());
        const std::vector<ContextEntry> population = context_population(s.context);
        std::size_t named = 0;
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (names[i].empty()) continue;
            ++named;
            CAPTURE(names[i]);
            const PanePart* part = part_named(desk.menu.parts, names[i]);
            REQUIRE(part != nullptr);
            CHECK(part->text == desk.menu.lines[i].text);
            CHECK(part->place.y == desk.menu.lines[i].place.y);
            // ITS POINT IS THAT LINE, as the menu's own press measurer reads it.
            const PointedAt at = canvas_point_of(part->space, part->x, part->y);
            const ContextPressAt hit = context_press_at(s, sc, part->space, part->x, part->y, at);
            REQUIRE(hit.entry);
            const ContextEntry& entry = population[hit.index];
            CHECK(std::string(entry.is_group ? entry.group : entry.row->id) == names[i]);
        }
        CHECK(named > 0);
        CHECK(named == desk.menu.parts.size());
        CHECK(part_named(desk.menu.parts, "setup.restore") != nullptr);
        // THE FIRST VERSION SAYS THE SAME LINES, and has no names to say.
        CHECK(d.desk().menu.lines.size() == desk.menu.lines.size());
    }
}

TEST_CASE("a pane's names survive the canvas: its next image draws a picture under the names its rows had, and each is pressed where the picture draws it") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say(m, SeatRows{"alpha",
                                           {TextRow{"first row", gfx::role::kFill},
                                            TextRow{"press [Save] here", gfx::role::kFill}},
                                           {RowPart{"row:first", 0, 0, body.columns},
                                            RowPart{"control:save", 1, 6, 6}}});
        });
        PaneView rows;
        REQUIRE(d.parts(kAlphaOffice, "alpha", rows).empty());
        REQUIRE(rows.parts.size() == 2);

        // THE PANE'S NEXT IMAGE DRAWS A PICTURE IN ITS AREA: the office's holder goes, and one that
        // draws a picture offers the same pane again.
        loom::WeaveId holder{};
        for (std::size_t i = 0; i < d.r.seats_.size(); ++i) {
            if (d.r.seats_[i] == d.alpha) holder = d.r.seat_ids[i];
        }
        REQUIRE(holder.valid());
        auto gone = d.r.bus.unregister_weave(holder);
        REQUIRE(gone);
        auto made = std::make_unique<SketchSeat>(kAlphaOffice, "alpha");
        SketchSeat* next = made.get();
        const loom::WeaveId id = d.r.bus.register_weave(std::move(made), sketch_grant(),
                                                        std::string(kAlphaOffice));
        next->zen_set_self(id);
        const auto drive = [&](std::function<void(SketchSeat&, loom::Mail&)> f) {
            next->next = std::move(f);
            (void)d.r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
            d.r.bus.drain_until_idle();
        };
        // ...and, arriving holding nothing, asks the window the room of the area it was given.
        const std::string area = held_area_of(d.alpha_kind);
        drive([area](SketchSeat&, loom::Mail& m) {
            (void)m.as_role(kAlphaOffice).send_to_role(kWorkshopProvider,
                v3::PaneOffered{"alpha", "Seat", "the same pane, drawn as a picture",
                                40 * kPaneCanvasUnit, 12 * kPaneCanvasUnit, 0});
            (void)m.as_role(kAlphaOffice).send_to_role(gfx::kGraphicsRole, gfx::RoomAsked{area});
        });
        REQUIRE_FALSE(next->rooms.empty());
        const CanvasRoom room = next->room();
        const std::int64_t u = kPaneCanvasUnit;
        CanvasPicture picture;
        picture.labels = {CanvasLabel{0, 0, "first row", gfx::role::kFill},
                          CanvasLabel{0, u, "press", gfx::role::kFill},
                          CanvasLabel{6 * u, u, "[Save]", gfx::role::kAccent},
                          CanvasLabel{13 * u, u, "here", gfx::role::kFill}};
        picture.parts = {CanvasPart{"row:first", 0, 0, room.width, u},
                         CanvasPart{"control:save", 6 * u, u, 6 * u, u}};
        drive([picture](SketchSeat& s, loom::Mail& m) { s.draw(m, picture); });
        REQUIRE(next->rejected.empty());

        // EVERY NAME AN AGENT WROTE AGAINST THE ROWS FINDS ITS PART IN THE PICTURE.
        PaneView drawn;
        REQUIRE(d.parts(kAlphaOffice, "alpha", drawn).empty());
        CHECK(names_of(drawn.parts) == names_of(rows.parts));
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.alpha_kind);
        for (const PanePart& before : rows.parts) {
            CAPTURE(before.name);
            const PanePart* after = part_named(drawn.parts, before.name);
            REQUIRE(after != nullptr);
            CHECK(after->text == before.text);
            next->pointers.clear();
            d.click(after->x, after->y, after->space);
            REQUIRE_FALSE(next->pointers.empty());
            CHECK(next->pointers.front().phase == canvas_pointer::kPress);
            const std::int64_t x = next->pointers.front().x + pane.placed_x;
            const std::int64_t y = next->pointers.front().y + pane.placed_y;
            CHECK(x >= after->place.x - (window ? 0 : u - 1));
            CHECK(x < after->place.x + after->place.w);
            CHECK(y >= after->place.y - (window ? 0 : u - 1));
            CHECK(y < after->place.y + after->place.h);
        }
    }
}

TEST_CASE("a canvas pane's rows set on its room's lattice are words on its parts' rows, and a part's text is the characters drawn inside it") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        const CanvasRoom room = d.sketch->room();
        const CanvasTextMetrics m = canvas_text_metrics(room);
        CanvasPicture p;
        p.rects.push_back(CanvasRect{0, 0, room.width, room.height, gfx::role::kGround});
        const std::vector<std::string> rows = {"> [open] Layouts", "  [    ] Loaded"};
        for (std::size_t r = 0; r < rows.size(); ++r) {
            CanvasText run{m.inset, static_cast<std::int64_t>(r) * m.line, rows[r], gfx::role::kFill};
            run.padded = false;
            p.texts.push_back(run);
        }
        p.parts.push_back(CanvasPart{"pane:a", m.inset, 0, 20 * m.advance, m.line});
        p.parts.push_back(CanvasPart{"mark:a", m.inset + 2 * m.advance, 0, 6 * m.advance, m.line});
        p.parts.push_back(CanvasPart{"pane:b", m.inset, m.line, 20 * m.advance, m.line});
        d.draw_picture(p);
        REQUIRE(d.sketch->rejected.empty());
        PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        // ONE WORD A ROW, its text the row's, on the row its parts stand on.
        REQUIRE(view.words.size() == 2);
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        for (std::size_t r = 0; r < rows.size(); ++r) {
            CAPTURE(r);
            CHECK(view.words[r].text == rows[r]);
            CHECK(view.words[r].place.x == pane.placed_x + m.inset);
            CHECK(view.words[r].place.y == pane.placed_y + static_cast<std::int64_t>(r) * m.line);
            CHECK(view.words[r].place.h == m.line);
        }
        const PanePart* row_a = part_named(view.parts, "pane:a");
        const PanePart* mark = part_named(view.parts, "mark:a");
        const PanePart* row_b = part_named(view.parts, "pane:b");
        REQUIRE(row_a != nullptr);
        REQUIRE(mark != nullptr);
        REQUIRE(row_b != nullptr);
        CHECK(row_a->place.y == view.words[0].place.y);
        CHECK(row_b->place.y == view.words[1].place.y);
        // A PART A WORD CROSSES SAYS THE CHARACTERS ON ITS SIDE, as a row part says its columns'.
        CHECK(row_a->text == rows[0]);
        CHECK(mark->text == "[open]");
        CHECK(row_b->text == rows[1]);
        // ...and a press at the mark's point lands inside the mark, in the pane's own pixels.
        d.sketch->pointers.clear();
        d.click(mark->x, mark->y, mark->space);
        REQUIRE_FALSE(d.sketch->pointers.empty());
        CHECK(inside_locally(d.sketch->pointers.front(), mark->place, pane));
    }
}

TEST_CASE("every place a named part gives is where the medium draws its words, in a window and in a terminal") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say(m, SeatRows{"alpha",
                                           {TextRow{"first row", gfx::role::kFill},
                                            TextRow{"press [Save] here", gfx::role::kFill}},
                                           {RowPart{"row:first", 0, 0, body.columns},
                                            RowPart{"control:save", 1, 6, 6},
                                            RowPart{"word:here", 1, 13, 4}}});
            s.caret(m, "alpha", RowsCaret{1, 2});
        });
        PaneView view;
        REQUIRE(d.parts(kAlphaOffice, "alpha", view).empty());
        REQUIRE(view.parts.size() == 3);
        const Session& s = d.r.session();
        // EACH PART'S WORDS ARE WHAT THE WINDOW SHOWS ACROSS ITS PLACE, a line of the window's text.
        for (const PanePart& p : view.parts) {
            CAPTURE(p.name);
            REQUIRE_FALSE(p.text.empty());
            const std::int64_t advance = window ? s.text_advance_px : gfx::kCellPx;
            CHECK(shown_word(d.r, DeskRect{p.place.x, p.place.y,
                                           static_cast<std::int64_t>(p.text.size()) * advance,
                                           p.place.h}) == p.text);
            CHECK(p.place.h == (window ? s.text_line_px : gfx::kCellPx));
        }
    }
}

namespace {

/// A KEPT CONVERSATION: the timeline heard, against tests/timelines/<name>.txt.
std::string kept_conversation(const zengine::tests::Timeline& heard, const std::string& name) {
    return heard.compare(std::filesystem::path(ZENGINE_SOURCE_DIR) / "tests" / "timelines" / (name + ".txt"),
                         std::filesystem::path(ZENGINE_TIMELINES_NOW) / (name + ".txt"));
}

} // namespace

TEST_CASE("the conversation of reading the desk, a pane's words and a character's point is kept") {
    DeskRig d;
    say_rows(d);
    zengine::tests::Timeline heard(d.r.bus);
    heard.varies("epoch"); // the window's incarnation, drawn afresh each run
    for (const char* office : {gfx::kGraphicsRole, "zengine.window.terminal", "zengine.window.sdl"}) {
        heard.without(office);
    }
    heard.name(d.asker_id, "asker");
    heard.name(d.r.painter_id, "painter");
    (void)d.desk();
    PaneView view;
    REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
    PanePoint at;
    REQUIRE(d.point(PanePointRequested{kAlphaOffice, "alpha", view.stamp.fingerprint, 1, 7}, at).empty());
    heard.stop();
    const std::string differs = kept_conversation(heard, "desk-words-point");
    CHECK_MESSAGE(differs.empty(), differs);
}

TEST_CASE("the conversation of a press on a word is kept") {
    DeskRig d;
    say_rows(d);
    PaneView view;
    REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
    zengine::tests::Timeline heard(d.r.bus);
    for (const char* office : {gfx::kGraphicsRole, "zengine.window.terminal", "zengine.window.sdl"}) {
        heard.without(office);
    }
    heard.name(d.asker_id, "asker");
    heard.name(d.r.painter_id, "painter");
    d.click(view.words[1].x, view.words[1].y, view.words[1].space);
    heard.stop();
    REQUIRE(d.alpha->presses.size() == 1);
    const std::string differs = kept_conversation(heard, "press-a-word");
    CHECK_MESSAGE(differs.empty(), differs);
}

TEST_CASE("the conversation of opening a pane is kept") {
    DeskRig d;
    ProviderSeat* gamma = d.r.mount_provider("zengine.test.desk-gamma");
    zengine::tests::Timeline heard(d.r.bus);
    for (const char* office : {gfx::kGraphicsRole, "zengine.window.terminal", "zengine.window.sdl"}) {
        heard.without(office);
    }
    heard.name(d.asker_id, "asker");
    heard.name(d.r.painter_id, "painter");
    REQUIRE(seat_pane_open(d.r, gamma, "zengine.test.desk-gamma", "gamma") != kNoPaneKind);
    heard.stop();
    const std::string differs = kept_conversation(heard, "open-a-pane");
    CHECK_MESSAGE(differs.empty(), differs);
}

namespace {

/// THE READING A DESK READ GAVE ONE PANE, or nullptr where it named the pane by its stamp alone.
const PaneView* reading_of(const DeskRead& read, const std::string& provider,
                               const std::string& pane) {
    for (const PaneView& v : read.panes) {
        if (v.stamp.provider == provider && v.stamp.pane == pane) return &v;
    }
    return nullptr;
}

/// ...and the stamp it named that pane by, or nullptr.
const PaneStamp* stamp_in(const DeskRead& read, const std::string& provider, const std::string& pane) {
    for (const PaneStamp& s : read.stamps) {
        if (s.provider == provider && s.pane == pane) return &s;
    }
    return nullptr;
}

/// What a reading stands on, as a page that continues it names it.
PaneStamp stamp_of_reading(const PaneView& v) { return v.stamp; }

/// The canvas pixels two rectangles share, or an empty rectangle.
PixelRect overlap_of(const PixelRect& a, const PixelRect& b) {
    const std::int64_t x0 = (std::max)(a.x, b.x), y0 = (std::max)(a.y, b.y);
    const std::int64_t x1 = (std::min)(a.x + a.w, b.x + b.w), y1 = (std::min)(a.y + a.h, b.y + b.h);
    return x1 > x0 && y1 > y0 ? PixelRect{x0, y0, x1 - x0, y1 - y0} : PixelRect{};
}

/// Whether a place shares a canvas pixel with a rectangle.
bool place_meets(const DeskRect& place, const PixelRect& r) {
    return overlap_of(PixelRect{place.x, place.y, place.w, place.h}, r).w > 0;
}

std::vector<std::string> texts_of(const std::vector<PaneWord>& words) {
    std::vector<std::string> out;
    for (const PaneWord& w : words) out.push_back(w.text);
    return out;
}

/// A SECOND ASKER beside the rig's own, granted the desk read alone.
struct OtherAsker {
    DeskAsker* asker = nullptr;
    loom::WeaveId id{};
};

OtherAsker other_asker(DeskRig& d) {
    auto made = std::make_unique<DeskAsker>();
    OtherAsker out;
    out.asker = made.get();
    loom::Grant grant;
    const auto shape = loom::schema_of<DeskReadRequested>();
    grant.allow_to_role(shape->name(), shape->version(), kWorkshopProvider);
    out.id = d.r.bus.register_weave(std::move(made), std::move(grant));
    out.asker->zen_set_self(out.id);
    return out;
}

/// The desk said whole to that asker, or the refusal's reason.
std::string read_by(DeskRig& d, const OtherAsker& who, DeskRead& out) {
    who.asker->refusals.clear();
    const std::size_t before = who.asker->reads.size();
    who.asker->next = [](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, DeskReadRequested{}); };
    (void)d.r.bus.send(who.id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
    d.r.bus.drain_until_idle();
    if (!who.asker->refusals.empty()) return who.asker->refusals.back();
    REQUIRE(who.asker->reads.size() == before + 1);
    out = who.asker->reads.back();
    return std::string();
}

} // namespace

TEST_CASE("a desk read's words stand where the medium draws them, Workshop's own band words included, in a "
          "window and in a terminal") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        say_rows(d);
        DeskRead read;
        REQUIRE(d.read(read).empty());
        // EVERY PRESENTED PANE IS READ IN THE ONE ANSWER -- the two text panes, the canvas pane and
        // Layouts -- with nothing over any of them, so every word each holds is said.
        CHECK(read.stamps.size() == 4);
        REQUIRE(read.panes.size() == read.stamps.size());
        REQUIRE(reading_of(read, kCanvasOffice, kCanvasPane) != nullptr);
        REQUIRE(reading_of(read, kWorkshopProvider, pane_key::kLayouts) != nullptr);
        std::vector<PaneWord> words;
        for (const PaneView& v : read.panes) {
            CAPTURE(v.stamp.pane);
            CHECK_FALSE(v.in_flight);
            CHECK(v.covered.by.empty());
            REQUIRE_FALSE(v.words.empty());
            words.insert(words.end(), v.words.begin(), v.words.end());
        }
        // ...AND THE BAND'S WORDS AFTER THEM, which have no point.
        const std::size_t pane_words = words.size();
        REQUIRE_FALSE(read.desk.words.empty());
        words.insert(words.end(), read.desk.words.begin(), read.desk.words.end());

        // WHAT THE WINDOW SHOWS, each pane's area and Workshop's own: every word there in one row.
        const DeskShown shown = d.r.shown();
        for (std::size_t i = 0; i < words.size(); ++i) {
            const PaneWord& w = words[i];
            CAPTURE(w.text);
            CHECK(shown.text_at_px(w.place.x, w.place.y,
                                   static_cast<std::int64_t>(w.text.size())) == w.text);
            if (window || i >= pane_words) continue;
            // ...and a pane word's point is one of its cells, on the console row the terminal reads.
            CHECK(w.x >= gfx::cell_of_pixel(w.place.x));
            CHECK(w.x < gfx::cell_of_pixel(w.place.x + w.place.w));
            CHECK(w.y == gfx::cell_of_pixel(w.place.y));
        }
    }
}

TEST_CASE("a part's listed point presses that part, one in the middle of a row too, and Layouts' tabs press "
          "the layout they name") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        REQUIRE(body.columns >= 20);
        // THREE CONTROLS ON ONE ROW, two of them past its start, and one in another row's middle.
        const std::vector<TextRow> rows{{"[one] [two] [three]", gfx::role::kFill},
                                                        {"press [Save] here", gfx::role::kFill}};
        d.r.drive(d.alpha, [&rows](ProviderSeat& s, loom::Mail& m) {
            s.say(m, SeatRows{"alpha", rows,
                                           {RowPart{"control:one", 0, 0, 5},
                                            RowPart{"control:two", 0, 6, 5},
                                            RowPart{"control:three", 0, 12, 7},
                                            RowPart{"control:save", 1, 6, 6}}});
        });
        const std::vector<NamedRun> runs{{"control:one", 0, 0, 5},
                                         {"control:two", 0, 6, 11},
                                         {"control:three", 0, 12, 19},
                                         {"control:save", 1, 6, 12}};
        DeskRead read;
        REQUIRE(d.read(read).empty());
        const PaneView* named = reading_of(read, kAlphaOffice, "alpha");
        REQUIRE(named != nullptr);
        REQUIRE(named->parts.size() == runs.size());
        // EACH PART'S POINT, PRESSED AS ORDINARY INPUT, reaches the pane on that part's row, at a
        // column inside it: the press its own measurer resolves.
        for (const NamedRun& run : runs) {
            const PanePart* part = part_named(named->parts, run.name);
            REQUIRE(part != nullptr);
            CHECK(part->space == (window ? input::space::kPixels : input::space::kCells));
            press_lands(d, *part, run);
        }

        // `layout:+` IS PRESSED AT ITS POINT, and a layout is made.
        const PaneView* layouts = reading_of(read, kWorkshopProvider, pane_key::kLayouts);
        REQUIRE(layouts != nullptr);
        const PanePart* plus = part_named(layouts->parts, "layout:+");
        REQUIRE(plus != nullptr);
        REQUIRE(plus->space != input::space::kUnknown);
        const std::size_t count = layout_count(d.r.session().setup);
        d.click(plus->x, plus->y, plus->space);
        REQUIRE(layout_count(d.r.session().setup) == count + 1);
        // ...AND EACH TAB'S POINT MAKES LIVE THE LAYOUT IT NAMES: the new desk is named as the
        // first is, and each name says its place in the weaver's order.
        for (const std::size_t at : {std::size_t{0}, std::size_t{1}}) {
            CAPTURE(at);
            REQUIRE(d.r.session().setup.active_at != at);
            REQUIRE(d.read(read).empty());
            layouts = reading_of(read, kWorkshopProvider, pane_key::kLayouts);
            REQUIRE(layouts != nullptr);
            const std::string name =
                std::string("layout:") + kDefaultSetupName + "#" + std::to_string(at + 1);
            const PanePart* tab = part_named(layouts->parts, name);
            REQUIRE(tab != nullptr);
            REQUIRE(tab->space != input::space::kUnknown);
            d.click(tab->x, tab->y, tab->space);
            CHECK(d.r.session().setup.active_at == at);
        }
    }
}

TEST_CASE("a covered pane says its visible words only, and what covers it: a pane in front, Workshop's "
          "menu, arranging -- and beside a menu nothing has a point") {
    DeskRig d;
    constexpr const char* kFrontOffice = "zengine.test.desk-front";
    ProviderSeat* front = d.r.mount_provider(kFrontOffice);
    d.r.drive(front, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"front", "Front", "the pane in front"});
    });
    d.r.pick(PaneRef{kFrontOffice, "front"});
    const RuntimePane* row = d.r.session().panes.runtime.find(kFrontOffice, "front");
    REQUIRE(row != nullptr);
    const std::int64_t front_kind = row->kind;
    // ALPHA, THE FRONT PANE OVER ITS RIGHT SIDE FROM ABOVE IT, and beta out of their way: places
    // and sizes in canvas cells, laid out again by a new extent.
    const auto put = [&d](const char* office, const char* pane, std::int64_t x, std::int64_t y,
                          std::int64_t w, std::int64_t h) {
        const PaneRef ref{office, pane};
        stage_desk(d.r, [&](Setup& desk) {
            REQUIRE(author_pane_place(desk, ref, x * gfx::kCellPx, y * gfx::kCellPx)
                        .accepted);
            REQUIRE(author_pane_size(desk, ref, PaneSize{pane_unit::kPixels, w * gfx::kCellPx},
                                     PaneSize{pane_unit::kPixels, h * gfx::kCellPx})
                        .accepted);
        });
    };
    put(kAlphaOffice, "alpha", 4, 5, 40, 12);
    put(kFrontOffice, "front", 30, 0, 40, 10);
    put(kBetaOffice, "beta", 80, 20, 30, 8);
    d.r.extent(149, 60); // a same-size extent reseats nothing: the desk re-seats every pane
    d.r.extent(150, 60);
    const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
    REQUIRE(body.columns >= 36);
    REQUIRE(body.rows >= 7);
    std::vector<TextRow> rows;
    for (const char* text : {"first row", "a row that runs under the front pane", "short", "", "", "",
                             "a row below the front pane, said all"}) {
        rows.push_back(TextRow{text, gfx::role::kFill});
    }
    d.r.drive(d.alpha, [&rows](ProviderSeat& s, loom::Mail& m) {
        s.say(m, SeatRows{"alpha", rows,
                                       {RowPart{"control:short", 2, 0, 5},
                                        RowPart{"control:front", 1, 26, 5},
                                        RowPart{"row:below", 6, 0, 36}}});
    });
    d.r.drive(front, [](ProviderSeat& s, loom::Mail& m) {
        s.say(m, SeatRows{"front", {TextRow{"in front", gfx::role::kFill}}, {}});
    });
    const auto page = [&d](PaneView& out) {
        return d.page(PaneViewRequested{kAlphaOffice, "alpha", 0, PaneStamp{}}, out);
    };
    const auto items = [](const PaneView& v) {
        return static_cast<std::int64_t>(v.words.size() + v.parts.size());
    };

    // ALPHA IN FRONT: nothing covers it, and its whole reading is said, each part with its point.
    press_body(d.r, d.alpha_kind);
    PaneView whole;
    REQUIRE(page(whole).empty());
    REQUIRE(whole.covered.by.empty());
    REQUIRE(whole.covered.words == 0);
    REQUIRE_FALSE(whole.parts.empty());
    for (const PanePart& p : whole.parts) {
        CAPTURE(p.name);
        CHECK(p.space == input::space::kCells);
    }
    // THE FRONT PANE LIFTED OVER IT, by a press on the part of it alpha does not cover.
    press_body(d.r, front_kind);
    const Session& s = d.r.session();
    REQUIRE(effective_pane_order(s.setup.active, s.panes).back() == front_kind);
    const Screen sc = screen_of(s);
    const PaneBounds alpha_at = bounds_of(s.panes, s.setup.active, d.alpha_kind, sc);
    const PaneBounds front_at = bounds_of(s.panes, s.setup.active, front_kind, sc);
    REQUIRE(alpha_at.open);
    REQUIRE(front_at.open);
    // WHAT IS STILL SAID: every word and part whose place the front pane leaves alone.
    std::vector<std::string> shown_words, shown_parts;
    std::int64_t hidden = 0;
    for (const PaneWord& w : whole.words) {
        if (place_meets(w.place, front_at.rect)) {
            ++hidden;
        } else {
            shown_words.push_back(w.text);
        }
    }
    for (const PanePart& p : whole.parts) {
        if (place_meets(p.place, front_at.rect)) {
            ++hidden;
        } else {
            shown_parts.push_back(p.name);
        }
    }
    REQUIRE(hidden > 0);
    REQUIRE_FALSE(shown_words.empty());
    REQUIRE_FALSE(shown_parts.empty());
    PaneView behind;
    REQUIRE(page(behind).empty());
    CHECK(behind.covered.by == std::vector<std::string>{"Front"});
    CHECK(behind.covered.words == hidden);
    // ...THE COVER BOUNDED WITHIN THE AREA THE READING IS OF, the pane's own under its title rows.
    const ExternalPane& alpha_pane = *s.panes.external_pane(d.alpha_kind);
    const PixelRect alpha_area{alpha_pane.placed_x, alpha_pane.placed_y, alpha_pane.placed_w,
                               alpha_pane.placed_h};
    CHECK(same(behind.covered.rect, overlap_of(alpha_area, front_at.rect)));
    CHECK(texts_of(behind.words) == shown_words);
    std::vector<std::string> said_parts;
    for (const PanePart& p : behind.parts) said_parts.push_back(p.name);
    CHECK(said_parts == shown_parts);
    CHECK(behind.total == items(behind));
    // ...THE SAME IN THE DESK READ.
    DeskRead read;
    REQUIRE(d.read(read).empty());
    const PaneView* in_read = reading_of(read, kAlphaOffice, "alpha");
    REQUIRE(in_read != nullptr);
    CHECK(in_read->covered.words == hidden);
    CHECK(texts_of(in_read->words) == shown_words);

    // WORKSHOP'S MENU, opened by a right press on alpha's title row, over alpha's words.
    press_outside(d.r, d.alpha_kind); // the keys are Workshop's
    const CellRect title = cells_covered(alpha_at.rect);
    d.r.hand_moment(input::PointerButton{3, true, title.x + 2,
                                                    title.y,
                                                    input::space::kCells, input::mod::kNone});
    REQUIRE(s.context.open);
    const PixelRect menu = context_bounds(s, screen_of(s));
    REQUIRE(overlap_of(menu, alpha_at.rect).w > 0);
    PaneView under_menu;
    REQUIRE(page(under_menu).empty());
    const std::vector<std::string>& by = under_menu.covered.by;
    CHECK(std::find(by.begin(), by.end(), "menu") != by.end());
    for (const PaneWord& w : under_menu.words) {
        CAPTURE(w.text);
        CHECK_FALSE(place_meets(w.place, menu));
    }
    for (const PanePart& p : under_menu.parts) {
        CAPTURE(p.name);
        CHECK_FALSE(place_meets(p.place, menu));
    }
    CHECK(under_menu.covered.words + items(under_menu) == items(whole));
    // ...AND BESIDE THE MENU NOTHING HAS A POINT: a press outside an open menu is spent closing it,
    // so what alpha and Layouts still say keeps its place and no point a press would land on.
    REQUIRE(items(under_menu) > 0);
    for (const PaneWord& w : under_menu.words) {
        CAPTURE(w.text);
        CHECK(w.space == input::space::kUnknown);
    }
    for (const PanePart& p : under_menu.parts) {
        CAPTURE(p.name);
        CHECK(p.space == input::space::kUnknown);
    }
    DeskRead with_menu;
    REQUIRE(d.read(with_menu).empty());
    const PaneView* layouts_beside = reading_of(with_menu, kWorkshopProvider, pane_key::kLayouts);
    REQUIRE(layouts_beside != nullptr);
    REQUIRE_FALSE(layouts_beside->parts.empty());
    for (const PanePart& p : layouts_beside->parts) {
        CAPTURE(p.name);
        CHECK(p.space == input::space::kUnknown);
    }
    d.r.key(input::scan::kEscape);
    REQUIRE_FALSE(s.context.open);

    // ARRANGING covers every pane whole: no word is said, and every one is counted.
    d.r.key(input::scan::kW);
    REQUIRE(s.arrange.open);
    const PaneBounds arranged_at = bounds_of(s.panes, s.setup.active, d.alpha_kind, screen_of(s));
    PaneView arranged;
    REQUIRE(page(arranged).empty());
    CHECK(arranged.words.empty());
    CHECK(arranged.parts.empty());
    const std::vector<std::string>& over = arranged.covered.by;
    CHECK(std::find(over.begin(), over.end(), "arranging") != over.end());
    CHECK(arranged.covered.words == items(whole));
    const ExternalPane& arranged_pane = *s.panes.external_pane(d.alpha_kind);
    CHECK(same(arranged.covered.rect,
               PixelRect{arranged_pane.placed_x, arranged_pane.placed_y, arranged_pane.placed_w,
                         arranged_pane.placed_h}));
    CHECK(arranged_at.open);
}

TEST_CASE("Layouts and the band are read as Workshop draws them: tabs as named parts, the band's words "
          "with no point") {
    DeskRig d;
    // LAYOUTS NAMED WITH A SPACE, TWO ALIKE, ONE WITH BYTES PAST ASCII, ONE WITH A PERCENT SIGN AND
    // ONE NAMED AS THE CREATE TAB IS, painted again by a new extent.
    stage_run(d.r, [](SetupState& setup) {
        for (const char* name : {"my desk", "twin", "twin", "caf\xC3\xA9", "100%", "+"}) {
            setup.shelved.push_back(Layout{Setup{name, {}, {}, {}, {}}, SetupLink{}});
        }
    });
    d.r.extent(149, 60);
    d.r.extent(150, 60);
    const std::vector<std::string> expected{"layout:Default", "layout:my desk",   "layout:twin#3",
                                            "layout:twin#4",  "layout:caf%C3%A9", "layout:100%25",
                                            "layout:%2B"};
    const SetupState& setup = d.r.run();
    REQUIRE(layout_count(setup) == expected.size());
    const Session& s = d.r.session();
    const Screen sc = screen_of(s);
    const BandStatus status = band_status(s, sc);
    REQUIRE(status.tabs.size() == expected.size()); // every tab is painted
    REQUIRE(status.create_columns > 0);
    DeskRead read;
    REQUIRE(d.read(read).empty());

    // A PART FOR EVERY PAINTED TAB AND FOR `+`, each named apart, on the tab row, with a point.
    const PaneView* layouts = reading_of(read, kWorkshopProvider, pane_key::kLayouts);
    REQUIRE(layouts != nullptr);
    REQUIRE_FALSE(layouts->words.empty());
    std::vector<std::string> want = expected;
    want.push_back("layout:+");
    std::sort(want.begin(), want.end());
    CHECK(names_of(layouts->parts) == want);
    const auto printable = [](const std::string& text) {
        return std::all_of(text.begin(), text.end(), [](char c) { return c >= 0x20 && c < 0x7F; });
    };
    for (const PanePart& p : layouts->parts) {
        CAPTURE(p.name);
        CHECK_FALSE(p.text.empty());
        CHECK(p.place.y == layouts->words[0].place.y);
        CHECK(p.space == input::space::kCells);
        if (!printable(p.text)) continue;
        // ...over the cells the terminal window shows its characters on.
        std::string cells = shown_word(d.r, p.place);
        while (!cells.empty() && cells.back() == ' ') cells.pop_back();
        CHECK(cells == p.text);
        CHECK(p.place.h == gfx::kCellPx);
    }
    // EACH TAB SAYS ITS LAYOUT'S NAME, as the weaver typed it.
    for (std::size_t at = 0; at < expected.size(); ++at) {
        const std::string& name = layout_at(setup, at).name;
        if (!printable(name)) continue;
        const PanePart* tab = part_named(layouts->parts, expected[at]);
        REQUIRE(tab != nullptr);
        CHECK(tab->text.find(name) != std::string::npos);
    }

    // THE BAND'S WORDS ARE ITS PAINTED ROWS, IN ORDER, inside the band, each with no point: the
    // band owns no pointer space.
    const ScreenRegion band = band_region(s, sc);
    std::vector<std::string> rows;
    for (const TextRow& r : band.rows) {
        std::string text = r.text;
        while (!text.empty() && text.back() == ' ') text.pop_back();
        if (!text.empty()) rows.push_back(text);
    }
    REQUIRE_FALSE(rows.empty());
    CHECK(texts_of(read.desk.words) == rows);
    for (std::size_t i = 0; i < read.desk.words.size(); ++i) {
        const PaneWord& w = read.desk.words[i];
        CAPTURE(w.text);
        CHECK(w.word == static_cast<std::int64_t>(i));
        CHECK(w.space == input::space::kUnknown);
        CHECK(w.x == 0);
        CHECK(w.y == 0);
        CHECK(w.place.y >= band.y);
        CHECK(w.place.y + w.place.h <= band.y + band.h);
    }

    // LAYOUTS PLACED LOW, reaching under the foot band: the band is drawn over it as over any pane,
    // and what the band hides is not said.
    const PaneRef layouts_ref{kWorkshopProvider, pane_key::kLayouts};
    const std::int64_t room_h = sc.notice_y - sc.room_y;
    stage_desk(d.r, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, layouts_ref, 0, room_h - gfx::kCellPx).accepted);
        REQUIRE(author_pane_size(desk, layouts_ref,
                                 PaneSize{pane_unit::kPixels, 100 * gfx::kCellPx},
                                 PaneSize{pane_unit::kPixels, 4 * gfx::kCellPx})
                    .accepted);
    });
    d.r.extent(149, 60);
    d.r.extent(150, 60);
    const Screen low_sc = screen_of(s);
    const PaneBounds low = bounds_of(s.panes, s.setup.active, pane_kind::kLayouts, low_sc);
    REQUIRE(low.open);
    REQUIRE(low.rect.y < low_sc.notice_y);
    REQUIRE(low.rect.y + low.rect.h > low_sc.notice_y);
    const PixelRect foot{low.rect.x, low_sc.notice_y, low.rect.w, low.rect.y + low.rect.h - low_sc.notice_y};
    DeskRead under;
    REQUIRE(d.read(under).empty());
    const PaneView* layouts_low = reading_of(under, kWorkshopProvider, pane_key::kLayouts);
    REQUIRE(layouts_low != nullptr);
    const std::vector<std::string>& by = layouts_low->covered.by;
    CHECK(std::find(by.begin(), by.end(), "band") != by.end());
    for (const PaneWord& w : layouts_low->words) {
        CAPTURE(w.text);
        CHECK_FALSE(place_meets(w.place, foot));
    }
    for (const PanePart& p : layouts_low->parts) {
        CAPTURE(p.name);
        CHECK_FALSE(place_meets(p.place, foot));
    }
}

TEST_CASE("a stamp names holder, incarnation and what the pane shows where: a reading after t or a "
          "reload in place is stale even for the same picture") {
    SketchRig d;
    const auto page = [&d](const PaneStamp& stamp, PaneView& out) {
        return d.page(PaneViewRequested{kCanvasOffice, kCanvasPane, 0, stamp}, out);
    };
    // THE PANE AS IT STANDS, asked under no stamp: its office's holder, that holder's incarnation,
    // and what it shows where.
    PaneView first;
    REQUIRE(page(PaneStamp{}, first).empty());
    REQUIRE_FALSE(first.in_flight);
    CHECK(first.stamp.fingerprint != 0);
    CHECK(first.stamp.holder == static_cast<std::int64_t>(d.sketch_id.value));
    CHECK(first.stamp.incarnation ==
          static_cast<std::int64_t>(d.r.bus.participant(d.sketch_id).incarnation));
    const PaneStamp kept = stamp_of_reading(first);
    PaneView continued;
    const std::string unrefused = page(kept, continued);
    REQUIRE_MESSAGE(unrefused.empty(), unrefused);
    CHECK(continued.stamp.fingerprint == kept.fingerprint);
    // A PAGE PAST THE READING its stamp still names -- a cover moved and shrank it -- is refused in
    // words saying to read it again from the start.
    const std::string past = d.page(PaneViewRequested{kCanvasOffice, kCanvasPane, first.total + 1, kept},
                                    continued);
    CHECK_MESSAGE(past.find("read it again from the start") != std::string::npos, past);

    // `t`: THE TITLES HIDDEN, A NEW ROOM, and the pane's first picture there the same as the last.
    press_outside(d.r, d.sketch_kind); // the keys are Workshop's...
    d.r.key(input::scan::kT);          // ...and the titles hidden
    d.r.text("t");
    REQUIRE_FALSE(d.r.session().pane_titles);
    // ...ITS NEW ROOM OUT AND NOT YET DRAWN FOR: the pane's next picture is in flight, said with no
    // word, and a page under the stamp before it is stale.
    PaneView awaited;
    const std::string unawaited = page(PaneStamp{}, awaited);
    REQUIRE_MESSAGE(unawaited.empty(), unawaited);
    CHECK(awaited.in_flight);
    CHECK(awaited.words.empty());
    CHECK(awaited.parts.empty());
    const std::string while_awaited = page(kept, continued);
    CHECK_MESSAGE(while_awaited.find("stale") != std::string::npos, while_awaited);
    d.draw();
    PaneView retitled;
    REQUIRE(page(PaneStamp{}, retitled).empty());
    REQUIRE_FALSE(retitled.in_flight);
    CHECK(retitled.stamp.fingerprint != kept.fingerprint); // the same picture, in another room
    CHECK(retitled.stamp.holder == kept.holder);
    CHECK(retitled.stamp.incarnation == kept.incarnation);
    const std::string after_t = page(kept, continued);
    CHECK_MESSAGE(after_t.find("stale") != std::string::npos, after_t);
    const PaneStamp now = stamp_of_reading(retitled);
    CHECK(page(now, continued).empty());

    // A RELOAD IN PLACE: the same holder at the same id, in its next incarnation, the same room and
    // picture -- and a page under the stamp before it is stale.
    REQUIRE(d.r.bus.swap_state(d.sketch_id, d.r.bus.snapshot_bytes(d.sketch_id)).revived);
    PaneView reloaded;
    const std::string unread = page(PaneStamp{}, reloaded);
    REQUIRE_MESSAGE(unread.empty(), unread);
    CHECK(reloaded.stamp.holder == now.holder);
    CHECK(reloaded.stamp.incarnation != now.incarnation);
    CHECK(reloaded.stamp.fingerprint == now.fingerprint);
    const std::string after_reload = page(now, continued);
    CHECK_MESSAGE(after_reload.find("stale") != std::string::npos, after_reload);
    // ...AND THE DESK READ NAMES THE PANE BY THE STAMP IT STANDS ON NOW.
    DeskRead read;
    REQUIRE(d.read(read).empty());
    const PaneStamp* in_read = stamp_in(read, kCanvasOffice, kCanvasPane);
    REQUIRE(in_read != nullptr);
    CHECK(in_read->holder == reloaded.stamp.holder);
    CHECK(in_read->incarnation == reloaded.stamp.incarnation);
    CHECK(in_read->fingerprint == reloaded.stamp.fingerprint);
}

TEST_CASE("a pane whose provider left waits for one, and no reading says it is in flight") {
    SketchRig d;
    say_rows(d);
    // ITS PICTURE DRAWN, then its holder gone and its area no longer placed, as the next placing
    // -- the window grown a column -- leaves out a pane whose office nobody holds.
    d.draw();
    auto gone = d.r.bus.unregister_weave(d.sketch_id);
    REQUIRE(gone);
    d.r.extent(151, 60);
    PaneView waiting;
    const std::string waits = d.page(PaneViewRequested{kCanvasOffice, kCanvasPane, 0, PaneStamp{}}, waiting);
    CHECK_MESSAGE(waits.find("waiting for the provider") != std::string::npos, waits);
    // THE REST OF THE DESK IS STILL READ.
    DeskRead read;
    REQUIRE(d.read(read).empty());
    const PaneView* alpha = reading_of(read, kAlphaOffice, "alpha");
    REQUIRE(alpha != nullptr);
    CHECK_FALSE(alpha->in_flight);
    CHECK_FALSE(alpha->words.empty());
}

TEST_CASE("past four desk reads in one second an asker is refused in words, and reads again a second later") {
    DeskRig d;
    d.r.clock.spaced(0); // every reading of the clock at one instant, until the case moves it
    DeskRead read;
    for (std::int64_t i = 0; i < kDeskReadsPerSecond; ++i) {
        CAPTURE(i);
        const std::string refused = d.read(read);
        REQUIRE_MESSAGE(refused.empty(), refused);
    }
    const std::string fifth = d.read(read);
    CHECK_MESSAGE(fifth.find("desk read refused") != std::string::npos, fifth);
    CHECK_MESSAGE(fifth.find("a second") != std::string::npos, fifth);
    // ANOTHER ASKER AT THAT INSTANT is not held to the first's reads.
    const OtherAsker other = other_asker(d);
    const std::string theirs = read_by(d, other, read);
    CHECK_MESSAGE(theirs.empty(), theirs);
    // ...AND THE FIRST READS AGAIN A SECOND AFTER ITS OLDEST READ, and not a moment before.
    d.r.clock.now += 999;
    CHECK_FALSE(d.read(read).empty());
    d.r.clock.now += 1;
    const std::string later = d.read(read);
    CHECK_MESSAGE(later.empty(), later);
}

TEST_CASE("the desk number moves when anything the desk says moves, and holds while nothing does") {
    DeskRig d;
    say_rows(d);
    const auto number = [&d] {
        DeskRead read;
        const std::string refused = d.read(read);
        REQUIRE_MESSAGE(refused.empty(), refused);
        return read.desk.desk;
    };
    // NOTHING MOVED between two reads: one number.
    const std::int64_t first = number();
    CHECK(number() == first);
    // A PANE SELECTED, and the keys pointed at it...
    press_body(d.r, d.alpha_kind);
    const std::int64_t selected = number();
    CHECK(selected != first);
    CHECK(number() == selected);
    // ...THE BAND'S NOTICE, and nothing else...
    d.r.session().notice = "a notice of its own";
    DeskRead noticed;
    REQUIRE(d.read(noticed).empty());
    CHECK(noticed.desk.desk != selected);
    REQUIRE_FALSE(noticed.desk.words.empty());
    CHECK(noticed.desk.words[0].text == "a notice of its own");
    CHECK(number() == noticed.desk.desk);
    // ...AND WORKSHOP'S MENU, opened by a right press on alpha's title row.
    press_outside(d.r, d.alpha_kind); // the keys are Workshop's
    const std::int64_t keys_back = number();
    CHECK(number() == keys_back);
    const CellRect title = cells_covered(external_pane_rect(d.r.session(), d.alpha_kind));
    d.r.hand_moment(input::PointerButton{3, true, title.x + 2,
                                                    title.y,
                                                    input::space::kCells, input::mod::kNone});
    REQUIRE(d.r.session().context.open);
    CHECK(number() != keys_back);
}

// ---- The desk in a graphics window ------------------------------------------------------------

namespace {

namespace gfx = zengine::graphics;
namespace gw = zengine::graphics::window;
namespace pw = zengine::testing::pane_window;
namespace apane = zengine::attention_pane;
/// The Attention pane's area in a window (D12).
const std::string kAttentionArea = pane_area(apane::kAttentionPaneRole, apane::kAttentionPane);

struct ArrangedDirectoryState {
    std::int64_t heard = 0;
    ZEN_SHAPE(ArrangedDirectoryState, 1, ZEN_FIELD(heard));
};

/// THE GRAPHICS DIRECTORY AS ITS ARRANGER MEETS IT: the `zengine.graphics` office keeping every
/// placement, skin and room said to it, with the office that said each, introducing nothing, and
/// saying it started afresh when a case asks.
class ArrangedDirectory
    : public loom::WeaveBase<ArrangedDirectory, ArrangedDirectoryState,
                             loom::Accept<gfx::AreaPlaced, gfx::SkinOffered, gfx::SkinChosen,
                                          gfx::RoomAsked, SeatDo>,
                             loom::Emit<gfx::GraphicsReady, gfx::ActionRequested>> {
public:
    void on(const gfx::AreaPlaced& p, loom::Mail& mail) {
        note(mail);
        placed.push_back(p);
        areas[p.area] = p;
    }
    void on(const gfx::SkinOffered& s, loom::Mail& mail) {
        note(mail);
        offered.push_back(s);
    }
    void on(const gfx::SkinChosen& c, loom::Mail& mail) {
        note(mail);
        chosen.push_back(c);
    }
    void on(const gfx::RoomAsked& r, loom::Mail& mail) {
        ++state_.heard;
        asked.push_back(r.area + " by " + std::string(mail.authored_role()));
    }
    /// The case's hand inside the directory's delivery; with none, it says it started afresh.
    void on(const SeatDo&, loom::Mail& mail) {
        auto run = std::move(next);
        next = {};
        if (run) {
            run(mail);
        } else {
            (void)mail.as_role(gfx::kGraphicsRole).publish(gfx::GraphicsReady{});
        }
    }

    std::function<void(loom::Mail&)> next;
    std::map<std::string, gfx::AreaPlaced> areas;
    std::vector<gfx::AreaPlaced> placed;
    std::vector<gfx::SkinOffered> offered;
    std::vector<gfx::SkinChosen> chosen;
    std::vector<std::string> asked;
    std::set<std::string> arrangers; ///< every office that placed, offered or chose

private:
    void note(loom::Mail& mail) {
        ++state_.heard;
        arrangers.insert(std::string(mail.authored_role()));
    }
};

struct DeskWindowState {
    std::int64_t drawn = 0;
    ZEN_SHAPE(DeskWindowState, 1, ZEN_FIELD(drawn));
};

/// WORKSHOP WITH A GRAPHICS DIRECTORY -- or none -- the Attention pane open on its desk, a desktop
/// declaring the row that shows a manual, and a tap keeping every graphics word Workshop's sends
/// delivered and every one refused.
struct WindowDesk {
    PaneRig r;
    /// The terminal window the desk is shown in: the case's own, offered at 100 by 40 cells.
    std::unique_ptr<pw::Window> window;
    std::uint64_t press_ = 0; ///< the window's number for the button held
    ArrangedDirectory* directory = nullptr;
    loom::WeaveId directory_id{};
    DesktopSeat* desktop = nullptr;
    std::int64_t kind = kNoPaneKind;
    /// What Workshop said, and had refused, of its graphics shapes to anyone but `window`.
    std::vector<std::string> said;
    std::vector<std::string> refused;
    loom::ObserverId tap{};

    explicit WindowDesk(bool with_directory, bool skins = true) {
        // THE DIRECTORY IS THIS CASE'S STAND-IN, or none, and so is the window: the rig's own
        // never comes up.
        r.in_window = false;
        window = std::make_unique<pw::Window>(r.bus, gfx::kTerminalWindow);
        r.host.dir = skins ? "/zen/window-desk" : "";
        if (with_directory) {
            mount_directory();
        }
        r.mount_workshop();
        const std::set<std::string> graphics{gfx::AreaPlaced::zen_name, gfx::SkinOffered::zen_name,
                                             gfx::SkinChosen::zen_name, gfx::RoomAsked::zen_name,
                                             gfx::Drawn::zen_name};
        tap = r.bus.add_observer([this, graphics](const loom::BusEvent& e) {
            if (e.sender != r.workshop_id || e.target == window->id() ||
                graphics.count(e.schema_name) == 0)
                return;
            if (e.kind == loom::EventKind::Delivered) said.push_back(e.schema_name);
            if (e.kind == loom::EventKind::Refused) refused.push_back(e.schema_name);
        });
        AppActions rows = shipped_app_actions();
        rows.rows.push_back(AppActionRow{kManualAction, "manual", input::scan::kF,
                                         input::mod::kCtrl, app_precedence::kAboveModes});
        desktop = mount_desktop(r, rows);
        r.ready();
        window->offer(kWorkshopProvider, cells_px(100), cells_px(40)); // the desk starts on it
        window->room(kWorkshopProvider, kWorkshopProvider, 100, 40);
        // THE WEAVER'S BUTTON lands in Workshop's own area of this window, at the cell it names.
        r.own_hand = [this](const input::PointerButton& b) {
            const bool cells = b.space == input::space::kCells;
            if (b.pressed) press_ = window->next_press();
            window->button(kWorkshopProvider, cells ? b.x : gfx::cell_of_pixel(b.x),
                           cells ? b.y : gfx::cell_of_pixel(b.y), b.pressed, press_, b.button);
        };
        REQUIRE(r.load(apane::kAttentionPaneStem, WORKSHOP_SO_ATTENTION_PANE,
                       apane::kAttentionPaneRole)
                    .valid());
        r.pick(PaneRef{apane::kAttentionPaneRole, apane::kAttentionPane});
        const RuntimePane* row =
            r.session().panes.runtime.find(apane::kAttentionPaneRole, apane::kAttentionPane);
        REQUIRE(row != nullptr);
        kind = row->kind;
        REQUIRE(r.session().panes.has(kind));
        press_outside(r, kind); // the keys are Workshop's
    }
    WindowDesk(const WindowDesk&) = delete;
    WindowDesk& operator=(const WindowDesk&) = delete;
    ~WindowDesk() { r.bus.remove_observer(tap); }

    /// A directory in `zengine.graphics`, keeping what it is told; it may forge a press.
    void mount_directory() {
        auto made = std::make_unique<ArrangedDirectory>();
        directory = made.get();
        loom::Grant grant;
        grant.allow_to_any(gfx::GraphicsReady::zen_name, gfx::GraphicsReady::zen_version);
        grant.allow_to_any(gfx::AreaButtonChanged::zen_name, gfx::AreaButtonChanged::zen_version);
        directory_id = r.bus.register_weave(std::move(made), std::move(grant), gfx::kGraphicsRole);
        directory->zen_set_self(directory_id);
    }
    /// The directory's hand, inside its own delivery; with none, it says it started afresh.
    void directory_does(std::function<void(loom::Mail&)> what = {}) {
        directory->next = std::move(what);
        (void)r.bus.send(directory_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    }

    /// Where the open pane's office draws its body: under its header, as a canvas is placed.
    PixelRect pane_body() {
        const Session& s = r.session();
        return canvas_body_place(external_pane_rect(s, kind), screen_of(s),
                                 external_title_rows(s.panes, kind, s.pane_titles));
    }
    /// The order of the open pane's own area: each pane in the desk's order stands two deep, its
    /// frame and then its body, above Workshop's own area.
    std::int64_t pane_order() {
        const std::vector<std::int64_t> order =
            effective_pane_order(r.session().setup.active, r.session().panes);
        return 2 * (static_cast<std::int64_t>(std::find(order.begin(), order.end(), kind) -
                                              order.begin()) +
                    1);
    }
    /// The orders the panes on the desk take, frames and bodies.
    std::int64_t panes_on_desk() {
        return 2 * static_cast<std::int64_t>(
                       effective_pane_order(r.session().setup.active, r.session().panes).size());
    }
    /// A key and the character it makes, as a backend reports both.
    void type(std::int64_t scancode, const char* character) {
        r.key(scancode);
        r.text(character);
    }
};

/// The cells an area's nodes paint, as a window of characters paints them.
std::set<std::pair<std::int64_t, std::int64_t>> painted_cells(const gw::WinArea& a) {
    gfx::PaintTask task;
    const auto at = gw::index_of(a);
    for (const gw::KeptNode& k : a.nodes) {
        task.nodes.push_back(
            gfx::PaintNode{k.node.path, gw::key_of(at, k.node.path), k.node.op, k.node.args});
    }
    std::set<std::pair<std::int64_t, std::int64_t>> out;
    for (const gfx::NodePaint& np : gfx::term::paint_task(task, gfx::term::Look{}).nodes) {
        for (const gfx::term::Cell& c : gfx::term::unpack(np.cells)) {
            out.insert({c.col, c.row});
        }
    }
    return out;
}

/// Whether any of `cells` lies inside `r`, a pixel rectangle on whole cells.
bool paints_inside(const std::set<std::pair<std::int64_t, std::int64_t>>& cells,
                   const PixelRect& r) {
    for (const auto& [col, row] : cells) {
        if (col * gfx::kCellPx >= r.x && col * gfx::kCellPx < r.x + r.w &&
            row * gfx::kCellPx >= r.y && row * gfx::kCellPx < r.y + r.h) {
            return true;
        }
    }
    return false;
}

std::string trimmed(std::string text) {
    text.erase(text.find_last_not_of(' ') + 1);
    return text;
}

} // namespace

TEST_CASE("with a graphics directory, Workshop places an open pane's area over its body in the "
          "desk's order and its own across the screen, behind the panes or in front while a weaver "
          "arranges or a menu is open, saying only what moved") {
    WindowDesk d(true);
    const Screen sc = screen_of(d.r.session());
    // Read from the directory mounted at the time: the case replaces it below.
    REQUIRE(d.directory->areas.count(kWorkshopProvider) == 1);
    REQUIRE(d.directory->areas.count(kAttentionArea) == 1);
    const auto desk = [&] { return d.directory->areas.at(kWorkshopProvider); };
    const auto pane = [&] { return d.directory->areas.at(kAttentionArea); };

    // WORKSHOP'S OWN AREA, the whole screen, behind every pane's; it hears the window's keys, since
    // no pane's own area takes them, and a press nothing drawn there takes.
    CHECK(desk().window == gfx::kTerminalWindow);
    CHECK(desk().owner == kWorkshopProvider);
    CHECK((desk().x == 0 && desk().y == 0 && desk().w == sc.w && desk().h == sc.h));
    CHECK(desk().order == 0);
    CHECK(desk().keys);
    CHECK(desk().pointer);
    // THE PANE'S, over the body its office draws, by its office's name, above in the desk's order.
    const PixelRect body = d.pane_body();
    REQUIRE_FALSE(body.empty());
    CHECK(pane().window == gfx::kTerminalWindow);
    CHECK(pane().owner == apane::kAttentionPaneRole);
    CHECK((pane().x == body.x && pane().y == body.y && pane().w == body.w &&
           pane().h == body.h));
    CHECK(pane().order == d.pane_order());
    CHECK(pane().order > desk().order);
    // ...not the window's keys, which are Workshop's words to the pane, but its pointer: that
    // office hears the window's moments in its own area.
    CHECK_FALSE(pane().keys);
    CHECK(pane().pointer);
    // ITS FRAME, Workshop's, over the pane's bounds just behind its body: the pane's chrome stands
    // at the pane's depth, so a pane in front covers the one behind, chrome and all.
    const std::string frame = pane_frame_area(d.r.session().panes, d.kind);
    REQUIRE(d.directory->areas.count(frame) == 1);
    const gfx::AreaPlaced framed = d.directory->areas.at(frame);
    const PixelRect bounds = external_pane_rect(d.r.session(), d.kind);
    CHECK(framed.owner == kWorkshopProvider);
    CHECK((framed.x == bounds.x && framed.y == bounds.y && framed.w == bounds.w &&
           framed.h == bounds.h));
    CHECK(framed.order == pane().order - 1);
    CHECK(framed.order > desk().order);
    // ...AND IN FRONT OF EVERY AREA OF A PANE BEHIND IT, frame and body: here Layouts'.
    const std::string layouts_frame = pane_frame_area(d.r.session().panes, pane_kind::kLayouts);
    REQUIRE(d.directory->areas.count(layouts_frame) == 1);
    CHECK(framed.order > d.directory->areas.at(layouts_frame).order);
    CHECK_FALSE(framed.keys);
    CHECK(framed.pointer);
    // WORKSHOP'S OVERLAY, in front of all, the whole screen, hearing neither keys nor the pointer.
    REQUIRE(d.directory->areas.count(kWorkshopOverlayArea) == 1);
    const auto overlay = [&] { return d.directory->areas.at(kWorkshopOverlayArea); };
    CHECK(overlay().owner == kWorkshopProvider);
    CHECK((overlay().x == 0 && overlay().y == 0 && overlay().w == sc.w && overlay().h == sc.h));
    CHECK(overlay().order == d.panes_on_desk() + 2);
    CHECK_FALSE(overlay().keys);
    CHECK_FALSE(overlay().pointer);
    // ALL OF IT SAID AS WORKSHOP'S OFFICE: the four skins offered once, each area's chosen once,
    // and Workshop's room asked once.
    CHECK(d.directory->arrangers == std::set<std::string>{kWorkshopProvider});
    REQUIRE(d.directory->offered.size() == 4);
    CHECK(d.directory->offered[0].skin == "classic");
    CHECK(d.directory->offered[0].library == d.r.host.so("zengine-paint-classic"));
    CHECK(d.directory->offered[1].skin == "block");
    CHECK(d.directory->offered[1].library == d.r.host.so("zengine-paint-block"));
    CHECK(d.directory->offered[2].skin == "day");
    CHECK(d.directory->offered[2].library == d.r.host.so("zengine-paint-day"));
    CHECK(d.directory->offered[3].skin == "night");
    CHECK(d.directory->offered[3].library == d.r.host.so("zengine-paint-night"));
    std::map<std::string, int> chosen;
    for (const gfx::SkinChosen& c : d.directory->chosen) {
        CHECK(c.skin == "classic");
        ++chosen[c.area];
    }
    std::map<std::string, int> every;
    for (const auto& [area, placed_area] : d.directory->areas) {
        every[area] = 1;
    }
    CHECK(chosen == every);
    CHECK(chosen.count(kAttentionArea) == 1);
    CHECK(chosen.count(frame) == 1);
    const std::string own_room = std::string(kWorkshopProvider) + " by " + kWorkshopProvider;
    const std::string overlay_room =
        std::string(kWorkshopOverlayArea) + " by " + kWorkshopProvider;
    CHECK(std::count(d.directory->asked.begin(), d.directory->asked.end(), own_room) == 1);
    CHECK(std::count(d.directory->asked.begin(), d.directory->asked.end(), overlay_room) == 1);
    CHECK(d.refused.empty());

    // A REPAINT THAT MOVED NOTHING SAYS NOTHING MORE.
    const std::size_t placed = d.directory->placed.size();
    d.r.key(input::scan::kGrave);
    CHECK(d.directory->placed.size() == placed);

    // ARRANGING PUTS WORKSHOP'S AREA IN FRONT, and leaving puts it back.
    d.type(input::scan::kW, "w");
    REQUIRE(d.r.session().arrange.open);
    CHECK(desk().order == d.panes_on_desk() + 1);
    CHECK(desk().keys);
    CHECK(pane().order == d.pane_order());
    d.r.key(input::scan::kEscape);
    REQUIRE_FALSE(d.r.session().arrange.open);
    CHECK(desk().order == 0);
    // ...AND SO DOES WORKSHOP'S OWN MENU.
    d.type(input::scan::kA, "a");
    REQUIRE(d.r.session().context.open);
    CHECK(desk().order == d.panes_on_desk() + 1);
    d.r.key(input::scan::kEscape);
    REQUIRE_FALSE(d.r.session().context.open);
    CHECK(desk().order == 0);

    // A DIRECTORY THAT STARTS AFRESH is offered, placed and chosen again, and asked the room.
    const std::size_t offered = d.directory->offered.size();
    const std::size_t again = d.directory->placed.size();
    d.directory_does();
    CHECK(d.directory->offered.size() == offered + 4);
    CHECK(d.directory->placed.size() == again + d.directory->areas.size());
    CHECK(std::count(d.directory->asked.begin(), d.directory->asked.end(), own_room) == 2);
    // ...AND SO IS ANOTHER HOLDER OF ITS OFFICE, at the next repaint, though it says nothing.
    const std::uint64_t before = d.directory_id.value;
    d.r.bus.unregister_weave(d.directory_id);
    d.mount_directory();
    REQUIRE(d.directory_id.value != before);
    d.r.key(input::scan::kGrave);
    CHECK(d.directory->offered.size() == 4);
    CHECK(d.directory->areas.count(kWorkshopProvider) == 1);
    CHECK(d.directory->areas.count(kAttentionArea) == 1);
    CHECK(d.directory->chosen.size() == d.directory->areas.size());
    CHECK(std::count(d.directory->asked.begin(), d.directory->asked.end(), own_room) == 1);

    // CLOSING THE PANE WITHDRAWS ITS AREA: placed in no window, nowhere.
    d.r.pick(PaneRef{apane::kAttentionPaneRole, apane::kAttentionPane});
    REQUIRE_FALSE(d.r.session().panes.has(d.kind));
    CHECK(pane().window.empty());
    CHECK((pane().w == 0 && pane().h == 0));
    CHECK(d.directory->areas.at(frame).window.empty()); // ...and its frame
    CHECK(d.directory->areas.at(kWorkshopOverlayArea).order == d.panes_on_desk() + 2);
    CHECK(d.refused.empty());
}

TEST_CASE("a host with no skin libraries offers and chooses no skin, and places its areas all the "
          "same") {
    WindowDesk bare(true, false);
    CHECK(bare.directory->offered.empty());
    CHECK(bare.directory->chosen.empty());
    CHECK(bare.directory->areas.count(kWorkshopProvider) == 1);
    CHECK(bare.directory->areas.count(kAttentionArea) == 1);
}

namespace {

/// AN EDITOR'S OFFICE AT THE SETTINGS DOORS: it reads and writes Workshop's own settings and a
/// pane's row by message, and keeps the answers.
class SkinEditor
    : public loom::WeaveBase<SkinEditor, ArrangedDirectoryState,
                             loom::Accept<OfficeSettingsAnswered, PaneSettingsAnswered, PaneSettingCommitAnswered,
                                          OfficeSettingCommitAnswered, SeatDo>,
                             loom::Emit<OfficeSettingsRequested, OfficeSettingCommitRequested,
                                        PaneSettingsRequested, PaneSettingCommitRequested>> {
public:
    void on(const OfficeSettingsAnswered& s, loom::Mail& mail) {
        if (mail.answers_ask()) office.push_back(s);
    }
    void on(const PaneSettingsAnswered& s, loom::Mail& mail) {
        if (mail.answers_ask()) rows.push_back(s);
    }
    void on(const PaneSettingCommitAnswered& a, loom::Mail& mail) {
        if (mail.answers_ask()) acted.push_back(a);
    }
    void on(const OfficeSettingCommitAnswered& a, loom::Mail& mail) {
        if (mail.answers_ask()) office_acted.push_back(a);
    }
    void on(const SeatDo&, loom::Mail& mail) {
        auto run = std::move(next);
        next = {};
        if (run) run(mail);
    }

    std::function<void(loom::Mail&)> next;
    std::vector<OfficeSettingsAnswered> office;
    std::vector<PaneSettingsAnswered> rows;
    std::vector<PaneSettingCommitAnswered> acted;
    std::vector<OfficeSettingCommitAnswered> office_acted;
};

constexpr const char* kSkinEditorOffice = "test.skin-editor";

/// THE WINDOW DESK WITH AN EDITOR BESIDE IT, which writes Workshop's skin and the Attention pane's.
struct SkinDesk : WindowDesk {
    SkinEditor* editor = nullptr;
    loom::WeaveId editor_id{};

    SkinDesk() : WindowDesk(true) {
        auto made = std::make_unique<SkinEditor>();
        editor = made.get();
        loom::Grant grant;
        grant.allow_to_role(OfficeSettingsRequested::zen_name, OfficeSettingsRequested::zen_version,
                            kLayoutsRole);
        grant.allow_to_role(OfficeSettingCommitRequested::zen_name,
                            OfficeSettingCommitRequested::zen_version, kLayoutsRole);
        grant.allow_to_role(PaneSettingsRequested::zen_name, PaneSettingsRequested::zen_version,
                            kLayoutsRole);
        grant.allow_to_role(PaneSettingCommitRequested::zen_name,
                            PaneSettingCommitRequested::zen_version, kLayoutsRole);
        editor_id = r.bus.register_weave(std::move(made), std::move(grant), kSkinEditorOffice);
        editor->zen_set_self(editor_id);
    }

    template <class Shape> void ask(const Shape& asked) {
        editor->next = [asked](loom::Mail& mail) {
            (void)mail.as_role(kSkinEditorOffice).send_to_role(door_of<Shape>(), asked);
        };
        (void)r.bus.send(editor_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    }
    PaneSettingCommitAnswered acted() {
        REQUIRE_FALSE(editor->acted.empty());
        return editor->acted.back();
    }
    /// Workshop's own skin, written at the revision read now.
    OfficeSettingCommitAnswered workshop_skin(const char* skin) {
        ask(OfficeSettingsRequested{kWorkshopProvider});
        REQUIRE_FALSE(editor->office.empty());
        OfficeSettingCommitRequested c;
        c.office = kWorkshopProvider;
        c.revision = editor->office.back().revision;
        c.key = "workshop.skin";
        c.text = std::string(skin);
        ask(c);
        REQUIRE_FALSE(editor->office_acted.empty());
        return editor->office_acted.back();
    }
    /// ...and the Attention pane's row's, or its clear for none.
    PaneSettingCommitAnswered pane_skin(const char* skin) {
        ask(PaneSettingsRequested{apane::kAttentionPaneRole, apane::kAttentionPane});
        REQUIRE_FALSE(editor->rows.empty());
        PaneSettingCommitRequested c;
        c.provider = apane::kAttentionPaneRole;
        c.pane = apane::kAttentionPane;
        c.revision = editor->rows.back().revision;
        c.key = "workshop.skin";
        if (skin != nullptr) {
            c.text = std::string(skin);
        } else {
            c.clear = true;
        }
        ask(c);
        return acted();
    }
    /// The skins said to the directory since `from`, by area.
    std::map<std::string, std::string> chosen_since(std::size_t from, std::size_t* count = nullptr) {
        std::map<std::string, std::string> out;
        for (std::size_t i = from; i < directory->chosen.size(); ++i) {
            out[directory->chosen[i].area] = directory->chosen[i].skin;
        }
        if (count != nullptr) {
            *count = directory->chosen.size() - from;
        }
        return out;
    }
};

using Skins = std::map<std::string, std::string>;

} // namespace

TEST_CASE("each graphics area wears its pane row's `workshop.skin`, else Workshop's own, and a "
          "skin is said only when an area's moved") {
    // ⚔ MUTATION: every area given `classic` whatever is kept -- Workshop's `block` never reaches
    // the directory, and neither does the row's.
    SkinDesk d;
    const std::string pane = kAttentionArea;
    REQUIRE(d.directory->areas.count(pane) == 1);
    REQUIRE(d.directory->areas.count(kWorkshopProvider) == 1);
    std::size_t said = 0;

    // WORKSHOP'S OWN, `block`: every area wears it, each said once.
    std::size_t from = d.directory->chosen.size();
    const OfficeSettingCommitAnswered wide = d.workshop_skin("block");
    REQUIRE_MESSAGE(wide.accepted, wide.refusal);
    CHECK(d.r.session().skin == "block");
    const std::string frame = pane_frame_area(d.r.session().panes, d.kind);
    const std::string layouts = pane_frame_area(d.r.session().panes, pane_kind::kLayouts);
    CHECK(d.chosen_since(from, &said) == Skins{{kWorkshopProvider, "block"},
                                               {kWorkshopOverlayArea, "block"},
                                               {pane, "block"},
                                               {frame, "block"},
                                               {layouts, "block"}});
    CHECK(said == 5);

    // THE PANE'S ROW, `classic`: its area and its frame alone move back, and the pane is handed
    // none of it.
    from = d.directory->chosen.size();
    const PaneSettingCommitAnswered own = d.pane_skin("classic");
    REQUIRE_MESSAGE(own.accepted, own.refusal);
    CHECK(own.handed == "stored");
    CHECK(d.chosen_since(from, &said) == Skins{{pane, "classic"}, {frame, "classic"}});
    CHECK(said == 2);
    const ExternalPane* held = d.r.session().panes.external_pane(d.kind);
    REQUIRE(held != nullptr);
    REQUIRE(held->settings_heard.has_value());
    CHECK(find_pane_setting(*held->settings_heard, "workshop.skin") == nullptr);

    // WORKSHOP'S OWN BACK TO `classic` moves Workshop's areas alone: the row already says it.
    from = d.directory->chosen.size();
    REQUIRE(d.workshop_skin("classic").accepted);
    CHECK(d.chosen_since(from) == Skins{{kWorkshopProvider, "classic"},
                                        {kWorkshopOverlayArea, "classic"},
                                        {layouts, "classic"}});
    // ...A CLEARED ROW TAKES WORKSHOP'S, `classic` too, so nothing more is said...
    from = d.directory->chosen.size();
    REQUIRE(d.pane_skin(nullptr).accepted);
    CHECK(d.chosen_since(from).empty());
    // ...AND A ROW SAYING `block` UNDER IT MOVES ITS AREA AND ITS FRAME ALONE.
    from = d.directory->chosen.size();
    REQUIRE(d.pane_skin("block").accepted);
    CHECK(d.chosen_since(from) == Skins{{pane, "block"}, {frame, "block"}});
    // `night` AND `day` ARE SKINS AS THE OTHER TWO ARE: Workshop's own reaches its area, the
    // row's its pane's, and each goes back as it came.
    from = d.directory->chosen.size();
    const OfficeSettingCommitAnswered night = d.workshop_skin("night");
    REQUIRE_MESSAGE(night.accepted, night.refusal);
    CHECK(d.r.session().skin == "night");
    CHECK(d.chosen_since(from) == Skins{{kWorkshopProvider, "night"},
                                        {kWorkshopOverlayArea, "night"},
                                        {layouts, "night"}});
    from = d.directory->chosen.size();
    const PaneSettingCommitAnswered day = d.pane_skin("day");
    REQUIRE_MESSAGE(day.accepted, day.refusal);
    CHECK(d.chosen_since(from) == Skins{{pane, "day"}, {frame, "day"}});
    from = d.directory->chosen.size();
    REQUIRE(d.workshop_skin("classic").accepted);
    REQUIRE(d.pane_skin("block").accepted);
    CHECK(d.chosen_since(from) == Skins{{kWorkshopProvider, "classic"},
                                        {kWorkshopOverlayArea, "classic"},
                                        {layouts, "classic"},
                                        {pane, "block"},
                                        {frame, "block"}});

    // A REPAINT THAT MOVED NOTHING SAYS NO SKIN.
    from = d.directory->chosen.size();
    d.r.key(input::scan::kGrave);
    CHECK(d.chosen_since(from).empty());
    // A DIRECTORY THAT STARTS AFRESH HEARS EVERY AREA'S SKIN AGAIN, the row's included.
    from = d.directory->chosen.size();
    d.directory_does();
    CHECK(d.chosen_since(from, &said) == Skins{{kWorkshopProvider, "classic"},
                                               {kWorkshopOverlayArea, "classic"},
                                               {layouts, "classic"},
                                               {pane, "block"},
                                               {frame, "block"}});
    CHECK(said == 5);
    CHECK(d.refused.empty());
}

TEST_CASE("a pane placed off the cell grid has its body under its title row: at once where the "
          "window shows every pixel, on the next cell where its unit is the cell") {
    Session s;
    s.screen_w = cells_px(120);
    s.screen_h = cells_px(60);
    const PixelRect pane{cells_px(10) + 5, 503, cells_px(40), cells_px(12)};
    // THE OS WINDOW'S UNIT, ITS PIXEL: the title row is drawn from the region's top a cell tall,
    // and the body stands where it ends, at the title's column.
    s.cell_px = gfx::kCellPx;
    Screen sc = screen_of(s);
    ExternalBodyPlace titled = external_body_place(pane, sc, 1);
    REQUIRE(titled.header_rows == 1);
    PixelRect body = canvas_body_place(pane, sc, 1);
    CHECK(body.y == titled.region_y + gfx::kCellPx);
    CHECK(body.x == titled.region_x);
    // A TERMINAL'S UNIT, ITS CELL: the terminal floors the title row to its cell, and the body is
    // the cell under it.
    s.cell_px = 0;
    sc = screen_of(s);
    titled = external_body_place(pane, sc, 1);
    body = canvas_body_place(pane, sc, 1);
    CHECK(body.y == gfx::px_of_cells(gfx::cell_of_pixel(titled.region_y) + 1));
    CHECK(body.x == gfx::px_of_cells(gfx::cell_of_pixel(titled.region_x)));
}

TEST_CASE("every pane's graphics area is `<office>.<pane>`, an office's one pane as each of its "
          "several") {
    // ⚔ MUTATION: an office's area named by the office alone -- two panes of one office would
    // share it, and its one pane would move area when a second is offered.
    Panes panes;
    const auto offered = [&panes](const char* office, const char* pane) {
        RuntimePane row;
        row.provider = office;
        row.pane = pane;
        panes.runtime.entries.push_back(row);
        return row;
    };
    const RuntimePane alone = offered("zengine.test.alone", "only");
    CHECK(pane_window_area(panes, alone) == "zengine.test.alone.only");
    CHECK(pane_window_area(panes, alone) == pane_area("zengine.test.alone", "only"));
    const RuntimePane first = offered("zengine.test.two", "first");
    const RuntimePane second = offered("zengine.test.two", "second");
    CHECK(pane_window_area(panes, first) == "zengine.test.two.first");
    CHECK(pane_window_area(panes, second) == "zengine.test.two.second");
    for (const char* office :
         {"zengine.desktop", "zengine.introspection", "zengine.info", "zengine.inventory-pane"}) {
        CAPTURE(office);
        CHECK(pane_window_area(panes, offered(office, "seat")) == std::string(office) + ".seat");
    }
}

TEST_CASE("with no graphics directory, or one that takes no placement, Workshop says nothing to it "
          "and nothing it says is refused") {
    for (const bool stand_in : {false, true}) {
        CAPTURE(stand_in);
        WindowDesk d(false);
        if (stand_in) {
            (void)pw::mount_directory(d.r.bus);
        }
        d.type(input::scan::kW, "w");
        d.r.key(input::scan::kEscape);
        d.type(input::scan::kA, "a");
        d.r.key(input::scan::kEscape);
        press_body(d.r, d.kind);
        CHECK(d.said.empty());
        CHECK(d.refused.empty());
    }
}

TEST_CASE("Workshop draws its desk in the window that shows its areas -- the band in its own, "
          "each pane's frame, Layouts' too, in one at the pane's depth -- leaving the pane's body "
          "to its own area, and a press there on a part the drawing names is that press on the "
          "desk") {
    WindowDesk d(true);
    pw::Window& window = *d.window;
    const Session& s = d.r.session();
    const Screen sc = screen_of(s);
    const std::string area = kWorkshopProvider;
    window.room(area, area, sc.w / gfx::kCellPx, sc.h / gfx::kCellPx);
    REQUIRE_FALSE(window.weave().heard.empty());
    CHECK(window.weave().heard.front().whole);
    // EACH PANE'S FRAME where Workshop placed it, its rows read from the frame's own corner.
    const auto framed = [&](std::int64_t kind) {
        const std::string frame = pane_frame_area(d.r.session().panes, kind);
        REQUIRE(d.directory->areas.count(frame) == 1);
        const gfx::AreaPlaced at = d.directory->areas.at(frame);
        window.room(frame, kWorkshopProvider, at.w / gfx::kCellPx, at.h / gfx::kCellPx);
        window.at(frame, at.x, at.y);
        return std::make_pair(frame, PixelRect{at.x, at.y, at.w, at.h});
    };
    const auto [layouts_frame, layouts_at] = framed(pane_kind::kLayouts);
    const auto [frame, bounds] = framed(d.kind);
    CHECK(window.weave().refusals.empty());
    const auto row_of = [&window](const std::string& in, const PixelRect& at, std::int64_t y) {
        const std::vector<std::string> shown = window.rows(in);
        const auto i = static_cast<std::size_t>((y - at.y) / gfx::kCellPx);
        return i < shown.size() ? shown[i] : std::string();
    };
    const std::vector<std::string> rows = window.rows(frame);

    // LAYOUTS, AT THE TOP, IN ITS FRAME: its tab run where the skin paints it.
    const ExternalBodyPlace layouts = layouts_body(s, sc);
    REQUIRE(layouts.present);
    CHECK(row_of(layouts_frame, layouts_at, layouts.region_y)
              .rfind(trimmed(band_status(s, layouts).text), 0) == 0);
    // THE PANE'S HEADER, with its `?` and gear, and its edge as a ring, in its frame.
    REQUIRE(bounds == external_pane_rect(s, d.kind));
    const ExternalBodyPlace header =
        external_body_place(bounds, sc, external_title_rows(s.panes, d.kind, s.pane_titles));
    const RuntimePane* row = s.panes.runtime.of_kind(d.kind);
    REQUIRE(row != nullptr);
    REQUIRE(s.panes.external_pane(d.kind)->manual);
    const std::string title = row_of(frame, bounds, header.region_y)
                                  .substr(static_cast<std::size_t>(
                                      (header.region_x - bounds.x) / gfx::kCellPx));
    CHECK(title == trimmed(external_header_row(*row, false, true, header.columns)));
    REQUIRE(title.size() >= 3);
    CHECK(title.substr(title.size() - 3) == kChromeMarks);
    bool ring = false;
    for (const gw::KeptNode& n : window.weave().find(frame)->nodes) {
        if (n.node.op != gfx::op::kRect) continue;
        const auto r = gfx::from_body<gfx::RectArgs>(n.node.args);
        ring = ring || (r && r->x == bounds.x && r->y == bounds.y && r->w == bounds.w &&
                        r->h == bounds.h && r->fill.none() && !r->stroke.none());
    }
    CHECK(ring);
    // THE BAND, AT THE FOOT, in Workshop's own area: what the keys mean, under what the tool last
    // said; and no pane there, which stands in its frame.
    const ScreenRegion band = band_region(s, sc);
    REQUIRE(band.rows.size() >= 2);
    CHECK(row_of(area, PixelRect{}, band.y + gfx::kCellPx) == trimmed(band.rows[1].text));
    CHECK_FALSE(paints_inside(painted_cells(*window.weave().find(area)), bounds));

    // THE PANE'S BODY IS ITS OWN AREA'S: behind it its frame lays ground and says only, on the
    // body's first row, that it waits for the pane -- words the pane's own ground covers -- and
    // while arranging Workshop's own area, in front of it, leaves it unpainted.
    const PixelRect body = d.pane_body();
    const PixelRect local{body.x - bounds.x, body.y - bounds.y, body.w, body.h};
    const auto left = static_cast<std::size_t>(local.x / gfx::kCellPx);
    const auto right = static_cast<std::size_t>((local.x + local.w) / gfx::kCellPx);
    for (std::int64_t y = body.y; y < body.y + body.h; y += gfx::kCellPx) {
        CAPTURE(y);
        const std::string said = row_of(frame, bounds, y).substr(0, right);
        if (y == body.y) {
            CHECK(said.find(kExternalWaiting, left) != std::string::npos);
        } else {
            CHECK(trimmed(said).size() <= left);
        }
    }
    CHECK(paints_inside(painted_cells(*window.weave().find(frame)), local));
    d.type(input::scan::kW, "w");
    REQUIRE(d.r.session().arrange.open);
    CHECK_FALSE(paints_inside(painted_cells(*window.weave().find(area)), body));
    d.r.key(input::scan::kEscape);
    REQUIRE_FALSE(d.r.session().arrange.open);

    // THE PARTS A PRESS THERE ASKS FOR, each over the cells Workshop's own press inverse reads.
    const std::string k = std::to_string(d.kind);
    const std::set<std::string> laid = window.actions(layouts_frame);
    CHECK(laid.count("layout:0") == 1);
    CHECK(laid.count("layout:new") == 1);
    const std::set<std::string> parts = window.actions(frame);
    for (const std::string& name :
         std::vector<std::string>{"focus:" + k, "settings:" + k, "manual:" + k}) {
        CAPTURE(name);
        CHECK(parts.count(name) == 1);
    }
    const auto manual = window.cell_of(frame, "manual:" + k);
    REQUIRE(manual.has_value());
    CHECK(chrome_mark_at(s.panes, s.setup.active, sc, d.kind, s.pane_titles,
                         input::space::kPixels, bounds.x + manual->first * gfx::kCellPx + 1,
                         bounds.y + manual->second * gfx::kCellPx + 1) == chrome_mark::kManual);
    const auto gear = window.cell_of(frame, "settings:" + k);
    REQUIRE(gear.has_value());
    CHECK(chrome_mark_at(s.panes, s.setup.active, sc, d.kind, s.pane_titles,
                         input::space::kPixels, bounds.x + gear->first * gfx::kCellPx + 1,
                         bounds.y + gear->second * gfx::kCellPx + 1) == chrome_mark::kSettings);

    // THE TITLE PRESSED THERE PUTS THE PANE IN FRONT AND GIVES IT THE KEYS; the drawing follows.
    REQUIRE(s.panes.selected != d.kind);
    std::size_t heard = window.weave().heard.size();
    REQUIRE(window.click(frame, "focus:" + k));
    CHECK(s.panes.selected == d.kind);
    CHECK(s.panes.keyboard == d.kind);
    CHECK(window.weave().heard.size() > heard);
    CHECK(window.rows(frame) != rows);
    // ...where Workshop still takes the window's keys, the pane's own area taking none of them.
    CHECK(typing_pane(s) == d.kind);
    CHECK(d.directory->areas.at(area).keys);
    CHECK_FALSE(d.directory->areas.at(kAttentionArea).keys);
    // ...THE `?`, the desktop's manual row asked about the pane...
    const std::size_t asked = d.desktop->asked().size();
    REQUIRE(window.click(frame, "manual:" + k));
    REQUIRE(d.desktop->asked().size() == asked + 1);
    CHECK(d.desktop->asked().back() == kManualAction);
    // ...THE GEAR, Workshop's own menu at the pane's settings, in front, its lines parts too. A
    // drawing that gains or loses a node is a new root, the old one removed; the same desk
    // drawn again is the same root, and a word changed in place keeps it.
    const auto root = [&] {
        for (const gw::KeptNode& n : window.weave().find(area)->nodes) {
            if (n.node.path.find('/') == std::string::npos) return n.node.path;
        }
        return std::string();
    };
    const std::string closed = root();
    REQUIRE(window.click(frame, "settings:" + k));
    REQUIRE(s.context.open);
    CHECK(d.directory->areas.at(area).order == d.panes_on_desk() + 1);
    CHECK(window.actions(area).count("menu:0") == 1);
    CHECK(root() != closed);
    d.r.key(input::scan::kEscape);
    REQUIRE_FALSE(s.context.open);
    CHECK(window.actions(area).count("menu:0") == 0);
    CHECK(root() == closed);
    {
        Session said = s;
        said.notice = "a notice of another length entirely";
        const gfx::Sketch now = workshop_window_drawing(s).sketch;
        const gfx::Sketch then = workshop_window_drawing(said).sketch;
        REQUIRE_FALSE(now.nodes().empty());
        CHECK(now.nodes().front().path == closed);
        CHECK(then.nodes().front().path == closed);
        CHECK(gfx::difference(now, then).put.size() == 1);
    }
    // ...AND `+`, A FRESH LAYOUT, live: its tab is a part, and the pane, not on it, has no area,
    // nor a frame.
    const std::size_t layouts_before = layout_count(s.setup);
    REQUIRE(window.click(layouts_frame, "layout:new"));
    CHECK(layout_count(s.setup) == layouts_before + 1);
    CHECK(s.setup.active_at == layouts_before);
    CHECK(window.actions(layouts_frame).count("layout:" + std::to_string(layouts_before)) == 1);
    CHECK(d.directory->areas.at(kAttentionArea).window.empty());
    CHECK(d.directory->areas.at(frame).window.empty());
    // A PRESS SAID BY ANYONE BUT THE WINDOW, or about another area, presses nothing.
    heard = window.weave().heard.size();
    const auto tab = window.cell_of(layouts_frame, "layout:0");
    REQUIRE(tab.has_value());
    const std::int64_t tx = layouts_at.x + tab->first * gfx::kCellPx + gfx::kCellPx / 2;
    const std::int64_t ty = layouts_at.y + tab->second * gfx::kCellPx + gfx::kCellPx / 2;
    for (const bool down : {true, false}) {
        d.directory_does([tx, ty, down](loom::Mail& m) {
            (void)m.as_role(gfx::kGraphicsRole)
                .send_to_role(kWorkshopProvider,
                              gfx::AreaButtonChanged{kWorkshopProvider, 1, down, tx, ty, 0});
        });
    }
    window.click("zengine.test.elsewhere", tab->first, tab->second);
    CHECK(window.weave().heard.size() == heard);
    CHECK(s.setup.active_at == layouts_before);

    // A CHANGE THE WINDOW REFUSES IS DRAWN AGAIN WHOLE, after any change already on its way, and
    // the window then shows the desk as it is: the first layout's tab pressed, the pane back on
    // the desk and its area and its frame placed again.
    window.weave().refuse_next = true;
    heard = window.weave().heard.size();
    REQUIRE(window.click(layouts_frame, "layout:0"));
    CHECK(s.setup.active_at == 0);
    REQUIRE(window.weave().refusals.size() == 1);
    REQUIRE(window.weave().heard.size() > heard + 1);
    CHECK_FALSE(window.weave().heard[heard].whole);
    bool again = false;
    for (std::size_t i = heard + 1; i < window.weave().heard.size(); ++i) {
        again = again || (window.weave().heard[i].area == window.weave().heard[heard].area &&
                          window.weave().heard[i].whole);
    }
    CHECK(again);
    CHECK(d.directory->areas.at(kAttentionArea).window == gfx::kTerminalWindow);
    (void)framed(d.kind);
    CHECK(window.actions(frame).count("focus:" + k) == 1);

    // THE TAB PRESSED TWICE NAMES THE LAYOUT: the name being typed, and its caret as a bar.
    d.r.clock.together();
    REQUIRE(window.click(layouts_frame, "layout:0"));
    REQUIRE(window.click(layouts_frame, "layout:0"));
    d.r.clock.apart();
    REQUIRE(s.setup.naming.open);
    const auto shown = window.rows(layouts_frame);
    CHECK(std::any_of(shown.begin(), shown.end(), [](const std::string& line) {
        return line.find(kSetupNamePrompt) != std::string::npos;
    }));
    const auto& nodes = window.weave().find(layouts_frame)->nodes;
    CHECK(std::any_of(nodes.begin(), nodes.end(), [](const gw::KeptNode& n) {
        return n.node.op == gfx::op::kLine;
    }));
    d.r.key(input::scan::kEscape);
    CHECK_FALSE(s.setup.naming.open);
    CHECK(d.refused.empty());
}

TEST_CASE("a press the graphics window gives Workshop's own area is the press it is, held to its own "
          "release, and one it gives a pane's area is the pane's, Workshop told where it landed") {
    WindowDesk d(true);
    pw::Window& window = *d.window;
    const Session& s = d.r.session();
    const Screen sc = screen_of(s);
    const std::string desk = kWorkshopProvider;
    window.room(desk, desk, sc.w / gfx::kCellPx, sc.h / gfx::kCellPx);
    std::size_t relayed = 0;
    const loom::ObserverId tap = d.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.sender == d.r.workshop_id && e.kind == loom::EventKind::Delivered &&
            (e.schema_name == "PaneCanvasPointer" || e.schema_name == "PanePressed" ||
             e.schema_name == "PaneButton")) {
            ++relayed;
        }
    });
    // THE PANE'S HEADER AND BODY, in the window's cells, wherever the pane stands now.
    ExternalBodyPlace body;
    std::int64_t col = 0, header_row = 0, body_row = 0;
    const auto measure = [&] {
        body = external_body_place(external_pane_rect(s, d.kind), sc,
                                   external_title_rows(s.panes, d.kind, s.pane_titles));
        col = body.region_x / gfx::kCellPx + 1;
        header_row = body.region_y / gfx::kCellPx;
        body_row = header_row + body.header_rows;
    };
    measure();

    // WHILE ARRANGING THE DESK STANDS IN FRONT: a press on the pane takes hold of it, and the hold
    // lasts through the window's moves until its own release, which places the pane.
    d.type(input::scan::kW, "w");
    REQUIRE(s.arrange.open);
    const PixelRect before = external_pane_rect(s, d.kind);
    const std::uint64_t press = window.next_press();
    window.button(desk, col, body_row, true, press);
    CHECK(s.pane_drag.active);
    window.move(desk, col + 8, body_row + 3, press);
    CHECK(s.pane_drag.active);
    CHECK(external_pane_rect(s, d.kind) != before);
    window.button(desk, col + 8, body_row + 3, false, press);
    CHECK_FALSE(s.pane_drag.active);
    // ...AND A RIGHT PRESS LEAVES THE ARRANGEMENT, spent on leaving it.
    window.click(desk, col, body_row, 3);
    CHECK_FALSE(s.arrange.open);
    CHECK_FALSE(s.context.open);

    // A RIGHT PRESS ON THE PANE'S HEADER ASKS WORKSHOP'S OWN MENU ABOUT IT, and a middle press is
    // dropped.
    measure();
    window.click(desk, col, header_row, 3);
    REQUIRE(s.context.open);
    CHECK(s.context.subject == context_subject::kPane);
    d.r.key(input::scan::kEscape);
    REQUIRE_FALSE(s.context.open);
    window.click(desk, col, header_row, 2);
    CHECK_FALSE(s.context.open);

    // A PRESS IN THE PANE'S OWN AREA IS THE PANE'S: Workshop, told where it landed under the
    // window's number, chooses the pane and gives it the keys, and relays nothing to it.
    const std::string area = kAttentionArea;
    window.room(area, apane::kAttentionPaneRole, body.columns, 4);
    window.at(area, body.region_x, body.region_y + body.header_rows * gfx::kCellPx);
    press_outside(d.r, d.kind);
    REQUIRE(s.panes.selected != d.kind);
    relayed = 0;
    window.click(area, 0, 0, 1, kWorkshopProvider);
    CHECK(s.panes.selected == d.kind);
    CHECK(s.panes.keyboard == d.kind);
    CHECK(relayed == 0);
    // ...and the same word from anyone but the window, or about an area this desk did not place,
    // chooses nothing.
    press_outside(d.r, d.kind);
    for (const bool down : {true, false}) {
        d.directory_does([&area, down](loom::Mail& m) {
            (void)m.as_role(gfx::kGraphicsRole)
                .send_to_role(kWorkshopProvider,
                              gfx::AreaPressed{gfx::kTerminalWindow, area, 1, down, 0, 0}, 7);
        });
    }
    window.click("zengine.test.elsewhere", 0, 0, 1, kWorkshopProvider);
    CHECK(s.panes.selected != d.kind);

    // A PRESS FROM ANYONE ELSE IS THE PRESS IT IS: the platform's, while arranging, holds until its
    // own release.
    d.type(input::scan::kW, "w");
    REQUIRE(s.arrange.open);
    const std::int64_t x = body.region_x + gfx::kCellPx / 2;
    const std::int64_t y = body.region_y + body.header_rows * gfx::kCellPx + gfx::kCellPx / 2;
    d.r.hand_moment(input::PointerButton{1, true, x, y, input::space::kPixels,
                                                    input::mod::kNone});
    CHECK(s.pane_drag.active);
    d.r.hand_moment(input::PointerButton{1, false, x, y, input::space::kPixels,
                                                    input::mod::kNone});
    CHECK_FALSE(s.pane_drag.active);
    d.r.bus.remove_observer(tap);
}

namespace {

struct AreaSeatState {
    std::int64_t heard = 0;
    ZEN_SHAPE(AreaSeatState, 1, ZEN_FIELD(heard));
};

/// A PANE'S OFFICE THAT DRAWS IN A GRAPHICS WINDOW, or (`Window` false) one that takes no window's
/// room: it offers when a case asks.
template <bool Window>
class AreaSeat;
template <>
class AreaSeat<true>
    : public loom::WeaveBase<AreaSeat<true>, AreaSeatState, loom::Accept<gfx::AreaRoom, SeatDo>,
                             loom::Emit<PaneOffered>> {
public:
    void on(const gfx::AreaRoom&, loom::Mail&) { ++state_.heard; }
    void on(const SeatDo&, loom::Mail& mail) {
        auto run = std::move(next);
        next = {};
        if (run) run(mail);
    }
    std::int64_t heard() const { return state_.heard; }
    std::function<void(loom::Mail&)> next;
};
template <>
class AreaSeat<false>
    : public loom::WeaveBase<AreaSeat<false>, AreaSeatState, loom::Accept<SeatDo>,
                             loom::Emit<PaneOffered>> {
public:
    void on(const SeatDo&, loom::Mail& mail) {
        auto run = std::move(next);
        next = {};
        if (run) run(mail);
    }
    std::function<void(loom::Mail&)> next;
};

/// ONE SUCH SEAT MOUNTED IN `office`, its pane `pane` offered and opened; its kind.
template <bool Window>
std::int64_t open_area_seat(PaneRig& r, const char* office, const char* pane, AreaSeat<Window>** made) {
    auto seat = std::make_unique<AreaSeat<Window>>();
    AreaSeat<Window>* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
    const loom::WeaveId id = r.bus.register_weave(std::move(seat), std::move(grant), office);
    raw->zen_set_self(id);
    raw->next = [office, pane](loom::Mail& m) {
        (void)m.as_role(office).send_to_role(kWorkshopProvider, PaneOffered{pane, "Seat", "a seat"});
    };
    (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
    r.bus.drain_until_idle();
    r.pick(PaneRef{office, pane});
    *made = raw;
    const RuntimePane* row = r.session().panes.runtime.find(office, pane);
    return row == nullptr ? kNoPaneKind : row->kind;
}

constexpr const char* kAreaSeatOffice = "zengine.test.desk-area-seat";
constexpr const char* kRowSeatOffice = "zengine.test.desk-row-seat";

} // namespace

TEST_CASE("a pane whose office draws in no graphics window has no area there, and its frame says so; "
          "one that does is placed its area, titled, and nothing is drawn over it while arranging") {
    WindowDesk d(true);
    pw::Window& window = *d.window;
    const Session& s = d.r.session();
    const std::string area = kWorkshopProvider;
    {
        const Screen sc = screen_of(s);
        window.room(area, area, sc.w / gfx::kCellPx, sc.h / gfx::kCellPx);
    }
    // EVERY FRAME WORKSHOP PLACED, its room told at its place, and the words its areas show.
    const auto frames = [&] {
        std::vector<std::string> out;
        for (const auto& [name, placed] : d.directory->areas) {
            if (placed.owner != kWorkshopProvider || name == area || placed.window.empty() ||
                name == kWorkshopOverlayArea) {
                continue;
            }
            const gw::WinArea* kept = window.weave().find(name);
            if (kept == nullptr || kept->w != placed.w || kept->h != placed.h) {
                window.room(name, kWorkshopProvider, placed.w / gfx::kCellPx,
                            placed.h / gfx::kCellPx);
            }
            window.at(name, placed.x, placed.y);
            out.push_back(name);
        }
        return out;
    };
    const auto shows = [&](const std::string& words) {
        for (const std::string& name : frames()) {
            for (const std::string& row : window.rows(name)) {
                if (row.find(words) != std::string::npos) return true;
            }
        }
        return false;
    };

    // AN OFFICE THAT TAKES NO WINDOW'S ROOM: no area, and its frame says it draws in no window.
    AreaSeat<false>* rows_seat = nullptr;
    const std::int64_t rows_kind = open_area_seat(d.r, kRowSeatOffice, "rows", &rows_seat);
    REQUIRE(rows_kind != kNoPaneKind);
    REQUIRE(s.panes.has(rows_kind));
    CHECK(d.directory->areas.count(pane_area(kRowSeatOffice, "rows")) == 0);
    CHECK(shows(std::string("Seat") + kExternalNoWindow));

    // AN OFFICE THAT TAKES ONE: its area, placed in the window.
    AreaSeat<true>* seat = nullptr;
    const std::int64_t kind = open_area_seat(d.r, kAreaSeatOffice, "seat", &seat);
    REQUIRE(kind != kNoPaneKind);
    REQUIRE(s.panes.has(kind));
    REQUIRE(d.directory->areas.count(pane_area(kAreaSeatOffice, "seat")) == 1);
    CHECK(d.directory->areas.at(pane_area(kAreaSeatOffice, "seat")).window == gfx::kTerminalWindow);
    // ...and a title, but no `?` or gear: its office answers no manual.
    (void)frames();
    const std::set<std::string> parts =
        window.actions(pane_frame_area(d.r.session().panes, kind));
    CHECK(parts.count("focus:" + std::to_string(kind)) == 1);
    CHECK(parts.count("manual:" + std::to_string(kind)) == 0);
    CHECK(parts.count("settings:" + std::to_string(kind)) == 0);
    // ...and while arranging, in front of its area, Workshop paints nothing over it.
    d.type(input::scan::kW, "w");
    REQUIRE(s.arrange.open);
    const PixelRect seat_body =
        canvas_body_place(external_pane_rect(s, kind), screen_of(s),
                          external_title_rows(s.panes, kind, s.pane_titles));
    REQUIRE_FALSE(seat_body.empty());
    CHECK_FALSE(paints_inside(painted_cells(*window.weave().find(area)), seat_body));
    d.r.key(input::scan::kEscape);
    REQUIRE_FALSE(s.arrange.open);
    CHECK(d.refused.empty());
}

namespace {

struct DeskSeatState {
    std::int64_t heard = 0;
    ZEN_SHAPE(DeskSeatState, 1, ZEN_FIELD(heard));
};

/// A PARTICIPANT THAT SAYS ONE THING AS ITS OFFICE inside its own delivery, when a case asks.
template <class Said>
class SayingSeat
    : public loom::WeaveBase<SayingSeat<Said>, DeskSeatState, loom::Accept<SeatDo>,
                             loom::Emit<Said>> {
public:
    void on(const SeatDo&, loom::Mail& mail) {
        auto run = std::move(next);
        next = {};
        if (run) run(mail);
    }
    std::function<void(loom::Mail&)> next;
};

/// ONE SUCH PARTICIPANT MOUNTED IN `office`, granted to say `Said` to anyone, and the case's hand.
template <class Said>
struct Sayer {
    PaneRig& r;
    std::string office;
    SayingSeat<Said>* seat = nullptr;
    loom::WeaveId id{};
    Sayer(PaneRig& rig, std::string in) : r(rig), office(std::move(in)) {
        auto made = std::make_unique<SayingSeat<Said>>();
        seat = made.get();
        loom::Grant grant;
        grant.allow_to_any(Said::zen_name, Said::zen_version);
        id = r.bus.register_weave(std::move(made), std::move(grant), office);
        seat->zen_set_self(id);
    }
    /// Publish `said` as this office, and let the bus settle.
    void publish(const Said& said) {
        seat->next = [as = office, said](loom::Mail& m) { (void)m.as_role(as).publish(said); };
        (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    }
};

std::string terminal_window_office() {
    return std::string(gfx::kWindowOfficePrefix) + gfx::kTerminalWindow;
}

std::string sdl_window_office() {
    return std::string(gfx::kWindowOfficePrefix) + gfx::kSdlWindow;
}

/// THE OS WINDOW AS WORKSHOP MEETS IT, in `zengine.window.sdl`: it says what a case hands it as
/// its office, keeps every drawing and place offered back, and answers a paste with what its
/// platform holds, counting each read.
class SdlWindowSeat
    : public loom::WeaveBase<SdlWindowSeat, DeskWindowState,
                             loom::Accept<gfx::Drawn, gfx::ClipboardTextRequested,
                                          gfx::SurfacePlacementRemembered, SeatDo>,
                             loom::Emit<gfx::WindowOffered, gfx::AreaRoom, gfx::AreaButtonChanged,
                                        gfx::ClipboardText>> {
public:
    void on(const gfx::Drawn&, loom::Mail&) { ++state_.drawn; }
    void on(const gfx::ClipboardTextRequested&, loom::Mail& mail) {
        ++reads;
        (void)mail.answer(gfx::ClipboardText{true, platform});
    }
    void on(const gfx::SurfacePlacementRemembered& p, loom::Mail&) { offered.push_back(p); }
    void on(const SeatDo&, loom::Mail& mail) {
        auto run = std::move(next);
        next = {};
        if (run) run(mail);
    }
    std::int64_t drawn() const { return state_.drawn; }

    std::function<void(loom::Mail&)> next;
    std::vector<gfx::SurfacePlacementRemembered> offered;
    std::string platform;
    int reads = 0;
};

} // namespace

TEST_CASE("the terminal window offered brings Workshop up in it, the last session restored first, "
          "and lays the desk out in the window's room") {
    TempDir dir("window-offered");
    PaneRig r;
    r.host.session_path = dir.file("session.json");
    REQUIRE(save_last_run(r.host.session_path, one_layout(setup_of("Offered", {})), 0,
                          cells_px(100), cells_px(40), window_persist::Placement{})
                .accepted);
    ProviderSeat* seat = r.mount_provider("zengine.test.offered-seat");
    r.mount_workshop();
    Sayer<gfx::WindowOffered> window(r, terminal_window_office());
    const auto offer = [&window](std::int64_t cols, std::int64_t rows,
                                 std::string name = gfx::kTerminalWindow) {
        window.publish(gfx::WindowOffered{name, cells_px(cols), cells_px(rows), 7, 0, gfx::kCellPx,
                                          gfx::kCellPx});
    };
    REQUIRE(seat->asks.empty());

    // OFFERED, WITH NOTHING UP YET: Workshop comes up -- the room asked who has panes, the last
    // session restored...
    offer(90, 30);
    CHECK(seat->asks == std::vector<std::string>{kWorkshopProvider});
    CHECK(r.session().setup.active.name == "Offered");
    CHECK(r.last_notice().find("reopened your last desk") != std::string::npos);
    // ...and lays the desk out in the window's room, taken after the room the session restored,
    // a cell of text a cell.
    CHECK(r.session().screen_w == cells_px(90));
    CHECK(r.session().screen_h == cells_px(30));
    CHECK(r.session().normal_w == cells_px(90));
    CHECK(r.session().normal_h == cells_px(30));
    CHECK(r.session().text_advance_px == 0);
    CHECK(r.session().text_line_px == 0);
    CHECK(r.session().cell_px == 0);
    CHECK(paint(r.session()).width == cells_px(90));
    CHECK(paint(r.session()).height == cells_px(30));

    // OFFERED AGAIN AT ITS SIZE: nothing comes up a second time, and the room stands.
    offer(90, 30);
    CHECK(seat->asks.size() == 1);
    CHECK(r.session().screen_w == cells_px(90));
    // ...AT ANOTHER: the desk is laid out in it again.
    offer(120, 40);
    CHECK(r.session().screen_w == cells_px(120));
    CHECK(r.session().screen_h == cells_px(40));
    CHECK(paint(r.session()).width == cells_px(120));

    // AN OFFER OF NO ROOM, OF ANOTHER WINDOW -- even from that window's own office -- OR FROM ANY
    // OFFICE BUT THE WINDOW'S MOVES NOTHING.
    offer(0, 24);
    offer(80, 0);
    offer(80, 24, "desk");
    Sayer<gfx::WindowOffered> desk_window(r, "zengine.window.desk");
    desk_window.publish(gfx::WindowOffered{"desk", cells_px(80), cells_px(24), 7});
    desk_window.publish(gfx::WindowOffered{gfx::kTerminalWindow, cells_px(80), cells_px(24), 7});
    CHECK(r.session().screen_w == cells_px(120));
    CHECK(r.session().screen_h == cells_px(40));
    CHECK(seat->asks.size() == 1);

    SUBCASE("a terminal window with no terminal to measure offers no size, and Workshop comes up "
            "in it in the room the last session restored") {
        TempDir none_dir("window-offered-none");
        PaneRig n;
        n.host.session_path = none_dir.file("session.json");
        REQUIRE(save_last_run(n.host.session_path, one_layout(setup_of("Measured", {})), 0,
                              cells_px(150), cells_px(60), window_persist::Placement{})
                    .accepted);
        n.mount_workshop();
        Sayer<gfx::WindowOffered> unmeasured(n, terminal_window_office());
        unmeasured.publish(gfx::WindowOffered{gfx::kTerminalWindow, 0, 0, 7});
        CHECK(n.session().setup.active.name == "Measured");
        CHECK(n.session().screen_w == cells_px(150));
        CHECK(n.session().screen_h == cells_px(60));
    }
    SUBCASE("the OS window offered the same way brings Workshop up in it: the skins offered, the "
            "areas placed on it, the place remembered and a paste asked of it") {
        // ⚔ MUTATION: the offer of any window but the terminal's ignored -- nothing comes up.
        TempDir os_dir("sdl-window-offered");
        PaneRig o;
        o.host.dir = "/zen/sdl-window-desk";
        o.host.session_path = os_dir.file("session.json");
        REQUIRE(save_last_run(o.host.session_path,
                              one_layout(setup_of("Offered", {pane_kind::kLayouts})), 0,
                              cells_px(100), cells_px(40),
                              window_persist::Placement{true, 40, 60, false})
                    .accepted);
        auto made_directory = std::make_unique<ArrangedDirectory>();
        ArrangedDirectory* directory = made_directory.get();
        directory->zen_set_self(o.bus.register_weave(std::move(made_directory), loom::Grant{},
                                                     gfx::kGraphicsRole));
        o.mount_workshop();
        auto made_window = std::make_unique<SdlWindowSeat>();
        SdlWindowSeat* os = made_window.get();
        loom::Grant says;
        says.allow_to_any(gfx::WindowOffered::zen_name, gfx::WindowOffered::zen_version);
        says.allow_to_any(gfx::AreaRoom::zen_name, gfx::AreaRoom::zen_version);
        says.allow_to_any(gfx::AreaButtonChanged::zen_name, gfx::AreaButtonChanged::zen_version);
        says.allow_to_any(gfx::ClipboardText::zen_name, gfx::ClipboardText::zen_version);
        const loom::WeaveId os_id =
            o.bus.register_weave(std::move(made_window), std::move(says), sdl_window_office());
        os->zen_set_self(os_id);
        const auto os_does = [&o, os, os_id](std::function<void(loom::Mail&)> what) {
            os->next = std::move(what);
            (void)o.bus.send(os_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
            o.bus.drain_until_idle();
        };

        os_does([](loom::Mail& m) {
            (void)m.as_role(sdl_window_office())
                .publish(gfx::WindowOffered{gfx::kSdlWindow, 1085, 361, 3, gfx::kCellPx, 8, 18});
        });
        // UP IN IT, the last session restored first, and laid out in its room in pixels, its text
        // the face the window measured and the device unit the pixel, as the window says...
        CHECK(o.session().setup.active.name == "Offered");
        CHECK(o.session().screen_w == 1085);
        CHECK(o.session().screen_h == 361);
        CHECK(o.session().text_advance_px == 8);
        CHECK(o.session().text_line_px == 18);
        CHECK(o.session().cell_px == gfx::kCellPx);
        // ...ITS PLACE ON THE DESKTOP OFFERED TO IT...
        REQUIRE(os->offered.size() == 1);
        CHECK(os->offered[0].x == 40);
        CHECK(os->offered[0].y == 60);
        CHECK_FALSE(os->offered[0].maximized);
        // ...EVERY PAINTER SKIN OFFERED THE DIRECTORY, and Workshop's area placed on the OS window.
        std::vector<std::string> offered;
        for (const gfx::SkinOffered& s : directory->offered) {
            offered.push_back(s.skin);
        }
        CHECK(offered == std::vector<std::string>{"classic", "block", "day", "night"});
        REQUIRE(directory->areas.count(kWorkshopProvider) == 1);
        CHECK(directory->areas.at(kWorkshopProvider).window == gfx::kSdlWindow);
        // ...AND A PANE WHOSE OFFICE DRAWS IN A WINDOW, opened, placed on it too.
        REQUIRE(o.load(apane::kAttentionPaneStem, WORKSHOP_SO_ATTENTION_PANE,
                       apane::kAttentionPaneRole)
                    .valid());
        o.pick(PaneRef{apane::kAttentionPaneRole, apane::kAttentionPane});
        REQUIRE(directory->areas.count(kAttentionArea) == 1);
        CHECK(directory->areas.at(kAttentionArea).window == gfx::kSdlWindow);
        for (const gfx::AreaPlaced& placed : directory->placed) {
            CHECK(placed.window == gfx::kSdlWindow);
        }

        // THE WINDOW SHOWS WORKSHOP'S AREA: Workshop draws the desk there...
        os_does([](loom::Mail& m) {
            (void)m.as_role(sdl_window_office())
                .send_to_role(kWorkshopProvider,
                              gfx::AreaRoom{kWorkshopProvider, gfx::kSdlWindow, 1, cells_px(90),
                                            cells_px(30), {}});
        });
        CHECK(os->drawn() > 0);
        // ...AND A PASTE INTO THE NAME EDITOR IS ASKED OF IT, its answer landing in the line; the
        // press that opens the editor lands in Workshop's area through the window.
        o.own_hand = [&os_does](const input::PointerButton& b) {
            const bool cells = b.space == input::space::kCells;
            const std::int64_t x = cells ? gfx::px_of_cells(b.x) : b.x;
            const std::int64_t y = cells ? gfx::px_of_cells(b.y) : b.y;
            os_does([b, x, y](loom::Mail& m) {
                (void)m.as_role(sdl_window_office())
                    .send_to_role(kWorkshopProvider,
                                  gfx::AreaButtonChanged{kWorkshopProvider, b.button, b.pressed, x,
                                                         y, b.modifiers});
            });
        };
        open_rename_on_live_tab(o);
        REQUIRE(o.session().setup.naming.open);
        os->platform = "Evening";
        o.key(input::scan::kA, input::mod::kCtrl);
        o.key(input::scan::kV, input::mod::kCtrl);
        CHECK(os->reads == 1);
        CHECK(o.session().setup.naming.line.text() == "Evening");
        // ...AND ASKED AGAIN, each paste its own read.
        os->platform = "Night";
        o.key(input::scan::kA, input::mod::kCtrl);
        o.key(input::scan::kV, input::mod::kCtrl);
        CHECK(os->reads == 2);
        CHECK(o.session().setup.naming.line.text() == "Night");
    }
}

namespace {

/// A GUEST A GUEST DOOR ADMITTED, as its session stands on the bus: it asks Workshop for a mark,
/// and keeps every answer.
class GuideGuest
    : public loom::WeaveBase<GuideGuest, DeskSeatState, loom::Accept<SeatDo, GuideAnswered>,
                             loom::Emit<GuideRequested>> {
public:
    void on(const SeatDo&, loom::Mail& mail) {
        auto run = std::move(next);
        next = {};
        if (run) run(mail);
    }
    void on(const GuideAnswered& a, loom::Mail&) { answers.push_back(a); }
    std::function<void(loom::Mail&)> next;
    std::vector<GuideAnswered> answers;
};

constexpr const char* kMarkedSeatOffice = "zengine.test.desk-marked-seat";

/// THE ROWS WORKSHOP DRAWS IN ITS OWN AREA OF THE WINDOW, and whether one of them says `words`.
bool window_shows(pw::Window& window, const std::string& area, const std::string& words) {
    for (const std::string& row : window.rows(area)) {
        if (row.find(words) != std::string::npos) return true;
    }
    return false;
}

/// Whether Workshop's overlay there draws a plane: a carried value or a guide mark.
bool window_overlaid(pw::Window& window) {
    const gw::WinArea* a = window.weave().find(kWorkshopOverlayArea);
    REQUIRE(a != nullptr);
    return std::any_of(a->nodes.begin(), a->nodes.end(), [](const gw::KeptNode& k) {
        return k.node.path.find("/plane") != std::string::npos;
    });
}

} // namespace

TEST_CASE("in the terminal window a point in cells is the window's own cell, with no console row "
          "above the canvas, and the menu's press measurer reads each line's there") {
    PaneRig r;
    r.mount_workshop();
    Sayer<gfx::WindowOffered> window(r, terminal_window_office());
    window.publish(gfx::WindowOffered{gfx::kTerminalWindow, cells_px(120), cells_px(40), 7, 0,
                                      gfx::kCellPx, gfx::kCellPx});
    const Session& s = r.session();
    r.key(input::scan::kA);
    REQUIRE(s.context.open);
    const Screen sc = screen_of(s);
    const DeskView desk = r.w->desk_view();
    REQUIRE(desk.menu.open);
    REQUIRE_FALSE(desk.menu.parts.empty());
    const std::vector<ContextEntry> population = context_population(s.context);
    for (const PanePart& part : desk.menu.parts) {
        CAPTURE(part.name);
        REQUIRE(part.space == input::space::kCells);
        // ON THE ROW THE WINDOW DRAWS IT ON, the canvas's own...
        CHECK(part.y == gfx::cell_of_pixel(part.place.y));
        CHECK(part.x >= gfx::cell_of_pixel(part.place.x));
        CHECK(part.x < gfx::cell_of_pixel(part.place.x + part.place.w));
        // ...AND READ THERE: a press in cells at that point is that line's.
        const PointedAt at = canvas_point_of(part.space, part.x, part.y);
        const ContextPressAt hit = context_press_at(s, sc, part.space, part.x, part.y, at);
        REQUIRE(hit.entry);
        const ContextEntry& entry = population[hit.index];
        CHECK(std::string(entry.is_group ? entry.group : entry.row->id) == part.name);
    }
}

TEST_CASE("in the terminal window a row's point is the cell the window shows it on, and a press in "
          "cells there reaches the pane as that row and column") {
    DeskRig d;
    // THE TERMINAL WINDOW SHOWS WORKSHOP'S AREA, so its own rows are the cells a point names.
    REQUIRE(d.r.windows.window() == gfx::kTerminalWindow);
    const std::string text = "abcdef";
    d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
        s.say(m, SeatRows{"alpha", {TextRow{text, gfx::role::kFill},
                                       TextRow{text, gfx::role::kFill}}});
    });
    PaneView view;
    REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
    REQUIRE(view.words.size() == 2);
    for (const std::int64_t row : {std::int64_t{0}, std::int64_t{1}}) {
        CAPTURE(row);
        PanePoint at;
        REQUIRE(d.point(PanePointRequested{kAlphaOffice, "alpha", view.stamp.fingerprint, row, 3}, at)
                    .empty());
        REQUIRE(at.space == input::space::kCells);
        CHECK(at.y == gfx::cell_of_pixel(view.words[static_cast<std::size_t>(row)].place.y));
        d.alpha->presses.clear();
        d.click(at.x, at.y, at.space);
        REQUIRE(d.alpha->presses.size() == 1);
        CHECK(d.alpha->presses[0].row == row);
        CHECK(d.alpha->presses[0].column == 3);
    }
}

TEST_CASE("in the terminal window a canvas part's point is the window's own cell, inside the "
          "part") {
    SketchRig d;
    d.medium(false);
    // THE TERMINAL WINDOW SHOWS WORKSHOP'S AREA.
    REQUIRE(d.r.windows.window() == gfx::kTerminalWindow);
    const std::int64_t u = kPaneCanvasUnit;
    d.draw_named(
        {CanvasPart{"top", 0, 0, 4 * u, u}, CanvasPart{"low", 0, 2 * u, 4 * u, u}});
    REQUIRE(d.sketch->rejected.empty());
    PaneView view;
    REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
    REQUIRE(view.parts.size() == 2);
    for (const PanePart& part : view.parts) {
        CAPTURE(part.name);
        REQUIRE(part.space == input::space::kCells);
        const PointedAt at = canvas_point_of(part.space, part.x, part.y);
        REQUIRE(at.understood);
        CHECK(at.px.y >= part.place.y);
        CHECK(at.px.y < part.place.y + part.place.h);
    }
}

TEST_CASE("a guest's guide mark stands over the panes in Workshop's overlay in the window, which "
          "hears no pointer, the desk staying behind them, and goes with the mark") {
    WindowDesk d(true);
    pw::Window& window = *d.window;
    const Session& s = d.r.session();
    const std::string area = kWorkshopProvider;
    const std::string overlay = kWorkshopOverlayArea;
    {
        const Screen sc = screen_of(s);
        window.room(area, area, sc.w / gfx::kCellPx, sc.h / gfx::kCellPx);
        window.room(overlay, kWorkshopProvider, sc.w / gfx::kCellPx, sc.h / gfx::kCellPx);
    }
    // THE MARKED PANE beside Attention, which draws its body in its own area: a seat's one row
    // naming one control, the body free below it.
    ProviderSeat* seat = d.r.mount_provider(kMarkedSeatOffice);
    const std::int64_t kind = seat_pane_open(d.r, seat, kMarkedSeatOffice, "marked");
    REQUIRE(kind != kNoPaneKind);
    {
        const std::string seat_area = held_area_of(kind);
        REQUIRE(d.directory->areas.count(seat_area) == 1);
        const gfx::AreaPlaced placed = d.directory->areas.at(seat_area);
        window.room(seat_area, kMarkedSeatOffice, placed.w / gfx::kCellPx, placed.h / gfx::kCellPx);
        window.at(seat_area, placed.x, placed.y);
    }
    d.r.drive(seat, [](ProviderSeat& p, loom::Mail& m) {
        p.say(m, SeatRows{"marked",
                                       {TextRow{"[go] there", gfx::role::kFill}}, {RowPart{"control:go", 0, 0, 4}}});
    });
    // THE GUEST, admitted as `tutor`, its session alive.
    auto made = std::make_unique<GuideGuest>();
    GuideGuest* guest = made.get();
    loom::Grant grant;
    grant.allow_to_role(GuideRequested::zen_name, GuideRequested::zen_version, kWorkshopProvider);
    const loom::WeaveId guest_id = d.r.bus.register_weave(std::move(made), std::move(grant));
    guest->zen_set_self(guest_id);
    d.r.host.guest_row = [guest_id](loom::WeaveId who) {
        scope::GuestRowFacts row;
        if (who == guest_id) {
            row.admitted = true;
            row.name = "tutor";
        }
        return row;
    };
    d.r.host.input_authority = [&bus = d.r.bus](loom::WeaveId who) {
        return bus.alive(who) ? loom::host_describe_only_authority(bus, who)
                              : loom::GrantAuthority{};
    };
    const auto ask = [&](std::string control) {
        guest->next = [control](loom::Mail& m) {
            (void)m.send_to_role(kWorkshopProvider,
                                 GuideRequested{std::string(kMarkedSeatOffice) + "/marked", control,
                                                "press go"});
        };
        (void)d.r.bus.send(guest_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        d.r.bus.drain_until_idle();
    };
    REQUIRE(d.directory->areas.at(area).order == 0);
    REQUIRE_FALSE(window_shows(window, overlay, "tutor: "));
    REQUIRE_FALSE(window_overlaid(window));

    // MARKED: its label drawn in the overlay, in front of every pane's area, which hears no
    // pointer, so a press on the mark reaches the pane under it; it paints nothing over a body a
    // pane draws itself, and the desk stays behind the panes.
    ask("go");
    REQUIRE_FALSE(guest->answers.empty());
    REQUIRE_MESSAGE(guest->answers.back().shown, guest->answers.back().said);
    CHECK_MESSAGE(window_shows(window, overlay, "tutor: \"press go\""),
                  guest->answers.back().said);
    CHECK(window_overlaid(window));
    CHECK_FALSE(window_shows(window, area, "tutor: "));
    CHECK(d.directory->areas.at(area).order == 0);
    CHECK(d.directory->areas.at(overlay).order == d.panes_on_desk() + 2);
    CHECK_FALSE(d.directory->areas.at(overlay).pointer);
    CHECK_FALSE(paints_inside(painted_cells(*window.weave().find(overlay)), d.pane_body()));
    // ...ITS LABEL ON A GROUND OF ITS OWN, the label's characters wide and a line tall: the overlay
    // grounds what it draws there, and nothing else.
    {
        std::vector<gfx::TextArgs> labels;
        std::vector<gfx::RectArgs> grounds;
        for (const gw::KeptNode& n : window.weave().find(overlay)->nodes) {
            if (n.node.op == gfx::op::kText) {
                if (const auto t = gfx::from_body<gfx::TextArgs>(n.node.args)) labels.push_back(*t);
            } else if (n.node.op == gfx::op::kRect) {
                const auto r = gfx::from_body<gfx::RectArgs>(n.node.args);
                if (r && r->fill.role == gfx::role::kGround) grounds.push_back(*r);
            }
        }
        REQUIRE_FALSE(labels.empty());
        CHECK(std::any_of(labels.begin(), labels.end(), [](const gfx::TextArgs& t) {
            return t.text.find("tutor: ") != std::string::npos;
        }));
        REQUIRE(grounds.size() == labels.size());
        for (const gfx::TextArgs& label : labels) {
            CAPTURE(label.text);
            CHECK(std::any_of(grounds.begin(), grounds.end(), [&](const gfx::RectArgs& g) {
                return g.x == label.x && g.y == label.y && g.h == gfx::kCellPx &&
                       g.w == static_cast<std::int64_t>(label.text.size()) * gfx::kCellPx;
            }));
        }
    }

    // CLEARED: gone from the window.
    ask("");
    REQUIRE_FALSE(guest->answers.back().shown);
    CHECK_FALSE(window_shows(window, overlay, "tutor: "));
    CHECK_FALSE(window_overlaid(window));
    CHECK(d.directory->areas.at(area).order == 0);
    CHECK(d.refused.empty());
}

namespace {

/// A PANE THAT CARRIES A VALUE OUT UNDER THE PRESS THAT TOOK HOLD OF IT: it keeps the number each
/// press in its area came under, and what Workshop answered its operation and its carry.
class CarrySeat
    : public loom::WeaveBase<CarrySeat, DeskSeatState,
                             loom::Accept<PaneCatalogRequested, gfx::AreaRoom, gfx::AreaButtonChanged,
                                          PaneOperationAnswered, PaneCarryAnswered, SeatDo>,
                             loom::Emit<PaneOffered, PaneOperationRequested,
                                        PaneValueCarryRequested>> {
public:
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const gfx::AreaRoom&, loom::Mail&) {}
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        if (b.pressed) presses.push_back(mail.correlation());
    }
    void on(const PaneOperationAnswered& a, loom::Mail&) { operations.push_back(a); }
    void on(const PaneCarryAnswered& a, loom::Mail&) { carries.push_back(a); }
    void on(const SeatDo&, loom::Mail& mail) {
        auto run = std::move(next);
        next = {};
        if (run) run(mail);
    }
    std::function<void(loom::Mail&)> next;
    std::vector<std::uint64_t> presses;
    std::vector<PaneOperationAnswered> operations;
    std::vector<PaneCarryAnswered> carries;
};

constexpr const char* kCarrySeatOffice = "zengine.test.desk-carry-seat";

} // namespace

TEST_CASE("a value the weaver drags out of a pane is carried under the hand in Workshop's overlay "
          "in the window, the desk in front of the panes' areas while it is carried, and goes when "
          "the hand lets go") {
    WindowDesk d(true);
    pw::Window& window = *d.window;
    const Session& s = d.r.session();
    const std::string area = kWorkshopProvider;
    const std::string overlay = kWorkshopOverlayArea;
    {
        const Screen sc = screen_of(s);
        window.room(area, area, sc.w / gfx::kCellPx, sc.h / gfx::kCellPx);
        window.room(overlay, kWorkshopProvider, sc.w / gfx::kCellPx, sc.h / gfx::kCellPx);
    }
    // THE PANE A VALUE IS DRAGGED OUT OF, beside Attention, which draws its body in its own area.
    auto made = std::make_unique<CarrySeat>();
    CarrySeat* seat = made.get();
    loom::Grant grant;
    grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
    grant.allow_to_role(PaneOperationRequested::zen_name, PaneOperationRequested::zen_version,
                        kWorkshopProvider);
    grant.allow_to_role(PaneValueCarryRequested::zen_name, PaneValueCarryRequested::zen_version,
                        kWorkshopProvider);
    const loom::WeaveId seat_id =
        d.r.bus.register_weave(std::move(made), std::move(grant), kCarrySeatOffice);
    seat->zen_set_self(seat_id);
    const auto seat_does = [&](std::function<void(loom::Mail&)> what) {
        seat->next = std::move(what);
        (void)d.r.bus.send(seat_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        d.r.bus.drain_until_idle();
    };
    seat_does([](loom::Mail& m) {
        (void)m.as_role(kCarrySeatOffice)
            .send_to_role(kWorkshopProvider, PaneOffered{"carry", "Carry", "a value to drag"});
    });
    d.r.pick(PaneRef{kCarrySeatOffice, "carry"});
    const RuntimePane* row = s.panes.runtime.find(kCarrySeatOffice, "carry");
    REQUIRE(row != nullptr);
    REQUIRE(s.panes.has(row->kind));
    const std::int64_t kind = row->kind;
    // ITS AREA, as the window shows it where Workshop placed it.
    const std::string carry_area = pane_area(kCarrySeatOffice, "carry");
    REQUIRE(d.directory->areas.count(carry_area) == 1);
    {
        const auto& placed = d.directory->areas.at(carry_area);
        window.room(carry_area, kCarrySeatOffice, placed.w / gfx::kCellPx,
                    placed.h / gfx::kCellPx);
        window.at(carry_area, placed.x, placed.y);
    }
    // THE WEAVER'S OWN HAND, as the input office says it: each moment attributed to the local
    // hand, at a terminal cell.
    const CellRect body = external_body_rect(s, kind);
    const std::int64_t x = body.x + 1, y = body.y + kExternalHeaderRows;

    // THE PRESS IN THE PANE'S AREA TAKES HOLD OF ITS ROW, the window telling Workshop where it
    // landed, and the pane asks to carry its value under that press.
    const std::uint64_t n = window.next_press();
    window.button(carry_area, 1, 0, true, n, 1, kWorkshopProvider);
    REQUIRE(seat->presses.size() == 1);
    const std::uint64_t press = seat->presses.back();
    REQUIRE(press != 0);
    seat_does([press](loom::Mail& m) {
        (void)m.as_role(kCarrySeatOffice)
            .send_to_role(kWorkshopProvider,
                          PaneOperationRequested{"carry", kWorkshopProvider,
                                                 PaneValueCarryRequested::zen_name, 1,
                                                 static_cast<std::int64_t>(press)});
        (void)m.as_role(kCarrySeatOffice)
            .send_to_role(kWorkshopProvider,
                          PaneValueCarryRequested{"carry", "a value", loom::Bytes{1, 2, 3}, true},
                          press);
    });
    REQUIRE(seat->operations.size() == 1);
    REQUIRE_MESSAGE(seat->operations.back().allowed, seat->operations.back().reason);
    REQUIRE(seat->carries.size() == 1);
    REQUIRE_MESSAGE(seat->carries.back().carried, seat->carries.back().reason);
    CHECK_FALSE(window_shows(window, overlay, "[value]"));
    // CARRYING PUTS THE DESK IN FRONT, so the carry's motion and its release land in Workshop's
    // own area, painting nothing over a body a pane draws itself.
    CHECK(d.directory->areas.at(area).order == d.panes_on_desk() + 1);
    CHECK_FALSE(paints_inside(painted_cells(*window.weave().find(area)), d.pane_body()));

    // THE HAND MOVES, in Workshop's own area now in front of the pane's: the value goes with it,
    // in the overlay.
    window.move(area, x + 4, y, n);
    CHECK(window_shows(window, overlay, "[value]"));
    CHECK(window_overlaid(window));

    // LET GO ON THE BAND, WHERE NO PANE STANDS: nothing takes it, and it is gone from the window.
    window.button(area, 1, screen_of(s).notice_y / gfx::kCellPx + 1, false, n);
    CHECK_FALSE(window_shows(window, overlay, "[value]"));
    CHECK_FALSE(window_overlaid(window));
    CHECK(d.directory->areas.at(area).order == 0);
    CHECK(d.refused.empty());
}

namespace {

/// WHAT A PANE THAT DRAWS IN A GRAPHICS WINDOW HEARD: each press there with the number it came
/// under, each word Workshop said about the keys, and what Workshop answered its continuations.
struct WindowSeatHeard {
    std::vector<std::pair<gfx::AreaButtonChanged, std::uint64_t>> buttons;
    std::vector<bool> keys;
    std::vector<PaneOperationAnswered> operations;
    std::vector<PaneCarryAnswered> carries;
    std::vector<PaneMenuAnswered> menus;
};

/// A PANE'S OFFICE THAT HEARS THE WINDOW'S POINTER IN ITS AREA, and continues a press there as a
/// case tells it, under the number the press came with.
class WindowSeat
    : public loom::WeaveBase<
          WindowSeat, DeskSeatState,
          loom::Accept<PaneCatalogRequested, gfx::AreaRoom, gfx::AreaButtonChanged,
                       gfx::AreaPointerMoved, gfx::AreaWheelTurned, PaneKeys, PaneOperationAnswered,
                       PaneCarryAnswered, PaneMenuAnswered, SeatDo>,
          loom::Emit<PaneOffered, PaneOperationRequested,
                     PaneValueCarryRequested, PaneCarryRequested, PanePassRequested,
                     PaneMenuRequested, gfx::Drawn>> {
public:
    explicit WindowSeat(WindowSeatHeard& heard) : heard_(&heard) {}
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const gfx::AreaRoom&, loom::Mail&) {}
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        heard_->buttons.emplace_back(b, mail.correlation());
    }
    void on(const gfx::AreaPointerMoved&, loom::Mail&) {}
    void on(const gfx::AreaWheelTurned&, loom::Mail&) {}
    void on(const PaneKeys& k, loom::Mail& mail) {
        if (mail.authored_from_role(kWorkshopProvider)) heard_->keys.push_back(k.here);
    }
    void on(const PaneOperationAnswered& a, loom::Mail&) { heard_->operations.push_back(a); }
    void on(const PaneCarryAnswered& a, loom::Mail&) { heard_->carries.push_back(a); }
    void on(const PaneMenuAnswered& a, loom::Mail&) { heard_->menus.push_back(a); }
    void on(const SeatDo&, loom::Mail& mail) {
        auto run = std::move(next);
        next = {};
        if (run) run(mail);
    }
    std::function<void(loom::Mail&)> next;

private:
    WindowSeatHeard* heard_;
};

constexpr const char* kWindowSeatOffice = "zengine.test.desk-window-seat";

/// THE DESK OF `WindowDesk`, the stand-in terminal window telling Workshop its two rooms, and a
/// pane beside Attention whose office hears the window's pointer and draws its row in its area, the
/// area shown there too.
struct PointerDesk {
    WindowDesk d{true};
    pw::Window& window = *d.window;
    WindowSeatHeard heard;
    WindowSeat* seat = nullptr;
    loom::WeaveId seat_id{};
    std::int64_t kind = kNoPaneKind;
    std::string area = pane_area(kWindowSeatOffice, "seat");

    PointerDesk() {
        const Session& s = d.r.session();
        const Screen sc = screen_of(s);
        window.room(kWorkshopProvider, kWorkshopProvider, sc.w / gfx::kCellPx,
                    sc.h / gfx::kCellPx);
        window.room(kWorkshopOverlayArea, kWorkshopProvider, sc.w / gfx::kCellPx,
                    sc.h / gfx::kCellPx);
        auto made = std::make_unique<WindowSeat>(heard);
        seat = made.get();
        loom::Grant grant;
        for (const auto& [name, version] : std::vector<std::pair<std::string, std::uint32_t>>{
                 {PaneOffered::zen_name, PaneOffered::zen_version},
                 {PaneOperationRequested::zen_name, PaneOperationRequested::zen_version},
                 {PaneValueCarryRequested::zen_name, PaneValueCarryRequested::zen_version},
                 {PaneCarryRequested::zen_name, PaneCarryRequested::zen_version},
                 {PanePassRequested::zen_name, PanePassRequested::zen_version},
                 {PaneMenuRequested::zen_name, PaneMenuRequested::zen_version}}) {
            grant.allow_to_role(name, version, kWorkshopProvider);
        }
        grant.allow_to_role(gfx::Drawn::zen_name, gfx::Drawn::zen_version, window.weave().office());
        seat_id = d.r.bus.register_weave(std::move(made), std::move(grant), kWindowSeatOffice);
        seat->zen_set_self(seat_id);
        does([](loom::Mail& m) {
            (void)m.as_role(kWindowSeatOffice)
                .send_to_role(kWorkshopProvider, PaneOffered{"seat", "Seat", "a pane in a window"});
        });
        d.r.pick(PaneRef{kWindowSeatOffice, "seat"});
        const RuntimePane* row = s.panes.runtime.find(kWindowSeatOffice, "seat");
        REQUIRE(row != nullptr);
        kind = row->kind;
        REQUIRE(d.directory->areas.count(area) == 1);
        const auto& placed = d.directory->areas.at(area);
        REQUIRE(placed.pointer);
        window.room(area, kWindowSeatOffice, placed.w / gfx::kCellPx, placed.h / gfx::kCellPx);
        window.at(area, placed.x, placed.y);
        // ITS ROW IN ITS OWN AREA, as a pane placed in a window draws: what the window says it shows.
        draw_row("[go] there");
        press_outside(d.r, kind);
    }
    /// The seat's one row drawn whole in its own area, `[go]` its part.
    void draw_row(const std::string& text) {
        const auto& placed = d.directory->areas.at(area);
        const RowsRoom room{placed.w, placed.h, gfx::TextMeasure{}};
        const RowsDrawing drawing =
            rows_drawing(std::vector<TextRow>{{text, gfx::role::kFill}},
                         std::vector<RowPart>{RowPart{"control:go", 0, 0, 4}}, room,
                         room.columns());
        const std::string office = window.weave().office();
        const std::string at = area;
        does([&drawing, office, at](loom::Mail& m) {
            (void)m.as_role(kWindowSeatOffice)
                .send_to_role(office, gfx::Drawn{at, true, drawing.sketch.nodes(), {},
                                                 drawing.sketch.records(), {}, false});
        });
    }
    void does(std::function<void(loom::Mail&)> what) {
        seat->next = std::move(what);
        (void)d.r.bus.send(seat_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        d.r.bus.drain_until_idle();
    }
    /// A press of `button` in the pane's area at its cell, the window telling Workshop where it
    /// landed; its number.
    std::uint64_t press(std::int64_t col, std::int64_t row, std::int64_t button = 1) {
        const std::uint64_t n = window.next_press();
        window.button(area, col, row, true, n, button, kWorkshopProvider);
        return n;
    }
    void release(std::int64_t col, std::int64_t row, std::uint64_t n, std::int64_t button = 1) {
        window.button(area, col, row, false, n, button, kWorkshopProvider);
    }
    /// THE FRAME OF PANE `of`, its room told where Workshop placed it: its area's name.
    std::string frame(std::int64_t of) {
        const std::string name = pane_frame_area(d.r.session().panes, of);
        REQUIRE(d.directory->areas.count(name) == 1);
        const gfx::AreaPlaced placed = d.directory->areas.at(name);
        const gw::WinArea* kept = window.weave().find(name);
        if (kept == nullptr || kept->w != placed.w || kept->h != placed.h) {
            window.room(name, kWorkshopProvider, placed.w / gfx::kCellPx,
                        placed.h / gfx::kCellPx);
        }
        window.at(name, placed.x, placed.y);
        return name;
    }
    /// A cell of Workshop's own area holding no pane, above the band.
    std::pair<std::int64_t, std::int64_t> room_cell() {
        const Session& s = d.r.session();
        const Screen sc = screen_of(s);
        for (std::int64_t r = sc.h / gfx::kCellPx - 1; r >= 0; --r) {
            for (std::int64_t c = 0; c < sc.w / gfx::kCellPx; ++c) {
                const std::int64_t x = c * gfx::kCellPx + gfx::kCellPx / 2;
                const std::int64_t y = r * gfx::kCellPx + gfx::kCellPx / 2;
                if (y >= sc.notice_y) continue;
                const PointedAt at = canvas_point_of(input::space::kPixels, x, y);
                if (!occupied_at(s.panes, s.setup.active, sc, at).occupied) return {c, r};
            }
        }
        FAIL("no free cell on the desk");
        return {0, 0};
    }
};

} // namespace

TEST_CASE("a press in a pane's area of the window is the pane's, and under the window's number "
          "Workshop tells it where the keys were, hands its press back, opens its menu and "
          "carries what it drags out or picks up") {
    PointerDesk p;
    const Session& s = p.d.r.session();

    // THE PRESS IS THE PANE'S, AND WORKSHOP, TOLD WHERE IT LANDED, GIVES IT THE KEYS AND SAYS SO;
    // keys moved elsewhere are said too, and the next press reads them.
    p.heard.keys.clear();
    const std::uint64_t first = p.press(1, 0);
    p.release(1, 0, first);
    REQUIRE(p.heard.buttons.size() == 2);
    CHECK(p.heard.buttons[0].second == first);
    CHECK(s.panes.selected == p.kind);
    CHECK(typing_pane(s) == p.kind);
    REQUIRE_FALSE(p.heard.keys.empty());
    CHECK(p.heard.keys.back());
    press_outside(p.d.r, p.kind);
    CHECK_FALSE(p.heard.keys.back());

    // A RIGHT PRESS THE PANE HANDS BACK opens Workshop's pane menu where it was made; a number
    // the window never gave this pane opens nothing.
    const std::uint64_t right = p.press(2, 1, 3);
    p.does([](loom::Mail& m) {
        (void)m.as_role(kWindowSeatOffice).send_to_role(kWorkshopProvider,
                                                        PanePassRequested{"seat"}, 999);
    });
    CHECK_FALSE(s.context.open);
    p.does([right](loom::Mail& m) {
        (void)m.as_role(kWindowSeatOffice).send_to_role(kWorkshopProvider,
                                                        PanePassRequested{"seat"}, right);
    });
    p.release(2, 1, right, 3);
    REQUIRE(s.context.open);
    CHECK(s.context.subject == context_subject::kPane);
    CHECK(s.context.pane == PaneRef{kWindowSeatOffice, "seat"});
    const auto& placed = p.d.directory->areas.at(p.area);
    CHECK(s.context.anchor_x == canvas_point_of(input::space::kPixels,
                                                placed.x + 2 * gfx::kCellPx + 6,
                                                placed.y + gfx::kCellPx + 6)
                                    .px.x);
    p.d.r.key(input::scan::kEscape);
    REQUIRE_FALSE(s.context.open);

    // ITS OWN MENU, ASKED UNDER A RIGHT PRESS'S NUMBER, IS ELIGIBLE: here no presenter holds the
    // office, and that is the refusal; under a stale number the refusal is lateness.
    const std::uint64_t menu = p.press(2, 1, 3);
    p.release(2, 1, menu, 3);
    const auto ask_menu = [&p](std::uint64_t n) {
        p.does([n](loom::Mail& m) {
            (void)pane_menu::Offer("seat", "it").row("seat.do", "Do").continuing(
                m, kWindowSeatOffice, n);
        });
    };
    ask_menu(menu);
    REQUIRE_FALSE(p.heard.menus.empty());
    CHECK(p.heard.menus.back().refusal.find("presenter") != std::string::npos);
    ask_menu(menu + 999);
    CHECK(p.heard.menus.back().refusal.rfind("late", 0) == 0);

    // A VALUE DRAGGED OUT UNDER THE PRESS: carried, the desk coming in front of the panes, so the
    // carry's motion and release land in Workshop's own area; let go on the band, it is dropped.
    const std::int64_t panes = p.d.panes_on_desk();
    const std::uint64_t drag = p.press(1, 0);
    p.does([drag](loom::Mail& m) {
        (void)m.as_role(kWindowSeatOffice)
            .send_to_role(kWorkshopProvider,
                          PaneOperationRequested{"seat", kWorkshopProvider,
                                                 PaneValueCarryRequested::zen_name, 1,
                                                 static_cast<std::int64_t>(drag)});
        (void)m.as_role(kWindowSeatOffice)
            .send_to_role(kWorkshopProvider,
                          PaneValueCarryRequested{"seat", "a value", loom::Bytes{1, 2, 3}, true},
                          drag);
    });
    REQUIRE_FALSE(p.heard.operations.empty());
    REQUIRE_MESSAGE(p.heard.operations.back().allowed, p.heard.operations.back().reason);
    REQUIRE_FALSE(p.heard.carries.empty());
    REQUIRE_MESSAGE(p.heard.carries.back().carried, p.heard.carries.back().reason);
    CHECK(p.d.directory->areas.at(kWorkshopProvider).order == panes + 1);
    const auto band = std::make_pair<std::int64_t, std::int64_t>(
        1, screen_of(s).notice_y / gfx::kCellPx + 1);
    p.window.move(kWorkshopProvider, band.first + 5, band.second);
    p.window.button(kWorkshopProvider, band.first, band.second, false, 0);
    CHECK(p.d.directory->areas.at(kWorkshopProvider).order == 0);

    // A PRESS LET GO AWAY FROM WHERE IT WAS MADE, BEFORE THE PANE ASKS TO CARRY UNDER IT, moved:
    // the carry it then asks is dropped where the hand let go, here on a pane that takes none.
    const std::uint64_t late = p.press(1, 0);
    p.release(8, 3, late);
    p.does([late](loom::Mail& m) {
        (void)m.as_role(kWindowSeatOffice)
            .send_to_role(kWorkshopProvider,
                          PaneOperationRequested{"seat", kWorkshopProvider,
                                                 PaneValueCarryRequested::zen_name, 1,
                                                 static_cast<std::int64_t>(late)});
        (void)m.as_role(kWindowSeatOffice)
            .send_to_role(kWorkshopProvider,
                          PaneValueCarryRequested{"seat", "a value", loom::Bytes{1, 2, 3}, true},
                          late);
    });
    REQUIRE_MESSAGE(p.heard.carries.back().carried, p.heard.carries.back().reason);
    CHECK(s.notice.rfind("Value was not placed", 0) == 0);
    CHECK(p.d.directory->areas.at(kWorkshopProvider).order == 0);

    // AN ITEM PICKED UP BY A CLICK, then a press on a place that takes none: the press is the
    // carry's, and says so; Escape lets it go.
    const std::uint64_t pick = p.press(1, 0);
    p.release(1, 0, pick);
    p.does([pick](loom::Mail& m) {
        (void)m.as_role(kWindowSeatOffice)
            .send_to_role(kWorkshopProvider,
                          PaneOperationRequested{"seat", kWorkshopProvider,
                                                 PaneCarryRequested::zen_name, 1,
                                                 static_cast<std::int64_t>(pick)});
        (void)m.as_role(kWindowSeatOffice)
            .send_to_role(kWorkshopProvider,
                          PaneCarryRequested{"seat", "an item", loom::Bytes{4, 5}}, pick);
    });
    REQUIRE_MESSAGE(p.heard.carries.back().carried, p.heard.carries.back().reason);
    CHECK(p.d.directory->areas.at(kWorkshopProvider).order == panes + 1);
    const auto [rc, rr] = p.room_cell();
    p.window.click(kWorkshopProvider, rc, rr);
    CHECK(s.notice.find("does not accept the carried item") != std::string::npos);
    p.d.r.key(input::scan::kEscape);
    CHECK(p.d.directory->areas.at(kWorkshopProvider).order == 0);
    CHECK(p.d.refused.empty());
}

TEST_CASE("Workshop's own area of the window answers each button as itself: a press on the room says "
          "so, a right press asks its menu about the room or a tab, re-asking it while one is open, "
          "a press on the gear anchors there, and a tab carried along the run reorders it") {
    PointerDesk p;
    const Session& s = p.d.r.session();
    const std::string desk = kWorkshopProvider;
    const auto [rc, rr] = p.room_cell();

    // THE BARE ROOM: nothing there, and nothing chosen.
    p.window.click(desk, rc, rr);
    CHECK(s.notice == "nothing there");
    CHECK(s.panes.selected == kNoPaneKind);
    // A MIDDLE PRESS IS DROPPED.
    p.window.click(desk, rc, rr, 2);
    CHECK_FALSE(s.context.open);
    CHECK(s.notice == "nothing there");
    // A RIGHT PRESS ON THE ROOM ASKS ABOUT THE ROOM; a left press outside the menu dismisses it,
    // and is spent on that.
    p.window.click(desk, rc, rr, 3);
    REQUIRE(s.context.open);
    CHECK(s.context.subject == context_subject::kRoot);
    const std::string laid = p.frame(pane_kind::kLayouts);
    const gfx::AreaPlaced laid_at = p.d.directory->areas.at(laid);
    const auto tab = p.window.cell_of(laid, "layout:0");
    REQUIRE(tab.has_value());
    // ...A FURTHER RIGHT PRESS, ON A TAB -- in Workshop's own area, in front while the menu is open
    // -- RE-ASKS IT ABOUT THE TAB.
    p.window.click(desk, laid_at.x / gfx::kCellPx + tab->first,
                   laid_at.y / gfx::kCellPx + tab->second, 3);
    REQUIRE(s.context.open);
    CHECK(s.context.subject == context_subject::kLayout);
    CHECK(s.context.layout == 0);
    const std::size_t active = s.setup.active_at;
    p.window.click(desk, rc, rr);
    CHECK_FALSE(s.context.open);
    CHECK(s.setup.active_at == active);
    CHECK(s.panes.selected == kNoPaneKind);

    // THE GEAR, PRESSED IN THE PANE'S FRAME, OPENS WORKSHOP'S MENU BESIDE THE HAND, at the press.
    const std::string k = std::to_string(p.d.kind);
    const std::string framed = p.frame(p.d.kind);
    const gfx::AreaPlaced framed_at = p.d.directory->areas.at(framed);
    const auto gear = p.window.cell_of(framed, "settings:" + k);
    REQUIRE(gear.has_value());
    p.window.click(framed, gear->first, gear->second);
    REQUIRE(s.context.open);
    CHECK(s.context.subject == context_subject::kSettings);
    CHECK(s.context.anchor_x ==
          canvas_point_of(input::space::kPixels, framed_at.x + gear->first * gfx::kCellPx + 6,
                          framed_at.y + gear->second * gfx::kCellPx + 6)
              .px.x);
    p.d.r.key(input::scan::kEscape);

    // A TAB HELD AND CARRIED ALONG THE RUN, IN LAYOUTS' FRAME, TAKES ITS LAYOUT THERE, until its
    // release.
    REQUIRE(p.window.click(p.frame(pane_kind::kLayouts), "layout:new"));
    const std::size_t made = s.setup.active_at;
    REQUIRE(made > 0);
    const std::string layouts = p.frame(pane_kind::kLayouts);
    const auto first = p.window.cell_of(layouts, "layout:0");
    const auto last = p.window.cell_of(layouts, "layout:" + std::to_string(made));
    REQUIRE((first.has_value() && last.has_value()));
    const std::uint64_t n = p.window.next_press();
    p.window.button(layouts, last->first, last->second, true, n);
    CHECK(s.tab_drag.active);
    p.window.move(layouts, first->first, first->second, n);
    CHECK(s.setup.active_at == 0);
    p.window.button(layouts, first->first, first->second, false, n);
    CHECK_FALSE(s.tab_drag.active);

    // A PANE THE WINDOW SHOWS IN ITS OWN AREA IS NEVER RELAYED A MOMENT: a right press over it
    // while Workshop's menu holds the desk in front re-asks the menu about it, and sends it
    // nothing.
    REQUIRE(p.window.click(p.frame(pane_kind::kLayouts), "layout:1"));
    REQUIRE(p.d.directory->areas.at(p.area).window == gfx::kTerminalWindow);
    p.window.click(desk, rc, rr, 3);
    REQUIRE(s.context.open);
    std::size_t relayed = 0;
    const loom::ObserverId tap = p.d.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.sender == p.d.r.workshop_id && e.kind == loom::EventKind::Delivered &&
            (e.schema_name == "PaneCanvasPointer" || e.schema_name == "PanePressed" ||
             e.schema_name == "PaneButton")) {
            ++relayed;
        }
    });
    const PixelRect canvas = p.d.pane_body();
    p.window.click(desk, canvas.x / gfx::kCellPx + 1, canvas.y / gfx::kCellPx + 1, 3);
    p.d.r.bus.remove_observer(tap);
    REQUIRE(s.context.open);
    CHECK(s.context.pane == PaneRef{apane::kAttentionPaneRole, apane::kAttentionPane});
    CHECK(relayed == 0);
    p.d.r.key(input::scan::kEscape);

    CHECK(p.d.refused.empty());
}

TEST_CASE("a press Workshop holds in its own area of the window ends when the window says it ended "
          "the hold, and not before: an area Workshop places while it holds is the window's to judge") {
    PointerDesk p;
    const Session& s = p.d.r.session();
    // A PRESS ON A LAYOUT'S TAB takes hold of the tab in Layouts' frame, Workshop's own area.
    const std::string tabs = p.frame(pane_kind::kLayouts);
    const auto at = p.window.cell_of(tabs, "layout:0");
    REQUIRE(at.has_value());
    const std::uint64_t n = p.window.next_press();
    p.window.button(tabs, at->first, at->second, true, n);
    p.d.r.bus.drain_until_idle();
    REQUIRE(s.tab_drag.active);

    // THE ATTENTION PANE CLOSED, Workshop places the desk's areas again while the press holds:
    // whether that ends the press is the window's word, not Workshop's.
    const RuntimePane* attention =
        s.panes.runtime.find(apane::kAttentionPaneRole, apane::kAttentionPane);
    REQUIRE(attention != nullptr);
    const std::int64_t closed = attention->kind;
    REQUIRE(s.panes.has(closed));
    p.d.r.pick(PaneRef{apane::kAttentionPaneRole, apane::kAttentionPane});
    REQUIRE_FALSE(s.panes.has(closed));
    CHECK(s.tab_drag.active);

    // ANOTHER BUTTON'S HOLD, OR A PANE'S, ENDS NOTHING OF THIS PRESS...
    p.window.hold_ended(tabs, 3);
    p.window.hold_ended(p.area, 1, kWorkshopProvider);
    CHECK(s.tab_drag.active);
    // ...AND THE WINDOW'S WORD THAT IT ENDED THIS ONE lets the tab go.
    p.window.hold_ended(tabs, 1);
    CHECK_FALSE(s.tab_drag.active);
    CHECK(p.d.refused.empty());
}

TEST_CASE("a weaver's press in a pane's area of the window ends a guest's guide mark standing "
          "there, and the press is the pane's") {
    PointerDesk p;
    auto made = std::make_unique<GuideGuest>();
    GuideGuest* guest = made.get();
    loom::Grant grant;
    grant.allow_to_role(GuideRequested::zen_name, GuideRequested::zen_version, kWorkshopProvider);
    const loom::WeaveId guest_id = p.d.r.bus.register_weave(std::move(made), std::move(grant));
    guest->zen_set_self(guest_id);
    p.d.r.host.guest_row = [guest_id](loom::WeaveId who) {
        scope::GuestRowFacts row;
        if (who == guest_id) {
            row.admitted = true;
            row.name = "tutor";
        }
        return row;
    };
    p.d.r.host.input_authority = [&bus = p.d.r.bus](loom::WeaveId who) {
        return bus.alive(who) ? loom::host_describe_only_authority(bus, who)
                              : loom::GrantAuthority{};
    };
    guest->next = [](loom::Mail& m) {
        (void)m.send_to_role(kWorkshopProvider,
                             GuideRequested{std::string(kWindowSeatOffice) + "/seat", "go",
                                            "press go"});
    };
    (void)p.d.r.bus.send(guest_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
    p.d.r.bus.drain_until_idle();
    REQUIRE_FALSE(guest->answers.empty());
    REQUIRE_MESSAGE(guest->answers.back().shown, guest->answers.back().said);
    REQUIRE(window_overlaid(p.window));

    const std::size_t answered = guest->answers.size();
    const std::uint64_t n = p.press(1, 0);
    p.release(1, 0, n);
    CHECK_FALSE(window_overlaid(p.window));
    CHECK(guest->answers.size() > answered);
    REQUIRE(p.heard.buttons.size() == 2);
    CHECK(p.heard.buttons[0].first.pressed);
}

// A PANE THAT DRAWS IN AN AREA OF ITS OWN IS READ FROM THE WINDOW: the words and parts the window
// says it shows there, not Workshop's own record of what the pane once sent it.
TEST_CASE("a pane drawing in an area of its own is read as the window says it shows there: its words, "
          "its parts with the point a press reaches them by, and a stamp that moves when it draws "
          "again") {
    PointerDesk p;
    auto made = std::make_unique<DeskAsker>();
    DeskAsker* asker = made.get();
    loom::Grant grant;
    grant.allow_to_role(PaneViewRequested::zen_name, PaneViewRequested::zen_version, kWorkshopProvider);
    const loom::WeaveId asker_id = p.d.r.bus.register_weave(std::move(made), std::move(grant));
    asker->zen_set_self(asker_id);
    const auto page = [&](PaneView& out) {
        asker->refusals.clear();
        const std::size_t before = asker->pages.size();
        asker->next = [](loom::Mail& m) {
            (void)m.send_to_role(kWorkshopProvider,
                                 PaneViewRequested{kWindowSeatOffice, "seat", 0, PaneStamp{}});
        };
        (void)p.d.r.bus.send(asker_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        p.d.r.bus.drain_until_idle();
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->pages.size() == before + 1);
        out = asker->pages.back();
        return std::string();
    };

    const int asked_before = p.window.weave().area_reads;
    PaneView seen;
    const std::string first = page(seen);
    REQUIRE_MESSAGE(first.empty(), first);
    CHECK(p.window.weave().area_reads > asked_before); // the window was asked
    REQUIRE_FALSE(seen.in_flight);
    REQUIRE(seen.words.size() == 1);
    CHECK(seen.words[0].text == "[go] there");
    REQUIRE(seen.parts.size() == 1);
    CHECK(seen.parts[0].name == "control:go");
    CHECK(seen.parts[0].space != input::space::kUnknown);
    CHECK(seen.stamp.fingerprint != 0);
    // THE PANE DRAWS OTHER WORDS IN ITS AREA, telling Workshop nothing: the window's words are read.
    p.draw_row("[go] elsewhere");
    PaneView again;
    REQUIRE(page(again).empty());
    REQUIRE(again.words.size() == 1);
    CHECK(again.words[0].text == "[go] elsewhere");
    CHECK(again.stamp.fingerprint != seen.stamp.fingerprint);
}

TEST_CASE("with pane titles hidden, a press the window gives a canvas pane's area, which brings the "
          "keys and so the pane's title, keeps the pane's room until the window says its release") {
    WindowDesk d(true);
    pw::Window& window = *d.window;
    const std::string area = kAttentionArea;
    press_outside(d.r, d.kind); // the keys are Workshop's...
    d.r.key(input::scan::kT);   // ...and the titles hidden
    d.r.text("t");
    REQUIRE_FALSE(d.r.session().pane_titles);
    const ExternalPane* pane = d.r.session().panes.external_pane(d.kind);
    REQUIRE(pane != nullptr);
    REQUIRE(pane->placed_h > 0);
    const ExternalPane kept = *pane;
    const auto& placed = d.directory->areas.at(area);
    window.room(area, apane::kAttentionPaneRole, placed.w / gfx::kCellPx, placed.h / gfx::kCellPx);
    window.at(area, placed.x, placed.y);

    const std::uint64_t n = window.next_press();
    window.button(area, 1, 1, true, n, 1, kWorkshopProvider);
    REQUIRE(typing_pane(d.r.session()) == d.kind);
    REQUIRE(external_title_rows(d.r.session().panes, d.kind, false) == 1);
    CHECK(pane->title_waits);
    CHECK(same_place(*pane, kept));
    window.button(area, 1, 1, false, n, 1, kWorkshopProvider);
    CHECK_FALSE(pane->title_waits);
    CHECK_FALSE(same_place(*pane, kept));

    // A MENU OPENED WHILE A PRESS IS HELD THERE puts the desk in front of the press; the room
    // the title row takes waits for the window's word that it ended the hold, then is granted.
    press_outside(d.r, d.kind);
    REQUIRE(external_title_rows(d.r.session().panes, d.kind, false) == 0);
    const ExternalPane before = *pane;
    const Screen sc = screen_of(d.r.session());
    window.room(kWorkshopProvider, kWorkshopProvider, sc.w / gfx::kCellPx, sc.h / gfx::kCellPx);
    const std::uint64_t m = window.next_press();
    window.button(area, 1, 1, true, m, 1, kWorkshopProvider);
    REQUIRE(pane->title_waits);
    REQUIRE(same_place(*pane, before));
    window.click(kWorkshopProvider, 0, sc.notice_y / gfx::kCellPx - 1, 3);
    REQUIRE(d.r.session().context.open);
    CHECK(pane->title_waits);
    CHECK(same_place(*pane, before));
    window.hold_ended(area, 1, kWorkshopProvider);
    CHECK_FALSE(pane->title_waits);
    CHECK_FALSE(same_place(*pane, before));
    // ...and closing the menu brings no wait back: that press holds nothing now, so the keys
    // leaving take the title row at once.
    d.r.key(input::scan::kEscape);
    REQUIRE_FALSE(d.r.session().context.open);
    CHECK_FALSE(pane->title_waits);
    const ExternalPane titled = *pane;
    press_outside(d.r, d.kind);
    CHECK_FALSE(pane->title_waits);
    CHECK_FALSE(same_place(*pane, titled));
    CHECK(d.refused.empty());
}
