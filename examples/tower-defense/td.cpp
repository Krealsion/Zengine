// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Tower Defense: a small game played in a Workshop pane, made from inside Workshop.
//
// One source file using only headers the installed Zengine and Loom packages publish, so a
// single-source recipe builds it with these links:
//     zengine::pane, zengine::activation, zengine::input, zengine::timer, loom::switchboard
// and a load-plan row loads it under the role "td.game" (kOffice, below).

#include "workshop/pane_vocabulary.hpp"

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "timer/binding.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace ws = zengine::workshop;
namespace surface = zengine::surface;
using zengine::timer::TimedWeave;
using zengine::timer::TimerFired;

constexpr const char* kOffice = "td.game";            // the role its load-plan row gives it
constexpr const char* kWorkshop = "zengine.workshop"; // the only office it answers
constexpr const char* kPane = "td";

// ---- What a reload in place carries -----------------------------------------------------------
//
// This shape is the game's save format across a reload: a rebuilt image whose state differs is
// refused, not migrated. So every field the finished game needs is here from the first build, and
// later changes are to the rules and the picture, never to these structs.
struct TdTower {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t cooldown = 0; ///< ticks until it may fire again
    ZEN_SHAPE(TdTower, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(cooldown));
};

struct TdEnemy {
    std::int64_t id = 0;
    std::int64_t hp = 0;
    std::int64_t step = 0; ///< how far along the path, in cells
    std::int64_t wait = 0; ///< ticks until it moves again
    ZEN_SHAPE(TdEnemy, 1, ZEN_FIELD(id), ZEN_FIELD(hp), ZEN_FIELD(step), ZEN_FIELD(wait));
};

struct TdState {
    std::int64_t started = 0;  ///< 0 until the first game is dealt
    std::int64_t gold = 0;
    std::int64_t lives = 0;
    std::int64_t wave = 0;     ///< waves begun, 0..kWaves
    std::int64_t phase = 0;    ///< kBuilding, kRunning, kWon or kLost
    std::int64_t paused = 0;
    std::int64_t tick = 0;     ///< simulation ticks since the game began
    std::int64_t to_spawn = 0; ///< enemies of this wave not yet on the path
    std::int64_t spawn_wait = 0;
    std::int64_t cursor_x = 0;
    std::int64_t cursor_y = 0;
    std::int64_t kills = 0;
    std::int64_t leaked = 0;
    std::int64_t next_id = 0;
    std::string message;
    std::vector<TdTower> towers;
    std::vector<TdEnemy> enemies;
    ZEN_EXPOSE();
    ZEN_SHAPE(TdState, 1, ZEN_FIELD(started), ZEN_FIELD(gold), ZEN_FIELD(lives), ZEN_FIELD(wave),
              ZEN_FIELD(phase), ZEN_FIELD(paused), ZEN_FIELD(tick), ZEN_FIELD(to_spawn),
              ZEN_FIELD(spawn_wait), ZEN_FIELD(cursor_x), ZEN_FIELD(cursor_y), ZEN_FIELD(kills),
              ZEN_FIELD(leaked), ZEN_FIELD(next_id), ZEN_FIELD(message), ZEN_FIELD(towers),
              ZEN_FIELD(enemies));
};

/// A command from another participant -- a stored Inventory command, a Compose form, a Terminal
/// `ask @td.game TdCommand 1 verb=status`: wave, pause, step, restart, check or status. It is
/// answered with a zen.Result saying what it did, or a zen.Refused saying why not.
struct TdCommand {
    std::string verb;
    ZEN_SHAPE(TdCommand, 1, ZEN_FIELD(verb));
};

// ---- The rules --------------------------------------------------------------------------------
constexpr std::int64_t kBuilding = 0;
constexpr std::int64_t kRunning = 1;
constexpr std::int64_t kWon = 2;
constexpr std::int64_t kLost = 3;
constexpr std::int64_t kWaves = 5;
constexpr std::int64_t kStartGold = 40;
constexpr std::int64_t kStartLives = 10;
constexpr std::int64_t kTowerCost = 10;
constexpr std::int64_t kTowerRange = 2; ///< cells, in any direction (a square)
constexpr std::int64_t kTowerReload = 2; ///< ticks a tower waits after a shot
constexpr std::int64_t kBounty = 2;      ///< gold for each enemy stopped
constexpr std::int64_t kWaveBonus = 5;   ///< gold for holding a wave
constexpr std::int64_t kSpawnGap = 3;    ///< ticks between enemies entering the road

