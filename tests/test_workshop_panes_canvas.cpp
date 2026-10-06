// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- a pane's own canvas picture: admission, clipping, capture, grants,
// and measured text, through a native provider seat.

#include "doctest.h"
#include "workshop_support.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/screen_canvas.hpp"
#include "view/host.hpp"
#include <limits>

namespace {
constexpr const char* canvas_office = "zengine.test.canvas";
constexpr const char* canvas_pane = "diagram";

class CanvasSeat : public loom::WeaveBase<CanvasSeat, SeatState,
    loom::Accept<PaneCatalogRequested, PaneRoom, PaneCanvasRoom, PaneCanvasPointer,
                 PaneCanvasHover, PaneCanvasRejected, SeatDo>,
    loom::Emit<PaneOffered, PaneCanvasContent, PaneContent, PanePassRequested>> {
public:
    std::vector<PaneCanvasRoom> rooms;
    std::vector<PaneCanvasPointer> pointers;
    std::vector<PaneCanvasRejected> rejected;
    std::vector<PaneCanvasHover> hovers;
    std::function<void(CanvasSeat&, loom::Mail&)> next;
    bool pass_right = false; ///< hand a right press back, as a picture that means nothing by it
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const PaneRoom&, loom::Mail&) {}
    void on(const PaneCanvasRoom& r, loom::Mail& m) {
        REQUIRE(m.authored_from_role(kWorkshopProvider));
        rooms.push_back(r);
    }
    void on(const PaneCanvasPointer& e, loom::Mail& m) {
        REQUIRE(m.authored_from_role(kWorkshopProvider));
        pointers.push_back(e);
        if (pass_right && e.phase == canvas_pointer::kPress && e.button == 3) {
            (void)pane_menu::pass_back(m, canvas_office, canvas_pane);
        }
    }
    void on(const PaneCanvasRejected& r, loom::Mail&) { rejected.push_back(r); }
    void on(const PaneCanvasHover& h, loom::Mail& m) {
        REQUIRE(m.authored_from_role(kWorkshopProvider));
        hovers.push_back(h);
    }
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next); next = {};
        if (run) run(*this, m);
    }
    void offer(loom::Mail& m) {
        (void)m.as_role(canvas_office).send_to_role(kWorkshopProvider,
            PaneOffered{canvas_pane, "Diagram", "a local picture"});
    }
};

struct CanvasRig {
    PaneRig r;
    CanvasSeat* seat = nullptr;
    loom::WeaveId id{};
    std::int64_t kind = kNoPaneKind;
    CanvasRig() {
        r.mount_workshop();
        r.host.role_holder = [this](std::string_view office) { return r.bus.role_holder(office); };
        r.ready();
        r.extent(150, 65);
        mount();
        drive([](CanvasSeat& s, loom::Mail& m) { s.offer(m); });
        r.pick(PaneRef{canvas_office, canvas_pane});
        const auto* row = r.session().panes.runtime.find(canvas_office, canvas_pane);
        REQUIRE(row);
        kind = row->kind;
        REQUIRE(!seat->rooms.empty());
        publish();
    }
    void mount() {
        auto w = std::make_unique<CanvasSeat>(); seat = w.get();
        loom::Grant grant;
        grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
        grant.allow_to_any(PaneCanvasContent::zen_name, PaneCanvasContent::zen_version);
        grant.allow_to_any(PaneContent::zen_name, PaneContent::zen_version);
        grant.allow_to_any(PanePassRequested::zen_name, PanePassRequested::zen_version);
        id = r.bus.register_weave(std::move(w), std::move(grant), std::string(canvas_office));
        seat->zen_set_self(id);
    }
    void drive(std::function<void(CanvasSeat&, loom::Mail&)> f) {
        seat->next = std::move(f);
        (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    }
    PaneCanvasContent picture(std::int64_t number = 1) {
        REQUIRE(!seat->rooms.empty());
        PaneCanvasContent p;
        p.pane = canvas_pane; p.grant = seat->rooms.back().grant; p.picture = number;
        p.rects.push_back(PaneCanvasRect{0, 0, 2 * kPaneCanvasUnit, kPaneCanvasUnit,
                                       surface::role::kAccent});
        p.labels.push_back(PaneCanvasLabel{0, kPaneCanvasUnit, "node", surface::role::kFill});
        return p;
    }
    void publish(std::int64_t number = 1) {
        const auto p = picture(number);
        drive([p](CanvasSeat&, loom::Mail& m) {
            (void)m.as_role(canvas_office).send_to_role(kWorkshopProvider, p);
        });
        REQUIRE(view().canvas.heard);
    }
    ExternalPane& view() {
        auto* p = r.session().panes.external_pane(kind); REQUIRE(p); return *p;
    }
    void button(std::int64_t button, bool down, std::int64_t x = kPaneCanvasUnit,
                std::int64_t y = kPaneCanvasUnit) {
        const auto c = view().canvas;
        r.publish(loom::to_value(input::PointerButton{button, down,
            (c.x + x) / kPaneCanvasUnit,
            (c.y + y) / kPaneCanvasUnit + surface::kTuiCanvasTopRow,
            input::space::kCells, input::mod::kNone}));
    }
    /// The pointer moved to a local place of the canvas, with no button held.
    void move(std::int64_t x, std::int64_t y) {
        const auto c = view().canvas;
        r.publish(loom::to_value(input::PointerMoved{(c.x + x) / kPaneCanvasUnit,
            (c.y + y) / kPaneCanvasUnit + surface::kTuiCanvasTopRow, 0, 0, input::space::kCells,
            input::mod::kNone}));
    }
};
}

TEST_CASE("pane canvas rejects malformed pictures whole and budgets data before rendering") {
    PaneCanvasContent c{canvas_pane, 1, 1, {{-9, -7, 10, 20, surface::role::kAccent}}, {}};
    CHECK(canvas_content_problem(c).empty());
    c.rects[0].w = 0;
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.rects[0].w = 1;
    c.rects[0].role = surface::role::kGround;
    CHECK(canvas_content_problem(c).empty());
    c.rects[0].role = 99;
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.rects.clear();
    c.labels.resize(kPaneCanvasMaxLabels + 1);
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.labels.assign(kPaneCanvasMaxTextBytes / kPaneCanvasMaxLabelBytes,
                    PaneCanvasLabel{0, 0, std::string(kPaneCanvasMaxLabelBytes, 'a'), 0});
    CHECK(canvas_content_problem(c).empty());
    c.labels.push_back(PaneCanvasLabel{0, 0, "a", 0});
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.labels.clear();
    c.labels.push_back(PaneCanvasLabel{0, 0, "line\nbreak", surface::role::kFill});
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.labels[0].text = std::string(kPaneCanvasMaxLabelBytes, 'a');
    CHECK(canvas_content_problem(c).empty());
    c.labels[0].text += 'a';
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.labels.clear();
    c.rects.resize(kPaneCanvasMaxRects, PaneCanvasRect{0, 0, 1, 1, 0});
    CHECK(canvas_content_problem(c).empty());
    c.rects.push_back(PaneCanvasRect{0, 0, 1, 1, 0});
    CHECK_FALSE(canvas_content_problem(c).empty());
}

