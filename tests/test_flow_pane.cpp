// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow-pane/vocabulary.hpp"
#include "flow-host/runtime.hpp"
#include "flow/workspace.hpp"
#include "input/vocabulary.hpp"
#include "inventory/codec.hpp"
#include "operator/primitives.hpp"
#include "operator/reference.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/pane_canvas_vocabulary.hpp"
#include "workshop/powers_door.hpp"
#include "lifecycle_door.hpp"
#include <zen/kernel/kernel.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {
namespace fp = zengine::flow_pane;
namespace fh = zengine::flow_host;
namespace flow = zengine::flow;
namespace ws = zengine::workshop;
namespace in = zengine::input;
namespace op = zengine::op;
constexpr const char* workshop_role = "zengine.workshop";
constexpr auto unit = ws::kPaneCanvasUnit;

// A real office on the real bus, standing in for Workshop's presenter. It captures
// the artifact's pictures; clicks target their published labels, never a second
// copy of the pane's hit-map implementation.
class Presenter final : public loom::Weave {
public:
    std::vector<loom::Message> messages;
    std::vector<ws::PaneCanvasContent> pictures;
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<ws::PaneOffered>(), loom::schema_of<ws::PaneActions>(),
            loom::schema_of<ws::PaneContent>(), loom::schema_of<ws::PaneCanvasContent>(),
            loom::schema_of<ws::PaneEscapeUnspent>(), loom::schema_of<ws::PaneQuitAnswered>(),
            loom::schema_of<ws::PanePassRequested>(), loom::schema_of<fp::FlowEdited>()};
    }
    void handle(const loom::Message& message, loom::Bus&) override {
        if (loom::same_identity(message.payload.schema(), *loom::schema_of<ws::PaneCanvasContent>())) {
            REQUIRE(message.provenance.authored_from_role(fp::kRole));
            pictures.push_back(loom::from_value<ws::PaneCanvasContent>(message.payload));
        }
        messages.push_back(message);
    }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("flowtest.Presenter", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
};

