// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXAMPLES_TILE_DEFENSE_RULES_HPP
#define ZENGINE_EXAMPLES_TILE_DEFENSE_RULES_HPP

// The tile defense's rules, pure: the road, the waves, the walkers, the towers and their shots,
// gold and lives, one step at a time. Every place is in tiles, a tile's centre at its column and
// row plus a half. Nothing here draws or reads a clock: a step is `kStepMs` of the game's time.
// Reference: examples/tile-defense/README.md.

#include "vocabulary.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::tile_defense {

/// The road's turns, in tiles: it comes in at the left edge and ends at the castle on the right.
struct Turn {
    double col, row;
};
inline constexpr std::array<Turn, 10> kRoad = {{{-1, 3},
                                                 {8, 3},
                                                 {8, 15},
                                                 {18, 15},
                                                 {18, 5},
                                                 {28, 5},
                                                 {28, 16},
                                                 {35, 16},
                                                 {35, 9},
                                                 {40, 9}}};
/// The castle's tile, where the road ends.
inline constexpr std::int64_t kCastleCol = 39, kCastleRow = 9;

// ---- the numbers of the game ---------------------------------------------------------------------

inline constexpr std::int64_t kStartGold = 150;
inline constexpr std::int64_t kStartLives = 20;
inline constexpr std::int64_t kWaves = 8;
inline constexpr std::int64_t kWaveBonus = 25;
inline constexpr std::int64_t kSpawnSteps = 40; ///< between two walkers of a wave

struct TowerKind {
    const char* name;
    std::int64_t cost;
    double range;        ///< tiles
    std::int64_t reload; ///< steps between shots
    double speed;        ///< its shot's, tiles a step
    std::int64_t damage;
};
inline constexpr std::array<TowerKind, 2> kTowers = {{
    {"arrow", 50, 7.5, 36, 0.26, 11},
    {"fireball", 80, 8.5, 84, 0.10, 34},
}};
/// A fireball's burst: how far it reaches, in tiles, and what it does to every walker there.
inline constexpr double kBurstReach = 1.4;
inline constexpr std::int64_t kBurstDamage = 16;

struct WalkerKind {
    const char* name;
    double speed; ///< tiles a step
    std::int64_t hp;
    std::int64_t bounty;
    std::int64_t bite; ///< lives it takes at the castle
};
inline constexpr std::array<WalkerKind, 2> kWalkers = {{
    {"runner", 0.045, 34, 6, 1},
    {"brute", 0.02, 120, 14, 2},
}};

/// How long a spark of a trail and of a burst lives, at most, in steps; and a burst's own.
inline constexpr std::int64_t kTrailLife = 26;
inline constexpr std::int64_t kBurstSparkLife = 34;
inline constexpr std::int64_t kBurstSteps = 18;
inline constexpr std::int64_t kBurstSparks = 26;

// ---- the road -------------------------------------------------------------------------------------

/// A place in tiles, a tile's centre at its column and row plus a half.
struct At {
    double x = 0, y = 0;
};

/// The road's length, in tiles.
inline double road_length() {
    double n = 0;
    for (std::size_t i = 1; i < kRoad.size(); ++i) {
        n += std::abs(kRoad[i].col - kRoad[i - 1].col) + std::abs(kRoad[i].row - kRoad[i - 1].row);
    }
    return n;
}

/// Where a walker `along` tiles down the road stands, at the middle of the road's tiles.
inline At on_road(double along) {
    double left = along;
    for (std::size_t i = 1; i < kRoad.size(); ++i) {
        const Turn a = kRoad[i - 1], b = kRoad[i];
        const double leg = std::abs(b.col - a.col) + std::abs(b.row - a.row);
        if (left <= leg || i + 1 == kRoad.size()) {
            const double t = leg > 0 ? std::min(left, leg) / leg : 0;
            return At{a.col + (b.col - a.col) * t + 0.5, a.row + (b.row - a.row) * t + 0.5};
        }
        left -= leg;
    }
    return At{kRoad.back().col + 0.5, kRoad.back().row + 0.5};
}

