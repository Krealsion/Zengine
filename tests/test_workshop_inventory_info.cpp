// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// INVENTORY THROUGH THE REAL LOADED Info, Inventory and Inventory pane images: toolboxes,
// portable slots, the collection gestures, Compose drops and terminal capture, each spending
// the current actor's own authority.

#include "workshop_support.hpp"
#include "census.hpp"
#include "inventory_story.hpp"
#include "pane_window.hpp"
#include "operator/reference.hpp"
#include "inventory/codec.hpp"
#include "message-draft/transfer.hpp"
#include "inventory/vocabulary.hpp"
#include "inventory/inventory-pane/slots.hpp"
#include "inventory/inventory-pane/presentation.hpp"
#include "input/input_weave.hpp"
#include "terminal/vocabulary.hpp"
#include "flow/flow-pane/vocabulary.hpp"
#include "view/view-builder/picture.hpp"
#include "view/view-builder/vocabulary.hpp"
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
    loom::Accept<RigNote>, loom::Emit<loom::Ack>> {
public:
    void on(const RigNote&, loom::Mail& mail) { (void)mail.answer(loom::Ack{}); }
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

/// EVERY STATE THE INVENTORY PANE'S CENSUS DRIVES IT INTO, each handed to `take` as Workshop holds
/// the pane's rows there: main Inventory browsing, with an entry picked to move, with a name line
/// open, and a portable single box.
void each_inventory_pane_state(InventoryStory& s, const std::function<void(const RuntimePane&)>& take) {
    const auto held = [&](const std::string& pane) {
        const RuntimePane* found = s.r.session().panes.runtime.find(slots::kRole, pane);
        REQUIRE_MESSAGE(found != nullptr, pane << " is not held");
        return found;
    };
    const auto declares = [&](const std::string& id) {
        for (const auto& a : held("inventory")->actions) if (a.id == id) return true;
        return false;
    };
    take(*held("inventory"));
    s.click(s.source); // the keys, with the compatibility slot's entry selected
    REQUIRE(s.r.session().panes.keyboard == s.source);
    s.key(input::scan::kX, input::mod::kCtrl);
    REQUIRE(declares("inventory.move.cancel"));
    take(*held("inventory"));
    s.key(input::scan::kEscape);
    REQUIRE_FALSE(declares("inventory.move.cancel"));
    s.key(input::scan::kN, input::mod::kCtrl);
    REQUIRE(declares("inventory.name.save"));
    take(*held("inventory"));
    s.key(input::scan::kEscape);
    REQUIRE(declares("inventory.copy"));
    take(*held(s.create("single")));
}

/// THE INVENTORY PANE'S CENSUS. The pane declares no modes, so a row it declares in several of the
/// states driven is one line.
const CensusDriverOf kInventoryPaneCensus{slots::kRole, [] {
    OfficeCensus census;
    InventoryStory s;
    each_inventory_pane_state(s, [&](const RuntimePane& held) { census_take(census, slots::kRole, held); });
    std::vector<std::string> once;
    for (std::string& line : census.lines) {
        if (std::find(once.begin(), once.end(), line) == once.end()) once.push_back(std::move(line));
    }
    census.lines = std::move(once);
    return census;
}};

/// THE INVENTORY PANE'S PRESSES: each row of each state the census drives, replayed for each press.
const PressDriverOf kInventoryPanePresses{slots::kRole, [](PressWitness& w) {
    press_each_state<InventoryStory>(
        w, [] { return std::make_unique<InventoryStory>(); },
        [](InventoryStory& s, const std::function<void(const RuntimePane&)>& at) {
            each_inventory_pane_state(s, at);
        });
}};
}

TEST_CASE("the Inventory pane answers its manual for main Inventory and each view, a section for "
          "every action it declares") {
    InventoryStory s;
    std::set<std::string> ids;
    std::vector<std::string> panes;
    each_inventory_pane_state(s, [&](const RuntimePane& held) {
        for (const auto& a : held.actions) ids.insert(a.id);
        if (std::find(panes.begin(), panes.end(), held.pane) == panes.end()) panes.push_back(held.pane);
    });
    REQUIRE(panes.size() == 2);
    ManualReader reader(s.r, slots::kRole);
    for (const std::string& pane : panes) {
        INFO("pane ", pane);
        check_answered_manual(reader, pane, ids);
    }
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
    // The physical weaver uses the same action, whose permission is independently attributed.
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
    // The refusal is the pane's notice; a view it places over the notice's start hides "refused".
    std::string said; for (const auto& row : held_canvas_text(*s.r.session().panes.external_pane(s.source))) said += row;
    CHECK_MESSAGE(said.find("refused")!=std::string::npos,said);
    CHECK_MESSAGE(s.shown(s.source).find("retained: `alt+1` is authored for")!=std::string::npos,s.shown(s.source));
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
                kLayoutsRole, loom::Message(loom::to_value(PaneCloseRequested{slots::kRole,"inventory"})));
            s.r.bus.drain_until_idle(); REQUIRE_FALSE(s.r.session().panes.has(s.source));
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
    // Physical weaver authority passes the permission check; the actual destination gate refuses.
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
    const auto row=s.create("row"); const auto kind=s.r.session().panes.runtime.find(slots::kRole,row)->kind;
    for(auto& p:s.r.session().setup.active.panes) if(p.ref.pane==row) {
        p.place={pane_unit::kPixels,2*gfx::kCellPx,34*gfx::kCellPx};
        p.width={pane_unit::kPixels,72*gfx::kCellPx}; p.height={pane_unit::kPixels,10*gfx::kCellPx};
    }
    s.r.extent(179,60); s.r.extent(180,60); // a same-size extent reseats nothing: the view is placed now
    auto press=s.button_at(s.source,2,true),release=s.button_at(kind,1,false),move=release;
    move.kind="PointerMoved"; move.dx=release.x-press.x; move.dy=release.y-press.y;
    s.batch({press,move,release});
    CHECK_MESSAGE(slots::placed(s.layout(),a)==row,(s.shown(s.source)+s.shown(kind)));
    CHECK(s.saved_entries().size()==1);
    const auto before=s.layout();
    const ExternalPane& held=*s.r.session().panes.external_pane(s.source);
    REQUIRE(shows_canvas(held));
    const CanvasRows lattice=held_canvas_rows(held);
    const std::string area=pane_window_area(s.r.session().panes,*s.r.session().panes.runtime.of_kind(s.source));
    s.r.bus.office_send_to_role_as(s.r.bus.role_holder(kWorkshopProvider),kWorkshopProvider,slots::kRole,
        loom::Message(loom::to_value(PaneCanvasValueDrop{"inventory",area,
            lattice.column_x(0)-lattice.x,lattice.row_y(0)-lattice.y,s.pair(88),slots::kRole,row,"made up"})));
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
    CHECK(t.shown(t.info).find("PROPERTIES") != std::string::npos);
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

