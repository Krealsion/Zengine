// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SNAKE_PLAY_STATE_HPP
#define ZENGINE_SNAKE_PLAY_STATE_HPP

// The play host's own weave state (play.cpp): how many answers its hand on the bus has heard.

#include <zen/weave/shape.hpp>

#include <cstdint>

namespace zengine::snake {

struct OperatorState {
    std::int64_t answers = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(OperatorState, 1, ZEN_FIELD(answers));
};

} // namespace zengine::snake

#endif // ZENGINE_SNAKE_PLAY_STATE_HPP
