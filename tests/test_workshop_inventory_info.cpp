// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "workshop_support.hpp"
#include "inventory_story.hpp"
#include "inventory/codec.hpp"
#include "message-draft/transfer.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory-pane/slots.hpp"
#include "inventory-pane/presentation.hpp"
#include "input/input_weave.hpp"
#include "terminal-pane/vocabulary.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"
#include <zen/host/grant_wiring.hpp>
#include <zen/history/dump.hpp>
#include <zen/weave/role_request.hpp>
namespace slots = zengine::inventory_pane;

namespace {
namespace inv = zengine::inventory;
using namespace inventory_story;
struct CapturedCommandSinkState { ZEN_SHAPE(CapturedCommandSinkState, 1); };
class CapturedCommandSink : public loom::WeaveBase<CapturedCommandSink, CapturedCommandSinkState,
    loom::Accept<surface::SurfaceText>, loom::Emit<loom::Ack>> {
public:
    void on(const surface::SurfaceText&, loom::Mail& mail) { (void)mail.answer(loom::Ack{}); }
};
class MissingReplyProbe : public loom::WeaveBase<MissingReplyProbe, InventoryHandState,
    loom::Accept<InventoryHandDo, TerminalValueAnswered>, loom::Emit<TerminalValueRequested>> {
public:
    loom::RoleRequest pending;
    void on(const InventoryHandDo&, loom::Mail& mail) {
        REQUIRE(pending.send_to_role(mail, kWorkshopProvider, TerminalValueRequested{}, 73));
    }
    void on(const TerminalValueAnswered&, loom::Mail& mail) {
        if (pending.matches_answer(mail)) pending.forget();
    }
};
}

TEST_CASE("loaded toolbox: save restore keeps typed data and inactive views while invalidating old edits") {
    TempDir files("toolbox-loaded");
    const auto path = files.file("retest.toolbox");
    InventoryStory s(191 | 512, true);
    const auto old = s.entry("story.RuntimeItem");
    const auto box = s.create("single", old.reference);
    s.bind(old.reference, "source.absent"); s.context(box, true);
    REQUIRE(s.layout().views.front().active);
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxSave{path}); });
    REQUIRE_MESSAGE(s.hand->toolboxes.size() == 1, s.shown(s.source));
    CHECK(s.hand->toolboxes.back().operation == "save");
    CHECK(std::filesystem::exists(path));
    s.store(99);
    s.hand->expect_refusal = true;
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxRestore{path, false}); });
    REQUIRE(s.hand->refusals.size() == 1);
    CHECK(s.stored().item.get("count")->as_int() == 99);
    s.hand->expect_refusal = false;
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxRestore{path, true}); });
    REQUIRE_MESSAGE(s.hand->toolboxes.size() == 2, s.shown(s.source));
    CHECK(s.hand->toolboxes.back().operation == "restore");
    CHECK(s.hand->toolboxes.back().entries == 1);
    CHECK(s.stored().item.get("count")->as_int() == 7);
    const auto fresh = s.entry("story.RuntimeItem");
    CHECK(fresh.reference.owner != old.reference.owner);
    CHECK(fresh.reference.entry == old.reference.entry);
    const auto layout = s.layout();
    REQUIRE(layout.views.size() == 1);
    CHECK(layout.views.front().id == box);
    CHECK_FALSE(layout.views.front().active);
    CHECK(layout.views.front().entries.front().owner == fresh.reference.owner);
    REQUIRE(layout.bindings.size() == 1);
    CHECK_FALSE(layout.bindings.front().enabled);
    CHECK(layout.bindings.front().target == "source.absent");
    CHECK(slots::shortcuts(layout).empty());
    s.hand->expect_refusal = true;
    s.act([&](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryRead{old.reference}); });
    REQUIRE(s.hand->refusals.size() == 2);
    // Each restore is another explicit reset, with fresh references but no accumulating views.
    s.hand->expect_refusal = false;
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxRestore{path, true}); });
    CHECK(s.hand->toolboxes.size() == 3);
    CHECK(s.layout().views.size() == 1);
    // The visible restore confirms replacement separately; cancellation preserves newer work.
    s.store(99); s.click(s.source);
    const auto choose_file = [&] {
        s.key(input::scan::kO, input::mod::kCtrl);
        s.key(input::scan::kA, input::mod::kCtrl); s.text(path); s.key(input::scan::kReturn);
    };
    choose_file(); CHECK(s.stored().item.get("count")->as_int() == 99);
    s.key(input::scan::kEscape); CHECK(s.stored().item.get("count")->as_int() == 99);
    choose_file(); s.text("ignored during confirmation"); s.key(input::scan::kReturn);
    CHECK_MESSAGE(s.stored().item.get("count")->as_int() == 7, s.shown(s.source));
    CHECK(s.shown(s.source).find("hotkeys OFF") != std::string::npos);
}

