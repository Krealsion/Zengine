// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_PANE_COMMAND_HPP
#define ZENGINE_INVENTORY_PANE_COMMAND_HPP
#include "vocabulary.hpp"
#include "inventory/codec.hpp"
#include "message-draft/transfer.hpp"
#include "workshop/pane_operation.hpp"
#include <zen/weave.hpp>
namespace zengine::inventory_pane {
class Command {
    std::optional<loom::Value> value_;
    std::string target_, shape_;
    std::uint32_t version_ = 0;
    std::uint64_t correlation_ = 0;
    loom::Ticket ticket_;
    bool authorizing_ = false;
public:
    std::string notice;
    bool busy() const { return authorizing_; }
    void begin(const inventory::InventoryEntry& entry, const SlotBinding& binding,
               const std::string& pane, std::uint64_t gesture, loom::Mail& mail, std::uint64_t& asks) {
        try {
            const auto pair = inventory::decode_pair({reinterpret_cast<const char*>(entry.pair.data()), entry.pair.size()});
            auto value = pair.item;
            if (message_draft::is_stored_draft(value)) {
                const auto draft = message_draft::read_draft(value);
                auto admitted = draft.draft.admit();
                if (!admitted) throw std::invalid_argument("Command is incomplete: " + admitted.first_error().message());
                value = admitted.value();
            }
            target_ = binding.target; shape_ = value.schema().name(); version_ = value.schema().version();
            if (target_.empty()) throw std::invalid_argument("Choose an explicit target office first");
            value_ = std::move(value); correlation_ = ++asks; authorizing_ = true;
            ticket_ = mail.as_role(kRole).send_to_role("zengine.workshop", workshop::PaneOperationRequested{
                pane, target_, shape_, version_, static_cast<std::int64_t>(gesture)}, correlation_);
            notice = "Checking authority for " + target_ + " / " + shape_;
            if (!ticket_.valid()) fail("Command permission request could not be queued");
        } catch (const std::exception& e) { fail(e.what()); }
    }
    bool hear(const workshop::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!authorizing_ || !mail.answers_ask() || mail.correlation() != correlation_) return false;
        authorizing_ = false;
        if (!answer.allowed) { fail(answer.reason); return true; }
        // The destination gate admits against its current schema, at delivery. A stored
        // description or permission to configure a binding never substitutes for this gate.
        ticket_ = mail.bus().office_send_to_role(kRole, target_, loom::Message(std::move(*value_), {}, {}, correlation_));
        value_.reset();
        notice = ticket_.valid() ? "Queued " + shape_ + " to " + target_ + "; delivery is not completion"
                                 : "Command could not be queued";
        return true;
    }
    bool hear(const loom::DispatchRefused& r, loom::Mail& mail) {
        const auto expected_role = authorizing_ ? "zengine.workshop" : target_;
        const auto expected_shape = authorizing_ ? workshop::PaneOperationRequested::zen_name : shape_;
        const auto expected_version = authorizing_ ? 1u : version_;
        if (!ticket_.valid() || !mail.dispatch_refused() || r.refused_attempt().seq != ticket_.seq ||
            r.role != expected_role || r.shape != expected_shape || r.version != expected_version || !r.target.empty()) return false;
        fail("Command refused: " + r.reason); return true;
    }
    bool hear(const loom::Refused& r, loom::Mail& mail) {
        if (authorizing_ || !ticket_.valid() || !mail.answers_ask() || mail.correlation() != correlation_) return false;
        fail("Command refused: " + r.reason); return true;
    }
    bool hear(const loom::Ack&, loom::Mail& mail) {
        if (authorizing_ || !ticket_.valid() || !mail.answers_ask() || mail.correlation() != correlation_) return false;
        ticket_ = {}; notice = "Acknowledged by " + target_; return true;
    }
private:
    void fail(std::string text) { authorizing_ = false; value_.reset(); ticket_ = {}; notice = std::move(text); }
};
}
#endif
