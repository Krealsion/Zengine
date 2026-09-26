// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The inventory suite: the codec (inventory/codec.hpp, the byte envelope) and the weave
// (inventory/weave.hpp) mounted directly, with a trusted grant, on an ordinary bus -- the
// compatibility slot, the named collection and its folders, capture and toolbox files, and a
// runtime-defined item schema stored and read back with no change here. What a guest reaches
// through its own grant is test_workshop_guests.cpp's; the live journey from an external Loom
// host is external-host/tools/workshop/inventory_capture.py, not repeated here.

#include "doctest.h"

#include "inventory/codec.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory/weave.hpp"
#include "inventory-pane/toolbox_file.hpp"
#include "message-draft/transfer.hpp"
#include "workshop/admission.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
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
                                          inv::InventoryCaptured, inv::InventoryListed, inv::v2::InventoryListed,
                                          inv::InventoryFolderState, inv::InventorySnapshot, inv::v2::InventorySnapshot,
                                          inv::InventoryRestored, loom::Ack, loom::Refused>,
                             loom::Emit<inv::InventorySet, inv::InventoryGet,
                                        inv::InventoryCaptureDescribe, inv::InventoryLocate,
                                        inv::InventoryRead, inv::InventoryWrite, inv::InventoryList, inv::v2::InventoryList,
                                        inv::InventoryAdd, inv::v2::InventoryAdd, inv::InventoryRename, inv::InventoryRemove,
                                        inv::InventoryFile, inv::InventoryFolderCreate, inv::InventoryFolderRename,
                                        inv::InventoryFolderMove, inv::InventoryFolderRemove,
                                        inv::InventoryCaptureAdd, inv::InventorySnapshotRequested, inv::InventoryRestore,
                                        inv::v2::InventorySnapshotRequested, inv::v2::InventoryRestore>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<inv::InventoryState> states;
    std::vector<inv::InventoryCaptured> captures;
    std::vector<inv::InventoryEntry> entries;
    std::vector<inv::InventoryListed> lists;
    std::vector<inv::InventorySnapshot> snapshots;
    std::vector<inv::v2::InventorySnapshot> organized;
    std::vector<inv::InventoryRestored> restored;
    std::vector<inv::v2::InventoryListed> listings;
    std::vector<inv::InventoryFolderState> folders;
    void on(const inv::InventorySnapshot& v, loom::Mail& mail) { CHECK(mail.answers_ask()); snapshots.push_back(v); }
    void on(const inv::v2::InventorySnapshot& v, loom::Mail& mail) { CHECK(mail.answers_ask()); organized.push_back(v); }
    void on(const inv::v2::InventoryListed& v, loom::Mail& mail) { CHECK(mail.answers_ask()); listings.push_back(v); }
    void on(const inv::InventoryFolderState& v, loom::Mail& mail) { CHECK(mail.answers_ask()); folders.push_back(v); }
    void on(const inv::InventoryRestored& v, loom::Mail& mail) { CHECK(mail.answers_ask()); restored.push_back(v); }
    void on(const inv::InventoryListed& list, loom::Mail& mail) {
        CHECK(mail.answers_ask()); lists.push_back(list);
    }
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



TEST_CASE("collection: appended entries coexist with legacy slot replacement and own independent values") {
    Rig r;
    const auto one = as_bytes(inv::encode_pair(make_sample(1, "first", {}), {}));
    const auto two = as_bytes(inv::encode_pair(make_sample(2, "second", {}), {}));
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryAdd{one, "first"}); });
    const auto first = r.caller->entries.back();
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryAdd{two, "second"}); });
    const auto second = r.caller->entries.back();
    CHECK(first.reference.entry != second.reference.entry);
    r.set(one); r.set(two);
    r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryList{}); });
    REQUIRE(r.caller->lists.back().entries.size() == 3);
    CHECK(r.caller->lists.back().entries[0].capture_slot);
    CHECK(r.caller->lists.back().entries[1].label == "first");
    CHECK(r.caller->lists.back().entries[2].schema == "inventorytest.Sample");
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRead{first.reference}); });
    CHECK(r.caller->entries.back().pair == one);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryWrite{first.reference, first.revision, two}); });
    CHECK(r.caller->entries.back().reference.entry == first.reference.entry);
    CHECK(r.caller->entries.back().revision == 2);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRead{second.reference}); });
    CHECK(r.caller->entries.back().revision == 1);
    CHECK(r.caller->entries.back().pair == two);
}

