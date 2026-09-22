// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The inventory suite -- ONE SLOT, EMPTY OR ONE ASSOCIATED ITEM+METADATA PAIR.
//
// THIS FILE OWNS the codec (inventory/codec.hpp: the byte envelope, encode_pair/decode_pair) and
// the weave (inventory/weave.hpp: InventorySet/InventoryGet/InventoryCaptureDescribe over an
// ordinary bus). What "inventory" reaches through a guest's own grant is the guests suite's
// (test_workshop_guests.cpp) -- this suite mounts the weave directly, with a trusted grant, and
// proves the weave's own contract: truthful empty/replacement/refusal, independent custody, and
// that a second, entirely runtime-defined item schema needs no change here to be stored and read
// back. The one live external-Loom-host journey is `external-host/tools/workshop/inventory_capture.py`,
// proven by the reportback's own evidence, not repeated here.

#include "doctest.h"

#include "inventory/codec.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory/weave.hpp"
#include "workshop/admission.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

TEST_SUITE_BEGIN("inventory");

namespace {
namespace inv = zengine::inventory;

loom::Bytes as_bytes(const std::string& s) { return loom::Bytes(s.begin(), s.end()); }

// =============================================================================
// The codec: encode_pair/decode_pair, entirely off the bus
// =============================================================================

std::shared_ptr<const loom::Schema> child_schema() {
    return loom::SchemaBuilder("inventorytest.Child", 1).field("note", loom::Kind::Text).build();
}

/// A schema with no C++ struct anywhere in this tree -- the "runtime-defined item schema" the
/// contract asks for, with a nested message and a list so this proves more than a scalar
/// container.
std::shared_ptr<const loom::Schema> sample_schema() {
    return loom::SchemaBuilder("inventorytest.Sample", 1)
        .field("count", loom::Kind::Int)
        .message("child", child_schema())
        .list("tags", loom::type_of(loom::Kind::Text))
        .build();
}

loom::Value make_sample(std::int64_t count, const std::string& note,
                        const std::vector<std::string>& tags) {
    loom::Value child(child_schema());
    child.set("note", loom::Cell::text(note));
    loom::Value sample(sample_schema());
    sample.set("count", loom::Cell::integer(count));
    sample.set("child", loom::Cell::message(std::move(child)));
    loom::Cell::Array tag_cells;
    for (const std::string& t : tags) {
        tag_cells.push_back(loom::Cell::text(t));
    }
    sample.set("tags", loom::Cell::list(std::move(tag_cells)));
    return sample;
}

std::shared_ptr<const loom::Schema> note_schema() {
    return loom::SchemaBuilder("inventorytest.Note", 1).field("who", loom::Kind::Text).build();
}

} // namespace

TEST_CASE("codec: a runtime-defined schema with a nested message and list round-trips with no "
         "compiled struct of its own") {
    const loom::Value sample = make_sample(7, "nested", {"a", "b"});
    loom::Value note(note_schema());
    note.set("who", loom::Cell::text("a fresh reader"));

    const std::string encoded = inv::encode_pair(sample, {note});
    const inv::DecodedPair decoded = inv::decode_pair(encoded);

    // Read back through the GENERIC Value/Cell surface alone -- exactly what a reader with no
    // compiled knowledge of "Sample"/"Child"/"Note" has available.
    CHECK(decoded.item.schema().name() == "inventorytest.Sample");
    CHECK(decoded.item.get("count")->as_int() == 7);
    CHECK(decoded.item.get("child")->as_message()->get("note")->as_text() == "nested");
    REQUIRE(decoded.item.get("tags")->as_list().size() == 2);
    CHECK(decoded.item.get("tags")->as_list()[0].as_text() == "a");
    CHECK(decoded.item.get("tags")->as_list()[1].as_text() == "b");
    REQUIRE(decoded.metadata.size() == 1);
    CHECK(decoded.metadata[0].schema().name() == "inventorytest.Note");
    CHECK(decoded.metadata[0].get("who")->as_text() == "a fresh reader");
}

