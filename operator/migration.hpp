// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_MIGRATION_HPP
#define ZENGINE_OPERATOR_MIGRATION_HPP

// Yesterday's bytes through today's catalog. A durable file written by an older build claims a
// schema this build does not admit; a migration is an ordinary provider-contributed operator
// whose signature is the edge it converts -- mounted, layered, replaced and spent under every
// other operator's law, in the one catalog. This header adds a naming convention
// (`zengine.migrate.<family>.v<from>-to-v<to>`), a shape predicate and one lookup; no store.
// Reference: docs/reference/operator-providers.md.

// Both halves of the shape are forced. The input schema is the historical message schema itself,
// since `loom::admit(Unverified, door)` asks the claim to name its door, so the file's own bytes
// enter `Catalog::evaluate(identity, loom::Unverified)` undecoded. The output is a one-port
// answer whose field is a Message of the target, since `Catalog::run` writes into
// `outputs()->fields()[0]`: the target is the port's message identity.

// The identity is derived from the edge, so two providers of one edge collide at mount, in the
// catalog's words, and a lookup is one `find()` -- no "closest", "newest" or "shortest". The
// name is diagnostic and the signature is the proof. One spend is one authored edge: no route
// search (a wanted chain is authored, as a composite), no loading (a version claim is a lookup
// key, never authority), no custody (the owner parsed the bytes and keeps the file), no cache.

#include "operator/catalog.hpp"
#include "operator/operator.hpp"

#include <zen/gate.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::op {

/// The one port a conversion answers on, deterministic like the identity: two builds of one edge
/// must agree on the wrapper before their content ids can. Not a semantic word; what the answer
/// means is the target schema, the port's type.
inline constexpr const char* kMigrationPort = "value";

/// The prefix every conventional conversion identity carries.
inline constexpr const char* kMigrationPrefix = "zengine.migrate.";

/// The conventional identity for one edge. `family` is the schema name both sides share: a
/// conversion within one durable shape's history. A translation between two different shapes is
/// an ordinary operator with an authored name, and unsayable here.
inline std::string migration_identity(std::string_view family, std::uint32_t from,
                                      std::uint32_t to) {
    return std::string(kMigrationPrefix) + std::string(family) + ".v" + std::to_string(from) +
           "-to-v" + std::to_string(to);
}

/// The same, read off the two schemas an edge actually joins.
inline std::string migration_identity(const loom::Schema& from, const loom::Schema& to) {
    return migration_identity(from.name(), from.version(), to.version());
}

/// What shape this definition converts to, or nullptr: the answer's single port must carry a
/// Message, whose schema is the target. Anything else is an ordinary operator, not a broken
/// conversion.
inline const loom::Schema* migration_target(const OperatorDef& def) noexcept {
    const std::vector<loom::Field>& ports = def.outputs()->fields();
    if (ports.size() != 1 || ports[0].type.kind != loom::Kind::Message) {
        return nullptr;
    }
    return ports[0].type.message.get();
}

/// Does this definition declare a conversion edge? Read off its two schemas, with no metadata and
/// no name: one family, two identities. `math.max` cannot match (its ports' names differ), nor
/// can an answer that is a message of its own input (one identity both sides: a copy).
inline bool declares_migration(const OperatorDef& def) noexcept {
    const loom::Schema* to = migration_target(def);
    return to != nullptr && def.inputs()->name() == to->name() &&
           !loom::same_identity(*def.inputs(), *to);
}

/// The answer schema for one edge, the one-port wrapper the evaluator writes into;
/// `identity + ".out"` is `make_operator`'s own spelling.
inline std::shared_ptr<const loom::Schema>
migration_answer_schema(const std::string& identity, std::shared_ptr<const loom::Schema> to) {
    return loom::make_schema(
        identity + ".out", 1,
        std::vector<loom::Field>{
            loom::Field{std::string(kMigrationPort), loom::type_message(std::move(to)),
                        /*required=*/true}});
}

/// Author one conversion edge with the convention applied: the identity and the answer wrapper
/// derive from the two schemas, so an honest contribution cannot name one edge and sign another
/// (a hand-built `OperatorDef` can, and `migrate` refuses it on the signature). `body` receives
/// the historical value, admitted at `from`, and answers with a Message cell of the target shape;
/// a throw is how a conversion refuses, and `Catalog::run` turns it into the evaluation's reason.
inline OperatorDef make_migration(std::shared_ptr<const loom::Schema> from,
                                  std::shared_ptr<const loom::Schema> to,
                                  OperatorDef::Native body) {
    const std::string identity = migration_identity(*from, *to);
    auto answer = migration_answer_schema(identity, std::move(to));
    return OperatorDef(identity, std::move(from), std::move(answer), std::move(body));
}

