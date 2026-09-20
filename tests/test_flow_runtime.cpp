// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow-host/runtime.hpp"
#include "flow/example.hpp"
#include "operator/primitives.hpp"
#include "timer/timer_weave.hpp"
#include "lifecycle_door.hpp"

namespace {
namespace fh = zengine::flow_host;
namespace flow = zengine::flow;
namespace op = zengine::op;

class Client final : public loom::Weave {
public:
    std::vector<loom::Message> messages;
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<fh::FlowAnswer>(), loom::schema_of<fh::FlowCatalogAnswer>(),
            loom::schema_of<fh::FlowChanged>()};
    }
    void handle(const loom::Message& in, loom::Bus&) override { messages.push_back(in); }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("flowtest.Client", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
};
struct Rig {
    op::Catalog catalog;
    loom::Switchboard bus;
    fh::RuntimeHost runtime{bus, catalog};
    Client* reader = nullptr;
    loom::WeaveId client{};
    std::uint64_t correlation = 0;
    Rig() {
        auto mounted = catalog.mount("fixture.basic", op::primitive_definitions());
        if (!mounted) throw std::runtime_error(mounted.reason);
        runtime.mount();
        auto value = std::make_unique<Client>();
        reader = value.get();
        loom::Grant grant;
        fh::allow_flow_requests(grant);
        client = bus.register_weave(std::move(value), std::move(grant));
        pump();
    }
    void pump() {
        for (unsigned n = 0; n < 30; ++n) {
            runtime.poll();
            if (!bus.pending()) return;
            bus.pump_pending();
        }
        REQUIRE(bus.pending() == 0);
    }
    template<class T> fh::FlowAnswer ask(const T& request) {
        const auto corr = ++correlation;
        bus.send_as_to_role(client, fh::kFlowHostRole,
            loom::Message(loom::to_value(request), client, {}, corr));
        pump();
        for (auto it = reader->messages.rbegin(); it != reader->messages.rend(); ++it)
            if (it->correlation == corr && loom::same_identity(it->payload.schema(), *loom::schema_of<fh::FlowAnswer>())) {
                CHECK(it->provenance.answers_ask());
                return loom::from_value<fh::FlowAnswer>(it->payload);
            }
        throw std::runtime_error("no correlated Flow answer");
    }
    flow::Project project() { return flow::thermostat(catalog); }
    fh::FlowAnswer run() { return ask(fh::FlowRun{"draft", fh::bytes(flow::project_bytes(project()))}); }
    fh::FlowAnswer inspect() { return ask(fh::FlowInspect{"draft", 0}); }
    fh::FlowAnswer send(std::int64_t reading) {
        auto definition = project().definition;
        auto shape = definition.accepts.front();
        loom::Value message(shape);
        message.set("degrees", loom::Cell::integer(reading));
        return ask(fh::FlowSend{"draft", fh::bytes(loom::serialize(message))});
    }
};
bool has_kind(const fh::FlowAnswer& answer, const std::string& kind) {
    return std::any_of(answer.events.begin(), answer.events.end(), [&kind](const auto& e) { return e.kind == kind; });
}
} // namespace

TEST_CASE("Flow host runs on the existing bus and catalog and reports fresh state and outputs") {
    Rig rig;
    const auto run = rig.run();
    REQUIRE(run.ok);
    REQUIRE_FALSE(run.subject.empty());
    CHECK(rig.bus.role_holder("thermostat").value == std::stoull(run.subject));
    const auto queued = rig.send(17);
    REQUIRE(queued.ok);
    CHECK(queued.pending == 1);
    CHECK_FALSE(flow::read_project(fh::view(queued.project)).state.get("heating")->as_bool());
    auto current = rig.inspect();
    REQUIRE(current.ok);
    CHECK(current.pending == 0);
    CHECK(flow::read_project(fh::view(current.project)).state.get("heating")->as_bool());
    CHECK(has_kind(current, "output"));
    CHECK(has_kind(current, "dispatched"));
    bool changed = false;
    for (const auto& message : rig.reader->messages)
        if (loom::same_identity(message.payload.schema(), *loom::schema_of<fh::FlowChanged>())) {
            CHECK(message.provenance.authored_from_role(fh::kFlowHostRole));
            changed = true;
        }
    CHECK(changed);
    const auto cursor = current.last_sequence;
    CHECK(rig.ask(fh::FlowInspect{"draft", cursor}).events.empty());
}