TEST_CASE("inventory Info: an owner reload keeps the pair and every reference held to it") {
    InventoryStory t;
    t.acquire(); t.place(); t.edit("42");
    t.r.enqueue_reload("zengine-inventory", WORKSHOP_SO_INVENTORY);
    t.r.bus.drain_until_idle();
    REQUIRE(t.r.load_refusals.empty());
    // THE DRAFT'S REFERENCE STILL NAMES ITS ENTRY: the save lands on the reloaded owner.
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK_MESSAGE(t.shown(t.info).find("no longer here") == std::string::npos, t.shown(t.info));
    CHECK(t.stored().item.get("count")->as_int() == 42);
    t.key(input::scan::kD, input::mod::kCtrl);
    t.acquire(); t.place();
    CHECK(t.shown(t.info).find("count: 42") != std::string::npos);
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

TEST_CASE("inventory Info: an ordinary message cannot impersonate physical weaver input") {
    InventoryStory t(false);
    t.click(t.source);
    input::InjectedEvent key; key.kind = "KeyPressed"; key.scancode = input::scan::kReturn;
    t.r.publish(loom::to_value(input::AttributedInput{true, 0, key}));
    CHECK(t.shown(t.source).find("Click a receiving pane") == std::string::npos);
    t.r.key(input::scan::kReturn);
    CHECK_MESSAGE(t.shown(t.source).find("attributed input gesture") != std::string::npos, t.shown(t.source));
}

TEST_CASE("inventory Info: a departed input actor cannot leave the weaver trapped carrying its reference") {
    InventoryStory t;
    t.acquire();
    t.r.bus.unregister_weave(t.hand_id).reset();
    t.hand = nullptr;
    // THE WEAVER'S OWN FIRST PRESS PUTS THE REFERENCE DOWN, spent there: the desk stood in front
    // of the panes while it was carried.
    t.physical_click(t.info);
    CHECK(t.r.last_notice() == "Carried reference released: its input actor has left");
    // ...AND THE NEXT REACHES THE PANE UNDER THE HAND.
    t.physical_click(t.info);
    REQUIRE(t.r.session().panes.keyboard == t.info);
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
            (event.schema_name == gfx::AreaPressed::zen_name ||
             event.schema_name == gfx::AreaButtonChanged::zen_name) &&
            event.target == t.r.workshop_id) {
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

TEST_CASE("inventory Info: the weaver's own key a window gives saves, and a key no window named is "
          "refused the save") {
    InventoryStory t;
    t.acquire();
    t.place();
    t.edit("42");
    REQUIRE(t.stored().item.get("count")->as_int() == 7);
    // A KEY NO WINDOW NAMED names no hand, so the save it asks is refused and nothing is written.
    t.r.key(input::scan::kS, input::mod::kCtrl);
    CHECK_MESSAGE(t.shown(t.info).find("attributed input gesture") != std::string::npos,
                  t.shown(t.info));
    CHECK(t.stored().item.get("count")->as_int() == 7);
    // THE SAME KEY, READ FROM THE WINDOW'S PLATFORM, is the weaver's own hand: saved.
    t.r.window_queue_key(input::scan::kS, input::mod::kCtrl);
    t.r.window_beat();
    CHECK_MESSAGE(t.stored().item.get("count")->as_int() == 42, t.shown(t.info));
}

TEST_CASE("inventory Info: the weaver's own save a copy, read from the window's platform, saves a "
          "new entry") {
    InventoryStory t;
    t.acquire();
    t.place();
    const std::size_t before = t.saved_entries().size();
    t.r.window_queue_key(input::scan::kS, input::mod::kCtrl | input::mod::kShift);
    t.r.window_beat();
    CHECK_MESSAGE(t.saved_entries().size() == before + 1, (t.shown(t.info) + t.trace));
    CHECK_MESSAGE(t.shown(t.info).find("Saved a new entry") != std::string::npos, t.shown(t.info));
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
        const auto compose_kind = r.session().panes.runtime.find(kComposerOffice, "compose")->kind;
        stage_desk(r, [&](Setup& staged) {
            for (auto& p : staged.panes) if (p.ref.provider == kComposerOffice) {
                p.place = {pane_unit::kPixels, 85*gfx::kCellPx, 4*gfx::kCellPx};
                p.width = {pane_unit::kPixels, 80*gfx::kCellPx};
                p.height = {pane_unit::kPixels, 24*gfx::kCellPx};
            }
        });
        r.extent(180, 60);
        REQUIRE(shows_canvas(*r.session().panes.external_pane(compose_kind)));
        // What the window showed of Compose before it had a target.
        const auto before_target = held_reading(compose_kind).parts;
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
        const PaneCanvasValueDrop forged{"compose", held_area_of(compose_kind), 0, 0,
                                         loom::Bytes(bytes.begin(), bytes.end()), "", "", ""};
        r.bus.send_to_role(kComposerOffice, loom::Message(loom::to_value(forged)));
        r.bus.drain_until_idle();
        CHECK(s.shown(compose_kind) == untouched);
        // A DROP THE WINDOW LANDED ON WHAT COMPOSE SHOWED BEFORE IT HAD A TARGET, where a target's
        // message stands now, is refused in words and fills nothing.
        std::int64_t row_then = -1, column_then = -1;
        for (const auto& [name, part] : held_reading(compose_kind).parts) {
            if (row_then < 0 && part_drawn_at(before_target, part.row, part.column) != name) {
                row_then = part.row;
                column_then = part.column;
            }
        }
        REQUIRE_MESSAGE(row_then >= 0, s.shown(compose_kind));
        const std::string compose_area = held_area_of(compose_kind);
        r.bus.office_send_to_role_as(r.bus.role_holder(kWorkshopProvider), kWorkshopProvider,
            kComposerOffice, loom::Message(loom::to_value(PaneCanvasValueDrop{"compose",
                compose_area, column_then * gfx::kCellPx + 6, row_then * gfx::kCellPx + 6,
                loom::Bytes(bytes.begin(), bytes.end()), "", "", "", compose_area,
                part_drawn_at(before_target, row_then, column_then), 0})));
        r.bus.drain_until_idle();
        CHECK_MESSAGE(s.shown(compose_kind).find("the form changed under it") != std::string::npos,
                      s.shown(compose_kind));
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

/// Compose on its canvas at the desk's right, its target Inventory: the pane a carried reference
/// is dropped on, by the place of a field of Inventory's own messages.
static std::int64_t compose_on_canvas_for_inventory(InventoryStory& s) {
    auto& r = s.r;
    REQUIRE(r.load_refusals.empty());
    r.pick({"zengine.info", "info"}); // give Compose the right-hand area
    r.pick(composer_ref());
    const auto compose_kind = r.session().panes.runtime.find(kComposerOffice, "compose")->kind;
    stage_desk(r, [&](Setup& staged) {
        for (auto& p : staged.panes) if (p.ref.provider == kComposerOffice) {
            p.place = {pane_unit::kPixels, 85*gfx::kCellPx, 4*gfx::kCellPx};
            p.width = {pane_unit::kPixels, 80*gfx::kCellPx};
            p.height = {pane_unit::kPixels, 24*gfx::kCellPx};
        }
    });
    r.extent(180, 60);
    auto selector = std::make_unique<InventoryHand>(); auto* raw = selector.get();
    loom::Grant grant; grant.allow_to_any(intro::LoadedSelected::zen_name, 1);
    const auto selector_id = r.bus.register_weave(std::move(selector), grant, kIntroOffice);
    raw->zen_set_self(selector_id);
    raw->next = [](loom::Mail& m) { m.as_role(kIntroOffice).publish(
        intro::LoadedSelected{"loaded", "zengine-inventory", inv::kInventoryRole}); };
    r.bus.send(selector_id, loom::Message(loom::to_value(InventoryHandDo{})));
    r.bus.drain_until_idle();
    REQUIRE(shows_canvas(*r.session().panes.external_pane(compose_kind)));
    return compose_kind;
}

/// The row of a pane's picture whose text holds `text`; -1 where none does.
static std::int64_t row_saying(InventoryStory& s, std::int64_t kind, const std::string& text) {
    const auto rows = pane_rows(s.r, kind);
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (rows[i].find(text) != std::string::npos) return static_cast<std::int64_t>(i);
    return -1;
}

TEST_CASE("a drop whose pane leaves before it is delivered is said not delivered: a reference or a "
          "value, on a canvas") {
    // The drop is queued and said sent; the pane leaves before the bus dispatches it.
    const auto leave_once_sent = [](InventoryStory& s, const std::string& said, const std::string& role,
                                    bool& removed) {
        return s.r.bus.add_observer([&s, said, role, &removed](const loom::BusEvent& e) {
            if (!removed && e.kind == loom::EventKind::Delivered && e.target == s.r.workshop_id &&
                s.r.last_notice().find(said) != std::string::npos)
                removed = s.r.kernel.unload_role(role);
        });
    };
    SUBCASE("a reference on Compose's canvas") {
        InventoryStory s(63, true);
        s.append(7, "source");
        const auto compose_kind = compose_on_canvas_for_inventory(s);
        const std::int64_t rename = row_saying(s, compose_kind, "InventoryRename v1");
        REQUIRE(rename >= 0);
        s.click(compose_kind, rename);
        const std::int64_t field = row_saying(s, compose_kind, "reference:");
        REQUIRE_MESSAGE(field >= 0, s.shown(compose_kind));
        s.acquire();
        bool removed = false;
        RemoveObserver watch{s.r.bus, leave_once_sent(s, " -> Compose", kComposerOffice, removed)};
        s.click(compose_kind, field);
        REQUIRE(removed);
        CHECK_MESSAGE(s.r.last_notice().find(std::string("Reference not delivered to ") +
                                             kComposerOffice) != std::string::npos,
                      s.r.last_notice());
    }
    SUBCASE("a value on Flow's canvas") {
        InventoryStory s(191, true, false, true);
        s.append(1, "A");
        REQUIRE(s.flow != 0);
        bool removed = false;
        RemoveObserver watch{s.r.bus, leave_once_sent(s, " -> Flow", "zengine.flow", removed)};
        auto press = s.button_at(s.source, 2, true);
        auto release = s.button_at(s.flow, 8, false);
        release.x += 40; // over the graph, right of Flow's rail
        auto move = release;
        move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
        s.batch({press, move, release});
        REQUIRE_MESSAGE(removed, s.r.last_notice());
        CHECK_MESSAGE(s.r.last_notice().find("Value not delivered to zengine.flow") != std::string::npos,
                      s.r.last_notice());
    }
    SUBCASE("a Terminal value on Inventory's canvas, the drop that names its source") {
        InventoryStory s(191 | 256);
        auto* terminal = s.r.mount_terminal();
        load::LoadPlan plan; load::ArtifactIntent artifact;
        artifact.stem = "zengine-terminal-pane"; artifact.weave = load::WeaveIntent{"zengine.terminal"};
        plan.artifacts.push_back(artifact);
        REQUIRE(s.r.run_plan(plan).ok);
        s.r.pick({"zengine.terminal", "terminal"});
        const auto kind = s.r.session().panes.runtime.find("zengine.terminal", "terminal")->kind;
        stage_desk(s.r, [&](Setup& staged) {
            for (auto& p : staged.panes) if (p.ref.provider == "zengine.terminal") {
                p.place = {pane_unit::kPixels, 2 * gfx::kCellPx, 31 * gfx::kCellPx};
                p.width = {pane_unit::kPixels, 80 * gfx::kCellPx};
                p.height = {pane_unit::kPixels, 24 * gfx::kCellPx};
            }
        });
        s.r.bus.send(terminal->id(), loom::Message(loom::to_value(loom::Ack{})));
        s.r.extent(181, 60);
        const std::int64_t at = row_saying(s, kind, "zen.Ack");
        REQUIRE_MESSAGE(at >= 0, s.shown(kind));
        bool removed = false;
        RemoveObserver watch{s.r.bus, leave_once_sent(s, " -> ", slots::kRole, removed)};
        auto press = s.button_at(kind, at, true), release = s.button_at(s.source, 0, false);
        auto move = release;
        move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
        s.batch({press, move, release});
        REQUIRE_MESSAGE(removed, s.r.last_notice());
        CHECK_MESSAGE(s.r.last_notice().find(std::string("Value not delivered to ") + slots::kRole) !=
                          std::string::npos,
                      s.r.last_notice());
    }
}

TEST_CASE("a reference placed on a canvas whose pane has no door for one is sent nothing and stays held") {
    InventoryStory s(191, true, false, true);
    s.append(1, "A");
    REQUIRE(s.flow != 0);
    std::int64_t drops = 0;
    RemoveObserver watch{s.r.bus, s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered && (e.schema_name == PaneCanvasDrop::zen_name ||
                                                    e.schema_name == PaneCanvasValueDrop::zen_name))
            ++drops;
    })};
    s.acquire();
    // Flow takes a value on its canvas and no reference: the place refuses, in words...
    s.click(s.flow, 8);
    CHECK(drops == 0);
    CHECK_MESSAGE(s.r.last_notice().find("does not accept the carried item") != std::string::npos,
                  s.r.last_notice());
    // ...and the reference is still in hand: Info, which takes one on its canvas, is given it.
    s.place();
    CHECK(drops == 1);
}

