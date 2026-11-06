// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SNAKE_PILOT_HPP
#define ZENGINE_SNAKE_PILOT_HPP

// The pilot's choice as a pure function of what the world publishes -- no bus, clock or
// randomness -- so the pilot weave (pilot.cpp) and the suite's lockstep mirror choose alike. It
// steers as an old game AI does: the shortest way to the food when its own tail can still be
// reached once it has eaten, else toward its own tail, else into the most room it has left.
// Reference: examples/snake/README.md.

#include "vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace zengine::snake {

namespace pilot_detail {

/// The four directions in the order the pilot breaks ties: up, right, down, left.
inline constexpr std::int64_t kDirs[] = {kUp, kRight, kDown, kLeft};

struct Board {
    std::int64_t w = 0;
    std::int64_t h = 0;

    bool inside(const Pos& p) const { return p.x >= 0 && p.x < w && p.y >= 0 && p.y < h; }
    std::size_t cell(const Pos& p) const { return static_cast<std::size_t>(p.y * w + p.x); }
    std::size_t cells() const { return static_cast<std::size_t>(w * h); }
};

inline Pos ahead(const Pos& p, std::int64_t dir) {
    switch (dir) {
    case kUp: return Pos{p.x, p.y - 1};
    case kRight: return Pos{p.x + 1, p.y};
    case kDown: return Pos{p.x, p.y + 1};
    default: return Pos{p.x - 1, p.y};
    }
}

inline bool same(const Pos& a, const Pos& b) { return a.x == b.x && a.y == b.y; }

/// The cells a head cannot enter on its next step: the body, less the tail, which vacates in the
/// same step -- unless that step eats.
inline std::vector<bool> walls(const Board& b, const std::vector<Pos>& snake, bool eats) {
    std::vector<bool> blocked(b.cells(), false);
    const std::size_t solid = snake.size() - (eats || snake.size() < 2 ? 0 : 1);
    for (std::size_t i = 0; i < solid; ++i) {
        blocked[b.cell(snake[i])] = true;
    }
    return blocked;
}

/// The shortest way from `from` to `to` around `blocked`, as the directions to take; empty when
/// there is none. Neighbours are tried in `kDirs` order, so a way is the same every time.
inline std::vector<std::int64_t> way(const Board& b, const Pos& from, const Pos& to,
                                     const std::vector<bool>& blocked) {
    if (!b.inside(to)) {
        return {};
    }
    std::vector<std::int64_t> came(b.cells(), -1);
    std::vector<bool> seen(b.cells(), false);
    std::deque<Pos> open{from};
    seen[b.cell(from)] = true;
    while (!open.empty()) {
        const Pos at = open.front();
        open.pop_front();
        if (same(at, to)) {
            std::vector<std::int64_t> dirs;
            for (Pos p = to; !same(p, from);) {
                const std::int64_t d = came[b.cell(p)];
                dirs.insert(dirs.begin(), d);
                p = ahead(p, (d + 2) % 4);
            }
            return dirs;
        }
        for (const std::int64_t d : kDirs) {
            const Pos n = ahead(at, d);
            if (!b.inside(n) || seen[b.cell(n)] || (blocked[b.cell(n)] && !same(n, to))) {
                continue;
            }
            seen[b.cell(n)] = true;
            came[b.cell(n)] = d;
            open.push_back(n);
        }
    }
    return {};
}

/// How many cells a head at `from` can reach around `blocked`.
inline std::size_t room(const Board& b, const Pos& from, const std::vector<bool>& blocked) {
    std::vector<bool> seen(b.cells(), false);
    std::deque<Pos> open{from};
    seen[b.cell(from)] = true;
    std::size_t count = 0;
    while (!open.empty()) {
        const Pos at = open.front();
        open.pop_front();
        ++count;
        for (const std::int64_t d : kDirs) {
            const Pos n = ahead(at, d);
            if (b.inside(n) && !seen[b.cell(n)] && !blocked[b.cell(n)]) {
                seen[b.cell(n)] = true;
                open.push_back(n);
            }
        }
    }
    return count;
}

/// The snake after following `dirs`, growing by one on the step that reaches `food`.
inline std::vector<Pos> follow(std::vector<Pos> snake, const std::vector<std::int64_t>& dirs,
                               const Pos& food) {
    for (const std::int64_t d : dirs) {
        const Pos head = ahead(snake.front(), d);
        snake.insert(snake.begin(), head);
        if (!same(head, food)) {
            snake.pop_back();
        }
    }
    return snake;
}

/// Whether a snake can still reach its own tail: the one way it never shuts itself in.
inline bool tail_in_reach(const Board& b, const std::vector<Pos>& snake) {
    if (snake.size() < 2) {
        return true;
    }
    return !way(b, snake.front(), snake.back(), walls(b, snake, false)).empty();
}

} // namespace pilot_detail

/// The direction the pilot steers the world for its next step. A dead or empty world keeps its
/// direction; so does a snake with no step left that does not kill it.
inline std::int64_t pilot_choice(const SnakeVisual& v) {
    using namespace pilot_detail;
    if (!v.alive || v.snake.empty()) {
        return kRight;
    }
    const Board b{v.width, v.height};
    const std::vector<Pos>& snake = v.snake;
    const Pos head = snake.front();
    std::int64_t going = kRight;
    if (snake.size() > 1) {
        for (const std::int64_t d : kDirs) {
            if (same(ahead(snake[1], d), head)) {
                going = d;
            }
        }
    }

    // The shortest way to the food, taken only when the tail is in reach once it has eaten.
    const std::vector<std::int64_t> to_food = way(b, head, v.food, walls(b, snake, false));
    if (!to_food.empty() && tail_in_reach(b, follow(snake, to_food, v.food))) {
        return to_food.front();
    }

    // Else the step that keeps the tail in reach and leaves it farthest away: following its own
    // tail the long way round, so the body clears the food's way as it goes.
    std::int64_t best = -1;
    std::size_t best_far = 0;
    std::int64_t roomiest = -1;
    std::size_t most_room = 0;
    for (const std::int64_t d : kDirs) {
        const Pos n = ahead(head, d);
        const bool eats = same(n, v.food);
        if (!b.inside(n) || walls(b, snake, eats)[b.cell(n)]) {
            continue;
        }
        const std::vector<Pos> next = follow(snake, {d}, v.food);
        if (tail_in_reach(b, next)) {
            const std::size_t far =
                next.size() < 2
                    ? 0
                    : way(b, next.front(), next.back(), walls(b, next, false)).size();
            if (best < 0 || far > best_far) {
                best = d;
                best_far = far;
            }
        }
        const std::size_t r = room(b, n, walls(b, next, false));
        if (roomiest < 0 || r > most_room) {
            roomiest = d;
            most_room = r;
        }
    }
    if (best >= 0) {
        return best;
    }
    return roomiest >= 0 ? roomiest : going;
}

} // namespace zengine::snake

#endif // ZENGINE_SNAKE_PILOT_HPP
