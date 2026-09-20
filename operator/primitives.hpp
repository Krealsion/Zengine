// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_PRIMITIVES_HPP
#define ZENGINE_OPERATOR_PRIMITIVES_HPP

// Shared scalar powers earned by composed rules: Timer normalization uses max
// and select_int; Flow's thermostat additionally needs integer comparison and Bool
// selection to retain its hysteresis state. Both consumers resolve through the
// host catalog, so a provider overlay changes the composed behavior at spend.
// Native leaves receive values, not a Bus; ambient C++ effects are not sandboxed.

#include "operator/catalog.hpp"
#include "operator/operator.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace zengine::op {

/// The larger of two integers.
inline std::int64_t max_int(std::int64_t lhs, std::int64_t rhs) { return std::max(lhs, rhs); }

/// One of two integers, chosen by a condition.
inline std::int64_t select_int(bool condition, std::int64_t when_true, std::int64_t when_false) {
    return condition ? when_true : when_false;
}

/// The thermostat's hysteresis compares a reading with its two thresholds.
inline bool less_int(std::int64_t lhs, std::int64_t rhs) { return lhs < rhs; }

/// Hysteresis keeps the prior Bool when neither threshold is crossed.
inline bool select_bool(bool condition, bool when_true, bool when_false) {
    return condition ? when_true : when_false;
}

inline constexpr const char* kLessInt = "compare.less_int";
inline constexpr const char* kSelectBool = "logic.select_bool";
inline constexpr const char* kMaxInt = "math.max";
inline constexpr const char* kSelectInt = "logic.select_int";

/// The basic scalar leaves, authored once by identity and port names.
///
/// It answers with DEFINITIONS rather than filling a catalog, because since PROV-0
/// this authoring has two destinations and neither may be a copy of the other: the
/// basic provider artifact CONTRIBUTES them to a host, and a Timer with no host at
/// all assembles them locally. One authoring, two doors -- the second of which is
/// the two-line `publish_primitives` below.
inline std::vector<OperatorDef> primitive_definitions() {
    std::vector<OperatorDef> defs;
    defs.push_back(make_operator<&max_int>(kMaxInt, {"lhs", "rhs"}, "result"));
    defs.push_back(make_operator<&select_int>(kSelectInt,
                                              {"condition", "when_true", "when_false"}, "result"));
    defs.push_back(make_operator<&less_int>(kLessInt, {"lhs", "rhs"}, "result"));
    defs.push_back(make_operator<&select_bool>(kSelectBool,
        {"condition", "when_true", "when_false"}, "result"));
    return defs;
}

/// Publish the primitive leaves into a catalog -- the local-assembly door.
inline void publish_primitives(Catalog& catalog) {
    for (OperatorDef& def : primitive_definitions()) {
        catalog.publish(std::move(def));
    }
}

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_PRIMITIVES_HPP
