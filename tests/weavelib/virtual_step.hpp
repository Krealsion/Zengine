// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_VIRTUAL_STEP_HPP
#define ZENGINE_TESTS_VIRTUAL_STEP_HPP

// The suites' virtual beat: what one beat of a suite's virtual Timer books when nothing is due
// sooner (`timer::SteppingClock`), so a suite's virtual seconds pass in many short beats.

#include <cstdint>

namespace zengine::testing {

inline constexpr std::int64_t kVirtualStepMs = 10;

} // namespace zengine::testing

#endif // ZENGINE_TESTS_VIRTUAL_STEP_HPP
