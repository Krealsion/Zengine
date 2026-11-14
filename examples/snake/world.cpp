// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The snake World weave, the whole game: its rules (logic.hpp), keys, beat, pause and picture. One
// source, two libraries: snake-world-v1 and, with `SNAKE_WORLD_V2`, snake-world-v2 (a larger board
// and `growths`, the heir). It publishes its news for anyone, takes `SnakeTick` and `SnakeTurn`
// from anyone, and draws its frame and its board into the window that shows its area: the board
// said once, as the game's own element (board.hpp), then each step as a change.
// Reference: examples/snake/README.md.

// Both worlds converse: they answer `zen.PrepareShutdown` with a letter carrying their whole
// state as one bequest item. The v2 heir asks the steward by role on its first wake and adopts a
// v2 item whole or passes a v1 item through `migrate()` -- the gate's `claim_item<T>` decides.

#include "board.hpp"
#include "logic.hpp"
#include "vocabulary.hpp"

#include "graphics/drawing.hpp"
#include "graphics/elements/button.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "timer/binding.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <chrono>
#include <cstdint>
#include <string>

namespace {

using namespace zengine::snake;
namespace gfx = zengine::graphics;
namespace input = zengine::input;
namespace scan = zengine::input::scan;
namespace timer = zengine::timer;

#if defined(SNAKE_WORLD_V2)
using State = v2::SnakeWorldState;
#else
using State = v1::SnakeWorldState;
#endif

/// The actions the world's buttons name: a new game, and a pause or a resume.
constexpr const char* kNewGame = "new-game";
constexpr const char* kPause = "pause";

class SnakeWorld
    : public gfx::With<gfx::ActionFacet>::Weave<
          SnakeWorld, State,
#if defined(SNAKE_WORLD_V2)
          loom::Accept<SnakeTick, SnakeTurn, gfx::AreaKeyPressed, loom::PrepareShutdown,
                       loom::Bequest, loom::Refused, loom::AskEnded>,
          loom::Emit<SnakeVisual, FoodEaten, SnakeDied, gfx::ElementPut, gfx::ElementChange,
                     loom::Bequest, loom::ClaimBequest>,
#else
          loom::Accept<SnakeTick, SnakeTurn, gfx::AreaKeyPressed, loom::PrepareShutdown>,
          loom::Emit<SnakeVisual, FoodEaten, SnakeDied, gfx::ElementPut, gfx::ElementChange,
                     loom::Bequest>,
#endif
          timer::TimedWeave> {
public:
    using Weave::on;

    SnakeWorld()
        : Weave(kWorldRole, kWorldArea),
          beat_(timers().repeat(kTickTimerId, std::chrono::milliseconds(kTickMs),
                                &SnakeWorld::on_beat)) {}

    /// Any other driver's tick: a replay, a stepper, a test. A paused world takes no step.
    void on(const SnakeTick&, loom::Mail& mail) {
        if (state_.paused) {
            return;
        }
        advance(mail);
    }

    /// Any other driver's turn: a pilot, a replay. A paused world takes no turn either.
    void on(const SnakeTurn& t, loom::Mail&) {
        if (!state_.paused) {
            turn(state_, t.direction);
        }
    }

    /// The world's own keys, from the window that shows it: `p` pauses and resumes, whoever
    /// steers; the four the state names, and the arrows, steer while `keys` says they do and the
    /// world is not paused.
    void on(const gfx::AreaKeyPressed& k, loom::Mail&) {
        if (k.scancode == scan::kP) {
            state_.paused = !state_.paused;
            return;
        }
        if (!state_.keys || state_.paused) {
            return;
        }
        std::int64_t dir = -1;
        if (k.scancode == state_.key_up || k.scancode == scan::kUp) {
            dir = kUp;
        } else if (k.scancode == state_.key_right || k.scancode == scan::kRight) {
            dir = kRight;
        } else if (k.scancode == state_.key_down || k.scancode == scan::kDown) {
            dir = kDown;
        } else if (k.scancode == state_.key_left || k.scancode == scan::kLeft) {
            dir = kLeft;
        }
        if (dir >= 0) {
            turn(state_, dir);
        }
    }

    /// The world's buttons, pressed in the window: the pause button pauses and resumes, as `p`
    /// does; the new-game button starts a new game, unpaused, sent to the board as a change.
    void on_action(const std::string& action, loom::Mail& mail) {
        if (action == kPause) {
            state_.paused = !state_.paused;
            return;
        }
        if (action != kNewGame) {
            return;
        }
#if defined(SNAKE_WORLD_V2)
        if (awaiting_claim_) {
            return; // the inheritance decides what game this is
        }
#endif
        const State keep = state_;
        state_ = State{};
        copy_bindings(keep);
        seed(state_);
        mail.publish(visual_of(state_));
        new_board();
    }

    /// "You are being replaced. Say what you want your heir to know": the whole state, as one
    /// item, to the steward that asked, echoing its correlation.
    void on(const loom::PrepareShutdown&, loom::Mail& mail) {
        loom::Bequest letter;
        letter.role = kWorldRole;
        letter.items.push_back(loom::bequeath_item(state_));
        mail.send(mail.sender(), letter, mail.correlation());
    }

#if defined(SNAKE_WORLD_V2)
    /// The inheritance arrived: a v2 item adopted whole, a v1 item migrated, each re-admitted
    /// through the gate before a field is touched. Unrecognized items are ignored.
    void on(const loom::Bequest& letter, loom::Mail& mail) {
        if (!awaiting_claim_ || mail.correlation() != kClaimCorrelation) {
            return; // unsolicited or stale: the consumer obligation, one-shot
        }
        awaiting_claim_ = false;
        for (const loom::Bytes& item : letter.items) {
            if (auto same = loom::claim_item<v2::SnakeWorldState>(item)) {
                state_ = *same;
            } else if (auto old = loom::claim_item<v1::SnakeWorldState>(item)) {
                state_ = migrate(*old);
            } else {
                continue;
            }
            mail.publish(visual_of(state_));
            present_board(); // the board it inherited, said again whole
            return;
        }
        begin_fresh(mail); // a letter holding nothing this world recognizes
    }

    /// "No bequest is held for you": a fresh game.
    void on(const loom::Refused&, loom::Mail& mail) {
        if (awaiting_claim_ && mail.correlation() == kClaimCorrelation) {
            awaiting_claim_ = false;
            begin_fresh(mail);
        }
    }

    /// ...and a fresh game too when the claim ended with no answer: no manager held the role.
    void on(const loom::AskEnded&, loom::Mail& mail) {
        if (awaiting_claim_ && mail.ask_ended() && mail.correlation() == kClaimCorrelation) {
            awaiting_claim_ = false;
            begin_fresh(mail);
        }
    }
#endif

    /// After every delivery, and every write into its state: the beat follows the state, at the
    /// pace it holds and only while the world plays. Started again by a reload, it does the same.
    void after_delivery(loom::Mail& mail) { keep_beat(mail); }

    /// The world's own nodes: the score, the death notice, the board's place, the new-game button
    /// and the pause button beside it, which says resume while the world is paused.
    void picture(gfx::Sketch& f) const {
        const std::int64_t c = kBoardCellPx;
        f.text("score", 0, 0, "score " + std::to_string(state_.score),
               gfx::colors::quiet);
        if (!state_.alive) {
            f.text("dead", 10 * c, 0, "DEAD", gfx::colors::alert);
        }
        f.group("board", 0, c);
        const std::int64_t row = (state_.height + 3) * c;
        gfx::Button new_game("new-game", " new game ");
        new_game.set_measure(canvas().text());
        new_game.set_bounds(0, row, 12 * c, c);
        new_game.sketch(f, kNewGame);
        const std::string said = state_.paused ? " resume " : " pause ";
        gfx::Button pause("pause", said);
        pause.set_measure(canvas().text());
        pause.set_bounds(13 * c, row, (static_cast<std::int64_t>(said.size()) + 2) * c, c);
        pause.sketch(f, kPause);
    }

    /// A window that does not hold the board: the board said there, whole.
    void on_room(const gfx::AreaRoom&, loom::Mail&) {
        if (!canvas().holds(kBoardRef)) {
            present_board();
        }
    }

private:
    /// The world's own beat.
    void on_beat(const timer::TimerFired& f, loom::Mail& mail) {
        if (f.id != beat_.id()) {
            return; // a firing of a pace already given up, in flight when it was
        }
        if (state_.beat_ms <= 0 || state_.paused) {
            return;
        }
        ++state_.ticks;
        advance(mail);
    }

    /// One step of world time: the news published and the board told what changed. The step
    /// that reaches `pause_at` pauses the world after it, once.
    void advance(loom::Mail& mail) {
#if defined(SNAKE_WORLD_V2)
        claim_once(mail);
        if (awaiting_claim_) {
            return; // the window shows the board it holds until the inheritance lands
        }
#endif
        if (state_.snake.empty()) {
            seed(state_);
            mail.publish(visual_of(state_));
            new_board();
            return;
        }
        const State before = state_;
        const StepEvents ev = step(state_);
        if (ev.ate) {
            mail.publish(FoodEaten{});
        }
        if (ev.died) {
            mail.publish(SnakeDied{});
        }
        if (ev.moved || ev.died) {
            ++state_.steps;
            if (state_.pause_at > 0 && state_.steps >= state_.pause_at) {
                state_.paused = true;
                state_.pause_at = 0;
            }
            mail.publish(visual_of(state_));
            step_board(before);
        }
    }

    std::int64_t cell_of(const Pos& p) const { return p.y * state_.width + p.x; }
    std::int64_t food_cell() const {
        return state_.food.x == kNoFood ? -1 : cell_of(state_.food);
    }
    std::vector<std::int64_t> body() const {
        std::vector<std::int64_t> out;
        for (const Pos& p : state_.snake) {
            out.push_back(cell_of(p));
        }
        return out;
    }

    /// The board said whole: where the window does not hold it, or to start it over.
    void present_board() {
        canvas().present("board/grid", kBoardRef,
                         SnakeBoardData{state_.width, state_.height, kBoardCellPx, body(),
                                        state_.snake.empty() ? -1 : food_cell()});
    }

    /// A new game on the board: a change where the window holds it.
    void new_board() {
        if (!canvas().holds(kBoardRef)) {
            present_board();
            return;
        }
        canvas().change<SnakeBoardChanges>(kBoardRef, SnakeNewGame{body(), food_cell()});
    }

    /// One step's facts on the board: a change where the window holds it.
    void step_board(const State& before) {
        if (!canvas().holds(kBoardRef)) {
            present_board();
            return;
        }
        SnakeStep s;
        const bool moved = !(state_.snake.front().x == before.snake.front().x &&
                             state_.snake.front().y == before.snake.front().y);
        if (moved) {
            s.head = cell_of(state_.snake.front());
            if (state_.snake.size() == before.snake.size()) {
                s.freed = cell_of(before.snake.back());
            }
        }
        if (state_.food.x != before.food.x || state_.food.y != before.food.y) {
            s.food = food_cell();
        }
        s.dead = !state_.alive && before.alive;
        canvas().change<SnakeBoardChanges>(kBoardRef, s);
    }

    /// What a new game keeps of the state it replaces: the bindings and the beats taken.
    void copy_bindings(const State& from) {
        state_.keys = from.keys;
        state_.key_up = from.key_up;
        state_.key_right = from.key_right;
        state_.key_down = from.key_down;
        state_.key_left = from.key_left;
        state_.beat_ms = from.beat_ms;
        state_.ticks = from.ticks;
    }

    /// The beat at the state's pace, given up while the world is paused and asked again when it
    /// resumes.
    void keep_beat(loom::Mail& mail) {
        retime(mail);
        if (armed_ms_ <= 0) {
            return;
        }
        if (state_.paused && beat_.waiting()) {
            beat_.cancel(mail);
        } else if (!state_.paused && beat_.canceled()) {
            beat_.restart(mail);
        }
    }

    /// A pace written since the beat was asked for: the beat given up and asked again at the new
    /// pace, under a new id (a binding's id is its own for the weave's life).
    void retime(loom::Mail& mail) {
        if (state_.beat_ms == armed_ms_) {
            return;
        }
        beat_.cancel(mail);
        armed_ms_ = state_.beat_ms;
        if (armed_ms_ > 0) {
            beat_ = timers().repeat(std::string(kTickTimerId) + "." + std::to_string(++paces_),
                                    std::chrono::milliseconds(armed_ms_), &SnakeWorld::on_beat);
            beat_.restart(mail);
        }
    }

#if defined(SNAKE_WORLD_V2)
    // The claim correlation is a fixed constant: the heir makes one claim in its life. The
    // stamped-sender half is waived -- the heir reaches the steward by role and cannot know its
    // id -- so an in-process peer could forge a Bequest into the waiting window; loaded code is
    // trusted in-process by declaration at this tier, and that is named here rather than hidden.
    static constexpr std::uint64_t kClaimCorrelation = 0xC1A1;
    bool asked_ = false;
    bool awaiting_claim_ = false;

    /// The heir asks when it wakes, by the one name that outlives every swap: the steward's role.
    void claim_once(loom::Mail& mail) {
        if (asked_) {
            return;
        }
        asked_ = true;
        awaiting_claim_ = true;
        mail.ask_role(loom::kManagerRole, loom::ClaimBequest{kWorldRole}, kClaimCorrelation);
    }

    /// No inheritance: a fresh game, its board said whole, since the board the window holds may be
    /// a predecessor's at another size.
    void begin_fresh(loom::Mail& mail) {
        seed(state_);
        mail.publish(visual_of(state_));
        present_board();
    }
#endif

    Handle beat_;
    std::int64_t armed_ms_ = kTickMs;
    std::int64_t paces_ = 0;
};

} // namespace

ZEN_EXPORT_WEAVE(SnakeWorld)
