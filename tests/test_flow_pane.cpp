// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "control_parts.hpp"
#include "flow/flow-pane/view.hpp" // `unit` -- Flow's authored grid, which a drag lands in
#include "flow/flow-pane/vocabulary.hpp"
#include "flow/flow-host/runtime.hpp"
#include "flow/shape.hpp"
#include "flow/workspace.hpp"
#include "input/vocabulary.hpp"
#include "inventory/codec.hpp"
#include "operator/primitives.hpp"
#include "operator/reference.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_canvas_drawing.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_picture.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/powers_door.hpp"
#include "lifecycle_door.hpp"
#include "pane_window.hpp"
#include "shown_picture.hpp"
#include <zen/kernel/kernel.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {
namespace fp = zengine::flow_pane;
/// The Flow pane's area in a window (D12).
const std::string kFlowArea = zengine::workshop::pane_area(fp::kRole, fp::kPane);
namespace fh = zengine::flow_host;
namespace flow = zengine::flow;
namespace ws = zengine::workshop;
namespace in = zengine::input;
namespace op = zengine::op;
namespace gfx = zengine::graphics;
namespace pw = zengine::testing::pane_window;
constexpr const char* workshop_role = "zengine.workshop";
constexpr auto unit = ws::kPaneCanvasUnit;

// A real office on the real bus, standing in for Workshop: it keeps what Flow tells it and approves
// an acquisition that continues a press. What Flow shows, it draws in its area, and a case reads
// that from the window showing the area, never a copy of the pane's own hit map.
class Presenter final : public loom::Weave {
public:
    std::vector<loom::Message> messages;
    std::vector<std::string> rows; ///< the ids of the rows last declared, the ones in force
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<ws::PaneOffered>(), loom::schema_of<ws::v3::PaneActions>(),
            loom::schema_of<ws::PaneEscapeUnspent>(), loom::schema_of<ws::PaneQuitAnswered>(),
            loom::schema_of<ws::PanePassRequested>(), loom::schema_of<fp::FlowEdited>(),
            loom::schema_of<ws::PaneMenuRequested>(), loom::schema_of<ws::PaneOperationRequested>(),
            loom::schema_of<ws::PaneValueCarryRequested>()};
    }
    loom::WeaveId self{};
    void handle(const loom::Message& message, loom::Bus& bus) override {
        // Workshop approves an acquisition that continues a current gesture; this stand-in does.
        if (loom::same_identity(message.payload.schema(), *loom::schema_of<ws::PaneOperationRequested>()))
            (void)bus.answer(loom::Message(loom::to_value(ws::PaneOperationAnswered{true, ""}), self));
        if (loom::same_identity(message.payload.schema(), *loom::schema_of<ws::v3::PaneActions>())) {
            rows.clear();
            for (const auto& row : loom::from_value<ws::v3::PaneActions>(message.payload).rows) rows.push_back(row.id);
        }
        messages.push_back(message);
    }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("flowtest.Presenter", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
};

/// A MOMENT OF THE HAND ON FLOW'S AREA, as the window showing it gives one: a button pressed or let
/// go, a move or the wheel, at a pixel of the area, under its press's number (`gesture`).
struct Moment {
    std::int64_t phase = ws::canvas_pointer::kPress;
    std::uint64_t gesture = 0;
    std::int64_t button = 1, x = 0, y = 0;
    double dy = 0;
    /// What the window drew under the hand when the moment landed: the area, the part named there
    /// and that drawing's number; none said, Flow reads the moment against its newest drawing.
    std::string under, part;
    std::int64_t drawing = 0;
};

struct Rig {
    op::Catalog catalog;
    loom::Switchboard bus;
    loom::Kernel kernel{bus, loom::trust_every_artifact("the fixture selects one Flow pane artifact")};
    fh::RuntimeHost runtime{bus, catalog};
    Presenter* presenter = nullptr;
    pw::PaneDirectory* directory = nullptr; ///< the graphics directory, asked the area's room
    /// THE WINDOW SHOWING FLOW'S AREA, a room of cells, and the one a case reads and presses: this
    /// one, or another a case gave the area since.
    pw::Window window{bus, "desk"};
    pw::Window* shown = &window;
    loom::WeaveId workshop{}, pane{}, door{}, finder{}, menus{};
    std::uint64_t correlation = 0;
    std::int64_t activation = 0, grant = 1;
    std::uint64_t gesture = 0;
    /// WHAT THE PANE ASKED, read off the bus: every question it put to the discovery door, the
    /// bytes of every answer it was given, and the highest correlation its own requests used.
    std::vector<ws::FindPowers> questions;
    std::vector<std::string> answers;
    std::uint64_t asked = 0;
    loom::ObserverId tap{};

    Rig() {
        tap = bus.add_observer([this](const loom::BusEvent& e) { observe(e); });
        REQUIRE(catalog.mount("flowtest.basic", op::primitive_definitions()));
        runtime.mount();
        // THE HOST'S DISCOVERY DOOR over the same catalog, with the grant Workshop writes for it.
        auto discovery = std::make_unique<ws::PowersDoor>(catalog);
        ws::PowersDoor* discovery_raw = discovery.get();
        loom::Grant say;
        say.allow_to_any(ws::PowersFound::zen_name, ws::PowersFound::zen_version);
        say.allow_to_any(ws::PowerDescribed::zen_name, ws::PowerDescribed::zen_version);
        say.allow_to_any(zengine::manual::ShapeDocumentShown::zen_name,
                         zengine::manual::ShapeDocumentShown::zen_version);
        finder = bus.register_weave(std::move(discovery), std::move(say), std::string(ws::kPowersRole));
        discovery_raw->zen_set_self(finder);
        auto presentation = std::make_unique<Presenter>();
        presenter = presentation.get();
        loom::Grant host_grant;
        for (const auto& schema : {loom::schema_of<ws::PaneCatalogRequested>(),
                loom::schema_of<ws::PaneKey>(), loom::schema_of<ws::PaneTextInput>(),
                loom::schema_of<ws::PaneCanvasValueDrop>(), loom::schema_of<fp::FlowEdit>(),
                loom::schema_of<ws::PaneActionRequested>()})
            host_grant.allow_to_role(schema->name(), schema->version(), fp::kRole);
        host_grant.allow_to_any(ws::PaneQuitRequested::zen_name, ws::PaneQuitRequested::zen_version);
        host_grant.allow_to_any(ws::PaneOperationAnswered::zen_name, ws::PaneOperationAnswered::zen_version);
        workshop = bus.register_weave(std::move(presentation), std::move(host_grant), workshop_role);
        presenter->self = workshop;
        loom::Grant choose;
        choose.allow_to_role(ws::PaneMenuAnswered::zen_name, ws::PaneMenuAnswered::zen_version, fp::kRole);
        menus = bus.register_weave(std::make_unique<Presenter>(), std::move(choose), ws::kPresenterRole);
        loom::Grant pane_grant;
        fh::allow_flow_requests(pane_grant);
        ws::allow_finding_powers(pane_grant);
        for (const auto& schema : {loom::schema_of<ws::PaneOffered>(), loom::schema_of<ws::v3::PaneActions>(),
                loom::schema_of<ws::PaneEscapeUnspent>(), loom::schema_of<ws::PanePassRequested>(),
                loom::schema_of<ws::PaneMenuRequested>(), loom::schema_of<ws::PaneOperationRequested>(),
                loom::schema_of<ws::PaneValueCarryRequested>()})
            pane_grant.allow_to_role(schema->name(), schema->version(), workshop_role);
        // Answers target the concrete requester; a role-addressed grant would not
        // authorize mail.answer(), even when that requester holds Workshop.
        pane_grant.allow(ws::PaneQuitAnswered::zen_name, ws::PaneQuitAnswered::zen_version, workshop);
        pane_grant.allow_to_any(fp::FlowEdited::zen_name, fp::FlowEdited::zen_version);
        // ...and its drawing, to the window that shows its area, and its room, asked of the directory.
        pane_grant.allow_to_any(gfx::Drawn::zen_name, gfx::Drawn::zen_version);
        pane_grant.allow_to_role(gfx::RoomAsked::zen_name, gfx::RoomAsked::zen_version, gfx::kGraphicsRole);
        directory = pw::mount_directory(bus);
        const auto loaded = kernel.load("flow-pane", FLOW_PANE_ARTIFACT, fp::kRole, std::move(pane_grant));
        INFO(loaded.error);
        REQUIRE(loaded.ok);
        pane = loaded.id;
        door = zengine::testing::mount_door(bus);
        activate();
        room();
    }
    ~Rig() { bus.remove_observer(tap); }
    void observe(const loom::BusEvent& e) {
        if (!pane.valid() || (e.kind != loom::EventKind::Delivered && e.kind != loom::EventKind::Refused))
            return;
        if (e.sender == pane) {
            for (const char* request : {ws::kFindPowersName, fh::FlowCatalog::zen_name, fh::FlowInspect::zen_name,
                     fh::FlowRun::zen_name, fh::FlowApply::zen_name, fh::FlowSend::zen_name, fh::FlowStop::zen_name})
                if (e.schema_name == request) asked = std::max(asked, e.correlation);
            if (e.kind == loom::EventKind::Delivered && e.schema_name == ws::kFindPowersName && e.payload)
                questions.push_back(ws::find_powers_from(*e.payload));
        }
        if (e.kind == loom::EventKind::Delivered && e.target == pane && e.payload &&
            e.schema_name == ws::PowersFound::zen_name)
            answers.push_back(loom::serialize(*e.payload));
    }
    void pump() {
        for (unsigned turn = 0; turn < 64; ++turn) {
            runtime.poll();
            if (!bus.pending()) return;
            bus.pump_pending();
        }
        REQUIRE(bus.pending() == 0);
    }
    void activate() {
        zengine::testing::order_activation(bus, door, pane, ++activation);
        pump();
    }
    template<class T> void host(const T& value, std::uint64_t corr = 0) {
        const auto ticket = bus.office_send_to_role_as(workshop, workshop_role, fp::kRole,
            loom::Message(loom::to_value(value), workshop, {}, corr));
        REQUIRE(ticket.valid());
        pump();
    }
    /// The same delivery, queued and not yet pumped, to order it among answers in flight.
    template<class T> void post(const T& value, std::uint64_t corr = 0) {
        REQUIRE(bus.office_send_to_role_as(workshop, workshop_role, fp::kRole,
            loom::Message(loom::to_value(value), workshop, {}, corr)).valid());
    }
    /// THE ROOM OF FLOW'S AREA in the rig's window, `cols` by `rows` cells, under the rig's `grant`:
    /// a new one is a window started again, which holds none of Flow's picture.
    void room(std::int64_t cols = 170, std::int64_t rows = 65) {
        shown = &window;
        window.room(kFlowArea, fp::kRole, cols, rows, grant);
        pump();
        REQUIRE(window.held(kFlowArea) != nullptr);
        REQUIRE_FALSE(window.held(kFlowArea)->nodes.empty());
    }
    /// WHAT THE WINDOW SHOWING FLOW'S AREA SHOWS, read as its picture -- and every one Flow draws
    /// names only controls its rows in force declare (WL-HAND-07).
    ws::CanvasPicture picture() const {
        const gfx::window::WinArea* a = shown->held(kFlowArea);
        REQUIRE(a != nullptr);
        const ws::CanvasPicture p = zengine::testing::shown_picture(
            *a, ws::canvas_window_room(a->w, a->h, shown->cell_px(), shown->measure()));
        std::vector<std::string> names;
        for (const auto& part : p.parts) names.push_back(part.name);
        const std::string problem = control_parts_problem("zengine.flow/flow", names, presenter->rows);
        CHECK_MESSAGE(problem.empty(), problem);
        return p;
    }
    /// How many drawings the window showing Flow's area has heard.
    std::size_t drawings() const { return shown->weave().heard.size(); }
    ws::CanvasText label(const std::string& text, bool prefix = false) const {
        const auto shown_now = picture();
        for (const auto& row : shown_now.texts)
            if (prefix ? row.text.rfind(text, 0) == 0 : row.text == text) return row;
        INFO("Missing pictured label: " << text);
        for (const auto& row : shown_now.texts) INFO(row.text);
        REQUIRE(false);
        return {};
    }
    bool shows(const std::string& text) const {
        const auto texts = picture().texts;
        return std::any_of(texts.begin(), texts.end(), [&](const auto& row) { return row.text == text; });
    }
    bool shows_part(const std::string& text) const {
        const auto texts = picture().texts;
        return std::any_of(texts.begin(), texts.end(),
                           [&](const auto& row) { return row.text.find(text) != std::string::npos; });
    }
    Moment press_for(const std::string& text, bool prefix = false) {
        const auto row = label(text, prefix);
        Moment m;
        m.gesture = ++gesture;
        m.x = row.x + 4;
        m.y = row.y + 4;
        return m;
    }
    /// A PRESS THE WINDOW LANDS ON `text` NOW, with its word of what it drew there: the narrowest
    /// part over the point, and the drawing's number. Delivered later, Flow judges it by that word.
    Moment landed(const std::string& text, bool prefix = false) {
        Moment m = press_for(text, prefix);
        m.under = kFlowArea;
        m.part = part_at(m.x, m.y);
        const gfx::window::WinArea* a = shown->held(kFlowArea);
        REQUIRE(a != nullptr);
        m.drawing = a->drawing;
        return m;
    }
    /// The part Flow's picture names over (`x`, `y`) of its area: the narrowest; "" none.
    std::string part_at(std::int64_t x, std::int64_t y) const {
        std::string name;
        std::int64_t area = -1;
        for (const auto& p : picture().parts) {
            if (x < p.x || y < p.y || x >= p.x + p.w || y >= p.y + p.h) continue;
            if (area < 0 || p.w * p.h < area) {
                area = p.w * p.h;
                name = p.name;
            }
        }
        return name;
    }
    /// THE MOMENT AS THE WINDOW SAYS IT: a button changed, a move or the wheel at its pixel.
    template <class Visit>
    void as_said(const Moment& m, Visit&& visit) const {
        if (m.phase == ws::canvas_pointer::kPress || m.phase == ws::canvas_pointer::kRelease) {
            gfx::AreaButtonChanged b{kFlowArea, m.button, m.phase == ws::canvas_pointer::kPress,
                                     m.x, m.y, 0};
            b.under = m.under;
            b.part = m.part;
            b.drawing = m.drawing;
            visit(b);
        } else if (m.phase == ws::canvas_pointer::kMove) {
            visit(gfx::AreaPointerMoved{kFlowArea, m.x, m.y, 0, 0, 0});
        } else {
            visit(gfx::AreaWheelTurned{kFlowArea, 0, m.dy, m.x, m.y, 0});
        }
    }
    /// A MOMENT GIVEN BY THE WINDOW SHOWING FLOW'S AREA, under its press's number -- or `corr`.
    void host(const Moment& m, std::uint64_t corr = 0) {
        as_said(m, [&](const auto& said) { shown->tell(kFlowArea, said, corr != 0 ? corr : m.gesture); });
        pump();
    }
    /// ...queued and not yet pumped.
    void post(const Moment& m, std::uint64_t corr = 0) {
        as_said(m, [&](const auto& said) { shown->post(kFlowArea, said, corr != 0 ? corr : m.gesture); });
    }
    void click(const std::string& text, bool prefix = false) { click(press_for(text, prefix)); }
    /// A press and its release, as the window landed them.
    void click(Moment m) {
        host(m);
        m.phase = ws::canvas_pointer::kRelease;
        host(m);
    }
    /// What Workshop delivers when a person drops a carried value at (x, y) of Flow's area.
    void drop(const loom::Value& value, std::int64_t x, std::int64_t y) {
        const auto bytes = zengine::inventory::encode_pair(value, {});
        host(ws::PaneCanvasValueDrop{fp::kPane, kFlowArea, x, y,
                                     loom::Bytes(bytes.begin(), bytes.end()),
                                     "zengine.inventory-pane", "inventory", ""});
    }
    void drop_on(const loom::Value& value, const std::string& text, bool prefix = false) {
        const auto row = label(text, prefix);
        drop(value, row.x + 4, row.y + 4);
    }
    loom::Value reference(const std::string& identity) const {
        const op::OperatorDef* def = catalog.find(identity);
        REQUIRE(def != nullptr);
        return op::encode_reference({identity, def->inputs()->content_id(), def->outputs()->content_id()});
    }
    void key(std::int64_t scancode, std::int64_t modifiers = 0) { host(ws::PaneKey{fp::kPane, scancode, modifiers}); }
    void text(const std::string& value) { host(ws::PaneTextInput{fp::kPane, value}); }
    void replace_text(const std::string& value) { key(in::scan::kA, in::mod::kCtrl); text(value); }
    fp::FlowEdited edit(const std::string& action, std::vector<std::string> arguments = {}) {
        const auto corr = ++correlation;
        host(fp::FlowEdit{action, std::move(arguments)}, corr);
        for (auto at = presenter->messages.rbegin(); at != presenter->messages.rend(); ++at) {
            if (at->correlation != corr || !loom::same_identity(at->payload.schema(), *loom::schema_of<fp::FlowEdited>())) continue;
            CHECK(at->provenance.answers_ask());
            return loom::from_value<fp::FlowEdited>(at->payload);
        }
        throw std::runtime_error("FlowEdit did not produce its correlated answer");
    }
    /// `add-node`'s arguments for an operator as the host's catalog holds it now: the reference a
    /// maker's Add carries.
    std::vector<std::string> ref(const std::string& identity) const {
        const op::OperatorDef* def = catalog.find(identity);
        REQUIRE(def != nullptr);
        return {identity, std::to_string(static_cast<std::int64_t>(def->inputs()->content_id())),
                std::to_string(static_cast<std::int64_t>(def->outputs()->content_id()))};
    }
    void edit_ok(const std::string& action, std::vector<std::string> arguments = {}) {
        const auto answer = edit(action, std::move(arguments));
        INFO(action << ": " << answer.reason);
        REQUIRE(answer.ok);
    }
    fp::FlowPaneState state() {
        const auto admitted = loom::admit(loom::parse(bus.snapshot_bytes(pane)), loom::schema_of<fp::FlowPaneState>());
        REQUIRE(admitted);
        return loom::from_value<fp::FlowPaneState>(admitted.value());
    }
    flow::Workspace workspace() { return flow::read_workspace(flow::byte_string(state().workspace)); }
    void graph_semantically() {
        edit_ok("new", {"meter", "discard"});
        edit_ok("state-field", {"value", "Int", "required"});
        edit_ok("message", {"Set"});
        edit_ok("message-field", {"0", "input", "Int", "required"});
        edit_ok("trigger", {"0", "value"});
        edit_ok("add-node", ref("math.max"));
        edit_ok("bind", {"0", "0", "$input"});
        edit_ok("bind", {"0", "1", "0"});
        edit_ok("result", {"0"});
    }
    std::int64_t live_value() {
        const auto subject = bus.role_holder("meter");
        REQUIRE(subject.valid());
        return bus.weave(subject)->snapshot().get("value")->as_int();
    }
};