TEST_CASE("a live reference carried from Inventory lands in the Compose field its place names on Compose's canvas") {
    InventoryStory s(63, true);
    s.append(7, "source");
    auto& r = s.r;
    const auto compose_kind = compose_on_canvas_for_inventory(s);
    const auto row_saying = [&](const std::string& text) { return ::row_saying(s, compose_kind, text); };
    // The form the reference is for, opened by its row on the canvas...
    const std::int64_t rename = row_saying("InventoryRename v1");
    REQUIRE(rename >= 0);
    s.click(compose_kind, rename);
    const std::int64_t field = row_saying("reference:");
    REQUIRE_MESSAGE(field >= 0, s.shown(compose_kind));
    // ...then the source entry's live reference, picked up in Inventory and clicked onto that
    // field: Workshop sends it as a canvas drop, and the field's row takes it.
    s.acquire();
    s.click(compose_kind, field);
    INFO(s.shown(compose_kind));
    // WHAT WAS CARRIED, FROM WHERE AND TO WHERE (WL-NOTE-06).
    CHECK_MESSAGE(r.last_notice().find(": Inventory -> Compose") != std::string::npos,
                  r.last_notice());
    CHECK(s.shown(compose_kind).find("Copied data into form") != std::string::npos);
    const auto rows = pane_rows(r, compose_kind);
    REQUIRE(static_cast<std::size_t>(field) < rows.size());
    CHECK(rows[static_cast<std::size_t>(field)].find("[copied message/list]") != std::string::npos);
}

/// The row of a pane's rows its marker stands on: the one starting `> `, or -1.
static std::int64_t marked_row(InventoryStory& s, std::int64_t kind) {
    const auto rows = pane_rows(s.r, kind);
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (rows[i].rfind("> ", 0) == 0) return static_cast<std::int64_t>(i);
    return -1;
}

TEST_CASE("Inventory draws its rows on its canvas on the medium's own ground, and a name line's caret "
          "and selection stand in the line, writing nothing into it, each move of them drawn") {
    InventoryStory s;
    s.append(10, "Zulu");
    s.click(s.source, s.row_of(s.source, "Zulu")); // the entry selected, and the keys here
    s.key(input::scan::kN, input::mod::kCtrl);      // rename it
    const auto held = [&s] {
        const ExternalPane* p = s.r.session().panes.external_pane(s.source);
        REQUIRE(p != nullptr);
        REQUIRE(shows_canvas(*p));
        return *p;
    };
    CHECK(wears_ground(held()));
    const std::int64_t line = s.row_of(s.source, "Name:") + 1;
    REQUIRE(line > 0);
    const auto rows = pane_rows(s.r, s.source);
    REQUIRE(static_cast<std::size_t>(line) < rows.size());
    CHECK(rows[static_cast<std::size_t>(line)] == "Zulu"); // no character written for the caret
    CHECK(held_caret(held()).row == line);
    CHECK(held_caret(held()).column == 4); // on the blank after the last character
    std::size_t drawn = 0;
    {
        RemoveObserver watch{s.r.bus, s.r.bus.add_observer([&drawn](const loom::BusEvent& e) {
            drawn += e.kind == loom::EventKind::Delivered && e.schema_name == gfx::Drawn::zen_name &&
                             e.authored_role == slots::kRole
                         ? 1u
                         : 0u;
        })};
        s.key(input::scan::kLeft); // the caret alone moves: a drawing of its own
    }
    CHECK(drawn >= 1);
    CHECK(held_caret(held()).column == 3);
    CHECK(pane_rows(s.r, s.source) == rows);
    s.key(input::scan::kLeft, input::mod::kShift);
    CHECK(held_caret(held()).column == 2);
    CHECK(held_caret(held()).sel_begin_row == line);
    CHECK(held_caret(held()).sel_begin_col == 2);
    CHECK(held_caret(held()).sel_end_col == 3);
    s.key(input::scan::kEscape); // the edit ends, and its caret with it
    CHECK(held_caret(held()).row == kNoCaret);
    CHECK(s.row_of(s.source, "Name:") < 0);
}

TEST_CASE("the wheel walks Inventory's selection a notch at a time on its canvas, fractions carried, "
          "and no further than its list") {
    InventoryStory s;
    s.append(10, "Alpha");
    s.append(20, "Bravo");
    REQUIRE(shows_canvas(*s.r.session().panes.external_pane(s.source)));
    const std::int64_t first = marked_row(s, s.source);
    REQUIRE(first >= 0);
    const auto wheel = [&s](double dy) {
        input::InjectedEvent e = s.button_at(s.source, 1, true);
        e.kind = "PointerWheel";
        e.wheel_dy = dy;
        s.event(e);
    };
    wheel(-1); // toward the weaver: the next entry
    CHECK(marked_row(s, s.source) == first + 1);
    wheel(0.5); // half a notch back moves nothing yet...
    CHECK(marked_row(s, s.source) == first + 1);
    wheel(0.5); // ...and the other half does
    CHECK(marked_row(s, s.source) == first);
    // A wheel turned as far as a number goes walks to the list's end at once, and no further.
    wheel(-1e300);
    const std::int64_t end = marked_row(s, s.source);
    CHECK(end > first);
    wheel(-1);
    CHECK(marked_row(s, s.source) == end);
    wheel(1e300);
    const std::int64_t start = marked_row(s, s.source);
    CHECK(start <= first);
    wheel(1);
    CHECK(marked_row(s, s.source) == start);
    for (int notch = 0; notch < 8 && marked_row(s, s.source) < first; ++notch) wheel(-1);
    CHECK(marked_row(s, s.source) == first);
}

TEST_CASE("while an entry is carried the wheel reaches the pane under the hand, past Workshop's area in "
          "front, and walks its selection; the carry stands, and is dropped where it is released") {
    InventoryStory s;
    s.append(10, "Alpha");
    s.append(20, "Bravo");
    s.append(30, "Charlie");
    REQUIRE(shows_canvas(*s.r.session().panes.external_pane(s.source)));
    const std::int64_t alpha = s.row_of(s.source, "Alpha");
    REQUIRE(alpha >= 0);
    // The hand takes Alpha up and moves it over Bravo's row, still held: the carry stands.
    input::InjectedEvent press = s.button_at(s.source, alpha, true);
    input::InjectedEvent move = s.button_at(s.source, alpha + 1, true);
    move.kind = "PointerMoved";
    move.dx = move.x - press.x;
    move.dy = move.y - press.y;
    const std::string said_before = s.r.last_notice();
    s.event(press);
    s.event(move);
    // A DRAG UNDER WAY SAYS NOTHING (WL-NOTE-06).
    CHECK_MESSAGE(s.r.last_notice() == said_before, s.r.last_notice());
    const std::int64_t before = marked_row(s, s.source);
    REQUIRE(before >= 0);
    input::InjectedEvent wheel = move;
    wheel.kind = "PointerWheel";
    wheel.wheel_dy = -1; // toward the weaver: the next entry
    s.event(wheel);
    CHECK_MESSAGE(s.r.last_notice() == said_before, s.r.last_notice());
    // The hand carries on over Info, so the carried value is drawn there and Inventory's rows read
    // as it draws them: its marker walked a row.
    input::InjectedEvent release = s.button_at(s.info, 0, false);
    input::InjectedEvent over = release;
    over.kind = "PointerMoved";
    over.dx = over.x - move.x;
    over.dy = over.y - move.y;
    s.event(over);
    CHECK_MESSAGE(marked_row(s, s.source) == before + 1, s.shown(s.source));
    s.event(release);
    CHECK_MESSAGE(s.shown(s.info).find("COPY story.RuntimeItem") != std::string::npos,
                  (s.shown(s.info) + s.r.last_notice()));
}

TEST_CASE("a right press on Inventory's rows offers its menu, and one beside them, where a prose press "
          "would have named no row, is handed back and Workshop's own pane menu opens") {
    InventoryStory s;
    s.r.extent_on_window(180, 60);
    const ExternalPane* held = s.r.session().panes.external_pane(s.source);
    REQUIRE(held != nullptr);
    REQUIRE(shows_canvas(*held));
    const CanvasRows lattice = held_canvas_rows(*held);
    REQUIRE(lattice.rows > 2);
    const auto right = [&s](std::int64_t px, std::int64_t py) {
        for (const bool down : {true, false})
            s.r.hand_moment(input::PointerButton{3, down, px, py, input::space::kPixels,
                                                 input::mod::kNone});
    };
    right(lattice.column_x(2) + 1, lattice.row_y(1) + lattice.line / 2); // an entry's row: its menu
    CHECK(s.r.session().presented.open);
    CHECK_FALSE(s.r.session().context.open);
    s.key(input::scan::kEscape);
    REQUIRE_FALSE(s.r.session().presented.open);
    // ...AND BELOW ITS ROWS, where it names none: handed back.
    right(lattice.column_x(2) + 1, lattice.row_y(lattice.rows - 1) + lattice.line / 2);
    CHECK_FALSE(s.r.session().presented.open);
    CHECK(s.r.session().context.open);
}

TEST_CASE("pane view reports the words the pane draws, each where a press names it, and names what hides them") {
    InventoryStory s;
    s.append(12, "visible item");
    auto query = [&] { s.act([](loom::Mail& m) { m.send_to_role("zengine.workshop",
        PaneViewRequested{"zengine.inventory-pane", "inventory", 0, {}}); }); };
    query(); REQUIRE(s.hand->words.size() == 1);
    const auto rows = pane_rows(s.r, s.source);
    REQUIRE_FALSE(s.hand->words.back().words.empty());
    for (const auto& word : s.hand->words.back().words) {
        const auto at = external_press_at(s.r.session().panes, s.r.session().setup.active,
            screen_of(s.r.session()), s.source, s.r.session().pane_titles, word.space, word.x, word.y);
        REQUIRE(at.named);
        REQUIRE(at.row < static_cast<std::int64_t>(rows.size()));
        std::string text = word.text;
        while (!text.empty() && text.back() == ' ') text.pop_back();
        CHECK(rows[static_cast<std::size_t>(at.row)] == text); // the word on the row a press there names
    }
    const auto covered_by = [&](const std::string& what) {
        const auto& by = s.hand->words.back().covered.by;
        return std::find(by.begin(), by.end(), what) != by.end();
    };
    // ARRANGING OVER THE DESK: the reading names it, and says no word of the pane.
    s.r.session().arrange.open = true;
    query();
    REQUIRE(s.hand->words.size() == 2);
    CHECK(covered_by("arranging"));
    CHECK(s.hand->words.back().words.empty());
    CHECK(s.hand->words.back().covered.words > 0);
    s.r.session().arrange.open = false;
}

