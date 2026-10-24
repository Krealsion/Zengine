// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The snake game proven headless, in three tiers: the ZEN_SHAPE spellings derive the locked
// contract's schemas by content id; the simulation and the migration are pure math; and the
// moments run end to end through the real kernel, manager and bus. That tier steers by LOCKSTEP
// -- a local State through the same logic.hpp, the published SnakeVisual equal to its projection
// every tick -- so determinism is pinned, not assumed. What the world draws, and what its window
// shows, is the graphics suite's (test_graphics.cpp); here the window is read as rows of text.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "logic.hpp"
#include "vocabulary.hpp"

#include "graphics_rig.hpp"

#include "graphics/elements/button.hpp"
#include "timer/vocabulary.hpp"

#include <fstream>
#include <iterator>

#include <zen/kernel/control.hpp>
#include <zen/kernel/admission.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <cstdint>
#include <exception>
#include <iostream>
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

/// fd-1 silencer: a library writing stdout must not garble the suite's own output.
/// Scoped exactly around pumps. The null device and the dup spellings are the only platform
/// seam (Windows: `NUL`, the _-prefixed CRT forms).
class Hush {
public:
    Hush() {
        std::fflush(stdout);
#if defined(_WIN32)
        // _dup2 on descriptor 1 also points the Windows standard handle at its new handle, which a
        // weave loading meanwhile keeps as its own stdout under the MSVC ABI: so the hush hands it
        // a NUL handle never closed, and gives back one a catcher holds.
        const HANDLE entry = ::GetStdHandle(STD_OUTPUT_HANDLE);
        pinned_ = entry != reinterpret_cast<HANDLE>(::_get_osfhandle(1)) ? entry : nullptr;
        saved_ = ::_dup(1);
        const int nul = ::_open("NUL", _O_WRONLY);
        if (nul >= 0) {
            ::_dup2(nul, 1);
            ::_close(nul);
        }
        ::SetStdHandle(STD_OUTPUT_HANDLE, process_nul());
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
            if (pinned_ != nullptr) {
                ::SetStdHandle(STD_OUTPUT_HANDLE, pinned_);
            }
#else
            ::dup2(saved_, STDOUT_FILENO);
            ::close(saved_);
#endif
        }
    }
    Hush(const Hush&) = delete;
    Hush& operator=(const Hush&) = delete;

private:
#if defined(_WIN32)
    /// One NUL handle for the process, opened once and never closed.
    static HANDLE process_nul() {
        static const HANDLE nul =
            ::CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, 0, nullptr);
        return nul;
    }
    HANDLE pinned_ = nullptr;
#endif
    int saved_ = -1;
};

struct Rig {
    loom::Switchboard bus;
    loom::Kernel kernel{bus, loom::trust_every_artifact(
        "Zengine's test harness: it loads only this build tree's output")};
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

    void drain() {
        Hush hush;
        bus.drain_until_idle();
    }

    /// Fire a lifecycle command AS the probe (its grant is the authority) and pump until the
    /// answer lands. Returns the answer BY VALUE: the recorder's vector grows with every answer,
    /// so a reference into it would dangle at the next command, a read ASan reports.
    template <class Cmd>
    Recorded::Answer command(const Cmd& cmd) {
        const std::uint64_t corr = next_corr++;
        bus.send_as(probe, manager, loom::Message(loom::to_value(cmd), probe, probe, corr));
        drain();
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
        drain();
        const Recorded::Answer* a = rec.find(corr);
        REQUIRE(a != nullptr);
        return *a;
    }

    void tick() {
        bus.send_to_role(kWorldRole, loom::Message(loom::to_value(SnakeTick{})));
        drain();
    }

    void turn_real(std::int64_t dir) {
        bus.send_to_role(kWorldRole, loom::Message(loom::to_value(SnakeTurn{dir})));
    }