class TempFiles {
public:
    TempFiles() {
        const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned suffix = 0; suffix < 100; ++suffix) {
            directory = std::filesystem::temp_directory_path() /
                ("zengine-flow-pane-" + std::to_string(seed) + "-" + std::to_string(suffix));
            if (std::filesystem::create_directory(directory)) return;
        }
        throw std::runtime_error("could not create a distinct Flow pane fixture directory");
    }
    ~TempFiles() {
        std::error_code ignored;
        std::filesystem::remove(directory / "workspace.flow", ignored);
        std::filesystem::remove(directory, ignored);
    }
    std::string file() const { return (directory / "workspace.flow").string(); }
private:
    std::filesystem::path directory;
};

/// A Source that yields an Int: zero weaver inputs, and its contributor's words.
op::OperatorDef answer_source() {
    return op::OperatorDef("flowtest.answer", loom::make_schema("flowtest.answer.in", 1, {}),
                           loom::SchemaBuilder("flowtest.answer.out", 1).field("value", loom::Kind::Int).build(),
                           [](const loom::Value&) { return loom::Cell::integer(42); },
                           op::Description{"forty-two, whenever it is asked"});
}

std::size_t escapes_unspent(const Presenter& presenter) {
    return static_cast<std::size_t>(std::count_if(presenter.messages.begin(), presenter.messages.end(),
        [](const auto& m) { return loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneEscapeUnspent>()); }));
}
} // namespace

TEST_CASE("loaded Flow pane authors runs edits and reopens a graphical project with reusable examples") {
    TempFiles files;
    Rig rig;
    rig.click("[New]");
    rig.replace_text("meter");
    rig.key(in::scan::kReturn);
    rig.click("[State]");
    rig.click("[Add state field]");
    rig.key(in::scan::kReturn);
    rig.click("[Messages]");
    rig.click("[New message]");
    rig.key(in::scan::kReturn);
    rig.click("[Add field]");
    rig.key(in::scan::kReturn);
    rig.click("[Graph]");
    rig.click("[Add trigger]");
    rig.key(in::scan::kReturn);
    rig.click("  math.max");
    rig.click("[Add]");
    rig.click("input.input : Int");
    rig.click("o lhs = ", true);
    rig.click("o rhs = ", true);
    rig.click("[Int constant]");
    (void)rig.label("> Value: 0", true);
    rig.key(in::scan::kReturn);
    rig.click("[Use as result]");
    const auto graph = rig.workspace();
    REQUIRE(graph.graph.project.definition.on.size() == 1);
    REQUIRE(graph.graph.project.definition.on.front().body.nodes.size() == 1);
    CHECK(graph.graph.project.definition.on.front().body.nodes.front().arguments.front().input_name() == "input");
    rig.click("[Run]");
    CHECK(rig.live_value() == 0);
    // FLOW'S LINE IN THE WEAVER'S WORDS: the host's yes is a word, an edit that shows itself says
    // nothing, and a file written is named (WL-NOTE-06).
    CHECK(rig.shows("Running"));
    rig.edit_ok("state-open");
    CHECK_FALSE(rig.shows_part(" complete"));
    CHECK_FALSE(rig.shows("Running"));
    const std::string exported = files.file() + ".project";
    rig.edit_ok("export-project", {exported});
    CHECK(rig.shows_part("Exported "));
    rig.click("[Messages]");
    rig.click("[meter.Set]");
    rig.click("input : Int = ", true);
    (void)rig.label("> Value:", true);
    rig.text("7");
    rig.key(in::scan::kReturn);
    rig.click("[Save example]");
    rig.replace_text("seven");
    rig.key(in::scan::kReturn);
    rig.click("[Send]");
    CHECK(rig.live_value() == 7);
    CHECK(rig.workspace().graph.project.state.get("value")->as_int() == 7);
    rig.edit_ok("bind", {"0", "1", "10"});
    rig.edit_ok("apply");
    CHECK(rig.live_value() == 7);
    rig.edit_ok("send");
    CHECK(rig.live_value() == 10);
    rig.edit_ok("move", {"0", "1300", "301"});
    rig.edit_ok("save", {files.file()});
    REQUIRE(std::filesystem::is_regular_file(files.file()));
    CHECK_FALSE(rig.state().dirty);
    const auto saved = flow::open_workspace(files.file());
    REQUIRE(saved.library.find("seven"));
    CHECK(saved.library.find("seven")->value().get("input")->as_int() == 7);
    CHECK(saved.graph.project.state.get("value")->as_int() == 10);
    rig.edit_ok("stop");
    CHECK(rig.shows("Stopped"));
    rig.edit_ok("new", {"temporary", "discard"});
    rig.edit_ok("open", {files.file(), "discard"});
    CHECK(rig.workspace().graph.places.front().x == 1300);
    CHECK(rig.workspace().graph.places.front().y == 301);
    rig.edit_ok("run");
    CHECK(rig.live_value() == 10);
    rig.edit_ok("preset-open", {"seven"});
    rig.edit_ok("send");
    CHECK(rig.live_value() == 10);
}

