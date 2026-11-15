// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXAMPLES_TILE_DEFENSE_GAME_HPP
#define ZENGINE_EXAMPLES_TILE_DEFENSE_GAME_HPP

// The tile defense's weave: the world (rules.hpp), its keys and presses, its beat, and its picture
// (picture.hpp), drawn whole after every delivery and sent to its window as what changed. It names
// no window and no medium: the window that shows its area says a tile's size and how it sets words
// in the room it gives, and shows the picture its own way. Loaded as `tile-defense` (game.cpp).
// Reference: examples/tile-defense/README.md.

#include "art.hpp"
#include "picture.hpp"
#include "rules.hpp"
#include "vocabulary.hpp"

#include "graphics/drawing.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "timer/binding.hpp"

#include <zen/weave.hpp>

#include <chrono>
#include <cstdint>
#include <string>

namespace zengine::tile_defense {

namespace scan = zengine::input::scan;

/// The id of the game's own beat.
inline constexpr const char* kStepTimerId = "tile-defense.step";

/// THE GAME'S WEAVE, in the role `kGameRole`, drawing in its area `kGameArea`.
class TileDefense
    : public gfx::With<gfx::DrawingFacet>::Weave<
          TileDefense, TileWorld,
          loom::Accept<gfx::AreaKeyPressed, gfx::AreaButtonChanged, gfx::AreaPointerMoved, TileTick>,
          loom::Emit<TileQuit>, zengine::timer::TimedWeave> {
public:
    using Weave::on;

    TileDefense()
        : Weave(kGameRole, kGameArea), art_(make_art()), map_(map_of(art_)),
          beat_(timers().repeat(kStepTimerId, std::chrono::milliseconds(kStepMs),
                                &TileDefense::on_beat)) {
        state_ = new_world();
    }

    /// A driver's steps: a replay, a stepper, a suite.
    void on(const TileTick& t, loom::Mail&) {
        for (std::int64_t i = 0; i < t.steps && i < 100000; ++i) {
            step(state_);
        }
    }

    /// The game's keys: A and F choose a tower, the arrows move the cursor, Return or B builds
    /// there, Space or N sends the next wave, P pauses, R starts again once the game is
    /// over, and Q or Escape quits.
    void on(const gfx::AreaKeyPressed& k, loom::Mail& mail) {
        if (k.area != kGameArea) {
            return;
        }
        switch (k.scancode) {
        case scan::kA: act(kActArrow, mail); break;
        case scan::kF: act(kActFire, mail); break;
        case scan::kSpace:
        case scan::kN: act(kActWave, mail); break;
        case scan::kQ:
        case scan::kEscape: act(kActQuit, mail); break;
        case scan::kP: state_.paused = !state_.paused; break;
        case scan::kR:
            if (over()) {
                state_ = new_world();
            }
            break;
        case scan::kReturn:
        case scan::kB:
            (void)build(state_, state_.cursor_col, state_.cursor_row);
            break;
        default: steer(k.scancode); break;
        }
    }

    /// A press: on a choice beneath the board, that choice; on the board, the cursor there and a
    /// tower built on its tile.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        if (b.area != kGameArea || b.button != 1 || !b.pressed) {
            return;
        }
        if (!b.part.empty()) {
            act(b.part, mail);
            return;
        }
        if (const auto tile = layout().board.tile_at(b.x, b.y, kCols, kRows)) {
            state_.cursor_col = tile->first;
            state_.cursor_row = tile->second;
            (void)build(state_, tile->first, tile->second);
        }
    }

    /// The pointer over the board: the cursor follows it.
    void on(const gfx::AreaPointerMoved& m, loom::Mail&) {
        if (m.area != kGameArea) {
            return;
        }
        if (const auto tile = layout().board.tile_at(m.x, m.y, kCols, kRows)) {
            state_.cursor_col = tile->first;
            state_.cursor_row = tile->second;
        }
    }

    /// The whole picture, from the world and the room the window gave.
    void picture(gfx::Sketch& drawing) const { draw(drawing, state_, art_, map_, layout()); }

    /// The world as it stands.
    const TileWorld& world() const { return state_; }

private:
    void on_beat(const zengine::timer::TimerFired& f, loom::Mail&) {
        if (f.id == beat_.id()) {
            step(state_);
        }
    }

    /// What a key or a press names.
    void act(const std::string& what, loom::Mail& mail) {
        if (what == kActArrow) {
            state_.build = kArrowTower;
            state_.note = "Arrow towers shoot quick and far.";
        } else if (what == kActFire) {
            state_.build = kFireTower;
            state_.note = "Fireball towers burst on what they strike.";
        } else if (what == kActWave) {
            if (!send_wave(state_) && !over()) {
                state_.note = "A wave still walks.";
            }
        } else if (what == kActQuit) {
            (void)mail.as_role(kGameRole).publish(TileQuit{});
        }
    }

    /// The cursor moved by an arrow key.
    void steer(std::int64_t scancode) {
        if (scancode == scan::kUp) {
            move_cursor(state_, 0, -1);
        } else if (scancode == scan::kDown) {
            move_cursor(state_, 0, 1);
        } else if (scancode == scan::kLeft) {
            move_cursor(state_, -1, 0);
        } else if (scancode == scan::kRight) {
            move_cursor(state_, 1, 0);
        }
    }

    bool over() const { return state_.lives <= 0 || (state_.wave >= kWaves && !state_.walking); }

    /// Where the picture stands, from the window's room as the canvas holds it.
    Layout layout() const { return Layout::of(canvas().cell_px(), canvas().text()); }

    Art art_;
    gfx::TileGrid map_;
    Handle beat_;
};

} // namespace zengine::tile_defense

#endif // ZENGINE_EXAMPLES_TILE_DEFENSE_GAME_HPP
