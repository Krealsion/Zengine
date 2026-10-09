// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop desk suite -- the desk and the words on it, read by message: every pane on the
// desk in Workshop's own numbers, and each run of words where the medium draws it.

#include "doctest.h"
#include "workshop_support.hpp"
#include "timeline.hpp"
#include "surface/skin_sdl_plan.hpp"
#include "surface/skin_tui.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/screen_canvas.hpp"
#include "view-builder/picture.hpp"
#include "view/view.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
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
                             loom::Accept<SeatDo, DeskView, v2::DeskView, v2::PaneView, v3::PaneView,
                                          v4::PaneView, DeskRead, v2::PanePoint, PaneView, PanePoint,
                                          loom::Refused>,
                             loom::Emit<DeskViewRequested, v2::DeskViewRequested,
                                        v2::PaneViewRequested, v3::PaneViewRequested,
                                        v4::PaneViewRequested, DeskReadRequested,
                                        v2::PanePointRequested, PaneViewRequested,
                                        PanePointRequested, v3::PanePointRequested>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<DeskView> desks;
    std::vector<v2::DeskView> named_desks;
    std::vector<v2::PaneView> views;
    std::vector<v3::PaneView> named_views;
    std::vector<v4::PaneView> pages;
    std::vector<DeskRead> reads;
    std::vector<v2::PanePoint> points;
    std::vector<PaneView> first_views;
    std::vector<PanePoint> first_points;
    std::vector<std::string> refusals;
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(m);
    }
    void on(const DeskView& d, loom::Mail&) { desks.push_back(d); }
    void on(const v2::DeskView& d, loom::Mail&) { named_desks.push_back(d); }
    void on(const v2::PaneView& v, loom::Mail&) { views.push_back(v); }
    void on(const v3::PaneView& v, loom::Mail&) { named_views.push_back(v); }
    void on(const v4::PaneView& v, loom::Mail&) { pages.push_back(v); }
    void on(const DeskRead& d, loom::Mail&) { reads.push_back(d); }
    void on(const v2::PanePoint& p, loom::Mail&) { points.push_back(p); }
    void on(const PaneView& v, loom::Mail&) { first_views.push_back(v); }
    void on(const PanePoint& p, loom::Mail&) { first_points.push_back(p); }
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
        for (const auto& shape : {loom::schema_of<DeskViewRequested>(),
                                  loom::schema_of<v2::DeskViewRequested>(),
                                  loom::schema_of<v2::PaneViewRequested>(),
                                  loom::schema_of<v3::PaneViewRequested>(),
                                  loom::schema_of<v4::PaneViewRequested>(),
                                  loom::schema_of<DeskReadRequested>(),
                                  loom::schema_of<v2::PanePointRequested>(),
                                  loom::schema_of<PaneViewRequested>(),
                                  loom::schema_of<PanePointRequested>(),
                                  loom::schema_of<v3::PanePointRequested>()}) {
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

    /// The desk, asked through the bus as any participant asks it.
    DeskView desk() {
        asker->refusals.clear();
        const std::size_t before = asker->desks.size();
        ask([](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, DeskViewRequested{}); });
        REQUIRE(asker->refusals.empty());
        REQUIRE(asker->desks.size() == before + 1);
        return asker->desks.back();
    }

    /// The desk with the menu's named lines, as its second version says it.
    v2::DeskView named_desk() {
        asker->refusals.clear();
        const std::size_t before = asker->named_desks.size();
        ask([](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, v2::DeskViewRequested{}); });
        REQUIRE(asker->refusals.empty());
        REQUIRE(asker->named_desks.size() == before + 1);
        return asker->named_desks.back();
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

    /// One page of a pane's reading, or the refusal's reason.
    std::string page(const v4::PaneViewRequested& asked, v4::PaneView& out) {
        asker->refusals.clear();
        const std::size_t before = asker->pages.size();
        ask([&](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, asked); });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->pages.size() == before + 1);
        out = asker->pages.back();
        return std::string();
    }

    /// A pane's words and named parts, or the refusal's reason when Workshop will not say them.
    std::string parts(const std::string& provider, const std::string& pane, v3::PaneView& out) {
        asker->refusals.clear();
        const std::size_t before = asker->named_views.size();
        ask([&](loom::Mail& m) {
            (void)m.send_to_role(kWorkshopProvider, v3::PaneViewRequested{provider, pane});
        });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->named_views.size() == before + 1);
        out = asker->named_views.back();
        return std::string();
    }

    /// A pane's words, or the refusal's reason (and no words) when Workshop will not say them.
    std::string words(const std::string& provider, const std::string& pane, v2::PaneView& out) {
        asker->refusals.clear();
        const std::size_t before = asker->views.size();
        ask([&](loom::Mail& m) {
            (void)m.send_to_role(kWorkshopProvider, v2::PaneViewRequested{provider, pane});
        });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->views.size() == before + 1);
        out = asker->views.back();
        return std::string();
    }

    /// Where one character of one word is, or the refusal's reason.
    std::string point(const v2::PanePointRequested& asked, v2::PanePoint& out) {
        asker->refusals.clear();
        const std::size_t before = asker->points.size();
        ask([&](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, asked); });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->points.size() == before + 1);
        out = asker->points.back();
        return std::string();
    }

    /// ...and the first version's point for a row and a column, or the refusal's reason.
    std::string first_point(const PanePointRequested& asked, PanePoint& out) {
        asker->refusals.clear();
        const std::size_t before = asker->first_points.size();
        ask([&](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, asked); });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->first_points.size() == before + 1);
        out = asker->first_points.back();
        return std::string();
    }

    /// ...and the third version's point for a cell of a pane's text lattice, or the refusal's reason.
    std::string lattice_point(const v3::PanePointRequested& asked, PanePoint& out) {
        asker->refusals.clear();
        const std::size_t before = asker->first_points.size();
        ask([&](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, asked); });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->first_points.size() == before + 1);
        out = asker->first_points.back();
        return std::string();
    }

    /// A primary click where a word says to press, in the space it says.
    void click(std::int64_t x, std::int64_t y, std::int64_t space) {
        for (const bool down : {true, false}) {
            r.publish(loom::to_value(input::PointerButton{1, down, x, y, space, input::mod::kNone}));
        }
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
    REQUIRE(place_at_canvas(d.r.session(), PaneRef{kAlphaOffice, "alpha"}, sc.w + 240, sc.room_y + 24)
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
    surface::SurfaceLayer layer;
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

/// A PROVIDER THAT DRAWS A PICTURE, and records every pointer event Workshop hands it.
class SketchSeat : public loom::WeaveBase<SketchSeat, SeatState,
    loom::Accept<PaneCatalogRequested, PaneRoom, PaneCanvasRoom, PaneCanvasPointer,
                 PaneCanvasHover, PaneCanvasRejected, SeatDo>,
    loom::Emit<v3::PaneOffered, PaneCanvasContent, v4::PaneCanvasContent,
               v5::PaneCanvasContent>> {
public:
    std::vector<PaneCanvasRoom> rooms;
    std::vector<PaneRoom> prose_rooms;
    std::vector<PaneCanvasPointer> pointers;
    std::vector<PaneCanvasRejected> rejected;
    std::function<void(SketchSeat&, loom::Mail&)> next;
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const PaneRoom& r, loom::Mail&) { prose_rooms.push_back(r); }
    void on(const PaneCanvasRoom& r, loom::Mail&) { rooms.push_back(r); }
    void on(const PaneCanvasPointer& e, loom::Mail&) { pointers.push_back(e); }
    void on(const PaneCanvasHover&, loom::Mail&) {}
    void on(const PaneCanvasRejected& r, loom::Mail&) { rejected.push_back(r); }
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(*this, m);
    }
};

/// What a canvas seat may say: its offer and its pictures, named or not.
loom::Grant sketch_grant() {
    loom::Grant grant;
    grant.allow_to_any(v3::PaneOffered::zen_name, v3::PaneOffered::zen_version);
    grant.allow_to_any(PaneCanvasContent::zen_name, PaneCanvasContent::zen_version);
    grant.allow_to_any(v4::PaneCanvasContent::zen_name, v4::PaneCanvasContent::zen_version);
    grant.allow_to_any(v5::PaneCanvasContent::zen_name, v5::PaneCanvasContent::zen_version);
    return grant;
}

/// THE DESK RIG WITH A CANVAS PANE BESIDE ITS TWO TEXT PANES, drawing two labels and two runs of
/// measured text -- one with a caret -- and a picture number of its own.
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
    /// The picture for the room last granted, numbered next.
    PaneCanvasContent picture() {
        REQUIRE_FALSE(sketch->rooms.empty());
        const PaneCanvasRoom room = sketch->rooms.back();
        const auto line = room.text_line_px > 0 ? room.text_line_px : kPaneCanvasUnit;
        PaneCanvasContent p;
        p.pane = kCanvasPane;
        p.grant = room.grant;
        p.picture = ++number;
        p.rects.push_back(PaneCanvasRect{0, 0, 10 * kPaneCanvasUnit, kPaneCanvasUnit,
                                         surface::role::kMuted});
        p.labels.push_back(PaneCanvasLabel{kPaneCanvasUnit, 0, "node one", surface::role::kFill});
        p.labels.push_back(PaneCanvasLabel{2 * kPaneCanvasUnit, 2 * kPaneCanvasUnit, "[Label]",
                                           surface::role::kAccent});
        p.texts.push_back(PaneCanvasText{0, 4 * kPaneCanvasUnit, "measured words", surface::role::kFill});
        PaneCanvasText typed{0, 4 * kPaneCanvasUnit + 2 * line, "typed here", surface::role::kFill};
        typed.caret_col = 3;
        p.texts.push_back(typed);
        return p;
    }
    /// The picture, drawn for the room last granted.
    void draw() {
        const PaneCanvasContent p = picture();
        drive([p](SketchSeat&, loom::Mail& m) {
            (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, p);
        });
        REQUIRE(r.session().panes.external_pane(sketch_kind)->canvas.heard);
    }
    /// ...and the same picture naming `parts`.
    void draw_named(std::vector<PaneCanvasPart> parts) {
        const PaneCanvasContent p = picture();
        const v4::PaneCanvasContent named{p.pane, p.grant, p.picture, p.rects, p.labels, p.texts,
                                          std::move(parts)};
        drive([named](SketchSeat&, loom::Mail& m) {
            (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, named);
        });
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

/// THE TEXT PANES' ROWS, said again for the room the medium just granted.
void say_rows(DeskRig& d) {
    d.r.drive(d.alpha, [](ProviderSeat& s, loom::Mail& m) {
        s.say(m, PaneContent{"alpha", {surface::SurfaceTextRow{"first row", surface::role::kFill},
                                       surface::SurfaceTextRow{"press [Save] here", surface::role::kFill}}});
    });
    d.r.drive(d.beta, [](ProviderSeat& s, loom::Mail& m) {
        s.say(m, PaneContent{"beta", {surface::SurfaceTextRow{"beta row", surface::role::kFill}}});
    });
}

/// Where a canvas press lands, in the pane's own canvas pixels: the place, less the body's corner.
bool inside_locally(const PaneCanvasPointer& e, const DeskRect& place, const ExternalPane& pane) {
    const auto x = e.x + pane.canvas.x;
    const auto y = e.y + pane.canvas.y;
    return x >= place.x && x < place.x + place.w && y >= place.y && y < place.y + place.h;
}

} // namespace

TEST_CASE("a text pane's words are its rows, each with its place and size, and a press at a word's point or a character's lands on that row and column") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        say_rows(d);
        v2::PaneView view;
        REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
        CHECK_FALSE(view.canvas);
        // An unnumbered provider's picture is zero, as the first version says.
        CHECK(view.picture == d.r.session().panes.external_pane(d.alpha_kind)->stamp.aimed);
        REQUIRE(view.words.size() == 2);
        CHECK(view.words[0].text == "first row");
        CHECK(view.words[1].text == "press [Save] here");
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        const std::int64_t advance = window ? body.fit.advance_px : surface::kCanvasCellPx;
        const std::int64_t line = window ? body.fit.line_px : surface::kCanvasCellPx;
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
        v2::PanePoint at;
        REQUIRE(d.point(v2::PanePointRequested{kAlphaOffice, "alpha", view.picture, 1, save}, at).empty());
        CHECK(at.space == view.words[1].space);
        d.alpha->presses.clear();
        d.click(at.x, at.y, at.space);
        REQUIRE(d.alpha->presses.size() == 1);
        CHECK(d.alpha->presses[0].row == 1);
        CHECK(d.alpha->presses[0].column == save);

        // A PICTURE THE CALLER DID NOT READ, A WORD OR A COLUMN THERE IS NOT: refused, by name.
        CHECK(d.point(v2::PanePointRequested{kAlphaOffice, "alpha", view.picture + 1, 1, 0}, at)
                  .find("picture moved") != std::string::npos);
        CHECK(d.point(v2::PanePointRequested{kAlphaOffice, "alpha", view.picture, 2, 0}, at)
                  .find("outside") != std::string::npos);
        CHECK(d.point(v2::PanePointRequested{kAlphaOffice, "alpha", view.picture, 0, 9}, at)
                  .find("outside") != std::string::npos);
    }
}

