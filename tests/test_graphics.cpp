// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The graphics package proven headless, in two tiers: the vocabulary, the canvas, the terminal
// painter and the seams as pure arithmetic; and the terminal window, the directory, the two skins
// and the snake's board through the real kernel -- what is said once, what crosses a tick, what a
// skin chosen or a participant replaced costs on the bus, where a press goes, and every bound
// refused in words. The window's terminal is the suite's own where a press must be typed.

// main() and the framework live in doctest_main.cpp -- the shared one that refuses a run selecting
// zero cases.
#include "doctest.h"

#include "graphics_rig.hpp"
#include "lifecycle_door.hpp"
#include "office_manual.hpp"

#include "board.hpp"

#include "attention/vocabulary.hpp"
#include "external-host/connections-pane/vocabulary.hpp"
#include "graphics/canvas.hpp"
#include "graphics/drawing.hpp"
#include "graphics/elements.hpp"
#include "graphics/implied.hpp"
#include "graphics/ops.hpp"
#include "graphics/painting.hpp"
#include "graphics/term_paint.hpp"
#include "graphics/vocabulary.hpp"
#include "graphics/window/window.hpp"
#include "input/translate.hpp"
#include "manuals/zengine.graphics.hpp"
#include "manuals/zengine.window.terminal.hpp"
#include "workshop/attention_seam_vocabulary.hpp"
#include "workshop/guest_seam_vocabulary.hpp"
#include "workshop/pane_facets.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gfx = zengine::graphics;
namespace snake = zengine::snake;
using zengine::testing::graphics_rig::Rig;
using zengine::testing::graphics_rig::window_office;

namespace {

constexpr const char* kArea = "snake.world";

/// The cells one node paints for a medium of characters, as text rows from the area's 0,0.
std::vector<std::string> painted(const gfx::Sketch& s, const gfx::term::Look& look) {
    gfx::PaintTask task;
    std::int64_t rank = 1;
    for (const gfx::Node& n : s.nodes()) {
        task.nodes.push_back(gfx::PaintNode{n.path, {rank++}, n.op, n.args});
    }
    std::vector<std::string> rows;
    for (const gfx::NodePaint& np : gfx::term::paint_task(task, look).nodes) {
        for (const gfx::term::Cell& c : gfx::term::unpack(np.cells)) {
            while (rows.size() <= static_cast<std::size_t>(c.row)) {
                rows.emplace_back();
            }
            std::string& row = rows[static_cast<std::size_t>(c.row)];
            if (row.size() <= static_cast<std::size_t>(c.col)) {
                row.resize(static_cast<std::size_t>(c.col) + 1, ' ');
            }
            row[static_cast<std::size_t>(c.col)] = c.glyph;
        }
    }
    return rows;
}

gfx::term::Look plain_look() {
    gfx::term::Look l;
    l.fill = {{gfx::role::kFill, '#'}, {gfx::role::kAccent, '@'}};
    return l;
}

} // namespace

// ============================================================================
// Tier 1 -- the vocabulary, the canvas and the painter, as arithmetic
// ============================================================================

TEST_CASE("an operand's shape is implied by its operator: a body alone, read back by the gate") {
    const gfx::RectArgs r{1, 2, 3, 4, gfx::by_role(gfx::role::kAccent), gfx::no_paint()};
    const loom::Bytes body = gfx::body_of(r);
    const loom::Bytes whole = loom::bequeath_item(r);
    CHECK(body.size() < whole.size()); // the envelope is left off
    const auto back = gfx::from_body<gfx::RectArgs>(body);
    REQUIRE(back.has_value());
    CHECK(back->w == 3);
    CHECK(back->fill.role == gfx::role::kAccent);
    // Another shape's body is not this one's: the gate refuses it.
    CHECK_FALSE(gfx::from_body<gfx::TextArgs>(gfx::body_of(gfx::LineArgs{})).has_value());
}

TEST_CASE("a canvas sends the whole picture to a new window, then only what changed") {
    gfx::Canvas canvas("test.owner", "test.area");
    gfx::Sketch first;
    first.text("score", 0, 0, "score 0", gfx::by_role(gfx::role::kMuted));
    first.fill_rect("head", 12, 12, 12, 12, gfx::by_role(gfx::role::kAccent));
    // No window shows the area yet: nothing is queued.
    canvas.draw(first);
    CHECK(canvas.window().empty());

    CHECK(canvas.room(gfx::AreaRoom{"test.area", "terminal", 7, 0, 0, {3}}));
    // The same window, the same incarnation: nothing to draw again.
    CHECK_FALSE(canvas.room(gfx::AreaRoom{"test.area", "terminal", 7, 0, 0, {}}));
    CHECK(canvas.holds(3)); // the element the room named
    CHECK_FALSE(canvas.holds(4));
    gfx::Sketch again = first;
    canvas.draw(again);
    gfx::Sketch moved;
    moved.text("score", 0, 0, "score 0", gfx::by_role(gfx::role::kMuted));
    moved.fill_rect("head", 24, 12, 12, 12, gfx::by_role(gfx::role::kAccent));
    const gfx::Drawn d = gfx::difference(again, moved);
    REQUIRE(d.put.size() == 1);
    CHECK(d.put[0].path == "head");
    CHECK(d.removed.empty());

    // A window started again holds nothing this canvas sent: the next drawing goes whole, and of
    // its elements only those its room names.
    CHECK(canvas.room(gfx::AreaRoom{"test.area", "terminal", 8, 0, 0, {}}));
    CHECK_FALSE(canvas.holds(3));
}

TEST_CASE("the terminal painter draws all seven operators, each by its fallback") {
    gfx::Sketch s;
    s.group("g", 12, 0);
    s.rect("g/r", 0, 0, 36, 24, gfx::no_paint(), gfx::by_role(gfx::role::kFill));
    const gfx::term::Look look = plain_look();
    const auto rect = painted(s, look);
    REQUIRE(rect.size() == 2);
    CHECK(rect[0] == " +-+");
    CHECK(rect[1] == " +-+");

    gfx::Sketch t;
    t.text("t", 0, 0, "hi\xC3\xA9!", gfx::by_role(gfx::role::kFill));
    CHECK(painted(t, look)[0] == "hi?!"); // one cell a character; outside ASCII, one '?'

    gfx::Sketch l;
    l.line("l", 0, 0, 36, 0, gfx::by_role(gfx::role::kFill));
    CHECK(painted(l, look)[0] == "----");

    gfx::Sketch e;
    e.ellipse("e", 0, 0, 36, 36, gfx::by_role(gfx::role::kAccent));
    const auto ellipse = painted(e, look);
    REQUIRE(ellipse.size() >= 2);
    CHECK(ellipse[1].find('@') != std::string::npos);

    gfx::Sketch p;
    p.path("p", "M 0 0 L 36 0 L 36 36 L 0 36 Z", gfx::by_role(gfx::role::kFill));
    const auto path = painted(p, look);
    REQUIRE(path.size() == 3);
    CHECK(path[1] == "###");

    gfx::Sketch i;
    i.image("i", 0, 0, 12, 12, 1, 1, loom::Bytes{255, 255, 255, 255});
    CHECK(painted(i, look)[0] == "@"); // white, opaque: the ramp's brightest

    gfx::Sketch u;
    u.put("u", "sparkle", gfx::RectArgs{});
    CHECK(painted(u, look)[0] == "?sparkle"); // an operator this medium does not know, named
}

TEST_CASE("a group offsets and clips its children") {
    gfx::Sketch s;
    s.group("g", 24, 12, 24, 12);
    s.fill_rect("g/r", 0, 0, 120, 120, gfx::by_role(gfx::role::kFill));
    const auto rows = painted(s, plain_look());
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].empty());
    CHECK(rows[1] == "  ##"); // two cells wide, one tall: the clip
}

// ============================================================================
// Tier 2 -- the window, the directory, the skins and the board, through the real kernel
// ============================================================================

namespace {

/// The snake in a terminal window, arranged as a host's arranger arranges it.
struct Snake {
    Rig r;
    Snake() {
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, window_office()).valid());
        REQUIRE(r.load("snake-world-v1", WORLD_V1_SO, snake::kWorldRole).valid());
        r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kArea, snake::kWorldRole,
                  "classic");
    }
};

bool any_whole(const std::vector<gfx::Drawn>& drawn) {
    for (const gfx::Drawn& d : drawn) {
        if (d.whole) {
            return true;
        }
    }
    return false;
}

/// A terminal the suite types into and reads: what the window writes, kept; what it reads, the
/// bytes the suite queued, parsed by Zengine's own parser as a terminal's are.
struct TermLog {
    std::string typed;
    std::string written;
    std::vector<std::string> writes; ///< each write, one showing each
};

class FakeTerminal {
public:
    FakeTerminal() = default;
    explicit FakeTerminal(TermLog* log) : log_(log) {}
    void claim() {}
    zengine::surface::TerminalSize size() const { return {80, 30}; }
    void write(std::string_view bytes) {
        if (log_ != nullptr) {
            log_->written += bytes;
            log_->writes.emplace_back(bytes);
        }
    }
    std::vector<zengine::input::InputEvent> poll() {
        if (log_ == nullptr || log_->typed.empty()) {
            return {};
        }
        const std::string bytes = std::move(log_->typed);
        log_->typed.clear();
        return zengine::input::terminal_bytes_to_events(
            reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
    }

private:
    TermLog* log_ = nullptr;
};

using FakeWindow = gfx::window::WindowT<FakeTerminal>;

/// A window whose terminal is the suite's, beside the directory: the window mounted in process at
/// its office and activated as Loom's control door activates.
struct TypedWindow {
    Rig r;
    TermLog term;
    loom::WeaveId window;
    TypedWindow() {
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        auto w = std::make_unique<FakeWindow>(FakeTerminal{&term});
        FakeWindow* raw = w.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        window = r.bus.register_weave(std::move(w), std::move(g), window_office());
        raw->zen_set_self(window);
        const loom::WeaveId door = zengine::testing::mount_door(r.bus);
        zengine::testing::order_activation(r.bus, door, window, 1);
        r.drain();
    }
    /// The window reads what was typed, on its own beat.
    void type(const std::string& bytes) {
        term.typed += bytes;
        r.bus.send(window, loom::Message(loom::to_value(zengine::timer::TimerFired{
                               gfx::window::kPumpTimerId})));
        r.drain();
    }
    /// A left press at a cell of the terminal, 0-based, as an SGR report.
    void press(int col, int row) {
        type("\x1b[<0;" + std::to_string(col + 1) + ";" + std::to_string(row + 1) + "M");
    }
};

/// The snake in a window whose terminal is the suite's.
struct Typed : TypedWindow {
    Typed() {
        REQUIRE(r.load("snake-world-v1", WORLD_V1_SO, snake::kWorldRole).valid());
        r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kArea, snake::kWorldRole,
                  "classic");
    }
};

/// An owner the suite speaks for: it holds its office, and keeps what the window says to it.
struct OwnerHeard {
    std::vector<std::string> refused;
};

struct GraphicsTestOwner {
    std::int64_t heard = 0;
    ZEN_SHAPE(GraphicsTestOwner, 1, ZEN_FIELD(heard));
};

class Owner : public loom::WeaveBase<Owner, GraphicsTestOwner,
                                     loom::Accept<gfx::DrawnRefused, gfx::AreaRoom>,
                                     loom::Emit<gfx::Drawn, gfx::ElementPut, gfx::ElementChange>> {
public:
    explicit Owner(OwnerHeard& heard) : heard_(&heard) {}
    void on(const gfx::DrawnRefused& r, loom::Mail&) { heard_->refused.push_back(r.reason); }
    void on(const gfx::AreaRoom&, loom::Mail&) { ++state_.heard; }

private:
    OwnerHeard* heard_;
};

constexpr const char* kOwner = "test.owner";
constexpr const char* kDrawer = "test.drawer";

struct GraphicsTestBeat {
    ZEN_SHAPE(GraphicsTestBeat, 1);
};

struct GraphicsTestDrawer {
    std::int64_t beats = 0;
    ZEN_SHAPE(GraphicsTestDrawer, 1, ZEN_FIELD(beats));
};

/// A drawer on the drawing weave, as a stranger writes one: its picture, a board it presents
/// where the window does not hold it, and a change of it each beat. It never calls `show`.
class BeatDrawer
    : public gfx::DrawingWeave<BeatDrawer, GraphicsTestDrawer, loom::Accept<GraphicsTestBeat>,
                               loom::Emit<gfx::ElementPut, gfx::ElementChange>> {
public:
    using DrawingWeave::on;

    BeatDrawer() : DrawingWeave(kDrawer) {}

    void on(const GraphicsTestBeat&, loom::Mail&) {
        ++state_.beats;
        canvas().change<snake::SnakeBoardChanges>(1, snake::SnakeStep{state_.beats % 3, -1, -1,
                                                                      false});
    }

    void picture(gfx::Sketch& f) const {
        f.text("beats", 0, 0, "beats " + std::to_string(state_.beats),
               gfx::by_role(gfx::role::kFill));
        f.group("board", 0, gfx::kCellPx);
    }

    void on_room(const gfx::AreaRoom&, loom::Mail&) {
        if (!canvas().holds(1)) {
            canvas().present("board/grid", 1, snake::SnakeBoardData{3, 1, gfx::kCellPx, {0}, -1});
        }
    }
};

struct Drawing {
    Rig r;
    loom::WeaveId drawer;
    Drawing() {
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, window_office()).valid());
        auto d = std::make_unique<BeatDrawer>();
        BeatDrawer* raw = d.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        drawer = r.bus.register_weave(std::move(d), std::move(g), kDrawer);
        raw->zen_set_self(drawer);
        r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kDrawer, kDrawer, "classic");
    }
    void beat() {
        r.to_role(kDrawer, GraphicsTestBeat{});
        r.drain();
    }
};

struct Bounded {
    Rig r;
    OwnerHeard heard;
    loom::WeaveId owner;
    Bounded() {
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, window_office()).valid());
        auto o = std::make_unique<Owner>(heard);
        Owner* raw = o.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        owner = r.bus.register_weave(std::move(o), std::move(g), kOwner);
        raw->zen_set_self(owner);
        r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kOwner, kOwner, "classic");
    }
    template <class T>
    void say(const T& msg) {
        r.bus.office_send_to_role_as(owner, kOwner, window_office(),
                                     loom::Message(loom::to_value(msg)));
        r.drain();
    }
    void put(std::int64_t i) {
        say(gfx::ElementPut{kOwner, "e" + std::to_string(i), "SnakeBoardData", i + 1,
                            loom::bequeath_item(snake::SnakeBoardData{1, 1, 12, {}, -1}), false});
    }
};

} // namespace

TEST_CASE("the board is said once, then a tick is the beat and one change of the board, shown once") {
    Snake s;
    Rig& r = s.r;
    r.tick(); // seeds: a new game, sent to the board said at its room
    CHECK(r.count(0, "ElementPut", snake::kWorldRole) == 1);

    std::int64_t showings = r.shown().showings;
    std::int64_t score = r.heard.visuals.back().score;
    for (int t = 0; t < 5; ++t) {
        const std::size_t mark = r.log.size();
        r.tick();
        const bool ate = r.heard.visuals.back().score != score;
        score = r.heard.visuals.back().score;
        CHECK(r.count(mark, "SnakeTick") == 1);                         // the beat
        CHECK(r.count(mark, "ElementChange", snake::kWorldRole) == 1);  // one change of the board
        CHECK(r.count(mark, "ElementPut", snake::kWorldRole) == 0);     // never said again
        CHECK(r.count(mark, "Drawn", snake::kWorldRole) == (ate ? 1U : 0U)); // its frame, if it changed
        const std::int64_t now = r.shown().showings;
        CHECK(now == showings + 1); // shown once
        showings = now;
    }
}

