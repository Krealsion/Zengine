// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The guests suite -- WHO MAY CONNECT TO THIS WORKSHOP, AND WHAT EACH MAY THEN SAY.
//
// THIS FILE OWNS the guest door and the file that becomes its policy (workshop/guests.hpp,
// workshop/guest_door.hpp): a credential admits AS THE ROW, a wrong one is refused in words, an
// "ask" row waits for the host's decision and can act on nothing until it comes, a guest's
// grant reaches exactly its powers, its session on this bus ends with its socket, and the
// inventory the door says is the server's own. The crossing itself is Loom's and is proven
// there (suite `bridge`); what is proven here is the Workshop side of it, over a REAL loopback
// socket, with the real Input weave (over a fake reader) on the same bus.
//
// The Skin is not here: the capture door is the surface suite's, over the shell. What this
// suite pins about capture is only that the `capture` power reaches the Skin's office and
// nothing else -- a grant is a grant, and the shell answers what it answers.

// main() and the framework live in doctest_main.cpp -- the shared one that refuses a run
// selecting zero cases (POP-01).
#include "doctest.h"

#include "workshop/guest_door.hpp"
#include "workshop/guest_seam_vocabulary.hpp"
#include "workshop/guests.hpp"
#include "workshop/pane_carry.hpp"

#include "input/input_weave.hpp"
#include "input/vocabulary.hpp"
#include "inventory/vocabulary.hpp"
#include "surface/skin.hpp"
#include "surface/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/bridge/client.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>
#include <zen/weave/describe.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <variant>
#include <vector>

TEST_SUITE_BEGIN("workshop_guests");

namespace {

namespace input = zengine::input;
namespace inv = zengine::inventory;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace guests = zengine::workshop::guests;

// ---- the file --------------------------------------------------------------------------

struct Scratch {
    std::string path;
    explicit Scratch(const char* name) : path(std::string("zen-guests-test-") + name + ".json") {
        std::remove(path.c_str());
    }
    ~Scratch() { std::remove(path.c_str()); }
    void write(const std::string& text) const {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    }
};

const char* kTwoGuests =
    R"({"listen":"127.0.0.1:0","guests":[)"
    R"({"name":"agent","credential":"open-sesame","may":["input","capture","inspect"]},)"
    R"({"name":"watcher","credential":"later","admit":"ask","may":["inspect"]}]})";

// ---- the rig: a bus with the real Input weave, ears, and the door over a real socket --------

using InputEvent = input::InputEvent;

struct FakeReader {
    std::vector<std::vector<InputEvent>>* feed = nullptr;
    std::vector<InputEvent> poll() {
        if (feed == nullptr || feed->empty()) {
            return {};
        }
        std::vector<InputEvent> batch = std::move(feed->front());
        feed->erase(feed->begin());
        return batch;
    }
};

struct EarsState {
    std::int64_t heard = 0;
    ZEN_SHAPE(EarsState, 1, ZEN_FIELD(heard));
};

class Ears : public loom::WeaveBase<Ears, EarsState,
                                    loom::Accept<input::KeyPressed, input::KeyReleased,
                                                 input::TextEntered, input::PointerMoved,
                                                 input::PointerButton, input::PointerWheel>,
                                    loom::Emit<>> {
public:
    explicit Ears(std::vector<InputEvent>& heard) : heard_(&heard) {}
    void on(const input::KeyPressed& e, loom::Mail&) { note(e); }
    void on(const input::KeyReleased& e, loom::Mail&) { note(e); }
    void on(const input::TextEntered& e, loom::Mail&) { note(e); }
    void on(const input::PointerMoved& e, loom::Mail&) { note(e); }
    void on(const input::PointerButton& e, loom::Mail&) { note(e); }
    void on(const input::PointerWheel& e, loom::Mail&) { note(e); }

private:
    template <class E>
    void note(const E& e) {
        ++state_.heard;
        heard_->push_back(e);
    }
    std::vector<InputEvent>* heard_;
};

/// Every inventory the door published, in order.
class InventoryEars
    : public loom::WeaveBase<InventoryEars, EarsState, loom::Accept<ws::GuestConnections>,
                             loom::Emit<>> {
public:
    explicit InventoryEars(std::vector<ws::GuestConnections>& said) : said_(&said) {}
    void on(const ws::GuestConnections& g, loom::Mail& mail) {
        if (mail.authored_from_role(ws::kGuestsRole)) {
            ++state_.heard;
            said_->push_back(g);
        }
    }

private:
    std::vector<ws::GuestConnections>* said_;
};

struct Rig {
    loom::Switchboard bus;
    std::vector<std::vector<InputEvent>> feed;
    std::vector<InputEvent> heard;
    std::vector<ws::GuestConnections> inventories;
    ws::GuestDoor* door = nullptr;
    loom::WeaveId door_id{};
    loom::WeaveId input_id{};
    std::uint16_t port = 0;

    explicit Rig(const guests::GuestsFile& file) {
        {
            // IN ITS OFFICE: a guest's session is asked of `zengine.input` by role, never by id.
            auto w = std::make_unique<input::InputWeaveT<FakeReader>>(FakeReader{&feed});
            input::InputWeaveT<FakeReader>* raw = w.get();
            loom::Grant grant = loom::emit_default_grant(*raw);
            loom::allow_poke_answers(grant);
            loom::allow_describe_answers(grant); // a guest with `inspect` asks it what it accepts
            input_id = bus.register_weave(std::move(w), std::move(grant),
                                          std::string(input::kInputRole));
            raw->zen_set_self(input_id);
        }
        (void)loom::mount<Ears>(bus, heard);
        (void)loom::mount<InventoryEars>(bus, inventories);
        std::string err;
        REQUIRE(loom::bridge_net_init(&err));
        const loom::socket_t listener = loom::bridge_listen_tcp(0, &err);
        REQUIRE_MESSAGE(listener != loom::kInvalidSocket, err);
        port = loom::bridge_socket_port(listener);
        auto d = std::make_unique<ws::GuestDoor>(bus, listener,
                                                 "127.0.0.1:" + std::to_string(port),
                                                 guests::admission_of(file));
        door = d.get();
        door_id = bus.register_weave(std::move(d), ws::guest_door_grant(),
                                     std::string(ws::kGuestsRole));
        door->zen_set_self(door_id);
    }

    /// ONE BEAT: the Timer's word to the door, exactly as the service would say it, then the
    /// bus drained. The door services the crossing inside it.
    void beat() {
        bus.send(door_id, loom::Message(loom::to_value(zengine::timer::TimerFired{ws::kGuestBeatId})));
        bus.drain_until_idle();
    }

