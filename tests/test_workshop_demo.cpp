// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The demo controls' reset and readiness waits, pane comfort's budgets and seating in both
// media, and a semantic setup's refusals before it changes the active layout.

#include "workshop_support.hpp"
#include "demo-control/control.hpp"

namespace {
namespace demo = zengine::demo;
struct DemoTestPump { ZEN_SHAPE(DemoTestPump, 1); };
class DemoHand final : public loom::WeaveBase<DemoHand, DemoTestPump,
    loom::Accept<DemoTestPump, loom::Ack, loom::Refused, demo::DemoWork, demo::DemoStatus,
                 v4::PaneContent, v5::PaneCanvasContent, PanePassRequested>,
    loom::Emit<demo::DemoServiceOpened, demo::DemoServiceClosed, demo::DemoWorkRequested,
               demo::DemoWorkFinished, demo::DemoResetRequested, demo::DemoReadyRequested,
               PanePressed, PaneRoom, PaneCanvasRoom, PaneCanvasPointer, SetupApplyRequested,
               v2::PaneOffered>> {
public:
    std::function<void(loom::Mail&)> action;
    std::vector<demo::DemoWork> work;
    std::vector<demo::DemoStatus> status;
    std::vector<v4::PaneContent> painted;
    std::vector<v5::PaneCanvasContent> pictures;
    std::vector<std::string> refused;
    int passed = 0;
    int acks = 0;
    void on(const DemoTestPump&, loom::Mail& m) { action(m); }
    void on(const loom::Ack&, loom::Mail& m) { CHECK(m.answers_ask()); ++acks; }
    void on(const loom::Refused& r, loom::Mail&) { refused.push_back(r.reason); }
    void on(const demo::DemoWork& w, loom::Mail& m) { CHECK(m.answers_ask()); work.push_back(w); }
    void on(const demo::DemoStatus& s, loom::Mail& m) { CHECK(m.answers_ask()); status.push_back(s); }
    void on(const v4::PaneContent& c, loom::Mail&) { painted.push_back(c); }
    void on(const v5::PaneCanvasContent& c, loom::Mail&) { pictures.push_back(c); }
    void on(const PanePassRequested&, loom::Mail&) { ++passed; }
};
struct DemoActor {
    loom::Switchboard& bus;
    loom::WeaveId id;
    DemoHand* hand;
    explicit DemoActor(loom::Switchboard& b, const char* role = "") : bus(b) {
        auto w = std::make_unique<DemoHand>(); hand = w.get();
        auto grant = loom::emit_default_grant(*w);
        id = bus.register_weave(std::move(w), std::move(grant), role);
        hand->zen_set_self(id);
    }
    template<class T> void say(const char* target, const T& request, const char* as = "") {
        hand->action = [&](loom::Mail& m) {
            if (*as) m.as_role(as).send_to_role(target, request);
            else m.send_to_role(target, request);
        };
        bus.send(id, loom::Message(loom::to_value(DemoTestPump{})));
        bus.drain_until_idle();
    }
};
struct DemoRig {
    loom::Switchboard bus;
    DemoActor worker{bus}, visitor{bus}, desk{bus, "zengine.workshop"};
    DemoRig() {
        auto w = std::make_unique<demo::Control>(); auto* raw = w.get();
        auto grant = loom::emit_default_grant(*w);
        raw->zen_set_self(bus.register_weave(std::move(w), std::move(grant), demo::kRole));
    }
    void ready() {
        worker.say(demo::kRole, demo::DemoServiceOpened{"Example"});
        worker.say(demo::kRole, demo::DemoWorkRequested{});
        REQUIRE(worker.hand->work.size() == 1);
        worker.say(demo::kRole, demo::DemoWorkFinished{1, true, "ready"});
    }
};
}

TEST_CASE("demo reset waits for its owner completion and preserves the service identity") {
    DemoRig r; r.ready();
    r.worker.say(demo::kRole, demo::DemoWorkRequested{});
    r.visitor.say(demo::kRole, demo::DemoResetRequested{});
    CHECK(r.visitor.hand->acks == 0);
    REQUIRE(r.worker.hand->work.size() == 2);
    CHECK(r.worker.hand->work.back().generation == 2);
    r.visitor.say(demo::kRole, demo::DemoWorkFinished{2, true, "forged"});
    r.worker.say(demo::kRole, demo::DemoWorkFinished{1, true, "stale"});
    CHECK(r.visitor.hand->acks == 0);
    r.worker.say(demo::kRole, demo::DemoWorkFinished{2, true, "finished"});
    CHECK(r.visitor.hand->acks == 1);
}

TEST_CASE("demo reset queues across the gap before the service asks again") {
    DemoRig r; r.ready();
    r.visitor.say(demo::kRole, demo::DemoResetRequested{});
    r.visitor.say(demo::kRole, demo::DemoResetRequested{});
    REQUIRE(r.visitor.hand->refused.size() == 1);
    r.worker.say(demo::kRole, demo::DemoWorkRequested{});
    REQUIRE(r.worker.hand->work.size() == 2);
    r.worker.say(demo::kRole, demo::DemoWorkFinished{2, false, "Info: operation pending"});
    REQUIRE(r.visitor.hand->refused.size() == 2);
    CHECK(r.visitor.hand->refused.back() == "Info: operation pending");
}

