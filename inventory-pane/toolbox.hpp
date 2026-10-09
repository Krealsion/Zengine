// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_PANE_TOOLBOX_HPP
#define ZENGINE_INVENTORY_PANE_TOOLBOX_HPP
#include "toolbox_file.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/actor_scope.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include <zen/weave/role_request.hpp>

namespace zengine::inventory_pane {
// One image-local conversation. The data owner performs the conditional collection commit;
// this controller owns file I/O and preparation of the pane's inactive restored configuration.
class Toolbox {
    enum class Stage { idle, permission, scope, snapshot, restoring, shortcuts };
    Stage stage_ = Stage::idle;
    bool restore_ = false, replace_ = false;
    std::string path_;
    InventoryViews layout_;
    v2::InventoryToolbox file_;
    loom::DeferredAnswer due_;
    loom::RoleRequest pending_;
    std::int64_t count_ = 0;
public:
    std::string notice;
    std::optional<InventoryViews> restored;
    bool busy() const { return stage_ != Stage::idle; }
    // With a pane, the gesture that committed the path asks Workshop once, naming the actor's grant
    // for the send; a SAVE IS CLASS `write` and asks that class too, and its snapshot and write
    // follow that one approval. A restore reads a file and writes none, so it asks the grant alone.
    void begin(std::string path, bool restore, bool replace, const InventoryViews& layout,
               loom::Mail& mail, std::uint64_t& asks, loom::DeferredAnswer due = {},
               const std::string& pane = {}) {
        prepare(std::move(path), restore, replace, layout, std::move(due));
        if (!pane.empty()) {
            stage_ = Stage::permission;
            const auto gesture = static_cast<std::int64_t>(mail.correlation());
            const bool queued = restore
                ? pending_.send_to_role(mail.as_role(kRole), "zengine.workshop",
                      workshop::PaneOperationRequested{pane, kRole, InventoryToolboxRestore::zen_name, 1,
                                                       gesture}, ++asks)
                : pending_.send_to_role(mail.as_role(kRole), "zengine.workshop",
                      workshop::v2::PaneOperationRequested{pane, kRole, InventoryToolboxSave::zen_name, 1,
                                                           gesture, {workshop::scope::kWrite}, {}}, ++asks);
            if (!queued) fail("Toolbox permission request could not be queued", mail);
            else notice = "Checking authority for the toolbox file";
        } else start(mail, asks);
    }
    // A SAVE SENT STRAIGHT TO THIS OFFICE crossed no gesture: before anything is read or written,
    // Workshop judges its sender's class `write`, and a refusal answers that sender in words.
    void begin_sent_save(std::string path, const InventoryViews& layout, loom::Mail& mail,
                         std::uint64_t& asks, loom::DeferredAnswer due, const std::string& pane) {
        prepare(std::move(path), false, false, layout, std::move(due));
        stage_ = Stage::scope;
        if (!pending_.send_to_role(mail.as_role(kRole), "zengine.workshop", workshop::ActorScopeRequested{
                static_cast<std::int64_t>(mail.sender().value), pane, {workshop::scope::kWrite},
                InventoryToolboxSave::zen_name}, ++asks))
            fail("Toolbox write check could not be queued", mail);
        else notice = "Checking the sender's authority to write the toolbox file";
    }
    bool hear(const workshop::PaneOperationAnswered& answer, loom::Mail& mail, std::uint64_t& asks) {
        if (stage_ != Stage::permission || !pending_.matches_answer(mail)) return false;
        pending_.forget();
        if (answer.allowed) start(mail, asks); else fail(answer.reason, mail);
        return true;
    }
    bool hear(const workshop::ActorScopeJudged& judged, loom::Mail& mail, std::uint64_t& asks) {
        if (stage_ != Stage::scope || !pending_.matches_answer(mail)) return false;
        pending_.forget();
        if (judged.allowed) start(mail, asks); else fail(judged.refusal, mail);
        return true;
    }
    bool hear(const inventory::v2::InventorySnapshot& snapshot, loom::Mail& mail, std::uint64_t& asks) {
        if (stage_ != Stage::snapshot || !pending_.matches_answer(mail)) return false;
        pending_.forget();
        try {
            if (!restore_) {
                file_ = toolbox_snapshot(snapshot, layout_);
                write_toolbox(path_, file_);
                count_ = static_cast<std::int64_t>(file_.archive.entries.size());
                finish(mail);
            } else {
                layout_ = toolbox_layout(file_, layout_);
                stage_ = Stage::restoring;
                if (!pending_.send_to_role(mail, inventory::kInventoryRole, inventory::v2::InventoryRestore{
                        snapshot.owner, snapshot.revision, file_.archive, replace_}, ++asks))
                    fail("Toolbox restore could not be queued", mail);
                else notice = "Restoring the toolbox against the current collection";
            }
        } catch (const std::exception& e) { fail(e.what(), mail); }
        return true;
    }
    bool hear(const inventory::InventoryRestored& answer, loom::Mail& mail, std::uint64_t& asks) {
        if (stage_ != Stage::restoring || !pending_.matches_answer(mail)) return false;
        if (answer.owner.empty() || answer.entries != static_cast<std::int64_t>(file_.archive.entries.size())) {
            fail("Inventory returned an invalid restore result; inspect the collection before retrying", mail);
            return true;
        }
        pending_.forget(); count_ = answer.entries;
        bind_toolbox_owner(layout_, answer.owner); restored = std::move(layout_);
        stage_ = Stage::shortcuts;
        if (!pending_.send_to_role(mail.as_role(kRole), workshop::kDesktopRole, workshop::PaneShortcuts{}, ++asks))
            fail("Entries restored with hotkeys OFF; Desktop cleanup could not be queued", mail);
        else notice = "Entries restored with hotkeys OFF; clearing previous shortcuts";
        return true;
    }
    bool hear(const workshop::PaneShortcutsAnswered& answer, loom::Mail& mail) {
        if (stage_ != Stage::shortcuts || !pending_.matches_answer(mail)) return false;
        pending_.forget();
        if (answer.accepted) finish(mail);
        else fail("Entries restored with hotkeys OFF; Desktop cleanup refused: " + answer.reason, mail);
        return true;
    }
    bool hear(const loom::Refused& answer, loom::Mail& mail) {
        if (!busy() || !pending_.matches_answer(mail)) return false;
        fail((stage_ == Stage::shortcuts ? "Entries restored with hotkeys OFF; " : "") + answer.reason, mail);
        return true;
    }
    bool hear(const loom::DispatchRefused& answer, loom::Mail& mail) {
        if (!busy() || !pending_.matches_refusal(answer, mail)) return false;
        fail((stage_ == Stage::shortcuts ? "Entries restored with hotkeys OFF; " : "") +
             std::string("Toolbox request refused: ") + answer.reason, mail);
        return true;
    }
private:
    void prepare(std::string path, bool restore, bool replace, const InventoryViews& layout,
                 loom::DeferredAnswer due) {
        restore_ = restore; replace_ = replace; layout_ = layout; due_ = std::move(due);
        path_ = std::move(path); count_ = 0; restored.reset();
    }
    void start(loom::Mail& mail, std::uint64_t& asks) {
        try {
            path_ = toolbox_path(path_);
            if (restore_) file_ = read_toolbox(path_);
            stage_ = Stage::snapshot;
            if (!pending_.send_to_role(mail, inventory::kInventoryRole, inventory::v2::InventorySnapshotRequested{}, ++asks))
                fail("Inventory snapshot could not be queued", mail);
            else notice = restore_ ? "Preparing toolbox restore" : "Taking a toolbox snapshot";
        } catch (const std::exception& e) { fail(e.what(), mail); }
    }
    void finish(loom::Mail& mail) {
        notice = (restore_ ? "Restored " : "Saved ") + std::to_string(count_) + " entries" +
                 (restore_ ? "; hotkeys OFF" : " to toolbox") + ": " + path_;
        if (due_.valid()) (void)loom::answer_deferred(due_, mail,
            InventoryToolboxFinished{restore_ ? "restore" : "save", path_, count_});
        clear();
    }
    void fail(std::string reason, loom::Mail& mail) {
        notice = std::move(reason);
        if (due_.valid()) (void)loom::answer_deferred(due_, mail, loom::Refused{notice});
        clear();
    }
    void clear() { due_ = {}; pending_.forget(); stage_ = Stage::idle; file_ = {}; layout_ = {}; }
};
}
#endif