/// Whether the tile (col, row) is road.
inline bool is_road(std::int64_t col, std::int64_t row) {
    for (std::size_t i = 1; i < kRoad.size(); ++i) {
        const Turn a = kRoad[i - 1], b = kRoad[i];
        const auto lo_c = static_cast<std::int64_t>(std::min(a.col, b.col));
        const auto hi_c = static_cast<std::int64_t>(std::max(a.col, b.col));
        const auto lo_r = static_cast<std::int64_t>(std::min(a.row, b.row));
        const auto hi_r = static_cast<std::int64_t>(std::max(a.row, b.row));
        if (col >= lo_c && col <= hi_c && row >= lo_r && row <= hi_r) {
            return true;
        }
    }
    return false;
}

/// Whether a tower stands on (col, row).
inline const TileTower* tower_on(const TileWorld& w, std::int64_t col, std::int64_t row) {
    for (const TileTower& t : w.towers) {
        if (t.col == col && t.row == row) {
            return &t;
        }
    }
    return nullptr;
}

// ---- the world's own dice ----------------------------------------------------------------------------

/// A number in [0, 1) from the world's seed, moving it on (xorshift).
inline double roll(TileWorld& w) {
    auto x = static_cast<std::uint64_t>(w.seed);
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    w.seed = static_cast<std::int64_t>(x);
    return static_cast<double>(x >> 11) / 9007199254740992.0;
}

// ---- the player's moves -------------------------------------------------------------------------------

/// A new world, its dice thrown from `seed`.
inline TileWorld new_world(std::int64_t seed = 0x2545F4914F6CDD1D) {
    TileWorld w;
    w.gold = kStartGold;
    w.lives = kStartLives;
    w.seed = seed == 0 ? 1 : seed;
    w.cursor_col = 4;
    w.cursor_row = 6;
    w.note = "Build towers, then Space sends a wave.";
    return w;
}

/// How many walkers wave `n` sends (from 1), and whether its `i`th is a brute.
inline std::int64_t wave_size(std::int64_t n) { return 6 + 3 * n; }
inline bool brute_at(std::int64_t n, std::int64_t i) { return n >= 2 && i % 3 == 2; }

/// The next wave sent: not while one walks, nor once the last is won or the castle fallen.
inline bool send_wave(TileWorld& w) {
    if (w.walking || w.lives <= 0 || w.wave >= kWaves) {
        return false;
    }
    ++w.wave;
    w.walking = true;
    w.to_spawn = wave_size(w.wave);
    w.spawn_in = 0;
    w.note = "Wave " + std::to_string(w.wave) + " comes.";
    return true;
}

/// A tower of the chosen kind built on (col, row): refused, with words, on the road, the castle, a
/// tower or off the board, and when the gold is short.
inline bool build(TileWorld& w, std::int64_t col, std::int64_t row) {
    const TowerKind& k = kTowers[static_cast<std::size_t>(w.build)];
    if (col < 0 || row < 0 || col >= kCols || row >= kRows) {
        return false;
    }
    if (is_road(col, row) || (col == kCastleCol && row == kCastleRow)) {
        w.note = "Not on the road.";
        return false;
    }
    if (tower_on(w, col, row) != nullptr) {
        w.note = "A tower stands there.";
        return false;
    }
    if (w.gold < k.cost) {
        w.note = std::string(w.build == kArrowTower ? "An " : "A ") + k.name + " tower costs " +
                 std::to_string(k.cost) + ".";
        return false;
    }
    w.gold -= k.cost;
    w.towers.push_back(TileTower{col, row, w.build, 0, -1000});
    w.note = std::string(w.build == kArrowTower ? "Built an " : "Built a ") + k.name + " tower.";
    return true;
}

