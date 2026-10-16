// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INFO_VALUE_WATCH_HPP
#define ZENGINE_INFO_VALUE_WATCH_HPP

// One Info value view's watch: the observation lease Workshop grants on the entry the view links,
// and the requests that ask for it, continue it and end it. It holds no draft: a read it is
// answered goes to the view, which alone decides what its draft does with newer data.
// Reference: info/docs/zengine.info.md.

#include "vocabulary.hpp"
#include "inventory/vocabulary.hpp"
#include "workshop/pane_operation.hpp"

#include <zen/weave/role_request.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>
#include <utility>

namespace zengine::info_pane {

/// WHAT AN ACT MAY SPEND BEYOND ITS VIEW: the delivery that brought it and the weave's one
/// correlation counter.
struct ViewContext {
    loom::Mail& mail;
    std::uint64_t& asks;
};

inline std::string clock_text() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char out[16] = {};
    if (const std::tm* local = std::localtime(&now)) std::strftime(out, sizeof out, "%H:%M:%S", local);
    return out;
}

/// ONE VIEW'S WATCH ON ITS LINKED ENTRY. Every request it sends names the view's pane key and
/// spends the weave's correlation counter; every sentence it returns is the view's to say, and
/// an empty one leaves the view's notice as it is.
class ValueWatch {
public:
    /// WHAT AN ANSWER DID HERE: whether it settled a record of this watch, and what it says.
    struct Heard {
        bool mine = false;
        std::string notice;
    };

    bool watching() const noexcept { return watch_.state != Watch::State::off; }
    /// A watch request this slot stopped before Workshop answered it is still waiting.
    bool ending() const noexcept { return ending_.pending(); }
    /// THE WATCH IN WORDS, for the view's state line.
    std::string status() const {
        if (watch_.state == Watch::State::on)
            return "watch ON" + (watch_.last_good.empty() ? std::string() : " read " + watch_.last_good);
        if (watch_.state == Watch::State::requesting) return "watch: asking";
        return "watch off";
    }
    const std::string& last_failure() const noexcept { return watch_.last_failure; }

    /// ASK WORKSHOP TO WATCH `entry`: one observation request per slot, live or being ended, so
    /// the book stays one record deep.
    std::string start(const std::string& pane, const inventory::InventoryReference& entry, ViewContext& c) {
        if (watch_.ask.pending() || ending_.pending()) return "Watch is already being approved";
        watch_ = Watch{};
        watch_.reference = entry;
        watch_.subject = entry.owner + ":" + entry.entry;
        const auto correlation = ++c.asks;
        const bool queued = watch_.ask.send_to_role(c.mail.as_role(kInfoPaneRole), "zengine.workshop",
            workshop::PaneObservationRequested{pane, inventory::kInventoryRole, inventory::InventoryRead::zen_name,
                inventory::InventoryRead::zen_version, static_cast<std::int64_t>(c.mail.correlation()), watch_.subject},
            correlation);
        if (!queued) { watch_ = Watch{}; return "Watch request could not be queued"; }
        watch_.state = Watch::State::requesting;
        return "Asking to watch this entry";
    }
    /// INVENTORY SAID SOMETHING CHANGED -- an invalidation, not a value. A watching view runs one
    /// observation cycle; a trigger during a cycle is remembered once, never queued.
    std::string changed(const std::string& pane, ViewContext& c) {
        if (watch_.state != Watch::State::on) return {};
        if (watch_.ask.pending() || watch_.read.pending()) { watch_.again = true; return {}; }
        return cycle(pane, c);
    }
    /// END THE WATCH, AT WORKSHOP TOO. A known lease is ended now; a request Workshop has not yet
    /// answered moves to `ending_`, so a lease it grants later is ended on arrival and restarts
    /// nothing. Forgetting the read here is what keeps a late answer off whatever comes next.
    /// `tell` is false only where Workshop has already forgotten the lease (it refused). Says
    /// `reason` when there was a watch to end.
    std::string stop(const std::string& pane, ViewContext& c, std::string reason, bool tell = true) {
        if (watch_.state == Watch::State::off && !watch_.ask.pending()) return {};
        if (watch_.state == Watch::State::requesting && watch_.ask.pending()) ending_ = watch_.ask;
        else if (tell && watch_.lease)
            (void)c.mail.as_role(kInfoPaneRole).send_to_role("zengine.workshop",
                workshop::PaneObservationEnded{pane, watch_.lease});
        const auto kept_good = watch_.last_good, kept_failure = watch_.last_failure;
        watch_ = Watch{};
        watch_.last_good = kept_good; watch_.last_failure = kept_failure;
        return reason;
    }
    /// WHAT THE SLOT KEEPS WHEN ITS VIEW RETIRES: only a watch request still waiting for Workshop.
    ValueWatch retire() {
        ValueWatch next;
        next.ending_ = std::move(ending_);
        return next;
    }

    // ---- answers -------------------------------------------------------------------------