TEST_CASE("codec: independent custody -- mutating the source value after encoding changes "
         "nothing already stored") {
    const auto schema = loom::SchemaBuilder("inventorytest.Mutable", 1)
                            .field("n", loom::Kind::Int)
                            .build();
    loom::Value item(schema);
    item.set("n", loom::Cell::integer(1));
    const std::string encoded = inv::encode_pair(item, {});
    item.set("n", loom::Cell::integer(999)); // the SAME object, mutated after encode_pair returned
    const inv::DecodedPair decoded = inv::decode_pair(encoded);
    CHECK(decoded.item.get("n")->as_int() == 1);
}

TEST_CASE("codec: a required field left absent is refused, not silently stored") {
    const auto schema = loom::SchemaBuilder("inventorytest.Required", 1)
                            .field("must_have", loom::Kind::Int)
                            .build();
    const loom::Value incomplete(schema); // every field starts absent
    CHECK_THROWS_AS(inv::encode_pair(incomplete, {}), std::invalid_argument);
}

TEST_CASE("codec: two roots disagreeing under one (name, version) are refused before anything "
         "is written") {
    const auto a = loom::SchemaBuilder("inventorytest.Conflict", 1).field("x", loom::Kind::Int).build();
    const auto b =
        loom::SchemaBuilder("inventorytest.Conflict", 1).field("x", loom::Kind::Text).build();
    loom::Value item(a);
    item.set("x", loom::Cell::integer(1));
    loom::Value meta(b);
    meta.set("x", loom::Cell::text("nope"));
    CHECK_THROWS_AS(inv::encode_pair(item, {meta}), loom::SchemaConflict);
}

TEST_CASE("codec: malformed bytes are refused cleanly, never crash or trust a partial value") {
    CHECK_THROWS_AS(inv::decode_pair(std::string_view("not an envelope at all")),
                    std::invalid_argument);
    const auto schema = loom::SchemaBuilder("inventorytest.Other", 1).field("n", loom::Kind::Int).build();
    loom::Value other(schema);
    other.set("n", loom::Cell::integer(5));
    // Well-formed bytes, but not a Pair envelope at all: refused by identity, not misread.
    CHECK_THROWS_AS(inv::decode_pair(loom::serialize(other)), std::invalid_argument);
}

// =============================================================================
// The weave: InventorySet/InventoryGet/InventoryCaptureDescribe over an ordinary bus
// =============================================================================

namespace {

struct PumpState {
    std::int64_t pumps = 0;
    ZEN_SHAPE(PumpState, 1, ZEN_FIELD(pumps));
};
struct Pump {
    ZEN_SHAPE(Pump, 1);
};

/// A small in-process caller: `act()` runs one callback inside a real delivery, so it may use
/// `Mail::send_to_role` the way a real caller would, and records every answer this suite's
/// three reply shapes bring back.
class Caller final
    : public loom::WeaveBase<Caller, PumpState,
                             loom::Accept<Pump, inv::InventoryState, inv::InventoryEntry,
                                          inv::InventoryCaptured, loom::Ack, loom::Refused>,
                             loom::Emit<inv::InventorySet, inv::InventoryGet,
                                        inv::InventoryCaptureDescribe, inv::InventoryLocate,
                                        inv::InventoryRead, inv::InventoryWrite>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<inv::InventoryState> states;
    std::vector<inv::InventoryCaptured> captures;
    std::vector<inv::InventoryEntry> entries;
    std::int64_t acks = 0;
    std::vector<std::string> refusals;

    void on(const Pump&, loom::Mail& mail) {
        ++state_.pumps;
        if (next) {
            next(mail);
        }
    }
    void on(const inv::InventoryState& s, loom::Mail&) { states.push_back(s); }
    void on(const inv::InventoryEntry& e, loom::Mail& mail) {
        CHECK(mail.answers_ask());
        entries.push_back(e);
    }
    void on(const loom::Ack&, loom::Mail&) { ++acks; }
    void on(const inv::InventoryCaptured& captured, loom::Mail& mail) {
        CHECK(mail.answers_ask());
        captures.push_back(captured);
    }
    void on(const loom::Refused& r, loom::Mail&) { refusals.push_back(r.reason); }
};

/// A poke-inspectable target with real state, so a captured zen.PokeStructure has something
/// genuine to say -- two scalar fields, the read-exposed (write-hidden) default.
struct SampleTargetState {
    std::int64_t count = 42;
    std::string label = "hello";
    ZEN_SHAPE(SampleTargetState, 1, ZEN_FIELD(count), ZEN_FIELD(label));
};
class SampleTarget final
    : public loom::WeaveBase<SampleTarget, SampleTargetState, loom::Accept<>, loom::Emit<>> {};

