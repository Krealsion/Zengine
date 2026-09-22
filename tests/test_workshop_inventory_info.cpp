// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "workshop_support.hpp"
#include "inventory/codec.hpp"
#include "inventory/vocabulary.hpp"
#include "input/input_weave.hpp"
#include <zen/host/grant_wiring.hpp>

namespace {
namespace inv = zengine::inventory;
struct InventoryHandState { ZEN_SHAPE(InventoryHandState, 1); };
struct InventoryHandDo { ZEN_SHAPE(InventoryHandDo, 1); };
class InventoryHand : public loom::WeaveBase<InventoryHand, InventoryHandState,
    loom::Accept<InventoryHandDo, input::InputSessionOpened, input::InputInjected, inv::InventoryEntry, loom::Refused>,
    loom::Emit<input::InputSessionRequested, input::InjectInput>> {
public:
    std::function<void(loom::Mail&)> next;
    std::int64_t session = 0;
    std::vector<inv::InventoryEntry> entries;
    void on(const inv::InventoryEntry& e, loom::Mail&) { entries.push_back(e); }
    void on(const InventoryHandDo&, loom::Mail& m) { next(m); }
    void on(const input::InputSessionOpened& s, loom::Mail&) { session = s.session; }
    void on(const input::InputInjected&, loom::Mail&) {}
    void on(const loom::Refused& r, loom::Mail&) { FAIL(r.reason); }
};
struct QuietReader {
    std::vector<std::variant<input::KeyPressed, input::KeyReleased, input::TextEntered,
        input::PointerMoved, input::PointerButton, input::PointerWheel>> poll() { return {}; }
};

struct InventoryStory {
    PaneRig r;
    InventoryHand* hand = nullptr;
    loom::WeaveId hand_id;
    std::int64_t source = 0, info = 0;
    std::string trace;

