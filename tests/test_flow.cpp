// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow/compiled.hpp"
#include "flow/generate.hpp"
#include "flow/graph.hpp"
#include "flow_fixture.hpp"
#include "maker_fixture.hpp"
#include <zen/kernel/kernel.hpp>
#include <filesystem>
#include <fstream>
#include <optional>

namespace {
namespace flow = zengine::flow;
namespace maker = zengine::maker;
namespace op = zengine::op;

class Listener final : public loom::Weave {
public:
    explicit Listener(std::vector<std::shared_ptr<const loom::Schema>> accepts) : shapes(std::move(accepts)) {}
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override { return shapes; }
    void handle(const loom::Message& message, loom::Bus&) override { messages.push_back(message); }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("flowtest.Listener", 1, {})); }
    loom::Value policy() const override { return maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
    std::vector<std::shared_ptr<const loom::Schema>> shapes;
    std::vector<loom::Message> messages;
};

struct Rig {
    op::Catalog catalog;
    std::shared_ptr<flow::CompiledModule> module;
    loom::Switchboard bus;
    loom::Kernel kernel{bus};
    maker::Definition definition;
    loom::WeaveId subject{}, client{};
    Listener* listener = nullptr;

    Rig(const std::string& kind, bool native, std::optional<loom::Grant> grant = {}) {
        flowfix::operators(catalog);
        std::string path;
        if (kind == "thermostat") { definition = flow::thermostat(catalog).definition; path = FLOW_THERMOSTAT; }
        else if (kind == "high_water") { definition = hwfix::high_water(catalog); path = FLOW_HIGH_WATER; }
        else { definition = flowfix::boundaries(catalog); path = FLOW_BOUNDARIES; }
        if (native) {
            module = flow::CompiledModule::open(catalog, path);
            if (maker::definition_bytes(module->definition()) != maker::definition_bytes(definition))
                throw std::runtime_error("fixture is stale");
            auto mounted = module->mount();
            if (!mounted) throw std::runtime_error(mounted.reason);
            const auto offer = module->offer();
            const auto result = kernel.load("flow", path, definition.name,
                                            grant ? *grant : maker::default_grant(definition));
            if (!result.ok) throw std::runtime_error(result.error);
            subject = result.id;
        } else {
            const auto result = maker::register_definition(bus, catalog, definition, grant);
            if (!result) throw std::runtime_error(result.reason);
            subject = result.id;
        }
        auto accepts = definition.emits;
        accepts.push_back(loom::schema_of<loom::Refused>());
        accepts.push_back(loom::schema_of<loom::PokeStructure>());
        accepts.push_back(loom::schema_of<loom::Result>());
        auto target = std::make_unique<Listener>(accepts);
        listener = target.get();
        loom::Grant speak;
        for (const auto& shape : definition.accepts) speak.allow_to_any(shape->name(), shape->version());
        speak.allow_to_any(loom::PokeDescribe::zen_name, loom::PokeDescribe::zen_version);
        speak.allow_to_any(maker::Quiesce::zen_name, maker::Quiesce::zen_version);
        client = bus.register_weave(std::move(target), std::move(speak));
        if (kind == "thermostat") {
            const auto state = flow::thermostat(catalog).state;
            const auto revived = bus.swap_state(subject, loom::serialize(state));
            if (!revived.revived) throw std::runtime_error(revived.refusal.message());
        }
        pump();
    }
    void pump() {
        for (unsigned i = 0; i < 16 && bus.pending() != 0; ++i) bus.pump_pending();
        REQUIRE(bus.pending() == 0);
    }
    loom::Value state() const { return bus.weave(subject)->snapshot(); }
    void send(loom::Value value) {
        listener->messages.clear();
        bus.send_as(client, subject, loom::Message(std::move(value), client, client, 77));
        pump();
    }
    void send(const std::string& name, std::optional<loom::Cell> value = {}, const std::string& field = "incoming") {
        for (const auto& schema : definition.accepts) {
            if (schema->name() != name) continue;
            loom::Value message(schema);
            if (value) message.set(field, *value);
            send(std::move(message));
            return;
        }
        throw std::runtime_error("test named no input");
    }
    std::string refusal() const {
        for (const auto& message : listener->messages)
            if (message.payload.schema().name() == "zen.Refused") return message.payload.get("reason")->as_text();
        return {};
    }
};

void same_state(const Rig& a, const Rig& b) { CHECK(loom::serialize(a.state()) == loom::serialize(b.state())); }

op::OperatorDef constant_overlay(const op::OperatorDef& original, loom::Cell value) {
    return {original.identity(), original.inputs(), original.outputs(),
            [value = std::move(value)](const loom::Value&) { return value; }};
}
} // namespace

