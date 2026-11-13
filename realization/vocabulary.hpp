// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_REALIZATION_VOCABULARY_HPP
#define ZENGINE_REALIZATION_VOCABULARY_HPP

// What the realization owner claims as its role, `zengine.realization` (builder/vocabulary.hpp):
// where the plan is, and which authored choice holds each switchable role. Latest claims, read by
// whoever the host grants observing them (`loom::Grant::allow_observe`), never by a reference to
// the owner (realization/docs/zengine.realization.md, "Follow").

#include "builder/vocabulary.hpp" // `kRealizationRole`, the role these are claimed as

#include <zen/weave.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::realization {

/// The words `PlanWhere::state` takes.
inline constexpr const char* kPlanLoading = "loading";   ///< rows are still being performed
inline constexpr const char* kPlanWaiting = "waiting";   ///< stopped at a row waiting on a build
inline constexpr const char* kPlanComplete = "complete"; ///< every authored row settled
inline constexpr const char* kPlanRefused = "refused";   ///< a row refused and the walk stopped

/// WHERE REALIZATION IS, claimed after every change to it: the plan's state, the row it waits on
/// and how many authored rows wait behind it, the optional rows stepped over, what rows could not
/// load and gave way from, and the roles a row will still load a weave into.
struct PlanWhere {
    std::string state = kPlanLoading;
    /// The row realization waits on, by its artifact; empty when it waits on none.
    std::string waiting_on;
    /// The authored rows behind `waiting_on`, waiting on it.
    std::int64_t behind = 0;
    /// The optional rows that refused and were stepped over, by artifact, in the order met.
    std::vector<std::string> unavailable;
    /// What rows could not load and gave way from, a sentence each, in the order met.
    std::vector<std::string> gave_way;
    /// The roles a plan row loading a weave into them has not settled: still to come, never
    /// unavailable.
    std::vector<std::string> to_come;
    ZEN_SHAPE(PlanWhere, 1, ZEN_FIELD(state), ZEN_FIELD(waiting_on), ZEN_FIELD(behind),
              ZEN_FIELD(unavailable), ZEN_FIELD(gave_way), ZEN_FIELD(to_come));
};

/// ONE AUTHORED CHOICE FOR A SWITCHABLE ROLE: the role, the choice's name and artifact, the image
/// a switch to it loads (its row's last, or the plan's own file), and whether it holds the role.
struct ChoiceHeld {
    std::string role;
    std::string name;
    std::string stem;
    std::string image;
    bool holds = false;
    ZEN_SHAPE(ChoiceHeld, 1, ZEN_FIELD(role), ZEN_FIELD(name), ZEN_FIELD(stem), ZEN_FIELD(image),
              ZEN_FIELD(holds));
};

/// EVERY AUTHORED CHOICE, in the plan's order, claimed after every change to which one holds a
/// role or what image it runs.
struct ChoicesHeld {
    std::vector<ChoiceHeld> choices;
    ZEN_SHAPE(ChoicesHeld, 1, ZEN_FIELD(choices));
};

/// THE REALIZATION OWNER'S LATEST CLAIM OF `Claim`, read in a delivery by the reader's grant; empty
/// when the role has claimed none yet or the reader is not granted observing it.
template <class Claim>
std::optional<Claim> latest(loom::Mail& mail) {
    const loom::SenseReading read =
        mail.latest_from_role<Claim>(zengine::builder::kRealizationRole);
    if (!read || !read.value.has_value()) {
        return std::nullopt;
    }
    return loom::from_value<Claim>(*read.value);
}

/// Is `role` still to come in this claim -- a row loading a weave into it not yet settled?
inline bool to_come(const PlanWhere& where, std::string_view role) {
    for (const std::string& r : where.to_come) {
        if (r == role) {
            return true;
        }
    }
    return false;
}

} // namespace zengine::realization

#endif // ZENGINE_REALIZATION_VOCABULARY_HPP