TEST_CASE("presets are independent partial data until filled and explicitly authorized by the current actor") {
    namespace md = zengine::message_draft;
    for (bool allowed : {false, true}) {
        InventoryStory s(allowed ? 127 : 111, true);
        auto& r = s.r;
        r.pick(composer_ref());
        const auto compose = r.session().panes.runtime.find(kComposerOffice, "compose")->kind;
        REQUIRE(r.session().keymap.app_row_of_id("desktop.deselect") != nullptr);
        REQUIRE(r.session().keymap.app_row_of_id("desktop.panes") != nullptr);
        stage_desk(r, [&](Setup& staged) {
            for (auto& p : staged.panes) if (p.ref.provider == kComposerOffice) {
                p.place = {pane_unit::kPixels, 85*gfx::kCellPx, 32*gfx::kCellPx};
                p.width = {pane_unit::kPixels, 80*gfx::kCellPx};
                p.height = {pane_unit::kPixels, 24*gfx::kCellPx};
            }
        });
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
        // THE CARRY'S WORDS ARE WORKSHOP'S while the copy is in hand, and Info says none of its own.
        REQUIRE_MESSAGE(r.last_notice().rfind("Carrying ", 0) == 0, r.last_notice());
        CHECK(s.shown(s.info).find("field copy") == std::string::npos);
        s.click(compose, row(compose, "revision"));
        REQUIRE_MESSAGE(s.shown(compose).find("Copied data into form") != std::string::npos, s.shown(compose));
        CHECK(r.last_notice().find("Carrying") == std::string::npos); // placed: nothing says carrying
        CHECK(s.shown(s.info).find("Carrying") == std::string::npos);
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

TEST_CASE("an Info field copy picked up by key is said carried by Workshop alone, so no sentence of it "
          "outlives the carry once it is put down") {
    InventoryStory s(127);
    s.acquire(); s.place();
    s.key(input::scan::kG, input::mod::kCtrl);
    REQUIRE_MESSAGE(s.r.last_notice().rfind("Carrying ", 0) == 0, s.r.last_notice());
    CHECK_MESSAGE(s.shown(s.info).find("Carrying") == std::string::npos, s.shown(s.info));
    CHECK(s.shown(s.info).find("Checking field acquisition authority") == std::string::npos);
    s.key(input::scan::kEscape); // put down
    CHECK_MESSAGE(s.r.last_notice().find("put down") != std::string::npos, s.r.last_notice());
    CHECK_MESSAGE(s.shown(s.info).find("Carrying") == std::string::npos, s.shown(s.info));
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
    const auto receiver_id = s.r.bus.register_weave(std::move(sink), grant, kRigBoard);
    receiver->zen_set_self(receiver_id);

    auto* terminal = s.r.mount_terminal();
    load::LoadPlan plan;
    load::ArtifactIntent artifact;
    artifact.stem = "zengine-terminal-pane";
    artifact.weave = load::WeaveIntent{"zengine.terminal"}; plan.artifacts.push_back(artifact);
    REQUIRE(s.r.run_plan(plan).ok);
    s.r.pick({"zengine.terminal", "terminal"});
    const auto kind = s.r.session().panes.runtime.find("zengine.terminal", "terminal")->kind;
    stage_desk(s.r, [&](Setup& staged) {
        for (auto& p : staged.panes) if (p.ref.provider == "zengine.terminal") {
            p.place = {pane_unit::kPixels, 2 * gfx::kCellPx, 31 * gfx::kCellPx};
            p.width = {pane_unit::kPixels, 80 * gfx::kCellPx};
            p.height = {pane_unit::kPixels, 24 * gfx::kCellPx};
        }
    });
    const auto sent = terminal->send(loom::Address::to_role(kRigBoard),
        RigNote::zen_name, 1, {{std::string("slot"), loom::FieldValue{std::string("score")}}, {std::string("text"), loom::FieldValue{std::string("captured original")}}});
    REQUIRE(sent);
    s.r.extent(181, 60);
    const auto rows = pane_rows(s.r, kind);
    std::int64_t at = -1;
    for (std::size_t i = 0; i < rows.size(); ++i) if (rows[i].find("SUBMITTED") != std::string::npos || rows[i].find(" -> ") != std::string::npos) at = static_cast<std::int64_t>(i);
    if (at < 0) for (std::size_t i=0;i<rows.size();++i) if(rows[i].find(RigNote::zen_name)!=std::string::npos) { at=static_cast<std::int64_t>(i); break; }
    REQUIRE_MESSAGE(at >= 0, s.shown(kind));
    // Capture alone never replays the command. A configured shortcut still needs its actor.
    int delivered = 0;
    const auto observer = s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered && e.schema_name == RigNote::zen_name && e.payload &&
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
    s.bind(s.entry("My captured command").reference, kRigBoard);
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
        const auto kind=s.r.session().panes.runtime.find("zengine.terminal","terminal")->kind;
        for(auto& p:s.r.session().setup.active.panes) if(p.ref.provider=="zengine.terminal") {
            p.place={pane_unit::kPixels,2*gfx::kCellPx,31*gfx::kCellPx};
            p.width={pane_unit::kPixels,80*gfx::kCellPx}; p.height={pane_unit::kPixels,24*gfx::kCellPx};
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
    DrawnNumbers numbers(s.r.bus);
    auto* terminal = s.r.mount_terminal();
    load::LoadPlan plan; load::ArtifactIntent artifact;
    artifact.stem = "zengine-terminal-pane"; artifact.weave = load::WeaveIntent{"zengine.terminal"};
    plan.artifacts.push_back(artifact); REQUIRE(s.r.run_plan(plan).ok);
    s.r.pick({"zengine.terminal", "terminal"});
    const auto kind = s.r.session().panes.runtime.find("zengine.terminal", "terminal")->kind;
    stage_desk(s.r, [&](Setup& staged) {
        for (auto& pane : staged.panes) if (pane.ref.provider == "zengine.terminal") {
            pane.place = {pane_unit::kPixels, 2 * gfx::kCellPx, 31 * gfx::kCellPx};
            pane.width = {pane_unit::kPixels, 24 * gfx::kCellPx};
            pane.height = {pane_unit::kPixels, 26 * gfx::kCellPx};
        }
    });
    REQUIRE(terminal->send(loom::Address::to_role(kRigBoard), RigNote::zen_name, 1,
        {{"slot", loom::FieldValue{std::string("score")}}, {"text", loom::FieldValue{std::string("wrapped")}}}));
    s.r.extent(181, 60);
    auto rows = pane_rows(s.r, kind); std::int64_t at = -1;
    for (std::size_t i=0;i<rows.size();++i) if(rows[i].rfind("^ RigNote",0)==0) at=static_cast<std::int64_t>(i);
    REQUIRE_MESSAGE(at>=0, s.shown(kind));
    REQUIRE(rows[static_cast<std::size_t>(at+1)].find(kRigBoard) != std::string::npos);
    auto press=s.button_at(kind,at+1,true), release=s.button_at(s.source,0,false);
    auto move=release; move.kind="PointerMoved"; move.dx=release.x-press.x; move.dy=release.y-press.y;
    s.batch({press,move,release});
    REQUIRE_MESSAGE(s.saved_entries().size()==1,(s.shown(kind)+s.shown(s.source)));
    CHECK(s.saved_entries().front().item.get("text")->as_text()=="wrapped");
    s.key(input::scan::kEscape); // keep the generated copy name
    // A PRESS THE WINDOW LANDED ON THE RECORD'S ROW BEFORE IT MOVED, as it drew it then: the part
    // named there, if any, and the transcript's number for that drawing.
    const std::int64_t aimed = at + 1;
    const std::string drawn = part_drawn_at(kind, aimed, 0);
    const std::int64_t drawing = numbers.of(s.r.session(), kind);
    REQUIRE(drawing != 0);
    // THE RECORD MOVES ON: a message received, and the transcript's next picture.
    s.r.bus.send(terminal->id(),loom::Message(loom::to_value(loom::Ack{}))); s.r.extent(182,60);
    REQUIRE(numbers.of(s.r.session(), kind) != drawing);
    press_as_drawn(s.r, kind, aimed, 0, drawn, drawing);
    CHECK_MESSAGE(s.shown(kind).find("That transcript") != std::string::npos, s.shown(kind));
    rows=pane_rows(s.r,kind); at=-1;
    for(std::size_t i=0;i<rows.size();++i) if(rows[i].rfind("v zen.Ack",0)==0) at=static_cast<std::int64_t>(i);
    REQUIRE_MESSAGE(at>=0,s.shown(kind)); s.menu(kind,at,0); s.click(s.source);
    REQUIRE_MESSAGE(s.saved_entries().size()==2,(s.shown(kind)+s.shown(s.source)));
    CHECK(s.saved_entries().back().item.schema().name()==loom::Ack::zen_name);
    CHECK(s.saved_entries().back().metadata.front().get("kind")->as_text()=="received");
}

TEST_CASE("a value dragged from Inventory onto Flow's canvas reaches Flow as a canvas drop where it "
          "was released, and Flow offers what it can become") {
    InventoryStory s(191, true, false, true);
    s.append(1, "A");
    REQUIRE(s.flow != 0);
    const auto* canvas = s.r.session().panes.external_pane(s.flow);
    REQUIRE(canvas != nullptr);
    REQUIRE(shows_canvas(*canvas));
    std::vector<PaneCanvasValueDrop> drops;
    const loom::ObserverId tap = s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered && e.payload &&
            e.schema_name == PaneCanvasValueDrop::zen_name)
            drops.push_back(loom::from_value<PaneCanvasValueDrop>(*e.payload));
    });
    auto press = s.button_at(s.source, 2, true);
    auto release = s.button_at(s.flow, 8, false);
    release.x += 40; // over the graph, right of Flow's rail
    auto move = release;
    move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
    s.batch({press, move, release});
    s.r.bus.remove_observer(tap);
    INFO(s.r.last_notice());
    INFO(s.shown(s.source));
    REQUIRE(drops.size() == 1);
    const ExternalPane& view = *s.r.session().panes.external_pane(s.flow);
    CHECK(drops[0].pane == "flow");
    CHECK(drops[0].area == pane_window_area(s.r.session().panes, *s.r.session().panes.runtime.of_kind(s.flow)));
    CHECK(drops[0].x > 0);
    CHECK(drops[0].x < view.placed_w);
    CHECK(drops[0].y > 0);
    CHECK(drops[0].y < view.placed_h);
    CHECK(drops[0].source_office == slots::kRole);
    CHECK(inv::decode_pair({reinterpret_cast<const char*>(drops[0].data.data()),
                            drops[0].data.size()}).item.get("count")->as_int() == 1);
    // FLOW HOLDS IT ON ITS OWN PAGE and offers its shape, which this definition does not have yet.
    const auto pictured = [&](const std::string& text) {
        for (const std::string& row : pane_rows(s.r, s.flow))
            if (row.find(text) != std::string::npos) return true;
        return false;
    };
    CHECK(pictured("Dropped story.RuntimeItem v1"));
    CHECK(pictured("[Declare story.RuntimeItem v1 as an accepted message]"));
    CHECK(pictured("[Cancel]"));
}

