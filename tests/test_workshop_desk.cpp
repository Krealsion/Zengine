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
                                          v2::PanePoint, PaneView, PanePoint, loom::Refused>,
                             loom::Emit<DeskViewRequested, v2::DeskViewRequested,
                                        v2::PaneViewRequested, v3::PaneViewRequested,
                                        v2::PanePointRequested, PaneViewRequested,
                                        PanePointRequested>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<DeskView> desks;
    std::vector<v2::DeskView> named_desks;
    std::vector<v2::PaneView> views;
    std::vector<v3::PaneView> named_views;
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
                                  loom::schema_of<v2::PanePointRequested>(),
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
    loom::Emit<v3::PaneOffered, PaneCanvasContent, v4::PaneCanvasContent>> {
public:
    std::vector<PaneCanvasRoom> rooms;
    std::vector<PaneCanvasPointer> pointers;
    std::vector<PaneCanvasRejected> rejected;
    std::function<void(SketchSeat&, loom::Mail&)> next;
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const PaneRoom&, loom::Mail&) {}
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
        // it does not, and a terminal draws a caret glyph into the run with the caret.
        if (window) {
            CHECK(view.words[2].place.w == 14 * d.r.session().text_advance_px);
            CHECK(view.words[3].place.w == 10 * d.r.session().text_advance_px);
        } else {
            CHECK(view.words[2].place.w == 14 * kPaneCanvasUnit);
            CHECK(view.words[3].place.w == 11 * kPaneCanvasUnit);
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
        // ...AND ONE AFTER THE CARET, where a terminal draws a caret glyph before it: `d`, the
        // run's fifth byte, stands in its sixth cell there, and in its fifth advance in a window.
        REQUIRE(d.point(v2::PanePointRequested{kCanvasOffice, kCanvasPane, view.picture, 3, 4}, at).empty());
        const DeskRect typed = view.words[3].place;
        if (window) {
            const auto advance = d.r.session().text_advance_px;
            CHECK(at.x == typed.x + 4 * advance + advance / 2);
        } else {
            CHECK(at.x == surface::cell_of_pixel(typed.x) + 5);
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
/// terminal rasterizer draws the canvas (`rasterize_canvas`), less the caret glyph it draws into
/// a run with the caret.
std::string terminal_cells(const surface::CanvasGrids& g, const DeskRect& p, bool& one_row) {
    one_row = p.h == surface::kCanvasCellPx;
    const std::int64_t y = surface::cell_of_pixel(p.y);
    const std::int64_t x0 = surface::cell_of_pixel(p.x);
    const std::int64_t x1 = surface::cell_of_pixel(p.x + p.w);
    std::string out;
    for (std::int64_t x = x0; x < x1; ++x) {
        if (x < 0 || y < 0 || x >= g.w || y >= g.h) return "(off the terminal)";
        const char c = g.glyphs[static_cast<std::size_t>(y * g.w + x)];
        if (c != surface::kCaretGlyph) out += c;
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

TEST_CASE("a character's point is the cell showing it, past a terminal's caret glyph, and a press on a cell reaches the pane as the column of the character it shows") {
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
            // THE CARET'S OWN CELL shows no character of the row: a press there is the caret's column.
            v2::PaneView view;
            REQUIRE(d.words(kAlphaOffice, "alpha", view).empty());
            const surface::CanvasGrids grid = surface::rasterize_canvas(d.r.last_canvas());
            const std::int64_t row = surface::cell_of_pixel(view.words[0].place.y);
            const std::int64_t x0 = surface::cell_of_pixel(view.words[0].place.x);
            const std::int64_t caret = terminal_cell_of(grid, row, x0, x0 + 7, surface::kCaretGlyph);
            REQUIRE(caret == x0 + 2);
            d.alpha->presses.clear();
            d.click(caret, row + surface::kTuiCanvasTopRow, input::space::kCells);
            REQUIRE(d.alpha->presses.size() == 1);
            CHECK(d.alpha->presses[0].column == 2);
        }
    }
}

TEST_CASE("a row as wide as its pane with a terminal's caret in it says only the characters shown, at a place inside the pane") {
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
        // A terminal inserts the caret's glyph and then cuts the row, so one character fewer shows,
        // unless the glyph stands past the last column, where the cut takes the glyph alone; a
        // window draws a bar, which takes no character's place.
        const std::int64_t shown = window || at_end ? body.columns : body.columns - 1;
        CHECK(w.text == full.substr(0, static_cast<std::size_t>(shown)));
        CHECK(w.place.x + w.place.w <= row_right_edge(body));
        if (!window) {
            const surface::CanvasGrids grid = surface::rasterize_canvas(d.r.last_canvas());
            bool one_row = false;
            CHECK(terminal_cells(grid, w.place, one_row) == w.text);
            CHECK(one_row);
        }
        // THE LAST CHARACTER SHOWN HAS A POINT, AND A PRESS THERE IS ITS COLUMN...
        v2::PanePoint at;
        REQUIRE(d.point(v2::PanePointRequested{kAlphaOffice, "alpha", view.picture, 0, shown - 1}, at)
                    .empty());
        d.alpha->presses.clear();
        d.click(at.x, at.y, at.space);
        REQUIRE(d.alpha->presses.size() == 1);
        CHECK(d.alpha->presses[0].column == shown - 1);
        if (!window && !at_end) {
            // ...AND THE ONE THE CUT TOOK HAS NONE, in either version.
            CHECK(d.point(v2::PanePointRequested{kAlphaOffice, "alpha", view.picture, 0, shown}, at)
                      .find("outside") != std::string::npos);
            PanePoint first;
            CHECK(d.first_point(PanePointRequested{kAlphaOffice, "alpha", view.picture, 0, shown}, first)
                      .find("not addressable") != std::string::npos);
        }
    }
}

TEST_CASE("a word that is only a caret is pressed on the caret's own cell, inside its place, in a text row and in a canvas field at the body's right edge") {
    // A TEXT ROW WITH NOTHING ON IT BUT THE CARET: a terminal draws the glyph in the row's first
    // cell, and that cell is the word.
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
        CHECK(terminal_glyph_at(grid, w.x, w.y) == surface::kCaretGlyph);
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

TEST_CASE("a text pane's named parts are said under the pane's own names over the cells that show them, and a press at a part's point lands on it, in a window and in a terminal") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        DeskRig d;
        if (window) {
            d.r.extent_on_window(150, 60);
        }
        const ExternalBodyPlace body = external_body_of(d.r.session(), d.alpha_kind);
        REQUIRE(body.columns >= 20);
        // The caret before `ess`: a terminal draws its glyph there, standing `[Save]` a cell on.
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
        // A ROW ENTIRE COVERS THE BODY'S COLUMNS, A CONTROL ITS OWN -- a cell on, past a
        // terminal's caret -- and an empty field its own, though nothing is drawn in it.
        CHECK(first->place.x == left);
        CHECK(first->place.w == body.columns * advance);
        CHECK(first->place.y == view.words[0].place.y);
        CHECK(save->place.x == left + (window ? 6 : 7) * advance);
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

TEST_CASE("a part's point is a place of its own: a row with a control inside it is pressed beside the control, the control on itself, a row whose text is all a control's on a blank cell of its own, and a row with no place of its own has no point, in a window and in a terminal") {
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
        CHECK((d.alpha->presses[0].column < 2 || d.alpha->presses[0].column >= 8));
        CHECK(d.alpha->presses[0].column < 14);
        press_lands(d, *part_named(view.parts, "mark:files"), NamedRun{"mark:files", 0, 2, 8});
        press_lands(d, *part_named(view.parts, "control:one"), NamedRun{"control:one", 1, 0, 5});
        press_lands(d, *part_named(view.parts, "control:two"), NamedRun{"control:two", 1, 5, 10});
        press_lands(d, *part_named(view.parts, "control:go"), NamedRun{"control:go", 2, 0, 4});
        // A PART WHOSE EVERY CHARACTER IS ANOTHER'S is pressed on a blank cell of its own...
        press_lands(d, *part_named(view.parts, "row:go"), NamedRun{"row:go", 2, 4, body.columns});
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

TEST_CASE("a running view's two overlapping buttons are each pressed where the view gives that button the press, in a window and in a terminal") {
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

TEST_CASE("a canvas part's point is sought over every unit of it the body shows: one whose centre, edges and middle lines are all another's is pressed where it is its own, and one with no place of its own has no point, never another's, in a window and in a terminal") {
    for (const bool window : {false, true}) {
        CAPTURE(window);
        SketchRig d;
        d.medium(window);
        const std::int64_t u = kPaneCanvasUnit;
        const std::int64_t grain = window ? surface::kPixelGrainPx : surface::kCellGrainPx;
        // `lined` has places it names nothing over its centre, top and bottom rows and its centre,
        // left and right columns, and its own places between them; `covered` lies under two parts.
        const std::vector<PaneCanvasPart> parts{PaneCanvasPart{"lined", 0, 0, 10 * u, 10 * u},
                                                PaneCanvasPart{"", 0, 0, 10 * u, u},
                                                PaneCanvasPart{"", 0, 9 * u, 10 * u, u},
                                                PaneCanvasPart{"", 0, 4 * u, 10 * u, 2 * u},
                                                PaneCanvasPart{"", 0, 0, u, 10 * u},
                                                PaneCanvasPart{"", 9 * u, 0, u, 10 * u},
                                                PaneCanvasPart{"", 4 * u, 0, 2 * u, 10 * u},
                                                PaneCanvasPart{"covered", 12 * u, 0, 4 * u, 2 * u},
                                                PaneCanvasPart{"cover:left", 12 * u, 0, 2 * u, 2 * u},
                                                PaneCanvasPart{"cover:right", 14 * u, 0, 2 * u, 2 * u}};
        d.draw_named(parts);
        REQUIRE(d.sketch->rejected.empty());
        v3::PaneView view;
        REQUIRE(d.parts(kCanvasOffice, kCanvasPane, view).empty());
        CHECK(names_of(view.parts) ==
              std::vector<std::string>{"cover:left", "cover:right", "covered", "lined"});
        const ExternalPane& pane = *d.r.session().panes.external_pane(d.sketch_kind);
        for (const PanePart& part : view.parts) {
            CAPTURE(part.name);
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

TEST_CASE("a picture of as many parts as a picture may name, nested and overlapping, gives each part a point a press there gives it, or none where every place of it is another's, in a window and in a terminal") {
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

TEST_CASE("a row's part is pressed where its row map gives it the press: not on a narrower run lying over part of it, nor on a run the pane names nothing, in a window and in a terminal") {
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
    CHECK(d.r.session().panes.external_pane(d.sketch_kind)->canvas.parts.size() == 3);
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