TEST_CASE("collection: rename and removal check identity and revision without shifting a reference") {
    Rig r;
    const auto pair = as_bytes(inv::encode_pair(make_sample(7, "data", {}), {}));
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryAdd{pair, "old"}); });
    const auto first = r.caller->entries.back();
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryAdd{pair, "neighbour"}); });
    const auto second = r.caller->entries.back();
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryRename{first.reference, first.revision, "renamed"}); });
    CHECK(r.caller->entries.back().revision == 2);
    CHECK(r.caller->entries.back().pair == pair);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryRemove{first.reference, first.revision}); });
    REQUIRE(r.caller->refusals.size() == 1);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRemove{first.reference, 2}); });
    CHECK(r.caller->acks == 1);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRead{first.reference}); });
    REQUIRE(r.caller->refusals.size() == 2);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRead{second.reference}); });
    CHECK(r.caller->entries.back().pair == pair);
    CHECK(r.caller->entries.back().revision == 1);
    r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryList{}); });
    REQUIRE(r.caller->lists.back().entries.size() == 1);
    CHECK(r.caller->lists.back().entries[0].label == "neighbour");
}

TEST_CASE("collection: append capture returns its own reference with typed request metadata") {
    Rig r;
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryCaptureAdd{kTargetRole, "target structure"}); });
    REQUIRE(r.caller->entries.size() == 1);
    const auto first = r.caller->entries.back();
    const auto decoded = inv::decode_pair({reinterpret_cast<const char*>(first.pair.data()), first.pair.size()});
    REQUIRE(decoded.metadata.size() == 2);
    CHECK(decoded.metadata[1].schema().name() == "CaptureAddRequest");
    CHECK(decoded.metadata[1].get("request")->as_message()->get("label")->as_text() == "target structure");
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryCaptureAdd{kTargetRole, "another"}); });
    CHECK(r.caller->entries.back().reference.entry != first.reference.entry);
    r.get(); CHECK_FALSE(r.caller->states.back().occupied);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRead{first.reference}); });
    CHECK(r.caller->entries.back().pair == first.pair);
}

TEST_CASE("collection: malformed additions and capacity refusal preserve all saved entries") {
    Rig r;
    const auto pair = as_bytes(inv::encode_pair(make_sample(7, "data", {}), {}));
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryAdd{{1,2}, "broken"}); });
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryAdd{pair, "bad\nname"}); });
    REQUIRE(r.caller->refusals.size() == 2);
    for (int i = 0; i < 256; ++i)
        r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryAdd{pair, std::to_string(i)}); });
    REQUIRE(r.caller->entries.size() == 256);
    const auto first = r.caller->entries.front();
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryAdd{pair, "overflow"}); });
    REQUIRE(r.caller->refusals.size() == 3);
    r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryList{}); });
    CHECK(r.caller->lists.back().entries.size() == 256);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRead{first.reference}); });
    CHECK(r.caller->entries.back().pair == first.pair);
}