TEST_CASE("WL-HAND-06: Flow names its controls, its nodes and ports by their place, and every place a press means something") {
    Rig rig;
    auto said = rig.picture();
    CHECK(ws::canvas_parts_problem(said.parts).empty());
    const auto named = [&](const std::string& name) {
        return std::any_of(said.parts.begin(), said.parts.end(), [&](const auto& p) { return p.name == name; });
    };
    CHECK(named("control:ask-save"));
    CHECK(named("control:page-graph"));
    // A GRAPH WITH A NODE: the node and its arguments named by its place, whatever index it has.
    rig.click("[New]");
    rig.replace_text("meter");
    rig.key(in::scan::kReturn);
    rig.click("[State]");
    rig.click("[Add state field]");
    rig.key(in::scan::kReturn);
    rig.click("[Messages]");
    rig.click("[New message]");
    rig.key(in::scan::kReturn);
    rig.click("[Add field]");
    rig.key(in::scan::kReturn);
    rig.click("[Graph]");
    rig.click("[Add trigger]");
    rig.key(in::scan::kReturn);
    rig.click("  math.max");
    rig.click("[Add]");
    said = rig.picture();
    std::size_t nodes = 0, ports = 0;
    for (const auto& p : said.parts) {
        nodes += p.name.rfind("node:", 0) == 0 ? 1u : 0u;
        ports += p.name.rfind("port:", 0) == 0 ? 1u : 0u;
    }
    CHECK(nodes == 1);
    CHECK(ports == 2);
    // EVERY NAMED PLACE IS WHERE A LABEL OR RUN SAYS SOMETHING, OR A BOX IS DRAWN.
    for (const auto& p : said.parts) {
        CAPTURE(p.name);
        CHECK(p.w > 0);
        CHECK(p.h > 0);
    }
    // EVERY PLACE A PRESS MEANS SOMETHING, LISTED AS THE PICTURE READS A PRESS: its hits in their
    // order, each under its name, or unnamed where an earlier one took it.
    const ws::CanvasRoom room{1200, 720, 1, true, 8, 16};
    const fp::Picture drawn = fp::picture(fp::Model{}, room);
    const auto listed = fp::named(drawn);
    REQUIRE_FALSE(drawn.hits.empty());
    REQUIRE(listed.parts.size() == drawn.hits.size());
    std::set<std::string> taken;
    for (std::size_t i = 0; i < drawn.hits.size(); ++i) {
        CAPTURE(i);
        const fp::Hit& hit = drawn.hits[i];
        CHECK(listed.parts[i].x == hit.x);
        CHECK(listed.parts[i].y == hit.y);
        CHECK(listed.parts[i].w == hit.w);
        CHECK(listed.parts[i].h == hit.h);
        const std::string name = fp::part_name(hit);
        CHECK(listed.parts[i].name == (taken.insert(name).second ? name : std::string()));
    }

    SUBCASE("in a graphics window that gives Flow its pointer, a node pressed there and moved under "
            "the press's number moves, as on Workshop's canvas, and a hold the window ends leaves "
            "it where it stands") {
        std::string node;
        for (const auto& p : said.parts)
            if (p.name.rfind("node:", 0) == 0) node = p.name;
        REQUIRE_FALSE(node.empty());
        pw::Window window(rig.bus, "flowtest");
        window.room(kFlowArea, fp::kRole, 170, 65);
        rig.pump();
        rig.shown = &window;
        REQUIRE(window.press(kFlowArea, "control:page-graph"));
        rig.pump();
        const auto x_of = [&]() -> std::int64_t {
            for (const auto& p : rig.picture().parts)
                if (p.name == node) return p.x;
            return -1;
        };
        const auto cell = window.cell_of(kFlowArea, node);
        REQUIRE(cell.has_value());
        const std::int64_t before = x_of();
        REQUIRE(before >= 0);
        const std::uint64_t n = window.next_press();
        window.button(kFlowArea, cell->first, cell->second, true, n);
        window.move(kFlowArea, cell->first + 6, cell->second, n);
        rig.pump();
        const std::int64_t moved = x_of();
        CHECK(moved > before);
        window.hold_ended(kFlowArea, 1);
        window.move(kFlowArea, cell->first + 12, cell->second, n);
        rig.pump();
        CHECK(x_of() == moved);
    }

    SUBCASE("in a graphics window: its room asked once a life, its picture drawn there whole and "
            "then as what changed, under the names Workshop's parts carry; a press there is the "
            "first-button press on Workshop's canvas, and a refused change is drawn whole again "
            "once while a refused whole picture is only said") {
        std::string node, port;
        for (const auto& p : said.parts) {
            if (p.name.rfind("node:", 0) == 0) node = p.name;
            if (p.name.rfind("port:", 0) == 0) port = p.name;
        }
        REQUIRE_FALSE(node.empty());
        REQUIRE_FALSE(port.empty());
        CHECK(rig.directory->times(kFlowArea, fp::kRole) == 1);
        rig.activate(); // activated again in the same life, before any window: no second ask
        CHECK(rig.directory->times(kFlowArea, fp::kRole) == 1);
        pw::Window window(rig.bus, "flowtest");
        const auto& seen = window.weave();
        // A ROOM OF THE RIG'S SIZE, so the picture is the one the rig's window shows, one for one.
        window.room(kFlowArea, fp::kRole, 170, 65);
        rig.pump();
        rig.shown = &window;
        REQUIRE(seen.heard.size() == 1);
        const gfx::Drawn first = seen.heard.front();
        CHECK(first.whole);
        CHECK(seen.refusals.empty());
        gfx::window::WinArea fresh;
        fresh.area = fp::kRole;
        CHECK(gfx::window::judge(fresh, gfx::window::Drawer{}, first).empty());
        std::set<std::string> names, recorded;
        for (const auto& p : said.parts)
            if (!p.name.empty()) names.insert(p.name);
        for (const auto& r : first.said) recorded.insert(r.action);
        CHECK(recorded == names);
        const auto pressed = window.actions(kFlowArea);
        CHECK(pressed.count("control:page-state") == 1);
        CHECK(pressed.count(node) == 1);
        CHECK(pressed.count(port) == 1);
        for (const auto& action : pressed) CHECK(names.count(action) == 1);

        // A PRESS ON A CONTROL performs it, and the window shows what it did.
        REQUIRE(window.press(kFlowArea, "control:page-state"));
        rig.pump();
        (void)rig.label("[Add state field]");
        REQUIRE(seen.heard.size() == 2);
        CHECK_FALSE(seen.heard.back().whole);
        CHECK(seen.refusals.empty());
        const auto rows = window.rows(kFlowArea);
        CHECK(std::any_of(rows.begin(), rows.end(),
                          [](const std::string& row) { return row.find("[Add state field]") != std::string::npos; }));
        // ...A PRESS ON A NODE selects it and begins no drag, so Delete removes that node.
        REQUIRE(window.press(kFlowArea, "control:page-graph"));
        rig.pump();
        for (int put_down = 0; put_down < 4 && rig.shows_part("Delete node"); ++put_down)
            rig.key(in::scan::kEscape);
        REQUIRE_FALSE(rig.shows_part("Delete node"));
        REQUIRE(window.press(kFlowArea, node));
        rig.pump();
        CHECK(rig.shows_part("Delete node"));
        rig.key(in::scan::kDelete);
        const auto after = rig.picture().parts;
        CHECK(std::none_of(after.begin(), after.end(), [&](const auto& p) { return p.name == node; }));
        CHECK(window.actions(kFlowArea).count(node) == 0);
        // ...ANOTHER WINDOW, or an action the picture shown there does not name, does nothing.
        const auto heard = seen.heard.size();
        loom::Grant forging;
        forging.allow_to_role(gfx::AreaRoom::zen_name, gfx::AreaRoom::zen_version, fp::kRole);
        const auto forger = rig.bus.register_weave(std::make_unique<Presenter>(), std::move(forging));
        rig.bus.send_as_to_role(forger, fp::kRole,
            loom::Message(loom::to_value(gfx::AreaRoom{kFlowArea, "flowtest", 9, 170 * unit, 65 * unit, {}}), forger));
        rig.pump();
        pw::Window other(rig.bus, "other");
        other.ask(kFlowArea, "control:page-state", fp::kRole);
        window.ask(kFlowArea, "control:no-such");
        rig.pump();
        CHECK_FALSE(rig.shows("[Add state field]"));
        CHECK(seen.heard.size() == heard);
        CHECK(other.weave().heard.empty());
        // ...A CONFIRMED WRITE asked from the window waits, since no Workshop gesture made the press.
        REQUIRE(window.press(kFlowArea, "control:ask-save"));
        rig.pump();
        REQUIRE(window.press(kFlowArea, "control:dialog-confirm"));
        rig.pump();
        CHECK(rig.shows_part("came under no gesture Workshop numbered, as a raw key or a press"));
        REQUIRE(window.press(kFlowArea, "control:dialog-cancel"));
        rig.pump();

        // A REFUSED CHANGE IS DRAWN AGAIN WHOLE AT ONCE...
        const auto before = seen.heard.size();
        window.weave().refuse_next = true;
        REQUIRE(window.press(kFlowArea, "control:page-messages"));
        rig.pump();
        REQUIRE(seen.heard.size() == before + 2);
        CHECK_FALSE(seen.heard[before].whole);
        CHECK(seen.heard[before + 1].whole);
        CHECK(seen.refusals.size() == 1);
        CHECK_FALSE(rig.shows_part("The graphics window refused Flow's picture"));
        // ...AND A REFUSED WHOLE PICTURE IS SAID in the picture drawn next, once, and nothing
        // more is drawn until the next change.
        window.weave().refuse_next = true;
        window.room(kFlowArea, fp::kRole, 170, 65, 2);
        rig.pump();
        REQUIRE(seen.heard.size() == before + 4);
        CHECK(seen.heard[before + 2].whole);
        CHECK(seen.heard.back().whole);
        CHECK(seen.refusals.size() == 2);
        CHECK(rig.shows_part("The graphics window refused Flow's picture: the case refused this drawing"));
        rig.pump();
        CHECK(seen.heard.size() == before + 4);
        rig.click("[Graph]");
        REQUIRE(seen.heard.size() == before + 5);
        CHECK(seen.refusals.size() == 2);

        // ONCE A LIFE: an activation asks no room again; a reloaded pane asks it afresh.
        rig.activate();
        CHECK(rig.directory->times(kFlowArea, fp::kRole) == 1);
        const auto reload = rig.kernel.reload_from("flow-pane", FLOW_PANE_ARTIFACT);
        INFO(reload.error);
        REQUIRE(reload.ok);
        rig.activate();
        CHECK(rig.directory->times(kFlowArea, fp::kRole) == 2);
    }
}

TEST_CASE("loaded Flow pane reload retains unfinished forms layout and its live office session") {
    Rig rig;
    rig.graph_semantically();
    rig.edit_ok("message-open", {"0"});
    rig.edit_ok("preset", {"unfinished"});
    rig.edit_ok("value", {"0", "7"});
    rig.edit_ok("preset", {"seven"});
    rig.edit_ok("run");
    rig.edit_ok("send");
    REQUIRE(rig.live_value() == 7);
    rig.edit_ok("value", {"0", "9"});
    rig.edit_ok("move", {"0", "1371", "349"});
    const auto before = rig.state();
    REQUIRE(before.dirty);
    const auto subject = rig.bus.role_holder("meter");
    const auto reload = rig.kernel.reload_from("flow-pane", FLOW_PANE_ARTIFACT);
    INFO(reload.error);
    REQUIRE(reload.ok);
    REQUIRE(reload.reloaded);
    rig.activate();
    ++rig.grant;
    rig.room();
    const auto after = rig.state();
    CHECK(after.dirty);
    CHECK(after.workspace == before.workspace);
    CHECK(rig.bus.role_holder("meter") == subject);
    (void)rig.label("[Running]");
    rig.edit_ok("send");
    CHECK(rig.live_value() == 9);
    rig.edit_ok("preset-open", {"unfinished"});
    const auto invalid = rig.edit("send");
    CHECK_FALSE(invalid.ok);
    CHECK(rig.live_value() == 9);
    rig.edit_ok("preset-open", {"seven"});
    rig.edit_ok("send");
    CHECK(rig.live_value() == 7);
}

TEST_CASE("loaded Flow pane ignores forged answers and personal Workshop gestures") {
    Rig rig;
    rig.graph_semantically();
    auto foreign = std::make_unique<Presenter>();
    loom::Grant authority;
    authority.allow_to_role(fh::FlowAnswer::zen_name, fh::FlowAnswer::zen_version, fp::kRole);
    authority.allow_to_role(gfx::AreaButtonChanged::zen_name, gfx::AreaButtonChanged::zen_version, fp::kRole);
    const auto impostor = rig.bus.register_weave(std::move(foreign), std::move(authority));
    rig.click("  math.max");
    const auto before = rig.state().workspace;
    const auto add = rig.press_for("[Add]");
    rig.bus.send_as_to_role(impostor, fp::kRole,
        loom::Message(loom::to_value(gfx::AreaButtonChanged{kFlowArea, 1, true, add.x, add.y, 0}), impostor, {},
                      add.gesture));
    rig.pump();
    CHECK(rig.state().workspace == before);
    // The run is the pane's next request, so it takes the next correlation. Deliver the forged
    // answer after the edit queues that ask, but before the actual manager sees it.
    const auto run_correlation = rig.asked + 1;
    rig.bus.office_send_to_role_as(rig.workshop, workshop_role, fp::kRole,
        loom::Message(loom::to_value(fp::FlowEdit{"run", {}}), rig.workshop, {}, 900));
    auto fabricated = rig.workspace().graph.project;
    fabricated.state.set("value", loom::Cell::integer(999));
    fh::FlowAnswer lie;
    lie.session = "workshop"; lie.action = "run"; lie.ok = true;
    lie.project = fh::bytes(flow::project_bytes(fabricated));
    rig.bus.send_as_to_role(impostor, fp::kRole,
        loom::Message(loom::to_value(lie), impostor, {}, run_correlation));
    rig.pump();
    CHECK(rig.live_value() == 0);
    CHECK(rig.workspace().graph.project.state.get("value")->as_int() == 0);
    (void)rig.label("[Running]");
}

TEST_CASE("loaded Flow pane hands a right press back to Workshop under the press's own number, and its picture moves nothing") {
    Rig rig;
    const auto before = rig.state();
    const auto pictures = rig.drawings();
    auto press = rig.press_for("[New]"); // a control a primary press would act on
    press.button = 3;
    rig.host(press, 77);
    std::vector<loom::Message> passed;
    for (const auto& message : rig.presenter->messages)
        if (loom::same_identity(message.payload.schema(), *loom::schema_of<ws::PanePassRequested>()))
            passed.push_back(message);
    REQUIRE(passed.size() == 1);
    CHECK(loom::from_value<ws::PanePassRequested>(passed[0].payload).pane == fp::kPane);
    CHECK(passed[0].correlation == 77);
    CHECK(passed[0].provenance.authored_from_role(fp::kRole));
    // NOTHING OF FLOW'S MOVED: no page, no selection, no new picture.
    CHECK(rig.drawings() == pictures);
    CHECK(rig.state().page == before.page);
    CHECK(rig.state().workspace == before.workspace);
}

