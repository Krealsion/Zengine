// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "activation/activation.hpp"
#include "inventory/pane_client.hpp"
#include "presentation.hpp"
#include "command.hpp"
#include "toolbox.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/setup_control.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include <zen/kernel/export.hpp>
#include <map>
#include <random>
#include <sstream>
namespace {
namespace ws = zengine::workshop;
namespace inv = zengine::inventory;
namespace slots = zengine::inventory_pane;
namespace input = zengine::input;
namespace component = zengine::component;
constexpr const char* office = slots::kRole;
constexpr const char* pane = "inventory";
struct InventoryPaneState {
    slots::InventoryViews layout;
    ZEN_SHAPE(InventoryPaneState, 2, ZEN_FIELD(layout));
};
class InventoryPane : public loom::WeaveBase<InventoryPane, InventoryPaneState,
    loom::Accept<ws::PaneResetRequested, loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom, ws::PaneButton,
        ws::v3::PanePressed, ws::PaneDragged, ws::PaneWheel, ws::PaneKey, ws::PaneTextInput,
        ws::PaneValueDrop, ws::v2::PaneValueDrop, ws::PaneActionRequested, ws::PaneMenuAnswered,
        ws::PaneOperationAnswered, ws::PaneCarryAnswered, ws::PaneShortcutsAnswered,
        ws::PaneShortcutsRequested, ws::PaneShortcutsWithdrawn, ws::PaneLaunchAnswered,
        slots::InventoryViewEdit, slots::InventoryViewsRequested, inv::InventoryEntry,
        inv::v2::InventoryListed, inv::InventoryFolderState, inv::InventoryChanged,
        inv::v2::InventorySnapshot, inv::InventoryRestored,
        slots::InventoryToolboxSave, slots::InventoryToolboxRestore,
        loom::Ack, loom::Refused, loom::DispatchRefused>,
    loom::Emit<ws::v2::PaneOffered, ws::PaneActions, ws::v3::PaneContent, ws::PaneMenuRequested,
        ws::PaneKeyboardRequested, ws::PaneOperationRequested, ws::PaneCarryRequested,
        ws::PaneValueCarryRequested, ws::v2::PaneValueCarryRequested, ws::PaneShortcuts,
        ws::PaneLaunchRequested, slots::InventoryViews, inv::v2::InventoryList, inv::InventoryRead,
        inv::v2::InventoryAdd, inv::InventoryRename, inv::InventoryRemove, inv::InventoryFile,
        inv::InventoryFolderCreate, inv::InventoryFolderRename, inv::InventoryFolderMove,
        inv::InventoryFolderRemove,
        inv::v2::InventorySnapshotRequested, inv::v2::InventoryRestore, slots::InventoryToolboxFinished>> {
    enum class Mode { drag, copy, reference, change, store, duplicate, run, organize };
    enum class Editing { none, rename, duplicate, binding, toolbox_save, toolbox_restore, toolbox_confirm,
                         folder_create, folder_rename };
    struct Read {
        inv::InventorySummary target; std::string from; Mode mode;
        std::uint64_t gesture, correlation; loom::Ticket ticket;
        std::optional<slots::SlotBinding> binding;
        inv::InventoryFolderReference folder; // where a duplicate is filed: its source's folder
    };
    // `folder` is the source's folder when the drag began: filing it names that as `from`.
    struct Transfer { inv::InventorySummary target; std::string from; std::uint64_t layout; inv::InventoryFolderReference folder; };
    // An item picked to move (Ctrl+X): identities captured at the pick, never a row or name.
    struct Pick {
        std::string key, label;
        std::optional<inv::InventoryReference> entry; inv::InventoryFolderReference from;
        std::optional<inv::InventoryFolderState> folder;
    };
    struct Edit { slots::InventoryViewEdit op; std::uint64_t correlation; loom::Ticket ticket; };
    struct Registration {
        slots::InventoryViews candidate; loom::DeferredAnswer due;
        std::uint64_t correlation; loom::Ticket ticket; std::string launch;
    };
public:
    void on(const slots::InventoryToolboxSave& request, loom::Mail& m) {
        if (busy() || editing_ != Editing::none) { (void)m.answer(loom::Refused{"Finish the current Inventory operation or editor first"}); return; }
        toolbox_.begin(request.path, false, false, state_.layout, m, asks_, m.defer_answer());
        notice_ = toolbox_.notice; draw(m);
    }
    void on(const slots::InventoryToolboxRestore& request, loom::Mail& m) {
        if (busy() || editing_ != Editing::none) { (void)m.answer(loom::Refused{"Finish the current Inventory operation or editor first"}); return; }
        toolbox_.begin(request.path, true, request.replace, state_.layout, m, asks_, m.defer_answer());
        notice_ = toolbox_.notice; draw(m);
    }
    void on(const inv::v2::InventorySnapshot& snapshot, loom::Mail& m) {
        if (toolbox_.hear(snapshot, m, asks_)) { notice_ = toolbox_.notice; draw(m); }
    }
    void on(const inv::InventoryRestored& answer, loom::Mail& m) {
        if (!toolbox_.hear(answer, m, asks_)) return;
        if (toolbox_.restored) {
            state_.layout = std::move(*toolbox_.restored); toolbox_.restored.reset();
            ++layout_revision_; shortcuts_live_ = false; transfers_.clear(); target_ = {}; pick_.reset();
            for (auto& [id, v] : views_) { (void)id; v.selected.clear(); v.map.clear(); }
            list_ = {}; refresh_again_ = false;
            announce_views(m); declare_all(m); refresh(m);
        }
        notice_ = toolbox_.notice; draw(m);
    }
    void on(const ws::PaneResetRequested& request, loom::Mail& m) {
        if (!known(request.pane) || busy()) { (void)m.answer(loom::Refused{"Inventory reset needs its pane and no pending operation"}); return; }
        editing_=Editing::none; remove_armed_=false; line_=component::TextBox{}; pick_.reset();
        auto& v=views_[request.pane]; v.selected.clear(); v.order=0; v.wheel=0;
        if(request.pane==pane) browser_.open({},listing_,v.selected);
        notice_.clear(); declare_all(m); refresh(m); (void)m.answer(loom::Ack{});
    }
    void on(const loom::Activated& a, loom::Mail& m) { if (activation_.accept(m,a)) announce(m); }
    void on(const ws::PaneCatalogRequested&, loom::Mail& m) { if (host(m)) announce(m); }
    void on(const ws::PaneRoom& room, loom::Mail& m) {
        if (!host(m) || !known(room.pane)) return;
        auto& v=views_[room.pane]; v.rows=room.rows; v.columns=room.columns; v.map.clear(); refresh(m); draw(m);
    }
    void on(const inv::InventoryChanged&, loom::Mail& m) { if (m.authored_from_role(inv::kInventoryRole)) refresh(m); }
    void on(const inv::v2::InventoryListed& a, loom::Mail& m) {
        if (!list_.valid() || !m.answers_ask() || m.correlation()!=list_ask_) return;
        list_={};
        try { listing_=slots::listing(a); }
        catch(const std::exception& e) { notice_=std::string("Inventory's folder answer was refused: ")+e.what(); draw(m); return; }
        // Navigation follows identity: a moved folder is followed, a vanished one falls back.
        auto& v=views_[pane]; const auto moved=browser_.reconcile(listing_,v.selected);
        if(!moved.empty()) notice_=moved;
        if(pick_ && !picked_here()) { pick_.reset(); notice_="The item picked to move is no longer here"; declare_all(m); }
        if (refresh_again_) { refresh_again_=false; refresh(m); }
        draw(m);
    }
    void on(const slots::InventoryViewsRequested&, loom::Mail& m) { (void)m.answer(state_.layout); }
    void on(const slots::InventoryViewEdit& edit, loom::Mail& m) {
        if (busy() || editing_ != Editing::none) { (void)m.answer(loom::Refused{"Finish the current inventory operation or editor first"}); return; }
        apply(edit,m,m.defer_answer());
    }
    void on(const ws::v3::PanePressed& p, loom::Mail& m) {
        if (!host(m) || !known(p.pane) || editing_!=Editing::none) return;
        remove_armed_=false;
        const auto pressed=std::exchange(pressed_folder_,std::string{});
        // Navigation is local view state: it works while an operation waits, and never touches
        // the pending operation's target, mode or view.
        if(const auto* meaning=hit(p.pane,p.row,p.column,p.picture); meaning && p.pane==pane) {
            if(*meaning==slots::kUpControl) { go_up(); draw(m); return; }
            if(*meaning==slots::kMoveHereControl) { move_here(m); draw(m); return; }
            if(const auto f=slots::folder_meant(*meaning,listing_.owner)) {
                // A press selects a folder and a second press on it opens it. A folder selected
                // any other way (keys, or remembered on climbing back) is only selected by one
                // press, so pointing at a folder to rename or move it never opens it.
                if(meaning->starts_with("crumb:")) go_to(*f);
                else if(pressed==*meaning && p.keys_went_here) open_folder(*f);
                else {
                    views_[pane].selected=pressed_folder_=*meaning;
                    notice_="Folder '"+listing_.name(*f)+"': press it again or Enter to open";
                }
                draw(m); return;
            }
        }
        if(busy()) { notice_=waiting(); draw(m); return; }
        const auto* e=pointed(p.pane,p.row,p.column,p.picture);
        if (!e) { notice_="Drag a current entry; right-click the heading for view actions"; draw(m); return; }
        select(*e,p.pane); acquire(Mode::drag,m);
    }
    void on(const ws::PaneDragged&, loom::Mail&) {}
    void on(const ws::PaneButton& p, loom::Mail& m) {
        if (!host(m) || !known(p.pane) || !p.pressed || p.button!=3 || p.lost || editing_!=Editing::none || busy()) return;
        remove_armed_=false; current_=p.pane; menu_folder_.reset(); pressed_folder_.clear();
        if (!views_[p.pane].map.current(p.picture)) { notice_="That picture moved; try again"; draw(m); return; }
        const auto* e=pointed(p.pane,p.row,p.column,p.picture); target_={}; if(e) select(*e,p.pane);
        if(const auto* meaning=hit(p.pane,p.row,p.column,p.picture); meaning && p.pane==pane && meaning->starts_with("dir:"))
            if(const auto f=slots::folder_meant(*meaning,listing_.owner)) if(const auto* state=listing_.folder(*f)) {
                menu_folder_=*state; views_[pane].selected=*meaning;
            }
        auto menu=ws::pane_menu::Offer(p.pane,e?e->label:menu_folder_?menu_folder_->name+"/":"Inventory view").at(p.row,p.column);
        if(menu_folder_) {
            menu.row("folder-open","Open folder").row("folder-rename","Rename folder...")
                .row("pick","Move to another folder...").row("folder-remove","Remove empty folder");
        }
        if(e) {
            menu.row("live","Grab live entry reference").row("copy","Pick up a copy")
                .row("rename","Rename entry...").row("duplicate","Duplicate with next number")
                .row("duplicate-name","Duplicate and name...").row("bind","Configure command hotkey...")
                .row("enable",binding_enabled(e->reference)?"Disable item hotkey":"Enable item hotkey")
                .row("run","Run configured command now");
            if(p.pane!=pane) menu.row("return","Return item to main Inventory");
            menu.row("remove","Remove entry...");
        }
        menu.row("single","Pop out single box").row("row","Pop out row").row("column","Pop out column")
            .row("context",context_active(p.pane)?"Turn this view's hotkeys OFF":"Turn this view's hotkeys ON")
            .row("toolbox-save","Save toolbox...").row("toolbox-restore","Restore toolbox...");
        // Folder acts follow the older rows, so their positions stay where makers and tools learnt them.
        if(e) menu.row("pick","Move to another folder...");
        if(p.pane==pane) {
            menu.row("folder-new","New folder here...");
            if(pick_) menu.row("here","Move '"+pick_->label+"' here");
        }
        menu_=menu.send(m,office); draw(m);
    }
    void on(const ws::PaneMenuAnswered& a, loom::Mail& m) {
        const auto choice=menu_.take(m,a); if(choice.empty()) return;
        // Open is navigation and waits for no key, so the keys stay where the maker put them.
        if(choice=="folder-open" && menu_folder_) { open_folder(menu_folder_->folder.folder); draw(m); return; }
        if(busy()) { notice_=waiting(); draw(m); return; }
        if(choice=="live") acquire(Mode::reference,m);
        else if(choice=="copy") acquire(Mode::copy,m);
        else if(choice=="duplicate") { duplicate_label_=slots::duplicate_name(target_.label,listing_.entries); prepare(Mode::duplicate,m); }
        else if(choice=="run") prepare(Mode::run,m);
        else if(choice=="single" || choice=="row" || choice=="column") {
            slots::InventoryViewEdit op; op.operation="create"; op.text=choice; op.entry=target_.reference; authorize(op,m);
        } else if(choice=="return") {
            slots::InventoryViewEdit op; op.operation="move"; op.view=pane; op.entry=target_.reference; authorize(op,m);
        } else if(choice=="context") {
            slots::InventoryViewEdit op; op.operation="context"; op.view=current_; op.enabled=!context_active(current_); authorize(op,m);
        } else if(choice=="enable") {
            slots::InventoryViewEdit op; op.operation="enable"; op.entry=target_.reference; op.enabled=!binding_enabled(target_.reference); authorize(op,m);
        } else if(choice=="pick") { if(pick(menu_folder_,menu_folder_?nullptr:summary(target_.reference),m)) take_keys(m); }
        else if(choice=="here") move_here(m);
        else if(choice=="folder-remove" && menu_folder_) { if(arm_folder_removal(*menu_folder_)) take_keys(m); }
        else {
            take_keys(m);
            if(choice=="toolbox-save" || choice=="toolbox-restore") edit_toolbox(choice=="toolbox-restore");
            else if(choice=="rename") begin_edit(Editing::rename,target_.label);
            else if(choice=="duplicate-name") begin_edit(Editing::duplicate,slots::duplicate_name(target_.label,listing_.entries));
            else if(choice=="bind") {
                const auto* b=slots::binding(state_.layout,target_.reference);
                begin_edit(Editing::binding,b?b->target+" "+ws::gesture_word({b->scancode,b->modifiers}):"");
                notice_="Enter target-office then a key, e.g. zengine.skin alt+1";
            } else if(choice=="remove") { remove_armed_=true; removing_folder_.reset(); notice_="Press Delete to confirm removing "+target_.label; }
            else if(choice=="folder-new") new_folder();
            else if(choice=="folder-rename" && menu_folder_) rename_folder(*menu_folder_);
        }
        declare_all(m); draw(m);
    }
    void on(const ws::PaneActionRequested& a, loom::Mail& m) {
        if(!host(m) || !known(a.pane)) return;
        pressed_folder_.clear();
        if(editing_!=Editing::none && a.pane!=current_) {
            notice_="Finish the open inventory edit before acting in another view"; draw(m); return;
        }
        for(const auto& b:state_.layout.bindings) if(slots::action(b)==a.id) {
            if(busy()) { notice_=waiting(); draw(m); return; }
            current_=a.pane;
            if(!shortcuts_live_ || !b.enabled || !context_active(slots::placed(state_.layout,b.reference))) return;
            if(const auto* e=summary(b.reference)) { target_=*e; prepare(Mode::run,m); }
            else { notice_="Shortcut's entry is unavailable; its identity was not replaced"; draw(m); } return;
        }
        if(editing_!=Editing::none) {
            if(a.id=="inventory.name.cancel") { editing_=Editing::none; notice_="Edit cancelled"; }
            else if(a.id=="inventory.name.save") finish_edit(m);
            // Resolved against the browsing keys before the editor's own took over: Backspace is text here.
            else if(a.id=="inventory.parent") line_.consume(input::scan::kBackspace,input::mod::kNone,clipboard_);
            declare_all(m); draw(m); return;
        }
        // Navigation first: local view state, allowed while an operation waits.
        const auto* folder=a.pane==pane?selected_folder():nullptr;
        if(a.id=="inventory.up" || a.id=="inventory.left") step(a.pane,-1);
        else if(a.id=="inventory.down" || a.id=="inventory.right") step(a.pane,1);
        else if(a.id=="inventory.parent") go_up();
        else if(a.id=="inventory.root") go_to({});
        else if(a.id=="inventory.copy" && folder) open_folder(folder->folder.folder);
        else if(a.id=="inventory.move.cancel" && pick_) { pick_.reset(); notice_="Move cancelled; nothing changed"; }
        else if(a.id=="inventory.refresh") refresh(m);
        else if(busy()) notice_=waiting();
        else {
            current_=a.pane;
            if(a.id=="inventory.sort" && current_==pane) { auto& v=views_[current_]; v.order=(v.order+1)%3; }
            else if(a.id=="inventory.toolbox.save" || a.id=="inventory.toolbox.restore") edit_toolbox(a.id=="inventory.toolbox.restore");
            else if(a.id=="inventory.folder.new" && current_==pane) new_folder();
            else if(a.id=="inventory.move.pick") pick(folder?std::optional(*folder):std::nullopt,folder?nullptr:selected(current_),m);
            else if(a.id=="inventory.move.here" && current_==pane) move_here(m);
            else if(a.id=="inventory.remove" && remove_armed_ && removing_folder_) {
                remove_armed_=false; const auto doomed=*removing_folder_;
                if(client_.begin(inv::InventoryFolderRemove{doomed.folder,doomed.revision},current_,office,m,asks_)) {
                    mode_=Mode::organize; organizing_="Removed empty folder '"+doomed.name+"'";
                }
                notice_=client_.notice;
            } else if(a.id=="inventory.remove" && remove_armed_) {
                remove_armed_=false; client_.begin(inv::InventoryRemove{target_.reference,target_.revision},current_,office,m,asks_);
            } else if(folder && (a.id=="inventory.rename" || a.id=="inventory.remove" || a.id=="inventory.live")) {
                if(a.id=="inventory.rename") {
                    m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,ws::PaneKeyboardRequested{current_},m.correlation());
                    rename_folder(*folder);
                } else if(a.id=="inventory.remove") arm_folder_removal(*folder);
                else notice_="A folder is not a value; Enter opens it";
            } else if(const auto* e=selected(current_)) {
                target_=*e;
                if(a.id=="inventory.copy") acquire(Mode::copy,m);
                else if(a.id=="inventory.live") acquire(Mode::reference,m);
                else if(a.id=="inventory.rename") begin_edit(Editing::rename,target_.label);
                else if(a.id=="inventory.remove") { remove_armed_=true; removing_folder_.reset(); notice_="Press Delete again to remove "+target_.label; }
            }
        }
        if(a.id!="inventory.remove") remove_armed_=false;
        declare_all(m); draw(m);
    }
    void on(const ws::PaneWheel& w, loom::Mail& m) {
        if(!host(m) || !known(w.pane) || editing_!=Editing::none) return;
        pressed_folder_.clear();
        auto& v=views_[w.pane]; v.wheel+=w.dy;
        while(v.wheel>=1) { step(w.pane,-1); v.wheel-=1; }
        while(v.wheel<=-1) { step(w.pane,1); v.wheel+=1; } remove_armed_=false; draw(m);
    }
    void on(const ws::PaneKey& k, loom::Mail& m) {
        if(host(m) && k.pane==current_ && editing_!=Editing::none && editing_!=Editing::toolbox_confirm && line_.consume(k.scancode,k.modifiers,clipboard_)) draw(m);
    }
    void on(const ws::PaneTextInput& t, loom::Mail& m) {
        if(!host(m) || t.pane!=current_ || editing_==Editing::none || editing_==Editing::toolbox_confirm) return;
        if(std::all_of(t.text.begin(),t.text.end(),[](unsigned char c){return c>=32 && c<=126;})) line_.type(t.text);
        draw(m);
    }
    void on(const ws::PaneValueDrop& v, loom::Mail& m) { copy_drop(v,m); }
    void on(const ws::v2::PaneValueDrop& v, loom::Mail& m) {
        if(!host(m) || !known(v.pane) || editing_!=Editing::none) return;
        if(busy()) { notice_=waiting(); draw(m); return; }
        if(v.source_office!=office || v.token.empty()) { copy_drop({v.pane,v.data,v.row,v.column,v.picture},m); return; }
        const auto it=transfers_.find(v.token);
        if(it==transfers_.end() || it->second.from!=v.source_pane || it->second.layout!=layout_revision_ ||
            !views_[v.pane].map.current(v.picture)) { notice_="That slot transfer expired; drag its current picture"; draw(m); return; }
        const auto transfer=it->second; transfers_.erase(it);
        const auto* now=summary(transfer.target.reference);
        if(!now || now->revision!=transfer.target.revision) { notice_="The source entry changed during this drag"; draw(m); return; }
        current_=v.pane;
        // A folder row, a crumb or [Up] files the entry there: membership, one owner, one approval.
        // Anywhere else moves its placement, as before. One gesture never changes both.
        if(const auto into=drop_folder(v.pane,v.row,v.column,v.picture)) {
            file(*now,transfer.folder,*into,m); draw(m); return;
        }
        slots::InventoryViewEdit op; op.operation="move"; op.view=v.pane; op.entry=now->reference;
        if(const auto* before=pointed(v.pane,v.row,v.column,v.picture)) op.before=before->reference;
        authorize(op,m);
    }
    void on(const ws::PaneOperationAnswered& a, loom::Mail& m) {
        if(toolbox_.hear(a,m,asks_)) notice_=toolbox_.notice;
        else if(edit_ && m.answers_ask() && m.correlation()==edit_->correlation) {
            auto op=edit_->op; edit_.reset(); if(a.allowed) apply(op,m); else notice_=a.reason;
        } else if(command_.hear(a,m)) notice_=command_.notice;
        else if(client_.hear(a,m)) { notice_=client_.notice; settle_pickup(); }
        draw(m);
    }
    void on(const loom::Ack& a, loom::Mail& m) {
        if(command_.hear(a,m)) notice_=command_.notice;
        // The notice belongs to the request this Ack answers, never to an earlier operation's purpose.
        else if(client_.hear(a,m)) { notice_=std::holds_alternative<inv::InventoryFolderRemove>(client_.request())?organizing_:"Entry updated"; refresh(m); }
        draw(m);
    }
    void on(const loom::Refused& a, loom::Mail& m) {
        if(toolbox_.hear(a,m)) notice_=toolbox_.notice;
        else if(list_.valid() && m.answers_ask() && m.correlation()==list_ask_) { list_={}; notice_=a.reason; }
        else if(read_ && m.answers_ask() && m.correlation()==read_->correlation) { read_.reset(); notice_=a.reason; }
        else if(command_.hear(a,m)) notice_=command_.notice;
        else if(client_.hear(a,m)) { notice_=client_.notice; settle_pickup(); }
        draw(m);
    }
    void on(const loom::DispatchRefused& a, loom::Mail& m) {
        if(!m.dispatch_refused()) return;
        const auto matches=[&](loom::Ticket t,const char* role,const std::string& shape,std::uint32_t version=1) {
            return t.valid() && a.refused_attempt().seq==t.seq && a.role==role && a.shape==shape && a.version==version && a.target.empty();
        };
        if(toolbox_.hear(a,m)) notice_=toolbox_.notice;
        else if(matches(list_,inv::kInventoryRole,inv::v2::InventoryList::zen_name,2)) { list_={}; notice_="Inventory list unavailable: "+a.reason; }
        else if(matches(carry_,ws::pane_menu::kWorkshopRole,carry_shape_,carry_version_)) { carry_={}; notice_="Pickup refused: "+a.reason; }
        else if(read_ && matches(read_->ticket,inv::kInventoryRole,inv::InventoryRead::zen_name)) { read_.reset(); notice_="Entry read refused: "+a.reason; }
        else if(edit_ && matches(edit_->ticket,ws::pane_menu::kWorkshopRole,ws::PaneOperationRequested::zen_name)) { edit_.reset(); notice_="View change refused: "+a.reason; }
        else if(registration_ && matches(registration_->ticket,ws::kDesktopRole,ws::PaneShortcuts::zen_name)) finish_registration(false,a.reason,m);
        else if(command_.hear(a,m)) notice_=command_.notice;
        else if(client_.hear(a,m)) { notice_=client_.notice; settle_pickup(); }
        draw(m);
    }
    void on(const inv::InventoryEntry& e, loom::Mail& m) {
        if(read_ && m.answers_ask() && m.correlation()==read_->correlation) {
            auto read=std::move(*read_); read_.reset();
            if(!slots::same(e.reference,read.target.reference) || e.revision!=read.target.revision) {
                notice_="The entry changed while preparing this operation; try again"; refresh(m); draw(m); return;
            }
            if(read.mode==Mode::run) {
                command_.begin(e,*read.binding,read.from,read.gesture,m,asks_); notice_=command_.notice;
            } else {
                duplicate_source_=read.target.reference;
                if(client_.begin(inv::v2::InventoryAdd{e.pair,duplicate_label_,read.folder},read.from,office,m,asks_,read.gesture)) mode_=Mode::duplicate;
                notice_=client_.notice;
            } draw(m); return;
        }
        if(!client_.hear(e,m)) return;
        // A pickup's record ends with its answer, whatever the answer was.
        const auto from=std::exchange(pickup_from_,std::nullopt);
        if(!client_.result) return;
        client_.result.reset();
        if(std::holds_alternative<inv::InventoryFile>(client_.request())) {
            notice_=organizing_;
            if(pick_ && pick_->entry && slots::same(*pick_->entry,e.reference)) { pick_.reset(); declare_all(m); }
            refresh(m); draw(m); return;
        }
        if(mode_==Mode::duplicate) {
            slots::duplicate_binding(state_.layout,duplicate_source_,e.reference);
            ++layout_revision_; notice_="Independent copy saved; copied hotkey is OFF"; declare_all(m); refresh(m); draw(m); return;
        }
        if(mode_==Mode::change || mode_==Mode::store) {
            auto candidate=state_.layout;
            if(drop_into_!=pane && known(drop_into_)) slots::move(candidate,e.reference,drop_into_);
            if(mode_==Mode::store) {
                target_={}; target_.reference=e.reference; target_.revision=e.revision;
                views_[current_].selected=slots::key(e.reference);
                begin_edit(Editing::rename,{});
                m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
                    ws::PaneKeyboardRequested{current_},client_.gesture);
            }
            drop_into_=pane; propose(std::move(candidate),m);
            if(mode_==Mode::store) notice_="Name this new item; Enter saves, Escape keeps its generated name";
            refresh(m); draw(m); return;
        }
        try {
            const auto decoded=inv::decode_pair(bytes(e.pair));
            const auto label=target_.label.empty()?decoded.item.schema().name():target_.label;
            carry_version_=1;
            if(mode_==Mode::reference) {
                // The label rides as descriptive metadata so a receiver can name what it links;
                // it is the name at pickup, never an identity or authority.
                auto named=target_; named.revision=e.revision;
                const auto encoded=inv::encode_pair(loom::to_value(e.reference),{loom::to_value(named)});
                carry_shape_=ws::PaneCarryRequested::zen_name;
                carry_=m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
                    ws::PaneCarryRequested{current_,label,loom::Bytes(encoded.begin(),encoded.end())},client_.gesture);
            } else if(mode_==Mode::drag) {
                // The folder recorded when the press began the pickup, never the listing now.
                if(!from) { notice_="That pickup ended before its entry arrived; drag it again"; draw(m); return; }
                transfers_.clear(); const auto token=nonce_+"."+std::to_string(++asks_);
                auto target=target_; target.revision=e.revision;
                transfers_[token]={target,current_,layout_revision_,*from};
                carry_shape_=ws::v2::PaneValueCarryRequested::zen_name; carry_version_=2;
                carry_=m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
                    ws::v2::PaneValueCarryRequested{current_,label,e.pair,true,token},client_.gesture);
            } else {
                carry_shape_=ws::PaneValueCarryRequested::zen_name;
                carry_=m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
                    ws::PaneValueCarryRequested{current_,label,e.pair,false},client_.gesture);
            }
            notice_=carry_.valid()?"Entry acquired; finish the gesture to place it":"Pickup could not be queued";
        } catch(const std::exception& error) { notice_=error.what(); } draw(m);
    }
    /// A folder operation's own answer: the notice names the actual folder and destination, and a
    /// folder created where the maker still is becomes the selection. Where the maker went
    /// meanwhile is left alone: the answer binds to the operation, not to the displayed folder.
    void on(const inv::InventoryFolderState& f, loom::Mail& m) {
        if(!client_.hear(f,m)) return;
        if(!client_.folder_result) { notice_=client_.notice; draw(m); return; }
        client_.folder_result.reset();
        const auto& request=client_.request();
        if(std::holds_alternative<inv::InventoryFolderCreate>(request)) {
            notice_="Created folder '"+f.name+"' in "+listing_.path(f.parent);
            if(browser_.folder==f.parent) views_[pane].selected=slots::folder_row(f.folder.owner,f.folder.folder);
        } else if(std::holds_alternative<inv::InventoryFolderRename>(request)) notice_="Renamed the folder to '"+f.name+"'";
        else {
            notice_="Moved '"+f.name+"' into "+listing_.path(f.parent);
            if(pick_ && pick_->folder && pick_->folder->folder.folder==f.folder.folder) { pick_.reset(); declare_all(m); }
        }
        refresh(m); draw(m);
    }
    void on(const ws::PaneCarryAnswered& a, loom::Mail& m) {
        if(!carry_.valid() || !m.answers_ask() || m.correlation()!=client_.gesture) return;
        carry_={}; notice_=!a.carried?a.reason:mode_==Mode::drag?
            "Drag moves between inventory views; other panes receive a copy":"Click a receiving pane; Escape cancels"; draw(m);
    }
    void on(const ws::PaneShortcutsAnswered& a, loom::Mail& m) {
        if(toolbox_.hear(a,m)) { notice_=toolbox_.notice; draw(m); }
        else if(registration_ && m.answers_ask() && m.correlation()==registration_->correlation) finish_registration(a.accepted,a.reason,m);
    }
    void on(const ws::PaneShortcutsRequested&, loom::Mail& m) {
        if(m.authored_from_role(ws::kDesktopRole) && !registration_ && !toolbox_.busy()) propose(state_.layout,m,{}, {},true);
    }
    void on(const ws::PaneShortcutsWithdrawn& a, loom::Mail& m) {
        if(!m.authored_from_role(ws::kDesktopRole)) return;
        shortcuts_live_=false; notice_="Hotkeys withdrawn: "+a.reason; draw(m);
    }
    void on(const ws::PaneLaunchAnswered& a, loom::Mail& m) {
        if(m.answers_ask() && launch_ask_ && m.correlation()==launch_ask_) {
            launch_ask_=0; if(!a.refusal.empty()) notice_="View created but not opened: "+a.refusal; draw(m);
        }
    }