TEST_CASE("pane canvas clips every primitive at its local boundary before translating") {
    constexpr auto unit = kPaneCanvasUnit;
    const PixelRect body{7 * unit, 11 * unit, 4 * unit, 3 * unit};
    for (std::int64_t x = -5 * unit; x <= 6 * unit; x += 7) {
        const auto c = canvas_clip_rect(PaneCanvasRect{x, -unit, 2 * unit, 3 * unit, 0}, body.w, body.h);
        if (!c.empty()) {
            CHECK(c.x >= 0); CHECK(c.y >= 0);
            CHECK(c.x + c.w <= body.w); CHECK(c.y + c.h <= body.h);
            CHECK(c.y == 0); CHECK(c.h == 2 * unit);
        }
    }
    const auto largest = (std::numeric_limits<std::int64_t>::max)();
    const auto clipped = canvas_clip_rect(PaneCanvasRect{-unit, -unit, largest, largest, 0}, body.w, body.h);
    CHECK(clipped == PixelRect{0, 0, body.w, body.h});
    PaneCanvasContent c{canvas_pane, 1, 1,
        {{-unit, -unit, 3 * unit, 3 * unit, surface::role::kAccent}},
        {{-unit, 0, "ABCDE", 0}, {0, -1, "hidden", 0}, {0, body.h - unit + 1, "hidden", 0}}};
    surface::SurfaceLayer layer;
    paint_pane_canvas(layer, body, c);
    REQUIRE(layer.rects.size() == 1);
    CHECK(layer.rects[0].x == body.x); CHECK(layer.rects[0].y == body.y);
    CHECK(layer.rects[0].w == 2 * unit); CHECK(layer.rects[0].h == 2 * unit);
    REQUIRE(layer.labels.size() == 1);
    CHECK(layer.labels[0].text == "BCDE");
    CHECK(layer.labels[0].x == body.x); CHECK(layer.labels[0].y == body.y);
}

namespace {
/// A provider built before the canvas spoke pixels: it accepts only the earlier doors.
class LegacyCanvasSeat : public loom::WeaveBase<LegacyCanvasSeat, SeatState,
    loom::Accept<PaneCatalogRequested, PaneRoom, v2::PaneCanvasRoom, v1::PaneCanvasPointer,
                 PaneCanvasRejected, SeatDo>,
    loom::Emit<PaneOffered, v2::PaneCanvasContent>> {
public:
    std::vector<v2::PaneCanvasRoom> rooms;
    std::vector<v1::PaneCanvasPointer> pointers;
    std::vector<PaneCanvasRejected> rejected;
    std::function<void(LegacyCanvasSeat&, loom::Mail&)> next;
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const PaneRoom&, loom::Mail&) {}
    void on(const v2::PaneCanvasRoom& r, loom::Mail&) { rooms.push_back(r); }
    void on(const v1::PaneCanvasPointer& e, loom::Mail&) { pointers.push_back(e); }
    void on(const PaneCanvasRejected& r, loom::Mail&) { rejected.push_back(r); }
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next); next = {};
        if (run) run(*this, m);
    }
};
}

