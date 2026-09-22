// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_WEAVE_HPP
#define ZENGINE_INVENTORY_WEAVE_HPP

// One loadable inventory: complete typed pairs are owned as independent bytes.
// CaptureDescribe is a narrow acquisition adapter; Set/Get stay schema-generic.
// See docs/reference/inventory.md for authority, pending behavior and lifetime.

#include "inventory/codec.hpp"
#include "inventory/grant.hpp"
#include "inventory/vocabulary.hpp"

#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/dispatch_refusal.hpp>
#include <zen/weave/poke.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace zengine::inventory {

/// Poke-inspectable like any weave's state (`occupied`, `pair`'s existence, type and tag-state
/// are always visible), but `pair` is Bytes, so scalar PokeRead cannot retrieve it.
/// Get returns the stored pair; a successful capture returns its own pair directly.
struct InventoryWeaveState {
    bool occupied = false;
    loom::Bytes pair;
    ZEN_SHAPE(InventoryWeaveState, 1, ZEN_FIELD(occupied), ZEN_FIELD(pair));
};

class InventoryWeave final
    : public loom::WeaveBase<
          InventoryWeave, InventoryWeaveState,
          loom::Accept<InventorySet, InventoryGet, InventoryCaptureDescribe, loom::PokeStructure,
                       loom::DispatchRefused>,
          loom::Emit<InventoryState, InventoryCaptured, loom::Ack, loom::Refused, loom::PokeDescribe>> {
public:
    InventoryWeave() : book_(1) {}

    // ---- the generic, schema-opaque doors -------------------------------------------------

    /// Replace the slot, or refuse whole. `decode_pair` is asked to validate `req.pair`
    /// completely -- malformed bytes, a closure that disagrees with itself, a value that does
    /// not admit against its own declared root -- before a single byte of the previous pair is
    /// touched; its decoded result is otherwise unused here; the stored form stays the caller's
    /// own bytes, which the next Get hands back exactly, rather than a re-encoding of what this
    /// weave happened to notice while checking.
    void on(const InventorySet& req, loom::Mail& mail) {
        try {
            (void)decode_pair(
                std::string_view(reinterpret_cast<const char*>(req.pair.data()), req.pair.size()));
        } catch (const std::exception& e) {
            (void)mail.answer(loom::Refused{std::string("Set refused: ") + e.what()});
            return;
        }
        state_.pair = req.pair;
        state_.occupied = true;
        (void)mail.answer(loom::Ack{});
    }

    /// Always answered: `occupied=false, pair=[]` before the first successful Set is the
    /// understandable empty result, not a refusal. `out.pair` is a value-copy of `state_.pair`,
    /// and the reply crosses the bus as its own admitted Value -- independently readable after a
    /// later Set replaces the slot, and unable to mutate this weave's storage back.
    void on(const InventoryGet&, loom::Mail& mail) {
        InventoryState out;
        out.occupied = state_.occupied;
        out.pair = state_.pair;
        (void)mail.answer(out);
    }

    // ---- the capture adapter's door -------------------------------------------------------

    void on(const InventoryCaptureDescribe& req, loom::Mail& mail) {
        if (book_.awaiting()) {
            (void)mail.answer(loom::Refused{"a capture is already in flight for this inventory"});
            return;
        }
        if (req.target_role.empty()) {
            (void)mail.answer(loom::Refused{"InventoryCaptureDescribe needs a target_role"});
            return;
        }
        loom::DeferredAnswer due = mail.defer_answer();
        if (!due.valid()) {
            (void)mail.answer(loom::Refused{"this delivery cannot defer its answer"});
            return;
        }
        pending_answer_ = std::move(due);
        requested_role_ = req.target_role;
        const loom::AskOpened opened = book_.open_to_role(
            req.target_role, loom::PokeDescribe::zen_name, loom::PokeDescribe::zen_version);
        if (!opened.ok) {
            finish(mail, loom::Refused{"this inventory's own book is full"});
            return;
        }
        const auto sent = mail.send_to_role(req.target_role, loom::PokeDescribe{}, opened.correlation);
        if (!sent.valid() || !book_.bind_attempt(opened.id, sent.seq)) {
            (void)book_.forget(opened.id);
            finish(mail, loom::Refused{"capture request could not be queued"});
        }
    }

    /// THE OBSERVED ANSWER. `structure` is a fresh struct this delivery decoded from its own
    /// gated Value -- independent of whatever `target_role`'s holder keeps -- and `mail.sender()`
    /// is Loom's own stamp, never something the reply's payload could claim to be.
    void on(const loom::PokeStructure& structure, loom::Mail& mail) {
        const std::optional<loom::PendingAsk> asked = settled(mail);
        if (!asked.has_value()) {
            return; // not this weave's own open conversation: data, and moves nothing
        }
        CaptureContext ctx;
        ctx.requested_role = requested_role_;
        ctx.request_shape = loom::PokeDescribe::zen_name;
        ctx.request_version = static_cast<std::int64_t>(loom::PokeDescribe::zen_version);
        ctx.answered_by = std::to_string(mail.sender().value);
        ctx.captured_at_epoch_s = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
        InventoryCaptured result;
        loom::Bytes stored;
        try {
            const std::string encoded = encode_pair(loom::to_value(structure), {
                loom::to_value(ctx), loom::to_value(CaptureRequest{{requested_role_}})});
            stored.assign(encoded.begin(), encoded.end());
            result.pair = stored;
        } catch (const std::exception& e) {
            finish(mail, loom::Refused{std::string("capture could not be stored: ") + e.what()});
            return;
        }
        state_.pair.swap(stored);
        state_.occupied = true;
        finish(mail, result);
    }

    // A notice is Loom's dispatch fact only with provenance AND this send's identity.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused() || !pending_answer_.valid()) {
            return;
        }
        const auto* asked = book_.match_attempt(refused.refused_attempt().seq);
        if (asked == nullptr || refused.role != requested_role_ || !refused.target.empty() ||
            refused.shape != loom::PokeDescribe::zen_name ||
            refused.version != loom::PokeDescribe::zen_version) {
            return;
        }
        (void)book_.forget(asked->id);
        finish(mail, loom::Refused{"'" + requested_role_ +
                                   "' could not be asked zen.PokeDescribe: " + refused.reason});
    }

private:
    // Role asks cannot pre-bind a respondent. AskBook matches bookkeeping; Loom's
    // answer provenance proves that the routed request earned this answer.
    std::optional<loom::PendingAsk> settled(loom::Mail& mail) {
        if (!pending_answer_.valid() || !mail.answers_ask()) {
            return std::nullopt;
        }
        return book_.settle(mail.correlation(), mail.sender());
    }

    template <class T>
    void finish(loom::Mail& mail, const T& answer) {
        loom::DeferredAnswer due = std::move(pending_answer_);
        pending_answer_ = loom::DeferredAnswer{};
        requested_role_.clear();
        (void)loom::answer_deferred(due, mail, answer);
    }

    loom::AskBook book_;
    loom::DeferredAnswer pending_answer_;
    std::string requested_role_;
};

} // namespace zengine::inventory

#endif // ZENGINE_INVENTORY_WEAVE_HPP