/// The converted value inside an accepted answer from `migrate`: the evaluator already admitted
/// it at the edge's declared target, so it needs no second admission to be read. The owner's own
/// current-shape law is still owed.
inline const loom::Value& migrated(const Evaluation& answer) {
    return *answer.value().at(0)->as_message();
}

namespace detail {

/// The one sentence for "no such conversion is live", for no catalog and for a catalog without
/// this edge alike. It names the missing identity and claims nothing else: not which artifact
/// would supply it, not that one exists on disk, not that anything should be installed.
inline std::string no_edge(std::string_view family, std::uint32_t from, std::uint32_t to) {
    return "no live conversion from `" + std::string(family) + "` v" + std::to_string(from) +
           " to v" + std::to_string(to) + " (`" + migration_identity(family, from, to) + "`)";
}

} // namespace detail

/// Convert one historical candidate to the shape its owner currently admits: given bytes claiming
/// an older version of `target`'s shape, spend the one live direct edge to `target`, or say why
/// not. This file says: not a well-formed envelope; not this shape's history (another family, or
/// the target's own version, is not a migration question); no such conversion is live; the
/// definition does not declare that edge. Anything after is `Catalog::evaluate`'s, quoted.
inline Evaluation migrate(const Catalog& conversions, const loom::Unverified& claim,
                          const std::shared_ptr<const loom::Schema>& target) {
    if (!claim.well_formed()) {
        return Evaluation::refuse("these bytes are not a Zen value, so they claim no shape "
                                  "any conversion could be chosen for");
    }
    // Is this a migration question at all? Narrow on purpose, as a safety property: a seam that
    // answered any admission failure with "try a conversion" would turn every corrupt, wrong or
    // hostile file into a search for something willing to eat it. Both checks read the claim
    // alone, before anything is looked up.
    if (claim.claimed_name() != target->name()) {
        return Evaluation::refuse("these bytes claim `" + claim.claimed_name() + "`, which is "
                                  "not a version of `" + target->name() + "`");
    }
    if (claim.claimed_version() == target->version()) {
        return Evaluation::refuse("these bytes already claim `" + target->name() + "` v" +
                                  std::to_string(target->version()) +
                                  ", which needs no conversion");
    }

    const std::string identity =
        migration_identity(target->name(), claim.claimed_version(), target->version());
    const OperatorDef* def = conversions.find(identity);
    if (def == nullptr) {
        // Resolved at the spend: an unmounted provider leaves nothing for this lookup to find.
        return Evaluation::refuse(
            detail::no_edge(target->name(), claim.claimed_version(), target->version()));
    }
    // The name said one edge; does the signature say the same one? A conventional identity is
    // how two providers collide at mount, not evidence of what a contribution converts, so the
    // definition is asked to declare the edge itself. The target is compared by full identity
    // (`same_identity`), so a provider built against another shape of this version is caught
    // here rather than at the owner's door.
    if (!declares_migration(*def)) {
        return Evaluation::refuse("`" + identity +
                                  "` is not a conversion: its answer is not one port "
                                  "carrying another version of `" + target->name() + "`");
    }
    if (def->inputs()->name() != target->name() ||
        def->inputs()->version() != claim.claimed_version()) {
        return Evaluation::refuse("`" + identity + "` converts `" + def->inputs()->name() +
                                  "` v" + std::to_string(def->inputs()->version()) +
                                  ", not `" + target->name() + "` v" +
                                  std::to_string(claim.claimed_version()));
    }
    if (!loom::same_identity(*migration_target(*def), *target)) {
        const loom::Schema& to = *migration_target(*def);
        return Evaluation::refuse("`" + identity + "` answers with `" + to.name() + "` v" +
                                  std::to_string(to.version()) +
                                  ", which is not the shape this reader admits");
    }
    // The one spend, through the door a caller holding bytes already had: the file's bytes are
    // admitted at the edge's input schema and the answer at its declared output before this
    // function sees it. Nothing here decodes anything or re-words a gate.
    return conversions.evaluate(identity, claim);
}

/// The same question for an owner that may have no catalog: no catalog and a catalog without
/// this edge are one fact to the file being read, and get `no_edge`'s one sentence. `nullptr` is
/// what a suite fixture or an isolated host has by default.
inline Evaluation migrate(const Catalog* conversions, const loom::Unverified& claim,
                          const std::shared_ptr<const loom::Schema>& target) {
    if (conversions != nullptr) {
        return migrate(*conversions, claim, target);
    }
    if (!claim.well_formed() || claim.claimed_name() != target->name() ||
        claim.claimed_version() == target->version()) {
        // Not a migration question either way: answered in the catalog arm's words by asking an
        // empty catalog. A local, not a static: a vague-linkage object in an inline function
        // means different things in a host and in an image it loaded.
        const Catalog none;
        return migrate(none, claim, target);
    }
    return Evaluation::refuse(
        detail::no_edge(target->name(), claim.claimed_version(), target->version()));
}

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_MIGRATION_HPP