TEST_CASE("toolbox restore is a conditional whole-collection replacement with fresh live references") {
    Rig r;
    const auto original = as_bytes(inv::encode_pair(make_sample(7, "retained", {"nested"}), {}));
    r.set(original);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryAdd{original, "Named copy"}); });
    const auto held = r.caller->entries.back();
    const auto snapshot = [&] {
        r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventorySnapshotRequested{}); });
        return r.caller->snapshots.back();
    };
    const auto saved = snapshot();
    REQUIRE(saved.archive.entries.size() == 2);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryRename{held.reference, held.revision, "Concurrent writer"}); });
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryRestore{saved.owner, saved.revision, saved.archive, true}); });
    REQUIRE(r.caller->refusals.size() == 1);
    CHECK(r.caller->restored.empty());
    auto now = snapshot();
    CHECK(now.archive.entries.back().label == "Concurrent writer");
    const auto unchanged = loom::serialize(loom::to_value(now));
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryRestore{now.owner, now.revision, saved.archive, false}); });
    CHECK(r.caller->refusals.size() == 2);
    CHECK(loom::serialize(loom::to_value(snapshot())) == unchanged);
    auto bad = saved.archive; bad.entries.back().pair = as_bytes("broken");
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryRestore{now.owner, now.revision, bad, true}); });
    CHECK(r.caller->refusals.size() == 3);
    CHECK(loom::serialize(loom::to_value(snapshot())) == unchanged);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryRestore{now.owner, now.revision, saved.archive, true}); });
    REQUIRE(r.caller->restored.size() == 1);
    now = snapshot();
    CHECK(now.owner != saved.owner);
    CHECK(now.archive.entries.back().label == "Named copy");
    CHECK(now.archive.entries.front().pair == original);
    CHECK(now.archive.entries.back().key == held.reference.entry);
    r.act([&](loom::Mail& m) {
        m.send_to_role(inv::kInventoryRole, inv::InventoryRead{held.reference});
        m.send_to_role(inv::kInventoryRole, inv::InventoryWrite{held.reference, held.revision, original});
    });
    CHECK(r.caller->refusals.size() == 5);
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryRead{{now.owner, held.reference.entry}}); });
    CHECK(r.caller->entries.back().pair == original);
    CHECK(r.caller->entries.back().revision == 1);
}