struct Rig {
    op::Catalog catalog;
    loom::Switchboard bus;
    loom::Kernel kernel{bus, loom::trust_every_artifact("the fixture selects one Flow pane artifact")};
    fh::RuntimeHost runtime{bus, catalog};
    Presenter* presenter = nullptr;
    loom::WeaveId workshop{}, pane{}, door{}, finder{};
    std::uint64_t correlation = 0;
    std::int64_t activation = 0, grant = 1, gesture = 0;
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
        finder = bus.register_weave(std::move(discovery), std::move(say), std::string(ws::kPowersRole));
        discovery_raw->zen_set_self(finder);
        auto presentation = std::make_unique<Presenter>();
        presenter = presentation.get();
        loom::Grant host_grant;
        for (const auto& schema : {loom::schema_of<ws::PaneCatalogRequested>(),
                loom::schema_of<ws::PaneRoom>(), loom::schema_of<ws::PaneCanvasRoom>(),
                loom::schema_of<ws::PaneCanvasPointer>(), loom::schema_of<ws::PaneKey>(),
                loom::schema_of<ws::PaneTextInput>(), loom::schema_of<ws::PaneCanvasValueDrop>(),
                loom::schema_of<fp::FlowEdit>()})
            host_grant.allow_to_role(schema->name(), schema->version(), fp::kRole);
        host_grant.allow_to_any(ws::PaneQuitRequested::zen_name, ws::PaneQuitRequested::zen_version);
        workshop = bus.register_weave(std::move(presentation), std::move(host_grant), workshop_role);
        loom::Grant pane_grant;
        fh::allow_flow_requests(pane_grant);
        ws::allow_finding_powers(pane_grant);
        for (const auto& schema : {loom::schema_of<ws::PaneOffered>(), loom::schema_of<ws::PaneActions>(),
                loom::schema_of<ws::PaneContent>(), loom::schema_of<ws::PaneCanvasContent>(),
                loom::schema_of<ws::PaneEscapeUnspent>(), loom::schema_of<ws::PanePassRequested>()})
            pane_grant.allow_to_role(schema->name(), schema->version(), workshop_role);
        // Answers target the concrete requester; a role-addressed grant would not
        // authorize mail.answer(), even when that requester holds Workshop.
        pane_grant.allow(ws::PaneQuitAnswered::zen_name, ws::PaneQuitAnswered::zen_version, workshop);
        pane_grant.allow_to_any(fp::FlowEdited::zen_name, fp::FlowEdited::zen_version);
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
    void room() {
        host(ws::PaneCanvasRoom{fp::kPane, grant, 170 * unit, 65 * unit, unit, true});
        REQUIRE_FALSE(presenter->pictures.empty());
    }
    const ws::PaneCanvasContent& picture() const {
        REQUIRE_FALSE(presenter->pictures.empty());
        return presenter->pictures.back();
    }
    ws::PaneCanvasText label(const std::string& text, bool prefix = false) const {
        for (const auto& row : picture().texts)
            if (prefix ? row.text.rfind(text, 0) == 0 : row.text == text) return row;
        INFO("Missing pictured label: " << text);
        for (const auto& row : picture().texts) INFO(row.text);
        REQUIRE(false);
        return {};
    }
    bool shows(const std::string& text) const {
        const auto& texts = picture().texts;
        return std::any_of(texts.begin(), texts.end(), [&](const auto& row) { return row.text == text; });
    }
    bool shows_part(const std::string& text) const {
        const auto& texts = picture().texts;
        return std::any_of(texts.begin(), texts.end(),
                           [&](const auto& row) { return row.text.find(text) != std::string::npos; });
    }
    ws::PaneCanvasPointer press_for(const std::string& text, bool prefix = false) {
        const auto row = label(text, prefix);
        ws::PaneCanvasPointer event;
        event.pane = fp::kPane; event.grant = grant; event.picture = picture().picture;
        event.gesture = ++gesture; event.phase = ws::canvas_pointer::kPress;
        event.button = 1; event.x = row.x + 4; event.y = row.y + 4; event.keys_went_here = true;
        return event;
    }
    void click(const std::string& text, bool prefix = false) { host(press_for(text, prefix)); }
    /// What Workshop delivers when a person drops a carried value on the picture at (x, y).
    void drop(const loom::Value& value, std::int64_t x, std::int64_t y) {
        const auto bytes = zengine::inventory::encode_pair(value, {});
        host(ws::PaneCanvasValueDrop{fp::kPane, grant, picture().picture, x, y,
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
    authority.allow_to_role(ws::PaneCanvasPointer::zen_name, ws::PaneCanvasPointer::zen_version, fp::kRole);
    const auto impostor = rig.bus.register_weave(std::move(foreign), std::move(authority));
    rig.click("  math.max");
    const auto before = rig.state().workspace;
    const auto add = rig.press_for("[Add]");
    rig.bus.send_as_to_role(impostor, fp::kRole, loom::Message(loom::to_value(add), impostor));
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
    const auto pictures = rig.presenter->pictures.size();
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
    CHECK(rig.presenter->pictures.size() == pictures);
    CHECK(rig.state().page == before.page);
    CHECK(rig.state().workspace == before.workspace);
}

TEST_CASE("loaded Flow pane binds gestures to the pictured room definition and interaction context") {
    Rig rig;
    rig.graph_semantically();
    rig.click("  math.max");
    auto stale_room = rig.press_for("[Add]");
    ++rig.grant;
    rig.room();
    const auto before_room = rig.state().workspace;
    rig.host(stale_room);
    CHECK(rig.state().workspace == before_room);
    auto stale_definition = rig.press_for("[Add]");
    rig.edit_ok("bind", {"0", "1", "4"});
    const auto before_definition = rig.state().workspace;
    rig.host(stale_definition);
    CHECK(rig.state().workspace == before_definition);

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
    CHECK(rig.workspace().graph.places.front().x == start.x + 97);
    CHECK(rig.workspace().graph.places.front().y == start.y + 31);
    auto release = motion; release.phase = ws::canvas_pointer::kRelease;
    rig.host(release);
    motion.x += 77;
    rig.host(motion);
    CHECK(rig.workspace().graph.places.front().x == start.x + 97);

    drag = rig.press_for("%0 math.max", true);
    rig.host(drag);
    rig.click("[State]");
    const auto hidden = rig.workspace().graph.places.front();
    motion = drag; motion.phase = ws::canvas_pointer::kMove; motion.x += 150;
    rig.host(motion);
    CHECK(rig.workspace().graph.places.front().x == hidden.x);

    ws::PaneCanvasPointer old_wheel;
    old_wheel.pane = fp::kPane; old_wheel.grant = rig.grant;
    old_wheel.picture = rig.picture().picture;
    old_wheel.phase = ws::canvas_pointer::kWheel;
    old_wheel.x = 30 * unit; old_wheel.y = 10 * unit; old_wheel.dy = -1;
    rig.click("[Graph]");
    const auto pan = rig.workspace().pan_y;
    rig.host(old_wheel);
    CHECK(rig.workspace().pan_y == pan);

    rig.click("[Save*]");
    const auto old_confirm = rig.press_for("[Confirm]");
    rig.click("[Cancel]");
    rig.click("[New]");
    rig.host(old_confirm);
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
    rig.click("input : Int = ", true);
    const auto stale_field = rig.press_for("  Row:", true);
    rig.key(in::scan::kEscape);
    const auto before_stale = rig.state();
    REQUIRE(before_stale.dialog_entries.empty());
    rig.host(stale_field);
    const auto after_stale = rig.state();
    CHECK(after_stale.dialog_entries.empty());
    CHECK(after_stale.workspace == before_stale.workspace);
    CHECK(rig.live_value() == 99);
}

TEST_CASE("loaded Flow pane keeps authored layout through native text drag pan save and reload") {
    TempFiles files;
    Rig rig;
    rig.graph_semantically();
    auto native_room = ws::PaneCanvasRoom{fp::kPane, ++rig.grant,
        170 * 36, 65 * 88, 4, true};
    native_room.text_advance_px = 9;
    native_room.text_line_px = 18;
    rig.host(native_room);
    CHECK(rig.picture().labels.empty());
    rig.edit_ok("save", {files.file()});
    REQUIRE_FALSE(rig.state().dirty);
    const auto start = rig.workspace().graph.places.front();
    auto drag = rig.press_for("%0 math.max", true);
    rig.host(drag);
    auto motion = drag;
    motion.phase = ws::canvas_pointer::kMove;
    motion.x += 5 * 36; motion.y += 3 * 88;
    rig.host(motion);
    CHECK(rig.workspace().graph.places.front().x == start.x + 5 * unit);
    CHECK(rig.workspace().graph.places.front().y == start.y + 3 * unit);
    CHECK(rig.state().dirty);
    auto release = motion; release.phase = ws::canvas_pointer::kRelease;
    rig.host(release);

    auto pan = rig.press_for("%0 math.max", true);
    pan.button = 2;
    rig.host(pan);
    motion = pan; motion.phase = ws::canvas_pointer::kMove;
    motion.x += 3 * 36; motion.y += 2 * 88;
    rig.host(motion);
    CHECK(rig.workspace().pan_x == 3 * unit);
    CHECK(rig.workspace().pan_y == 2 * unit);
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
    native_room.grant = ++rig.grant;
    rig.host(native_room);
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
    CHECK(refused.role == zengine::surface::role::kAlert);
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
    (void)rig.label("%0 fold (choose its body)");
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
    (void)rig.label("%0 fold math.add (count rhs, acc lhs)");
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
    rig.host(ws::PaneCanvasRoom{fp::kPane, ++rig.grant, 90 * unit, 65 * unit, unit, true});
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
    rig.host(ws::PaneCanvasRoom{fp::kPane, ++rig.grant, 170 * unit, 65 * unit, unit, true});

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