TEST_CASE("the rows two canvas panes show are each read off the pane's own plane where their bodies "
          "share a corner, the one laying the medium's ground and the one laying black") {
    InventoryStory s(191, false, false, /*with_flow=*/true, /*with_powers=*/true); // both at one corner
    REQUIRE(s.powers != 0);
    REQUIRE(s.flow != 0);
    REQUIRE(external_body_rect(s.r.session(), s.powers).x == external_body_rect(s.r.session(), s.flow).x);
    REQUIRE(external_body_rect(s.r.session(), s.powers).y == external_body_rect(s.r.session(), s.flow).y);
    const auto powers = pane_rows(s.r, s.powers);
    const auto flow = pane_rows(s.r, s.flow);
    std::string seen;
    for (const auto& row : powers) seen += "Powers | " + row + "\n";
    for (const auto& row : flow) seen += "Flow   | " + row + "\n";
    INFO(seen);
    REQUIRE_FALSE(powers.empty());
    REQUIRE_FALSE(flow.empty());
    CHECK(powers != flow);
    CHECK(std::find(flow.begin(), flow.end(), powers.front()) == flow.end());
}

TEST_CASE("an operator reference dragged out of Powers is kept by Inventory like any value: the "
          "identity and the two content ids the door's row carried") {
    InventoryStory s(191 | 64, true, false, false, true); // the actor may carry a value
    REQUIRE(s.powers != 0);
    const auto entries_before = s.saved_entries().size();
    // The Operators view, found by what it is for.
    s.click(s.powers, 0);
    s.key(input::scan::kTab);
    s.text("sum");
    const auto rows = pane_rows(s.r, s.powers);
    std::int64_t at = -1;
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (rows[i].find("math.add") != std::string::npos && at < 0) at = static_cast<std::int64_t>(i);
    INFO(s.shown(s.powers));
    REQUIRE(at >= 0);
    auto press = s.button_at(s.powers, at, true);
    press.x += 4; // on the power's name, past the selection mark
    auto release = s.button_at(s.source, 0, false);
    auto move = release;
    move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
    s.batch({press, move, release});
    INFO(s.r.last_notice());
    INFO(s.trace);
    const auto entries = s.saved_entries();
    REQUIRE(entries.size() == entries_before + 1);
    const auto& kept = entries.back().item;
    REQUIRE(zengine::op::is_reference(kept));
    const auto ref = zengine::op::decode_reference(kept);
    const auto* add = s.r.catalog.find("math.add");
    REQUIRE(add != nullptr);
    CHECK(ref.identity == "math.add");
    CHECK(ref.authored_in == add->inputs()->content_id());
    CHECK(ref.authored_out == add->outputs()->content_id());
}

namespace {
/// A press of `button` where a canvas pane drew a line beginning `start`, through the actor's
/// input session, as a hand would make it.
void press_text(InventoryStory& s, std::int64_t kind, const std::string& start, std::int64_t button = 1) {
    const auto* pane = s.r.session().panes.external_pane(kind);
    REQUIRE(pane != nullptr);
    for (const HeldText& t : held_texts(kind))
        if (t.text.rfind(start, 0) == 0) {
            input::InjectedEvent e;
            e.kind = "PointerButton"; e.button = button; e.pressed = true; e.space = input::space::kCells;
            e.x = (pane->placed_x + t.x) / gfx::kCellPx + 1;
            e.y = (pane->placed_y + t.y) / gfx::kCellPx;
            s.event(e);
            e.pressed = false;
            s.event(e);
            return;
        }
    std::string all;
    for (const HeldText& t : held_texts(kind)) all += t.text + " | ";
    FAIL("no line beginning `" << start << "` in " << all);
}
/// Choose the presented menu's row that reads `label`, with the hand.
void choose(InventoryStory& s, const std::string& label) {
    REQUIRE(menu_shown(s.r.session()));
    const auto line = presented_line_of(s.r, label);
    REQUIRE_MESSAGE(line >= 0, label);
    input::InjectedEvent e;
    e.kind = "PointerButton"; e.button = 1; e.pressed = true; e.space = input::space::kCells;
    e.x = context_cell_x(s.r.session());
    e.y = context_entry_cell_y(s.r.session(), static_cast<std::size_t>(line));
    s.event(e);
    e.pressed = false;
    s.event(e);
}
/// Where a canvas pane drew a line beginning `start`, as a hand's pointer event in cells.
input::InjectedEvent at_text(InventoryStory& s, std::int64_t kind, const std::string& start,
                             const char* what = "PointerButton") {
    const auto* pane = s.r.session().panes.external_pane(kind);
    REQUIRE(pane != nullptr);
    input::InjectedEvent e;
    e.kind = what; e.button = 1; e.space = input::space::kCells;
    for (const HeldText& t : held_texts(kind))
        if (t.text.rfind(start, 0) == 0) {
            e.x = (pane->placed_x + t.x) / gfx::kCellPx + 1;
            e.y = (pane->placed_y + t.y) / gfx::kCellPx;
            return e;
        }
    std::string all;
    for (const HeldText& t : held_texts(kind)) all += t.text + " | ";
    FAIL("no line beginning `" << start << "` in " << all);
    return e;
}
/// A local place of a canvas pane, in pixels, as a hand's pointer event in cells.
input::InjectedEvent at_local(InventoryStory& s, std::int64_t kind, std::int64_t x, std::int64_t y,
                              const char* what = "PointerButton") {
    const auto* pane = s.r.session().panes.external_pane(kind);
    REQUIRE(pane != nullptr);
    input::InjectedEvent e;
    e.kind = what; e.button = 1; e.space = input::space::kCells;
    e.x = (pane->placed_x + x) / gfx::kCellPx;
    e.y = (pane->placed_y + y) / gfx::kCellPx;
    return e;
}
/// The View Builder's design area, read off its picture: the ground the view's own picture brought.
zengine::view_builder::Area design_of(InventoryStory& s) {
    for (const HeldRect& r : held_rects(s.builder))
        if (r.role == gfx::role::kGround && (r.x != 0 || r.y != 0)) return {r.x, r.y, r.w, r.h};
    FAIL("the builder drew no design area");
    return {};
}
/// A hand's drag in one batch: pressed where `from` was drawn, moved, and released at `to`.
void drag_between(InventoryStory& s, input::InjectedEvent from, input::InjectedEvent to) {
    from.kind = "PointerButton"; from.pressed = true;
    to.kind = "PointerButton"; to.pressed = false;
    auto move = to;
    move.kind = "PointerMoved"; move.dx = to.x - from.x; move.dy = to.y - from.y;
    s.batch({from, move, to});
}
bool pictured(InventoryStory&, std::int64_t kind, const std::string& part) {
    for (const HeldText& t : held_texts(kind))
        if (t.text.find(part) != std::string::npos) return true;
    return false;
}
template <class T> void tell(InventoryStory& s, const char* office, const T& value) {
    s.r.bus.send_to_role(office, loom::Message(loom::to_value(value)));
    s.r.bus.drain_until_idle();
}
} // namespace

TEST_CASE("shapes cross between Flow and the View Builder by carry: what Flow says onto a label, "
          "and the builder's intent onto Flow as a declaration it offers") {
    InventoryStory s(255, false, false, true, false, true);
    REQUIRE(s.flow != 0);
    REQUIRE(s.builder != 0);
    namespace fp = zengine::flow_pane;
    namespace vb = zengine::view_builder;
    for (const auto& [action, args] : std::vector<std::pair<std::string, std::vector<std::string>>>{
             {"new", {"tally", "discard"}}, {"state-field", {"total", "Int", "required"}},
             {"emitted-message", {"Total"}}, {"emitted-field", {"0", "total", "Int", "required"}}})
        tell(s, fp::kRole, fp::FlowEdit{action, args});
    for (const auto& [action, args] : std::vector<std::pair<std::string, std::vector<std::string>>>{
             {"new", {"tally.panel", "discard"}}, {"add", {"number"}}, {"add", {"button"}},
             {"element", {"1", "count", "Count", "0", "28", "96", "24"}}, {"intent", {"1", "Count"}},
             {"add", {"label"}}, {"select", {"2"}}})
        tell(s, vb::kRole, vb::ViewEdit{action, args});
    press_text(s, s.flow, "[Messages]");
    REQUIRE(pictured(s, s.flow, "tally.Total {total: Int}"));

    // FLOW'S EMITTED MESSAGE ONTO THE LABEL: a right press offers to carry it, the choice carries
    // its shape, and a click on the label's row in the builder is where it lands.
    press_text(s, s.flow, "tally.Total {", 3);
    choose(s, "Carry tally.Total");
    INFO(s.r.last_notice());
    press_text(s, s.builder, "> label1  label");
    CHECK(pictured(s, s.builder, "shows tally.Total.total"));

    // THE BUILDER'S INTENT ONTO FLOW: its `says` line offers to carry it, and Flow, given it,
    // offers to declare it accepted.
    tell(s, vb::kRole, vb::ViewEdit{"select", {"1"}});
    press_text(s, s.builder, "says tally.panel.", 3);
    choose(s, "Carry tally.panel.Count");
    press_text(s, s.flow, "Emitted");
    CHECK(pictured(s, s.flow, "[Declare tally.panel.Count v1 as an accepted message]"));
}