TEST_CASE("toolbox files round trip nested typed data partial commands and inactive configuration") {
    namespace slots = zengine::inventory_pane;
    namespace draft = zengine::message_draft;
    const auto root = std::filesystem::temp_directory_path() /
        ("zengine-toolbox-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{root};
    const auto path = (root / "saved.toolbox").string();
    loom::Value metadata(note_schema()); metadata.set("who", loom::Cell::text("old source"));
    draft::Draft partial(sample_schema()); partial.set({std::string("count")}, loom::Cell::integer(9));
    const std::string a(32, 'a'), b(32, 'b');
    slots::v2::InventoryToolbox file;
    file.archive.entries = {
        {a, "Nested", as_bytes(inv::encode_pair(make_sample(17, "child", {"x", "y"}), {metadata})), false, {}},
        {b, "Partial command", as_bytes(inv::encode_pair(draft::store_draft("Incomplete", partial), {})), true, {}}};
    file.views = {{"inventory.1", "row", {a, b}}};
    file.bindings = {{a, "source.current.office", zengine::input::scan::kA, zengine::input::mod::kAlt}};
    slots::write_toolbox(path, file);
    const auto read = slots::read_toolbox(path);
    CHECK(loom::serialize(loom::to_value(read)) == loom::serialize(loom::to_value(file)));
    const auto pair = inv::decode_pair({reinterpret_cast<const char*>(read.archive.entries[0].pair.data()), read.archive.entries[0].pair.size()});
    CHECK(pair.item.get("child")->as_message()->get("note")->as_text() == "child");
    CHECK(pair.metadata[0].get("who")->as_text() == "old source");
    const auto command = inv::decode_pair({reinterpret_cast<const char*>(read.archive.entries[1].pair.data()), read.archive.entries[1].pair.size()});
    const auto restored = draft::read_draft(command.item);
    CHECK_FALSE(restored.draft.admit());
    CHECK(restored.draft.value().get("count")->as_int() == 9);
    slots::InventoryViews previous;
    previous.serial = 3; previous.inventory_active = true;
    previous.views = {{"inventory.1", "single", true, {{"obsolete", a}}}, {"inventory.2", "column", true, {}}};
    auto layout = slots::toolbox_layout(read, previous);
    slots::bind_toolbox_owner(layout, "new-owner");
    CHECK_FALSE(layout.inventory_active);
    REQUIRE(layout.views.size() == 2);
    CHECK(layout.views.front().id == "inventory.1");
    CHECK(layout.views.front().entries.front().owner == "new-owner");
    CHECK(layout.views.back().entries.empty());
    CHECK_FALSE(layout.views.back().active);
    REQUIRE(layout.bindings.size() == 1);
    CHECK_FALSE(layout.bindings.front().enabled);
    CHECK(layout.bindings.front().target == "source.current.office");
    CHECK(slots::shortcuts(layout).empty());
    for (int i = 0; i < 30; ++i) layout = slots::toolbox_layout(read, layout);
    CHECK(layout.views.size() == 2);
    auto named = read; named.views.front().id = "inventory.9";
    CHECK(slots::toolbox_layout(named, {}).views.front().id == "inventory.9");
    slots::InventoryViews full;
    for (int i = 10; i < 22; ++i) full.views.push_back({"inventory." + std::to_string(i), "row", false, {}});
    CHECK_THROWS(slots::toolbox_layout(read, full));
    // Ordinary overwrite works on Windows as well as Linux, and invalid candidates do not write.
    file.archive.entries[0].label = "Updated"; slots::write_toolbox(path, file);
    auto broken = file; broken.views.front().entries.push_back(a);
    CHECK_THROWS(slots::write_toolbox(path, broken));
    CHECK(slots::read_toolbox(path).archive.entries[0].label == "Updated");
    std::filesystem::create_directory(path + ".saving");
    CHECK_THROWS(slots::write_toolbox(path, read));
    CHECK(slots::read_toolbox(path).archive.entries[0].label == "Updated");
    { std::ofstream out(root / "corrupt.toolbox"); out << "bad"; }
    CHECK_THROWS(slots::read_toolbox((root / "corrupt.toolbox").string()));
    { std::ofstream out(root / "large.toolbox", std::ios::binary); out.seekp(slots::kMaxToolboxFileBytes); out.put('x'); }
    CHECK_THROWS(slots::read_toolbox((root / "large.toolbox").string()));
}

TEST_CASE("toolbox validation rejects duplicate keys incompatible versions and unresolved configuration") {
    namespace slots = zengine::inventory_pane;
    const auto pair = as_bytes(inv::encode_pair(make_sample(1, "data", {}), {}));
    const std::string a(32, 'a'), b(32, 'b');
    inv::InventoryArchive archive{{{a, "one", pair, false}, {b, "two", pair, false}}};
    auto invalid = archive; invalid.entries[1].key = a;
    CHECK_THROWS(inv::validate_archive(invalid));
    invalid = archive; invalid.entries[0].capture_slot = true; invalid.entries[1].capture_slot = true;
    CHECK_THROWS(inv::validate_archive(invalid));
    invalid = archive; invalid.entries[0].pair.resize(inv::kMaxArchivePairBytes + 1);
    CHECK_THROWS(inv::validate_archive(invalid));
    slots::v2::InventoryToolbox file{inv::organized(archive), {{"inventory.1", "row", {a}}}, {}};
    file.views.front().entries.push_back("absent"); CHECK_THROWS(slots::validate_toolbox(file));
    slots::InventoryViews layout; layout.views = {{"inventory.1", "row", false, {{"old-owner", a}}}};
    CHECK_THROWS(slots::toolbox_snapshot({"current-owner", 1, inv::organized(archive)}, layout));
    // An incompatible envelope version never enters a file schema it does not claim.
    const auto other = loom::SchemaBuilder("InventoryToolbox", 3).build();
    CHECK_FALSE(loom::admit(loom::parse(loom::serialize(loom::Value(other))), loom::schema_of<slots::v2::InventoryToolbox>()));
    CHECK_FALSE(loom::admit(loom::parse(loom::serialize(loom::Value(other))), loom::schema_of<slots::InventoryToolbox>()));
}

namespace {
/// Folder requests through the suite's caller, and the whole organized collection as bytes: a
/// refusal is proven to change nothing by comparing those bytes before and after.
struct Organizer {
    Rig& r;
    inv::v2::InventoryListed list() {
        r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::v2::InventoryList{}); });
        return r.caller->listings.back();
    }
    std::string owner() { return list().owner; }
    std::string bytes() {
        r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::v2::InventorySnapshotRequested{}); });
        return loom::serialize(loom::to_value(r.caller->organized.back()));
    }
    template <class Request> bool ask(Request request) {
        const auto refusals = r.caller->refusals.size();
        r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, request); });
        return r.caller->refusals.size() == refusals;
    }
    /// A request the owner must refuse, leaving every byte of the collection as it was.
    template <class Request> std::string refused(Request request) {
        const auto before = bytes();
        const auto refusals = r.caller->refusals.size();
        r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, request); });
        REQUIRE(r.caller->refusals.size() == refusals + 1);
        CHECK(bytes() == before);
        return r.caller->refusals.back();
    }
    inv::InventoryFolderState create(const std::string& parent, const std::string& name) {
        REQUIRE(ask(inv::InventoryFolderCreate{{owner(), parent}, name}));
        return r.caller->folders.back();
    }
    inv::InventoryFolderState state(const std::string& id) {
        for (const auto& f : list().folders) if (f.folder.folder == id) return f;
        FAIL("no folder " << id); return {};
    }
    std::string folder_of(const inv::InventoryReference& ref) {
        for (const auto& e : list().entries) if (e.reference.entry == ref.entry) return e.folder;
        FAIL("no entry"); return {};
    }
};
} // namespace