TEST_CASE("the classic and the block skin paint the snake's board, each in its own glyphs") {
    Snake s;
    s.r.tick();
    const std::vector<std::string> classic = s.r.shown().rows;
    // The frame: the score on the area's first row, the board's border on the next.
    REQUIRE(classic.size() >= 20);
    CHECK(classic[0] == "score 0");
    CHECK(classic[1] == "+------------------------+");
    CHECK(classic[10].substr(0, 14) == "|   *      ooO"); // the seed, (10,8) to (12,8), and its food
    CHECK(classic[18] == "+------------------------+");
    CHECK(classic[19] == "[ new game ]");

    s.r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{kArea, "block"});
    s.r.drain();
    const std::vector<std::string> block = s.r.shown().rows;
    CHECK(block[1] == "==========================");
    CHECK(block[10].substr(0, 14) == "=   %      ##@");
}

TEST_CASE("an element said again starts over: drawn whole from its new data, nothing of it left") {
    Bounded b;
    b.say(gfx::ElementPut{kOwner, "e", "SnakeBoardData", 1,
                          loom::bequeath_item(snake::SnakeBoardData{3, 1, 12, {0}, -1}), false});
    CHECK(b.r.shown().rows[1] == "|O  |");
    b.say(gfx::ElementPut{kOwner, "e", "SnakeBoardData", 1,
                          loom::bequeath_item(snake::SnakeBoardData{3, 1, 12, {2}, -1}), false});
    CHECK(b.r.shown().rows[1] == "|  O|"); // the new data, and nothing of the old
    CHECK(b.heard.refused.empty());
}

TEST_CASE("a new game through the reset door reaches the board as a change: the window holds it") {
    Snake s;
    Rig& r = s.r;
    for (int t = 0; t < 4; ++t) {
        r.tick(); // the seed, and three steps right
    }
    CHECK(s.r.shown().rows[10].substr(0, 17) == "|   *         ooO");

    // The substrate's reset door: the whole state its defaults. The world's state names no window,
    // so the board the window holds is told the new game, never said again.
    const std::size_t mark = r.log.size();
    r.to_role(snake::kWorldRole, loom::PokeResetState{});
    r.tick();
    CHECK(r.count(mark, "ElementPut", snake::kWorldRole) == 0);
    const auto changes = r.read<gfx::ElementChange>(mark, snake::kWorldRole);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].kind == 1);                                  // SnakeNewGame
    CHECK(r.shown().rows[10] == "|   *      ooO           |"); // the new seed, and nothing else
}

TEST_CASE("a skin chosen mid-game: the window paints again, and nothing is drawn on the bus") {
    Snake s;
    Rig& r = s.r;
    r.tick();
    const std::int64_t calls = r.shown().painter_calls;
    const std::size_t mark = r.log.size();
    r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{kArea, "block"});
    r.drain();
    CHECK(r.count(mark, "Drawn") == 0);
    CHECK(r.count(mark, "ElementPut") == 0);
    CHECK(r.count(mark, "ElementChange") == 0);
    CHECK(r.count(mark, "AreaIntroduced") == 1); // the directory's word, to the window alone
    CHECK(r.shown().painter_calls == calls + 1);  // painted once, by the other code
}

TEST_CASE("the directory replaced mid-game: nothing is drawn") {
    Snake s;
    Rig& r = s.r;
    r.tick();
    const std::string before = r.screen();
    const std::size_t mark = r.log.size();
    const auto swapped = r.command(
        loom::SwapWeave{gfx::kGraphicsRole, "zengine-graphics", GRAPHICS_SO, /*graceful=*/false});
    REQUIRE_MESSAGE(swapped.kind == 0, swapped.text);
    // The new directory knows nothing: the window offers itself again on its hello, and the
    // arranger arranges again.
    r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kArea, snake::kWorldRole,
              "classic");
    CHECK(r.count(mark, "GraphicsReady") >= 1);
    CHECK(r.count(mark, "AreaRoom") >= 1); // the world told its room again...
    CHECK(r.count(mark, "Drawn", snake::kWorldRole) == 0); // ...and drew nothing
    CHECK(r.count(mark, "ElementPut", snake::kWorldRole) == 0);
    CHECK(r.screen() == before);
}

TEST_CASE("the directory's page answers for every shape it accepts, and names no other") {
    Rig r;
    REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
    const std::vector<std::string> described =
        described_shapes_of(r.bus, r.bus.role_holder(gfx::kGraphicsRole));
    REQUIRE_FALSE(described.empty());
    CHECK(check_office_manual(r.bus, gfx::kGraphicsRole) >= described.size());
    check_page_names_accepted(zengine::manual::pages::zengine_graphics::kManual, described);
}

TEST_CASE("the window reloaded: nothing is drawn, and it shows what it kept") {
    Snake s;
    Rig& r = s.r;
    r.tick();
    const gfx::WindowShown before = r.shown();
    const std::size_t mark = r.log.size();
    const auto reloaded =
        r.command(loom::ReloadWeave{"zengine-window-terminal", WINDOW_SO});
    REQUIRE_MESSAGE(reloaded.kind == 1, reloaded.text);
    CHECK(r.count(mark, "Drawn", snake::kWorldRole) == 0);
    CHECK(r.count(mark, "ElementPut", snake::kWorldRole) == 0);
    const gfx::WindowShown after = r.shown();
    CHECK(after.epoch == before.epoch); // the same incarnation, carried by its state
    CHECK(after.rows == before.rows);

    r.tick(); // and the game goes on in it, one change a tick
    CHECK(r.count(mark, "ElementPut", snake::kWorldRole) == 0);
}

TEST_CASE("the window replaced: the frame drawn whole once and the board said once") {
    Snake s;
    Rig& r = s.r;
    r.tick();
    const gfx::WindowShown before = r.shown();
    const std::size_t mark = r.log.size();
    const auto swapped = r.command(loom::SwapWeave{window_office(), "zengine-window-terminal",
                                                   WINDOW_SO, /*graceful=*/false});
    REQUIRE_MESSAGE(swapped.kind == 0, swapped.text);
    const auto drawn = r.read<gfx::Drawn>(mark, snake::kWorldRole);
    CHECK(drawn.size() == 1);
    CHECK(any_whole(drawn));
    CHECK(r.count(mark, "ElementPut", snake::kWorldRole) == 1);
    const gfx::WindowShown after = r.shown();
    CHECK(after.epoch != before.epoch);
    CHECK(after.rows == before.rows);
}

TEST_CASE("the window's page answers for every shape it accepts, and names no other") {
    Rig r;
    REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, window_office()).valid());
    const std::vector<std::string> described =
        described_shapes_of(r.bus, r.bus.role_holder(window_office()));
    REQUIRE_FALSE(described.empty());
    CHECK(check_office_manual(r.bus, window_office()) >= described.size());
    check_page_names_accepted(zengine::manual::pages::zengine_window_terminal::kManual, described);
}

TEST_CASE("the world reloaded: its room asked, its frame drawn whole once, its board not said again") {
    Snake s;
    Rig& r = s.r;
    r.tick();
    r.tick();
    const std::vector<std::string> before = r.shown().rows;
    const std::size_t mark = r.log.size();
    const auto reloaded = r.command(loom::ReloadWeave{"snake-world-v1", WORLD_V1_SO});
    REQUIRE_MESSAGE(reloaded.kind == 1, reloaded.text);
    CHECK(r.count(mark, "RoomAsked", snake::kWorldRole) == 1);
    // The window's answer names the board it still presents; the world's state names no window.
    const auto rooms = r.read<gfx::AreaRoom>(mark, window_office());
    REQUIRE(rooms.size() == 1);
    CHECK(rooms[0].elements == std::vector<std::int64_t>{snake::kBoardRef});
    for (const auto* schema : {loom::schema_of<snake::v1::SnakeWorldState>().get(),
                               loom::schema_of<snake::v2::SnakeWorldState>().get()}) {
        for (const loom::Field& f : schema->fields()) {
            CHECK_MESSAGE(f.name.find("window") == std::string::npos, f.name);
            CHECK_MESSAGE(f.name.find("epoch") == std::string::npos, f.name);
        }
    }
    const auto drawn = r.read<gfx::Drawn>(mark, snake::kWorldRole);
    CHECK(drawn.size() == 1);
    CHECK(any_whole(drawn));
    CHECK(r.count(mark, "ElementPut", snake::kWorldRole) == 0);
    CHECK(r.shown().rows == before);

    const std::size_t next = r.log.size();
    r.tick(); // the reloaded world steps its board by a change
    CHECK(r.count(next, "ElementChange", snake::kWorldRole) == 1);
}

namespace {

/// The pictures a terminal shows, one a write: `grid` (the rows a showing ended on) with each write
/// laid over it in turn -- a cursor move, an ink, then glyphs -- as a terminal lays them.
std::vector<std::vector<std::string>> pictures_of(std::vector<std::string> grid,
                                                  const std::vector<std::string>& writes) {
    std::vector<std::vector<std::string>> out;
    for (const std::string& w : writes) {
        std::size_t row = 0, col = 0;
        for (std::size_t i = 0; i < w.size(); ++i) {
            if (w[i] == '\x1b' && i + 1 < w.size() && w[i + 1] == '[') {
                std::size_t end = i + 2;
                while (end < w.size() && !(w[end] >= '@' && w[end] <= '~')) {
                    ++end;
                }
                if (end < w.size() && w[end] == 'H') {
                    const std::string at = w.substr(i + 2, end - i - 2);
                    const std::size_t semi = at.find(';');
                    row = static_cast<std::size_t>(std::stoi(at.substr(0, semi)) - 1);
                    col = static_cast<std::size_t>(std::stoi(at.substr(semi + 1)) - 1);
                }
                i = end;
                continue;
            }
            if (grid.size() <= row) {
                grid.resize(row + 1);
            }
            if (grid[row].size() <= col) {
                grid[row].resize(col + 1, ' ');
            }
            grid[row][col++] = w[i];
        }
        out.push_back(grid);
    }
    return out;
}

/// How many cells below the score row hold `glyph`: the board and the button beneath it.
std::size_t board_cells(const std::vector<std::string>& rows, char glyph) {
    std::size_t n = 0;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        for (const char c : rows[r]) {
            n += c == glyph ? 1u : 0u;
        }
    }
    return n;
}

std::vector<std::string> trimmed(std::vector<std::string> rows) {
    for (std::string& r : rows) {
        r.erase(r.find_last_not_of(' ') + 1);
    }
    while (!rows.empty() && rows.back().empty()) {
        rows.pop_back();
    }
    return rows;
}

} // namespace

TEST_CASE("a grown world shows the old board until its inheritance lands, then the inherited board "
          "whole, and never a blank or a seeded one") {
    Typed t;
    Rig& r = t.r;
    for (int i = 0; i < 4; ++i) {
        r.tick();
    }
    const gfx::WindowShown before = r.shown();
    REQUIRE_FALSE(r.heard.visuals.empty());
    const snake::SnakeVisual old = r.heard.visuals.back();
    REQUIRE(old.width == 24);
    const std::size_t length = old.snake.size();
    REQUIRE(board_cells(before.rows, 'O') == 1);
    REQUIRE(board_cells(before.rows, 'o') == length - 1);
    const std::size_t mark = r.log.size();
    const std::size_t shown_before = t.term.writes.size();

    // THE WORLD GROWS: the graceful swap to the larger world, then its first beat.
    const auto grown = r.command(
        loom::SwapWeave{snake::kWorldRole, "snake-world-v2", WORLD_V2_SO, /*graceful=*/true});
    REQUIRE_MESSAGE(grown.kind == 0, grown.text);
    r.tick();

    // EVERY PICTURE SHOWN SINCE holds one head and the whole body: the old board, or the
    // inherited one -- never a board with no snake on it.
    const std::vector<std::string> writes(t.term.writes.begin() +
                                              static_cast<std::ptrdiff_t>(shown_before),
                                          t.term.writes.end());
    REQUIRE_FALSE(writes.empty());
    const auto pictures = pictures_of(before.rows, writes);
    for (std::size_t i = 0; i < pictures.size(); ++i) {
        CAPTURE(i);
        CAPTURE(trimmed(pictures[i]));
        CHECK(board_cells(pictures[i], 'O') == 1);
        CHECK(board_cells(pictures[i], 'o') == length - 1);
    }
    // ...THE LAST OF THEM IS WHAT THE WINDOW SAYS IT SHOWS: the inherited board.
    const gfx::WindowShown after = r.shown();
    CHECK(trimmed(pictures.back()) == trimmed(after.rows));

    // THE BOARD WAS SAID WHOLE, ONCE THE INHERITANCE LANDED, and never changed before: the first
    // word about it from the grown world is the inherited board, at its new size.
    const auto puts = r.read<gfx::ElementPut>(mark, snake::kWorldRole);
    CHECK(r.count(mark, "ElementChange", snake::kWorldRole) == 0);
    REQUIRE(puts.size() == 1);
    const auto data = loom::claim_item<snake::SnakeBoardData>(puts[0].data);
    REQUIRE(data.has_value());
    CHECK(data->cols == 48);
    CHECK(data->rows == 24);
    REQUIRE(data->body.size() == length);
    for (std::size_t i = 0; i < length; ++i) {
        CHECK(data->body[i] == (old.snake[i].y + 4) * 48 + old.snake[i].x + 12);
    }
}

TEST_CASE("a world grown with no letter to inherit starts a fresh game, said whole at its new size") {
    Typed t;
    Rig& r = t.r;
    r.tick();
    const std::size_t mark = r.log.size();
    const std::size_t shown_before = t.term.writes.size();
    const std::vector<std::string> before = r.shown().rows;

    // A SWAP THAT ASKS FOR NO LETTER: the heir's claim is refused, and its game is its own.
    const auto grown = r.command(
        loom::SwapWeave{snake::kWorldRole, "snake-world-v2", WORLD_V2_SO, /*graceful=*/false});
    REQUIRE_MESSAGE(grown.kind == 0, grown.text);
    r.tick();

    CHECK(r.count(mark, "ElementChange", snake::kWorldRole) == 0);
    const auto puts = r.read<gfx::ElementPut>(mark, snake::kWorldRole);
    REQUIRE(puts.size() == 1);
    const auto data = loom::claim_item<snake::SnakeBoardData>(puts[0].data);
    REQUIRE(data.has_value());
    CHECK(data->cols == 48);
    CHECK(data->rows == 24);
    CHECK(data->body == std::vector<std::int64_t>{12 * 48 + 24, 12 * 48 + 23, 12 * 48 + 22});
    // ...AND EVERY PICTURE SHOWN SINCE HOLDS A SNAKE: the old board, then the fresh one.
    const std::vector<std::string> writes(t.term.writes.begin() +
                                              static_cast<std::ptrdiff_t>(shown_before),
                                          t.term.writes.end());
    for (const auto& picture : pictures_of(before, writes)) {
        CAPTURE(trimmed(picture));
        CHECK(board_cells(picture, 'O') == 1);
    }
}

