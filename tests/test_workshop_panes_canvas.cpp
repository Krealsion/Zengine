// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- a pane's own canvas picture: admission, clipping, capture, grants,
// and measured text, through a native provider seat.

#include "doctest.h"
#include "workshop_support.hpp"
#include "workshop/screen_canvas.hpp"
#include <limits>

namespace {
constexpr const char* canvas_office = "zengine.test.canvas";
constexpr const char* canvas_pane = "diagram";

class CanvasSeat : public loom::WeaveBase<CanvasSeat, SeatState,
    loom::Accept<PaneCatalogRequested, PaneRoom, PaneCanvasRoom, PaneCanvasPointer,
                 PaneCanvasRejected, SeatDo>,
    loom::Emit<PaneOffered, PaneCanvasContent, PaneContent>> {
public:
    std::vector<PaneCanvasRoom> rooms;
    std::vector<PaneCanvasPointer> pointers;
    std::vector<PaneCanvasRejected> rejected;
    std::function<void(CanvasSeat&, loom::Mail&)> next;
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const PaneRoom&, loom::Mail&) {}
    void on(const PaneCanvasRoom& r, loom::Mail& m) {
        REQUIRE(m.authored_from_role(kWorkshopProvider));
        rooms.push_back(r);
    }
    void on(const PaneCanvasPointer& e, loom::Mail& m) {
        REQUIRE(m.authored_from_role(kWorkshopProvider));
        pointers.push_back(e);
    }
    void on(const PaneCanvasRejected& r, loom::Mail&) { rejected.push_back(r); }
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
        const auto* row = r.session().panels.runtime.find(canvas_office, canvas_pane);
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
        auto* p = r.session().panels.external_pane(kind); REQUIRE(p); return *p;
    }
    void button(std::int64_t button, bool down, std::int64_t x = kPaneCanvasUnit,
                std::int64_t y = kPaneCanvasUnit) {
        const auto c = view().canvas;
        r.publish(loom::to_value(input::PointerButton{button, down,
            (c.x + x) / kPaneCanvasUnit,
            (c.y + y) / kPaneCanvasUnit + surface::kTuiCanvasTopRow,
            input::space::kCells, input::mod::kNone}));
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
    const FineRect body{7 * unit, 11 * unit, 4 * unit, 3 * unit};
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
    CHECK(clipped == FineRect{0, 0, body.w, body.h});
    PaneCanvasContent c{canvas_pane, 1, 1,
        {{-unit, -unit, 3 * unit, 3 * unit, surface::role::kAccent}},
        {{-unit, 0, "ABCDE", 0}, {0, -1, "hidden", 0}, {0, body.h - unit + 1, "hidden", 0}}};
    surface::SurfaceLayer layer;
    paint_pane_canvas(layer, body, c);
    REQUIRE(layer.rects.size() == 1);
    CHECK(layer.rects[0].x == 7); CHECK(layer.rects[0].y == 11);
    CHECK(layer.rects[0].w == 2); CHECK(layer.rects[0].h == 2);
    REQUIRE(layer.labels.size() == 1);
    CHECK(layer.labels[0].text == "BCDE");
    CHECK(layer.labels[0].x == 7); CHECK(layer.labels[0].y == 11);
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
    CHECK(t.seat->rooms.back().grain == surface::kPixelGrainSubs);
    CHECK(t.seat->rooms.back().graphical);
    t.publish();
    const auto c = t.view().canvas;
    t.r.publish(loom::to_value(input::PointerWheel{0, -1,
        (c.x + kPaneCanvasUnit) / surface::kPixelGrainSubs,
        (c.y + kPaneCanvasUnit) / surface::kPixelGrainSubs, input::space::kPixels, input::mod::kCtrl}));
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
                              4, true, 8, 18};
    const auto metric = canvas_text_metrics(room);
    CHECK(metric.advance == 32);
    CHECK(metric.line == 72);
    CHECK(metric.inset == 8);
    CHECK(metric.graphical);
    const PaneCanvasText text{24, 16, "ABCD", surface::role::kAccent, 2, 1, 3};
    const auto placed = clip_canvas_text(text, {0, 0, room.width, room.height}, room);
    REQUIRE(placed.visible());
    CHECK(placed.bounds.x == 24);
    CHECK(placed.bounds.y == 16);
    CHECK(placed.bounds.w == 144);
    CHECK(placed.bounds.h == 88);
    CHECK(placed.fit.columns == 4);
    CHECK(placed.fit.rows == 1);
    PaneCanvasContent c{canvas_pane, 1, 1, {}, {{0, 0, "old", surface::role::kFill}}, {text}};
    surface::SurfaceLayer layer;
    const FineRect body{101 * 4, 93 * 4, room.width, room.height};
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
    const auto fit = surface::fit_region(region, surface::SurfaceExtent{0, 0, 8, 18, 12});
    CHECK(fit.columns == placed.fit.columns);
    CHECK(fit.rows == placed.fit.rows);
    CHECK(fit.view.x == placed.fit.view.x + 101);
    CHECK(fit.view.y == placed.fit.view.y + 93);
    CHECK(surface::prose_column_of_pixel(fit.view.x + fit.origin_x + 2 * fit.advance_px,
                                         region.x, fit) == 2);
    CHECK(PaneCanvasRoom::zen_version == 2);
    CHECK(PaneCanvasContent::zen_version == 2);
    CHECK(surface::SurfaceTextRegion::zen_version == 6);
    CHECK(surface::SurfaceCanvas::zen_version == 8);
}