TEST_CASE("Flow thermostat retains hysteresis state in both forms") {
    Rig interpreted("thermostat", false), native("thermostat", true);
    const std::vector<std::pair<std::int64_t, bool>> readings{{17, true}, {20, true}, {22, true}, {23, false}, {20, false}, {18, false}, {16, true}};
    for (const auto& [reading, expected] : readings) {
        for (auto* rig : {&interpreted, &native}) {
            rig->send("thermostat.Reading", loom::Cell::integer(reading), "degrees");
            CHECK(rig->state().get("heating")->as_bool() == expected);
            REQUIRE(rig->listener->messages.size() == 1);
            const auto& message = rig->listener->messages.front();
            CHECK(message.payload.schema().name() == "thermostat.Status");
            CHECK(message.payload.get("heating")->as_bool() == expected);
            CHECK(message.sender == rig->subject);
            CHECK_FALSE(message.reply_to.valid());
            CHECK(message.correlation == 77);
            CHECK_FALSE(message.provenance.answers_ask());
        }
        same_state(interpreted, native);
    }
    for (auto* rig : {&interpreted, &native}) {
        rig->send("thermostat.Lower", loom::Cell::integer(99), "lower");
        CHECK(rig->state().get("on_below")->as_int() == 22);
        rig->send("thermostat.Upper", loom::Cell::integer(7), "upper");
        CHECK(rig->state().get("off_above")->as_int() == 22);
    }
    same_state(interpreted, native);
}

TEST_CASE("Flow high-water control runs compiled bodies rather than the graph walker") {
    Rig interpreted("high_water", false), native("high_water", true);
    const auto root = native.definition.trigger_identity(native.definition.on[0]);
    REQUIRE(native.catalog.find(root) != nullptr);
    CHECK_FALSE(native.catalog.find(root)->is_composite());
    CHECK(interpreted.catalog.find(root)->is_composite());
    for (const auto value : {3, 7, 2}) {
        interpreted.send(hwfix::sample(value)); native.send(hwfix::sample(value));
        same_state(interpreted, native);
    }
    CHECK(native.state().get("high")->as_int() == 7);
}

TEST_CASE("Flow resolves leaf overlays and reveals the original without regeneration") {
    Rig a("high_water", false), b("high_water", true);
    for (auto* rig : {&a, &b}) {
        auto definition = constant_overlay(*rig->catalog.find(op::kMaxInt), loom::Cell::integer(41));
        REQUIRE(rig->catalog.mount("overlay", {definition}, op::MountMode::Overlay));
        rig->send(hwfix::sample(7));
        CHECK(rig->state().get("high")->as_int() == 41);
        rig->catalog.unmount("overlay");
        rig->send(hwfix::sample(53));
        CHECK(rig->state().get("high")->as_int() == 53);
    }
    same_state(a, b);
}

TEST_CASE("Flow resolves outer trigger overlays and unmounts too") {
    Rig a("high_water", false), b("high_water", true);
    for (auto* rig : {&a, &b}) {
        const auto root = rig->definition.trigger_identity(rig->definition.on[0]);
        REQUIRE(rig->catalog.mount("outer", {constant_overlay(*rig->catalog.find(root), loom::Cell::integer(83))}, op::MountMode::Overlay));
        rig->send(hwfix::sample(9)); CHECK(rig->state().get("high")->as_int() == 83);
        rig->catalog.unmount("outer");
        rig->send(hwfix::sample(91)); CHECK(rig->state().get("high")->as_int() == 91);
        rig->catalog.unmount(rig->definition.provider());
        rig->send(hwfix::sample(99));
        CHECK(rig->state().get("high")->as_int() == 91);
        CHECK(rig->refusal() == "unresolved trigger `hw.r1.on.hw.Sample`: its body is not mounted");
    }
}

TEST_CASE("Flow compiled invocations stay with their own host") {
    Rig a("high_water", true), b("high_water", true);
    REQUIRE(a.catalog.mount("different", {constant_overlay(*a.catalog.find(op::kMaxInt), loom::Cell::integer(27))}, op::MountMode::Overlay));
    a.send(hwfix::sample(8)); b.send(hwfix::sample(8));
    CHECK(a.state().get("high")->as_int() == 27);
    CHECK(b.state().get("high")->as_int() == 8);
}

