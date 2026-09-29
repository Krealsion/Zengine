// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_MAKER_SUCCESSION_HPP
#define ZENGINE_MAKER_SUCCESSION_HPP

// A schema edit is a succession: a prepared replacement whose conversion is written as data in the
// successor's definition, mounted as an edge in `operator/migration.hpp`'s convention and spent by
// the host's one coordinator, `Succession`. It needs nothing Loom's prepared replacement does not
// already offer. The six steps, and who may speak at the boundary: docs/reference/maker-weave.md.

#include "maker/definition.hpp"
#include "maker/vocabulary.hpp"
#include "maker/weave.hpp"
#include "operator/catalog.hpp"
#include "operator/migration.hpp"

#include <zen/host/prepared_replacement.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard/bus.hpp>
#include <zen/switchboard/grant.hpp>
#include <zen/switchboard/message.hpp>
#include <zen/switchboard/switchboard.hpp>
#include <zen/switchboard/weave_contract.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::maker {

/// The coordinator's own (nearly empty) state shape: every weave has one.
inline std::shared_ptr<const loom::Schema> succession_state_schema() {
    static const auto s = loom::SchemaBuilder("zengine.maker.SuccessionState", 1)
                              .field("edits", loom::Kind::Int)
                              .build();
    return s;
}

/// THE COORDINATOR of a maker weave's schema edits. One per host, generic across definitions: it
/// speaks the five ceremony shapes and carries state as bytes, so it needs no weave's schema.
// MW-SUCC-06 -- agents/maker/succession.md
class Succession final : public loom::Weave {
public:
    explicit Succession(op::Catalog& catalog) : catalog_(&catalog) {}

    void set_self(loom::WeaveId id) noexcept { self_ = id; }
    loom::WeaveId id() const noexcept { return self_; }

    /// Bind this coordinator to one edit: the host's handle, the successor's state schema (the
    /// edge's target), the incumbent, and the boundary token.
    void begin(loom::PreparedReplacement* txn, std::shared_ptr<const loom::Schema> target,
               loom::WeaveId incumbent, std::int64_t token) {
        txn_ = txn;
        target_ = std::move(target);
        incumbent_ = incumbent;
        token_ = token;
        quiesce_sent_ = true;
        quiesced_ = converted_ = asked_ = adopted_ = ready_ = offered_ = aborted_ = false;
        reason_.clear();
        why_.clear();
        ++edits_;
    }

    /// Forget the edit: after commit or abort.
    void end() {
        txn_ = nullptr;
        quiesce_sent_ = false;
    }

    // ---- what happened, for the host ---------------------------------------------------------
    bool quiesce_sent() const noexcept { return quiesce_sent_; }
    bool quiesced() const noexcept { return quiesced_; }
    bool converted() const noexcept { return converted_; }
    bool asked() const noexcept { return asked_; }
    bool adopted() const noexcept { return adopted_; }
    bool ready() const noexcept { return ready_; }
    bool offered() const noexcept { return offered_; }
    bool aborted() const noexcept { return aborted_; }
    /// The edge's refusal, in the write's or the gate's own words.
    const std::string& reason() const noexcept { return reason_; }
    /// The candidate's own reason for refusing to adopt.
    const std::string& why() const noexcept { return why_; }
    loom::WeaveId incumbent() const noexcept { return incumbent_; }
    std::int64_t token() const noexcept { return token_; }

    // ---- loom::Weave ----------------------------------------------------------------------

    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<Quiesced>(), loom::schema_of<Adopted>()};
    }

    void handle(const loom::Message& in, loom::Bus& bus) override {
        const loom::Schema& shape = in.payload.schema();
        if (loom::same_identity(*loom::schema_of<Quiesced>(), shape)) {
            on_quiesced(loom::from_value<Quiesced>(in.payload), in, bus);
            return;
        }
        if (loom::same_identity(*loom::schema_of<Adopted>(), shape)) {
            on_adopted(loom::from_value<Adopted>(in.payload));
            return;
        }
    }

    loom::Value snapshot() const override {
        loom::Value v(succession_state_schema());
        v.set("edits", loom::Cell::integer(edits_));
        return v;
    }

    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(4));
        v.set("revive_from_last_good", loom::Cell::boolean(true));
        return v;
    }

    void revive(const loom::Value& state) override {
        if (const loom::Cell* c = state.get("edits"); c != nullptr) {
            edits_ = c->as_int();
        }
    }

