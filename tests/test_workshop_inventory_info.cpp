// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "workshop_support.hpp"
#include "inventory/codec.hpp"
#include "message-draft/transfer.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory-pane/slots.hpp"
#include "input/input_weave.hpp"
#include <zen/host/grant_wiring.hpp>
namespace slots = zengine::inventory_pane;

namespace {
namespace inv = zengine::inventory;
struct InventoryHandState { ZEN_SHAPE(InventoryHandState, 1); };
struct InventoryHandDo { ZEN_SHAPE(InventoryHandDo, 1); };
class InventoryHand : public loom::WeaveBase<InventoryHand, InventoryHandState,
    loom::Accept<InventoryHandDo, input::InputSessionOpened, input::InputInjected, PaneView, inv::InventoryListed, inv::InventoryEntry, loom::Refused>,
    loom::Emit<intro::LoadedSelected, input::InputSessionRequested, input::InjectInput>> {
public:
    std::function<void(loom::Mail&)> next;
    std::int64_t session = 0;
    inv::InventoryListed listing;
    void on(const inv::InventoryListed& v, loom::Mail&) { listing = v; }
    std::vector<PaneView> views;
    std::vector<std::string> refusals;
    bool expect_refusal = false;
    void on(const PaneView& v, loom::Mail&) { views.push_back(v); }
    std::vector<inv::InventoryEntry> entries;
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
        actor_grant.allow_to_role(PaneViewRequested::zen_name, 1, "zengine.workshop");
        if (permissions & 1) actor_grant.allow_to_role(inv::InventoryLocate::zen_name, 1, inv::kInventoryRole);
        if (permissions & 2) actor_grant.allow_to_role(inv::InventoryRead::zen_name, 1, inv::kInventoryRole);
        if (permissions & 4) actor_grant.allow_to_role(inv::InventoryWrite::zen_name, 1, inv::kInventoryRole);
        if (permissions & 8) actor_grant.allow_to_role(inv::InventoryAdd::zen_name, 1, inv::kInventoryRole);
        if (permissions & 16) actor_grant.allow_to_role(inv::InventoryRename::zen_name, 1, inv::kInventoryRole);
        if (permissions & 32) actor_grant.allow_to_role(inv::InventoryRemove::zen_name, 1, inv::kInventoryRole);
        if (permissions & 64) actor_grant.allow_to_role(PaneValueCarryRequested::zen_name, 1, "zengine.workshop");
        if (permissions & 128) actor_grant.allow_to_role(zengine::inventory_pane::InventoryViewEdit::zen_name, 1, "zengine.inventory-pane");
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
    void physical_click(std::int64_t kind) {
        const auto rect = external_body_rect(r.session(), kind);
        const auto y = rect.y + surface::kTuiCanvasTopRow +
            external_title_rows(r.session().panels, kind, r.session().pane_titles);
        physical->push_back(input::PointerButton{1, true, rect.x + 1, y, input::space::kCells, 0});
        physical->push_back(input::PointerButton{1, false, rect.x + 1, y, input::space::kCells, 0});
        pump_physical();
    }
};
}

TEST_CASE("portable slots: arrangement preserves ownership, reorders strips and returns a displaced single") {
    InventoryStory s(191,true); s.append(1,"A"); s.append(2,"B");
    const auto a=s.entry("A").reference,b=s.entry("B").reference;
    const auto row=s.create("row",a),box=s.create("single",b),column=s.create("column");
    slots::InventoryViewEdit op; op.operation="move"; op.entry=b; op.view=row; op.before=a; s.change(op);
    auto state=s.layout(); REQUIRE(state.views[0].entries.size()==2);
    CHECK(slots::same(state.views[0].entries[0],b)); CHECK(state.views[1].entries.empty());
    op.entry=a; op.view=box; op.before={}; s.change(op);
    op.entry=b; s.change(op);
    state=s.layout(); CHECK(slots::placed(state,a)=="inventory"); CHECK(slots::placed(state,b)==box);
    op.view=column; s.change(op); CHECK(slots::placed(s.layout(),b)==column);
    CHECK(s.saved_entries().size()==2);
    CHECK(s.entry("A").revision==1); CHECK(s.entry("B").revision==1);
}