TEST_CASE("Flow compiled state and inputs retain all seven kinds") {
    Rig a("boundaries", false), b("boundaries", true);
    loom::Value nested(flowfix::nested()); nested.set("text", loom::Cell::text("nested data"));
    const std::vector<std::pair<std::string, loom::Cell>> values{
        {"integer", loom::Cell::integer(-97)}, {"real", loom::Cell::real(1.625)},
        {"text", loom::Cell::text("quotes \" and unicode λ")}, {"flag", loom::Cell::boolean(true)},
        {"bytes", loom::Cell::bytes({0, 17, 255})}, {"nested", loom::Cell::message(nested)},
        {"list", loom::Cell::list({loom::Cell::message(nested), loom::Cell::message(nested)})}};
    auto expected = maker::default_value(a.definition.state);
    for (const auto& [name, value] : values) {
        expected.set(name, value);
        a.send("flowtest.Set_" + name, value); b.send("flowtest.Set_" + name, value);
        CHECK(a.refusal().empty()); CHECK(b.refusal().empty());
        CHECK(loom::serialize(b.state()) == loom::serialize(expected));
        same_state(a, b);
    }
    CHECK_FALSE(b.state().has("absent"));
    const flow::Project saved{b.definition, b.state()};
    CHECK(flow::project_bytes(flow::read_project(flow::project_bytes(saved))) == flow::project_bytes(saved));
}

TEST_CASE("Flow missing optional input refuses but unused extra arguments are not read") {
    Rig a("boundaries", false), b("boundaries", true);
    for (auto* rig : {&a, &b}) {
        const auto before = loom::serialize(rig->state());
        rig->send("flowtest.Optional");
        CHECK(loom::serialize(rig->state()) == before);
        CHECK(rig->refusal().find("no input named 'incoming'") != std::string::npos);
        rig->send("flowtest.Extra");
        CHECK(rig->refusal().empty());
        CHECK(rig->state().get("integer")->as_int() == 71);
    }
    same_state(a, b);
}

TEST_CASE("Flow preserves length-bearing operator identities through native calls") {
    Rig a("boundaries", false), b("boundaries", true);
    a.send("flowtest.Nul", loom::Cell::integer(37)); b.send("flowtest.Nul", loom::Cell::integer(37));
    CHECK(a.refusal().empty()); CHECK(b.refusal().empty());
    CHECK(b.state().get("integer")->as_int() == 37); same_state(a, b);
}

TEST_CASE("Flow nodes after the selected result still execute and can refuse the whole write") {
    Rig a("boundaries", false), b("boundaries", true);
    for (auto* rig : {&a, &b}) {
        unsigned calls = 0;
        const auto* original = rig->catalog.find("flowtest.after");
        op::OperatorDef after(original->identity(), original->inputs(), original->outputs(),
            [&](const loom::Value&) -> loom::Cell { ++calls; throw std::runtime_error("late failure"); });
        REQUIRE(rig->catalog.mount("late", {after}, op::MountMode::Overlay));
        const auto before = loom::serialize(rig->state());
        rig->send("flowtest.Late", loom::Cell::integer(19));
        CHECK(calls == 1);
        CHECK(loom::serialize(rig->state()) == before);
        CHECK(rig->refusal() == "'flowtest.after' could not be spent: late failure");
        rig->catalog.unmount("late");
    }
    CHECK(a.refusal() == b.refusal());
}

TEST_CASE("Flow outer output admission refuses a wrong result before state write") {
    Rig a("boundaries", false), b("boundaries", true);
    for (auto* rig : {&a, &b}) {
        const auto before = loom::serialize(rig->state());
        rig->send("flowtest.Wrong", loom::Cell::boolean(true));
        CHECK(loom::serialize(rig->state()) == before);
        CHECK(rig->refusal().find("produced an answer its own output schema refuses") != std::string::npos);
    }
    CHECK(a.refusal() == b.refusal());
}

TEST_CASE("Flow emit failure keeps the write and continues the remaining emits") {
    Rig a("boundaries", false), b("boundaries", true);
    for (auto* rig : {&a, &b}) {
        rig->send("flowtest.Emit", loom::Cell::integer(63));
        CHECK(rig->state().get("integer")->as_int() == 63);
        REQUIRE(rig->listener->messages.size() == 2);
        CHECK(rig->listener->messages[0].payload.schema().name() == "zen.Refused");
        CHECK(rig->listener->messages[1].payload.schema().name() == "flowtest.Written");
        CHECK(rig->listener->messages[1].payload.get("value")->as_int() == 63);
    }
    CHECK(a.refusal() == b.refusal()); same_state(a, b);
}

