// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_OBSERVATION_HPP
#define ZENGINE_INVENTORY_OBSERVATION_HPP

// What one structure observation records, shared by Inventory's capture adapter and Info's
// per-view Sample Source. A record describes a past observation; it names who Loom attested
// answered and the date of the turn the answer was delivered in. It is never a permission or a
// request to replay.

#include "inventory/vocabulary.hpp"

#include <zen/weave/poke.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zengine::inventory {

/// A fresh record of one PokeDescribe answer. `answered_by` must be the delivery's Loom-stamped
/// sender, never a value read from the answer's payload, and `turn_date_ms` the delivery's
/// `turn_date_ms()`: the record keeps that date in whole seconds, and 0 when the turn carried none.
inline CaptureContext observed_structure(const std::string& requested_role, loom::WeaveId answered_by,
                                         std::optional<std::int64_t> turn_date_ms) {
    CaptureContext ctx;
    ctx.requested_role = requested_role;
    ctx.request_shape = loom::PokeDescribe::zen_name;
    ctx.request_version = static_cast<std::int64_t>(loom::PokeDescribe::zen_version);
    ctx.answered_by = std::to_string(answered_by.value);
    ctx.captured_at_epoch_s = turn_date_ms ? *turn_date_ms / 1000 : 0;
    return ctx;
}

/// The one sampling route a stored pair can name: a recorded PokeDescribe observation of a role.
/// Any other provenance (a Terminal transcript, a stored command, no record) is not a route.
inline std::optional<CaptureContext> recorded_structure_observation(const std::vector<loom::Value>& metadata) {
    for (const auto& value : metadata) {
        if (!loom::same_identity(value.schema(), *loom::schema_of<CaptureContext>())) continue;
        auto ctx = loom::from_value<CaptureContext>(value);
        if (ctx.request_shape == loom::PokeDescribe::zen_name &&
            ctx.request_version == static_cast<std::int64_t>(loom::PokeDescribe::zen_version) &&
            !ctx.requested_role.empty())
            return ctx;
    }
    return std::nullopt;
}

} // namespace zengine::inventory

#endif // ZENGINE_INVENTORY_OBSERVATION_HPP