TEST_CASE("folders: nesting, renames and moves keep identity; names conflict only among siblings") {
    Rig r; Organizer o{r};
    const auto workbench = o.create("", "Workbench");
    const auto samples = o.create(workbench.folder.folder, "Samples");
    const auto commands = o.create(workbench.folder.folder, "Commands");
    const auto drafts = o.create(commands.folder.folder, "Drafts");
    CHECK(drafts.parent == commands.folder.folder);
    CHECK(drafts.revision == 1);
    // The same name under different parents is fine; siblings compare ignoring ASCII case.
    const auto other_drafts = o.create(workbench.folder.folder, "Drafts");
    CHECK(other_drafts.folder.folder != drafts.folder.folder);
    CHECK(o.refused(inv::InventoryFolderCreate{{o.owner(), commands.folder.folder}, "drafts"}).find("already a folder") != std::string::npos);
    const auto mixed = o.create("", "MiXeD Case");
    CHECK(o.state(mixed.folder.folder).name == "MiXeD Case"); // stored as typed
    // Invalid names are refused whole; nothing is trimmed or renamed to fit.
    for (const auto& bad : std::vector<std::string>{"", "   ", "a/b", " lead", "trail ", ".", "..", std::string(65, 'x'), std::string("caf\xc3\xa9")})
        (void)o.refused(inv::InventoryFolderCreate{{o.owner(), ""}, bad});
    // Rename keeps identity and advances the folder's own revision; stale or conflicting renames refuse.
    REQUIRE(o.ask(inv::InventoryFolderRename{drafts.folder, 1, "Presets"}));
    const auto presets = r.caller->folders.back();
    CHECK(presets.folder.folder == drafts.folder.folder);
    CHECK(presets.revision == 2);
    (void)o.refused(inv::InventoryFolderRename{drafts.folder, 1, "Old"});
    (void)o.refused(inv::InventoryFolderRename{other_drafts.folder, 1, "SAMPLES"});
    REQUIRE(o.ask(inv::InventoryFolderRename{other_drafts.folder, 1, "drafts"})); // its own case changes
    // A move carries the subtree and keeps identity; a folder never moves into itself or below.
    const auto deep = o.create(presets.folder.folder, "Deep");
    REQUIRE(o.ask(inv::InventoryFolderMove{commands.folder, 1, {o.owner(), samples.folder.folder}}));
    CHECK(o.state(commands.folder.folder).parent == samples.folder.folder);
    CHECK(o.state(deep.folder.folder).parent == presets.folder.folder);
    CHECK(o.refused(inv::InventoryFolderMove{workbench.folder, 1, {o.owner(), deep.folder.folder}}).find("inside it") != std::string::npos);
    (void)o.refused(inv::InventoryFolderMove{workbench.folder, 1, workbench.folder});
    (void)o.refused(inv::InventoryFolderMove{commands.folder, 1, {o.owner(), ""}}); // stale revision
    // A sibling conflict at the destination refuses the move.
    const auto root_samples = o.create("", "Samples");
    (void)o.refused(inv::InventoryFolderMove{root_samples.folder, 1, workbench.folder});
    // Moving into its own parent changes nothing and says so by answering the unchanged folder.
    const auto before = o.bytes();
    REQUIRE(o.ask(inv::InventoryFolderMove{root_samples.folder, 1, {o.owner(), ""}}));
    CHECK(o.bytes() == before);
    // A reference from another owner, or a missing folder, never lands anywhere.
    (void)o.refused(inv::InventoryFolderCreate{{"another-owner", ""}, "Elsewhere"});
    (void)o.refused(inv::InventoryFolderCreate{{o.owner(), std::string(32, 'f')}, "Nowhere"});
}