TEST_CASE("portable slots: duplicate retains target and key but starts off with independent data") {
    InventoryStory s(191,true); s.append(1,"Button");
    const auto original=s.entry("Button"); s.bind(original.reference); s.context("inventory",true);
    s.menu(s.source,2,3);
    const auto copy=s.entry("Button 2");
    CHECK_FALSE(slots::same(original.reference,copy.reference));
    const auto state=s.layout(); REQUIRE(state.bindings.size()==2);
    CHECK(state.bindings[0].enabled); CHECK_FALSE(state.bindings[1].enabled);
    CHECK(state.bindings[1].scancode==state.bindings[0].scancode);
    CHECK(state.bindings[1].target==state.bindings[0].target);
    CHECK(state.bindings[1].serial!=state.bindings[0].serial);
    s.menu(s.source,3,4); // duplicate the copy, with an immediate name editor
    s.key(input::scan::kA,input::mod::kCtrl); s.text("My alternate"); s.key(input::scan::kReturn);
    const auto named=s.entry("My alternate"); CHECK_FALSE(slots::same(named.reference,copy.reference));
    const auto named_state=s.layout(); REQUIRE(named_state.bindings.size()==3); CHECK_FALSE(named_state.bindings.back().enabled);
    s.r.bus.send_to_role(inv::kInventoryRole,loom::Message(loom::to_value(inv::InventoryWrite{copy.reference,copy.revision,s.pair(8)})));
    s.r.bus.drain_until_idle();
    const auto values=s.saved_entries(); REQUIRE(values.size()==3);
    CHECK(values[0].item.get("count")->as_int()==1); CHECK(values[1].item.get("count")->as_int()==8);
    CHECK(values[2].item.get("count")->as_int()==1);
}

TEST_CASE("portable slots: active context collision refuses atomically and keeps desktop defaults") {
    InventoryStory s(191,true); s.append(1,"A"); s.append(2,"B");
    const auto a=s.entry("A").reference,b=s.entry("B").reference;
    const auto left=s.create("row",a),right=s.create("column",b);
    s.bind(a); s.bind(b); s.context(left,true); s.context(right,true);
    auto state=s.layout(); CHECK(state.views[0].active); CHECK_FALSE(state.views[1].active);
    CHECK_MESSAGE(s.shown(s.source).find("refused")!=std::string::npos,s.shown(s.source));
    s.context(left,false); s.context(right,true);
    state=s.layout(); CHECK_FALSE(state.views[0].active); CHECK(state.views[1].active);
    slots::InventoryViewEdit op; op.operation="bind"; op.entry=b; op.text=inv::kInventoryRole;
    op.scancode=input::scan::kT; op.modifiers=input::mod::kCtrl; s.change(op);
    CHECK(slots::binding(s.layout(),b)->scancode==30);
    CHECK(s.r.session().keymap.app_row_of_id("desktop.terminal")!=nullptr);
}

TEST_CASE("portable slots: invoking a configured hotkey checks current actor authority for the command") {
    for(const bool allowed:{false,true}) {
        InventoryStory s(allowed?191:175,true,allowed); s.append(1,"victim"); const auto victim=s.entry("victim");
        const auto bytes=inv::encode_pair(loom::to_value(inv::InventoryRename{victim.reference,victim.revision,"executed"}),{});
        s.r.bus.send_to_role(inv::kInventoryRole,loom::Message(loom::to_value(inv::InventoryAdd{loom::Bytes(bytes.begin(),bytes.end()),"command"})));
        s.r.bus.drain_until_idle(); s.bind(s.entry("command").reference);
        s.key(30,input::mod::kAlt); CHECK(s.entry("victim").revision==1); // main defaults inactive
        s.context("inventory",true);
        REQUIRE(s.layout().inventory_active);
        if (allowed) {
            s.r.bus.office_send_to_role_as(s.r.bus.role_holder(slots::kRole), slots::kRole,
                kWorkshopProvider, loom::Message(loom::to_value(PaneCloseRequested{slots::kRole,"inventory"})));
            s.r.bus.drain_until_idle(); REQUIRE_FALSE(s.r.session().panels.has(s.source));
        }
        s.r.pick({"zengine.info","info"}); // current actor survives the Desktop hop, including a hidden source pane
        s.key(30,input::mod::kAlt);
        CHECK_MESSAGE(s.entry(allowed?"executed":"victim").revision==(allowed?2:1),s.shown(s.source));
        if(!allowed) CHECK_MESSAGE(s.shown(s.source).find("no authority")!=std::string::npos,s.shown(s.source));
    }
}

