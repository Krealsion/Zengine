// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SNAKE_VOCABULARY_HPP
#define ZENGINE_SNAKE_VOCABULARY_HPP

// The snake package's message vocabulary, in one file. The world publishes its news -- the game as
// a drawer would need it (`SnakeVisual`), a meal and a death -- for anyone; it takes world time
// (`SnakeTick`) and steering (`SnakeTurn`) from anyone, by role, as well as from its own beat and
// keys; and it holds its state. Its board is the game's own element (board.hpp).
// Reference: examples/snake/README.md.

#include "input/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::snake {

/// Direction encoding, shared by SnakeWorldState.direction and SnakeTurn:
/// 0=up, 1=right, 2=down, 3=left.
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

/// The game as a drawer would need it, published for anyone after each step. `snake` is
/// head-first.
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

/// The run ended, published for whoever cares.
struct SnakeDied {
    ZEN_SHAPE(SnakeDied, 1);
};

/// Advance one step of world time: the world's open door for any driver -- a replay, a stepper,
/// a test -- beside its own beat.
struct SnakeTick {
    ZEN_SHAPE(SnakeTick, 1);
};

/// Steer: the world's open door for any driver -- a pilot, a replay -- beside its own keys.
/// Reversals (up->down etc.) are ignored by the world, not refused: a game input is a wish.
struct SnakeTurn {
    std::int64_t direction = kRight;
    ZEN_SHAPE(SnakeTurn, 1, ZEN_FIELD(direction));
};

/// The world's pace: its own beat, a repeating Timer timer, every this many milliseconds.
inline constexpr std::int64_t kTickMs = 120;

namespace v1 {

/// The first world's state: the game (an empty `snake` is a new game, seeded on the next tick),
/// its bindings -- whether its keys steer and which four, and its pace, 0 for no beat of its own
/// -- and the beats it has taken. Every field is poke-manipulable: the state holds no secrets,
/// and the suite uses that hand as its game-master.
struct SnakeWorldState {
    std::int64_t width = 24;
    std::int64_t height = 16;
    std::vector<Pos> snake;
    Pos food;
    std::int64_t direction = kRight;
    bool alive = true;
    std::int64_t score = 0;
    bool keys = true;
    std::int64_t key_up = zengine::input::scan::kW;
    std::int64_t key_right = zengine::input::scan::kD;
    std::int64_t key_down = zengine::input::scan::kS;
    std::int64_t key_left = zengine::input::scan::kA;
    std::int64_t beat_ms = kTickMs;
    std::int64_t ticks = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(SnakeWorldState, 5, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(snake),
              ZEN_FIELD(food), ZEN_FIELD(direction), ZEN_FIELD(alive), ZEN_FIELD(score),
              ZEN_FIELD(keys), ZEN_FIELD(key_up), ZEN_FIELD(key_right), ZEN_FIELD(key_down),
              ZEN_FIELD(key_left), ZEN_FIELD(beat_ms), ZEN_FIELD(ticks));
};

} // namespace v1

namespace v2 {

/// The heir's state: the first world's plus `growths`, on a larger default board. Same wire name,
/// the next version: a distinct content id by construction, the migration's key.
struct SnakeWorldState {
    std::int64_t width = 48;
    std::int64_t height = 24;
    std::vector<Pos> snake;
    Pos food;
    std::int64_t direction = kRight;
    bool alive = true;
    std::int64_t score = 0;
    bool keys = true;
    std::int64_t key_up = zengine::input::scan::kW;
    std::int64_t key_right = zengine::input::scan::kD;
    std::int64_t key_down = zengine::input::scan::kS;
    std::int64_t key_left = zengine::input::scan::kA;
    std::int64_t beat_ms = kTickMs;
    std::int64_t ticks = 0;
    std::int64_t growths = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(SnakeWorldState, 6, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(snake),
              ZEN_FIELD(food), ZEN_FIELD(direction), ZEN_FIELD(alive), ZEN_FIELD(score),
              ZEN_FIELD(keys), ZEN_FIELD(key_up), ZEN_FIELD(key_right), ZEN_FIELD(key_down),
              ZEN_FIELD(key_left), ZEN_FIELD(beat_ms), ZEN_FIELD(ticks), ZEN_FIELD(growths));
};

} // namespace v2

/// The one role this package binds: roles are the addresses that survive replacement. It is also
/// the world's office, the owner of its area.
inline constexpr const char* kWorldRole = "snake.world";
/// The area the world draws in, and the score weave's office and area.
inline constexpr const char* kWorldArea = "snake.world";
inline constexpr const char* kScoreRole = "snake.score";

/// The world's own beat, a repeating timer under this id (a new pace asks under a new one).
inline constexpr const char* kTickTimerId = "snake.tick";

/// The element type the board is presented as, and the world's reference for it.
inline constexpr std::int64_t kBoardRef = 1;
/// The pixels of one board cell: one terminal cell.
inline constexpr std::int64_t kBoardCellPx = 12;

} // namespace zengine::snake

#endif // ZENGINE_SNAKE_VOCABULARY_HPP