TEST_CASE("a drawer that never calls show still shows once a beat, its nodes before its elements") {
    Drawing d;
    // Its room: its picture whole and its board said, shown.
    REQUIRE(d.r.shown().rows.size() >= 4);
    CHECK(d.r.shown().rows[0] == "beats 0");
    CHECK(d.r.shown().rows[2] == "|O  |");

    const std::int64_t showings = d.r.shown().showings;
    const std::size_t mark = d.r.log.size();
    d.beat();
    const auto drawn = d.r.read<gfx::Drawn>(mark, kDrawer);
    const auto changes = d.r.read<gfx::ElementChange>(mark, kDrawer);
    REQUIRE(drawn.size() == 1);
    REQUIRE(changes.size() == 1);
    CHECK(drawn[0].more);         // the beat's nodes first, more to follow
    CHECK_FALSE(changes[0].more); // its element last: the beat's drawing is whole
    const gfx::WindowShown after = d.r.shown();
    CHECK(after.showings == showings + 1); // shown once
    CHECK(after.rows[0] == "beats 1");
    CHECK(after.rows[2] == "|oO |");
}

TEST_CASE("a drawing weave takes a room only from the window it names") {
    Drawing d;
    const std::size_t mark = d.r.log.size();
    // A room from a sender that holds no window's office: not taken.
    d.r.to_role(kDrawer, gfx::AreaRoom{kDrawer, "elsewhere", 99, 0, 0, {}});
    d.r.drain();
    const std::int64_t showings = d.r.shown().showings;
    d.beat();
    CHECK(d.r.shown().showings == showings + 1); // the beat still shows in its own window
    CHECK(d.r.shown().rows[0] == "beats 1");
    CHECK(d.r.count(mark, "ElementPut", kDrawer) == 0);
}

TEST_CASE("an area given another owner starts empty, its room naming none of the last owner's") {
    Bounded b;
    b.put(0);
    REQUIRE(b.r.shown().elements == 1);
    OwnerHeard heir_heard;
    auto heir = std::make_unique<Owner>(heir_heard);
    Owner* raw = heir.get();
    loom::Grant g = loom::emit_default_grant(*raw);
    raw->zen_set_self(b.r.bus.register_weave(std::move(heir), std::move(g), "test.heir"));

    const std::size_t mark = b.r.log.size();
    b.r.to_role(gfx::kGraphicsRole,
                gfx::AreaPlaced{kOwner, gfx::kTerminalWindow, "test.heir", 0, 0,
                                50 * gfx::kCellPx, 22 * gfx::kCellPx, 0, true});
    b.r.drain();
    const auto rooms = b.r.read<gfx::AreaRoom>(mark, window_office());
    REQUIRE(rooms.size() == 1);
    CHECK(rooms[0].elements.empty());
    CHECK(b.r.shown().elements == 0);
}

/// EVERYTHING ONE PARTICIPANT SENDS while this is held, delivered or refused: its shape's name.
struct SentBy {
    loom::Switchboard& bus;
    loom::ObserverId tap = 0;
    std::vector<std::string> sent;
    SentBy(loom::Switchboard& b, loom::WeaveId who) : bus(b) {
        tap = bus.add_observer([this, who](const loom::BusEvent& e) {
            if ((e.kind == loom::EventKind::Delivered || e.kind == loom::EventKind::Refused) &&
                e.sender == who) {
                sent.push_back(e.schema_name);
            }
        });
    }
    ~SentBy() { bus.remove_observer(tap); }
    SentBy(const SentBy&) = delete;
    SentBy& operator=(const SentBy&) = delete;
};

TEST_CASE("a press nothing drawn there takes, in an area not placed to hear the pointer, sends "
          "nothing") {
    Typed t;
    t.r.tick();
    const std::size_t mark = t.r.log.size();
    SentBy window(t.r.bus, t.window);
    t.press(13, 10); // the head's cell: board cell (12,8), one cell in from the border, below row 0
    CHECK(window.sent.empty()); // no press, no pointer event, and so no refusal either
    CHECK(t.r.read<zengine::input::PointerButton>(mark).empty());
    for (std::size_t i = mark; i < t.r.log.size(); ++i) {
        CHECK(t.r.log[i].addressed_role != snake::kWorldRole);
    }
}

/// AN OWNER OF AN AREA PLACED TO HEAR THE POINTER: it keeps each press the window gives it.
struct PointerHeard {
    std::vector<zengine::input::PointerButton> pressed;
};

class PointerOwner
    : public loom::WeaveBase<PointerOwner, GraphicsTestOwner,
                             loom::Accept<gfx::AreaRoom, zengine::input::PointerButton>,
                             loom::Emit<>> {
public:
    explicit PointerOwner(PointerHeard& heard) : heard_(&heard) {}
    void on(const gfx::AreaRoom&, loom::Mail&) { ++state_.heard; }
    void on(const zengine::input::PointerButton& b, loom::Mail& mail) {
        if (mail.authored_from_role(window_office())) heard_->pressed.push_back(b);
    }

private:
    PointerHeard* heard_;
};

TEST_CASE("a press nothing drawn there takes reaches the owner of an area placed to hear the "
          "pointer, at the area's own point") {
    Typed t;
    PointerHeard heard;
    auto owner = std::make_unique<PointerOwner>(heard);
    PointerOwner* raw = owner.get();
    loom::Grant g = loom::emit_default_grant(*raw);
    raw->zen_set_self(t.r.bus.register_weave(std::move(owner), std::move(g), "test.pointer"));
    // BESIDE THE BOARD, EMPTY: nothing is drawn there, so nothing there takes a press.
    gfx::AreaPlaced placed{"test.pointer", gfx::kTerminalWindow, "test.pointer", 52 * gfx::kCellPx,
                           0, 10 * gfx::kCellPx, 2 * gfx::kCellPx, 1, false};
    placed.pointer = true;
    t.r.to_role(gfx::kGraphicsRole, placed);
    t.r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{"test.pointer", "classic"});
    t.r.drain();
    t.press(54, 1);
    REQUIRE(heard.pressed.size() == 1);
    CHECK(heard.pressed[0].button == 1);
    CHECK(heard.pressed[0].pressed);
    CHECK(heard.pressed[0].x == 2 * gfx::kCellPx + gfx::kCellPx / 2);
    CHECK(heard.pressed[0].y == gfx::kCellPx + gfx::kCellPx / 2);
    CHECK(heard.pressed[0].space == zengine::input::space::kPixels); // the area's pixels
    // ...AND THE SAME PRESS ON THE BOARD, PLACED WITHOUT IT, STILL REACHES NO ONE.
    t.press(13, 10);
    CHECK(heard.pressed.size() == 1);
}

TEST_CASE("the new-game button is the world's action, and a new game reaches the board as a change") {
    Typed t;
    for (int i = 0; i < 4; ++i) {
        t.r.tick();
    }
    // ONLY THE WINDOW SHOWING THE WORLD'S AREA ASKS ITS ACTIONS, about that area: the same action
    // from anyone else, or from the window about another area, is delivered and does nothing.
    const std::size_t asked = t.r.log.size();
    t.r.to_role(snake::kWorldRole, gfx::ActionRequested{kArea, "new-game"});
    t.r.bus.office_send_to_role_as(
        t.window, window_office(), snake::kWorldRole,
        loom::Message(loom::to_value(gfx::ActionRequested{"test.elsewhere", "new-game"})));
    t.r.drain();
    REQUIRE(t.r.read<gfx::ActionRequested>(asked).size() == 2);
    const auto from_window = t.r.read<gfx::ActionRequested>(asked, window_office());
    REQUIRE(from_window.size() == 1);
    CHECK(from_window[0].area == "test.elsewhere");
    CHECK(t.r.count(asked, "ElementChange", snake::kWorldRole) == 0);
    CHECK(t.r.count(asked, "SnakeVisual") == 0);

    const std::size_t mark = t.r.log.size();
    t.press(3, 19); // "[ new game ]"
    const auto requested = t.r.read<gfx::ActionRequested>(mark);
    REQUIRE(requested.size() == 1);
    CHECK(requested[0].action == "new-game");
    const auto changes = t.r.read<gfx::ElementChange>(mark, snake::kWorldRole);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].kind == 1); // SnakeNewGame, the board's second change
    CHECK(t.r.count(mark, "ElementPut", snake::kWorldRole) == 0);
    CHECK(t.r.heard.visuals.back().snake.front().x == 12); // seeded afresh
}

TEST_CASE("keys go from the window to the owner of each area placed to hear them") {
    Typed t;
    t.r.tick();
    t.type("w"); // the world's own up key
    t.r.tick();
    CHECK(t.r.heard.visuals.back().snake.front().y == 7);
}

namespace {

/// WHAT AN OWNER PLACED IN THE WINDOW WAS GIVEN, in order, as the window's office.
struct KeysHeard {
    std::vector<std::string> said;
};

/// AN OWNER THAT TAKES KEYS AND NO TEXT.
class KeysOwner
    : public loom::WeaveBase<KeysOwner, GraphicsTestOwner,
                             loom::Accept<gfx::AreaRoom, zengine::input::KeyPressed>,
                             loom::Emit<>> {
public:
    explicit KeysOwner(KeysHeard& heard) : heard_(&heard) {}
    void on(const gfx::AreaRoom&, loom::Mail&) { ++state_.heard; }
    void on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        if (mail.authored_from_role(window_office())) heard_->said.push_back("key " + k.name);
    }

private:
    KeysHeard* heard_;
};

/// AN OWNER THAT TAKES KEYS AND TEXT.
class TextOwner
    : public loom::WeaveBase<TextOwner, GraphicsTestOwner,
                             loom::Accept<gfx::AreaRoom, zengine::input::KeyPressed,
                                          zengine::input::TextEntered>,
                             loom::Emit<>> {
public:
    explicit TextOwner(KeysHeard& heard) : heard_(&heard) {}
    void on(const gfx::AreaRoom&, loom::Mail&) { ++state_.heard; }
    void on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        if (mail.authored_from_role(window_office())) heard_->said.push_back("key " + k.name);
    }
    void on(const zengine::input::TextEntered& e, loom::Mail& mail) {
        if (mail.authored_from_role(window_office())) heard_->said.push_back("text " + e.text);
    }

private:
    KeysHeard* heard_;
};

} // namespace

TEST_CASE("what a key types goes with the key from the window to the owner of each area placed to "
          "hear keys, and to no other; an owner that takes no text refuses it on the bus") {
    TypedWindow t;
    t.r.to_role(gfx::kGraphicsRole, gfx::SkinOffered{"classic", PAINT_CLASSIC_SO});
    t.r.drain();
    const auto mount = [&t](auto made, const char* office, bool keys, std::int64_t y) {
        auto* raw = made.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        const loom::WeaveId id = t.r.bus.register_weave(std::move(made), std::move(g), office);
        raw->zen_set_self(id);
        t.r.to_role(gfx::kGraphicsRole, gfx::AreaPlaced{office, gfx::kTerminalWindow, office, 0, y,
                                                        10 * gfx::kCellPx, 2 * gfx::kCellPx, 1,
                                                        keys});
        t.r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{office, "classic"});
        t.r.drain();
        return id;
    };
    KeysHeard typing, deaf, keys_only;
    const loom::WeaveId typing_id =
        mount(std::make_unique<TextOwner>(typing), "test.typing", true, 0);
    (void)mount(std::make_unique<TextOwner>(deaf), "test.deaf", false, 3 * gfx::kCellPx);
    const loom::WeaveId keys_id =
        mount(std::make_unique<KeysOwner>(keys_only), "test.keys-only", true, 6 * gfx::kCellPx);
    std::vector<std::pair<std::uint64_t, std::string>> refused;
    const loom::ObserverId tap = t.r.bus.add_observer([&refused](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Refused) refused.emplace_back(e.target.value, e.schema_name);
    });
    const std::size_t mark = t.r.log.size();
    t.type("hi");
    t.r.bus.remove_observer(tap);
    // EACH KEY, AND THEN THE CHARACTER IT TYPED, to the owner placed to hear keys, as the window's
    // office, to that owner alone...
    CHECK(typing.said == std::vector<std::string>{"key H", "text h", "key I", "text i"});
    std::size_t text_delivered = 0;
    for (std::size_t i = mark; i < t.r.log.size(); ++i) {
        if (t.r.log[i].shape != zengine::input::TextEntered::zen_name) continue;
        CHECK(t.r.log[i].authored_role == window_office());
        CHECK(t.r.log[i].target == typing_id.value);
        ++text_delivered;
    }
    CHECK(text_delivered == 2);
    // ...NOTHING TO ONE PLACED WITHOUT THEM...
    CHECK(deaf.said.empty());
    // ...AND TO AN OWNER THAT TAKES KEYS ALONE, its keys, the bus refusing the text it does not
    // take, and nothing else refused.
    CHECK(keys_only.said == std::vector<std::string>{"key H", "key I"});
    CHECK(refused == std::vector<std::pair<std::uint64_t, std::string>>{
                         {keys_id.value, zengine::input::TextEntered::zen_name},
                         {keys_id.value, zengine::input::TextEntered::zen_name}});
}

namespace {

/// EVERY WINDOW OFFER AN ARRANGER BESIDE THE DIRECTORY HEARD, and whether the window's office said
/// it.
struct OffersHeard {
    std::vector<gfx::WindowOffered> offers;
    std::vector<bool> from_window;
};

class OfferFollower
    : public loom::WeaveBase<OfferFollower, GraphicsTestOwner, loom::Accept<gfx::WindowOffered>,
                             loom::Emit<>> {
public:
    explicit OfferFollower(OffersHeard& heard) : heard_(&heard) {}
    void on(const gfx::WindowOffered& w, loom::Mail& mail) {
        heard_->offers.push_back(w);
        heard_->from_window.push_back(mail.authored_from_role(window_office()));
    }

private:
    OffersHeard* heard_;
};

} // namespace

