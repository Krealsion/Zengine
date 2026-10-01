// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_ARRANGEMENT_HPP
#define ZENGINE_WORKSHOP_ARRANGEMENT_HPP

// The host's read-only observation door (docs/reference/introspection.md): a derivation, pure over
// the live realization owner it reads, and the weave that answers an office that asks. It keeps no
// store, publishes nothing, and cannot mount,
// unmount, overlay, evaluate, load, unload, reload or replace anything. Which powers resolve
// is the discovery door's (workshop/powers_door.hpp).

#include "arrangement_vocabulary.hpp"
#include "load_execute.hpp"
#include "load_persist.hpp" // `mode_word` -- ONE spelling of `normal`/`overlay`
#include "load_plan.hpp"

#include "operator/host_surface.hpp" // `op::OfferOutcome`, the handoff a row reports

#include <zen/weave.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

// ---- Words for two enumerations ------------------------------------------------

/// The token for one operator-handoff outcome, total. It lives here rather than beside
/// `op::OfferOutcome`: the word belongs to the thing that spells it.
inline const char* offer_token(op::OfferOutcome outcome) {
    switch (outcome) {
    case op::OfferOutcome::Offered: return kOfferedToken;
    case op::OfferOutcome::VersionMismatch: return kVersionMismatchToken;
    case op::OfferOutcome::NotOpened: return kNotOpenedToken;
    case op::OfferOutcome::NotAConsumer: break;
    }
    return kNotAConsumerToken;
}

/// The token for where realization has got with one authored row, total; here for
/// `offer_token`'s reason.
inline const char* state_token(load::RowState state) {
    switch (state) {
    case load::RowState::Pending: return kPendingToken;
    case load::RowState::Loading: return kLoadingToken;
    case load::RowState::Resolved: return kResolvedToken;
    case load::RowState::Refused: return kRefusedToken;
    case load::RowState::Reloading: return kReloadingToken;
    case load::RowState::Switched: return kSwitchedToken;
    case load::RowState::Unavailable: return kUnavailableToken;
    case load::RowState::Authored: break;
    }
    return kAuthoredToken;
}

// ---- The arrangement, derived --------------------------------------------------

/// WHAT A WEAVER CAN DO ABOUT A ROW THAT IS NOT RUNNING, said by the owner that knows why.
/// Empty for a row that needs nothing, or whose next step no one here can name.
inline std::string next_action_of(load::RowState state, const std::string& stem) {
    switch (state) {
    case load::RowState::Unavailable:
        return "make '" + stem + "' available (build it), then relaunch Workshop -- this run "
               "stepped over it and will not return to it";
    case load::RowState::Refused:
        return "fix what the refusal names, then relaunch -- the project stopped at this row";
    case load::RowState::Pending:
        return "build it; its authored participation is performed then";
    default: return std::string();
    }
}

/// Pair every authored row with what realization made of it, by stem (a key: `check_plan` refuses
/// a repeat), in authored order. Both halves come from the one owner: the mount mode exists only
/// in the plan and the resolved identity only in its rows, so neither is rebuilt from the other.
inline v2::ResolvedArrangement describe_resolved(const load::PlanExecutor& realization,
                                                 std::string plan) {
    const load::LoadPlan& authored = realization.plan();
    v2::ResolvedArrangement out;
    out.plan = std::move(plan);
    out.artifacts.reserve(authored.artifacts.size());
    for (const load::ArtifactIntent& intent : authored.artifacts) {
        v3::ArtifactParticipation row;
        row.artifact = intent.stem;
        row.optional = intent.optional;
        // `mode_word` is the plan file's own function: what a weaver wrote is what they read.
        if (intent.provider.has_value()) {
            row.authored_provider = load_persist::mode_word(intent.provider->mode);
        }
        if (intent.weave.has_value()) {
            row.authored_role = intent.weave->role;
        }
        // Where realization has got with it, asked even when nothing resolved.
        const load::RowState state = realization.state_of(intent.stem);
        row.state = state_token(state);
        // The reason and the next step are the owner's, asked rather than parsed out of a banner.
        row.reason = realization.reason_of(intent.stem);
        row.next = next_action_of(state, intent.stem);
        if (row.state != kResolvedToken && row.state != kReloadingToken) {
            // Only a settled row carries resolved fields; a reloading row is settled and live.
            out.artifacts.push_back(std::move(row));
            continue;
        }
        for (const load::ResolvedArtifact& done : realization.resolved()) {
            if (done.stem != intent.stem) {
                continue;
            }
            row.provider = done.provider;
            row.powers = static_cast<std::int64_t>(done.contributed);
            row.weave = done.weave_loaded ? static_cast<std::int64_t>(done.weave.value) : 0;
            // A provider-only row had no offer made: its `NotAConsumer` is a default, not seen.
            row.offer = done.weave_loaded ? offer_token(done.offer) : kOfferNone;
            break;
        }
        out.artifacts.push_back(std::move(row));
    }
    // THEN THE CHOICES THAT RAN WITHOUT BEING ARTIFACT ROWS, in authored order: an office a
    // switch moved is held by one of these, and it is authored intent as much as a row is.
    for (const load::ChoiceIntent& choice : authored.choices) {
        bool is_row = false;
        for (const load::ArtifactIntent& intent : authored.artifacts) {
            is_row = is_row || intent.stem == choice.stem;
        }
        if (is_row) {
            continue;
        }
        for (const load::ResolvedArtifact& done : realization.resolved()) {
            if (done.stem != choice.stem) {
                continue;
            }
            v3::ArtifactParticipation row;
            row.artifact = choice.stem;
            row.authored_role = choice.role;
            row.state = state_token(realization.state_of(choice.stem));
            row.weave = done.weave_loaded ? static_cast<std::int64_t>(done.weave.value) : 0;
            row.offer = done.weave_loaded ? offer_token(done.offer) : kOfferNone;
            out.artifacts.push_back(std::move(row));
            break;
        }
    }
    return out;
}

