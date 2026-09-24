// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_PANE_CLIENT_HPP
#define ZENGINE_INVENTORY_PANE_CLIENT_HPP
#include "inventory/vocabulary.hpp"
#include "workshop/pane_operation.hpp"
#include <zen/weave.hpp>
#include <zen/weave/role_request.hpp>
#include <zen/weave/dispatch_refusal.hpp>
#include <zen/weave/standard_shapes.hpp>
#include <optional>
#include <variant>

namespace zengine::inventory {
// Shared by the inventory presentation and Info. The entry owner still performs the read or
// conditional write. This helper binds one UI intent to its permission answer and owner answer.
class PaneClient {
public:
    enum class Phase { idle, authorizing, owner };
    using Request = std::variant<InventoryLocate, InventoryRead, InventoryWrite, InventoryAdd,
                                 InventoryRename, InventoryRemove, v2::InventoryAdd, InventoryFile,
                                 InventoryFolderCreate, InventoryFolderRename, InventoryFolderMove,
                                 InventoryFolderRemove>;
    Phase phase = Phase::idle;
    std::string notice;
    std::optional<InventoryEntry> result;
    std::optional<InventoryFolderState> folder_result;
    std::uint64_t gesture = 0;

    bool busy() const { return phase != Phase::idle; }
    template<class RequestType>
    bool begin(RequestType request, std::string pane, const char* office, loom::Mail& mail, std::uint64_t& asks, std::uint64_t gesture_override = 0) {
        if (busy()) { notice = "An inventory operation is still pending"; return false; }
        request_ = std::move(request);
        gesture = gesture_override ? gesture_override : mail.correlation();
        const auto permission_correlation = ++asks;
        owner_correlation_ = ++asks;
        result.reset(); folder_result.reset();
        notice = "Checking this operation's authority";
        phase = Phase::authorizing;
        const bool queued = pending_.send_to_role(mail.as_role(office), "zengine.workshop",
            workshop::PaneOperationRequested{
            std::move(pane), kInventoryRole, RequestType::zen_name, RequestType::zen_version, static_cast<std::int64_t>(gesture)},
            permission_correlation);
        if (!queued) failed("The permission request could not be queued");
        return busy();
    }
    bool hear(const workshop::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (phase != Phase::authorizing || !pending_.matches_answer(mail)) return false;
        if (!answer.allowed) { failed(answer.reason); return true; }
        phase = Phase::owner;
        pending_.forget();
        notice = "Waiting for inventory";
        std::visit([&](const auto& req) {
            pending_.send_to_role(mail, kInventoryRole, req, owner_correlation_);
        }, request_);
        if (!pending_.pending()) failed("The inventory request could not be queued");
        return true;
    }
    bool hear(const InventoryEntry& entry, loom::Mail& mail) {
        if (phase != Phase::owner || !pending_.matches_answer(mail) || entry.reference.owner.empty() ||
            entry.reference.entry.empty() || entry.revision <= 0) return false;
        bool same = true;
        std::visit([&](const auto& req) {
            using T = std::decay_t<decltype(req)>;
            if constexpr (requires { req.reference.entry; })
                same = req.reference.owner == entry.reference.owner &&
                       req.reference.entry == entry.reference.entry;
            else if constexpr (!std::is_same_v<T, InventoryLocate> && !std::is_same_v<T, InventoryAdd> &&
                               !std::is_same_v<T, v2::InventoryAdd>)
                same = false; // a folder operation is never answered with an entry
        }, request_);
        if (!same) { failed("Inventory answered about a different entry"); return true; }
        result = entry;
        pending_.forget();
        phase = Phase::idle;
        notice.clear();
        return true;
    }
    /// A folder operation's answer must name the folder it asked about (a create, its parent).
    bool hear(const InventoryFolderState& folder, loom::Mail& mail) {
        if (phase != Phase::owner || !pending_.matches_answer(mail) || folder.folder.folder.empty()) return false;
        bool same = false;
        std::visit([&](const auto& req) {
            using T = std::decay_t<decltype(req)>;
            if constexpr (std::is_same_v<T, InventoryFolderCreate>)
                same = req.parent.owner == folder.folder.owner && req.parent.folder == folder.parent &&
                       req.name == folder.name;
            else if constexpr (std::is_same_v<T, InventoryFolderRename> || std::is_same_v<T, InventoryFolderMove>)
                same = req.folder.owner == folder.folder.owner && req.folder.folder == folder.folder.folder;
        }, request_);
        if (!same) { failed("Inventory answered about a different folder"); return true; }
        folder_result = folder;
        pending_.forget();
        phase = Phase::idle;
        notice.clear();
        return true;
    }
    bool hear(const loom::Ack&, loom::Mail& mail) {
        if (phase != Phase::owner || !pending_.matches_answer(mail) ||
            !(std::holds_alternative<InventoryRemove>(request_) ||
              std::holds_alternative<InventoryFolderRemove>(request_))) return false;
        pending_.forget();
        phase = Phase::idle;
        notice = std::holds_alternative<InventoryRemove>(request_) ? "Entry removed" : "Folder removed";
        return true;
    }
    const Request& request() const { return request_; }
    bool hear(const loom::Refused& answer, loom::Mail& mail) {
        if (phase != Phase::owner || !pending_.matches_answer(mail)) return false;
        failed(answer.reason); return true;
    }
    bool hear(const loom::DispatchRefused& answer, loom::Mail& mail) {
        if (!busy() || !pending_.matches_refusal(answer, mail)) return false;
        failed("Inventory operation was not delivered: " + answer.reason); return true;
    }
    /// STOP WAITING: forget the operation in flight. Local bookkeeping, as `RoleRequest::forget`
    /// is -- the owner may still do the work, and its late answer matches nothing here.
    void abandon() { failed("stopped waiting"); }
private:
    void failed(std::string reason) {
        pending_.forget();
        phase = Phase::idle;
        notice = std::move(reason);
    }
    loom::RoleRequest pending_;
    std::uint64_t owner_correlation_ = 0;
    Request request_;
};
} // namespace zengine::inventory
#endif