TEST_CASE("the window offers itself as its own office to whoever follows it: the directory, which "
          "introduces its areas, and an arranger beside it, on each activation and new directory") {
    Rig r;
    REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
    OffersHeard heard;
    auto made = std::make_unique<OfferFollower>(heard);
    OfferFollower* raw = made.get();
    loom::Grant g = loom::emit_default_grant(*raw);
    const loom::WeaveId follower =
        r.bus.register_weave(std::move(made), std::move(g), "test.arranger");
    raw->zen_set_self(follower);
    const std::size_t mark = r.log.size();
    REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, window_office()).valid());
    // ACTIVATED: one offer, published as the window's office, heard by the directory and the
    // arranger alike.
    REQUIRE(heard.offers.size() == 1);
    CHECK(heard.from_window[0]);
    CHECK(heard.offers[0].window == gfx::kTerminalWindow);
    CHECK(heard.offers[0].cols > 0);
    CHECK(heard.offers[0].rows > 0);
    std::set<std::uint64_t> to;
    for (std::size_t i = mark; i < r.log.size(); ++i) {
        if (r.log[i].shape != gfx::WindowOffered::zen_name) continue;
        CHECK(r.log[i].authored_role == window_office());
        CHECK(r.log[i].addressed_role.empty());
        to.insert(r.log[i].target);
    }
    CHECK(to == std::set<std::uint64_t>{r.bus.role_holder(gfx::kGraphicsRole).value,
                                        follower.value});
    // THE DIRECTORY HEARD IT AS THE WINDOW'S: an area placed now is introduced, and the window
    // tells its owner the room.
    OwnerHeard owned;
    auto o = std::make_unique<Owner>(owned);
    Owner* oraw = o.get();
    loom::Grant og = loom::emit_default_grant(*oraw);
    const loom::WeaveId owner = r.bus.register_weave(std::move(o), std::move(og), kOwner);
    oraw->zen_set_self(owner);
    const std::size_t placed = r.log.size();
    r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kOwner, kOwner, "classic");
    std::size_t rooms = 0;
    for (std::size_t i = placed; i < r.log.size(); ++i) {
        if (r.log[i].shape == gfx::AreaRoom::zen_name && r.log[i].target == owner.value &&
            r.log[i].authored_role == window_office()) {
            ++rooms;
        }
    }
    CHECK(rooms >= 1);
    // A NEW DIRECTORY: the window offers itself again, the same incarnation, to both.
    const std::size_t again = r.log.size();
    r.bus.office_send_to_role_as(r.bus.role_holder(gfx::kGraphicsRole), gfx::kGraphicsRole,
                                 window_office(),
                                 loom::Message(loom::to_value(gfx::GraphicsReady{})));
    r.drain();
    REQUIRE(heard.offers.size() == 2);
    CHECK(heard.from_window[1]);
    CHECK(heard.offers[1].epoch == heard.offers[0].epoch);
    CHECK(r.count(again, gfx::WindowOffered::zen_name, window_office()) == 2);
}

TEST_CASE("an area at its element bound still takes a change, and a record said again is itself") {
    Bounded b;
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(gfx::kMaxElementsPerArea); ++i) {
        b.put(i);
    }
    REQUIRE(b.heard.refused.empty());
    CHECK(b.r.shown().elements == static_cast<std::int64_t>(gfx::kMaxElementsPerArea));
    b.say(gfx::ElementChange{8, 0, gfx::body_of(snake::SnakeStep{0, -1, -1, false}), false});
    CHECK(b.heard.refused.empty());

    // Records: the area's bound, met, and one said again at its own path.
    gfx::Drawn d;
    d.area = kOwner;
    for (std::size_t i = 0; i < gfx::kMaxElementsPerArea; ++i) {
        d.put.push_back(gfx::Node{"r" + std::to_string(i), gfx::op::kGroup, gfx::body_of(gfx::GroupArgs{})});
        d.said.push_back(gfx::ElementRecord{"r" + std::to_string(i), {}, {}, "act"});
    }
    b.say(d);
    REQUIRE(b.heard.refused.empty());
    gfx::Drawn again;
    again.area = kOwner;
    again.said.push_back(gfx::ElementRecord{"r5", {}, {}, "another"});
    b.say(again);
    CHECK(b.heard.refused.empty());
}

TEST_CASE("a bound reached is refused to its drawer, in words") {
    Bounded b;
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(gfx::kMaxElementsPerArea); ++i) {
        b.put(i);
    }
    REQUIRE(b.heard.refused.empty());
    b.put(static_cast<std::int64_t>(gfx::kMaxElementsPerArea));
    REQUIRE(b.heard.refused.size() == 1);
    CHECK(b.heard.refused[0].find("presents 1024 elements already") != std::string::npos);

    gfx::Drawn d;
    d.area = kOwner;
    for (std::size_t i = 0; i <= gfx::kMaxElementsPerArea; ++i) {
        d.put.push_back(gfx::Node{"r" + std::to_string(i), gfx::op::kGroup, gfx::body_of(gfx::GroupArgs{})});
        d.said.push_back(gfx::ElementRecord{"r" + std::to_string(i), {}, {}, "act"});
    }
    b.say(d);
    REQUIRE(b.heard.refused.size() == 2);
    CHECK(b.heard.refused[1].find("would hold more than 1024 elements") != std::string::npos);
}

TEST_CASE("an element whose data does not read is refused to its drawer, in words") {
    Bounded b;
    b.say(gfx::ElementPut{kOwner, "e", "SnakeBoardData", 1,
                          loom::bequeath_item(snake::SnakeStep{}), false});
    REQUIRE(b.heard.refused.size() == 1);
    CHECK(b.heard.refused[0].find("its data does not read as SnakeBoardData") != std::string::npos);
    CHECK(b.r.shown().elements == 0); // nothing of it stays

    b.say(gfx::ElementPut{kOwner, "f", "SnakeLadderData", 2, {}, false});
    REQUIRE(b.heard.refused.size() == 2);
    CHECK(b.heard.refused[1].find("no code offered presents 'SnakeLadderData'") != std::string::npos);
}

TEST_CASE("a library that is no skin is refused in words, and paints nothing") {
    Snake s;
    s.r.to_role(gfx::kGraphicsRole, gfx::SkinOffered{"board", SNAKE_SO_BOARD});
    s.r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{kArea, "board"});
    s.r.drain();
    s.r.tick();
    const gfx::WindowShown shown = s.r.shown();
    CHECK(shown.last_refusal.find("exports no skin") != std::string::npos);
}

TEST_CASE("a tick that changes the board and the score is shown once") {
    Snake s;
    Rig& r = s.r;
    r.tick();
    for (int t = 0; t < 400; ++t) {
        const snake::SnakeVisual v = r.heard.visuals.back();
        REQUIRE(v.alive);
        // The way the snake heads, from its head and neck; a turn toward the food, never back.
        const snake::Pos h = v.snake[0], n = v.snake[1];
        const std::int64_t heading = h.x > n.x ? snake::kRight
                                     : h.x < n.x ? snake::kLeft
                                     : h.y > n.y ? snake::kDown
                                                 : snake::kUp;
        // Greedy: a way that closes on the food, never back and never into a wall.
        const auto viable = [&](std::int64_t d) {
            if ((heading + 2) % 4 == d) {
                return false;
            }
            const std::int64_t x = h.x + (d == snake::kRight) - (d == snake::kLeft);
            const std::int64_t y = h.y + (d == snake::kDown) - (d == snake::kUp);
            return x >= 0 && x < v.width && y >= 0 && y < v.height;
        };
        std::int64_t want = viable(heading) ? heading : -1;
        for (const auto& [closer, d] :
             {std::pair{v.food.x > h.x, snake::kRight}, std::pair{v.food.x < h.x, snake::kLeft},
              std::pair{v.food.y > h.y, snake::kDown}, std::pair{v.food.y < h.y, snake::kUp}}) {
            if (closer && viable(d)) {
                want = d;
                break;
            }
        }
        for (std::int64_t d = 0; want < 0 && d < 4; ++d) {
            if (viable(d)) {
                want = d;
            }
        }
        r.to_role(snake::kWorldRole, snake::SnakeTurn{want});
        const std::int64_t before = r.shown().showings;
        const std::size_t mark = r.log.size();
        r.tick();
        if (r.heard.visuals.back().score > v.score) {
            CHECK(r.count(mark, "Drawn", snake::kWorldRole) == 1);         // the score
            CHECK(r.count(mark, "ElementChange", snake::kWorldRole) == 1); // the board
            CHECK(r.shown().showings == before + 1);                       // one showing
            return;
        }
    }
    FAIL("the snake never ate");
}

namespace {

inline constexpr zengine::manual::Section kFacetedSections[] = {
    {"GraphicsTestBeat", "Commands", "One beat for the faceted drawer."},
};

inline constexpr zengine::manual::Manual kFacetedManual{
    kDrawer, "faceted drawer", "sha256:test", kFacetedSections, 1};

/// A weave that composes both `DrawingFacet` and `ManualFacet` and defines its own
/// `after_delivery(loom::Mail&)`. Also repeats `AreaRoom` in its own `Accept` to exercise
/// deduplication in the mounted schema union.
class FacetedDrawer
    : public zengine::With<
          gfx::DrawingFacet,
          zengine::manual::ManualFacet<kFacetedManual>>::
          Weave<FacetedDrawer, GraphicsTestDrawer,
                loom::Accept<GraphicsTestBeat, gfx::AreaRoom>> {
public:
    using Weave::on;

    FacetedDrawer() : Weave(kDrawer) {}

    void on(const GraphicsTestBeat&, loom::Mail&) { ++state_.beats; }

    void on_room(const gfx::AreaRoom&, loom::Mail&) { ++rooms_; }

    void after_delivery(loom::Mail&) {
        after_hook_beats_ = state_.beats;
        last_w_ = canvas().width();
        last_h_ = canvas().height();
    }

    void picture(gfx::Sketch& f) const {
        f.text("hook", 0, 0,
               "hook " + std::to_string(after_hook_beats_) + " " + std::to_string(last_w_) + "x" +
                   std::to_string(last_h_),
               gfx::by_role(gfx::role::kFill));
    }

    std::int64_t rooms() const { return rooms_; }

private:
    std::int64_t after_hook_beats_ = -1;
    std::int64_t last_w_ = 0;
    std::int64_t last_h_ = 0;
    std::int64_t rooms_ = 0;
};

struct FacetedRig {
    Rig r;
    loom::WeaveId drawer{};
    FacetedDrawer* raw = nullptr;

    FacetedRig() {
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, window_office()).valid());
        auto d = std::make_unique<FacetedDrawer>();
        raw = d.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        loom::allow_describe_answers(g);
        drawer = r.bus.register_weave(std::move(d), std::move(g), kDrawer);
        raw->zen_set_self(drawer);
        r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kDrawer, kDrawer, "classic");
    }
};

class DescribeAsker : public loom::Weave {
public:
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::accepted_shapes_schema()};
    }
    void handle(const loom::Message& in, loom::Bus&) override {
        loom::Registry deps;
        loom::decode_accepted_referenced(in.payload, deps);
        for (const auto& s : loom::decode_accepted_roots(in.payload, deps)) {
            roots.push_back(s->name());
        }
    }
    loom::Value snapshot() const override { return loom::to_value(GraphicsTestDrawer{}); }
    loom::Value policy() const override { return loom::to_value(GraphicsTestDrawer{}); }
    void revive(const loom::Value&) override {}
    std::vector<std::string> roots;
};

struct ActivatedFacet {
    using Accept = loom::Accept<loom::Activated>;

    void on(const loom::Activated& a, loom::Mail&) {
        ++activations;
        last_sequence = a.sequence;
    }

    std::int64_t activations = 0;
    std::int64_t last_sequence = 0;
};

class TimedActivatedDrawer
    : public zengine::With<gfx::DrawingFacet, ActivatedFacet>::Weave<
          TimedActivatedDrawer, GraphicsTestDrawer, loom::Accept<GraphicsTestBeat>,
          loom::Emit<>, zengine::timer::TimedWeave> {
public:
    using Weave::on;

    TimedActivatedDrawer() : Weave(kDrawer) {}

    void on(const GraphicsTestBeat&, loom::Mail&) { ++state_.beats; }

    void on_timed_activation(const loom::Activated& a, loom::Mail&) {
        ++timed_activations_;
        timed_sequence_ = a.sequence;
    }

    void picture(gfx::Sketch& f) const {
        const auto& af = facet<ActivatedFacet>();
        f.text("act", 0, 0,
               "timed " + std::to_string(timed_activations_) + ":" +
                   std::to_string(timed_sequence_) + " facet " +
                   std::to_string(af.activations) + ":" + std::to_string(af.last_sequence),
               gfx::by_role(gfx::role::kFill));
    }

    std::int64_t timed_activations() const { return timed_activations_; }
    std::int64_t facet_activations() const { return facet<ActivatedFacet>().activations; }

private:
    std::int64_t timed_activations_ = 0;
    std::int64_t timed_sequence_ = 0;
};

} // namespace

TEST_CASE("a weave with DrawingFacet and ManualFacet runs its own after_delivery before the "
          "drawing facet flushes in the same delivery") {
    FacetedRig f;
    REQUIRE_FALSE(f.r.shown().rows.empty());
    CHECK(f.r.shown().rows[0] == "hook 0 600x264");

    const std::int64_t showings = f.r.shown().showings;
    f.r.to_role(kDrawer, GraphicsTestBeat{});
    f.r.drain();
    const gfx::WindowShown after = f.r.shown();
    CHECK(after.showings == showings + 1);
    CHECK(after.rows[0] == "hook 1 600x264");

    ShapeDocumentReader reader(f.r.bus, kDrawer);
    const auto doc = reader.ask("GraphicsTestBeat", 1);
    CHECK(doc.refusal.empty());
    CHECK(doc.text == "One beat for the faceted drawer.");
}

TEST_CASE("an AreaRoom with the same window and epoch and a changed w or h updates the canvas's "
          "room and redraws") {
    gfx::Canvas canvas("test.owner", "test.area");
    CHECK(canvas.room(gfx::AreaRoom{"test.area", "terminal", 7, 240, 120, {3}}));
    CHECK(canvas.width() == 240);
    CHECK(canvas.height() == 120);
    CHECK_FALSE(canvas.room(gfx::AreaRoom{"test.area", "terminal", 7, 240, 120, {3}}));
    CHECK(canvas.room(gfx::AreaRoom{"test.area", "terminal", 7, 360, 120, {3}}));
    CHECK(canvas.width() == 360);
    CHECK(canvas.height() == 120);
    CHECK(canvas.room(gfx::AreaRoom{"test.area", "terminal", 7, 360, 180, {3}}));
    CHECK(canvas.width() == 360);
    CHECK(canvas.height() == 180);

    FacetedRig f;
    REQUIRE(f.raw->rooms() == 1);
    const std::size_t mark = f.r.log.size();
    f.r.to_role(gfx::kGraphicsRole,
                gfx::AreaPlaced{kDrawer, gfx::kTerminalWindow, kDrawer, 0, 0,
                                40 * gfx::kCellPx, 18 * gfx::kCellPx, 0, true});
    f.r.drain();
    const auto rooms = f.r.read<gfx::AreaRoom>(mark, window_office());
    REQUIRE(rooms.size() == 1);
    CHECK(rooms[0].w == 40 * gfx::kCellPx);
    CHECK(rooms[0].h == 18 * gfx::kCellPx);
    CHECK(f.raw->rooms() == 2);
    const auto drawn = f.r.read<gfx::Drawn>(mark, kDrawer);
    REQUIRE(drawn.size() == 1);
    CHECK(drawn[0].whole);
    CHECK(f.r.shown().rows[0] == "hook 0 480x216");
}

TEST_CASE("zen.DescribeAccepted on a weave with facets reports the exact union of the weave's and "
          "its facets' accepted shapes") {
    FacetedRig f;
    std::vector<std::string> accepted;
    for (const auto& s : f.r.bus.accepted_schemas(f.drawer)) {
        accepted.push_back(s->name());
    }
    std::sort(accepted.begin(), accepted.end());
    const std::vector<std::string> expected{
        "AreaRoom",
        "DrawnRefused",
        "GraphicsTestBeat",
        "ShapeDocumentRequested",
        "zen.DescribeAccepted",
        "zen.PokeDescribe",
        "zen.PokeRead",
        "zen.PokeResetState",
        "zen.PokeWrite",
    };
    CHECK(accepted == expected);

    auto asker = std::make_unique<DescribeAsker>();
    DescribeAsker* raw_asker = asker.get();
    loom::Grant g;
    g.allow_to_role(loom::DescribeAccepted::zen_name, loom::DescribeAccepted::zen_version, kDrawer);
    const loom::WeaveId id = f.r.bus.register_weave(std::move(asker), std::move(g));
    (void)f.r.bus.send_as_to_role(
        id, kDrawer, loom::Message(loom::to_value(loom::DescribeAccepted{}), id, id, 1));
    f.r.drain();
    std::vector<std::string> wire_names = raw_asker->roots;
    std::sort(wire_names.begin(), wire_names.end());
    CHECK(wire_names == expected);
}

