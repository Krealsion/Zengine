// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The demo controls' reset and readiness waits, their census and manual, pane comfort's budgets
// and seating in both media, and a semantic setup's refusals before it changes the active layout.

#include "workshop_support.hpp"
#include "census.hpp"
#include "office_manual.hpp"
#include "pane_window.hpp"
#include "external-host/demo-control/control.hpp"

namespace {
namespace demo = zengine::demo;
namespace gfx = zengine::graphics;
/// The demo controls' area in a window (D12).
const std::string kDemoArea = zengine::workshop::pane_area(demo::kRole, demo::kControlsPane);
/// A room `columns` by `rows` of a window whose text is the cell.
RowsRoom cell_room(std::int64_t columns, std::int64_t rows) {
    return RowsRoom{columns * gfx::kCellPx, rows * gfx::kCellPx, {}};
}
struct DemoTestPump { ZEN_SHAPE(DemoTestPump, 1); };
class DemoHand final : public loom::WeaveBase<DemoHand, DemoTestPump,
    loom::Accept<DemoTestPump, loom::Ack, loom::Refused, demo::DemoWork, demo::DemoStatus,
                 PanePassRequested, v3::PaneActions, v2::PaneOffered>,
    loom::Emit<demo::DemoServiceOpened, demo::DemoServiceClosed, demo::DemoWorkRequested,
               demo::DemoWorkFinished, demo::DemoResetRequested, demo::DemoReadyRequested,
               demo::DemoStatusRequested, SetupApplyRequested, v2::PaneOffered,
               PaneActionRequested, PaneCatalogRequested, gfx::ActionRequested,
               gfx::AreaButtonChanged>> {
public:
    std::function<void(loom::Mail&)> action;
    std::vector<demo::DemoWork> work;
    std::vector<demo::DemoStatus> status;
    std::vector<std::string> rows; ///< the ids of the rows last declared, the ones in force
    std::vector<std::string> refused;
    int passed = 0;
    int acks = 0;
    void on(const DemoTestPump&, loom::Mail& m) { action(m); }
    void on(const loom::Ack&, loom::Mail& m) { CHECK(m.answers_ask()); ++acks; }
    void on(const loom::Refused& r, loom::Mail&) { refused.push_back(r.reason); }
    void on(const demo::DemoWork& w, loom::Mail& m) { CHECK(m.answers_ask()); work.push_back(w); }
    void on(const demo::DemoStatus& s, loom::Mail& m) { CHECK(m.answers_ask()); status.push_back(s); }
    void on(const v2::PaneOffered&, loom::Mail&) {}
    void on(const v3::PaneActions& a, loom::Mail&) {
        rows.clear();
        for (const v3::PaneActionRow& row : a.rows) rows.push_back(row.id);
    }
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
        // WORKSHOP ASKS FOR THE OFFER FIRST, as a desk does, so the controls' rows are in force.
        desk.say(demo::kRole, PaneCatalogRequested{}, "zengine.workshop");
    }
    void ready() {
        worker.say(demo::kRole, demo::DemoServiceOpened{"Example"});
        worker.say(demo::kRole, demo::DemoWorkRequested{});
        REQUIRE(worker.hand->work.size() == 1);
        worker.say(demo::kRole, demo::DemoWorkFinished{1, true, "ready"});
    }
};

/// THE DEMO CONTROLS ON A DESK: the weave in its office in a real Workshop, offered when Workshop
/// asks for its panes, and picked onto the desk.
struct DemoDesk {
    PaneRig r;
    DemoDesk() {
        r.mount_workshop();
        auto w = std::make_unique<demo::Control>(); auto* raw = w.get();
        auto grant = loom::emit_default_grant(*w);
        raw->zen_set_self(r.bus.register_weave(std::move(w), std::move(grant), demo::kRole));
        r.ready();
        r.extent(160, 48);
        r.pick(PaneRef{demo::kRole, "controls"});
    }
    const RuntimePane* held() { return r.session().panes.runtime.find(demo::kRole, "controls"); }
};

/// THE DEMO CONTROLS' CENSUS: their one row, in the one mode they have.
const CensusDriverOf kDemoCensus{demo::kRole, [] {
    OfficeCensus census;
    DemoDesk d;
    const RuntimePane* held = d.held();
    REQUIRE(held != nullptr);
    census_take(census, demo::kRole, *held);
    return census;
}};

/// THE DEMO CONTROLS' PRESSES: their one row, with no demo service attached.
const PressDriverOf kDemoPresses{demo::kRole, [](PressWitness& w) {
    press_each_state<DemoDesk>(
        w, [] { return std::make_unique<DemoDesk>(); },
        [](DemoDesk& d, const std::function<void(const RuntimePane&)>& at) { at(*d.held()); });
}};
}

