// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXAMPLES_TILE_DEFENSE_VOCABULARY_HPP
#define ZENGINE_EXAMPLES_TILE_DEFENSE_VOCABULARY_HPP

// The tile defense's roles, its area and its shapes: the world the game keeps (`TileWorld`), what it
// says when its player quits (`TileQuit`), and the step a driver may give it (`TileTick`).
// Reference: examples/tile-defense/README.md.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::tile_defense {

/// The game's role, which draws in its area of the same name, and the stage's, which places it.
inline constexpr const char* kGameRole = "tile_defense.game";
inline constexpr const char* kGameArea = "tile_defense.game";
inline constexpr const char* kStageRole = "tile_defense.stage";

/// The world in tiles, and a tile's side where the window shows every pixel.
inline constexpr std::int64_t kCols = 40;
inline constexpr std::int64_t kRows = 20;
inline constexpr std::int64_t kTileWantPx = 16;
/// The lines of words beneath the board.
inline constexpr std::int64_t kHudLines = 3;

/// The game's beat: one step of the world, in milliseconds.
inline constexpr std::int64_t kStepMs = 16;

/// What walks the road: a runner, quick and frail; a brute, slow and hard.
inline constexpr std::int64_t kRunner = 0;
inline constexpr std::int64_t kBrute = 1;
/// What shoots: an arrow tower and a fireball tower.
inline constexpr std::int64_t kArrowTower = 0;
inline constexpr std::int64_t kFireTower = 1;

/// One walker: who, what, how far along the road (in tiles), and its health.
struct TileWalker {
    std::int64_t id = 0;
    std::int64_t kind = kRunner;
    double along = 0;
    std::int64_t hp = 0, max_hp = 0;
    ZEN_SHAPE(TileWalker, 1, ZEN_FIELD(id), ZEN_FIELD(kind), ZEN_FIELD(along), ZEN_FIELD(hp),
              ZEN_FIELD(max_hp));
};

/// One tower: where (a tile), what, the steps until it may shoot again, and the step it last shot.
struct TileTower {
    std::int64_t col = 0, row = 0;
    std::int64_t kind = kArrowTower;
    std::int64_t cooldown = 0;
    std::int64_t shot_at = -1000;
    ZEN_SHAPE(TileTower, 1, ZEN_FIELD(col), ZEN_FIELD(row), ZEN_FIELD(kind), ZEN_FIELD(cooldown),
              ZEN_FIELD(shot_at));
};

/// One shot in flight: what, where (in tiles), the walker it chases, where it saw it last, and the
/// step it was loosed.
struct TileShot {
    std::int64_t kind = kArrowTower;
    double x = 0, y = 0;
    double dx = 1, dy = 0;
    std::int64_t target = 0;
    double tx = 0, ty = 0;
    std::int64_t born = 0;
    ZEN_SHAPE(TileShot, 1, ZEN_FIELD(kind), ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(dx), ZEN_FIELD(dy),
              ZEN_FIELD(target), ZEN_FIELD(tx), ZEN_FIELD(ty), ZEN_FIELD(born));
};

/// One spark: where and how fast (tiles, tiles a step), its age and its life, in steps.
struct TileSpark {
    double x = 0, y = 0, vx = 0, vy = 0;
    std::int64_t age = 0, life = 0;
    ZEN_SHAPE(TileSpark, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(vx), ZEN_FIELD(vy), ZEN_FIELD(age),
              ZEN_FIELD(life));
};

/// One burst where a fireball struck: where, and the step it struck.
struct TileBurst {
    double x = 0, y = 0;
    std::int64_t born = 0;
    ZEN_SHAPE(TileBurst, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(born));
};

/// THE WORLD: the step it stands at, the player's gold and lives, the wave, its walkers still to
/// come and the steps to the next, whether it walks, the tower to build and the tile the cursor
/// stands on, the words beneath the board, and everything on the board.
struct TileWorld {
    std::int64_t step = 0;
    std::int64_t gold = 0, lives = 0;
    std::int64_t wave = 0;
    std::int64_t to_spawn = 0;
    std::int64_t spawn_in = 0;
    bool walking = false;
    bool paused = false;
    std::int64_t build = kArrowTower;
    std::int64_t cursor_col = 0, cursor_row = 0;
    std::string note;
    std::int64_t next_id = 1;
    std::int64_t seed = 0;
    std::vector<TileWalker> enemies;
    std::vector<TileTower> towers;
    std::vector<TileShot> shots;
    std::vector<TileSpark> sparks;
    std::vector<TileBurst> bursts;
    ZEN_SHAPE(TileWorld, 1, ZEN_FIELD(step), ZEN_FIELD(gold), ZEN_FIELD(lives), ZEN_FIELD(wave),
              ZEN_FIELD(to_spawn), ZEN_FIELD(spawn_in), ZEN_FIELD(walking), ZEN_FIELD(paused),
              ZEN_FIELD(build), ZEN_FIELD(cursor_col), ZEN_FIELD(cursor_row), ZEN_FIELD(note),
              ZEN_FIELD(next_id), ZEN_FIELD(seed), ZEN_FIELD(enemies), ZEN_FIELD(towers),
              ZEN_FIELD(shots), ZEN_FIELD(sparks), ZEN_FIELD(bursts));
};

/// The game -> anyone: its player asked to quit.
struct TileQuit {
    ZEN_SHAPE(TileQuit, 1);
};

/// Anyone -> the game, by its role: take `steps` steps of the world now, as its own beat would.
struct TileTick {
    std::int64_t steps = 1;
    ZEN_SHAPE(TileTick, 1, ZEN_FIELD(steps));
};

} // namespace zengine::tile_defense

#endif // ZENGINE_EXAMPLES_TILE_DEFENSE_VOCABULARY_HPP