TEST_CASE("a facet on Activated composed over TimedWeave deduplicates zen.Activated and runs both "
          "on_timed_activation and the facet's handler on an accepted activation") {
    Rig r;
    REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
    REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, window_office()).valid());
    auto d = std::make_unique<TimedActivatedDrawer>();
    TimedActivatedDrawer* raw = d.get();
    loom::Grant g = loom::emit_default_grant(*raw);
    const loom::WeaveId id = r.bus.register_weave(std::move(d), std::move(g), kDrawer);
    raw->zen_set_self(id);
    r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kDrawer, kDrawer, "classic");

    std::size_t activated_count = 0;
    for (const auto& s : r.bus.accepted_schemas(id)) {
        if (s->name() == loom::Activated::zen_name) {
            ++activated_count;
        }
    }
    CHECK(activated_count == 1);

    r.bus.send(id, loom::Message(loom::to_value(loom::Activated{1})));
    r.drain();
    CHECK(raw->timed_activations() == 0);
    CHECK(raw->facet_activations() == 0);

    const loom::WeaveId door = zengine::testing::mount_door(r.bus);
    zengine::testing::order_activation(r.bus, door, id, 1);
    r.drain();
    CHECK(raw->timed_activations() == 1);
    CHECK(raw->facet_activations() == 1);
    REQUIRE_FALSE(r.shown().rows.empty());
    CHECK(r.shown().rows[0] == "timed 1:1 facet 1:1");
}



namespace {

struct GraphicsTestClaimed {
    std::int64_t n = 0;
    ZEN_SHAPE(GraphicsTestClaimed, 1, ZEN_FIELD(n));
};

struct GraphicsTestFacetClaimed {
    std::int64_t n = 0;
    ZEN_SHAPE(GraphicsTestFacetClaimed, 1, ZEN_FIELD(n));
};

struct GraphicsTestUnclaimed {
    std::int64_t n = 0;
    ZEN_SHAPE(GraphicsTestUnclaimed, 1, ZEN_FIELD(n));
};

/// A facet that claims a shape of its own.
struct ClaimingFacet {
    using Claims = loom::Claims<GraphicsTestFacetClaimed, GraphicsTestClaimed>;
};

/// A weave that claims, composed with a facet that claims too: its claim-set is the union, each
/// shape once. It declines its own shape when the bus publishes it, and has no handler for the
/// facet's.
class Claimer
    : public zengine::With<ClaimingFacet>::Weave<Claimer, GraphicsTestDrawer,
                                                 loom::Accept<GraphicsTestBeat>, loom::Emit<>,
                                                 loom::WeaveBase,
                                                 loom::Claims<GraphicsTestClaimed>> {
public:
    void on(const GraphicsTestBeat&, loom::Mail& mail) {
        own = mail.claim(GraphicsTestClaimed{1});
        facets = mail.claim(GraphicsTestFacetClaimed{2});
        undeclared = mail.claim(GraphicsTestUnclaimed{3});
    }
    bool on_claim_published(const GraphicsTestClaimed& c) {
        published.push_back(c.n);
        return false;
    }
    std::vector<std::int64_t> published;
    loom::SenseClaimResult own, facets, undeclared;
};

} // namespace

TEST_CASE("a weave composed with facets declares its own claims and each facet's, once each, "
          "claims them and hears them published; an undeclared shape is refused") {
    Rig r;
    auto c = std::make_unique<Claimer>();
    Claimer* raw = c.get();
    loom::Grant g = loom::emit_default_grant(*raw);
    const loom::WeaveId id = r.bus.register_weave(std::move(c), std::move(g), "test.claimer");
    raw->zen_set_self(id);
    std::vector<std::string> claimed;
    for (const auto& s : raw->claimed_schemas()) claimed.push_back(s->name());
    CHECK(claimed == std::vector<std::string>{"GraphicsTestFacetClaimed", "GraphicsTestClaimed"});
    r.bus.send(id, loom::Message(loom::to_value(GraphicsTestBeat{})));
    r.drain();
    CHECK(raw->own.accepted);
    CHECK(raw->facets.accepted);
    CHECK_FALSE(raw->undeclared.accepted);
    CHECK(raw->undeclared.why == loom::SenseRefusal::Undeclared);
    // PUBLISHED BY THE BUS: its own handler answers for its shape, and a shape with none is
    // applied as it stands, as Loom answers a weave with no handler.
    CHECK(raw->claim_published(loom::to_value(GraphicsTestClaimed{7})) ==
          loom::Weave::PublishedClaim::Declined);
    CHECK(raw->published == std::vector<std::int64_t>{7});
    CHECK(raw->claim_published(loom::to_value(GraphicsTestFacetClaimed{8})) ==
          loom::Weave::PublishedClaim::Applied);
    CHECK(raw->published.size() == 1);
}

// ============================================================================
// The parts an owner draws (graphics/elements.hpp): as arithmetic, then pressed through the window
// ============================================================================

namespace {

using Paths = std::vector<std::string>;

/// The classic skin's look, a glyph for every role a part paints: the ground a blank.
gfx::term::Look parts_look() {
    gfx::term::Look l;
    l.fill = {{gfx::role::kFill, 'o'},
              {gfx::role::kAccent, 'O'},
              {gfx::role::kMuted, '.'},
              {gfx::role::kAlert, '*'},
              {gfx::role::kGround, ' '}};
    return l;
}

Paths node_paths(const gfx::Sketch& s) {
    Paths out;
    for (const gfx::Node& n : s.nodes()) {
        out.push_back(n.path);
    }
    return out;
}

Paths put_paths(const gfx::Drawn& d) {
    Paths out;
    for (const gfx::Node& n : d.put) {
        out.push_back(n.path);
    }
    return out;
}

Paths said_paths(const gfx::Drawn& d) {
    Paths out;
    for (const gfx::ElementRecord& r : d.said) {
        out.push_back(r.path);
    }
    return out;
}

Paths record_paths(const gfx::Sketch& s) {
    Paths out;
    for (const gfx::ElementRecord& r : s.records()) {
        out.push_back(r.path);
    }
    return out;
}

const gfx::ElementRecord& record_at(const gfx::Sketch& s, const std::string& path) {
    for (const gfx::ElementRecord& r : s.records()) {
        if (r.path == path) {
            return r;
        }
    }
    FAIL("no record at " << path);
    return s.records().front();
}

const gfx::Node& node_at(const gfx::Sketch& s, const std::string& path) {
    for (const gfx::Node& n : s.nodes()) {
        if (n.path == path) {
            return n;
        }
    }
    FAIL("no node at " << path);
    return s.nodes().front();
}

/// The face recorded at `path`, read back as the shape it was said as.
template <class Face>
Face face_at(const gfx::Sketch& s, const std::string& path) {
    const gfx::ElementRecord& r = record_at(s, path);
    CHECK(r.type == loom::schema_of<Face>()->name());
    const auto f = loom::claim_item<Face>(r.data);
    REQUIRE(f.has_value());
    return *f;
}

constexpr std::int64_t kCell = gfx::kCellPx;

} // namespace

TEST_CASE("a button draws its face and its label at its path, its face the record there with the "
          "action its owner named, and none while disabled or when named none") {
    gfx::Button b("save", "save");
    b.set_bounds(2 * kCell, kCell, 10 * kCell, kCell);
    gfx::Sketch s;
    b.sketch(s, "file.save");
    CHECK(node_paths(s) == Paths{"save", "save/face", "save/label"});
    CHECK(record_paths(s) == Paths{"save"});
    CHECK(record_at(s, "save").action == "file.save");
    const auto f = face_at<gfx::ButtonFace>(s, "save");
    CHECK(f.x == 2 * kCell);
    CHECK(f.y == kCell);
    CHECK(f.w == 10 * kCell);
    CHECK(f.h == kCell);
    CHECK(f.label == "save");
    CHECK(f.enabled);
    CHECK_FALSE(f.pressed);
    CHECK_FALSE(f.hovered);
    CHECK(f.fill.role == gfx::role::kGround);
    CHECK(f.ink.role == gfx::role::kFill);
    // ITS WORDS CENTRED ON A FACE AS WIDE AS ITS BOX, so a press anywhere on the box lands on it.
    const auto rows = painted(s, parts_look());
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].empty());
    CHECK(rows[1] == "    [save]  ");

    // NAMED NONE: a press on it sends nothing.
    gfx::Sketch quiet;
    b.sketch(quiet);
    CHECK(record_at(quiet, "save").action.empty());

    // DISABLED: muted, in round brackets, never pressed, and silent whatever its owner names.
    b.set_enabled(false);
    b.press();
    CHECK_FALSE(b.pressed());
    gfx::Sketch off;
    b.sketch(off, "file.save");
    CHECK(record_at(off, "save").action.empty());
    CHECK(face_at<gfx::ButtonFace>(off, "save").ink.role == gfx::role::kMuted);
    CHECK_FALSE(face_at<gfx::ButtonFace>(off, "save").enabled);
    CHECK(painted(off, parts_look())[1] == "    (save)  ");

    // PRESSED: its face in the accent. HOVERED: its words in the accent. Disabled while pressed:
    // released.
    b.set_enabled(true);
    b.press();
    CHECK(b.pressed());
    gfx::Sketch down;
    b.sketch(down, "file.save");
    CHECK(painted(down, parts_look())[1] == "  OO[save]OO");
    CHECK(face_at<gfx::ButtonFace>(down, "save").pressed);
    b.set_enabled(false);
    CHECK_FALSE(b.pressed());
    b.set_enabled(true);
    b.hover(true);
    CHECK(b.face().ink.role == gfx::role::kAccent);
    CHECK(b.face().fill.role == gfx::role::kGround);
    CHECK(b.face().hovered);
}

TEST_CASE("a part drawn again sends only the nodes and the record its change touched") {
    gfx::Button b("ok", "ok");
    b.set_bounds(0, 0, 6 * kCell, kCell);
    gfx::Sketch before;
    b.sketch(before, "ok");
    b.hover(true);
    gfx::Sketch hovered;
    b.sketch(hovered, "ok");
    gfx::Drawn d = gfx::difference(before, hovered);
    CHECK(put_paths(d) == Paths{"ok/label"}); // its ink alone
    CHECK(said_paths(d) == Paths{"ok"});
    CHECK(d.removed.empty());
    CHECK(d.unsaid.empty());

    b.hover(false);
    b.press();
    gfx::Sketch pressed;
    b.sketch(pressed, "ok");
    d = gfx::difference(hovered, pressed);
    CHECK(put_paths(d) == Paths{"ok/face", "ok/label"});

    // Unchanged: nothing. The action alone changed: the record, and no node.
    gfx::Sketch same;
    b.sketch(same, "ok");
    d = gfx::difference(pressed, same);
    CHECK(d.put.empty());
    CHECK(d.said.empty());
    gfx::Sketch other;
    b.sketch(other, "ok.again");
    d = gfx::difference(same, other);
    CHECK(d.put.empty());
    CHECK(said_paths(d) == Paths{"ok"});

    gfx::Checkbox c("agree", "I agree");
    c.set_bounds(0, kCell, 12 * kCell, kCell);
    gfx::Sketch unchecked;
    c.sketch(unchecked, "agree.toggle");
    c.toggle();
    gfx::Sketch checked;
    c.sketch(checked, "agree.toggle");
    d = gfx::difference(unchecked, checked);
    CHECK(put_paths(d) == Paths{"agree/mark"});
    CHECK(said_paths(d) == Paths{"agree"});
}

TEST_CASE("a checkbox says its state in its mark, keeps that state its owner's, and asks its "
          "action only while enabled") {
    gfx::Checkbox c("wrap", "wrap lines");
    c.set_bounds(0, 0, 14 * kCell, kCell);
    CHECK_FALSE(c.checked());
    CHECK(c.label() == "wrap lines");
    gfx::Sketch s;
    c.sketch(s, "view.wrap");
    CHECK(node_paths(s) == Paths{"wrap", "wrap/face", "wrap/mark", "wrap/label"});
    CHECK(painted(s, parts_look())[0] == "[ ] wrap lines");
    CHECK(record_at(s, "wrap").action == "view.wrap");
    CHECK_FALSE(face_at<gfx::CheckboxFace>(s, "wrap").checked);

    c.toggle();
    CHECK(c.checked());
    gfx::Sketch on;
    c.sketch(on, "view.wrap");
    CHECK(painted(on, parts_look())[0] == "[x] wrap lines");
    CHECK(face_at<gfx::CheckboxFace>(on, "wrap").checked);
    CHECK(gfx::Checkbox("pre", "", true).checked());

    // DISABLED: muted and silent; its state is still the owner's to change.
    c.set_enabled(false);
    c.set_checked(false);
    CHECK_FALSE(c.checked());
    c.toggle();
    CHECK(c.checked());
    gfx::Sketch off;
    c.sketch(off, "view.wrap");
    CHECK(record_at(off, "wrap").action.empty());
    const auto f = face_at<gfx::CheckboxFace>(off, "wrap");
    CHECK_FALSE(f.enabled);
    CHECK(f.box_fill.role == gfx::role::kGround);
    CHECK(f.check_ink.role == gfx::role::kMuted);
    CHECK(f.label_ink.role == gfx::role::kMuted);
    c.set_enabled(true);
    c.hover(true);
    CHECK(c.face().label_ink.role == gfx::role::kAccent);
    CHECK(c.face().hovered);
}

TEST_CASE("a list chooses within its rows, keeps the chosen row in its window, and holds its "
          "scroll between the first row and the last full window") {
    gfx::List<std::string> l("rows");
    CHECK(l.empty());
    l.select_next(); // no rows: nothing to choose
    CHECK(l.selected() == -1);
    l.set_rows({"a", "b", "c", "d", "e"});
    l.set_visible_rows(2);
    CHECK(l.size() == 5);
    CHECK(l.at(3) == "d");
    CHECK(l.selected() == -1);
    l.select_next(); // none chosen: the first
    CHECK(l.selected() == 0);
    CHECK(l.scroll_offset() == 0);
    l.select_next();
    l.select_next();
    CHECK(l.selected() == 2);
    CHECK(l.scroll_offset() == 1); // kept in the window
    l.select(4);
    CHECK(l.scroll_offset() == 3);
    l.select_next(); // held at the last
    CHECK(l.selected() == 4);
    l.select(0);
    CHECK(l.scroll_offset() == 0);
    l.select_prev(); // held at the first
    CHECK(l.selected() == 0);

    l.scroll_to(99);
    CHECK(l.scroll_offset() == 3);
    l.scroll_to(-5);
    CHECK(l.scroll_offset() == 0);

    l.select(9); // names no row: none
    CHECK(l.selected() == -1);
    l.select_prev(); // none chosen: the last
    CHECK(l.selected() == 4);
    CHECK(l.scroll_offset() == 3);

    l.hover(3);
    CHECK(l.hovered() == 3);
    l.hover(7);
    CHECK(l.hovered() == -1);
    l.hover(4);
    // FEWER ROWS: the choice held at the last row, the pointed row dropped, the scroll held.
    l.set_rows({"a", "b"});
    CHECK(l.selected() == 1);
    CHECK(l.hovered() == -1);
    CHECK(l.scroll_offset() == 0);
    l.set_rows({});
    CHECK(l.selected() == -1);

    // EVERY ROW SHOWN: no scroll at all.
    l.set_rows({"a", "b", "c"});
    l.set_visible_rows(0);
    l.scroll_to(2);
    CHECK(l.scroll_offset() == 0);
}

