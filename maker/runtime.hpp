// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_MAKER_RUNTIME_HPP
#define ZENGINE_MAKER_RUNTIME_HPP

// Shared message/state behavior for interpreted and generated maker weaves.
// Only evaluation of a trigger body differs between the two forms.

#include "maker/definition.hpp"
#include "maker/vocabulary.hpp"
#include "maker/write.hpp"
#include "operator/operator.hpp"

#include <zen/gate.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard/bus.hpp>
#include <zen/switchboard/grant.hpp>
#include <zen/switchboard/message.hpp>
#include <zen/switchboard/switchboard.hpp>
#include <zen/switchboard/weave_contract.hpp>
#include <zen/value.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/poke.hpp>
#include <zen/weave/shape.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace zengine::maker {

/// A maker weave. Owned by the Switchboard once registered; the host reaches it through
/// `Switchboard::weave(id)` (host authority) or the pointer `register_definition` returns.
// MW-WEAVE-01 -- agents/maker/weave.md
class Runtime : public loom::Weave {
public:
    explicit Runtime(Definition definition)
        : definition_(std::move(definition)), state_(default_value(definition_.state)) {}

    void set_self(loom::WeaveId id) noexcept { self_ = id; }
    loom::WeaveId id() const noexcept { return self_; }
    const Definition& definition() const noexcept { return definition_; }

    /// HOST AUTHORITY: this weave holds a role (it is the service), or it is an unbound candidate.
    /// Set by `register_definition`; a candidate turns bound when its activation is attested.
    void set_bound(bool bound) noexcept { bound_ = bound; }
    bool bound() const noexcept { return bound_; }

    /// HOST AUTHORITY: arm this weave for one boundary -- the coordinator whose `Quiesce`,
    /// `Resume` and `Adopt` are honoured, and the token they must carry. Called by
    /// `begin_schema_edit` through the object the host holds, never by a message; `disarm` ends
    /// it, and `Resume` from the armed coordinator disarms.
    // MW-WEAVE-10 -- agents/maker/weave.md
    void arm(loom::WeaveId coordinator, std::int64_t token) noexcept {
        coordinator_ = coordinator;
        token_ = token;
    }
    void disarm() noexcept {
        coordinator_ = loom::WeaveId{};
        token_ = 0;
    }
    loom::WeaveId coordinator() const noexcept { return coordinator_; }
    const loom::Value& state() const noexcept { return state_; }

    /// Triggers that ran and wrote.
    std::uint64_t handled() const noexcept { return handled_; }
    /// Bodies that could not be spent, emits that could not be written.
    std::uint64_t refused() const noexcept { return refused_; }
    /// Messages declined by name after the boundary, while this weave still held the role.
    std::uint64_t refused_after_boundary() const noexcept { return refused_after_boundary_; }
    /// Ceremony messages refused at the door: a stranger's, or an `Adopt` on a bound weave.
    std::uint64_t refused_at_door() const noexcept { return refused_at_door_; }
    bool quiescing() const noexcept { return quiescing_; }
    /// The sequence of the last `zen.Activated` delivered here, and whether Loom attested it.
    std::optional<std::int64_t> activated() const noexcept { return activated_; }
    bool activation_attested() const noexcept { return activation_attested_; }

    // ---- loom::Weave ----------------------------------------------------------------------