TEST_CASE("Flow host behavior edits retain live state and resolve current host operators") {
    Rig rig;
    REQUIRE(rig.run().ok);
    REQUIRE(rig.send(17).ok);
    auto edit = rig.project().definition;
    ++edit.revision;
    REQUIRE(rig.ask(fh::FlowApply{"draft", fh::bytes(zengine::maker::definition_bytes(edit))}).ok);
    CHECK(flow::read_project(fh::view(rig.inspect().project)).state.get("heating")->as_bool());
    const auto* original = rig.catalog.find(op::kSelectBool);
    REQUIRE(original);
    op::OperatorDef replacement(original->identity(), original->inputs(), original->outputs(),
        [](const loom::Value&) { return loom::Cell::boolean(false); });
    REQUIRE(rig.catalog.mount("fixture.overlay", {std::move(replacement)}, op::MountMode::Overlay));
    REQUIRE(rig.send(17).ok);
    CHECK_FALSE(flow::read_project(fh::view(rig.inspect().project)).state.get("heating")->as_bool());
    REQUIRE(rig.catalog.unmount("fixture.overlay"));
    REQUIRE(rig.send(17).ok);
    CHECK(flow::read_project(fh::view(rig.inspect().project)).state.get("heating")->as_bool());
}

TEST_CASE("Flow host refuses missing powers in the operation's own event trail") {
    Rig rig;
    REQUIRE(rig.run().ok);
    REQUIRE(rig.catalog.unmount("fixture.basic"));
    // Build the input from the running project; no fallback catalog can restore the power.
    const auto live = flow::read_project(fh::view(rig.inspect().project));
    loom::Value value(live.definition.accepts.front());
    value.set("degrees", loom::Cell::integer(17));
    REQUIRE(rig.ask(fh::FlowSend{"draft", fh::bytes(loom::serialize(value))}).ok);
    const auto answer = rig.inspect();
    CHECK_FALSE(flow::read_project(fh::view(answer.project)).state.get("heating")->as_bool());
    REQUIRE(has_kind(answer, "refused"));
    bool named = false;
    for (const auto& event : answer.events)
        if (event.kind == "refused") named = event.detail.find("compare.less_int") != std::string::npos;
    CHECK(named);
}

TEST_CASE("Flow sessions belong to the requesting participant and saved payloads confer no authority") {
    Rig rig;
    REQUIRE(rig.run().ok);
    auto stranger = std::make_unique<Client>();
    auto* view = stranger.get();
    loom::Grant grant;
    fh::allow_flow_requests(grant);
    const auto alien = rig.bus.register_weave(std::move(stranger), std::move(grant));
    rig.bus.send_as_to_role(alien, fh::kFlowHostRole,
        loom::Message(loom::to_value(fh::FlowStop{"draft"}), alien, {}, 999));
    rig.pump();
    REQUIRE(view->messages.size() == 1);
    CHECK_FALSE(loom::from_value<fh::FlowAnswer>(view->messages.back().payload).ok);
    CHECK(rig.inspect().ok);
    const auto bad = rig.ask(fh::FlowSend{"draft", fh::bytes(loom::serialize(loom::to_value(fh::FlowStop{"draft"})))});
    CHECK_FALSE(bad.ok);
    CHECK(rig.inspect().ok);
    const auto duplicate = rig.run();
    CHECK_FALSE(duplicate.ok);
}

TEST_CASE("Flow stop returns a restorable final project and unregisters its contributions") {
    Rig rig;
    const auto started = rig.run();
    REQUIRE(started.ok);
    REQUIRE(rig.send(17).ok);
    const auto stopped = rig.ask(fh::FlowStop{"draft"});
    REQUIRE(stopped.ok);
    CHECK_FALSE(rig.bus.role_holder("thermostat").valid());
    CHECK(rig.catalog.find("thermostat.r1.on.thermostat.Reading") == nullptr);
    CHECK(rig.bus.weave(loom::WeaveId{std::stoull(started.subject)}) == nullptr);
    const auto reopened = rig.ask(fh::FlowRun{"draft", stopped.project});
    REQUIRE(reopened.ok);
    CHECK(reopened.subject != started.subject);
    CHECK(flow::read_project(fh::view(reopened.project)).state.get("heating")->as_bool());
}

TEST_CASE("Flow catalog replies contain live complete operator descriptors") {
    Rig rig;
    rig.bus.send_as_to_role(rig.client, fh::kFlowHostRole,
        loom::Message(loom::to_value(fh::FlowCatalog{}), rig.client, {}, 73));
    rig.pump();
    bool seen = false;
    for (const auto& message : rig.reader->messages) {
        if (!loom::same_identity(message.payload.schema(), *loom::schema_of<fh::FlowCatalogAnswer>())) continue;
        REQUIRE(message.provenance.answers_ask());
        const auto answer = loom::from_value<fh::FlowCatalogAnswer>(message.payload);
        REQUIRE(answer.ok);
        for (const auto& descriptor : answer.operators) {
            if (descriptor.identity != "math.max") continue;
            const auto decoded = loom::admit(loom::parse(fh::view(descriptor.descriptor)), op::operator_desc_schema());
            REQUIRE(decoded);
            loom::Registry vocabulary;
            loom::decode_referenced(decoded.value(), vocabulary);
            const auto inputs = loom::decode_schema(*decoded.value().get("inputs")->as_message(), vocabulary);
            CHECK(loom::same_identity(*inputs, *rig.catalog.find("math.max")->inputs()));
            seen = true;
        }
    }
    CHECK(seen);
}