TEST_CASE("loaded toolbox: injected inventory input cannot acquire file authority through the pane") {
    TempDir files("toolbox-denied");
    const auto path = files.file("must-not-exist.toolbox");
    InventoryStory s(191, true);
    s.click(s.source); s.key(input::scan::kS, input::mod::kCtrl | input::mod::kShift);
    s.key(input::scan::kA, input::mod::kCtrl); s.text(path); s.key(input::scan::kReturn);
    CHECK_FALSE(std::filesystem::exists(path));
    CHECK_MESSAGE(s.shown(s.source).find("authorit") != std::string::npos, s.shown(s.source));
    // The physical maker uses the same action, whose permission is independently attributed.
    s.physical_click(s.source);
    input::KeyPressed physical_key; physical_key.scancode=input::scan::kS;
    physical_key.modifiers=input::mod::kCtrl | input::mod::kShift;
    s.physical->push_back(physical_key);
    s.pump_physical();
    physical_key.scancode=input::scan::kA; physical_key.modifiers=input::mod::kCtrl;
    s.physical->push_back(physical_key);
    s.physical->push_back(input::TextEntered{path});
    physical_key.scancode=input::scan::kReturn; physical_key.modifiers=input::mod::kNone;
    s.physical->push_back(physical_key);
    s.pump_physical();
    CHECK_MESSAGE(std::filesystem::exists(path), s.shown(s.source));
}

TEST_CASE("loaded toolbox: a missing Desktop reports the committed collection without claiming rollback") {
    TempDir files("toolbox-cleanup-refusal");
    const auto path = files.file("saved.toolbox");
    InventoryStory s(191 | 512); // no Desktop provider
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxSave{path}); });
    REQUIRE(s.hand->toolboxes.size() == 1);
    s.store(64);
    s.hand->expect_refusal = true;
    s.act([&](loom::Mail& m) { m.send_to_role(slots::kRole, slots::InventoryToolboxRestore{path, true}); });
    REQUIRE(s.hand->refusals.size() == 1);
    CHECK(s.hand->refusals.back().find("Entries restored with hotkeys OFF") != std::string::npos);
    CHECK(s.stored().item.get("count")->as_int() == 7);
    CHECK(s.hand->toolboxes.size() == 1); // no fabricated complete restore
}