TEST_CASE("a list draws its shown rows each at its index with its own record and action, and a "
          "scroll removes the row scrolled out and puts the row scrolled in") {
    gfx::List<std::string> l("rows");
    l.set_origin(0, kCell);
    l.set_row_size(8 * kCell, kCell);
    l.set_rows({"alpha", "beta", "gamma"});
    l.set_visible_rows(2);
    l.select(0);
    const auto words = [](const std::string& r) { return r; };
    const auto open = [](std::size_t i, const std::string& r) {
        return "open." + std::to_string(i) + "." + r;
    };
    gfx::Sketch a;
    l.sketch(a, words, open);
    CHECK(record_paths(a) == Paths{"rows/0", "rows/1"}); // the shown rows alone
    CHECK(record_at(a, "rows/0").action == "open.0.alpha");
    CHECK(record_at(a, "rows/1").action == "open.1.beta");
    const auto chosen = face_at<gfx::ListRowFace>(a, "rows/0");
    CHECK(chosen.text == "alpha");
    CHECK(chosen.selected);
    CHECK(chosen.ink.role == gfx::role::kAccent);
    CHECK(chosen.y == 0);
    CHECK(chosen.w == 8 * kCell);
    const auto next = face_at<gfx::ListRowFace>(a, "rows/1");
    CHECK_FALSE(next.selected);
    CHECK(next.ink.role == gfx::role::kFill);
    CHECK(next.y == kCell);
    // The chosen row says so in its words too, for a medium without colour.
    const auto rows = painted(a, parts_look());
    REQUIRE(rows.size() == 3);
    CHECK(rows[1] == "> alpha ");
    CHECK(rows[2] == "  beta  ");

    // The last row chosen: the window scrolls one row down.
    l.select(2);
    gfx::Sketch b;
    l.sketch(b, words, open);
    const gfx::Drawn d = gfx::difference(a, b);
    CHECK(d.removed == Paths{"rows/0"}); // its subtree with it
    CHECK(put_paths(d) == Paths{"rows/1", "rows/2", "rows/2/face", "rows/2/text"});
    CHECK(said_paths(d) == Paths{"rows/1", "rows/2"});
    CHECK(d.unsaid.empty());

    // No action named: every row silent.
    gfx::Sketch quiet;
    l.sketch(quiet, words);
    CHECK(record_at(quiet, "rows/1").action.empty());
    CHECK(record_at(quiet, "rows/2").action.empty());
}

TEST_CASE("a grid draws only the cells that hold something, each at its own path with its record; "
          "a cell emptied is removed") {
    gfx::Grid<int> g("board", 4, 3);
    CHECK(g.width() == 4);
    CHECK(g.height() == 3);
    CHECK(g.in_bounds(3, 2));
    CHECK_FALSE(g.in_bounds(4, 0));
    CHECK_FALSE(g.in_bounds(0, -1));
    CHECK(g.set(1, 2, 7));
    CHECK_FALSE(g.set(4, 0, 1)); // off the board: nothing changes
    CHECK_FALSE(g.set(-1, 0, 1));
    CHECK(g.at(1, 2) == 7);
    CHECK(g.at(9, 9) == 0);
    CHECK(g.cell_path(1, 2) == "board/c.1.2");

    const auto face = [](std::int64_t, std::int64_t, int v) {
        gfx::GridCellFace f;
        f.fill = gfx::by_role(v == 7 ? gfx::role::kAccent : gfx::role::kMuted);
        f.glyph = v == 7 ? "#" : "";
        return f;
    };
    gfx::Sketch a;
    g.sketch(a, face);
    CHECK(node_paths(a) ==
          Paths{"board", "board/c.1.2", "board/c.1.2/fill", "board/c.1.2/glyph"});
    const auto f = face_at<gfx::GridCellFace>(a, "board/c.1.2");
    CHECK(f.x == kCell);
    CHECK(f.y == 2 * kCell);
    CHECK(f.w == kCell);
    CHECK(f.h == kCell);
    CHECK(f.glyph == "#");
    CHECK(record_at(a, "board/c.1.2").action.empty()); // no action named: silent
    const auto rows = painted(a, parts_look());
    REQUIRE(rows.size() == 3);
    CHECK(rows[2] == " #");

    // A cell set: only it is put.
    g.set(0, 0, 1);
    gfx::Sketch b;
    g.sketch(b, face);
    gfx::Drawn d = gfx::difference(a, b);
    CHECK(put_paths(d) == Paths{"board/c.0.0", "board/c.0.0/fill"});
    CHECK(said_paths(d) == Paths{"board/c.0.0"});
    CHECK(d.removed.empty());
    // A cell emptied: removed, its subtree and record with it.
    CHECK(g.clear(1, 2));
    gfx::Sketch c;
    g.sketch(c, face);
    d = gfx::difference(b, c);
    CHECK(d.removed == Paths{"board/c.1.2"});
    CHECK(d.put.empty());
    CHECK(d.said.empty());

    g.fill(3);
    gfx::Sketch full;
    g.sketch(full, face);
    CHECK(full.records().size() == 12);
    g.clear_all();
    gfx::Sketch none;
    g.sketch(none, face);
    CHECK(node_paths(none) == Paths{"board"});

    // AN ACTION A CELL, named by the owner from the cell.
    g.set(2, 1, 5);
    gfx::Sketch acting;
    g.sketch(acting, face, [](std::int64_t col, std::int64_t row, int v) {
        return "dig." + std::to_string(col) + "." + std::to_string(row) + "." +
               std::to_string(v);
    });
    CHECK(record_at(acting, "board/c.2.1").action == "dig.2.1.5");

    // A board of yes and no.
    gfx::Grid<bool> life("life", 2, 2, 2 * kCell, 2 * kCell);
    life.fill(true);
    life.clear(0, 1);
    gfx::Sketch lit;
    life.sketch(lit, [](std::int64_t, std::int64_t, bool) { return gfx::GridCellFace{}; });
    CHECK(record_paths(lit) == Paths{"life/c.0.0", "life/c.1.0", "life/c.1.1"});
    CHECK(face_at<gfx::GridCellFace>(lit, "life/c.1.1").x == 2 * kCell);
}

TEST_CASE("an icon draws its named outline scaled into its square, a name its table lacks as a "
          "muted frame, and its record names it either way") {
    CHECK(gfx::Icon::path_data("6 2 11 10 1 10", 24) == "M 12 4 L 22 20 L 2 20 Z");
    CHECK(gfx::Icon::path_data("", 24).empty());
    CHECK_FALSE(gfx::Icon::outline("check").empty());
    CHECK(gfx::Icon::outline("sparkle").empty());

    gfx::Icon i("ok", "check");
    i.set_position(kCell, 0);
    i.set_size(2 * kCell);
    i.set_role(gfx::role::kAlert);
    CHECK(i.name() == "check");
    CHECK(i.size() == 2 * kCell);
    CHECK(i.role() == gfx::role::kAlert);
    gfx::Sketch s;
    i.sketch(s, "confirm");
    CHECK(node_paths(s) == Paths{"ok", "ok/shape"});
    CHECK(node_at(s, "ok/shape").op == gfx::op::kPath);
    const auto shape = gfx::from_body<gfx::PathArgs>(node_at(s, "ok/shape").args);
    REQUIRE(shape.has_value());
    CHECK(shape->d == gfx::Icon::path_data(gfx::Icon::outline("check"), 2 * kCell));
    CHECK(shape->fill.role == gfx::role::kAlert);
    const auto f = face_at<gfx::IconFace>(s, "ok");
    CHECK(f.name == "check");
    CHECK(f.x == kCell);
    CHECK(f.size == 2 * kCell);
    CHECK(f.ink.role == gfx::role::kAlert);
    CHECK(record_at(s, "ok").action == "confirm");

    // Every outline the table holds fills its one cell in a terminal: an icon a cell large shows.
    for (const gfx::IconOutline& o : gfx::kIconOutlines) {
        CAPTURE(o.name);
        gfx::Icon each("e", std::string(o.name), kCell);
        gfx::Sketch e;
        each.sketch(e);
        CHECK(painted(e, parts_look()) == Paths{"o"});
    }

    i.set_name("sparkle");
    gfx::Sketch unknown;
    i.sketch(unknown);
    CHECK(node_at(unknown, "ok/shape").op == gfx::op::kRect);
    CHECK(face_at<gfx::IconFace>(unknown, "ok").name == "sparkle");
    CHECK(record_at(unknown, "ok").action.empty());
}

TEST_CASE("an image shows the rectangle of its sheet its uv corners choose, tinted by an exact "
          "colour, and with no pixels its frame and the words of its source") {
    const loom::Bytes sheet{255, 0,   0,   255, 0,   255, 0,   255,  // red, green
                            0,   0,   255, 255, 255, 255, 255, 255}; // blue, white
    gfx::Image im("sprite", "sheet.png");
    CHECK_FALSE(im.set_pixels(2, 2, loom::Bytes(15, 0))); // not 2 x 2 x 4 bytes: none kept
    CHECK_FALSE(im.has_pixels());
    REQUIRE(im.set_pixels(2, 2, sheet));
    CHECK(im.slice().rgba == sheet); // the whole sheet by default

    im.set_uv(0.5, 0.0, 1.0, 0.5);
    gfx::Image::Slice sl = im.slice();
    CHECK(sl.pw == 1);
    CHECK(sl.ph == 1);
    CHECK(sl.rgba == loom::Bytes{0, 255, 0, 255}); // green, alone
    im.set_uv(0.0, 0.5, 1.0, 1.0);
    CHECK(im.slice().rgba == loom::Bytes{0, 0, 255, 255, 255, 255, 255, 255});
    im.set_uv(-1.0, -1.0, 2.0, 2.0); // held between 0 and 1
    CHECK(im.slice().rgba == sheet);

    im.set_tint(gfx::exact(gfx::role::kFill, 0x80ff00));
    sl = im.slice();
    CHECK(sl.rgba[0] == 128); // red by 0x80
    CHECK(sl.rgba[5] == 255); // green by 0xff
    CHECK(sl.rgba[14] == 0);  // blue by 0x00
    CHECK(sl.rgba[3] == 255); // alpha untouched
    im.set_tint(gfx::by_role(gfx::role::kAccent)); // a role alone: no tint
    CHECK(im.slice().rgba == sheet);

    im.set_bounds(0, 0, 4 * kCell, 2 * kCell);
    gfx::Sketch s;
    im.sketch(s, "zoom");
    CHECK(node_paths(s) == Paths{"sprite", "sprite/pixels"});
    const auto px = gfx::from_body<gfx::ImageArgs>(node_at(s, "sprite/pixels").args);
    REQUIRE(px.has_value());
    CHECK(px->pw == 2);
    CHECK(px->w == 4 * kCell);
    const auto f = face_at<gfx::ImageFace>(s, "sprite");
    CHECK(f.source == "sheet.png");
    CHECK(f.u1 == 1.0);
    CHECK(f.tint.role == gfx::role::kAccent);
    CHECK(record_at(s, "sprite").action == "zoom");

    // AN EMPTY RECTANGLE, or no pixels: a frame and the words.
    im.set_uv(0.6, 0.0, 0.4, 1.0);
    CHECK(im.slice().rgba.empty());
    gfx::Sketch none;
    im.sketch(none);
    CHECK(node_paths(none) == Paths{"sprite", "sprite/frame", "sprite/words"});
    const auto rows = painted(none, parts_look());
    REQUIRE(rows.size() == 2);
    CHECK(rows[0] == "shee"); // the words over the frame's top edge, clipped to the box
    CHECK(rows[1] == "+--+");
}

TEST_CASE("a scroll view holds its offset in range, sizes and places its thumb by what shows, and "
          "says which items it shows") {
    gfx::ScrollView v("view");
    v.set_bounds(0, 0, 10 * kCell, 4 * kCell);
    v.set_content_size(12 * kCell);
    CHECK(v.viewport_size() == 4 * kCell);
    CHECK(v.content_size() == 12 * kCell);
    CHECK(v.max_scroll() == 8 * kCell);
    CHECK(v.content_width() == 9 * kCell);
    v.scroll_to(500 * kCell);
    CHECK(v.scroll_offset() == 8 * kCell);
    v.scroll_by(-1000 * kCell);
    CHECK(v.scroll_offset() == 0);

    CHECK(v.thumb_size() == 4 * kCell * 4 * kCell / (12 * kCell)); // a third of the track
    CHECK(v.thumb_offset() == 0);
    v.scroll_to(8 * kCell);
    CHECK(v.thumb_offset() == 4 * kCell - v.thumb_size()); // at the track's end
    v.scroll_to(4 * kCell);
    CHECK(v.thumb_offset() == (4 * kCell - v.thumb_size()) / 2);
    CHECK(v.visible_range(kCell) == std::pair<std::int64_t, std::int64_t>{4, 8});
    v.scroll_by(2);
    CHECK(v.visible_range(kCell) == std::pair<std::int64_t, std::int64_t>{4, 9}); // in part
    CHECK(v.visible_range(0) == std::pair<std::int64_t, std::int64_t>{0, 0});

    // THE CONTENT SHRINKS: the offset held, the thumb the whole track.
    v.set_content_size(2 * kCell);
    CHECK(v.scroll_offset() == 0);
    CHECK(v.max_scroll() == 0);
    CHECK(v.thumb_size() == 4 * kCell);
    CHECK(v.visible_range(kCell) == std::pair<std::int64_t, std::int64_t>{0, 2});
    // A great deal of content: the thumb never smaller than one cell.
    v.set_content_size(10000 * kCell);
    CHECK(v.thumb_size() == kCell);
    // A shorter box: the offset held in its new range.
    v.scroll_to(v.max_scroll());
    v.set_bounds(0, 0, 10 * kCell, 8 * kCell);
    CHECK(v.scroll_offset() == v.max_scroll());
}