TEST_CASE("Flow refusal and observation bounds remain explicit") {
    Rig rig;
    REQUIRE(rig.run().ok);
    auto edit = rig.project().definition;
    ++edit.revision;
    edit.state = loom::SchemaBuilder("thermostat.DifferentState", 1).field("other", loom::Kind::Int).build();
    const auto invalid = rig.ask(fh::FlowApply{"draft", fh::bytes(zengine::maker::definition_bytes(edit))});
    CHECK_FALSE(invalid.ok);
    CHECK_FALSE(invalid.reason.empty());
    CHECK(rig.inspect().ok);
    for (int i = 0; i < 50; ++i) REQUIRE(rig.send(17).ok);
    const auto retained = rig.inspect();
    CHECK(retained.events.size() == fh::kMaxEvents);
    CHECK(retained.dropped > 0);
    CHECK(retained.first_sequence > 1);
    CHECK(retained.last_sequence >= retained.first_sequence);
}

TEST_CASE("Flow office custody survives holder replacement and does not follow personal speech") {
    Rig rig;
    loom::Grant grant;
    fh::allow_flow_requests(grant);
    auto first = std::make_unique<Client>();
    auto* first_reader = first.get();
    const auto incumbent = rig.bus.register_weave(std::move(first), grant, "flowtest.pane");
    rig.bus.office_send_to_role_as(incumbent, "flowtest.pane", fh::kFlowHostRole,
        loom::Message(loom::to_value(fh::FlowRun{"pane", fh::bytes(flow::project_bytes(rig.project()))}), incumbent, {}, 500));
    rig.pump();
    REQUIRE_FALSE(first_reader->messages.empty());
    bool started = false;
    for (const auto& message : first_reader->messages)
        if (message.correlation == 500) started = loom::from_value<fh::FlowAnswer>(message.payload).ok;
    REQUIRE(started);
    rig.bus.unregister_weave(incumbent);
    auto second = std::make_unique<Client>();
    auto* successor_reader = second.get();
    const auto successor = rig.bus.register_weave(std::move(second), grant, "flowtest.pane");
    rig.bus.office_send_to_role_as(successor, "flowtest.pane", fh::kFlowHostRole,
        loom::Message(loom::to_value(fh::FlowInspect{"pane", 0}), successor, {}, 501));
    rig.pump();
    REQUIRE_FALSE(successor_reader->messages.empty());
    CHECK(loom::from_value<fh::FlowAnswer>(successor_reader->messages.back().payload).ok);
    rig.bus.send_as_to_role(successor, fh::kFlowHostRole,
        loom::Message(loom::to_value(fh::FlowInspect{"pane", 0}), successor, {}, 502));
    rig.pump();
    CHECK_FALSE(loom::from_value<fh::FlowAnswer>(successor_reader->messages.back().payload).ok);
}

TEST_CASE("Flow refuses stopping while its own input is still dispatching") {
    Rig rig;
    REQUIRE(rig.run().ok);
    const auto definition = rig.project().definition;
    loom::Value reading(definition.accepts.front());
    reading.set("degrees", loom::Cell::integer(17));
    rig.bus.send_as_to_role(rig.client, fh::kFlowHostRole,
        loom::Message(loom::to_value(fh::FlowSend{"draft", fh::bytes(loom::serialize(reading))}), rig.client, {}, 800));
    rig.bus.send_as_to_role(rig.client, fh::kFlowHostRole,
        loom::Message(loom::to_value(fh::FlowStop{"draft"}), rig.client, {}, 801));
    rig.pump();
    bool refused = false;
    for (const auto& message : rig.reader->messages)
        if (message.correlation == 801) {
            const auto answer = loom::from_value<fh::FlowAnswer>(message.payload);
            refused = !answer.ok && answer.reason.find("pending dispatch") != std::string::npos;
        }
    CHECK(refused);
    CHECK(rig.inspect().ok);
    CHECK(rig.ask(fh::FlowStop{"draft"}).ok);
}

namespace {
struct StepClock {
    std::int64_t* now = nullptr;
    std::int64_t now_ms() const { return *now; }
    void nap_ms(std::int64_t milliseconds) const {
        if (milliseconds > 0) *now += milliseconds;
    }
};
} // namespace