constexpr const char* kTargetRole = "inventorytest.target";

struct Rig {
    loom::Switchboard bus;
    inv::InventoryWeave* inventory = nullptr;
    loom::WeaveId inventory_id{};
    SampleTarget* target = nullptr;
    loom::WeaveId target_id{};
    Caller* caller = nullptr;
    loom::WeaveId caller_id{};

    Rig() {
        {
            auto w = std::make_unique<inv::InventoryWeave>();
            inventory = w.get();
            inventory_id =
                bus.register_weave(std::move(w), inv::inventory_grant(), std::string(inv::kInventoryRole));
            inventory->zen_set_self(inventory_id);
        }
        {
            auto w = std::make_unique<SampleTarget>();
            target = w.get();
            loom::Grant grant = loom::emit_default_grant(*target);
            loom::allow_poke_answers(grant);
            loom::allow_describe_answers(grant);
            target_id = bus.register_weave(std::move(w), std::move(grant), std::string(kTargetRole));
            target->zen_set_self(target_id);
        }
        caller_id = loom::mount<Caller>(bus);
        caller = static_cast<Caller*>(bus.weave(caller_id));
    }

    void act(std::function<void(loom::Mail&)> f) {
        caller->next = std::move(f);
        (void)bus.send(caller_id, loom::Message(loom::to_value(Pump{})));
        bus.drain_until_idle();
        caller->next = nullptr;
    }

    void set(const loom::Bytes& pair) {
        act([&](loom::Mail& mail) {
            inv::InventorySet req;
            req.pair = pair;
            (void)mail.send_to_role(inv::kInventoryRole, req);
        });
    }
    void get() {
        act([](loom::Mail& mail) { (void)mail.send_to_role(inv::kInventoryRole, inv::InventoryGet{}); });
    }
    void capture(const std::string& target_role) {
        act([&](loom::Mail& mail) {
            inv::InventoryCaptureDescribe req;
            req.target_role = target_role;
            (void)mail.send_to_role(inv::kInventoryRole, req);
        });
    }
};

} // namespace

TEST_CASE("weave: Get before any Set answers a truthful, understandable empty result") {
    Rig r;
    r.get();
    REQUIRE(r.caller->states.size() == 1);
    CHECK_FALSE(r.caller->states.back().occupied);
    CHECK(r.caller->states.back().pair.empty());
}

TEST_CASE("entry reference: saves preserve identity and stale revisions preserve the winner") {
    Rig r;
    const auto first = as_bytes(inv::encode_pair(make_sample(1, "original", {}), {}));
    const auto edited = as_bytes(inv::encode_pair(make_sample(2, "edited", {}), {}));
    r.set(first);
    r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryLocate{}); });
    REQUIRE(r.caller->entries.size() == 1);
    const auto held = r.caller->entries.back();
    CHECK_FALSE(held.reference.owner.empty());
    CHECK_FALSE(held.reference.entry.empty());
    r.act([&](loom::Mail& m) {
        m.send_to_role(inv::kInventoryRole, inv::InventoryWrite{held.reference, held.revision, edited});
    });
    REQUIRE(r.caller->entries.size() == 2);
    CHECK(r.caller->entries.back().reference.entry == held.reference.entry);
    CHECK(r.caller->entries.back().revision == held.revision + 1);
    CHECK(r.caller->entries.back().pair == edited);
    r.act([&](loom::Mail& m) {
        m.send_to_role(inv::kInventoryRole, inv::InventoryWrite{held.reference, held.revision, first});
    });
    REQUIRE(r.caller->refusals.size() == 1);
    r.act([&](loom::Mail& m) {
        m.send_to_role(inv::kInventoryRole, inv::InventoryRead{held.reference});
    });
    CHECK(r.caller->entries.back().pair == edited);
    CHECK(held.pair == first);
}

