// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_WEAVE_HPP
#define ZENGINE_INVENTORY_WEAVE_HPP

// THE INVENTORY WEAVE: one slot, empty or one associated item+metadata pair, reached only by
// ordinary message (vocabulary.hpp names the doors; codec.hpp owns the pair's own encoding).
//
// Set/Get are entirely schema-opaque: this weave decodes a Set's `pair` bytes only far enough to
// validate them (codec.hpp's `decode_pair`, which recovers its own schemas from the bytes alone),
// and never inspects what the item or a metadata entry actually claims to be. A new item schema
// is therefore never a reason to touch this file.
//
// InventoryCaptureDescribe is the one narrow exception, and the whole reason it exists here
// rather than being left to a caller: `zen.PokeDescribe` -> `zen.PokeStructure` is the one
// substrate door EVERY woven Weave already answers unconditionally (the "no secret state" floor,
// `zen/weave/poke.hpp`) -- so it is a genuinely general capture source, not a hand-picked one --
// and `zen.PokeStructure` is a real ZEN_SHAPE struct the authoring sugar already knows how to
// receive, unlike `zen.AcceptedShapes` (hand-built, no C++ struct of its own). The capture is
// done HERE, server-side, so the automatically-derived `CaptureContext.answered_by` is a fact
// Loom itself attested (`mail.sender()` on the PokeStructure reply) rather than a claim a client
// relayed -- and so the byte-level envelope construction (schema closure + native serialization,
// see codec.hpp) is written exactly once, in the one language that already has it.
//
// A caller reaches InventoryCaptureDescribe/InventorySet/InventoryGet only under the guest
// "inventory" power (workshop/guests.hpp) -- a narrow grant of its own, never a reinterpretation
// of "input"/"capture"/"inspect". What THIS weave may itself send -- `zen.PokeDescribe` to an
// arbitrary target, and its own three answer shapes -- is `inventory_grant()`, below, entirely
// separate from any guest's grant.

#include "inventory/codec.hpp"
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
/// are always visible -- the no-secret-state floor), but `pair` is Bytes, so only Get can ever
/// hand its contents out: `loom::poke_read` refuses every field whose kind is not a message-read
/// scalar. That is what makes Get -- a deliberate, ordinary message -- the one retrieval door.
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
          loom::Emit<InventoryState, loom::Ack, loom::Refused, loom::PokeDescribe>> {
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
        state_.occupied = true;
        state_.pair = req.pair;
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
        (void)mail.send_to_role(req.target_role, loom::PokeDescribe{}, opened.correlation);
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
        try {
            const std::string encoded = encode_pair(loom::to_value(structure), {loom::to_value(ctx)});
            state_.occupied = true;
            state_.pair.assign(encoded.begin(), encoded.end());
        } catch (const std::exception& e) {
            finish(mail, loom::Refused{std::string("capture could not be stored: ") + e.what()});
            return;
        }
        finish(mail, loom::Ack{});
    }

    /// THE SEND ITSELF DID NOT REACH ANYONE: an unheld role, a gate refusal, or the like. Matched
    /// by the shape and role this weave's own outstanding ask named -- book_ holds at most one
    /// conversation, so there is nothing else this notice could be about while one is open.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!pending_answer_.valid() || !book_.awaiting()) {
            return;
        }
        if (refused.role != requested_role_ || refused.shape != loom::PokeDescribe::zen_name) {
            return;
        }
        (void)book_.forget(book_.entries().front().id);
        finish(mail, loom::Refused{"'" + requested_role_ +
                                   "' could not be asked zen.PokeDescribe: " + refused.reason});
    }

private:
    /// THE ONE WALL: correlation identifies the conversation, `mail.sender()` -- stamped by
    /// Loom, never read from the payload -- authenticates who may settle it; `book_.settle`
    /// requires both (ask_book.hpp). `Mail::answers_ask()` is NOT layered on top here,
    /// deliberately: it is set only for a respondent that replies through `bus.answer(...)`,
    /// and the one answer this weave awaits, `zen.PokeStructure`, is the construction layer's
    /// own substrate reply -- sent with an ordinary `bus.send` (`WeaveBase::answer_substrate`),
    /// answering no less genuinely for it. The book's own pair is the whole wall this needs.
    std::optional<loom::PendingAsk> settled(loom::Mail& mail) {
        if (!pending_answer_.valid()) {
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

/// THIS WEAVE'S OWN AUTHORITY TO SPEAK -- entirely separate from which guest may reach its
/// doors (workshop/guests.hpp's `kPowerInventory`). `PokeDescribe -> any` is what the capture
/// door spends; the rest answers Set/Get/CaptureDescribe and this weave's own poke/describe
/// floor. A host mounts this weave with `mount_in_office` (workshop.cpp), so nothing is derived
/// from Emit<...> automatically -- this grant is the whole of it.
inline loom::Grant inventory_grant() {
    loom::Grant g;
    g.allow_to_any(loom::PokeDescribe::zen_name, loom::PokeDescribe::zen_version);
    g.allow_to_any(InventoryState::zen_name, InventoryState::zen_version);
    g.allow_to_any(loom::Ack::zen_name, loom::Ack::zen_version);
    g.allow_to_any(loom::Refused::zen_name, loom::Refused::zen_version);
    loom::allow_poke_answers(g);
    loom::allow_describe_answers(g);
    return g;
}

} // namespace zengine::inventory

#endif // ZENGINE_INVENTORY_WEAVE_HPP