TEST_CASE("terminal capture: a withheld native reply grant is attributable without settling the ask") {
    PaneRig r;
    r.mount_workshop();
    auto native = r.take_workshop_off();
    loom::Grant withheld;
    const auto production = zengine::workshop::workshop_grant();
    for (const auto& rule : production.rules()) {
        REQUIRE_FALSE(rule.any_shape);
        if (rule.shape_name == TerminalValueAnswered::zen_name) continue;
        if (rule.any_target) withheld.allow_to_any(rule.shape_name, rule.shape_version);
        else if (!rule.target_role.empty())
            withheld.allow_to_role(rule.shape_name, rule.shape_version, rule.target_role);
        else withheld.allow(rule.shape_name, rule.shape_version, rule.target);
    }
    r.workshop_id = r.bus.register_weave(std::move(native), withheld, kWorkshopProvider);
    r.w->zen_set_self(r.workshop_id);
    auto probe = std::make_unique<MissingReplyProbe>();
    auto* client = probe.get();
    loom::Grant ask;
    ask.allow_to_role(TerminalValueRequested::zen_name, 1, kWorkshopProvider);
    const auto id = r.bus.register_weave(std::move(probe), ask);
    client->zen_set_self(id);
    CHECK(production.permits(TerminalValueAnswered::zen_name, 1, id));
    CHECK_FALSE(withheld.permits(TerminalValueAnswered::zen_name, 1, id));

    TempDir files("missing-reply-log");
    const auto path = (files.path() / "workshop.log").string();
    loom::LoggerSelection selection;
    selection.log_refusals = true;
    selection.shapes.push_back({TerminalValueRequested::zen_name, 0});
    loom::Logger logger(r.bus, selection);
    REQUIRE(logger.open(path));
    r.bus.send(id, loom::Message(loom::to_value(InventoryHandDo{})));
    r.bus.drain_until_idle();
    CHECK(client->pending.pending());
    r.bus.drain_until_idle();
    CHECK(client->pending.pending());
    logger.close();

    std::vector<loom::LogRecord> records;
    REQUIRE(loom::Logger::read(path, &records));
    const loom::HistoryRecord* request = nullptr;
    const loom::HistoryRecord* reply = nullptr;
    for (const auto& record : records) {
        if (record.origin != loom::LogOrigin::BusObservation) continue;
        const auto& observed = record.observation;
        if (observed.shape == TerminalValueRequested::zen_name) request = &observed;
        if (observed.shape == TerminalValueAnswered::zen_name) reply = &observed;
    }
    REQUIRE(request != nullptr);
    REQUIRE(reply != nullptr);
    CHECK(request->outcome == loom::RecordedOutcome::Delivered);
    CHECK(reply->outcome == loom::RecordedOutcome::Refused);
    CHECK(reply->refusal == loom::RefusalReason::CapabilityDenied);
    CHECK(reply->sender == r.workshop_id);
    CHECK(reply->target == id);
    CHECK(reply->shape_version == 1);
    CHECK(reply->correlation == 73);
    CHECK(reply->dispatch_parent == request->seq);
    CHECK(request->seq == client->pending.attempt().seq);
    std::ostringstream rendered;
    loom::dump_log(records, rendered);
    CHECK(rendered.str().find("CapabilityDenied") != std::string::npos);
    CHECK(rendered.str().find(TerminalValueAnswered::zen_name) != std::string::npos);
}

TEST_CASE("portable slots: compact tiles share borders and clipped tiles have no hit targets") {
    slots::InventoryViews state;
    state.views.push_back({"strip","row",false,{}});
    std::vector<inv::InventorySummary> entries={{{"owner","a"},1,"Heal","Item",1,false},
                                               {{"owner","b"},1,"Shield","Item",1,false}};
    slots::View view; view.rows=7; view.columns=17;
    auto rows=slots::render(state,"strip",view,entries,{}); view.map.settle();
    REQUIRE(rows.size()>=6);
    CHECK(rows[1].text=="+-------+-------+");
    REQUIRE(view.map.at(3,2)); CHECK(*view.map.at(3,2)=="owner:a");
    REQUIRE(view.map.at(3,10)); CHECK(*view.map.at(3,10)=="owner:b");
    CHECK(view.map.at(3,8)==nullptr); CHECK(view.map.at(1,2)==nullptr);
    const auto picture=view.map.picture();
    view.columns=8; slots::render(state,"strip",view,entries,{}); view.map.settle();
    CHECK(view.map.size()==0); CHECK_FALSE(view.map.current(picture));
    state.views[0].kind="column"; view.columns=9; view.rows=11;
    rows=slots::render(state,"strip",view,entries,{}); view.map.settle();
    CHECK(rows[5].text=="+-------+");
    REQUIRE(view.map.at(7,2)); CHECK(*view.map.at(7,2)=="owner:b");
    CHECK(view.map.at(5,2)==nullptr);
}

