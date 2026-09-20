// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow-pane/vocabulary.hpp"
#include "flow-host/runtime.hpp"
#include "flow/workspace.hpp"
#include "input/vocabulary.hpp"
#include "operator/primitives.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/pane_canvas_vocabulary.hpp"
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
            loom::schema_of<fp::FlowEdited>()};
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
    loom::WeaveId workshop{}, pane{}, door{};
    std::uint64_t correlation = 0;
    std::int64_t activation = 0, grant = 1, gesture = 0;

    Rig() {
        REQUIRE(catalog.mount("flowtest.basic", op::primitive_definitions()));
        runtime.mount();
        auto presentation = std::make_unique<Presenter>();
        presenter = presentation.get();
        loom::Grant host_grant;
        for (const auto& schema : {loom::schema_of<ws::PaneCatalogRequested>(),
                loom::schema_of<ws::PaneRoom>(), loom::schema_of<ws::PaneCanvasRoom>(),
                loom::schema_of<ws::PaneCanvasPointer>(), loom::schema_of<ws::PaneKey>(),
                loom::schema_of<ws::PaneTextInput>(),
                loom::schema_of<fp::FlowEdit>()})
            host_grant.allow_to_role(schema->name(), schema->version(), fp::kRole);
        host_grant.allow_to_any(ws::PaneQuitRequested::zen_name, ws::PaneQuitRequested::zen_version);
        workshop = bus.register_weave(std::move(presentation), std::move(host_grant), workshop_role);
        loom::Grant pane_grant;
        fh::allow_flow_requests(pane_grant);
        for (const auto& schema : {loom::schema_of<ws::PaneOffered>(), loom::schema_of<ws::PaneActions>(),
                loom::schema_of<ws::PaneContent>(), loom::schema_of<ws::PaneCanvasContent>(),
                loom::schema_of<ws::PaneEscapeUnspent>()})
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
    ws::PaneCanvasPointer press_for(const std::string& text, bool prefix = false) {
        const auto row = label(text, prefix);
        ws::PaneCanvasPointer event;
        event.pane = fp::kPane; event.grant = grant; event.picture = picture().picture;
        event.gesture = ++gesture; event.phase = ws::canvas_pointer::kPress;
        event.button = 1; event.x = row.x + 4; event.y = row.y + 4; event.keys_went_here = true;
        return event;
    }
    void click(const std::string& text, bool prefix = false) { host(press_for(text, prefix)); }
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
        edit_ok("add-node", {"math.max"});
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
    rig.click("[math.max]");
    rig.click("input.input : Int");
    rig.click("o lhs = ", true);
    rig.click("o rhs = ", true);
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
    const auto before = rig.state().workspace;
    const auto add = rig.press_for("[math.max]");
    rig.bus.send_as_to_role(impostor, fp::kRole, loom::Message(loom::to_value(add), impostor));
    rig.pump();
    CHECK(rig.state().workspace == before);
    // Activation used requests 1 and 2. This run uses 3. Deliver the forged
    // answer after the edit queues that ask, but before the actual manager sees it.
    rig.bus.office_send_to_role_as(rig.workshop, workshop_role, fp::kRole,
        loom::Message(loom::to_value(fp::FlowEdit{"run", {}}), rig.workshop, {}, 900));
    auto fabricated = rig.workspace().graph.project;
    fabricated.state.set("value", loom::Cell::integer(999));
    fh::FlowAnswer lie;
    lie.session = "workshop"; lie.action = "run"; lie.ok = true;
    lie.project = fh::bytes(flow::project_bytes(fabricated));
    rig.bus.send_as_to_role(impostor, fp::kRole,
        loom::Message(loom::to_value(lie), impostor, {}, 3));
    rig.pump();
    CHECK(rig.live_value() == 0);
    CHECK(rig.workspace().graph.project.state.get("value")->as_int() == 0);
    (void)rig.label("[Running]");
}

TEST_CASE("loaded Flow pane binds gestures to the pictured room definition and interaction context") {
    Rig rig;
    rig.graph_semantically();
    auto stale_room = rig.press_for("[math.max]");
    ++rig.grant;
    rig.room();
    const auto before_room = rig.state().workspace;
    rig.host(stale_room);
    CHECK(rig.state().workspace == before_room);
    auto stale_definition = rig.press_for("[math.max]");
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
    (void)rig.label("> Value: 0", true);
    rig.replace_text("12");
    rig.key(in::scan::kReturn);
    CHECK(rig.workspace().graph.project.definition.on.front().body.nodes.front()
        .arguments.at(1).constant_cell().as_int() == 12);
    CHECK(rig.state().dirty);
}