TEST_CASE("a right press on a declared message offers to carry its shape, and the choice carries it out as a description") {
    Rig rig;
    rig.edit_ok("new", {"tally", "discard"});
    rig.edit_ok("state-field", {"total", "Int", "required"});
    rig.edit_ok("emitted-message", {"Total"});
    rig.edit_ok("emitted-field", {"0", "total", "Int", "required"});
    rig.click("[Messages]");
    auto press = rig.press_for("0 tally.Total {total: Int}");
    press.button = 3;
    rig.host(press, 91);
    const loom::Message* menu = nullptr;
    for (const auto& m : rig.presenter->messages)
        if (loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneMenuRequested>())) menu = &m;
    REQUIRE(menu != nullptr);
    CHECK(menu->correlation == 91); // it continues the right press
    const auto asked = loom::from_value<ws::PaneMenuRequested>(menu->payload);
    CHECK(asked.subject == "tally.Total");
    REQUIRE(asked.rows.size() == 1);
    CHECK(asked.rows[0].label == "Carry tally.Total");

    REQUIRE(rig.bus.office_send_to_role_as(rig.menus, ws::kPresenterRole, fp::kRole,
        loom::Message(loom::to_value(ws::PaneMenuAnswered{fp::kPane, "tally.Total", true, "carry", ""}),
                      rig.menus, {}, 91)).valid());
    rig.pump();
    const loom::Message* carried = nullptr;
    for (const auto& m : rig.presenter->messages)
        if (loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneValueCarryRequested>())) carried = &m;
    REQUIRE(carried != nullptr);
    CHECK(carried->correlation == 91);
    const auto value = loom::from_value<ws::PaneValueCarryRequested>(carried->payload);
    CHECK_FALSE(value.drag);
    const auto item = zengine::inventory::decode_pair(
        std::string_view(reinterpret_cast<const char*>(value.data.data()), value.data.size())).item;
    REQUIRE(zengine::flow::shape::is_description(item));
    CHECK(loom::same_identity(*zengine::flow::shape::described(item),
                              *rig.workspace().graph.project.definition.emits.at(0)));

    // A right press anywhere else on the page means nothing to Flow: handed back.
    auto elsewhere = rig.press_for("Emitted");
    elsewhere.button = 3;
    rig.host(elsewhere, 92);
    CHECK(loom::same_identity(rig.presenter->messages.back().payload.schema(), *loom::schema_of<ws::PanePassRequested>()));
}

TEST_CASE("a press on a declared message asks under that press to drag its shape out, and still opens it; a right press on a found operator is handed back") {
    Rig rig;
    rig.edit_ok("new", {"tally", "discard"});
    rig.edit_ok("state-field", {"total", "Int", "required"});
    rig.edit_ok("emitted-message", {"Total"});
    rig.edit_ok("emitted-field", {"0", "total", "Int", "required"});
    rig.click("[Messages]");
    rig.host(rig.press_for("0 tally.Total {total: Int}"), 93);
    const loom::Message* carried = nullptr;
    for (const auto& m : rig.presenter->messages)
        if (loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneValueCarryRequested>())) carried = &m;
    REQUIRE(carried != nullptr);
    CHECK(carried->correlation == 93); // the acquisition continues the primary press
    const auto value = loom::from_value<ws::PaneValueCarryRequested>(carried->payload);
    CHECK(value.drag);
    CHECK(value.label == "tally.Total");
    // The press still does what it did: the row says how its shape is carried.
    CHECK(rig.shows("Drag an emitted message, or right-press it, to carry its shape to another pane"));

    // A row whose words name no message, right-pressed, is handed back rather than misread.
    rig.edit_ok("message", {"Count"});
    rig.edit_ok("message-field", {"0", "by", "Int", "required"});
    rig.edit_ok("trigger", {"0", "total"});
    auto found = rig.press_for("  math.max");
    found.button = 3;
    rig.host(found, 94);
    CHECK(loom::same_identity(rig.presenter->messages.back().payload.schema(), *loom::schema_of<ws::PanePassRequested>()));
    CHECK(rig.presenter->messages.back().correlation == 94);
}

TEST_CASE("a declared message that nests another is carried out as a carried shape with the shape it nests, and dropped back it is named for that shape and declared from what it carries") {
    Rig rig;
    const auto detail = loom::SchemaBuilder("review.Detail", 1).field("note", loom::Kind::Text).build();
    const auto review = loom::SchemaBuilder("review.Total", 1)
        .field("total", loom::Kind::Int).message("detail", detail).build();
    loom::Value said(review);
    said.set("total", loom::Cell::integer(7));
    loom::Value note(detail);
    note.set("note", loom::Cell::text("seven"));
    said.set("detail", loom::Cell::message(note));
    rig.edit_ok("new", {"review", "discard"});
    rig.edit_ok("state-field", {"total", "Int", "required"});
    rig.drop(said, 40 * unit, 20 * unit);
    rig.click("[Declare review.Total v1 as an accepted message]");
    REQUIRE(rig.workspace().graph.project.definition.accepts.size() == 1);

    // CARRIED OUT by a press on its row: what is carried holds review.Detail beside review.Total.
    rig.click("[Messages]");
    rig.host(rig.press_for("[review.Total]"), 95);
    const loom::Message* carried = nullptr;
    for (const auto& m : rig.presenter->messages)
        if (loom::same_identity(m.payload.schema(), *loom::schema_of<ws::PaneValueCarryRequested>())) carried = &m;
    REQUIRE(carried != nullptr);
    const auto bytes = loom::from_value<ws::PaneValueCarryRequested>(carried->payload).data;
    const auto item = zengine::inventory::decode_pair(
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size())).item;
    CHECK(item.schema().name() == "zengine.flow.CarriedShape");

    // DROPPED BACK into a fresh draft, its page names the shape and what it nests, never the
    // message it travels in, and it is declared from what it carries alone.
    rig.edit_ok("new", {"again", "discard"});
    rig.drop(item, 40 * unit, 20 * unit);
    CHECK(rig.shows("Dropped the shape review.Total v1, nesting review.Detail v1"));
    CHECK(rig.shows_part("total: Int"));
    CHECK_FALSE(rig.shows_part("AcceptedShapes"));
    CHECK_FALSE(rig.shows_part("accepted ="));
    CHECK_FALSE(rig.shows_part("CarriedShape"));
    rig.click("[Declare review.Total v1 as an accepted message]");
    const auto& accepts = rig.workspace().graph.project.definition.accepts;
    REQUIRE(accepts.size() == 1);
    CHECK(loom::same_identity(*accepts[0], *review));

    // A BARE DESCRIPTION of a shape that nests nothing still declares.
    const auto flat = loom::SchemaBuilder("again.Flat", 1).field("n", loom::Kind::Int).build();
    rig.drop(loom::encode_schema(*flat), 40 * unit, 20 * unit);
    rig.click("[Declare again.Flat v1 as an accepted message]");
    CHECK(rig.workspace().graph.project.definition.accepts.size() == 2);
}

TEST_CASE("a dropped description that names a shape it does not carry declares nothing, and Flow says why in Loom's words") {
    Rig rig;
    const auto detail = loom::SchemaBuilder("review.Detail", 1).field("note", loom::Kind::Text).build();
    const auto review = loom::SchemaBuilder("review.Total", 1)
        .field("total", loom::Kind::Int).message("detail", detail).build();
    rig.edit_ok("new", {"review", "discard"});
    // A BARE DESCRIPTION of a shape that nests another carries nothing beside it.
    rig.drop(loom::encode_schema(*review), 40 * unit, 20 * unit);
    // Said on the page itself, which stays until it is put down, and in the notice.
    const auto texts = rig.picture().texts;
    const auto said = std::count_if(texts.begin(), texts.end(), [](const auto& row) {
        return row.text.find("Not declarable: schema descriptor: unresolved nested schema 'review.Detail'") != std::string::npos;
    });
    CHECK(said == 2);
    CHECK_FALSE(rig.shows_part("[Declare review.Total"));
    CHECK(rig.shows("[Cancel]"));
}

TEST_CASE("loaded Flow pane binds gestures to the pictured room definition and interaction context") {
    Rig rig;
    rig.graph_semantically();
    rig.click("  math.max");

    auto drag = rig.press_for("%0 math.max", true);
    rig.host(drag);
    const auto start = rig.workspace().graph.places.front();
    auto motion = drag;
    motion.phase = ws::canvas_pointer::kMove;
    motion.x += 97; motion.y += 31;
    ++motion.gesture;
    rig.host(motion);
    CHECK(rig.workspace().graph.places.front().x == start.x);
    --motion.gesture;
    rig.host(motion);
    // Flow's grid is `fp::unit` to a character, and a character is a cell in this room.
    CHECK(rig.workspace().graph.places.front().x == start.x + 97 * fp::unit / unit);
    CHECK(rig.workspace().graph.places.front().y == start.y + 31 * fp::unit / unit);
    auto release = motion; release.phase = ws::canvas_pointer::kRelease;
    rig.host(release);
    motion.x += 77;
    rig.host(motion);
    CHECK(rig.workspace().graph.places.front().x == start.x + 97 * fp::unit / unit);

    drag = rig.press_for("%0 math.max", true);
    rig.host(drag);
    rig.click("[State]");
    const auto hidden = rig.workspace().graph.places.front();
    motion = drag; motion.phase = ws::canvas_pointer::kMove; motion.x += 150;
    rig.host(motion);
    CHECK(rig.workspace().graph.places.front().x == hidden.x);
}

TEST_CASE("a press the window landed on Flow takes where the same part is drawn again -- a room "
          "given afresh, a definition edited, a dialog typed in -- and is refused in words where "
          "another stands: another dialog's Confirm in its place") {
    // THE WINDOW SAYS WHAT IT DREW UNDER THE HAND, and Flow acts on that part where it stands now.
    Rig rig;
    rig.graph_semantically();
    const auto body = [&] {
        return rig.workspace().graph.project.definition.on.front().body.nodes.size();
    };
    const auto first = body();
    // A DEFINITION EDITED draws [Add] again where it stood: the press takes, and adds the node.
    rig.click("  math.max");
    const auto across_edit = rig.landed("[Add]");
    REQUIRE(across_edit.part.rfind("add-found:", 0) == 0);
    rig.edit_ok("bind", {"0", "1", "4"});
    REQUIRE(rig.part_at(across_edit.x, across_edit.y) == across_edit.part);
    rig.click(across_edit);
    CHECK_MESSAGE(body() == first + 1, rig.workspace().graph.project.definition.name);
    // A ROOM GIVEN AFRESH draws it there again too.
    rig.click("  math.max");
    const auto across_room = rig.landed("[Add]");
    ++rig.grant;
    rig.room();
    REQUIRE(rig.part_at(across_room.x, across_room.y) == across_room.part);
    rig.click(across_room);
    CHECK(body() == first + 2);
    // A DIALOG TYPED IN draws its Cancel again in its place: the press takes, and puts it down.
    rig.click("[Save*]");
    const auto cancel = rig.landed("[Cancel]");
    rig.replace_text("elsewhere.flow-workspace");
    REQUIRE_FALSE(rig.state().dialog_entries.empty());
    rig.click(cancel);
    CHECK(rig.state().dialog_entries.empty());
    // ANOTHER DIALOG'S CONFIRM IN ITS PLACE, under the same name: Save's Confirm, pressed once
    // Export asks, is another element. Refused in words; Export's dialog stays, unconfirmed.
    rig.click("[Save*]");
    const auto save_confirm = rig.landed("[Confirm]");
    REQUIRE(save_confirm.part == "control:dialog-confirm");
    rig.click("[Cancel]");
    rig.click("[Export]");
    REQUIRE(rig.part_at(save_confirm.x, save_confirm.y) == save_confirm.part);
    const auto asked = rig.state();
    REQUIRE_FALSE(asked.dialog_entries.empty());
    rig.click(save_confirm);
    CHECK(rig.shows_part("The picture changed under the press"));
    CHECK(rig.state().dialog_entries.size() == asked.dialog_entries.size());
    CHECK(rig.state().workspace == asked.workspace);
    // ...AND THE OLD CONFIRM BEFORE NEW, where New's second field now stands, confirms nothing.
    rig.click("[Cancel]");
    rig.click("[Save*]");
    const auto old_confirm = rig.landed("[Confirm]");
    rig.click("[Cancel]");
    rig.click("[New]");
    rig.click(old_confirm);
    CHECK(rig.workspace().graph.project.definition.name == "meter");
}

TEST_CASE("loaded Flow pane preserves authored next-run state across incumbent inspection and reload") {
    Rig rig;
    rig.graph_semantically();
    rig.edit_ok("message-open", {"0"});
    rig.edit_ok("value", {"0", "7"});
    rig.edit_ok("run");
    rig.edit_ok("send");
    REQUIRE(rig.live_value() == 7);
    rig.edit_ok("state-open");
    rig.edit_ok("value", {"0", "99"});
    rig.edit_ok("keep-state");
    REQUIRE(rig.workspace().graph.project.state.get("value")->as_int() == 99);
    rig.edit_ok("inspect");
    CHECK(rig.workspace().graph.project.state.get("value")->as_int() == 99);
    const auto reload = rig.kernel.reload_from("flow-pane", FLOW_PANE_ARTIFACT);
    INFO(reload.error);
    REQUIRE(reload.ok);
    REQUIRE(reload.reloaded);
    rig.activate();
    ++rig.grant;
    rig.room();
    CHECK(rig.live_value() == 7);
    CHECK(rig.workspace().graph.project.state.get("value")->as_int() == 99);
    rig.edit_ok("stop");
    rig.edit_ok("run");
    CHECK(rig.live_value() == 99);
}