TEST_CASE("Flow missing and reshaped leaves refuse by the same detecting layer") {
    Rig a("boundaries", false), b("boundaries", true);
    for (auto* rig : {&a, &b}) {
        rig->catalog.unmount("flowtest.operators");
        rig->send("flowtest.Set_integer", loom::Cell::integer(29));
        CHECK(rig->refusal().find("unresolved operator reference 'flowtest.echo_integer'") != std::string::npos);
    }
    CHECK(a.refusal() == b.refusal());
    for (auto* rig : {&a, &b}) {
        REQUIRE(rig->catalog.mount("reshaped", {flowfix::echo("flowtest.echo_integer", loom::type_of(loom::Kind::Bool))}));
        rig->send("flowtest.Set_integer", loom::Cell::integer(29));
        CHECK(rig->refusal().find("not the signature this composition was authored against") != std::string::npos);
        CHECK(rig->state().get("integer")->as_int() == 0);
    }
    CHECK(a.refusal() == b.refusal());
}

TEST_CASE("Flow native publication still needs the subject grant") {
    for (const bool native : {false, true}) {
        Rig rig("high_water", native, loom::Grant{});
        std::vector<loom::BusEvent> denied;
        const auto observer_id = rig.bus.add_observer([&](const loom::BusEvent& event) {
            if (event.kind == loom::EventKind::Refused && event.refusal.reason == loom::RefusalReason::CapabilityDenied)
                denied.push_back(event);
        });
        rig.send(hwfix::sample(13));
        CHECK(rig.state().get("high")->as_int() == 13);
        CHECK(rig.listener->messages.empty());
        REQUIRE(denied.size() == 1);
        CHECK(denied[0].sender == rig.subject);
        CHECK(denied[0].schema_name == "hw.HighWater");
        rig.bus.remove_observer(observer_id);
    }
}

TEST_CASE("Flow common runtime retains inspection and rejects unarmed ceremony") {
    Rig a("high_water", false), b("high_water", true);
    for (auto* rig : {&a, &b}) {
        rig->send(loom::to_value(loom::PokeDescribe{}));
        REQUIRE(rig->listener->messages.size() == 1);
        CHECK(rig->listener->messages[0].sender == rig->subject);
        CHECK_FALSE(rig->listener->messages[0].provenance.answers_ask());
        const auto structure = loom::from_value<loom::PokeStructure>(rig->listener->messages[0].payload);
        CHECK(structure.state_schema == "hw.State");
        CHECK(structure.fields[0].name == "high");
        CHECK_FALSE(structure.fields[0].writable);
        rig->send(loom::to_value(maker::Quiesce{7}));
        CHECK(rig->refusal().find("sender is not the coordinator") != std::string::npos);
        rig->send(hwfix::sample(31)); CHECK(rig->state().get("high")->as_int() == 31);
    }
}

TEST_CASE("Flow compilation and recovery retain definitions and reject handwritten changes") {
    op::Catalog catalog; flowfix::operators(catalog);
    for (const auto& definition : {flowfix::boundaries(catalog), hwfix::high_water(catalog),
                                  flow::thermostat(catalog).definition}) {
        const auto source = flow::generate_cpp(definition);
        CHECK(maker::definition_bytes(flow::recover_cpp(source)) == maker::definition_bytes(definition));
        CHECK_THROWS_WITH_AS(flow::recover_cpp(source + "// edited\n"),
            "generated C++ was edited or uses another generator version; the retained definition cannot describe those changes",
            std::invalid_argument);
        CHECK(source.find("step_0.run()") != std::string::npos);
    }
}

TEST_CASE("Flow empty admitted definitions remain generation-capable but registration refuses") {
    op::Catalog catalog; flowfix::operators(catalog);
    auto module = flow::CompiledModule::open(catalog, FLOW_EMPTY);
    CHECK(module->definition().on.empty());
    const auto native = module->mount();
    loom::Switchboard bus;
    const auto interpreted = maker::register_definition(bus, catalog, module->definition());
    CHECK_FALSE(native); CHECK_FALSE(interpreted);
    CHECK(native.reason == interpreted.reason);
}

TEST_CASE("Flow live draft changes keep state and persist one coherent project") {
    Rig rig("thermostat", false);
    rig.send("thermostat.Reading", loom::Cell::integer(17), "degrees");
    flow::Draft draft(rig.catalog, {rig.definition, rig.state()});
    for (const auto* line : {"on Reading heating", "node logic.select_bool true false $heating",
            "result 0", "emit Status heating=$heating on_below=$on_below off_above=$off_above", "end"})
        draft.command(flow::words(line));
    const auto edit = draft.finish();
    const auto before = loom::serialize(rig.state());
    REQUIRE(maker::apply_behaviour_edit(rig.bus, rig.catalog, rig.subject, edit.definition));
    CHECK(loom::serialize(rig.state()) == before);
    rig.send("thermostat.Reading", loom::Cell::integer(10), "degrees");
    CHECK_FALSE(rig.state().get("heating")->as_bool());
    const flow::Project project{edit.definition, rig.state()};
    const auto restored = flow::read_project(flow::project_bytes(project));
    CHECK(maker::definition_bytes(restored.definition) == maker::definition_bytes(edit.definition));
    CHECK(loom::serialize(restored.state) == loom::serialize(rig.state()));
    CHECK(flow::graph_svg(restored.definition).find("logic.select_bool") != std::string::npos);
}