TEST_CASE("entry reference: replacement cannot redirect an old reference even to identical bytes") {
    Rig r;
    const auto pair = as_bytes(inv::encode_pair(make_sample(4, "same bytes", {}), {}));
    r.set(pair);
    r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryLocate{}); });
    const auto held = r.caller->entries.back();
    r.set(pair);
    r.act([&](loom::Mail& m) {
        m.send_to_role(inv::kInventoryRole, inv::InventoryRead{held.reference});
        m.send_to_role(inv::kInventoryRole, inv::InventoryWrite{held.reference, held.revision, pair});
    });
    CHECK(r.caller->refusals.size() == 2);
    CHECK(r.caller->entries.size() == 1);
    r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryLocate{}); });
    CHECK(r.caller->entries.back().reference.entry != held.reference.entry);
    CHECK(r.caller->entries.back().pair == pair);
}

TEST_CASE("entry reference: empty owner mismatch and malformed edits refuse without a mutation") {
    Rig r;
    r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryLocate{}); });
    REQUIRE(r.caller->refusals.size() == 1);
    r.set(as_bytes(inv::encode_pair(make_sample(1, "kept", {}), {})));
    r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryLocate{}); });
    const auto held = r.caller->entries.back();
    auto wrong = held.reference;
    wrong.owner += "-another-owner";
    r.act([&](loom::Mail& m) {
        m.send_to_role(inv::kInventoryRole, inv::InventoryRead{wrong});
        m.send_to_role(inv::kInventoryRole,
                       inv::InventoryWrite{held.reference, held.revision, as_bytes("broken")});
    });
    CHECK(r.caller->refusals.size() == 3);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRead{held.reference}); });
    CHECK(r.caller->entries.back().pair == held.pair);
    CHECK(r.caller->entries.back().revision == held.revision);
}

TEST_CASE("weave: a valid Set is acknowledged and Get returns exactly what was stored") {
    Rig r;
    const loom::Value sample = make_sample(3, "first", {"x"});
    const std::string encoded = inv::encode_pair(sample, {});
    r.set(as_bytes(encoded));
    REQUIRE(r.caller->acks == 1);
    r.get();
    REQUIRE(r.caller->states.size() == 1);
    CHECK(r.caller->states.back().occupied);
    const inv::DecodedPair decoded = inv::decode_pair(
        std::string_view(reinterpret_cast<const char*>(r.caller->states.back().pair.data()),
                         r.caller->states.back().pair.size()));
    CHECK(decoded.item.get("count")->as_int() == 3);
}

TEST_CASE("weave: a second, differently-shaped item replaces the first with no change to this "
         "weave") {
    Rig r;
    r.set(as_bytes(inv::encode_pair(make_sample(1, "one", {}), {})));
    REQUIRE(r.caller->acks == 1);
    // An item schema this weave has never seen, with its own differently-shaped metadata entry.
    const auto second_schema =
        loom::SchemaBuilder("inventorytest.SecondKind", 1).field("flag", loom::Kind::Bool).build();
    loom::Value second(second_schema);
    second.set("flag", loom::Cell::boolean(true));
    loom::Value meta(note_schema());
    meta.set("who", loom::Cell::text("second capture"));
    r.set(as_bytes(inv::encode_pair(second, {meta})));
    REQUIRE(r.caller->acks == 2);
    r.get();
    const inv::DecodedPair decoded = inv::decode_pair(
        std::string_view(reinterpret_cast<const char*>(r.caller->states.back().pair.data()),
                         r.caller->states.back().pair.size()));
    CHECK(decoded.item.schema().name() == "inventorytest.SecondKind");
    CHECK(decoded.item.get("flag")->as_bool());
    REQUIRE(decoded.metadata.size() == 1);
    CHECK(decoded.metadata[0].get("who")->as_text() == "second capture");
}

TEST_CASE("weave: a malformed Set is refused and the previous pair survives unchanged") {
    Rig r;
    const std::string good = inv::encode_pair(make_sample(9, "kept", {}), {});
    r.set(as_bytes(good));
    REQUIRE(r.caller->acks == 1);
    r.set(as_bytes(std::string("not an envelope")));
    REQUIRE(r.caller->refusals.size() == 1);
    r.get();
    CHECK(r.caller->states.back().pair == as_bytes(good));
}