TEST_CASE("portable slots: invalid target and incomplete preset are attributable refusals") {
    InventoryStory s(191,true);
    const auto wrong=inv::encode_pair(loom::to_value(inv::InventoryList{}),{});
    s.r.bus.send_to_role(inv::kInventoryRole,loom::Message(loom::to_value(inv::InventoryAdd{loom::Bytes(wrong.begin(),wrong.end()),"wrong target"})));
    s.r.bus.drain_until_idle(); s.bind(s.entry("wrong target").reference,input::kInputRole); s.context("inventory",true);
    // Physical maker authority passes the permission check; the actual destination gate refuses.
    s.physical->push_back(input::KeyPressed{30,"1",input::mod::kAlt}); s.pump_physical();
    CHECK_MESSAGE(s.shown(s.source).find("Command refused")!=std::string::npos,s.shown(s.source));
    auto shape=loom::schema_of<inv::InventoryRename>(); zengine::message_draft::Draft draft(shape);
    const auto encoded=inv::encode_pair(zengine::message_draft::store_draft("partial",draft),{});
    s.r.bus.send_to_role(inv::kInventoryRole,loom::Message(loom::to_value(inv::InventoryAdd{loom::Bytes(encoded.begin(),encoded.end()),"partial"})));
    s.r.bus.drain_until_idle(); s.bind(s.entry("partial").reference,inv::kInventoryRole,31);
    s.physical->push_back(input::KeyPressed{31,"2",input::mod::kAlt}); s.pump_physical();
    CHECK_MESSAGE(s.shown(s.source).find("incomplete")!=std::string::npos,s.shown(s.source));
}

TEST_CASE("portable slots: one batched drag moves into a row and a forged transfer cannot move it") {
    InventoryStory s(191,true); s.append(1,"A"); const auto a=s.entry("A").reference;
    const auto row=s.create("row"); const auto kind=s.r.session().panels.runtime.find(slots::kRole,row)->kind;
    for(auto& p:s.r.session().setup.active.panes) if(p.ref.pane==row) {
        p.place={pane_unit::kSubcells,2*surface::kCellSubs,34*surface::kCellSubs};
        p.width={pane_unit::kSubcells,72*surface::kCellSubs}; p.height={pane_unit::kSubcells,10*surface::kCellSubs};
    }
    s.r.extent(180,60);
    auto press=s.button_at(s.source,2,true),release=s.button_at(kind,1,false),move=release;
    move.kind="PointerMoved"; move.dx=release.x-press.x; move.dy=release.y-press.y;
    s.batch({press,move,release});
    CHECK_MESSAGE(slots::placed(s.layout(),a)==row,(s.shown(s.source)+s.shown(kind)));
    CHECK(s.saved_entries().size()==1);
    const auto before=s.layout();
    const auto picture=s.r.session().panels.external_pane(s.source)->picture;
    s.r.bus.office_send_to_role_as(s.r.bus.role_holder(kWorkshopProvider),kWorkshopProvider,slots::kRole,
        loom::Message(loom::to_value(v2::PaneValueDrop{"inventory",s.pair(88),0,0,picture,slots::kRole,row,"made up"})));
    s.r.bus.drain_until_idle(); CHECK(slots::placed(s.layout(),a)==slots::placed(before,a));
    CHECK_MESSAGE(s.shown(s.source).find("expired")!=std::string::npos,s.shown(s.source));
}

TEST_CASE("portable slots: view changes require their own authority through a right-click") {
    InventoryStory s(63,true);
    s.menu(s.source,1,9); // Pop out single box
    CHECK(s.layout().views.empty());
    CHECK_MESSAGE(s.shown(s.source).find("no authority")!=std::string::npos,s.shown(s.source));
}

TEST_CASE("inventory Info: authorized input carries an entry edits it and reads a fresh copy") {
    InventoryStory t;
    t.acquire();
    REQUIRE_MESSAGE(t.shown(t.source).find("Click a receiving pane") != std::string::npos, t.shown(t.source));
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
    t.click(t.source, 1, 3);
    REQUIRE(t.r.session().presented.open);
    t.key(input::scan::kReturn);
    REQUIRE_MESSAGE(t.shown(t.source).find("Click a receiving pane") != std::string::npos, t.shown(t.source));
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
    CHECK(t.shown(t.source).find("Click a receiving pane") == std::string::npos);
    t.r.key(input::scan::kReturn);
    CHECK_MESSAGE(t.shown(t.source).find("attributed input gesture") != std::string::npos, t.shown(t.source));
}