std::int64_t wave_size(std::int64_t wave) { return 4 + 2 * wave; }
std::int64_t wave_hp(std::int64_t wave) { return 2 + 2 * wave; }
std::int64_t wave_pace(std::int64_t wave) { return wave <= 2 ? 3 : 2; } ///< ticks per cell

void new_game(TdState& s) {
    s = TdState{};
    s.started = 1;
    s.gold = kStartGold;
    s.lives = kStartLives;
    s.phase = kBuilding;
    s.cursor_x = 3;
    s.cursor_y = 3;
    s.message = "Place towers beside the road, then start a wave.";
}

// ---- The map ----------------------------------------------------------------------------------
constexpr int kWidth = 24;
constexpr int kHeight = 9;

struct Cell {
    int x = 0;
    int y = 0;
};

/// The road enemies walk, entrance first: straight runs between these turns.
const std::vector<Cell>& path() {
    static const std::vector<Cell> cells = [] {
        const Cell turns[] = {{0, 1}, {6, 1}, {6, 6}, {12, 6}, {12, 2}, {18, 2}, {18, 7}, {23, 7}};
        std::vector<Cell> out{turns[0]};
        for (std::size_t i = 1; i < sizeof(turns) / sizeof(turns[0]); ++i) {
            Cell at = out.back();
            while (at.x != turns[i].x || at.y != turns[i].y) {
                at.x += (turns[i].x > at.x) - (turns[i].x < at.x);
                at.y += (turns[i].y > at.y) - (turns[i].y < at.y);
                out.push_back(at);
            }
        }
        return out;
    }();
    return cells;
}

bool on_path(std::int64_t x, std::int64_t y) {
    for (const Cell& c : path()) {
        if (c.x == x && c.y == y) {
            return true;
        }
    }
    return false;
}

void start_wave(TdState& s) {
    if (s.phase != kBuilding) {
        s.message = s.phase == kRunning ? "A wave is already on the road."
                                        : "The game is over -- r starts a new one.";
        return;
    }
    ++s.wave;
    s.phase = kRunning;
    s.paused = 0;
    s.to_spawn = wave_size(s.wave);
    s.spawn_wait = 0;
    s.message = "Wave " + std::to_string(s.wave) + ": " + std::to_string(s.to_spawn) +
                " enemies with " + std::to_string(wave_hp(s.wave)) + " hp each.";
}

/// One tick of a running wave. Deterministic: the same state gives the same next state.
void step(TdState& s) {
    if (s.phase != kRunning) {
        return;
    }
    ++s.tick;
    // Enemies enter one at a time at the start of the road.
    if (s.to_spawn > 0) {
        if (s.spawn_wait > 0) {
            --s.spawn_wait;
        } else {
            s.enemies.push_back(TdEnemy{++s.next_id, wave_hp(s.wave), 0, wave_pace(s.wave)});
            --s.to_spawn;
            s.spawn_wait = kSpawnGap;
        }
    }
    // They walk the road; one that reaches the base costs a life.
    const std::int64_t last = static_cast<std::int64_t>(path().size()) - 1;
    for (TdEnemy& e : s.enemies) {
        if (e.wait > 0) {
            --e.wait;
        } else {
            ++e.step;
            e.wait = wave_pace(s.wave) - 1;
        }
    }
    for (auto it = s.enemies.begin(); it != s.enemies.end();) {
        if (it->step >= last) {
            --s.lives;
            ++s.leaked;
            it = s.enemies.erase(it);
        } else {
            ++it;
        }
    }
    // Each ready tower shoots the enemy in reach that is nearest the base.
    for (TdTower& t : s.towers) {
        if (t.cooldown > 0) {
            --t.cooldown;
            continue;
        }
        TdEnemy* target = nullptr;
        for (TdEnemy& e : s.enemies) {
            const Cell& c = path()[static_cast<std::size_t>(e.step)];
            const std::int64_t dx = c.x > t.x ? c.x - t.x : t.x - c.x;
            const std::int64_t dy = c.y > t.y ? c.y - t.y : t.y - c.y;
            if (std::max(dx, dy) <= kTowerRange && (target == nullptr || e.step > target->step)) {
                target = &e;
            }
        }
        if (target != nullptr) {
            --target->hp;
            t.cooldown = kTowerReload;
        }
    }
    for (auto it = s.enemies.begin(); it != s.enemies.end();) {
        if (it->hp <= 0) {
            ++s.kills;
            s.gold += kBounty;
            it = s.enemies.erase(it);
        } else {
            ++it;
        }
    }
    if (s.lives <= 0) {
        s.lives = 0;
        s.phase = kLost;
        s.enemies.clear();
        s.to_spawn = 0;
        s.message = "The base fell during wave " + std::to_string(s.wave) + ". Press r to play again.";
    } else if (s.to_spawn == 0 && s.enemies.empty()) {
        if (s.wave >= kWaves) {
            s.phase = kWon;
            s.message = "All " + std::to_string(kWaves) + " waves held with " + std::to_string(s.lives) +
                        " lives left. Press r to play again.";
        } else {
            s.phase = kBuilding;
            s.gold += kWaveBonus;
            s.message = "Wave " + std::to_string(s.wave) + " held: +" + std::to_string(kWaveBonus) +
                        " gold. Space starts wave " + std::to_string(s.wave + 1) + ".";
        }
    }
}