    loom::WeaveId load(const char* name, const char* path, const char* role) {
        const Recorded::Answer a = command(loom::LoadWeave{name, path, role});
        REQUIRE_MESSAGE(a.kind == 0, "load refused: ", a.text);
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(a.text))};
    }

    /// A load by the kernel itself, pumped raw, so no Hush is around it: under the MSVC ABI a
    /// weave keeps the stdout it loaded with, and a catcher must be what it loaded with.
    loom::WeaveId load_unhushed(const char* name, const char* path, const char* role) {
        const loom::LoadResult loaded = kernel.load(name, path, role);
        REQUIRE_MESSAGE(loaded.ok, "load refused: ", loaded.error);
        bus.drain_until_idle();
        return loaded.id;
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

    const auto world = SchemaBuilder("SnakeWorldState", 5)
                           .field("width", Kind::Int)
                           .field("height", Kind::Int)
                           .list("snake", loom::type_message(pos))
                           .message("food", pos)
                           .field("direction", Kind::Int)
                           .field("alive", Kind::Bool)
                           .field("score", Kind::Int)
                           .field("keys", Kind::Bool)
                           .field("key_up", Kind::Int)
                           .field("key_right", Kind::Int)
                           .field("key_down", Kind::Int)
                           .field("key_left", Kind::Int)
                           .field("beat_ms", Kind::Int)
                           .field("ticks", Kind::Int)
                           .build();
    CHECK(schema_of<v1::SnakeWorldState>()->content_id() == world->content_id());
}

TEST_CASE("contract: v2 state is a genuinely different identity") {
    CHECK(schema_of<v1::SnakeWorldState>()->content_id() !=
          schema_of<v2::SnakeWorldState>()->content_id());
    CHECK(schema_of<v1::SnakeWorldState>()->name() == schema_of<v2::SnakeWorldState>()->name());
    CHECK(schema_of<v1::SnakeWorldState>()->version() == 5);
    CHECK(schema_of<v2::SnakeWorldState>()->version() == 6);
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

TEST_CASE("moment 1: another skin is chosen while the game continues, and nothing is drawn again") {
    namespace rig = zengine::testing::graphics_rig;
    rig::Rig r;
    REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, zengine::graphics::kGraphicsRole).valid());
    REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, rig::window_office()).valid());
    REQUIRE(r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole).valid());
    r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kWorldArea, kWorldRole, "classic");

    r.tick(); // seeds: the window shows the classic head
    v1::SnakeWorldState local;
    seed(local);
    REQUIRE(r.heard.visuals.size() == 1);
    REQUIRE(same_visual(r.heard.visuals.back(), visual_of(local)));
    CHECK(r.screen().find('O') != std::string::npos);

    // The choice: mid-game, through the directory. The window opens the other painter code and
    // paints what it keeps; the world draws nothing.
    const std::size_t mark = r.log.size();
    r.to_role(zengine::graphics::kGraphicsRole, zengine::graphics::SkinChosen{kWorldArea, "block"});
    r.drain();
    CHECK(r.count(mark, "Drawn", kWorldRole) == 0);
    CHECK(r.count(mark, "ElementPut", kWorldRole) == 0);
    CHECK(r.count(mark, "ElementChange", kWorldRole) == 0);
    const std::string block = r.screen();
    CHECK(block.find('@') != std::string::npos);
    CHECK(block.find('O') == std::string::npos);

    // The world never stopped: the next tick advances the SAME run.
    r.tick();
    (void)step(local);
    REQUIRE(r.heard.visuals.size() == 2);
    CHECK(same_visual(r.heard.visuals.back(), visual_of(local)));
}

TEST_CASE("moment 2: a score weave loaded late counts only what it witnesses") {
    Rig r;
    r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);

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
    const loom::WeaveId score = r.load("snake-score", SNAKE_SO_SCORE, kScoreRole);
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
    r.tick(); // the heir's first beat claims, and the adopted state is its first visual
    CHECK(same_visual(r.rec.visuals.back(), pre_second));
    const loom::WeaveId heir{static_cast<std::uint64_t>(std::stoll(second.text))};
    CHECK(r.poke(heir, loom::PokeRead{"growths"}).text == "1");
}

TEST_CASE("snapshot/revive: an in-place reload carries the state through the gate") {
    Rig r;
    r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);
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