/// The cursor moved by (dc, dr), held on the board.
inline void move_cursor(TileWorld& w, std::int64_t dc, std::int64_t dr) {
    w.cursor_col = std::clamp<std::int64_t>(w.cursor_col + dc, 0, kCols - 1);
    w.cursor_row = std::clamp<std::int64_t>(w.cursor_row + dr, 0, kRows - 1);
}

// ---- a step of the world ---------------------------------------------------------------------------

/// A tower's centre, in tiles.
inline At centre_of(const TileTower& t) {
    return At{static_cast<double>(t.col) + 0.5, static_cast<double>(t.row) + 0.5};
}

inline TileWalker* enemy_by_id(TileWorld& w, std::int64_t id) {
    for (TileWalker& e : w.enemies) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

/// The walker a tower aims at: the one furthest down the road within its reach.
inline const TileWalker* aim(const TileWorld& w, const TileTower& t) {
    const TowerKind& k = kTowers[static_cast<std::size_t>(t.kind)];
    const At c = centre_of(t);
    const TileWalker* best = nullptr;
    for (const TileWalker& e : w.enemies) {
        const At p = on_road(e.along);
        const double dx = p.x - c.x, dy = p.y - c.y;
        if (dx * dx + dy * dy <= k.range * k.range && (best == nullptr || e.along > best->along)) {
            best = &e;
        }
    }
    return best;
}

/// A walker struck: its health taken, and, gone to nothing, its bounty paid.
inline void strike(TileWorld& w, TileWalker& e, std::int64_t damage) {
    if (e.hp <= 0) {
        return;
    }
    e.hp -= damage;
    if (e.hp <= 0) {
        w.gold += kWalkers[static_cast<std::size_t>(e.kind)].bounty;
    }
}

/// A spark thrown from (x, y) at (vx, vy) tiles a step, living `life` steps.
inline void spark(TileWorld& w, double x, double y, double vx, double vy, std::int64_t life) {
    w.sparks.push_back(TileSpark{x, y, vx, vy, 0, life});
}

/// A fireball's burst at (x, y): every walker in reach struck, a burst shown, sparks thrown.
inline void burst(TileWorld& w, double x, double y) {
    for (TileWalker& e : w.enemies) {
        const At p = on_road(e.along);
        const double dx = p.x - x, dy = p.y - y;
        if (dx * dx + dy * dy <= kBurstReach * kBurstReach) {
            strike(w, e, kBurstDamage);
        }
    }
    w.bursts.push_back(TileBurst{x, y, w.step});
    for (std::int64_t i = 0; i < kBurstSparks; ++i) {
        const double a = roll(w) * 6.283185307179586;
        const double v = 0.03 + roll(w) * 0.07;
        spark(w, x, y, std::cos(a) * v, std::sin(a) * v,
              kBurstSparkLife / 2 + static_cast<std::int64_t>(roll(w) * kBurstSparkLife / 2));
    }
}

/// ONE STEP OF THE WORLD: a walker sent when its time comes, each walker on, each tower aimed and
/// loosed, each shot on to its mark, each spark and burst older; the castle bitten, the dead
/// taken off, and a wave won when none is left to come or walk.
inline void step(TileWorld& w) {
    if (w.paused || w.lives <= 0) {
        return;
    }
    ++w.step;
    // the wave's walkers, one at a time
    if (w.walking && w.to_spawn > 0) {
        if (w.spawn_in <= 0) {
            const std::int64_t i = wave_size(w.wave) - w.to_spawn;
            const std::int64_t kind = brute_at(w.wave, i) ? kBrute : kRunner;
            const WalkerKind& k = kWalkers[static_cast<std::size_t>(kind)];
            const std::int64_t hp = k.hp + k.hp * (w.wave - 1) / 4;
            w.enemies.push_back(TileWalker{w.next_id++, kind, 0, hp, hp});
            --w.to_spawn;
            w.spawn_in = kSpawnSteps;
        } else {
            --w.spawn_in;
        }
    }
    // the walkers on down the road, and those at its end into the castle
    const double length = road_length();
    for (TileWalker& e : w.enemies) {
        e.along += kWalkers[static_cast<std::size_t>(e.kind)].speed;
        if (e.along >= length && e.hp > 0) {
            w.lives = std::max<std::int64_t>(0, w.lives - kWalkers[static_cast<std::size_t>(e.kind)].bite);
            e.hp = 0;
            w.note = w.lives > 0 ? "A walker reached the castle." : "The castle fell.";
        }
    }
    // each tower aimed and loosed
    for (TileTower& t : w.towers) {
        if (t.cooldown > 0) {
            --t.cooldown;
            continue;
        }
        const TileWalker* target = aim(w, t);
        if (target == nullptr) {
            continue;
        }
        const TowerKind& k = kTowers[static_cast<std::size_t>(t.kind)];
        const At c = centre_of(t);
        const At p = on_road(target->along);
        w.shots.push_back(TileShot{t.kind, c.x, c.y, p.x - c.x, p.y - c.y, target->id, p.x, p.y, w.step});
        t.cooldown = k.reload;
        t.shot_at = w.step;
    }
    // each shot on to its mark: an arrow strikes its walker, a fireball bursts, trailing sparks
    std::vector<TileShot> flying;
    for (TileShot& s : w.shots) {
        if (TileWalker* e = enemy_by_id(w, s.target); e != nullptr && e->hp > 0) {
            const At p = on_road(e->along);
            s.tx = p.x;
            s.ty = p.y;
        }
        const TowerKind& k = kTowers[static_cast<std::size_t>(s.kind)];
        const double dx = s.tx - s.x, dy = s.ty - s.y;
        const double d = std::sqrt(dx * dx + dy * dy);
        if (d <= k.speed) {
            if (TileWalker* e = enemy_by_id(w, s.target)) {
                strike(w, *e, k.damage);
            }
            if (s.kind == kFireTower) {
                burst(w, s.tx, s.ty);
            }
            continue;
        }
        s.dx = dx / d;
        s.dy = dy / d;
        s.x += s.dx * k.speed;
        s.y += s.dy * k.speed;
        if (s.kind == kFireTower) {
            for (int i = 0; i < 2; ++i) {
                spark(w, s.x - s.dx * 0.2, s.y - s.dy * 0.2,
                      -s.dx * 0.01 + (roll(w) - 0.5) * 0.03, -s.dy * 0.01 + (roll(w) - 0.5) * 0.03,
                      kTrailLife / 2 + static_cast<std::int64_t>(roll(w) * kTrailLife / 2));
            }
        }
        flying.push_back(s);
    }
    w.shots = std::move(flying);
    // sparks drift and slow; bursts and sparks grow old
    for (TileSpark& s : w.sparks) {
        s.x += s.vx;
        s.y += s.vy;
        s.vx *= 0.94;
        s.vy *= 0.94;
        ++s.age;
    }
    std::erase_if(w.sparks, [](const TileSpark& s) { return s.age >= s.life; });
    std::erase_if(w.bursts, [&](const TileBurst& b) { return w.step - b.born >= kBurstSteps; });
    std::erase_if(w.enemies, [](const TileWalker& e) { return e.hp <= 0; });
    // the wave won
    if (w.walking && w.to_spawn == 0 && w.enemies.empty() && w.lives > 0) {
        w.walking = false;
        w.gold += kWaveBonus;
        w.note = w.wave >= kWaves ? "Every wave held off: the castle stands."
                                  : "Wave " + std::to_string(w.wave) + " held. Space sends the next.";
    }
}

} // namespace zengine::tile_defense

#endif // ZENGINE_EXAMPLES_TILE_DEFENSE_RULES_HPP
