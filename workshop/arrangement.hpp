// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_ARRANGEMENT_HPP
#define ZENGINE_WORKSHOP_ARRANGEMENT_HPP

// THE HOST'S READ-ONLY OBSERVATION DOOR (INTR-1) -- two derivations and one weave.
//
//     describe_arrangement   the authored plan + the executor's resolved rows
//                            -> `ResolvedArrangement`
//     describe_powers        the live `op::Catalog`
//                            -> `ResolvedPowers`
//     ArrangementDoor        when to answer, and whom
//
// The two functions are pure over the owners they read, exactly as
// `introspection/loaded.hpp` is pure over a Manager's answer: what a projection MEANS
// is provable over a value, and only the weave beside them needs a bus.
//
// ---- IT DERIVES; IT DOES NOT REMEMBER ------------------------------------------
//
// THERE IS NO STORE IN THIS FILE. No map, no cache, no mirror, no registry, no
// snapshot kept between asks, and nothing that has to be updated when something
// mounts, loads, shadows or unloads. Every field of every answer is read out of its
// owner at the moment the question is asked:
//
//     the arrangement   `load::LoadPlan` (authored)  +  `load::PlanExecutor`'s rows
//     the powers        `op::Catalog`'s contribution stacks
//
// That is the load-bearing decision of the phase and it is what makes an overlay
// mounted after the last ask show up in the next one with nothing having notified
// anybody. A second store would agree with the first for exactly as long as nothing
// changed -- which is to say, until the moment it mattered.
//
// ---- WHY A WEAVE, AND NOT A HEADER THE TOOL INCLUDES ---------------------------
//
// Because the tool is in ANOTHER IMAGE. `zengine-introspection` is loaded by the
// Kernel at run time; the catalog and the executor are locals of the host's `main`.
// Nothing but a value may cross that line -- not a pointer, not a reference, not a
// container, and not a callable that closes over one -- so the seam is the one Loom
// already has for exactly this: ask a role a question, hear the answer.
//
// It is the SAME SEAM the Loaded pane already spends. `zen.ListLoaded` goes to the
// Weave Manager, which is an in-process weave holding kernel reach, and comes back as
// a value. This door is that arrangement for two facts the Manager does not own, and
// it is deliberately not a second mechanism.
//
// ---- WHAT IT CANNOT DO ---------------------------------------------------------
//
// It answers two shapes and says nothing else, ever. It cannot mount, unmount,
// overlay, evaluate, load, unload, reload or replace anything; it holds its two
// stores as `const` references and its own header has no verb that would let it try.
// It publishes nothing -- every answer goes to the one weave that asked, through
// Loom's answer door -- so these facts do not become ambient knowledge for
// participants that never asked, and a weave that never asks never sees them.
//
// AND IT IS NOT A SERVICE LOCATOR. It hands out no capability, no handle, no index
// and no address. What it gives an asker is a picture; what it gives an asker no way
// to do is anything at all.

#include "arrangement_vocabulary.hpp"
#include "load_execute.hpp"
#include "load_persist.hpp" // `mode_word` -- ONE spelling of `normal`/`overlay`
#include "load_plan.hpp"

#include "operator/catalog.hpp"
#include "operator/host_surface.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

// ---- Words for two enumerations ------------------------------------------------

/// THE TOKEN FOR ONE OPERATOR-HANDOFF OUTCOME. TOTAL over the enumeration.
///
/// It lives here rather than beside `op::OfferOutcome` for `load_persist::mode_word`'s
/// reason: the word belongs to the thing that SPELLS it, not to the thing that means
/// it. The operator package has no opinion about how a host describes a handoff, and
/// giving it one would make every consumer of an enum carry somebody's prose.
inline const char* offer_token(op::OfferOutcome outcome) {
    switch (outcome) {
    case op::OfferOutcome::Offered: return kOfferedToken;
    case op::OfferOutcome::VersionMismatch: return kVersionMismatchToken;
    case op::OfferOutcome::NotOpened: return kNotOpenedToken;
    case op::OfferOutcome::NotAConsumer: break;
    }
    return kNotAConsumerToken;
}

// ---- The arrangement, derived --------------------------------------------------