    bool beat_until(const std::function<bool()>& done, int timeout_ms = 3000) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        for (;;) {
            beat();
            if (done()) {
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return done();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

/// A guest: a client on the far side of the socket, in this process, with its events kept.
struct Guest {
    std::unique_ptr<loom::BridgeClient> client;
    std::vector<loom::BridgeEvent> events;

    Guest(std::uint16_t port, const char* name, const char* credential) {
        std::string err;
        const loom::socket_t s = loom::bridge_connect_tcp("127.0.0.1", port, &err);
        REQUIRE_MESSAGE(s != loom::kInvalidSocket, err);
        client = std::make_unique<loom::BridgeClient>(s);
        REQUIRE(client->hello(name, credential));
    }
    void poll() {
        std::vector<loom::BridgeEvent> got;
        client->poll(got);
        for (loom::BridgeEvent& e : got) {
            events.push_back(std::move(e));
        }
    }
    const loom::BridgeEvent* last(loom::BridgeEvent::Kind kind) const {
        const loom::BridgeEvent* found = nullptr;
        for (const loom::BridgeEvent& e : events) {
            if (e.kind == kind) {
                found = &e;
            }
        }
        return found;
    }
    template <class T>
    void ask(const char* far_role, std::uint64_t correlation, const T& msg, bool settle = false) {
        client->send_to_role(far_role, correlation, loom::serialize(loom::to_value(msg)), settle);
        client->flush();
    }
    /// Every word delivered to this guest that admits as `T`, in arrival order, with the index
    /// of the event that carried it.
    template <class T>
    std::vector<std::pair<std::size_t, T>> said_as() const {
        std::vector<std::pair<std::size_t, T>> out;
        for (std::size_t i = 0; i < events.size(); ++i) {
            const loom::BridgeEvent& e = events[i];
            if (e.kind != loom::BridgeEvent::Kind::Delivered) {
                continue;
            }
            loom::Unverified u = loom::parse(e.payload);
            loom::Admission a = loom::admit(u, loom::schema_of<T>());
            if (a.ok()) {
                out.emplace_back(i, loom::from_value<T>(a.value()));
            }
        }
        return out;
    }
    std::size_t index_of(loom::BridgeEvent::Kind kind, std::uint64_t correlation) const {
        for (std::size_t i = 0; i < events.size(); ++i) {
            if (events[i].kind == kind && events[i].correlation == correlation) {
                return i;
            }
        }
        return events.size();
    }
    /// The delivered answer under `correlation`, admitted against `schema`, or nullopt.
    std::optional<loom::Value> answered_as(std::uint64_t correlation,
                                           const std::shared_ptr<const loom::Schema>& schema) const {
        for (const loom::BridgeEvent& e : events) {
            if (e.kind != loom::BridgeEvent::Kind::Delivered || e.correlation != correlation) {
                continue;
            }
            loom::Unverified u = loom::parse(e.payload);
            loom::Admission a = loom::admit(u, schema);
            if (a.ok()) {
                return std::move(a).value();
            }
        }
        return std::nullopt;
    }
    /// The delivered answer under `correlation`, admitted as `T`, or nullopt.
    template <class T>
    std::optional<T> answered(std::uint64_t correlation) const {
        for (const loom::BridgeEvent& e : events) {
            if (e.kind != loom::BridgeEvent::Kind::Delivered || e.correlation != correlation) {
                continue;
            }
            loom::Unverified u = loom::parse(e.payload);
            loom::Admission a = loom::admit(u, loom::schema_of<T>());
            if (a.ok()) {
                return loom::from_value<T>(a.value());
            }
        }
        return std::nullopt;
    }
};

template <class T>
const T& as(const std::vector<InputEvent>& v, std::size_t i) {
    return std::get<T>(v.at(i));
}

} // namespace

// =============================================================================
// The file
// =============================================================================

TEST_CASE("guests file: rows read back whole, and the refusals name the row and the word") {
    Scratch f("two");
    f.write(kTwoGuests);
    guests::GuestsFile file;
    std::string why;
    REQUIRE_MESSAGE(guests::read_guests_file(f.path, &file, &why), why);
    CHECK(file.listen == "127.0.0.1:0");
    REQUIRE(file.rows.size() == 2);
    CHECK(file.rows[0].name == "agent");
    CHECK(file.rows[0].credential == "open-sesame");
    CHECK(file.rows[0].may.size() == 3);
    CHECK_FALSE(file.rows[0].ask);
    CHECK(file.rows[1].ask);

    SUBCASE("a missing file is an error, not an empty policy") {
        guests::GuestsFile none;
        CHECK_FALSE(guests::read_guests_file("zen-guests-test-nowhere.json", &none, &why));
        CHECK(why.find("cannot be read") != std::string::npos);
    }
    SUBCASE("an unknown power") {
        f.write(R"({"guests":[{"name":"a","credential":"x","may":["load"]}]})");
        CHECK_FALSE(guests::read_guests_file(f.path, &file, &why));
        CHECK(why.find("'load'") != std::string::npos);
    }
    SUBCASE("an empty credential admits nobody, so it is refused up front") {
        f.write(R"({"guests":[{"name":"a","credential":""}]})");
        CHECK_FALSE(guests::read_guests_file(f.path, &file, &why));
        CHECK(why.find("no credential") != std::string::npos);
    }
    SUBCASE("two rows sharing a credential could not be told apart") {
        f.write(R"({"guests":[{"name":"a","credential":"x"},{"name":"b","credential":"x"}]})");
        CHECK_FALSE(guests::read_guests_file(f.path, &file, &why));
        CHECK(why.find("share a credential") != std::string::npos);
    }
    SUBCASE("a listener off loopback is refused by name") {
        f.write(R"({"listen":"0.0.0.0:7654","guests":[{"name":"a","credential":"x"}]})");
        CHECK_FALSE(guests::read_guests_file(f.path, &file, &why));
        CHECK(why.find("loopback") != std::string::npos);
    }
    SUBCASE("a word for admit that is neither now nor ask") {
        f.write(R"({"guests":[{"name":"a","credential":"x","admit":"maybe"}]})");
        CHECK_FALSE(guests::read_guests_file(f.path, &file, &why));
        CHECK(why.find("'maybe'") != std::string::npos);
    }
}

TEST_CASE("guests file: an observe list reads back entry by entry, and an entry naming too little is refused") {
    Scratch f("observe");
    f.write(R"({"guests":[{"name":"agent","credential":"x","may":["input"],"observe":[)"
            R"({"producer":"zengine.builder","shape":"BuildStatus","version":"4"},)"
            R"({"producer":"td.game","shape":"TdOccurred","version":"1"}]}]})");
    guests::GuestsFile file;
    std::string why;
    REQUIRE_MESSAGE(guests::read_guests_file(f.path, &file, &why), why);
    REQUIRE(file.rows[0].observe.size() == 2);
    CHECK(file.rows[0].observe[0].producer == "zengine.builder");
    CHECK(file.rows[0].observe[0].shape == "BuildStatus");
    CHECK(file.rows[0].observe[0].version == 4);
    CHECK(file.rows[0].observe[1].producer == "td.game");
    CHECK(file.rows[0].observe[1].version == 1);
    SUBCASE("an entry with no producer office") {
        f.write(R"({"guests":[{"name":"a","credential":"x","observe":[)"
                R"({"producer":"","shape":"BuildStatus","version":"4"}]}]})");
        CHECK_FALSE(guests::read_guests_file(f.path, &file, &why));
        CHECK(why.find("guest 'a' observes an entry without a producer office") != std::string::npos);
    }
    SUBCASE("version 0 names no shape") {
        f.write(R"({"guests":[{"name":"a","credential":"x","observe":[)"
                R"({"producer":"td.game","shape":"TdSeen","version":"0"}]}]})");
        CHECK_FALSE(guests::read_guests_file(f.path, &file, &why));
    }
    SUBCASE("an Int is the file's base-10 string, never a JSON number") {
        f.write(R"({"guests":[{"name":"a","credential":"x","observe":[)"
                R"({"producer":"td.game","shape":"TdSeen","version":1}]}]})");
        CHECK_FALSE(guests::read_guests_file(f.path, &file, &why));
    }
}

TEST_CASE("observation: a row's observe list is the whole of what its session may observe, and no power implies it") {
    guests::GuestRow agent;
    agent.name = "agent";
    agent.may = {guests::kPowerInput, guests::kPowerCapture, guests::kPowerInspect};
    agent.observe = {{"zengine.builder", "BuildStatus", 4}, {"zengine.builder", "BuildAsked", 1}};
    guests::GuestRow hands = agent;
    hands.name = "hands";
    hands.observe.clear();
    guests::GuestsFile file;
    file.rows = {agent, hands};
    const loom::WeaveId a{101}, h{102}, local{103};
    const auto policy = guests::observation_of(file, [&](loom::WeaveId s) {
        return s == a ? std::string("agent") : s == h ? std::string("hands") : std::string();
    });
    using Shapes = std::vector<loom::observe::ShapeRef>;
    const auto ask = [&](loom::WeaveId who, const char* producer, const Shapes& shapes) {
        return policy(loom::observe::ObserveRequest{who, producer, shapes, "suite"});
    };
    CHECK(ask(a, "zengine.builder", {{"BuildStatus", 4}}).allowed);
    CHECK(ask(a, "zengine.builder", {{"BuildStatus", 4}, {"BuildAsked", 1}}).allowed);
    const auto more = ask(a, "zengine.builder", {{"BuildStatus", 4}, {"RecipeCatalog", 1}});
    CHECK_FALSE(more.allowed); // one shape too many refuses the whole ask
    CHECK(more.reason.find("may not observe RecipeCatalog v1 from zengine.builder") != std::string::npos);
    CHECK_FALSE(ask(a, "zengine.builder", {{"BuildStatus", 3}}).allowed); // another version
    CHECK_FALSE(ask(a, "td.game", {{"BuildStatus", 4}}).allowed);         // another office
    const auto powers = ask(h, "zengine.builder", {{"BuildStatus", 4}});
    CHECK_FALSE(powers.allowed); // input, capture and inspect are not observation
    CHECK(powers.reason.find("guest 'hands'") != std::string::npos);
    const auto stranger = ask(local, "zengine.builder", {{"BuildStatus", 4}});
    CHECK_FALSE(stranger.allowed); // a participant no row admitted -- a local weave -- is refused
    CHECK(stranger.reason.find("the asker is not one") != std::string::npos);
    // ASKING THE RELAY is a listed row's grant; observing grants nothing to say.
    const loom::Grant ga = guests::grant_for(agent);
    const loom::Grant gh = guests::grant_for(hands);
    CHECK(ga.permits_role(loom::observe::Subscribe::zen_name, 1, loom::observe::kObserveRole));
    CHECK(ga.permits_role(loom::observe::Release::zen_name, 1, loom::observe::kObserveRole));
    CHECK_FALSE(gh.permits_role(loom::observe::Subscribe::zen_name, 1, loom::observe::kObserveRole));
    CHECK_FALSE(ga.permits_role("BuildRequested", 1, "zengine.builder"));
}

TEST_CASE("observation: seeing the Builder's whole picture brings one read of the current one, and no act") {
    guests::GuestRow watcher;
    watcher.name = "watcher"; // no power at all: observation alone
    watcher.observe = {{"zengine.builder", "BuildStatus", 4}};
    const loom::Grant g = guests::grant_for(watcher);
    // THE BASELINE a returning observer joins, asked of the Builder's office alone...
    CHECK(g.permits_role("BuildStatusRequested", 1, "zengine.builder"));
    CHECK_FALSE(g.permits_role("BuildStatusRequested", 1, "zengine.builder-pane"));
    // ...and nothing that acts, or republishes to everybody, or reads anything else.
    CHECK_FALSE(g.permits_role("BuildRequested", 2, "zengine.builder"));
    CHECK_FALSE(g.permits_role("StatusRequested", 1, "zengine.builder"));
    CHECK_FALSE(g.permits_role("BuildOutputRequested", 1, "zengine.builder"));
    CHECK_FALSE(g.permits_role(input::InjectInput::zen_name, 1, input::kInputRole));
    // A row that sees only the Builder's asks, or nothing of it, may not ask.
    guests::GuestRow asks_only = watcher;
    asks_only.observe = {{"zengine.builder", "BuildAsked", 1},
                         {"zengine.realization", "RealizationAsked", 1}};
    CHECK_FALSE(guests::grant_for(asks_only).permits_role("BuildStatusRequested", 1, "zengine.builder"));
    guests::GuestRow blind = watcher;
    blind.observe.clear();
    blind.may = {guests::kPowerInput, guests::kPowerCapture, guests::kPowerInspect};
    CHECK_FALSE(guests::grant_for(blind).permits_role("BuildStatusRequested", 1, "zengine.builder"));
    // Observing the realization owner lets a guest ask the relay, and still says nothing to it.
    CHECK_FALSE(guests::grant_for(asks_only).permits_role("PromoteArtifact", 1, "zengine.realization"));
    CHECK_FALSE(guests::grant_for(asks_only).permits_role("RevertArtifact", 1, "zengine.realization"));
}

TEST_CASE("toolbox file access is an explicit power separate from inventory input and execution") {
    guests::GuestRow row;
    row.may = {guests::kPowerInput, guests::kPowerInventory, guests::kPowerInspect};
    const auto ordinary = guests::grant_for(row);
    CHECK_FALSE(ordinary.permits_role("InventoryToolboxSave", 1, "zengine.inventory-pane"));
    CHECK_FALSE(ordinary.permits_role("InventoryToolboxRestore", 1, "zengine.inventory-pane"));
    row.may = {guests::kPowerToolbox};
    const auto files = guests::grant_for(row);
    CHECK(files.permits_role("InventoryToolboxSave", 1, "zengine.inventory-pane"));
    CHECK(files.permits_role("InventoryToolboxRestore", 1, "zengine.inventory-pane"));
    CHECK_FALSE(files.permits_role("InventoryToolboxRestore", 1, "elsewhere"));
    CHECK_FALSE(files.permits_role("InventoryRestore", 1, inv::kInventoryRole));
    CHECK_FALSE(files.permits_role("InventoryViewEdit", 1, "zengine.inventory-pane"));
    CHECK_FALSE(files.permits_role("InjectInput", 1, input::kInputRole));
    CHECK_FALSE(files.permits_role("SurfaceText", 1, surface::kSkinRole));
    // Organizing folders is inventory authority; a whole-collection restore is not, in either version.
    for (const char* shape : {"InventoryFile", "InventoryFolderCreate", "InventoryFolderRename",
                              "InventoryFolderMove", "InventoryFolderRemove"}) {
        CHECK_MESSAGE(ordinary.permits_role(shape, 1, inv::kInventoryRole), shape);
        CHECK_MESSAGE(!files.permits_role(shape, 1, inv::kInventoryRole), shape);
    }
    CHECK(ordinary.permits_role("InventoryAdd", 2, inv::kInventoryRole));
    CHECK(ordinary.permits_role("InventoryList", 2, inv::kInventoryRole));
    for (const auto& may : {ordinary, files}) {
        CHECK_FALSE(may.permits_role("InventoryRestore", 2, inv::kInventoryRole));
        CHECK_FALSE(may.permits_role("InventorySnapshotRequested", 2, inv::kInventoryRole));
    }
}

TEST_CASE("guests file: each power is exactly its grant, and a row with none may say nothing") {
    guests::GuestRow row;
    row.name = "agent";
    row.credential = "x";
    const loom::Grant none = guests::grant_for(row);
    CHECK_FALSE(none.permits_role(input::InputSessionRequested::zen_name, 1, input::kInputRole));

    row.may = {guests::kPowerInput};
    const loom::Grant in = guests::grant_for(row);
    CHECK(in.permits_role(input::InputSessionRequested::zen_name, 1, input::kInputRole));
    CHECK(in.permits_role(input::InjectInput::zen_name, 1, input::kInputRole));
    CHECK(in.permits_role(input::InputSessionClosed::zen_name, 1, input::kInputRole));
    CHECK_FALSE(in.permits_role(input::InjectInput::zen_name, 1, surface::kSkinRole));
    CHECK_FALSE(in.permits_role(surface::SurfaceCaptureRequested::zen_name, 1, surface::kSkinRole));
    CHECK_FALSE(in.permits(loom::DescribeAccepted::zen_name, 1, loom::WeaveId{7}));

    row.may = {guests::kPowerCapture};
    const loom::Grant cap = guests::grant_for(row);
    CHECK(cap.permits_role(surface::SurfaceCaptureRequested::zen_name, 1, surface::kSkinRole));
    CHECK(cap.permits_role(surface::SurfaceCaptureChunkRequested::zen_name, 1, surface::kSkinRole));
    CHECK_FALSE(cap.permits_role(input::InjectInput::zen_name, 1, input::kInputRole));

    row.may = {guests::kPowerInspect};
    const loom::Grant look = guests::grant_for(row);
    CHECK(look.permits(loom::DescribeAccepted::zen_name, 1, loom::WeaveId{7}));
    CHECK(look.permits_role(ws::GuestConnectionsRequested::zen_name, 1, ws::kGuestsRole));
    CHECK_FALSE(look.permits_role(input::InjectInput::zen_name, 1, input::kInputRole));
    CHECK_FALSE(in.permits_role(ws::GuestConnectionsRequested::zen_name, 1, ws::kGuestsRole));
    // "inspect" reaches DISCOVERY (zen.DescribeAccepted), never the inventory's own doors: the
    // prompt this phase implements is explicit that inventory access is never a reinterpretation
    // of an existing power.
    CHECK_FALSE(look.permits_role(inv::InventorySet::zen_name, 1, inv::kInventoryRole));

    row.may = {guests::kPowerInventory};
    const loom::Grant stow = guests::grant_for(row);
    CHECK(stow.permits_role(inv::InventorySet::zen_name, 1, inv::kInventoryRole));
    CHECK(stow.permits_role(inv::InventoryGet::zen_name, 1, inv::kInventoryRole));
    for (const char* shape : {inv::InventoryLocate::zen_name, inv::InventoryRead::zen_name,
                              inv::InventoryWrite::zen_name}) {
        CHECK(stow.permits_role(shape, 1, inv::kInventoryRole));
        CHECK_FALSE(in.permits_role(shape, 1, inv::kInventoryRole));
    }
    CHECK(stow.permits_role(inv::InventoryCaptureDescribe::zen_name, 1, inv::kInventoryRole));
    CHECK_FALSE(stow.permits(loom::DescribeAccepted::zen_name, 1, loom::WeaveId{7}));
    CHECK_FALSE(stow.permits_role(input::InjectInput::zen_name, 1, input::kInputRole));
    CHECK_FALSE(stow.permits_role(surface::SurfaceCaptureRequested::zen_name, 1, surface::kSkinRole));
}

// =============================================================================
// The door
// =============================================================================

TEST_CASE("door: a credential admits as its row, and the inventory says both names") {
    Scratch f("admit");
    f.write(kTwoGuests);
    guests::GuestsFile file;
    std::string why;
    REQUIRE_MESSAGE(guests::read_guests_file(f.path, &file, &why), why);
    Rig r(file);
    Guest g(r.port, "whoever-i-say", "open-sesame");
    REQUIRE(r.beat_until([&] {
        g.poll();
        return g.client->admitted();
    }));
    CHECK(g.client->established_name() == "agent");
    // The inventory was said, and it keeps the claim and the establishment apart.
    REQUIRE(r.beat_until([&] {
        for (const ws::GuestConnections& said : r.inventories) {
            for (const ws::GuestConnection& c : said.rows) {
                if (c.state == "admitted" && c.established == "agent" &&
                    c.claimed == "whoever-i-say" && c.session == static_cast<std::int64_t>(g.client->session())) {
                    return true;
                }
            }
        }
        return false;
    }));
    CHECK(r.inventories.back().listen == "127.0.0.1:" + std::to_string(r.port));
}

TEST_CASE("door: a wrong credential is refused in words and leaves no session; nothing acts") {
    Scratch f("refuse");
    f.write(kTwoGuests);
    guests::GuestsFile file;
    std::string why;
    REQUIRE_MESSAGE(guests::read_guests_file(f.path, &file, &why), why);
    Rig r(file);
    Guest g(r.port, "agent", "wrong");
    REQUIRE(r.beat_until([&] {
        g.poll();
        return g.client->denied();
    }));
    CHECK(g.client->denial() == "no guest of this Workshop presents that credential");
    REQUIRE(r.beat_until([&] { return r.door->server().connection_count() == 0; }));
    CHECK(r.bus.list_weaves().size() == 4); // input, ears, inventory ears, the door: no proxy
    CHECK(r.door->inventory().refused == 1);
    // Nothing it said reached the Input weave: no session exists.
    Guest again(r.port, "agent", "open-sesame");
    REQUIRE(r.beat_until([&] {
        again.poll();
        return again.client->admitted();
    }));
    again.ask(input::kInputRole, 1, input::InputSessionRequested{"mine"});
    REQUIRE(r.beat_until([&] {
        again.poll();
        return again.answered<input::InputSessionOpened>(1).has_value();
    }));
    CHECK(again.answered<input::InputSessionOpened>(1)->session == 1);
}

TEST_CASE("door: an `ask` row waits on the host's decision, acting on nothing meanwhile") {
    Scratch f("ask");
    f.write(kTwoGuests);
    guests::GuestsFile file;
    std::string why;
    REQUIRE_MESSAGE(guests::read_guests_file(f.path, &file, &why), why);
    Rig r(file);
    Guest g(r.port, "watcher", "later");
    std::int64_t waiting = 0;
    REQUIRE(r.beat_until([&] {
        g.poll();
        for (const ws::GuestConnection& c : r.door->inventory().rows) {
            if (c.state == "awaiting-decision") {
                waiting = c.connection;
            }
        }
        return waiting != 0;
    }));
    CHECK_FALSE(g.client->admitted());
    CHECK_FALSE(g.client->denied());
    g.ask(input::kInputRole, 1, input::InputSessionRequested{"too early"});
    REQUIRE(r.beat_until([&] {
        g.poll();
        return g.last(loom::BridgeEvent::Kind::SendRefused) != nullptr;
    }));
    CHECK(g.last(loom::BridgeEvent::Kind::SendRefused)->reason.find("not admitted") !=
          std::string::npos);
    // THE HOST DECIDES -- the seam a popup attaches to -- and the grant is the host's word.
    loom::ConnectionAdmitted a;
    a.grant = guests::grant_for(file.rows[1]);
    a.established_name = "watcher";
    REQUIRE(r.door->decide(static_cast<std::uint64_t>(waiting), loom::ConnectionVerdict::admit(a)));
    REQUIRE(r.beat_until([&] {
        g.poll();
        return g.client->admitted();
    }));
    CHECK(g.client->established_name() == "watcher");
    // The watcher may inspect and may not inject: the far bus refuses, and the guest is told.
    g.ask(input::kInputRole, 2, input::InputSessionRequested{"still not mine"});
    REQUIRE(r.beat_until([&] {
        g.poll();
        const loom::BridgeEvent* d = g.last(loom::BridgeEvent::Kind::Delivered);
        return d != nullptr && d->correlation == 2 && d->dispatch_refused;
    }));
    g.ask(input::kInputRole, 3, loom::DescribeAccepted{});
    REQUIRE(r.beat_until([&] {
        g.poll();
        return g.answered_as(3, loom::accepted_shapes_schema()).has_value();
    }));
    CHECK_FALSE(g.answered_as(3, loom::accepted_shapes_schema())->get("accepted")->as_list().empty());
}

TEST_CASE("door: a guest injects through the real Input weave, and its socket's end ends its session") {
    Scratch f("journey");
    f.write(kTwoGuests);
    guests::GuestsFile file;
    std::string why;
    REQUIRE_MESSAGE(guests::read_guests_file(f.path, &file, &why), why);
    Rig r(file);
    {
        Guest g(r.port, "agent", "open-sesame");
        REQUIRE(r.beat_until([&] {
            g.poll();
            return g.client->admitted();
        }));
        g.ask(input::kInputRole, 1, input::InputSessionRequested{"the journey"});
        REQUIRE(r.beat_until([&] {
            g.poll();
            return g.answered<input::InputSessionOpened>(1).has_value();
        }));
        const std::int64_t session = g.answered<input::InputSessionOpened>(1)->session;
        input::InjectInput batch;
        batch.session = session;
        input::InjectedEvent down;
        down.kind = "KeyPressed";
        down.scancode = input::scan::kP;
        down.modifiers = input::mod::kCtrl;
        input::InjectedEvent held;
        held.kind = "PointerButton";
        held.button = 1;
        held.pressed = true;
        held.x = 5;
        held.y = 6;
        held.space = input::space::kPixels;
        batch.events = {down, held};
        g.ask(input::kInputRole, 2, batch);
        REQUIRE(r.beat_until([&] {
            g.poll();
            return g.answered<input::InputInjected>(2).has_value();
        }));
        CHECK(g.answered<input::InputInjected>(2)->admitted == 2);
        REQUIRE(r.heard.size() == 2);
        CHECK(as<input::KeyPressed>(r.heard, 0).scancode == input::scan::kP);
        CHECK(as<input::PointerButton>(r.heard, 1).pressed);
        // The Delivered carried the Input weave's stamp and Loom's attestation.
        const loom::BridgeEvent* d = g.last(loom::BridgeEvent::Kind::Delivered);
        REQUIRE(d != nullptr);
        CHECK(d->answers_ask);
        CHECK(d->sender == r.input_id.value);
    } // the guest's socket closes here, with a key and a button still held

    // THE DOOR NOTICES, AND CLOSES THE SESSION ON THE GUEST'S BEHALF: the held key and button
    // come up from the same producer, and the next guest can open a session of its own.
    REQUIRE(r.beat_until([&] { return r.heard.size() == 4; }));
    CHECK(as<input::KeyReleased>(r.heard, 2).scancode == input::scan::kP);
    CHECK_FALSE(as<input::PointerButton>(r.heard, 3).pressed);
    CHECK(as<input::PointerButton>(r.heard, 3).x == 5);
    REQUIRE(r.beat_until([&] { return r.door->server().connection_count() == 0; }));
    Guest next(r.port, "agent", "open-sesame");
    REQUIRE(r.beat_until([&] {
        next.poll();
        return next.client->admitted();
    }));
    next.ask(input::kInputRole, 7, input::InputSessionRequested{"the next"});
    REQUIRE(r.beat_until([&] {
        next.poll();
        return next.answered<input::InputSessionOpened>(7).has_value();
    }));
    CHECK(next.answered<input::InputSessionOpened>(7)->session == 2);
    // ...and the inventory said the first connection closed before it was dropped.
    bool said_closed = false;
    for (const ws::GuestConnections& said : r.inventories) {
        for (const ws::GuestConnection& c : said.rows) {
            said_closed = said_closed || c.state == "closed";
        }
    }
    CHECK(said_closed);
}

// =============================================================================
// Presentation after input: WHEN may an agent take the picture of what its input did?
// =============================================================================
//
// One bus, the real Input weave and the real Skin shell (over a medium that records what it
// was last told to paint), and an ordinary input consumer that does not paint in the delivery
// that heard the key: it takes several deliveries of its own first, as a desk that asks a
// pane for its rows does. The agent asks for a picture the moment the Input weave says its
// moments are published.

namespace {

/// A medium that remembers the width of the last canvas it painted and hands that back as
/// its picture -- enough to tell which paint a capture saw.
struct WidthMedium {
    std::int64_t width = 0;
    std::int64_t painted = 0;
    void frame(const zengine::snake::SnakeVisual&, bool) {}
    void canvas(const surface::SurfaceCanvas& c, bool) {
        width = c.width;
        ++painted;
    }
    void note(std::string_view, std::string_view) {}
    void pump() {}
    void clipboard_copy(const std::string&) {}
    std::optional<std::string> clipboard_text() { return std::nullopt; }
    surface::SurfaceExtent extent() const { return {}; }
    std::optional<surface::SurfacePlacement> placement() { return std::nullopt; }
    void place(const surface::SurfacePlacementRemembered&) {}
    std::optional<surface::CapturedPicture> capture() {
        if (painted == 0) {
            return std::nullopt;
        }
        surface::CapturedPicture p;
        p.width = width;
        p.height = 1;
        p.format = "text/cells";
        p.bytes = std::string(static_cast<std::size_t>(width), '#') + "\n";
        return p;
    }
};

struct DeskStep {
    std::int64_t left = 0;
    ZEN_SHAPE(DeskStep, 1, ZEN_FIELD(left));
};

/// An ordinary consumer of input: a key starts `steps` deliveries of its own, and only the
/// last one paints -- at `width_`, which the case sets per run.
class SlowDesk : public loom::WeaveBase<SlowDesk, EarsState, loom::Accept<input::KeyPressed, DeskStep>,
                                        loom::Emit<DeskStep, surface::SurfaceCanvas>> {
public:
    std::int64_t steps = 3;
    std::int64_t width_ = 20;
    void on(const input::KeyPressed&, loom::Mail& mail) {
        ++state_.heard;
        (void)mail.send(this->self_, DeskStep{steps});
    }
    void on(const DeskStep& s, loom::Mail& mail) {
        if (s.left > 0) {
            (void)mail.send(this->self_, DeskStep{s.left - 1});
            return;
        }
        surface::SurfaceCanvas c;
        c.width = width_;
        c.height = 1;
        mail.publish(c);
    }
};

/// The agent: opens a session, injects, and asks for a picture when told its moments are
/// published -- the ordering the capture door used to promise.
class OrderAgent : public loom::WeaveBase<OrderAgent, EarsState,
                                          loom::Accept<input::PumpInput, input::InputSessionOpened,
                                                       input::InputInjected, loom::Ack, loom::Refused,
                                                       surface::SurfaceCaptured>,
                                          loom::Emit<input::InputSessionRequested, input::InjectInput,
                                                     surface::SurfaceCaptureRequested>> {
public:
    std::function<void(loom::Mail&)> next_;
    std::int64_t session = 0;
    bool capture_on_admission = true;
    std::vector<surface::SurfaceCaptured> pictures;
    std::vector<std::string> said;
    void on(const input::PumpInput&, loom::Mail& mail) {
        if (next_) {
            next_(mail);
        }
    }
    void on(const input::InputSessionOpened& o, loom::Mail& mail) {
        if (mail.answers_ask()) {
            session = o.session;
        }
    }
    void on(const input::InputInjected& i, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        said.push_back("injected " + std::to_string(i.admitted));
        if (capture_on_admission) {
            (void)mail.send_to_role(surface::kSkinRole, surface::SurfaceCaptureRequested{});
        }
    }
    void on(const loom::Ack&, loom::Mail&) {}
    void on(const loom::Refused& r, loom::Mail&) { said.push_back("refused: " + r.reason); }
    void on(const surface::SurfaceCaptured& c, loom::Mail& mail) {
        if (mail.answers_ask()) {
            pictures.push_back(c);
        }
    }
};

struct OrderRig {
    loom::Switchboard bus;
    std::vector<std::vector<InputEvent>> feed;
    loom::WeaveId input_id{};
    loom::WeaveId skin_id{};
    surface::SkinT<WidthMedium>* skin = nullptr;
    SlowDesk* desk = nullptr;
    OrderAgent* agent = nullptr;
    loom::WeaveId agent_id{};

    OrderRig() {
        {
            auto w = std::make_unique<input::InputWeaveT<FakeReader>>(FakeReader{&feed});
            input::InputWeaveT<FakeReader>* raw = w.get();
            loom::Grant grant = loom::emit_default_grant(*raw);
            loom::allow_poke_answers(grant);
            input_id = bus.register_weave(std::move(w), std::move(grant),
                                          std::string(input::kInputRole));
            raw->zen_set_self(input_id);
        }
        {
            auto w = std::make_unique<surface::SkinT<WidthMedium>>();
            skin = w.get();
            loom::Grant grant = loom::emit_default_grant(*skin);
            loom::allow_poke_answers(grant);
            skin_id = bus.register_weave(std::move(w), std::move(grant),
                                         std::string(surface::kSkinRole));
            skin->zen_set_self(skin_id);
        }
        const loom::WeaveId desk_id = loom::mount<SlowDesk>(bus);
        desk = static_cast<SlowDesk*>(bus.weave(desk_id));
        agent_id = loom::mount<OrderAgent>(bus);
        agent = static_cast<OrderAgent*>(bus.weave(agent_id));
        surface::SurfaceCanvas first;
        first.width = 10;
        first.height = 1;
        (void)bus.send(skin_id, loom::Message(loom::to_value(first)));
        bus.drain_until_idle();
    }
    void act(std::function<void(loom::Mail&)> f) {
        agent->next_ = std::move(f);
        (void)bus.send(agent_id, loom::Message(loom::to_value(input::PumpInput{})));
        bus.drain_until_idle();
        agent->next_ = nullptr;
    }
};

input::InjectedEvent ctrl_p() {
    input::InjectedEvent e;
    e.kind = "KeyPressed";
    e.scancode = input::scan::kP;
    e.modifiers = input::mod::kCtrl;
    return e;
}

} // namespace

TEST_CASE("order: a picture taken on the injection's answer can miss what the input painted") {
    // THE PINNED NEGATIVE. `InputInjected` says the moments are PUBLISHED -- admission, not
    // presentation -- and a consumer that paints after deliveries of its own is overtaken by a
    // request queued after that answer, FIFO or not. Nobody may promise otherwise again.
    OrderRig r;
    r.act([](loom::Mail& m) {
        (void)m.send_to_role(input::kInputRole, input::InputSessionRequested{"order"});
    });
    REQUIRE(r.agent->session == 1);
    r.act([&](loom::Mail& m) {
        input::InjectInput batch;
        batch.session = r.agent->session;
        batch.events = {ctrl_p()};
        (void)m.send_to_role(input::kInputRole, batch);
    });
    REQUIRE(r.agent->pictures.size() == 1);
    const surface::SurfaceCaptured& shot = r.agent->pictures[0];
    INFO("the picture: width " << shot.width << " at frame " << shot.frame << "; the medium then "
                               << "shows width " << r.skin->medium().width << " at frame "
                               << r.skin->frames());
    CHECK(shot.width == 10);                  // the canvas from before the key
    CHECK(r.skin->medium().width == 20);      // ...which the key's own paint replaced afterwards
}

TEST_CASE("order: a picture asked for after the injection SETTLES shows what the input painted, every run") {
    OrderRig r;
    r.agent->capture_on_admission = false;
    r.act([](loom::Mail& m) {
        (void)m.send_to_role(input::kInputRole, input::InputSessionRequested{"order"});
    });
    REQUIRE(r.agent->session == 1);
    std::int64_t seen_frame = 1; // what an agent that remembered frames would have remembered
    for (const std::int64_t width : {std::int64_t{20}, std::int64_t{40}}) {
        r.desk->width_ = width;
        const std::size_t shots = r.agent->pictures.size();
        // THE HOST INJECTS ON THE AGENT'S BEHALF, FENCED -- as a door does for a guest.
        input::InjectInput batch;
        batch.session = r.agent->session;
        batch.events = {ctrl_p()};
        loom::Fence fence;
        REQUIRE(r.bus
                    .send_as_to_role_fenced(r.agent_id, input::kInputRole,
                                            loom::Message(loom::to_value(batch)), &fence)
                    .valid());
        // ...and somebody else paints in the middle of the input's work: a frame newer than
        // the last one the agent saw, which is not the input's.
        surface::SurfaceCanvas unrelated;
        unrelated.width = 30;
        unrelated.height = 1;
        (void)r.bus.send(r.skin_id, loom::Message(loom::to_value(unrelated)));
        int turns = 0;
        bool newer_before_settled = false;
        while (r.bus.fence_state(fence) == loom::FenceState::Open && turns < 50) {
            (void)r.bus.pump_pending();
            ++turns;
            if (r.bus.fence_state(fence) == loom::FenceState::Open && r.skin->frames() > seen_frame) {
                newer_before_settled = true; // "a later frame" arrived before the input was done
            }
        }
        REQUIRE(r.bus.fence_state(fence) == loom::FenceState::Settled);
        r.bus.release_fence(fence);
        CHECK(newer_before_settled);
        // THE PICTURE, ASKED FOR NOW: after everything the injection set in motion.
        r.act([](loom::Mail& m) {
            (void)m.send_to_role(surface::kSkinRole, surface::SurfaceCaptureRequested{});
        });
        REQUIRE(r.agent->pictures.size() == shots + 1);
        const surface::SurfaceCaptured& shot = r.agent->pictures.back();
        INFO("run for width " << width << ": the picture is width " << shot.width << " at frame "
                              << shot.frame);
        CHECK(shot.ok);
        CHECK(shot.width == width);
        CHECK(shot.frame == r.skin->frames());
        seen_frame = shot.frame;
    }
}

TEST_CASE("order: through the real door, a settle-requested injection is told Settled after the desk painted") {
    guests::GuestsFile file;
    guests::GuestRow agent;
    agent.name = "agent";
    agent.credential = "open-sesame";
    agent.may = {guests::kPowerInput, guests::kPowerCapture};
    file.rows.push_back(agent);
    Rig r(file);
    // The desk and the Skin, beside the door and the Input weave.
    const loom::WeaveId desk_id = loom::mount<SlowDesk>(r.bus);
    (void)desk_id;
    surface::SkinT<WidthMedium>* skin = nullptr;
    loom::WeaveId skin_id{};
    {
        auto w = std::make_unique<surface::SkinT<WidthMedium>>();
        skin = w.get();
        loom::Grant grant = loom::emit_default_grant(*skin);
        loom::allow_poke_answers(grant);
        skin_id = r.bus.register_weave(std::move(w), std::move(grant),
                                       std::string(surface::kSkinRole));
        skin->zen_set_self(skin_id);
    }
    surface::SurfaceCanvas first;
    first.width = 10;
    first.height = 1;
    (void)r.bus.send(skin_id, loom::Message(loom::to_value(first)));
    r.bus.drain_until_idle();
    Guest g(r.port, "agent", "open-sesame");
    REQUIRE(r.beat_until([&] {
        g.poll();
        return g.client->admitted();
    }));
    g.ask(input::kInputRole, 1, input::InputSessionRequested{"order through the door"});
    REQUIRE(r.beat_until([&] {
        g.poll();
        return g.answered<input::InputSessionOpened>(1).has_value();
    }));
    input::InjectInput batch;
    batch.session = g.answered<input::InputSessionOpened>(1)->session;
    batch.events = {ctrl_p()};
    g.client->send_to_role(input::kInputRole, 2, loom::serialize(loom::to_value(batch)),
                           /*settle=*/true);
    g.client->flush();
    bool answered_first = false;
    REQUIRE(r.beat_until([&] {
        g.poll();
        for (const loom::BridgeEvent& e : g.events) {
            if (e.kind == loom::BridgeEvent::Kind::Settled && e.correlation == 2) {
                answered_first = g.answered<input::InputInjected>(2).has_value();
                return true;
            }
        }
        return false;
    }));
    CHECK(answered_first);
    CHECK(skin->medium().width == 20); // the desk's paint is done by the time Settled is told
    g.ask(surface::kSkinRole, 3, surface::SurfaceCaptureRequested{});
    REQUIRE(r.beat_until([&] {
        g.poll();
        return g.answered<surface::SurfaceCaptured>(3).has_value();
    }));
    CHECK(g.answered<surface::SurfaceCaptured>(3)->width == 20);
    CHECK(r.bus.fences_held() == 0); // the door let its fence go once it told the guest
}

TEST_CASE("observe: through the real door, a guest observes only what its row lists, marked with its "
          "own send, until the host revokes it or its socket ends") {
    Scratch f("relay");
    f.write(R"({"listen":"127.0.0.1:0","guests":[)"
            R"({"name":"agent","credential":"a-cred","may":["input"],"observe":[)"
            R"({"producer":"zengine.input","shape":"KeyPressed","version":")" +
            std::to_string(input::KeyPressed::zen_version) + R"("}]},)"
            R"({"name":"hands","credential":"h-cred","may":["input"]}]})");
    guests::GuestsFile file;
    std::string why;
    REQUIRE_MESSAGE(guests::read_guests_file(f.path, &file, &why), why);
    Rig r(file);
    loom::observe::Relay* relay = ws::mount_observation(r.bus, *r.door, file);
    namespace ob = loom::observe;
    const auto subscribe = [](const char* shape) {
        ob::Subscribe s;
        s.producer = input::kInputRole;
        s.shapes = {ob::ShapeRef{shape, input::KeyPressed::zen_version}}; // KeyReleased: the same
        s.encoding = ob::kEncodingNative;
        s.label = "suite";
        return s;
    };
    Guest hands(r.port, "hands", "h-cred");
    {
        Guest g(r.port, "agent", "a-cred");
        REQUIRE(r.beat_until([&] {
            g.poll();
            hands.poll();
            return g.client->admitted() && hands.client->admitted();
        }));
        // A ROW WITHOUT `observe` MAY NOT EVEN ASK: the far bus refuses the send.
        hands.ask(ob::kObserveRole, 1, subscribe("KeyPressed"));
        REQUIRE(r.beat_until([&] {
            hands.poll();
            const loom::BridgeEvent* d = hands.last(loom::BridgeEvent::Kind::Delivered);
            return d != nullptr && d->correlation == 1 && d->dispatch_refused;
        }));
        // A LISTED ROW is refused a shape its list does not name, in the policy's words...
        g.ask(ob::kObserveRole, 1, subscribe("KeyReleased"));
        REQUIRE(r.beat_until([&] {
            g.poll();
            return g.answered<loom::Refused>(1).has_value();
        }));
        const std::string refused = g.answered<loom::Refused>(1)->reason;
        CHECK_MESSAGE(refused.find("may not observe KeyReleased v" +
                                   std::to_string(input::KeyReleased::zen_version)) !=
                          std::string::npos,
                      refused);
        // ...and told yes for the one it does, beginning now.
        g.ask(ob::kObserveRole, 2, subscribe("KeyPressed"));
        REQUIRE(r.beat_until([&] {
            g.poll();
            return g.answered<ob::Subscribed>(2).has_value();
        }));
        const ob::Subscribed sub = *g.answered<ob::Subscribed>(2);
        CHECK(sub.holder == static_cast<std::int64_t>(r.input_id.value));
        CHECK(relay->active() == 1);
        // THE GUEST'S OWN SETTLED INJECTION: the key it pressed is told to it, carrying its own
        // correlation as the cause, and arrives before the settlement.
        g.ask(input::kInputRole, 3, input::InputSessionRequested{"observed"});
        REQUIRE(r.beat_until([&] {
            g.poll();
            return g.answered<input::InputSessionOpened>(3).has_value();
        }));
        input::InjectInput batch;
        batch.session = g.answered<input::InputSessionOpened>(3)->session;
        input::InjectedEvent down;
        down.kind = "KeyPressed";
        down.scancode = input::scan::kT;
        batch.events = {down};
        g.ask(input::kInputRole, 4, batch, /*settle=*/true);
        REQUIRE(r.beat_until([&] {
            g.poll();
            return g.index_of(loom::BridgeEvent::Kind::Settled, 4) < g.events.size();
        }));
        const auto told = g.said_as<ob::Observed>();
        REQUIRE(told.size() == 1);
        CHECK(told[0].second.seq == 1);
        CHECK(told[0].second.shape == "KeyPressed");
        CHECK(told[0].second.cause == 4);
        CHECK(told[0].second.producer == static_cast<std::int64_t>(r.input_id.value));
        CHECK(told[0].first < g.index_of(loom::BridgeEvent::Kind::Settled, 4));
        // THE HOST REVOKES -- the seam a maker's control will call -- and the guest is TOLD.
        CHECK(relay->revoke(loom::WeaveId{g.client->session()}, "the maker withdrew it") == 1);
        REQUIRE(r.beat_until([&] {
            g.poll();
            return !g.said_as<ob::Ended>().empty();
        }));
        CHECK(g.said_as<ob::Ended>()[0].second.kind == ob::kEndedRevoked);
        CHECK(g.said_as<ob::Ended>()[0].second.last == 1);
        CHECK(relay->active() == 0);
        // Subscribed again, then the socket ends with the subscription held.
        g.ask(ob::kObserveRole, 5, subscribe("KeyPressed"));
        REQUIRE(r.beat_until([&] {
            g.poll();
            return g.answered<ob::Subscribed>(5).has_value();
        }));
        CHECK(relay->active() == 1);
    }
    // THE DOOR SAYS THE SESSION IS GONE, AND THE RELAY FORGETS WHAT IT HELD FOR IT.
    REQUIRE(r.beat_until([&] { return relay->active() == 0; }));
    CHECK(hands.said_as<ob::Observed>().empty()); // nothing reached the guest that asked for nothing
}

