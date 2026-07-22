#ifndef ZENGINE_SNAKE_LOGIC_HPP
#define ZENGINE_SNAKE_LOGIC_HPP

// The snake simulation as pure functions over the state structs — no bus, no
// weave, no I/O, no clock, no global randomness. Everything here is a
// deterministic function of its arguments, which is what lets the suite test
// the game (and the migration) as math, and lets the same functions serve both
// world versions (the templates bind to any state with the shared field set).
//
// The world weave is a thin shell around these; keeping them apart means the
// interesting logic never needs a bus to be proven.

#include "vocabulary.hpp"

#include <cstdint>

namespace zengine::snake {

/// What one step did — the world turns these into published messages.
struct StepEvents {
    bool moved = false;
    bool ate = false;
    bool died = false;
};

/// Food's "nowhere to spawn" sentinel (board full). Off-board on purpose; a
/// drawer simply finds no cell to mark. Practically unreachable in play, but
/// the functions are total, not hopeful.
inline constexpr std::int64_t kNoFood = -1;

namespace detail {

/// splitmix64 — a tiny, well-known mixer. Seeded entirely from state, so food
/// placement is a pure function of the world it lands in.
inline std::uint64_t mix(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

template <class State>
bool on_snake(const State& s, std::int64_t x, std::int64_t y) {
    for (const Pos& p : s.snake) {
        if (p.x == x && p.y == y) {
            return true;
        }
    }
    return false;
}

} // namespace detail

/// Drop food on a free cell, chosen deterministically from the state itself.
/// Board full → the off-board sentinel.
template <class State>
void spawn_food(State& s) {
    const std::int64_t cells = s.width * s.height;
    const std::int64_t occupied = static_cast<std::int64_t>(s.snake.size());
    const std::int64_t free_count = cells - occupied;
    if (free_count <= 0) {
        s.food = Pos{kNoFood, kNoFood};
        return;
    }
    std::uint64_t h = detail::mix(static_cast<std::uint64_t>(s.score) ^
                                  (static_cast<std::uint64_t>(s.snake.size()) << 20));
    if (!s.snake.empty()) {
        h = detail::mix(h ^ (static_cast<std::uint64_t>(s.snake.front().x) << 32) ^
                        static_cast<std::uint64_t>(s.snake.front().y));
    }
    h = detail::mix(h ^ (static_cast<std::uint64_t>(s.width) << 16) ^
                    static_cast<std::uint64_t>(s.height));
    std::int64_t nth = static_cast<std::int64_t>(h % static_cast<std::uint64_t>(free_count));
    for (std::int64_t y = 0; y < s.height; ++y) {
        for (std::int64_t x = 0; x < s.width; ++x) {
            if (detail::on_snake(s, x, y)) {
                continue;
            }
            if (nth == 0) {
                s.food = Pos{x, y};
                return;
            }
            --nth;
        }
    }
    s.food = Pos{kNoFood, kNoFood}; // unreachable: free_count > 0 finds a cell
}

/// New game on the state's own board size: a 3-long snake laid horizontally at
/// the center, heading right, food placed by spawn_food. Everything else keeps
/// the struct's defaults (alive, score untouched by design — the caller resets
/// the struct first if it wants a truly fresh game).
template <class State>
void seed(State& s) {
    const std::int64_t cx = s.width / 2;
    const std::int64_t cy = s.height / 2;
    s.snake = {Pos{cx, cy}, Pos{cx - 1, cy}, Pos{cx - 2, cy}};
    s.direction = kRight;
    s.alive = true;
    spawn_food(s);
}

/// Steer. A wish, not a command: ignored when dead or unseeded, and a straight
/// reversal (up→down, left→right) is ignored so the snake cannot eat its own
/// neck. Last wish before a tick wins.
template <class State>
void turn(State& s, std::int64_t dir) {
    if (!s.alive || s.snake.empty()) {
        return;
    }
    if (dir < kUp || dir > kLeft) {
        return;
    }
    if (s.snake.size() > 1 && ((s.direction + 2) % 4) == dir) {
        return;
    }
    s.direction = dir;
}

/// One step of world time. Precondition: alive with a seeded snake (the world
/// guards). Classical rules: the tail cell vacates in the same step the head
/// advances, so moving into where the tail *was* is legal — unless this step
/// eats (the tail stays, the snake grows).
template <class State>
StepEvents step(State& s) {
    StepEvents ev;
    if (!s.alive || s.snake.empty()) {
        return ev;
    }
    Pos head = s.snake.front();
    switch (s.direction) {
    case kUp: --head.y; break;
    case kRight: ++head.x; break;
    case kDown: ++head.y; break;
    case kLeft: --head.x; break;
    default: return ev;
    }
    if (head.x < 0 || head.x >= s.width || head.y < 0 || head.y >= s.height) {
        s.alive = false;
        ev.died = true;
        return ev;
    }
    const bool eats = (head.x == s.food.x && head.y == s.food.y);
    const std::size_t solid = s.snake.size() - (eats ? 0 : 1); // tail vacates unless growing
    for (std::size_t i = 0; i < solid; ++i) {
        if (s.snake[i].x == head.x && s.snake[i].y == head.y) {
            s.alive = false;
            ev.died = true;
            return ev;
        }
    }
    s.snake.insert(s.snake.begin(), head);
    if (eats) {
        ++s.score;
        ev.ate = true;
        spawn_food(s);
    } else {
        s.snake.pop_back();
    }
    ev.moved = true;
    return ev;
}

/// Project the world's state onto the drawer contract. SnakeVisual stays v1
/// whatever the state's version — the drawer never feels the world evolve.
template <class State>
SnakeVisual visual_of(const State& s) {
    SnakeVisual v;
    v.width = s.width;
    v.height = s.height;
    v.snake = s.snake;
    v.food = s.food;
    v.alive = s.alive;
    v.score = s.score;
    return v;
}

/// THE MIGRATION, v1 → v2 — explicit, total, and pure, exactly so it can be
/// pinned as math. The old board is laid centered inside the new one and the
/// whole scene moves as a rigid body: every segment and the food translate by
/// the same (dx, dy), so the snake's pose — its shape, its relation to the
/// food, its heading — is preserved exactly; the world simply grows around it.
/// Score, direction, and aliveness carry; `growths` records that one growth
/// has happened. (A dead world migrates dead — continuity is honest, not
/// cosmetic.) The off-board food sentinel is preserved, not translated.
inline v2::SnakeWorldState migrate(const v1::SnakeWorldState& old) {
    v2::SnakeWorldState next; // v2 defaults carry the new, larger board
    const std::int64_t dx = (next.width - old.width) / 2;
    const std::int64_t dy = (next.height - old.height) / 2;
    next.snake.reserve(old.snake.size());
    for (const Pos& p : old.snake) {
        next.snake.push_back(Pos{p.x + dx, p.y + dy});
    }
    if (old.food.x == kNoFood) {
        next.food = old.food;
    } else {
        next.food = Pos{old.food.x + dx, old.food.y + dy};
    }
    next.direction = old.direction;
    next.alive = old.alive;
    next.score = old.score;
    next.growths = 1;
    return next;
}

} // namespace zengine::snake

#endif // ZENGINE_SNAKE_LOGIC_HPP
