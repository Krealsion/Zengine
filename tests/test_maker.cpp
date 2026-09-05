// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE MAKER SUITE: a Loom weave built from a maker's DEFINITION, exercised on the High-water
// forcing case -- definition, state, one trigger, one emit, and a schema edit by succession.
//
// Every case here authors High-water as data (maker_fixture.hpp): `loom::SchemaBuilder` for the
// shapes, `op::Builder` for the trigger's body, the composition wire form to carry it, native
// bytes to persist it. No ZEN_SHAPE and no weave class of High-water's own exists anywhere in
// this repository -- that absence is the claim, and the fresh-process case is its witness.
//
// The case names are the plan sheet's pins, one per clause, and the registers under
// agents/maker/ cite them by these exact literals.

#include "doctest.h"

#include "maker/definition.hpp"
#include "maker/files.hpp"
#include "maker/succession.hpp"
#include "maker/vocabulary.hpp"
#include "maker/weave.hpp"
#include "maker/write.hpp"
#include "maker_fixture.hpp"
#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"

#include <zen/host/prepared_replacement.hpp>
#include <zen/kind.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard/message.hpp>
#include <zen/switchboard/switchboard.hpp>
#include <zen/value.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/poke.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace op = zengine::op;
namespace maker = zengine::maker;
using loom::Message;
using loom::WeaveId;

namespace maker_test {

/// A `math.max` that is a min -- the saboteur the operator suite uses, for the overlay case and
/// for a behaviour edit whose new body is visibly another rule.
inline std::int64_t min_int(std::int64_t lhs, std::int64_t rhs) { return lhs < rhs ? lhs : rhs; }

/// An operator answering a Bool, so a trigger can be authored whose answer is not the kind of
/// the field it writes.
inline bool is_big(std::int64_t value) { return value > 100; }

inline std::shared_ptr<const loom::Schema> probe_schema() {
    static const auto s =
        loom::SchemaBuilder("maker.test.Probe", 1).field("seen", loom::Kind::Int).build();
    return s;
}

/// A raw listener recording every delivery.
class Probe final : public loom::Weave {
public:
    explicit Probe(std::vector<std::shared_ptr<const loom::Schema>> accepts)
        : accepts_(std::move(accepts)) {}
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return accepts_;
    }
    void handle(const Message& in, loom::Bus&) override { got.push_back(in); }
    loom::Value snapshot() const override {
        loom::Value v(probe_schema());
        v.set("seen", loom::Cell::integer(static_cast<std::int64_t>(got.size())));
        return v;
    }
    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(4));
        v.set("revive_from_last_good", loom::Cell::boolean(true));
        return v;
    }
    void revive(const loom::Value&) override {}

    /// The last delivery of one shape, or nullptr.
    const Message* last(std::string_view schema_name) const {
        for (auto it = got.rbegin(); it != got.rend(); ++it) {
            if (it->payload.schema().name() == schema_name) {
                return &*it;
            }
        }
        return nullptr;
    }
    std::size_t count(std::string_view schema_name) const {
        std::size_t n = 0;
        for (const Message& m : got) {
            if (m.payload.schema().name() == schema_name) {
                ++n;
            }
        }
        return n;
    }

    std::vector<Message> got;

private:
    std::vector<std::shared_ptr<const loom::Schema>> accepts_;
};

/// A host: ONE catalog, declared before the bus so the weaves unmount before it goes; a client
/// probe that speaks with `allow_any` and hears the shapes a High-water host hears.
struct Host {
    op::Catalog catalog;
    loom::Switchboard bus;
    Probe* client = nullptr;
    WeaveId client_id{};

    explicit Host(bool primitives = true) {
        if (primitives) {
            op::publish_primitives(catalog);
        }
    }

    WeaveId add_probe(Probe*& out, std::vector<std::shared_ptr<const loom::Schema>> accepts) {
        auto probe = std::make_unique<Probe>(std::move(accepts));
        out = probe.get();
        return bus.register_weave(std::move(probe), loom::Grant{}.allow_any());
    }

    void listen() {
        client_id = add_probe(client, {hwfix::high_water_schema(), loom::schema_of<loom::Refused>(),
                                       loom::schema_of<loom::Result>(),
                                       loom::schema_of<loom::PokeStructure>(),
                                       loom::schema_of<loom::Ack>()});
    }

    loom::Ticket send(WeaveId target, loom::Value payload, std::uint64_t correlation = 0) {
        return bus.send_as(client_id, target,
                           Message(std::move(payload), client_id, client_id, correlation));
    }
    loom::Ticket send_to_role(std::string_view role, loom::Value payload,
                              std::uint64_t correlation = 0) {
        return bus.send_as_to_role(client_id, role,
                                   Message(std::move(payload), client_id, client_id, correlation));
    }
    void pump() { bus.drain_until_idle(); }
};

inline std::int64_t high_of(const maker::Weave& w) { return w.state().get("high")->as_int(); }

inline std::string reason_of(const Message& m) {
    return loom::from_value<loom::Refused>(m.payload).reason;
}

inline bool contains(std::string_view text, std::string_view piece) {
    return text.find(piece) != std::string_view::npos;
}

/// A definition through its own file bytes and back -- the door a Workshop editor uses.
inline maker::Admitted round_trip(const maker::Definition& d) {
    return maker::read_definition(maker::definition_bytes(d));
}

} // namespace maker_test

using namespace maker_test;

// ---- FC-2: the state schema ----------------------------------------------------------------

