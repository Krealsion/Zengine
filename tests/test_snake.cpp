// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The snake suite — the Stage 2 vertical slice, proven headless.
//
// Three tiers, deliberately ordered:
//   1. CONTRACT pins — the ZEN_SHAPE spellings in vocabulary.hpp derive schemas
//      content-id-identical to the locked contract's SchemaBuilder spellings.
//      A drift between the code and the contract is a red test, not an opinion.
//   2. LOGIC pins — the simulation and THE MIGRATION as pure math (no bus).
//   3. THE THREE MOMENTS, end to end — real .so libraries through the real
//      Kernel, orchestrated by the real Weave Manager over the real bus:
//      drawing replaced mid-game (since the Surface migration that means the
//      SKIN — the painting code still leaves the process and different code
//      takes the surface), a score weave arriving late, and the world growing
//      v1 → v2 through the letter. Plus the doors that must NOT open: reload
//      across a state-schema version change refuses cleanly. And the phase's
//      own negative space: a skinless game writes ZERO bytes to stdout —
//      snake publishes intent, it does not paint (with a painted-bytes
//      negative control so the zero is a measurement, not a broken meter).
//
// The e2e tier steers the real world by LOCKSTEP: the test runs its own local
// State through the same pure logic.hpp functions and asserts the published
// SnakeVisual equals its local projection after every tick — so determinism
// is not assumed by the steering, it is pinned by it.
//
// The real skin .so's paint stdout when frames arrive; the suite redirects
// fd 1 around each pump so the proof stays readable. Their participation is
// asserted through their own poked frame counters, not their pixels (the
// pixels' own suite is test_surface.cpp, golden bytes and all).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "logic.hpp"
#include "vocabulary.hpp"

#include "surface/vocabulary.hpp" // the skin role: drawing's address since the migration

#include <fstream>
#include <iterator>

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using namespace zengine::snake;
using loom::schema_of;

namespace {

// ---- tier 3 rig -------------------------------------------------------------

/// What the recorder has seen. Owned by the test; the probe weave writes here.
struct Recorded {
    struct Answer {
        std::uint64_t corr = 0;
        int kind = 0; // 0 Result, 1 Ack, 2 Refused
        std::string text;
        std::uint64_t sender = 0;
    };
    std::vector<Answer> answers;
    std::vector<SnakeVisual> visuals;
    int eaten = 0;
    int died = 0;

    const Answer* find(std::uint64_t corr) const {
        for (const Answer& a : answers) {
            if (a.corr == corr) {
                return &a;
            }
        }
        return nullptr;
    }
};

struct ProbeState {
    std::int64_t noted = 0;
    ZEN_SHAPE(ProbeState, 1, ZEN_FIELD(noted));
};

/// The test's hand and ears on the bus: it holds the manager-reach grant (the
/// commands are sent AS it), hears every standard-shape answer, and — as an
/// accepter of the published gameplay shapes — witnesses the same traffic any
/// drawer would.
class Probe : public loom::WeaveBase<
                  Probe, ProbeState,
                  loom::Accept<loom::Result, loom::Ack, loom::Refused, SnakeVisual, FoodEaten,
                               SnakeDied>,
                  loom::Emit<>> {
public:
    explicit Probe(Recorded& rec) : rec_(&rec) {}

    void on(const loom::Result& r, loom::Mail& mail) { note(mail, 0, r.value); }
    void on(const loom::Ack&, loom::Mail& mail) { note(mail, 1, ""); }
    void on(const loom::Refused& r, loom::Mail& mail) { note(mail, 2, r.reason); }
    void on(const SnakeVisual& v, loom::Mail&) { rec_->visuals.push_back(v); }
    void on(const FoodEaten&, loom::Mail&) { ++rec_->eaten; }
    void on(const SnakeDied&, loom::Mail&) { ++rec_->died; }

private:
    void note(loom::Mail& mail, int kind, std::string text) {
        ++state_.noted;
        rec_->answers.push_back(
            Recorded::Answer{mail.correlation(), kind, std::move(text), mail.sender().value});
    }
    Recorded* rec_;
};

/// fd-1 silencer: the real drawers paint stdout; the suite's own output must
/// stay legible. Scoped exactly around pumps. The null device and the dup
/// spellings are the only platform seam (Windows: `NUL`, the _-prefixed CRT
/// forms).
class Hush {
public:
    Hush() {
        std::fflush(stdout);
#if defined(_WIN32)
        saved_ = ::_dup(1);
        const int nul = ::_open("NUL", _O_WRONLY);
        if (nul >= 0) {
            ::_dup2(nul, 1);
            ::_close(nul);
        }
#else
        saved_ = ::dup(STDOUT_FILENO);
        const int nul = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (nul >= 0) {
            ::dup2(nul, STDOUT_FILENO);
            ::close(nul);
        }
#endif
    }
    ~Hush() {
        std::fflush(stdout);
        if (saved_ >= 0) {
#if defined(_WIN32)
            ::_dup2(saved_, 1);
            ::_close(saved_);
#else
            ::dup2(saved_, STDOUT_FILENO);
            ::close(saved_);
#endif
        }
    }
    Hush(const Hush&) = delete;
    Hush& operator=(const Hush&) = delete;

private:
    int saved_ = -1;
};

struct Rig {
    loom::Switchboard bus;
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    Recorded rec;
    loom::WeaveId probe = mount_probe();
    std::uint64_t next_corr = 1;

