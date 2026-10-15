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

#include "graphics/canvas.hpp"
#include "graphics/drawing.hpp"
#include "graphics/implied.hpp"
#include "graphics/ops.hpp"
#include "graphics/painting.hpp"
#include "graphics/term_paint.hpp"
#include "graphics/vocabulary.hpp"
#include "graphics/window/window.hpp"
#include "input/translate.hpp"
#include "manuals/zengine.graphics.hpp"
#include "manuals/zengine.window.terminal.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <memory>
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

/// The snake in a window whose terminal is the suite's: the window mounted in process at its
/// office and activated as Loom's control door activates.
struct Typed {
    Rig r;
    TermLog term;
    loom::WeaveId window;
    Typed() {
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        auto w = std::make_unique<FakeWindow>(FakeTerminal{&term});
        FakeWindow* raw = w.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        window = r.bus.register_weave(std::move(w), std::move(g), window_office());
        raw->zen_set_self(window);
        const loom::WeaveId door = zengine::testing::mount_door(r.bus);
        zengine::testing::order_activation(r.bus, door, window, 1);
        r.drain();
        REQUIRE(r.load("snake-world-v1", WORLD_V1_SO, snake::kWorldRole).valid());
        r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kArea, snake::kWorldRole,
                  "classic");
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

TEST_CASE("a press the board does not take reaches the world that drew it") {
    Typed t;
    t.r.tick();
    const std::size_t mark = t.r.log.size();
    t.press(13, 10); // the head's cell: board cell (12,8), one cell in from the border, below row 0
    const auto pressed = t.r.read<gfx::ElementPressed>(mark);
    REQUIRE(pressed.size() == 1);
    CHECK(pressed[0].area == kArea);
    CHECK(pressed[0].element == "board/grid");
    CHECK(pressed[0].path == "board/grid/c." + std::to_string(8 * 24 + 12));
    CHECK(t.r.log[t.r.log.size() - 1].addressed_role == snake::kWorldRole);
}

TEST_CASE("the new-game button is the world's action, and a new game reaches the board as a change") {
    Typed t;
    for (int i = 0; i < 4; ++i) {
        t.r.tick();
    }
    const std::size_t mark = t.r.log.size();
    t.press(3, 19); // "[ new game ]"
    const auto issued = t.r.read<gfx::ActionIssued>(mark);
    REQUIRE(issued.size() == 1);
    CHECK(issued[0].action == "new-game");
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


