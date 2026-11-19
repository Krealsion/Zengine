// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXAMPLES_TILE_DEFENSE_PICTURE_HPP
#define ZENGINE_EXAMPLES_TILE_DEFENSE_PICTURE_HPP

// The tile defense's picture, pure: the world drawn whole, in layers a game counts on -- the sheet,
// then the board's map, towers, walkers, health bars, shots, sparks and cursor, then the words
// beneath -- each layer one node however much it holds. It says what it draws; each window shows
// it its own way. Reference: examples/tile-defense/README.md.

#include "art.hpp"
#include "rules.hpp"
#include "vocabulary.hpp"

#include "graphics/canvas.hpp"
#include "graphics/colors.hpp" // generated from graphics/pool.json
#include "graphics/group.hpp"
#include "graphics/points.hpp"
#include "graphics/sprites.hpp"
#include "graphics/tiles.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::tile_defense {

/// What a press or a key names beneath the board: build an arrow tower, a fireball tower, send the
/// next wave, quit.
inline constexpr const char* kActArrow = "arrow";
inline constexpr const char* kActFire = "fire";
inline constexpr const char* kActWave = "wave";
inline constexpr const char* kActQuit = "quit";

/// The node the sheet is drawn as.
inline const std::string kSheetPath = "sheet";

/// WHERE THE PICTURE STANDS in its window: a tile's side and how the window sets words, as its
/// room says them.
struct Layout {
    gfx::TileSpace board;      ///< the board's corner and a tile's side
    gfx::TextMeasure words;    ///< a character's advance and a line's height
    std::int64_t hud_y = 0;    ///< the top of the words beneath the board

    static Layout of(std::int64_t cell_px, const gfx::TextMeasure& words) {
        Layout l;
        l.board = gfx::TileSpace{0, 0, gfx::tile_px(kTileWantPx, cell_px)};
        l.words = words;
        l.hud_y = kRows * l.board.tile + words.line / 2;
        return l;
    }
    /// The area this picture wants: the board, and its lines of words beneath.
    std::int64_t width() const { return kCols * board.tile; }
    std::int64_t height() const { return hud_y + kHudLines * words.line; }
};

/// The map's tiles: the road, the castle at its end, and grass with trees and rocks scattered off
/// the road, the same every game.
inline gfx::TileGrid map_of(const Art& a) {
    gfx::TileGrid map(kCols, kRows);
    for (std::int64_t r = 0; r < kRows; ++r) {
        for (std::int64_t c = 0; c < kCols; ++c) {
            const auto h = static_cast<std::uint32_t>((c * 2654435761u) ^ (r * 40503u + 7u));
            gfx::FrameId f = a.grass[h % 3];
            if (is_road(c, r)) {
                f = a.road;
            } else if (h % 23 == 0) {
                f = a.tree;
            } else if (h % 41 == 0) {
                f = a.rock;
            }
            map.set(c, r, f);
        }
    }
    map.set(kCastleCol, kCastleRow, a.castle);
    return map;
}

/// The frame of eight an arrow flying (dx, dy) shows: east first, then clockwise.
inline std::size_t direction_of(double dx, double dy) {
    const double a = std::atan2(dy, dx); // y grows down the board, so this turns clockwise
    const auto eighth = static_cast<std::int64_t>(std::lround(a / 0.7853981633974483));
    return static_cast<std::size_t>(((eighth % 8) + 8) % 8);
}

/// A tower's frame now: an arrow tower's bow drawn as it nears its shot and loosed just after; a
/// fireball tower's brazier burning, and flaring as it throws.
inline gfx::FrameId tower_frame(const Art& a, const TileTower& t, std::int64_t step) {
    const std::int64_t since = step - t.shot_at;
    if (t.kind == kArrowTower) {
        if (since < 8) {
            return a.arrow_tower[3];
        }
        if (t.cooldown > 0 && t.cooldown <= 6) {
            return a.arrow_tower[2];
        }
        if (t.cooldown > 0 && t.cooldown <= 14) {
            return a.arrow_tower[1];
        }
        return a.arrow_tower[0];
    }
    if (since < 10) {
        return a.fire_tower[3];
    }
    return a.fire_tower[static_cast<std::size_t>((step / 9) % 3)];
}

/// A spark's colour by its age: white-hot, yellow, orange, red, then smoke.
inline gfx::Color spark_color(const TileSpark& s) {
    static const std::array<gfx::Color, 4> ramp = {
        gfx::rgba(0xffe14dffu), gfx::rgba(0xff8c1affu), gfx::rgba(0xd8361affu),
        gfx::rgba(0x6b6b6bffu)};
    const std::int64_t at = s.life > 0 ? s.age * 4 / s.life : 3;
    return ramp[static_cast<std::size_t>(std::clamp<std::int64_t>(at, 0, 3))];
}