    loom::WeaveId mount_probe() {
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
        reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
        reach.allow(loom::ListLoaded::zen_name, loom::ListLoaded::zen_version, manager);
        return loom::mount_granted<Probe>(bus, std::move(reach), rec);
    }

    void pump() {
        Hush hush;
        bus.pump();
    }

    /// Fire a lifecycle command AS the probe (its grant is the authority) and
    /// pump until the answer lands. Returns the answer BY VALUE — the recorder's
    /// vector grows with every answer, so a reference into it would dangle at
    /// the next command (the exact dangling-into-a-grown-vector ASan caught in
    /// the UI-Builder phase; not twice).
    template <class Cmd>
    Recorded::Answer command(const Cmd& cmd) {
        const std::uint64_t corr = next_corr++;
        bus.send_as(probe, manager, loom::Message(loom::to_value(cmd), probe, probe, corr));
        pump();
        const Recorded::Answer* a = rec.find(corr);
        REQUIRE(a != nullptr);
        return *a;
    }

    /// Root-send a poke to a target with the probe as the answer address.
    /// By value, same reason.
    template <class Poke>
    Recorded::Answer poke(loom::WeaveId target, const Poke& p) {
        const std::uint64_t corr = next_corr++;
        bus.send(target, loom::Message(loom::to_value(p), loom::WeaveId{}, probe, corr));
        pump();
        const Recorded::Answer* a = rec.find(corr);
        REQUIRE(a != nullptr);
        return *a;
    }

    void tick() {
        bus.send_to_role(kWorldRole, loom::Message(loom::to_value(SnakeTick{})));
        pump();
    }

    void turn_real(std::int64_t dir) {
        bus.send_to_role(kWorldRole, loom::Message(loom::to_value(SnakeTurn{dir})));
    }

    loom::WeaveId load(const char* name, const char* path, const char* role) {
        const Recorded::Answer a = command(loom::LoadWeave{name, path, role});
        REQUIRE_MESSAGE(a.kind == 0, "load refused: ", a.text);
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(a.text))};
    }
};

bool same_pos(const Pos& a, const Pos& b) { return a.x == b.x && a.y == b.y; }

bool same_visual(const SnakeVisual& a, const SnakeVisual& b) {
    if (a.width != b.width || a.height != b.height || a.alive != b.alive || a.score != b.score ||
        !same_pos(a.food, b.food) || a.snake.size() != b.snake.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.snake.size(); ++i) {
        if (!same_pos(a.snake[i], b.snake[i])) {
            return false;
        }
    }
    return true;
}

/// The lockstep pilot: choose a direction that closes on the food, never a
/// reversal, never a wall. (Greedy is enough for the handful of foods the
/// suite eats; the lockstep equality would fail loudly if it ever weren't.)
template <class State>
std::int64_t steer_toward_food(const State& s) {
    const Pos h = s.snake.front();
    const auto viable = [&](std::int64_t d) {
        if (s.snake.size() > 1 && ((s.direction + 2) % 4) == d) {
            return false; // reversal: the world would ignore it
        }
        Pos n = h;
        switch (d) {
        case kUp: --n.y; break;
        case kRight: ++n.x; break;
        case kDown: ++n.y; break;
        default: --n.x; break;
        }
        return n.x >= 0 && n.x < s.width && n.y >= 0 && n.y < s.height;
    };
    if (s.food.x > h.x && viable(kRight)) {
        return kRight;
    }
    if (s.food.x < h.x && viable(kLeft)) {
        return kLeft;
    }
    if (s.food.y > h.y && viable(kDown)) {
        return kDown;
    }
    if (s.food.y < h.y && viable(kUp)) {
        return kUp;
    }
    for (const std::int64_t d : {kUp, kRight, kDown, kLeft}) {
        if (viable(d)) {
            return d;
        }
    }
    return s.direction;
}