TEST_CASE("folders: filing an entry keeps its identity revision and pair; only empty folders can be removed") {
    Rig r; Organizer o{r};
    const auto samples = o.create("", "Samples");
    const auto pair = as_bytes(inv::encode_pair(make_sample(3, "kept", {"a"}), {}));
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryAdd{pair, "At root"}); });
    const auto root_entry = r.caller->entries.back();
    REQUIRE(o.ask(inv::v2::InventoryAdd{pair, "Filed", samples.folder}));
    const auto filed = r.caller->entries.back();
    CHECK(o.folder_of(root_entry.reference).empty());
    CHECK(o.folder_of(filed.reference) == samples.folder.folder);
    // A move names where the entry was seen; organization never touches the entry's revision.
    REQUIRE(o.ask(inv::InventoryFile{root_entry.reference, {o.owner(), ""}, samples.folder}));
    CHECK(r.caller->entries.back().reference.entry == root_entry.reference.entry);
    CHECK(r.caller->entries.back().revision == root_entry.revision);
    CHECK(r.caller->entries.back().pair == pair);
    CHECK(o.folder_of(root_entry.reference) == samples.folder.folder);
    // So a conditional save read before the move still succeeds after it.
    REQUIRE(o.ask(inv::InventoryWrite{root_entry.reference, root_entry.revision, pair}));
    (void)o.refused(inv::InventoryFile{root_entry.reference, {o.owner(), ""}, {o.owner(), ""}}); // stale 'from'
    (void)o.refused(inv::InventoryFile{root_entry.reference, samples.folder, {o.owner(), std::string(32, 'e')}});
    (void)o.refused(inv::v2::InventoryAdd{pair, "Lost", {o.owner(), std::string(32, 'e')}});
    (void)o.refused(inv::v2::InventoryAdd{pair, "Stale", {"an-earlier-owner", ""}});
    // Removal refuses a folder that holds anything and never removes an entry.
    const auto inner = o.create(samples.folder.folder, "Inner");
    const auto why = o.refused(inv::InventoryFolderRemove{samples.folder, 1});
    CHECK(why.find("2 entries and 1 folder") != std::string::npos);
    CHECK(why.find("never removes entries") != std::string::npos);
    (void)o.refused(inv::InventoryFolderRemove{inner.folder, 7}); // stale revision
    REQUIRE(o.ask(inv::InventoryFolderRemove{inner.folder, 1}));
    CHECK(r.caller->acks >= 1);
    // An entry's membership leaves with the entry.
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRemove{filed.reference, filed.revision}); });
    REQUIRE(o.ask(inv::InventoryFile{root_entry.reference, samples.folder, {o.owner(), ""}}));
    REQUIRE(o.ask(inv::InventoryFolderRemove{samples.folder, 1}));
    CHECK(o.list().folders.empty());
    CHECK(o.list().entries.size() == 1);
}

TEST_CASE("folders: a replaced legacy slot is a new entry at the root and inherits no folder") {
    Rig r; Organizer o{r};
    const auto kept = o.create("", "Kept");
    r.set(as_bytes(inv::encode_pair(make_sample(1, "first", {}), {})));
    auto listed = o.list();
    REQUIRE(listed.entries.size() == 1);
    const auto first = listed.entries.front().reference;
    REQUIRE(o.ask(inv::InventoryFile{first, {o.owner(), ""}, kept.folder}));
    CHECK(o.folder_of(first) == kept.folder.folder);
    r.set(as_bytes(inv::encode_pair(make_sample(2, "second", {}), {})));
    listed = o.list();
    REQUIRE(listed.entries.size() == 1);
    CHECK(listed.entries.front().reference.entry != first.entry);
    CHECK(listed.entries.front().capture_slot);
    CHECK(listed.entries.front().folder.empty());
}