TEST_CASE("FC-2: the state schema is built from data, the registry resolves it by name, and its "
          "content id is the descriptor's") {
    Host h;
    const maker::Admitted read = round_trip(hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(read.ok, read.reason);
    // The decoded state is the descriptor's shape, not a copy that happens to agree.
    CHECK(read.definition.state->content_id() == hwfix::state_v1()->content_id());
    CHECK(read.definition.state->name() == "hw.State");

    const maker::Registered r = maker::register_definition(h.bus, h.catalog, read.definition);
    REQUIRE_MESSAGE(r.ok, r.reason);
    // The first snapshot claimed it: the bus resolves the data-built schema by its own name.
    const std::shared_ptr<const loom::Schema> resolved = h.bus.resolve_schema("hw.State", 1);
    REQUIRE(resolved != nullptr);
    CHECK(resolved->content_id() == hwfix::state_v1()->content_id());
    CHECK(loom::same_identity(*resolved, *r.weave->definition().state));
    CHECK(h.bus.role_holder("hw") == r.id);
}

TEST_CASE("FC-2: a state schema outside the definition's namespace is refused, naming the prefix "
          "it needed") {
    Host h;
    maker::Definition d = hwfix::high_water(h.catalog);
    d.state = loom::SchemaBuilder("other.State", 1).field("high", loom::Kind::Int).build();
    const maker::Admitted read = round_trip(d);
    CHECK_FALSE(read.ok);
    CHECK(contains(read.reason, "other.State"));
    CHECK(contains(read.reason, "`hw.`"));
}

// ---- FC-3 / FC-4: the accept-set, the pack, the body, the write-back -----------------------

TEST_CASE("FC-3: the accept-set is the definition's; hw.Sample is delivered and an unlisted shape "
          "is refused NotAccepted") {
    Host h;
    h.listen();
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(r.ok, r.reason);

    std::vector<std::string> doors;
    for (const auto& s : h.bus.accepted_schemas(r.id)) {
        doors.push_back(s->name());
    }
    auto has = [&doors](const char* name) {
        for (const std::string& d : doors) {
            if (d == name) {
                return true;
            }
        }
        return false;
    };
    CHECK(has("hw.Sample"));
    CHECK(has("zen.PokeDescribe"));
    CHECK(has("zen.PokeRead"));
    CHECK(has("zen.Activated"));
    CHECK(has("zengine.maker.Quiesce"));
    CHECK(has("zengine.maker.Resume"));
    CHECK(has("zengine.maker.Adopt"));
    CHECK_FALSE(has("hw.HighWater")); // emitted, not accepted

    h.send(r.id, hwfix::sample(3));
    h.pump();
    CHECK(high_of(*r.weave) == 3);

    const auto other = loom::SchemaBuilder("hw.Other", 1).field("x", loom::Kind::Int).build();
    loom::Value stranger(other);
    stranger.set("x", loom::Cell::integer(1));
    const loom::Ticket t = h.send(r.id, std::move(stranger));
    h.pump();
    const loom::DeliveryOutcome outcome = h.bus.outcome(t);
    CHECK(outcome.disposition == loom::Disposition::Refused);
    CHECK(outcome.refusal.reason == loom::RefusalReason::NotAccepted);
    CHECK(high_of(*r.weave) == 3);
}

TEST_CASE("FC-4: the pack is state then message, and a field name both carry is refused at "
          "admission") {
    Host h;
    const maker::Admitted read = round_trip(hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(read.ok, read.reason);
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, read.definition);
    REQUIRE_MESSAGE(r.ok, r.reason);
    const op::OperatorDef* body = h.catalog.find("hw.r1.on.hw.Sample");
    REQUIRE(body != nullptr);
    REQUIRE(body->inputs()->fields().size() == 2);
    CHECK(body->inputs()->fields()[0].name == "high");  // the state's, first
    CHECK(body->inputs()->fields()[1].name == "value"); // then the message's
    CHECK(body->is_composite());
    // ...and the pack is FILLED from both: a body reading the state's `high` is spent, and a
    // pack carrying the message alone would refuse it as `no input named 'high'`.
    h.listen();
    h.send(r.id, hwfix::sample(3));
    h.send(r.id, hwfix::sample(1));
    h.pump();
    CHECK(high_of(*r.weave) == 3);
    CHECK(r.weave->handled() == 2);

    // A message that also declares `high`: the pack cannot carry two.
    maker::Definition clash = hwfix::high_water(h.catalog);
    const auto colliding =
        loom::SchemaBuilder("hw.Sample", 1).field("high", loom::Kind::Int).build();
    clash.accepts = {colliding};
    clash.on[0].message = colliding;
    // The body must still be a graph the codec carries; its bindings name `high` and `value`,
    // and the refusal comes first anyway, from the schemas.
    const maker::Admitted refused = round_trip(clash);
    CHECK_FALSE(refused.ok);
    CHECK(contains(refused.reason, "`high`"));
    CHECK(contains(refused.reason, "both declare"));
}

TEST_CASE("FC-4: the body is a composition spent through the host's catalog -- a power overlaid "
          "underneath moves the trigger, and revealing it moves it back") {
    Host h;
    h.listen();
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(r.ok, r.reason);
    h.send(r.id, hwfix::sample(3));
    h.send(r.id, hwfix::sample(7));
    h.pump();
    CHECK(high_of(*r.weave) == 7);

    // A `math.max` that is a min, overlaid UNDERNEATH the trigger, in the host's one catalog.
    std::vector<op::OperatorDef> saboteur;
    saboteur.push_back(op::make_operator<&min_int>(op::kMaxInt, {"lhs", "rhs"}, "result"));
    const op::MountReport covered =
        h.catalog.mount("test.saboteur", std::move(saboteur), op::MountMode::Overlay);
    REQUIRE_MESSAGE(covered.ok, covered.reason);
    h.send(r.id, hwfix::sample(5));
    h.pump();
    CHECK(high_of(*r.weave) == 5); // min(7, 5): the trigger moved, because it resolves at spend

    REQUIRE(h.catalog.unmount("test.saboteur"));
    h.send(r.id, hwfix::sample(9));
    h.pump();
    CHECK(high_of(*r.weave) == 9); // max(5, 9): revealed, and the trigger moved back
}

TEST_CASE("FC-4: the answer lands in the named state field, and an answer of another kind is "
          "refused with the state unchanged") {
    Host h;
    h.listen();
    // `label` FIRST, so writing slot 0 instead of the named field would be caught (VM-FIX-05).
    const auto labelled = loom::SchemaBuilder("hw.State", 1)
                              .field("label", loom::Kind::Text)
                              .field("high", loom::Kind::Int)
                              .build();
    maker::Definition d = hwfix::high_water(h.catalog);
    d.state = labelled;
    d.on[0].body = hwfix::two_arg_body(h.catalog, d.trigger_identity(d.on[0]), *d.state,
                                       *d.on[0].message, op::kMaxInt);
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, d);
    REQUIRE_MESSAGE(r.ok, r.reason);
    h.send(r.id, hwfix::sample(5));
    h.pump();
    CHECK(high_of(*r.weave) == 5);
    CHECK(r.weave->state().get("label")->as_text().empty());

    // A body whose answer is a Bool, written to an Int field: the output gate refuses it, in
    // the catalog's words, and the state is untouched.
    Host h2;
    h2.listen();
    h2.catalog.publish(op::make_operator<&is_big>("test.is_big", {"value"}, "result"));
    maker::Definition wrong = hwfix::high_water(h2.catalog);
    {
        op::Builder b(h2.catalog, wrong.trigger_identity(wrong.on[0]),
                      hwfix::pack_ports(*wrong.state, *wrong.on[0].message));
        const op::Builder::Ref answer = b.call("test.is_big", {b.input("value")});
        const op::OperatorDef def = std::move(b).result("value", answer);
        wrong.on[0].body = *def.composition();
    }
    const maker::Registered w = maker::register_definition(h2.bus, h2.catalog, wrong);
    REQUIRE_MESSAGE(w.ok, w.reason);
    h2.send(w.id, hwfix::sample(200), 7);
    h2.pump();
    CHECK(high_of(*w.weave) == 0);
    CHECK(w.weave->refused() == 1);
    CHECK(w.weave->handled() == 0);
    const Message* refused = h2.client->last("zen.Refused");
    REQUIRE(refused != nullptr);
    CHECK(refused->correlation == 7);
    CHECK(contains(reason_of(*refused), "output schema refuses"));
}