TEST_CASE("inventory Info: a departed input actor cannot leave the maker trapped carrying its reference") {
    InventoryStory t;
    t.acquire();
    t.r.bus.unregister_weave(t.hand_id).reset();
    t.hand = nullptr;
    t.physical_click(t.info);
    REQUIRE(t.r.session().panels.keyboard == t.info);
    CHECK(t.shown(t.info).find("story.RuntimeItem") == std::string::npos);
    t.physical_click(t.source);
    t.physical->push_back(input::KeyPressed{input::scan::kReturn, "", input::mod::kCtrl});
    t.pump_physical();
    REQUIRE_MESSAGE(t.shown(t.source).find("Click a receiving pane") != std::string::npos, t.shown(t.source));
    t.physical_click(t.info);
    CHECK_MESSAGE(t.shown(t.info).find("story.RuntimeItem") != std::string::npos, t.shown(t.info));
}

TEST_CASE("inventory Info: a receiver leaving before delivery reports the failed placement") {
    InventoryStory t;
    t.acquire();
    bool removed = false;
    RemoveObserver watch{t.r.bus, t.r.bus.add_observer([&](const loom::BusEvent& event) {
        if (!removed && event.kind == loom::EventKind::Delivered &&
            event.schema_name == input::AttributedInput::zen_name && event.target == t.r.workshop_id) {
            // Delivered is observed after Workshop's handler queued the drop, before the
            // bus dispatches it. Remove the destination at that exact host boundary.
            removed = t.r.kernel.unload_role("zengine.info");
        }
    })};
    t.click(t.info);
    REQUIRE(removed);
    REQUIRE(t.r.load_refusals.empty());
    CHECK_MESSAGE(t.r.last_notice().find("Reference not delivered to zengine.info") != std::string::npos,
                  t.r.last_notice());
    CHECK(t.stored().item.get("count")->as_int() == 7);
}

TEST_CASE("inventory collection UI: a single batched drag copies into Info and saves a separate entry") {
    InventoryStory t;
    t.drag();
    REQUIRE_MESSAGE(t.shown(t.info).find("COPY story.RuntimeItem") != std::string::npos, (t.shown(t.info) + t.r.last_notice() + t.trace));
    t.edit("42");
    CHECK(t.stored().item.get("count")->as_int() == 7);
    t.key(input::scan::kS, input::mod::kCtrl);
    auto entries = t.saved_entries();
    REQUIRE_MESSAGE(entries.size() == 1, (t.shown(t.info) + t.trace));
    CHECK(entries[0].item.get("count")->as_int() == 42);
    CHECK(t.stored().item.get("count")->as_int() == 7);
    CHECK(entries[0].metadata[0].get("source")->as_text() == "capture observation");
    CHECK(t.shown(t.info).find("LIVE ENTRY") != std::string::npos);
    t.edit("43"); t.key(input::scan::kS, input::mod::kCtrl);
    entries = t.saved_entries(); REQUIRE(entries.size() == 1);
    CHECK(entries[0].item.get("count")->as_int() == 43);
    CHECK(t.shown(t.source).find("INVENTORY 2") != std::string::npos);
}

TEST_CASE("inventory collection UI: a slow drag and a simple click have distinct outcomes") {
    InventoryStory t;
    t.click(t.source, 1);
    t.click(t.info);
    CHECK(t.shown(t.info).find("story.RuntimeItem") == std::string::npos);
    t.drag(1, false);
    CHECK_MESSAGE(t.shown(t.info).find("COPY story.RuntimeItem") != std::string::npos, t.shown(t.info));
    t.edit("99");
    t.key(input::scan::kR, input::mod::kCtrl);
    CHECK(t.shown(t.info).find("Unsaved draft") != std::string::npos);
    t.key(input::scan::kR, input::mod::kCtrl);
    CHECK(t.shown(t.info).find("count: 7") != std::string::npos);
    CHECK(t.saved_entries().empty());
}