TEST_CASE("loaded Flow pane retains unconfirmed field text and selection on reload and refuses quit") {
    TempFiles files;
    Rig rig;
    rig.graph_semantically();
    rig.edit_ok("message-open", {"0"});
    rig.edit_ok("value", {"0", "7"});
    rig.edit_ok("run");
    rig.edit_ok("save", {files.file()});
    REQUIRE_FALSE(rig.state().dirty);
    rig.click("input : Int = ", true);
    (void)rig.label("> Value:", true);
    rig.replace_text("42");
    rig.key(in::scan::kA, in::mod::kCtrl);
    (void)rig.label("> Value: 42", true);
    auto quit = [&](std::uint64_t correlation) {
        const auto asked = rig.bus.office_publish_as(rig.workshop, workshop_role,
            loom::Message(loom::to_value(ws::PaneQuitRequested{}), rig.workshop, {}, correlation));
        REQUIRE(asked.authored);
        REQUIRE(asked.recipients == 1);
        rig.pump();
        for (auto at = rig.presenter->messages.rbegin(); at != rig.presenter->messages.rend(); ++at) {
            if (at->correlation != correlation ||
                !loom::same_identity(at->payload.schema(), *loom::schema_of<ws::PaneQuitAnswered>())) continue;
            CHECK(at->provenance.answers_ask());
            return loom::from_value<ws::PaneQuitAnswered>(at->payload);
        }
        throw std::runtime_error("Flow quit gate did not answer");
    };
    CHECK_FALSE(quit(701).permitted);
    const auto reload = rig.kernel.reload_from("flow-pane", FLOW_PANE_ARTIFACT);
    INFO(reload.error);
    REQUIRE(reload.ok);
    REQUIRE(reload.reloaded);
    rig.activate();
    ++rig.grant;
    rig.room();
    CHECK_FALSE(quit(702).permitted);
    (void)rig.label("> Value: 42", true);
    // The retained selection replaces 42. Keeping only the string would append
    // or insert at a different caret and yield another executable value.
    rig.text("9");
    rig.key(in::scan::kReturn);
    rig.edit_ok("send");
    CHECK(rig.live_value() == 9);
    rig.edit_ok("save", {files.file()});
    CHECK(quit(703).permitted);

    rig.edit_ok("value", {"0", "42"});
    rig.edit_ok("save", {files.file()});
    REQUIRE_FALSE(rig.state().dirty);
    auto queue_edit = [&](const std::string& action, std::vector<std::string> args, std::uint64_t correlation) {
        const auto ticket = rig.bus.office_send_to_role_as(rig.workshop, workshop_role, fp::kRole,
            loom::Message(loom::to_value(fp::FlowEdit{action, std::move(args)}), rig.workshop, {}, correlation));
        REQUIRE(ticket.valid());
    };
    auto queue_quit = [&](std::uint64_t correlation) {
        const auto asked = rig.bus.office_publish_as(rig.workshop, workshop_role,
            loom::Message(loom::to_value(ws::PaneQuitRequested{}), rig.workshop, {}, correlation));
        REQUIRE(asked.authored);
        REQUIRE(asked.recipients == 1);
    };
    auto delivered_quit = [&](std::uint64_t correlation) {
        for (const auto& message : rig.presenter->messages) {
            if (message.correlation == correlation &&
                loom::same_identity(message.payload.schema(), *loom::schema_of<ws::PaneQuitAnswered>())) {
                CHECK(message.provenance.answers_ask());
                return loom::from_value<ws::PaneQuitAnswered>(message.payload);
            }
        }
        throw std::runtime_error("Flow quit gate did not answer queued publication");
    };
    auto drain_without_observation = [&] {
        // The initial Timer request was refused because this fixture has no
        // service. This parks between the Send answer and the real host's next
        // observation beat, without bypassing any ordinary message delivery.
        for (unsigned turn = 0; turn < 64 && rig.bus.pending(); ++turn) rig.bus.pump_pending();
        REQUIRE(rig.bus.pending() == 0);
    };
    queue_edit("send", {}, 810);
    queue_edit("save", {files.file()}, 811);
    queue_quit(812);
    drain_without_observation();
    CHECK_FALSE(delivered_quit(812).permitted); // Send still awaits its answer.
    CHECK(rig.live_value() == 42);
    CHECK(rig.workspace().graph.project.state.get("value")->as_int() == 9);
    REQUIRE_FALSE(rig.state().dirty);
    queue_quit(813);
    drain_without_observation();
    CHECK_FALSE(delivered_quit(813).permitted); // Answer arrived; observation has not.
    queue_edit("save", {files.file()}, 814);
    queue_quit(815);
    drain_without_observation();
    CHECK_FALSE(delivered_quit(815).permitted); // Saving old state cannot settle Send.
    queue_edit("inspect", {}, 816);
    queue_quit(817);
    drain_without_observation();
    CHECK_FALSE(delivered_quit(817).permitted); // Inspection itself is still in flight.
    CHECK(rig.workspace().graph.project.state.get("value")->as_int() == 42);
    REQUIRE(rig.state().dirty);
    const auto unsettled_save = quit(818);
    CHECK_FALSE(unsettled_save.permitted);
    CHECK(unsettled_save.refusal.find("unsaved") != std::string::npos);
    rig.edit_ok("save", {files.file()});
    CHECK(quit(819).permitted);
}

TEST_CASE("loaded Flow pane keeps an unfinished dialog bound to its original form") {
    Rig rig;
    rig.graph_semantically();
    rig.edit_ok("message-open", {"0"});
    rig.edit_ok("value", {"0", "7"});
    rig.click("input : Int = ", true);
    (void)rig.label("> Value:", true);
    rig.replace_text("99");
    rig.click("[State]");
    (void)rig.label("Edit input");
    (void)rig.label("> Value: 99", true);
    rig.click("[Save*]");
    (void)rig.label("Edit input");
    (void)rig.label("> Value: 99", true);
    const auto retarget = rig.edit("state-open");
    CHECK_FALSE(retarget.ok);
    CHECK(rig.workspace().graph.project.state.get("value")->as_int() == 0);
    rig.key(in::scan::kReturn);
    rig.edit_ok("run");
    rig.edit_ok("send");
    CHECK(rig.live_value() == 99);
    // A PRESS THE WINDOW LANDED ON THE DIALOG'S FIELD, delivered once the dialog is put down,
    // stands on another part or none: it is refused in words, and edits nothing.
    rig.click("input : Int = ", true);
    const auto stale_field = rig.landed("  Row:", true);
    REQUIRE(stale_field.part.rfind("dialog-field:", 0) == 0);
    rig.key(in::scan::kEscape);
    const auto before_stale = rig.state();
    REQUIRE(before_stale.dialog_entries.empty());
    rig.host(stale_field);
    const auto after_stale = rig.state();
    CHECK(after_stale.dialog_entries.empty());
    CHECK(after_stale.workspace == before_stale.workspace);
    CHECK(rig.live_value() == 99);
    CHECK(rig.shows_part("The picture changed under the press"));
}

TEST_CASE("a query typed to the search line's end is drawn whole in a window, its caret a bar after it") {
    // A WINDOW'S CARET IS A BAR between characters, so the line gives it no blank of its own: a
    // query filling the field is drawn to its last character, never cut and marked.
    Rig rig;
    rig.graph_semantically();
    pw::Window native(rig.bus, "native");
    native.set_measure(gfx::TextMeasure{9, 18});
    native.room_px(kFlowArea, fp::kRole, 170 * 9, 65 * 22);
    rig.pump();
    rig.shown = &native;
    rig.text("abcdefghijklmno"); // the field's fifteen columns, the caret after them
    const ws::CanvasText line = rig.label("find: abcdefghijklmno"); // exactly: not cut
    CHECK(line.caret_col == 21);
}

TEST_CASE("loaded Flow pane keeps authored layout through native text drag pan save and reload") {
    TempFiles files;
    Rig rig;
    rig.graph_semantically();
    pw::Window native(rig.bus, "native");
    native.set_measure(gfx::TextMeasure{9, 18});
    native.room_px(kFlowArea, fp::kRole, 170 * 9, 65 * 22);
    rig.pump();
    rig.shown = &native;
    rig.edit_ok("save", {files.file()});
    REQUIRE_FALSE(rig.state().dirty);
    const auto start = rig.workspace().graph.places.front();
    auto drag = rig.press_for("%0 math.max", true);
    rig.host(drag);
    auto motion = drag;
    motion.phase = ws::canvas_pointer::kMove;
    motion.x += 5 * 9; motion.y += 3 * 22;
    rig.host(motion);
    CHECK(rig.workspace().graph.places.front().x == start.x + 5 * fp::unit);
    CHECK(rig.workspace().graph.places.front().y == start.y + 3 * fp::unit);
    CHECK(rig.state().dirty);
    auto release = motion; release.phase = ws::canvas_pointer::kRelease;
    rig.host(release);

    auto pan = rig.press_for("%0 math.max", true);
    pan.button = 2;
    rig.host(pan);
    motion = pan; motion.phase = ws::canvas_pointer::kMove;
    motion.x += 3 * 9; motion.y += 2 * 22;
    rig.host(motion);
    CHECK(rig.workspace().pan_x == 3 * fp::unit);
    CHECK(rig.workspace().pan_y == 2 * fp::unit);
    release = motion; release.phase = ws::canvas_pointer::kRelease;
    rig.host(release);
    rig.edit_ok("save", {files.file()});
    const auto saved = rig.state().workspace;
    REQUIRE_FALSE(rig.state().dirty);
    const auto reload = rig.kernel.reload_from("flow-pane", FLOW_PANE_ARTIFACT);
    INFO(reload.error);
    REQUIRE(reload.ok);
    REQUIRE(reload.reloaded);
    rig.activate();
    native.room_px(kFlowArea, fp::kRole, 170 * 9, 65 * 22, 2);
    rig.pump();
    CHECK(rig.state().workspace == saved);
    CHECK_FALSE(rig.state().dirty);
    motion.x += 36;
    rig.host(motion);
    CHECK(rig.state().workspace == saved);
    rig.click("o rhs = 0", true);
    rig.click("[Int constant]");
    (void)rig.label("> Value: 0", true);
    rig.replace_text("12");
    rig.key(in::scan::kReturn);
    CHECK(rig.workspace().graph.project.definition.on.front().body.nodes.front()
        .arguments.at(1).constant_cell().as_int() == 12);
    CHECK(rig.state().dirty);
}

TEST_CASE("Flow finds what to compose through the discovery door, by what a power is for") {
    Rig rig;
    rig.graph_semantically();
    // EVERY POWER ITS CONTRIBUTOR OFFERS, under the classification the door read off it -- and the
    // rail Flow wires from is called what it is.
    (void)rig.label("In scope");
    CHECK_FALSE(rig.shows_part("(click to wire)"));
    (void)rig.label("Operators");
    (void)rig.label("  math.max");
    const auto spent = op::invocations();

    // THE SEARCH LINE: typed text is the door's question, and the rail is the door's answer.
    rig.text("larger");
    (void)rig.label("find: larger", true);
    REQUIRE_FALSE(rig.questions.empty());
    const ws::FindPowers asked = rig.questions.back();
    CHECK(asked.text == std::optional<std::string>("larger"));
    CHECK(asked.offered == std::optional<bool>(true));
    CHECK_FALSE(asked.yields.has_value());
    REQUIRE_FALSE(rig.answers.empty());
    CHECK(rig.answers.back() == loom::serialize(loom::to_value(ws::find_powers(rig.catalog, asked))));
    (void)rig.label("  math.max");
    CHECK_FALSE(rig.shows("  logic.select_int"));

    // THE PREVIEW is the row expanded, in its contributor's words, and reading it runs nothing.
    rig.click("  math.max");
    (void)rig.label("math.max -- operator, native, from flowtest.basic");
    (void)rig.label("(lhs: Int, rhs: Int) -> result: Int");
    (void)rig.label("the larger of two integers");
    CHECK(op::invocations() == spent);

    // ADD IS TODAY'S add-node, and the node it adds is the one selected.
    rig.click("[Add]");
    const auto nodes = rig.workspace().graph.project.definition.on.front().body.nodes;
    REQUIRE(nodes.size() == 2);
    CHECK(nodes.back().identity == "math.max");
    (void)rig.label("[Delete node]");
    CHECK(op::invocations() == spent);

    // ESCAPE SHEDS ONE LAYER PER PRESS: the preview, the node, the search line -- and only then is
    // the pane put down.
    rig.key(in::scan::kEscape);
    CHECK_FALSE(rig.shows("the larger of two integers"));
    (void)rig.label("[Delete node]");
    rig.key(in::scan::kEscape);
    CHECK_FALSE(rig.shows("[Delete node]"));
    (void)rig.label("find: larger", true);
    rig.key(in::scan::kEscape);
    CHECK_FALSE(rig.questions.back().text.has_value());
    CHECK(escapes_unspent(*rig.presenter) == 0);
    rig.key(in::scan::kEscape);
    CHECK(escapes_unspent(*rig.presenter) == 1);
}