TEST_CASE("FC-4: a body that cannot be spent leaves the state unchanged and refuses by name") {
    Host h(/*primitives=*/false);
    h.listen();
    // The primitives arrive from a provider this case can take away again.
    std::vector<op::OperatorDef> basic;
    basic.push_back(op::make_operator<&op::max_int>(op::kMaxInt, {"lhs", "rhs"}, "result"));
    REQUIRE(h.catalog.mount("test.basic", std::move(basic)).ok);
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(r.ok, r.reason);
    h.send(r.id, hwfix::sample(3));
    h.pump();
    CHECK(high_of(*r.weave) == 3);

    REQUIRE(h.catalog.unmount("test.basic"));
    h.send(r.id, hwfix::sample(9), 11);
    h.pump();
    CHECK(high_of(*r.weave) == 3);
    CHECK(r.weave->refused() == 1);
    const Message* refused = h.client->last("zen.Refused");
    REQUIRE(refused != nullptr);
    CHECK(refused->correlation == 11);
    CHECK(contains(reason_of(*refused), "unresolved operator reference 'math.max'"));

    // Resolved at spend: the power returns and the same trigger spends it.
    std::vector<op::OperatorDef> again;
    again.push_back(op::make_operator<&op::max_int>(op::kMaxInt, {"lhs", "rhs"}, "result"));
    REQUIRE(h.catalog.mount("test.basic", std::move(again)).ok);
    h.send(r.id, hwfix::sample(9));
    h.pump();
    CHECK(high_of(*r.weave) == 9);
}

// ---- FC-5: the emit -----------------------------------------------------------------------

TEST_CASE("FC-5: after the trigger the weave publishes hw.HighWater with the written value under "
          "its own grant; ungranted, the publication is CapabilityDenied on the tap") {
    Host h;
    h.listen();
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(r.ok, r.reason);
    h.send(r.id, hwfix::sample(3), 21);
    h.send(r.id, hwfix::sample(7), 22);
    h.pump();
    REQUIRE(h.client->count("hw.HighWater") == 2);
    const Message* published = h.client->last("hw.HighWater");
    CHECK(published->payload.get("high")->as_int() == 7); // the WRITTEN value, after write-back
    CHECK(published->sender == r.id);
    CHECK(published->correlation == 22);

    // The same definition registered with an EMPTY grant: the trigger still writes, and the
    // publication is refused at delivery -- CapabilityDenied, visible on the tap.
    Host bare;
    bare.listen();
    std::vector<loom::BusEvent> denied;
    bare.bus.add_observer([&denied](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Refused &&
            ev.refusal.reason == loom::RefusalReason::CapabilityDenied) {
            denied.push_back(ev);
        }
    });
    const maker::Registered u = maker::register_definition(bare.bus, bare.catalog,
                                                           hwfix::high_water(bare.catalog),
                                                           loom::Grant{});
    REQUIRE_MESSAGE(u.ok, u.reason);
    bare.send(u.id, hwfix::sample(4));
    bare.pump();
    CHECK(high_of(*u.weave) == 4);
    CHECK(bare.client->count("hw.HighWater") == 0);
    REQUIRE(denied.size() == 1);
    CHECK(denied[0].schema_name == "hw.HighWater");
    CHECK(denied[0].sender == u.id);
    CHECK(denied[0].target == bare.client_id);
}

// ---- e: the behaviour edit -------------------------------------------------------------------

TEST_CASE("e: a behaviour edit with the schema unchanged is a swap_state -- same WeaveId, Revived "
          "announced, state kept, the new body spent") {
    Host h;
    h.listen();
    h.catalog.publish(op::make_operator<&min_int>("test.min", {"lhs", "rhs"}, "result"));
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(r.ok, r.reason);
    h.send(r.id, hwfix::sample(3));
    h.send(r.id, hwfix::sample(7));
    h.pump();
    REQUIRE(high_of(*r.weave) == 7);

    std::vector<loom::BusEvent> revived;
    h.bus.add_observer([&revived](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Revived) {
            revived.push_back(ev);
        }
    });
    const maker::Admitted next = round_trip(hwfix::high_water(h.catalog, 2, "test.min"));
    REQUIRE_MESSAGE(next.ok, next.reason);
    const maker::Edited edited = maker::apply_behaviour_edit(h.bus, h.catalog, r.id, next.definition);
    REQUIRE_MESSAGE(edited.ok, edited.reason);

    REQUIRE(revived.size() == 1);
    CHECK(revived[0].target == r.id);
    CHECK(revived[0].schema_name == "hw.State");
    CHECK(h.bus.weave(r.id) == r.weave);              // the same object at the same id
    CHECK(high_of(*r.weave) == 7);                    // state kept
    CHECK(r.weave->definition().revision == 2);
    CHECK(h.catalog.mounted("zengine.maker.hw.r2"));
    CHECK_FALSE(h.catalog.mounted("zengine.maker.hw.r1"));

    h.send(r.id, hwfix::sample(5));
    h.pump();
    CHECK(high_of(*r.weave) == 5); // min(7, 5): the new body is the one spent
}

TEST_CASE("e: a definition whose state schema differs is refused as a behaviour edit, and the live "
          "weave is untouched") {
    Host h;
    h.listen();
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(r.ok, r.reason);
    h.send(r.id, hwfix::sample(7));
    h.pump();
    std::size_t revived = 0;
    h.bus.add_observer([&revived](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Revived) {
            ++revived;
        }
    });
    const maker::Edited edited =
        maker::apply_behaviour_edit(h.bus, h.catalog, r.id, hwfix::high_water_v2(h.catalog));
    CHECK_FALSE(edited.ok);
    CHECK(contains(edited.reason, "succession"));
    CHECK(contains(edited.reason, "hw.State v2"));
    CHECK(revived == 0);
    CHECK(high_of(*r.weave) == 7);
    CHECK(r.weave->definition().revision == 1);
    CHECK(h.catalog.mounted("zengine.maker.hw.r1"));
    CHECK_FALSE(h.catalog.mounted("zengine.maker.hw.r2"));
}