TEST_CASE("the substrate's own reset door starts a new game") {
    Rig r;
    const loom::WeaveId world = r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);
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
// Snake's negative space: it publishes, it does not paint
// ============================================================================

namespace {

/// fd-1 catcher: like Hush, but the bytes land in a file the test can weigh; a zero is a
/// measurement only because the same catcher finds a painting library's bytes. Under the MSVC ABI a
/// weave's own C runtime takes as its stdout the Windows standard handle it loads with, which
/// _dup2 on descriptor 1 points at the file: so a catcher is made before the weaves it weighs
/// load, and outlives them. What it caught is copied to stderr when a check throws inside it.
class CatchStdout {
public:
    explicit CatchStdout(const char* path)
        : path_(path), unwinding_(std::uncaught_exceptions()) {
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
        if (std::uncaught_exceptions() > unwinding_) {
            std::ifstream in(path_, std::ios::binary);
            std::cerr << in.rdbuf() << std::flush;
        }
    }
    CatchStdout(const CatchStdout&) = delete;
    CatchStdout& operator=(const CatchStdout&) = delete;

private:
    std::string path_;
    int unwinding_ = 0;
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

TEST_CASE("snake draws into its window, never onto the terminal: with no terminal, zero bytes") {
    // A stretch of real life -- ticks, a witnessed meal, a spoken tally, the window keeping and
    // composing the picture -- in a process whose standard output is no terminal: every one of
    // them a message on the bus, none of them bytes on stdout. Raw pumps on purpose: the catcher,
    // not the Hush, must be the observer. Each catcher is made before its rig, and the rig's
    // weaves load inside it, unhushed, and leave before it closes.
    std::string eaten;
    std::int64_t showings = 0;
    {
        CatchStdout catcher("snake_no_paint.tmp");
        namespace rig = zengine::testing::graphics_rig;
        rig::Rig r;
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, zengine::graphics::kGraphicsRole).valid());
        REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, rig::window_office()).valid());
        const loom::WeaveId score = r.load("snake-score", SNAKE_SO_SCORE, kScoreRole);
        REQUIRE(r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole).valid());
        r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kWorldArea, kWorldRole,
                  "classic");
        for (int i = 0; i < 6; ++i) {
            r.tick();
        }
        r.bus.send(score, loom::Message(loom::to_value(FoodEaten{})));
        r.drain();
        const loom::Value read = loom::to_value(loom::PokeRead{"eaten"});
        const std::uint64_t corr = r.next_corr++;
        r.bus.send(score, loom::Message(read, loom::WeaveId{}, r.ears, corr));
        r.drain();
        eaten = r.heard.find(corr) != nullptr ? r.heard.find(corr)->text : "";
        showings = r.shown().showings;
    }
    CHECK(file_size_then_remove("snake_no_paint.tmp") == 0);
    CHECK(eaten == "1");    // the game DID run
    CHECK(showings >= 6);   // and its window DID compose the picture, every tick

    // The negative control: the same meter, a loaded library writing its stdout -- paint.
    {
        CatchStdout catcher("snake_painted.tmp");
        Rig r;
        r.load_unhushed("zengine-test-stdout-painter", STDOUT_PAINTER_SO, "zengine.test.painter");
        loom::Value paint(loom::SchemaBuilder("PaintStdout", 1).field("text", loom::Kind::Text).build());
        paint.set("text", loom::Cell::text("painted\n"));
        r.bus.send_to_role("zengine.test.painter", loom::Message(std::move(paint)));
        r.bus.drain_until_idle();
    }
    CHECK(file_size_then_remove("snake_painted.tmp") > 0);
}

// ============================================================================
// The host holds no privileged wind (TIMER-02)
// ============================================================================