/// The same answer in version 1's words, derived from the one derivation above. Version 1 has no
/// `unavailable`, so such a row is said `refused`, the nearest true word it has.
inline ResolvedArrangement in_version_one(const v2::ResolvedArrangement& said) {
    ResolvedArrangement out;
    out.plan = said.plan;
    out.artifacts.reserve(said.artifacts.size());
    for (const v3::ArtifactParticipation& a : said.artifacts) {
        ArtifactParticipation row;
        row.artifact = a.artifact;
        row.authored_provider = a.authored_provider;
        row.authored_role = a.authored_role;
        row.state = a.state == kUnavailableToken ? std::string(kRefusedToken) : a.state;
        row.provider = a.provider;
        row.powers = a.powers;
        row.weave = a.weave;
        row.offer = a.offer;
        out.artifacts.push_back(std::move(row));
    }
    return out;
}

inline ResolvedArrangement describe_arrangement(const load::PlanExecutor& realization,
                                                std::string plan) {
    return in_version_one(describe_resolved(realization, std::move(plan)));
}

// ---- The door -------------------------------------------------------------------

/// What this door has done: counters only. No answer is kept between asks.
struct ArrangementDoorState {
    std::int64_t arrangements = 0;
    std::int64_t refused = 0; ///< asks that were not authored as any office
    ZEN_EXPOSE();
    ZEN_SHAPE(ArrangementDoorState, 2, ZEN_FIELD(arrangements), ZEN_FIELD(refused));
};

/// The host's read-only observation participant. It holds a `const` reference to the host's
/// realization owner and a copy of the plan's path, owning neither: a copy would be a mirror, a
/// non-const reference a controller. It reads a live owner, so it answers mid-realization. Only an
/// office may ask; that rule names nobody and is not containment.
class ArrangementDoor
    : public loom::WeaveBase<ArrangementDoor, ArrangementDoorState,
                             loom::Accept<ArrangementRequested>,
                             loom::Emit<ResolvedArrangement, v2::ResolvedArrangement>> {
public:
    /// DOES THE OFFICE THAT ASKED ACCEPT THIS SHAPE NOW? The host's `holder_accepts_on`, wired
    /// by whoever mounts the door. Empty answers no, and every answer crosses in version 1.
    using Accepts = std::function<bool(std::string_view role, const loom::Schema& shape)>;

    ArrangementDoor(const load::PlanExecutor& realization, std::string plan,
                    Accepts accepts = Accepts())
        : realization_(&realization), plan_(std::move(plan)), accepts_(std::move(accepts)) {}

    /// What the project asked for and where it has got to, derived now from the live owner.
    void on(const ArrangementRequested&, loom::Mail& mail) {
        if (!answerable(mail)) {
            return;
        }
        ++state_.arrangements;
        // Answered, not sent: the recipient and correlation are Loom's. One ask, one answer, in
        // the version the asking office's holder reads.
        v2::ResolvedArrangement said = describe_resolved(*realization_, plan_);
        if (accepts_ &&
            accepts_(mail.authored_role(), *loom::schema_of<v2::ResolvedArrangement>())) {
            (void)mail.answer(said);
            return;
        }
        (void)mail.answer(in_version_one(said));
    }

private:
    bool answerable(loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            ++state_.refused;
            return false; // personal speech, or a root: nobody to be answerable to
        }
        return true;
    }

    const load::PlanExecutor* realization_;
    /// The file the host read: provenance, not identity.
    std::string plan_;
    Accepts accepts_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_ARRANGEMENT_HPP
