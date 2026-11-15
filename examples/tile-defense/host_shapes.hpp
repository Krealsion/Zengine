// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXAMPLES_TILE_DEFENSE_HOST_SHAPES_HPP
#define ZENGINE_EXAMPLES_TILE_DEFENSE_HOST_SHAPES_HPP

// The shapes the tile defense's host (play.cpp) and its bench (bench.cpp) keep in their own weaves:
// what each door and ear counts, and the bench's order to start what it mounts.

#include <zen/weave/shape.hpp>

#include <cstdint>

namespace zengine::tile_defense {

/// What the host's door heard.
struct TileDoorState {
    std::int64_t heard = 0;
    ZEN_SHAPE(TileDoorState, 1, ZEN_FIELD(heard));
};

/// The bench -> its door: start the weave `target` it mounted, as Loom's control door starts one.
struct BenchActivate {
    std::int64_t target = 0;
    ZEN_SHAPE(BenchActivate, 1, ZEN_FIELD(target));
};
/// What the bench's door started, and what its ears heard.
struct BenchDoorState {
    std::int64_t announced = 0;
    ZEN_SHAPE(BenchDoorState, 1, ZEN_FIELD(announced));
};
struct BenchEarsState {
    std::int64_t heard = 0;
    ZEN_SHAPE(BenchEarsState, 1, ZEN_FIELD(heard));
};

} // namespace zengine::tile_defense

#endif // ZENGINE_EXAMPLES_TILE_DEFENSE_HOST_SHAPES_HPP