TEST_CASE("folders: count and depth bounds refuse without mutation") {
    Rig r; Organizer o{r};
    std::string parent;
    std::vector<inv::InventoryFolderState> chain;
    for (int depth = 1; depth <= 8; ++depth) { chain.push_back(o.create(parent, "Level " + std::to_string(depth))); parent = chain.back().folder.folder; }
    CHECK(o.refused(inv::InventoryFolderCreate{{o.owner(), parent}, "Nine"}).find("eight deep") != std::string::npos);
    // A subtree two levels tall cannot move below the seventh level.
    const auto top = o.create("", "Top");
    (void)o.create(top.folder.folder, "Below");
    (void)o.refused(inv::InventoryFolderMove{top.folder, 1, chain[6].folder});
    REQUIRE(o.ask(inv::InventoryFolderMove{top.folder, 1, chain[5].folder}));
    for (int n = static_cast<int>(o.list().folders.size()); n < 128; ++n) (void)o.create("", "F" + std::to_string(n));
    CHECK(o.list().folders.size() == 128);
    CHECK(o.refused(inv::InventoryFolderCreate{{o.owner(), ""}, "One more"}).find("128") != std::string::npos);
}

TEST_CASE("folders: organized snapshots restore whole, flat snapshots refuse, and malformed candidates keep the collection") {
    Rig r; Organizer o{r};
    const auto pair = as_bytes(inv::encode_pair(make_sample(5, "saved", {"t"}), {}));
    const auto outer = o.create("", "Outer");
    const auto inner = o.create(outer.folder.folder, "Inner");
    REQUIRE(o.ask(inv::v2::InventoryAdd{pair, "Inside", inner.folder}));
    const auto inside = r.caller->entries.back();
    (void)o.create("", "Empty");
    // Version 1 cannot say where anything is filed, so it refuses rather than dropping folders.
    const auto refusals = r.caller->refusals.size();
    r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventorySnapshotRequested{}); });
    REQUIRE(r.caller->refusals.size() == refusals + 1);
    CHECK(r.caller->refusals.back().find("version 2") != std::string::npos);
    r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::v2::InventorySnapshotRequested{}); });
    const auto saved = r.caller->organized.back();
    REQUIRE(saved.archive.folders.size() == 3);
    // Every malformed candidate refuses before commit and leaves each byte as it was.
    const auto now = [&] { r.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::v2::InventorySnapshotRequested{}); }); return r.caller->organized.back(); };
    const auto candidate = [&](auto change) { auto a = saved.archive; change(a); const auto n = now(); return inv::v2::InventoryRestore{n.owner, n.revision, a, true}; };
    const auto folder_index = [&](const inv::v2::InventoryArchive& a, const std::string& key) {
        for (std::size_t i = 0; i < a.folders.size(); ++i) if (a.folders[i].key == key) return i;
        FAIL("no folder"); return std::size_t{0};
    };
    (void)o.refused(candidate([&](auto& a) { a.folders[folder_index(a, outer.folder.folder)].parent = inner.folder.folder; })); // cycle
    (void)o.refused(candidate([&](auto& a) { a.folders[folder_index(a, inner.folder.folder)].parent = std::string(32, 'd'); })); // missing parent
    (void)o.refused(candidate([&](auto& a) { a.entries.back().folder = std::string(32, 'd'); })); // missing member folder
    (void)o.refused(candidate([&](auto& a) { a.folders.push_back(a.folders.front()); })); // duplicate identity
    (void)o.refused(candidate([&](auto& a) { a.folders.front().key = a.entries.back().key; })); // shared with an entry
    (void)o.refused(candidate([&](auto& a) { a.folders.front().name = "a/b"; }));
    (void)o.refused(candidate([&](auto& a) { a.folders.push_back({std::string(32, 'c'), "outer", ""}); })); // sibling conflict
    (void)o.refused(candidate([&](auto& a) {
        std::string parent;
        for (int i = 0; i < 9; ++i) { const auto key = std::string(31, 'a') + static_cast<char>('0' + i); a.folders.push_back({key, "D" + std::to_string(i), parent}); parent = key; }
    }));
    (void)o.refused(candidate([&](auto& a) { for (int i = 0; i < 126; ++i) { char k[33]; std::snprintf(k, sizeof k, "%032x", 1000 + i); a.folders.push_back({k, "N" + std::to_string(i), ""}); } }));
    // A writer between preparation and restore wins: the old stamp refuses.
    const auto stale = now();
    (void)o.create("", "Newer");
    CHECK(o.refused(inv::v2::InventoryRestore{stale.owner, stale.revision, saved.archive, true}).find("changed") != std::string::npos);
    // The complete candidate commits: folders keep keys and membership; live identity rotates.
    const auto current = now();
    REQUIRE(o.ask(inv::v2::InventoryRestore{current.owner, current.revision, saved.archive, true}));
    const auto listed = o.list();
    CHECK(listed.owner != saved.owner);
    CHECK(listed.folders.size() == 3);
    for (const auto& f : listed.folders) CHECK(f.revision == 1);
    CHECK(o.folder_of(inside.reference) == inner.folder.folder);
    CHECK(o.state(inner.folder.folder).parent == outer.folder.folder);
    // Old live folder references do not come back to life under the new owner, not even for an
    // addition begun against the old collection, though the restored folder kept its key.
    (void)o.refused(inv::InventoryFolderRename{outer.folder, 1, "Revived"});
    (void)o.refused(inv::v2::InventoryAdd{pair, "Late addition", outer.folder});
    (void)o.refused(inv::v2::InventoryAdd{pair, "Late root addition", {saved.owner, ""}});
    // A flat archive restores with every entry at the root and no folders.
    const auto flat = now();
    inv::InventoryArchive flat_archive{{{std::string(32, '9'), "Flat", pair, false}}};
    r.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRestore{flat.owner, flat.revision, flat_archive, true}); });
    const auto after = o.list();
    CHECK(after.folders.empty());
    REQUIRE(after.entries.size() == 1);
    CHECK(after.entries.front().folder.empty());
}

