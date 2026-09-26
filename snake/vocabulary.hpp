// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SNAKE_VOCABULARY_HPP
#define ZENGINE_SNAKE_VOCABULARY_HPP

// The snake package's message vocabulary, in one file to diff against the locked spellings: the
// contract's five shapes (Pos, SnakeVisual, FoodEaten, SnakeDied, SnakeWorldState, all v1),
// whose derived schemas suite `snake` pins by content id against the contract's own spelling.
// Reference: docs/reference/snake.md.

// Named additions, since the five cannot play: `SnakeTick` and `SnakeTurn` (world-owned; the
// clock and controls adapters produce them), and `SnakeWorldState` v2 (v1 plus `growths`, a real
// shape change). The world emits the visual, food and death shapes and holds the state; the
// active Skin holds the drawer's seat, accepting `SnakeVisual` alone, which stays v1 across the
// world's migration -- the drawer does not feel the world grow.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::snake {

/// Direction encoding, shared by SnakeWorldState.direction and SnakeTurn:
/// 0=up, 1=right, 2=down, 3=left (locked by the contract's comment).
inline constexpr std::int64_t kUp = 0;
inline constexpr std::int64_t kRight = 1;
inline constexpr std::int64_t kDown = 2;
inline constexpr std::int64_t kLeft = 3;

/// One grid cell. (0,0) is top-left; x grows right, y grows down.
struct Pos {
    std::int64_t x = 0;
    std::int64_t y = 0;
    ZEN_SHAPE(Pos, 1, ZEN_FIELD(x), ZEN_FIELD(y));
};

/// Everything a drawer needs, and nothing else. `snake` is head-first.
struct SnakeVisual {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::vector<Pos> snake;
    Pos food;
    bool alive = true;
    std::int64_t score = 0;
    ZEN_SHAPE(SnakeVisual, 1, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(snake),
              ZEN_FIELD(food), ZEN_FIELD(alive), ZEN_FIELD(score));
};

/// Something was eaten. An event, not a count — the count is the listener's.
struct FoodEaten {
    ZEN_SHAPE(FoodEaten, 1);
};

/// The run ended. Published for whoever cares; today nobody accepts it — a
/// deliberate open seam (a leaderboard, a death-cam), not an oversight.
struct SnakeDied {
    ZEN_SHAPE(SnakeDied, 1);
};

/// Advance one step of world time. Producer unspecified by design.
struct SnakeTick {
    ZEN_SHAPE(SnakeTick, 1);
};

/// Steer. Reversals (up→down etc.) are ignored by the world, not refused —
/// a game input is a wish, not a command.
struct SnakeTurn {
    std::int64_t direction = kRight;
    ZEN_SHAPE(SnakeTurn, 1, ZEN_FIELD(direction));
};

namespace v1 {

/// The world's own state, exactly as locked. An empty `snake` means a new game is pending: the
/// world seeds itself on the next tick, so a poke-reset (default state) means "new game" for
/// free. `ZEN_EXPOSE()`: every field is poke-manipulable, in the open -- the state holds no
/// secrets, and the suite uses that hand as its game-master.
struct SnakeWorldState {
    std::int64_t width = 24;
    std::int64_t height = 16;
    std::vector<Pos> snake;
    Pos food;
    std::int64_t direction = kRight;
    bool alive = true;
    std::int64_t score = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(SnakeWorldState, 1, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(snake),
              ZEN_FIELD(food), ZEN_FIELD(direction), ZEN_FIELD(alive), ZEN_FIELD(score));
};

} // namespace v1

namespace v2 {

/// v2 = v1 + `growths`, on a larger default map. Same wire name, new version:
/// a distinct content-id by construction — the migration chain's key.
struct SnakeWorldState {
    std::int64_t width = 48;
    std::int64_t height = 24;
    std::vector<Pos> snake;
    Pos food;
    std::int64_t direction = kRight;
    bool alive = true;
    std::int64_t score = 0;
    std::int64_t growths = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(SnakeWorldState, 2, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(snake),
              ZEN_FIELD(food), ZEN_FIELD(direction), ZEN_FIELD(alive), ZEN_FIELD(score),
              ZEN_FIELD(growths));
};

} // namespace v2

/// The one role slot this package still binds. Roles are the addresses that
/// survive replacement — the Stage 2 phase was three demonstrations of exactly
/// that. (The old snake.drawer role retired with the drawers: painting is the
/// Surface package's ground now, addressed as zengine.skin.)
inline constexpr const char* kWorldRole = "snake.world";

/// The world's pace — the host's old hard-coded 120ms cadence, moved home.
/// Owned and asked for by the snake-clock adapter (clock.cpp): it starts a
/// repeating Timer-package timer under this id and relays each TimerFired
/// into a SnakeTick for whoever holds snake.world. The world itself never
/// learns where ticks come from; only the SOURCE of time moved.
inline constexpr const char* kTickTimerId = "snake.tick";
inline constexpr std::int64_t kTickMs = 120;

} // namespace zengine::snake

#endif // ZENGINE_SNAKE_VOCABULARY_HPP