TEST_CASE("a selected port lists what could fill it: in scope, a typed constant, a Source, then "
          "operators that yield its type") {
    Rig rig;
    rig.graph_semantically();
    // A SOURCE MOUNTED AFTER THE PANE READ ITS PORTS: the door finds it all the same.
    REQUIRE(rig.catalog.mount("flowtest.sources", {answer_source()}));
    rig.edit_ok("add-node", rig.ref("math.max"));
    rig.click("o rhs = [unwired]");
    (void)rig.label("For %1 rhs : Int");
    REQUIRE_FALSE(rig.questions.empty());
    CHECK(rig.questions.back().yields == std::optional<std::string>("Int"));
    CHECK(rig.questions.back().offered == std::optional<bool>(true));

    // IN THE ORDER A MAKER REACHES FOR THEM.
    const std::vector<std::int64_t> order{
        rig.label("In scope").y, rig.label("  state.value").y, rig.label("  input.input").y,
        rig.label("  %0 math.max").y, rig.label("Constant").y, rig.label("[Int constant]").y,
        rig.label("Sources").y, rig.label("  flowtest.answer").y, rig.label("Operators").y,
        rig.label("  math.max").y};
    CHECK(std::is_sorted(order.begin(), order.end()));
    CHECK(std::adjacent_find(order.begin(), order.end()) == order.end());
    // ...and nothing that yields another type.
    CHECK_FALSE(rig.shows("  logic.select_bool"));

    // A CANDIDATE IN SCOPE FILLS THE PORT through today's bind, and the port is put down.
    rig.click("  %0 math.max");
    const auto body = rig.workspace().graph.project.definition.on.front().body;
    REQUIRE(body.nodes.size() == 2);
    CHECK(body.nodes.at(1).arguments.at(1).from() == op::Binding::From::Node);
    CHECK(body.nodes.at(1).arguments.at(1).node_index() == 0);
    CHECK_FALSE(rig.shows("For %1 rhs : Int"));

    // A FOUND SOURCE THE GRAPH'S PORTS DID NOT YET DESCRIBE is read from the host, then added.
    rig.click("  flowtest.answer");
    (void)rig.label("flowtest.answer -- source, native, from flowtest.sources");
    (void)rig.label("forty-two, whenever it is asked");
    const auto before = op::invocations();
    rig.click("[Add]");
    const auto grown = rig.workspace().graph.project.definition.on.front().body;
    REQUIRE(grown.nodes.size() == 3);
    CHECK(grown.nodes.back().identity == "flowtest.answer");
    CHECK(op::invocations() == before);
}

TEST_CASE("an Add waiting on the host for an operator's ports is refused when the graph changed "
          "meanwhile, so it never lands on the node now at that place") {
    Rig rig;
    rig.graph_semantically();
    REQUIRE(rig.catalog.mount("flowtest.sources", {answer_source()}));
    rig.edit_ok("add-node", rig.ref("math.max"));
    rig.click("o rhs = [unwired]");
    rig.click("  flowtest.answer");
    const auto press = rig.press_for("[Add into %1 rhs]");

    // THE ADD ASKS THE HOST, and before its answer comes, %1 is removed and another takes its place.
    rig.post(press);
    rig.bus.pump_pending();
    rig.post(fp::FlowEdit{"remove", {"1"}}, ++rig.correlation);
    rig.post(fp::FlowEdit{"add-node", rig.ref("math.max")}, ++rig.correlation);
    rig.pump();
    const auto after = rig.workspace();
    const auto& nodes = after.graph.project.definition.on.front().body.nodes;
    REQUIRE(nodes.size() == 2);
    CHECK(nodes.at(1).identity == "math.max");
    CHECK(rig.shows("The graph changed while the ports of flowtest.answer were read; Add it again"));
}

TEST_CASE("Flow never offers a running definition's trigger body, which the door lists as not offered") {
    Rig rig;
    rig.graph_semantically();
    rig.edit_ok("run");
    // THE BODY IS MOUNTED, and the door, asked for everything, lists it -- saying it is not offered.
    const ws::PowersFound everything = ws::find_powers(rig.catalog, ws::FindPowers{});
    const auto body = std::find_if(everything.rows.begin(), everything.rows.end(),
        [](const ws::PowerRow& row) { return row.identity.rfind("meter.r", 0) == 0; });
    REQUIRE(body != everything.rows.end());
    CHECK_FALSE(body->offered);

    // FLOW ASKS ONLY FOR WHAT IS OFFERED, so no participant's own reaction is in its rail.
    rig.text("meter");
    REQUIRE_FALSE(rig.questions.empty());
    CHECK(rig.questions.back().offered == std::optional<bool>(true));
    CHECK_FALSE(rig.shows("  " + body->identity));
    CHECK_FALSE(rig.shows("  " + body->identity + " (composite)"));
    (void)rig.label("none offered match");
}

TEST_CASE("a search the door refuses is said in the door's words, never as an empty match") {
    Rig rig;
    rig.graph_semantically();
    rig.text(std::string(ws::kMaxPowersQueryBytes + 1, 'x'));
    REQUIRE_FALSE(rig.answers.empty());
    const auto refused = rig.label("a query carries", true);
    CHECK(refused.role == zengine::graphics::role::kAlert);
    CHECK_FALSE(rig.shows("none offered match"));
}

TEST_CASE("Add places a found operator before the selected node, or before the node whose port it "
          "fills and into that port, renumbering what follows") {
    Rig rig;
    rig.graph_semantically();
    rig.text("larger");
    rig.click("%0 math.max");
    rig.click("  math.max");
    rig.click("[Add before %0]");
    auto body = rig.workspace().graph.project.definition.on.front().body;
    REQUIRE(body.nodes.size() == 2);
    CHECK(body.result_node == 1); // the node that was %0 is %1, and still the result
    CHECK(body.nodes.at(1).arguments.at(0).input_name() == "input");

    // INTO A PORT: the port being filled names the node to place before, and Add wires it there.
    rig.click("o rhs = 0");
    (void)rig.label("For %1 rhs : Int");
    rig.click("  math.max");
    rig.click("[Add into %1 rhs]");
    body = rig.workspace().graph.project.definition.on.front().body;
    REQUIRE(body.nodes.size() == 3);
    CHECK(body.nodes.at(2).arguments.at(1).from() == op::Binding::From::Node);
    CHECK(body.nodes.at(2).arguments.at(1).node_index() == 1);
    CHECK(body.result_node == 2);
    CHECK_FALSE(rig.shows("For %2 rhs : Int"));
}

TEST_CASE("the tally composed in the pane: the fold found by what it is for, placed with its step "
          "at 1, its body chosen from the slot by its count port, and Run answers 45") {
    Rig rig;
    rig.edit_ok("new", {"tally", "discard"});
    rig.edit_ok("state-field", {"total", "Int", "required"});
    rig.edit_ok("message", {"tally.panel.Count"});
    for (const char* field : {"start", "limit", "step"})
        rig.edit_ok("message-field", {"0", field, "Int", "required"});
    rig.edit_ok("trigger", {"0", "total"});

    // FOUND BY WHAT IT IS FOR, a form ahead of the operators, and placed with its step at 1.
    rig.text("count");
    (void)rig.label("Forms");
    rig.click("  fold");
    (void)rig.label("fold -- a form the evaluator spends");
    rig.click("[Add]");
    (void)rig.label("%0 fold");
    (void)rig.label("o step = 1");

    // THE BODY SLOT asks the door for what a fold could spend, and the maker names the count's port.
    rig.key(in::scan::kEscape); // the preview
    rig.key(in::scan::kEscape); // the node
    rig.key(in::scan::kEscape); // the search line
    rig.click("body = [choose]");
    REQUIRE_FALSE(rig.questions.empty());
    CHECK(rig.questions.back().fits == std::optional<std::string>("fold"));
    (void)rig.label("For %0 fold's body");
    CHECK(rig.shows("  logic.select_int"));
    CHECK_FALSE(rig.shows("  compare.less_int"));
    rig.click("  math.add");
    (void)rig.label("[count lhs, acc rhs]");
    rig.click("[count rhs, acc lhs]");
    (void)rig.label("%0 fold math.add");
    (void)rig.label("count rhs, acc lhs");
    CHECK_FALSE(rig.shows("For %0 fold's body"));

    rig.edit_ok("bind", {"0", "0", "$start"});
    rig.edit_ok("bind", {"0", "1", "$limit"});
    rig.edit_ok("bind", {"0", "3", "0"});
    rig.edit_ok("result", {"0"});
    rig.edit_ok("run");
    rig.edit_ok("message-open", {"0"});
    rig.edit_ok("value", {"0", "0"});
    rig.edit_ok("value", {"1", "10"});
    rig.edit_ok("value", {"2", "1"});
    rig.edit_ok("send");
    const auto subject = rig.bus.role_holder("tally");
    REQUIRE(subject.valid());
    CHECK(rig.bus.weave(subject)->snapshot().get("total")->as_int() == 45);

    // SAY THE ANSWER: an emitted message authored here, its field from state.total, and seen in
    // Events as what the participant published. A new emitted shape changes what it may say, so
    // it is stopped and run again rather than applied.
    rig.edit_ok("emitted-message", {"Total"});
    rig.edit_ok("emitted-field", {"0", "total", "Int", "required"});
    rig.edit_ok("emit", {"0"});
    rig.edit_ok("stop");
    rig.edit_ok("run");
    rig.edit_ok("message-open", {"0"});
    rig.edit_ok("send");
    rig.click("[Events]");
    (void)rig.label("state.total = 45");
    CHECK(rig.shows_part("tally.Total"));
    (void)rig.label("  total = 45");

    // GENERATION REFUSES A FOLD in words, and writes nothing.
    const auto out = std::filesystem::temp_directory_path() / "zengine-flow-pane-fold-generated";
    std::error_code ignored;
    std::filesystem::remove_all(out, ignored);
    const auto generated = rig.edit("generate", {out.string()});
    CHECK_FALSE(generated.ok);
    CHECK(generated.reason == "the trigger on tally.panel.Count folds at node 0: a fold is spent by "
                              "the evaluator, so this definition is not generated as C++ and runs "
                              "interpreted");
    CHECK_FALSE(std::filesystem::exists(out));
}

TEST_CASE("a dropped operator reference becomes a node: where it was released, into the port it "
          "lands on, or as the body the fold's slot then offers; a stale one is refused") {
    Rig rig;
    rig.graph_semantically();
    // WHERE IT WAS RELEASED: a step at the end, its box placed at the drop.
    const auto& graph = rig.label("[Reset view]");
    rig.drop(rig.reference("math.max"), graph.x - 40 * unit, graph.y - 12 * unit);
    auto body = rig.workspace().graph.project.definition.on.front().body;
    REQUIRE(body.nodes.size() == 2);
    CHECK(body.nodes.at(1).identity == "math.max");
    (void)rig.label("%1 math.max");

    // INTO THE PORT IT LANDS ON: placed before that port's node and wired there.
    rig.drop_on(rig.reference("math.max"), "o rhs = 0");
    body = rig.workspace().graph.project.definition.on.front().body;
    REQUIRE(body.nodes.size() == 3);
    CHECK(body.nodes.at(1).arguments.at(1).from() == op::Binding::From::Node);
    CHECK(body.nodes.at(1).arguments.at(1).node_index() == 0);

    // STALE: its ports changed since it was found, so it is refused and nothing is added.
    rig.drop(op::encode_reference({"math.max", 1, 2}), graph.x - 40 * unit, graph.y - 12 * unit);
    CHECK(rig.workspace().graph.project.definition.on.front().body.nodes.size() == 3);
    (void)rig.label("'math.max' is not the operator this reference was found at: its ports "
                    "changed since; find it again");

    // ON A FOLD'S BODY SLOT: the slot opens with the operator found, for the maker to name the count.
    rig.edit_ok("add-fold");
    rig.drop_on(rig.reference("math.add"), "body = [choose]");
    (void)rig.label("For %3 fold's body");
    (void)rig.label("[count rhs, acc lhs]");
}

TEST_CASE("a stale reference dropped on a fold's body slot is refused in words, never installed "
          "at the ports the door holds now") {
    Rig rig;
    rig.graph_semantically();
    rig.edit_ok("add-fold");
    rig.drop_on(op::encode_reference({"math.add", 1, 2}), "body = [choose]");
    // Whatever the slot then offers carries the reference that was dropped, not the door's.
    if (rig.shows("[count rhs, acc lhs]")) rig.click("[count rhs, acc lhs]");
    const auto after = rig.workspace();
    CHECK(after.graph.project.definition.on.front().body.nodes.at(1).identity != "math.add");
    (void)rig.label("'math.add' is not the operator this reference was found at: its ports "
                    "changed since; find it again");

    // FOUND WHILE CURRENT, then reshaped before the body is chosen: the dropped one is refused,
    // never the reshaped one installed in its place.
    rig.drop_on(rig.reference("math.add"), "body = [choose]");
    (void)rig.label("[count rhs, acc lhs]");
    REQUIRE(rig.catalog.unmount("flowtest.basic"));
    REQUIRE(rig.catalog.mount("flowtest.reshaped",
        {op::make_operator<&op::add_int>(op::kAddInt, {"lhs", "rhs"}, "sum")}));
    rig.edit_ok("catalog");
    rig.click("[count rhs, acc lhs]");
    const auto reshaped = rig.workspace();
    CHECK(reshaped.graph.project.definition.on.front().body.nodes.at(1).identity != "math.add");
    (void)rig.label("'math.add' is not the operator this reference was found at: its ports "
                    "changed since; find it again");

    // REFUSED ONCE, THEN PUT DOWN: the slot no longer offers the reference it refused, so choosing
    // again spends the row the door holds now, and the reshaped math.add is the body.
    rig.click("[count rhs, acc lhs]");
    const auto chosen = rig.workspace().graph.project.definition.on.front().body.nodes.at(1);
    CHECK(chosen.identity == "math.add");
    REQUIRE(chosen.fold.has_value());
    CHECK(chosen.fold->count == "rhs");
    (void)rig.label("%1 fold math.add");
}