TEST_CASE("weave: a receiver mutating its own copy of a Get's bytes cannot reach this weave's "
         "storage") {
    Rig r;
    r.set(as_bytes(inv::encode_pair(make_sample(5, "own copy", {}), {})));
    r.get();
    loom::Bytes mine = r.caller->states.back().pair;
    REQUIRE_FALSE(mine.empty());
    mine[0] = static_cast<std::uint8_t>(mine[0] ^ 0xFF); // mutate the RECEIVER's own copy
    r.get();
    CHECK(r.caller->states.back().pair != mine);
    CHECK(inv::decode_pair(std::string_view(reinterpret_cast<const char*>(
                                                r.caller->states.back().pair.data()),
                                            r.caller->states.back().pair.size()))
              .item.get("count")
              ->as_int() == 5);
}

TEST_CASE("weave: an already-returned Get snapshot stays readable after the slot is replaced") {
    Rig r;
    r.set(as_bytes(inv::encode_pair(make_sample(1, "first", {}), {})));
    r.get();
    const loom::Bytes first_snapshot = r.caller->states.back().pair;
    r.set(as_bytes(inv::encode_pair(make_sample(2, "second", {}), {})));
    CHECK(inv::decode_pair(std::string_view(reinterpret_cast<const char*>(first_snapshot.data()),
                                            first_snapshot.size()))
              .item.get("count")
              ->as_int() == 1);
}

TEST_CASE("weave: CaptureDescribe stores a real target's zen.PokeStructure with observed, "
         "attributable metadata") {
    Rig r;
    r.capture(kTargetRole);
    REQUIRE(r.caller->captures.size() == 1);
    r.get();
    REQUIRE(r.caller->states.size() == 1);
    REQUIRE(r.caller->states.back().occupied);
    const inv::DecodedPair decoded = inv::decode_pair(
        std::string_view(reinterpret_cast<const char*>(r.caller->states.back().pair.data()),
                         r.caller->states.back().pair.size()));
    CHECK(decoded.item.schema().name() == "zen.PokeStructure");
    CHECK(decoded.item.get("state_schema")->as_text() == "SampleTargetState");
    REQUIRE(decoded.item.get("fields")->as_list().size() == 2);
    CHECK(decoded.item.get("fields")->as_list()[0].as_message()->get("name")->as_text() == "count");
    CHECK(decoded.item.get("fields")->as_list()[1].as_message()->get("name")->as_text() == "label");
    REQUIRE(decoded.metadata.size() == 2);
    CHECK(decoded.metadata[1].schema().name() == "CaptureRequest");
    CHECK(decoded.metadata[1].get("request")->as_message()->get("target_role")->as_text() == kTargetRole);
    CHECK(r.caller->captures.front().pair == r.caller->states.back().pair);
    CHECK(decoded.metadata[0].schema().name() == "CaptureContext");
    CHECK(decoded.metadata[0].get("requested_role")->as_text() == kTargetRole);
    CHECK(decoded.metadata[0].get("request_shape")->as_text() == "zen.PokeDescribe");
    // OBSERVED, never a claim the target's own payload could make: Loom's own stamped sender.
    CHECK(decoded.metadata[0].get("answered_by")->as_text() ==
          std::to_string(r.target_id.value));
}

TEST_CASE("weave: CaptureDescribe against a role nobody holds is refused in words, and stores "
         "nothing") {
    Rig r;
    r.capture("inventorytest.nobody-holds-this-role");
    REQUIRE(r.caller->refusals.size() == 1);
    r.get();
    CHECK_FALSE(r.caller->states.back().occupied);
}

TEST_CASE("weave: a capture already in flight refuses a second one rather than displacing it") {
    Rig r;
    // Two CaptureDescribe asks queued before either is serviced: the first opens the book and
    // sends zen.PokeDescribe (itself only queued, not yet answered); the second arrives to find
    // the book already awaiting, and is refused immediately, in the same drain.
    r.act([&](loom::Mail& mail) {
        inv::InventoryCaptureDescribe first;
        first.target_role = kTargetRole;
        (void)mail.send_to_role(inv::kInventoryRole, first);
        inv::InventoryCaptureDescribe second;
        second.target_role = kTargetRole;
        (void)mail.send_to_role(inv::kInventoryRole, second);
    });
    REQUIRE(r.caller->captures.size() == 1);
    REQUIRE(r.caller->refusals.size() == 1);
    CHECK(r.caller->refusals.back().find("already in flight") != std::string::npos);
}