TEST_CASE("the playable host sends no Drive: time is the composition's, not the host's") {
    // WHY THIS READS THE SOURCE: a root Drive carries no activation key, so the service ignores
    // it (TIMER-01), and a wind left in the host would change no behaviour. The claim at risk is
    // "the host contributes nothing to time", and only the file can guard it -- defense in depth
    // against that claim quietly becoming false, not a proof: a wind by another spelling still
    // compiles, and nobody writes one by accident.
    std::ifstream host(HOST_PLAY_CPP);
    REQUIRE_MESSAGE(host.is_open(), "cannot read the host source: ", std::string(HOST_PLAY_CPP));
    const std::string source((std::istreambuf_iterator<char>(host)),
                             std::istreambuf_iterator<char>());
    REQUIRE(source.size() > 1000); // we really did read the host, not an empty path

    // No Drive is constructed, sent, or named anywhere in the playable host.
    CHECK(source.find("Drive") == std::string::npos);
    // And the timer vocabulary IS reached for -- the host names the role it loads the service
    // into -- so the absence above is of the WIND, not of a host that speaks no timer at all.
    CHECK(source.find("timer::kTimerRole") != std::string::npos);
}

TEST_CASE("the world's beat uses the timer binding, not hand-written ceremony") {
    // WHY THIS READS THE SOURCE: the binding and a hand-written ceremony behave IDENTICALLY --
    // that is the point, and why no black-box test can tell them apart. The claim at risk is
    // "ordinary authors do not write Timer lifecycle", and only the file can witness it; like
    // the no-wind tripwire above, it keeps the claim from quietly becoming false, it does not
    // make the ceremony unwritable.
    std::ifstream world(WORLD_CPP);
    REQUIRE_MESSAGE(world.is_open(), "cannot read the world source: ", std::string(WORLD_CPP));
    const std::string source((std::istreambuf_iterator<char>(world)),
                             std::istreambuf_iterator<char>());
    REQUIRE(source.size() > 2000); // we really read the world, not an empty path

    // It declares a binding...
    CHECK(source.find("TimedWeave") != std::string::npos);
    CHECK(source.find("timers().repeat") != std::string::npos);

    // ...and authors none of the three ceremony handlers the binding owns. Matched on the
    // HANDLER SIGNATURE, not the bare shape name: the file may say "TimerReady" in a comment,
    // and a tripwire that could not tell prose from code would train authors to stop explaining
    // themselves.
    CHECK(source.find("void on(const loom::Activated") == std::string::npos);
    CHECK(source.find("void on(const timer::TimerReady") == std::string::npos);
    CHECK(source.find("void on(const timer::TimerFired") == std::string::npos);
    CHECK(source.find("ActivationCursor") == std::string::npos);

    // And its doors stay open beside the beat: world time and steering from anyone.
    CHECK(source.find("on(const SnakeTick&") != std::string::npos);
    CHECK(source.find("on(const SnakeTurn&") != std::string::npos);
}

// ============================================================================
// The world's new-game button is a part its window holds
// ============================================================================

namespace {

/// A source's code with its line comments stripped, so a tripwire reads code, not prose.
std::string code_of(const char* path) {
    std::ifstream in(path);
    REQUIRE_MESSAGE(in.is_open(), "cannot read the source: ", std::string(path));
    std::string code;
    std::string line;
    while (std::getline(in, line)) {
        bool quoted = false;
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '"' && (i == 0 || line[i - 1] != '\\')) {
                quoted = !quoted;
            } else if (!quoted && line.compare(i, 2, "//") == 0) {
                line.resize(i);
                break;
            }
        }
        code += line + '\n';
    }
    return code;
}

} // namespace

TEST_CASE("the world draws its new-game button as a part: a ButtonFace at new-game, naming the "
          "world's action") {
    namespace rig = zengine::testing::graphics_rig;
    namespace gfx = zengine::graphics;
    rig::Rig r;
    REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
    REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, rig::window_office()).valid());
    REQUIRE(r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole).valid());
    r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kWorldArea, kWorldRole, "classic");
    r.tick();

    // The world's drawing as its window received it, the record at new-game read back as the
    // shape it was said as.
    std::vector<gfx::ElementRecord> said;
    for (const gfx::Drawn& d : r.read<gfx::Drawn>(0, kWorldRole)) {
        for (const gfx::ElementRecord& e : d.said) {
            if (e.path == "new-game") {
                said.push_back(e);
            }
        }
    }
    REQUIRE(!said.empty());
    const gfx::ElementRecord& record = said.back();
    CHECK(record.type == schema_of<gfx::ButtonFace>()->name());
    CHECK(record.action == "new-game");
    const auto face = loom::claim_item<gfx::ButtonFace>(record.data);
    REQUIRE(face.has_value());
    CHECK(face->label == " new game ");
    CHECK(face->enabled);
    // Beneath the score's row, the board's border and its rows, and the border under them.
    const std::int64_t row = v1::SnakeWorldState{}.height + 3;
    CHECK(face->y == row * kBoardCellPx);
    CHECK(face->w == 12 * kBoardCellPx);

    // ...and the window shows it as its words, on that row.
    const std::vector<std::string> rows = r.shown().rows;
    REQUIRE(rows.size() > static_cast<std::size_t>(row));
    CHECK(rows[static_cast<std::size_t>(row)] == "[ new game ]");
}