TEST_SUITE_END();

TEST_CASE("demo power reaches only its setup and control owners") {
    zengine::workshop::guests::GuestRow row;
    row.may = {"input", "capture", "inspect", "inventory"};
    auto grant = zengine::workshop::guests::grant_for(row);
    CHECK_FALSE(grant.permits_role("SetupApplyRequested", 1, "zengine.workshop"));
    CHECK_FALSE(grant.permits_role("DemoResetRequested", 1, "zengine.demo"));
    row.may = {"demo"}; grant = zengine::workshop::guests::grant_for(row);
    CHECK(grant.permits_role("SetupApplyRequested", 1, "zengine.workshop"));
    CHECK(grant.permits_role("PaneResetRequested", 1, "zengine.info"));
    CHECK(grant.permits_role("PaneResetRequested", 1, "zengine.composer"));
    CHECK(grant.permits_role("PaneResetRequested", 1, "zengine.inventory-pane"));
    CHECK_FALSE(grant.permits_role("PaneResetRequested", 1, "other.pane"));
    CHECK_FALSE(grant.permits_role("InjectInput", 1, "zengine.input"));
    CHECK_FALSE(grant.permits_role("InventoryWrite", 1, "zengine.inventory"));
}

TEST_CASE("inventory power permits field pickup but input and inspection alone do not") {
    zengine::workshop::guests::GuestRow row;
    row.may = {"inventory"};
    CHECK(zengine::workshop::guests::grant_for(row).permits_role(
        zengine::workshop::PaneValueCarryRequested::zen_name, 1, "zengine.workshop"));
    row.may = {"input", "inspect"};
    CHECK_FALSE(zengine::workshop::guests::grant_for(row).permits_role(
        zengine::workshop::PaneValueCarryRequested::zen_name, 1, "zengine.workshop"));
}