TEST_CASE("inventory collection UI: dragging needs read authority and saving a copy needs add authority") {
    SUBCASE("input only") {
        InventoryStory t(0); t.drag();
        CHECK_MESSAGE(t.shown(t.source).find("no authority") != std::string::npos, t.shown(t.source));
        CHECK(t.shown(t.info).find("story.RuntimeItem") == std::string::npos);
    }
    SUBCASE("read only") {
        InventoryStory t(2); t.drag(); t.edit("99");
        t.key(input::scan::kS, input::mod::kCtrl);
        CHECK(t.saved_entries().empty());
        CHECK(t.shown(t.info).find("no authority") != std::string::npos);
        CHECK(t.shown(t.info).find("count: 99") != std::string::npos);
    }
}

TEST_CASE("inventory collection UI: cancelled drags leave no held item or accidental receiver edit") {
    SUBCASE("outside release") {
        InventoryStory t; t.drag(1, true, true); t.click(t.info);
        CHECK(t.shown(t.info).find("story.RuntimeItem") == std::string::npos);
        t.drag(); CHECK(t.shown(t.info).find("COPY") != std::string::npos);
    }
    SUBCASE("Escape before release") {
        InventoryStory t;
        t.event(t.button_at(t.source, 1, true));
        t.key(input::scan::kEscape);
        t.event(t.button_at(t.info, 0, false)); t.click(t.info);
        CHECK(t.shown(t.info).find("story.RuntimeItem") == std::string::npos);
    }
    SUBCASE("newer input overtakes an acquisition") {
        InventoryStory t;
        auto press=t.button_at(t.source, 1, true), release=t.button_at(t.info, 0, false);
        auto move=release; move.kind="PointerMoved";
        input::InjectedEvent key; key.kind="KeyPressed"; key.scancode=input::scan::kEscape;
        t.batch({press, move, release, key});
        CHECK(t.shown(t.info).find("story.RuntimeItem") == std::string::npos);
        t.click(t.info); CHECK(t.saved_entries().empty());
    }
    SUBCASE("another actor cannot release the drag") {
        InventoryStory t;
        t.event(t.button_at(t.source, 1, true));
        auto release = t.button_at(t.info, 0, false);
        auto move = release; move.kind = "PointerMoved";
        t.event(move);
        t.physical->push_back(input::PointerButton{1, false, release.x, release.y, release.space, 0});
        t.pump_physical();
        CHECK(t.shown(t.info).find("story.RuntimeItem") == std::string::npos);
        t.event(release);
        CHECK(t.shown(t.info).find("COPY story.RuntimeItem") != std::string::npos);
    }
}

TEST_CASE("inventory collection UI: sorted rows drag by identity and renaming keeps the selected entry") {
    InventoryStory t;
    t.append(10, "Zulu"); t.append(20, "Alpha");
    t.click(t.source); t.key(input::scan::kS, input::mod::kCtrl);
    REQUIRE(t.shown(t.source).find("sort: name") != std::string::npos);
    t.drag(1);
    REQUIRE_MESSAGE(t.shown(t.info).find("count: 20") != std::string::npos, t.shown(t.info));
    t.click(t.source); t.key(input::scan::kN, input::mod::kCtrl);
    t.key(input::scan::kA, input::mod::kCtrl); t.text("Bravo"); t.key(input::scan::kReturn);
    CHECK_MESSAGE(t.shown(t.source).find("Bravo") != std::string::npos, t.shown(t.source));
    t.key(input::scan::kDelete); t.key(input::scan::kDelete);
    auto entries=t.saved_entries(); REQUIRE(entries.size() == 1);
    CHECK(entries[0].item.get("count")->as_int() == 10);
    CHECK(t.stored().item.get("count")->as_int() == 7);
}

TEST_CASE("inventory collection UI: a value shaped like a reference stays a value on the copy route") {
    InventoryStory t;
    const auto encoded=inv::encode_pair(loom::to_value(inv::InventoryReference{"data-owner","data-entry"}), {});
    t.r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventorySet{
        loom::Bytes(encoded.begin(), encoded.end())})));
    t.r.bus.drain_until_idle(); t.drag();
    CHECK_MESSAGE(t.shown(t.info).find("COPY InventoryReference") != std::string::npos, t.shown(t.info));
    CHECK(t.shown(t.info).find("data-owner") != std::string::npos);
}