private:
    static bool host(const loom::Mail& m) { return m.authored_from_role(ws::pane_menu::kWorkshopRole); }
    static std::string_view bytes(const loom::Bytes& b) { return {reinterpret_cast<const char*>(b.data()),b.size()}; }
    bool known(const std::string& id) const {
        return id==pane || std::any_of(state_.layout.views.begin(),state_.layout.views.end(),[&](const auto& v){return v.id==id;});
    }
    bool busy() const { return toolbox_.busy() || client_.busy() || carry_.valid() || read_ || edit_ || registration_ || command_.busy(); }
    bool context_active(const std::string& id) const { return slots::active(state_.layout,id); }
    bool binding_enabled(const inv::InventoryReference& r) const { const auto* b=slots::binding(state_.layout,r); return b && b->enabled; }
    void select(const inv::InventorySummary& e,const std::string& view) { current_=view; target_=e; views_[view].selected=slots::key(e.reference); }
    const inv::InventorySummary* summary(const inv::InventoryReference& ref) const {
        for(const auto& e:listing_.entries) if(slots::same(e.reference,ref)) return &e;
        return nullptr;
    }
    const inv::InventorySummary* selected(const std::string& id) const {
        const auto it=views_.find(id); if(it==views_.end()) return nullptr;
        const auto entries=ordered(id);
        for(const auto& e:entries) if(slots::key(e.reference)==it->second.selected) return summary(e.reference);
        if(!it->second.selected.empty()) return nullptr;
        const auto keys=row_keys(id);
        return keys.empty() || entries.empty() || keys.front()!=slots::key(entries.front().reference) ? nullptr : summary(entries.front().reference);
    }
    /// The folder row selected in main Inventory, if it is still a child of the displayed folder.
    const inv::InventoryFolderState* selected_folder() const {
        const auto it=views_.find(pane); if(it==views_.end() || !it->second.selected.starts_with("dir:")) return nullptr;
        const auto id=slots::folder_meant(it->second.selected,listing_.owner); if(!id) return nullptr;
        const auto* folder=listing_.folder(*id);
        return folder && folder->parent==browser_.folder ? folder : nullptr;
    }
    /// What a row or column of the current picture means; nullptr for an earlier picture.
    const std::string* hit(const std::string& view,std::int64_t row,std::int64_t col,std::int64_t picture) const {
        const auto it=views_.find(view); if(it==views_.end() || !it->second.map.current(picture)) return nullptr;
        return it->second.map.at(row,col);
    }
    const inv::InventorySummary* pointed(const std::string& view,std::int64_t row,std::int64_t col,std::int64_t picture) const {
        const auto* id=hit(view,row,col,picture);
        if(id) for(const auto& e:listing_.entries) if(slots::key(e.reference)==*id) return &e;
        return nullptr;
    }
    /// The folder a drop in main Inventory names: a folder row, a crumb, or [Up] (the parent).
    std::optional<inv::InventoryFolderReference> drop_folder(const std::string& view,std::int64_t row,std::int64_t col,std::int64_t picture) const {
        const auto* meaning=view==pane?hit(view,row,col,picture):nullptr;
        if(!meaning) return std::nullopt;
        if(*meaning==slots::kUpControl) {
            const auto* here=listing_.folder(browser_.folder);
            if(here) return inv::InventoryFolderReference{listing_.owner,here->parent};
            return std::nullopt;
        }
        if(const auto f=slots::folder_meant(*meaning,listing_.owner)) return inv::InventoryFolderReference{listing_.owner,*f};
        return std::nullopt;
    }
    std::vector<inv::InventorySummary> ordered(const std::string& id) const {
        std::vector<inv::InventorySummary> result;
        if(id==pane) {
            for(const auto& e:listing_.entries)
                if(slots::placed(state_.layout,e.reference)==pane && listing_.member_of(e.reference)==browser_.folder) result.push_back(e);
        } else for(const auto& v:state_.layout.views) if(v.id==id) for(const auto& ref:v.entries) {
            if(const auto* e=summary(ref)) result.push_back(*e);
            else result.push_back({ref,0,"[unavailable]","source absent",0,false});
        }
        const auto found=views_.find(id); const auto order=found==views_.end()?0:found->second.order;
        if(id==pane && order) std::stable_sort(result.begin(),result.end(),[&](const auto&a,const auto&b){return (order==1?a.label:a.schema)<(order==1?b.label:b.schema);});
        return result;
    }
    /// Selection keys in display order: main Inventory's subfolders first, then entries.
    std::vector<std::string> row_keys(const std::string& id) const {
        std::vector<std::string> keys;
        if(id==pane) for(const auto* f:listing_.children(browser_.folder)) keys.push_back(slots::folder_row(listing_.owner,f->folder.folder));
        for(const auto& e:ordered(id)) keys.push_back(slots::key(e.reference));
        return keys;
    }
    void step(const std::string& id,int delta) {
        const auto keys=row_keys(id); if(keys.empty()) return;
        auto& v=views_[id]; const auto it=std::find(keys.begin(),keys.end(),v.selected);
        const auto at=it==keys.end()?0:it-keys.begin();
        v.selected=keys[static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(at+delta,0,static_cast<std::ptrdiff_t>(keys.size()-1)))];
    }
    // ---- folder navigation: main Inventory's local view state ---------------------------------
    void open_folder(const std::string& id) {
        if(!listing_.folder(id)) { notice_="That folder is no longer here"; return; }
        browser_.open(id,listing_,views_[pane].selected); remove_armed_=false;
        notice_="In "+listing_.path(id);
    }
    void go_up() {
        remove_armed_=false;
        notice_=browser_.up(listing_,views_[pane].selected)?"Up to "+listing_.path(browser_.folder):"Already at Root";
    }
    /// Jump to an ancestor (or anywhere by identity), selecting the child we came through.
    void go_to(const std::string& id) {
        remove_armed_=false;
        if(id==browser_.folder) { notice_="Already in "+listing_.path(id); return; }
        if(!listing_.tree.contains(id)) { notice_="That folder is no longer here"; return; }
        const auto path=browser_.path; auto& selected=views_[pane].selected;
        browser_.open(id,listing_,selected);
        const auto at=id.empty()?path.begin():std::find(path.begin(),path.end(),id);
        if(at!=path.end() && (id.empty() || at+1!=path.end())) selected=slots::folder_row(listing_.owner,id.empty()?*at:*(at+1));
        notice_="In "+listing_.path(id);
    }
    std::string waiting() const {
        return "Still waiting for the previous Inventory operation; folders still open";
    }
    // ---- organizing: every act is one owner request under the current actor's authority -----
    void new_folder() {
        folder_parent_={listing_.owner,browser_.folder};
        begin_edit(Editing::folder_create,{});
        notice_="New folder in "+listing_.path(browser_.folder)+"; Enter creates, Escape cancels";
    }
    void rename_folder(const inv::InventoryFolderState& folder) {
        folder_target_=folder; begin_edit(Editing::folder_rename,folder.name);
        notice_="Rename folder '"+folder.name+"'; Enter saves, Escape cancels";
    }
    /// Arm an empty folder's removal for the confirming Delete; true when armed. A folder that
    /// holds anything refuses now, and nothing then waits for a key.
    bool arm_folder_removal(const inv::InventoryFolderState& folder) {
        if(const auto n=listing_.members(folder.folder.folder)) {
            remove_armed_=false;
            notice_="'"+folder.name+"' still holds "+std::to_string(n)+" item"+(n==1?"":"s")+
                "; move them out first. Removing a folder never removes entries";
            return false;
        }
        remove_armed_=true; removing_folder_=folder;
        notice_="Press Delete again to remove the empty folder '"+folder.name+"'";
        return true;
    }
    /// Pick an entry or folder to move; true when picked, and then Ctrl+V, [Move here] or Escape
    /// finishes it.
    bool pick(const std::optional<inv::InventoryFolderState>& folder,const inv::InventorySummary* e,loom::Mail& m) {
        if(folder) pick_=Pick{slots::folder_row(folder->folder.owner,folder->folder.folder),folder->name,std::nullopt,{},folder};
        else if(e) pick_=Pick{slots::key(e->reference),e->label,e->reference,{e->reference.owner,listing_.member_of(e->reference)},std::nullopt};
        else { notice_="Select an entry or folder to move"; return false; }
        declare_all(m);
        notice_="Moving '"+pick_->label+"': open the destination, then Ctrl+V or [Move here]; Escape cancels";
        return true;
    }
    /// A chosen menu row that begins something the maker finishes by key -- a line to type, a
    /// Delete to confirm, a picked item waiting for Ctrl+V or Escape -- asks for the keys the menu
    /// left where they were. Workshop grants them once, and only while the choice is still the
    /// maker's latest act, so a press or key the maker made since keeps them (WL-CTX-09).
    void take_keys(loom::Mail& m) { (void)ws::pane_menu::take_keyboard(m,office,current_); }
    /// Is the picked item still in the collection this pane lists?
    bool picked_here() const {
        if(!pick_) return false;
        if(pick_->folder) return pick_->folder->folder.owner==listing_.owner && listing_.folder(pick_->folder->folder.folder);
        return pick_->entry && summary(*pick_->entry);
    }
    void move_here(loom::Mail& m) {
        if(!pick_) { notice_="Pick an entry or folder to move first (Ctrl+X)"; return; }
        if(busy()) { notice_=waiting(); return; }
        const inv::InventoryFolderReference into{listing_.owner,browser_.folder};
        current_=pane;
        if(pick_->folder) {
            const auto& f=*pick_->folder;
            if(listing_.tree.contains(f.folder.folder) && listing_.tree.within(browser_.folder,f.folder.folder)) {
                notice_="A folder cannot move into itself or a folder inside it"; return;
            }
            if(const auto* now=listing_.folder(f.folder.folder); now && now->parent==browser_.folder) {
                notice_="'"+now->name+"' is already in "+listing_.path(browser_.folder); return;
            }
            if(client_.begin(inv::InventoryFolderMove{f.folder,f.revision,into},current_,office,m,asks_)) mode_=Mode::organize;
            notice_=client_.notice;
        } else if(const auto* e=summary(*pick_->entry)) file(*e,pick_->from,into,m);
        else { pick_.reset(); declare_all(m); notice_="The item picked to move is no longer here"; }
    }
    /// One membership move: from the folder the maker saw it in when the pick or pickup began --
    /// never the folder a later listing shows -- into a folder named by identity. The owner
    /// refuses if the entry has left `from` meanwhile.
    void file(const inv::InventorySummary& e,const inv::InventoryFolderReference& from,const inv::InventoryFolderReference& into,loom::Mail& m) {
        if(from.folder==into.folder && from.owner==into.owner) { notice_="'"+e.label+"' is already in "+listing_.path(into.folder); return; }
        if(client_.begin(inv::InventoryFile{e.reference,from,into},current_,office,m,asks_)) {
            mode_=Mode::organize; organizing_="Filed '"+e.label+"' in "+listing_.path(into.folder);
            if(slots::placed(state_.layout,e.reference)!=pane) organizing_+="; it stays placed in its view";
        }
        notice_=client_.notice;
    }
    void begin_edit(Editing kind,std::string text) { editing_=kind; line_.set(text,text.size()); }
    void edit_toolbox(bool restore) {
        begin_edit(restore ? Editing::toolbox_restore : Editing::toolbox_save, toolbox_path_);
        notice_=restore ? "Enter reviews replacement; Escape cancels" : "Enter saves this toolbox file; Escape cancels";
    }
    void finish_edit(loom::Mail& m) {
        if(editing_==Editing::toolbox_restore) {
            editing_=Editing::toolbox_confirm;
            notice_="Enter again replaces the inventory; hotkeys start OFF. Escape cancels";
        } else if(editing_==Editing::toolbox_save || editing_==Editing::toolbox_confirm) {
            const bool restore=editing_==Editing::toolbox_confirm;
            toolbox_path_=line_.text(); editing_=Editing::none;
            toolbox_.begin(toolbox_path_,restore,restore,state_.layout,m,asks_,{},current_);
            notice_=toolbox_.notice;
        } else if(editing_==Editing::rename) {
            if(client_.begin(inv::InventoryRename{target_.reference,target_.revision,line_.text()},current_,office,m,asks_)) {mode_=Mode::change; editing_=Editing::none;}
        } else if(editing_==Editing::duplicate) {
            duplicate_label_=line_.text(); editing_=Editing::none; prepare(Mode::duplicate,m);
        } else if(editing_==Editing::folder_create || editing_==Editing::folder_rename) {
            // The rules are the owner's; checking here only spares a pointless permission request.
            const auto name=line_.text();
            if(const auto problem=inv::folder_name_problem(name); !problem.empty()) { notice_=problem; return; }
            const bool create=editing_==Editing::folder_create;
            const bool asked=create
                ? client_.begin(inv::InventoryFolderCreate{folder_parent_,name},current_,office,m,asks_)
                : client_.begin(inv::InventoryFolderRename{folder_target_->folder,folder_target_->revision,name},current_,office,m,asks_);
            if(asked) { mode_=Mode::organize; editing_=Editing::none; }
            notice_=client_.notice;
        } else {
            const auto text=line_.text(); const auto at=text.find(' ');
            if(at==std::string::npos) {notice_="Use: target-office alt+1"; return;}
            const auto parsed=ws::parse_gesture(text.substr(at+1));
            if(!parsed.accepted || !parsed.gesture.scancode) {notice_=parsed.refusal.empty()?"Choose a bound key":parsed.refusal; return;}
            slots::InventoryViewEdit op; op.operation="bind"; op.entry=target_.reference; op.text=text.substr(0,at);
            op.scancode=parsed.gesture.scancode; op.modifiers=parsed.gesture.modifiers; editing_=Editing::none; authorize(op,m);
        }
    }
    /// Begin a pickup: permission, then the owner's read. Where the entry is filed is fixed NOW,
    /// with the pickup; listings that arrive before the read answers never change it.
    void acquire(Mode mode,loom::Mail& m) {
        const inv::InventoryFolderReference from{target_.reference.owner,listing_.member_of(target_.reference)};
        if(client_.begin(inv::InventoryRead{target_.reference},current_,office,m,asks_)) { mode_=mode; pickup_from_=from; }
        notice_=client_.notice;
        draw(m);
    }
    /// A pickup's record ends when its acquisition settles without an entry -- permission denied,
    /// a request Loom did not deliver, the owner's refusal -- so nothing is left for another.
    void settle_pickup() { if(!client_.busy()) pickup_from_.reset(); }
    void prepare(Mode mode,loom::Mail& m) {
        if(!summary(target_.reference)) {notice_="That entry is unavailable"; draw(m); return;}
        const auto* binding=slots::binding(state_.layout,target_.reference);
        if(mode==Mode::run && !binding) {notice_="Configure an explicit command target first"; draw(m); return;}
        if(mode==Mode::duplicate && binding && state_.layout.bindings.size()>=16) {notice_="No room to retain the duplicate's hotkey"; draw(m); return;}
        const auto correlation=++asks_;
        auto ticket=m.send_to_role(inv::kInventoryRole,inv::InventoryRead{target_.reference},correlation);
        if(!ticket.valid()) {notice_="Entry read could not be queued"; draw(m); return;}
        read_=Read{target_,current_,mode,m.correlation(),correlation,ticket,binding?std::optional(*binding):std::nullopt,
                   {target_.reference.owner,listing_.member_of(target_.reference)}};
        notice_="Reading the current entry before requesting operation authority"; draw(m);
    }
    /// A new copy lands where it was dropped: a folder row or crumb, else the displayed folder
    /// in main Inventory, else the root. The destination is fixed now, not when the owner answers.
    void copy_drop(const ws::PaneValueDrop& value,loom::Mail& m) {
        if(!host(m) || !known(value.pane) || editing_!=Editing::none) return;
        if(busy()) { notice_=waiting(); draw(m); return; }
        if(!views_[value.pane].map.current(value.picture)) {notice_="Drop picture changed; try again"; draw(m); return;}
        current_=value.pane;
        auto folder=drop_folder(value.pane,value.row,value.column,value.picture)
            .value_or(inv::InventoryFolderReference{listing_.owner,value.pane==pane?browser_.folder:std::string{}});
        if(client_.begin(inv::v2::InventoryAdd{value.data,{},folder},current_,office,m,asks_)) {mode_=Mode::store; drop_into_=current_;}
        notice_=client_.notice;
        draw(m);
    }
    void authorize(const slots::InventoryViewEdit& op,loom::Mail& m) {
        const auto correlation=++asks_;
        auto ticket=m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,ws::PaneOperationRequested{
            current_,office,slots::InventoryViewEdit::zen_name,1,static_cast<std::int64_t>(m.correlation())},correlation);
        if(ticket.valid()) {edit_=Edit{op,correlation,ticket}; notice_="Checking authority for this view change";}
        else notice_="View permission request could not be queued";
    }
    void apply(const slots::InventoryViewEdit& op,loom::Mail& m,loom::DeferredAnswer due={}) {
        try {
            if(!op.entry.entry.empty() && !summary(op.entry)) throw std::invalid_argument("The entry's owner or identity is unavailable");
            if((op.operation=="move" || op.operation=="bind" || op.operation=="enable") && op.entry.entry.empty()) throw std::invalid_argument("This operation needs an entry");
            if(op.operation=="bind" && (ws::key_name_of(op.scancode)==nullptr ||
                (op.modifiers & ~(input::mod::kCtrl|input::mod::kShift|input::mod::kAlt|input::mod::kSuper))))
                throw std::invalid_argument("Choose a key and modifiers the Workshop keymap can name");
            auto candidate=slots::edited(state_.layout,op);
            const auto launch=op.operation=="create"?candidate.views.back().id:std::string{};
            propose(std::move(candidate),m,std::move(due),launch);
        } catch(const std::exception& error) {
            notice_=error.what(); if(due.valid()) (void)loom::answer_deferred(due,m,loom::Refused{notice_}); draw(m);
        }
    }
    static bool same_shortcuts(const slots::InventoryViews& a,const slots::InventoryViews& b) {
        const auto x=slots::shortcuts(a),y=slots::shortcuts(b); if(x.size()!=y.size()) return false;
        for(std::size_t i=0;i<x.size();++i) if(x[i].id!=y[i].id || x[i].label!=y[i].label || x[i].scancode!=y[i].scancode || x[i].modifiers!=y[i].modifiers) return false;
        return true;
    }
    void propose(slots::InventoryViews candidate,loom::Mail& m,loom::DeferredAnswer due={},std::string launch={},bool force=false) {
        if(!force && same_shortcuts(candidate,state_.layout)) {
            state_.layout=std::move(candidate); ++layout_revision_; notice_="Inventory arrangement saved";
            if(due.valid()) (void)loom::answer_deferred(due,m,loom::Ack{});
            announce_views(m); declare_all(m); open(launch,m); draw(m); return;
        }
        declare_all(m,&candidate);
        const auto correlation=++asks_;
        const auto ticket=m.as_role(office).send_to_role(ws::kDesktopRole,ws::PaneShortcuts{slots::shortcuts(candidate)},correlation);
        registration_=Registration{std::move(candidate),std::move(due),correlation,ticket,std::move(launch)};
        notice_="Waiting for Desktop to admit the active shortcuts";
        if(!ticket.valid()) finish_registration(false,"Desktop registration could not be queued",m);
    }
    void finish_registration(bool accepted,const std::string& reason,loom::Mail& m) {
        auto done=std::move(*registration_); registration_.reset();
        if(accepted) {state_.layout=std::move(done.candidate); ++layout_revision_; shortcuts_live_=true; notice_="Inventory hotkeys admitted";}
        else notice_="View change refused; previous arrangement retained: "+reason;
        if(done.due.valid()) {
            if(accepted) (void)loom::answer_deferred(done.due,m,loom::Ack{});
            else (void)loom::answer_deferred(done.due,m,loom::Refused{notice_});
        }
        announce_views(m); declare_all(m); if(accepted) open(done.launch,m); draw(m);
    }
    void open(const std::string& id,loom::Mail& m) {
        if(id.empty()) return;
        launch_ask_=++asks_; m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,ws::PaneLaunchRequested{office,id},launch_ask_);
    }
    void announce(loom::Mail& m) {
        announce_views(m); declare_all(m); refresh(m);
        if(!registration_ && !toolbox_.busy()) propose(state_.layout,m,{}, {},true);
    }
    void announce_views(loom::Mail& m) {
        m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,ws::v2::PaneOffered{pane,"Inventory","Owned entries and portable toolboxes",7,54});
        for(const auto& v:state_.layout.views) m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,
            ws::v2::PaneOffered{v.id,"Inventory "+v.kind+" "+v.id.substr(10),"Drag to move; right-click to name, copy or configure",v.kind=="column"?15:7,v.kind=="row"?41:slots::kSlotColumns});
    }
    void declare_all(loom::Mail& m,const slots::InventoryViews* candidate=nullptr) {
        declare(pane,m,candidate); for(const auto& v:state_.layout.views) declare(v.id,m,candidate);
    }
    void declare(const std::string& id,loom::Mail& m,const slots::InventoryViews* candidate) {
        using namespace input; std::vector<ws::PaneActionRow> rows;
        if(editing_!=Editing::none && id==current_) rows={
            {"inventory.name.save","save",scan::kReturn,mod::kNone},{"inventory.name.cancel","cancel",scan::kEscape,mod::kNone}};
        else {
            rows={
            {"inventory.copy",id==pane?"open folder / pick up copy":"pick up copy",scan::kReturn,mod::kNone},{"inventory.live","pick up live reference",scan::kReturn,mod::kCtrl},
            {"inventory.up","previous entry",scan::kUp,mod::kNone},{"inventory.down","next entry",scan::kDown,mod::kNone},
            {"inventory.left","previous slot",scan::kLeft,mod::kNone},{"inventory.right","next slot",scan::kRight,mod::kNone},
            {"inventory.sort","cycle sorting",scan::kS,mod::kCtrl},{"inventory.refresh","refresh entries",scan::kR,mod::kCtrl},
            {"inventory.rename","rename entry",scan::kN,mod::kCtrl},{"inventory.remove","remove entry",scan::kDelete,mod::kNone},
            {"inventory.toolbox.save","save toolbox",scan::kS,mod::kCtrl|mod::kShift},
            {"inventory.toolbox.restore","restore toolbox",scan::kO,mod::kCtrl},
            {"inventory.move.pick","move to another folder",scan::kX,mod::kCtrl}};
            // Backspace climbs only while no editor is open; an open name line keeps it as text.
            if(id==pane) rows.insert(rows.end(),{{"inventory.parent","up a folder",scan::kBackspace,mod::kNone},
                {"inventory.root","go to Root",scan::kHome,mod::kAlt},{"inventory.folder.new","new folder",scan::kD,mod::kCtrl},
                {"inventory.move.here","move the picked item here",scan::kV,mod::kCtrl}});
            if(pick_) rows.push_back({"inventory.move.cancel","cancel the move",scan::kEscape,mod::kNone});
        }
        if(id==pane) for(const auto& b:(candidate?*candidate:state_.layout).bindings) rows.push_back({slots::action(b),"run inventory command",0,0});
        m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,ws::PaneActions{id,std::move(rows)});
    }
    void refresh(loom::Mail& m) {
        if(list_.valid()) {refresh_again_=true; return;}
        list_ask_=++asks_; list_=m.send_to_role(inv::kInventoryRole,inv::v2::InventoryList{},list_ask_);
        if(!list_.valid()) notice_="Inventory list request could not be queued";
    }
    /// Main Inventory's folder view. A collection without folders shows none of it.
    slots::Browse browse() const {
        slots::Browse b; b.show=!listing_.folders.empty() || !browser_.folder.empty();
        if(!b.show) return b;
        b.owner=listing_.owner; b.crumbs.push_back({{},"Root"});
        if(listing_.tree.contains(browser_.folder))
            for(const auto& id:listing_.tree.path(browser_.folder)) b.crumbs.push_back({id,listing_.name(id)});
        for(const auto* f:listing_.children(browser_.folder)) b.folders.push_back({f,listing_.members(f->folder.folder)});
        for(const auto& e:listing_.entries)
            b.placed+=listing_.member_of(e.reference)==browser_.folder && slots::placed(state_.layout,e.reference)!=pane;
        if(pick_) b.moving=pick_->key;
        return b;
    }
    void draw(loom::Mail& m) {
        for(auto& [id,v]:views_) {
            if(v.rows<=0 || v.columns<=0) continue;
            const bool file_edit=editing_==Editing::toolbox_save || editing_==Editing::toolbox_restore || editing_==Editing::toolbox_confirm;
            const auto label=editing_!=Editing::none && id==current_ ? (file_edit?"Toolbox file: ":editing_==Editing::binding?"Target Key: ":
                editing_==Editing::folder_create?"New folder: ":editing_==Editing::folder_rename?"Folder name: ":"Name: ") : "";
            const auto entries=ordered(id);
            if(v.selected.empty()) if(const auto keys=row_keys(id); !keys.empty()) v.selected=keys.front();
            std::string edit_text;
            if(*label) {
                line_.keep_caret_visible(std::max<std::int64_t>(0,v.columns-1));
                edit_text=line_.visible(std::max<std::int64_t>(0,v.columns-1));
                edit_text.insert(std::min(line_.caret_column(),edit_text.size()),"|");
            }
            auto rows=slots::render(state_.layout,id,v,entries,notice_,label,edit_text,id==pane?browse():slots::Browse{});
            m.as_role(office).send_to_role(ws::pane_menu::kWorkshopRole,ws::v3::PaneContent{id,std::move(rows),0,v.map.settle()});
        }
    }
    static std::string nonce() { std::random_device rng; std::ostringstream s; s<<std::hex<<rng()<<rng()<<rng()<<rng(); return s.str(); }
    zengine::ActivationCursor activation_;
    ws::pane_menu::Asked menu_;
    inv::PaneClient client_;
    slots::Command command_;
    slots::Toolbox toolbox_;
    std::map<std::string,slots::View> views_;
    std::map<std::string,Transfer> transfers_;
    component::TextBox line_;
    component::Clipboard clipboard_;
    slots::Listing listing_;
    slots::Browser browser_;
    std::optional<Pick> pick_;
    std::optional<inv::InventoryFolderState> menu_folder_,removing_folder_,folder_target_;
    inv::InventoryFolderReference folder_parent_;
    inv::InventorySummary target_;
    inv::InventoryReference duplicate_source_;
    std::optional<Read> read_;
    std::optional<inv::InventoryFolderReference> pickup_from_; // a pickup in flight: its entry's folder at the press
    std::optional<Edit> edit_;
    std::optional<Registration> registration_;
    loom::Ticket carry_,list_;
    std::uint64_t asks_=0,list_ask_=0,launch_ask_=0,layout_revision_=0;
    std::string current_=pane,drop_into_=pane,carry_shape_,notice_,duplicate_label_,organizing_,nonce_=nonce();
    std::string pressed_folder_; // the folder row the last press selected, until any other input
    std::string toolbox_path_="inventory.toolbox";
    Mode mode_=Mode::copy;
    Editing editing_=Editing::none;
    std::uint32_t carry_version_=1;
    bool refresh_again_=false,remove_armed_=false,shortcuts_live_=false;
};
}
ZEN_EXPORT_WEAVE(InventoryPane)