/// A press on the controls' canvas, at a cell of a row of the room `demo_room` grants.
PaneCanvasPointer demo_press(std::int64_t row, std::int64_t button = 1) {
    return PaneCanvasPointer{"controls", 9, 1, 1, canvas_pointer::kPress, button,
                             3 * kPaneCanvasUnit, row * kPaneCanvasUnit};
}
const PaneCanvasRoom demo_room{"controls", 9, 56 * kPaneCanvasUnit, 4 * kPaneCanvasUnit,
                               kPaneCanvasUnit, false, 0, 0};

TEST_CASE("demo button trusts Workshop and service close settles outstanding waits") {
    DemoRig r; r.ready();
    r.desk.say(demo::kRole, demo_room, "zengine.workshop");
    r.visitor.say(demo::kRole, demo_press(1));
    r.worker.say(demo::kRole, demo::DemoWorkRequested{});
    CHECK(r.worker.hand->work.size() == 1);
    r.desk.say(demo::kRole, demo_press(0), "zengine.workshop"); // the setup's name: no button
    CHECK(r.worker.hand->work.size() == 1);
    r.desk.say(demo::kRole, demo_press(1, 3), "zengine.workshop"); // handed back, not a reset
    CHECK(r.desk.hand->passed == 1);
    CHECK(r.worker.hand->work.size() == 1);
    r.desk.say(demo::kRole, demo_press(1), "zengine.workshop");
    REQUIRE(r.worker.hand->work.size() == 2);
    r.visitor.say(demo::kRole, demo::DemoReadyRequested{2});
    CHECK(r.visitor.hand->status.empty());
    r.worker.say(demo::kRole, demo::DemoServiceClosed{});
    CHECK(r.visitor.hand->refused.size() == 1);
    r.visitor.say(demo::kRole, demo::DemoResetRequested{});
    CHECK(r.visitor.hand->refused.size() == 2);
}

TEST_CASE("WL-HAND-06: the demo controls name their reset row and the state beneath it") {
    DemoRig r; r.ready();
    // ON ITS CANVAS, the picture Workshop shows: each name over the row it names.
    r.desk.say(demo::kRole, demo_room, "zengine.workshop");
    REQUIRE_FALSE(r.desk.hand->pictures.empty());
    const v5::PaneCanvasContent& drawn = r.desk.hand->pictures.back();
    CHECK(canvas_parts_problem(drawn.parts).empty());
    REQUIRE(drawn.parts.size() == 2);
    const auto run_at = [&drawn](const PaneCanvasPart& part) {
        for (const v2::PaneCanvasText& t : drawn.texts) {
            if (t.y == part.y) return t.text;
        }
        return std::string();
    };
    CHECK(drawn.parts[0].name == "control:reset");
    CHECK(run_at(drawn.parts[0]).find("Reset demo") != std::string::npos);
    CHECK(drawn.parts[1].name == "status");
    CHECK(run_at(drawn.parts[1]).rfind("ready", 0) == 0);
    // ...AND AS PROSE to a host that grants no canvas, under the same names.
    DemoRig prose; prose.ready();
    prose.desk.say(demo::kRole, PaneRoom{"controls", 4, 56}, "zengine.workshop");
    REQUIRE_FALSE(prose.desk.hand->painted.empty());
    const v4::PaneContent& said = prose.desk.hand->painted.back();
    REQUIRE(said.parts.size() == 2);
    CHECK(said.parts[0].name == "control:reset");
    CHECK(said.rows[static_cast<std::size_t>(said.parts[0].row)].text.find("Reset demo") != std::string::npos);
    CHECK(said.parts[1].name == "status");
    CHECK(row_parts_problem(said.parts, static_cast<std::int64_t>(said.rows.size()), 56).empty());
}

TEST_CASE("demo ready wait before attachment completes once after the initial preparation") {
    DemoRig r;
    r.visitor.say(demo::kRole, demo::DemoReadyRequested{1});
    CHECK(r.visitor.hand->status.empty());
    r.ready();
    REQUIRE(r.visitor.hand->status.size() == 1);
    CHECK(r.visitor.hand->status.back().state == "ready");
    r.visitor.say(demo::kRole, demo::DemoServiceOpened{"another"});
    CHECK(r.visitor.hand->refused.size() == 1);
}