/// Drive the REAL world and a LOCAL mirror in lockstep until the mirror eats,
/// asserting the published visual equals the local projection every tick.
/// Returns how many ticks it took.
template <class State>
int eat_one_food(Rig& r, State& local, int max_ticks = 400) {
    for (int t = 0; t < max_ticks; ++t) {
        const std::int64_t want = steer_toward_food(local);
        if (want != local.direction) {
            r.turn_real(want);
            turn(local, want);
        }
        r.tick();
        const StepEvents ev = step(local);
        REQUIRE(local.alive);
        REQUIRE(!r.rec.visuals.empty());
        REQUIRE(same_visual(r.rec.visuals.back(), visual_of(local)));
        if (ev.ate) {
            return t + 1;
        }
    }
    FAIL("lockstep pilot failed to reach food within the tick budget");
    return -1;
}

} // namespace

// ============================================================================
// Tier 1 — the locked contract, pinned by content-id
// ============================================================================

TEST_CASE("contract: ZEN_SHAPE spellings derive the locked schemas exactly") {
    using loom::Kind;
    using loom::SchemaBuilder;
    const auto pos = SchemaBuilder("Pos", 1)
                         .field("x", Kind::Int)
                         .field("y", Kind::Int)
                         .build();
    CHECK(schema_of<Pos>()->content_id() == pos->content_id());

    const auto visual = SchemaBuilder("SnakeVisual", 1)
                            .field("width", Kind::Int)
                            .field("height", Kind::Int)
                            .list("snake", loom::type_message(pos))
                            .message("food", pos)
                            .field("alive", Kind::Bool)
                            .field("score", Kind::Int)
                            .build();
    CHECK(schema_of<SnakeVisual>()->content_id() == visual->content_id());

    CHECK(schema_of<FoodEaten>()->content_id() ==
          SchemaBuilder("FoodEaten", 1).build()->content_id());
    CHECK(schema_of<SnakeDied>()->content_id() ==
          SchemaBuilder("SnakeDied", 1).build()->content_id());

    const auto world = SchemaBuilder("SnakeWorldState", 1)
                           .field("width", Kind::Int)
                           .field("height", Kind::Int)
                           .list("snake", loom::type_message(pos))
                           .message("food", pos)
                           .field("direction", Kind::Int)
                           .field("alive", Kind::Bool)
                           .field("score", Kind::Int)
                           .build();
    CHECK(schema_of<v1::SnakeWorldState>()->content_id() == world->content_id());
}

TEST_CASE("contract: v2 state is a genuinely different identity") {
    CHECK(schema_of<v1::SnakeWorldState>()->content_id() !=
          schema_of<v2::SnakeWorldState>()->content_id());
    CHECK(schema_of<v1::SnakeWorldState>()->name() == schema_of<v2::SnakeWorldState>()->name());
    CHECK(schema_of<v1::SnakeWorldState>()->version() == 1);
    CHECK(schema_of<v2::SnakeWorldState>()->version() == 2);
}

// ============================================================================
// Tier 2 — the simulation as math
// ============================================================================

TEST_CASE("logic: seeding centers a 3-long snake heading right, food on a free cell") {
    v1::SnakeWorldState s;
    seed(s);
    REQUIRE(s.snake.size() == 3);
    CHECK(same_pos(s.snake[0], Pos{12, 8}));
    CHECK(same_pos(s.snake[1], Pos{11, 8}));
    CHECK(same_pos(s.snake[2], Pos{10, 8}));
    CHECK(s.direction == kRight);
    CHECK(s.alive);
    CHECK(s.food.x >= 0);
    CHECK(s.food.x < s.width);
    CHECK(s.food.y >= 0);
    CHECK(s.food.y < s.height);
    for (const Pos& p : s.snake) {
        CHECK(!same_pos(p, s.food));
    }
}

TEST_CASE("logic: food placement is a pure function of the state") {
    v1::SnakeWorldState a;
    seed(a);
    v1::SnakeWorldState b;
    seed(b);
    CHECK(same_pos(a.food, b.food));
}

