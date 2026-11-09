// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SNAKE_PLAY_STATE_HPP
#define ZENGINE_SNAKE_PLAY_STATE_HPP

// The play host's own weave state (play.cpp): how many answers its hand on the bus has heard.

#include <zen/weave/shape.hpp>

#include <cstdint>

namespace zengine::snake {

struct ConsoleState {
    std::int64_t answers = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ConsoleState, 1, ZEN_FIELD(answers));
};

} // namespace zengine::snake

#endif // ZENGINE_SNAKE_PLAY_STATE_HPP