TEST_CASE("a form and an operator that share a name are two rows: each previews and adds itself") {
    Rig rig;
    rig.graph_semantically();
    // AN OPERATOR NAMED LIKE THE FORM, offered beside it in one answer.
    REQUIRE(rig.catalog.mount("flowtest.named-fold",
        {op::make_operator<&op::max_int>("fold", {"lhs", "rhs"}, "result",
                                         "the larger of two, named like the form, to count")}));
    rig.edit_ok("catalog");
    rig.text("count");
    const auto listed = rig.picture().texts;
    const auto both = std::count_if(listed.begin(), listed.end(),
                                    [](const auto& t) { return t.text == "  fold"; });
    REQUIRE(both == 2);
    // The form's row, under Forms, previews and adds the form.
    rig.click("  fold");
    (void)rig.label("fold -- a form the evaluator spends");
    rig.click("[Add]");
    auto nodes = rig.workspace().graph.project.definition.on.front().body.nodes;
    REQUIRE(nodes.size() == 2);
    CHECK(nodes.back().fold.has_value());
    // The operator's row, under Operators, previews and adds the operator.
    rig.key(in::scan::kEscape); // the preview
    rig.key(in::scan::kEscape); // the node
    const auto operators = rig.label("Operators");
    const auto texts = rig.picture().texts;
    const ws::CanvasText* named = nullptr;
    for (const auto& t : texts)
        if (t.text == "  fold" && t.y > operators.y) named = &t;
    REQUIRE(named != nullptr);
    Moment press;
    press.gesture = ++rig.gesture;
    press.x = named->x + 4; press.y = named->y + 4;
    rig.host(press);
    (void)rig.label("fold -- operator, native, from flowtest.named-fold");
    rig.key(in::scan::kReturn);
    nodes = rig.workspace().graph.project.definition.on.front().body.nodes;
    REQUIRE(nodes.size() == 3);
    CHECK_FALSE(nodes.back().fold.has_value());
    CHECK(nodes.back().identity == "fold");
}

TEST_CASE("a dropped value waiting on a port is put down when another workspace replaces the "
          "graph, so its offer never lands on a node of the new one") {
    Rig rig;
    rig.edit_ok("new", {"tally", "discard"});
    rig.edit_ok("state-field", {"total", "Int", "required"});
    const auto count_schema = loom::SchemaBuilder("tally.panel.Count", 1)
        .field("start", loom::Kind::Int).field("limit", loom::Kind::Int)
        .field("step", loom::Kind::Int).build();
    loom::Value count(count_schema);
    count.set("start", loom::Cell::integer(7));
    count.set("limit", loom::Cell::integer(10));
    count.set("step", loom::Cell::integer(1));
    rig.drop(count, 40 * unit, 20 * unit);
    rig.click("[Declare tally.panel.Count v1 as an accepted message]");
    rig.edit_ok("trigger", {"0", "total"});
    rig.edit_ok("add-fold");
    rig.drop_on(count, "o start = [unwired]");
    (void)rig.label("[Use start = 7 on %0 start]");

    // The same places, numbered afresh, in a new graph: the offer is gone with the old one.
    rig.edit_ok("new", {"other", "discard"});
    rig.edit_ok("state-field", {"total", "Int", "required"});
    rig.edit_ok("message", {"Ping"});
    rig.edit_ok("trigger", {"0", "total"});
    rig.edit_ok("add-fold");
    CHECK_FALSE(rig.shows("Dropped tally.panel.Count v1"));
    CHECK_FALSE(rig.shows("[Use start = 7 on %0 start]"));
    const auto other = rig.workspace();
    const auto& fresh = other.graph.project.definition.on[0].body.nodes[0].arguments;
    REQUIRE_FALSE(fresh.empty());
    CHECK(fresh[0].from() != op::Binding::From::Constant);
}

TEST_CASE("a dropped value offers what it can be here: its shape declared, then an example to send, "
          "a field as a constant on the port it landed on; Escape puts it down") {
    Rig rig;
    rig.edit_ok("new", {"tally", "discard"});
    rig.edit_ok("state-field", {"total", "Int", "required"});
    const auto count_schema = loom::SchemaBuilder("tally.panel.Count", 1)
        .field("start", loom::Kind::Int).field("limit", loom::Kind::Int)
        .field("step", loom::Kind::Int).build();
    loom::Value count(count_schema);
    count.set("start", loom::Cell::integer(0));
    count.set("limit", loom::Cell::integer(10));
    count.set("step", loom::Cell::integer(1));

    // A NEW SHAPE IS DECLARED as it came: name, version, fields, and so its identity.
    rig.drop(count, 40 * unit, 20 * unit);
    (void)rig.label("Dropped tally.panel.Count v1");
    CHECK_FALSE(rig.shows("[Send as example]"));
    rig.click("[Declare tally.panel.Count v1 as an accepted message]");
    REQUIRE(rig.workspace().graph.project.definition.accepts.size() == 1);
    CHECK(loom::same_identity(*rig.workspace().graph.project.definition.accepts[0], *count_schema));
    CHECK_FALSE(rig.shows("Dropped tally.panel.Count v1"));

    // THE TALLY, then the same value is an example to send, and a field fills a port.
    rig.edit_ok("trigger", {"0", "total"});
    rig.edit_ok("add-fold");
    const auto add = rig.ref("math.add");
    rig.edit_ok("fold-body", {"0", add[0], add[1], add[2], "rhs", "lhs"});
    rig.edit_ok("bind", {"0", "0", "$start"});
    rig.edit_ok("bind", {"0", "1", "$limit"});
    rig.edit_ok("bind", {"0", "2", "$step"});
    rig.drop_on(count, "o initial = [unwired]");
    (void)rig.label("[Use start = 0 on %0 initial]");
    (void)rig.label("[Use limit = 10 on %0 initial]");
    rig.click("[Use start = 0 on %0 initial]");
    CHECK(rig.workspace().graph.project.definition.on[0].body.nodes[0].arguments[3].constant_cell().as_int() == 0);
    rig.edit_ok("result", {"0"});
    rig.edit_ok("run");
    rig.drop(count, 40 * unit, 20 * unit);
    rig.click("[Send as example]");
    const auto subject = rig.bus.role_holder("tally");
    REQUIRE(subject.valid());
    CHECK(rig.bus.weave(subject)->snapshot().get("total")->as_int() == 45);

    // A REFUSAL READS WHOLE: in a narrow room its Events row continues, indented, beneath itself.
    ++rig.grant;
    rig.room(90, 65);
    loom::Value zero = count;
    zero.set("step", loom::Cell::integer(0));
    rig.drop(zero, 40 * unit, 20 * unit);
    rig.click("[Send as example]");
    rig.edit_ok("inspect");
    rig.click("[Events]");
    std::string pictured;
    bool continued = false;
    for (const auto& row : rig.picture().texts) {
        continued = continued || row.text.rfind("    ", 0) == 0;
        for (const char c : row.text + " ")
            if (c != ' ' || (!pictured.empty() && pictured.back() != ' ')) pictured += c;
    }
    CHECK(continued);
    CHECK(pictured.find("a step of 0 never moves the count from 0 toward 10") != std::string::npos);
    CHECK(rig.bus.weave(subject)->snapshot().get("total")->as_int() == 45);
    ++rig.grant;
    rig.room();

    // AN EMITTED SHAPE inside the definition's namespace, and Escape puts a drop down unused.
    const auto total_schema = loom::SchemaBuilder("tally.Total", 1).field("total", loom::Kind::Int).build();
    loom::Value said(total_schema);
    said.set("total", loom::Cell::integer(0));
    rig.drop(said, 40 * unit, 20 * unit);
    (void)rig.label("[Declare tally.Total v1 as an emitted message]");
    rig.key(in::scan::kEscape);
    CHECK_FALSE(rig.shows("Dropped tally.Total v1"));
    CHECK(rig.workspace().graph.project.definition.emits.empty());
}

namespace {
using Cell = std::pair<std::int64_t, std::int64_t>;

/// THE FLOW PANE IN A WINDOW OF ITS OWN, a terminal's, its graph shown.
struct WireWindow {
    Rig& rig;
    pw::Window window;
    explicit WireWindow(Rig& r) : rig(r), window(r.bus, "flowtest") {
        rig.shown = &window; // the window that shows Flow's area now, which the rig reads
        window.room(kFlowArea, fp::kRole, 170, 65);
        rig.pump();
        REQUIRE(window.press(kFlowArea, "control:page-graph"));
        rig.pump();
    }
    /// The first cell whose press asks `part`.
    Cell at(const std::string& part) {
        const auto cell = window.cell_of(kFlowArea, part);
        INFO("no cell asks " << part);
        REQUIRE(cell.has_value());
        return *cell;
    }
    /// The first cell, in columns [c0, c1) and rows [r0, r1), where a press asks nothing.
    Cell empty(std::int64_t c0, std::int64_t c1, std::int64_t r0, std::int64_t r1) {
        for (auto r = r0; r < r1; ++r)
            for (auto c = c0; c < c1; ++c)
                if (window.action_at(kFlowArea, c, r).empty()) return {c, r};
        FAIL("no empty cell there");
        return {0, 0};
    }
    /// The first cell showing `words` right of the rail, where the graph and its search are; none
    /// when no row shows them there.
    std::optional<Cell> showing(const std::string& words) {
        const auto rows = window.rows(kFlowArea);
        for (std::size_t r = 0; r < rows.size(); ++r)
            if (const auto c = rows[r].find(words, 22); c != std::string::npos)
                return Cell{static_cast<std::int64_t>(c), static_cast<std::int64_t>(r)};
        return std::nullopt;
    }
    void press(Cell c, std::uint64_t n) {
        window.button(kFlowArea, c.first, c.second, true, n);
        rig.pump();
    }
    void move(Cell c, std::uint64_t n) {
        window.move(kFlowArea, c.first, c.second, n);
        rig.pump();
    }
    void release(Cell c, std::uint64_t n) {
        window.button(kFlowArea, c.first, c.second, false, n);
        rig.pump();
    }
    /// A WIRE DRAGGED: pressed at `from`, moved to `to`, let go there, under one number.
    void drag(Cell from, Cell to) {
        const auto n = window.next_press();
        press(from, n);
        move(to, n);
        release(to, n);
    }
    void click(Cell c) { window.click(kFlowArea, c.first, c.second); rig.pump(); }
    /// Empty graph room, well right of the nodes the fixture places.
    Cell room() { return empty(100, 140, 25, 40); }
};

std::int64_t place_id(Rig& rig, std::int64_t node) {
    for (const auto& p : rig.workspace().graph.places)
        if (p.node == node) return p.id;
    FAIL("no place for node " << node);
    return 0;
}
std::string port_part(Rig& rig, std::int64_t node, std::size_t port) {
    return "port:" + std::to_string(place_id(rig, node)) + "." + std::to_string(port);
}
const op::Binding& argument(Rig& rig, std::size_t node, std::size_t port) {
    static flow::Workspace held;
    held = rig.workspace();
    return held.graph.project.definition.on.front().body.nodes.at(node).arguments.at(port);
}
bool from_node(const op::Binding& b, std::size_t n) {
    return b.from() == op::Binding::From::Node && b.node_index() == n;
}
} // namespace