TEST_CASE("the manual the Demo controls answer is the page their build embedded, and names the one action they declare") {
    namespace manual = zengine::manual;
    const manual::Manual& page = manual::pages::zengine_demo::kManual;
    DemoDesk d;
    const RuntimePane* held = d.held();
    REQUIRE(held != nullptr);
    std::set<std::string> ids;
    for (const v4::PaneActionRow& a : held->actions) ids.insert(a.id);
    ManualReader reader(d.r, demo::kRole);
    check_answered_manual(reader, "controls", ids);
    const PaneDocumentShown whole = reader.ask("controls", "");
    CHECK(whole.content_id == std::string(page.content_id));
    CHECK(whole.sections.size() == page.count);
    CHECK(reader.ask("elsewhere", "").refusal == "zengine.demo offers no pane `elsewhere`");
}

TEST_CASE("the Demo office says what each shape it accepts does from its page, and its page hears no other") {
    namespace manual = zengine::manual;
    const manual::Manual& page = manual::pages::zengine_demo::kManual;
    DemoRig r;
    const std::vector<std::string> described =
        described_shapes_of(r.bus, r.bus.role_holder(demo::kRole));
    CHECK(check_office_manual(r.bus, demo::kRole) >= described.size());
    // ...AND EVERY SHAPE IT HEARS IS ONE ITS GATE ACCEPTS. Its Commands are its pane's action ids,
    // which the pane witness holds to what it declares, so only Hears is held to the gate here.
    std::size_t heard = 0;
    for (std::size_t i = 0; i < page.count; ++i) {
        const manual::Section& s = page.sections[i];
        if (!manual::is_heard_group(s.group) || s.key == "hears") continue;
        INFO("hears ", std::string(s.key));
        CHECK(std::find(described.begin(), described.end(), std::string(s.key)) != described.end());
        ++heard;
    }
    CHECK(heard == described.size());
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

TEST_CASE("demo button trusts the window that shows it and service close settles outstanding waits") {
    DemoRig r; r.ready();
    zengine::testing::pane_window::Window window(r.bus);
    window.room(kDemoArea, demo::kRole, 56, 4);
    // A STRANGER'S PRESS, said as itself, at the button's row: nothing.
    r.visitor.say(demo::kRole, gfx::AreaButtonChanged{kDemoArea, 1, true, 3 * gfx::kCellPx + 6,
                                                      gfx::kCellPx + 6, 0});
    r.worker.say(demo::kRole, demo::DemoWorkRequested{});
    CHECK(r.worker.hand->work.size() == 1);
    window.click(kDemoArea, 3, 0); // the setup's name: no button
    CHECK(r.worker.hand->work.size() == 1);
    window.click(kDemoArea, 3, 1, 3); // handed back, not a reset
    CHECK(r.desk.hand->passed == 1);
    CHECK(r.worker.hand->work.size() == 1);
    window.click(kDemoArea, 3, 1);
    REQUIRE(r.worker.hand->work.size() == 2);
    r.visitor.say(demo::kRole, demo::DemoReadyRequested{2});
    CHECK(r.visitor.hand->status.empty());
    r.worker.say(demo::kRole, demo::DemoServiceClosed{});
    CHECK(r.visitor.hand->refused.size() == 1);
    r.visitor.say(demo::kRole, demo::DemoResetRequested{});
    CHECK(r.visitor.hand->refused.size() == 2);
}

TEST_CASE("a press the window landed on the demo controls where it drew no control resets nothing "
          "and is said, and one on the reset button drawn there resets") {
    DemoRig r; r.ready();
    zengine::testing::pane_window::Window window(r.bus);
    window.room(kDemoArea, demo::kRole, 56, 4);
    r.worker.say(demo::kRole, demo::DemoWorkRequested{}); // a worker that takes the demo's work
    const std::size_t work = r.worker.hand->work.size();
    const auto landed = [&](const std::string& part, std::uint64_t press) {
        for (const bool down : {true, false}) {
            gfx::AreaButtonChanged b{kDemoArea, 1, down, 3 * gfx::kCellPx + 6,
                                     gfx::kCellPx + 6, 0};
            b.under = kDemoArea;
            b.part = part;
            window.tell(kDemoArea, b, press);
        }
    };
    landed("", 501); // the window drew the status line there: no control
    CHECK(r.worker.hand->work.size() == work);
    const std::vector<std::string> shown = window.rows(kDemoArea);
    CHECK(std::any_of(shown.begin(), shown.end(), [](const std::string& row) {
        return row.find("the controls moved -- press again") != std::string::npos;
    }));
    window.click(kDemoArea, 3, 1); // the reset button, as the window drew it there now
    CHECK(r.worker.hand->work.size() == work + 1);
}

TEST_CASE("WL-HAND-06: the demo controls name their reset row and the state beneath it") {
    SUBCASE("rows drawn for a window that sets type stand a line of its face apart and a "
            "character an advance, on the ground of the whole room, read back and pressed in that "
            "face") {
        const gfx::TextMeasure face{8, 18};
        const std::vector<TextRow> rows{
            {"[a] one   [x] [y]", gfx::role::kFill}, {"plain", gfx::role::kMuted}};
        const std::vector<RowPart> parts{{"entry:one", 0, 0, 20, true},
                                                 {"control:a", 0, 0, 3, true},
                                                 {"control:x", 0, 10, 3, true}};
        const RowsRoom room{20 * face.advance, 4 * face.line, face};
        const RowsDrawing drawing = rows_drawing(rows, parts, room, room.columns());
        // THE ROOM'S GROUND, ALL OF IT, before anything else: no row leaves a blank behind it.
        REQUIRE_FALSE(drawing.sketch.nodes().empty());
        const gfx::Node& first = drawing.sketch.nodes().front();
        REQUIRE(first.op == gfx::op::kRect);
        const auto ground = gfx::from_body<gfx::RectArgs>(first.args);
        REQUIRE(ground.has_value());
        CHECK(ground->x == 0);
        CHECK(ground->y == 0);
        CHECK(ground->w == room.width);
        CHECK(ground->h == room.height);
        CHECK(ground->fill.role == gfx::role::kGround);
        // EACH RUN where the face sets it: its column an advance across, its row a line down.
        bool plain = false, control = false;
        for (const gfx::Node& n : drawing.sketch.nodes()) {
            if (n.op != gfx::op::kText) continue;
            const auto t = gfx::from_body<gfx::TextArgs>(n.args);
            REQUIRE(t.has_value());
            plain = plain || (t->text == "plain" && t->x == 0 && t->y == face.line);
            control = control || (t->text == "[x]" && t->x == 10 * face.advance && t->y == 0);
        }
        CHECK(plain);
        CHECK(control);
        // READ BACK IN THE FACE, each part where it was said...
        zengine::component::RowMap<DrawnPart> map;
        const std::vector<TextRow> shown =
            drawn_rows(drawing.sketch, room.rows(), room.columns(), map, face);
        REQUIRE(shown.size() >= 2);
        CHECK(shown[1].text == "plain");
        REQUIRE(map.at(0, 11) != nullptr);
        CHECK(map.at(0, 11)->action == "control:x");
        // ...AND A POINT LANDS ON THE CHARACTER HOLDING IT: the eleventh's last pixel, the first
        // row's last.
        const RowCell at = window_cell(11 * face.advance - 1, face.line - 1, room.rows(),
                                       room.columns(), face);
        CHECK(at.shown);
        CHECK(at.row == 0);
        CHECK(at.column == 10);
        CHECK(window_cell(4 * face.advance, face.line, room.rows(), room.columns(), face).row == 1);
    }
    SUBCASE("rows drawn for a graphics window ask, cell by cell, for the part a press there "
            "reaches: a control inside a named row is its own, and an unnamed run is no part") {
        const std::vector<TextRow> rows{
            {"[a] one   [x] [y]", gfx::role::kFill}, {"plain", gfx::role::kMuted}};
        // IN THE ORDER A PRESS READS THEM: the row entire, then the runs inside it.
        const std::vector<RowPart> parts{{"entry:one", 0, 0, 20, true},
                                                 {"control:a", 0, 0, 3, true},
                                                 {"control:x", 0, 10, 3, true},
                                                 {"", 0, 14, 3, true}};
        const RowsDrawing drawing = rows_drawing(rows, parts, cell_room(20, 2), 20);
        zengine::component::RowMap<DrawnPart> map;
        const std::vector<TextRow> shown = drawn_rows(drawing.sketch, 2, 20, map);
        REQUIRE(shown.size() == 2);
        CHECK(shown[0].text.rfind("[a] one   [x] [y]", 0) == 0);
        CHECK(shown[1].text == "plain");
        const auto asks = [&](std::int64_t column) {
            const DrawnPart* part = map.at(0, column);
            return part == nullptr ? std::string() : part->action;
        };
        for (std::int64_t c = 0; c < 20; ++c) {
            CAPTURE(c);
            const char* want = c < 3 ? "control:a"
                               : c >= 10 && c < 13 ? "control:x"
                               : c >= 14 && c < 17 ? ""
                                                   : "entry:one";
            CHECK(asks(c) == want);
        }
        CHECK(map.at(1, 0) == nullptr);
        // ...AND A PRESS ON EACH LANDS ON A CELL IT OWNS, never on a run inside it.
        REQUIRE(drawing.places.size() == 3);
        REQUIRE(drawing.places.count("control:a") == 1);
        CHECK(drawing.places.at("control:a").row == 0);
        CHECK(drawing.places.at("control:a").column == 0);
        REQUIRE(drawing.places.count("entry:one") == 1);
        CHECK(drawing.places.at("entry:one").column == 3);
        REQUIRE(drawing.places.count("control:x") == 1);
        CHECK(drawing.places.at("control:x").column == 10);
        // TWO PARTS THAT OVERLAP: each lands on a cell it alone holds, though a press on the
        // overlap reaches the later one.
        const RowsDrawing overlap = rows_drawing(
            std::vector<TextRow>{{"abcdefghij", gfx::role::kFill}},
            std::vector<RowPart>{{"left", 0, 0, 6, true}, {"right", 0, 3, 7, true}},
            cell_room(10, 1), 10);
        REQUIRE(overlap.places.size() == 2);
        CHECK(overlap.places.at("left").column == 0);
        CHECK(overlap.places.at("right").column == 6);
    }
    SUBCASE("a caret and a selection drawn there stand in front of their rows, each run asking "
            "what the part beneath it asks: none without a caret, the caret on the last column "
            "when it stands past it, and neither on a row past the rows asked for") {
        namespace gfx = zengine::graphics;
        const std::vector<TextRow> rows{{"first row", gfx::role::kFill},
                                        {"second", gfx::role::kFill},
                                        {"third", gfx::role::kFill}};
        const std::vector<RowPart> parts{{"one", 0, 0, 10, true}, {"two", 1, 0, 3, true}};
        const auto node = [](const RowsDrawing& d, const std::string& path) -> const gfx::Node* {
            for (const gfx::Node& n : d.sketch.nodes()) {
                if (n.path == path) return &n;
            }
            return nullptr;
        };
        const auto asks = [](const RowsDrawing& d, const std::string& path) {
            for (const gfx::ElementRecord& r : d.sketch.records()) {
                if (r.path == path) return r.action;
            }
            return std::string();
        };
        const auto words = [&](const RowsDrawing& d, const std::string& path) {
            const gfx::Node* n = node(d, path);
            REQUIRE(n != nullptr);
            CHECK(n->op == gfx::op::kText);
            const std::optional<gfx::TextArgs> text = gfx::from_body<gfx::TextArgs>(n->args);
            REQUIRE(text.has_value());
            CHECK(text->ink.role == gfx::role::kAccent);
            return *text;
        };
        const RowsDrawing plain = rows_drawing(rows, parts, cell_room(10, 3), 10);
        CHECK(node(plain, "caret") == nullptr);
        CHECK(node(plain, "sel0.0") == nullptr);
        // FROM THE FIRST ROW'S SEVENTH COLUMN TO THE SECOND ROW'S FIFTH, cut where the part
        // beneath changes; the caret past the room's last column stands on it.
        RowsCaret caret;
        caret.row = 1;
        caret.column = 40;
        caret.sel_begin_row = 0;
        caret.sel_begin_col = 6;
        caret.sel_end_row = 1;
        caret.sel_end_col = 5;
        const RowsDrawing marked = rows_drawing(rows, parts, cell_room(10, 3), 10, caret);
        const gfx::TextArgs first = words(marked, "sel0.0");
        CHECK(first.text == "row");
        CHECK(first.x == 6 * gfx::kCellPx);
        CHECK(first.y == 0);
        CHECK(asks(marked, "sel0.0") == "one");
        CHECK(node(marked, "sel0.1") == nullptr);
        const gfx::TextArgs named = words(marked, "sel1.0");
        CHECK(named.text == "sec");
        CHECK(named.y == gfx::kCellPx);
        CHECK(asks(marked, "sel1.0") == "two");
        const gfx::TextArgs bare = words(marked, "sel1.1");
        CHECK(bare.text == "on");
        CHECK(bare.x == 3 * gfx::kCellPx);
        CHECK(asks(marked, "sel1.1").empty());
        CHECK(node(marked, "sel2.0") == nullptr);
        const gfx::Node* bar = node(marked, "caret");
        REQUIRE(bar != nullptr);
        REQUIRE(bar->op == gfx::op::kLine);
        const std::optional<gfx::LineArgs> line = gfx::from_body<gfx::LineArgs>(bar->args);
        REQUIRE(line.has_value());
        CHECK(line->x1 == 9 * gfx::kCellPx);
        CHECK(line->x2 == 9 * gfx::kCellPx);
        CHECK(line->y1 == gfx::kCellPx);
        CHECK(line->y2 == 2 * gfx::kCellPx - 1);
        CHECK(line->ink.role == gfx::role::kAccent);
        CHECK(asks(marked, "caret").empty());
        // THE MARKS STAND IN FRONT: drawn after every row.
        CHECK(marked.sketch.nodes().back().path == "caret");
        CHECK(node(marked, "row2.0") < node(marked, "sel0.0"));
        // ONE ROW ASKED FOR: the rows after it go, and the marks on them.
        const RowsDrawing capped = rows_drawing(rows, parts, cell_room(10, 3), 10, caret, 1);
        CHECK(node(capped, "row0.0") != nullptr);
        CHECK(node(capped, "row1.0") == nullptr);
        CHECK(node(capped, "sel0.0") != nullptr);
        CHECK(node(capped, "sel1.0") == nullptr);
        CHECK(node(capped, "caret") == nullptr);
        CHECK(capped.places.count("two") == 0);
        // ROWS PAST THE WINDOW'S BOUNDS go, and the marks on them: none stands on a row not drawn.
        std::vector<TextRow> many;
        std::vector<RowPart> each;
        const auto past = static_cast<std::int64_t>(gfx::kMaxElementsPerArea) + 20;
        for (std::int64_t i = 0; i < past + 10; ++i) {
            many.push_back({"xxxxxx", gfx::role::kFill});
            each.push_back({"p" + std::to_string(i), i, 0, 2, true});
        }
        RowsCaret beyond;
        beyond.row = past;
        beyond.column = 5;
        beyond.sel_begin_row = past;
        beyond.sel_begin_col = 2;
        beyond.sel_end_row = past;
        beyond.sel_end_col = 5;
        const RowsDrawing bounded = rows_drawing(many, each, cell_room(10, 3), 10, beyond);
        CHECK(node(bounded, "row0.0") != nullptr);
        CHECK(node(bounded, "row" + std::to_string(past) + ".0") == nullptr);
        CHECK(node(bounded, "sel" + std::to_string(past) + ".0") == nullptr);
        CHECK(node(bounded, "caret") == nullptr);
    }
    DemoRig r; r.ready();
    // IN ITS AREA OF A WINDOW: each name over the row it names (WL-HAND-07)...
    zengine::testing::pane_window::Window shown_in(r.bus);
    shown_in.room(kDemoArea, demo::kRole, 56, 4);
    // ...the window's press asking only what the rows in force declare.
    for (const std::string& asked : shown_in.actions(kDemoArea)) {
        CAPTURE(asked);
        CHECK(std::find(r.desk.hand->rows.begin(), r.desk.hand->rows.end(), asked) !=
              r.desk.hand->rows.end());
    }
    const std::vector<std::string> drawn = shown_in.rows(kDemoArea);
    REQUIRE(drawn.size() >= 3);
    CHECK(drawn[1].find("Reset demo") != std::string::npos);
    CHECK(shown_in.action_at(kDemoArea, 3, 1) == "reset");
    CHECK(drawn[2].rfind("ready", 0) == 0);
    // ...THE RESET ROW'S NAME IS ITS ACTION'S: asked by Workshop, `reset` resets as the press does,
    // and asked by anyone else it does nothing.
    r.worker.say(demo::kRole, demo::DemoWorkRequested{});
    REQUIRE(r.worker.hand->work.size() == 1);
    r.visitor.say(demo::kRole, PaneActionRequested{"controls", "reset"});
    CHECK(r.worker.hand->work.size() == 1);
    r.desk.say(demo::kRole, PaneActionRequested{"controls", "reset"}, "zengine.workshop");
    CHECK(r.worker.hand->work.size() == 2);

    SUBCASE("in a window that sets type, the same lines a line of its face apart, and a press on "
            "the words of the button resets") {
        // ⚔ MUTATION: the picture set a row a cell in every window -- its lines stand 12 pixels
        // apart in a face 18 tall.
        DemoRig w; w.ready();
        zengine::testing::pane_window::Window window(w.bus, "light");
        const gfx::TextMeasure face{8, 18};
        window.set_measure(face);
        window.room(kDemoArea, demo::kRole, 40, 10);
        const zengine::graphics::window::WinArea* a = window.weave().find(kDemoArea);
        REQUIRE(a != nullptr);
        // THE ROOM'S GROUND FIRST, the whole of it.
        REQUIRE_FALSE(a->nodes.empty());
        const auto ground = gfx::from_body<gfx::RectArgs>(a->nodes.front().node.args);
        REQUIRE(ground.has_value());
        CHECK(a->nodes.front().node.op == gfx::op::kRect);
        CHECK((ground->w == a->w && ground->h == a->h));
        CHECK(ground->fill.role == gfx::role::kGround);
        std::set<std::int64_t> lines;
        for (const zengine::graphics::window::KeptNode& n : a->nodes) {
            if (n.node.op != gfx::op::kText) continue;
            const auto t = gfx::from_body<gfx::TextArgs>(n.node.args);
            REQUIRE(t.has_value());
            lines.insert(t->y);
        }
        CHECK(lines == std::set<std::int64_t>{0, face.line, 2 * face.line});
        CHECK(window.rows(kDemoArea) ==
              std::vector<std::string>{"Example", "[ Reset demo ]", "ready: ready"});
        w.worker.say(demo::kRole, demo::DemoWorkRequested{});
        REQUIRE(w.worker.hand->work.size() == 1);
        REQUIRE(window.press_words(kDemoArea, "Reset demo"));
        CHECK(w.worker.hand->work.size() == 2);
    }
    SUBCASE("in a graphics window, the same lines under the same names, and the window's press on "
            "the button resets") {
        // A WINDOW'S ROOM, the pane seated nowhere: its three lines, drawn whole.
        DemoRig w; w.ready();
        zengine::testing::pane_window::Window window(w.bus);
        window.room(kDemoArea, demo::kRole, 40, 10);
        REQUIRE(window.weave().heard.size() == 1);
        CHECK(window.weave().heard[0].whole);
        CHECK(window.weave().refusals.empty());
        std::vector<std::string> shown = window.rows(kDemoArea);
        REQUIRE(shown.size() == 3);
        CHECK(shown[0] == "Example");
        CHECK(shown[1] == "[ Reset demo ]");
        CHECK(shown[2] == "ready: ready");
        CHECK(window.action_at(kDemoArea, 3, 1) == "reset");
        CHECK(window.action_at(kDemoArea, 0, 0).empty());
        CHECK(window.action_at(kDemoArea, 0, 2).empty());

        // THE PRESS THERE ASKS `reset`, which resets as Workshop's press does, and the window is
        // sent only the line that changed.
        w.worker.say(demo::kRole, demo::DemoWorkRequested{});
        REQUIRE(w.worker.hand->work.size() == 1);
        REQUIRE(window.press_words(kDemoArea, "Reset demo"));
        REQUIRE(w.worker.hand->work.size() == 2);
        CHECK(w.worker.hand->work.back().generation == 2);
        const gfx::Drawn& change = window.weave().heard.back();
        CHECK_FALSE(change.whole);
        REQUIRE(change.put.size() == 1);
        CHECK(change.put[0].path == "status");
        CHECK(window.rows(kDemoArea)[2] == "working: Resetting demo");

        // ...AND ONLY THE WINDOW'S OWN WORD, FOR AN ACTION ITS PICTURE NAMES, IS A PRESS: the same
        // action from anyone else, and one the picture does not name, do nothing.
        w.worker.say(demo::kRole, demo::DemoWorkFinished{2, true, "ready again"});
        w.worker.say(demo::kRole, demo::DemoWorkRequested{});
        const std::size_t drawings = window.weave().heard.size();
        w.visitor.say(demo::kRole, gfx::ActionRequested{kDemoArea, "reset"});
        window.ask(kDemoArea, "status");
        window.ask(kDemoArea, "control:reset");
        CHECK(w.worker.hand->work.size() == 2);
        CHECK(window.weave().heard.size() == drawings);

        // A REFUSED CHANGE LEAVES THE WINDOW AS IT WAS, and the picture last sent goes again at
        // once, whole; that refused too, nothing is sent again, and the next picture goes whole.
        const std::vector<std::string> before = window.rows(kDemoArea);
        REQUIRE_FALSE(window.weave().heard.back().whole);
        std::size_t mark = window.weave().heard.size();
        window.refuse(kDemoArea, "the suite refuses a change");
        REQUIRE(window.weave().heard.size() == mark + 1);
        CHECK(window.weave().heard.back().whole);
        CHECK(window.rows(kDemoArea) == before);
        mark = window.weave().heard.size();
        window.refuse(kDemoArea, "the suite refuses the whole");
        CHECK(window.weave().heard.size() == mark);
        REQUIRE(window.press_words(kDemoArea, "Reset demo"));
        REQUIRE(w.worker.hand->work.size() == 3);
        REQUIRE(window.weave().heard.size() == mark + 1);
        CHECK(window.weave().heard.back().whole);
        CHECK(window.rows(kDemoArea)[2] == "working: Resetting demo");

        // A ROOM OF ONE ROW: the name alone, and no part recorded for a line not drawn.
        window.room(kDemoArea, demo::kRole, 40, 1, 2);
        CHECK(window.weave().heard.back().whole);
        CHECK(window.weave().refusals.empty());
        CHECK(window.rows(kDemoArea) == std::vector<std::string>{"Example"});
    }
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

namespace {
/// The Weave Manager's caller, as Workshop's plan booter is.
struct DemoBooterState { std::int64_t said = 0; ZEN_SHAPE(DemoBooterState, 1, ZEN_FIELD(said)); };
class DemoBooter final : public loom::WeaveBase<DemoBooter, DemoBooterState,
    loom::Accept<loom::Result, loom::Ack, loom::Refused>,
    loom::Emit<loom::LoadWeave, loom::ReloadWeave>> {
public:
    std::vector<std::string> said;
    void on(const loom::Result& r, loom::Mail&) { said.push_back(r.value); }
    void on(const loom::Ack&, loom::Mail&) { said.push_back("ack"); }
    void on(const loom::Refused& r, loom::Mail&) { said.push_back("refused: " + r.reason); }
};
}

TEST_CASE("a reloaded demo office never stands working with no service: it says the service must "
          "start again, and the next one carries the generations on") {
    loom::Switchboard bus;
    loom::Kernel kernel{bus, loom::trust_every_artifact(
        "Zengine's test harness: it loads only this build tree's output")};
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);
    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
    const loom::WeaveId booter_id = loom::mount_granted<DemoBooter>(bus, std::move(reach));
    auto* booter = static_cast<DemoBooter*>(bus.weave(booter_id));
    const auto boot = [&](const auto& command) {
        (void)bus.send_as(booter_id, manager,
                          loom::Message(loom::to_value(command), booter_id, booter_id, 1));
        bus.drain_until_idle();
        REQUIRE_FALSE(booter->said.empty());
        return booter->said.back();
    };
    DemoActor desk{bus, "zengine.workshop"}, worker{bus}, visitor{bus}, next{bus};
    const std::string loaded =
        boot(loom::LoadWeave{"zengine-demo-control", WORKSHOP_SO_DEMO_CONTROL, demo::kRole});
    REQUIRE_MESSAGE(loaded.rfind("refused", 0) != 0, loaded);
    const auto status = [&] {
        visitor.say(demo::kRole, demo::DemoStatusRequested{});
        REQUIRE_FALSE(visitor.hand->status.empty());
        return visitor.hand->status.back();
    };
    worker.say(demo::kRole, demo::DemoServiceOpened{"Example"});
    worker.say(demo::kRole, demo::DemoWorkRequested{});
    REQUIRE(worker.hand->work.size() == 1);
    REQUIRE(status().state == "working");

    // RELOADED THROUGH THE MANAGER, MID-WORK: the service it served is not this image's.
    CHECK(boot(loom::ReloadWeave{"zengine-demo-control", WORKSHOP_SO_DEMO_CONTROL}) == "ack");
    demo::DemoStatus now = status();
    CHECK(now.state == "unavailable");
    CHECK(now.note.find("reloaded") != std::string::npos);
    CHECK(now.generation == 1);
    // ...so the old service's word is refused out loud, never taken as this image's work...
    worker.say(demo::kRole, demo::DemoWorkFinished{1, true, "ready"});
    REQUIRE_FALSE(worker.hand->refused.empty());
    CHECK(status().state == "unavailable");
    // ...and a service started again attaches, its work numbered after the reload's.
    next.say(demo::kRole, demo::DemoServiceOpened{"Example"});
    next.say(demo::kRole, demo::DemoWorkRequested{});
    REQUIRE(next.hand->work.size() == 1);
    CHECK(next.hand->work.back().generation == 2);
    REQUIRE(status().state == "working");

    // RELOADED BY THE KERNEL ITSELF, with no activation to follow: the same, said at revival.
    const loom::ReloadResult direct =
        kernel.reload_from("zengine-demo-control", WORKSHOP_SO_DEMO_CONTROL);
    REQUIRE_MESSAGE(direct.reloaded, direct.error);
    now = status();
    CHECK(now.state == "unavailable");
    CHECK(now.note.find("reloaded") != std::string::npos);
    CHECK(now.generation == 2);
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

TEST_CASE("pane comfort: `fit` follows a re-offer and `default` keeps the first, and a measure that comes back is held") {
    // ⚔ MUTATION: `pane_measure` reading the first offer for `fit` -- the fitted pane stays as tall
    // as its first offer asked, whatever it asks now.
    Panes p;
    Setup setup;
    setup.name = "Comfort";
    setup.panes.clear();
    const auto fits = admit_pane_offer(p.runtime, "test.pane", {"fits", "Fits", "list"}, 7, 54);
    const auto stays = admit_pane_offer(p.runtime, "test.pane", {"stays", "Stays", "list"}, 7, 54);
    REQUIRE(fits.written.accepted);
    REQUIRE(stays.written.accepted);
    for (const auto& [key, x] : {std::pair{"fits", 0}, std::pair{"stays", 60}}) {
        SetupPane row;
        row.ref = {"test.pane", key};
        row.place = {pane_unit::kPixels, cells_px(x), 0};
        row.height = {std::string_view(key) == "fits" ? ui::kSizeFit : pane_unit::kDefault, 0};
        row.front = static_cast<std::int64_t>(setup.panes.size());
        setup.panes.push_back(row);
    }
    REQUIRE(check_setup(setup).accepted);
    reconcile(p, setup);
    const auto sc = screen_of(cells_px(140), cells_px(44));
    const auto tall = [&](std::int64_t kind) { return bounds_of(p, setup, kind, sc).resolved.h; };
    const std::int64_t first = tall(fits.kind);
    CHECK(tall(stays.kind) == first); // one offer: `fit` and `default` read the same measure
    // THE PANE SAYS ITS CONTENT GREW, by offering again: `fit` follows, `default` does not.
    REQUIRE(admit_pane_offer(p.runtime, "test.pane", {"fits", "Fits", "list"}, 20, 54).refreshed);
    REQUIRE(admit_pane_offer(p.runtime, "test.pane", {"stays", "Stays", "list"}, 20, 54).refreshed);
    const std::int64_t grown = tall(fits.kind);
    CHECK(grown > first);
    CHECK(tall(stays.kind) == first);
    CHECK_FALSE(p.runtime.of_kind(fits.kind)->fit_held);
    // A MEASURE THAT COMES BACK -- the one before the latest -- IS HELD where it is: a pane whose
    // measure follows its room would otherwise answer every grant with the other one.
    // ⚔ MUTATION: no hold -- the fitted pane shrinks back to its first height.
    REQUIRE(admit_pane_offer(p.runtime, "test.pane", {"fits", "Fits", "list"}, 7, 54).refreshed);
    CHECK(p.runtime.of_kind(fits.kind)->fit_held);
    CHECK(tall(fits.kind) == grown);
    // ...and a measure it has not said before is taken, and nothing is held.
    REQUIRE(admit_pane_offer(p.runtime, "test.pane", {"fits", "Fits", "list"}, 12, 54).refreshed);
    CHECK_FALSE(p.runtime.of_kind(fits.kind)->fit_held);
    CHECK(tall(fits.kind) > first);
    CHECK(tall(fits.kind) < grown);
    CHECK(tall(stays.kind) == first);
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
    actor.say(kLayoutsRole, SetupApplyRequested{"bad"});
    Setup candidate; candidate.name = "Test layout"; candidate.panes.clear();
    SetupPane missing; missing.ref = {"missing.provider", "pane"}; candidate.panes.push_back(missing);
    actor.say(kLayoutsRole, SetupApplyRequested{setup_persist::to_text(candidate)});
    REQUIRE(actor.hand->refused.size() == 2);
    // The missing pane is named, so a caller knows which provider to prepare.
    CHECK(actor.hand->refused.back() ==
          "setup names a pane this Workshop cannot present: missing.provider pane");
    CHECK(setup_persist::to_text(live.session().setup.active) == before);
    candidate.panes.clear();
    actor.say(kLayoutsRole, SetupApplyRequested{setup_persist::to_text(candidate)});
    CHECK(actor.hand->acks == 1);
    CHECK(live.session().setup.active.name == "Test layout");
}

TEST_CASE("an agent's desk holding a setting with no value is refused by its key, and changes nothing") {
    // THE DESK AN AGENT HANDS ACROSS IS READ BY THE SETUP FILE'S OWN READER, so a setting that
    // holds no value, or two, is refused here as it is in a file -- by its key.
    Live live; DemoActor actor(live.bus);
    live.ready(); // the desk started: the layouts' owner knows which panes Workshop presents
    const auto before = setup_persist::to_text(live.session().setup.active);
    // A desk naming only a pane this Workshop presents, so its settings are all it is judged on.
    Setup candidate;
    candidate.name = "Agent desk";
    candidate.panes.push_back(SetupPane{ref_of(pane_kind::kLayouts), {}, {}, {}, 0, {}});
    candidate.panes.front().settings = {PaneSetting{"legend", {}, {}, {}}};
    actor.say(kLayoutsRole, SetupApplyRequested{setup_persist::to_text(candidate)});
    REQUIRE(actor.hand->refused.size() == 1);
    CHECK(actor.hand->refused.back().find("setting `legend` holds no value") != std::string::npos);
    CHECK(setup_persist::to_text(live.session().setup.active) == before);

    // ...AND ONE HOLDING EXACTLY ONE VALUE IS KEPT AS IT CAME, whatever the pane makes of it.
    candidate.panes.front().settings = {PaneSetting{"legend", {}, {}, std::string("hiden")}};
    actor.say(kLayoutsRole, SetupApplyRequested{setup_persist::to_text(candidate)});
    CHECK(actor.hand->acks == 1);
    CHECK(live.run().active.panes.front().settings == candidate.panes.front().settings);
}

TEST_CASE("a desk put live keeps every lock: `r` and an agent's desk are refused whole naming each locked value they would change, and a lock the desk brings joins") {
    // ⚔ MUTATION: `replacement_refused` answering nothing -- the incoming desk changes the locked
    // legend, and the lock stands over a value nobody locked.
    TempDir dir("replace-keeps-locks");
    Live live;
    live.host.setup_path = dir.file("setup.json");
    DemoActor actor(live.bus);
    live.ready();
    // A DESK OF THE LAYOUTS PANE ALONE, which every Workshop presents, its legend locked.
    const PaneRef layouts = ref_of(pane_kind::kLayouts);
    stage_desk(live, [&](Setup& desk) {
        desk = setup_of("Locked", {pane_kind::kLayouts});
        pane_of(desk, layouts)->settings = {PaneSetting{"legend", {}, {}, std::string("shown")}};
        desk.locks = {SetupLock{layouts, "legend"}};
    });
    const Setup locked = live.run().active;
    const std::string says =
        "the desk would change what a lock holds -- legend of zengine.workshop/layouts; nothing was "
        "replaced";

    // A DESK CHANGING THE LOCKED VALUE, as an agent hands it across: refused whole, in words.
    Setup changed = locked;
    changed.locks.clear();
    pane_of(changed, layouts)->settings = {PaneSetting{"legend", {}, {}, std::string("hidden")}};
    actor.say(kLayoutsRole, SetupApplyRequested{setup_persist::to_text(changed)});
    REQUIRE(actor.hand->refused.size() == 1);
    CHECK(actor.hand->refused.back() == says);
    CHECK(live.run().active == locked);
    // ...AND AS `r` READS IT FROM THE FILE: the same words, the same desk.
    REQUIRE(setup_persist::save_file(live.host.setup_path, changed).accepted);
    live.key(input::scan::kR);
    CHECK(live.session().notice.find(says) != std::string::npos);
    CHECK(live.run().active == locked);
    // ...AND ONE DROPPING THE LOCKED ROW, which changes what the lock holds as surely.
    Setup dropped = locked;
    dropped.locks.clear();
    REQUIRE(remove_pane(dropped, layouts));
    actor.say(kLayoutsRole, SetupApplyRequested{setup_persist::to_text(dropped)});
    REQUIRE(actor.hand->refused.size() == 2);
    CHECK(actor.hand->refused.back() == says);

    // A DESK LEAVING THE VALUE IS PUT LIVE, keeping the lock, and the lock it brings joins it.
    Setup kept = locked;
    kept.name = "Kept";
    kept.locks = {SetupLock{layouts, "style"}};
    actor.say(kLayoutsRole, SetupApplyRequested{setup_persist::to_text(kept)});
    CHECK(actor.hand->acks == 1);
    CHECK(live.run().active.name == "Kept");
    CHECK(live.run().active.locks ==
          std::vector<SetupLock>{SetupLock{layouts, "style"}, SetupLock{layouts, "legend"}});
}