TEST_CASE("pane comfort survives offer refresh and rejects malformed preferences") {
    Panes p;
    auto a = admit_pane_offer(p.runtime, "test.pane", {"list", "List", "items"}, 7, 54);
    REQUIRE(a.written.accepted);
    REQUIRE(admit_pane_offer(p.runtime, "test.pane", {"list", "New label", "items"}, 20, 90).written.accepted);
    CHECK(p.runtime.of_kind(a.kind)->preferred_rows == 7);
    CHECK(p.runtime.of_kind(a.kind)->preferred_columns == 54);
    for (const auto rows : {-1, 0, 513})
        CHECK_FALSE(admit_pane_offer(p.runtime, "test.pane", {"other", "Other", "items"}, rows, 54).written.accepted);
    REQUIRE(p.runtime.entries.size() == 1);
}

TEST_CASE("pane comfort budgets body text with chrome in both real medium metrics") {
    RuntimePane p; p.preferred_rows = 7; p.preferred_columns = 54;
    for (const bool graphical : {false, true}) {
        auto sc = screen_of(cells_px(120), cells_px(56));
        if (graphical) { sc.cell_px = 12; sc.text_advance_px = 8; sc.text_line_px = 18; }
        const auto got = project_pane(placement::kOverlayStack, 0, nullptr, sc, &p);
        const auto body = external_body_place(got.visible, sc, 1);
        CHECK(body.rows >= 7);
        CHECK(body.columns >= 54);
        SetupPane authored;
        authored.width = {pane_unit::kPixels, cells_px(30)};
        authored.height = {pane_unit::kPixels, cells_px(8)};
        const auto smaller = project_pane(placement::kOverlayStack, 0, &authored, sc, &p);
        CHECK(smaller.resolved.w == cells_px(30));
        CHECK(smaller.resolved.h == cells_px(8));
        CHECK(project_pane(placement::kOverlayStack, 0, nullptr, sc).resolved ==
              placement_bounds(placement::kOverlayStack, 0, sc));
    }
}

TEST_CASE("pane comfort: the column spends each pane's preferred height, and begins again at its top when spent") {
    Panes p; Setup setup; setup.panes.clear();
    for (const auto key : {"one", "two", "three"}) {
        const auto a = admit_pane_offer(p.runtime, "test.pane", {key, key, "list"}, 7, 54);
        REQUIRE(a.written.accepted);
        SetupPane row; row.ref = {"test.pane", key}; row.front = static_cast<std::int64_t>(setup.panes.size());
        setup.panes.push_back(row);
    }
    auto sc = screen_of(cells_px(120), cells_px(30));
    const auto seats = seat_panes(setup, p);
    REQUIRE(seats.wanted.size() == 3); // every resolved pane is seated
    reconcile(p, setup);
    const auto first = bounds_of(p, setup, seats.wanted[0], sc);
    const auto second = bounds_of(p, setup, seats.wanted[1], sc);
    const auto third = bounds_of(p, setup, seats.wanted[2], sc);
    CHECK(first.resolved.y == sc.room_y);
    CHECK(second.resolved.y == first.resolved.y + first.resolved.h + cells_px(kStackGap));
    CHECK(second.resolved.y + second.resolved.h <= sc.room_y + sc.room_h);
    // THE THIRD WOULD PASS THE FLOOR, so the column begins again at its top.
    CHECK(second.resolved.y + second.resolved.h + cells_px(kStackGap) + third.resolved.h >
          sc.room_y + sc.room_h);
    CHECK(third.resolved.y == sc.room_y);
    CHECK(third.resolved.x == first.resolved.x);
    // An authored position stands outside the column, wherever the column has got to.
    setup.panes.back().place = {pane_unit::kPixels, cells_px(60), cells_px(2)};
    CHECK(bounds_of(p, setup, seats.wanted[2], sc).resolved.x == cells_px(60));
    CHECK(bounds_of(p, setup, seats.wanted[2], sc).resolved.y == sc.room_y + cells_px(2));
    p.runtime.entries[0].preferred_rows = 512;
    const auto clipped = preferred_extent(&p.runtime.entries[0], stack_capacity(sc));
    CHECK(clipped.height == sc.room_h);
}

TEST_CASE("semantic setup refuses malformed or missing panes before changing the active layout") {
    Live live; DemoActor actor(live.bus);
    const auto before = setup_persist::to_text(live.session().setup.active);
    actor.say(kWorkshopProvider, SetupApplyRequested{"bad"});
    Setup candidate; candidate.name = "Test layout"; candidate.panes.clear();
    SetupPane missing; missing.ref = {"missing.provider", "pane"}; candidate.panes.push_back(missing);
    actor.say(kWorkshopProvider, SetupApplyRequested{setup_persist::to_text(candidate)});
    REQUIRE(actor.hand->refused.size() == 2);
    // The missing pane is named, so a caller knows which provider to prepare.
    CHECK(actor.hand->refused.back() ==
          "setup names a pane this Workshop cannot present: missing.provider pane");
    CHECK(setup_persist::to_text(live.session().setup.active) == before);
    candidate.panes.clear();
    actor.say(kWorkshopProvider, SetupApplyRequested{setup_persist::to_text(candidate)});
    CHECK(actor.hand->acks == 1);
    CHECK(live.session().setup.active.name == "Test layout");
}