/// Build a tower on the cursor's cell when the rules allow it; the message says what happened.
void build_at_cursor(TdState& s) {
    const std::int64_t x = s.cursor_x;
    const std::int64_t y = s.cursor_y;
    if (s.phase == kWon || s.phase == kLost) {
        s.message = "The game is over -- r starts a new one.";
        return;
    }
    if (on_path(x, y)) {
        s.message = "Towers go beside the road, not on it.";
        return;
    }
    for (const TdTower& t : s.towers) {
        if (t.x == x && t.y == y) {
            s.message = "There is already a tower there.";
            return;
        }
    }
    if (s.gold < kTowerCost) {
        s.message = "A tower costs " + std::to_string(kTowerCost) + " gold; you have " +
                    std::to_string(s.gold) + ".";
        return;
    }
    s.gold -= kTowerCost;
    s.towers.push_back(TdTower{x, y, 0});
    s.message = "Tower built at " + std::to_string(x) + "," + std::to_string(y) + ".";
}

void move_cursor(TdState& s, std::int64_t dx, std::int64_t dy) {
    s.cursor_x = std::min<std::int64_t>(kWidth - 1, std::max<std::int64_t>(0, s.cursor_x + dx));
    s.cursor_y = std::min<std::int64_t>(kHeight - 1, std::max<std::int64_t>(0, s.cursor_y + dy));
}

/// The rules, checked on scratch games: deterministic, and independent of the game on screen.
std::string check_rules() {
    int passed = 0;
    int total = 0;
    std::string failed;
    auto expect = [&](bool ok, const char* what) {
        ++total;
        if (ok) {
            ++passed;
        } else if (failed.empty()) {
            failed = what;
        }
    };
    TdState s;
    new_game(s);
    expect(s.gold == kStartGold && s.lives == kStartLives && s.phase == kBuilding && s.wave == 0,
           "a new game starts with its gold and lives and no wave");
    s.cursor_x = 0;
    s.cursor_y = 1;
    build_at_cursor(s);
    expect(s.towers.empty() && s.gold == kStartGold, "no tower on the road");
    s.cursor_x = 5;
    s.cursor_y = 2;
    build_at_cursor(s);
    expect(s.towers.size() == 1 && s.gold == kStartGold - kTowerCost, "a tower beside the road costs gold");
    build_at_cursor(s);
    expect(s.towers.size() == 1, "one tower per cell");
    TdState poor = s;
    poor.gold = kTowerCost - 1;
    poor.cursor_x = 7;
    build_at_cursor(poor);
    expect(poor.towers.size() == 1 && poor.gold == kTowerCost - 1, "no tower without the gold");
    start_wave(s);
    expect(s.phase == kRunning && s.wave == 1 && s.to_spawn == wave_size(1),
           "a wave starts with its enemies waiting");
    step(s);
    expect(s.enemies.size() == 1 && s.enemies[0].step == 0 && s.to_spawn == wave_size(1) - 1,
           "the first enemy enters at the start of the road");
    TdState walk;
    new_game(walk);
    start_wave(walk);
    for (std::int64_t i = 0; i <= wave_pace(1); ++i) {
        step(walk);
    }
    expect(!walk.enemies.empty() && walk.enemies[0].step == 1, "an enemy moves one cell per pace");
    TdState fight;
    new_game(fight);
    fight.cursor_x = 1;
    fight.cursor_y = 2;
    build_at_cursor(fight);
    start_wave(fight);
    const std::int64_t before = fight.gold;
    for (int i = 0; i < 40 && fight.kills == 0; ++i) {
        step(fight);
    }
    expect(fight.kills >= 1 && fight.gold >= before + kBounty, "a tower stops an enemy and earns its bounty");
    TdState lost;
    new_game(lost);
    lost.lives = 1;
    start_wave(lost);
    for (int i = 0; i < 400 && lost.phase == kRunning; ++i) {
        step(lost);
    }
    expect(lost.phase == kLost && lost.lives == 0, "losing the last life ends the game");
    TdState held;
    new_game(held);
    held.phase = kRunning;
    held.wave = 1;
    const std::int64_t gold = held.gold;
    step(held);
    expect(held.phase == kBuilding && held.gold == gold + kWaveBonus, "a held wave pays its bonus");
    TdState won;
    new_game(won);
    won.phase = kRunning;
    won.wave = kWaves;
    step(won);
    expect(won.phase == kWon, "holding the last wave wins");
    return "rules check: " + std::to_string(passed) + "/" + std::to_string(total) + " passed" +
           (failed.empty() ? std::string() : " -- first failure: " + failed);
}

