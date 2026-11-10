// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// THE ROLES A CASE'S LOAD NAMES: Loom binds a weave to a set of roles, and a case that loads into
// one role, or none, names it here -- an empty role is no role, never a role named "".

#ifndef ZENGINE_TESTS_ROLE_SET_HPP
#define ZENGINE_TESTS_ROLE_SET_HPP

#include <string>
#include <string_view>
#include <vector>

namespace zengine::testing {

inline std::vector<std::string> role_set(std::string_view role) {
    return role.empty() ? std::vector<std::string>{} : std::vector<std::string>{std::string(role)};
}

} // namespace zengine::testing

#endif // ZENGINE_TESTS_ROLE_SET_HPP