TEST_CASE("a press on a canvas pane drags a value out as a prose press does: the hold ends as lost "
          "when the carry begins, the value lands where the hand lets go, and a click carries nothing") {
    InventoryStory s(255, false, false, true, false, true);
    REQUIRE(s.flow != 0);
    REQUIRE(s.builder != 0);
    namespace fp = zengine::flow_pane;
    namespace vb = zengine::view_builder;
    for (const auto& [action, args] : std::vector<std::pair<std::string, std::vector<std::string>>>{
             {"new", {"tally", "discard"}}, {"state-field", {"total", "Int", "required"}},
             {"emitted-message", {"Total"}}, {"emitted-field", {"0", "total", "Int", "required"}}})
        tell(s, fp::kRole, fp::FlowEdit{action, args});
    for (const auto& [action, args] : std::vector<std::pair<std::string, std::vector<std::string>>>{
             {"new", {"tally.panel", "discard"}}, {"add", {"label"}}, {"select", {"0"}}})
        tell(s, vb::kRole, vb::ViewEdit{action, args});
    press_text(s, s.flow, "[Messages]");
    REQUIRE(pictured(s, s.flow, "tally.Total {total: Int}"));
    const auto* flow = s.r.session().panes.runtime.of_kind(s.flow);
    REQUIRE(flow != nullptr);
    const auto flow_id = s.r.bus.role_holder(flow->provider);
    // WHAT FLOW IS TOLD OF ITS PRESS, by phase, by the window whose area it draws in.
    std::vector<std::int64_t> told_flow;
    std::vector<PaneCanvasValueDrop> drops;
    const loom::ObserverId tap = s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind != loom::EventKind::Delivered || !e.payload) return;
        if (e.target == flow_id && e.schema_name == gfx::AreaButtonChanged::zen_name)
            told_flow.push_back(loom::from_value<gfx::AreaButtonChanged>(*e.payload).pressed
                                    ? canvas_pointer::kPress
                                    : canvas_pointer::kRelease);
        if (e.target == flow_id && e.schema_name == gfx::AreaPointerMoved::zen_name)
            told_flow.push_back(canvas_pointer::kMove);
        if (e.target == flow_id && e.schema_name == gfx::AreaHoldEnded::zen_name)
            told_flow.push_back(canvas_pointer::kLost);
        if (e.schema_name == PaneCanvasValueDrop::zen_name)
            drops.push_back(loom::from_value<PaneCanvasValueDrop>(*e.payload));
    });
    RemoveObserver untap{s.r.bus, tap};

    // A CLICK ON THE MESSAGE: Flow asks to carry it under the press, and the carry begins, so the
    // press's hold ends as lost; the hand never moved, so nothing lands and nothing is held.
    auto click = at_text(s, s.flow, "tally.Total {");
    click.pressed = true; s.event(click);
    REQUIRE(told_flow.size() == 2);
    CHECK(told_flow[0] == canvas_pointer::kPress);
    CHECK(told_flow[1] == canvas_pointer::kLost);
    click.pressed = false; s.event(click);
    CHECK(told_flow.size() == 2);
    CHECK(drops.empty());
    CHECK_FALSE(pictured(s, s.builder, "shows tally.Total"));
    CHECK(s.r.last_notice().find("Dragging") == std::string::npos);

    // A DRAG FROM IT TO THE LABEL'S ROW, the hand's moments one by one: the motion is the
    // carry's, never Flow's, and the builder is told where the value was released.
    told_flow.clear();
    auto from = at_text(s, s.flow, "tally.Total {");
    auto to = at_text(s, s.builder, "> label1  label");
    from.pressed = true; s.event(from);
    auto move = to; move.kind = "PointerMoved"; move.dx = to.x - from.x; move.dy = to.y - from.y;
    s.event(move);
    CHECK(s.r.last_notice().find("Dragging") == std::string::npos);
    to.pressed = false; s.event(to);
    INFO(s.r.last_notice());
    // ITS BEFORE AND ITS AFTER ALONE, the arrow one the face draws (WL-NOTE-06).
    CHECK(s.r.last_notice() == "tally.Total: Flow -> View Builder");
    REQUIRE(told_flow.size() == 2);
    CHECK(told_flow[1] == canvas_pointer::kLost);
    REQUIRE(drops.size() == 1);
    CHECK(drops[0].pane == vb::kPane);
    CHECK(drops[0].source_office == flow->provider);
    CHECK(pictured(s, s.builder, "shows tally.Total.total"));

    // THE SAME DRAG IN ONE BATCH: the release reaches Workshop before Flow asks to carry, so Flow
    // sees its own release, and the carry still lands where the hand let go.
    told_flow.clear();
    drops.clear();
    tell(s, vb::kRole, vb::ViewEdit{"unshow", {"0"}});
    REQUIRE_FALSE(pictured(s, s.builder, "shows tally.Total"));
    drag_between(s, at_text(s, s.flow, "tally.Total {"), at_text(s, s.builder, "> label1  label"));
    REQUIRE(told_flow.size() == 3);
    CHECK(told_flow[2] == canvas_pointer::kRelease);
    REQUIRE(drops.size() == 1);
    CHECK(pictured(s, s.builder, "shows tally.Total.total"));
}

TEST_CASE("shapes cross between Flow and the View Builder by dragging: the intent onto Flow, and Flow's "
          "message onto a label on the design canvas, which marks where it would land") {
    InventoryStory s(255, false, false, true, false, true);
    REQUIRE(s.flow != 0);
    REQUIRE(s.builder != 0);
    namespace fp = zengine::flow_pane;
    namespace vb = zengine::view_builder;
    for (const auto& [action, args] : std::vector<std::pair<std::string, std::vector<std::string>>>{
             {"new", {"tally", "discard"}}, {"state-field", {"total", "Int", "required"}},
             {"emitted-message", {"Total"}}, {"emitted-field", {"0", "total", "Int", "required"}}})
        tell(s, fp::kRole, fp::FlowEdit{action, args});
    for (const auto& [action, args] : std::vector<std::pair<std::string, std::vector<std::string>>>{
             {"new", {"tally.panel", "discard"}}, {"add", {"number"}}, {"add", {"button"}},
             {"element", {"1", "count", "Count", "0", "28", "96", "24"}}, {"intent", {"1", "Count"}},
             {"add", {"label"}}, {"select", {"1"}}})
        tell(s, vb::kRole, vb::ViewEdit{action, args});
    press_text(s, s.flow, "[Messages]");
    REQUIRE(pictured(s, s.flow, "tally.Total {total: Int}"));

    // THE INTENT ONTO FLOW: dragged from what `count` says, and let go over Flow's messages.
    drag_between(s, at_text(s, s.builder, "says tally.panel."), at_text(s, s.flow, "Emitted"));
    INFO(s.r.last_notice());
    CHECK(pictured(s, s.flow, "[Declare tally.panel.Count v1 as an accepted message]"));
    press_text(s, s.flow, "[Declare tally.panel.Count v1 as an accepted message]");
    press_text(s, s.flow, "[Messages]");
    REQUIRE(pictured(s, s.flow, "tally.panel.Count"));

    // FLOW'S MESSAGE ONTO THE LABEL where the view draws it, a moment at a time: over the label the
    // builder marks where it would land, and the label shows it once the hand lets go.
    const auto area = design_of(s);
    const auto label = vb::element_area(area, zengine::view::Element{"label1", zengine::view::Kind::label, "", 0, 56, 192, 24, ""});
    auto from = at_text(s, s.flow, "tally.Total {");
    from.pressed = true;
    s.event(from);
    auto over = at_local(s, s.builder, label.x + 2 * gfx::kCellPx, label.y + gfx::kCellPx / 2, "PointerMoved");
    over.dx = over.x - from.x; over.dy = over.y - from.y;
    s.event(over);
    const auto landing = [&] {
        const std::vector<HeldRect> rects = held_rects(s.builder);
        return std::any_of(rects.begin(), rects.end(), [&](const auto& r) {
            return r.role == gfx::role::kAccent && r.y == label.y + label.h && r.h == 2 * gfx::kCellPx;
        });
    };
    CHECK(landing());
    CHECK_FALSE(pictured(s, s.builder, "shows tally.Total"));
    auto to = over;
    to.kind = "PointerButton"; to.pressed = false;
    s.event(to);
    CHECK(pictured(s, s.builder, "shows tally.Total.total"));
    CHECK(pictured(s, s.builder, "label1: waiting"));
    CHECK_FALSE(landing());
}

