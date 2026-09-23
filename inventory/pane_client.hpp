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
                                 InventoryRename, InventoryRemove>;
    Phase phase = Phase::idle;
    std::string notice;
    std::optional<InventoryEntry> result;
    std::uint64_t gesture = 0;

    bool busy() const { return phase != Phase::idle; }
    template<class RequestType>
    bool begin(RequestType request, std::string pane, const char* office, loom::Mail& mail, std::uint64_t& asks, std::uint64_t gesture_override = 0) {
        if (busy()) { notice = "An inventory operation is still pending"; return false; }
        request_ = std::move(request);
        gesture = gesture_override ? gesture_override : mail.correlation();
        const auto permission_correlation = ++asks;
        owner_correlation_ = ++asks;
        result.reset();
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
            if constexpr (!std::is_same_v<T, InventoryLocate> && !std::is_same_v<T, InventoryAdd>)
                same = req.reference.owner == entry.reference.owner &&
                       req.reference.entry == entry.reference.entry;
        }, request_);
        if (!same) { failed("Inventory answered about a different entry"); return true; }
        result = entry;
        pending_.forget();
        phase = Phase::idle;
        notice.clear();
        return true;
    }
    bool hear(const loom::Ack&, loom::Mail& mail) {
        if (phase != Phase::owner || !pending_.matches_answer(mail) ||
            !std::holds_alternative<InventoryRemove>(request_)) return false;
        pending_.forget();
        phase = Phase::idle;
        notice = "Entry removed";
        return true;
    }
    bool hear(const loom::Refused& answer, loom::Mail& mail) {
        if (phase != Phase::owner || !pending_.matches_answer(mail)) return false;
        failed(answer.reason); return true;
    }
    bool hear(const loom::DispatchRefused& answer, loom::Mail& mail) {
        if (!busy() || !pending_.matches_refusal(answer, mail)) return false;
        failed("Inventory operation was not delivered: " + answer.reason); return true;
    }
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