TEST_CASE("a canvas pane's words are its labels and text runs, each where it is drawn, and a press at a word's point lands inside it in the pane's own canvas") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        v2::PaneView view;
        REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
        CHECK(view.canvas);
        REQUIRE(view.words.size() == 4);
        CHECK(view.words[0].text == "node one");
        CHECK(view.words[1].text == "[Label]");
        CHECK(view.words[2].text == "measured words");
        CHECK(view.words[3].text == "typed here");
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        // A LABEL IS ONE CANVAS CELL A BYTE, from its own corner in the body.
        CHECK(view.words[1].place.x == pane.canvas.x + 2 * kPaneCanvasUnit);
        CHECK(view.words[1].place.y == pane.canvas.y + 2 * kPaneCanvasUnit);
        CHECK(view.words[1].place.w == 7 * kPaneCanvasUnit);
        CHECK(view.words[1].place.h == kPaneCanvasUnit);
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
            const PaneCanvasPointer& press = d.sketch->pointers.front();
            CHECK(press.phase == canvas_pointer::kPress);
            CHECK(press.picture == view.picture);
            CHECK(inside_locally(press, w.place, pane));
        }
        // ONE CHARACTER OF A CANVAS WORD, by its column: inside that character's own cell.
        v2::PanePoint at;
        REQUIRE(d.point(v2::PanePointRequested{kCanvasOffice, kCanvasPane, view.picture, 1, 1}, at).empty());
        d.sketch->pointers.clear();
        d.click(at.x, at.y, at.space);
        REQUIRE_FALSE(d.sketch->pointers.empty());
        const DeskRect l = view.words[1].place;
        CHECK(inside_locally(d.sketch->pointers.front(),
                             DeskRect{l.x + kPaneCanvasUnit, l.y, kPaneCanvasUnit, l.h}, pane));
        // ...AND ONE AFTER THE CARET, which moves no character: `d`, the run's fifth byte, stands
        // in its fifth cell in a terminal and its fifth advance in a window.
        REQUIRE(d.point(v2::PanePointRequested{kCanvasOffice, kCanvasPane, view.picture, 3, 4}, at).empty());
        const DeskRect typed = view.words[3].place;
        if (window) {
            const auto advance = d.r.session().text_advance_px;
            CHECK(at.x == typed.x + 4 * advance + advance / 2);
        } else {
            CHECK(at.x == surface::cell_of_pixel(typed.x) + 4);
        }
        // THE FIRST VERSION STILL ANSWERS IN ROWS, AND STILL SAYS A PICTURE IS NOT ROWS.
        d.asker->refusals.clear();
        d.ask([](loom::Mail& m) {
            (void)m.send_to_role(kWorkshopProvider, PaneViewRequested{kCanvasOffice, kCanvasPane});
        });
        REQUIRE(d.asker->refusals.size() == 1);
        CHECK(d.asker->refusals[0].find("picture, not text rows") != std::string::npos);
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
        v2::PaneView before;
        REQUIRE(d.words(kCanvasOffice, kCanvasPane, before).empty());
        REQUIRE(before.words.size() == 4);
        const ExternalPane kept = *d.r.session().panes.external_pane(d.sketch_kind);
        // A PRESS HELD ON THE PANE takes the keys, and with them the title the pane wears while
        // it has them (WL-FOCUS-11); the room that title takes waits for the press.
        const PaneWord& pressed = before.words[2];
        const std::size_t prose_before = d.sketch->prose_rooms.size();
        d.r.publish(loom::to_value(input::PointerButton{1, true, pressed.x, pressed.y, pressed.space,
                                                        input::mod::kNone}));
        REQUIRE(keyboard_pane(d.r.session().panes) == d.sketch_kind);
        REQUIRE(external_title_rows(d.r.session().panes, d.sketch_kind, false) == 1);
        const ExternalPane& held = *d.r.session().panes.external_pane(d.sketch_kind);
        REQUIRE(held.canvas.grant == kept.canvas.grant);
        // ...and its rows with it: no prose room is granted while the press is held.
        CHECK(held.rows == kept.rows);
        CHECK(d.sketch->prose_rooms.size() == prose_before);
        // READ WHERE IT IS PAINTED, before the pane draws again, as it may never: the words stand
        // in the room the press kept, where the painter draws the picture.
        const auto read = [&](v2::PaneView& view) {
            const std::string refused = d.words(kCanvasOffice, kCanvasPane, view);
            REQUIRE_MESSAGE(refused.empty(), refused);
            REQUIRE(view.words.size() == 4);
            CHECK(view.words[1].place.x == kept.canvas.x + 2 * kPaneCanvasUnit);
            CHECK(view.words[1].place.y == kept.canvas.y + 2 * kPaneCanvasUnit);
        };
        const auto painted_at = [&](const ExternalPane::Canvas& room) {
            for (const surface::SurfaceLayer& layer : d.r.last_canvas().layers)
                for (const surface::SurfaceLabel& label : layer.labels)
                    if (label.text == "node one" && label.x == room.x + kPaneCanvasUnit &&
                        label.y == room.y)
                        return true;
            return false;
        };
        v2::PaneView unredrawn;
        read(unredrawn);
        CHECK(unredrawn.picture == before.picture);
        CHECK(painted_at(kept.canvas));
        // ...AND ONCE IT DRAWS AGAIN in the room it still holds, that picture read in it.
        d.draw();
        REQUIRE(held.canvas.grant == kept.canvas.grant);
        v2::PaneView during;
        read(during);
        CHECK(during.picture == d.number);
        CHECK(painted_at(kept.canvas));
        // ...under no title: the title waits for the press, over the row the picture still holds.
        const RuntimePane* row = d.r.session().panes.runtime.of_kind(d.sketch_kind);
        REQUIRE(row != nullptr);
        const std::string title = external_header(*row, true);
        const auto titled = [&] {
            for (const surface::SurfaceLayer& layer : d.r.last_canvas().layers)
                for (const surface::SurfaceTextRegion& region : layer.texts)
                    for (const surface::SurfaceTextRow& line : region.rows)
                        if (line.text.rfind(title, 0) == 0) return true;
            return false;
        };
        CHECK_FALSE(titled());
        // ...AND A POINT THERE: the release at it reaches the pane inside that word, in the room
        // the press was aimed at.
        v2::PanePoint at;
        const std::string unpointed =
            d.point(v2::PanePointRequested{kCanvasOffice, kCanvasPane, during.picture, 1, 1}, at);
        REQUIRE_MESSAGE(unpointed.empty(), unpointed);
        d.sketch->pointers.clear();
        d.r.publish(loom::to_value(input::PointerButton{1, false, at.x, at.y, at.space, input::mod::kNone}));
        REQUIRE_FALSE(d.sketch->pointers.empty());
        const PaneCanvasPointer& released = d.sketch->pointers.front();
        CHECK(released.phase == canvas_pointer::kRelease);
        CHECK(released.grant == kept.canvas.grant);
        const DeskRect l = during.words[1].place;
        CHECK(inside_locally(released, DeskRect{l.x + kPaneCanvasUnit, l.y, kPaneCanvasUnit, l.h},
                             kept));
        // THE PRESS ENDED: the room under the title is granted, and painted at once -- the picture
        // shown in that room, where the pointer answers now -- and the title is drawn once the pane
        // draws there.
        const ExternalPane& after = *d.r.session().panes.external_pane(d.sketch_kind);
        CHECK(after.canvas.grant != kept.canvas.grant);
        CHECK_FALSE(after.canvas.title_waits);
        CHECK(after.canvas.y > kept.canvas.y);
        CHECK(painted_at(after.canvas));
        // ...its rows granted with it, a title row fewer, in one prose room.
        CHECK(after.rows == kept.rows - 1);
        REQUIRE(d.sketch->prose_rooms.size() == prose_before + 1);
        CHECK(d.sketch->prose_rooms.back().rows == kept.rows - 1);
        d.draw();
        CHECK(titled());
    }
}

TEST_CASE("a held press keeps a canvas pane's room only from its title row: a pane moved while the press "
          "is held is granted its new room at once, and the press is lost") {
    SketchRig d;
    const auto author = [&](std::int64_t y, std::int64_t h) {
        for (auto& p : d.r.session().setup.active.panes) {
            if (p.ref.provider != kCanvasOffice || p.ref.pane != kCanvasPane) continue;
            p.place = {pane_unit::kPixels, 4 * surface::kCanvasCellPx, y * surface::kCanvasCellPx};
            p.width = {pane_unit::kPixels, 60 * surface::kCanvasCellPx};
            p.height = {pane_unit::kPixels, h * surface::kCanvasCellPx};
        }
        d.r.extent(149, 60); // a same-size extent reseats nothing: the desk re-seats every pane
        d.r.extent(150, 60);
    };
    author(20, 16);
    d.draw();
    v2::PaneView view;
    REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
    const ExternalPane before = *d.r.session().panes.external_pane(d.sketch_kind);
    // A SECONDARY PRESS HELD ON THE PANE: its hold is the canvas's, and the keys stay where they are.
    const PaneWord& w = view.words[2];
    d.sketch->pointers.clear();
    d.r.publish(loom::to_value(input::PointerButton{3, true, w.x, w.y, w.space, input::mod::kNone}));
    REQUIRE_FALSE(d.sketch->pointers.empty());
    REQUIRE(d.sketch->pointers.back().phase == canvas_pointer::kPress);
    // ...AND THE PANE'S TOP EDGE MOVED two rows down, the same width ending where it did: no title
    // row made that change, so the room is not kept for the press.
    author(22, 14);
    const ExternalPane& moved = *d.r.session().panes.external_pane(d.sketch_kind);
    CHECK(moved.canvas.grant != before.canvas.grant);
    CHECK_FALSE(moved.canvas.title_waits);
    CHECK(moved.canvas.y > before.canvas.y);
    CHECK(moved.canvas.y + moved.canvas.height == before.canvas.y + before.canvas.height);
    CHECK(d.sketch->pointers.back().phase == canvas_pointer::kLost);
}

TEST_CASE("a canvas pane whose title waits for a held press, moved while it waits, is granted its new room "
          "at once and the press is lost") {
    SketchRig d;
    press_outside(d.r, d.sketch_kind); // the keys are Workshop's...
    d.r.key(input::scan::kT);          // ...and the titles hidden
    d.r.text("t");
    REQUIRE_FALSE(d.r.session().pane_titles);
    const auto author = [&](std::int64_t y, std::int64_t h) {
        for (auto& p : d.r.session().setup.active.panes) {
            if (p.ref.provider != kCanvasOffice || p.ref.pane != kCanvasPane) continue;
            p.place = {pane_unit::kPixels, 4 * surface::kCanvasCellPx, y * surface::kCanvasCellPx};
            p.width = {pane_unit::kPixels, 60 * surface::kCanvasCellPx};
            p.height = {pane_unit::kPixels, h * surface::kCanvasCellPx};
        }
        d.r.extent(149, 60); // a same-size extent reseats nothing: the desk re-seats every pane
        d.r.extent(150, 60);
    };
    author(20, 16);
    d.draw();
    v2::PaneView view;
    REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
    const ExternalPane before = *d.r.session().panes.external_pane(d.sketch_kind);
    // A PRIMARY PRESS HELD ON THE PANE takes the keys, and the room their title row takes waits.
    const PaneWord& w = view.words[2];
    d.sketch->pointers.clear();
    d.r.publish(loom::to_value(input::PointerButton{1, true, w.x, w.y, w.space, input::mod::kNone}));
    REQUIRE(keyboard_pane(d.r.session().panes) == d.sketch_kind);
    REQUIRE(d.r.session().panes.external_pane(d.sketch_kind)->canvas.title_waits);
    REQUIRE(d.r.session().panes.external_pane(d.sketch_kind)->canvas.grant == before.canvas.grant);
    // ...AND THE PANE MOVED two rows down while its title waits: no room is kept for the press.
    author(22, 14);
    const ExternalPane& moved = *d.r.session().panes.external_pane(d.sketch_kind);
    CHECK(moved.canvas.grant != before.canvas.grant);
    CHECK_FALSE(moved.canvas.title_waits);
    CHECK(moved.canvas.y > before.canvas.y);
    CHECK(d.sketch->pointers.back().phase == canvas_pointer::kLost);
}