namespace {
class Forger final : public loom::WeaveBase<Forger, PumpState, loom::Accept<Pump>,
    loom::Emit<loom::PokeStructure, loom::DispatchRefused>> {
public:
    std::function<void(loom::Mail&)> action;
    void on(const Pump&, loom::Mail& mail) { action(mail); }
};
loom::WeaveId forge_after_caller(Rig& r, bool refusal) {
    auto f = std::make_unique<Forger>();
    auto* raw = f.get();
    loom::Grant g;
    if (refusal) g.allow(loom::DispatchRefused::zen_name, 1, r.inventory_id);
    else g.allow(loom::PokeStructure::zen_name, 1, r.inventory_id);
    const auto id = r.bus.register_weave(std::move(f), std::move(g));
    raw->zen_set_self(id);
    std::uint64_t expected_attempt = 0;
    raw->action = [&, refusal](loom::Mail& mail) {
        if (refusal) {
            loom::DispatchRefused fabricated;
            fabricated.attempt = std::to_string(expected_attempt);
            fabricated.role = kTargetRole;
            fabricated.shape = loom::PokeDescribe::zen_name;
            fabricated.version = 1;
            fabricated.reason = "FORGED-NOT-A-LOOM-NOTICE";
            (void)mail.send(r.inventory_id, fabricated, 1);
        } else {
            loom::PokeStructure fabricated;
            fabricated.state_schema = "FORGED-NOT-THE-TARGET";
            fabricated.state_version = 1;
            (void)mail.send(r.inventory_id, fabricated, 1);
        }
    };
    r.caller->next = [](loom::Mail& mail) {
        (void)mail.send_to_role(inv::kInventoryRole, inv::InventoryCaptureDescribe{kTargetRole});
    };
    // FIFO: caller -> forger -> capture -> fake -> genuine PokeDescribe/reply.
    const auto first = r.bus.send(r.caller_id, loom::Message(loom::to_value(Pump{})));
    (void)r.bus.send(id, loom::Message(loom::to_value(Pump{})));
    // In this controlled FIFO, PokeDescribe is the fifth enqueue. The assertion
    // below proves the forgery matched the REAL attempt, so provenance is the wall.
    expected_attempt = first.seq + 4;
    std::uint64_t actual_attempt = 0;
    const auto observer = r.bus.add_observer([&](const loom::BusEvent& event) {
        if (event.schema_name == loom::PokeDescribe::zen_name) {
            actual_attempt = event.seq;
        }
    });
    r.bus.drain_until_idle();
    CHECK(actual_attempt == expected_attempt);
    r.bus.remove_observer(observer);
    r.caller->next = nullptr;
    return id;
}
}

TEST_CASE("weave: an unrelated structure or refusal cannot settle a role capture") {
    for (const bool refusal : {false, true}) {
        Rig r;
        const auto forger = forge_after_caller(r, refusal);
        REQUIRE(r.caller->captures.size() == 1);
        CHECK(r.caller->refusals.empty());
        r.get();
        const auto& pair = r.caller->states.back().pair;
        const auto decoded = inv::decode_pair(std::string(pair.begin(), pair.end()));
        CHECK(decoded.item.get("state_schema")->as_text() == "SampleTargetState");
        CHECK(decoded.metadata.front().get("answered_by")->as_text() ==
              std::to_string(r.target_id.value));
        CHECK(forger != r.target_id);
    }
}

TEST_CASE("weave: nested payload and metadata survive source mutation and destruction") {
    Rig r;
    auto item = make_sample(4, "item before", {"i"});
    auto metadata = make_sample(8, "metadata before", {"m"});
    const auto pair = as_bytes(inv::encode_pair(item, {metadata}));
    r.set(pair);
    item.get("child")->as_message()->set("note", loom::Cell::text("item after"));
    metadata.get("child")->as_message()->set("note", loom::Cell::text("metadata after"));
    item = loom::Value(sample_schema());
    metadata = loom::Value(sample_schema()); // release the original nested source objects
    r.get();
    const auto& returned = r.caller->states.back().pair;
    auto decoded = inv::decode_pair(std::string(returned.begin(), returned.end()));
    CHECK(decoded.item.get("child")->as_message()->get("note")->as_text() == "item before");
    CHECK(decoded.metadata.front().get("child")->as_message()->get("note")->as_text() == "metadata before");
    decoded.metadata.front().get("child")->as_message()->set("note", loom::Cell::text("receiver edit"));
    r.get();
    CHECK(r.caller->states.back().pair == pair);
}