private:
    /// THE EDGE, SPENT: the incumbent's final bytes through `op::migrate` to the successor's
    /// state schema. A refusal reaches no candidate.
    // MW-SUCC-03, MW-SUCC-04 -- agents/maker/succession.md
    void on_quiesced(const Quiesced& final, const loom::Message& in, loom::Bus& bus) {
        if (final.token != token_ || !(in.sender == incumbent_)) {
            return; // not this edit's boundary; a stranger's word converts nothing
        }
        quiesced_ = true;
        const std::string_view bytes(reinterpret_cast<const char*>(final.state.data()),
                                     final.state.size());
        const op::Evaluation edge = op::migrate(*catalog_, loom::parse(bytes), target_);
        if (!edge) {
            reason_ = edge.reason();
            aborted_ = true;
            if (txn_ != nullptr) {
                txn_->abort();
            }
            bus.send(incumbent_,
                     loom::Message(loom::to_value(Resume{token_}), self_, self_, in.correlation));
            return;
        }
        converted_ = true;
        const std::string converted = loom::serialize(op::migrated(edge));
        Adopt ask;
        ask.state.assign(converted.begin(), converted.end());
        if (txn_ != nullptr) {
            asked_ = txn_->ask(ask).ok;
        }
    }

    /// The candidate's own answer, offered to the transaction's authenticated readiness gate.
    void on_adopted(const Adopted& answer) {
        adopted_ = true;
        ready_ = answer.ready;
        why_ = answer.why;
        if (txn_ != nullptr) {
            offered_ = txn_->offer_current_answer(answer.ready ? loom::PreparationAnswer::Ready
                                                               : loom::PreparationAnswer::Refused)
                           .ok;
        }
    }

    op::Catalog* catalog_;
    loom::WeaveId self_{};
    loom::PreparedReplacement* txn_ = nullptr;
    std::shared_ptr<const loom::Schema> target_;
    loom::WeaveId incumbent_{};
    std::int64_t token_ = 0;
    std::int64_t edits_ = 0;
    bool quiesce_sent_ = false;
    bool quiesced_ = false;
    bool converted_ = false;
    bool asked_ = false;
    bool adopted_ = false;
    bool ready_ = false;
    bool offered_ = false;
    bool aborted_ = false;
    std::string reason_;
    std::string why_;
};

/// The registered coordinator: its id and the weave the bus owns.
struct Coordinator {
    loom::WeaveId id{};
    Succession* weave = nullptr;
};

/// Register the host's one coordinator, granted the three ceremony shapes it speaks.
inline Coordinator register_succession(loom::Switchboard& bus, op::Catalog& catalog) {
    auto weave = std::make_unique<Succession>(catalog);
    Succession* raw = weave.get();
    loom::Grant grant;
    grant.allow_to_any(Quiesce::zen_name, Quiesce::zen_version);
    grant.allow_to_any(Resume::zen_name, Resume::zen_version);
    grant.allow_to_any(Adopt::zen_name, Adopt::zen_version);
    const loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant));
    raw->set_self(id);
    return Coordinator{id, raw};
}

/// What beginning a schema edit did, or why not.
struct Begun {
    bool ok = false;
    std::string reason;
    loom::WeaveId incumbent{};
    loom::WeaveId candidate{};
    Weave* candidate_weave = nullptr;

    static Begun no(std::string why) {
        Begun b;
        b.reason = std::move(why);
        return b;
    }
    explicit operator bool() const noexcept { return ok; }
};

