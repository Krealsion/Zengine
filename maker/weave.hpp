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
#include "maker/runtime.hpp"
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

/// The interpreted form mounts and spends bodies in the host's live catalog.
// MW-WEAVE-01 -- agents/maker/weave.md
class Weave final : public Runtime {
public:
    Weave(op::Catalog& catalog, Definition definition)
        : Runtime(std::move(definition)), catalog_(&catalog) {}

    ~Weave() override { unmount(); }

    op::MountReport mount() {
        op::MountReport report = catalog_->mount(definition_.provider(), definitions_of(definition_));
        if (report) mounted_provider_ = definition_.provider();
        return report;
    }

    void unmount() {
        if (!mounted_provider_.empty()) {
            catalog_->unmount(mounted_provider_);
            mounted_provider_.clear();
        }
    }

    void adopt_definition(Definition next) {
        unmount();
        definition_ = std::move(next);
        mounted_provider_ = definition_.provider();
    }

private:
    op::Evaluation evaluate_body(const On& trigger, const loom::Value& message) override {
        const std::string identity = definition_.trigger_identity(trigger);
        const op::OperatorDef* def = catalog_->find(identity);
        if (def == nullptr) {
            return op::Evaluation::refuse("unresolved trigger `" + identity +
                                         "`: its body is not mounted");
        }
        return catalog_->evaluate(identity, pack(state(), message, def->inputs()));
    }
    op::Catalog* catalog_;
    std::string mounted_provider_;
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
    auto same_schemas = [](const auto& before, const auto& after) {
        if (before.size() != after.size()) return false;
        for (const auto& shape : before) {
            bool found = false;
            for (const auto& candidate : after) {
                if (loom::same_identity(*shape, *candidate)) found = true;
            }
            if (!found) return false;
        }
        // Admission permits repeated declarations. Check the reverse inclusion too,
        // so [A, A] cannot be edited to [A, B] while keeping a stale registered surface.
        for (const auto& shape : after) {
            bool found = false;
            for (const auto& candidate : before) {
                if (loom::same_identity(*shape, *candidate)) found = true;
            }
            if (!found) return false;
        }
        return true;
    };
    if (!same_schemas(current.accepts, next.accepts) || !same_schemas(current.emits, next.emits)) {
        return Edited::no("a behaviour edit keeps accepted and emitted schema contracts; "
                          "changing the registered surface needs replacement");
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