    /// WORKSHOP ANSWERED A WATCH REQUEST. `revision` is the base the view's draft holds, which a
    /// watch that begins names.
    Heard hear(const workshop::PaneObservationAnswered& answer, const std::string& pane, std::int64_t revision,
               ViewContext& c) {
        if (ending_.matches_answer(c.mail)) {
            // A WATCH STOPPED BEFORE WORKSHOP ANSWERED IT: a lease granted now is ended now, and
            // nothing here starts watching again.
            ending_.forget();
            if (answer.allowed && answer.lease)
                (void)c.mail.as_role(kInfoPaneRole).send_to_role("zengine.workshop",
                    workshop::PaneObservationEnded{pane, answer.lease});
            return {true, {}};
        }
        if (!watch_.ask.matches_answer(c.mail)) return {};
        watch_.ask.forget();
        if (!answer.allowed) {
            const bool started = watch_.state == Watch::State::requesting;
            return {true, stop(pane, c, (started ? "Watch refused: " : "Watch ended: ") + answer.reason, false)};
        }
        Heard heard{true, {}};
        if (watch_.state == Watch::State::requesting) {
            watch_.state = Watch::State::on;
            watch_.lease = answer.lease;
            heard.notice = "Watching rev " + std::to_string(revision) + "; changes appear here, your edits are never replaced";
        }
        // THE CONTINUATION IS APPROVED: one read, under this view's own number.
        const auto correlation = ++c.asks;
        if (!watch_.read.send_to_role(c.mail.as_role(kInfoPaneRole), inventory::kInventoryRole,
                                      inventory::InventoryRead{watch_.reference}, correlation)) {
            if (auto said = stop(pane, c, "Watch ended: the read could not be queued", true); !said.empty())
                heard.notice = std::move(said);
        }
        return heard;
    }

    /// INVENTORY ANSWERED THIS WATCH'S READ. An entry it watches goes to `observe`, the view's to
    /// adopt or hold; a change heard during the cycle then runs one more.
    template <class Observe>
    Heard hear(const inventory::InventoryEntry& entry, const std::string& pane, ViewContext& c, Observe&& observe) {
        if (!watch_.read.matches_answer(c.mail)) return {};
        watch_.read.forget();
        if (entry.reference.owner != watch_.reference.owner || entry.reference.entry != watch_.reference.entry)
            return {true, stop(pane, c, "Watch ended: inventory answered about a different entry", true)};
        ++watch_.observations;
        watch_.last_good = clock_text();
        watch_.last_failure.clear();
        observe(entry);
        if (!watch_.again) return {true, {}};
        watch_.again = false;
        return {true, cycle(pane, c)};
    }

    /// INVENTORY REFUSED THIS WATCH'S READ: true when it was this watch's, kept as its last
    /// failure. Ending the watch, and what the refusal says of the link, are the view's.
    bool read_refused(const loom::Refused& refused, const loom::Mail& mail) {
        if (!watch_.read.matches_answer(mail)) return false;
        watch_.read.forget();
        watch_.last_failure = clock_text() + " " + refused.reason;
        return true;
    }

    /// A WATCH REQUEST WAS NOT DELIVERED.
    Heard hear(const loom::DispatchRefused& refused, const std::string& pane, ViewContext& c) {
        if (ending_.matches_refusal(refused, c.mail)) { ending_.forget(); return {true, {}}; } // it granted nothing
        if (!watch_.ask.matches_refusal(refused, c.mail) && !watch_.read.matches_refusal(refused, c.mail)) return {};
        // AN UNDELIVERED CONTINUATION OR READ: the lease Workshop approved is still in its book,
        // so it is ended there too. A request refused before it arrived holds no lease.
        if (watch_.state == Watch::State::requesting) watch_.ask.forget();
        watch_.read.forget();
        watch_.last_failure = clock_text() + " not delivered: " + refused.reason;
        return {true, stop(pane, c, "Watch ended: its request was not delivered (" + refused.reason + ")")};
    }

    // Evidence doors for loaded tests: read-only views of what this watch holds.
    std::int64_t lease() const noexcept { return watch_.lease; }
    std::uint64_t observations() const noexcept { return watch_.observations; }
    bool cycle_pending() const noexcept { return watch_.ask.pending() || watch_.read.pending(); }
    bool again() const noexcept { return watch_.again; }

private:
    struct Watch {
        enum class State { off, requesting, on } state = State::off;
        std::int64_t lease = 0;
        std::string subject;
        inventory::InventoryReference reference;
        loom::RoleRequest ask;  ///< PaneObservationRequested, then each PaneObservationContinued
        loom::RoleRequest read; ///< the approved InventoryRead
        bool again = false;     ///< a change during a cycle: one more cycle, never a queue
        std::uint64_t observations = 0;
        std::string last_good, last_failure;
    };

    std::string cycle(const std::string& pane, ViewContext& c) {
        const auto correlation = ++c.asks;
        if (!watch_.ask.send_to_role(c.mail.as_role(kInfoPaneRole), "zengine.workshop",
                workshop::PaneObservationContinued{pane, watch_.lease, watch_.subject}, correlation))
            return stop(pane, c, "Watch ended: its observation could not be queued", true);
        return {};
    }

    Watch watch_;
    /// A WATCH REQUEST STOPPED BEFORE WORKSHOP ANSWERED IT. Kept by the slot across retirement,
    /// at most one: its approval, if any, is ended on arrival (`hear(PaneObservationAnswered)`).
    loom::RoleRequest ending_;
};

} // namespace zengine::info_pane
#endif