/// THE WHOLE PICTURE of `w`, laid out as `l` says, from the frames of `a` and the map's tiles.
inline void draw(gfx::Sketch& drawing, const TileWorld& w, const Art& a, const gfx::TileGrid& map,
                 const Layout& l) {
    namespace colors = gfx::colors;
    const gfx::TileSpace& tiles = l.board;
    const std::int64_t tile = tiles.tile;
    drawing.sheet(kSheetPath, a.sheet);

    const gfx::Group board(drawing, "board", tiles.whole(kCols, kRows));
    const gfx::TileSpace inside{0, 0, tile};
    board.tiles("map", map.args(kSheetPath, inside));

    // THE REACH of what stands under the cursor, or would be built there, in dots beneath all.
    const std::int64_t spark_px = std::max<std::int64_t>(1, tile / 8);
    const TileTower* under = tower_on(w, w.cursor_col, w.cursor_row);
    const double reach = kTowers[static_cast<std::size_t>(under ? under->kind : w.build)].range;
    gfx::PointBatch ring(spark_px);
    const double cx = static_cast<double>(w.cursor_col) + 0.5;
    const double cy = static_cast<double>(w.cursor_row) + 0.5;
    for (int i = 0; i < 72; ++i) {
        const double t = i * 6.283185307179586 / 72;
        (void)ring.add(inside.px_x(cx + std::cos(t) * reach), inside.px_y(cy + std::sin(t) * reach),
                       colors::text);
    }
    board.points("reach", ring.args());


    gfx::SpriteBatch towers;
    for (const TileTower& t : w.towers) {
        towers.add(inside.box(static_cast<double>(t.col), static_cast<double>(t.row)),
                   tower_frame(a, t, w.step));
    }
    board.sprites("towers", towers.args(kSheetPath));

    gfx::SpriteBatch walkers, bars;
    for (const TileWalker& e : w.enemies) {
        const At p = on_road(e.along);
        const auto step = static_cast<std::size_t>((w.step / 7 + e.id) % 4);
        const gfx::Box at = inside.box(p.x - 0.5, p.y - 0.5);
        walkers.add(at, e.kind == kBrute ? a.brute[step] : a.runner[step]);
        const std::int64_t bar_w = at.w * 3 / 4;
        const gfx::Box bar{at.x + (at.w - bar_w) / 2, at.y - tile / 6, bar_w, (tile + 7) / 8};
        bars.add(bar, a.bar_back);
        bars.add(bar.sized(e.max_hp > 0 ? bar_w * e.hp / e.max_hp : 0, bar.h), a.bar_front);
    }
    board.sprites("walkers", walkers.args(kSheetPath));
    board.sprites("bars", bars.args(kSheetPath));

    gfx::SpriteBatch shots;
    for (const TileShot& s : w.shots) {
        if (s.kind == kArrowTower) {
            shots.add(inside.box(s.x - 0.5, s.y - 0.5), a.arrow[direction_of(s.dx, s.dy)]);
        } else {
            const auto flicker = static_cast<std::size_t>(((w.step - s.born) / 3) % 4);
            shots.add(inside.box(s.x - 0.4, s.y - 0.4, 0.8, 0.8), a.fireball[flicker]);
        }
    }
    for (const TileBurst& b : w.bursts) {
        const auto age = static_cast<std::size_t>(
            std::clamp<std::int64_t>((w.step - b.born) * 3 / kBurstSteps, 0, 2));
        shots.add(inside.box(b.x - 0.75, b.y - 0.75, 1.5, 1.5), a.burst[age]);
    }
    board.sprites("shots", shots.args(kSheetPath));

    gfx::PointBatch sparks(spark_px);
    for (const TileSpark& s : w.sparks) {
        (void)sparks.add(inside.px_x(s.x) - spark_px / 2, inside.px_y(s.y) - spark_px / 2,
                         spark_color(s));
    }
    board.points("sparks", sparks.args());

    // THE CURSOR, over everything on the board.
    gfx::SpriteBatch cursor;
    cursor.add(inside.box(static_cast<double>(w.cursor_col), static_cast<double>(w.cursor_row)),
               a.cursor);
    board.sprites("cursor", cursor.args(kSheetPath));

    // THE WORDS BENEATH THE BOARD: the count, the choices, and what just happened.
    const gfx::Group hud(drawing, "hud", 0, l.hud_y);
    std::string count = "Gold " + std::to_string(w.gold) + "   Lives " + std::to_string(w.lives) +
                        "   Wave " + std::to_string(w.wave) + "/" + std::to_string(kWaves);
    if (w.paused) {
        count += "   Paused";
    }
    hud.text("count", 0, 0, count, w.lives > 0 ? colors::text : colors::alert);
    struct Choice {
        const char* name;
        std::string words;
        bool chosen;
    };
    const std::vector<Choice> choices = {
        {kActArrow, "[A] Arrow " + std::to_string(kTowers[0].cost), w.build == kArrowTower},
        {kActFire, "[F] Fireball " + std::to_string(kTowers[1].cost), w.build == kFireTower},
        {kActWave, "[Space] Wave", false},
        {kActQuit, "[Q] Quit", false},
    };
    std::int64_t col = 0;
    for (const Choice& c : choices) {
        hud.text(c.name, col * l.words.advance, l.words.line, c.words,
                 c.chosen ? colors::accent : colors::soft);
        hud.action(c.name, c.name);
        col += static_cast<std::int64_t>(c.words.size()) + 2;
    }
    hud.text("note", 0, 2 * l.words.line, w.note, w.lives > 0 ? colors::done : colors::alert);
}

} // namespace zengine::tile_defense

#endif // ZENGINE_EXAMPLES_TILE_DEFENSE_PICTURE_HPP