TEST_CASE("toolbox files: a flat version 1 file reads at the root, version 2 keeps folders, unknown files refuse") {
    namespace slots = zengine::inventory_pane;
    const auto root = std::filesystem::temp_directory_path() / ("zengine-folders-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{root};
    const auto write_raw = [&](const std::string& name, const loom::Value& value) {
        const auto path = (root / name).string(); std::ofstream out(path, std::ios::binary);
        const auto bytes = loom::serialize(value); out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return path;
    };
    const auto pair = as_bytes(inv::encode_pair(make_sample(8, "v1", {}), {}));
    const std::string a(32, 'a'), b(32, 'b'), f(32, 'f'), g(32, 'e');
    // A version 1 file, exactly as that format writes it: no folders.
    slots::InventoryToolbox flat{{{{a, "Old", pair, false}, {b, "Slot", pair, true}}}, {{"inventory.1", "row", {a}}}, {}};
    const auto old = slots::read_toolbox(write_raw("flat.toolbox", loom::to_value(flat)));
    REQUIRE(old.archive.entries.size() == 2);
    CHECK(old.archive.folders.empty());
    for (const auto& e : old.archive.entries) CHECK(e.folder.empty());
    CHECK(old.views.front().entries.front() == a);
    // An organized file round trips its folders, membership and portable configuration.
    slots::v2::InventoryToolbox organized{{{{a, "Filed", pair, false, g}, {b, "Root", pair, false, {}}},
        {{f, "Workbench", ""}, {g, "Samples", f}}}, {{"inventory.1", "single", {a}}}, {}};
    const auto path = (root / "organized.toolbox").string();
    slots::write_toolbox(path, organized);
    CHECK(loom::serialize(loom::to_value(slots::read_toolbox(path))) == loom::serialize(loom::to_value(organized)));
    // Neither an unknown version, another schema, nor a cycle written around the writer is read.
    CHECK_THROWS_WITH(slots::read_toolbox(write_raw("v3.toolbox", loom::Value(loom::SchemaBuilder("InventoryToolbox", 3).build()))),
                      doctest::Contains("version 3"));
    CHECK_THROWS(slots::read_toolbox(write_raw("other.toolbox", loom::to_value(inv::InventoryArchive{}))));
    auto cycle = organized; cycle.archive.folders[0].parent = g;
    CHECK_THROWS(slots::read_toolbox(write_raw("cycle.toolbox", loom::to_value(cycle))));
    CHECK_THROWS(slots::write_toolbox(path, cycle));
    CHECK(loom::serialize(loom::to_value(slots::read_toolbox(path))) == loom::serialize(loom::to_value(organized)));
}

TEST_SUITE_END();