// ---- FC-7: the schema edit -------------------------------------------------------------------

TEST_CASE("FC-7: a schema edit is a succession -- v2 authored with its conversion, prepared, "
          "adopted, committed; the role moves, high is still 7, label reads high water, the "
          "predecessor is gone") {
    Host h;
    h.listen();
    const maker::Coordinator c = maker::register_succession(h.bus, h.catalog);
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(r.ok, r.reason);
    h.send(r.id, hwfix::sample(3));
    h.send(r.id, hwfix::sample(7));
    h.pump();
    REQUIRE(high_of(*r.weave) == 7);

    // The successor, through its own bytes: v2 with its conversion.
    const maker::Admitted v2 = round_trip(hwfix::high_water_v2(h.catalog));
    REQUIRE_MESSAGE(v2.ok, v2.reason);
    REQUIRE(v2.definition.conversion.has_value());

    // THE PROTOCOL'S COST, measured on this edit: every delivery and refusal on the tap from
    // begin to outcome, the bounded pump turns it took, and the wall time -- printed, so the
    // panel phase inherits the numbers from the run rather than from a report.
    std::size_t deliveries = 0;
    std::size_t refusals = 0;
    const loom::ObserverId tap = h.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Delivered) {
            ++deliveries;
        } else if (ev.kind == loom::EventKind::Refused) {
            ++refusals;
        }
    });
    std::size_t turns = 0;
    auto pump_counted = [&] {
        while (h.bus.pending() > 0) {
            h.bus.pump_pending();
            ++turns;
        }
    };
    const auto started = std::chrono::steady_clock::now();

    loom::PreparedReplacement txn(h.bus);
    const maker::Begun b = maker::begin_schema_edit(h.bus, h.catalog, c, "hw", v2.definition, txn);
    REQUIRE_MESSAGE(b.ok, b.reason);
    CHECK(b.incumbent == r.id);
    CHECK(h.bus.sealed(b.candidate));
    CHECK(h.catalog.mounted("zengine.maker.hw.r2"));
    // The conversion is an edge in the host's catalog, in the migration convention.
    const op::OperatorDef* edge = h.catalog.find("zengine.migrate.hw.State.v1-to-v2");
    REQUIRE(edge != nullptr);
    CHECK(op::declares_migration(*edge));

    pump_counted(); // Quiesce -> Quiesced -> the edge -> Adopt -> Adopted -> offered
    CHECK(r.weave->quiescing());
    CHECK(c.weave->quiesced());
    CHECK_MESSAGE(c.weave->converted(), c.weave->reason());
    CHECK(c.weave->asked());
    CHECK(c.weave->adopted());
    CHECK_MESSAGE(c.weave->ready(), c.weave->why());
    CHECK(c.weave->offered());
    REQUIRE(txn.state() == loom::TxnState::Ready);

    // Commit is the host's decision, and it SCHEDULES.
    REQUIRE(txn.commit(1).ok);
    CHECK(txn.state() == loom::TxnState::AdmissionPending);
    pump_counted();
    const auto outcome = txn.take_outcome();
    const auto ended = std::chrono::steady_clock::now();
    h.bus.remove_observer(tap);
    std::printf("[maker measure] one schema edit, begin to outcome: %zu deliveries, %zu refusals, "
                "%zu pump turns, %lld us\n",
                deliveries, refusals, turns,
                static_cast<long long>(
                    std::chrono::duration_cast<std::chrono::microseconds>(ended - started).count()));
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Committed);
    CHECK(h.bus.role_holder("hw") == b.candidate);
    CHECK(b.candidate_weave->activated() == 1);
    CHECK(b.candidate_weave->activation_attested());

    // The converted state: the label written from the constant, high carried -- by NAME, with
    // `label` first in v2.
    CHECK(high_of(*b.candidate_weave) == 7);
    CHECK(b.candidate_weave->state().get("label")->as_text() == "high water");
    CHECK(b.candidate_weave->state().schema().version() == 2);

    // Retire the predecessor: its bodies leave the catalog with it.
    std::unique_ptr<loom::Weave> retired = h.bus.unregister_weave(b.incumbent);
    REQUIRE(retired != nullptr);
    retired.reset();
    CHECK(h.bus.weave(b.incumbent) == nullptr);
    CHECK_FALSE(h.catalog.mounted("zengine.maker.hw.r1"));
    CHECK(h.catalog.mounted("zengine.maker.hw.r2"));

    // The successor serves under the role.
    h.send_to_role("hw", hwfix::sample(9));
    h.pump();
    CHECK(high_of(*b.candidate_weave) == 9);
    const Message* published = h.client->last("hw.HighWater");
    REQUIRE(published != nullptr);
    CHECK(published->payload.get("high")->as_int() == 9);
    CHECK(published->sender == b.candidate);
}

TEST_CASE("FC-7: a hw.Sample is handled before the boundary, refused by name after it while the "
          "incumbent holds the role, and handled by the successor after the role moves -- never "
          "lost") {
    Host h;
    h.listen();
    const maker::Coordinator c = maker::register_succession(h.bus, h.catalog);
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(r.ok, r.reason);

    // A: queued before the boundary.
    h.send_to_role("hw", hwfix::sample(3), 1);
    loom::PreparedReplacement txn(h.bus);
    const maker::Begun b =
        maker::begin_schema_edit(h.bus, h.catalog, c, "hw", hwfix::high_water_v2(h.catalog), txn);
    REQUIRE_MESSAGE(b.ok, b.reason);
    // B: queued after the boundary, while the incumbent still holds the role.
    h.send_to_role("hw", hwfix::sample(9), 2);
    h.pump();
    CHECK(high_of(*r.weave) == 3);               // A handled; B did not move it
    CHECK(r.weave->handled() == 1);
    CHECK(r.weave->refused_after_boundary() == 1);
    const Message* refused = h.client->last("zen.Refused");
    REQUIRE(refused != nullptr);
    CHECK(refused->correlation == 2);
    CHECK(contains(reason_of(*refused), "hw.Sample"));
    CHECK(contains(reason_of(*refused), "quiesced"));
    REQUIRE(txn.state() == loom::TxnState::Ready);

    // C: queued around commit -- behind the admission envelope, so it meets the successor.
    REQUIRE(txn.commit(1).ok);
    h.send_to_role("hw", hwfix::sample(5), 3);
    h.pump();
    const auto outcome = txn.take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Committed);
    CHECK(h.bus.role_holder("hw") == b.candidate);
    CHECK(high_of(*b.candidate_weave) == 5); // converted 3, then C's 5
    CHECK(b.candidate_weave->handled() == 1);

    // D: after the role moved.
    h.send_to_role("hw", hwfix::sample(4), 4);
    h.pump();
    CHECK(high_of(*b.candidate_weave) == 5);
    CHECK(b.candidate_weave->handled() == 2);
    // Nothing was lost: A is in the value the successor inherited, B was refused to its sender
    // by name, C and D were handled by the successor.
    CHECK(h.client->count("zen.Refused") == 1);
    CHECK(h.client->count("hw.HighWater") == 3);
}

