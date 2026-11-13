// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_MOMENTS_HPP
#define ZENGINE_TESTS_MOMENTS_HPP

// The moments a test's hand composes from where its presses land: a pointer moment is one of
// Input's own shapes, built from the press or release it follows.

#include "input/vocabulary.hpp"

namespace moments {

namespace input = zengine::input;

/// The motion from `from` to `to`, landing where `to` does, in its space.
inline input::PointerMoved moved(const input::PointerButton& from, const input::PointerButton& to) {
    return {to.x, to.y, to.x - from.x, to.y - from.y, to.space, input::mod::kNone};
}

/// The pointer over `at`, moved there from nowhere it says.
inline input::PointerMoved over(const input::PointerButton& at) {
    return {at.x, at.y, 0, 0, at.space, input::mod::kNone};
}

/// The wheel turned `dy` notches (+1 away from the weaver) where `at` is.
inline input::PointerWheel wheel(const input::PointerButton& at, double dy) {
    return {0, dy, at.x, at.y, at.space, input::mod::kNone};
}

/// A key pressed and released, each with its modifiers.
inline input::KeyPressed key_down(std::int64_t scan, std::int64_t mods = input::mod::kNone) {
    return {scan, "", mods};
}
inline input::KeyReleased key_up(std::int64_t scan, std::int64_t mods = input::mod::kNone) {
    return {scan, "", mods};
}

} // namespace moments

#endif // ZENGINE_TESTS_MOMENTS_HPP