TEST_CASE("weave: malformed inner item or metadata refuses the whole replacement") {
    Rig r;
    const auto good = as_bytes(inv::encode_pair(make_sample(2, "kept", {}),
                                               {make_sample(3, "kept metadata", {})}));
    r.set(good);
    for (const bool damage_metadata : {false, true}) {
        const auto admitted = loom::admit(loom::parse(std::string(good.begin(), good.end())),
                                           inv::pair_schema());
        REQUIRE(admitted);
        auto envelope = admitted.value();
        // Valid native serialization, missing every required inner field.
        const auto incomplete = as_bytes(loom::serialize(loom::Value(sample_schema())));
        if (damage_metadata) {
            envelope.set("metadata", loom::Cell::list({loom::Cell::bytes(incomplete)}));
        } else {
            envelope.set("item", loom::Cell::bytes(incomplete));
        }
        r.set(as_bytes(loom::serialize(envelope)));
        r.get();
        CHECK(r.caller->states.back().pair == good);
    }
    CHECK(r.caller->refusals.size() == 2);
    CHECK(r.caller->acks == 1);
}

TEST_CASE("weave: the capture answer keeps its own pair after another writer replaces the slot") {
    Rig r;
    r.capture(kTargetRole);
    REQUIRE(r.caller->captures.size() == 1);
    const auto captured = r.caller->captures.front().pair;
    r.set(as_bytes(inv::encode_pair(make_sample(99, "later write", {}), {})));
    r.get();
    CHECK(r.caller->states.back().pair != captured);
    CHECK(inv::decode_pair(std::string(captured.begin(), captured.end())).item.schema().name() == "zen.PokeStructure");
}

TEST_CASE("weave: inventory loads unloads and loads fresh through the ordinary kernel") {
    Rig r;
    r.bus.unregister_weave(r.inventory_id).reset();
    loom::Kernel kernel(r.bus, zengine::workshop::artifact_admission());
    auto loaded = kernel.load("inventory", INVENTORY_SO, inv::kInventoryRole);
    REQUIRE_MESSAGE(loaded.ok, loaded.error);
    r.get();
    CHECK_FALSE(r.caller->states.back().occupied);
    const auto denied = r.bus.send_as(loaded.id, r.caller_id, loom::Message(loom::to_value(Pump{})));
    r.bus.drain_until_idle();
    CHECK(r.bus.outcome(denied).refusal.reason == loom::RefusalReason::CapabilityDenied);
    // The source and the inventory are both the loaded image: exercises substrate
    // answer provenance, dispatch tickets and deferred answers across the ABI.
    r.capture(inv::kInventoryRole);
    REQUIRE(r.caller->captures.size() == 1);
    r.capture("inventorytest.unheld");
    REQUIRE(r.caller->refusals.size() == 1);
    REQUIRE(kernel.unload_role(inv::kInventoryRole));
    loaded = kernel.load("inventory-again", INVENTORY_SO, inv::kInventoryRole);
    REQUIRE_MESSAGE(loaded.ok, loaded.error);
    r.get();
    CHECK_FALSE(r.caller->states.back().occupied);
    CHECK_FALSE(r.caller->captures.front().pair.empty());
}

TEST_CASE("weave: a saved capture survives removal of its actual source") {
    Rig r;
    r.capture(kTargetRole);
    REQUIRE(r.caller->captures.size() == 1);
    const auto captured = r.caller->captures.front().pair;
    r.bus.unregister_weave(r.target_id).reset();
    r.get();
    CHECK(r.caller->states.back().pair == captured);
    const auto decoded = inv::decode_pair(std::string(captured.begin(), captured.end()));
    CHECK(decoded.item.get("state_schema")->as_text() == "SampleTargetState");
    CHECK(decoded.metadata[1].get("request")->as_message()->get("target_role")->as_text() == kTargetRole);
}

TEST_SUITE_END();