TEST_CASE("escape: Loaded and Powers let go of the row a weaver chose first, and are put down next") {
    // Escape's default in a pane that holds the keys: the first Escape drops the chosen row and
    // keeps the pane, the second finds nothing selected and hands the Escape back (WL-ARR-15).
    InventoryStory s(191, false, false, false, true);
    (void)mount_desktop(s.r);
    s.r.pick({intro::kIntrospectionRole, intro::kLoadedPane});
    const RuntimePane* loaded = s.r.session().panes.runtime.find(intro::kIntrospectionRole, intro::kLoadedPane);
    REQUIRE(loaded != nullptr);
    REQUIRE(s.r.session().panes.has(loaded->kind));
    // A loaded weave's row in Loaded: the first row a press there marks.
    std::int64_t entry_row = -1;
    for (std::int64_t row = 0; row < 12 && entry_row < 0; ++row) {
        const std::string before = s.shown(loaded->kind);
        s.click(loaded->kind, row);
        if (s.shown(loaded->kind) != before) entry_row = row;
    }
    REQUIRE_MESSAGE(entry_row >= 0, s.shown(loaded->kind));
    struct Chosen {
        std::int64_t kind;
        std::function<void()> choose;
    };
    for (const Chosen& c : {Chosen{loaded->kind, [&] { s.click(loaded->kind, entry_row); }},
                            Chosen{s.powers, [&] { s.click(s.powers); s.key(input::scan::kTab);
                                                   s.key(input::scan::kDown); }}}) {
        CAPTURE(c.kind);
        c.choose();
        REQUIRE(s.r.session().panes.keyboard == c.kind);
        const std::string marked = s.shown(c.kind);
        s.key(input::scan::kEscape);
        CHECK(s.r.session().panes.selected == c.kind);
        CHECK(s.r.session().panes.keyboard == c.kind);
        CHECK_MESSAGE(s.shown(c.kind) != marked, marked);
        s.key(input::scan::kEscape);
        CHECK(s.r.session().panes.selected == kNoPaneKind);
        CHECK(s.r.session().panes.keyboard == kNoPaneKind);
        CHECK(s.r.session().panes.has(c.kind));
    }
}

namespace {

/// An office that counts the story's commands it is sent: where a hotkey's command lands.
struct SinkState {
    std::int64_t heard = 0;
    ZEN_SHAPE(SinkState, 1, ZEN_FIELD(heard));
};
class StorySink final : public loom::Weave {
public:
    StorySink()
        : shape_(loom::SchemaBuilder("story.RuntimeItem", 1).field("count", loom::Kind::Int).build()) {}
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override { return {shape_}; }
    void handle(const loom::Message& in, loom::Bus&) override {
        if (loom::same_identity(*shape_, in.payload.schema())) ++state_.heard;
    }
    loom::Value snapshot() const override { return loom::to_value(state_); }
    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(0));
        v.set("revive_from_last_good", loom::Cell::boolean(false));
        return v;
    }
    void revive(const loom::Value& state) override { state_ = loom::from_value<SinkState>(state); }
    SinkState state_;

private:
    std::shared_ptr<const loom::Schema> shape_;
};

} // namespace

TEST_CASE("hotkeys: many hotkeys on at once, each reaching its own command, nothing refusing first") {
    // Nothing between a configured hotkey and its command refuses first: Inventory, one provider's
    // shortcuts at Desktop, the application rows and the pane's own rows. Sixty-four runs the whole
    // chain at a fraction of the full bound's cost, which is one pane edit per hotkey; each bound
    // is pinned at its full size by its own case. The chords are the keymap's own names under
    // modifier sets no host row uses.
    InventoryStory s(191 | 512, true);
    std::vector<std::pair<std::int64_t, std::int64_t>> chords;
    using input::mod::kAlt;
    using input::mod::kCtrl;
    using input::mod::kShift;
    using input::mod::kSuper;
    for (const std::int64_t mods : {kSuper, kSuper | kCtrl, kSuper | kAlt, kSuper | kShift,
                                    kSuper | kCtrl | kAlt, kSuper | kCtrl | kShift,
                                    kSuper | kAlt | kShift, kSuper | kCtrl | kAlt | kShift,
                                    kCtrl | kAlt | kShift}) {
        for (std::int64_t sc = 1; sc < 512; ++sc) {
            if (key_name_of(sc) != nullptr) chords.emplace_back(sc, mods);
        }
    }
    const std::size_t count = 64;
    REQUIRE(chords.size() > slots::kMaxConfiguredHotkeys);
    const auto padded = [](std::size_t i) {
        std::string n = std::to_string(i);
        return std::string(4 - n.size(), '0') + n;
    };
    std::vector<inv::InventoryReference> refs;
    for (std::size_t i = 0; i < count; ++i) s.append(static_cast<std::int64_t>(i), "cmd" + padded(i));
    s.act([](loom::Mail& m) { m.send_to_role(inv::kInventoryRole, inv::InventoryList{}); });
    for (const auto& e : s.hand->listing.entries) {
        if (e.label.rfind("cmd", 0) == 0) refs.push_back(e.reference);
    }
    REQUIRE(refs.size() == count);
    for (std::size_t i = 0; i < count; ++i) {
        slots::InventoryViewEdit op;
        op.operation = "bind"; op.entry = refs[i]; op.text = "story.target." + padded(i);
        op.scancode = chords[i].first; op.modifiers = chords[i].second;
        s.change(op);
        op = {}; op.operation = "enable"; op.entry = refs[i]; op.enabled = true;
        s.change(op);
    }
    REQUIRE(s.layout().bindings.size() == count);
    // ALL ON AT ONCE: one registration of every one of them.
    s.context("inventory", true);
    REQUIRE(slots::shortcuts(s.layout()).size() == count);
    // PRESSED ON THE WEAVER'S OWN KEYBOARD, whose commands need no guest's authority: the first,
    // a middle and the last chord each send their own command to their own office, and only there.
    std::vector<std::pair<std::size_t, StorySink*>> sinks;
    for (const std::size_t i : {std::size_t{0}, count / 2, count - 1}) {
        auto made = std::make_unique<StorySink>();
        sinks.emplace_back(i, made.get());
        (void)s.r.bus.register_weave(std::move(made), loom::Grant{}, "story.target." + padded(i));
    }
    for (const auto& [i, sink] : sinks) {
        CAPTURE(i);
        input::KeyPressed pressed;
        pressed.scancode = chords[i].first;
        pressed.modifiers = chords[i].second;
        s.physical->push_back(pressed);
        s.pump_physical();
        CHECK_MESSAGE(sink->state_.heard == 1, (s.shown(s.source) + s.r.last_notice()));
    }
    for (const auto& [i, sink] : sinks) {
        CHECK(sink->state_.heard == 1); // and no chord reached another's office
    }
}

TEST_CASE("views: every view Inventory makes is offered beside the other panes, and one desk seats them all") {
    // Each view is a pane Workshop's catalog holds, beside the panes this story loads, and a desk
    // naming all of them is applied and every view seated. Applying it is a repaint per pane, so it
    // is also the case that keeps that repaint's cost from growing with the desk's square.
    InventoryStory s(191, true, false, false, true, true);
    s.r.extent(156, 60);
    for (std::size_t i = 0; i < slots::kMaxPortableViews; ++i)
        (void)s.create(i % 3 == 0 ? "single" : (i % 3 == 1 ? "row" : "column"));
    const auto views = s.layout().views;
    REQUIRE(views.size() == slots::kMaxPortableViews);
    for (const auto& v : views) REQUIRE(s.r.session().panes.runtime.find(slots::kRole, v.id) != nullptr);
    CHECK(s.r.session().panes.runtime.entries.size() + kBuiltinPaneCount > 32);
    Setup desk;
    desk.name = "Thirty-six views";
    const auto seat = [&](const PaneRef& ref, std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) {
        REQUIRE(add_pane(desk, ref));
        for (SetupPane& row : desk.panes) {
            if (row.ref == ref) {
                row.place = {pane_unit::kPixels, x * gfx::kCellPx, y * gfx::kCellPx};
                row.width = {pane_unit::kPixels, w * gfx::kCellPx};
                row.height = {pane_unit::kPixels, h * gfx::kCellPx};
            }
        }
    };
    REQUIRE(add_pane(desk, PaneRef{"zengine.workshop", "layouts"}));
    seat({slots::kRole, "inventory"}, 1, 2, 50, 22);
    seat({"zengine.info", "info"}, 122, 2, 33, 22);
    for (std::size_t i = 0; i < views.size(); ++i) {
        const auto col = static_cast<std::int64_t>(i % 6), line = static_cast<std::int64_t>(i / 6);
        seat({slots::kRole, views[i].id}, 1 + col * 25, 29 + line * 5, 24, 5);
    }
    REQUIRE(desk.panes.size() > 32);
    const Written judged = check_setup(desk);
    REQUIRE_MESSAGE(judged.accepted, judged.refusal);
    // THE LAYOUTS' OWNER KNOWS EVERY PANE OF IT FOR ONE WORKSHOP PRESENTS, from Workshop's inventory.
    for (const SetupPane& row : desk.panes) {
        CHECK_MESSAGE(s.r.owner->layouts().presentable(row.ref), ref_text(row.ref));
    }
    // As the demo guest's door would: the layouts' owner's setup application, whole.
    (void)s.r.bus.send_to_role("zengine.layouts",
                               loom::Message(loom::to_value(SetupApplyRequested{setup_persist::to_text(desk)})));
    s.r.bus.drain_until_idle();
    CHECK(s.r.session().setup.active.name == "Thirty-six views");
    for (const auto& v : views) {
        CAPTURE(v.id);
        const RuntimePane* row = s.r.session().panes.runtime.find(slots::kRole, v.id);
        REQUIRE(row != nullptr);
        CHECK(s.r.session().panes.has(row->kind));
    }
}

