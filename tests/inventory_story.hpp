// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_INVENTORY_STORY_HPP
#define ZENGINE_TESTS_INVENTORY_STORY_HPP
// A LOADED WORKSHOP WITH INFO, INVENTORY AND ITS PANE, an input session for an injected actor
// whose Loom authority each case chooses, and the gestures a maker's hand makes -- shared by the
// Inventory and Info suites and the Editor transfer story. Before writing a case, read
// docs/contributing/testing-workshop-panes.md: which traps are the product's, Loom's or the rig's.
// The `permissions` default omits the value carry, the terminal value, the toolbox, PokeDescribe,
// PanePoint and folder organization; `until_delivered` parks a delivery mid-turn (VM-FIX-24).
#include "workshop_support.hpp"
#include "inventory/codec.hpp"
#include "message-draft/transfer.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory-pane/slots.hpp"
#include "inventory-pane/presentation.hpp"
#include "input/input_weave.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"
#include <zen/host/grant_wiring.hpp>
#include <zen/weave/poke.hpp>
namespace inventory_story {
namespace inv = zengine::inventory;
namespace slots = zengine::inventory_pane;
struct InventoryHandState { ZEN_SHAPE(InventoryHandState, 1); };
struct InventoryHandDo { ZEN_SHAPE(InventoryHandDo, 1); };
class InventoryHand : public loom::WeaveBase<InventoryHand, InventoryHandState,
    loom::Accept<InventoryHandDo, input::InputSessionOpened, input::InputInjected, PaneView, PanePoint,
        inv::InventoryListed, inv::v2::InventoryListed, inv::InventoryFolderState, inv::InventoryEntry,
        slots::InventoryToolboxFinished, loom::Refused>,
    loom::Emit<intro::LoadedSelected, input::InputSessionRequested, input::InjectInput>> {
public:
    std::function<void(loom::Mail&)> next;
    std::int64_t session = 0;
    inv::InventoryListed listing;
    void on(const inv::InventoryListed& v, loom::Mail&) { listing = v; }
    inv::v2::InventoryListed organized;
    void on(const inv::v2::InventoryListed& v, loom::Mail&) { organized = v; }
    std::vector<inv::InventoryFolderState> folders;
    void on(const inv::InventoryFolderState& v, loom::Mail&) { folders.push_back(v); }
    std::vector<PaneView> views;
    std::vector<std::string> refusals;
    bool expect_refusal = false;
    void on(const PaneView& v, loom::Mail&) { views.push_back(v); }
    std::vector<PanePoint> points;
    void on(const PanePoint& p, loom::Mail&) { points.push_back(p); }
    std::vector<inv::InventoryEntry> entries;
    std::vector<slots::InventoryToolboxFinished> toolboxes;
    void on(const slots::InventoryToolboxFinished& v, loom::Mail& mail) { CHECK(mail.answers_ask()); toolboxes.push_back(v); }
    void on(const inv::InventoryEntry& e, loom::Mail&) { entries.push_back(e); }
    void on(const InventoryHandDo&, loom::Mail& m) { next(m); }
    void on(const input::InputSessionOpened& s, loom::Mail&) { session = s.session; }
    void on(const input::InputInjected&, loom::Mail&) {}
    void on(const loom::Refused& r, loom::Mail&) { refusals.push_back(r.reason); if (!expect_refusal) FAIL(r.reason); }
};
struct QuietReader {
    using Event = std::variant<input::KeyPressed, input::KeyReleased, input::TextEntered,
        input::PointerMoved, input::PointerButton, input::PointerWheel>;
    std::shared_ptr<std::vector<Event>> pending;
    std::vector<Event> poll() {
        std::vector<Event> out;
        if (pending) out.swap(*pending);
        return out;
    }
};
struct RemoveObserver {
    loom::Switchboard& bus;
    loom::ObserverId id;
    ~RemoveObserver() { bus.remove_observer(id); }
};

struct InventoryStory {
    /// The actor may organize folders and file entries (not in the default mask).
    static constexpr int kOrganize = 4096;
    PaneRig r;
    InventoryHand* hand = nullptr;
    loom::WeaveId hand_id;
    std::int64_t source = 0, info = 0;
    std::string trace;
    loom::ObserverId trace_observer{};
    std::shared_ptr<std::vector<QuietReader::Event>> physical =
        std::make_shared<std::vector<QuietReader::Event>>();