TEST_CASE("pane canvas text clipping preserves surviving positions through both edges") {
    PaneCanvasRoom room{canvas_pane, 1, 144, 176, 4, true, 8, 18};
    const PaneCanvasText source{-20, 4, "ABCDE", surface::role::kFill, 2, 0, 5};
    const auto text = clip_canvas_text(source, {0, 0, room.width, room.height}, room);
    REQUIRE(text.visible());
    CHECK(text.first_column == 1);
    CHECK(text.text.text == "BCD");
    CHECK(text.bounds.x == 12); // original -20 plus one 32-subunit advance
    CHECK(text.bounds.w == 112); // three advances plus both insets
    CHECK(text.text.caret_col == 1);
    CHECK(text.text.sel_begin_col == 0);
    CHECK(text.text.sel_end_col == 3);
    CHECK(text.fit.columns == 3);
    CHECK_FALSE(clip_canvas_text(source, {0, 8, room.width, room.height - 8}, room).visible());
    const auto empty_caret = clip_canvas_text({0, 0, "", surface::role::kFill, 0},
                                               {0, 0, room.width, room.height}, room);
    REQUIRE(empty_caret.visible());
    CHECK(empty_caret.fit.graphical());
    CHECK(empty_caret.fit.columns == 1);
    CHECK(canvas_text_region(empty_caret).caret_col == 0);
    for (const auto& metrics : {std::pair<std::int64_t, std::int64_t>{8, 18}, {11, 19}, {0, 0}}) {
        room.text_advance_px = metrics.first;
        room.text_line_px = metrics.second;
        for (std::int64_t x = -192; x <= 192; x += 7) {
            for (std::int64_t y = -48; y <= 176; y += 11) {
                auto candidate = source;
                candidate.x = x; candidate.y = y;
                const auto clipped = clip_canvas_text(candidate, {9, 7, 122, 149}, room);
                if (!clipped.visible()) continue;
                CHECK(clipped.bounds.x >= 9);
                CHECK(clipped.bounds.y >= 7);
                CHECK(clipped.bounds.x + clipped.bounds.w <= 131);
                CHECK(clipped.bounds.y + clipped.bounds.h <= 156);
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
    authored->width = PaneSize{pane_unit::kSubcells, 20 * kPaneCanvasUnit};
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
    authored->width = PaneSize{pane_unit::kSubcells, 22 * kPaneCanvasUnit};
    t.r.key(input::scan::kUnknown);
    REQUIRE(t.view().canvas.preview);
    const auto closing_grant = t.view().canvas.grant;
    t.r.pick(PaneRef{canvas_office, canvas_pane});
    CHECK(t.r.session().panels.external_pane(t.kind) == nullptr);
    t.r.pick(PaneRef{canvas_office, canvas_pane});
    CHECK_FALSE(t.view().canvas.preview);
    CHECK_FALSE(t.view().canvas.heard);
    CHECK(t.view().canvas.content.labels.empty());
    CHECK(t.view().canvas.grant != closing_grant);
}
