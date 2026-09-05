// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_MAKER_WEAVE_HPP
#define ZENGINE_MAKER_WEAVE_HPP

// THE INTERPRETER: one Loom weave per definition, implementing the raw `loom::Weave` contract
// over a state schema nothing in C++ declared (docs/reference/maker-weave.md).
//
// REGISTRATION. `register_definition` mounts the definition's trigger bodies into the host's ONE
// catalog under the revision's provider identity, constructs the weave at its default state,
// mints the grant from the definition's emits, and calls `register_weave` bound to the
// definition's name as its role -- so the first snapshot claims the data-built state schema, and
// the accept-set is the definition's shapes plus the doors every maker weave answers: the four
// poke doors, `zen.Activated`, and the package's own `Quiesce`, `Resume` and `Adopt`.
//
// DELIVERY. A trigger's message is packed with the state (state fields, then the message's),
// spent through `Catalog::evaluate` -- every operator resolved at spend, so a power overlaid
// underneath moves the trigger and revealing it moves it back -- and the one answer is written to
// the named state field; the output gate's kind check is the catalog's own. Each declared emit is
// then written field-wise from the new state and published under the weave's own grant. A body
// that cannot be spent leaves the state, answers `zen.Refused` with the deepest layer's words, and
// counts.
//
// INSPECTION. `zen.PokeDescribe` names the state schema and every field; `zen.PokeRead` reads a
// scalar; `zen.PokeWrite` and `zen.PokeResetState` are refused by name -- a maker weave's state
// is written by its triggers.
//
// THE CEREMONY DOORS TRUST THE SENDER, NOT THE SHAPE -- Loom's own rule. Every maker weave accepts
// `Quiesce`, `Resume` and `Adopt`, so the shape alone would let any participant freeze a weave,
// un-freeze one mid-edit, or rewrite its state with bytes that admit. Instead the HOST arms the
// weave for one boundary through the object it holds (`arm`): the coordinator's bus-stamped id and
// the boundary token. `Quiesce` and `Resume` are honoured only from that sender with that token;
// `Adopt` only while the weave is an unbound candidate and only from that sender. Anything else is
// refused by name and the weave keeps serving. A weave registered bound to its role is never a
// candidate; a candidate becomes bound when Loom attests its `zen.Activated`.
//
// THE TWO EDITS. A behaviour edit with the schema unchanged is `apply_behaviour_edit`: the
// successor revision's bodies mount beside the incumbent's, the weave takes the new definition,
// the old bodies unmount, and `swap_state` bumps the incarnation and announces `Revived` -- same
// WeaveId, state kept. A definition whose state schema differs is refused here and is a
// succession (maker/succession.hpp).

#include "maker/definition.hpp"
#include "maker/vocabulary.hpp"
#include "maker/write.hpp"
#include "operator/catalog.hpp"
#include "operator/migration.hpp"
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

/// The one port a trigger's body answers on.
inline constexpr const char* kAnswerPort = "value";

/// THE OPERATOR DEFINITIONS ONE REVISION MOUNTS: one composite per trigger -- inputs the pack,
/// output one port carrying the target field's own type, so the output gate is the kind check --
/// and, on a schema edit's successor, the conversion edge in MIG-0's convention: input the
/// predecessor's state schema whole, output one port carrying the successor's, body the
/// field-wise write closed over the conversion record.
// MW-WEAVE-04, MW-WEAVE-05 -- agents/maker/weave.md; MW-SUCC-03 -- agents/maker/succession.md
inline std::vector<op::OperatorDef> definitions_of(const Definition& d) {
    std::vector<op::OperatorDef> out;
    for (const On& trigger : d.on) {
        const std::string identity = d.trigger_identity(trigger);
        const loom::Field* target = d.state->find(trigger.output);
        if (target == nullptr) {
            throw std::invalid_argument("the trigger on `" + trigger.message->name() +
                                        "` writes `" + trigger.output +
                                        "`, which the state does not declare");
        }
        auto answer = loom::make_schema(
            identity + ".out", 1,
            std::vector<loom::Field>{loom::Field{kAnswerPort, target->type, /*required=*/true}});
        out.emplace_back(identity, pack_schema(identity, *d.state, *trigger.message),
                         std::move(answer), trigger.body);
    }
    if (d.conversion) {
        const std::shared_ptr<const loom::Schema> to = d.state;
        const std::vector<FieldSource> fields = d.conversion->fields;
        const std::optional<std::vector<std::string>> drops = d.conversion->drops;
        out.push_back(op::make_migration(
            d.conversion->from, to, [to, fields, drops](const loom::Value& in) -> loom::Cell {
                Written written = write_fields(in, to, fields, drops, true, "the conversion");
                if (!written) {
                    // A refusal is a throw here, and `Catalog::run` turns it into this
                    // evaluation's reason -- the write's own words reach the coordinator.
                    throw std::runtime_error(written.reason);
                }
                return loom::Cell::message(std::move(*written.value));
            }));
    }
    return out;
}