/// PAIR EVERY AUTHORED ROW WITH WHAT THE EXECUTOR MADE OF IT.
///
/// TWO OWNERS, PAIRED BY STEM, AND NEITHER IS RECONSTRUCTED FROM THE OTHER. The
/// authored half exists ONLY in the plan -- `load::ResolvedArtifact` retains no mount
/// MODE at all, and a resolved row cannot say whether its mount was an overlay. The
/// resolved half exists ONLY in the executor's rows -- a provider identity is what an
/// artifact declared about ITSELF and a `WeaveId` is what this Kernel minted, neither
/// of which is anywhere in the file. So this walks the authored list and asks the
/// resolved list about each stem; it does not walk the Kernel's loaded map and the
/// catalog's provider list and guess which of them came from the same artifact.
///
/// THE STEM IS A KEY BECAUSE `check_plan` MADE IT ONE. A plan naming one artifact
/// twice is refused before it is executed, so the first match is the only match and
/// the pairing is exact rather than best-effort.
///
/// AUTHORED ORDER, ALWAYS. The plan is walked, not the resolved rows, so a row that
/// did not resolve keeps its place and its authored intent instead of vanishing --
/// which is what makes `performed` legible as a count against the list's own length.
inline ResolvedArrangement
describe_arrangement(const load::LoadPlan& authored,
                     const std::vector<load::ResolvedArtifact>& resolved, std::string plan) {
    ResolvedArrangement out;
    out.plan = std::move(plan);
    out.artifacts.reserve(authored.artifacts.size());
    for (const load::ArtifactIntent& intent : authored.artifacts) {
        ArtifactParticipation row;
        row.artifact = intent.stem;
        // ---- what a person wrote --------------------------------------------
        //
        // `mode_word` IS THE FILE'S OWN FUNCTION, deliberately: a maker who wrote
        // `overlay` in their plan must read `overlay` in their pane, and a second
        // spelling of that word here is how one of the two comes to say something
        // else. An absent surface is the empty string, so a reader tests presence
        // rather than parsing a sentence.
        if (intent.provider.has_value()) {
            row.authored_provider = load_persist::mode_word(intent.provider->mode);
        }
        if (intent.weave.has_value()) {
            row.authored_role = intent.weave->role;
        }
        // ---- and what this run made of it ------------------------------------
        for (const load::ResolvedArtifact& done : resolved) {
            if (done.stem != intent.stem) {
                continue;
            }
            row.performed = true;
            row.provider = done.provider;
            row.powers = static_cast<std::int64_t>(done.contributed);
            row.weave = done.weave_loaded ? static_cast<std::int64_t>(done.weave.value) : 0;
            // THE OFFER IS A FACT ABOUT A WEAVE LOAD AND ABOUT NOTHING ELSE. A record
            // with no weave intent had no offer made around it, and `ResolvedArtifact`
            // carries `NotAConsumer` there because that is its default rather than
            // because anything was observed -- so reporting the token for a
            // provider-only row would be publishing a default as an observation.
            row.offer = done.weave_loaded ? offer_token(done.offer) : kOfferNone;
            break;
        }
        out.artifacts.push_back(std::move(row));
    }
    return out;
}

// ---- The powers, derived --------------------------------------------------------

/// READ EVERY IDENTITY'S WHOLE CONTRIBUTION STACK OFF THE ONE STORE.
///
/// `identities()` AND `contributions()` WALK THE SAME MAP `find()` RESOLVES THROUGH,
/// which is what makes this projection and the evaluation a maker's Timer actually
/// spends one truth rather than two that agree. Nothing here keeps a provider map, and
/// nothing here calls `find` and then looks the stack up somewhere else: the last
/// element of `contributions(id)` IS what `find(id)` answers, by construction, because
/// `find` returns `stack.back()`.
///
/// NOTHING IS EVALUATED. A power's meaning is not asked for, no arguments are made up,
/// and no operator is run -- the question is who currently satisfies each identity,
/// and running one to find out would be a side effect in a view.
///
/// THE ORDER IS THE CATALOG'S, TWICE OVER: identities are name-ordered because the
/// store is a map, and each stack is oldest-first because that is how contributions
/// were pushed. Neither is sorted here; a view that reordered a stack would be
/// reordering the answer to "which one is active".
inline ResolvedPowers describe_powers(const op::Catalog& catalog) {
    ResolvedPowers out;
    out.providers = catalog.providers();
    const std::vector<std::string> identities = catalog.identities();
    out.powers.reserve(identities.size());
    for (const std::string& identity : identities) {
        PowerStack stack;
        stack.power = identity;
        for (const op::Contribution& c : catalog.contributions(identity)) {
            PowerContribution said;
            said.provider = c.provider;
            // `definition` is never null for a contribution the catalog holds -- both
            // `publish` and `mount` build it with `make_shared` -- and it is tested
            // anyway, because a view that dereferenced on a promise would be a view
            // whose correctness lives in another file.
            said.composite = c.definition != nullptr && c.definition->is_composite();
            stack.contributions.push_back(std::move(said));
        }
        out.powers.push_back(std::move(stack));
    }
    return out;
}

