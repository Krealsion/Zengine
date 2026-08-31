// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_MIGRATION_HPP
#define ZENGINE_OPERATOR_MIGRATION_HPP

// YESTERDAY'S BYTES, THROUGH TODAY'S CATALOG (MIG-0) — the convention that makes one
// operator a CONVERSION EDGE, and the one lookup that spends it.
//
// A durable file written by an older build claims a schema this build does not admit.
// Somebody has to translate it, and the question this file answers is WHO, and by whose
// permission. The answer needs no new mechanism at all:
//
//     A MIGRATION IS AN ORDINARY PROVIDER-CONTRIBUTED OPERATOR WHOSE SIGNATURE IS
//     THE EDGE IT CONVERTS.
//
// Not a weave, not a role, not a second ABI, not a plugin species, not a callback the
// persistence owner holds. An operator — mounted, layered, replaced, unmounted and spent
// under exactly the law every other operator lives under, in the ONE catalog
// (`operator/catalog.hpp`). This header adds a naming convention, a shape predicate and a
// projection. It adds no store.
//
// ---- THE SHAPE OF AN EDGE, and why each half is forced ----------------------
//
//     input schema      IS the historical message schema itself -- `WorkshopSession` v1,
//                       name, version and content id -- because `loom::admit(Unverified,
//                       door)` asks the claim to name the door. A durable file's bytes can
//                       only be admitted at a door whose identity they already claim, so
//                       the edge's input port list IS yesterday's shape. That is what lets
//                       `Catalog::evaluate(identity, loom::Unverified)` take the file's own
//                       bytes with nobody having decoded them first.
//
//     output schema     is a ONE-PORT answer whose single field is a Message of the target
//                       schema, because `Catalog::run` writes an answer into
//                       `outputs()->fields()[0]` -- an operator's output schema is a port
//                       list everywhere in this package, and a migration is not an
//                       exception to that. So the TARGET identity is the port's message
//                       identity, not the answer schema's own.
//
// The edge is therefore readable off the definition with no metadata beside it, and
// `declares_migration` is that reading. Nothing derives it from a name, a registration
// door, a flag or an implementation technique -- exactly the argument `op::is_source`
// makes one file over.
//
// ---- ...AND THE IDENTITY IS DERIVED FROM THE EDGE ---------------------------
//
//     zengine.migrate.<family>.v<from>-to-v<to>
//
// DETERMINISTIC ON PURPOSE, and it buys two things a scan could not. Two providers
// describing the same logical edge COLLIDE AT MOUNT -- where a maker can see it, in the
// catalog's own words -- rather than becoming an ambiguity somebody meets at spend. And a
// consumer's lookup is one `find()` rather than a walk over the store, so there is no
// "closest", no "newest", no "shortest" and no place for a policy to grow.
//
// THE NAME IS DIAGNOSTIC; THE SIGNATURE IS THE PROOF. `migrate` verifies that the
// definition it found actually declares the edge its name claims, and refuses one that
// does not -- so a contribution whose identity says `v1-to-v3` while its schemas say
// something else is named and not spent. `make_migration` derives the name FROM the
// schemas, so an honest provider cannot get the pair out of step by hand.
//
// ---- WHAT THIS FILE DELIBERATELY IS NOT ------------------------------------
//
// No second registry: a migration is discovered by asking the one catalog what currently
// satisfies an identity, which is the same sentence `find`, `evaluate`, a composite's own
// nodes and a loaded consumer all read.
//
// No route search. ONE SPEND IS ONE AUTHORED EDGE. Two mounted edges that happen to meet
// in the middle are not a third edge, and this file will not compose them: a searched
// multi-hop is a result no participant authored. A chain that is wanted is a chain
// somebody AUTHORS -- an ordinary composite whose exterior contract is the direct edge the
// consumer needs -- and it is then found by the same one lookup as any other.
//
// No loading. A version claim is a LOOKUP KEY and never authority: nothing here opens a
// file, realizes a plan row, scans a directory or mounts anything. If the edge is not
// already live, this answers with a refusal that names what is missing, and old bytes get
// nothing more powerful than that.
//
// No custody. The durable artifact stays the owner's: this takes an `Unverified` the owner
// parsed under the owner's own size law, hands back a candidate value, and touches no file.
// What the owner does next -- its own current-shape admission, its own semantic law, its
// own installation -- is unchanged and is still the owner's.
//
// No caching. The definition is resolved at the spend, exactly as `op::sample` resolves
// one: unmount the provider and the edge is simply gone; cover it lawfully and the next
// spend follows the catalog's current truth. There is no held callable, no contribution
// index and no migration handle for anything to go stale in.

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

/// The one port a conversion answers on.
///
/// Deterministic like the identity, and for the same reason: the wrapper's own name and
/// port are part of what two builds of the same edge must agree on before their content
/// ids can agree. It is not a semantic word -- what the answer MEANS is the target schema,
/// which is the port's type.
inline constexpr const char* kMigrationPort = "value";