TEST_CASE("inventory collection UI: another drag cannot replace an unsaved Info draft") {
    InventoryStory t;
    t.append(20, "Second");
    t.drag(); t.edit("99");
    t.drag(2);
    CHECK(t.shown(t.info).find("count: 99") != std::string::npos);
    CHECK(t.shown(t.info).find("before opening another value") != std::string::npos);
    t.key(input::scan::kS, input::mod::kCtrl);
    const auto entries = t.saved_entries();
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].item.get("count")->as_int() == 20);
    CHECK(entries[1].item.get("count")->as_int() == 99);
    CHECK(t.stored().item.get("count")->as_int() == 7);
}

TEST_CASE("inventory collection UI: dragging an owned entry back into Inventory moves without copying") {
    InventoryStory t;
    auto press = t.button_at(t.source, 1, true), release = t.button_at(t.source, 0, false);
    auto move = release; move.kind = "PointerMoved";
    t.batch({press, move, release});
    CHECK_MESSAGE(t.saved_entries().empty(), t.shown(t.source));
    CHECK(t.stored().item.get("count")->as_int() == 7);
    t.key(input::scan::kReturn); t.click(t.source, 0);
    REQUIRE_MESSAGE(t.saved_entries().size() == 1, t.shown(t.source));
    CHECK(t.saved_entries()[0].item.get("count")->as_int() == 7);
}

TEST_CASE("Compose drops are data and submission spends the input actor's exact authority") {
    for (const bool allowed : {false, true}) {
        InventoryStory s(allowed ? 63 : 47, true); // withhold InventoryRename only
        s.append(7, "source");
        auto& r = s.r;
        REQUIRE(r.load_refusals.empty());
        r.pick({"zengine.info", "info"}); // give Compose the right-hand area
        r.pick(composer_ref());
        const auto compose_kind = r.session().panels.runtime.find(kComposerOffice, "compose")->kind;
        for (auto& p : r.session().setup.active.panes) if (p.ref.provider == kComposerOffice) {
            p.place = {pane_unit::kSubcells, 85*surface::kCellSubs, 4*surface::kCellSubs};
            p.width = {pane_unit::kSubcells, 80*surface::kCellSubs};
            p.height = {pane_unit::kSubcells, 24*surface::kCellSubs};
        }
        r.extent(180, 60);
        auto selector = std::make_unique<InventoryHand>(); auto* raw = selector.get();
        loom::Grant grant; grant.allow_to_any(intro::LoadedSelected::zen_name, 1);
        const auto selector_id = r.bus.register_weave(std::move(selector), grant, kIntroOffice);
        raw->zen_set_self(selector_id);
        raw->next = [](loom::Mail& m) { m.as_role(kIntroOffice).publish(
            intro::LoadedSelected{"loaded", "zengine-inventory", inv::kInventoryRole}); };
        r.bus.send(selector_id, loom::Message(loom::to_value(InventoryHandDo{})));
        r.bus.drain_until_idle();
        REQUIRE_MESSAGE(s.shown(compose_kind).find("InventoryRename") != std::string::npos, s.shown(compose_kind));
        s.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryList{}); });
        const auto source_entry = std::find_if(s.hand->listing.entries.begin(), s.hand->listing.entries.end(),
            [](const auto& e) { return e.label == "source"; });
        REQUIRE(source_entry != s.hand->listing.entries.end());
        const auto reference = source_entry->reference;
        const auto bytes = inv::encode_pair(loom::to_value(inv::InventoryRename{reference, 1, "renamed"}), {});
        r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(
            inv::InventoryAdd{loom::Bytes(bytes.begin(), bytes.end()), "rename command"})));
        r.bus.drain_until_idle();
        const auto untouched = s.shown(compose_kind);
        const auto current_picture = r.session().panels.external_pane(compose_kind)->picture;
        const PaneValueDrop forged{"compose", loom::Bytes(bytes.begin(), bytes.end()), 0, 0, current_picture};
        r.bus.send_to_role(kComposerOffice, loom::Message(loom::to_value(forged)));
        r.bus.drain_until_idle();
        CHECK(s.shown(compose_kind) == untouched);
        auto stale = forged; stale.picture = current_picture - 1;
        r.bus.office_send_to_role_as(r.bus.role_holder(kWorkshopProvider), kWorkshopProvider,
            kComposerOffice, loom::Message(loom::to_value(stale)));
        r.bus.drain_until_idle();
        CHECK(s.shown(compose_kind).find("picture changed") != std::string::npos);
        CHECK(s.shown(compose_kind).find("Copied data") == std::string::npos);
        s.info = compose_kind;
        std::int64_t command_row = -1;
        const auto rows = pane_rows(r, s.source);
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].find("rename command") != std::string::npos) command_row = static_cast<std::int64_t>(i);
        REQUIRE(command_row >= 0);
        s.drag(command_row);
        REQUIRE_MESSAGE(s.shown(compose_kind).find("Copied data into form") != std::string::npos, s.shown(compose_kind));
        auto current = [&] { return r.bus.weave(r.bus.role_holder(inv::kInventoryRole))->snapshot(); };
        CHECK(current().get("entries")->as_list()[0].as_message()->get("label")->as_text() == "source");
        s.key(input::scan::kReturn, input::mod::kCtrl);
        INFO(s.shown(compose_kind));
        CHECK(current().get("entries")->as_list()[0].as_message()->get("label")->as_text() ==
              (allowed ? "renamed" : "source"));
        CHECK(current().get("entries")->as_list()[1].as_message()->get("revision")->as_int() == 1);
        if (!allowed) CHECK(s.shown(compose_kind).find("no authority") != std::string::npos);
    }
}