    explicit InventoryStory(int permissions = 7) {
        r.mount_workshop();
        r.host.input_authority = [&](loom::WeaveId actor) {
            return r.bus.alive(actor) ? loom::host_grant_authority(r.bus, actor,
                loom::LiveAuthority::nothing()) : loom::GrantAuthority{};
        };
        load::LoadPlan plan;
        for (const auto& [stem, role] : std::vector<std::pair<std::string, std::string>>{
            {"zengine-info-pane", "zengine.info"}, {"zengine-inventory", inv::kInventoryRole},
            {"zengine-inventory-pane", "zengine.inventory-pane"},
            {"zengine-menu-presenter", kPresenterRole}}) {
            load::ArtifactIntent artifact;
            artifact.stem = stem; artifact.weave = load::WeaveIntent{role};
            plan.artifacts.push_back(artifact);
        }
        const auto done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready(); r.extent(180, 60);
        r.pick({"zengine.inventory-pane", "inventory"});
        source = r.session().panels.runtime.find("zengine.inventory-pane", "inventory")->kind;
        info = r.session().panels.runtime.find("zengine.info", "info")->kind;
        for (auto& pane : r.session().setup.active.panes) {
            if (pane.ref.provider != "zengine.info" && pane.ref.provider != "zengine.inventory-pane") continue;
            pane.place = {pane_unit::kSubcells,
                (pane.ref.provider == "zengine.info" ? 85 : 2) * surface::kCellSubs, 4 * surface::kCellSubs};
            pane.width = {pane_unit::kSubcells, 80 * surface::kCellSubs};
            pane.height = {pane_unit::kSubcells, 24 * surface::kCellSubs};
        }
        r.extent(180, 60);
        using Input = input::InputWeaveT<QuietReader>;
        auto reader = std::make_unique<Input>();
        auto* reader_ptr = reader.get();
        auto grant = loom::emit_default_grant(*reader);
        const auto id = r.bus.register_weave(std::move(reader), grant, input::kInputRole);
        reader_ptr->zen_set_self(id);
        auto actor = std::make_unique<InventoryHand>(); hand = actor.get();
        loom::Grant actor_grant;
        actor_grant.allow_to_role(input::InputSessionRequested::zen_name, 1, input::kInputRole);
        actor_grant.allow_to_role(input::InjectInput::zen_name, 1, input::kInputRole);
        if (permissions & 1) actor_grant.allow_to_role(inv::InventoryLocate::zen_name, 1, inv::kInventoryRole);
        if (permissions & 2) actor_grant.allow_to_role(inv::InventoryRead::zen_name, 1, inv::kInventoryRole);
        if (permissions & 4) actor_grant.allow_to_role(inv::InventoryWrite::zen_name, 1, inv::kInventoryRole);
        hand_id = r.bus.register_weave(std::move(actor), actor_grant);
        hand->zen_set_self(hand_id);
        act([](loom::Mail& m) { m.send_to_role(input::kInputRole, input::InputSessionRequested{"inventory story"}); });
        REQUIRE(hand->session > 0);
        r.bus.add_observer([this](const loom::BusEvent& e) {
            if (e.schema_name == PaneDrop::zen_name || e.schema_name == PaneOperationRequested::zen_name ||
                e.schema_name == PaneOperationAnswered::zen_name || e.schema_name == inv::InventoryRead::zen_name ||
                e.schema_name == inv::InventoryEntry::zen_name || e.schema_name == loom::DispatchRefused::zen_name)
                trace += e.schema_name + " event=" + std::to_string(static_cast<int>(e.kind)) +
                    " to=" + std::to_string(e.target.value) + " from=" + e.authored_role + "\n";
        });
        store(7);
    }
    void act(std::function<void(loom::Mail&)> action) {
        hand->next = std::move(action);
        r.bus.send(hand_id, loom::Message(loom::to_value(InventoryHandDo{})));
        r.bus.drain_until_idle(); hand->next = {};
    }
    void event(input::InjectedEvent e) {
        act([&](loom::Mail& m) {
            m.send_to_role(input::kInputRole, input::InjectInput{hand->session, {std::move(e)}});
        });
    }
    void key(std::int64_t scan, std::int64_t mods = input::mod::kNone) {
        input::InjectedEvent e; e.kind = "KeyPressed"; e.scancode = scan; e.modifiers = mods;
        event(e); e.kind = "KeyReleased"; event(e);
    }
    void text(const std::string& text) {
        input::InjectedEvent e; e.kind = "TextEntered"; e.text = text; event(e);
    }
    void click(std::int64_t kind, std::int64_t row = 0, std::int64_t button = 1) {
        const auto rect = external_body_rect(r.session(), kind);
        input::InjectedEvent e; e.kind = "PointerButton"; e.button = button; e.pressed = true;
        e.space = input::space::kCells; e.x = rect.x + 1;
        e.y = rect.y + row + surface::kTuiCanvasTopRow +
            external_title_rows(r.session().panels, kind, r.session().pane_titles);
        REQUIRE(external_press_at(r.session().panels, r.session().setup.active,
            screen_of(r.session()), kind, r.session().pane_titles, e.space, e.x, e.y).named);
        event(e); e.pressed = false; event(e);
    }
    std::string shown(std::int64_t kind) {
        std::string text;
        for (const auto& row : pane_rows(r, kind)) text += row + "\n";
        return text;
    }
    void acquire() {
        click(source);
        REQUIRE(r.session().panels.keyboard == source);
        key(input::scan::kReturn);
    }
    void place(bool expect_entry = true) {
        click(info);
        INFO(trace);
        REQUIRE_MESSAGE(r.session().panels.keyboard == info, r.last_notice());
        if (expect_entry) REQUIRE_MESSAGE(shown(info).find("story.RuntimeItem") != std::string::npos,
                                         (r.last_notice() + "\n" + shown(info) + trace));
    }
    void store(std::int64_t number) {
        auto shape = loom::SchemaBuilder("story.RuntimeItem", 1).field("count", loom::Kind::Int).build();
        loom::Value item(shape); item.set("count", loom::Cell::integer(number));
        auto meta_shape = loom::SchemaBuilder("story.Metadata", 1).field("source", loom::Kind::Text).build();
        loom::Value meta(meta_shape); meta.set("source", loom::Cell::text("capture observation"));
        const auto data = inv::encode_pair(item, {meta});
        r.bus.send_to_role(inv::kInventoryRole,
            loom::Message(loom::to_value(inv::InventorySet{loom::Bytes(data.begin(), data.end())})));
        r.bus.drain_until_idle();
    }
    inv::DecodedPair stored() {
        const auto state = r.bus.weave(r.bus.role_holder(inv::kInventoryRole))->snapshot();
        const auto& bytes = state.get("pair")->as_bytes();
        return inv::decode_pair({reinterpret_cast<const char*>(bytes.data()), bytes.size()});
    }
    void edit(std::string value) {
        key(input::scan::kReturn); key(input::scan::kA, input::mod::kCtrl);
        text(value); key(input::scan::kReturn);
    }
};
}

TEST_CASE("inventory Info: authorized input carries an entry edits it and reads a fresh copy") {
    InventoryStory t;
    t.acquire();
    REQUIRE_MESSAGE(t.shown(t.source).find("Click Info") != std::string::npos, t.shown(t.source));
    t.place();
    REQUIRE_MESSAGE(t.shown(t.info).find("story.RuntimeItem") != std::string::npos, t.shown(t.info));
    t.edit("42");
    CHECK(t.stored().item.get("count")->as_int() == 7);
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK_MESSAGE(t.stored().item.get("count")->as_int() == 42, t.shown(t.info));
    CHECK(t.stored().metadata[0].get("source")->as_text() == "capture observation");
    t.edit("99");
    t.key(input::scan::kR, input::mod::kCtrl);
    CHECK(t.shown(t.info).find("Unsaved draft") != std::string::npos);
    t.key(input::scan::kR, input::mod::kCtrl);
    CHECK_MESSAGE(t.shown(t.info).find("count: 42") != std::string::npos, t.shown(t.info));
}