/// The prefix every conventional conversion identity carries.
inline constexpr const char* kMigrationPrefix = "zengine.migrate.";

/// THE CONVENTIONAL IDENTITY FOR ONE EDGE, from the two things that define it.
///
/// `family` is the schema name both sides share -- a conversion within one durable shape's
/// history, which is the only kind this convention spells. A translation between two
/// DIFFERENT shapes is an ordinary operator and wants an ordinary authored name; it is not
/// this seam's business and is deliberately unsayable here.
inline std::string migration_identity(std::string_view family, std::uint32_t from,
                                      std::uint32_t to) {
    return std::string(kMigrationPrefix) + std::string(family) + ".v" + std::to_string(from) +
           "-to-v" + std::to_string(to);
}

/// The same, read off the two schemas an edge actually joins.
inline std::string migration_identity(const loom::Schema& from, const loom::Schema& to) {
    return migration_identity(from.name(), from.version(), to.version());
}

/// WHAT SHAPE DOES THIS DEFINITION CONVERT TO, or nullptr if it converts nothing.
///
/// The answer's single port must carry a Message, and that message's schema is the target.
/// An operator whose answer is an Int, a list, or two ports is not a conversion and is not
/// being judged as a broken one -- it is an ordinary operator, which is what almost every
/// definition in a catalog is.
inline const loom::Schema* migration_target(const OperatorDef& def) noexcept {
    const std::vector<loom::Field>& ports = def.outputs()->fields();
    if (ports.size() != 1 || ports[0].type.kind != loom::Kind::Message) {
        return nullptr;
    }
    return ports[0].type.message.get();
}

/// DOES THIS DEFINITION DECLARE A CONVERSION EDGE? Read off its own two schemas, with no
/// metadata beside them and no name involved.
///
/// One family, two identities: the same durable shape said twice, differently. `math.max`
/// cannot match it (its ports are `math.max.in`/`math.max.out`, two different names), and
/// neither can an operator whose answer happens to be a message of its own input -- that
/// would be the same identity on both sides, which is not an edge but a copy.
inline bool declares_migration(const OperatorDef& def) noexcept {
    const loom::Schema* to = migration_target(def);
    return to != nullptr && def.inputs()->name() == to->name() &&
           !loom::same_identity(*def.inputs(), *to);
}

/// THE ANSWER SCHEMA FOR ONE EDGE -- the one-port wrapper the evaluator writes into.
///
/// `identity + ".out"` is `make_operator`'s own spelling, kept because a reader who knows
/// what an operator's ports look like should not have to learn a second convention to read
/// a conversion's.
inline std::shared_ptr<const loom::Schema>
migration_answer_schema(const std::string& identity, std::shared_ptr<const loom::Schema> to) {
    return loom::make_schema(
        identity + ".out", 1,
        std::vector<loom::Field>{
            loom::Field{std::string(kMigrationPort), loom::type_message(std::move(to)),
                        /*required=*/true}});
}

/// AUTHOR ONE CONVERSION EDGE, with the convention applied rather than remembered.
///
/// A provider hands over the two schemas and a body; the identity and the answer wrapper
/// are derived here, so an honest contribution cannot end up with a name that says one edge
/// while its signature says another. (A contribution that WANTS to say two different things
/// can still be hand-built -- `OperatorDef` takes any pair of schemas -- and `migrate`
/// refuses it on the signature, which is where a hostile artifact has to be caught anyway.)
///
/// `body` receives the historical value, admitted at `from` by the one gate, and answers
/// with a Message cell of the target shape. Throwing out of it is how a conversion refuses:
/// `Catalog::run` contains the throw and turns it into this evaluation's reason, which is
/// what carries yesterday's own vocabulary to a maker.
inline OperatorDef make_migration(std::shared_ptr<const loom::Schema> from,
                                  std::shared_ptr<const loom::Schema> to,
                                  OperatorDef::Native body) {
    const std::string identity = migration_identity(*from, *to);
    auto answer = migration_answer_schema(identity, std::move(to));
    return OperatorDef(identity, std::move(from), std::move(answer), std::move(body));
}

/// THE CONVERTED VALUE INSIDE A SUCCESSFUL ANSWER.
///
/// Precondition: `answer` came back accepted from `migrate`. The evaluator has already put
/// it through the output gate at the edge's declared target, so what comes out here is an
/// admitted value of the target shape and needs no second admission to be READ -- the
/// owner's own current-shape law is a different question and is still owed.
inline const loom::Value& migrated(const Evaluation& answer) {
    return *answer.value().at(0)->as_message();
}

namespace detail {

/// The one sentence for "no such conversion is live", said in one place because the two
/// ways to arrive at it -- no catalog at all, and a catalog without this edge -- are the
/// same fact to whoever asked.
///
/// IT NAMES THE MISSING POWER AND CLAIMS NOTHING ELSE. Not which artifact would supply it
/// (nothing here knows), not that one exists on disk (no unloaded discovery exists), and
/// not that anybody should install anything. What a host CAN truthfully say is which
/// identity is unresolved, and that is a name a maker can look for in the powers this run
/// actually has.
inline std::string no_edge(std::string_view family, std::uint32_t from, std::uint32_t to) {
    return "no live conversion from `" + std::string(family) + "` v" + std::to_string(from) +
           " to v" + std::to_string(to) + " (`" + migration_identity(family, from, to) + "`)";
}

} // namespace detail