TEST_CASE("the world's new-game button is a gfx::Button, not a hand-written text and action") {
    // WHY THIS READS THE SOURCE: defence in depth beside the case above, which reads the button's
    // face back from the window's drawing. A second, hand-written action elsewhere in the world
    // would draw and press as well, and only the file says it is there.
    const std::string code = code_of(WORLD_CPP);
    REQUIRE(code.size() > 2000); // we really read the world, not an empty path
    CHECK(code.find("#include \"graphics/elements/button.hpp\"") != std::string::npos);
    CHECK(code.find("gfx::Button ") != std::string::npos);
    CHECK(code.find(".action(") == std::string::npos);
}

// ============================================================================
// What the world's doors keep: a pilot, a replay, a pace, a reload
// ============================================================================

namespace {

/// The world with its own beat, over the virtual Timer: time moves only while a pump waits for
/// it, so a pace is a count of the world's beats in a stated span.
struct Timed {
    Rig r;
    loom::WeaveId world;
    std::int64_t drives = 0;
    std::int64_t stop_after = -1;
    std::uint64_t stop_on = 0; ///< a correlation whose answer ends the pump
    loom::ObserverId tap = r.bus.add_observer([this](const loom::BusEvent& e) {
        if (e.kind != loom::EventKind::Delivered) {
            return;
        }
        if ((e.schema_name == "Drive" && ++drives == stop_after) ||
            (stop_on != 0 && e.correlation == stop_on && e.target == r.probe)) {
            r.bus.stop();
        }
    });

    Timed() {
        world = r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);
        // Loaded through the steward, which activates it: from here the beat chain never idles,
        // so every pump is bounded by the Timer's own beats.
        const std::uint64_t corr = r.next_corr++;
        r.bus.send_as(r.probe, r.manager,
                      loom::Message(loom::to_value(loom::LoadWeave{"zengine-timer-virtual",
                                                                   TIMER_VIRTUAL_SO,
                                                                   zengine::timer::kTimerRole}),
                                    r.probe, r.probe, corr));
        pump_until(corr);
        const Recorded::Answer* loaded = r.rec.find(corr);
        REQUIRE(loaded != nullptr);
        REQUIRE_MESSAGE(loaded->kind == 0, "timer load refused: ", loaded->text);
    }
    Timed(const Timed&) = delete;
    Timed& operator=(const Timed&) = delete;
    ~Timed() { r.bus.remove_observer(tap); }

    /// A stretch of virtual time: each beat of the virtual Timer is `kBeatCapMs` of it.
    void pump_for(std::int64_t ms) {
        stop_after = drives + ms / zengine::timer::kBeatCapMs;
        Hush hush;
        r.bus.drain_until_idle();
        stop_after = -1;
    }

    /// Until the answer to `corr` arrives, bounded by a hundred beats.
    void pump_until(std::uint64_t corr) {
        stop_on = corr;
        pump_for(100 * zengine::timer::kBeatCapMs);
        stop_on = 0;
    }