    /// The definition's accepted shapes, then the doors every maker weave answers.
    // MW-WEAVE-02 -- agents/maker/weave.md
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        std::vector<std::shared_ptr<const loom::Schema>> out = definition_.accepts;
        auto add = [&out](std::shared_ptr<const loom::Schema> s) {
            for (const auto& have : out) {
                if (have->name() == s->name() && have->version() == s->version()) {
                    return;
                }
            }
            out.push_back(std::move(s));
        };
        for (auto& s : loom::poke_door_schemas()) {
            add(std::move(s));
        }
        add(loom::schema_of<loom::Activated>());
        add(loom::schema_of<Quiesce>());
        add(loom::schema_of<Resume>());
        add(loom::schema_of<Adopt>());
        return out;
    }

    std::vector<std::shared_ptr<const loom::Schema>> emitted_schemas() const override {
        return definition_.emits;
    }

    void handle(const loom::Message& in, loom::Bus& bus) override {
        const loom::Schema& shape = in.payload.schema();
        if (loom::same_identity(*loom::schema_of<loom::PokeDescribe>(), shape)) {
            answer(in, bus, loom::to_value(structure()));
            return;
        }
        if (loom::same_identity(*loom::schema_of<loom::PokeRead>(), shape)) {
            const loom::PokeRead req = loom::from_value<loom::PokeRead>(in.payload);
            std::visit([&](const auto& a) { answer(in, bus, loom::to_value(a)); }, read(req.field));
            return;
        }
        if (loom::same_identity(*loom::schema_of<loom::PokeWrite>(), shape) ||
            loom::same_identity(*loom::schema_of<loom::PokeResetState>(), shape)) {
            refuse(in, bus,
                   "a maker weave's state is written by its triggers; zen.PokeWrite and "
                   "zen.PokeResetState are refused -- send the message a trigger accepts");
            return;
        }
        if (loom::same_identity(*loom::schema_of<loom::Activated>(), shape)) {
            const loom::Activated fact = loom::from_value<loom::Activated>(in.payload);
            activated_ = fact.sequence;
            activation_attested_ = in.provenance.lifecycle_activation() &&
                                   in.provenance.attested_sequence() == fact.sequence;
            if (activation_attested_) {
                // Loom's word that this incarnation is the committed service: a candidate no
                // longer, so `Adopt` is refused from here on, whoever asks.
                bound_ = true;
            }
            return;
        }
        if (loom::same_identity(*loom::schema_of<Quiesce>(), shape)) {
            const Quiesce boundary = loom::from_value<Quiesce>(in.payload);
            if (!from_armed_coordinator(in, boundary.token)) {
                refuse_at_door(in, bus, "zengine.maker.Quiesce");
                return;
            }
            // THE BOUNDARY (HANDOFF-02): from here the state is final and exact, and every
            // trigger message is declined by name until Resume or retirement.
            quiescing_ = true;
            Quiesced final;
            final.token = boundary.token;
            const std::string bytes = loom::serialize(state_);
            final.state.assign(bytes.begin(), bytes.end());
            answer(in, bus, loom::to_value(final));
            return;
        }
        if (loom::same_identity(*loom::schema_of<Resume>(), shape)) {
            if (!from_armed_coordinator(in, loom::from_value<Resume>(in.payload).token)) {
                refuse_at_door(in, bus, "zengine.maker.Resume");
                return;
            }
            quiescing_ = false;
            disarm();
            return;
        }
        if (loom::same_identity(*loom::schema_of<Adopt>(), shape)) {
            if (bound_) {
                ++refused_at_door_;
                refuse(in, bus,
                       "`zengine.maker.Adopt` refused: `" + definition_.name +
                           "` is bound to its role and is not a candidate; a maker weave's state "
                           "is written by its triggers");
                return;
            }
            if (!(coordinator_.valid() && in.sender == coordinator_)) {
                refuse_at_door(in, bus, "zengine.maker.Adopt");
                return;
            }
            adopt(in, bus);
            return;
        }
        for (const On& trigger : definition_.on) {
            if (!loom::same_identity(*trigger.message, shape)) {
                continue;
            }
            if (quiescing_) {
                ++refused_after_boundary_;
                refuse(in, bus,
                       "`" + shape.name() + " v" + std::to_string(shape.version()) +
                           "` refused: `" + definition_.name +
                           "` has quiesced for a schema edit and its final value is authored; "
                           "the successor handles it once the role moves");
                return;
            }
            fire(trigger, in, bus);
            return;
        }
        refuse(in, bus, "no trigger of `" + definition_.name + "` accepts `" + shape.name() + "`");
    }

    loom::Value snapshot() const override { return state_; }

    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(4));
        v.set("revive_from_last_good", loom::Cell::boolean(true));
        return v;
    }

    void revive(const loom::Value& state) override { state_ = state; }