TEST_CASE("a pane moved one title row's height while a press is held, its title unchanged, is granted its "
          "new room at once and the press is lost, in a window and in a terminal, titled or not") {
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
                for (auto& p : d.r.session().setup.active.panes) {
                    if (p.ref.provider != kCanvasOffice || p.ref.pane != kCanvasPane) continue;
                    p.place = {pane_unit::kPixels, 4 * surface::kCanvasCellPx, y};
                    p.width = {pane_unit::kPixels, 60 * surface::kCanvasCellPx};
                    p.height = {pane_unit::kPixels, h};
                }
                // a same-size extent reseats nothing: the desk re-seats every pane
                if (window) {
                    d.r.extent_on_window(149, 60);
                    d.r.extent_on_window(150, 60);
                } else {
                    d.r.extent(149, 60);
                    d.r.extent(150, 60);
                }
            };
            const std::int64_t top = 20 * surface::kCanvasCellPx, tall = 16 * surface::kCanvasCellPx;
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
            v2::PaneView view;
            REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
            const ExternalPane before = *d.r.session().panes.external_pane(d.sketch_kind);
            // A SECONDARY PRESS HELD ON THE PANE: the keys, and so its title rows, stay as they are.
            const PaneWord& w = view.words[2];
            d.sketch->pointers.clear();
            d.r.publish(loom::to_value(input::PointerButton{3, true, w.x, w.y, w.space, input::mod::kNone}));
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
            CHECK(moved.canvas.grant != before.canvas.grant);
            CHECK_FALSE(moved.canvas.title_waits);
            CHECK(moved.canvas.y == before.canvas.y + (hidden ? -step : step));
            CHECK(moved.canvas.y + moved.canvas.height == before.canvas.y + before.canvas.height);
            CHECK(d.sketch->pointers.back().phase == canvas_pointer::kLost);
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
    v2::PaneView view;
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
        for (const surface::SurfaceLayer& layer : d.r.last_canvas().layers)
            for (const surface::SurfaceTextRegion& region : layer.texts)
                for (const surface::SurfaceTextRow& line : region.rows)
                    if (line.text.rfind(title, 0) == 0) return true;
        return false;
    };
    REQUIRE(titled());
    // A PRESS HELD ON IT, and a launch of another pane taking the keys -- and the title row --
    // away while it is held: the pane keeps the room the press was aimed at, untitled.
    const PaneWord& held_at = view.words[2];
    d.r.publish(loom::to_value(input::PointerButton{1, true, held_at.x, held_at.y, held_at.space,
                                                    input::mod::kNone}));
    (void)hand_launch(d.r, PaneRef{kAlphaOffice, "alpha"});
    REQUIRE(keyboard_pane(d.r.session().panes) == d.alpha_kind);
    const ExternalPane& now = *d.r.session().panes.external_pane(d.sketch_kind);
    CHECK(now.canvas.grant == titled_room.canvas.grant);
    CHECK(now.canvas.title_waits);
    CHECK_FALSE(titled());
    // ...AND A LAUNCH OF THE PANE ITSELF giving them back, still held: its room and its title
    // agree again, so it waits for nothing and its title is drawn.
    (void)hand_launch(d.r, PaneRef{kCanvasOffice, kCanvasPane});
    REQUIRE(keyboard_pane(d.r.session().panes) == d.sketch_kind);
    CHECK(now.canvas.grant == titled_room.canvas.grant);
    CHECK_FALSE(now.canvas.title_waits);
    CHECK(titled());
    d.r.publish(loom::to_value(input::PointerButton{1, false, held_at.x, held_at.y, held_at.space,
                                                    input::mod::kNone}));
    CHECK(d.r.session().panes.external_pane(d.sketch_kind)->canvas.grant == titled_room.canvas.grant);
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
    v2::PaneView view;
    REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
    const ExternalPane kept = *d.r.session().panes.external_pane(d.sketch_kind);
    // A PRESS HELD ON THE PANE gives it the keys, and the room their title row makes waits for it.
    const PaneWord& w = view.words[2];
    d.r.publish(loom::to_value(input::PointerButton{1, true, w.x, w.y, w.space, input::mod::kNone}));
    REQUIRE(keyboard_pane(d.r.session().panes) == d.sketch_kind);
    REQUIRE(d.r.session().panes.external_pane(d.sketch_kind)->canvas.title_waits);
    // ...AND A RIGHT PRESS ON THE ROOM BELOW IT, the first still held: the room's menu opens and
    // takes the hold, and in that repaint the wait is over -- no press holds the room.
    const ui::Rect pane_rect = cells_covered(external_pane_rect(d.r.session(), d.sketch_kind));
    d.r.publish(loom::to_value(input::PointerButton{3, true, pane_rect.x + 1,
                                                    pane_rect.y + pane_rect.h + 1 + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    REQUIRE(d.r.session().context.open);
    const ExternalPane& now = *d.r.session().panes.external_pane(d.sketch_kind);
    CHECK_FALSE(now.canvas.title_waits);
    CHECK(now.canvas.grant != kept.canvas.grant);
    CHECK(now.canvas.y > kept.canvas.y);
    CHECK(now.rows == kept.rows - 1);
    REQUIRE_FALSE(d.sketch->pointers.empty());
    CHECK(d.sketch->pointers.back().phase == canvas_pointer::kLost);
}

TEST_CASE("a pane's words are refused while a menu covers it or arranging is open, and the desk still answers") {
    DeskRig d;
    say_rows(d);
    v2::PaneView view;
    REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
    d.r.key(input::scan::kA);
    REQUIRE(d.r.session().context.open);
    CHECK(d.words(kAlphaOffice, "alpha", view).find("covered by an interaction") != std::string::npos);
    CHECK(d.desk().menu.open);
    d.r.key(input::scan::kEscape);
    d.r.key(input::scan::kW);
    REQUIRE(d.r.session().arrange.open);
    CHECK(d.words(kAlphaOffice, "alpha", view).find("covered by an interaction") != std::string::npos);
    CHECK(d.desk().arranging);
    // A PANE NOBODY OFFERED is no pane at all.
    CHECK(d.words("zengine.test.nobody", "none", view).find("unknown") != std::string::npos);
}

namespace {

/// THE TERMINAL'S OWN PICTURE OF A PLACE: the glyphs on the cells a canvas place covers, as the
/// terminal rasterizer draws the canvas (`rasterize_canvas`).
std::string terminal_cells(const surface::CanvasGrids& g, const DeskRect& p, bool& one_row) {
    one_row = p.h == surface::kCanvasCellPx;
    const std::int64_t y = surface::cell_of_pixel(p.y);
    const std::int64_t x0 = surface::cell_of_pixel(p.x);
    const std::int64_t x1 = surface::cell_of_pixel(p.x + p.w);
    std::string out;
    for (std::int64_t x = x0; x < x1; ++x) {
        if (x < 0 || y < 0 || x >= g.w || y >= g.h) return "(off the terminal)";
        out += g.glyphs[static_cast<std::size_t>(y * g.w + x)];
    }
    return out;
}

/// THE WINDOW'S OWN PLAN OF A PLACE (`plan_canvas`, which the SDL edge executes): a row of a
/// region set in type whose glyphs start at the place's corner and spell the word, one advance a
/// byte and one line tall -- or, for text the window draws as bitmap cells, a glyph stroke inside
/// every non-blank character's cell.
bool window_draws(const std::vector<surface::PlanLayer>& plan, const PaneWord& w,
                  std::int64_t advance) {
    for (const surface::PlanLayer& layer : plan) {
        for (const surface::PlanTextRegion& region : layer.regions) {
            const std::int64_t x0 = region.view.x + region.origin_x;
            const std::int64_t y0 = region.view.y + region.origin_y;
            if (w.place.h != region.line_px || (w.place.y - y0) % region.line_px != 0 ||
                (w.place.x - x0) % advance != 0 ||
                w.place.w != static_cast<std::int64_t>(w.text.size()) * advance) {
                continue;
            }
            const std::int64_t row = (w.place.y - y0) / region.line_px;
            const std::int64_t column = (w.place.x - x0) / advance;
            if (row < 0 || row >= static_cast<std::int64_t>(region.rows.size()) || column < 0) continue;
            const std::string& text = region.rows[static_cast<std::size_t>(row)].text;
            if (text.compare(static_cast<std::size_t>(column), w.text.size(), w.text) == 0) return true;
        }
    }
    if (w.place.h != surface::kCanvasCellPx ||
        w.place.w < static_cast<std::int64_t>(w.text.size()) * surface::kCanvasCellPx) {
        return false;
    }
    for (std::size_t i = 0; i < w.text.size(); ++i) {
        if (w.text[i] == ' ') continue;
        const std::int64_t cx = w.place.x + static_cast<std::int64_t>(i) * surface::kCanvasCellPx;
        bool stroke = false;
        for (const surface::PlanLayer& layer : plan) {
            for (const surface::PlanRect& q : layer.quads) {
                stroke = stroke || (q.w < surface::kCanvasCellPx && q.x >= cx &&
                                    q.x + q.w <= cx + surface::kCanvasCellPx && q.y >= w.place.y &&
                                    q.y + q.h <= w.place.y + surface::kCanvasCellPx);
            }
        }
        if (!stroke) return false;
    }
    return true;
}

/// A pane's frame where the medium draws it: one rectangle of the canvas exactly the place, in
/// the chrome's voice, which the window plans as one quad and the terminal rasterizes to the
/// place's cells, floored.
bool frame_drawn(const surface::SurfaceCanvas& c, const DeskRect& p, bool window,
                 const std::vector<surface::PlanLayer>& plan) {
    for (const surface::SurfaceLayer& layer : c.layers) {
        for (const surface::SurfaceRect& r : layer.rects) {
            if (r.x != p.x || r.y != p.y || r.w != p.w || r.h != p.h ||
                (r.role != kPaneChrome && r.role != kPaneChromeSelected)) {
                continue;
            }
            if (window) {
                for (const surface::PlanLayer& l : plan) {
                    for (const surface::PlanRect& q : l.quads) {
                        if (q.x == p.x && q.y == p.y && q.w == p.w && q.h == p.h) return true;
                    }
                }
                return false;
            }
            // The terminal: the cells its rasterizer gives that rectangle, alone on the canvas
            // (the pane's own text covers it in the whole picture), are the place's, floored.
            surface::SurfaceCanvas alone{c.width, c.height, {}};
            alone.layers.push_back(surface::SurfaceLayer{});
            alone.layers.back().rects.push_back(r);
            const surface::CanvasGrids cells = surface::rasterize_canvas(alone);
            std::int64_t x0 = cells.w, y0 = cells.h, x1 = -1, y1 = -1;
            for (std::int64_t y = 0; y < cells.h; ++y) {
                for (std::int64_t x = 0; x < cells.w; ++x) {
                    if (cells.roles[static_cast<std::size_t>(y * cells.w + x)] < 0) continue;
                    x0 = (std::min)(x0, x); y0 = (std::min)(y0, y);
                    x1 = (std::max)(x1, x); y1 = (std::max)(y1, y);
                }
            }
            return x0 == surface::cell_of_pixel(p.x) && y0 == surface::cell_of_pixel(p.y) &&
                   x1 + 1 == surface::cell_of_pixel(p.x + p.w) &&
                   y1 + 1 == surface::cell_of_pixel(p.y + p.h);
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
            v2::PaneView view;
            REQUIRE(d.words(office, pane, view).empty());
            REQUIRE_FALSE(view.words.empty());
            words.insert(words.end(), view.words.begin(), view.words.end());
        }
        const DeskView desk = d.desk();

        // THE PICTURE THE MEDIUM WAS HANDED, and each medium's own reading of it.
        const surface::SurfaceCanvas& canvas = d.r.last_canvas();
        const surface::SurfaceExtent metric{canvas.width, canvas.height, s.text_advance_px,
                                            s.text_line_px, s.cell_px};
        const std::vector<surface::PlanLayer> plan =
            surface::plan_canvas(canvas, metric, surface::canvas_window_size(canvas));
        const surface::CanvasGrids grid = surface::rasterize_canvas(canvas);

        for (const PaneWord& w : words) {
            CAPTURE(w.text);
            if (window) {
                CHECK(window_draws(plan, w, s.text_advance_px));
            } else {
                bool one_row = false;
                CHECK(terminal_cells(grid, w.place, one_row) == w.text);
                CHECK(one_row);
                // ...and its point is one of those cells, on the console row the terminal reads.
                CHECK(w.x >= surface::cell_of_pixel(w.place.x));
                CHECK(w.x < surface::cell_of_pixel(w.place.x + w.place.w));
                CHECK(w.y - surface::kTuiCanvasTopRow == surface::cell_of_pixel(w.place.y));
            }
        }
        // EVERY OPEN PANE'S FRAME, at the place the desk gives.
        std::size_t framed = 0;
        for (const DeskPane& p : desk.panes) {
            if (p.state != "open") continue;
            CAPTURE(p.pane);
            CHECK(frame_drawn(canvas, p.visible, window, plan));
            ++framed;
        }
        CHECK(framed == 4);

        // AND A MENU'S LINES, where its popup draws them.
        d.r.key(input::scan::kA);
        REQUIRE(s.context.open);
        const DeskView with_menu = d.desk();
        REQUIRE(with_menu.menu.open);
        const surface::SurfaceCanvas& menu_canvas = d.r.last_canvas();
        const std::vector<surface::PlanLayer> menu_plan =
            surface::plan_canvas(menu_canvas, metric, surface::canvas_window_size(menu_canvas));
        const surface::CanvasGrids menu_grid = surface::rasterize_canvas(menu_canvas);
        REQUIRE_FALSE(with_menu.menu.lines.empty());
        for (const PaneWord& line : with_menu.menu.lines) {
            CAPTURE(line.text);
            if (window) {
                CHECK(window_draws(menu_plan, line, s.text_advance_px));
            } else {
                bool one_row = false;
                CHECK(terminal_cells(menu_grid, line.place, one_row) == line.text);
                CHECK(one_row);
            }
        }
        d.r.key(input::scan::kEscape);
    }
}

namespace {

/// THE CELL A TERMINAL SHOWS A GLYPH IN on one canvas cell row, from `x0` up to `x1`, read from
/// the terminal's own picture (`rasterize_canvas`): -1 where the glyph is not there exactly once.
std::int64_t terminal_cell_of(const surface::CanvasGrids& g, std::int64_t row, std::int64_t x0,
                              std::int64_t x1, char glyph) {
    std::int64_t found = -1;
    for (std::int64_t x = (std::max)(std::int64_t{0}, x0); x < x1 && x < g.w; ++x) {
        if (row < 0 || row >= g.h || g.glyphs[static_cast<std::size_t>(row * g.w + x)] != glyph) {
            continue;
        }
        if (found >= 0) return -1;
        found = x;
    }
    return found;
}

/// The glyph a terminal shows at a point a word or a character gives, in its own picture.
char terminal_glyph_at(const surface::CanvasGrids& g, std::int64_t x, std::int64_t console_y) {
    const std::int64_t y = console_y - surface::kTuiCanvasTopRow;
    if (x < 0 || y < 0 || x >= g.w || y >= g.h) return '\0';
    return g.glyphs[static_cast<std::size_t>(y * g.w + x)];
}

/// Where a body's row of glyphs ends, in canvas pixels: its columns from the first glyph's corner.
std::int64_t row_right_edge(const ExternalBodyPlace& body) {
    const surface::RegionFit& fit = body.fit;
    return fit.graphical() ? fit.view.x + fit.origin_x + body.columns * fit.advance_px
                           : fit.view.x + body.columns * surface::kCanvasCellPx;
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
            s.say(m, PaneContent{"alpha", {surface::SurfaceTextRow{text, surface::role::kFill}}});
            s.caret(m, PaneCaret{"alpha", 0, 2}); // before the `c`
        });
        for (std::int64_t column = 0; column < static_cast<std::int64_t>(text.size()); ++column) {
            CAPTURE(column);
            v2::PaneView view;
            REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
            REQUIRE(view.words.size() == 1);
            REQUIRE(view.words[0].text == text);
            v2::PanePoint at;
            REQUIRE(d.point(v2::PanePointRequested{kAlphaOffice, "alpha", view.picture, 0, column}, at)
                        .empty());
            if (window) {
                // A window draws the caret as a bar between two characters, which moves none.
                const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
                CHECK(at.x >= view.words[0].place.x + column * body.fit.advance_px);
                CHECK(at.x < view.words[0].place.x + (column + 1) * body.fit.advance_px);
            } else {
                // THE TERMINAL'S OWN PICTURE: the cell the point names shows that character.
                const surface::CanvasGrids grid = surface::rasterize_canvas(d.r.last_canvas());
                CHECK(terminal_glyph_at(grid, at.x, at.y) == text[static_cast<std::size_t>(column)]);
            }
            // ...the first version names the same cell for that row and column...
            PanePoint first;
            REQUIRE(d.first_point(PanePointRequested{kAlphaOffice, "alpha", view.picture, 0, column},
                                  first)
                        .empty());
            CHECK(first.x == at.x);
            CHECK(first.y == at.y);
            // ...and a press there reaches the pane as that column.
            d.alpha->presses.clear();
            d.click(at.x, at.y, at.space);
            REQUIRE(d.alpha->presses.size() == 1);
            CHECK(d.alpha->presses[0].row == 0);
            CHECK(d.alpha->presses[0].column == column);
        }
        if (!window) {
            // THE CARET STANDS ON THE CELL OF THE CHARACTER IT SITS BEFORE, inverted, and a press
            // there is that character's column, the caret's own.
            v2::PaneView view;
            REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
            const surface::CanvasGrids grid = surface::rasterize_canvas(d.r.last_canvas());
            const std::int64_t row = surface::cell_of_pixel(view.words[0].place.y);
            const std::int64_t x0 = surface::cell_of_pixel(view.words[0].place.x);
            const std::int64_t caret = terminal_cell_of(grid, row, x0, x0 + 6, 'c');
            REQUIRE(caret == x0 + 2);
            CHECK(grid.carets[static_cast<std::size_t>(row * grid.w + caret)] != 0);
            d.alpha->presses.clear();
            d.click(caret, row + surface::kTuiCanvasTopRow, input::space::kCells);
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
            s.say(m, PaneContent{"alpha", {surface::SurfaceTextRow{full, surface::role::kFill}}});
            s.caret(m, PaneCaret{"alpha", 0, at_end ? body.columns : 3});
        });
        v2::PaneView view;
        REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
        REQUIRE(view.words.size() == 1);
        const PaneWord& w = view.words[0];
        // A caret moves no character in either medium, so every character of the row shows; a
        // terminal shows the caret past the last one on the last cell, which still shows it.
        const std::int64_t shown = body.columns;
        CHECK(w.text == full);
        CHECK(w.place.x + w.place.w <= row_right_edge(body));
        if (!window) {
            const surface::CanvasGrids grid = surface::rasterize_canvas(d.r.last_canvas());
            bool one_row = false;
            CHECK(terminal_cells(grid, w.place, one_row) == w.text);
            CHECK(one_row);
            const std::int64_t row = surface::cell_of_pixel(w.place.y);
            const std::int64_t x0 = surface::cell_of_pixel(w.place.x);
            const std::int64_t caret_cell = at_end ? x0 + body.columns - 1 : x0 + 3;
            CHECK(grid.carets[static_cast<std::size_t>(row * grid.w + caret_cell)] != 0);
        }
        // THE LAST CHARACTER SHOWN HAS A POINT, AND A PRESS THERE IS ITS COLUMN...
        v2::PanePoint at;
        REQUIRE(d.point(v2::PanePointRequested{kAlphaOffice, "alpha", view.picture, 0, shown - 1}, at)
                    .empty());
        d.alpha->presses.clear();
        d.click(at.x, at.y, at.space);
        REQUIRE(d.alpha->presses.size() == 1);
        CHECK(d.alpha->presses[0].column == shown - 1);
        // ...AND A COLUMN PAST THE BODY HAS NONE, in either version.
        CHECK(d.point(v2::PanePointRequested{kAlphaOffice, "alpha", view.picture, 0, shown}, at)
                  .find("outside") != std::string::npos);
        PanePoint first;
        CHECK(d.first_point(PanePointRequested{kAlphaOffice, "alpha", view.picture, 0, shown}, first)
                  .find("outside") != std::string::npos);
    }
}