TEST_CASE("portable slots: a newly stored copy offers naming and later rename keeps its identity") {
    for(const bool rename_allowed:{false,true}) {
        InventoryStory s(rename_allowed?191:175,true);
        s.click(s.source,0); s.key(input::scan::kReturn); s.click(s.source,0); // copy the selected capture slot into Inventory
        REQUIRE(s.saved_entries().size()==1);
        s.act([](loom::Mail& m){m.send_to_role(inv::kInventoryRole,inv::InventoryList{});});
        const auto original=s.hand->listing.entries.back();
        CHECK_MESSAGE(s.shown(s.source).find("Name:")!=std::string::npos,s.shown(s.source));
        s.text("Useful"); s.key(input::scan::kReturn);
        if(!rename_allowed) {
            CHECK_MESSAGE(s.shown(s.source).find("no authority")!=std::string::npos,s.shown(s.source));
            CHECK(s.entry(original.label).revision==original.revision);
        } else {
            const auto renamed=s.entry("Useful"); CHECK(slots::same(renamed.reference,original.reference));
            s.menu(s.source,2,2); s.key(input::scan::kA,input::mod::kCtrl); s.text("Better"); s.key(input::scan::kReturn);
            CHECK(slots::same(s.entry("Better").reference,original.reference));
        }
        CHECK(s.saved_entries().size()==1); CHECK(s.saved_entries()[0].item.get("count")->as_int()==7);
        CHECK(s.stored().item.get("count")->as_int()==7);
    }
    InventoryStory cancelled;
    cancelled.click(cancelled.source,0); cancelled.key(input::scan::kReturn); cancelled.click(cancelled.source,0);
    REQUIRE(cancelled.saved_entries().size()==1);
    cancelled.key(input::scan::kEscape);
    CHECK(cancelled.saved_entries().size()==1);
    CHECK(cancelled.shown(cancelled.source).find("Name:")==std::string::npos);
    CHECK(cancelled.entry("story.RuntimeItem").revision==1);
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
    CHECK(t.shown(t.info).find("LINKED") != std::string::npos);
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
    CHECK(t.shown(t.info).find("whole value would replace this draft") != std::string::npos);
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

TEST_CASE("terminal capture: primary drag stores exact authored content and names the new copy") {
    InventoryStory s(191 | 256, true);
    auto sink = std::make_unique<CapturedCommandSink>(); auto* receiver = sink.get();
    const auto grant = loom::emit_default_grant(*sink);
    const auto receiver_id = s.r.bus.register_weave(std::move(sink), grant, surface::kSkinRole);
    receiver->zen_set_self(receiver_id);

    auto* terminal = s.r.mount_terminal();
    load::LoadPlan plan;
    load::ArtifactIntent artifact;
    artifact.stem = "zengine-terminal-pane";
    artifact.weave = load::WeaveIntent{"zengine.terminal"}; plan.artifacts.push_back(artifact);
    REQUIRE(s.r.run_plan(plan).ok);
    s.r.pick({"zengine.terminal", "terminal"});
    const auto kind = s.r.session().panels.runtime.find("zengine.terminal", "terminal")->kind;
    for (auto& p : s.r.session().setup.active.panes) if (p.ref.provider == "zengine.terminal") {
        p.place = {pane_unit::kSubcells, 2 * surface::kCellSubs, 31 * surface::kCellSubs};
        p.width = {pane_unit::kSubcells, 80 * surface::kCellSubs};
        p.height = {pane_unit::kSubcells, 24 * surface::kCellSubs};
    }
    const auto sent = terminal->send(loom::Address::to_role("zengine.skin"),
        surface::SurfaceText::zen_name, 1, {{std::string("slot"), loom::FieldValue{std::string("score")}}, {std::string("text"), loom::FieldValue{std::string("captured original")}}});
    REQUIRE(sent);
    s.r.extent(181, 60);
    const auto rows = pane_rows(s.r, kind);
    std::int64_t at = -1;
    for (std::size_t i = 0; i < rows.size(); ++i) if (rows[i].find("SUBMITTED") != std::string::npos || rows[i].find(" -> ") != std::string::npos) at = static_cast<std::int64_t>(i);
    if (at < 0) for (std::size_t i=0;i<rows.size();++i) if(rows[i].find(surface::SurfaceText::zen_name)!=std::string::npos) { at=static_cast<std::int64_t>(i); break; }
    REQUIRE_MESSAGE(at >= 0, s.shown(kind));
    // Capture alone never replays the command. A configured shortcut still needs its actor.
    int delivered = 0;
    const auto observer = s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered && e.schema_name == surface::SurfaceText::zen_name && e.payload &&
            e.payload->get("text")->as_text() == "captured original")
            ++delivered;
    });
    RemoveObserver remove{s.r.bus, observer};
    auto press = s.button_at(kind, at, true), release = s.button_at(s.source, 0, false);
    auto move = release; move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
    s.batch({press, move, release});
    REQUIRE_MESSAGE(s.saved_entries().size() == 1, (s.shown(kind) + s.shown(s.source) + s.r.last_notice()));
    const auto saved = s.saved_entries().front();
    CHECK(saved.item.get("text")->as_text() == "captured original");
    REQUIRE(saved.metadata.size() == 1);
    CHECK(saved.metadata.front().get("observation")->as_int() == static_cast<std::int64_t>(sent.entry));
    CHECK(saved.metadata.front().get("kind")->as_text() == "submitted");
    s.text("My captured command"); s.key(input::scan::kReturn);
    CHECK(s.entry("My captured command").revision == 2);
    REQUIRE(terminal->transcript().retained_value(sent.entry));
    CHECK(terminal->transcript().retained_value(sent.entry)->get("text")->as_text() == "captured original");
    s.bind(s.entry("My captured command").reference, surface::kSkinRole);
    s.context("inventory", true);
    s.key(30, input::mod::kAlt);
    CHECK(delivered == 0);
    CHECK(s.shown(s.source).find("no authority") != std::string::npos);
    s.physical->push_back(input::KeyPressed{30, "1", input::mod::kAlt}); s.pump_physical();
    CHECK_MESSAGE(delivered == 1,(s.shown(s.source)+s.r.last_notice()));

}