    explicit InventoryStory(int permissions = 191, bool composer = false, bool desktop_first = false) {
        r.mount_workshop();
        r.host.input_authority = [&](loom::WeaveId actor) {
            return r.bus.alive(actor) ? loom::host_grant_authority(r.bus, actor,
                loom::LiveAuthority::nothing()) : loom::GrantAuthority{};
        };
        load::LoadPlan plan;
        if (composer && desktop_first) {
            load::ArtifactIntent desktop; desktop.stem="zengine-desktop-pane";
            desktop.weave=load::WeaveIntent{"zengine.desktop"}; plan.artifacts.push_back(desktop);
        }
        for (const auto& [stem, role] : std::vector<std::pair<std::string, std::string>>{
            {"zengine-info-pane", "zengine.info"}, {"zengine-inventory", inv::kInventoryRole},
            {"zengine-inventory-pane", "zengine.inventory-pane"},
            {"zengine-menu-presenter", kPresenterRole}}) {
            load::ArtifactIntent artifact;
            artifact.stem = stem; artifact.weave = load::WeaveIntent{role};
            plan.artifacts.push_back(artifact);
        }
        if (composer) {
            load::ArtifactIntent artifact;
            artifact.stem = "zengine-composer"; artifact.weave = load::WeaveIntent{kComposerOffice};
            plan.artifacts.push_back(artifact);
            load::ArtifactIntent desktop;
            desktop.stem = "zengine-desktop-pane"; desktop.weave = load::WeaveIntent{"zengine.desktop"};
            if (!desktop_first) plan.artifacts.push_back(desktop);
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
        auto reader = std::make_unique<Input>(QuietReader{physical});
        auto* reader_ptr = reader.get();
        auto grant = loom::emit_default_grant(*reader);
        const auto id = r.bus.register_weave(std::move(reader), grant, input::kInputRole);
        reader_ptr->zen_set_self(id);
        auto actor = std::make_unique<InventoryHand>(); hand = actor.get();
        loom::Grant actor_grant;
        actor_grant.allow_to_role(input::InputSessionRequested::zen_name, 1, input::kInputRole);
        actor_grant.allow_to_role(input::InjectInput::zen_name, 1, input::kInputRole);
        actor_grant.allow_to_role(inv::InventoryList::zen_name, 1, inv::kInventoryRole);
        actor_grant.allow_to_role(inv::v2::InventoryList::zen_name, 2, inv::kInventoryRole);
        actor_grant.allow_to_role(PaneViewRequested::zen_name, 1, "zengine.workshop");
        if (permissions & 256) actor_grant.allow_to_role(TerminalValueRequested::zen_name, 1, "zengine.workshop");
        if (permissions & 1) actor_grant.allow_to_role(inv::InventoryLocate::zen_name, 1, inv::kInventoryRole);
        if (permissions & 2) actor_grant.allow_to_role(inv::InventoryRead::zen_name, 1, inv::kInventoryRole);
        if (permissions & 4) actor_grant.allow_to_role(inv::InventoryWrite::zen_name, 1, inv::kInventoryRole);
        if (permissions & 8) {
            actor_grant.allow_to_role(inv::InventoryAdd::zen_name, 1, inv::kInventoryRole);
            actor_grant.allow_to_role(inv::v2::InventoryAdd::zen_name, 2, inv::kInventoryRole);
        }
        if (permissions & kOrganize)
            for (const char* shape : {inv::InventoryFile::zen_name, inv::InventoryFolderCreate::zen_name,
                                      inv::InventoryFolderRename::zen_name, inv::InventoryFolderMove::zen_name,
                                      inv::InventoryFolderRemove::zen_name})
                actor_grant.allow_to_role(shape, 1, inv::kInventoryRole);
        if (permissions & 16) actor_grant.allow_to_role(inv::InventoryRename::zen_name, 1, inv::kInventoryRole);
        if (permissions & 32) actor_grant.allow_to_role(inv::InventoryRemove::zen_name, 1, inv::kInventoryRole);
        if (permissions & 64) actor_grant.allow_to_role(PaneValueCarryRequested::zen_name, 1, "zengine.workshop");
        if (permissions & 128) actor_grant.allow_to_role(zengine::inventory_pane::InventoryViewEdit::zen_name, 1, "zengine.inventory-pane");
        if (permissions & 1024) actor_grant.allow_to_any(loom::PokeDescribe::zen_name, loom::PokeDescribe::zen_version);
        if (permissions & 2048) actor_grant.allow_to_role(PanePointRequested::zen_name, 1, "zengine.workshop");
        actor_grant.allow_to_role(inv::InventoryCaptureAdd::zen_name, 1, inv::kInventoryRole);
        if (permissions & 512) {
            actor_grant.allow_to_role(slots::InventoryToolboxSave::zen_name, 1, slots::kRole);
            actor_grant.allow_to_role(slots::InventoryToolboxRestore::zen_name, 1, slots::kRole);
        }
        hand_id = r.bus.register_weave(std::move(actor), actor_grant);
        hand->zen_set_self(hand_id);
        act([](loom::Mail& m) { m.send_to_role(input::kInputRole, input::InputSessionRequested{"inventory story"}); });
        REQUIRE(hand->session > 0);
        trace_observer = r.bus.add_observer([this](const loom::BusEvent& e) {
            if (e.schema_name == PaneDrop::zen_name || e.schema_name == PaneOperationRequested::zen_name ||
                e.schema_name == PaneOperationAnswered::zen_name || e.schema_name == inv::InventoryRead::zen_name ||
                e.schema_name == inv::InventoryEntry::zen_name || e.schema_name == loom::DispatchRefused::zen_name)
                trace += e.schema_name + " event=" + std::to_string(static_cast<int>(e.kind)) +
                    " to=" + std::to_string(e.target.value) + " from=" + e.authored_role + "\n";
        });
        store(7);
    }
    ~InventoryStory() { r.bus.remove_observer(trace_observer); }
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
        key(input::scan::kReturn, input::mod::kCtrl);
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
    input::InjectedEvent button_at(std::int64_t kind, std::int64_t row, bool down) {
        const auto rect = external_body_rect(r.session(), kind);
        input::InjectedEvent e; e.kind = "PointerButton"; e.button = 1; e.pressed = down;
        e.space = input::space::kCells; e.x = rect.x + 1;
        e.y = rect.y + row + surface::kTuiCanvasTopRow +
            external_title_rows(r.session().panels, kind, r.session().pane_titles);
        return e;
    }
    void batch(std::vector<input::InjectedEvent> events) {
        act([&](loom::Mail& m) { m.send_to_role(input::kInputRole, input::InjectInput{hand->session, events}); });
    }
    input::InjectedEvent key_down(std::int64_t scan, std::int64_t mods = input::mod::kNone) {
        input::InjectedEvent e; e.kind = "KeyPressed"; e.scancode = scan; e.modifiers = mods;
        return e;
    }
    /// ONE INJECTED EVENT, DELIVERED TURN BY TURN until `shape` has reached `target`, and the
    /// turn stopped there (VM-FIX-24): whatever that delivery queued is still waiting, so what a
    /// case enqueues next lands between a request and its answer. Nothing is drained.
    bool until_delivered(const input::InjectedEvent& e, const std::string& shape, loom::WeaveId target) {
        bool seen = false;
        const auto stop = r.bus.add_observer([&](const loom::BusEvent& ev) {
            if (seen || ev.kind != loom::EventKind::Delivered || ev.schema_name != shape || ev.target != target) return;
            seen = true;
            r.bus.stop();
        });
        hand->next = [&](loom::Mail& m) { m.send_to_role(input::kInputRole, input::InjectInput{hand->session, {e}}); };
        r.bus.send(hand_id, loom::Message(loom::to_value(InventoryHandDo{})));
        for (int turn = 0; turn < 64 && !seen && r.bus.pending() != 0; ++turn) (void)r.bus.pump_pending();
        r.bus.remove_observer(stop);
        hand->next = {};
        return seen;
    }
    void drag(std::int64_t from_row = 1, bool batched = true, bool outside = false) {
        auto press = button_at(source, from_row, true);
        auto release = button_at(info, 0, false);
        if (outside) { release.x = 179; release.y = 57; }
        auto move = release; move.kind = "PointerMoved";
        move.dx = release.x - press.x; move.dy = release.y - press.y;
        if (batched) batch({press, move, release});
        else { event(press); event(move); event(release); }
    }
    loom::Bytes pair(std::int64_t count) {
        auto shape = loom::SchemaBuilder("story.RuntimeItem", 1).field("count", loom::Kind::Int).build();
        loom::Value value(shape); value.set("count", loom::Cell::integer(count));
        const auto encoded = inv::encode_pair(value, {});
        return {encoded.begin(), encoded.end()};
    }
    void append(std::int64_t count, std::string label) {
        r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryAdd{pair(count), label})));
        r.bus.drain_until_idle();
    }
    std::vector<inv::DecodedPair> saved_entries() {
        std::vector<inv::DecodedPair> out;
        const auto state = r.bus.weave(r.bus.role_holder(inv::kInventoryRole))->snapshot();
        for (const auto& cell : state.get("entries")->as_list()) {
            const auto& bytes = cell.as_message()->get("pair")->as_bytes();
            out.push_back(inv::decode_pair({reinterpret_cast<const char*>(bytes.data()), bytes.size()}));
        }
        return out;
    }
    void pump_physical() {
        r.bus.send_to_role(input::kInputRole, loom::Message(loom::to_value(input::PumpInput{})));
        r.bus.drain_until_idle();
    }
    slots::InventoryViews layout() {
        return loom::from_value<slots::InventoryViews>(*r.bus.weave(r.bus.role_holder(slots::kRole))->snapshot().get("layout")->as_message());
    }
    void change(slots::InventoryViewEdit op) {
        r.bus.send_to_role(slots::kRole,loom::Message(loom::to_value(op))); r.bus.drain_until_idle();
    }
    inv::InventorySummary entry(const std::string& label) {
        act([](loom::Mail& m){m.send_to_role(inv::kInventoryRole,inv::InventoryList{});});
        for(const auto& e:hand->listing.entries) if(e.label==label) return e;
        FAIL(("No entry named " + label)); return {};
    }
    void bind(const inv::InventoryReference& ref, std::string target=inv::kInventoryRole, std::int64_t key=30) {
        slots::InventoryViewEdit op; op.operation="bind"; op.entry=ref; op.text=target; op.scancode=key; op.modifiers=input::mod::kAlt; change(op);
        op={}; op.operation="enable"; op.entry=ref; op.enabled=true; change(op);
    }
    void context(std::string view, bool active) {
        slots::InventoryViewEdit op; op.operation="context"; op.view=view; op.enabled=active; change(op);
    }
    std::string create(std::string kind,const inv::InventoryReference& ref={}) {
        slots::InventoryViewEdit op; op.operation="create"; op.text=kind; op.entry=ref; change(op);
        return layout().views.back().id;
    }
    void menu(std::int64_t kind,std::int64_t row,int index) {
        click(kind,row,3); REQUIRE(r.session().presented.open);
        for(int n=0;n<index;++n) key(input::scan::kDown);
        key(input::scan::kReturn);
    }
    // ---- folders ---------------------------------------------------------------------------
    inv::v2::InventoryListed folders() {
        act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::v2::InventoryList{}); });
        return hand->organized;
    }
    std::string folder_id(const std::string& name) {
        for (const auto& f : folders().folders) if (f.name == name) return f.folder.folder;
        FAIL(("No folder named " + name)); return {};
    }
    std::string member_of(const std::string& label) {
        for (const auto& e : folders().entries) if (e.label == label) return e.folder;
        FAIL(("No entry named " + label)); return {};
    }
    /// Arrange a folder as the test root (setup, not a maker's gesture).
    std::string make_folder(const std::string& parent, const std::string& name) {
        const auto owner = folders().owner;
        r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryFolderCreate{{owner, parent}, name})));
        r.bus.drain_until_idle();
        return folder_id(name);
    }
    /// The painted row whose text contains `text`, or -1.
    std::int64_t row_of(std::int64_t kind, const std::string& text) {
        const auto rows = pane_rows(r, kind);
        for (std::size_t i = 0; i < rows.size(); ++i) if (rows[i].find(text) != std::string::npos) return static_cast<std::int64_t>(i);
        return -1;
    }
    std::int64_t column_of(std::int64_t kind, std::int64_t row, const std::string& text) {
        const auto rows = pane_rows(r, kind);
        REQUIRE(row >= 0); REQUIRE(static_cast<std::size_t>(row) < rows.size());
        const auto at = rows[static_cast<std::size_t>(row)].find(text);
        REQUIRE_MESSAGE(at != std::string::npos, text << " is not on row " << row << ": " << rows[static_cast<std::size_t>(row)]);
        return static_cast<std::int64_t>(at);
    }
    input::InjectedEvent button_at(std::int64_t kind, std::int64_t row, std::int64_t column, bool down, std::int64_t button = 1) {
        auto e = button_at(kind, row, down);
        e.x = external_body_rect(r.session(), kind).x + column; e.button = button;
        return e;
    }
    void click_at(std::int64_t kind, std::int64_t row, std::int64_t column, std::int64_t button = 1) {
        event(button_at(kind, row, column, true, button)); event(button_at(kind, row, column, false, button));
    }
    /// Drag between two painted places in one batch (press, motion, release).
    void drag_to(std::int64_t from_kind, std::int64_t from_row, std::int64_t to_kind, std::int64_t to_row, std::int64_t to_column = 1) {
        auto press = button_at(from_kind, from_row, true), release = button_at(to_kind, to_row, to_column, false);
        auto move = release; move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
        batch({press, move, release});
    }
    void physical_click(std::int64_t kind) {
        const auto rect = external_body_rect(r.session(), kind);
        const auto y = rect.y + surface::kTuiCanvasTopRow +
            external_title_rows(r.session().panels, kind, r.session().pane_titles);
        physical->push_back(input::PointerButton{1, true, rect.x + 1, y, input::space::kCells, 0});
        physical->push_back(input::PointerButton{1, false, rect.x + 1, y, input::space::kCells, 0});
        pump_physical();
    }
};
} // namespace inventory_story
#endif
