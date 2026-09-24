// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_OBSERVATION_HPP
#define ZENGINE_INVENTORY_OBSERVATION_HPP

// What one structure observation records, shared by Inventory's capture adapter and Info's
// per-view Sample Source. A record describes a past observation; it names who Loom attested
// answered and when this process saw it. It is never a permission or a request to replay.

#include "inventory/vocabulary.hpp"

#include <zen/weave/poke.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace zengine::inventory {

/// A fresh record of one PokeDescribe answer. `answered_by` must be the delivery's Loom-stamped
/// sender, never a value read from the answer's payload.
inline CaptureContext observed_structure(const std::string& requested_role, loom::WeaveId answered_by) {
    CaptureContext ctx;
    ctx.requested_role = requested_role;
    ctx.request_shape = loom::PokeDescribe::zen_name;
    ctx.request_version = static_cast<std::int64_t>(loom::PokeDescribe::zen_version);
    ctx.answered_by = std::to_string(answered_by.value);
    ctx.captured_at_epoch_s = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
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