TEST_CASE("terminal capture: retrieval and Inventory storage need separate current actor authority") {
    for (const int permissions : {191, 256}) {
        InventoryStory s(permissions);
        auto* terminal = s.r.mount_terminal();
        load::LoadPlan plan; load::ArtifactIntent artifact;
        artifact.stem="zengine-terminal-pane"; artifact.weave=load::WeaveIntent{"zengine.terminal"}; plan.artifacts.push_back(artifact);
        REQUIRE(s.r.run_plan(plan).ok); s.r.pick({"zengine.terminal","terminal"});
        const auto kind=s.r.session().panels.runtime.find("zengine.terminal","terminal")->kind;
        for(auto& p:s.r.session().setup.active.panes) if(p.ref.provider=="zengine.terminal") {
            p.place={pane_unit::kSubcells,2*surface::kCellSubs,31*surface::kCellSubs};
            p.width={pane_unit::kSubcells,80*surface::kCellSubs}; p.height={pane_unit::kSubcells,24*surface::kCellSubs};
        }
        s.r.bus.send(terminal->id(),loom::Message(loom::to_value(loom::Ack{})));
        s.r.extent(181,60);
        const auto rows=pane_rows(s.r,kind); std::int64_t at=-1;
        for(std::size_t i=0;i<rows.size();++i) if(rows[i].find("zen.Ack")!=std::string::npos) { at=static_cast<std::int64_t>(i);break; }
        REQUIRE_MESSAGE(at>=0,s.shown(kind));
        auto press=s.button_at(kind,at,true), release=s.button_at(s.source,0,false);
        auto move=release; move.kind="PointerMoved"; move.dx=release.x-press.x;move.dy=release.y-press.y;
        s.batch({press,move,release});
        CHECK(s.saved_entries().empty());
        CHECK_MESSAGE(s.shown(permissions==191?kind:s.source).find("no authority")!=std::string::npos,
                      (s.shown(kind)+s.shown(s.source)+s.r.last_notice()));
    }
}