// ---- The door -------------------------------------------------------------------

/// WHAT THIS DOOR HAS DONE, and it is all counters.
///
/// NO ANSWER IS IN HERE, and that is the whole point of the file: an answer kept
/// between asks would be the mirror this phase exists not to build. What survives a
/// snapshot is the bookkeeping -- how many times each question was answered, and how
/// many asks were refused for arriving without an office.
struct ArrangementDoorState {
    std::int64_t arrangements = 0;
    std::int64_t powers = 0;
    std::int64_t refused = 0; ///< asks that were not authored as any office
    ZEN_EXPOSE();
    ZEN_SHAPE(ArrangementDoorState, 1, ZEN_FIELD(arrangements), ZEN_FIELD(powers),
              ZEN_FIELD(refused));
};

/// THE HOST'S OWN READ-ONLY OBSERVATION PARTICIPANT.
///
/// It holds three `const` references and one string, and every one of them belongs to
/// the host's `main`: the authored plan it read, the executor that performed it, the
/// catalog the plan mounted into, and the path the plan came from. IT OWNS NONE OF
/// THEM. A door that took copies would be a mirror; a door that took non-const
/// references would be a controller.
///
/// ---- WHO MAY ASK ---------------------------------------------------------------
///
/// AN OFFICE, AND ONLY AN OFFICE. `mail.authored_from_role` is empty for personal
/// speech, so an ask that is not deliberately authored as some office is refused and
/// counted. That is a small rule and it is spelled out rather than oversold:
///
///   IT NAMES NOBODY. There is no allow-list here, no `zengine.introspection`, and no
///     identity of any kind -- the rule is "say who you are", not "be somebody I know".
///     A tool added tomorrow asks the same way with no edit here.
///   IT IS NOT CONTAINMENT. `Kernel::load` binds `allow_any()` to every library it
///     opens and a loaded weave is bound to an office by the plan, so any dynamic
///     weave in this process could satisfy it. What it excludes is anonymous speech
///     and a root send; what it buys is that every answer this door gives went to a
///     named office, which is a fact a report can state.
///   THE FACTS DO NOT LEAK ANYWAY. Nothing here publishes. `mail.answer` sends to
///     exactly the weave Loom recorded as the asker, once per ask, so a participant
///     that never asks is never told -- which is the property that actually keeps
///     provider identities and power overlays from becoming ambient.
class ArrangementDoor
    : public loom::WeaveBase<ArrangementDoor, ArrangementDoorState,
                             loom::Accept<ArrangementRequested, PowersRequested>,
                             loom::Emit<ResolvedArrangement, ResolvedPowers>> {
public:
    ArrangementDoor(const load::LoadPlan& authored, const load::PlanExecutor& executed,
                    const op::Catalog& catalog, std::string plan)
        : authored_(&authored), executed_(&executed), catalog_(&catalog),
          plan_(std::move(plan)) {}

    /// WHAT THE PROJECT ASKED FOR AND WHAT CAME OF IT -- derived now, from the two
    /// owners, and dropped the moment it has been said.
    void on(const ArrangementRequested&, loom::Mail& mail) {
        if (!answerable(mail)) {
            return;
        }
        ++state_.arrangements;
        // ANSWERED, NOT SENT. `mail.answer` is Loom's own door: the recipient and the
        // correlation are the bus's, not this weave's, so the answer cannot be aimed
        // elsewhere or relabelled, and the asker reads `answers_ask()` off provenance
        // no payload can write. One ask, one answer.
        (void)mail.answer(describe_arrangement(*authored_, executed_->resolved(), plan_));
    }

    /// WHICH POWERS RESOLVE HERE AND WHOSE CODE SATISFIES EACH -- read off the live
    /// catalog at the moment of the ask, which is why an overlay mounted since the
    /// last answer is in this one.
    void on(const PowersRequested&, loom::Mail& mail) {
        if (!answerable(mail)) {
            return;
        }
        ++state_.powers;
        (void)mail.answer(describe_powers(*catalog_));
    }

private:
    /// ONE RULE, IN ONE PLACE, so the two questions cannot come to disagree about who
    /// may ask them.
    bool answerable(loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            ++state_.refused;
            return false; // personal speech, or a root: nobody to be answerable to
        }
        return true;
    }

    const load::LoadPlan* authored_;
    const load::PlanExecutor* executed_;
    const op::Catalog* catalog_;
    /// THE FILE THE HOST READ, copied once at construction because it is a string the
    /// host owns and never changes. It is provenance and not identity: the arrangement
    /// is the authored rows (arrangement_vocabulary.hpp).
    std::string plan_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_ARRANGEMENT_HPP
