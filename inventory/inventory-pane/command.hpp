// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_PANE_COMMAND_HPP
#define ZENGINE_INVENTORY_PANE_COMMAND_HPP
#include "vocabulary.hpp"
#include "inventory/codec.hpp"
#include "message-draft/transfer.hpp"
#include "pane/operation.hpp"
#include "pane/menu.hpp"
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
            if (target_.empty()) throw std::invalid_argument("Choose an explicit target role first");
            value_ = std::move(value); correlation_ = ++asks; authorizing_ = true;
            ticket_ = mail.as_role(kRole).ask_role(zengine::pane::pane_menu::kWorkshopRole, pane::PaneOperationRequested{
                pane, target_, shape_, version_, static_cast<std::int64_t>(gesture)}, correlation_);
            notice = "Checking authority for " + target_ + " / " + shape_;
            if (!ticket_.valid()) fail("Command permission request could not be queued");
        } catch (const std::exception& e) { fail(e.what()); }
    }
    bool hear(const pane::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!authorizing_ || !mail.answers_ask() || mail.correlation() != correlation_) return false;
        authorizing_ = false;
        if (!answer.allowed) { fail(answer.reason); return true; }
        // The destination gate admits against its current schema, at delivery. A stored
        // description or permission to configure a binding never substitutes for this gate.
        ticket_ = mail.bus().role_send_to_role(kRole, target_, loom::Message(std::move(*value_), {}, {}, correlation_));
        value_.reset();
        notice = ticket_.valid() ? "Queued " + shape_ + " to " + target_ + "; delivery is not completion"
                                 : "Command could not be queued";
        return true;
    }
    /// The command itself is a plain send, whose receiver owes no answer: its refusal is heard.
    bool hear(const loom::DispatchRefused& r, loom::Mail& mail) {
        if (authorizing_ || !ticket_.valid() || !mail.dispatch_refused() || r.refused_attempt().seq != ticket_.seq ||
            r.role != target_ || r.shape != shape_ || r.version != version_ || !r.target.empty()) return false;
        fail("Command refused: " + r.reason); return true;
    }
    /// The permission it asks Workshop for ends with an answer or Loom's word.
    bool hear(const loom::AskEnded& ended, loom::Mail& mail) {
        if (!authorizing_ || !ticket_.valid() || !mail.ask_ended() || ended.ended_attempt().seq != ticket_.seq)
            return false;
        fail("Command not allowed: " + zengine::pane::no_answer(ended)); return true;
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