private:
    /// Send a substrate answer to the requester: reply_to if given, else the stamped sender --
    /// `WeaveBase::answer_substrate`'s one rule, restated for a raw weave. A request with
    /// neither has nowhere to answer and is performed silently by design.
    void answer(const loom::Message& in, loom::Bus& bus, loom::Value payload) {
        const loom::WeaveId to = in.reply_to.valid() ? in.reply_to : in.sender;
        if (!to.valid()) {
            return;
        }
        bus.send(to, loom::Message(std::move(payload), self_, {}, in.correlation));
    }

    void refuse(const loom::Message& in, loom::Bus& bus, std::string reason) {
        answer(in, bus, loom::to_value(loom::Refused{std::move(reason)}));
    }

    /// Is this delivery the armed coordinator's, carrying this boundary's token? The sender is
    /// the bus's stamp, never a payload field.
    // MW-WEAVE-10 -- agents/maker/weave.md
    bool from_armed_coordinator(const loom::Message& in, std::int64_t token) const noexcept {
        return coordinator_.valid() && in.sender == coordinator_ && token == token_;
    }

    void refuse_at_door(const loom::Message& in, loom::Bus& bus, const char* shape) {
        ++refused_at_door_;
        refuse(in, bus,
               std::string("`") + shape + "` refused: the sender is not the coordinator the host "
                                          "armed for `" + definition_.name +
                                          "`'s boundary, or the token is not this boundary's; the "
                                          "weave keeps serving");
    }

    /// One trigger: pack, spend, write back, emit.
    // MW-WEAVE-03, MW-WEAVE-04, MW-WEAVE-06, MW-WEAVE-07 -- agents/maker/weave.md
    void fire(const On& trigger, const loom::Message& in, loom::Bus& bus) {
        op::Evaluation answered = evaluate_body(trigger, in.payload);
        if (!answered) {
            ++refused_;
            refuse(in, bus, answered.reason());
            return;
        }
        loom::Value next = state_;
        next.set(trigger.output, *answered.value().at(0));
        state_ = std::move(next);
        ++handled_;
        for (const Emit& e : trigger.emits) {
            Written written = write_fields(state_, e.message, e.fields, std::nullopt, false,
                                           "the emit of `" + e.message->name() + "`");
            if (!written) {
                ++refused_;
                refuse(in, bus, written.reason);
                continue;
            }
            bus.publish(loom::Message(std::move(*written.value), self_, {}, in.correlation));
        }
    }

    /// The preparation ask: admit the converted bytes at THIS weave's own state schema -- the
    /// one gate -- and answer for itself through `Bus::answer`.
    // MW-SUCC-01 -- agents/maker/succession.md
    void adopt(const loom::Message& in, loom::Bus& bus) {
        const Adopt ask = loom::from_value<Adopt>(in.payload);
        const std::string_view bytes(reinterpret_cast<const char*>(ask.state.data()),
                                     ask.state.size());
        loom::Admission admitted = loom::admit(loom::parse(bytes), definition_.state);
        Adopted reply;
        if (admitted) {
            state_ = std::move(admitted).value();
            reply.ready = true;
        } else {
            reply.why = admitted.first_error().message();
        }
        bus.answer(loom::Message(loom::to_value(reply), self_));
    }

    /// Every field, name and type, none hidden, none writable.
    // MW-WEAVE-08 -- agents/maker/weave.md
    loom::PokeStructure structure() const {
        loom::PokeStructure out;
        out.state_schema = definition_.state->name();
        out.state_version = static_cast<std::int64_t>(definition_.state->version());
        for (const loom::Field& f : definition_.state->fields()) {
            out.fields.push_back(loom::PokeFieldInfo{f.name, loom::poke_type_name(f.type),
                                                     /*writable=*/false, /*hidden=*/false});
        }
        return out;
    }

    std::variant<loom::Result, loom::Refused> read(std::string_view field) const {
        const loom::Field* f = definition_.state->find(field);
        if (f == nullptr) {
            return loom::Refused{"no field '" + std::string(field) +
                                 "' -- zen.PokeDescribe lists the structure"};
        }
        const loom::Cell* c = state_.get(field);
        if (c == nullptr) {
            return loom::Refused{"field '" + std::string(field) + "' is optional and absent"};
        }
        switch (c->kind()) {
        case loom::Kind::Int:
            return loom::Result{loom::poke_render(c->as_int())};
        case loom::Kind::Float:
            return loom::Result{loom::poke_render(c->as_float())};
        case loom::Kind::Text:
            return loom::Result{loom::poke_render(c->as_text())};
        case loom::Kind::Bool:
            return loom::Result{loom::poke_render(c->as_bool())};
        default:
            return loom::Refused{"field '" + std::string(field) + "' has kind " +
                                 loom::poke_type_name(f->type) +
                                 " -- only scalar fields are message-readable this phase"};
        }
    }

protected:
    virtual op::Evaluation evaluate_body(const On& trigger, const loom::Value& message) = 0;
    Definition definition_;

private:
    loom::Value state_;
    loom::WeaveId self_{};
    std::uint64_t handled_ = 0;
    std::uint64_t refused_ = 0;
    std::uint64_t refused_after_boundary_ = 0;
    std::uint64_t refused_at_door_ = 0;
    bool quiescing_ = false;
    bool bound_ = true;
    loom::WeaveId coordinator_{};
    std::int64_t token_ = 0;
    std::optional<std::int64_t> activated_;
    bool activation_attested_ = false;
};

} // namespace zengine::maker

#endif // ZENGINE_MAKER_RUNTIME_HPP