TEST_CASE("logic: a step moves the head and drags the tail") {
    v1::SnakeWorldState s;
    seed(s);
    s.food = Pos{0, 0}; // out of the way
    const StepEvents ev = step(s);
    CHECK(ev.moved);
    CHECK(!ev.ate);
    CHECK(!ev.died);
    REQUIRE(s.snake.size() == 3);
    CHECK(same_pos(s.snake[0], Pos{13, 8}));
    CHECK(same_pos(s.snake[1], Pos{12, 8}));
    CHECK(same_pos(s.snake[2], Pos{11, 8}));
}

TEST_CASE("logic: turns steer, reversals and dead worlds ignore them") {
    v1::SnakeWorldState s;
    seed(s);
    turn(s, kLeft); // straight reversal of kRight: ignored
    CHECK(s.direction == kRight);
    turn(s, kUp);
    CHECK(s.direction == kUp);
    turn(s, kDown); // reversal of kUp: ignored
    CHECK(s.direction == kUp);
    turn(s, static_cast<std::int64_t>(9)); // nonsense: ignored
    CHECK(s.direction == kUp);
    s.alive = false;
    turn(s, kRight);
    CHECK(s.direction == kUp);
}

TEST_CASE("logic: eating grows, scores, and respawns food off the snake") {
    v1::SnakeWorldState s;
    seed(s);
    s.food = Pos{13, 8}; // directly ahead
    const StepEvents ev = step(s);
    CHECK(ev.ate);
    CHECK(ev.moved);
    CHECK(s.score == 1);
    REQUIRE(s.snake.size() == 4); // grew: tail stayed
    CHECK(same_pos(s.snake[0], Pos{13, 8}));
    CHECK(same_pos(s.snake[3], Pos{10, 8}));
    CHECK(!same_pos(s.food, Pos{13, 8})); // respawned elsewhere
    for (const Pos& p : s.snake) {
        CHECK(!same_pos(p, s.food));
    }
}

TEST_CASE("logic: walls kill; the corpse stays where it stood") {
    v1::SnakeWorldState s;
    seed(s);
    s.snake = {Pos{23, 8}, Pos{22, 8}, Pos{21, 8}};
    s.food = Pos{0, 0};
    const StepEvents ev = step(s);
    CHECK(ev.died);
    CHECK(!ev.moved);
    CHECK(!s.alive);
    CHECK(same_pos(s.snake[0], Pos{23, 8}));
    // …and a dead world's steps are no-ops, not crashes.
    const StepEvents again = step(s);
    CHECK(!again.moved);
    CHECK(!again.died);
}

TEST_CASE("logic: the tail cell vacates - unless this step eats it solid") {
    // A 2×2 loop of snake: head may chase its own tail forever…
    v1::SnakeWorldState s;
    s.snake = {Pos{2, 2}, Pos{3, 2}, Pos{3, 3}, Pos{2, 3}};
    s.direction = kDown; // head (2,2) → (2,3), the tail cell
    s.food = Pos{9, 9};
    StepEvents ev = step(s);
    CHECK(ev.moved);
    CHECK(!ev.died);
    CHECK(same_pos(s.snake[0], Pos{2, 3}));

    // …but if the tail cell holds FOOD, the tail does not vacate, and biting
    // it is death.
    v1::SnakeWorldState t;
    t.snake = {Pos{2, 2}, Pos{3, 2}, Pos{3, 3}, Pos{2, 3}};
    t.direction = kDown;
    t.food = Pos{2, 3};
    ev = step(t);
    CHECK(ev.died);
    CHECK(!t.alive);
}

TEST_CASE("logic: self-collision kills") {
    v1::SnakeWorldState s;
    s.snake = {Pos{5, 5}, Pos{5, 6}, Pos{6, 6}, Pos{6, 5}, Pos{7, 5}};
    s.direction = kRight; // head (5,5) → (6,5): body, and not the tail
    s.food = Pos{0, 0};
    const StepEvents ev = step(s);
    CHECK(ev.died);
    CHECK(!s.alive);
}

TEST_CASE("logic: a full board yields the off-board food sentinel") {
    v1::SnakeWorldState s;
    s.width = 2;
    s.height = 2;
    s.snake = {Pos{0, 0}, Pos{1, 0}, Pos{1, 1}, Pos{0, 1}};
    spawn_food(s);
    CHECK(s.food.x == kNoFood);
    CHECK(s.food.y == kNoFood);
}

// ============================================================================
// Tier 2b — THE MIGRATION as math
// ============================================================================