TEST_CASE("pane view reports the painter's rows and refuses hidden content") {
    InventoryStory s;
    s.append(12, "visible item");
    auto query = [&] { s.act([](loom::Mail& m) { m.send_to_role("zengine.workshop",
        PaneViewRequested{"zengine.inventory-pane", "inventory"}); }); };
    query(); REQUIRE(s.hand->views.size() == 1);
    for (const auto& row : s.hand->views.back().rows) {
        const auto at = external_press_at(s.r.session().panels, s.r.session().setup.active,
            screen_of(s.r.session()), s.source, s.r.session().pane_titles, row.space, row.x, row.y);
        CHECK(at.named); CHECK(at.row == row.row);
    }
    s.hand->expect_refusal = true;
    s.r.session().context.open = true;
    query();
    CHECK(s.hand->views.size() == 1);
    REQUIRE(s.hand->refusals.size() == 1);
    CHECK(s.hand->refusals.back().find("covered") != std::string::npos);
    s.r.session().context.open = false;
    for (auto& p : s.r.session().setup.active.panes) if (p.ref.provider == "zengine.inventory-pane")
        p.place.y = 55 * surface::kCellSubs;
    s.r.extent(180, 60);
    query();
    REQUIRE(s.hand->refusals.size() == 2);
    CHECK(s.hand->refusals.back().find("outside") != std::string::npos);
}