// ---- f: the conversion -----------------------------------------------------------------------

TEST_CASE("f: a conversion the write refuses reaches no candidate -- the transaction aborts, the "
          "incumbent is resumed and is still the service") {
    Host h;
    h.listen();
    const maker::Coordinator c = maker::register_succession(h.bus, h.catalog);
    // A v1 with an optional, unbound `note`, never written; a v2 that REQUIRES `note` and sources
    // it from v1's -- a plan the schemas admit and a value refuses.
    const auto v1 = loom::SchemaBuilder("hw.State", 1)
                        .field("high", loom::Kind::Int)
                        .field("note", loom::Kind::Text, /*required=*/false)
                        .build();
    const auto v2 = loom::SchemaBuilder("hw.State", 2)
                        .field("high", loom::Kind::Int)
                        .field("note", loom::Kind::Text)
                        .build();
    maker::Definition d1 = hwfix::high_water(h.catalog);
    d1.state = v1;
    d1.on[0].body = hwfix::two_arg_body(h.catalog, d1.trigger_identity(d1.on[0]), *d1.state,
                                        *d1.on[0].message, op::kMaxInt);
    maker::Definition d2 = hwfix::high_water(h.catalog, 2);
    d2.state = v2;
    d2.on[0].body = hwfix::two_arg_body(h.catalog, d2.trigger_identity(d2.on[0]), *d2.state,
                                        *d2.on[0].message, op::kMaxInt);
    maker::Conversion conv;
    conv.from = v1;
    conv.fields.push_back(maker::FieldSource{"high", std::string("high"), std::nullopt});
    conv.fields.push_back(maker::FieldSource{"note", std::string("note"), std::nullopt});
    d2.conversion = conv;
    REQUIRE_MESSAGE(round_trip(d2).ok, round_trip(d2).reason); // the plan admits

    const maker::Registered r = maker::register_definition(h.bus, h.catalog, d1);
    REQUIRE_MESSAGE(r.ok, r.reason);
    h.send(r.id, hwfix::sample(7));
    h.pump();

    loom::PreparedReplacement txn(h.bus);
    const maker::Begun b = maker::begin_schema_edit(h.bus, h.catalog, c, "hw", d2, txn);
    REQUIRE_MESSAGE(b.ok, b.reason);
    h.pump(); // Quiesced -> the edge refuses -> abort, Resume
    CHECK(c.weave->quiesced());
    CHECK_FALSE(c.weave->converted());
    CHECK(c.weave->aborted());
    CHECK_FALSE(c.weave->asked());
    CHECK_FALSE(c.weave->adopted());
    CHECK(contains(c.weave->reason(), "`note`"));
    CHECK(contains(c.weave->reason(), "absent"));
    const auto outcome = txn.take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Aborted);
    CHECK(outcome->reason == loom::TxnReason::ExplicitAbort);

    // No candidate was reached; the incumbent is resumed and still the service, state intact.
    CHECK(h.bus.role_holder("hw") == r.id);
    CHECK_FALSE(r.weave->quiescing());
    CHECK(high_of(*r.weave) == 7);
    h.send(r.id, hwfix::sample(9));
    h.pump();
    CHECK(high_of(*r.weave) == 9);
    CHECK(r.weave->refused_after_boundary() == 0);
}