TEST_CASE("migration: the old board sits centered in the new; the scene moves rigidly") {
    v1::SnakeWorldState old;
    seed(old);
    old.food = Pos{13, 8};
    (void)step(old); // eat: length 4, score 1
    turn(old, kUp);
    (void)step(old); // head (13,7)
    REQUIRE(old.snake.size() == 4);
    REQUIRE(old.score == 1);
    REQUIRE(old.direction == kUp);

    const v2::SnakeWorldState next = migrate(old);
    CHECK(next.width == 48);
    CHECK(next.height == 24);
    const std::int64_t dx = (48 - 24) / 2; // 12
    const std::int64_t dy = (24 - 16) / 2; // 4
    REQUIRE(next.snake.size() == old.snake.size());
    for (std::size_t i = 0; i < old.snake.size(); ++i) {
        CHECK(next.snake[i].x == old.snake[i].x + dx);
        CHECK(next.snake[i].y == old.snake[i].y + dy);
    }
    CHECK(next.food.x == old.food.x + dx);
    CHECK(next.food.y == old.food.y + dy);
    CHECK(next.score == old.score);
    CHECK(next.direction == old.direction);
    CHECK(next.alive == old.alive);
    CHECK(next.growths == 1);

    // Rigid: every segment-to-segment offset is preserved exactly.
    for (std::size_t i = 1; i < old.snake.size(); ++i) {
        CHECK(next.snake[i].x - next.snake[i - 1].x == old.snake[i].x - old.snake[i - 1].x);
        CHECK(next.snake[i].y - next.snake[i - 1].y == old.snake[i].y - old.snake[i - 1].y);
    }
}

TEST_CASE("migration: a dead world migrates dead, and the food sentinel is preserved") {
    v1::SnakeWorldState old;
    seed(old);
    old.alive = false;
    old.food = Pos{kNoFood, kNoFood};
    const v2::SnakeWorldState next = migrate(old);
    CHECK(!next.alive);
    CHECK(next.food.x == kNoFood);
    CHECK(next.food.y == kNoFood);
}

// ============================================================================
// Tier 3 — the three moments, end to end (real .so, real kernel, real steward)
// ============================================================================

TEST_CASE("moment 1: the drawing code is replaced while the game continues") {
    Rig r;
    r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);
    const loom::WeaveId classic = r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC,
                                         zengine::surface::kSkinRole);

    r.tick(); // seeds, and both the classic skin and the probe hear the frame
    v1::SnakeWorldState local;
    seed(local);
    CHECK(r.poke(classic, loom::PokeRead{"frames"}).text == "1");
    REQUIRE(r.rec.visuals.size() == 1);
    REQUIRE(same_visual(r.rec.visuals.back(), visual_of(local)));

    // The swap: hard, mid-game. The classic skin is unloaded (its code leaves
    // the process, its claim on the surface released in its destructor);
    // different painting code takes the surface.
    const Recorded::Answer swapped =
        r.command(loom::SwapWeave{zengine::surface::kSkinRole, "zengine-skin-tui-block",
                                  SKIN_SO_TUI_BLOCK, /*graceful=*/false});
    REQUIRE(swapped.kind == 0);
    const loom::WeaveId block{static_cast<std::uint64_t>(std::stoll(swapped.text))};
    CHECK(block.value != classic.value);

    const Recorded::Answer listed = r.command(loom::ListLoaded{});
    CHECK(listed.text.find("zengine-skin-tui-block@zengine.skin") != std::string::npos);
    CHECK(listed.text.find("classic") == std::string::npos);
    CHECK(listed.text.find("snake-world-v1@snake.world") != std::string::npos);

    // The world never stopped: the next tick advances the SAME run (the
    // lockstep mirror is the proof — whatever this tick did, eat or move, the
    // world did it too), and the NEW skin receives the frame.
    r.tick();
    (void)step(local);
    CHECK(r.poke(block, loom::PokeRead{"frames"}).text == "1");
    REQUIRE(r.rec.visuals.size() == 2);
    CHECK(same_visual(r.rec.visuals.back(), visual_of(local)));
}