/// A maker weave. Owned by the Switchboard once registered; the host reaches it through
/// `Switchboard::weave(id)` (host authority) or the pointer `register_definition` returns.
// MW-WEAVE-01 -- agents/maker/weave.md
class Weave final : public loom::Weave {
public:
    /// The catalog must outlive the bus that owns this weave: the destructor unmounts.
    Weave(op::Catalog& catalog, Definition definition)
        : catalog_(&catalog), definition_(std::move(definition)),
          state_(default_value(definition_.state)) {}

    ~Weave() override { unmount(); }

    // ---- the package's side --------------------------------------------------------------

    /// Mount this revision's bodies. Called by `register_definition` before the weave is handed
    /// to the bus, so a mount refusal registers nothing.
    op::MountReport mount() {
        op::MountReport report = catalog_->mount(definition_.provider(), definitions_of(definition_));
        if (report) {
            mounted_provider_ = definition_.provider();
        }
        return report;
    }

    /// Unmount this revision's bodies, revealing whatever they covered.
    void unmount() {
        if (!mounted_provider_.empty()) {
            catalog_->unmount(mounted_provider_);
            mounted_provider_.clear();
        }
    }

    /// Take a successor definition whose state schema is this one's -- a behaviour edit. The
    /// caller has mounted the successor's bodies already; this unmounts the incumbent's.
    void adopt_definition(Definition next) {
        unmount();
        definition_ = std::move(next);
        mounted_provider_ = definition_.provider();
    }

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
        bus.send(to, loom::Message(std::move(payload), self_, self_, in.correlation));
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
        const std::string identity = definition_.trigger_identity(trigger);
        const op::OperatorDef* def = catalog_->find(identity);
        if (def == nullptr) {
            ++refused_;
            refuse(in, bus, "unresolved trigger `" + identity + "`: its body is not mounted");
            return;
        }
        op::Evaluation answered =
            catalog_->evaluate(identity, pack(state_, in.payload, def->inputs()));
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
            bus.publish(loom::Message(std::move(*written.value), self_, self_, in.correlation));
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

    op::Catalog* catalog_;
    Definition definition_;
    loom::Value state_;
    loom::WeaveId self_{};
    std::string mounted_provider_;
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

// ---- registration -------------------------------------------------------------------------------

/// The grant a definition implies: each emitted shape to any accepter, the poke answers, and the
/// two ceremony answers this weave speaks (`Quiesced` to its coordinator, `Adopted` through the
/// answer door). The trusted in-process default; a host that wants its own passes one.
// MW-WEAVE-07 -- agents/maker/weave.md
inline loom::Grant default_grant(const Definition& d) {
    loom::Grant grant;
    for (const auto& s : d.emits) {
        grant.allow_to_any(s->name(), s->version());
    }
    loom::allow_poke_answers(grant);
    grant.allow_to_any(Quiesced::zen_name, Quiesced::zen_version);
    grant.allow_to_any(Adopted::zen_name, Adopted::zen_version);
    return grant;
}

/// What registering a definition produced.
struct Registered {
    bool ok = false;
    std::string reason;
    loom::WeaveId id{};
    Weave* weave = nullptr;