TEST_CASE("a canvas provider that speaks only the earlier doors is answered in them") {
    // FOUR SUB-UNITS TO A PIXEL, BOTH WAYS: its room and its pointer are said in the sub-units
    // it was built for, and its picture lands on the pixels the window always painted it at.
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    r.extent(150, 65);
    auto w = std::make_unique<LegacyCanvasSeat>();
    LegacyCanvasSeat* seat = w.get();
    loom::Grant grant;
    grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
    grant.allow_to_any(v2::PaneCanvasContent::zen_name, v2::PaneCanvasContent::zen_version);
    const loom::WeaveId id = r.bus.register_weave(std::move(w), std::move(grant),
                                                  std::string(canvas_office));
    seat->zen_set_self(id);
    const auto drive = [&](std::function<void(LegacyCanvasSeat&, loom::Mail&)> f) {
        seat->next = std::move(f);
        (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    };
    drive([](LegacyCanvasSeat&, loom::Mail& m) {
        (void)m.as_role(canvas_office).send_to_role(kWorkshopProvider,
            PaneOffered{canvas_pane, "Diagram", "a local picture"});
    });
    r.pick(PaneRef{canvas_office, canvas_pane});
    const auto* row = r.session().panes.runtime.find(canvas_office, canvas_pane);
    REQUIRE(row);
    const std::int64_t kind = row->kind;
    const auto& c = r.session().panes.external_pane(kind)->canvas;
    CHECK(c.legacy);
    REQUIRE(seat->rooms.size() == 1);
    CHECK(seat->rooms.back().width == kPaneCanvasLegacySubs * c.width);
    CHECK(seat->rooms.back().height == kPaneCanvasLegacySubs * c.height);
    CHECK(seat->rooms.back().grain == kPaneCanvasLegacySubs * c.grain);

    // A RECT FROM SUB-UNIT 2 TO 50 WAS PAINTED ON PIXELS 0 TO 12, and lands there.
    v2::PaneCanvasContent old;
    old.pane = canvas_pane; old.grant = seat->rooms.back().grant; old.picture = 1;
    old.rects.push_back(PaneCanvasRect{2, 5, 48, 47, surface::role::kAccent});
    old.labels.push_back(PaneCanvasLabel{9, 48, "node", surface::role::kFill});
    drive([old](LegacyCanvasSeat&, loom::Mail& m) {
        (void)m.as_role(canvas_office).send_to_role(kWorkshopProvider, old);
    });
    const auto& got = r.session().panes.external_pane(kind)->canvas;
    REQUIRE(got.heard);
    REQUIRE(got.content.rects.size() == 1);
    CHECK(got.content.rects[0].x == 0); CHECK(got.content.rects[0].y == 1);
    CHECK(got.content.rects[0].w == 12); CHECK(got.content.rects[0].h == 12);
    REQUIRE(got.content.labels.size() == 1);
    CHECK(got.content.labels[0].x == 2);
    CHECK(got.content.labels[0].y == 12);

    // A PRESS ONE CELL INTO THE BODY reaches it at that cell's corner, in sub-units.
    r.publish(loom::to_value(input::PointerButton{1, true,
        (got.x + kPaneCanvasUnit) / kPaneCanvasUnit,
        (got.y + kPaneCanvasUnit) / kPaneCanvasUnit + surface::kTuiCanvasTopRow,
        input::space::kCells, input::mod::kNone}));
    REQUIRE(seat->pointers.size() == 1);
    CHECK(seat->pointers.back().phase == canvas_pointer::kPress);
    const std::int64_t at_x = surface::px_of_cells(surface::cell_of_pixel(got.x) + 1) - got.x;
    const std::int64_t at_y = surface::px_of_cells(surface::cell_of_pixel(got.y) + 1) - got.y;
    CHECK(seat->pointers.back().x == kPaneCanvasLegacySubs * at_x);
    CHECK(seat->pointers.back().y == kPaneCanvasLegacySubs * at_y);
}

TEST_CASE("an older canvas picture is judged by its own rules, and a rect that floors to nothing "
          "is dropped, not the picture") {
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    r.extent(150, 65);
    auto w = std::make_unique<LegacyCanvasSeat>();
    LegacyCanvasSeat* seat = w.get();
    loom::Grant grant;
    grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
    grant.allow_to_any(v2::PaneCanvasContent::zen_name, v2::PaneCanvasContent::zen_version);
    const loom::WeaveId id = r.bus.register_weave(std::move(w), std::move(grant),
                                                  std::string(canvas_office));
    seat->zen_set_self(id);
    const auto drive = [&](std::function<void(LegacyCanvasSeat&, loom::Mail&)> f) {
        seat->next = std::move(f);
        (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    };
    drive([](LegacyCanvasSeat&, loom::Mail& m) {
        (void)m.as_role(canvas_office).send_to_role(kWorkshopProvider,
            PaneOffered{canvas_pane, "Diagram", "a local picture"});
    });
    r.pick(PaneRef{canvas_office, canvas_pane});
    const auto* row = r.session().panes.runtime.find(canvas_office, canvas_pane);
    REQUIRE(row);
    const std::int64_t kind = row->kind;
    REQUIRE(seat->rooms.size() == 1);
    const auto send = [&](const v2::PaneCanvasContent& old) {
        drive([old](LegacyCanvasSeat&, loom::Mail& m) {
            (void)m.as_role(canvas_office).send_to_role(kWorkshopProvider, old);
        });
    };

    // A SLIVER ONE SUB-UNIT WIDE is a rect its own doors allow, and the window painted no pixel of
    // it: the picture is kept without it. The same sliver three sub-units over reaches pixel 1.
    v2::PaneCanvasContent old;
    old.pane = canvas_pane; old.grant = seat->rooms.back().grant; old.picture = 1;
    old.rects.push_back(PaneCanvasRect{0, 0, 1, 48, surface::role::kAccent});
    old.rects.push_back(PaneCanvasRect{3, 0, 1, 48, surface::role::kAccent});
    old.rects.push_back(PaneCanvasRect{8, 8, 40, 40, surface::role::kFill});
    send(old);
    CHECK(seat->rejected.empty());
    const auto& got = r.session().panes.external_pane(kind)->canvas;
    REQUIRE(got.heard);
    CHECK(got.content.picture == 1);
    REQUIRE(got.content.rects.size() == 2);
    CHECK(got.content.rects[0].x == 0); CHECK(got.content.rects[0].w == 1);
    CHECK(got.content.rects[0].h == 12);
    CHECK(got.content.rects[1].x == 2); CHECK(got.content.rects[1].y == 2);
    CHECK(got.content.rects[1].w == 10); CHECK(got.content.rects[1].h == 10);

    // ...AND A RECT ITS OWN RULES REFUSE refuses the picture, in those rules' words; the last good
    // picture stays.
    v2::PaneCanvasContent bad = old;
    bad.picture = 2;
    bad.rects.push_back(PaneCanvasRect{4, 4, 0, 8, surface::role::kFill});
    send(bad);
    REQUIRE(seat->rejected.size() == 1);
    CHECK(seat->rejected.back().picture == 2);
    CHECK(seat->rejected.back().reason == "canvas rectangles must have positive extents");
    CHECK(r.session().panes.external_pane(kind)->canvas.content.picture == 1);
}

TEST_CASE("pane canvas grants fenced room and keeps a good picture after a refused update") {
    CanvasRig t;
    const auto room = t.seat->rooms.back();
    CHECK(room.grant > 0); CHECK(room.width > 0); CHECK(room.height > 0);
    CHECK(room.grain == kPaneCanvasUnit); CHECK_FALSE(room.graphical);
    CHECK(t.view().stamp.aimed == 1);
    auto bad = t.picture(2); bad.rects[0].w = -1;
    t.drive([bad](CanvasSeat&, loom::Mail& m) {
        (void)m.as_role(canvas_office).send_to_role(kWorkshopProvider, bad);
    });
    REQUIRE(t.seat->rejected.size() == 1);
    CHECK(t.seat->rejected.back().picture == 2);
    CHECK(t.view().picture == 1); CHECK(t.view().canvas.heard);
    t.publish(2);
    CHECK(t.view().stamp.aimed == 2);
    t.publish(2);
    CHECK(t.seat->rejected.size() == 2);
    CHECK(t.view().picture == 2);
    auto forged = t.picture(3);
    t.r.publish(loom::to_value(forged));
    CHECK(t.view().picture == 2);
}

TEST_CASE("pane canvas capture keeps the press picture through repaint motion and outside release") {
    CanvasRig t;
    t.button(1, true);
    REQUIRE(t.seat->pointers.size() == 1);
    const auto press = t.seat->pointers.back();
    CHECK(press.phase == canvas_pointer::kPress);
    CHECK(press.x == kPaneCanvasUnit); CHECK(press.y == kPaneCanvasUnit);
    t.publish(2);
    t.r.publish(loom::to_value(input::PointerMoved{-5, -3, 0, 0, input::space::kCells, 0}));
    REQUIRE(t.seat->pointers.size() == 2);
    const auto move = t.seat->pointers.back();
    CHECK(move.phase == canvas_pointer::kMove);
    CHECK(move.x < 0); CHECK(move.y < 0);
    CHECK(move.gesture == press.gesture); CHECK(move.picture == 1);
    t.r.publish(loom::to_value(input::PointerButton{1, false, -9, -8, input::space::kCells, 0}));
    REQUIRE(t.seat->pointers.size() == 3);
    CHECK(t.seat->pointers.back().phase == canvas_pointer::kRelease);
    CHECK(t.seat->pointers.back().gesture == press.gesture);
    CHECK(t.seat->pointers.back().picture == 1);
    t.r.publish(loom::to_value(input::PointerMoved{0, 0, 0, 0, input::space::kCells, 0}));
    CHECK(t.seat->pointers.size() == 3);
}

TEST_CASE("a canvas that accepts the hover door hears where the pointer rests and that it left; resting selects, focuses and presses nothing") {
    constexpr auto unit = kPaneCanvasUnit;
    CanvasRig t;
    const auto selected = t.r.session().panes.selected;
    const auto keyboard = t.r.session().panes.keyboard;
    t.move(unit, unit);
    REQUIRE(t.seat->hovers.size() == 1);
    const auto first = t.seat->hovers.back();
    CHECK(first.over);
    CHECK_FALSE(first.carrying);
    CHECK(first.pane == canvas_pane);
    CHECK(first.grant == t.view().canvas.grant);
    CHECK(first.picture == t.view().stamp.aimed);
    CHECK(first.x == unit);
    CHECK(first.y == unit);
    // The same place again says nothing new; another place does.
    t.move(unit, unit);
    CHECK(t.seat->hovers.size() == 1);
    t.move(3 * unit, unit);
    REQUIRE(t.seat->hovers.size() == 2);
    CHECK(t.seat->hovers.back().x == 3 * unit);
    // RESTING MOVES NOTHING: no selection, no keyboard, no pointer gesture.
    CHECK(t.r.session().panes.selected == selected);
    CHECK(t.r.session().panes.keyboard == keyboard);
    CHECK(t.seat->pointers.empty());
    // Off the canvas, the pane is told the pointer left it, once.
    t.move(-2 * unit, -2 * unit);
    REQUIRE(t.seat->hovers.size() == 3);
    CHECK_FALSE(t.seat->hovers.back().over);
    t.move(-3 * unit, -2 * unit);
    CHECK(t.seat->hovers.size() == 3);
    // A press owns the pointer: the hover is put down, and comes back after the release.
    t.move(unit, unit);
    REQUIRE(t.seat->hovers.size() == 4);
    t.button(1, true);
    REQUIRE(t.seat->hovers.size() == 5); // put down by the press itself, before any motion
    CHECK_FALSE(t.seat->hovers.back().over);
    t.move(2 * unit, unit);
    CHECK(t.seat->pointers.back().phase == canvas_pointer::kMove);
    CHECK(t.seat->hovers.size() == 5);
    t.button(1, false, 2 * unit, unit);
    t.move(unit, 2 * unit);
    REQUIRE(t.seat->hovers.size() == 6);
    CHECK(t.seat->hovers.back().over);
    // A menu over the desk puts it down too.
    t.seat->pass_right = true;
    t.button(3, true);
    REQUIRE(t.r.session().context.open);
    t.move(2 * unit, 2 * unit);
    REQUIRE(t.seat->hovers.size() == 7);
    CHECK_FALSE(t.seat->hovers.back().over);
    t.button(3, false);
    t.r.key(input::scan::kEscape);
    REQUIRE_FALSE(t.r.session().context.open);
    t.move(unit, unit);
    REQUIRE(t.seat->hovers.size() == 8);
    // A FRESH ROOM PUTS THE HOVER DOWN WITH IT: the provider is told nothing more under the old
    // grant, and the next motion names the new one.
    const auto old = t.view().canvas.grant;
    t.drive([](CanvasSeat& s, loom::Mail& m) { s.offer(m); });
    t.publish();
    CHECK(t.seat->hovers.size() == 8);
    t.move(unit, unit);
    REQUIRE(t.seat->hovers.size() == 9);
    CHECK(t.seat->hovers.back().over);
    CHECK(t.seat->hovers.back().grant != old);
}

TEST_CASE("a right press a canvas picture hands back opens the host's pane menu at the press; one it keeps opens nothing") {
    CanvasRig t;
    // KEPT: the picture took the press, and the press is its own.
    t.button(3, true);
    REQUIRE(t.seat->pointers.size() == 1);
    CHECK(t.seat->pointers.back().button == 3);
    CHECK_FALSE(t.r.session().context.open);
    t.button(3, false);
    // HANDED BACK: the host's pane menu opens about the canvas pane, beside the hand.
    t.seat->pass_right = true;
    t.button(3, true);
    REQUIRE(t.r.session().context.open);
    CHECK(t.r.session().context.subject == context_subject::kPane);
    CHECK(t.r.session().context.pane == (PaneRef{canvas_office, canvas_pane}));
    CHECK(t.r.session().context.anchored);
    CHECK(surface::cell_of_pixel(t.r.session().context.anchor_x) == (t.view().canvas.x + kPaneCanvasUnit) / kPaneCanvasUnit);
    CHECK(surface::cell_of_pixel(t.r.session().context.anchor_y) == (t.view().canvas.y + kPaneCanvasUnit) / kPaneCanvasUnit);
}

TEST_CASE("pane canvas grants turn over on reoffer and old content and capture cannot survive") {
    CanvasRig t;
    const auto old = t.picture(8);
    t.button(2, true);
    REQUIRE(t.seat->pointers.size() == 1);
    t.drive([](CanvasSeat& s, loom::Mail& m) { s.offer(m); });
    REQUIRE(t.seat->pointers.size() == 2);
    CHECK(t.seat->pointers.back().phase == canvas_pointer::kLost);
    CHECK(t.seat->rooms.back().grant != old.grant);
    CHECK_FALSE(t.view().canvas.heard);
    CHECK_FALSE(t.view().canvas.preview);
    t.drive([old](CanvasSeat&, loom::Mail& m) {
        (void)m.as_role(canvas_office).send_to_role(kWorkshopProvider, old);
    });
    REQUIRE(!t.seat->rejected.empty());
    CHECK_FALSE(t.view().canvas.heard);
    t.publish();
    t.button(2, false);
    CHECK(t.seat->pointers.size() == 2);
}

TEST_CASE("pane canvas provider replacement cannot acquire its predecessor's held gesture") {
    CanvasRig t;
    SUBCASE("after the predecessor received its press") {
        t.button(1, true);
        REQUIRE(t.seat->pointers.size() == 1);
    }
    SUBCASE("after host custody but before the press is delivered") {
        const auto c = t.view().canvas;
        (void)t.r.bus.publish(loom::Message(loom::to_value(input::PointerButton{1, true,
            c.x / kPaneCanvasUnit + 1, c.y / kPaneCanvasUnit + surface::kTuiCanvasTopRow + 1,
            input::space::kCells, 0}), {}, {}, 0));
        REQUIRE(t.r.bus.pump_pending() >= 1);
        REQUIRE(t.seat->pointers.empty());
    }
    auto old = t.r.bus.unregister_weave(t.id);
    REQUIRE(old);
    t.mount();
    t.drive([](CanvasSeat& s, loom::Mail& m) { s.offer(m); });
    REQUIRE(!t.seat->rooms.empty());
    CHECK_FALSE(t.view().canvas.preview);
    t.publish();
    t.button(1, false);
    CHECK(t.seat->pointers.empty());
    t.button(1, true);
    REQUIRE(t.seat->pointers.size() == 1);
    CHECK(t.seat->pointers.back().phase == canvas_pointer::kPress);
}

TEST_CASE("pane canvas resize loses capture and wheel names a local point in the latest picture") {
    CanvasRig t;
    t.button(3, true);
    const auto old = t.seat->rooms.back().grant;
    t.r.extent_on_window(150, 65);
    REQUIRE(t.seat->pointers.size() == 2);
    CHECK(t.seat->pointers.back().phase == canvas_pointer::kLost);
    REQUIRE(!t.seat->rooms.empty());
    CHECK(t.seat->rooms.back().grant != old);
    CHECK(t.seat->rooms.back().grain == surface::kPixelGrainPx);
    CHECK(t.seat->rooms.back().graphical);
    t.publish();
    const auto c = t.view().canvas;
    t.r.publish(loom::to_value(input::PointerWheel{0, -1,
        (c.x + kPaneCanvasUnit) / surface::kPixelGrainPx,
        (c.y + kPaneCanvasUnit) / surface::kPixelGrainPx, input::space::kPixels, input::mod::kCtrl}));
    REQUIRE(t.seat->pointers.size() == 3);
    const auto wheel = t.seat->pointers.back();
    CHECK(wheel.phase == canvas_pointer::kWheel);
    CHECK(wheel.x == kPaneCanvasUnit); CHECK(wheel.y == kPaneCanvasUnit);
    CHECK(wheel.dy == -1); CHECK(wheel.modifiers == input::mod::kCtrl);
    CHECK(wheel.picture == 1);
}

TEST_CASE("pane canvas measured text admission bounds all bytes and its editing positions") {
    PaneCanvasContent c{canvas_pane, 1, 1, {}, {},
        {{0, 0, "ABCD", surface::role::kFill, 2, 1, 3}}};
    CHECK(canvas_content_problem(c).empty());
    c.texts[0].caret_col = 5;
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.texts[0].caret_col = 4;
    c.texts[0].sel_end_col = 5;
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.texts[0].sel_end_col = 0;
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.texts[0].sel_begin_col = -1;
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.texts[0] = {0, 0, "\xC3\xA9", surface::role::kFill};
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.texts[0].text = "line\nbreak";
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.texts[0].text.assign(kPaneCanvasMaxTextRunBytes, 'x');
    CHECK(canvas_content_problem(c).empty());
    c.texts[0].text += 'x';
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.texts.assign(kPaneCanvasMaxTexts + 1, PaneCanvasText{});
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.texts.assign(kPaneCanvasMaxTexts, PaneCanvasText{});
    CHECK(canvas_content_problem(c).empty());
    c.labels.assign(kPaneCanvasMaxTextBytes / kPaneCanvasMaxLabelBytes,
                    PaneCanvasLabel{0, 0, std::string(kPaneCanvasMaxLabelBytes, 'a'), 0});
    c.texts.assign(1, PaneCanvasText{0, 0, "x", surface::role::kFill});
    CHECK_FALSE(canvas_content_problem(c).empty());
    c.labels.back().text.pop_back();
    CHECK(canvas_content_problem(c).empty());
    c.texts[0].role = 99;
    CHECK_FALSE(canvas_content_problem(c).empty());
}

TEST_CASE("pane canvas measured text shares its fit with existing surface type and preserves labels") {
    const PaneCanvasRoom room{canvas_pane, 1, 20 * kPaneCanvasUnit, 10 * kPaneCanvasUnit,
                              1, true, 8, 18};
    // THE MEDIUM'S OWN METRIC, in the canvas's own pixels: nothing to convert.
    const auto metric = canvas_text_metrics(room);
    CHECK(metric.advance == 8);
    CHECK(metric.line == 18);
    CHECK(metric.inset == surface::kTextInsetPx);
    CHECK(metric.graphical);
    const PaneCanvasText text{6, 4, "ABCD", surface::role::kAccent, 2, 1, 3};
    const auto placed = clip_canvas_text(text, {0, 0, room.width, room.height}, room);
    REQUIRE(placed.visible());
    CHECK(placed.bounds.x == 6);
    CHECK(placed.bounds.y == 4);
    CHECK(placed.bounds.w == 4 * 8 + 2 * surface::kTextInsetPx);
    CHECK(placed.bounds.h == 18 + 2 * surface::kTextInsetPx);
    CHECK(placed.fit.columns == 4);
    CHECK(placed.fit.rows == 1);
    PaneCanvasContent c{canvas_pane, 1, 1, {}, {{0, 0, "old", surface::role::kFill}}, {text}};
    surface::SurfaceLayer layer;
    const PixelRect body{101, 93, room.width, room.height};
    paint_pane_canvas(layer, body, c, room.text_advance_px, room.text_line_px, room.grain);
    REQUIRE(layer.labels.size() == 1);
    CHECK(layer.labels[0].text == "old");
    REQUIRE(layer.texts.size() == 1);
    const auto& region = layer.texts[0];
    CHECK(region.ground == surface::kGroundBeneath);
    REQUIRE(region.rows.size() == 1);
    CHECK(region.rows[0].background == surface::role::kNone);
    CHECK(region.rows[0].text == "ABCD");
    CHECK(region.caret_row == 0);
    CHECK(region.caret_col == 2);
    CHECK(region.sel_begin_col == 1);
    CHECK(region.sel_end_col == 3);
    const auto fit = surface::fit_region(region, surface::SurfaceExtent{cells_px(0), cells_px(0), 8, 18, 12});
    CHECK(fit.columns == placed.fit.columns);
    CHECK(fit.rows == placed.fit.rows);
    CHECK(fit.view.x == placed.fit.view.x + 101);
    CHECK(fit.view.y == placed.fit.view.y + 93);
    CHECK(surface::prose_column_of_pixel(fit.view.x + fit.origin_x + 2 * fit.advance_px, fit) ==
          2);
    CHECK(PaneCanvasRoom::zen_version == 3);
    CHECK(PaneCanvasContent::zen_version == 3);
    CHECK(surface::SurfaceTextRegion::zen_version == 7);
    CHECK(surface::SurfaceCanvas::zen_version == 9);
}

TEST_CASE("pane canvas text clipping preserves surviving positions through both edges") {
    PaneCanvasRoom room{canvas_pane, 1, 36, 44, 1, true, 8, 18};
    const PaneCanvasText source{-5, 1, "ABCDE", surface::role::kFill, 2, 0, 5};
    const auto text = clip_canvas_text(source, {0, 0, room.width, room.height}, room);
    REQUIRE(text.visible());
    CHECK(text.first_column == 1);
    CHECK(text.text.text == "BCD");
    CHECK(text.bounds.x == 3);  // original -5 plus one 8-pixel advance
    CHECK(text.bounds.w == 28); // three advances plus both insets
    CHECK(text.text.caret_col == 1);
    CHECK(text.text.sel_begin_col == 0);
    CHECK(text.text.sel_end_col == 3);
    CHECK(text.fit.columns == 3);
    CHECK_FALSE(clip_canvas_text(source, {0, 2, room.width, room.height - 2}, room).visible());
    const auto empty_caret = clip_canvas_text({0, 0, "", surface::role::kFill, 0},
                                               {0, 0, room.width, room.height}, room);
    REQUIRE(empty_caret.visible());
    CHECK(empty_caret.fit.graphical());
    CHECK(empty_caret.fit.columns == 1);
    CHECK(canvas_text_region(empty_caret).caret_col == 0);
    for (const auto& metrics : {std::pair<std::int64_t, std::int64_t>{8, 18}, {11, 19}, {0, 0}}) {
        room.text_advance_px = metrics.first;
        room.text_line_px = metrics.second;
        for (std::int64_t x = -48; x <= 48; x += 2) {
            for (std::int64_t y = -12; y <= 44; y += 3) {
                auto candidate = source;
                candidate.x = x; candidate.y = y;
                const auto clipped = clip_canvas_text(candidate, {2, 2, 31, 37}, room);
                if (!clipped.visible()) continue;
                CHECK(clipped.bounds.x >= 2);
                CHECK(clipped.bounds.y >= 2);
                CHECK(clipped.bounds.x + clipped.bounds.w <= 33);
                CHECK(clipped.bounds.y + clipped.bounds.h <= 39);
                CHECK(clipped.fit.columns > 0);
                CHECK(clipped.fit.rows == 1);
            }
        }
    }
    const auto lo = (std::numeric_limits<std::int64_t>::min)();
    const auto hi = (std::numeric_limits<std::int64_t>::max)();
    for (const auto x : {lo, lo + 1, hi - 1, hi}) {
        auto distant = source; distant.x = x;
        CHECK_FALSE(clip_canvas_text(distant, {0, 0, room.width, room.height}, room).visible());
    }
    room.text_advance_px = hi; room.text_line_px = hi; room.grain = hi;
    CHECK_FALSE(clip_canvas_text(source, {0, 0, room.width, room.height}, room).visible());
    room.text_advance_px = lo; room.text_line_px = 0; room.grain = lo;
    CHECK(canvas_text_metrics(room).advance == kPaneCanvasUnit);
    CHECK(canvas_text_metrics(room).grain == kPaneCanvasUnit);
}

TEST_CASE("pane canvas cell text cropping accounts for the inserted caret and preserves its suffix") {
    const PaneCanvasRoom room{canvas_pane, 1, 3 * kPaneCanvasUnit, 2 * kPaneCanvasUnit,
                              kPaneCanvasUnit, false, 0, 0};
    const auto placed = clip_canvas_text({-2 * kPaneCanvasUnit, 0, "ABCD", 0, 1, 1, 4},
                                         {0, 0, room.width, room.height}, room);
    REQUIRE(placed.visible());
    CHECK(placed.first_column == 1);
    CHECK(placed.text.text == "BCD");
    CHECK(placed.text.caret_col == surface::kNoCaret);
    CHECK(placed.text.sel_begin_col == 0);
    CHECK(placed.text.sel_end_col == 3);
    CHECK(placed.bounds.x == 0);
    surface::SurfaceLayer layer;
    layer.texts.push_back(canvas_text_region(placed));
    auto rows = surface::project_text_regions(layer);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].label.text == "BCD");
    const auto at_start = clip_canvas_text({-kPaneCanvasUnit, 0, "ABCD", 0, 1, 1, 4},
                                           {0, 0, room.width, room.height}, room);
    REQUIRE(at_start.visible());
    CHECK(at_start.text.text == "BC");
    CHECK(at_start.text.caret_col == 0);
    layer.texts[0] = canvas_text_region(at_start);
    rows = surface::project_text_regions(layer);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].label.text == "_BC");
}