TEST_CASE("moment 2: a score weave loaded late counts only what it witnesses") {
    Rig r;
    r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);
    r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, zengine::surface::kSkinRole);

    r.tick(); // seed
    v1::SnakeWorldState local;
    seed(local);
    REQUIRE(same_visual(r.rec.visuals.back(), visual_of(local)));

    // First food: eaten with NO score weave anywhere in the process.
    eat_one_food(r, local);
    CHECK(r.rec.eaten == 1);
    CHECK(local.score == 1);
    const Recorded::Answer before = r.command(loom::ListLoaded{});
    CHECK(before.text.find("snake-score") == std::string::npos);

    // The late arrival, into a live game.
    const loom::WeaveId score = r.load("snake-score", SNAKE_SO_SCORE, "");
    CHECK(r.poke(score, loom::PokeRead{"eaten"}).text == "0");

    // Second food: now it is witnessed.
    eat_one_food(r, local);
    CHECK(r.rec.eaten == 2);
    CHECK(local.score == 2);
    CHECK(r.poke(score, loom::PokeRead{"eaten"}).text == "1"); // joined late, counted once
}

TEST_CASE("moment 3: the world grows - graceful swap, the letter, the migration") {
    Rig r;
    const loom::WeaveId world_v1 = r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);
    r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, zengine::surface::kSkinRole);

    r.tick();
    v1::SnakeWorldState local;
    seed(local);
    eat_one_food(r, local); // some life worth migrating: length 4, score 1
    const SnakeVisual before = r.rec.visuals.back();
    REQUIRE(before.width == 24);
    REQUIRE(before.height == 16);
    REQUIRE(before.score == 1);
    REQUIRE(before.snake.size() == 4);

    // The ceremony: ask the incumbent for its letter, replace it, let the heir
    // claim. One command from the operator's seat.
    const Recorded::Answer grown = r.command(
        loom::SwapWeave{kWorldRole, "snake-world-v2", SNAKE_SO_WORLD_V2, /*graceful=*/true});
    REQUIRE(grown.kind == 0);
    const loom::WeaveId world_v2{static_cast<std::uint64_t>(std::stoll(grown.text))};
    CHECK(world_v2.value != world_v1.value);
    const Recorded::Answer listed = r.command(loom::ListLoaded{});
    CHECK(listed.text.find("snake-world-v2@snake.world") != std::string::npos);
    CHECK(listed.text.find("snake-world-v1") == std::string::npos);

    // First tick of the heir: it claims by role, the gate detects the v1 shape,
    // migrate() runs, and the published visual is the OLD scene translated
    // rigidly onto the larger board.
    r.tick();
    const SnakeVisual after = r.rec.visuals.back();
    CHECK(after.width == 48);
    CHECK(after.height == 24);
    const std::int64_t dx = 12;
    const std::int64_t dy = 4;
    CHECK(after.score == before.score);
    REQUIRE(after.snake.size() == before.snake.size());
    for (std::size_t i = 0; i < before.snake.size(); ++i) {
        CHECK(after.snake[i].x == before.snake[i].x + dx);
        CHECK(after.snake[i].y == before.snake[i].y + dy);
    }
    CHECK(after.food.x == before.food.x + dx);
    CHECK(after.food.y == before.food.y + dy);
    CHECK(r.poke(world_v2, loom::PokeRead{"growths"}).text == "1");

    // Continuity: the next tick STEPS the migrated snake — same run, larger
    // world. (Loose on score/size on purpose: the tick may legitimately eat.)
    r.tick();
    const SnakeVisual stepped = r.rec.visuals.back();
    CHECK(stepped.snake.size() >= after.snake.size());
    CHECK(!same_pos(stepped.snake.front(), after.snake.front()));
    CHECK(stepped.score >= after.score);
    CHECK(stepped.alive);

    // And a v2→v2 graceful swap is same-shape succession: the heir ADOPTS the
    // letter whole (no migration, growths unchanged).
    const SnakeVisual pre_second = r.rec.visuals.back();
    const Recorded::Answer second = r.command(
        loom::SwapWeave{kWorldRole, "snake-world-v2", SNAKE_SO_WORLD_V2, /*graceful=*/true});
    REQUIRE(second.kind == 0);
    r.tick(); // heir seeds fresh, then folds the adopted state (last visual wins)
    CHECK(same_visual(r.rec.visuals.back(), pre_second));
    const loom::WeaveId heir{static_cast<std::uint64_t>(std::stoll(second.text))};
    CHECK(r.poke(heir, loom::PokeRead{"growths"}).text == "1");
}