TEST_CASE("a wire dragged from an output and let go on empty graph room asks what takes its type, "
          "a row a port, and a row chosen makes the node where the hand let go, wired from it") {
    SUBCASE("in a terminal's window, by its cells") {
        Rig rig;
        rig.graph_semantically();
        WireWindow w(rig);
        const auto before = rig.workspace();
        const auto room = w.room();
        w.drag(w.at("source-node:0"), room);

        // THE SEARCH AT THE HAND, in the window's rows and its picture alike, its question the
        // output's type as what a power takes.
        CHECK(w.showing("From %0 : Int -- what takes it"));
        CHECK(rig.shows("From %0 : Int -- what takes it"));
        REQUIRE_FALSE(rig.questions.empty());
        CHECK(rig.questions.back().takes == std::optional<std::string>("Int"));
        CHECK_FALSE(rig.questions.back().yields.has_value());
        // ...A ROW A PORT of that type, and nothing made yet.
        const auto lhs = w.showing("math.add -> lhs");
        const auto rhs = w.showing("math.add -> rhs");
        REQUIRE(lhs);
        REQUIRE(rhs);
        CHECK(w.showing("math.max -> lhs"));
        CHECK(lhs->first == rhs->first);
        CHECK(lhs->second != rhs->second);
        CHECK(flow::workspace_bytes(rig.workspace()) == flow::workspace_bytes(before));

        // CHOSEN: the node made after what it reads, its chosen port wired from it, in one edit.
        w.click(*rhs);
        const auto after = rig.workspace();
        const auto& body = after.graph.project.definition.on.front().body;
        REQUIRE(body.nodes.size() == 2);
        CHECK(body.nodes[1].identity == "math.add");
        CHECK(from_node(body.nodes[1].arguments.at(1), 0));
        CHECK(body.nodes[1].arguments.at(0).from() == op::Binding::From::Input);
        CHECK(body.nodes[1].arguments.at(0).input_name().empty());
        CHECK(rig.shows("Added %1 math.add, wired from %0"));
        CHECK(rig.shows_part("Delete node")); // the node made is selected
        CHECK_FALSE(w.showing("what takes it"));
        // ...WHERE THE HAND LET GO: its header stands at the release's cell.
        const auto header = w.at("node:" + std::to_string(place_id(rig, 1)));
        CHECK(std::abs(header.first - room.first) <= 1);
        CHECK(std::abs(header.second - room.second) <= 1);

        // IN A SHORT WINDOW the list keeps to the graph's room: no row of it reaches the two
        // lines at the foot, the draft's and the notice.
        w.window.room(kFlowArea, fp::kRole, 170, 16, 2);
        rig.pump();
        w.drag(w.at("source-field:input"), w.empty(60, 140, 4, 8));
        REQUIRE(w.showing("what takes it"));
        const auto shown = w.window.rows(kFlowArea);
        REQUIRE(shown.size() >= 2);
        for (std::size_t r = shown.size() - 2; r < shown.size(); ++r)
            CHECK(shown[r].find(" -> ") == std::string::npos);
    }
    SUBCASE("in the rig's window, by its pixels") {
        Rig rig;
        rig.graph_semantically();
        auto press = rig.press_for("[-> state.value]");
        rig.host(press);
        auto motion = press;
        motion.phase = ws::canvas_pointer::kMove;
        motion.x = 125 * unit;
        motion.y = 32 * unit;
        rig.host(motion);
        auto release = motion;
        release.phase = ws::canvas_pointer::kRelease;
        rig.host(release);
        CHECK(rig.shows("From %0 : Int -- what takes it"));
        CHECK(rig.questions.back().takes == std::optional<std::string>("Int"));
        rig.click("  math.add -> rhs");
        const auto after = rig.workspace();
        const auto& body = after.graph.project.definition.on.front().body;
        REQUIRE(body.nodes.size() == 2);
        CHECK(body.nodes[1].identity == "math.add");
        CHECK(from_node(body.nodes[1].arguments.at(1), 0));
        const auto title = rig.label("%1 math.add");
        CHECK(std::abs(title.x - motion.x) <= unit);
        CHECK(std::abs(title.y - motion.y) <= unit);
    }
}

TEST_CASE("a wire dragged from an input port and let go on empty graph room asks what fills it: in "
          "scope, a constant, then what yields its type, each made at the hand") {
    {
        Rig rig;
        rig.graph_semantically();
        WireWindow w(rig);
        const auto room = w.room();
        w.drag(w.at(port_part(rig, 0, 1)), room);
        CHECK(rig.shows("Into %0 rhs : Int -- what fills it"));
        REQUIRE_FALSE(rig.questions.empty());
        CHECK(rig.questions.back().yields == std::optional<std::string>("Int"));
        CHECK_FALSE(rig.questions.back().takes.has_value());
        // IN THE ORDER A MAKER REACHES FOR THEM.
        const auto scope = w.showing("input.input");
        const auto constant = w.showing("Int constant");
        const auto found = w.showing("  math.add");
        REQUIRE(scope);
        REQUIRE(constant);
        REQUIRE(found);
        CHECK(scope->second < constant->second);
        CHECK(constant->second < found->second);

        // AN IN SCOPE ROW binds the port and makes nothing.
        w.click(*scope);
        CHECK(rig.workspace().graph.project.definition.on.front().body.nodes.size() == 1);
        CHECK(argument(rig, 0, 1).input_name() == "input");

        // A FOUND POWER is placed before the port's node, wired into the port, at the hand.
        w.drag(w.at(port_part(rig, 0, 1)), room);
        const auto add = w.showing("  math.add");
        REQUIRE(add);
        w.click(*add);
        const auto made = rig.workspace();
        const auto& body = made.graph.project.definition.on.front().body;
        REQUIRE(body.nodes.size() == 2);
        CHECK(body.nodes[0].identity == "math.add");
        CHECK(from_node(body.nodes[1].arguments.at(1), 0));
        CHECK(rig.shows("Added %0 math.add, wired into %1 rhs"));
        const auto header = w.at("node:" + std::to_string(place_id(rig, 0)));
        CHECK(std::abs(header.first - room.first) <= 1);
        CHECK(std::abs(header.second - room.second) <= 1);

        // THE CONSTANT ROW opens the binding editor, as the rail's does.
        w.drag(w.at(port_part(rig, 1, 0)), w.room());
        const auto typed = w.showing("Int constant");
        REQUIRE(typed);
        w.click(*typed);
        (void)rig.label("> Value: 0", true);
    }
}

TEST_CASE("a wire let go on the other end binds from either end; before the threshold or back on "
          "its origin it was a click; anywhere off the graph it is put down") {
    Rig rig;
    rig.graph_semantically();
    rig.edit_ok("add-node", rig.ref("math.add"));
    WireWindow w(rig);

    // FROM AN OUTPUT ONTO A PORT, the port lit while the wire is over it.
    {
        const auto n = w.window.next_press();
        const auto port = w.at(port_part(rig, 1, 0));
        CHECK(rig.label("o lhs = [unwired]").role == gfx::role::kFill);
        w.press(w.at("source-node:0"), n);
        w.move(port, n);
        CHECK(rig.label("o lhs = [unwired]").role == gfx::role::kAccent);
        w.release(port, n);
    }
    CHECK(from_node(argument(rig, 1, 0), 0));
    CHECK_FALSE(rig.shows_part("what takes it"));

    // FROM A PORT ONTO AN OUTPUT.
    w.drag(w.at(port_part(rig, 1, 1)), w.at("source-node:0"));
    CHECK(from_node(argument(rig, 1, 1), 0));

    // FROM A PORT ONTO WHAT IS IN SCOPE: the press selected the port, so the rail lists it.
    {
        const auto n = w.window.next_press();
        w.press(w.at(port_part(rig, 1, 1)), n);
        (void)rig.label("For %1 rhs : Int");
        const auto field = w.at("scope:1:1:$input");
        w.move(field, n);
        w.release(field, n);
        CHECK(argument(rig, 1, 1).input_name() == "input");
    }

    const auto bound = rig.workspace();
    // BEFORE THE THRESHOLD: a click, the wire in hand, no list.
    {
        const auto n = w.window.next_press();
        const auto out = w.at("source-node:0");
        w.press(out, n);
        w.release(out, n);
        CHECK(rig.shows("Choose an input port to connect node 0"));
        CHECK_FALSE(rig.shows_part("what takes it"));
        rig.key(in::scan::kEscape);
    }
    // MOVED AWAY AND BACK ON ITS ORIGIN: a click all the same.
    {
        const auto n = w.window.next_press();
        const auto out = w.at("source-node:0");
        w.press(out, n);
        w.move(w.room(), n);
        w.move(out, n);
        w.release(out, n);
        CHECK(rig.shows("Choose an input port to connect node 0"));
        CHECK_FALSE(rig.shows_part("what takes it"));
        rig.key(in::scan::kEscape);
    }
    // OFF THE GRAPH, on the rail's empty room: put down, nothing made.
    w.drag(w.at("source-node:0"), w.empty(1, 20, 40, 60));
    CHECK(rig.shows("The wire was put down: let go off the graph; nothing was made"));
    CHECK(flow::workspace_bytes(rig.workspace()) == flow::workspace_bytes(bound));
    // ...and nothing is left in hand: a press on a port now selects it rather than binding it.
    w.click(w.at(port_part(rig, 1, 0)));
    (void)rig.label("For %1 lhs : Int");
}

TEST_CASE("every way out of a wire's search makes nothing: Escape, a press outside its list, which "
          "is spent on nothing more, and an edit from elsewhere") {
    Rig rig;
    rig.graph_semantically();
    WireWindow w(rig);
    // Nothing selected, so a press spent on a node would show.
    for (int put_down = 0; put_down < 4 && rig.shows_part("Delete node"); ++put_down)
        rig.key(in::scan::kEscape);
    REQUIRE_FALSE(rig.shows_part("Delete node"));
    const auto before = flow::workspace_bytes(rig.workspace());
    const auto escapes = escapes_unspent(*rig.presenter);
    // NOTHING HALF MADE: a node whose wire is refused is not made either.
    auto refused = rig.ref("math.add");
    refused.insert(refused.end(), {"%0", "9", "0", "0"});
    CHECK_FALSE(rig.edit("add-node-from", refused).ok);
    CHECK(flow::workspace_bytes(rig.workspace()) == before);

    // ESCAPE closes it and puts the wire down; the pane keeps the key.
    w.drag(w.at("source-node:0"), w.room());
    REQUIRE(rig.shows("From %0 : Int -- what takes it"));
    rig.key(in::scan::kEscape);
    CHECK_FALSE(rig.shows_part("what takes it"));
    CHECK(rig.shows("The wire was put down: Escape; nothing was made"));
    CHECK(escapes_unspent(*rig.presenter) == escapes);
    CHECK(flow::workspace_bytes(rig.workspace()) == before);
    // ...nothing is left in hand: a press on a port selects it rather than binding it.
    w.click(w.at(port_part(rig, 0, 1)));
    (void)rig.label("For %0 rhs : Int");
    CHECK(flow::workspace_bytes(rig.workspace()) == before);
    rig.key(in::scan::kEscape);

    // A PRESS INSIDE THE LIST on none of its rows keeps it open.
    w.drag(w.at("source-node:0"), w.room());
    const auto heading = w.showing("From %0 : Int");
    REQUIRE(heading);
    w.click(*heading);
    CHECK(rig.shows("From %0 : Int -- what takes it"));
    // A PRESS OUTSIDE IT closes it, and is not also a press on the node it landed on.
    w.click(w.at("node:" + std::to_string(place_id(rig, 0))));
    CHECK_FALSE(rig.shows_part("what takes it"));
    CHECK_FALSE(rig.shows_part("Delete node"));
    CHECK(rig.shows("The wire was put down: a press outside its list; nothing was made"));
    CHECK(flow::workspace_bytes(rig.workspace()) == before);

    // TYPED TEXT narrows the list; Return makes nothing when no row is left.
    w.drag(w.at("source-node:0"), w.room());
    rig.text("zzzz");
    CHECK(w.showing("find: zzzz"));
    rig.key(in::scan::kReturn);
    CHECK(flow::workspace_bytes(rig.workspace()) == before);
    // AN EDIT FROM ELSEWHERE closes it: the graph it was let go on has changed.
    rig.edit_ok("add-node", rig.ref("math.add"));
    CHECK_FALSE(rig.shows_part("what takes it"));
    CHECK(rig.workspace().graph.project.definition.on.front().body.nodes.size() == 2);
}

TEST_CASE("ctrl+n's find-wire opens the same search for a wire held by clicks or a port selected, "
          "the keys choose in it, and a row whose ports changed since is refused") {
    Rig rig;
    rig.graph_semantically();
    WireWindow w(rig);
    const auto before = flow::workspace_bytes(rig.workspace());

    // NOTHING HELD: said, nothing opened.
    rig.host(ws::PaneActionRequested{fp::kPane, "find-wire"});
    CHECK(rig.shows_part("Hold a wire first"));
    CHECK_FALSE(rig.shows_part("what takes it"));

    // A WIRE HELD BY A CLICK on an output.
    w.click(w.at("source-node:0"));
    rig.host(ws::PaneActionRequested{fp::kPane, "find-wire"});
    CHECK(rig.shows("From %0 : Int -- what takes it"));
    CHECK(rig.questions.back().takes == std::optional<std::string>("Int"));
    rig.key(in::scan::kEscape);

    // A PORT SELECTED: the keys move to the constant and choose it.
    w.click(w.at(port_part(rig, 0, 1)));
    rig.host(ws::PaneActionRequested{fp::kPane, "find-wire"});
    REQUIRE(rig.shows("Into %0 rhs : Int -- what fills it"));
    CHECK(w.showing("> state.value"));
    rig.key(in::scan::kDown);
    rig.key(in::scan::kDown);
    CHECK(w.showing("> Int constant"));
    rig.key(in::scan::kReturn);
    (void)rig.label("> Value: 0", true);
    rig.key(in::scan::kEscape);
    CHECK(flow::workspace_bytes(rig.workspace()) == before);

    // A ROW WHOSE PORTS CHANGED since Flow read them: the door finds math.add as it is now, Flow
    // read it as it was, and the row is refused in words, nothing made.
    REQUIRE(rig.catalog.unmount("flowtest.basic"));
    REQUIRE(rig.catalog.mount("flowtest.reshaped",
        {op::make_operator<&op::add_int>(op::kAddInt, {"lhs", "rhs"}, "sum")}));
    w.drag(w.at("source-node:0"), w.room());
    const auto row = w.showing("math.add -> lhs");
    REQUIRE(row);
    w.click(*row);
    (void)rig.label("'math.add' is not the operator this reference was found at: its ports "
                    "changed since; find it again");
    CHECK(flow::workspace_bytes(rig.workspace()) == before);
}