TEST_CASE("pane canvas text metric changes renew the grant even when its body stays fixed") {
    CanvasRig t;
    t.r.extent_on_window(150, 65);
    t.publish();
    const auto first = t.seat->rooms.back();
    CHECK(first.text_advance_px == 8);
    CHECK(first.text_line_px == 18);
    const auto room_count = t.seat->rooms.size();
    t.button(1, true);
    REQUIRE(t.seat->pointers.back().phase == canvas_pointer::kPress);
    t.r.extent(150, 65, 9, 18, surface::kCanvasCellPx);
    REQUIRE(t.seat->rooms.size() == room_count + 1);
    const auto next = t.seat->rooms.back();
    CHECK(next.width == first.width);
    CHECK(next.height == first.height);
    CHECK(next.grant != first.grant);
    CHECK(next.text_advance_px == 9);
    CHECK(next.text_line_px == 18);
    CHECK_FALSE(t.view().canvas.heard);
    CHECK_FALSE(t.view().canvas.preview);
    CHECK(t.view().stamp.aimed == 0);
    CHECK(t.seat->pointers.back().phase == canvas_pointer::kLost);
}

TEST_CASE("pane canvas resize preview keeps only the same provider's picture and never its input") {
    CanvasRig t;
    const auto old_room = t.seat->rooms.back();
    const auto old_picture = t.view().canvas.content;
    t.button(1, true);
    auto* authored = pane_of(t.r.session().setup.active, PaneRef{canvas_office, canvas_pane});
    REQUIRE(authored);
    authored->width = PaneSize{pane_unit::kPixels, 20 * kPaneCanvasUnit};
    t.r.key(input::scan::kUnknown);
    REQUIRE(t.seat->rooms.back().grant != old_room.grant);
    CHECK(t.seat->rooms.back().width != old_room.width);
    CHECK(t.view().canvas.preview);
    CHECK_FALSE(t.view().canvas.heard);
    CHECK(t.view().stamp.aimed == 0);
    CHECK(t.view().canvas.content.grant == old_picture.grant);
    CHECK(t.seat->pointers.back().phase == canvas_pointer::kLost);
    bool old_words = false, updating = false;
    for (const auto& label : all_labels(t.r.last_canvas()))
        if (label.text == "node") old_words = true;
    for (const auto& region : all_texts(t.r.last_canvas()))
        for (const auto& row : region.rows)
            if (row.text.find("(updating)") != std::string::npos) updating = true;
    CHECK(old_words);
    CHECK(updating);
    const auto pointers = t.seat->pointers.size();
    t.button(1, true);
    CHECK(t.seat->pointers.size() == pointers);
    auto bad = t.picture(); bad.rects[0].w = -1;
    t.drive([bad](CanvasSeat&, loom::Mail& m) {
        (void)m.as_role(canvas_office).send_to_role(kWorkshopProvider, bad);
    });
    CHECK(t.view().canvas.preview);
    CHECK_FALSE(t.view().canvas.heard);
    t.publish();
    CHECK_FALSE(t.view().canvas.preview);
    CHECK(t.view().canvas.heard);
    authored = pane_of(t.r.session().setup.active, PaneRef{canvas_office, canvas_pane});
    REQUIRE(authored);
    authored->width = PaneSize{pane_unit::kPixels, 22 * kPaneCanvasUnit};
    t.r.key(input::scan::kUnknown);
    REQUIRE(t.view().canvas.preview);
    const auto closing_grant = t.view().canvas.grant;
    t.r.pick(PaneRef{canvas_office, canvas_pane});
    CHECK(t.r.session().panes.external_pane(t.kind) == nullptr);
    t.r.pick(PaneRef{canvas_office, canvas_pane});
    CHECK_FALSE(t.view().canvas.preview);
    CHECK_FALSE(t.view().canvas.heard);
    CHECK(t.view().canvas.content.labels.empty());
    CHECK(t.view().canvas.grant != closing_grant);
}