TEST_CASE("terminal capture: wrapped rows and context pickup preserve identity while stale pictures refuse") {
    InventoryStory s(191 | 256);
    auto* terminal = s.r.mount_terminal();
    load::LoadPlan plan; load::ArtifactIntent artifact;
    artifact.stem = "zengine-terminal-pane"; artifact.weave = load::WeaveIntent{"zengine.terminal"};
    plan.artifacts.push_back(artifact); REQUIRE(s.r.run_plan(plan).ok);
    s.r.pick({"zengine.terminal", "terminal"});
    const auto kind = s.r.session().panels.runtime.find("zengine.terminal", "terminal")->kind;
    for (auto& pane : s.r.session().setup.active.panes) if (pane.ref.provider == "zengine.terminal") {
        pane.place = {pane_unit::kSubcells, 2 * surface::kCellSubs, 31 * surface::kCellSubs};
        pane.width = {pane_unit::kSubcells, 24 * surface::kCellSubs};
        pane.height = {pane_unit::kSubcells, 26 * surface::kCellSubs};
    }
    REQUIRE(terminal->send(loom::Address::to_role(surface::kSkinRole), surface::SurfaceText::zen_name, 1,
        {{"slot", loom::FieldValue{std::string("score")}}, {"text", loom::FieldValue{std::string("wrapped")}}}));
    s.r.extent(181, 60);
    auto rows = pane_rows(s.r, kind); std::int64_t at = -1;
    for (std::size_t i=0;i<rows.size();++i) if(rows[i].rfind("^ SurfaceText",0)==0) at=static_cast<std::int64_t>(i);
    REQUIRE_MESSAGE(at>=0, s.shown(kind));
    REQUIRE(rows[static_cast<std::size_t>(at+1)].find("zengine.skin") != std::string::npos);
    auto press=s.button_at(kind,at+1,true), release=s.button_at(s.source,0,false);
    auto move=release; move.kind="PointerMoved"; move.dx=release.x-press.x; move.dy=release.y-press.y;
    s.batch({press,move,release});
    REQUIRE_MESSAGE(s.saved_entries().size()==1,(s.shown(kind)+s.shown(s.source)));
    CHECK(s.saved_entries().front().item.get("text")->as_text()=="wrapped");
    s.key(input::scan::kEscape); // keep the generated copy name
    const auto old_picture=s.r.session().panels.external_pane(kind)->picture;
    s.r.bus.send(terminal->id(),loom::Message(loom::to_value(loom::Ack{}))); s.r.extent(182,60);
    s.r.bus.office_send_to_role_as(s.r.workshop_id,kWorkshopProvider,"zengine.terminal",
        loom::Message(loom::to_value(v3::PanePressed{"terminal",at+1,0,false,old_picture})));
    s.r.bus.drain_until_idle();
    CHECK_MESSAGE(s.shown(kind).find("That transcript")!=std::string::npos,s.shown(kind));
    rows=pane_rows(s.r,kind); at=-1;
    for(std::size_t i=0;i<rows.size();++i) if(rows[i].rfind("v zen.Ack",0)==0) at=static_cast<std::int64_t>(i);
    REQUIRE_MESSAGE(at>=0,s.shown(kind)); s.menu(kind,at,0); s.click(s.source);
    REQUIRE_MESSAGE(s.saved_entries().size()==2,(s.shown(kind)+s.shown(s.source)));
    CHECK(s.saved_entries().back().item.schema().name()==loom::Ack::zen_name);
    CHECK(s.saved_entries().back().metadata.front().get("kind")->as_text()=="received");
}