TEST_CASE("snapshot/revive: an in-place reload carries the state through the gate") {
    Rig r;
    r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);
    r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, zengine::surface::kSkinRole);
    r.tick();
    v1::SnakeWorldState local;
    seed(local);
    eat_one_food(r, local);
    const SnakeVisual before = r.rec.visuals.back();

    const Recorded::Answer reloaded =
        r.command(loom::ReloadWeave{"snake-world-v1", SNAKE_SO_WORLD_V1});
    CHECK(reloaded.kind == 1); // Ack

    // The state rode the snapshot: the next tick continues the same run.
    r.tick();
    (void)step(local);
    const SnakeVisual after = r.rec.visuals.back();
    CHECK(same_visual(after, visual_of(local)));
    CHECK(after.score == before.score);
    CHECK(after.snake.size() == before.snake.size());
}

TEST_CASE("the closed door: reload across a state-schema version change refuses cleanly") {
    Rig r;
    r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);
    r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, zengine::surface::kSkinRole);
    r.tick();
    v1::SnakeWorldState local;
    seed(local);

    // Reload-in-place toward the v2 library: refused, with the reason — reload
    // transplants across the SAME shape; growing is the letter's job.
    const Recorded::Answer refused =
        r.command(loom::ReloadWeave{"snake-world-v1", SNAKE_SO_WORLD_V2});
    CHECK(refused.kind == 2);
    CHECK(refused.text.find("state schema version mismatch") != std::string::npos);

    // The old library keeps running — the refusal cost the game nothing.
    r.tick();
    (void)step(local);
    CHECK(same_visual(r.rec.visuals.back(), visual_of(local)));
}

// (The -fno-gnu-unique linkage pins that used to ride the drawer pair moved to
// the Surface suite with the drawers' successors: the skins are now the weave
// libraries sharing vocabulary headers across every load-unload-load shape.)

TEST_CASE("the substrate's own reset door starts a new game") {
    Rig r;
    const loom::WeaveId world = r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);
    r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, zengine::surface::kSkinRole);
    r.tick();
    v1::SnakeWorldState local;
    seed(local);
    eat_one_food(r, local);
    REQUIRE(r.rec.visuals.back().score == 1);

    CHECK(r.poke(world, loom::PokeResetState{}).kind == 1); // Ack: every field is exposed
    r.tick();                                               // empty snake = new game: reseeds
    const SnakeVisual fresh = r.rec.visuals.back();
    CHECK(fresh.score == 0);
    CHECK(fresh.snake.size() == 3);
    CHECK(fresh.width == 24);
    CHECK(fresh.alive);
}

// ============================================================================
// The phase's negative space: snake publishes, it does not paint
// ============================================================================

namespace {

/// fd-1 catcher: like Hush, but the bytes land in a file the test can weigh.
/// The zero it measures is only a measurement because the same catcher then
/// weighs a SKINNED run and finds paint — a meter proven live, not assumed.
class CatchStdout {
public:
    explicit CatchStdout(const char* path) {
        std::fflush(stdout);
#if defined(_WIN32)
        saved_ = ::_dup(1);
        const int fd = ::_open(path, _O_WRONLY | _O_CREAT | _O_TRUNC, 0600);
        if (fd >= 0) {
            ::_dup2(fd, 1);
            ::_close(fd);
        }
#else
        saved_ = ::dup(STDOUT_FILENO);
        const int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
        if (fd >= 0) {
            ::dup2(fd, STDOUT_FILENO);
            ::close(fd);
        }
#endif
    }
    ~CatchStdout() {
        std::fflush(stdout);
        if (saved_ >= 0) {
#if defined(_WIN32)
            ::_dup2(saved_, 1);
            ::_close(saved_);
#else
            ::dup2(saved_, STDOUT_FILENO);
            ::close(saved_);
#endif
        }
    }
    CatchStdout(const CatchStdout&) = delete;
    CatchStdout& operator=(const CatchStdout&) = delete;

private:
    int saved_ = -1;
};

long file_size_then_remove(const char* path) {
    long size = -1;
    if (std::FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END);
        size = std::ftell(f);
        std::fclose(f);
    }
    std::remove(path);
    return size;
}

} // namespace