TEST_CASE("a scroll view clips the content drawn under it, moved up by its offset, and records "
          "its bar alone, where its action is") {
    gfx::ScrollView v("view");
    v.set_bounds(0, 0, 8 * kCell, 4 * kCell);
    v.set_content_size(8 * kCell);
    v.scroll_to(2 * kCell);
    gfx::Sketch s;
    v.sketch(s, "view.page");
    CHECK(node_paths(s) == Paths{"view", "view/content", "view/bar", "view/bar/track",
                                 "view/bar/thumb"});
    CHECK(record_paths(s) == Paths{"view/bar"}); // a press on the content is the content's
    CHECK(record_at(s, "view/bar").action == "view.page");
    const auto f = face_at<gfx::ScrollBarFace>(s, "view/bar");
    CHECK(f.x == 7 * kCell);
    CHECK(f.w == kCell);
    CHECK(f.content_size == 8 * kCell);
    CHECK(f.viewport_size == 4 * kCell);
    CHECK(f.scroll_offset == 2 * kCell);
    CHECK(f.thumb_size == 2 * kCell);
    CHECK(f.thumb_offset == kCell);
    CHECK(f.track_fill.role == gfx::role::kMuted);
    CHECK(f.thumb_fill.role == gfx::role::kFill);
    for (int i = 0; i < 8; ++i) {
        s.text(v.content_path() + "/l" + std::to_string(i), 0, i * kCell,
               "line " + std::to_string(i), gfx::by_role(gfx::role::kFill));
    }
    const auto rows = painted(s, parts_look());
    CHECK(rows == Paths{"line 2 .", "line 3 o", "line 4 o", "line 5 ."});
}

TEST_CASE("a text box edits whole characters at its cursor, and a disabled one takes no key and "
          "no focus") {
    gfx::TextBox t("name", "your name");
    CHECK(t.placeholder() == "your name");
    t.insert("abc");
    CHECK(t.text() == "abc");
    CHECK(t.cursor() == 3);
    t.move_left();
    t.insert("X");
    CHECK(t.text() == "abXc");
    CHECK(t.cursor() == 3);
    t.move_home();
    t.delete_forward();
    CHECK(t.text() == "bXc");
    CHECK(t.cursor() == 0);
    t.backspace(); // at the start: nothing before it
    CHECK(t.text() == "bXc");
    t.move_end();
    t.backspace();
    CHECK(t.text() == "bX");
    CHECK(t.cursor() == 2);
    t.move_right(); // at the end: it stays
    CHECK(t.cursor() == 2);
    t.delete_forward(); // nothing after it
    CHECK(t.text() == "bX");

    // UTF-8: the cursor steps over a character's every byte, and an edit takes them all.
    t.set_text("h\xC3\xA9!");
    CHECK(t.cursor() == 4);
    t.move_left();
    CHECK(t.cursor() == 3);
    t.move_left();
    CHECK(t.cursor() == 1);
    t.move_right();
    CHECK(t.cursor() == 3);
    t.move_left();
    t.delete_forward();
    CHECK(t.text() == "h!");
    t.set_text("a\xC3\xA9");
    t.backspace();
    CHECK(t.text() == "a");
    t.clear();
    CHECK(t.text().empty());
    CHECK(t.cursor() == 0);

    // FOCUS, and a disabled box: no key, no focus, the focus it had dropped.
    t.set_focused(true);
    CHECK(t.focused());
    t.set_enabled(false);
    CHECK_FALSE(t.focused());
    t.set_focused(true);
    CHECK_FALSE(t.focused());
    t.insert("z");
    t.backspace();
    CHECK(t.text().empty());
    t.set_text("set");
    t.move_home();
    CHECK(t.cursor() == 3); // keys move nothing
    t.delete_forward();
    CHECK(t.text() == "set"); // ...and the owner's own text still lands
}

TEST_CASE("a text box shows its placeholder while empty and unfocused, the part of its text "
          "around its cursor, its cursor while focused, and asks its action only while enabled") {
    gfx::TextBox t("q", "search");
    t.set_bounds(0, 0, 8 * kCell, kCell);
    gfx::Sketch a;
    t.sketch(a, "focus.q");
    CHECK(node_paths(a) == Paths{"q", "q/box", "q/text"});
    CHECK(painted(a, parts_look())[0] == "+search+");
    CHECK(record_at(a, "q").action == "focus.q");
    const auto f = face_at<gfx::TextBoxFace>(a, "q");
    CHECK(f.placeholder == "search");
    CHECK(f.text.empty());
    CHECK(f.border.role == gfx::role::kMuted);

    t.set_focused(true);
    t.insert("abcdefghij");
    gfx::Sketch b;
    t.sketch(b, "focus.q");
    CHECK(node_paths(b) == Paths{"q", "q/box", "q/text", "q/cursor"});
    CHECK(painted(b, parts_look())[0] == "+fghij|+"); // the cursor at the end, in view
    const auto typed = face_at<gfx::TextBoxFace>(b, "q");
    CHECK(typed.text == "abcdefghij"); // the record holds it whole
    CHECK(typed.cursor == 10);
    CHECK(typed.focused);
    CHECK(typed.border.role == gfx::role::kAccent);
    t.move_home();
    gfx::Sketch c;
    t.sketch(c, "focus.q");
    CHECK(painted(c, parts_look())[0] == "+|bcdef+");

    // DISABLED: unfocused, muted, its cursor removed, and silent.
    t.set_enabled(false);
    gfx::Sketch d;
    t.sketch(d, "focus.q");
    CHECK(record_at(d, "q").action.empty());
    CHECK(face_at<gfx::TextBoxFace>(d, "q").ink.role == gfx::role::kMuted);
    CHECK(gfx::difference(c, d).removed == Paths{"q/cursor"});
}

namespace {

constexpr const char* kParts = "test.parts";

struct PartsHeard {
    std::vector<std::string> refused;
    std::vector<std::string> actions;
};

/// AN OWNER THAT DRAWS PARTS: it keeps the refusals and the actions the window asks of it.
class PartsOwner
    : public loom::WeaveBase<PartsOwner, GraphicsTestOwner,
                             loom::Accept<gfx::AreaRoom, gfx::DrawnRefused, gfx::ActionRequested>,
                             loom::Emit<gfx::Drawn>> {
public:
    explicit PartsOwner(PartsHeard& heard) : heard_(&heard) {}
    void on(const gfx::AreaRoom&, loom::Mail&) { ++state_.heard; }
    void on(const gfx::DrawnRefused& r, loom::Mail&) { heard_->refused.push_back(r.reason); }
    void on(const gfx::ActionRequested& a, loom::Mail& mail) {
        if (mail.authored_from_role(window_office())) heard_->actions.push_back(a.action);
    }

private:
    PartsHeard* heard_;
};

/// Parts drawn into a window whose terminal is the suite's, by an owner the suite speaks for, its
/// area placed at the window's corner and not to hear the pointer.
struct Parts : TypedWindow {
    PartsHeard heard;
    loom::WeaveId owner;
    Parts() {
        auto o = std::make_unique<PartsOwner>(heard);
        PartsOwner* raw = o.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        owner = r.bus.register_weave(std::move(o), std::move(g), kParts);
        raw->zen_set_self(owner);
        r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kParts, kParts, "classic");
    }
    /// The owner's picture, sent as its canvas sends one: whole first, then what changed.
    void draw(const gfx::Sketch& s) {
        gfx::Drawn d;
        if (drawn_) {
            d = gfx::difference(last_, s);
        } else {
            d.whole = true;
            d.put = s.nodes();
            d.said = s.records();
        }
        d.area = kParts;
        last_ = s;
        drawn_ = true;
        r.bus.office_send_to_role_as(owner, kParts, window_office(),
                                     loom::Message(loom::to_value(d)));
        r.drain();
    }
    /// What a press at a cell made the window send, and the actions the owner heard from it.
    std::vector<std::string> press_sent(int col, int row) {
        SentBy sent(r.bus, window);
        heard.actions.clear();
        press(col, row);
        return sent.sent;
    }

private:
    gfx::Sketch last_;
    bool drawn_ = false;
};

/// One of every part, cell by cell: the actions named, one button named none, one disabled.
struct Board {
    gfx::Button ok{"ok", "ok"};
    gfx::Button quiet{"quiet", "quiet"};
    gfx::Button off{"off", "off"};
    gfx::Checkbox agree{"agree", "agree"};
    gfx::List<std::string> colours{"colours"};
    gfx::Grid<int> grid{"grid", 3, 3};
    gfx::TextBox name{"name", "name"};
    gfx::Icon icon{"icon", "square", 2 * kCell};
    gfx::Image image{"image", "sheet"};
    gfx::ScrollView view{"view"};
    Board() {
        ok.set_bounds(0, 0, 8 * kCell, kCell);
        quiet.set_bounds(0, 2 * kCell, 8 * kCell, kCell);
        off.set_bounds(0, 4 * kCell, 8 * kCell, kCell);
        off.set_enabled(false);
        agree.set_bounds(0, 6 * kCell, 12 * kCell, kCell);
        colours.set_origin(0, 8 * kCell);
        colours.set_row_size(10 * kCell, kCell);
        colours.set_rows({"red", "green", "blue"});
        colours.set_visible_rows(2);
        grid.set_origin(20 * kCell, 0);
        grid.set(1, 1, 1);
        name.set_bounds(20 * kCell, 5 * kCell, 10 * kCell, kCell);
        icon.set_position(20 * kCell, 8 * kCell);
        image.set_bounds(24 * kCell, 8 * kCell, 6 * kCell, 2 * kCell);
        view.set_bounds(32 * kCell, 0, 8 * kCell, 4 * kCell);
        view.set_content_size(8 * kCell);
    }
    gfx::Sketch picture() const {
        gfx::Sketch s;
        ok.sketch(s, "test.ok");
        quiet.sketch(s);
        off.sketch(s, "test.off");
        agree.sketch(s, "test.agree");
        colours.sketch(
            s, [](const std::string& c) { return c; },
            [](std::size_t i, const std::string&) { return "test.row." + std::to_string(i); });
        grid.sketch(
            s,
            [](std::int64_t, std::int64_t, int) {
                return gfx::GridCellFace{0, 0, 0, 0, gfx::by_role(gfx::role::kAccent), ""};
            },
            [](std::int64_t col, std::int64_t row, int) {
                return "test.cell." + std::to_string(col) + "." + std::to_string(row);
            });
        name.sketch(s, "test.focus");
        icon.sketch(s, "test.icon");
        image.sketch(s, "test.image");
        view.sketch(s, "test.bar");
        for (int i = 0; i < 8; ++i) {
            s.text(view.content_path() + "/l" + std::to_string(i), 0, i * kCell,
                   "line " + std::to_string(i), gfx::by_role(gfx::role::kFill));
        }
        return s;
    }
};

} // namespace

TEST_CASE("a part pressed in the window asks its owner for the action the owner named; a part "
          "named none, a disabled part and a part's empty ground send nothing") {
    Parts p;
    Board b;
    p.draw(b.picture());
    REQUIRE(p.heard.refused.empty()); // every record stands on a node, within every bound
    const std::vector<std::string> rows = p.r.shown().rows;
    REQUIRE(rows.size() > 9);
    CHECK(rows[0].substr(0, 6) == "  [ok]");
    CHECK(rows[4].substr(0, 6) == " (off)");
    CHECK(rows[6].substr(0, 9) == "[ ] agree");

    const Paths asked{"ActionRequested"};
    CHECK(p.press_sent(3, 0) == asked); // the button
    CHECK(p.heard.actions == Paths{"test.ok"});
    CHECK(p.press_sent(7, 0) == asked); // its face's far edge
    CHECK(p.heard.actions == Paths{"test.ok"});
    CHECK(p.press_sent(9, 6) == asked); // the checkbox's face, past its label
    CHECK(p.heard.actions == Paths{"test.agree"});
    CHECK(p.press_sent(1, 9) == asked); // the list's second row
    CHECK(p.heard.actions == Paths{"test.row.1"});
    CHECK(p.press_sent(21, 1) == asked); // the grid's filled cell
    CHECK(p.heard.actions == Paths{"test.cell.1.1"});
    CHECK(p.press_sent(22, 5) == asked); // the text box
    CHECK(p.heard.actions == Paths{"test.focus"});
    CHECK(p.press_sent(21, 9) == asked); // the icon
    CHECK(p.heard.actions == Paths{"test.icon"});
    CHECK(p.press_sent(24, 8) == asked); // the image's frame
    CHECK(p.heard.actions == Paths{"test.image"});
    CHECK(p.press_sent(39, 1) == asked); // the scroll view's bar
    CHECK(p.heard.actions == Paths{"test.bar"});

    // SILENT: a button named none, a disabled button, the grid's empty cell, the scroll view's
    // content, which records nothing of its own.
    CHECK(p.press_sent(3, 2).empty());
    CHECK(p.press_sent(3, 4).empty());
    CHECK(p.press_sent(20, 0).empty());
    CHECK(p.press_sent(33, 0).empty());
    CHECK(p.heard.actions.empty());
    CHECK(p.heard.refused.empty());
}

TEST_CASE("a part changed and sent as what changed lands in the window: a scrolled list's rows "
          "show their new words and answer with their own actions") {
    Parts p;
    Board b;
    p.draw(b.picture());
    REQUIRE(p.heard.refused.empty());
    CHECK(p.press_sent(1, 9) == Paths{"ActionRequested"});
    CHECK(p.heard.actions == Paths{"test.row.1"});

    // The last row chosen: the list scrolls one row, its first row removed, its last put.
    b.colours.select(2);
    b.agree.toggle();
    p.draw(b.picture());
    REQUIRE(p.heard.refused.empty());
    const std::vector<std::string> rows = p.r.shown().rows;
    REQUIRE(rows.size() > 9);
    CHECK(rows[6].substr(0, 9) == "[x] agree");
    CHECK(rows[8].substr(0, 10) == "  green   ");
    CHECK(rows[9].substr(0, 10) == "> blue    ");
    CHECK(p.press_sent(1, 8) == Paths{"ActionRequested"});
    CHECK(p.heard.actions == Paths{"test.row.1"});
    CHECK(p.press_sent(1, 9) == Paths{"ActionRequested"});
    CHECK(p.heard.actions == Paths{"test.row.2"});

    // The button disabled: drawn again, and silent where it asked before.
    b.ok.set_enabled(false);
    p.draw(b.picture());
    CHECK(p.press_sent(3, 0).empty());
    CHECK(p.heard.refused.empty());
}

// ============================================================================
// A pane Workshop loads, drawn in a window: Attention and Connections, the libraries themselves
// ============================================================================

namespace {

namespace ws = zengine::workshop;
namespace apane = zengine::attention_pane;
namespace cpane = zengine::connections_pane;

/// THE OFFICES A PANE HEARS ITS READING FROM, as far as the window's suite needs them: each holds
/// its office and says what the suite gives it, and hears nothing.
class WorkshopSpeaker
    : public loom::WeaveBase<WorkshopSpeaker, GraphicsTestOwner, loom::Accept<>,
                             loom::Emit<ws::StandingConditions>> {};
class DoorSpeaker
    : public loom::WeaveBase<DoorSpeaker, GraphicsTestOwner, loom::Accept<>,
                             loom::Emit<ws::v3::GuestConnections>> {};

template <class W>
loom::WeaveId mount_as(Rig& r, const std::string& office) {
    auto w = std::make_unique<W>();
    W* raw = w.get();
    loom::Grant g = loom::emit_default_grant(*raw);
    const loom::WeaveId id = r.bus.register_weave(std::move(w), std::move(g), office);
    raw->zen_set_self(id);
    return id;
}

/// An area of the terminal window for a pane's office, at its corner, 62 cells by 10, painted by
/// the classic skin.
void place_pane(Rig& r, const std::string& office) {
    r.to_role(gfx::kGraphicsRole, gfx::SkinOffered{"classic", PAINT_CLASSIC_SO});
    r.to_role(gfx::kGraphicsRole, gfx::AreaPlaced{office, gfx::kTerminalWindow, office, 0, 0,
                                                  62 * gfx::kCellPx, 10 * gfx::kCellPx, 0});
    r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{office, "classic"});
    r.drain();
}