TEST_CASE("f: the field-wise write refuses a target with no source, a source the schema lacks, a "
          "kind mismatch, two sources, a constant of a non-scalar kind, and a predecessor field "
          "neither copied nor dropped") {
    const auto sub = loom::SchemaBuilder("w.Sub", 1).field("k", loom::Kind::Int).build();
    const auto from = loom::SchemaBuilder("w.From", 1)
                          .field("a", loom::Kind::Int)
                          .field("b", loom::Kind::Text)
                          .field("c", loom::Kind::Int)
                          .field("o", loom::Kind::Text, /*required=*/false)
                          .message("m", sub)
                          .build();
    const auto to = loom::SchemaBuilder("w.To", 1)
                        .field("a", loom::Kind::Int)
                        .field("x", loom::Kind::Int)
                        .message("m", sub)
                        .field("p", loom::Kind::Text, /*required=*/false)
                        .build();
    using FS = maker::FieldSource;
    const FS a{"a", std::string("a"), std::nullopt};
    const FS m{"m", std::string("m"), std::nullopt};

    // A required target with neither a source nor a constant.
    std::string why = maker::plan_fields(*from, *to, {a, m}, std::nullopt, false, "the emit");
    CHECK(contains(why, "required field `x`"));
    // A source the schema lacks.
    why = maker::plan_fields(*from, *to, {a, m, FS{"x", std::string("zz"), std::nullopt}},
                             std::nullopt, false, "the emit");
    CHECK(contains(why, "reads `zz`"));
    // A kind mismatch.
    why = maker::plan_fields(*from, *to, {a, m, FS{"x", std::string("b"), std::nullopt}},
                             std::nullopt, false, "the emit");
    CHECK(contains(why, "the kinds differ"));
    // Two sources.
    why = maker::plan_fields(*from, *to,
                             {a, m, FS{"x", std::string("c"), loom::Cell::integer(1)}},
                             std::nullopt, false, "the emit");
    CHECK(contains(why, "two sources"));
    // A constant of a non-scalar kind (a Message field from a constant).
    why = maker::plan_fields(*from, *to, {a, FS{"m", std::nullopt, loom::Cell::integer(1)},
                                          FS{"x", std::string("c"), std::nullopt}},
                             std::nullopt, false, "the emit");
    CHECK(contains(why, "one of Int, Float, Text or Bool"));
    // A constant of the wrong scalar kind.
    why = maker::plan_fields(*from, *to, {a, m, FS{"x", std::nullopt, loom::Cell::text("1")}},
                             std::nullopt, false, "the emit");
    CHECK(contains(why, "from a Text constant"));
    // A target the shape lacks, and a field named twice.
    why = maker::plan_fields(*from, *to, {a, m, FS{"x", std::string("c"), std::nullopt},
                                          FS{"q", std::string("c"), std::nullopt}},
                             std::nullopt, false, "the emit");
    CHECK(contains(why, "does not declare: `q`"));
    why = maker::plan_fields(*from, *to, {a, a, m, FS{"x", std::string("c"), std::nullopt}},
                             std::nullopt, false, "the emit");
    CHECK(contains(why, "names `a` twice"));

    // The emit leaves unnamed source fields alone; the EDGE does not.
    const std::vector<FS> copy_ax{a, m, FS{"x", std::string("c"), std::nullopt}};
    CHECK(maker::plan_fields(*from, *to, copy_ax, std::nullopt, false, "the emit").empty());
    why = maker::plan_fields(*from, *to, copy_ax, std::nullopt, true, "the conversion");
    CHECK(contains(why, "neither copies nor drops the predecessor's `b`"));
    why = maker::plan_fields(*from, *to, copy_ax, std::vector<std::string>{"b", "nope"}, true,
                             "the conversion");
    CHECK(contains(why, "drops `nope`"));
    why = maker::plan_fields(*from, *to, copy_ax, std::vector<std::string>{"b", "a", "o"}, true,
                             "the conversion");
    CHECK(contains(why, "both copies and drops `a`"));
    // Authored loss: every predecessor field copied or dropped, and the plan is sound.
    const std::vector<std::string> drops{"b", "o"};
    CHECK(maker::plan_fields(*from, *to, copy_ax, drops, true, "the conversion").empty());

    // The write itself, on a value: copies by NAME, constants land, and the one refusal only a
    // value can raise -- a required target whose optional source is absent.
    loom::Value was(from);
    was.set("a", loom::Cell::integer(4));
    was.set("b", loom::Cell::text("bee"));
    was.set("c", loom::Cell::integer(9));
    loom::Value inner(sub);
    inner.set("k", loom::Cell::integer(1));
    was.set("m", loom::Cell::message(inner));
    maker::Written w = maker::write_fields(was, to, copy_ax, drops, true, "the conversion");
    REQUIRE_MESSAGE(w.ok, w.reason);
    CHECK(w.value->get("a")->as_int() == 4);
    CHECK(w.value->get("x")->as_int() == 9);
    CHECK(w.value->get("m")->as_message()->get("k")->as_int() == 1);
    CHECK(w.value->get("p") == nullptr);
    const auto needs_p = loom::SchemaBuilder("w.To", 2)
                             .field("a", loom::Kind::Int)
                             .field("p", loom::Kind::Text)
                             .build();
    w = maker::write_fields(was, needs_p, {a, FS{"p", std::string("o"), std::nullopt}},
                            std::vector<std::string>{"b", "c", "m"}, true, "the conversion");
    CHECK_FALSE(w.ok);
    CHECK(contains(w.reason, "requires `p`"));
    CHECK(contains(w.reason, "`o` is absent"));
}

// ---- FC-8: inspection and the two files ---------------------------------------------------

TEST_CASE("FC-8: zen.PokeDescribe names hw.State v1 and every field; zen.PokeRead reads high; write "
          "and reset are refused by name") {
    Host h;
    h.listen();
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(r.ok, r.reason);
    h.send(r.id, hwfix::sample(7));
    h.pump();

    h.send(r.id, loom::to_value(loom::PokeDescribe{}), 31);
    h.pump();
    const Message* described = h.client->last("zen.PokeStructure");
    REQUIRE(described != nullptr);
    const loom::PokeStructure structure = loom::from_value<loom::PokeStructure>(described->payload);
    CHECK(structure.state_schema == "hw.State");
    CHECK(structure.state_version == 1);
    REQUIRE(structure.fields.size() == 1);
    CHECK(structure.fields[0].name == "high");
    CHECK(structure.fields[0].type == "Int");
    CHECK_FALSE(structure.fields[0].writable);
    CHECK_FALSE(structure.fields[0].hidden);
    CHECK(described->correlation == 31);

    h.send(r.id, loom::to_value(loom::PokeRead{"high"}), 32);
    h.pump();
    const Message* read = h.client->last("zen.Result");
    REQUIRE(read != nullptr);
    CHECK(loom::from_value<loom::Result>(read->payload).value == "7");

    h.send(r.id, loom::to_value(loom::PokeRead{"nope"}), 33);
    h.pump();
    const Message* unknown = h.client->last("zen.Refused");
    REQUIRE(unknown != nullptr);
    CHECK(unknown->correlation == 33);
    CHECK(contains(reason_of(*unknown), "no field 'nope'"));

    h.send(r.id, loom::to_value(loom::PokeWrite{"high", "1"}), 34);
    h.pump();
    const Message* write = h.client->last("zen.Refused");
    REQUIRE(write->correlation == 34);
    CHECK(contains(reason_of(*write), "written by its triggers"));
    h.send(r.id, loom::to_value(loom::PokeResetState{}), 35);
    h.pump();
    const Message* reset = h.client->last("zen.Refused");
    REQUIRE(reset->correlation == 35);
    CHECK(contains(reason_of(*reset), "PokeResetState"));
    CHECK(h.client->count("zen.Ack") == 0);
    CHECK(high_of(*r.weave) == 7);
}

