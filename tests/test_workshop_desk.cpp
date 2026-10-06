// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop desk suite -- the desk and the words on it, read by message: every pane on the
// desk in Workshop's own numbers, and each run of words where the medium draws it.

#include "doctest.h"
#include "workshop_support.hpp"

#include <functional>
#include <memory>
#include <string>
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
                             loom::Accept<SeatDo, DeskView, v2::PaneView, v2::PanePoint, PaneView,
                                          loom::Refused>,
                             loom::Emit<DeskViewRequested, v2::PaneViewRequested,
                                        v2::PanePointRequested, PaneViewRequested>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<DeskView> desks;
    std::vector<v2::PaneView> views;
    std::vector<v2::PanePoint> points;
    std::vector<PaneView> first_views;
    std::vector<std::string> refusals;
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(m);
    }
    void on(const DeskView& d, loom::Mail&) { desks.push_back(d); }
    void on(const v2::PaneView& v, loom::Mail&) { views.push_back(v); }
    void on(const v2::PanePoint& p, loom::Mail&) { points.push_back(p); }
    void on(const PaneView& v, loom::Mail&) { first_views.push_back(v); }
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
                                  loom::schema_of<v2::PaneViewRequested>(),
                                  loom::schema_of<v2::PanePointRequested>(),
                                  loom::schema_of<PaneViewRequested>()}) {
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
    loom::Emit<v3::PaneOffered, PaneCanvasContent>> {
public:
    std::vector<PaneCanvasRoom> rooms;
    std::vector<PaneCanvasPointer> pointers;
    std::function<void(SketchSeat&, loom::Mail&)> next;
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const PaneRoom&, loom::Mail&) {}
    void on(const PaneCanvasRoom& r, loom::Mail&) { rooms.push_back(r); }
    void on(const PaneCanvasPointer& e, loom::Mail&) { pointers.push_back(e); }
    void on(const PaneCanvasHover&, loom::Mail&) {}
    void on(const PaneCanvasRejected&, loom::Mail&) {}
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(*this, m);
    }
};

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
        loom::Grant grant;
        grant.allow_to_any(v3::PaneOffered::zen_name, v3::PaneOffered::zen_version);
        grant.allow_to_any(PaneCanvasContent::zen_name, PaneCanvasContent::zen_version);
        sketch_id = r.bus.register_weave(std::move(made), std::move(grant), std::string(kCanvasOffice));
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
    /// The picture, drawn for the room last granted.
    void draw() {
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
        drive([p](SketchSeat&, loom::Mail& m) {
            (void)m.as_role(kCanvasOffice).send_to_role(kWorkshopProvider, p);
        });
        REQUIRE(r.session().panes.external_pane(sketch_kind)->canvas.heard);
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
