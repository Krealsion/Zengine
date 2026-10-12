// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_CONTROL_PARTS_HPP
#define ZENGINE_TESTS_CONTROL_PARTS_HPP

// The judge of WL-HAND-07 (agents/workshop/pane-parts.md), read by the suites Workshop hosts
// through tests/workshop_support.hpp and by the suites that stand in for Workshop themselves.

#include <algorithm>
#include <string>
#include <vector>

/// THE ADDRESS LAW: every part a picture names `control:<x>` names an action `x` among its pane's
/// rows in force. "" when each does; else the pane, each part naming none, and the rows in force.
inline std::string control_parts_problem(const std::string& pane,
                                         const std::vector<std::string>& names,
                                         const std::vector<std::string>& rows) {
    const std::string prefix = "control:";
    std::string missing;
    for (const std::string& name : names) {
        if (name.rfind(prefix, 0) != 0) continue;
        if (std::find(rows.begin(), rows.end(), name.substr(prefix.size())) == rows.end())
            missing += " `" + name + "`";
    }
    if (missing.empty()) return std::string();
    std::string in_force;
    for (const std::string& id : rows) in_force += " " + id;
    return pane + " names" + missing + ", and its rows in force are:" +
           (in_force.empty() ? std::string(" none") : in_force);
}

#endif