TEST_CASE("inventory Info: input permission alone cannot acquire a reference through a trusted pane") {
    InventoryStory t(false);
    t.acquire();
    CHECK_MESSAGE(t.shown(t.source).find("no authority") != std::string::npos, t.shown(t.source));
    t.place(false);
    CHECK(t.shown(t.info).find("story.RuntimeItem") == std::string::npos);
    CHECK(t.stored().item.get("count")->as_int() == 7);
}

TEST_CASE("inventory Info: replacing the slot leaves an open draft attached to its lost entry") {
    InventoryStory t;
    t.acquire(); t.place(); t.edit("42");
    t.store(8);
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK_MESSAGE(t.shown(t.info).find("no longer here") != std::string::npos, t.shown(t.info));
    CHECK(t.stored().item.get("count")->as_int() == 8);
    CHECK(t.shown(t.info).find("count: 42") != std::string::npos);
}

TEST_CASE("inventory Info: right-click acquisition and metadata inspection share the live route") {
    InventoryStory t;
    t.click(t.source, 0, 3);
    REQUIRE(t.r.session().presented.open);
    t.key(input::scan::kReturn);
    REQUIRE_MESSAGE(t.shown(t.source).find("Click Info") != std::string::npos, t.shown(t.source));
    t.place();
    t.key(input::scan::kDown);
    t.key(input::scan::kReturn);
    CHECK(t.shown(t.info).find("metadata is read-only") != std::string::npos);
    t.key(input::scan::kI, input::mod::kCtrl);
    CHECK(t.shown(t.info).find("PANES") != std::string::npos);
    t.key(input::scan::kI, input::mod::kCtrl);
    CHECK(t.shown(t.info).find("story.RuntimeItem") != std::string::npos);
}

TEST_CASE("inventory Info: read permission does not authorize a save through Info") {
    InventoryStory t(3);
    t.acquire(); t.place(); t.edit("42");
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK_MESSAGE(t.shown(t.info).find("no authority") != std::string::npos, t.shown(t.info));
    CHECK(t.stored().item.get("count")->as_int() == 7);
    CHECK(t.shown(t.info).find("count: 42") != std::string::npos);
}

TEST_CASE("inventory Info: an owner reload keeps the pair but invalidates old references") {
    InventoryStory t;
    t.acquire(); t.place(); t.edit("42");
    t.r.enqueue_reload("zengine-inventory", WORKSHOP_SO_INVENTORY);
    t.r.bus.drain_until_idle();
    REQUIRE(t.r.load_refusals.empty());
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK_MESSAGE(t.shown(t.info).find("no longer here") != std::string::npos, t.shown(t.info));
    CHECK(t.stored().item.get("count")->as_int() == 7);
    t.key(input::scan::kD, input::mod::kCtrl);
    t.acquire(); t.place();
    CHECK(t.shown(t.info).find("count: 7") != std::string::npos);
}

TEST_CASE("inventory Info: a concurrent writer wins and the stale draft remains readable") {
    InventoryStory t;
    t.acquire(); t.place(); t.edit("42");
    t.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryLocate{}, 501); });
    REQUIRE(t.hand->entries.size() == 1);
    auto entry = t.hand->entries.back();
    auto decoded = inv::decode_pair({reinterpret_cast<const char*>(entry.pair.data()), entry.pair.size()});
    decoded.item.set("count", loom::Cell::integer(8));
    const auto encoded = inv::encode_pair(decoded.item, decoded.metadata);
    t.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole,
        inv::InventoryWrite{entry.reference, entry.revision, loom::Bytes(encoded.begin(), encoded.end())}, 502); });
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK_MESSAGE(t.shown(t.info).find("entry changed") != std::string::npos, t.shown(t.info));
    CHECK(t.stored().item.get("count")->as_int() == 8);
    CHECK(t.shown(t.info).find("count: 42") != std::string::npos);
}

TEST_CASE("inventory Info: an ordinary message cannot impersonate physical maker input") {
    InventoryStory t(false);
    t.click(t.source);
    input::InjectedEvent key; key.kind = "KeyPressed"; key.scancode = input::scan::kReturn;
    t.r.publish(loom::to_value(input::AttributedInput{true, 0, key}));
    CHECK(t.shown(t.source).find("Click Info") == std::string::npos);
    t.r.key(input::scan::kReturn);
    CHECK_MESSAGE(t.shown(t.source).find("attributed input gesture") != std::string::npos, t.shown(t.source));
}