TEST_CASE("FC-8: the definition and the state are two native files written by one process, and a "
          "fresh process reads them back with high == 7") {
    // The directory belongs to this suite and this process (VM-POP-14).
#ifdef _WIN32
    const int pid = _getpid();
#else
    const int pid = static_cast<int>(getpid());
#endif
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("zengine-maker-" + std::to_string(pid));
    std::filesystem::remove_all(dir);
    REQUIRE(std::filesystem::create_directories(dir));

    std::string command = "\"" + std::filesystem::path(MAKER_AUTHOR_EXE).make_preferred().string() +
                          "\" \"" + dir.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\""; // cmd.exe's rule for a line that begins with a quote
#endif
    const int status = std::system(command.c_str());
    // The work HAPPENED before anything about what it said (VM-FIX-17).
    REQUIRE_MESSAGE(status == 0, command);

    const maker::FileBytes definition_file = maker::read_file((dir / "hw.definition").string());
    REQUIRE_MESSAGE(definition_file.ok, definition_file.reason);
    const maker::FileBytes state_file = maker::read_file((dir / "hw.state").string());
    REQUIRE_MESSAGE(state_file.ok, state_file.reason);
    // Native bytes: a well-formed native envelope, and not the compat text codec.
    CHECK(loom::parse(definition_file.bytes).well_formed());
    CHECK(loom::parse(state_file.bytes).well_formed());
    CHECK_FALSE(loom::compat::parse(definition_file.bytes).well_formed());
    CHECK_FALSE(loom::compat::parse(state_file.bytes).well_formed());

    const maker::Admitted read = maker::read_definition(definition_file.bytes);
    REQUIRE_MESSAGE(read.ok, read.reason);
    CHECK(read.definition.name == "hw");
    const maker::Written state = maker::read_state(state_file.bytes, read.definition.state);
    REQUIRE_MESSAGE(state.ok, state.reason);
    CHECK(state.value->get("high")->as_int() == 7);

    // Registered in THIS process from the two files, the weave reports the same through its
    // inspection door.
    Host h;
    h.listen();
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, read.definition);
    REQUIRE_MESSAGE(r.ok, r.reason);
    const loom::ReviveOutcome swapped = h.bus.swap_state(r.id, state_file.bytes);
    REQUIRE(swapped.revived);
    h.send(r.id, loom::to_value(loom::PokeRead{"high"}));
    h.pump();
    const Message* answer = h.client->last("zen.Result");
    REQUIRE(answer != nullptr);
    CHECK(loom::from_value<loom::Result>(answer->payload).value == "7");

    std::filesystem::remove_all(dir);
}

// ---- b: the format ----------------------------------------------------------------------------

TEST_CASE("b: a definition claiming another version is refused by its number, and one whose own "
          "version field disagrees with its envelope is a forgery") {
    Host h;
    const maker::Definition d = hwfix::high_water(h.catalog);
    const loom::Value v1 = maker::encode_definition(d);

    // The same fields under an envelope of version 2: refused ON THE CLAIM, by its number.
    const auto envelope_v2 = loom::make_schema(maker::definition_schema()->name(), 2,
                                               maker::definition_schema()->fields());
    loom::Value v2(envelope_v2);
    for (const loom::Field& f : v1.schema().fields()) {
        if (const loom::Cell* c = v1.get(f.name); c != nullptr) {
            v2.set(f.name, *c);
        }
    }
    const maker::Admitted other = maker::read_definition(loom::serialize(v2));
    CHECK_FALSE(other.ok);
    CHECK(contains(other.reason, "version 2"));
    CHECK(contains(other.reason, "converts no other"));

    // A version-1 envelope whose own field says 2: a forgery.
    loom::Value forged = v1;
    forged.set("format_version", loom::Cell::integer(2));
    const maker::Admitted forgery = maker::read_definition(loom::serialize(forged));
    CHECK_FALSE(forgery.ok);
    CHECK(contains(forgery.reason, "forgery"));

    // And another word is another kind of file.
    loom::Value other_word = v1;
    other_word.set("format", loom::Cell::text("something-else"));
    const maker::Admitted word = maker::read_definition(loom::serialize(other_word));
    CHECK_FALSE(word.ok);
    CHECK(contains(word.reason, "not a maker definition"));

    CHECK(maker::read_definition(loom::serialize(v1)).ok);
    CHECK(maker::kDefinitionSchemaVersion == static_cast<std::uint32_t>(maker::kFormatVersion));
}

TEST_CASE("b: the definition schema carries no author field, and the file says so") {
    CHECK(maker::definition_schema()->find("author") == nullptr);
    CHECK(maker::definition_schema()->find("signature") == nullptr);

    // A source tripwire (VM-WALL-10): a pure string check over the whole file, matching the
    // TOKEN and not a substring -- `authored` is not it -- and a comment can trip it.
    std::ifstream in(MAKER_DEFINITION_HPP, std::ios::binary);
    REQUIRE(in);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(contains(text, "format_version")); // the positive control: the file was read
    auto is_word = [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
               ch == '_';
    };
    std::size_t hits = 0;
    const std::string token = "author";
    for (std::size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + 1)) {
        const bool left = at == 0 || !is_word(text[at - 1]);
        const bool right = at + token.size() >= text.size() || !is_word(text[at + token.size()]);
        if (left && right) {
            ++hits;
        }
    }
    CHECK(hits == 0);
}

TEST_CASE("b: a state file of another version is refused by name at load, and nothing converts it") {
    Host h;
    // A v1 state on disk...
    loom::Value was(hwfix::state_v1());
    was.set("high", loom::Cell::integer(7));
    const std::string bytes = loom::serialize(was);
    // ...and a v2 definition LIVE, with its conversion edge mounted in the catalog.
    const maker::Registered v2 = maker::register_definition(h.bus, h.catalog, hwfix::high_water_v2(h.catalog));
    REQUIRE_MESSAGE(v2.ok, v2.reason);
    REQUIRE(h.catalog.find("zengine.migrate.hw.State.v1-to-v2") != nullptr);

    const maker::Written read = maker::read_state(bytes, v2.weave->definition().state);
    CHECK_FALSE(read.ok);
    CHECK(contains(read.reason, "`hw.State v1`"));
    CHECK(contains(read.reason, "`hw.State v2`"));
    CHECK(contains(read.reason, "nothing converts"));
    // The same bytes at their own version admit.
    CHECK(maker::read_state(bytes, hwfix::state_v1()).ok);
}

// ---- a: admission -----------------------------------------------------------------------------

TEST_CASE("a: a definition is refused when an on names an unaccepted message, an unknown output "
          "field, or an emit field with no source") {
    Host h;
    const auto other = loom::SchemaBuilder("hw.Other", 1).field("value", loom::Kind::Int).build();

    maker::Definition unaccepted = hwfix::high_water(h.catalog);
    unaccepted.on[0].message = other; // accepted list still names hw.Sample only
    maker::Admitted a = round_trip(unaccepted);
    CHECK_FALSE(a.ok);
    CHECK(contains(a.reason, "`hw.Other v1`"));
    CHECK(contains(a.reason, "does not accept"));

    maker::Definition unknown_output = hwfix::high_water(h.catalog);
    unknown_output.on[0].output = "nope";
    a = round_trip(unknown_output);
    CHECK_FALSE(a.ok);
    CHECK(contains(a.reason, "writes `nope`"));

    maker::Definition undeclared_emit = hwfix::high_water(h.catalog);
    undeclared_emit.on[0].emits[0].message = other;
    a = round_trip(undeclared_emit);
    CHECK_FALSE(a.ok);
    CHECK(contains(a.reason, "emits `hw.Other v1`"));
    CHECK(contains(a.reason, "does not declare"));

    maker::Definition sourceless = hwfix::high_water(h.catalog);
    sourceless.on[0].emits[0].fields.clear();
    a = round_trip(sourceless);
    CHECK_FALSE(a.ok);
    CHECK(contains(a.reason, "the emit of `hw.HighWater`"));
    CHECK(contains(a.reason, "required field `high`"));

    CHECK(round_trip(hwfix::high_water(h.catalog)).ok);
}