    static Registered no(std::string why) {
        Registered r;
        r.reason = std::move(why);
        return r;
    }
    explicit operator bool() const noexcept { return ok; }
};

/// REGISTER ONE LOOM WEAVE FROM A DEFINITION. Mounts the bodies (a refusal registers nothing),
/// constructs the weave at its default state, mints the grant, registers it bound to the
/// definition's name as its role -- or unbound, for a candidate that a succession will seal.
/// Returns after registration; nothing here pumps.
// MW-WEAVE-01 -- agents/maker/weave.md
inline Registered register_definition(loom::Switchboard& bus, op::Catalog& catalog,
                                      Definition definition,
                                      std::optional<loom::Grant> grant = std::nullopt,
                                      bool hold_role = true) {
    auto weave = std::make_unique<Weave>(catalog, std::move(definition));
    Weave* raw = weave.get();
    op::MountReport mounted;
    try {
        mounted = raw->mount();
    } catch (const std::exception& e) {
        // Admission guarantees the bodies can be built; a definition that reached here
        // another way is refused in the builder's words rather than thrown at the host.
        return Registered::no(e.what());
    }
    if (!mounted) {
        return Registered::no(mounted.reason);
    }
    loom::Grant authority = grant ? std::move(*grant) : default_grant(raw->definition());
    const std::string role = raw->definition().name;
    loom::WeaveId id;
    try {
        id = hold_role ? bus.register_weave(std::move(weave), std::move(authority), role)
                       : bus.register_weave(std::move(weave), std::move(authority));
    } catch (const std::exception& e) {
        // The weave was destroyed with the refused registration, and its destructor unmounted.
        return Registered::no(e.what());
    }
    raw->set_self(id);
    raw->set_bound(hold_role);
    Registered r;
    r.ok = true;
    r.id = id;
    r.weave = raw;
    return r;
}

// ---- the behaviour edit --------------------------------------------------------------------------

/// What an edit did, or why not.
struct Edited {
    bool ok = false;
    std::string reason;

    static Edited no(std::string why) {
        Edited e;
        e.reason = std::move(why);
        return e;
    }
    explicit operator bool() const noexcept { return ok; }
};

/// A BEHAVIOUR EDIT: the same state schema, a new revision of the triggers. The successor's
/// bodies mount beside the incumbent's (a refusal changes nothing), the live weave takes the new
/// definition and unmounts the old bodies, and `swap_state` re-admits the state it already holds
/// -- same WeaveId, incarnation bumped, `Revived` announced, the old incarnation's deferred
/// answers ended. A definition whose state schema differs is refused here: that is a succession.
// MW-WEAVE-09 -- agents/maker/weave.md
inline Edited apply_behaviour_edit(loom::Switchboard& bus, op::Catalog& catalog, loom::WeaveId id,
                                   Definition next) {
    auto* live = dynamic_cast<Weave*>(bus.weave(id));
    if (live == nullptr) {
        return Edited::no("no maker weave is registered at that id");
    }
    const Definition& current = live->definition();
    if (next.name != current.name) {
        return Edited::no("`" + next.name + "` is not `" + current.name +
                          "`; a behaviour edit keeps the maker's name");
    }
    if (!loom::same_identity(*current.state, *next.state)) {
        return Edited::no("`" + next.state->name() + " v" + std::to_string(next.state->version()) +
                          "` is not `" + current.state->name() + " v" +
                          std::to_string(current.state->version()) +
                          "`: a definition whose state schema differs is a schema edit -- a "
                          "succession, not a behaviour edit");
    }
    if (next.provider() == current.provider()) {
        return Edited::no("a behaviour edit bumps the revision; `" + current.provider() +
                          "` is the live one");
    }
    op::MountReport mounted;
    try {
        mounted = catalog.mount(next.provider(), definitions_of(next));
    } catch (const std::exception& e) {
        return Edited::no(e.what());
    }
    if (!mounted) {
        return Edited::no(mounted.reason);
    }
    live->adopt_definition(std::move(next));
    const loom::ReviveOutcome swapped = bus.swap_state(id, loom::serialize(live->snapshot()));
    if (!swapped.revived) {
        return Edited::no("swap_state refused the state it already held: " +
                          swapped.refusal.message());
    }
    Edited e;
    e.ok = true;
    return e;
}

} // namespace zengine::maker

#endif // ZENGINE_MAKER_WEAVE_HPP
