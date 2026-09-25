// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_ATTENTION_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_ATTENTION_SEAM_VOCABULARY_HPP

// What is true right now, said across the pane seam: the host derives every standing condition
// and publishes them for the Attention pane (WL-ATTN). Said only when it changes: a pane answers a
// publication with rows and the host repaints, so an unconditional one would never terminate. An
// action crosses as a resolved sentence (`try: <gesture> <label>`), never an id the pane could
// press. Unlike the in-host model (WL-ATTN-11), these shapes ride the ordinary bus.

#include "surface/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// One condition true right now, as a value: `Condition`'s four fields (workshop/attention.hpp),
/// plus `suggestion`.
struct StandingCondition {
    std::string key;
    std::string compact;
    std::string detail;
    std::int64_t role = surface::role::kAccent;

    /// What a maker could press, in words composed against the effective keymap, or empty. Words
    /// and not an id: the pane cannot read a keymap.
    std::string suggestion;

    ZEN_SHAPE(StandingCondition, 1, ZEN_FIELD(key), ZEN_FIELD(compact), ZEN_FIELD(detail),
              ZEN_FIELD(role), ZEN_FIELD(suggestion));
};

/// Every condition true right now, in the host's order (`ranks_before`, WL-ATTN-07), published to
/// any: which weave presents it is the load plan's business. Empty is a real answer, and it is
/// the retraction.
struct StandingConditions {
    std::vector<StandingCondition> rows;
    ZEN_SHAPE(StandingConditions, 1, ZEN_FIELD(rows));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_ATTENTION_SEAM_VOCABULARY_HPP