namespace {
/// Asks the view host as a builder would, and keeps its answers.
class ViewAsker : public loom::WeaveBase<ViewAsker, SeatState, loom::Accept<zengine::view::ViewAnswer>,
                                         loom::Emit<zengine::view::ViewRun, zengine::view::ViewStop>> {
public:
    std::vector<zengine::view::ViewAnswer> answers;
    void on(const zengine::view::ViewAnswer& a, loom::Mail&) { answers.push_back(a); }
};
}

TEST_CASE("a described view of the greatest size its rules allow asks for a pane Workshop admits, and is seated and drawn cut to it") {
    namespace view = zengine::view;
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    r.extent(150, 65);
    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);

    view::Description d;
    d.name = "wide.panel";
    d.width = view::kMaxSizePx;
    d.height = view::kMaxSizePx;
    d.elements = {{"far", view::Kind::label, "Far", view::kMaxPixels, view::kMaxPixels, 192, 24, ""},
                  {"near", view::Kind::label, "Near", 0, 0, 192, 24, ""}};
    const auto asked = view::offered(d);
    CHECK(asked.width == kMaxPaneBodyPx);
    CHECK(asked.height == kMaxPaneBodyPx);
    const auto bytes = view::description_bytes(d);
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewRun{"builder", loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
    r.bus.drain_until_idle();
    REQUIRE(client->answers.size() == 1);
    REQUIRE_MESSAGE(client->answers[0].ok, client->answers[0].reason);
    // ADMITTED AND SEATED: the catalog lists it, the desk holds it, and it drew there.
    const auto* row = r.session().panes.runtime.find("wide.panel", view::kPane);
    REQUIRE(row);
    CHECK(r.session().panes.has(row->kind));
    auto* pane = r.session().panes.external_pane(row->kind);
    REQUIRE(pane);
    REQUIRE(pane->canvas.heard);
    std::string words;
    for (const auto& t : pane->canvas.content.texts) words += t.text + "|";
    CHECK(words.find("Near|") != std::string::npos);
    CHECK(words.find("Far|") == std::string::npos); // past the pane, cut
}