// ---- e: an aborted succession ------------------------------------------------------------------

TEST_CASE("e: an aborted succession discards the sealed candidate and leaves the incumbent the "
          "service with its state") {
    Host h;
    h.listen();
    const maker::Coordinator c = maker::register_succession(h.bus, h.catalog);
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, hwfix::high_water(h.catalog));
    REQUIRE_MESSAGE(r.ok, r.reason);
    h.send(r.id, hwfix::sample(7));
    h.pump();

    loom::PreparedReplacement txn(h.bus);
    const maker::Begun b =
        maker::begin_schema_edit(h.bus, h.catalog, c, "hw", hwfix::high_water_v2(h.catalog), txn);
    REQUIRE_MESSAGE(b.ok, b.reason);
    h.pump();
    REQUIRE(txn.state() == loom::TxnState::Ready);
    CHECK(r.weave->quiescing());
    CHECK(h.catalog.mounted("zengine.maker.hw.r2"));

    // The host decides against it.
    const loom::TxnResult aborted = maker::abort_schema_edit(h.bus, c, txn);
    REQUIRE(aborted.ok);
    h.pump(); // Resume reaches the incumbent
    const auto outcome = txn.take_outcome();
    REQUIRE(outcome.has_value());
    CHECK(outcome->state == loom::TxnState::Aborted);
    CHECK(outcome->reason == loom::TxnReason::ExplicitAbort);

    // The sealed candidate is discarded by the substrate, and its bodies left with it.
    CHECK(h.bus.weave(b.candidate) == nullptr);
    CHECK_FALSE(h.catalog.mounted("zengine.maker.hw.r2"));
    // The incumbent is the service, resumed, with its state.
    CHECK(h.bus.role_holder("hw") == r.id);
    CHECK_FALSE(r.weave->quiescing());
    CHECK(high_of(*r.weave) == 7);
    h.send_to_role("hw", hwfix::sample(9));
    h.pump();
    CHECK(high_of(*r.weave) == 9);
    CHECK(h.catalog.mounted("zengine.maker.hw.r1"));
}

// ---- 2, 3: the maker's two decisions on kinds and required -------------------------------------

TEST_CASE("2: a definition whose state nests a message and a list decodes through its referenced "
          "section -- the seven kinds, closed") {
    Host h;
    const auto nested = loom::SchemaBuilder("nest.Sample", 1).field("value", loom::Kind::Int).build();
    const auto state = loom::SchemaBuilder("nest.State", 1)
                           .message("last", nested)
                           .list("history", loom::type_message(nested))
                           .field("high", loom::Kind::Int)
                           .build();
    maker::Definition d;
    d.name = "nest";
    d.state = state;
    d.accepts = {nested};
    maker::On on;
    on.message = nested;
    on.body = hwfix::two_arg_body(h.catalog, d.trigger_identity(on), *state, *nested, op::kMaxInt);
    on.output = "high";
    d.on.push_back(std::move(on));

    const loom::Value encoded = maker::encode_definition(d);
    const loom::Cell* referenced = encoded.get("referenced");
    REQUIRE(referenced != nullptr);
    REQUIRE(referenced->as_list().size() == 1); // nest.Sample once, though nested twice
    CHECK(referenced->as_list()[0].as_message()->get("name")->as_text() == "nest.Sample");

    const maker::Admitted read = maker::read_definition(loom::serialize(encoded));
    REQUIRE_MESSAGE(read.ok, read.reason);
    CHECK(read.definition.state->content_id() == state->content_id());
    CHECK(read.definition.state->fields()[0].type.kind == loom::Kind::Message);
    CHECK(read.definition.state->fields()[1].type.kind == loom::Kind::List);

    const maker::Registered r = maker::register_definition(h.bus, h.catalog, read.definition);
    REQUIRE_MESSAGE(r.ok, r.reason);
    // The default state: a default nested message, an empty list, zero.
    CHECK(r.weave->state().get("last")->as_message()->get("value")->as_int() == 0);
    CHECK(r.weave->state().get("history")->as_list().empty());
    loom::Value s(nested);
    s.set("value", loom::Cell::integer(4));
    h.bus.send(r.id, Message(std::move(s)));
    h.pump();
    CHECK(high_of(*r.weave) == 4);
}

TEST_CASE("3: an optional state field bound by a trigger is refused at admission; an unbound "
          "optional field is admitted and absent in the default state") {
    Host h;
    const auto state = loom::SchemaBuilder("hw.State", 1)
                           .field("high", loom::Kind::Int)
                           .field("bonus", loom::Kind::Int, /*required=*/false)
                           .build();
    maker::Definition bound = hwfix::high_water(h.catalog);
    bound.state = state;
    {
        op::Builder b(h.catalog, bound.trigger_identity(bound.on[0]),
                      hwfix::pack_ports(*state, *bound.on[0].message));
        const op::Builder::Ref answer = b.call(op::kMaxInt, {b.input("high"), b.input("bonus")});
        const op::OperatorDef def = std::move(b).result("value", answer);
        bound.on[0].body = *def.composition();
    }
    const maker::Admitted refused = round_trip(bound);
    CHECK_FALSE(refused.ok);
    CHECK(contains(refused.reason, "optional state field `bonus`"));

    maker::Definition unbound = hwfix::high_water(h.catalog);
    unbound.state = state;
    unbound.on[0].body = hwfix::two_arg_body(h.catalog, unbound.trigger_identity(unbound.on[0]),
                                             *state, *unbound.on[0].message, op::kMaxInt);
    const maker::Admitted admitted = round_trip(unbound);
    REQUIRE_MESSAGE(admitted.ok, admitted.reason);
    const maker::Registered r = maker::register_definition(h.bus, h.catalog, admitted.definition);
    REQUIRE_MESSAGE(r.ok, r.reason); // the default state conformed with `bonus` absent
    CHECK(r.weave->state().get("bonus") == nullptr);
    CHECK(r.weave->state().get("high")->as_int() == 0);
    h.bus.send(r.id, Message(hwfix::sample(6)));
    h.pump();
    CHECK(high_of(*r.weave) == 6);
    CHECK(r.weave->state().get("bonus") == nullptr);
}