/// CONVERT ONE HISTORICAL CANDIDATE TO THE SHAPE ITS OWNER CURRENTLY ADMITS.
///
/// Given bytes that claim an older version of `target`'s own shape, is there exactly one
/// currently-authorized direct edge to `target` that can be spent right now -- and if so,
/// what does it answer?
///
/// THE FIVE THINGS IT CAN SAY, and who owns each sentence:
///
///     the bytes are not a well-formed envelope   this file, once
///     that claim is not this shape's history     this file -- a different family, or the
///                                                target's own version, is not a migration
///                                                question and must not become one
///     no such conversion is live                 this file -- the honest floor (§no_edge)
///     the definition does not declare that edge  this file -- the signature is the proof
///     anything after that                        `Catalog::evaluate`, quoted rather than
///                                                re-worded: the gate's refusal, a
///                                                provider's own words, the output gate's
///
/// WHAT IT DOES NOT DO, restated where a reader will meet it: no loading, no scanning, no
/// route search, no rewriting, no caching, and no admission of the ANSWER at anything other
/// than the edge's own declared target -- which the evaluator already did.
inline Evaluation migrate(const Catalog& conversions, const loom::Unverified& claim,
                          const std::shared_ptr<const loom::Schema>& target) {
    if (!claim.well_formed()) {
        return Evaluation::refuse("these bytes are not a Zen value, so they claim no shape "
                                  "any conversion could be chosen for");
    }
    // ---- IS THIS EVEN A MIGRATION QUESTION? (MIG-0 §3.1) ---------------------
    //
    // NARROW ON PURPOSE, and the narrowness is a safety property rather than tidiness. A
    // seam that answered "try a conversion" to any admission failure would turn every
    // corrupt file, every wrong file and every hostile file into a search for something
    // willing to eat it. Two questions, both answerable from the CLAIM alone and before
    // anything is looked up: is this the same durable shape, and is it already the one the
    // owner admits?
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
        // RESOLVED AT THE SPEND, and this is the branch that proves it: a provider that has
        // been unmounted leaves nothing behind for this lookup to find, because the lookup
        // is the store read at this instant and not a handle taken earlier.
        return Evaluation::refuse(
            detail::no_edge(target->name(), claim.claimed_version(), target->version()));
    }
    // ---- THE NAME SAID ONE EDGE; DOES THE SIGNATURE SAY THE SAME ONE? --------
    //
    // A conventional identity is how two providers COLLIDE at mount; it is not evidence
    // about what the contribution converts. So the definition that answered to that name is
    // asked to declare the edge itself, and a disagreement is named and refused rather than
    // spent -- which is the whole of what stops a contribution from claiming a conversion it
    // does not implement. The TARGET is compared by full identity (`same_identity`: name,
    // version and content id), so a provider built against a different shape of this
    // version is caught here rather than at the owner's door with a field-level complaint.
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
    // ---- THE ONE SPEND, THROUGH THE ONE DOOR --------------------------------
    //
    // `evaluate(identity, Unverified)` is the entrance a caller holding BYTES already had,
    // and it is exactly the right door for a durable artifact: the file's own bytes are
    // admitted at the edge's own input schema -- the version claim checked, the content id
    // checked, the structure checked -- and the answer is admitted at the edge's declared
    // output before this function ever sees it. Nothing here decodes anything, and nothing
    // here re-words a gate.
    return conversions.evaluate(identity, claim);
}

/// The same question asked by an owner that may have no catalog at all.
///
/// A host that mounted no conversions and a host whose catalog lacks this edge are ONE fact
/// to the file being read, and they get one sentence -- said by `no_edge`, in `no_edge`'s
/// one place. `nullptr` is what every suite fixture and every `--isolated`-style host has by
/// default, so a reader that never meets a conversion never has to know this seam exists.
inline Evaluation migrate(const Catalog* conversions, const loom::Unverified& claim,
                          const std::shared_ptr<const loom::Schema>& target) {
    if (conversions != nullptr) {
        return migrate(*conversions, claim, target);
    }
    if (!claim.well_formed() || claim.claimed_name() != target->name() ||
        claim.claimed_version() == target->version()) {
        // Not a migration question either way; answer it with the same words the catalog
        // arm would, by asking an EMPTY catalog rather than repeating three branches. A
        // LOCAL and not a static: a catalog is two empty maps, and a vague-linkage object
        // inside an inline function means different things in a host and in an image it
        // loaded (`provider.hpp` measured that), which is a hazard worth more than the
        // allocation it would save.
        const Catalog none;
        return migrate(none, claim, target);
    }
    return Evaluation::refuse(
        detail::no_edge(target->name(), claim.claimed_version(), target->version()));
}

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_MIGRATION_HPP
