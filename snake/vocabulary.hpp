#ifndef ZENGINE_SNAKE_VOCABULARY_HPP
#define ZENGINE_SNAKE_VOCABULARY_HPP

// The snake package's message vocabulary — the whole contract in one file, so
// there is exactly one place to diff against the locked spellings.
//
// THE LOCKED CONTRACT (Vision chat; quoted in the Stage 2 phase prompt) is five
// shapes: Pos v1, SnakeVisual v1, FoodEaten v1, SnakeDied v1, SnakeWorldState v1.
// They are spelled here as ZEN_SHAPE structs whose derived schemas are
// field-for-field identical — same names, same order, same kinds — to the
// contract's SchemaBuilder spellings; the suite pins that identity by
// content-id, so a drift between this file and the contract is a red test, not
// an opinion.
//
// NAMED ADDITIONS (the contract proved insufficient here; recorded face-up per
// the phase's report-back rule, never silently):
//   - SnakeTick v1, SnakeTurn v1 — the contract has no time or input vocabulary
//     at all, and a world nobody can tick or steer is not playable. Both are
//     world-owned shapes: the world accepts them; who produces them (a host
//     loop today, an input weave later) is deliberately unspecified.
//   - SnakeWorldState v2 — the prompt says migration *will introduce* v2 but
//     does not lock its fields. v2 = v1 + `growths` (how many map-growths this
//     world has lived through), so the version change is a real shape change,
//     not a bare version bump.
//
// Ownership (locked): the World emits SnakeVisual/FoodEaten/SnakeDied and holds
// SnakeWorldState; a Drawer accepts only SnakeVisual; a Score weave accepts
// FoodEaten. SnakeVisual deliberately stays v1 across the world's v1→v2 state
// migration — the drawer contract does not feel the world grow, which is the
// point of the visual/world split.

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

/// The world's own state, exactly as locked. An EMPTY `snake` means "new game
/// pending": the world seeds itself on the next tick (which is also what makes
/// a poke-reset — default-constructed state — mean "new game" for free).
///
/// ZEN_EXPOSE(): every field is poke-manipulable. Deliberate and in the open —
/// this state holds no secrets, live manipulation is the point of the
/// substrate, and the suite uses that hand (placing food, forcing state) as its
/// game-master. The no-secret-state floor is satisfied trivially.
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

/// The two role slots this package binds. Roles are the addresses that survive
/// replacement — the whole phase is three demonstrations of exactly that.
inline constexpr const char* kWorldRole = "snake.world";
inline constexpr const char* kDrawerRole = "snake.drawer";

} // namespace zengine::snake

#endif // ZENGINE_SNAKE_VOCABULARY_HPP