TEST_CASE("a view asks for its size in pixels and its pane grants exactly that room: to the pixel in a window, to the cells that hold it in a terminal") {
    namespace view = zengine::view;
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    r.extent(160, 90, 8, 18, surface::kCanvasCellPx); // the shipped window's face
    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);

    struct Size { const char* name; std::int64_t w, h; };
    for (const Size& size : {Size{"even.view", 680, 360}, Size{"odd.view", 683, 361}}) {
        const std::string named = size.name;
        CAPTURE(named);
        view::Description d;
        d.name = size.name;
        d.width = size.w;
        d.height = size.h;
        d.elements = {{"note", view::Kind::label, "Note", 0, 0, 96, 24, ""}};
        const auto bytes = view::description_bytes(d);
        (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
            loom::Message(loom::to_value(view::ViewRun{size.name, loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
        r.bus.drain_until_idle();
        REQUIRE_FALSE(client->answers.empty());
        REQUIRE_MESSAGE(client->answers.back().ok, client->answers.back().reason);
        const auto* row = r.session().panes.runtime.find(size.name, view::kPane);
        REQUIRE(row);
        REQUIRE(r.session().panes.has(row->kind));

        // IN THE WINDOW: the size to the pixel, and the notice's three rows of the face beneath it.
        const auto band = 3 * (18 + 2 * surface::kTextInsetPx);
        const auto* pane = r.session().panes.external_pane(row->kind);
        REQUIRE(pane);
        CHECK(pane->canvas.width == size.w);
        CHECK(pane->canvas.height == size.h + band);
        CHECK(pane->canvas.grain == 1);
    }

    // IN A TERMINAL, the same panes derived: each size rounded up to the whole cells that hold
    // it, and three rows of cells beneath.
    r.extent(160, 90);
    for (const Size& size : {Size{"even.view", 680, 360}, Size{"odd.view", 683, 361}}) {
        const std::string named = size.name;
        CAPTURE(named);
        const auto* row = r.session().panes.runtime.find(size.name, view::kPane);
        REQUIRE(row);
        const auto* pane = r.session().panes.external_pane(row->kind);
        REQUIRE(pane);
        const auto up = [](std::int64_t px) { return (px + 11) / 12 * 12; };
        CHECK(pane->canvas.width == up(size.w));
        CHECK(pane->canvas.height == up(size.h) + 3 * surface::kCanvasCellPx);
        CHECK(pane->canvas.grain == surface::kCanvasCellPx);
    }
}

TEST_CASE("a view run when the stack's column is spent is shown at the column's top, in front") {
    // ⚔ MUTATION: the view's ask to be shown judged by a stack rationed to its column's height --
    // refused for room, as a view run from the View Builder beside the Pane Manager was.
    namespace view = zengine::view;
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t hello = r.session().panes.runtime.entries[0].kind;
    REQUIRE(r.session().panes.has(hello));
    const Screen sc = screen_of(r.session());
    REQUIRE(stack_slots_that_fit(sc) == 1);
    const PixelRect first = bounds_of(r.session().panes, r.session().setup.active, hello, sc).rect;

    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);
    view::Description d;
    d.name = "greeting";
    d.width = 192;
    d.height = 48;
    d.elements = {{"hello", view::Kind::label, "Hello", 0, 0, 192, 24, ""}};
    const auto bytes = view::description_bytes(d);
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewRun{"builder", loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
    r.bus.drain_until_idle();
    REQUIRE(client->answers.size() == 1);
    REQUIRE_MESSAGE(client->answers[0].ok, client->answers[0].reason);
    // SHOWN: the column begins again at its top, the view stands there in front of the pane it
    // covers, and that pane did not move.
    const auto* row = r.session().panes.runtime.find("greeting", view::kPane);
    REQUIRE(row);
    REQUIRE(r.session().panes.has(row->kind));
    CHECK(has_pane(r.session().setup.active, PaneRef{"greeting", view::kPane}));
    const PixelRect landed = bounds_of(r.session().panes, r.session().setup.active, row->kind, sc).rect;
    CHECK(landed.x == first.x);
    CHECK(landed.y == sc.room_y);
    CHECK(effective_pane_order(r.session().setup.active, r.session().panes).back() == row->kind);
    CHECK(bounds_of(r.session().panes, r.session().setup.active, hello, sc).rect == first);
    CHECK(r.last_notice().find("showing greeting") != std::string::npos);
}

TEST_CASE("a view shown where its own place stands off this screen stays there, and the band says so and how to bring it back") {
    // ⚔ MUTATION: the reveal saying the view is shown and has the keys, of a pane no one can see.
    namespace view = zengine::view;
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    // THE VIEW'S ROW ON THE DESK BEFORE IT RUNS, its own place past the room's right edge.
    const PaneRef greeting{"greeting", view::kPane};
    REQUIRE(add_pane(r.session().setup.active, greeting));
    REQUIRE(author_pane_place(r.session().setup.active, greeting, cells_px(300), 0).accepted);
    const PanePlace authored = pane_of(r.session().setup.active, greeting)->place;

    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);
    view::Description d;
    d.name = "greeting";
    d.width = 192;
    d.height = 48;
    d.elements = {{"hello", view::Kind::label, "Hello", 0, 0, 192, 24, ""}};
    const auto bytes = view::description_bytes(d);
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewRun{"builder", loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
    r.bus.drain_until_idle();
    REQUIRE(client->answers.size() == 1);
    REQUIRE_MESSAGE(client->answers[0].ok, client->answers[0].reason);
    // SEATED WHERE IT WAS PUT, OFF THIS SCREEN, and said so: never "showing ... it has the keys".
    const auto* row = r.session().panes.runtime.find("greeting", view::kPane);
    REQUIRE(row);
    REQUIRE(r.session().panes.has(row->kind));
    CHECK(pane_of(r.session().setup.active, greeting)->place == authored);
    CHECK(bounds_of(r.session().panes, r.session().setup.active, row->kind, screen_of(r.session()))
              .rect.empty());
    CHECK(r.last_notice() == "greeting is off this screen -- Reset > place brings it back (the "
                             "Pane Manager's manage greeting > reaches it), or hide it and show it "
                             "again");
    CHECK(r.session().notice_is_bad);
}

TEST_CASE("a described view offers its own pane through the view host, Workshop seats and draws it, a press reaches it, and a stop leaves a picture that says so") {
    namespace view = zengine::view;
    PaneRig r;
    r.mount_workshop();
    r.host.role_holder = [&r](std::string_view office) { return r.bus.role_holder(office); };
    r.ready();
    r.extent(150, 65);
    view::Host views(r.bus);
    views.mount();
    auto asker = std::make_unique<ViewAsker>();
    auto* client = asker.get();
    loom::Grant asking;
    view::allow_view_requests(asking);
    const auto client_id = r.bus.register_weave(std::move(asker), std::move(asking));
    client->zen_set_self(client_id);

    view::Description d;
    d.name = "tally.panel";
    d.width = 480;
    d.height = 120;
    d.elements = {{"step", view::Kind::number, "step", 0, 0, 144, 24, "1"},
                  {"count", view::Kind::button, "Count", 0, 28, 96, 24, ""},
                  {"total", view::Kind::label, "Total", 0, 56, 192, 24, ""}};
    const auto said = loom::SchemaBuilder("tally.panel.Count", 1).field("step", loom::Kind::Int).build();
    d.intents = {{"count", said, {{"step", "step"}}}};
    d.shows = {{"total", loom::SchemaBuilder("tally.Total", 1).field("total", loom::Kind::Int).build(), "total"}};
    const auto bytes = view::description_bytes(d);
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewRun{"builder", loom::Bytes(bytes.begin(), bytes.end())}), client_id, {}, 1));
    r.bus.drain_until_idle();
    REQUIRE(client->answers.size() == 1);
    REQUIRE_MESSAGE(client->answers[0].ok, client->answers[0].reason);

    // Offered as its own office, and seated on the desk by its own ask.
    const auto* row = r.session().panes.runtime.find("tally.panel", view::kPane);
    REQUIRE(row);
    CHECK(row->name == "tally.panel");
    CHECK(r.session().panes.has(row->kind));
    auto* pane = r.session().panes.external_pane(row->kind);
    REQUIRE(pane);
    CHECK(pane->canvas.owner == r.bus.role_holder("tally.panel"));
    REQUIRE(pane->canvas.heard);
    const auto words = [&] {
        std::string out;
        for (const auto& t : r.session().panes.external_pane(row->kind)->canvas.content.texts) out += t.text + "|";
        return out;
    };
    CHECK(words().find("Total: waiting|") != std::string::npos);

    // A press on Count, through Workshop's own pointer route, is the view's to use.
    const auto c = pane->canvas;
    const auto at = [&](std::int64_t px_x, std::int64_t px_y) {
        r.publish(loom::to_value(input::PointerButton{1, true,
            (c.x + px_x) / kPaneCanvasUnit,
            (c.y + px_y) / kPaneCanvasUnit + surface::kTuiCanvasTopRow,
            input::space::kCells, input::mod::kNone}));
        r.publish(loom::to_value(input::PointerButton{1, false,
            (c.x + px_x) / kPaneCanvasUnit,
            (c.y + px_y) / kPaneCanvasUnit + surface::kTuiCanvasTopRow,
            input::space::kCells, input::mod::kNone}));
    };
    CHECK(words().find("Count|") != std::string::npos);
    // A view has no hover door: resting over it tells it nothing.
    std::size_t hovers = 0;
    const auto tap = r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.schema_name == PaneCanvasHover::zen_name) ++hovers;
    });
    r.publish(loom::to_value(input::PointerMoved{(c.x + 12) / kPaneCanvasUnit,
        (c.y + 36) / kPaneCanvasUnit + surface::kTuiCanvasTopRow, 0, 0,
        input::space::kCells, input::mod::kNone}));
    r.bus.remove_observer(tap);
    CHECK(hovers == 0);
    at(12, 36);
    // The notice may wrap at the pane's width: read it as one sentence.
    std::string sentence;
    for (const auto& t : r.session().panes.external_pane(row->kind)->canvas.content.texts) sentence += t.text + " ";
    CHECK(sentence.find("said tally.panel.Count; nothing accepts it") != std::string::npos);

    // Stopped: the last picture Workshop holds says so, and no control is left looking live.
    (void)r.bus.send_as_to_role(client_id, view::kViewHostRole,
        loom::Message(loom::to_value(view::ViewStop{"builder"}), client_id, {}, 2));
    r.bus.drain_until_idle();
    REQUIRE(client->answers.size() == 2);
    CHECK(client->answers[1].ok);
    CHECK_FALSE(r.bus.role_holder("tally.panel").valid());
    const auto* stopped = r.session().panes.external_pane(row->kind);
    REQUIRE(stopped);
    CHECK(words().find("tally.panel stopped") == 0);
    CHECK(stopped->canvas.content.rects.size() == 1); // its ground alone
    // ...and once Workshop repaints and sees the provider gone, the pane waits for one: neither
    // picture leaves a field or a button that looks live.
    r.key(input::scan::kUnknown);
    const auto* waiting = r.session().panes.external_pane(row->kind);
    REQUIRE(waiting);
    CHECK(waiting->canvas.grant == 0);
    CHECK(waiting->canvas.content.texts.empty());
    CHECK(waiting->canvas.content.rects.empty());
}