// ---- The picture ------------------------------------------------------------------------------
char glyph(const TdState& s, int x, int y) {
    for (const TdEnemy& e : s.enemies) {
        const Cell& c = path()[static_cast<std::size_t>(e.step)];
        if (c.x == x && c.y == y) {
            return e.hp > 3 ? 'O' : 'o';
        }
    }
    for (const TdTower& t : s.towers) {
        if (t.x == x && t.y == y) {
            return 'T';
        }
    }
    if (x == path().back().x && y == path().back().y) {
        return '@';
    }
    return on_path(x, y) ? '=' : '.';
}

std::string phase_words(const TdState& s) {
    if (s.phase == kWon) {
        return "YOU WIN";
    }
    if (s.phase == kLost) {
        return "GAME OVER";
    }
    if (s.phase == kRunning) {
        return s.paused ? "wave paused" : "wave running";
    }
    return "building";
}

/// One line a test or a Terminal can read back: the numbers the rules change.
std::string status_line(const TdState& s) {
    return "wave " + std::to_string(s.wave) + "/" + std::to_string(kWaves) + " gold " +
           std::to_string(s.gold) + " lives " + std::to_string(s.lives) + " " + phase_words(s) +
           " towers " + std::to_string(s.towers.size()) + " enemies " +
           std::to_string(s.enemies.size()) + " kills " + std::to_string(s.kills) + " tick " +
           std::to_string(s.tick);
}

std::vector<surface::SurfaceTextRow> picture(const TdState& s) {
    std::vector<surface::SurfaceTextRow> rows;
    rows.push_back({"TOWER DEFENSE   wave " + std::to_string(s.wave) + "/" + std::to_string(kWaves) +
                        "   gold " + std::to_string(s.gold) + "   lives " + std::to_string(s.lives) +
                        "   " + phase_words(s),
                    surface::role::kAccent});
    for (int y = 0; y < kHeight; ++y) {
        std::string row = " ";
        for (int x = 0; x < kWidth; ++x) {
            const bool here = x == s.cursor_x && y == s.cursor_y;
            if (here) {
                row.back() = '[';
            }
            row += glyph(s, x, y);
            row += here ? ']' : ' ';
        }
        rows.push_back({row, surface::role::kFill});
    }
    const bool road = on_path(s.cursor_x, s.cursor_y);
    rows.push_back({"cursor " + std::to_string(s.cursor_x) + "," + std::to_string(s.cursor_y) + " -- " +
                        (road ? "the road" : "ground") + "; a tower costs " + std::to_string(kTowerCost) +
                        " gold and reaches " + std::to_string(kTowerRange) + " cells",
                    surface::role::kMuted});
    rows.push_back({s.message, s.phase >= kWon ? surface::role::kAlert : surface::role::kFill});
    rows.push_back({"arrows move  t build  space next wave  p pause  . step  r restart  c check",
                    surface::role::kMuted});
    return rows;
}