TEST_CASE("every cell of a canvas pane's text lattice has a point, a blank one and the one after a row's last character too, and a press there reaches the pane at that cell; a text pane's cell is the first version's") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        if (window) d.medium(true);
        // THE PANE'S ROWS ON ITS LATTICE: a row, a blank one, and a longer row.
        REQUIRE_FALSE(d.sketch->rooms.empty());
        const PaneCanvasRoom room = d.sketch->rooms.back();
        const CanvasRows lattice = canvas_rows(room);
        REQUIRE(lattice.rows > 3);
        const std::vector<surface::SurfaceTextRow> rows{{"abc", surface::role::kFill},
                                                        {"", surface::role::kFill},
                                                        {"defg", surface::role::kFill}};
        const v5::PaneCanvasContent drawn = rows_picture(room, ++d.number, rows);
        d.drive([drawn](SketchSeat&, loom::Mail& m) {
            (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, drawn);
        });
        v2::PaneView view;
        REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
        REQUIRE(view.picture == d.number);
        // ...THE CELL AFTER A ROW'S LAST CHARACTER, A BLANK ROW'S CELL AND A ROW'S LAST CELL: each a
        // point a press reaches the pane at, as that cell of its lattice.
        for (const auto& [row, column] : {std::pair<std::int64_t, std::int64_t>{0, 3}, {1, 0}, {2, 1},
                                          {0, lattice.columns - 1}}) {
            CAPTURE(row);
            CAPTURE(column);
            PanePoint at;
            REQUIRE(d.lattice_point(v3::PanePointRequested{kCanvasOffice, kCanvasPane, view.picture, row,
                                                           column},
                                    at)
                        .empty());
            CHECK(at.row == row);
            CHECK(at.column == column);
            d.sketch->pointers.clear();
            d.click(at.x, at.y, at.space);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            const PaneCanvasPointer& pressed = d.sketch->pointers.front();
            REQUIRE(pressed.phase == canvas_pointer::kPress);
            const RowCell cell = row_cell_at(lattice, pressed.x, pressed.y);
            CHECK(cell.shown);
            CHECK(cell.row == row);
            CHECK(cell.column == column);
        }
        // ...AND NONE PAST THE LATTICE, OR FOR A PICTURE SINCE MOVED.
        PanePoint none;
        CHECK(d.lattice_point(v3::PanePointRequested{kCanvasOffice, kCanvasPane, view.picture,
                                                     lattice.rows, 0},
                              none)
                  .find("outside the pane's text lattice") != std::string::npos);
        CHECK(d.lattice_point(v3::PanePointRequested{kCanvasOffice, kCanvasPane, view.picture, 0,
                                                     lattice.columns},
                              none)
                  .find("outside the pane's text lattice") != std::string::npos);
        CHECK(d.lattice_point(v3::PanePointRequested{kCanvasOffice, kCanvasPane, view.picture + 1, 0, 0},
                              none)
                  .find("picture moved") != std::string::npos);
        // A TEXT PANE'S CELL IS THE ONE THE FIRST VERSION NAMES.
        say_rows(d);
        REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
        for (std::int64_t column = 0; column < 4; ++column) {
            CAPTURE(column);
            PanePoint first, third;
            REQUIRE(d.first_point(PanePointRequested{kAlphaOffice, "alpha", view.picture, 1, column}, first)
                        .empty());
            REQUIRE(d.lattice_point(v3::PanePointRequested{kAlphaOffice, "alpha", view.picture, 1, column},
                                    third)
                        .empty());
            CHECK(third.x == first.x);
            CHECK(third.y == first.y);
            CHECK(third.space == first.space);
        }
    }
}

TEST_CASE("a word that is only a caret is pressed on the caret's own cell, inside its place, in a text row and in a canvas field at the body's right edge") {
    // A TEXT ROW WITH NOTHING ON IT BUT THE CARET: a terminal shows the row's first cell
    // inverted, and that cell is the word.
    {
        DeskRig d;
        d.r.drive(d.alpha, [](ProviderSeat& s, loom::Mail& m) {
            s.say(m, PaneContent{"alpha", {surface::SurfaceTextRow{"", surface::role::kFill},
                                           surface::SurfaceTextRow{"below", surface::role::kFill}}});
            s.caret(m, PaneCaret{"alpha", 0, 0});
        });
        v2::PaneView view;
        REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
        REQUIRE(view.words.size() == 2);
        const PaneWord& w = view.words[0];
        CHECK(w.text.empty());
        CHECK(w.place.w == surface::kCanvasCellPx);
        CHECK(w.x == surface::cell_of_pixel(w.place.x));
        CHECK(w.y - surface::kTuiCanvasTopRow == surface::cell_of_pixel(w.place.y));
        const surface::CanvasGrids grid = surface::rasterize_canvas(d.r.last_canvas());
        const std::int64_t at = (w.y - surface::kTuiCanvasTopRow) * grid.w + w.x;
        CHECK(grid.carets[static_cast<std::size_t>(at)] != 0);
        d.alpha->presses.clear();
        d.click(w.x, w.y, w.space);
        REQUIRE(d.alpha->presses.size() == 1);
        CHECK(d.alpha->presses[0].row == 0);
        CHECK(d.alpha->presses[0].column == 0);
    }
    // AN EMPTY FIELD IN A CANVAS PANE'S LAST CELL: the press must reach the provider, inside it.
    {
        SketchRig d;
        const PaneCanvasRoom room = d.sketch->rooms.back();
        PaneCanvasContent p;
        p.pane = kCanvasPane;
        p.grant = room.grant;
        p.picture = ++d.number;
        PaneCanvasText field{room.width - kPaneCanvasUnit, 2 * kPaneCanvasUnit, "", surface::role::kFill};
        field.caret_col = 0;
        p.texts.push_back(field);
        d.drive([p](SketchSeat&, loom::Mail& m) {
            (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, p);
        });
        v2::PaneView view;
        REQUIRE(d.words(kCanvasOffice, kCanvasPane, view).empty());
        REQUIRE(view.words.size() == 1);
        const PaneWord& w = view.words[0];
        CHECK(w.text.empty());
        CHECK(w.place.w == surface::kCanvasCellPx);
        CHECK(w.x == surface::cell_of_pixel(w.place.x));
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        d.sketch->pointers.clear();
        d.click(w.x, w.y, w.space);
        REQUIRE_FALSE(d.sketch->pointers.empty());
        CHECK(inside_locally(d.sketch->pointers.front(), w.place, pane));
    }
}

TEST_CASE("the first version's rows are read in a body one to three columns wide with a terminal's caret in it, each row's point inside the body") {
    std::size_t seen = 0;
    for (std::int64_t cells = 3; cells <= 12; ++cells) {
        CAPTURE(cells);
        DeskRig d;
        REQUIRE(author_pane_size(d.r.session().setup.active, PaneRef{kAlphaOffice, "alpha"},
                                 PaneSize{pane_unit::kPixels, cells * surface::kCanvasCellPx},
                                 PaneSize{pane_unit::kPixels, 8 * surface::kCanvasCellPx})
                    .accepted);
        // A new extent lays the desk out again, granting the pane the room its size gives.
        d.r.extent(151, 60);
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        if (!body.present || body.columns < 1 || body.columns > 3) continue;
        ++seen;
        CAPTURE(body.columns);
        // Rows as wide as the body, so the content fits the room it was granted.
        const auto width = static_cast<std::size_t>(body.columns);
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say(m, PaneContent{"alpha", {surface::SurfaceTextRow{std::string("abc").substr(0, width),
                                                                   surface::role::kFill},
                                           surface::SurfaceTextRow{std::string("def").substr(0, width),
                                                                   surface::role::kFill}}});
            s.caret(m, PaneCaret{"alpha", 0, 0});
        });
        const ExternalPane& held = *d.r.session().panes.external_pane(d.alpha_kind);
        REQUIRE(held.columns == body.columns);
        REQUIRE(held.heard);
        REQUIRE(held.caret_row == 0);
        d.asker->refusals.clear();
        d.ask([](loom::Mail& m) {
            (void)m.send_to_role(kWorkshopProvider, PaneViewRequested{kAlphaOffice, "alpha"});
        });
        const std::string refused = d.asker->refusals.empty() ? std::string() : d.asker->refusals.back();
        REQUIRE_MESSAGE(refused.empty(), refused);
        REQUIRE_FALSE(d.asker->first_views.empty());
        const PaneView& rows = d.asker->first_views.back();
        REQUIRE(rows.rows.size() == 2);
        for (const PaneViewRow& row : rows.rows) {
            CAPTURE(row.row);
            d.alpha->presses.clear();
            d.click(row.x, row.y, row.space);
            REQUIRE(d.alpha->presses.size() == 1);
            CHECK(d.alpha->presses[0].row == row.row);
        }
    }
    CHECK(seen > 0);
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
            s.say_named(m, v4::PaneContent{"alpha", {surface::SurfaceTextRow{full, surface::role::kFill}},
                                           0, 0, {PaneRowPart{"control:edge", 0, edge, 1}}});
            s.caret(m, PaneCaret{"alpha", 0, edge});
        });
        v3::PaneView view;
        REQUIRE(d.parts(kAlphaOffice, "alpha", view).empty());
        const PanePart* part = part_named(view.parts, "control:edge");
        REQUIRE(part != nullptr);
        CHECK(part->text == full.substr(static_cast<std::size_t>(edge)));
        REQUIRE(part->space == (window ? input::space::kPixels : input::space::kCells));
        if (!window) {
            const surface::CanvasGrids grid = surface::rasterize_canvas(d.r.last_canvas());
            const std::int64_t at = (part->y - surface::kTuiCanvasTopRow) * grid.w + part->x;
            CHECK(grid.carets[static_cast<std::size_t>(at)] != 0); // the caret's own cell
        }
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
            s.say_named(m, v4::PaneContent{"alpha",
                                           {surface::SurfaceTextRow{"first row", surface::role::kFill},
                                            surface::SurfaceTextRow{"press [Save] here", surface::role::kFill},
                                            surface::SurfaceTextRow{"", surface::role::kFill}},
                                           0, 0,
                                           {PaneRowPart{"row:first", 0, 0, body.columns},
                                            PaneRowPart{"control:save", 1, 6, 6},
                                            PaneRowPart{"field:empty", 2, 0, 4}}});
            s.caret(m, PaneCaret{"alpha", 1, 2});
        });
        v3::PaneView view;
        REQUIRE(d.parts(kAlphaOffice, "alpha", view).empty());
        REQUIRE(view.words.size() == 3);
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
        const std::int64_t advance = window ? body.fit.advance_px : surface::kCanvasCellPx;
        const std::int64_t line = window ? body.fit.line_px : surface::kCanvasCellPx;
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
        // ...AND THE SECOND VERSION STILL SAYS THE WORDS, naming nothing.
        v2::PaneView words;
        REQUIRE(d.words(kAlphaOffice, "alpha", words).empty());
        CHECK(words.words.size() == 3);
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
            s.say_named(m, v4::PaneContent{"alpha",
                                           {surface::SurfaceTextRow{"> [open] Files", surface::role::kFill},
                                            surface::SurfaceTextRow{"[one][two]", surface::role::kFill},
                                            surface::SurfaceTextRow{"[Go]", surface::role::kFill}},
                                           0, 0,
                                           {PaneRowPart{"pane:files", 0, 0, body.columns},
                                            PaneRowPart{"mark:files", 0, 2, 6},
                                            PaneRowPart{"row:both", 1, 0, 10},
                                            PaneRowPart{"control:one", 1, 0, 5},
                                            PaneRowPart{"control:two", 1, 5, 5},
                                            PaneRowPart{"row:go", 2, 0, body.columns},
                                            PaneRowPart{"control:go", 2, 0, 4}}});
        });
        v3::PaneView view;
        REQUIRE(d.parts(kAlphaOffice, "alpha", view).empty());
        REQUIRE(view.parts.size() == 7);
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
        // ...AND ONE WITH NO PLACE OF ITS OWN IS SAID WITH ITS WORDS AND PLACE AND NO POINT: no
        // press reaches it, and none is given another part's.
        const PanePart* both = part_named(view.parts, "row:both");
        REQUIRE(both != nullptr);
        CHECK(both->text == "[one][two]");
        CHECK(both->place.w > 0);
        CHECK(both->space == input::space::kUnknown);
        CHECK(both->x == 0);
        CHECK(both->y == 0);
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
    const std::vector<PaneRowPart> parts =
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
    PartNames<PaneCanvasPart> picture;
    CHECK(picture.add(PaneCanvasPart{"node", 0, 0, 12, 12}));
    CHECK_FALSE(picture.add(PaneCanvasPart{"node", 12, 0, 12, 12}));
    CHECK_FALSE(picture.add(PaneCanvasPart{"", 24, 0, 12, 12}));
    std::size_t named = 0;
    for (std::size_t i = 3; i <= kMaxPaneParts; ++i) {
        named += picture.add(PaneCanvasPart{"n" + std::to_string(i), 0, 0, 1, 1}) ? 1u : 0u;
    }
    CHECK(named == kMaxPaneParts - 2);
    const std::vector<PaneCanvasPart> kept = picture.take();
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
    const auto draw = [&](std::vector<std::string> rows, std::vector<PaneRowPart> parts) {
        v4::PaneContent c{"alpha", {}, 0, 0, std::move(parts)};
        for (std::string& text : rows) {
            c.rows.push_back(surface::SurfaceTextRow{std::move(text), surface::role::kFill});
        }
        d.r.drive(d.alpha, [c](ProviderSeat& s, loom::Mail& m) { s.say_named(m, c); });
    };
    draw({"alpha", "[Save]"},
         {PaneRowPart{"row:alpha", 0, 0, body.columns}, PaneRowPart{"control:save", 1, 0, 6}});
    v3::PaneView before;
    REQUIRE(d.parts(kAlphaOffice, "alpha", before).empty());
    const PanePart* was = part_named(before.parts, "control:save");
    REQUIRE(was != nullptr);
    // THE REDRAW: a row arrives above, and the control moves right along its own row.
    draw({"a notice", "alpha", "x [Save]"},
         {PaneRowPart{"row:alpha", 1, 0, body.columns}, PaneRowPart{"control:save", 2, 2, 6}});
    v3::PaneView after;
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