TEST_CASE("Flow scalar authoring refuses malformed drafts before changing a running weave") {
    op::Catalog catalog; flowfix::operators(catalog);
    flow::Draft draft(catalog, "counter");
    draft.command(flow::words("state high Int 0"));
    draft.command(flow::words("accept Sample input:Int"));
    draft.command(flow::words("on Sample high"));
    CHECK_THROWS_AS(draft.command(flow::words("node unknown $input")), std::invalid_argument);
    CHECK_THROWS_AS(draft.finish(), std::invalid_argument);
    draft.command(flow::words("node math.max $input $high"));
    draft.command(flow::words("result 0")); draft.command(flow::words("end"));
    CHECK(draft.finish().definition.name == "counter");
    CHECK_THROWS_AS(flow::words("state x Text \"unterminated"), std::invalid_argument);
    CHECK_THROWS_AS(flow::index_of("-1"), std::invalid_argument);
}

TEST_CASE("Flow emitted contracts participate in registration before any listener") {
    for (const bool native : {false, true}) {
        op::Catalog catalog; flowfix::operators(catalog);
        auto module = flow::CompiledModule::open(catalog, FLOW_HIGH_WATER);
        loom::Switchboard bus;
        loom::Kernel kernel(bus);
        const auto conflicting = loom::SchemaBuilder("hw.HighWater", 1)
            .field("high", loom::Kind::Bool).build();
        auto owner = std::make_unique<Listener>(std::vector<std::shared_ptr<const loom::Schema>>{conflicting});
        bus.register_weave(std::move(owner), loom::Grant{});
        if (native) {
            REQUIRE(module->mount());
            const auto offer = module->offer();
            const auto loaded = kernel.load("conflict", FLOW_HIGH_WATER, "hw", maker::default_grant(module->definition()));
            CHECK_FALSE(loaded.ok);
            CHECK_FALSE(bus.role_holder("hw").valid());
        } else {
            const auto result = maker::register_definition(bus, catalog, hwfix::high_water(catalog));
            CHECK_FALSE(result);
            CHECK_FALSE(bus.role_holder("hw").valid());
        }
    }
}

TEST_CASE("Flow behavior edits cannot silently change the registered message surface") {
    Rig rig("high_water", false);
    rig.send(hwfix::sample(43));
    const auto before = loom::serialize(rig.state());
    const auto original = rig.definition;
    for (const bool accepted : {false, true}) {
        auto edited = original; ++edited.revision;
        const auto extra = loom::make_schema("hw.Extra", 1, {});
        (accepted ? edited.accepts : edited.emits).push_back(extra);
        const auto result = maker::apply_behaviour_edit(rig.bus, rig.catalog, rig.subject, edited);
        CHECK_FALSE(result);
        CHECK(loom::serialize(rig.state()) == before);
        CHECK(rig.catalog.find(original.trigger_identity(original.on[0])) != nullptr);
        CHECK(rig.catalog.find(edited.trigger_identity(edited.on[0])) == nullptr);
    }
}

TEST_CASE("Flow repeated schema declarations cannot hide a changed live contract") {
    for (const bool accepted : {false, true}) {
        op::Catalog catalog; flowfix::operators(catalog);
        auto definition = hwfix::high_water(catalog);
        auto& contracts = accepted ? definition.accepts : definition.emits;
        REQUIRE(contracts.size() == 1);
        contracts.push_back(contracts.front());
        REQUIRE(maker::read_definition(maker::definition_bytes(definition)));
        loom::Switchboard bus;
        const auto registered = maker::register_definition(bus, catalog, definition);
        REQUIRE_MESSAGE(registered, registered.reason);
        auto changed = definition; ++changed.revision;
        (accepted ? changed.accepts : changed.emits).back() = loom::make_schema("hw.New", 1, {});
        const auto edited = maker::apply_behaviour_edit(bus, catalog, registered.id, changed);
        CHECK_FALSE(edited);
        CHECK(catalog.find(changed.trigger_identity(changed.on.front())) == nullptr);
        CHECK(catalog.find(definition.trigger_identity(definition.on.front())) != nullptr);
    }
}
