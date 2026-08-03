// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_SKIN_SDL_PLAN_HPP
#define ZENGINE_SURFACE_SKIN_SDL_PLAN_HPP

// The SDL Skin's brain, SDL-free (the zen-ui-pixel move): a SnakeVisual
// becomes a list of colored rectangles and a window geometry, as pure math.
// Every lane pins this — including the ones that never build SDL — so the
// only thing the SDL edge (skin_sdl.cpp) adds is executing the plan against a
// real renderer, and the only thing that can be wrong THERE is plumbing.
//
// The plan speaks pixels because the medium does; the INTENT it consumes has
// no pixel anywhere in it. That direction — medium-specific numbers derived
// from medium-agnostic intent, never the reverse — is the agnosticism the
// phase exists to prove.

#include "snake/vocabulary.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::surface {

/// One filled rectangle, window coordinates, opaque color.
struct PlanRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

struct PlanSize {
    std::int64_t w = 0;
    std::int64_t h = 0;
};

inline constexpr std::int64_t kCellPx = 24;  ///< one grid cell, square
inline constexpr std::int64_t kMarginPx = 12; ///< board inset from the window edge
inline constexpr std::int64_t kCellGapPx = 1; ///< breathing room inside a cell

/// The window sized to the board: the visual's grid dictates geometry, the
/// medium never dictates the visual (a growth mid-game is a window resize).
inline PlanSize window_size_of(const zengine::snake::SnakeVisual& v) {
    return PlanSize{v.width * kCellPx + 2 * kMarginPx, v.height * kCellPx + 2 * kMarginPx};
}

/// The frame as rectangles, background first (painter's order):
///   - background: near-black while alive, a dark red the moment the run dies
///     (the whole window is the death banner — no font required);
///   - food: gold, inset deeper than the snake so it reads as a pickup;
///   - body: green; head: brighter green, drawn last.
/// The off-board food sentinel (kNoFood) simply plans no food rectangle.
inline std::vector<PlanRect> plan_frame(const zengine::snake::SnakeVisual& v) {
    std::vector<PlanRect> out;
    out.reserve(v.snake.size() + 2);
    const PlanSize win = window_size_of(v);

    if (v.alive) {
        out.push_back(PlanRect{0, 0, win.w, win.h, 18, 18, 24});
    } else {
        out.push_back(PlanRect{0, 0, win.w, win.h, 88, 16, 16});
    }

    const auto cell = [](std::int64_t cx, std::int64_t cy, std::int64_t inset) {
        return PlanRect{kMarginPx + cx * kCellPx + inset, kMarginPx + cy * kCellPx + inset,
                        kCellPx - 2 * inset, kCellPx - 2 * inset, 0, 0, 0};
    };

    if (v.food.x >= 0 && v.food.x < v.width && v.food.y >= 0 && v.food.y < v.height) {
        PlanRect f = cell(v.food.x, v.food.y, kCellPx / 4);
        f.r = 232;
        f.g = 196;
        f.b = 64;
        out.push_back(f);
    }

    for (std::size_t i = v.snake.size(); i > 1; --i) {
        PlanRect s = cell(v.snake[i - 1].x, v.snake[i - 1].y, kCellGapPx);
        s.r = 64;
        s.g = 168;
        s.b = 80;
        out.push_back(s);
    }
    if (!v.snake.empty()) {
        PlanRect h = cell(v.snake.front().x, v.snake.front().y, kCellGapPx);
        h.r = 112;
        h.g = 232;
        h.b = 128;
        out.push_back(h);
    }
    return out;
}

/// The window title carries the text slots — a real, visible projection of
/// SurfaceText that costs no font stack. ASCII by the house charset rule.
inline std::string title_of(const std::string& status, const std::string& score) {
    std::string t = "zengine [sdl skin]";
    if (!status.empty()) {
        t += " | " + status;
    }
    if (!score.empty()) {
        t += " | " + score;
    }
    return t;
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_SKIN_SDL_PLAN_HPP
