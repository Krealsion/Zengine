// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_DESKTOP_SHORTCUTS_HPP
#define ZENGINE_DESKTOP_SHORTCUTS_HPP
#include "workshop/pane_shortcuts.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include <zen/weave.hpp>
#include <map>
#include <optional>
#include <set>
namespace zengine::desktop_pane {
class Shortcuts {
    struct Owner { loom::WeaveId holder; std::vector<workshop::PaneShortcut> rows; };
    using Owners = std::map<std::string, Owner>;
    struct Pending { Owners owners; loom::DeferredAnswer due; std::uint64_t attempt; loom::Ticket ticket; };
    Owners owners_;
    std::optional<Pending> pending_;
    static std::string id(const std::string& office, const std::string& local) {
        return "shortcut." + std::to_string(office.size()) + "." + office + "." + local;
    }
    static void append(std::vector<workshop::AppActionRow>& result, const Owners& owners) {
        for (const auto& [office, owner] : owners) for (const auto& row : owner.rows)
            result.push_back({id(office, row.id), row.label, row.scancode, row.modifiers,
                              workshop::app_precedence::kAboveModes});
    }
public:
    bool pending() const { return pending_.has_value(); }
    void append(std::vector<workshop::AppActionRow>& result) const { append(result, owners_); }
    bool propose(const workshop::PaneShortcuts& request, loom::Mail& mail,
                 std::vector<workshop::AppActionRow> base, std::uint64_t attempt) {
        const auto refuse = [&](std::string reason) {
            (void)mail.answer(workshop::PaneShortcutsAnswered{false, std::move(reason)}); return false;
        };
        const std::string office(mail.authored_role());
        if (office.empty()) return refuse("Shortcuts require an authored provider office");
        if (pending_) return refuse("Another shortcut declaration is pending; try again");
        if (request.rows.size() > 24) return refuse("A provider may propose at most 24 active shortcuts");
        std::set<std::string> ids;
        for (const auto& row : request.rows)
            if (row.id.empty() || row.pane.empty() || row.action.empty() || row.label.empty() ||
                !ids.insert(row.id).second) return refuse("Shortcuts need unique ids, panes, actions and labels");
        auto candidate = owners_;
        if (request.rows.empty()) candidate.erase(office);
        else candidate[office] = {mail.sender(), request.rows};
        append(base, candidate);
        auto due = mail.defer_answer();
        if (!due.valid()) return refuse("This registration cannot retain its answer");
        pending_.emplace(Pending{std::move(candidate), std::move(due), attempt, {}});
        const auto sent = mail.as_role(workshop::kDesktopRole).send_to_role("zengine.workshop",
            workshop::AppActions{std::move(base)}, attempt);
        pending_->ticket = sent;
        if (!sent.valid()) finish(false, "Application shortcut declaration could not be queued", mail);
        return true;
    }
    bool judged(std::uint64_t attempt, bool accepted, const std::string& reason, loom::Mail& mail) {
        if (!pending_ || pending_->attempt != attempt) return false;
        finish(accepted, reason, mail); return true;
    }
    bool refused(const loom::DispatchRefused& refusal, loom::Mail& mail) {
        if (!pending_ || !mail.dispatch_refused() || refusal.refused_attempt().seq != pending_->ticket.seq ||
            refusal.shape != workshop::AppActions::zen_name || refusal.version != 1 ||
            refusal.role != "zengine.workshop" || !refusal.target.empty()) return false;
        finish(false, "Shortcut declaration was not delivered: " + refusal.reason, mail); return true;
    }
    void withdrawn(const std::string& reason, loom::Mail& mail) const {
        for (const auto& [office, owner] : owners_) {
            (void)office;
            mail.as_role(workshop::kDesktopRole).send(owner.holder, workshop::PaneShortcutsWithdrawn{reason});
        }
    }
    bool invoke(const std::string& action, loom::Mail& mail) const {
        for (const auto& [office, owner] : owners_) for (const auto& row : owner.rows)
            if (id(office, row.id) == action) {
                mail.as_role(workshop::kDesktopRole).send_to_role("zengine.workshop",
                    workshop::PaneShortcutInvoked{office, row.pane, row.action,
                        static_cast<std::int64_t>(owner.holder.value)}, mail.correlation());
                return true;
            }
        return false;
    }
private:
    void finish(bool accepted, std::string reason, loom::Mail& mail) {
        auto done = std::move(*pending_); pending_.reset();
        if (accepted) owners_ = std::move(done.owners);
        (void)loom::answer_deferred(done.due, mail,
            workshop::PaneShortcutsAnswered{accepted, std::move(reason)});
    }
};
} // namespace zengine::desktop_pane
#endif
