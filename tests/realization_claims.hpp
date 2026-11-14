// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_REALIZATION_CLAIMS_HPP
#define ZENGINE_TESTS_REALIZATION_CLAIMS_HPP

// The realization owner's claims as a case says them, and as a host reads them: a stand-in in the
// owner's role, or the owner itself, claiming where the plan is and the authored choices
// (realization/vocabulary.hpp).

#include "doctest.h"

#include "builder/vocabulary.hpp"
#include "realization/vocabulary.hpp"
#include "workshop/panes.hpp" // `ProjectFrontier`

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct RealizationStandInState {
    std::int64_t claims = 0;
    ZEN_SHAPE(RealizationStandInState, 1, ZEN_FIELD(claims));
};

/// A STAND-IN FOR THE REALIZATION OWNER'S CLAIMS: a weave in `zengine.realization` that claims, as
/// that role, what a case says -- where the plan is, and the authored choices -- so the desk and the
/// doors that read the owner's claims read a case's facts by their own grants, as in a host.
class RealizationStandIn
    : public loom::WeaveBase<RealizationStandIn, RealizationStandInState, loom::Accept<>, loom::Emit<>,
                             loom::Claims<zengine::realization::PlanWhere,
                                          zengine::realization::ChoicesHeld>> {};

/// WHAT THE REALIZATION OWNER CLAIMS THE PROJECT WAITS ON, read off the bus as a host reads it.
inline zengine::workshop::ProjectFrontier frontier_claimed(const loom::Switchboard& bus) {
    zengine::workshop::ProjectFrontier now;
    const loom::SenseReading read = bus.observe_role(
        zengine::builder::kRealizationRole, zengine::realization::PlanWhere::zen_name,
        zengine::realization::PlanWhere::zen_version);
    if (read && read.value.has_value()) {
        const auto where = loom::from_value<zengine::realization::PlanWhere>(*read.value);
        now.artifact = where.waiting_on;
        now.waiting = !now.artifact.empty();
        now.blocked = static_cast<std::size_t>(where.behind);
    }
    return now;
}

struct RealizationClaims {
    loom::Switchboard* bus = nullptr;
    loom::WeaveId id{};

    explicit RealizationClaims(loom::Switchboard& on) : bus(&on) {
        id = on.register_weave(std::make_unique<RealizationStandIn>(), loom::Grant{},
                               std::string(zengine::builder::kRealizationRole));
    }
    /// ...or as the owner that holds the role already: a case's claim stands until the owner's
    /// next change, which claims its own.
    RealizationClaims(loom::Switchboard& on, loom::WeaveId holder) : bus(&on), id(holder) {}

    /// Claim where the plan is.
    void where(const zengine::realization::PlanWhere& w) const {
        REQUIRE(bus->role_claim_as(id, zengine::builder::kRealizationRole, loom::to_value(w))
                    .accepted);
    }
    /// Claim where the plan is as a frontier says it: waiting on `artifact` with `blocked` behind,
    /// or at rest with nothing to wait on.
    void frontier(const zengine::workshop::ProjectFrontier& f) const {
        zengine::realization::PlanWhere w;
        w.state = zengine::realization::PlanState::complete;
        if (f.waiting) {
            w.state = zengine::realization::PlanState::waiting;
            w.waiting_on = f.artifact;
            w.behind = static_cast<std::int64_t>(f.blocked);
        }
        where(w);
    }
    /// Claim the authored choices.
    void choices(const zengine::realization::ChoicesHeld& held) const {
        REQUIRE(bus->role_claim_as(id, zengine::builder::kRealizationRole, loom::to_value(held))
                    .accepted);
    }
};

#endif // ZENGINE_TESTS_REALIZATION_CLAIMS_HPP