TEST_CASE("Flow observes quiet state changes through the existing timer conversation") {
    Rig rig; // ClockStart has already drained while no Timer service exists.
    auto project = rig.project();
    project.definition.emits.clear();
    for (auto& trigger : project.definition.on) trigger.emits.clear();
    REQUIRE(rig.ask(fh::FlowRun{"draft", fh::bytes(flow::project_bytes(project))}).ok);
    rig.reader->messages.clear();
    loom::Value reading(project.definition.accepts.front());
    reading.set("degrees", loom::Cell::integer(17));
    rig.bus.send_as_to_role(rig.client, fh::kFlowHostRole,
        loom::Message(loom::to_value(fh::FlowSend{"draft", fh::bytes(loom::serialize(reading))}), rig.client, {}, 900));
    for (unsigned turn = 0; turn < 8 && rig.bus.pending(); ++turn) rig.bus.pump_pending();
    const auto changed = [&] {
        return std::any_of(rig.reader->messages.begin(), rig.reader->messages.end(), [](const auto& message) {
            return loom::same_identity(message.payload.schema(), *loom::schema_of<fh::FlowChanged>());
        });
    };
    CHECK_FALSE(changed());
    // The actual Timer service appears later and announces itself through its
    // attested activation. The virtual clock makes its real scheduling policy
    // deterministic. No host poll, hand-authored TimerReady, or synthetic firing
    // bridges the initial refusal: the standing binding must reconcile itself.
    std::int64_t now = 0;
    using Service = zengine::timer::TimerServiceT<StepClock>;
    auto service = std::make_unique<Service>(StepClock{&now});
    auto* timer = service.get();
    auto grant = loom::emit_default_grant(*timer);
    const auto timer_id = rig.bus.register_weave(std::move(service), std::move(grant), zengine::timer::kTimerRole);
    timer->zen_set_self(timer_id);
    const auto door = zengine::testing::mount_door(rig.bus);
    zengine::testing::order_activation(rig.bus, door, timer_id, 1);
    for (unsigned turn = 0; turn < 128 && !changed(); ++turn) rig.bus.pump_pending();
    REQUIRE(changed());
    CHECK(now >= 16);
    rig.bus.send_as_to_role(rig.client, fh::kFlowHostRole,
        loom::Message(loom::to_value(fh::FlowInspect{"draft", 0}), rig.client, {}, 901));
    std::optional<fh::FlowAnswer> observed;
    for (unsigned turn = 0; turn < 16 && !observed; ++turn) {
        rig.bus.pump_pending();
        for (const auto& message : rig.reader->messages)
            if (message.correlation == 901 && loom::same_identity(message.payload.schema(), *loom::schema_of<fh::FlowAnswer>()))
                observed = loom::from_value<fh::FlowAnswer>(message.payload);
    }
    REQUIRE(observed);
    CHECK(observed->ok);
    CHECK(observed->pending == 0);
    CHECK(flow::read_project(fh::view(observed->project)).state.get("heating")->as_bool());
}

TEST_CASE("Flow input identity is target scoped and foreign outputs cannot impersonate its subject") {
    Rig rig;
    const auto started = rig.run();
    REQUIRE(started.ok);
    const loom::WeaveId subject{std::stoull(started.subject)};
    loom::WeaveId proxy;
    const auto tap = rig.bus.add_observer([&](const loom::BusEvent& event) {
        if (event.target == subject && event.schema_name == "thermostat.Reading") proxy = event.sender;
    });
    REQUIRE(rig.send(17).ok);
    rig.bus.remove_observer(tap);
    REQUIRE(proxy.valid());
    CHECK(proxy != rig.client);
    const auto project = rig.project();
    loom::Value reading(project.definition.accepts.front());
    reading.set("degrees", loom::Cell::integer(99));
    const auto attempt = rig.bus.send_as(proxy, rig.client, loom::Message(reading, proxy));
    rig.pump();
    CHECK(rig.bus.outcome(attempt).disposition == loom::Disposition::Refused);
    CHECK(rig.bus.outcome(attempt).refusal.reason == loom::RefusalReason::CapabilityDenied);
    const auto before = rig.inspect().last_sequence;
    loom::Grant foreign_grant;
    const auto output_schema = project.definition.emits.front();
    foreign_grant.allow_to_any(output_schema->name(), output_schema->version());
    const auto foreign = rig.bus.register_weave(std::make_unique<Client>(), foreign_grant);
    auto fabricated = zengine::maker::default_value(output_schema);
    rig.bus.publish_as(foreign, loom::Message(std::move(fabricated), foreign, {}, 700));
    rig.pump();
    CHECK(rig.inspect().last_sequence == before);
}