TEST_CASE("snake publishes, never paints: a skinless game writes zero bytes to stdout") {
    Rig r;
    const loom::WeaveId score = r.load("snake-score", SNAKE_SO_SCORE, "");
    r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);

    // A stretch of real life with NO skin anywhere in the process: ticks,
    // a witnessed meal, a spoken tally — every one of them intent on the bus,
    // none of them bytes on stdout. Raw pumps on purpose: the catcher, not
    // the Hush, must be the observer.
    {
        CatchStdout catcher("snake_no_paint.tmp");
        for (int i = 0; i < 6; ++i) {
            r.bus.send_to_role(kWorldRole, loom::Message(loom::to_value(SnakeTick{})));
            r.bus.pump();
        }
        r.bus.send(score, loom::Message(loom::to_value(FoodEaten{})));
        r.bus.pump();
    }
    CHECK(file_size_then_remove("snake_no_paint.tmp") == 0);
    CHECK(r.poke(score, loom::PokeRead{"eaten"}).text == "1"); // the game DID run

    // The negative control: the same meter, a skin on the surface — paint.
    r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, zengine::surface::kSkinRole);
    {
        CatchStdout catcher("snake_painted.tmp");
        r.bus.send_to_role(kWorldRole, loom::Message(loom::to_value(SnakeTick{})));
        r.bus.pump();
    }
    CHECK(file_size_then_remove("snake_painted.tmp") > 0);
}

// ============================================================================
// R2A-2 — the host holds no privileged wind
// ============================================================================

TEST_CASE("the playable host sends no Drive: time is the composition's, not the host's") {
    // WHY THIS READS THE SOURCE, said plainly. Since R2A-2 a root Drive is
    // INERT — it carries no activation key, so the service ignores it — which
    // means a wind left behind in the host cannot be caught by behaviour: the
    // game would run identically with one. The claim at risk is therefore not
    // "the game works" but "the host contributes nothing to time", and the only
    // thing that can guard it is the file itself.
    //
    // The precedent is the console's geometry-name tripwire, and so is the
    // honesty about what it is: defense in depth against a claim quietly
    // becoming false again, NOT a proof of unrepresentability. Someone
    // determined can still write a wind by another spelling; nobody will do it
    // by accident.
    std::ifstream host(HOST_PLAY_CPP);
    REQUIRE_MESSAGE(host.is_open(), "cannot read the host source: " HOST_PLAY_CPP);
    const std::string source((std::istreambuf_iterator<char>(host)),
                             std::istreambuf_iterator<char>());
    REQUIRE(source.size() > 1000); // we really did read the host, not an empty path

    // No Drive is constructed, sent, or named anywhere in the playable host.
    CHECK(source.find("Drive") == std::string::npos);
    // And the timer vocabulary is still reached for — the host names the role
    // it loads the service into — so the absence above is about the WIND, not
    // about the host having stopped speaking timer at all.
    CHECK(source.find("timer::kTimerRole") != std::string::npos);
}

TEST_CASE("the clock adapter uses the timer binding, not hand-written ceremony") {
    // WHY THIS READS THE SOURCE. The binding and the ceremony it replaced are
    // behaviourally IDENTICAL — that equivalence is the whole point, and it is
    // also why no black-box test can tell them apart. The claim at risk is not
    // "the clock works" but "ordinary authors no longer write Timer lifecycle",
    // and only the file can witness that. Defense in depth, exactly like the
    // host's no-wind tripwire above: it stops the claim quietly becoming false,
    // it does not make the ceremony unwritable.
    std::ifstream clock(CLOCK_CPP);
    REQUIRE_MESSAGE(clock.is_open(), "cannot read the clock source: " CLOCK_CPP);
    const std::string source((std::istreambuf_iterator<char>(clock)),
                             std::istreambuf_iterator<char>());
    REQUIRE(source.size() > 500); // we really read the adapter, not an empty path

    // It declares a binding...
    CHECK(source.find("TimedWeave") != std::string::npos);
    CHECK(source.find("timers().repeat") != std::string::npos);

    // ...and authors none of the three ceremony handlers the binding owns.
    // Matched on the HANDLER SIGNATURE, not the bare shape name: the file is
    // allowed to say "TimerReady" in a comment explaining what left, and a
    // tripwire that could not tell prose from code would be training authors to
    // stop explaining themselves.
    CHECK(source.find("on(const loom::Activated") == std::string::npos);
    CHECK(source.find("on(const timer::TimerReady") == std::string::npos);
    CHECK(source.find("on(const timer::TimerFired") == std::string::npos);
    CHECK(source.find("ActivationCursor") == std::string::npos);

    // The adapter itself REMAINS a weave, and that is not an accident: the
    // time-to-world policy is replaceable (a pause driver, a slow-motion clock,
    // a replay feeder). Only the ceremony left.
    CHECK(source.find("SnakeTick") != std::string::npos);
    CHECK(source.find("ZEN_EXPORT_WEAVE") != std::string::npos);
}