TEST_CASE("a pane's names are judged with the rows they name: a name twice, one on a row not said or past the room, or one that is not a name refuses the rows whole, saying why") {
    DeskRig d;
    const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
    const std::vector<surface::SurfaceTextRow> rows{{"one", surface::role::kFill},
                                                    {"two", surface::role::kFill}};
    const auto said = [&](std::vector<PaneRowPart> parts) {
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say_named(m, v4::PaneContent{"alpha", rows, 0, 0, parts});
        });
        return d.r.session().panes.external_pane(d.alpha_kind);
    };
    std::vector<PaneRowPart> too_many;
    for (std::size_t i = 0; i <= kMaxPaneParts; ++i) {
        too_many.push_back(PaneRowPart{"p" + std::to_string(i), 0, 0, 1});
    }
    const std::vector<std::pair<std::vector<PaneRowPart>, std::string>> bad{
        {{PaneRowPart{"row", 0, 0, 3}, PaneRowPart{"row", 1, 0, 3}}, "names two parts `row`"},
        {{PaneRowPart{"row", 2, 0, 3}}, "on a row its content does not say"},
        {{PaneRowPart{"row", 0, body.columns - 2, 3}}, "outside the room granted"},
        {{PaneRowPart{"row", 0, 0, 0}}, "outside the room granted"},
        {{PaneRowPart{"row", 0, -1, 2}}, "outside the room granted"},
        {{PaneRowPart{"", 0, 0, 0}}, "outside the room granted"},
        {{PaneRowPart{"   ", 0, 0, 3}}, "more than spaces"},
        {{PaneRowPart{"tab\there", 0, 0, 3}}, "printable ASCII"},
        {{PaneRowPart{std::string(kMaxPanePartNameLen + 1, 'n'), 0, 0, 3}}, "too long"},
        {too_many, "more than"},
    };
    for (const auto& [parts, why] : bad) {
        CAPTURE(why);
        const ExternalPane* pane = said(parts);
        CHECK_FALSE(pane->heard);
        CHECK(pane->shown.empty());
        CHECK(pane->parts.empty());
        CHECK(pane->refusal_why.find(why) != std::string::npos);
        // ...and named rightly, the rows and their names are admitted again.
        pane = said({PaneRowPart{"row", 0, 0, 3}});
        CHECK(pane->heard);
        CHECK(pane->parts.size() == 1);
        CHECK(pane->refusal_why.empty());
    }
    // A PLACE THE PANE NAMES NOTHING is admitted with the rows, a place and not a name: two of
    // them share their name with nothing.
    const ExternalPane* pane =
        said({PaneRowPart{"row", 0, 0, 3}, PaneRowPart{"", 0, 1, 1}, PaneRowPart{"", 1, 0, 2}});
    CHECK(pane->heard);
    CHECK(pane->refusal_why.empty());
    CHECK(pane->parts.size() == 3);
}

TEST_CASE("a canvas pane's named parts are said where its body shows them, with the words inside them, and a press at a part's point lands inside it in the pane's own canvas, in a window and in a terminal") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        const PaneCanvasRoom room = d.sketch->rooms.back();
        const std::int64_t u = kPaneCanvasUnit;
        // THE TYPED RUN'S OWN BOX, as a provider lays it out (`clip_canvas_text`).
        const PaneCanvasContent picture = d.picture();
        const CanvasTextLayout typed = clip_canvas_text(picture.texts[1], {0, 0, room.width, room.height}, room);
        REQUIRE(typed.visible());
        d.draw_named({PaneCanvasPart{"tool:label", 2 * u, 2 * u, 7 * u, u},
                      PaneCanvasPart{"node", 0, 0, 10 * u, u},
                      PaneCanvasPart{"field:typed", typed.bounds.x, typed.bounds.y, typed.bounds.w,
                                     typed.bounds.h},
                      PaneCanvasPart{"edge", room.width - u / 2, 6 * u, u, u},
                      PaneCanvasPart{"gone", room.width + u, 0, u, u}});
        REQUIRE(d.sketch->rejected.empty());
        v3::PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        CHECK(view.canvas);
        // A PART THE ROOM DOES NOT SHOW IS NOT SAID; one it cuts is said as far as it shows.
        CHECK(names_of(view.parts) == std::vector<std::string>{"edge", "field:typed", "node", "tool:label"});
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        const PanePart* label = part_named(view.parts, "tool:label");
        REQUIRE(label != nullptr);
        CHECK(label->text == "[Label]");
        CHECK(label->place.x == pane.canvas.x + 2 * u);
        CHECK(label->place.y == pane.canvas.y + 2 * u);
        CHECK(label->place.w == 7 * u);
        CHECK(label->place.h == u);
        CHECK(part_named(view.parts, "node")->text == "node one");
        CHECK(part_named(view.parts, "field:typed")->text == "typed here");
        const PanePart* edge = part_named(view.parts, "edge");
        REQUIRE(edge != nullptr);
        CHECK(edge->text.empty());
        CHECK(edge->place.w == u / 2);
        CHECK(edge->place.x + edge->place.w == pane.canvas.x + pane.canvas.width);
        for (const PanePart& p : view.parts) {
            CAPTURE(p.name);
            d.sketch->pointers.clear();
            d.click(p.x, p.y, p.space);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            CHECK(d.sketch->pointers.front().phase == canvas_pointer::kPress);
            CHECK(d.sketch->pointers.front().picture == view.picture);
            if (window) {
                CHECK(inside_locally(d.sketch->pointers.front(), p.place, pane));
            } else {
                // A terminal names a cell it paints the part on, by that cell's corner.
                const auto& e = d.sketch->pointers.front();
                const std::int64_t x = e.x + pane.canvas.x;
                const std::int64_t y = e.y + pane.canvas.y;
                CHECK(x + u > p.place.x);
                CHECK(x < p.place.x + p.place.w);
                CHECK(y + u > p.place.y);
                CHECK(y < p.place.y + p.place.h);
            }
        }
    }
}

TEST_CASE("with pane titles hidden, a refused picture's mark is Workshop's own: the pane view waits while "
          "it covers the picture, a press on it reaches no provider and a right press opens Workshop's "
          "menu, and the picture beside it takes a press as before, a press held or not") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        const std::int64_t u = kPaneCanvasUnit;
        // A DESK WITH TITLES HIDDEN and the sketch drawn, naming the part `node` over `node one`.
        const auto hidden = [&](SketchRig& d) {
            d.medium(window);
            press_outside(d.r, d.sketch_kind); // the keys are Workshop's...
            d.r.key(input::scan::kT);          // ...and the titles hidden
            d.r.text("t");
            REQUIRE_FALSE(d.r.session().pane_titles);
            REQUIRE(external_title_rows(d.r.session().panes, d.sketch_kind, false) == 0);
            d.draw_named({PaneCanvasPart{"node", 0, 0, 10 * u, u}});
        };
        const auto refuse = [](SketchRig& d) {
            PaneCanvasContent refused = d.picture();
            refused.labels[0].text = "caf\xC3\xA9";
            d.drive([refused](SketchSeat&, loom::Mail& m) {
                (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, refused);
            });
            REQUIRE(d.r.session().panes.external_pane(d.sketch_kind)->refusal ==
                    kExternalPictureRefused);
        };
        // WHERE THE MARK IS PAINTED: the band of the row that carries it, none when none does.
        const auto mark_band = [](SketchRig& d) {
            const Screen sc = screen_of(d.r.session());
            for (const surface::SurfaceLayer& layer : d.r.last_canvas().layers)
                for (const surface::SurfaceTextRegion& region : layer.texts) {
                    const surface::RegionFit fit = surface::fit_region(
                        region.x, region.y, region.w, region.h, sc.text_advance_px, sc.text_line_px);
                    const std::int64_t line = fit.graphical() ? fit.line_px : kPaneCanvasUnit;
                    const std::int64_t top = fit.graphical() ? region.y + fit.origin_y : region.y;
                    for (std::size_t i = 0; i < region.rows.size(); ++i)
                        if (region.rows[i].text.find(kExternalRefusedMark) != std::string::npos)
                            return DeskRect{region.x, top + static_cast<std::int64_t>(i) * line,
                                            region.w, line};
                }
            return DeskRect{};
        };
        const auto covers = [](const DeskRect& a, const DeskRect& b) {
            return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 && a.x < b.x + b.w && b.x < a.x + a.w &&
                   a.y < b.y + b.h && b.y < a.y + a.h;
        };
        const auto word_named = [](const v3::PaneView& view, const std::string& text) {
            PaneWord found;
            for (const PaneWord& w : view.words)
                if (w.text == text) found = w;
            return found;
        };
        // THE PANE VIEW WAITS while the mark covers its picture: no word, part or point is said.
        const auto unread = [](SketchRig& d, const PaneWord& word) {
            v3::PaneView view;
            CHECK_FALSE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
            v2::PaneView words;
            CHECK_FALSE(d.words(kCanvasOffice, kCanvasPane, words).empty());
            const std::int64_t picture = d.r.session().panes.external_pane(d.sketch_kind)->stamp.aimed;
            v2::PanePoint at;
            CHECK_FALSE(d.point(v2::PanePointRequested{kCanvasOffice, kCanvasPane, picture, word.word,
                                                       0}, at).empty());
            PanePoint cell;
            CHECK_FALSE(d.lattice_point(v3::PanePointRequested{kCanvasOffice, kCanvasPane, picture, 0,
                                                               0}, cell).empty());
        };

        // NO PRESS HELD: the mark stands at the picture's corner, over `node one`.
        SketchRig d;
        hidden(d);
        v3::PaneView before;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, before).empty());
        const PaneWord node_one = word_named(before, "node one");
        REQUIRE_FALSE(node_one.text.empty());
        refuse(d);
        const DeskRect band = mark_band(d);
        REQUIRE(band.w > 0);
        CHECK(covers(band, node_one.place));
        CHECK(d.r.session().panes.external_pane(d.sketch_kind)->canvas.heard);
        unread(d, node_one);
        // THE FOURTH READING SAYS THE PICTURE BESIDE THE MARK: the mark named as its cover, and
        // nothing under it said.
        v4::PaneView marked;
        REQUIRE(d.page(v4::PaneViewRequested{kCanvasOffice, kCanvasPane, 0, PaneStamp{}}, marked).empty());
        REQUIRE_FALSE(marked.in_flight);
        const std::vector<std::string>& marked_by = marked.covered.by;
        CHECK(std::find(marked_by.begin(), marked_by.end(), "refused mark") != marked_by.end());
        CHECK(marked.covered.words > 0);
        REQUIRE_FALSE(marked.words.empty());
        for (const PaneWord& w : marked.words) {
            CAPTURE(w.text);
            CHECK_FALSE(covers(band, w.place));
        }
        // A RIGHT PRESS ON THE MARK is Workshop's, as on a title row: its menu for the pane opens.
        d.sketch->pointers.clear();
        d.r.publish(loom::to_value(input::PointerButton{3, true, node_one.x, node_one.y,
                                                        node_one.space, input::mod::kNone}));
        CHECK(d.sketch->pointers.empty());
        CHECK(d.r.session().context.open);
        CHECK(d.r.session().context.pane == PaneRef{kCanvasOffice, kCanvasPane});
        d.r.key(input::scan::kEscape);
        REQUIRE_FALSE(d.r.session().context.open);
        // ...AND A PRESS ON IT reaches no provider.
        d.click(node_one.x, node_one.y, node_one.space);
        CHECK(d.sketch->pointers.empty());
        // ...AND A NEW ROOM AFTER THE REFUSAL, on the same medium, carries the picture before it as
        // a preview: a picture in flight, not an update kept none of.
        if (window) {
            d.r.extent_on_window(149, 60);
        } else {
            d.r.extent(149, 60);
        }
        REQUIRE(d.r.session().panes.external_pane(d.sketch_kind)->canvas.preview);
        v4::PaneView after_room;
        const std::string unawaited = d.page(v4::PaneViewRequested{kCanvasOffice, kCanvasPane, 0, PaneStamp{}},
                                             after_room);
        CHECK_MESSAGE(unawaited.empty(), unawaited);
        CHECK(after_room.in_flight);
        // ...UNTIL THE PICTURE THAT ROOM ASKED FOR IS REFUSED TOO: nothing more is coming, and the
        // refusal is said in words, the preview still painted.
        refuse(d);
        REQUIRE(d.r.session().panes.external_pane(d.sketch_kind)->canvas.preview);
        v4::PaneView answered;
        const std::string refused_too = d.page(v4::PaneViewRequested{kCanvasOffice, kCanvasPane, 0, PaneStamp{}},
                                               answered);
        CHECK_MESSAGE(refused_too.find("refused the pane's last update") != std::string::npos, refused_too);

        // THE PICTURE BESIDE THE MARK takes a press as before: it is still the one admitted.
        SketchRig beside;
        hidden(beside);
        v3::PaneView shown;
        REQUIRE(beside.parts(kCanvasOffice, kCanvasPane, shown).empty());
        const PaneWord measured = word_named(shown, "measured words");
        REQUIRE_FALSE(measured.text.empty());
        refuse(beside);
        REQUIRE_FALSE(covers(mark_band(beside), measured.place));
        beside.sketch->pointers.clear();
        beside.click(measured.x, measured.y, measured.space);
        REQUIRE_FALSE(beside.sketch->pointers.empty());
        CHECK(beside.sketch->pointers.front().phase == canvas_pointer::kPress);

        // A PRESS HELD on the picture keeps its room, its title waiting: a refusal arriving then
        // stands at the corner and the view waits, until the release gives the mark the title row.
        SketchRig held;
        hidden(held);
        v3::PaneView start;
        REQUIRE(held.parts(kCanvasOffice, kCanvasPane, start).empty());
        const PaneWord pressed = word_named(start, "measured words");
        REQUIRE_FALSE(pressed.text.empty());
        held.r.publish(loom::to_value(input::PointerButton{1, true, pressed.x, pressed.y,
                                                           pressed.space, input::mod::kNone}));
        REQUIRE(held.r.session().panes.external_pane(held.sketch_kind)->canvas.title_waits);
        refuse(held);
        REQUIRE(mark_band(held).w > 0);
        const PaneWord under = word_named(start, "node one");
        unread(held, under);
        // ...and a press on the mark meanwhile reaches no provider either.
        for (const bool down : {true, false}) {
            held.r.publish(loom::to_value(input::PointerButton{2, down, under.x, under.y, under.space,
                                                               input::mod::kNone}));
        }
        for (const PaneCanvasPointer& e : held.sketch->pointers) CHECK(e.button != 2);
        held.r.publish(loom::to_value(input::PointerButton{1, false, pressed.x, pressed.y,
                                                           pressed.space, input::mod::kNone}));
        const ExternalPane& released = *held.r.session().panes.external_pane(held.sketch_kind);
        CHECK_FALSE(released.canvas.title_waits);
        const DeskRect titled = mark_band(held);
        REQUIRE(titled.w > 0);
        CHECK(titled.y + titled.h <= released.canvas.y);
    }
}