TEST_CASE("presets are independent partial data until filled and explicitly authorized by the current actor") {
    namespace md = zengine::message_draft;
    for (bool allowed : {false, true}) {
        InventoryStory s(allowed ? 127 : 111, true);
        auto& r = s.r;
        r.pick(composer_ref());
        const auto compose = r.session().panels.runtime.find(kComposerOffice, "compose")->kind;
        REQUIRE(r.session().keymap.app_row_of_id("desktop.deselect") != nullptr);
        REQUIRE(r.session().keymap.app_row_of_id("desktop.panes") != nullptr);
        for (auto& p : r.session().setup.active.panes) if (p.ref.provider == kComposerOffice) {
            p.place = {pane_unit::kSubcells, 85*surface::kCellSubs, 32*surface::kCellSubs};
            p.width = {pane_unit::kSubcells, 80*surface::kCellSubs};
            p.height = {pane_unit::kSubcells, 24*surface::kCellSubs};
        }
        r.extent(180, 60);
        auto selector = std::make_unique<InventoryHand>(); auto* raw = selector.get();
        loom::Grant grant; grant.allow_to_any(intro::LoadedSelected::zen_name, 1);
        const auto id = r.bus.register_weave(std::move(selector), grant, kIntroOffice); raw->zen_set_self(id);
        raw->next = [](loom::Mail& m) { m.as_role(kIntroOffice).publish(
            intro::LoadedSelected{"loaded", "zengine-inventory", inv::kInventoryRole}); };
        r.bus.send(id, loom::Message(loom::to_value(InventoryHandDo{}))); r.bus.drain_until_idle();
        auto row = [&](std::int64_t kind, const std::string& text) {
            const auto rows = pane_rows(r, kind);
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (rows[i].find(text) != std::string::npos) return static_cast<std::int64_t>(i);
            FAIL_CHECK(("missing row: " + text + " in " + s.shown(kind))); return std::int64_t(-1);
        };
        auto drag = [&](std::int64_t from, std::int64_t into, std::int64_t target_row = 0) {
            auto press = s.button_at(s.source, from, true);
            auto release = s.button_at(into, target_row, false);
            auto move = release; move.kind = "PointerMoved";
            s.batch({press, move, release});
        };
        s.append(1, "source");
        s.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryList{}); });
        const auto source = std::find_if(s.hand->listing.entries.begin(), s.hand->listing.entries.end(),
            [](const auto& e) { return e.label == "source"; });
        REQUIRE(source != s.hand->listing.entries.end());
        const auto ref = source->reference;
        const auto encoded = inv::encode_pair(loom::to_value(inv::InventoryRename{ref, 1, "renamed"}), {});
        r.bus.send_to_role(inv::kInventoryRole, loom::Message(loom::to_value(inv::InventoryAdd{
            loom::Bytes(encoded.begin(), encoded.end()), "complete command"}))); r.bus.drain_until_idle();
        drag(row(s.source, "complete command"), s.info);
        REQUIRE(r.session().keymap.app_row_of_id("desktop.deselect") != nullptr);
        s.key(input::scan::kB, input::mod::kCtrl);
        REQUIRE_MESSAGE(s.shown(s.info).find("PRESET COPY") != std::string::npos, s.shown(s.info));
        s.click(s.info, row(s.info, "revision: 1")); s.key(input::scan::kU, input::mod::kCtrl);
        CHECK(s.shown(s.info).find("revision: absent (required)") != std::string::npos);
        s.key(input::scan::kS, input::mod::kCtrl);
        const auto saved = s.saved_entries(); REQUIRE(saved.size() == 3);
        CHECK(saved[1].item.get("revision")->as_int() == 1);
        const auto preset = md::read_draft(saved[2].item);
        CHECK(preset.draft.get({"revision"}) == nullptr);
        CHECK_FALSE(preset.draft.admit());
        drag(row(s.source, "preset"), compose);
        REQUIRE_MESSAGE(s.shown(compose).find("Copied data into form") != std::string::npos, s.shown(compose));
        s.key(input::scan::kReturn, input::mod::kCtrl);
        CHECK(s.shown(compose).find("still needed: revision") != std::string::npos);
        drag(row(s.source, "source"), s.info);
        REQUIRE_MESSAGE(s.shown(s.info).find("count: 1") != std::string::npos, s.shown(s.info));
        s.key(input::scan::kG, input::mod::kCtrl);
        REQUIRE_MESSAGE(s.shown(s.info).find("Carrying field copy") != std::string::npos, s.shown(s.info));
        s.click(compose, row(compose, "revision"));
        REQUIRE_MESSAGE(s.shown(compose).find("Copied data into form") != std::string::npos, s.shown(compose));
        auto label = [&] { return r.bus.weave(r.bus.role_holder(inv::kInventoryRole))->snapshot()
            .get("entries")->as_list().front().as_message()->get("label")->as_text(); };
        CHECK(label() == "source");
        s.key(input::scan::kReturn, input::mod::kCtrl);
        CHECK_MESSAGE(label() == (allowed ? "renamed" : "source"), s.shown(compose));
        if (!allowed) CHECK(s.shown(compose).find("no authority") != std::string::npos);
        CHECK(s.saved_entries()[1].item.get("revision")->as_int() == 1);
        CHECK(md::read_draft(s.saved_entries()[2].item).draft.get({"revision"}) == nullptr);
    }
}

TEST_CASE("Info field acquisition needs its own current actor permission and does not alter stored data") {
    InventoryStory s(63);
    s.acquire(); s.place();
    s.key(input::scan::kG, input::mod::kCtrl);
    CHECK_MESSAGE(s.shown(s.info).find("no authority") != std::string::npos, s.shown(s.info));
    CHECK(s.stored().item.get("count")->as_int() == 7);
    s.key(input::scan::kB, input::mod::kCtrl); s.key(input::scan::kU, input::mod::kCtrl);
    s.key(input::scan::kG, input::mod::kCtrl);
    CHECK(s.shown(s.info).find("present field") != std::string::npos);
}