// ---- The pane ---------------------------------------------------------------------------------
class Game : public TimedWeave<Game, TdState,
                               loom::Accept<ws::PaneCatalogRequested, ws::PaneRoom,
                                            ws::PaneActionRequested, ws::PanePressed, TdCommand>,
                               loom::Emit<ws::v2::PaneOffered, ws::PaneActions, ws::PaneContent>> {
public:
    Game() : tick_(timers().repeat("td.tick", std::chrono::milliseconds(150), &Game::on_tick)) {}

    using TimedWeave::on;

    void on_timed_activation(const loom::Activated&, loom::Mail& mail) {
        if (state_.started == 0) {
            new_game(state_);
        }
        offer(mail);
    }

    void on(const ws::PaneCatalogRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(kWorkshop)) {
            offer(mail);
        }
    }

    void on(const ws::PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || room.pane != kPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        show(mail);
    }

    void on(const ws::PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || asked.pane != kPane) {
            return;
        }
        if (asked.id.rfind("td.", 0) == 0) {
            (void)perform(asked.id.substr(3));
        }
        show(mail);
    }

    void on(const ws::PanePressed& pressed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || pressed.pane != kPane) {
            return;
        }
        // Map rows start under the header, and each cell is two columns: its glyph, then a gap.
        const std::int64_t y = pressed.row - 1;
        const std::int64_t x = (pressed.column - 1) / 2;
        if (y >= 0 && y < kHeight && pressed.column >= 1 && x < kWidth) {
            const bool again = x == state_.cursor_x && y == state_.cursor_y;
            state_.cursor_x = x;
            state_.cursor_y = y;
            if (again) {
                build_at_cursor(state_); // a second press on the chosen cell builds there
            } else {
                state_.message = "Cell " + std::to_string(x) + "," + std::to_string(y) +
                                 " chosen -- press it again or t to build.";
            }
        }
        show(mail);
    }

    void on(const TdCommand& command, loom::Mail& mail) {
        if (!perform(command.verb)) {
            (void)mail.answer(loom::Refused{"TdCommand knows wave, pause, step, restart, check and "
                                            "status -- not `" + command.verb + "`"});
            return;
        }
        show(mail);
        (void)mail.answer(loom::Result{status_line(state_) + " -- " + state_.message});
    }

    void on_tick(const TimerFired&, loom::Mail& mail) {
        if (state_.phase == kRunning && !state_.paused) {
            step(state_);
            show(mail);
        }
    }

private:
    /// One verb, whoever asked: a declared key (`td.<verb>`) or a TdCommand. False if unknown.
    bool perform(const std::string& verb) {
        if (verb == "left" || verb == "right" || verb == "up" || verb == "down") {
            move_cursor(state_, verb == "left" ? -1 : verb == "right" ? 1 : 0,
                        verb == "up" ? -1 : verb == "down" ? 1 : 0);
        } else if (verb == "build") {
            build_at_cursor(state_);
        } else if (verb == "wave") {
            start_wave(state_);
        } else if (verb == "pause") {
            if (state_.phase == kRunning) {
                state_.paused = state_.paused ? 0 : 1;
                state_.message = state_.paused ? "Paused -- . steps once, p resumes." : "Resumed.";
            } else {
                state_.message = "Pause works while a wave is running.";
            }
        } else if (verb == "step") {
            if (state_.phase == kRunning && state_.paused) {
                step(state_);
            } else {
                state_.message = "Step works while a wave is paused (p).";
            }
        } else if (verb == "restart") {
            new_game(state_);
        } else if (verb == "check") {
            state_.message = check_rules();
        } else if (verb != "status") {
            return false;
        }
        return true;
    }

    void offer(loom::Mail& mail) {
        (void)mail.as_role(kOffice).send_to_role(
            kWorkshop, ws::v2::PaneOffered{kPane, "Tower Defense", "defend the base from five waves", 14, 60});
        namespace scan = zengine::input::scan;
        const std::int64_t none = zengine::input::mod::kNone;
        ws::PaneActions actions;
        actions.pane = kPane;
        actions.rows = {{"td.left", "cursor left", scan::kLeft, none},
                        {"td.right", "cursor right", scan::kRight, none},
                        {"td.up", "cursor up", scan::kUp, none},
                        {"td.down", "cursor down", scan::kDown, none},
                        {"td.build", "build a tower", scan::kT, none},
                        {"td.wave", "start the next wave", scan::kSpace, none},
                        {"td.pause", "pause or resume", scan::kP, none},
                        {"td.step", "one step while paused", scan::kPeriod, none},
                        {"td.restart", "new game", scan::kR, none},
                        {"td.check", "check the rules", scan::kC, none}};
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, actions);
    }

    // Workshop refuses a row wider than the room, and a room holds only so many rows.
    void show(loom::Mail& mail) {
        ws::PaneContent said;
        said.pane = kPane;
        for (surface::SurfaceTextRow row : picture(state_)) {
            if (static_cast<std::int64_t>(said.rows.size()) >= rows_ || columns_ <= 0) {
                break;
            }
            if (static_cast<std::int64_t>(row.text.size()) > columns_) {
                row.text.resize(static_cast<std::size_t>(columns_));
            }
            said.rows.push_back(row);
        }
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, said);
    }

    Handle tick_;
    std::int64_t rows_ = 0; // the room Workshop last granted; not state, so a reload re-asks
    std::int64_t columns_ = 0;
};

} // namespace

ZEN_EXPORT_WEAVE(Game)