TEST_CASE("a canvas part's point is its own: a part with another over its centre is pressed where nothing inside it is, and a part a terminal paints on no cell is not said there") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        const std::int64_t u = kPaneCanvasUnit;
        const std::vector<PaneCanvasPart> parts{PaneCanvasPart{"canvas", 0, 6 * u, 12 * u, 5 * u},
                                                PaneCanvasPart{"element:a", 4 * u, 7 * u, 4 * u, 3 * u},
                                                PaneCanvasPart{"handle:a", 7 * u, 9 * u, u, u},
                                                PaneCanvasPart{"tiny", u / 4, 0, u / 2, u / 2}};
        d.draw_named(parts);
        REQUIRE(d.sketch->rejected.empty());
        v3::PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        // A PART SMALLER THAN A CELL AND INSIDE ONE paints on no cell of a terminal, so it is not
        // said there; a window shows it.
        CHECK(names_of(view.parts) == (window ? std::vector<std::string>{"canvas", "element:a", "handle:a", "tiny"}
                                              : std::vector<std::string>{"canvas", "element:a", "handle:a"}));
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        const std::int64_t grain = window ? surface::kPixelGrainPx : surface::kCellGrainPx;
        const auto lands = [&](const PaneCanvasPointer& e, const PaneCanvasPart& part) {
            return surface::px_span_contains(pane.canvas.x + part.x, part.w, pane.canvas.x + e.x, grain) &&
                   surface::px_span_contains(pane.canvas.y + part.y, part.h, pane.canvas.y + e.y, grain);
        };
        // EACH PART'S POINT IS A PRESS ON IT, by the paint rule read backwards, and on no part
        // inside it: `canvas` beside `element:a`, `element:a` beside its handle.
        for (const PaneCanvasPart& part : parts) {
            const PanePart* said = part_named(view.parts, part.name);
            if (said == nullptr) continue;
            CAPTURE(part.name);
            d.sketch->pointers.clear();
            d.click(said->x, said->y, said->space);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            const PaneCanvasPointer& e = d.sketch->pointers.front();
            CHECK(lands(e, part));
            for (const PaneCanvasPart& inner : parts) {
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
const PaneCanvasPart* reached(const std::vector<PaneCanvasPart>& parts, const PaneCanvasPointer& e,
                              std::int64_t grain) {
    for (auto at = parts.rbegin(); at != parts.rend(); ++at) {
        if (surface::px_span_contains(at->x, at->w, e.x, grain) &&
            surface::px_span_contains(at->y, at->h, e.y, grain)) {
            return &*at;
        }
    }
    return nullptr;
}

/// A picture a shipped canvas pane drew, sent as the sketch pane's, for the room last granted.
void draw_as_sketch(SketchRig& d, v4::PaneCanvasContent named) {
    named.pane = kCanvasPane;
    d.drive([named](SketchSeat&, loom::Mail& m) {
        (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, named);
    });
}

} // namespace

TEST_CASE("a part under another is pressed where a press reaches it as its pane reads a press: the later of two parts takes the place they share, and so does a place the pane names nothing, in a window and in a terminal") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        const std::int64_t u = kPaneCanvasUnit;
        const std::int64_t grain = window ? surface::kPixelGrainPx : surface::kCellGrainPx;
        const auto every_point_reaches = [&](const std::vector<PaneCanvasPart>& parts,
                                             const std::vector<std::string>& said) {
            d.sketch->rejected.clear();
            d.draw_named(parts);
            CHECK(d.sketch->rejected.empty());
            if (!d.sketch->rejected.empty()) {
                CAPTURE(d.sketch->rejected.front().reason);
                return;
            }
            v3::PaneView view;
            REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
            CHECK(names_of(view.parts) == said);
            for (const PanePart& part : view.parts) {
                CAPTURE(part.name);
                d.sketch->pointers.clear();
                d.click(part.x, part.y, part.space);
                REQUIRE_FALSE(d.sketch->pointers.empty());
                const PaneCanvasPart* got = reached(parts, d.sketch->pointers.front(), grain);
                REQUIRE(got != nullptr);
                CHECK(got->name == part.name);
            }
        };
        // `second` IS LISTED AFTER `first` AND LIES OVER ITS CENTRE: a press there is `second`'s.
        every_point_reaches({PaneCanvasPart{"first", 2 * u, 2 * u, 8 * u, 2 * u},
                             PaneCanvasPart{"second", 5 * u, 2 * u, 8 * u, 2 * u}},
                            {"first", "second"});
        // A PLACE THE PANE NAMES NOTHING lies over `third`'s centre, is said nowhere, and is still
        // the place a press there reaches.
        every_point_reaches({PaneCanvasPart{"third", 2 * u, 6 * u, 10 * u, 2 * u},
                             PaneCanvasPart{"", 5 * u, 6 * u, 2 * u, 2 * u}},
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
        const view::Picture drawn =
            view::picture(v, {}, view::Presentation{}, d.sketch->rooms.back(), ++d.number);
        draw_as_sketch(d, view::named(drawn));
        REQUIRE(d.sketch->rejected.empty());
        v3::PaneView said;
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
        const vb::Picture drawn = vb::picture(m, shown, d.sketch->rooms.back(), ++d.number);
        draw_as_sketch(d, vb::named(drawn, m.description));
        REQUIRE(d.sketch->rejected.empty());
        v3::PaneView said;
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
        const std::int64_t grain = window ? surface::kPixelGrainPx : surface::kCellGrainPx;
        // `lined` has places it names nothing over its centre, top and bottom rows and its centre,
        // left and right columns, and its own places between them; `covered` lies under two parts;
        // `centred` has one over its left end, beside its centre.
        const std::vector<PaneCanvasPart> parts{PaneCanvasPart{"lined", 0, 0, 10 * u, 10 * u},
                                                PaneCanvasPart{"", 0, 0, 10 * u, u},
                                                PaneCanvasPart{"", 0, 9 * u, 10 * u, u},
                                                PaneCanvasPart{"", 0, 4 * u, 10 * u, 2 * u},
                                                PaneCanvasPart{"", 0, 0, u, 10 * u},
                                                PaneCanvasPart{"", 9 * u, 0, u, 10 * u},
                                                PaneCanvasPart{"", 4 * u, 0, 2 * u, 10 * u},
                                                PaneCanvasPart{"covered", 12 * u, 0, 4 * u, 2 * u},
                                                PaneCanvasPart{"cover:left", 12 * u, 0, 2 * u, 2 * u},
                                                PaneCanvasPart{"cover:right", 14 * u, 0, 2 * u, 2 * u},
                                                PaneCanvasPart{"centred", 18 * u, 0, 6 * u, 2 * u},
                                                PaneCanvasPart{"", 18 * u, 0, u, 2 * u}};
        d.draw_named(parts);
        REQUIRE(d.sketch->rejected.empty());
        v3::PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        CHECK(names_of(view.parts) ==
              std::vector<std::string>{"centred", "cover:left", "cover:right", "covered", "lined"});
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        // A UNIT OF THE BODY, as the medium's own numbers say it.
        const auto unit_at = [&](std::int64_t px, std::int64_t py) {
            return window ? std::pair{pane.canvas.x + px, pane.canvas.y + py}
                          : std::pair{surface::cell_of_pixel(pane.canvas.x) + px / u,
                                      surface::cell_of_pixel(pane.canvas.y) + py / u + surface::kTuiCanvasTopRow};
        };
        for (const PanePart& part : view.parts) {
            CAPTURE(part.name);
            if (part.name == "lined") {
                // THE MIDDLE OF ITS WIDEST STRETCH ON THE ROW NEAREST ITS CENTRE: the row above the
                // centre's strip, and the left of two stretches as wide.
                CHECK(std::pair{part.x, part.y} == (window ? unit_at(29, 47) : unit_at(2 * u, 3 * u)));
            }
            if (part.name == "centred") {
                // ITS CENTRE, ITS OWN, and not the middle of the stretch the place over its end leaves.
                CHECK(std::pair{part.x, part.y} == (window ? unit_at(251, 11) : unit_at(20 * u, 0)));
            }
            if (part.name == "covered") {
                // SAID WITH ITS PLACE AND NO POINT: no press reaches it, and none is given another's.
                CHECK(part.place.x == pane.canvas.x + 12 * u);
                CHECK(part.place.w == 4 * u);
                CHECK(part.space == input::space::kUnknown);
                CHECK(part.x == 0);
                CHECK(part.y == 0);
                continue;
            }
            d.sketch->pointers.clear();
            d.click(part.x, part.y, part.space);
            REQUIRE_FALSE(d.sketch->pointers.empty());
            const PaneCanvasPart* got = reached(parts, d.sketch->pointers.front(), grain);
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
        d.draw_named({PaneCanvasPart{"node", kPaneCanvasUnit, 0, 8 * kPaneCanvasUnit, kPaneCanvasUnit}});
        REQUIRE(d.sketch->rejected.empty());
        v3::PaneView held;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, held).empty());
        REQUIRE(held.parts.size() == 1);
        REQUIRE(held.parts[0].space != input::space::kUnknown);
        // THE OFFICE HELD BY NO ONE NOW, the picture standing until the desk grants its room afresh.
        d.r.host.role_holder = [](std::string_view) { return loom::WeaveId{}; };
        v3::PaneView now;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, now).empty());
        CHECK(now.picture == held.picture);
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
        // NEITHER POINT DOOR GIVES ONE...
        v2::PanePoint character;
        CHECK(d.point(v2::PanePointRequested{kCanvasOffice, kCanvasPane, now.picture, 0, 0}, character)
                  .find("takes no press") != std::string::npos);
        PanePoint cell;
        CHECK(d.lattice_point(v3::PanePointRequested{kCanvasOffice, kCanvasPane, now.picture, 0, 0}, cell)
                  .find("takes no press") != std::string::npos);
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
        const std::int64_t grain = window ? surface::kPixelGrainPx : surface::kCellGrainPx;
        const PaneCanvasRoom room = d.sketch->rooms.back();
        // SQUARES NESTED IN RUNS, EACH INSIDE THE LAST AND A LATER RUN OVER AN EARLIER, THEN STRIPS
        // AND BOXES STREWN OVER THEM, some past the room's edge and some smaller than a cell,
        // every one named.
        std::vector<PaneCanvasPart> parts;
        for (std::int64_t i = 0; parts.size() < kMaxPaneParts; ++i) {
            const std::string name = "p" + std::to_string(i);
            if (i < 680) {
                const std::int64_t inset = i % 68;
                parts.push_back(PaneCanvasPart{name, inset, inset, room.width - 2 * inset,
                                               room.height - 2 * inset});
            } else if (i % 2 == 0) {
                parts.push_back(PaneCanvasPart{name, (i * 37) % room.width - u, (i * 11) % room.height,
                                               (i * 13) % (3 * u) + 1, u / 4 + i % 5});
            } else {
                parts.push_back(PaneCanvasPart{name, (i * 53) % room.width, (i * 29) % room.height - u,
                                               u / 2 + (i * 7) % (2 * u), (i * 3) % (2 * u) + 1});
            }
        }
        d.draw_named(parts);
        REQUIRE(d.sketch->rejected.empty());
        v3::PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        // WHICH PART EACH UNIT OF THE BODY GIVES A PRESS TO, as every shipped canvas pane reads one.
        const std::int64_t across = room.width / grain, down = room.height / grain;
        std::vector<std::size_t> owner(static_cast<std::size_t>(across * down), parts.size());
        std::vector<char> owns(parts.size(), 0);
        for (std::int64_t y = 0; y < down; ++y) {
            for (std::int64_t x = 0; x < across; ++x) {
                for (std::size_t i = parts.size(); i-- > 0;) {
                    if (surface::px_span_contains(parts[i].x, parts[i].w, x * grain, grain) &&
                        surface::px_span_contains(parts[i].y, parts[i].h, y * grain, grain)) {
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
            const std::int64_t x = surface::floor_div_px(pressed.px.x - pane.canvas.x, grain);
            const std::int64_t y = surface::floor_div_px(pressed.px.y - pane.canvas.y, grain);
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
        const std::vector<PaneRowPart> parts =
            row_parts(map, body.columns, [](const std::string& meaning) { return meaning; });
        d.r.drive(d.alpha, [&](ProviderSeat& s, loom::Mail& m) {
            s.say_named(m, v4::PaneContent{"alpha",
                                           {surface::SurfaceTextRow{"0123456789abcdef", surface::role::kFill},
                                            surface::SurfaceTextRow{"one two three", surface::role::kFill}},
                                           0, 0, parts});
        });
        v3::PaneView view;
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

TEST_CASE("a picture's names are judged with it: a name twice or a part with no extent rejects the picture whole, and the last good one stays") {
    SketchRig d;
    const std::int64_t before = d.r.session().panes.external_pane(d.sketch_kind)->picture;
    for (const auto& [parts, why] :
         std::vector<std::pair<std::vector<PaneCanvasPart>, std::string>>{
             {{PaneCanvasPart{"node", 0, 0, 12, 12}, PaneCanvasPart{"node", 12, 0, 12, 12}},
              "names two parts `node`"},
             {{PaneCanvasPart{"node", 0, 0, 0, 12}}, "with no extent"},
             {{PaneCanvasPart{"", 0, 0, 12, 0}}, "with no extent"}}) {
        CAPTURE(why);
        d.sketch->rejected.clear();
        d.draw_named(parts);
        REQUIRE(d.sketch->rejected.size() == 1);
        CHECK(d.sketch->rejected[0].reason.find(why) != std::string::npos);
        CHECK(d.r.session().panes.external_pane(d.sketch_kind)->picture == before);
    }
    // A PLACE THE PANE NAMES NOTHING is admitted with the picture, two of them beside a name.
    d.sketch->rejected.clear();
    d.draw_named({PaneCanvasPart{"node", 0, 0, 12, 12}, PaneCanvasPart{"", 6, 0, 12, 12},
                  PaneCanvasPart{"", 0, 6, 12, 12}});
    CHECK(d.sketch->rejected.empty());
    CHECK(d.r.session().panes.external_pane(d.sketch_kind)->canvas.content.parts.size() == 3);
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
        const v2::DeskView desk = d.named_desk();
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
            s.say_named(m, v4::PaneContent{"alpha",
                                           {surface::SurfaceTextRow{"first row", surface::role::kFill},
                                            surface::SurfaceTextRow{"press [Save] here", surface::role::kFill}},
                                           0, 0,
                                           {PaneRowPart{"row:first", 0, 0, body.columns},
                                            PaneRowPart{"control:save", 1, 6, 6}}});
        });
        v3::PaneView rows;
        REQUIRE(d.parts(kAlphaOffice, "alpha", rows).empty());
        REQUIRE_FALSE(rows.canvas);
        REQUIRE(rows.parts.size() == 2);

        // THE PANE'S NEXT IMAGE DRAWS ON THE CANVAS: the office's holder goes, and one that takes a
        // picture's room offers the same pane again.
        loom::WeaveId holder{};
        for (std::size_t i = 0; i < d.r.seats_.size(); ++i) {
            if (d.r.seats_[i] == d.alpha) holder = d.r.seat_ids[i];
        }
        REQUIRE(holder.valid());
        auto gone = d.r.bus.unregister_weave(holder);
        REQUIRE(gone);
        auto made = std::make_unique<SketchSeat>();
        SketchSeat* next = made.get();
        const loom::WeaveId id = d.r.bus.register_weave(std::move(made), sketch_grant(),
                                                        std::string(kAlphaOffice));
        next->zen_set_self(id);
        const auto drive = [&](std::function<void(SketchSeat&, loom::Mail&)> f) {
            next->next = std::move(f);
            (void)d.r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
            d.r.bus.drain_until_idle();
        };
        drive([](SketchSeat&, loom::Mail& m) {
            (void)m.as_role(kAlphaOffice).send_to_role(kWorkshopProvider,
                v3::PaneOffered{"alpha", "Seat", "the same pane, drawn as a picture",
                                40 * kPaneCanvasUnit, 12 * kPaneCanvasUnit, 0});
        });
        REQUIRE_FALSE(next->rooms.empty());
        const PaneCanvasRoom room = next->rooms.back();
        const std::int64_t u = kPaneCanvasUnit;
        v4::PaneCanvasContent picture{"alpha", room.grant, 1, {}, {}, {}, {}};
        picture.labels = {PaneCanvasLabel{0, 0, "first row", surface::role::kFill},
                          PaneCanvasLabel{0, u, "press", surface::role::kFill},
                          PaneCanvasLabel{6 * u, u, "[Save]", surface::role::kAccent},
                          PaneCanvasLabel{13 * u, u, "here", surface::role::kFill}};
        picture.parts = {PaneCanvasPart{"row:first", 0, 0, room.width, u},
                         PaneCanvasPart{"control:save", 6 * u, u, 6 * u, u}};
        drive([picture](SketchSeat&, loom::Mail& m) {
            (void)m.as_role(kAlphaOffice).send_to_role(kWorkshopProvider, picture);
        });
        REQUIRE(next->rejected.empty());

        // EVERY NAME AN AGENT WROTE AGAINST THE ROWS FINDS ITS PART IN THE PICTURE.
        v3::PaneView drawn;
        REQUIRE(d.parts(kAlphaOffice, "alpha", drawn).empty());
        CHECK(drawn.canvas);
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
            const std::int64_t x = next->pointers.front().x + pane.canvas.x;
            const std::int64_t y = next->pointers.front().y + pane.canvas.y;
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
        const PaneCanvasRoom room = d.sketch->rooms.back();
        const CanvasTextMetrics m = canvas_text_metrics(room);
        v5::PaneCanvasContent p;
        p.pane = kCanvasPane;
        p.grant = room.grant;
        p.picture = ++d.number;
        p.rects.push_back(PaneCanvasRect{0, 0, room.width, room.height, surface::role::kGround});
        const std::vector<std::string> rows = {"> [open] Layouts", "  [    ] Loaded"};
        for (std::size_t r = 0; r < rows.size(); ++r) {
            v2::PaneCanvasText run{m.inset, static_cast<std::int64_t>(r) * m.line, rows[r],
                                   surface::role::kFill};
            run.padded = false;
            p.texts.push_back(run);
        }
        p.parts.push_back(PaneCanvasPart{"pane:a", m.inset, 0, 20 * m.advance, m.line});
        p.parts.push_back(PaneCanvasPart{"mark:a", m.inset + 2 * m.advance, 0, 6 * m.advance, m.line});
        p.parts.push_back(PaneCanvasPart{"pane:b", m.inset, m.line, 20 * m.advance, m.line});
        d.drive([p](SketchSeat&, loom::Mail& mail) {
            (void)mail.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, p);
        });
        REQUIRE(d.sketch->rejected.empty());
        v3::PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        CHECK(view.canvas);
        // ONE WORD A ROW, its text the row's, on the row its parts stand on.
        REQUIRE(view.words.size() == 2);
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        for (std::size_t r = 0; r < rows.size(); ++r) {
            CAPTURE(r);
            CHECK(view.words[r].text == rows[r]);
            CHECK(view.words[r].place.x == pane.canvas.x + m.inset);
            CHECK(view.words[r].place.y == pane.canvas.y + static_cast<std::int64_t>(r) * m.line);
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
            s.say_named(m, v4::PaneContent{"alpha",
                                           {surface::SurfaceTextRow{"first row", surface::role::kFill},
                                            surface::SurfaceTextRow{"press [Save] here", surface::role::kFill}},
                                           0, 0,
                                           {PaneRowPart{"row:first", 0, 0, body.columns},
                                            PaneRowPart{"control:save", 1, 6, 6},
                                            PaneRowPart{"word:here", 1, 13, 4}}});
            s.caret(m, PaneCaret{"alpha", 1, 2});
        });
        v3::PaneView view;
        REQUIRE(d.parts(kAlphaOffice, "alpha", view).empty());
        REQUIRE(view.parts.size() == 3);
        const Session& s = d.r.session();
        const surface::SurfaceCanvas& canvas = d.r.last_canvas();
        const surface::SurfaceExtent metric{canvas.width, canvas.height, s.text_advance_px,
                                            s.text_line_px, s.cell_px};
        const std::vector<surface::PlanLayer> plan =
            surface::plan_canvas(canvas, metric, surface::canvas_window_size(canvas));
        const surface::CanvasGrids grid = surface::rasterize_canvas(canvas);
        for (const PanePart& p : view.parts) {
            CAPTURE(p.name);
            REQUIRE_FALSE(p.text.empty());
            if (window) {
                PaneWord drawn;
                drawn.text = p.text;
                drawn.place = DeskRect{p.place.x, p.place.y,
                                       static_cast<std::int64_t>(p.text.size()) * s.text_advance_px,
                                       p.place.h};
                CHECK(window_draws(plan, drawn, s.text_advance_px));
            } else {
                bool one_row = false;
                std::string cells = terminal_cells(grid, p.place, one_row);
                while (!cells.empty() && cells.back() == ' ') cells.pop_back();
                CHECK(cells == p.text);
                CHECK(one_row);
                CHECK(terminal_glyph_at(grid, p.x, p.y) != '\0');
            }
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
    heard.name(d.asker_id, "asker");
    heard.name(d.r.painter_id, "painter");
    (void)d.desk();
    v2::PaneView view;
    REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
    v2::PanePoint at;
    REQUIRE(d.point(v2::PanePointRequested{kAlphaOffice, "alpha", view.picture, 1, 7}, at).empty());
    heard.stop();
    const std::string differs = kept_conversation(heard, "desk-words-point");
    CHECK_MESSAGE(differs.empty(), differs);
}

TEST_CASE("the conversation of a press on a word is kept") {
    DeskRig d;
    say_rows(d);
    v2::PaneView view;
    REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
    zengine::tests::Timeline heard(d.r.bus);
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
    heard.name(d.asker_id, "asker");
    heard.name(d.r.painter_id, "painter");
    REQUIRE(seat_pane_open(d.r, gamma, "zengine.test.desk-gamma", "gamma") != kNoPaneKind);
    heard.stop();
    const std::string differs = kept_conversation(heard, "open-a-pane");
    CHECK_MESSAGE(differs.empty(), differs);
}

namespace {

/// THE READING A DESK READ GAVE ONE PANE, or nullptr where it named the pane by its stamp alone.
const v4::PaneView* reading_of(const DeskRead& read, const std::string& provider,
                               const std::string& pane) {
    for (const v4::PaneView& v : read.panes) {
        if (v.provider == provider && v.pane == pane) return &v;
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
PaneStamp stamp_of_reading(const v4::PaneView& v) {
    return PaneStamp{v.provider, v.pane, v.holder, v.incarnation, v.grant, v.picture};
}

/// The status the rig's painter last heard Workshop publish.
std::string published_status(const PaneRig& r) {
    for (std::size_t i = r.notes.size(); i > 0; --i) {
        if (r.notes[i - 1].slot == surface::kSlotStatus) return r.notes[i - 1].text;
    }
    return std::string();
}

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
        const Session& s = d.r.session();
        DeskRead read;
        REQUIRE(d.read(read).empty());
        // EVERY PRESENTED PANE IS READ IN THE ONE ANSWER -- the two text panes, the canvas pane and
        // Layouts -- with nothing over any of them, so every word each holds is said.
        CHECK(read.stamps.size() == 4);
        REQUIRE(read.panes.size() == read.stamps.size());
        REQUIRE(reading_of(read, kCanvasOffice, kCanvasPane) != nullptr);
        REQUIRE(reading_of(read, kWorkshopProvider, pane_key::kLayouts) != nullptr);
        std::vector<PaneWord> words;
        for (const v4::PaneView& v : read.panes) {
            CAPTURE(v.pane);
            CHECK_FALSE(v.in_flight);
            CHECK(v.covered.by.empty());
            REQUIRE_FALSE(v.words.empty());
            words.insert(words.end(), v.words.begin(), v.words.end());
        }
        // ...AND THE BAND'S WORDS AFTER THEM, which have no point.
        const std::size_t pane_words = words.size();
        REQUIRE_FALSE(read.desk.words.empty());
        words.insert(words.end(), read.desk.words.begin(), read.desk.words.end());

        // THE PICTURE THE MEDIUM WAS HANDED, and each medium's own reading of it.
        const surface::SurfaceCanvas& canvas = d.r.last_canvas();
        const surface::SurfaceExtent metric{canvas.width, canvas.height, s.text_advance_px,
                                            s.text_line_px, s.cell_px};
        const std::vector<surface::PlanLayer> plan =
            surface::plan_canvas(canvas, metric, surface::canvas_window_size(canvas));
        const surface::CanvasGrids grid = surface::rasterize_canvas(canvas);
        for (std::size_t i = 0; i < words.size(); ++i) {
            const PaneWord& w = words[i];
            CAPTURE(w.text);
            if (window) {
                CHECK(window_draws(plan, w, s.text_advance_px));
                continue;
            }
            bool one_row = false;
            CHECK(terminal_cells(grid, w.place, one_row) == w.text);
            CHECK(one_row);
            if (i >= pane_words) continue;
            // ...and a pane word's point is one of those cells, on the console row the terminal reads.
            CHECK(w.x >= surface::cell_of_pixel(w.place.x));
            CHECK(w.x < surface::cell_of_pixel(w.place.x + w.place.w));
            CHECK(w.y - surface::kTuiCanvasTopRow == surface::cell_of_pixel(w.place.y));
        }

        // THE STATUS SLOT: the text the medium was handed for it, a word with no place.
        REQUIRE(read.desk.slots.size() == 1);
        CHECK(read.desk.slots[0].slot == surface::kSlotStatus);
        CHECK_FALSE(read.desk.slots[0].text.empty());
        CHECK(read.desk.slots[0].text == published_status(d.r));
        for (const PaneWord& w : words) {
            CHECK(w.text != read.desk.slots[0].text);
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
        const std::vector<surface::SurfaceTextRow> rows{{"[one] [two] [three]", surface::role::kFill},
                                                        {"press [Save] here", surface::role::kFill}};
        d.r.drive(d.alpha, [&rows](ProviderSeat& s, loom::Mail& m) {
            s.say_named(m, v4::PaneContent{"alpha", rows, 0, 0,
                                           {PaneRowPart{"control:one", 0, 0, 5},
                                            PaneRowPart{"control:two", 0, 6, 5},
                                            PaneRowPart{"control:three", 0, 12, 7},
                                            PaneRowPart{"control:save", 1, 6, 6}}});
        });
        const std::vector<NamedRun> runs{{"control:one", 0, 0, 5},
                                         {"control:two", 0, 6, 11},
                                         {"control:three", 0, 12, 19},
                                         {"control:save", 1, 6, 12}};
        DeskRead read;
        REQUIRE(d.read(read).empty());
        const v4::PaneView* named = reading_of(read, kAlphaOffice, "alpha");
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
        const v4::PaneView* layouts = reading_of(read, kWorkshopProvider, pane_key::kLayouts);
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
        Setup& desk = d.r.session().setup.active;
        const PaneRef ref{office, pane};
        REQUIRE(author_pane_place(desk, ref, x * surface::kCanvasCellPx, y * surface::kCanvasCellPx)
                    .accepted);
        REQUIRE(author_pane_size(desk, ref, PaneSize{pane_unit::kPixels, w * surface::kCanvasCellPx},
                                 PaneSize{pane_unit::kPixels, h * surface::kCanvasCellPx})
                    .accepted);
    };
    put(kAlphaOffice, "alpha", 4, 5, 40, 12);
    put(kFrontOffice, "front", 30, 0, 40, 10);
    put(kBetaOffice, "beta", 80, 20, 30, 8);
    d.r.extent(149, 60); // a same-size extent reseats nothing: the desk re-seats every pane
    d.r.extent(150, 60);
    const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
    REQUIRE(body.columns >= 36);
    REQUIRE(body.rows >= 7);
    std::vector<surface::SurfaceTextRow> rows;
    for (const char* text : {"first row", "a row that runs under the front pane", "short", "", "", "",
                             "a row below the front pane, said all"}) {
        rows.push_back(surface::SurfaceTextRow{text, surface::role::kFill});
    }
    d.r.drive(d.alpha, [&rows](ProviderSeat& s, loom::Mail& m) {
        s.say_named(m, v4::PaneContent{"alpha", rows, 0, 0,
                                       {PaneRowPart{"control:short", 2, 0, 5},
                                        PaneRowPart{"control:front", 1, 26, 5},
                                        PaneRowPart{"row:below", 6, 0, 36}}});
    });
    d.r.drive(front, [](ProviderSeat& s, loom::Mail& m) {
        s.say(m, PaneContent{"front", {surface::SurfaceTextRow{"in front", surface::role::kFill}}});
    });
    const auto page = [&d](v4::PaneView& out) {
        return d.page(v4::PaneViewRequested{kAlphaOffice, "alpha", 0, PaneStamp{}}, out);
    };
    const auto items = [](const v4::PaneView& v) {
        return static_cast<std::int64_t>(v.words.size() + v.parts.size());
    };

    // ALPHA IN FRONT: nothing covers it, and its whole reading is said, each part with its point.
    press_body(d.r, d.alpha_kind);
    v4::PaneView whole;
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
    v4::PaneView behind;
    REQUIRE(page(behind).empty());
    CHECK(behind.covered.by == std::vector<std::string>{"Front"});
    CHECK(behind.covered.words == hidden);
    CHECK(same(behind.covered.rect, overlap_of(alpha_at.rect, front_at.rect)));
    CHECK(texts_of(behind.words) == shown_words);
    std::vector<std::string> said_parts;
    for (const PanePart& p : behind.parts) said_parts.push_back(p.name);
    CHECK(said_parts == shown_parts);
    CHECK(behind.total == items(behind));
    // ...THE SAME IN THE DESK READ, and the third version still refuses the pane whole.
    DeskRead read;
    REQUIRE(d.read(read).empty());
    const v4::PaneView* in_read = reading_of(read, kAlphaOffice, "alpha");
    REQUIRE(in_read != nullptr);
    CHECK(in_read->covered.words == hidden);
    CHECK(texts_of(in_read->words) == shown_words);
    v3::PaneView refused;
    CHECK(d.parts(kAlphaOffice, "alpha", refused).find("another pane overlaps it") != std::string::npos);

    // WORKSHOP'S MENU, opened by a right press on alpha's title row, over alpha's words.
    press_outside(d.r, d.alpha_kind); // the keys are Workshop's
    const ui::Rect title = cells_covered(alpha_at.rect);
    d.r.publish(loom::to_value(input::PointerButton{3, true, title.x + 2,
                                                    title.y + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    REQUIRE(s.context.open);
    const PixelRect menu = context_bounds(s, screen_of(s));
    REQUIRE(overlap_of(menu, alpha_at.rect).w > 0);
    v4::PaneView under_menu;
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
    CHECK(d.parts(kAlphaOffice, "alpha", refused).find("covered by an interaction") != std::string::npos);
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
    const v4::PaneView* layouts_beside = reading_of(with_menu, kWorkshopProvider, pane_key::kLayouts);
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
    v4::PaneView arranged;
    REQUIRE(page(arranged).empty());
    CHECK(arranged.words.empty());
    CHECK(arranged.parts.empty());
    const std::vector<std::string>& over = arranged.covered.by;
    CHECK(std::find(over.begin(), over.end(), "arranging") != over.end());
    CHECK(arranged.covered.words == items(whole));
    CHECK(same(arranged.covered.rect, arranged_at.rect));
    CHECK(d.parts(kAlphaOffice, "alpha", refused).find("covered by an interaction") != std::string::npos);
}

TEST_CASE("Layouts and the band are read as Workshop draws them: tabs as named parts, the band's words "
          "with no point") {
    DeskRig d;
    // LAYOUTS NAMED WITH A SPACE, TWO ALIKE, ONE WITH BYTES PAST ASCII, ONE WITH A PERCENT SIGN AND
    // ONE NAMED AS THE CREATE TAB IS, painted again by a new extent.
    SetupState& setup = d.r.session().setup;
    for (const char* name : {"my desk", "twin", "twin", "caf\xC3\xA9", "100%", "+"}) {
        setup.shelved.push_back(Layout{Setup{name, {}}, SetupLink{}});
    }
    d.r.extent(149, 60);
    d.r.extent(150, 60);
    const std::vector<std::string> expected{"layout:Default", "layout:my desk",   "layout:twin#3",
                                            "layout:twin#4",  "layout:caf%C3%A9", "layout:100%25",
                                            "layout:%2B"};
    REQUIRE(layout_count(setup) == expected.size());
    const Session& s = d.r.session();
    const Screen sc = screen_of(s);
    const BandStatus status = band_status(s, sc);
    REQUIRE(status.tabs.size() == expected.size()); // every tab is painted
    REQUIRE(status.create_columns > 0);
    DeskRead read;
    REQUIRE(d.read(read).empty());

    // A PART FOR EVERY PAINTED TAB AND FOR `+`, each named apart, on the tab row, with a point.
    const v4::PaneView* layouts = reading_of(read, kWorkshopProvider, pane_key::kLayouts);
    REQUIRE(layouts != nullptr);
    REQUIRE_FALSE(layouts->words.empty());
    std::vector<std::string> want = expected;
    want.push_back("layout:+");
    std::sort(want.begin(), want.end());
    CHECK(names_of(layouts->parts) == want);
    const surface::CanvasGrids grid = surface::rasterize_canvas(d.r.last_canvas());
    const auto printable = [](const std::string& text) {
        return std::all_of(text.begin(), text.end(), [](char c) { return c >= 0x20 && c < 0x7F; });
    };
    for (const PanePart& p : layouts->parts) {
        CAPTURE(p.name);
        CHECK_FALSE(p.text.empty());
        CHECK(p.place.y == layouts->words[0].place.y);
        CHECK(p.space == input::space::kCells);
        if (!printable(p.text)) continue;
        // ...over the cells the terminal shows its characters on.
        bool one_row = false;
        std::string cells = terminal_cells(grid, p.place, one_row);
        while (!cells.empty() && cells.back() == ' ') cells.pop_back();
        CHECK(cells == p.text);
        CHECK(one_row);
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
    const surface::SurfaceTextRegion band = band_region(s, sc);
    std::vector<std::string> rows;
    for (const surface::SurfaceTextRow& r : band.rows) {
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
    Setup& desk = d.r.session().setup.active;
    const PaneRef layouts_ref{kWorkshopProvider, pane_key::kLayouts};
    const std::int64_t room_h = sc.notice_y - sc.room_y;
    REQUIRE(author_pane_place(desk, layouts_ref, 0, room_h - surface::kCanvasCellPx).accepted);
    REQUIRE(author_pane_size(desk, layouts_ref, PaneSize{pane_unit::kPixels, 100 * surface::kCanvasCellPx},
                             PaneSize{pane_unit::kPixels, 4 * surface::kCanvasCellPx})
                .accepted);
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
    const v4::PaneView* layouts_low = reading_of(under, kWorkshopProvider, pane_key::kLayouts);
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

TEST_CASE("a stamp names holder, incarnation, room and picture: a reading after t or a reload in place is "
          "stale even at an equal picture number") {
    SketchRig d;
    const auto page = [&d](const PaneStamp& stamp, v4::PaneView& out) {
        return d.page(v4::PaneViewRequested{kCanvasOffice, kCanvasPane, 0, stamp}, out);
    };
    // THE PANE AS IT STANDS, asked under no stamp: its office's holder, that holder's incarnation,
    // the room it was granted, and its picture.
    v4::PaneView first;
    REQUIRE(page(PaneStamp{}, first).empty());
    REQUIRE_FALSE(first.in_flight);
    {
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        CHECK(first.picture == pane.stamp.aimed);
        CHECK(first.grant == pane.canvas.grant);
    }
    CHECK(first.holder == static_cast<std::int64_t>(d.sketch_id.value));
    CHECK(first.incarnation == static_cast<std::int64_t>(d.r.bus.participant(d.sketch_id).incarnation));
    const PaneStamp kept = stamp_of_reading(first);
    v4::PaneView continued;
    const std::string unrefused = page(kept, continued);
    REQUIRE_MESSAGE(unrefused.empty(), unrefused);
    CHECK(continued.picture == kept.picture);
    // A PAGE PAST THE READING its stamp still names -- a cover moved and shrank it -- is refused in
    // words saying to read it again from the start.
    const std::string past = d.page(v4::PaneViewRequested{kCanvasOffice, kCanvasPane, first.total + 1, kept},
                                    continued);
    CHECK_MESSAGE(past.find("read it again from the start") != std::string::npos, past);

    // `t`: THE TITLES HIDDEN, A NEW ROOM, and the pane's first picture there numbered as the last.
    press_outside(d.r, d.sketch_kind); // the keys are Workshop's...
    d.r.key(input::scan::kT);          // ...and the titles hidden
    d.r.text("t");
    REQUIRE_FALSE(d.r.session().pane_titles);
    // ...ITS NEW ROOM OUT AND NOT YET DRAWN FOR: the pane's next picture is in flight, said with no
    // word, and a page under the stamp before it is stale.
    v4::PaneView awaited;
    const std::string unawaited = page(PaneStamp{}, awaited);
    REQUIRE_MESSAGE(unawaited.empty(), unawaited);
    CHECK(awaited.in_flight);
    CHECK(awaited.words.empty());
    CHECK(awaited.parts.empty());
    const std::string while_awaited = page(kept, continued);
    CHECK_MESSAGE(while_awaited.find("stale") != std::string::npos, while_awaited);
    d.number = kept.picture - 1;
    d.draw();
    v4::PaneView retitled;
    REQUIRE(page(PaneStamp{}, retitled).empty());
    REQUIRE_FALSE(retitled.in_flight);
    REQUIRE(retitled.picture == kept.picture);
    CHECK(retitled.grant != kept.grant);
    CHECK(retitled.holder == kept.holder);
    CHECK(retitled.incarnation == kept.incarnation);
    const std::string after_t = page(kept, continued);
    CHECK_MESSAGE(after_t.find("stale") != std::string::npos, after_t);
    const PaneStamp now = stamp_of_reading(retitled);
    CHECK(page(now, continued).empty());

    // A RELOAD IN PLACE: the same holder at the same id, in its next incarnation, the same room and
    // picture -- and a page under the stamp before it is stale.
    REQUIRE(d.r.bus.swap_state(d.sketch_id, d.r.bus.snapshot_bytes(d.sketch_id)).revived);
    v4::PaneView reloaded;
    const std::string unread = page(PaneStamp{}, reloaded);
    REQUIRE_MESSAGE(unread.empty(), unread);
    CHECK(reloaded.holder == now.holder);
    CHECK(reloaded.incarnation != now.incarnation);
    CHECK(reloaded.grant == now.grant);
    CHECK(reloaded.picture == now.picture);
    const std::string after_reload = page(now, continued);
    CHECK_MESSAGE(after_reload.find("stale") != std::string::npos, after_reload);
    // ...AND THE DESK READ NAMES THE PANE BY THE STAMP IT STANDS ON NOW.
    DeskRead read;
    REQUIRE(d.read(read).empty());
    const PaneStamp* in_read = stamp_in(read, kCanvasOffice, kCanvasPane);
    REQUIRE(in_read != nullptr);
    CHECK(in_read->holder == reloaded.holder);
    CHECK(in_read->incarnation == reloaded.incarnation);
    CHECK(in_read->grant == reloaded.grant);
    CHECK(in_read->picture == reloaded.picture);
}

TEST_CASE("a pane whose newest picture is in flight is read as in flight, with no word, and the desk still "
          "reads") {
    SketchRig d;
    say_rows(d);
    // A PICTURE SENT AND THE DESK ASKED BEHIND IT IN ONE TURN: the ask is answered before the fence
    // behind that picture comes round, so the picture is the pane's newest and not yet aimed at.
    const PaneCanvasContent newest = d.picture();
    d.sketch->next = [newest](SketchSeat&, loom::Mail& m) {
        (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, newest);
    };
    d.asker->next = [](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, DeskReadRequested{}); };
    d.asker->refusals.clear();
    const std::size_t before = d.asker->reads.size();
    (void)d.r.bus.send(d.sketch_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
    (void)d.r.bus.send(d.asker_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
    d.r.bus.drain_until_idle();
    REQUIRE(d.asker->refusals.empty());
    REQUIRE(d.asker->reads.size() == before + 1);
    const DeskRead during = d.asker->reads.back();
    const v4::PaneView* sketch = reading_of(during, kCanvasOffice, kCanvasPane);
    REQUIRE(sketch != nullptr);
    CHECK(sketch->in_flight);
    CHECK(sketch->words.empty());
    CHECK(sketch->parts.empty());
    CHECK(sketch->total == 0);
    CHECK(sketch->picture != newest.picture);
    // THE REST OF THE DESK IS READ in that same answer.
    CHECK(during.panes.size() == during.stamps.size());
    for (const auto& [office, pane] : {std::pair{kAlphaOffice, "alpha"}, std::pair{kBetaOffice, "beta"},
                                       std::pair{kWorkshopProvider, pane_key::kLayouts}}) {
        CAPTURE(pane);
        const v4::PaneView* other = reading_of(during, office, pane);
        REQUIRE(other != nullptr);
        CHECK_FALSE(other->in_flight);
        CHECK_FALSE(other->words.empty());
    }
    // ...AND ONCE THE FENCE HAS COME ROUND, that picture is read with its words.
    REQUIRE(d.r.session().panes.external_pane(d.sketch_kind)->stamp.aimed == newest.picture);
    DeskRead after;
    REQUIRE(d.read(after).empty());
    const v4::PaneView* drawn = reading_of(after, kCanvasOffice, kCanvasPane);
    REQUIRE(drawn != nullptr);
    CHECK_FALSE(drawn->in_flight);
    CHECK(drawn->picture == newest.picture);
    CHECK_FALSE(drawn->words.empty());

    // A LATER PAGE ASKED WHILE A NEWER PICTURE IS IN FLIGHT says so too, under the stamp it
    // continues, rather than being refused as past a reading of no items.
    v4::PaneView settled;
    REQUIRE(d.page(v4::PaneViewRequested{kCanvasOffice, kCanvasPane, 0, PaneStamp{}}, settled).empty());
    REQUIRE(settled.total > 1);
    const PaneCanvasContent newer = d.picture();
    d.sketch->next = [newer](SketchSeat&, loom::Mail& m) {
        (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, newer);
    };
    const v4::PaneViewRequested later{kCanvasOffice, kCanvasPane, 1, stamp_of_reading(settled)};
    d.asker->next = [later](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, later); };
    d.asker->refusals.clear();
    const std::size_t pages_before = d.asker->pages.size();
    (void)d.r.bus.send(d.sketch_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
    (void)d.r.bus.send(d.asker_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
    d.r.bus.drain_until_idle();
    REQUIRE_MESSAGE(d.asker->refusals.empty(), d.asker->refusals.front());
    REQUIRE(d.asker->pages.size() == pages_before + 1);
    const v4::PaneView flying = d.asker->pages.back();
    CHECK(flying.in_flight);
    CHECK(flying.from == 1);
    CHECK(flying.words.empty());
    CHECK(flying.parts.empty());

    // A TEXT PANE'S NEW ROOM, NOT YET ANSWERED, is in flight as well...
    const auto alpha_page = [&d](v4::PaneView& out) {
        return d.page(v4::PaneViewRequested{kAlphaOffice, "alpha", 0, PaneStamp{}}, out);
    };
    d.r.extent(149, 60);
    v4::PaneView awaited;
    const std::string unawaited = alpha_page(awaited);
    REQUIRE_MESSAGE(unawaited.empty(), unawaited);
    CHECK(awaited.in_flight);
    CHECK(awaited.words.empty());
    // ...and once an update Workshop refused, keeping none of it, stands, that is said in words.
    d.r.drive(d.alpha, [](ProviderSeat& s, loom::Mail& m) {
        s.say_named(m, v4::PaneContent{"alpha", {surface::SurfaceTextRow{"one row", surface::role::kFill}},
                                       0, 0, {PaneRowPart{"control:far", 5, 0, 3}}});
    });
    REQUIRE_FALSE(d.r.session().panes.external_pane(d.alpha_kind)->refusal.empty());
    v4::PaneView refused;
    const std::string why = alpha_page(refused);
    CHECK_MESSAGE(why.find("refused the pane's last update") != std::string::npos, why);

    // A FIRST PICTURE REFUSED, nothing standing before it -- the pane offered again, its room's
    // first picture refused -- is said in words, not in flight.
    d.drive([](SketchSeat&, loom::Mail& m) {
        (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider,
            v3::PaneOffered{kCanvasPane, "Sketch", "a local picture", 40 * kPaneCanvasUnit,
                            12 * kPaneCanvasUnit, 0});
    });
    PaneCanvasContent first_refused = d.picture();
    first_refused.labels[0].text = "caf\xC3\xA9";
    d.drive([first_refused](SketchSeat&, loom::Mail& m) {
        (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, first_refused);
    });
    {
        const ExternalPane& sketch_now = *d.r.session().panes.external_pane(d.sketch_kind);
        REQUIRE(sketch_now.refusal == kExternalPictureRefused);
        REQUIRE_FALSE(sketch_now.canvas.heard);
        REQUIRE_FALSE(sketch_now.canvas.preview);
    }
    v4::PaneView none_kept;
    const std::string kept_none =
        d.page(v4::PaneViewRequested{kCanvasOffice, kCanvasPane, 0, PaneStamp{}}, none_kept);
    CHECK_MESSAGE(kept_none.find("refused the pane's last update") != std::string::npos, kept_none);

    // A PANE WHOSE PROVIDER LEFT waits for one, and no reading says it is in flight: its picture
    // drawn, then its holder gone and its room taken back.
    d.draw();
    auto gone = d.r.bus.unregister_weave(d.sketch_id);
    REQUIRE(gone);
    d.r.extent(150, 60);
    v4::PaneView waiting;
    const std::string waits = d.page(v4::PaneViewRequested{kCanvasOffice, kCanvasPane, 0, PaneStamp{}}, waiting);
    CHECK_MESSAGE(waits.find("waiting for the provider") != std::string::npos, waits);
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
    const ui::Rect title = cells_covered(external_pane_rect(d.r.session(), d.alpha_kind));
    d.r.publish(loom::to_value(input::PointerButton{3, true, title.x + 2,
                                                    title.y + surface::kTuiCanvasTopRow,
                                                    input::space::kCells, input::mod::kNone}));
    REQUIRE(d.r.session().context.open);
    CHECK(number() != keys_back);
}