/// Whether the window's row `row` begins with `words`.
bool row_says(const std::vector<std::string>& rows, std::size_t row, const std::string& words) {
    return row < rows.size() && rows[row].rfind(words, 0) == 0;
}

/// Whether any of the window's rows holds `words`.
bool shows(const std::vector<std::string>& rows, const std::string& words) {
    for (const std::string& row : rows) {
        if (row.find(words) != std::string::npos) {
            return true;
        }
    }
    return false;
}

ws::StandingCondition condition(const char* key, const char* compact, const char* detail) {
    return ws::StandingCondition{key, compact, detail, zengine::surface::role::kAlert, ""};
}

/// THE ATTENTION PANE, the library Workshop loads, in a window whose terminal is the suite's: its
/// area placed for its office, and the host's office the suite speaks as.
struct AttentionWindow : TypedWindow {
    loom::WeaveId workshop;
    AttentionWindow() {
        workshop = mount_as<WorkshopSpeaker>(r, "zengine.workshop");
        REQUIRE(r.load(apane::kAttentionPaneStem, ATTENTION_PANE_SO, apane::kAttentionPaneRole)
                    .valid());
        place_pane(r, apane::kAttentionPaneRole);
    }
    /// The host says what is true, as its own office.
    void say(std::vector<ws::StandingCondition> rows) {
        r.bus.office_send_to_role_as(
            workshop, "zengine.workshop", apane::kAttentionPaneRole,
            loom::Message(loom::to_value(ws::StandingConditions{std::move(rows)})));
        r.drain();
    }
    /// The window asks the pane's area for an action, as the window's office.
    void ask(const std::string& action) {
        r.bus.office_send_to_role_as(
            window, window_office(), apane::kAttentionPaneRole,
            loom::Message(loom::to_value(gfx::ActionRequested{apane::kAttentionPaneRole, action})));
        r.drain();
    }
    /// What the pane drew since `mark`, as its office.
    std::vector<gfx::Drawn> drawn(std::size_t mark) const {
        return r.read<gfx::Drawn>(mark, apane::kAttentionPaneRole);
    }
};

constexpr const char* kTestPane = "test.pane";

struct GraphicsTestPaneTurn {
    bool broken = false;
    ZEN_SHAPE(GraphicsTestPaneTurn, 1, ZEN_FIELD(broken));
};

/// A PANE ON THE DRAWING FACET, as a stranger writes one, shown in a window alone: each turn draws
/// again, and a broken turn draws a node in a parent it never drew, which the window refuses. Its
/// manual is any page; nothing asks for it.
using TestDrawing = ws::PaneDrawingFacet<kTestPane, kTestPane,
                                         zengine::manual::pages::zengine_window_terminal::kManual>;

class TestPane
    : public zengine::With<TestDrawing>::Weave<TestPane, GraphicsTestOwner,
                                               loom::Accept<GraphicsTestPaneTurn>> {
public:
    ws::v2::PaneOffered offering() const {
        return ws::v2::PaneOffered{kTestPane, "test", "a pane the suite draws", 1, 10};
    }
    void on_room(loom::Mail& mail) { this->template facet<TestDrawing>().show(*this, mail); }
    void on(const GraphicsTestPaneTurn& turn, loom::Mail& mail) {
        broken_ = turn.broken;
        ++state_.heard;
        on_room(mail);
    }
    void picture(gfx::Sketch& f) const {
        f.text("words", 0, 0, "turn " + std::to_string(state_.heard),
               gfx::by_role(gfx::role::kFill));
        if (broken_) {
            f.text("nowhere/words", 0, gfx::kCellPx, "orphan", gfx::by_role(gfx::role::kFill));
        }
    }

private:
    bool broken_ = false;
};

} // namespace

TEST_CASE("a pane's drawing its window refused leaves the window as it was, and the pane's next "
          "drawing goes whole, never as a change from a picture the window does not hold") {
    Rig r;
    REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
    REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, window_office()).valid());
    (void)mount_as<TestPane>(r, kTestPane);
    place_pane(r, kTestPane);
    REQUIRE(r.read<gfx::Drawn>(0, kTestPane).size() == 1);
    CHECK(row_says(r.shown().rows, 0, "turn 0"));
    const auto turn = [&](bool broken) {
        r.to_role(kTestPane, GraphicsTestPaneTurn{broken});
        r.drain();
    };

    // A BROKEN CHANGE, REFUSED: the picture goes again at once, whole -- broken still, so the
    // window refuses that too, and nothing more is sent.
    std::size_t mark = r.log.size();
    turn(true);
    std::vector<gfx::Drawn> drawn = r.read<gfx::Drawn>(mark, kTestPane);
    REQUIRE(drawn.size() == 2);
    CHECK_FALSE(drawn[0].whole);
    CHECK(drawn[1].whole);
    REQUIRE(r.read<gfx::DrawnRefused>(mark, window_office()).size() == 2);
    CHECK(row_says(r.shown().rows, 0, "turn 0")); // what the window showed stays

    mark = r.log.size();
    turn(false);
    drawn = r.read<gfx::Drawn>(mark, kTestPane);
    REQUIRE(drawn.size() == 1);
    CHECK(drawn[0].whole);
    CHECK(put_paths(drawn[0]) == Paths{"words"});
    CHECK(r.read<gfx::DrawnRefused>(mark, window_office()).empty());
    CHECK(row_says(r.shown().rows, 0, "turn 2"));
}

TEST_CASE("the Attention pane draws in a graphics window: whole on its room, then only what "
          "changed; a press on a condition's row moves the cursor there, a dismissal the window "
          "asks removes the row, and a reload asks its room and draws whole with the hiding kept") {
    AttentionWindow a;
    // ITS ROOM: the picture whole, the host not heard yet.
    std::vector<gfx::Drawn> drawn = a.drawn(0);
    REQUIRE(drawn.size() == 1);
    CHECK(drawn[0].whole);
    CHECK(put_paths(drawn[0]) == Paths{"glance"});
    CHECK(drawn[0].said.empty());
    CHECK(row_says(a.r.shown().rows, 0, "ATTENTION (waiting)"));

    // THE HOST SPEAKS: what changed, each condition's row named by its key, a press there asking
    // for it.
    std::size_t mark = a.r.log.size();
    const std::vector<ws::StandingCondition> two{
        condition("a.one", "the first thing", "why the first"),
        condition("b.two", "the second thing", "why the second")};
    a.say(two);
    drawn = a.drawn(mark);
    REQUIRE(drawn.size() == 1);
    CHECK_FALSE(drawn[0].whole);
    CHECK(put_paths(drawn[0]) ==
          Paths{"glance", "condition:a.one", "line", "line/2", "condition:b.two"});
    CHECK(drawn[0].removed.empty());
    CHECK(said_paths(drawn[0]) == Paths{"condition:a.one", "condition:b.two"});
    for (const gfx::ElementRecord& record : drawn[0].said) {
        CHECK(record.action == record.path);
    }
    std::vector<std::string> rows = a.r.shown().rows;
    CHECK(row_says(rows, 0, "the first thing (+1 more)"));
    CHECK(row_says(rows, 1, "> the first thing"));
    CHECK(row_says(rows, 2, "    why the first"));
    CHECK(row_says(rows, 3, "  the second thing"));

    // A PRESS ON THE SECOND CONDITION'S ROW: the window asks for it by name, the cursor moves
    // there, and only the rows that moved are drawn again -- the glance is not.
    mark = a.r.log.size();
    a.press(4, 3);
    const auto asked = a.r.read<gfx::ActionRequested>(mark, window_office());
    REQUIRE(asked.size() == 1);
    CHECK(asked[0].area == apane::kAttentionPaneRole);
    CHECK(asked[0].action == "condition:b.two");
    drawn = a.drawn(mark);
    REQUIRE(drawn.size() == 1);
    CHECK_FALSE(drawn[0].whole);
    CHECK(put_paths(drawn[0]) == Paths{"condition:a.one", "condition:b.two", "line/3"});
    CHECK(drawn[0].removed == Paths{"line/2"});
    CHECK(drawn[0].said.empty());
    rows = a.r.shown().rows;
    CHECK(row_says(rows, 1, "  the first thing"));
    CHECK(row_says(rows, 2, "> the second thing"));
    CHECK(row_says(rows, 3, "    why the second"));

    // A DECLARED ID THE WINDOW ASKS: the condition under the cursor is hidden, its row and its
    // name go, and the notice about it leads.
    mark = a.r.log.size();
    a.ask(apane::kActionDismiss);
    drawn = a.drawn(mark);
    REQUIRE(drawn.size() == 1);
    CHECK_FALSE(drawn[0].whole);
    CHECK(drawn[0].removed == Paths{"condition:b.two"});
    const Paths put = put_paths(drawn[0]);
    CHECK(std::find(put.begin(), put.end(), "notice") != put.end());
    rows = a.r.shown().rows;
    CHECK(row_says(rows, 0, "hidden -- the second thing is still true"));
    CHECK(row_says(rows, 1, "the first thing (+1 more)"));
    CHECK(row_says(rows, 2, "> the first thing"));
    CHECK_FALSE(shows(rows, "  the second thing"));

    // AN ID THE PANE NEITHER DECLARED NOR DREW, ASKED BY THE WINDOW, AND A DECLARED ONE ASKED BY
    // ANYONE BUT THE WINDOW: no act, so the notice stands and nothing is drawn.
    mark = a.r.log.size();
    a.ask("attention.no-such-action");
    a.r.to_role(apane::kAttentionPaneRole,
                gfx::ActionRequested{apane::kAttentionPaneRole, apane::kActionUp});
    a.r.drain();
    CHECK(a.drawn(mark).empty());
    CHECK(row_says(a.r.shown().rows, 0, "hidden -- the second thing is still true"));

    // RELOADED: the new image holds no room, so it asks, and its picture goes whole once; the
    // hiding is the pane's own state, so the same reading said again still hides the second.
    mark = a.r.log.size();
    const auto reloaded =
        a.r.command(loom::ReloadWeave{apane::kAttentionPaneStem, ATTENTION_PANE_SO});
    REQUIRE_MESSAGE(reloaded.kind == 1, reloaded.text);
    CHECK(a.r.count(mark, "RoomAsked", apane::kAttentionPaneRole) == 1);
    drawn = a.drawn(mark);
    REQUIRE(drawn.size() == 1);
    CHECK(drawn[0].whole);
    CHECK(row_says(a.r.shown().rows, 0, "ATTENTION (waiting)"));
    a.say(two);
    rows = a.r.shown().rows;
    CHECK(row_says(rows, 0, "the first thing (+1 more)"));
    CHECK(row_says(rows, 1, "> the first thing"));
    CHECK_FALSE(shows(rows, "the second thing"));

    // A KEY WITH A `/` IN IT IS ONE NAME: its row is drawn and named, and a press there asks for
    // the key as the host said it.
    std::vector<ws::StandingCondition> three = two;
    three.push_back(condition("load.unavailable/x.y", "a third thing", "why the third"));
    mark = a.r.log.size();
    a.say(three);
    CHECK(a.r.read<gfx::DrawnRefused>(mark, window_office()).empty());
    drawn = a.drawn(mark);
    REQUIRE(drawn.size() == 1);
    CHECK(said_paths(drawn[0]) == Paths{"condition:load.unavailable%2Fx.y"});
    REQUIRE(row_says(a.r.shown().rows, 3, "  a third thing"));
    mark = a.r.log.size();
    a.press(4, 3);
    const auto third = a.r.read<gfx::ActionRequested>(mark, window_office());
    REQUIRE(third.size() == 1);
    CHECK(third[0].action == "condition:load.unavailable/x.y");
    CHECK(row_says(a.r.shown().rows, 2, "> a third thing"));
}

TEST_CASE("the Connections pane draws in a graphics window: whole on its room, each new reading "
          "as what changed, and nothing named for a press") {
    Rig r;
    REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
    REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, window_office()).valid());
    const loom::WeaveId door = mount_as<DoorSpeaker>(r, ws::kGuestsRole);
    REQUIRE(r.load(cpane::kConnectionsPaneStem, CONNECTIONS_PANE_SO, cpane::kConnectionsPaneRole)
                .valid());
    place_pane(r, cpane::kConnectionsPaneRole);
    const auto say = [&](const ws::v3::GuestConnections& said) {
        r.bus.office_send_to_role_as(door, ws::kGuestsRole, cpane::kConnectionsPaneRole,
                                     loom::Message(loom::to_value(said)));
        r.drain();
    };

    std::vector<gfx::Drawn> drawn = r.read<gfx::Drawn>(0, cpane::kConnectionsPaneRole);
    REQUIRE(drawn.size() == 1);
    CHECK(drawn[0].whole);
    CHECK(put_paths(drawn[0]) == Paths{"waiting"});
    CHECK(row_says(r.shown().rows, 0, "CONNECTIONS (waiting for the guest door)"));

    ws::v3::GuestConnections said;
    said.listen = "127.0.0.1:4242";
    said.rows.push_back(ws::v3::GuestConnection{1, "admitted", "tool", "tool", "127.0.0.1:5000", 7,
                                                "", {}, "", 0, {}, "", {}});
    std::size_t mark = r.log.size();
    say(said);
    drawn = r.read<gfx::Drawn>(mark, cpane::kConnectionsPaneRole);
    REQUIRE(drawn.size() == 1);
    CHECK_FALSE(drawn[0].whole);
    CHECK(put_paths(drawn[0]) == Paths{"header", "rows", "rows/0"});
    CHECK(drawn[0].removed == Paths{"waiting"});
    std::vector<std::string> rows = r.shown().rows;
    CHECK(row_says(rows, 0, "CONNECTIONS -- 1 connection at 127.0.0.1:4242"));
    CHECK(row_says(rows, 1, "  #1 tool  admitted  weave 7  from 127.0.0.1:5000"));

    said.rows.push_back(ws::v3::GuestConnection{2, "awaiting-decision", "stranger", "", "", 0, "",
                                                {}, "", 0, {}, "", {}});
    mark = r.log.size();
    say(said);
    drawn = r.read<gfx::Drawn>(mark, cpane::kConnectionsPaneRole);
    REQUIRE(drawn.size() == 1);
    CHECK_FALSE(drawn[0].whole);
    CHECK(put_paths(drawn[0]) == Paths{"header", "rows/1"}); // the first connection stands
    CHECK(drawn[0].removed.empty());
    rows = r.shown().rows;
    CHECK(row_says(rows, 0, "CONNECTIONS -- 2 connections at 127.0.0.1:4242"));
    CHECK(row_says(rows, 1, "  #1 tool  admitted  weave 7  from 127.0.0.1:5000"));
    CHECK(row_says(rows, 2, "  #2 'stranger' (unverified)  awaiting-decision"));

    // NOTHING IN IT IS ACTED ON: no drawing it sent names a part.
    for (const gfx::Drawn& d : r.read<gfx::Drawn>(0, cpane::kConnectionsPaneRole)) {
        CHECK(d.said.empty());
        CHECK(d.unsaid.empty());
    }
}