TEST_CASE("open power reaches only the managed opening, and no other power opens a source") {
    zengine::workshop::guests::GuestRow row;
    row.may = {"input", "capture", "inspect", "inventory", "toolbox", "demo"};
    auto grant = zengine::workshop::guests::grant_for(row);
    CHECK_FALSE(grant.permits_role("OpenSourceRequested", 1, "zengine.opening"));
    row.may = {"open"};
    grant = zengine::workshop::guests::grant_for(row);
    CHECK(grant.permits_role("OpenSourceRequested", 1, "zengine.opening"));
    // Not the Editor's old door, not a preparation, not a save, and no input or carry of its own.
    CHECK_FALSE(grant.permits_role("OpenSourceRequested", 1, "zengine.editor"));
    CHECK_FALSE(grant.permits_role("PrepareSourceRequested", 1, "zengine.editor"));
    CHECK_FALSE(grant.permits_role("PaneActionRequested", 1, "zengine.editor"));
    CHECK_FALSE(grant.permits_role("InjectInput", 1, "zengine.input"));
    CHECK_FALSE(grant.permits_role("PaneValueCarryRequested", 1, "zengine.workshop"));
    // ...and a guests file may name it.
    Scratch f("open");
    f.write(R"({"guests":[{"name":"a","credential":"x","may":["input","open"]}]})");
    zengine::workshop::guests::GuestsFile file;
    std::string why;
    CHECK_MESSAGE(zengine::workshop::guests::read_guests_file(f.path, &file, &why), why);
    REQUIRE(file.rows.size() == 1);
    CHECK(file.rows[0].may.size() == 2);
}