    /// A poke under the live chain: answered within a beat, the pump bounded by the beats.
    std::string read(const char* field) {
        const std::uint64_t corr = r.next_corr++;
        r.bus.send(world, loom::Message(loom::to_value(loom::PokeRead{field}), loom::WeaveId{},
                                        r.probe, corr));
        pump_until(corr);
        const Recorded::Answer* a = r.rec.find(corr);
        REQUIRE(a != nullptr);
        return a->text;
    }
    void write(const char* field, const char* value) {
        const std::uint64_t corr = r.next_corr++;
        r.bus.send(world, loom::Message(loom::to_value(loom::PokeWrite{field, value}),
                                        loom::WeaveId{}, r.probe, corr));
        pump_until(corr);
    }
    std::int64_t ticks() { return std::stoll(read("ticks")); }
};

} // namespace

TEST_CASE("a pilot steers through the world's door while the world's own keys are off") {
    Rig r;
    const loom::WeaveId world = r.load("snake-world-v1", SNAKE_SO_WORLD_V1, kWorldRole);
    r.tick();
    v1::SnakeWorldState local;
    seed(local);
    REQUIRE(r.poke(world, loom::PokeWrite{"keys", "false"}).kind == 1);

    // A player's key reaches the world and moves nothing: its keys are off.
    r.bus.send_to_role(kWorldRole, loom::Message(loom::to_value(
                                       zengine::input::KeyPressed{zengine::input::scan::kW, "W"})));
    r.tick();
    (void)step(local);
    CHECK(same_visual(r.rec.visuals.back(), visual_of(local)));

    // The pilot's turn, by role through the open door, steers.
    r.turn_real(kDown);
    turn(local, kDown);
    r.tick();
    (void)step(local);
    CHECK(same_visual(r.rec.visuals.back(), visual_of(local)));
    CHECK(r.rec.visuals.back().snake.front().y == 9);
}

TEST_CASE("a replay drives the world through its doors while the world's own beat is off") {
    Timed t;
    t.write("beat_ms", "0");
    t.pump_for(1200); // ten paces of virtual time
    CHECK(t.r.rec.visuals.empty()); // the world took no step of its own

    // The replay's feed: world time and steering, by role, as any driver speaks them.
    v1::SnakeWorldState local;
    for (int i = 0; i < 6; ++i) {
        if (i == 3) {
            t.r.turn_real(kUp);
            turn(local, kUp);
        }
        t.r.bus.send_to_role(kWorldRole, loom::Message(loom::to_value(SnakeTick{})));
        if (local.snake.empty()) {
            seed(local);
        } else {
            (void)step(local);
        }
    }
    t.pump_for(100);
    REQUIRE(t.r.rec.visuals.size() == 6);
    CHECK(same_visual(t.r.rec.visuals.back(), visual_of(local)));
}

TEST_CASE("a faster pace is a field of the world's state: no rebuild, the beat follows it") {
    Timed t;
    const std::int64_t start = t.ticks();
    t.pump_for(1200);
    const std::int64_t at_120 = t.ticks() - start;
    CHECK(at_120 >= 9);
    CHECK(at_120 <= 11);

    t.write("beat_ms", "60");
    const std::int64_t mid = t.ticks();
    t.pump_for(1200);
    const std::int64_t at_60 = t.ticks() - mid;
    CHECK(at_60 >= 2 * at_120 - 2);
    CHECK(at_60 <= 2 * at_120 + 2);
}

TEST_CASE("a reload of the world keeps one beat, not two") {
    Timed t;
    const std::int64_t start = t.ticks();
    t.pump_for(1200);
    const std::int64_t before = t.ticks() - start;

    const std::uint64_t corr = t.r.next_corr++;
    t.r.bus.send_as(t.r.probe, t.r.manager,
                    loom::Message(loom::to_value(loom::ReloadWeave{"snake-world-v1",
                                                                   SNAKE_SO_WORLD_V1}),
                                  t.r.probe, t.r.probe, corr));
    t.pump_until(corr);
    const Recorded::Answer* reloaded = t.r.rec.find(corr);
    REQUIRE(reloaded != nullptr);
    REQUIRE_MESSAGE(reloaded->kind == 1, "reload refused: ", reloaded->text);

    const std::int64_t mid = t.ticks();
    t.pump_for(1200);
    const std::int64_t after = t.ticks() - mid;
    CHECK(after >= before - 1);
    CHECK(after <= before + 1); // a second beat would double it
}