TEST_CASE("views: making a view offers only that view, so a view already on the desk never stands blank") {
    InventoryStory s;
    const std::string first = s.create("single");
    const RuntimePane* row = s.r.session().panes.runtime.find(slots::kRole, first);
    REQUIRE(row != nullptr);
    const std::int64_t kind = row->kind;
    REQUIRE(s.r.session().panes.has(kind)); // launched as it was made
    // IN THE COLUMN, so the second view, opened at the room's centre, stands clear of it.
    stage_desk(s.r, [&row](Setup& desk) {
        if (SetupPane* seat = pane_of(desk, PaneRef{row->provider, row->pane})) {
            seat->place = PanePlace{};
            seat->width = PaneSize{};
            seat->height = PaneSize{};
        }
    });
    s.r.key(input::scan::kUnknown);
    const std::vector<std::string> before = pane_rows(s.r, kind);
    REQUIRE_FALSE(before.empty());
    // A SECOND VIEW MADE: every showing of the window meanwhile shows the first view as it was.
    s.r.windows.watch();
    (void)s.create("row");
    const std::vector<DeskShown> showings = s.r.windows.showings();
    REQUIRE_FALSE(showings.empty());
    std::size_t blank = 0;
    for (const DeskShown& shown : showings) {
        if (canvas_rows_shown(s.r.session(), shown, kind) != before) {
            ++blank;
        }
    }
    CHECK(blank == 0);
    const auto views = s.layout().views;
    REQUIRE(views.size() == 2);
    const RuntimePane* second = s.r.session().panes.runtime.find(slots::kRole, views.back().id);
    REQUIRE(second != nullptr);
    CHECK(s.r.session().panes.has(second->kind));
}

TEST_CASE("a right press and a drop the window landed on Inventory's rows before another entry stood there are refused in words and change nothing, and a right press on the entry still there offers its menu") {
    InventoryStory s(191, true);
    s.append(1, "A");
    const auto before = held_reading(s.source).parts;
    s.append(2, "B"); // a row more: an entry where none was
    const std::int64_t a = s.row_of(s.source, "A");
    const std::int64_t b = s.row_of(s.source, "B");
    REQUIRE(a >= 0);
    REQUIRE(b >= 0);
    const std::string was_at_b = part_drawn_at(before, b, 2);
    REQUIRE(was_at_b != part_drawn_at(s.source, b, 2));
    press_as_drawn(s.r, s.source, b, 2, was_at_b, 0, 3);
    CHECK_MESSAGE(s.shown(s.source).find("That picture moved") != std::string::npos, s.shown(s.source));
    CHECK_FALSE(s.r.session().presented.open);
    // ...A DROP LANDED THERE THEN, judged likewise.
    const auto entries = s.saved_entries().size();
    const RuntimePane* pane = s.r.session().panes.runtime.of_kind(s.source);
    REQUIRE(pane != nullptr);
    const std::string area = pane_window_area(s.r.session().panes, *pane);
    s.r.bus.office_send_to_role_as(s.r.bus.role_holder(kWorkshopProvider), kWorkshopProvider, slots::kRole,
        loom::Message(loom::to_value(PaneCanvasValueDrop{pane->pane, area, 2 * gfx::kCellPx + 6,
            b * gfx::kCellPx + 6, s.pair(9), "", "", "", area, was_at_b, 0})));
    s.r.bus.drain_until_idle();
    CHECK_MESSAGE(s.shown(s.source).find("Drop picture changed") != std::string::npos, s.shown(s.source));
    CHECK(s.saved_entries().size() == entries);
    // A RIGHT PRESS ON A, WHICH STANDS WHERE IT STOOD, opens A's menu.
    REQUIRE(part_drawn_at(before, a, 2) == part_drawn_at(s.source, a, 2));
    press_as_drawn(s.r, s.source, a, 2, part_drawn_at(s.source, a, 2), 0, 3);
    CHECK(s.r.session().presented.open);
}

TEST_CASE("portable slots: an entry's transfer placed where the window drew a view's rows before another entry moved in there moves nothing and says it expired") {
    InventoryStory s(191, true);
    s.append(1, "A");
    s.append(2, "B");
    const auto a = s.entry("A").reference;
    const auto row = s.create("row");
    const auto kind = s.r.session().panes.runtime.find(slots::kRole, row)->kind;
    stage_desk(s.r, [&](Setup& staged) {
        for (auto& p : staged.panes) if (p.ref.pane == row) {
            p.place = {pane_unit::kPixels, 2 * gfx::kCellPx, 34 * gfx::kCellPx};
            p.width = {pane_unit::kPixels, 72 * gfx::kCellPx};
            p.height = {pane_unit::kPixels, 10 * gfx::kCellPx};
        }
    });
    s.r.extent(179, 60); s.r.extent(180, 60); // a same-size extent reseats nothing: the view is placed now
    // What the window showed of the row view while it held nothing, then B moved into it.
    const auto first = held_reading(kind).parts;
    slots::InventoryViewEdit op; op.operation = "move"; op.view = row; op.entry = s.entry("B").reference;
    s.change(op);
    REQUIRE(slots::placed(s.layout(), s.entry("B").reference) == row);
    // A real drag of A begins in main Inventory, and Inventory hands Workshop its transfer token.
    std::string token;
    RemoveObserver watch{s.r.bus, s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered &&
            e.schema_name == v2::PaneValueCarryRequested::zen_name && e.payload)
            token = e.payload->get("token")->as_text();
    })};
    const std::int64_t at = row_saying(s, s.source, "A : ");
    REQUIRE_MESSAGE(at >= 0, s.shown(s.source));
    auto press = s.button_at(s.source, at, true), move = s.button_at(s.source, at + 3, false);
    move.kind = "PointerMoved"; move.dx = 0; move.dy = 3;
    s.batch({press, move});
    REQUIRE_MESSAGE(!token.empty(), s.shown(s.source));
    // That transfer, placed where the window drew the row view before B came: refused.
    const auto before = s.layout();
    const std::string area = pane_window_area(s.r.session().panes, *s.r.session().panes.runtime.of_kind(kind));
    // WHERE B STANDS IN THE ROW VIEW NOW, and what the window drew there before it came.
    std::int64_t b_row = -1, b_column = -1;
    for (const auto& [name, part] : held_reading(kind).parts) {
        if (b_row < 0 && part_drawn_at(first, part.row, part.column) != name) {
            b_row = part.row;
            b_column = part.column;
        }
    }
    REQUIRE_MESSAGE(b_row >= 0, s.shown(kind));
    const std::string drawn = part_drawn_at(first, b_row, b_column);
    s.r.bus.office_send_to_role_as(s.r.bus.role_holder(kWorkshopProvider), kWorkshopProvider, slots::kRole,
        loom::Message(loom::to_value(PaneCanvasValueDrop{row, area, b_column * gfx::kCellPx + 6,
            b_row * gfx::kCellPx + 6, s.pair(1), slots::kRole, "inventory", token, area, drawn, 0})));
    s.r.bus.drain_until_idle();
    const std::string said = s.shown(kind) + s.shown(s.source);
    CHECK_MESSAGE(said.find("That slot transfer expired") != std::string::npos, said);
    CHECK(slots::placed(s.layout(), a) == slots::placed(before, a));
}

TEST_CASE("a value released on Flow before its carry is answered carries the window's word from its release, though Flow repainted meanwhile") {
    InventoryStory s(191, true, false, true);
    s.append(1, "A");
    REQUIRE(s.flow != 0);
    const auto released_on = held_reading(s.flow).parts;
    std::vector<PaneCanvasValueDrop> drops;
    unsigned buttons = 0;
    const loom::WeaveId workshop = s.r.workshop_id;
    const loom::ObserverId tap = s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind != loom::EventKind::Delivered) return;
        if (e.payload && e.schema_name == PaneCanvasValueDrop::zen_name)
            drops.push_back(loom::from_value<PaneCanvasValueDrop>(*e.payload));
        // THE RELEASE HANDLED BY WORKSHOP, its carry not yet answered: the turn stops here.
        if (e.target == workshop && e.schema_name == gfx::AreaPressed::zen_name && ++buttons == 2)
            s.r.bus.stop();
    });
    auto press = s.button_at(s.source, 2, true);
    auto release = s.button_at(s.flow, 8, false);
    release.x += 40; // over the graph, right of Flow's rail
    auto move = release;
    move.kind = "PointerMoved"; move.dx = release.x - press.x; move.dy = release.y - press.y;
    s.hand->next = [&](loom::Mail& m) {
        m.send_to_role(input::kInputRole, input::InjectInput{s.hand->session, {press, move, release}});
    };
    s.r.bus.send(s.hand_id, loom::Message(loom::to_value(InventoryHandDo{})));
    for (int turn = 0; turn < 64 && buttons < 2 && s.r.bus.pending() != 0; ++turn) (void)s.r.bus.pump_pending();
    s.hand->next = {};
    REQUIRE(buttons == 2);
    REQUIRE(drops.empty());
    // FLOW REPAINTS before the carry is answered: typing on its search line.
    const auto* flow = s.r.session().panes.runtime.of_kind(s.flow);
    REQUIRE(flow != nullptr);
    REQUIRE(s.r.bus.office_send_to_role_as(workshop, kWorkshopProvider, flow->provider,
        loom::Message(loom::to_value(PaneTextInput{flow->pane, "m"}))).valid());
    s.r.bus.drain_until_idle();
    s.r.bus.remove_observer(tap);
    INFO(s.r.last_notice());
    REQUIRE(drops.size() == 1);
    CHECK(drops[0].under == pane_window_area(s.r.session().panes, *flow));
    CHECK(drops[0].part == part_drawn_at(released_on, drops[0].y / gfx::kCellPx, drops[0].x / gfx::kCellPx));
}

TEST_CASE("a graphics window shows Inventory and Info in the areas Workshop places them in") {
    namespace pw = zengine::testing::pane_window;
    InventoryStory s;
    pw::Window w(s.r.bus);
    for (const auto& [office, pane] : std::vector<std::pair<std::string, std::string>>{
             {slots::kRole, "inventory"}, {"zengine.info", "info"}}) {
        CAPTURE(office);
        const RuntimePane* row = s.r.session().panes.runtime.find(office, pane);
        REQUIRE(row != nullptr);
        // THE AREA WORKSHOP PLACES the pane in is the one its office draws in, given its room.
        const std::string area = pane_window_area(s.r.session().panes, *row);
        const std::size_t heard = w.weave().heard.size();
        w.room(area, office, 80, 20);
        REQUIRE(w.weave().heard.size() > heard);
        CHECK(w.weave().heard.back().area == area);
        CHECK(w.weave().heard.back().whole);
        const std::vector<std::string> shown = w.rows(area);
        CHECK(std::any_of(shown.begin(), shown.end(),
                          [](const std::string& line) { return !line.empty(); }));
    }
}