/// BEGIN A SCHEMA EDIT: register the successor unbound, seal it to the coordinator, begin the
/// prepared replacement around it, bind the coordinator, and send `Quiesce` to the incumbent as
/// the coordinator. SCHEDULES AND RETURNS: nothing here pumps, and commit is the host's call
/// (`txn.commit(sequence)`), as the handle's own law says. A refusal at any step leaves the
/// world as it was.
// MW-SUCC-01 -- agents/maker/succession.md; MW-WEAVE-10 -- agents/maker/weave.md
inline Begun begin_schema_edit(loom::Switchboard& bus, op::Catalog& catalog,
                               const Coordinator& coordinator, const std::string& role,
                               Definition successor, loom::PreparedReplacement& txn,
                               std::uint32_t budget = 16, std::int64_t token = 1) {
    if (!successor.conversion) {
        return Begun::no("a schema edit's successor carries a conversion from the predecessor's "
                         "state; without one it is a behaviour edit or a new weave");
    }
    const loom::WeaveId incumbent = bus.role_holder(role);
    if (!incumbent.valid()) {
        return Begun::no("nobody holds the role `" + role + "`");
    }
    auto* incumbent_weave = dynamic_cast<Weave*>(bus.weave(incumbent));
    if (incumbent_weave == nullptr) {
        return Begun::no("the holder of `" + role + "` is not a maker weave");
    }
    Registered candidate =
        register_definition(bus, catalog, std::move(successor), std::nullopt, /*hold_role=*/false);
    if (!candidate) {
        return Begun::no(candidate.reason);
    }
    if (!bus.seal_weave(candidate.id, coordinator.id)) {
        bus.unregister_weave(candidate.id);
        return Begun::no("the candidate could not be sealed to the coordinator");
    }
    const loom::PreparedReplacement::StartResult started =
        txn.start_existing({coordinator.id, coordinator.id, role, candidate.id, budget});
    if (!started.ok) {
        bus.unregister_weave(candidate.id);
        return Begun::no(std::string("the prepared replacement refused to begin: ") +
                         loom::name_of(started.begin_reason));
    }
    // WHO MAY SPEAK AT THE BOUNDARY: the host arms both weaves for this coordinator and this
    // token through the objects it holds -- never by a message -- before the first ceremony
    // message is sent. Any other sender's Quiesce, Resume or Adopt is refused by name.
    incumbent_weave->arm(coordinator.id, token);
    candidate.weave->arm(coordinator.id, token);
    coordinator.weave->begin(&txn, candidate.weave->definition().state, incumbent, token);
    bus.send_as(coordinator.id, incumbent,
                loom::Message(loom::to_value(Quiesce{token}), coordinator.id, coordinator.id,
                              static_cast<std::uint64_t>(token)));
    Begun b;
    b.ok = true;
    b.incumbent = incumbent;
    b.candidate = candidate.id;
    b.candidate_weave = candidate.weave;
    return b;
}

/// ABORT A SCHEMA EDIT the host decided against: the transaction aborts (the substrate discards
/// the sealed candidate; its destructor unmounts the successor's bodies) and, if the incumbent
/// was quiesced, `Resume` goes to it as the coordinator. Returns after scheduling.
// MW-SUCC-05 -- agents/maker/succession.md
inline loom::TxnResult abort_schema_edit(loom::Switchboard& bus, const Coordinator& coordinator,
                                         loom::PreparedReplacement& txn) {
    const loom::TxnResult aborted = txn.abort();
    if (coordinator.weave->quiesce_sent()) {
        bus.send_as(coordinator.id, coordinator.weave->incumbent(),
                    loom::Message(loom::to_value(Resume{coordinator.weave->token()}),
                                  coordinator.id, coordinator.id, 0));
    }
    coordinator.weave->end();
    return aborted;
}

} // namespace zengine::maker

#endif // ZENGINE_MAKER_SUCCESSION_HPP
