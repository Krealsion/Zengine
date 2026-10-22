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
#include "window_backends.hpp"

#include "board.hpp"

#include "attention/vocabulary.hpp"
#include "external-host/connections-pane/vocabulary.hpp"
#include "graphics/canvas.hpp"
#include "graphics/drawing.hpp"
#include "graphics/elements.hpp"
#include "graphics/implied.hpp"
#include "graphics/light_paint.hpp"
#include "graphics/ops.hpp"
#include "graphics/painting.hpp"
#include "graphics/role.hpp"
#include "graphics/term_paint.hpp"
#include "graphics/vocabulary.hpp"
#include "graphics/window/terminal_io.hpp"
#include "graphics/window/window_sdl.hpp"
#include "graphics/window/window_terminal.hpp"
#include "input/terminal_reader.hpp"
#include "input/translate.hpp"
#include "manuals/zengine.graphics.hpp"
#include "manuals/zengine.window.sdl.hpp"
#include "manuals/zengine.window.terminal.hpp"
#include "workshop/attention_seam_vocabulary.hpp"
#include "workshop/guest_seam_vocabulary.hpp"
#include "workshop/pane_facets.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(GRAPHICS_HAS_SDL)
#include "graphics/window/window_sdl_backend.hpp"

#include <SDL3/SDL.h>
#endif

namespace gfx = zengine::graphics;
namespace snake = zengine::snake;
using zengine::testing::graphics_rig::Rig;
using zengine::testing::graphics_rig::window_office;
using zengine::testing::window_backends::FakeSdlBackend;
using zengine::testing::window_backends::FakeTerminal;
using zengine::testing::window_backends::LightLog;
using zengine::testing::window_backends::TermLog;

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

using FakeWindow = gfx::window::WindowT<FakeTerminal>;

/// What a window answered the suite's asker for its platform, by the correlation of the ask each
/// answers, and what the window published as its office; every shape heard, in order.
struct PlatformHeard {
    std::vector<std::string> order;
    std::vector<std::pair<std::uint64_t, gfx::ClipboardText>> texts;
    std::vector<std::pair<std::uint64_t, gfx::SurfaceCaptured>> captures;
    std::vector<std::pair<std::uint64_t, gfx::SurfaceCaptureChunk>> chunks;
    std::vector<std::pair<std::uint64_t, std::string>> refused;
    std::vector<gfx::SurfacePlacement> placements;
    std::vector<gfx::WindowOffered> offers;
    int closes = 0;
};

bool placed_at(const gfx::SurfacePlacement& p, std::int64_t x, std::int64_t y, bool maximized) {
    return p.x == x && p.y == y && p.maximized == maximized;
}

struct GraphicsTestPlatform {
    std::int64_t heard = 0;
    ZEN_SHAPE(GraphicsTestPlatform, 1, ZEN_FIELD(heard));
};

class PlatformEars
    : public loom::WeaveBase<
          PlatformEars, GraphicsTestPlatform,
          loom::Accept<gfx::ClipboardText, gfx::SurfaceCaptured, gfx::SurfaceCaptureChunk,
                       loom::Refused, gfx::SurfacePlacement, gfx::SurfaceCloseRequested,
                       gfx::WindowOffered>,
          loom::Emit<gfx::ClipboardCopy, gfx::ClipboardTextRequested,
                     gfx::SurfacePlacementRemembered, gfx::PumpSurface,
                     gfx::SurfaceCaptureRequested, gfx::SurfaceCaptureChunkRequested>> {
public:
    PlatformEars(PlatformHeard& heard, std::string window)
        : heard_(&heard), window_(std::move(window)) {}
    void on(const gfx::ClipboardText& c, loom::Mail& mail) {
        note("ClipboardText");
        heard_->texts.emplace_back(mail.correlation(), c);
    }
    void on(const gfx::SurfaceCaptured& c, loom::Mail& mail) {
        note("SurfaceCaptured");
        heard_->captures.emplace_back(mail.correlation(), c);
    }
    void on(const gfx::SurfaceCaptureChunk& c, loom::Mail& mail) {
        note("SurfaceCaptureChunk");
        heard_->chunks.emplace_back(mail.correlation(), c);
    }
    void on(const loom::Refused& r, loom::Mail& mail) {
        note("Refused");
        heard_->refused.emplace_back(mail.correlation(), r.reason);
    }
    void on(const gfx::SurfacePlacement& p, loom::Mail& mail) {
        if (mail.authored_from_role(window_)) {
            note("SurfacePlacement");
            heard_->placements.push_back(p);
        }
    }
    void on(const gfx::SurfaceCloseRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(window_)) {
            note("SurfaceCloseRequested");
            ++heard_->closes;
        }
    }
    void on(const gfx::WindowOffered& w, loom::Mail& mail) {
        if (mail.authored_from_role(window_)) {
            note("WindowOffered");
            heard_->offers.push_back(w);
        }
    }

private:
    void note(const char* shape) {
        ++state_.heard;
        heard_->order.emplace_back(shape);
    }
    PlatformHeard* heard_;
    std::string window_;
};

/// The suite as a participant asking a window for its platform: it asks the window's office and
/// keeps each answer by its ask, publishes a copy, and hears what the window publishes.
struct PlatformAsker {
    Rig* r = nullptr;
    std::string window;
    loom::WeaveId id;
    PlatformHeard heard;

    void mount(Rig& rig, std::string office) {
        r = &rig;
        window = std::move(office);
        auto made = std::make_unique<PlatformEars>(heard, window);
        PlatformEars* raw = made.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        id = r->bus.register_weave(std::move(made), std::move(g), "test.platform");
        raw->zen_set_self(id);
    }
    /// One ask of the window, its correlation returned so the answer is known as this ask's.
    template <class T>
    std::uint64_t ask(const T& msg) {
        const std::uint64_t corr = r->next_corr++;
        r->bus.send_as_to_role(id, window, loom::Message(loom::to_value(msg), id, id, corr));
        r->drain();
        return corr;
    }
    void copy(const std::string& text) {
        r->bus.publish_as(id, loom::Message(loom::to_value(gfx::ClipboardCopy{text})));
        r->drain();
    }
    /// The answer to one ask, by value: a later answer grows the list it was kept in.
    std::optional<gfx::SurfaceCaptured> captured(std::uint64_t corr) const {
        for (const auto& [c, v] : heard.captures) {
            if (c == corr) return v;
        }
        return std::nullopt;
    }
    std::optional<std::string> refused(std::uint64_t corr) const {
        for (const auto& [c, v] : heard.refused) {
            if (c == corr) return v;
        }
        return std::nullopt;
    }
    /// The retained picture `capture`, fetched chunk by chunk from the start; empty when a chunk
    /// does not come.
    std::string fetch(std::int64_t capture, std::int64_t total, std::size_t* asks = nullptr) {
        std::string bytes;
        std::size_t n = 0;
        while (static_cast<std::int64_t>(bytes.size()) < total) {
            const std::uint64_t corr = ask(gfx::SurfaceCaptureChunkRequested{
                capture, static_cast<std::int64_t>(bytes.size())});
            ++n;
            const gfx::SurfaceCaptureChunk* got = nullptr;
            for (const auto& [c, v] : heard.chunks) {
                if (c == corr) got = &v;
            }
            if (got == nullptr || got->data.empty() || got->total != total ||
                got->offset != static_cast<std::int64_t>(bytes.size())) {
                return {};
            }
            bytes.append(got->data.begin(), got->data.end());
        }
        if (asks != nullptr) *asks = n;
        return bytes;
    }
};

/// A window whose terminal is the suite's, beside the directory: the window mounted in process at
/// its office and activated as Loom's control door activates. Asked to, the suite's asker is
/// mounted first, and hears the window from its activation on.
struct TypedWindow {
    Rig r;
    TermLog term;
    PlatformAsker platform;
    loom::WeaveId window;
    FakeWindow* instance = nullptr;
    explicit TypedWindow(bool hear = false) {
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        if (hear) {
            platform.mount(r, window_office());
        }
        auto w = std::make_unique<FakeWindow>(FakeTerminal{&term});
        FakeWindow* raw = w.get();
        instance = raw;
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
    /// The window's own beat, with nothing typed.
    void pump() { type(""); }
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

TEST_CASE("the window holds the process's terminal input while it is loaded, reloaded or replaced, "
          "and gives it back when it goes") {
    namespace in = zengine::input;
    REQUIRE_FALSE(in::window_holds_terminal_input());
    {
        Rig r;
        REQUIRE(r.load("zengine-window-terminal", WINDOW_SO, window_office()).valid());
        CHECK(in::window_terminal_input_claims() == 1);

        // A reload makes the successor before it destroys its predecessor: one claim, not none.
        const auto reloaded = r.command(loom::ReloadWeave{"zengine-window-terminal", WINDOW_SO});
        REQUIRE_MESSAGE(reloaded.kind == 1, reloaded.text);
        CHECK(in::window_terminal_input_claims() == 1);

        const auto swapped = r.command(loom::SwapWeave{window_office(), "zengine-window-terminal",
                                                       WINDOW_SO, /*graceful=*/false});
        REQUIRE_MESSAGE(swapped.kind == 0, swapped.text);
        CHECK(in::window_terminal_input_claims() == 1);
    }
    CHECK_FALSE(in::window_holds_terminal_input());
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
    CHECK(rooms[0].empty);
    CHECK(b.r.shown().elements == 0);
}

TEST_CASE("an area withdrawn and placed again at the same size starts empty, its room says so, and "
          "its owner draws it whole") {
    Drawing d;
    d.beat();
    REQUIRE(d.r.shown().rows[0] == "beats 1");
    REQUIRE(d.r.shown().elements == 1);
    const auto place = [&](const std::string& window) {
        d.r.to_role(gfx::kGraphicsRole,
                    gfx::AreaPlaced{kDrawer, window, kDrawer, 0, 0, 50 * gfx::kCellPx,
                                    22 * gfx::kCellPx, 0, true});
        d.r.drain();
    };
    // PLACED AGAIN WHERE IT STANDS, the window keeps the drawing and says so.
    std::size_t mark = d.r.log.size();
    place(gfx::kTerminalWindow);
    auto rooms = d.r.read<gfx::AreaRoom>(mark, window_office());
    REQUIRE(rooms.size() == 1);
    CHECK_FALSE(rooms[0].empty);
    CHECK(rooms[0].elements == std::vector<std::int64_t>{1});
    // WITHDRAWN, then placed again in the same window, incarnation and size: nothing of it is
    // left there, the room says the area is empty, and the owner's picture and element come back.
    place("");
    CHECK(d.r.shown().elements == 0);
    mark = d.r.log.size();
    place(gfx::kTerminalWindow);
    rooms = d.r.read<gfx::AreaRoom>(mark, window_office());
    REQUIRE(rooms.size() == 1);
    CHECK(rooms[0].empty);
    CHECK(rooms[0].elements.empty());
    CHECK(d.r.shown().rows[0] == "beats 1");
    CHECK(d.r.shown().elements == 1);
}

TEST_CASE("a terminal window with no terminal to measure shows as far as its areas reach, and no "
          "less than its unmeasured size") {
    Drawing d; // the suite's terminal is no terminal: a pipe
    d.beat();
    REQUIRE(d.r.shown().rows.size() == static_cast<std::size_t>(gfx::window::kUnmeasuredRows));
    d.r.to_role(gfx::kGraphicsRole,
                gfx::AreaPlaced{kDrawer, gfx::kTerminalWindow, kDrawer, 100 * gfx::kCellPx,
                                30 * gfx::kCellPx, 50 * gfx::kCellPx, 22 * gfx::kCellPx, 0, true});
    d.r.drain();
    const gfx::WindowShown shown = d.r.shown();
    REQUIRE(shown.rows.size() == 52);
    CHECK(shown.rows[30].substr(100) == "beats 1");
    // BACK WHERE IT STARTED, the window is its unmeasured size again.
    d.r.to_role(gfx::kGraphicsRole,
                gfx::AreaPlaced{kDrawer, gfx::kTerminalWindow, kDrawer, 0, 0, 50 * gfx::kCellPx,
                                22 * gfx::kCellPx, 0, true});
    d.r.drain();
    CHECK(d.r.shown().rows.size() == static_cast<std::size_t>(gfx::window::kUnmeasuredRows));
    CHECK(d.r.shown().rows[0] == "beats 1");
}

TEST_CASE("a canvas told its area is empty draws whole again, in the same window and room") {
    gfx::Canvas canvas("test.area");
    REQUIRE(canvas.room(gfx::AreaRoom{"test.area", "terminal", 7, 240, 120, {3}}));
    CHECK(canvas.holds(3));
    CHECK_FALSE(canvas.room(gfx::AreaRoom{"test.area", "terminal", 7, 240, 120, {3}}));
    CHECK(canvas.room(gfx::AreaRoom{"test.area", "terminal", 7, 240, 120, {}, true}));
    CHECK_FALSE(canvas.holds(3));
}

TEST_CASE("a canvas told its window sets text in another measure draws whole again, and keeps the "
          "measure for its owner") {
    gfx::Canvas canvas("test.area");
    REQUIRE(canvas.room(gfx::AreaRoom{"test.area", "sdl", 7, 240, 120, {}, false, gfx::kCellPx,
                                      8, 18}));
    CHECK(canvas.cell_px() == gfx::kCellPx);
    CHECK(canvas.text() == gfx::TextMeasure{8, 18});
    CHECK_FALSE(canvas.room(gfx::AreaRoom{"test.area", "sdl", 7, 240, 120, {}, false,
                                          gfx::kCellPx, 8, 18}));
    CHECK(canvas.room(gfx::AreaRoom{"test.area", "sdl", 7, 240, 120, {}, false, gfx::kCellPx, 7,
                                    16}));
    CHECK(canvas.text() == gfx::TextMeasure{7, 16});
    // A ROOM THAT SAYS NO MEASURE is a window whose text is the cell.
    CHECK(canvas.room(gfx::AreaRoom{"test.area", "terminal", 7, 240, 120, {}, false, 0, 0, 0}));
    CHECK(canvas.text() == gfx::TextMeasure{});
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
    CHECK(t.r.read<gfx::AreaButtonChanged>(mark).empty());
    for (std::size_t i = mark; i < t.r.log.size(); ++i) {
        CHECK(t.r.log[i].addressed_role != snake::kWorldRole);
    }
}

/// AN OWNER OF AN AREA, as the window's office tells it: every pointer moment it is given, in
/// order, each with the number its delivery carried; the actions it is asked; and what a window
/// told it of the holds and hover there. Asked to, it draws `words` at its corner,
/// naming `action` when one is given.
struct MomentsHeard {
    std::vector<std::pair<gfx::AreaButtonChanged, std::uint64_t>> buttons;
    std::vector<std::pair<gfx::AreaPointerMoved, std::uint64_t>> moves;
    std::vector<gfx::AreaWheelTurned> wheels;
    std::vector<gfx::AreaHoldEnded> ended;
    std::vector<gfx::AreaPointerLeft> left;
    std::vector<gfx::ActionRequested> actions;
    std::vector<std::string> order;
    bool empty() const { return order.empty(); }
};

bool from_a_window(const loom::Mail& mail) {
    return mail.authored_role().rfind(gfx::kWindowOfficePrefix, 0) == 0;
}

class MomentsOwner
    : public loom::WeaveBase<MomentsOwner, GraphicsTestOwner,
                             loom::Accept<gfx::AreaRoom, gfx::AreaButtonChanged,
                                          gfx::AreaPointerMoved, gfx::AreaWheelTurned,
                                          gfx::AreaHoldEnded, gfx::AreaPointerLeft,
                                          gfx::ActionRequested>,
                             loom::Emit<gfx::Drawn>> {
public:
    MomentsOwner(MomentsHeard& heard, std::string office, std::string words,
                 std::string action)
        : heard_(&heard), office_(std::move(office)), words_(std::move(words)),
          action_(std::move(action)) {}
    void on(const gfx::AreaRoom& r, loom::Mail& mail) {
        ++state_.heard;
        if (words_.empty() || !from_a_window(mail)) {
            return;
        }
        gfx::Sketch s;
        s.text("words", 0, 0, words_, gfx::by_role(gfx::role::kFill));
        if (!action_.empty()) {
            s.action("words", action_);
        }
        (void)mail.as_role(office_).send_to_role(
            std::string(gfx::kWindowOfficePrefix) + r.window,
            gfx::Drawn{r.area, true, s.nodes(), {}, s.records(), {}, false});
    }
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        if (!from_a_window(mail)) return;
        heard_->buttons.emplace_back(b, mail.correlation());
        heard_->order.push_back("button");
    }
    void on(const gfx::AreaPointerMoved& m, loom::Mail& mail) {
        if (!from_a_window(mail)) return;
        heard_->moves.emplace_back(m, mail.correlation());
        heard_->order.push_back("move");
    }
    void on(const gfx::AreaWheelTurned& w, loom::Mail& mail) {
        if (!from_a_window(mail)) return;
        heard_->wheels.push_back(w);
        heard_->order.push_back("wheel");
    }
    void on(const gfx::AreaHoldEnded& e, loom::Mail& mail) {
        if (!from_a_window(mail)) return;
        heard_->ended.push_back(e);
        heard_->order.push_back("ended");
    }
    void on(const gfx::AreaPointerLeft& l, loom::Mail& mail) {
        if (!from_a_window(mail)) return;
        heard_->left.push_back(l);
        heard_->order.push_back("left");
    }
    void on(const gfx::ActionRequested& a, loom::Mail& mail) {
        if (!from_a_window(mail)) return;
        heard_->actions.push_back(a);
        heard_->order.push_back("action");
    }

private:
    MomentsHeard* heard_;
    std::string office_, words_, action_;
};

/// AN ARRANGER THE SUITE SPEAKS AS: it places areas under its own office, and keeps where the
/// window tells it presses landed, each with the number its delivery carried, and what the window
/// tells it of the holds and hover in the areas it placed.
struct PressesHeard {
    std::vector<std::pair<gfx::AreaPressed, std::uint64_t>> pressed;
    std::vector<gfx::AreaButtonChanged> own;
    std::vector<gfx::AreaHoldEnded> ended;
    std::vector<gfx::AreaPointerLeft> left;
};

class ArrangerOwner
    : public loom::WeaveBase<ArrangerOwner, GraphicsTestOwner,
                             loom::Accept<gfx::AreaRoom, gfx::AreaPressed, gfx::AreaButtonChanged,
                                          gfx::AreaHoldEnded, gfx::AreaPointerLeft>,
                             loom::Emit<gfx::AreaPlaced, gfx::SkinChosen>> {
public:
    explicit ArrangerOwner(PressesHeard& heard) : heard_(&heard) {}
    void on(const gfx::AreaRoom&, loom::Mail&) { ++state_.heard; }
    void on(const gfx::AreaPressed& p, loom::Mail& mail) {
        if (from_a_window(mail)) heard_->pressed.emplace_back(p, mail.correlation());
    }
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        if (from_a_window(mail)) heard_->own.push_back(b);
    }
    void on(const gfx::AreaHoldEnded& e, loom::Mail& mail) {
        if (from_a_window(mail)) heard_->ended.push_back(e);
    }
    void on(const gfx::AreaPointerLeft& l, loom::Mail& mail) {
        if (from_a_window(mail)) heard_->left.push_back(l);
    }

private:
    PressesHeard* heard_;
};

constexpr const char* kArranger = "test.arranger";

/// THE TERMINAL WINDOW WITH OWNERS OF THE SUITE'S OWN, each area placed by the suite as root, or
/// by the suite's arranger under its office.
struct PointerDesk : TypedWindow {
    PressesHeard presses;
    loom::WeaveId arranger;
    std::vector<std::unique_ptr<MomentsHeard>> heard;
    PointerDesk() {
        r.to_role(gfx::kGraphicsRole, gfx::SkinOffered{"classic", PAINT_CLASSIC_SO});
        auto made = std::make_unique<ArrangerOwner>(presses);
        ArrangerOwner* raw = made.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        arranger = r.bus.register_weave(std::move(made), std::move(g), kArranger);
        raw->zen_set_self(arranger);
        r.drain();
    }
    /// An owner of `office`, its area placed at cell (`col`, `row`), `w` by `h` cells.
    MomentsHeard& own(const char* office, std::int64_t col, std::int64_t row, std::int64_t w,
                      std::int64_t h, std::int64_t order, bool pointer, bool by_arranger = false,
                      std::string words = {}, std::string action = {}) {
        heard.push_back(std::make_unique<MomentsHeard>());
        auto made = std::make_unique<MomentsOwner>(*heard.back(), office, std::move(words),
                                                   std::move(action));
        MomentsOwner* raw = made.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        raw->zen_set_self(r.bus.register_weave(std::move(made), std::move(g), office));
        place(office, office, col, row, w, h, order, pointer, by_arranger);
        r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{office, "classic"});
        r.drain();
        return *heard.back();
    }
    void place(const char* area, const char* owner, std::int64_t col, std::int64_t row,
               std::int64_t w, std::int64_t h, std::int64_t order, bool pointer,
               bool by_arranger = false) {
        constexpr std::int64_t c = gfx::kCellPx;
        gfx::AreaPlaced placed{area,  gfx::kTerminalWindow, owner, col * c, row * c, w * c, h * c,
                               order, false};
        placed.pointer = pointer;
        if (by_arranger) {
            r.bus.office_send_to_role_as(arranger, kArranger, gfx::kGraphicsRole,
                                         loom::Message(loom::to_value(placed)));
        } else {
            r.to_role(gfx::kGraphicsRole, placed);
        }
        r.drain();
    }
    /// A button pressed (`M`) or let go (`m`) at a cell, 0-based, as an SGR report: 0 left,
    /// 1 middle, 2 right.
    void button(int code, int col, int row, bool down) {
        type("\x1b[<" + std::to_string(code) + ";" + std::to_string(col + 1) + ";" +
             std::to_string(row + 1) + (down ? "M" : "m"));
    }
    /// The pointer moved to a cell with no button held, or with the left one held.
    void move(int col, int row, bool held = false) {
        type("\x1b[<" + std::to_string(held ? 32 : 35) + ";" + std::to_string(col + 1) + ";" +
             std::to_string(row + 1) + "M");
    }
};

/// A window cell's centre, in the pixels of an area whose corner stands at cell `origin`.
std::int64_t local(std::int64_t cell, std::int64_t origin) {
    return cell * gfx::kCellPx + gfx::kCellPx / 2 - origin * gfx::kCellPx;
}

TEST_CASE("the terminal window gives the owner of an area placed to hear the pointer each moment "
          "there as the moment it was, naming the area, at its own point in pixels, and an area "
          "placed without it none") {
    PointerDesk t;
    MomentsHeard& hearing = t.own("test.hearing", 10, 2, 20, 5, 1, true);
    MomentsHeard& deaf = t.own("test.deaf", 10, 8, 20, 5, 1, false);

    t.button(0, 12, 3, false); // the left button let go at cell (12, 3), no press held for it
    // The right button pressed at (13, 4) and dragged with it held to (15, 4), then (16, 5).
    t.type("\x1b[<2;14;5M\x1b[<34;16;5M\x1b[<34;17;6M");
    t.button(2, 16, 5, false);
    t.type("\x1b[<64;15;4M"); // the wheel turned up at (14, 3)
    CHECK(hearing.order ==
          std::vector<std::string>{"button", "button", "move", "button", "wheel"});
    REQUIRE(hearing.buttons.size() == 3);
    const gfx::AreaButtonChanged& released = hearing.buttons[0].first;
    CHECK(released.area == "test.hearing");
    CHECK(released.button == 1);
    CHECK_FALSE(released.pressed);
    CHECK(released.x == local(12, 10));
    CHECK(released.y == local(3, 2));
    CHECK(hearing.buttons[0].second == 0); // a release no press of this window's holds
    CHECK(hearing.buttons[1].first.button == 3);
    CHECK(hearing.buttons[1].first.pressed);
    CHECK(hearing.buttons[1].first.x == local(13, 10));
    // TWO MOVES READ IN ONE BEAT GO AS ONE: the last point, the deltas summed, in pixels.
    REQUIRE(hearing.moves.size() == 1);
    CHECK(hearing.moves[0].first.x == local(16, 10));
    CHECK(hearing.moves[0].first.y == local(5, 2));
    CHECK(hearing.moves[0].first.dx == 3 * gfx::kCellPx);
    CHECK(hearing.moves[0].first.dy == gfx::kCellPx);
    REQUIRE(hearing.wheels.size() == 1);
    CHECK(hearing.wheels[0].area == "test.hearing");
    CHECK(hearing.wheels[0].dy == doctest::Approx(1.0));
    CHECK(hearing.wheels[0].x == local(14, 10));
    CHECK(hearing.wheels[0].y == local(3, 2));

    // THE SAME MOMENTS IN AN AREA PLACED WITHOUT THE POINTER, OR IN NONE, REACH NO ONE.
    t.type("\x1b[<0;13;10m\x1b[<2;14;11M\x1b[<34;16;11M\x1b[<2;16;11m\x1b[<64;15;10M");
    t.type("\x1b[<0;60;20m\x1b[<2;60;20M\x1b[<34;61;21M\x1b[<2;61;21m\x1b[<64;60;20M");
    CHECK(deaf.empty());
    CHECK(hearing.order.size() == 5);
}

TEST_CASE("a press holds its button for the area that took it: every move while it is held, and "
          "its release, go to that area wherever the pointer is, all under the press's number") {
    PointerDesk t;
    MomentsHeard& hearing = t.own("test.hearing", 10, 2, 20, 5, 1, true);
    MomentsHeard& other = t.own("test.other", 40, 2, 20, 5, 1, true);

    t.button(0, 12, 3, true);
    t.move(45, 4, true);      // over the other area, the button held
    t.button(0, 70, 20, false); // let go outside every area
    REQUIRE(hearing.buttons.size() == 2);
    const std::uint64_t press = hearing.buttons[0].second;
    CHECK(press != 0);
    REQUIRE(hearing.moves.size() == 1);
    CHECK(hearing.moves[0].first.x == local(45, 10)); // outside its own area, in its own pixels
    CHECK(hearing.moves[0].second == press);
    CHECK_FALSE(hearing.buttons[1].first.pressed);
    CHECK(hearing.buttons[1].first.x == local(70, 10));
    CHECK(hearing.buttons[1].first.y == local(20, 2));
    CHECK(hearing.buttons[1].second == press);
    CHECK(other.empty()); // the pointer crossed it, held by another's press

    // ONCE LET GO, A MOVE IS THE AREA'S IT LANDS IN, and the next press has a number of its own.
    t.move(45, 4);
    REQUIRE(other.moves.size() == 1);
    CHECK(other.moves[0].second == 0);
    t.button(0, 46, 4, true);
    REQUIRE(other.buttons.size() == 1);
    CHECK(other.buttons[0].second != 0);
    CHECK(other.buttons[0].second != press);
    CHECK(hearing.moves.size() == 1);
}

TEST_CASE("the window tells the arranger of an area another office owns where each press there and "
          "its release landed, under the press's number, and no arranger that owns the area") {
    PointerDesk t;
    MomentsHeard& owned = t.own("test.hearing", 10, 2, 20, 5, 1, true, /*by_arranger=*/true);
    MomentsHeard& rooted = t.own("test.rooted", 40, 2, 20, 5, 1, true);
    t.place(kArranger, kArranger, 10, 10, 20, 5, 1, true, /*by_arranger=*/true);
    t.r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{kArranger, "classic"});
    t.r.drain();

    t.button(2, 12, 3, true);
    t.button(2, 50, 15, false); // let go elsewhere: still the press's area's release
    REQUIRE(owned.buttons.size() == 2);
    REQUIRE(t.presses.pressed.size() == 2);
    const gfx::AreaPressed& down = t.presses.pressed[0].first;
    CHECK(down.window == gfx::kTerminalWindow);
    CHECK(down.area == "test.hearing");
    CHECK(down.button == 3);
    CHECK(down.pressed);
    CHECK(down.x == 12 * gfx::kCellPx + gfx::kCellPx / 2); // the window's own pixels
    CHECK(down.y == 3 * gfx::kCellPx + gfx::kCellPx / 2);
    CHECK(t.presses.pressed[0].second == owned.buttons[0].second);
    CHECK_FALSE(t.presses.pressed[1].first.pressed);
    CHECK(t.presses.pressed[1].first.x == 50 * gfx::kCellPx + gfx::kCellPx / 2);
    CHECK(t.presses.pressed[1].second == owned.buttons[0].second);

    // An area placed by personal speech names no arranger, and one the arranger owns is its own.
    t.button(0, 45, 3, true);
    t.button(0, 45, 3, false);
    t.button(0, 12, 12, true);
    t.button(0, 12, 12, false);
    CHECK(rooted.buttons.size() == 2);
    CHECK(t.presses.own.size() == 2);
    CHECK(t.presses.pressed.size() == 2);
}

TEST_CASE("in an area placed to hear the pointer a press on a part naming an action is the press "
          "it is, and in one placed without it the action is asked") {
    PointerDesk t;
    MomentsHeard& hearing = t.own("test.hearing", 10, 2, 20, 5, 1, true, false, "act", "act");
    MomentsHeard& deaf = t.own("test.deaf", 10, 8, 20, 5, 1, false, false, "act", "act");
    REQUIRE(t.r.screen().find("act") != std::string::npos);

    t.button(0, 11, 2, true);
    t.button(0, 11, 2, false);
    CHECK(hearing.actions.empty());
    REQUIRE(hearing.buttons.size() == 2);
    CHECK(hearing.buttons[0].first.pressed);
    CHECK(hearing.buttons[0].first.x == local(11, 10));

    t.button(0, 11, 8, true);
    t.button(0, 11, 8, false);
    REQUIRE(deaf.actions.size() == 1);
    CHECK(deaf.actions[0].area == "test.deaf");
    CHECK(deaf.actions[0].action == "act");
    CHECK(deaf.buttons.empty());
}

TEST_CASE("an area placed to hear the pointer takes every point inside it, painted there or not, "
          "and an area placed without it lets the pointer through wherever nothing it drew there "
          "takes a press") {
    PointerDesk t;
    MomentsHeard& back = t.own("test.back", 0, 0, 40, 10, 1, true, false, "back");
    MomentsHeard& front = t.own("test.front", 5, 0, 10, 4, 2, true);
    MomentsHeard& lid = t.own("test.lid", 20, 0, 10, 4, 3, false, false, "lid");
    MomentsHeard& knob = t.own("test.knob", 30, 0, 10, 4, 3, false, false, "knob", "turn");

    t.button(0, 6, 0, true); // the front area painted nothing here; behind it, the back drew
    t.button(0, 6, 0, false);
    CHECK(front.buttons.size() == 2);
    CHECK(back.empty());

    t.button(0, 21, 0, true); // the lid's words name nothing: the press is the back's
    t.button(0, 21, 0, false);
    REQUIRE(back.buttons.size() == 2);
    CHECK(back.buttons[0].first.x == local(21, 0));
    t.move(22, 0);
    CHECK(back.moves.size() == 1);
    CHECK(lid.empty());

    t.button(0, 31, 0, true); // the knob's words name an action: its owner is asked it
    REQUIRE(knob.actions.size() == 1);
    CHECK(knob.actions[0].action == "turn");
    CHECK(back.buttons.size() == 2);
}

TEST_CASE("a press held for an area ends with no release when the area is withdrawn, handed to "
          "another owner, or covered where it was made by an area placed to hear the pointer in "
          "front of it, and the window tells the area's owner, and its arranger when another "
          "office, that it ended -- and no one else") {
    PointerDesk t;
    MomentsHeard& hearing = t.own("test.hearing", 10, 2, 20, 5, 1, true);
    MomentsHeard& beside = t.own("test.beside", 40, 2, 10, 5, 2, true);

    SUBCASE("an area in front elsewhere leaves the hold standing; one covering the press ends it") {
        t.button(0, 12, 3, true);
        t.place("test.beside", "test.beside", 40, 2, 10, 5, 5, true); // in front, not over it
        t.move(14, 4, true);
        CHECK(hearing.ended.empty());
        CHECK(hearing.moves.size() == 1);
        MomentsHeard& cover = t.own("test.cover", 11, 2, 5, 3, 2, true);
        REQUIRE(hearing.ended.size() == 1);
        CHECK(hearing.ended[0].area == "test.hearing");
        CHECK(hearing.ended[0].button == 1);
        CHECK(beside.ended.empty()); // an owner of another area hears nothing of it
        CHECK(cover.ended.empty());
        CHECK(t.presses.ended.empty()); // an area placed personally names no arranger
        // THE RELEASE THEN GOES WHERE IT LANDS: the area in front hears it, numbered by no press.
        t.button(0, 12, 3, false);
        CHECK(hearing.buttons.size() == 1);
        REQUIRE(cover.buttons.size() == 1);
        CHECK_FALSE(cover.buttons[0].first.pressed);
        CHECK(cover.buttons[0].second == 0);
    }
    SUBCASE("withdrawn") {
        t.button(2, 12, 3, true);
        t.r.to_role(gfx::kGraphicsRole,
                    gfx::AreaPlaced{"test.hearing", "", "test.hearing", 0, 0, 0, 0, 0, false});
        t.r.drain();
        REQUIRE(hearing.ended.size() == 1);
        CHECK(hearing.ended[0].button == 3);
        t.button(2, 12, 3, false);
        CHECK(hearing.buttons.size() == 1);
    }
    SUBCASE("handed to another owner") {
        t.button(0, 12, 3, true);
        MomentsHeard& heir = t.own("test.heir", 60, 20, 1, 1, 1, true);
        t.place("test.hearing", "test.heir", 10, 2, 20, 5, 1, true);
        REQUIRE(hearing.ended.size() == 1);
        CHECK(heir.ended.empty()); // told to the owner the press was held for
        t.button(0, 12, 3, false);
        CHECK(hearing.buttons.size() == 1);
        CHECK(heir.buttons.size() == 1); // the area's new owner hears the release where it lands
    }
    SUBCASE("in an area another office placed, its arranger is told too") {
        MomentsHeard& placed = t.own("test.placed", 10, 10, 20, 5, 1, true, /*by_arranger=*/true);
        t.button(0, 12, 12, true);
        t.r.bus.office_send_to_role_as(
            t.arranger, kArranger, gfx::kGraphicsRole,
            loom::Message(loom::to_value(
                gfx::AreaPlaced{"test.placed", "", "test.placed", 0, 0, 0, 0, 0, false})));
        t.r.drain();
        REQUIRE(placed.ended.size() == 1);
        REQUIRE(t.presses.ended.size() == 1);
        CHECK(t.presses.ended[0].area == "test.placed");
        CHECK(t.presses.ended[0].button == 1);
        CHECK(hearing.ended.empty());
        CHECK(beside.ended.empty());
    }
}

TEST_CASE("the pointer leaving an area placed to hear it, with no button held, is told once, when "
          "the next move lands elsewhere, to the area's owner and its arranger when another office, "
          "and to no one else") {
    PointerDesk t;
    MomentsHeard& a = t.own("test.a", 10, 2, 10, 5, 1, true);
    MomentsHeard& b = t.own("test.b", 30, 2, 10, 5, 1, true);
    t.move(12, 3);
    t.move(13, 3);
    CHECK(a.left.empty());
    t.move(32, 3);
    REQUIRE(a.left.size() == 1);
    CHECK(a.left[0].area == "test.a");
    CHECK(b.left.empty()); // the area it landed in hears its move, not where it came from
    t.move(60, 20);
    REQUIRE(b.left.size() == 1);
    CHECK(b.left[0].area == "test.b");
    t.move(61, 20);
    CHECK(a.left.size() == 1);
    CHECK(b.left.size() == 1);
    CHECK(a.order == std::vector<std::string>{"move", "move", "left"});
    CHECK(t.presses.left.empty()); // an area placed personally names no arranger

    SUBCASE("in an area another office placed, its arranger is told too") {
        MomentsHeard& placed = t.own("test.placed", 10, 10, 10, 5, 1, true, /*by_arranger=*/true);
        t.move(12, 12);
        t.move(60, 20);
        REQUIRE(placed.left.size() == 1);
        REQUIRE(t.presses.left.size() == 1);
        CHECK(t.presses.left[0].area == "test.placed");
        CHECK(a.left.size() == 1);
        CHECK(b.left.size() == 1);
    }
    SUBCASE("an area handed to another owner while the pointer rests in it: the owner it had is "
            "told the pointer left, and the new one is not") {
        MomentsHeard& heir = t.own("test.heir", 60, 0, 1, 1, 1, true);
        t.move(12, 3);
        t.place("test.a", "test.heir", 10, 2, 10, 5, 1, true);
        REQUIRE(a.left.size() == 2);
        CHECK(heir.left.empty());
        t.move(60, 20);
        CHECK(heir.left.empty());
        CHECK(a.left.size() == 2);
    }
}

TEST_CASE("the directory names the office an area was placed as its arranger, and a placing said "
          "personally names none") {
    PointerDesk t;
    const std::size_t mark = t.r.log.size();
    (void)t.own("test.hearing", 10, 2, 20, 5, 1, true, /*by_arranger=*/true);
    (void)t.own("test.rooted", 40, 2, 20, 5, 1, true);
    std::map<std::string, std::string> arranger;
    for (const gfx::AreaIntroduced& i : t.r.read<gfx::AreaIntroduced>(mark, gfx::kGraphicsRole)) {
        arranger[i.area] = i.arranger;
    }
    CHECK(arranger.at("test.hearing") == kArranger);
    CHECK(arranger.at("test.rooted").empty());
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

/// AN OWNER THAT TAKES KEYS, THEIR RELEASES AND TEXT.
class TextOwner
    : public loom::WeaveBase<TextOwner, GraphicsTestOwner,
                             loom::Accept<gfx::AreaRoom, zengine::input::KeyPressed,
                                          zengine::input::KeyReleased,
                                          zengine::input::TextEntered>,
                             loom::Emit<>> {
public:
    explicit TextOwner(KeysHeard& heard) : heard_(&heard) {}
    void on(const gfx::AreaRoom&, loom::Mail&) { ++state_.heard; }
    void on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        if (mail.authored_from_role(window_office())) heard_->said.push_back("key " + k.name);
    }
    void on(const zengine::input::KeyReleased& k, loom::Mail& mail) {
        if (mail.authored_from_role(window_office())) heard_->said.push_back("up " + k.name);
    }
    void on(const zengine::input::TextEntered& e, loom::Mail& mail) {
        if (mail.authored_from_role(window_office())) heard_->said.push_back("text " + e.text);
    }

private:
    KeysHeard* heard_;
};

} // namespace

TEST_CASE("what a key types goes with the key and its release from the window to the owner of each "
          "area placed to hear keys, and to no other; an owner that takes no text or release "
          "refuses it on the bus") {
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
    // EACH KEY, THE CHARACTER IT TYPED, AND ITS RELEASE, to the owner placed to hear keys, as the
    // window's office, to that owner alone...
    CHECK(typing.said == std::vector<std::string>{"key H", "text h", "up H", "key I", "text i",
                                                  "up I"});
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
    // ...AND TO AN OWNER THAT TAKES KEYS ALONE, its keys, the bus refusing the text and the
    // releases it does not take, and nothing else refused.
    CHECK(keys_only.said == std::vector<std::string>{"key H", "key I"});
    CHECK(refused == std::vector<std::pair<std::uint64_t, std::string>>{
                         {keys_id.value, zengine::input::TextEntered::zen_name},
                         {keys_id.value, zengine::input::KeyReleased::zen_name},
                         {keys_id.value, zengine::input::TextEntered::zen_name},
                         {keys_id.value, zengine::input::KeyReleased::zen_name}});
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
    // ...its terminal's size in pixels, a cell's each, or none where the suite's output is no
    // terminal it can measure, and its text a cell.
    CHECK(((heard.offers[0].width > 0 && heard.offers[0].height > 0) ||
           (heard.offers[0].width == 0 && heard.offers[0].height == 0)));
    CHECK(heard.offers[0].width % gfx::kCellPx == 0);
    CHECK(heard.offers[0].text_advance_px == gfx::kCellPx);
    CHECK(heard.offers[0].text_line_px == gfx::kCellPx);
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

namespace {

/// An arranger as Workshop's lays its desk out while no skin names its room: the owner's area
/// across the whole room each offer of the window names, and each offer kept.
class RoomArranger
    : public loom::WeaveBase<RoomArranger, GraphicsTestOwner, loom::Accept<gfx::WindowOffered>,
                             loom::Emit<gfx::AreaPlaced>> {
public:
    explicit RoomArranger(OffersHeard& heard) : heard_(&heard) {}
    void on(const gfx::WindowOffered& w, loom::Mail& mail) {
        if (!mail.authored_from_role(window_office())) {
            return;
        }
        heard_->offers.push_back(w);
        heard_->from_window.push_back(true);
        (void)mail.send_to_role(gfx::kGraphicsRole,
                                gfx::AreaPlaced{kOwner, w.window, kOwner, 0, 0, w.width, w.height,
                                                0, true});
    }

private:
    OffersHeard* heard_;
};

/// An owner's words in a window whose terminal is the suite's: its area placed two cells in and
/// three down, the words at the area's corner, and the incarnation the window showed them in.
struct Resized : TypedWindow {
    OwnerHeard heard;
    loom::WeaveId owner;
    std::int64_t epoch = 0;
    explicit Resized(bool hear = false) : TypedWindow(hear) {
        auto o = std::make_unique<Owner>(heard);
        Owner* raw = o.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        owner = r.bus.register_weave(std::move(o), std::move(g), kOwner);
        raw->zen_set_self(owner);
        r.arrange(PAINT_CLASSIC_SO, PAINT_BLOCK_SO, SNAKE_SO_BOARD, kOwner, kOwner, "classic",
                  2 * gfx::kCellPx, 3 * gfx::kCellPx);
        gfx::Sketch s;
        s.text("words", 0, 0, "hello", gfx::by_role(gfx::role::kFill));
        gfx::Drawn d;
        d.area = kOwner;
        d.whole = true;
        d.put = s.nodes();
        r.bus.office_send_to_role_as(owner, kOwner, window_office(),
                                     loom::Message(loom::to_value(d)));
        r.drain();
        epoch = r.shown().epoch;
    }
};

/// The window's offers since `from` that reached the directory.
std::vector<gfx::WindowOffered> offers_to_directory(Rig& r, std::size_t from) {
    std::vector<gfx::WindowOffered> out;
    const std::uint64_t directory = r.bus.role_holder(gfx::kGraphicsRole).value;
    for (std::size_t i = from; i < r.log.size(); ++i) {
        const auto& d = r.log[i];
        if (d.shape != gfx::WindowOffered::zen_name || d.authored_role != window_office() ||
            d.target != directory) {
            continue;
        }
        if (auto w = loom::claim_item<gfx::WindowOffered>(loom::Bytes(d.bytes.begin(),
                                                                      d.bytes.end()))) {
            out.push_back(*w);
        }
    }
    return out;
}

/// The terminal at another size, then one beat: the window offers itself once in its new room
/// (none where the terminal measures none), the same incarnation, and writes one showing that
/// covers every cell of the new size -- laid over a terminal still holding something else
/// everywhere, nothing of that is left.
void check_resized(Resized& t, zengine::graphics::window::TerminalSize size, std::int64_t cols,
                   std::int64_t rows) {
    const bool measured = size.measured();
    Rig& r = t.r;
    const std::int64_t showings = r.shown().showings;
    const std::size_t writes = t.term.writes.size();
    const std::size_t mark = r.log.size();
    t.term.size = size;
    t.pump();
    const std::vector<gfx::WindowOffered> offers = offers_to_directory(r, mark);
    REQUIRE(offers.size() == 1);
    CHECK(offers[0].window == gfx::kTerminalWindow);
    CHECK(offers[0].width == (measured ? cols * gfx::kCellPx : 0));
    CHECK(offers[0].height == (measured ? rows * gfx::kCellPx : 0));
    CHECK(offers[0].epoch == t.epoch);
    CHECK(offers[0].cell_px == 0); // its unit is the cell...
    CHECK(offers[0].text_advance_px == gfx::kCellPx); // ...and its text a cell a character
    CHECK(offers[0].text_line_px == gfx::kCellPx);

    REQUIRE(t.term.writes.size() == writes + 1);
    const gfx::WindowShown after = r.shown();
    CHECK(after.showings == showings + 1);
    CHECK(after.epoch == t.epoch);
    REQUIRE(after.rows.size() == static_cast<std::size_t>(rows));
    CHECK(after.rows[3] == "  hello"); // the area where it was placed
    const std::vector<std::string> stale(static_cast<std::size_t>(rows),
                                         std::string(static_cast<std::size_t>(cols), '~'));
    const std::vector<std::string> picture = pictures_of(stale, {t.term.writes.back()}).back();
    REQUIRE(picture.size() == static_cast<std::size_t>(rows));
    for (const std::string& row : picture) {
        CHECK(row.size() == static_cast<std::size_t>(cols));
        CHECK(row.find('~') == std::string::npos);
    }
    CHECK(trimmed(picture) == trimmed(after.rows));
}

} // namespace

TEST_CASE("the window measures its terminal each beat: at another size it shows its whole picture "
          "there and offers itself again, the same incarnation; at the same size, nothing") {
    Resized t;
    Rig& r = t.r;
    REQUIRE(t.heard.refused.empty());
    REQUIRE(r.shown().rows.size() == 30);
    REQUIRE(r.shown().rows[3] == "  hello");

    SUBCASE("the same size: a beat offers nothing and shows nothing") {
        const std::size_t mark = r.log.size();
        const std::size_t writes = t.term.writes.size();
        const std::int64_t showings = r.shown().showings;
        t.pump();
        CHECK(r.count(mark, gfx::WindowOffered::zen_name) == 0);
        CHECK(t.term.writes.size() == writes);
        CHECK(r.shown().showings == showings);
    }
    SUBCASE("a larger terminal, and then a beat at that size, which offers and shows nothing") {
        check_resized(t, {100, 36}, 100, 36);
        const std::size_t mark = r.log.size();
        const std::size_t writes = t.term.writes.size();
        t.pump();
        CHECK(r.count(mark, gfx::WindowOffered::zen_name) == 0);
        CHECK(t.term.writes.size() == writes);
    }
    SUBCASE("a smaller terminal") {
        check_resized(t, {60, 20}, 60, 20);
    }
    SUBCASE("a terminal that cannot be measured: the window shows the unmeasured size, as far down "
            "as its area reaches, and offers none") {
        check_resized(t, {0, 0}, gfx::window::kUnmeasuredCols, 3 + 22); // the area's row and height
    }
}

TEST_CASE("an arranger following the window's offer places its area across the room a resized "
          "terminal gives, and the window tells the owner that room in the same incarnation") {
    Resized t;
    Rig& r = t.r;
    OffersHeard heard;
    auto made = std::make_unique<RoomArranger>(heard);
    RoomArranger* raw = made.get();
    loom::Grant g = loom::emit_default_grant(*raw);
    const loom::WeaveId arranger =
        r.bus.register_weave(std::move(made), std::move(g), "test.arranger");
    raw->zen_set_self(arranger);
    const std::size_t mark = r.log.size();
    t.term.size = {100, 36};
    t.pump();
    // THE ARRANGER HEARD THE NEW ROOM, from the window, in the incarnation it had...
    REQUIRE(heard.offers.size() == 1);
    CHECK(heard.offers[0].width == 100 * gfx::kCellPx);
    CHECK(heard.offers[0].height == 36 * gfx::kCellPx);
    CHECK(heard.offers[0].epoch == t.epoch);
    // ...AND PLACED ITS AREA ACROSS IT, which the directory introduced to the window there...
    const auto introduced = r.read<gfx::AreaIntroduced>(mark, gfx::kGraphicsRole);
    REQUIRE_FALSE(introduced.empty());
    CHECK(introduced.back().area == kOwner);
    CHECK(introduced.back().x == 0);
    CHECK(introduced.back().y == 0);
    CHECK(introduced.back().w == 100 * gfx::kCellPx);
    CHECK(introduced.back().h == 36 * gfx::kCellPx);
    // ...AND THE WINDOW TOLD THE OWNER ITS NEW ROOM, the incarnation unchanged...
    const auto rooms = r.read<gfx::AreaRoom>(mark, window_office());
    REQUIRE_FALSE(rooms.empty());
    CHECK(rooms.back().area == kOwner);
    CHECK(rooms.back().w == 100 * gfx::kCellPx);
    CHECK(rooms.back().h == 36 * gfx::kCellPx);
    CHECK(rooms.back().epoch == t.epoch);
    // ...AND SHOWS THE AREA WHERE IT NOW STANDS, at the terminal's new size, nothing left where it
    // stood before.
    const gfx::WindowShown after = r.shown();
    CHECK(after.epoch == t.epoch);
    REQUIRE(after.rows.size() == 36);
    CHECK(after.rows[0] == "hello");
    CHECK(after.rows[3].empty());
}

TEST_CASE("the terminal window copies by OSC 52, reads no clipboard, has no placement to say or take, "
          "and reads back the cells it last showed, at the size it showed them, none before") {
    {
        TypedWindow bare(true); // activated, and nothing shown yet
        CHECK_FALSE(bare.instance->capture().has_value());
        const std::uint64_t asked = bare.platform.ask(gfx::SurfaceCaptureRequested{});
        const std::optional<gfx::SurfaceCaptured> none = bare.platform.captured(asked);
        REQUIRE(none.has_value());
        CHECK_FALSE(none->ok);
        CHECK(none->refusal == "this medium has no picture to read back right now");
    }
    Resized t(true);
    Rig& r = t.r;
    FakeWindow& w = *t.instance;

    // A copy is the copy sequence, written to the terminal and no showing; a paste has no read.
    const std::int64_t showings = r.shown().showings;
    const std::size_t writes = t.term.writes.size();
    w.clipboard_copy("soup");
    REQUIRE(t.term.writes.size() == writes + 1);
    CHECK(t.term.writes.back() == "\x1b]52;c;c291cA==\x07");
    CHECK(r.shown().showings == showings);
    CHECK_FALSE(w.clipboard_text().has_value());
    CHECK_FALSE(w.placement().has_value());
    w.place(gfx::window::WindowPlacementValue{10, 20, true});
    CHECK_FALSE(w.placement().has_value());
    CHECK(t.term.writes.size() == writes + 1);

    // What it showed: every row at the terminal's width, each ending in a newline -- the rows it
    // reports, their trailing spaces kept.
    const auto shown = w.capture();
    REQUIRE(shown.has_value());
    CHECK(shown->format == "text/cells");
    CHECK(shown->cell_px == 0);
    CHECK(shown->width == 80);
    CHECK(shown->height == 30);
    REQUIRE(shown->bytes.size() == 30u * 81u);
    const auto row = [](const gfx::window::CapturedWindowPicture& p, std::size_t i) {
        const std::size_t width = static_cast<std::size_t>(p.width);
        return p.bytes.substr(i * (width + 1), width + 1);
    };
    CHECK(row(*shown, 3) == "  hello" + std::string(73, ' ') + "\n");
    CHECK(row(*shown, 0) == std::string(80, ' ') + "\n");
    const gfx::WindowShown s = r.shown();
    REQUIRE(s.rows.size() == 30);
    for (std::size_t i = 0; i < 30; ++i) {
        std::string trimmed = row(*shown, i).substr(0, 80);
        trimmed.erase(trimmed.find_last_not_of(' ') + 1);
        CHECK(trimmed == s.rows[i]);
    }

    // A drawing saying more is not shown yet, so the picture is still the one shown; shown, the
    // next picture has it.
    const auto draw = [&](const std::string& words, bool more) {
        gfx::Sketch sk;
        sk.text("words", 0, 0, words, gfx::by_role(gfx::role::kFill));
        gfx::Drawn d;
        d.area = kOwner;
        d.whole = true;
        d.put = sk.nodes();
        d.more = more;
        r.bus.office_send_to_role_as(t.owner, kOwner, window_office(),
                                     loom::Message(loom::to_value(d)));
        r.drain();
    };
    draw("world", true);
    CHECK(w.capture() == shown);
    draw("world", false);
    const auto again = w.capture();
    REQUIRE(again.has_value());
    CHECK(row(*again, 3).substr(0, 8) == "  world ");

    // At another size the next showing covers every cell, and the picture has that size.
    t.term.size = {100, 36};
    t.pump();
    const auto larger = w.capture();
    REQUIRE(larger.has_value());
    CHECK(larger->width == 100);
    CHECK(larger->height == 36);
    REQUIRE(larger->bytes.size() == 36u * 101u);
    CHECK(row(*larger, 3) == "  world" + std::string(93, ' ') + "\n");
    t.term.size = {60, 20};
    t.pump();
    const auto smaller = w.capture();
    REQUIRE(smaller.has_value());
    CHECK(smaller->width == 60);
    CHECK(smaller->height == 20);
    CHECK(smaller->bytes.size() == 20u * 61u);

    PlatformAsker& ask = t.platform;
    SUBCASE("on the bus: a copy is the copy sequence, a paste reads none, and a place offered "
            "back, a pump and the beat publish no place and no close") {
        const std::size_t before = t.term.writes.size();
        const std::int64_t shown_before = r.shown().showings;
        const std::size_t offered = ask.heard.offers.size(); // activation's and the resizes'
        ask.copy("stew");
        REQUIRE(t.term.writes.size() == before + 1);
        CHECK(t.term.writes.back() == gfx::window::terminal_clipboard_sequence("stew"));
        CHECK(r.shown().showings == shown_before);
        const std::uint64_t paste = ask.ask(gfx::ClipboardTextRequested{});
        REQUIRE(ask.heard.texts.size() == 1);
        CHECK(ask.heard.texts[0].first == paste);
        CHECK_FALSE(ask.heard.texts[0].second.readable);
        CHECK(ask.heard.texts[0].second.text.empty());

        (void)ask.ask(gfx::SurfacePlacementRemembered{10, 20, true});
        (void)ask.ask(gfx::PumpSurface{});
        t.pump();
        CHECK(ask.heard.placements.empty());
        CHECK(ask.heard.closes == 0);
        CHECK(ask.heard.offers.size() == offered); // the same size: nothing to offer again
        CHECK(t.term.writes.size() == before + 1);

        // A pump is the beat's work: at another size the window offers itself again.
        t.term.size = {70, 24};
        (void)ask.ask(gfx::PumpSurface{});
        REQUIRE(ask.heard.offers.size() == offered + 1);
        CHECK(ask.heard.offers.back().width == 70 * gfx::kCellPx);
        CHECK(ask.heard.offers.back().height == 24 * gfx::kCellPx);
        CHECK(ask.heard.offers.back().epoch == t.epoch);
        CHECK(ask.heard.placements.empty());
        CHECK(ask.heard.closes == 0);
    }
    SUBCASE("on the bus: a picture now, retained and fetched by chunk, and the chunk door's "
            "refusals") {
        const std::uint64_t early = ask.ask(gfx::SurfaceCaptureChunkRequested{1, 0});
        REQUIRE(ask.refused(early).has_value());
        CHECK(*ask.refused(early) == "no picture is retained; ask for a capture first");

        // Larger than one chunk: 300 x 120 cells, a newline each row.
        t.term.size = {300, 120};
        t.pump();
        const std::int64_t frame = r.shown().showings;
        const std::uint64_t now = ask.ask(gfx::SurfaceCaptureRequested{});
        const std::optional<gfx::SurfaceCaptured> one = ask.captured(now);
        REQUIRE(one.has_value());
        CHECK(one->ok);
        CHECK(one->capture == 1);
        CHECK(one->frame == frame);
        CHECK(one->format == "text/cells");
        CHECK(one->cell_px == 0);
        CHECK(one->width == 300);
        CHECK(one->height == 120);
        REQUIRE(one->bytes == 120 * 301);
        REQUIRE(one->bytes > gfx::kCaptureChunkBytes);
        std::size_t asks = 0;
        const std::string bytes = ask.fetch(one->capture, one->bytes, &asks);
        CHECK(asks == 2);
        CHECK(ask.heard.chunks.front().second.data.size() ==
              static_cast<std::size_t>(gfx::kCaptureChunkBytes));
        const auto direct = w.capture();
        REQUIRE(direct.has_value());
        CHECK(bytes == direct->bytes);
        const std::uint64_t at_end = ask.ask(gfx::SurfaceCaptureChunkRequested{1, one->bytes});
        REQUIRE(ask.heard.chunks.back().first == at_end);
        CHECK(ask.heard.chunks.back().second.data.empty());

        // A second picture replaces the first, and the first is gone.
        const std::uint64_t next = ask.ask(gfx::SurfaceCaptureRequested{});
        REQUIRE(ask.captured(next).has_value());
        CHECK(ask.captured(next)->capture == 2);
        const std::uint64_t gone = ask.ask(gfx::SurfaceCaptureChunkRequested{1, 0});
        REQUIRE(ask.refused(gone).has_value());
        CHECK(*ask.refused(gone) == "capture 1 is gone; the retained one is 2");
        const std::uint64_t past =
            ask.ask(gfx::SurfaceCaptureChunkRequested{2, one->bytes + 1});
        REQUIRE(ask.refused(past).has_value());
        CHECK(*ask.refused(past) == "offset " + std::to_string(one->bytes + 1) +
                                        " is outside a picture of " +
                                        std::to_string(one->bytes) + " bytes");
        const std::uint64_t before_start = ask.ask(gfx::SurfaceCaptureChunkRequested{2, -1});
        REQUIRE(ask.refused(before_start).has_value());
        CHECK(*ask.refused(before_start) ==
              "offset -1 is outside a picture of " + std::to_string(one->bytes) + " bytes");
    }
    SUBCASE("on the bus: a picture asked for after this showing waits through a drawing saying "
            "more, is answered by the showing that passes it, and one waits at a time") {
        const std::int64_t showings_now = r.shown().showings;
        const std::uint64_t later = ask.ask(gfx::SurfaceCaptureRequested{showings_now});
        CHECK(ask.heard.captures.empty());
        const std::uint64_t second = ask.ask(gfx::SurfaceCaptureRequested{showings_now});
        REQUIRE(ask.captured(second).has_value());
        CHECK_FALSE(ask.captured(second)->ok);
        CHECK(ask.captured(second)->refusal == "a capture is already waiting for frame " +
                                                   std::to_string(showings_now + 1) +
                                                   "; one waits at a time");
        t.pump(); // a beat that shows nothing passes no frame
        CHECK(!ask.captured(later).has_value());
        draw("later", true);
        CHECK(!ask.captured(later).has_value());
        draw("later", false);
        const std::optional<gfx::SurfaceCaptured> due = ask.captured(later);
        REQUIRE(due.has_value());
        CHECK(due->ok);
        CHECK(due->frame == showings_now + 1);
        CHECK(r.shown().showings == showings_now + 1);
        const std::string bytes = ask.fetch(due->capture, due->bytes);
        CHECK(bytes.substr(3 * 61, 8) == "  later ");
        // Answered once: a later showing answers nothing more.
        const std::size_t answered = ask.heard.captures.size();
        draw("again", false);
        CHECK(ask.heard.captures.size() == answered);
    }
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
    return ws::StandingCondition{key, compact, detail, zengine::graphics::role::kAlert, ""};
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
    CHECK(put_paths(drawn[0]) == Paths{"room", "words"});
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
    CHECK(put_paths(drawn[0]) == Paths{"room", "glance"});
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
    CHECK(put_paths(drawn[0]) == Paths{"room", "waiting"});
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

// ============================================================================
// Light -- the painter for light, its skins, and the OS window over a pixel backend
// ============================================================================

namespace {

namespace light = zengine::graphics::light;
namespace gw = zengine::graphics::window;

std::string sdl_office() { return std::string(gfx::kWindowOfficePrefix) + gfx::kSdlWindow; }

/// The primitives each node of a sketch paints for light, in the sketch's order.
std::vector<light::NodePrimitives> lit(const gfx::Sketch& s, const light::Look& look) {
    gfx::PaintTask task;
    std::int64_t rank = 1;
    for (const gfx::Node& n : s.nodes()) {
        task.nodes.push_back(gfx::PaintNode{n.path, {rank++}, n.op, n.args});
    }
    std::vector<light::NodePrimitives> out;
    for (const gfx::NodePaint& np : light::paint_task(task, look).nodes) {
        auto p = light::unpack_node(np.cells);
        REQUIRE(p.has_value());
        out.push_back(std::move(*p));
    }
    return out;
}

std::uint32_t ink_of(const light::Look& look, std::int64_t role) {
    return light::rgba_of(look.colour.at(role));
}

/// Every operator once, in one picture: a group holding the rest.
gfx::Sketch every_operator() {
    gfx::Sketch s;
    s.group("g", 10, 20);
    s.rect("g/rect", 0, 0, 40, 30, gfx::by_role(gfx::role::kAccent), gfx::by_role(gfx::role::kFill));
    s.ellipse("g/ellipse", 50, 0, 20, 10, gfx::exact(gfx::role::kFill, 0x123456),
              gfx::by_role(gfx::role::kMuted));
    s.line("g/line", 0, 40, 30, 40, gfx::by_role(gfx::role::kAlert));
    s.path("g/path", "M 0 50 L 20 50 L 20 60 L 0 60 Z", gfx::by_role(gfx::role::kGuide),
           gfx::by_role(gfx::role::kFill));
    s.text("g/text", 0, 70, "hi\xC3\xA9!", gfx::by_role(gfx::role::kFill));
    s.image("g/image", 0, 90, 4, 2, 2, 1, loom::Bytes{255, 0, 0, 255, 0, 255, 0, 255});
    return s;
}

} // namespace

TEST_CASE("the painter for light paints all seven operators into pixel primitives, each in its "
          "exact colour or its role's in the look") {
    const light::Look day = light::day_look();
    const auto nodes = lit(every_operator(), day);
    REQUIRE(nodes.size() == 6); // the group paints nothing of its own

    const light::NodePrimitives& rect = nodes[0];
    REQUIRE(rect.quads.size() == 5); // the fill, then four edges a pixel wide
    CHECK(rect.quads[0] == light::Quad{10, 20, 40, 30, ink_of(day, gfx::role::kAccent)});
    CHECK(rect.quads[1] == light::Quad{10, 20, 40, 1, ink_of(day, gfx::role::kFill)});
    CHECK(rect.quads[4] == light::Quad{49, 20, 1, 30, ink_of(day, gfx::role::kFill)});

    const light::NodePrimitives& ellipse = nodes[1];
    CHECK(ellipse.quads.size() == 10); // a span a pixel row
    for (const light::Quad& q : ellipse.quads) {
        CHECK(q.rgba == light::rgba_of(0x123456)); // the exact colour, not its role's
        CHECK(q.x >= 60);
        CHECK(q.x + q.w <= 80);
    }
    CHECK(ellipse.lines.size() == 48);
    CHECK(ellipse.lines[0].rgba == ink_of(day, gfx::role::kMuted));

    const light::NodePrimitives& line = nodes[2];
    REQUIRE(line.lines.size() == 1);
    CHECK(line.lines[0] == light::Line{10, 60, 40, 60, ink_of(day, gfx::role::kAlert)});

    const light::NodePrimitives& path = nodes[3];
    CHECK(path.quads.size() == 10); // the square's ten pixel rows
    CHECK(path.quads[0] == light::Quad{10, 70, 20, 1, ink_of(day, gfx::role::kGuide)});
    CHECK(path.lines.size() == 3); // between its points as flattened: a closing Z adds none

    const light::NodePrimitives& text = nodes[4];
    REQUIRE(text.texts.size() == 1);
    CHECK(text.texts[0] == light::TextRun{10, 90, ink_of(day, gfx::role::kFill), "hi?!"});

    const light::NodePrimitives& image = nodes[5];
    REQUIRE(image.images.size() == 1);
    CHECK(image.images[0].x == 10);
    CHECK(image.images[0].y == 110);
    CHECK(image.images[0].w == 4);
    CHECK(image.images[0].pw == 2);
    CHECK(image.images[0].rgba == loom::Bytes{255, 0, 0, 255, 0, 255, 0, 255});

    // An operator this medium does not know, named and muted.
    gfx::Sketch u;
    u.put("u", "sparkle", gfx::RectArgs{});
    const auto unknown = lit(u, day);
    REQUIRE(unknown[0].texts.size() == 1);
    CHECK(unknown[0].texts[0].text == "?sparkle");
    CHECK(unknown[0].texts[0].rgba == ink_of(day, gfx::role::kMuted));

    // The night look inks the same picture otherwise, and text in the medium's ground is fill.
    gfx::Sketch g;
    g.text("t", 0, 0, "x", gfx::by_role(gfx::role::kMediumGround));
    const light::Look night = light::night_look();
    CHECK(lit(g, night)[0].texts[0].rgba == ink_of(night, gfx::role::kFill));
    CHECK(ink_of(night, gfx::role::kFill) != ink_of(day, gfx::role::kFill));
}

TEST_CASE("a group's clip cuts what its children paint for light: quads, lines, text and images") {
    gfx::Sketch s;
    s.group("g", 10, 10, 20, 12); // the clip, (10, 10) to (30, 22)
    s.fill_rect("g/r", -5, -5, 100, 100, gfx::by_role(gfx::role::kFill));
    s.line("g/l", -10, 5, 50, 5, gfx::by_role(gfx::role::kFill));
    s.text("g/t", 0, 0, "abcdef", gfx::by_role(gfx::role::kFill));
    s.text("g/below", 0, 40, "gone", gfx::by_role(gfx::role::kFill));
    s.image("g/i", 10, 0, 20, 10, 2, 1, loom::Bytes{1, 1, 1, 255, 2, 2, 2, 255});
    const auto nodes = lit(s, light::day_look());
    REQUIRE(nodes.size() == 5);
    REQUIRE(nodes[0].quads.size() == 1);
    CHECK(nodes[0].quads[0].x == 10);
    CHECK(nodes[0].quads[0].y == 10);
    CHECK(nodes[0].quads[0].w == 20);
    CHECK(nodes[0].quads[0].h == 12);
    REQUIRE(nodes[1].lines.size() == 1);
    CHECK(nodes[1].lines[0].x1 == 10);
    CHECK(nodes[1].lines[0].x2 == 29);
    REQUIRE(nodes[2].texts.size() == 1);
    CHECK(nodes[2].texts[0].text == "a"); // one whole character, a cell, in twenty pixels
    CHECK(nodes[3].empty());               // a line whose top is outside the clip
    REQUIRE(nodes[4].images.size() == 1);
    CHECK(nodes[4].images[0].x == 20);
    CHECK(nodes[4].images[0].w == 10); // half the image shows
    CHECK(nodes[4].images[0].pw == 1); // and half its pixels go with it
    CHECK(nodes[4].images[0].rgba == loom::Bytes{1, 1, 1, 255});
}

TEST_CASE("a node painted for light packs and reads back whole, and bytes that are not one are "
          "refused") {
    light::NodePrimitives n;
    n.quads.push_back(light::Quad{-3, 4, 5, 6, 0x11223344u});
    n.lines.push_back(light::Line{1, 2, 3, 4, 0xaabbccddu});
    n.texts.push_back(light::TextRun{7, 8, 0x01020304u, "words"});
    n.images.push_back(light::ImageBlit{0, 0, 2, 2, 1, 1, loom::Bytes{9, 9, 9, 9}});
    const loom::Bytes packed = light::pack_node(n);
    CHECK(light::is_light(packed));
    const auto back = light::unpack_node(packed);
    REQUIRE(back.has_value());
    CHECK(*back == n);
    CHECK(light::unpack_node(light::pack_node(light::NodePrimitives{})).has_value());

    // Cut short, run on, untagged, or an image whose pixels are not pw * ph * 4 bytes: refused.
    CHECK_FALSE(light::unpack_node(loom::Bytes(packed.begin(), packed.end() - 1)).has_value());
    loom::Bytes longer = packed;
    longer.push_back(0);
    CHECK_FALSE(light::unpack_node(longer).has_value());
    loom::Bytes untagged = packed;
    untagged[0] = 'X';
    CHECK_FALSE(light::unpack_node(untagged).has_value());
    light::NodePrimitives bad;
    bad.images.push_back(light::ImageBlit{0, 0, 1, 1, 2, 2, loom::Bytes{1, 2, 3, 4}});
    CHECK_FALSE(light::unpack_node(light::pack_node(bad)).has_value());
    // A count larger than the bytes behind it is refused before anything is reserved for it.
    loom::Bytes huge(light::kTag.begin(), light::kTag.end());
    const loom::Bytes counts{0xff, 0xff, 0xff, 0x7f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    huge.insert(huge.end(), counts.begin(), counts.end());
    CHECK_FALSE(light::unpack_node(huge).has_value());

    // What a character painter writes is not light.
    gfx::Sketch s;
    s.fill_rect("r", 0, 0, 24, 12, gfx::by_role(gfx::role::kFill));
    gfx::PaintTask task;
    task.nodes.push_back(gfx::PaintNode{"r", {1}, s.nodes()[0].op, s.nodes()[0].args});
    const gfx::PaintResult cells = gfx::term::paint_task(task, gfx::term::Look{});
    REQUIRE(cells.nodes.size() == 1);
    CHECK_FALSE(light::is_light(cells.nodes[0].cells));
    CHECK_FALSE(light::unpack_node(cells.nodes[0].cells).has_value());
}

TEST_CASE("a point lands on a node painted for light where it painted, and within a pixel of a "
          "line") {
    light::NodePrimitives n;
    n.quads.push_back(light::Quad{10, 10, 5, 5, 0});
    n.texts.push_back(light::TextRun{40, 0, 0, "abc"});
    n.images.push_back(light::ImageBlit{0, 40, 8, 8, 1, 1, loom::Bytes{0, 0, 0, 0}});
    n.lines.push_back(light::Line{100, 0, 100, 50, 0});
    const gfx::TextMeasure face{8, 18};
    CHECK(light::hits_node(n, 10, 10, face));
    CHECK(light::hits_node(n, 14, 14, face));
    CHECK_FALSE(light::hits_node(n, 15, 15, face));
    CHECK(light::hits_node(n, 40 + 3 * 8 - 1, 18 - 1, face)); // the run's box, in its face
    CHECK_FALSE(light::hits_node(n, 40 + 3 * 8, 0, face));
    CHECK_FALSE(light::hits_node(n, 40, 18, face));
    CHECK(light::hits_node(n, 7, 47, face));
    CHECK(light::hits_node(n, 101, 25, face)); // a pixel from the line
    CHECK_FALSE(light::hits_node(n, 102, 25, face));
    CHECK_FALSE(light::hits_node(n, 100, 52, face)); // past its end
}

TEST_CASE("in light a run of text stands a character every advance of the window's face, as the "
          "task says it, and a line of that face tall") {
    gfx::Sketch s;
    s.group("g", 0, 0, 3 * 8, 18);
    s.text("g/row", 0, 0, "abcdef", gfx::by_role(gfx::role::kFill));
    s.text("mark", 40, 0, "xyz", gfx::by_role(gfx::role::kFill));
    gfx::PaintTask task;
    task.text_advance = 8;
    task.text_line = 18;
    std::int64_t rank = 1;
    for (const gfx::Node& n : s.nodes()) {
        task.nodes.push_back(gfx::PaintNode{n.path, {rank++}, n.op, n.args});
    }
    std::vector<light::NodePrimitives> nodes;
    for (const gfx::NodePaint& np : light::paint_task(task, light::day_look()).nodes) {
        auto p = light::unpack_node(np.cells);
        REQUIRE(p.has_value());
        nodes.push_back(std::move(*p)); // the group paints none
    }
    REQUIRE(nodes.size() == 2);
    // A CLIP THREE ADVANCES WIDE holds three characters of a run, not two of a cell's.
    REQUIRE(nodes[0].texts.size() == 1);
    CHECK(nodes[0].texts[0].text == "abc");
    // A RUN'S BOX is its characters' advances, a line tall.
    const gfx::TextMeasure face{8, 18};
    REQUIRE(nodes[1].texts.size() == 1);
    CHECK(light::hits_node(nodes[1], 40 + 3 * 8 - 1, 17, face));
    CHECK_FALSE(light::hits_node(nodes[1], 40 + 3 * 8, 0, face));
    // A TASK THAT SAYS NO MEASURE sets its text a cell a character.
    const std::vector<light::NodePrimitives> cells = lit(s, light::day_look());
    REQUIRE(cells.size() == 2);
    CHECK(cells[0].texts[0].text == "ab");
}

TEST_CASE("no source in the graphics package includes Surface") {
    std::size_t read = 0;
    std::vector<std::string> including;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(GRAPHICS_SOURCE_DIR)) {
        const std::string ext = entry.path().extension().string();
        if (!entry.is_regular_file() || (ext != ".hpp" && ext != ".cpp" && ext != ".h")) {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ++read;
        if (text.find("#include \"surface/") != std::string::npos) {
            including.push_back(entry.path().generic_string());
        }
    }
    CHECK(read > 20); // the population is the package's sources, not an empty folder
    CHECK(including.empty());
}

TEST_CASE("the terminal's ink and its pointer modes are the graphics package's") {
    CHECK(std::string(gw::sgr_for_role(static_cast<int>(gfx::role::kAccent))) == "\x1b[36m");
    CHECK(std::string(gw::sgr_for_role(static_cast<int>(gfx::role::kGuide))) == "\x1b[95m");
    CHECK(std::string(gw::sgr_for_role(static_cast<int>(gfx::role::kMediumGround))) == "\x1b[39m");
    CHECK(gw::tui_enter_sequence(true).find(gw::kTuiPointerOn) != std::string::npos);
}

TEST_CASE("a copy reaches a terminal as OSC 52: the text in standard base64 between the "
          "sequence's own bytes") {
    // RFC 4648's own vectors, both paddings among them.
    CHECK(gw::terminal_base64("") == "");
    CHECK(gw::terminal_base64("f") == "Zg==");
    CHECK(gw::terminal_base64("fo") == "Zm8=");
    CHECK(gw::terminal_base64("foo") == "Zm9v");
    CHECK(gw::terminal_base64("foob") == "Zm9vYg==");
    CHECK(gw::terminal_base64("fooba") == "Zm9vYmE=");
    CHECK(gw::terminal_base64("foobar") == "Zm9vYmFy");
    // Every byte is a byte, read unsigned: high ones and a zero, and the alphabet's last letters.
    CHECK(gw::terminal_base64(std::string("\xff\xfe\x00", 3)) == "//4A");
    CHECK(gw::terminal_base64(std::string("\xfb\xff", 2)) == "+/8=");
    CHECK(gw::terminal_clipboard_sequence("hi") == "\x1b]52;c;aGk=\x07");
    CHECK(gw::terminal_clipboard_sequence("") == "\x1b]52;c;\x07");
}

TEST_CASE("a window is put whole on the display it belongs to: as it was when it is, moved in only "
          "as far as it must, its top-left kept when it is larger, and not moved with no display") {
    const std::vector<gw::DesktopSpan> one = {{0, 0, 1000, 500}};
    CHECK(gw::placement_within(100, 100, 200, 100, one) == gw::DesktopPoint{100, 100});
    CHECK(gw::placement_within(800, 400, 200, 100, one) == gw::DesktopPoint{800, 400}); // edges
    CHECK(gw::placement_within(-700, 100, 200, 100, one) == gw::DesktopPoint{0, 100});
    CHECK(gw::placement_within(801, 100, 200, 100, one) == gw::DesktopPoint{800, 100});
    CHECK(gw::placement_within(100, -1, 200, 100, one) == gw::DesktopPoint{100, 0});
    CHECK(gw::placement_within(100, 450, 200, 100, one) == gw::DesktopPoint{100, 400});
    // Larger than the display: the strip a hand drags it by stays on the screen.
    CHECK(gw::placement_within(-4000, -4000, 2400, 1400, one) == gw::DesktopPoint{0, 0});
    CHECK(gw::placement_within(300, 200, 2400, 300, one) == gw::DesktopPoint{0, 200});

    // Two displays, one at negative x: the one a window overlaps most, else the nearest.
    const std::vector<gw::DesktopSpan> two = {{0, 0, 1920, 1040}, {-1920, 0, 1920, 1040}};
    CHECK(gw::placement_within(-1800, 50, 800, 600, two) == gw::DesktopPoint{-1800, 50});
    CHECK(gw::placement_within(-200, 50, 800, 600, two) == gw::DesktopPoint{0, 50});
    CHECK(gw::placement_within(-600, 50, 800, 600, two) == gw::DesktopPoint{-800, 50});
    CHECK(gw::placement_within(-5000, 50, 800, 600, two) == gw::DesktopPoint{-1920, 50});
    CHECK(gw::placement_within(5000, 50, 800, 600, two) == gw::DesktopPoint{1920 - 800, 50});
    CHECK(gw::placement_within(400, 1200, 800, 600, two) == gw::DesktopPoint{400, 1040 - 600});
    CHECK(gw::home_display(-700, 50, 800, 600, two).x == -1920);
    CHECK(gw::home_display(-100, 50, 800, 600, two).x == 0);
    CHECK(gw::moved_within(50, 10, 0, 100) == 50);
    CHECK(gw::moved_within(95, 10, 0, 100) == 90);
    CHECK(gw::moved_within(-5, 10, 0, 100) == 0);
    CHECK(gw::moved_within(5, 200, 0, 100) == 0);

    // With no display truth there is no answer: a remembered place is never replayed blind.
    CHECK_FALSE(gw::placement_within(100, 100, 800, 600, {}).has_value());
}

TEST_CASE("a window nobody placed is centred on its display, a larger one at its top-left, and a "
          "window grows only as far as its display holds it, frame and all, never smaller") {
    const gw::DesktopSpan primary{0, 0, 1920, 1040};
    CHECK(gw::centred_within(800, 600, primary) == gw::DesktopPoint{560, 220});
    CHECK(gw::centred_within(800, 600, gw::DesktopSpan{-1920, -1080, 1920, 1040}) ==
          gw::DesktopPoint{-1920 + 560, -1080 + 220});
    CHECK(gw::centred_within(800, 1400, primary) == gw::DesktopPoint{560, 0});
    CHECK(gw::centred_within(2400, 1400, primary) == gw::DesktopPoint{0, 0});

    const gw::DesktopSpan display{0, 0, 1920, 1032};
    CHECK(gw::grown_within({1200, 700}, {800, 600}, 16, 39, display) == gw::PixelSize{1200, 700});
    CHECK(gw::grown_within({2400, 1440}, {800, 600}, 16, 39, display) ==
          gw::PixelSize{1920 - 16, 1032 - 39});
    // A weaver's own larger window stays as large.
    CHECK(gw::grown_within({2400, 700}, {2200, 600}, 16, 39, display) == gw::PixelSize{2200, 700});
    CHECK(gw::grown_within({100, 100}, {800, 600}, 16, 39, display) == gw::PixelSize{800, 600});
}

TEST_CASE("a picture read back from light is a 24-bit bottom-up BMP: its header says its size, each "
          "row is padded to four bytes, and the last row comes first") {
    // Two rows of two pixels, blue, green, red each, a row every eight bytes as SDL's may be.
    const std::uint8_t px[] = {1, 2, 3, 4,  5,  6,  0xaa, 0xaa,
                               7, 8, 9, 10, 11, 12, 0xbb, 0xbb};
    const std::string bmp = gw::bmp_of_bgr24(2, 2, 8, px);
    const auto le = [&bmp](std::size_t at, int bytes) {
        std::uint64_t v = 0;
        for (int i = bytes - 1; i >= 0; --i) {
            v = (v << 8) | static_cast<unsigned char>(bmp[at + static_cast<std::size_t>(i)]);
        }
        return v;
    };
    REQUIRE(bmp.size() == 54 + 2 * 8);
    CHECK(bmp.substr(0, 2) == "BM");
    CHECK(le(2, 4) == bmp.size());
    CHECK(le(10, 4) == 54); // where the pixels start
    CHECK(le(14, 4) == 40);
    CHECK(le(18, 4) == 2);  // wide
    CHECK(le(22, 4) == 2);  // tall, and positive: bottom-up
    CHECK(le(26, 2) == 1);
    CHECK(le(28, 2) == 24);
    CHECK(le(30, 4) == 0); // no compression
    CHECK(le(34, 4) == 16);
    CHECK(bmp.substr(54, 8) == std::string("\x07\x08\x09\x0a\x0b\x0c\x00\x00", 8));
    CHECK(bmp.substr(62, 8) == std::string("\x01\x02\x03\x04\x05\x06\x00\x00", 8));

    // A row four bytes long already takes no padding, and one pixel's row takes one byte.
    const std::uint8_t four[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    CHECK(gw::bmp_of_bgr24(4, 1, 12, four).size() == 54 + 12);
    CHECK(gw::bmp_of_bgr24(1, 1, 3, four).substr(54) == std::string("\x01\x02\x03\x00", 4));
    // Nothing to draw is the header alone; a size the format cannot hold, or no pixels, is none.
    CHECK(gw::bmp_of_bgr24(0, 0, 0, nullptr).size() == 54);
    CHECK(gw::bmp_of_bgr24(-3, 5, 0, nullptr).size() == 54);
    CHECK(gw::bmp_of_bgr24(2, 2, 8, nullptr).empty());
    CHECK(gw::bmp_of_bgr24(INT32_MAX, INT32_MAX, 0, px).empty());
}

TEST_CASE("the day and night skins paint light across the painter seam, each in its own look, and "
          "a skin for characters does not") {
    gw::Painters painters;
    std::string why;
    const ZenginePainterV1* day = painters.get(PAINT_DAY_SO, why);
    REQUIRE_MESSAGE(day != nullptr, why);
    const ZenginePainterV1* night = painters.get(PAINT_NIGHT_SO, why);
    REQUIRE_MESSAGE(night != nullptr, why);
    CHECK(std::string(day->name) == "day");
    CHECK(std::string(night->name) == "night");

    gfx::Sketch s;
    s.fill_rect("r", 0, 0, 24, 12, gfx::by_role(gfx::role::kFill));
    s.text("t", 0, 0, "seam", gfx::exact(gfx::role::kFill, 0x0a0b0c));
    gfx::PaintTask task;
    std::int64_t rank = 1;
    for (const gfx::Node& n : s.nodes()) {
        task.nodes.push_back(gfx::PaintNode{n.path, {rank++}, n.op, n.args});
    }
    const auto by_day = painters.paint(day, task, why);
    const auto by_night = painters.paint(night, task, why);
    REQUIRE_MESSAGE(by_day.has_value(), why);
    REQUIRE_MESSAGE(by_night.has_value(), why);
    REQUIRE(by_day->nodes.size() == 2);
    const auto day_rect = light::unpack_node(by_day->nodes[0].cells);
    const auto night_rect = light::unpack_node(by_night->nodes[0].cells);
    REQUIRE(day_rect.has_value());
    REQUIRE(night_rect.has_value());
    CHECK(day_rect->quads[0].rgba == ink_of(light::day_look(), gfx::role::kFill));
    CHECK(night_rect->quads[0].rgba == ink_of(light::night_look(), gfx::role::kFill));
    const auto day_text = light::unpack_node(by_day->nodes[1].cells);
    REQUIRE(day_text.has_value());
    CHECK(day_text->texts[0].rgba == light::rgba_of(0x0a0b0c)); // exact in every look
    CHECK(painters.calls == 2);

    const ZenginePainterV1* classic = painters.get(PAINT_CLASSIC_SO, why);
    REQUIRE_MESSAGE(classic != nullptr, why);
    const auto by_classic = painters.paint(classic, task, why);
    REQUIRE(by_classic.has_value());
    CHECK_FALSE(light::is_light(by_classic->nodes[0].cells));
}

namespace {

using FakeSdlWindow = gw::SdlWindowT<FakeSdlBackend>;

/// What an owner in the OS window was told, as the window's office.
struct LitHeard {
    std::vector<gfx::AreaRoom> rooms;
    std::vector<std::string> refused;
    std::vector<std::string> said;
    std::vector<std::pair<gfx::AreaButtonChanged, std::uint64_t>> pressed;
    std::vector<std::pair<gfx::AreaPointerMoved, std::uint64_t>> moves;
    std::vector<gfx::AreaWheelTurned> wheels;
    std::vector<gfx::ActionRequested> actions;
    std::vector<gfx::AreaHoldEnded> ended;
};

class LitOwner
    : public loom::WeaveBase<
          LitOwner, GraphicsTestOwner,
          loom::Accept<gfx::AreaRoom, gfx::DrawnRefused, zengine::input::KeyPressed,
                       zengine::input::KeyReleased, zengine::input::TextEntered,
                       gfx::AreaButtonChanged, gfx::AreaPointerMoved, gfx::AreaWheelTurned,
                       gfx::AreaHoldEnded, gfx::ActionRequested>,
          loom::Emit<gfx::Drawn, gfx::ElementPut, gfx::ElementChange>> {
public:
    explicit LitOwner(LitHeard& heard) : heard_(&heard) {}
    void on(const gfx::AreaRoom& r, loom::Mail& mail) {
        if (mail.authored_from_role(sdl_office())) heard_->rooms.push_back(r);
    }
    void on(const gfx::DrawnRefused& r, loom::Mail& mail) {
        if (mail.authored_from_role(sdl_office())) heard_->refused.push_back(r.reason);
    }
    void on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        if (mail.authored_from_role(sdl_office())) heard_->said.push_back("key " + k.name);
    }
    void on(const zengine::input::KeyReleased& k, loom::Mail& mail) {
        if (mail.authored_from_role(sdl_office())) heard_->said.push_back("up " + k.name);
    }
    void on(const zengine::input::TextEntered& e, loom::Mail& mail) {
        if (mail.authored_from_role(sdl_office())) heard_->said.push_back("text " + e.text);
    }
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        if (mail.authored_from_role(sdl_office())) {
            heard_->pressed.emplace_back(b, mail.correlation());
        }
    }
    void on(const gfx::AreaPointerMoved& m, loom::Mail& mail) {
        if (mail.authored_from_role(sdl_office())) {
            heard_->moves.emplace_back(m, mail.correlation());
        }
    }
    void on(const gfx::AreaWheelTurned& w, loom::Mail& mail) {
        if (mail.authored_from_role(sdl_office())) heard_->wheels.push_back(w);
    }
    void on(const gfx::AreaHoldEnded& e, loom::Mail& mail) {
        if (mail.authored_from_role(sdl_office())) heard_->ended.push_back(e);
    }
    void on(const gfx::ActionRequested& a, loom::Mail& mail) {
        if (mail.authored_from_role(sdl_office())) heard_->actions.push_back(a);
    }

private:
    LitHeard* heard_;
};

/// The whole of a sketch, as one drawing of an area.
gfx::Drawn whole(const std::string& area, const gfx::Sketch& s) {
    return gfx::Drawn{area, true, s.nodes(), {}, s.records(), {}, false};
}

/// The OS window over the suite's backend, beside the directory, its skins offered: mounted in
/// process at its office and activated as Loom's control door activates.
struct LitWindow {
    Rig r;
    LightLog light;
    loom::WeaveId window;
    std::vector<std::unique_ptr<LitHeard>> heard;
    std::vector<std::pair<std::string, loom::WeaveId>> owners;

    FakeSdlWindow* instance = nullptr;

    PlatformAsker platform;

    /// `kept`, when given, is the state a window before this one left: this one revives it, as a
    /// reload's successor does, before it is activated. With `hear`, the suite's asker is mounted
    /// first and hears the window from its activation on, its backend's desktop place `at`.
    explicit LitWindow(const loom::Value* kept = nullptr, bool hear = false,
                       std::optional<gw::WindowPlacementValue> at = std::nullopt) {
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        light.placement = at;
        if (hear) {
            platform.mount(r, sdl_office());
        }
        auto w = std::make_unique<FakeSdlWindow>(FakeSdlBackend{&light});
        if (kept != nullptr) {
            w->revive(*kept);
        }
        FakeSdlWindow* raw = w.get();
        instance = raw;
        loom::Grant g = loom::emit_default_grant(*raw);
        window = r.bus.register_weave(std::move(w), std::move(g), sdl_office());
        raw->zen_set_self(window);
        const loom::WeaveId door = zengine::testing::mount_door(r.bus);
        zengine::testing::order_activation(r.bus, door, window, 1);
        r.to_role(gfx::kGraphicsRole, gfx::SkinOffered{"classic", PAINT_CLASSIC_SO});
        r.to_role(gfx::kGraphicsRole, gfx::SkinOffered{"day", PAINT_DAY_SO});
        r.to_role(gfx::kGraphicsRole, gfx::SkinOffered{"night", PAINT_NIGHT_SO});
        r.to_role(gfx::kGraphicsRole, gfx::ElementCodeOffered{"SnakeBoardData", SNAKE_SO_BOARD});
        r.drain();
    }

    /// An owner of an area of the window: placed there, its skin chosen. What it hears is kept.
    LitHeard& own(const std::string& office, std::int64_t x, std::int64_t y, std::int64_t w,
                  std::int64_t h, const std::string& skin, std::int64_t order = 0,
                  bool keys = false, bool pointer = false) {
        heard.push_back(std::make_unique<LitHeard>());
        auto made = std::make_unique<LitOwner>(*heard.back());
        LitOwner* raw = made.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        const loom::WeaveId id = r.bus.register_weave(std::move(made), std::move(g), office);
        raw->zen_set_self(id);
        owners.emplace_back(office, id);
        gfx::AreaPlaced placed{office, gfx::kSdlWindow, office, x, y, w, h, order, keys};
        placed.pointer = pointer;
        r.to_role(gfx::kGraphicsRole, placed);
        r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{office, skin});
        r.drain();
        return *heard.back();
    }

    /// The owner of `office` says `msg` to the window, as its office.
    template <class T>
    void say(const std::string& office, const T& msg) {
        for (const auto& [o, id] : owners) {
            if (o == office) {
                r.bus.office_send_to_role_as(id, office, sdl_office(),
                                             loom::Message(loom::to_value(msg)));
            }
        }
        r.drain();
    }

    /// The window's own beat, with what the suite queued for it to read.
    void pump() {
        r.bus.send(window, loom::Message(loom::to_value(
                               zengine::timer::TimerFired{gw::kPumpTimerId})));
        r.drain();
    }
    void press(std::int64_t x, std::int64_t y) {
        light.queued.events.emplace_back(zengine::input::PointerButton{
            1, true, x, y, zengine::input::space::kPixels, zengine::input::mod::kNone});
        pump();
    }
    /// A left press and its release at one pixel.
    void click(std::int64_t x, std::int64_t y) {
        for (const bool down : {true, false}) {
            light.queued.events.emplace_back(zengine::input::PointerButton{
                1, down, x, y, zengine::input::space::kPixels, zengine::input::mod::kNone});
        }
        pump();
    }

    gfx::WindowShown shown() {
        r.bus.send_as_to_role(r.ears, sdl_office(),
                              loom::Message(loom::to_value(gfx::WindowShowAsked{}), r.ears,
                                            r.ears, r.next_corr++));
        r.drain();
        return r.heard.shown.empty() ? gfx::WindowShown{} : r.heard.shown.back();
    }
};

bool any_row_has(const gfx::WindowShown& s, const std::string& words) {
    return std::any_of(s.rows.begin(), s.rows.end(),
                       [&](const std::string& row) { return row.find(words) != std::string::npos; });
}

} // namespace

TEST_CASE("the OS window paints every operator for light, holds a drawing saying more back, and "
          "shows the beat once") {
    LitWindow t;
    LitHeard& o = t.own("test.lit", 0, 0, 40 * gfx::kCellPx, 20 * gfx::kCellPx, "night");
    REQUIRE(o.rooms.size() == 1);
    CHECK(o.rooms[0].window == gfx::kSdlWindow);
    const std::size_t before = t.light.frames.size();
    gfx::Drawn first = whole("test.lit", every_operator());
    first.more = true;
    t.say("test.lit", first);
    CHECK(t.light.frames.size() == before); // held back: more of the beat follows
    gfx::Sketch tail;
    tail.text("tail", 0, 0, "tail", gfx::by_role(gfx::role::kFill));
    t.say("test.lit", gfx::Drawn{"test.lit", false, tail.nodes(), {}, {}, {}, false});
    REQUIRE(t.light.frames.size() == before + 1); // the beat, shown once
    CHECK(o.refused.empty());

    const light::Look night = light::night_look();
    CHECK(t.light.frames.back().ground == light::rgba_of(night.ground));
    const LightLog::Area* a = t.light.area("test.lit");
    REQUIRE(a != nullptr);
    REQUIRE(a->nodes.size() == 7); // every operator but the group, and the tail
    CHECK(a->paths[0] == "g/rect");
    CHECK(a->nodes[0].quads[0].rgba == ink_of(night, gfx::role::kAccent));
    CHECK(a->nodes[5].images.size() == 1);

    const gfx::WindowShown s = t.shown();
    CHECK(s.window == gfx::kSdlWindow);
    CHECK(s.nodes == 8);
    CHECK(s.rows.size() == static_cast<std::size_t>(gw::kDefaultHeight / t.light.face.line));
    CHECK(any_row_has(s, "hi?!"));
    CHECK(s.rows[0].rfind("tail", 0) == 0);
    CHECK(s.showings == static_cast<std::int64_t>(t.light.frames.size()));
    CHECK(s.device_bytes > 0);

    t.say("test.lit", gfx::Drawn{"test.nowhere", false, {}, {}, {}, {}, false});
    REQUIRE(o.refused.size() == 1);
    CHECK(o.refused[0] == "window 'sdl' shows no area 'test.nowhere'");
}

TEST_CASE("the OS window sets text in the face it measured: its owners are told it, a skin sets "
          "each run a character an advance, the window reads its text so, and a press lands on a "
          "run's characters") {
    LitWindow t;
    const gfx::TextMeasure face = t.light.face;
    REQUIRE_FALSE(face == gfx::TextMeasure{});
    LitHeard& o = t.own("test.day", 0, 0, 40 * face.advance, 4 * face.line, "day");
    REQUIRE_FALSE(o.rooms.empty());
    CHECK(o.rooms.back().cell_px == gfx::kCellPx);
    CHECK(o.rooms.back().text_advance_px == face.advance);
    CHECK(o.rooms.back().text_line_px == face.line);
    gfx::Sketch s;
    s.group("g", 0, face.line, 3 * face.advance, face.line);
    s.text("g/row", 0, 0, "abcdef", gfx::by_role(gfx::role::kFill));
    s.text("word", 5 * face.advance, 0, "go", gfx::by_role(gfx::role::kFill));
    s.action("word", "go.on");
    t.say("test.day", whole("test.day", s));
    REQUIRE(o.refused.empty());
    // THE DAY SKIN, painter code in its own library, set each run by the face the task carried: a
    // clip three advances wide keeps three characters.
    const LightLog::Area* a = t.light.area("test.day");
    REQUIRE(a != nullptr);
    std::vector<std::string> runs;
    for (const light::NodePrimitives& n : a->nodes) {
        for (const light::TextRun& r : n.texts) {
            runs.push_back(r.text);
        }
    }
    CHECK(runs == std::vector<std::string>{"abc", "go"});
    // READ AS TEXT a character an advance of the face and a row a line of it.
    const gfx::WindowShown shown = t.shown();
    REQUIRE(shown.rows.size() >= 2);
    CHECK(shown.rows[0] == "     go");
    CHECK(shown.rows[1] == "abc");
    // A PRESS ON THE RUN'S LAST CHARACTER is a press on the run; one past it is not.
    t.click(7 * face.advance - 1, face.line - 1);
    REQUIRE(o.actions.size() == 1);
    CHECK(o.actions[0].action == "go.on");
    t.click(7 * face.advance, face.line - 1);
    CHECK(o.actions.size() == 1);
}

TEST_CASE("the OS window paints an area whose skin paints characters in its own look, and one "
          "whose skin paints light with that skin") {
    LitWindow t;
    t.own("test.classic", 0, 0, 20 * gfx::kCellPx, 5 * gfx::kCellPx, "classic");
    t.own("test.day", 0, 6 * gfx::kCellPx, 20 * gfx::kCellPx, 5 * gfx::kCellPx, "day", 1);
    gfx::Sketch s;
    s.fill_rect("r", 0, 0, 24, 12, gfx::by_role(gfx::role::kFill));
    t.say("test.classic", whole("test.classic", s));
    t.say("test.day", whole("test.day", s));
    const LightLog::Area* classic = t.light.area("test.classic");
    const LightLog::Area* day = t.light.area("test.day");
    REQUIRE(classic != nullptr);
    REQUIRE(day != nullptr);
    REQUIRE(classic->nodes.size() == 1);
    REQUIRE(day->nodes.size() == 1);
    CHECK(classic->nodes[0].quads[0].rgba == ink_of(light::night_look(), gfx::role::kFill));
    CHECK(day->nodes[0].quads[0].rgba == ink_of(light::day_look(), gfx::role::kFill));
    // The ground is the back-most area's look's.
    CHECK(t.light.frames.back().ground == light::rgba_of(light::night_look().ground));

    // The skin for characters was asked once and is not asked again; the day skin paints each time.
    const gfx::WindowShown first = t.shown();
    CHECK(first.refusals == 0);
    s.fill_rect("r", 0, 0, 36, 12, gfx::by_role(gfx::role::kFill));
    t.say("test.classic", whole("test.classic", s));
    CHECK(t.shown().painter_calls == first.painter_calls);
    t.say("test.day", whole("test.day", s));
    CHECK(t.shown().painter_calls == first.painter_calls + 1);
    REQUIRE(t.light.area("test.classic")->nodes[0].quads.size() == 1);
    CHECK(t.light.area("test.classic")->nodes[0].quads[0].w == 36);
}

TEST_CASE("a terminal window refuses a skin that paints light, in words, and shows none of it") {
    TypedWindow t;
    OwnerHeard heard;
    auto o = std::make_unique<Owner>(heard);
    Owner* raw = o.get();
    loom::Grant g = loom::emit_default_grant(*raw);
    const loom::WeaveId owner = t.r.bus.register_weave(std::move(o), std::move(g), kOwner);
    raw->zen_set_self(owner);
    t.r.to_role(gfx::kGraphicsRole, gfx::SkinOffered{"day", PAINT_DAY_SO});
    t.r.to_role(gfx::kGraphicsRole, gfx::AreaPlaced{kOwner, gfx::kTerminalWindow, kOwner, 0, 0,
                                                    20 * gfx::kCellPx, 2 * gfx::kCellPx, 0, false});
    t.r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{kOwner, "day"});
    t.r.drain();
    gfx::Sketch s;
    s.text("t", 0, 0, "words", gfx::by_role(gfx::role::kFill));
    t.r.bus.office_send_to_role_as(owner, kOwner, window_office(),
                                   loom::Message(loom::to_value(whole(kOwner, s))));
    t.r.drain();
    const gfx::WindowShown shown = t.r.shown();
    CHECK(shown.last_refusal.find("paints light, which a terminal does not show") !=
          std::string::npos);
    for (const std::string& row : shown.rows) {
        CHECK(row.empty());
    }
}

TEST_CASE("a left press on the OS window lands on the top node painted at its pixel: in an area "
          "placed to hear the pointer the press itself, a part's or not, and in one placed without "
          "it a record's action, nothing drawn there taking it letting it through") {
    LitWindow t;
    LitHeard& back = t.own("test.back", 0, 0, 400, 200, "night", 0, false, /*pointer=*/true);
    LitHeard& front = t.own("test.front", 100, 50, 200, 50, "night", 1);
    gfx::Sketch b;
    b.fill_rect("plate", 0, 0, 400, 200, gfx::by_role(gfx::role::kGround));
    b.fill_rect("button", 10, 10, 50, 20, gfx::by_role(gfx::role::kAccent));
    b.action("button", "serve");
    t.say("test.back", whole("test.back", b));
    gfx::Sketch f;
    f.text("lid", 10, 10, "lid", gfx::by_role(gfx::role::kFill));
    f.fill_rect("knob", 150, 10, 20, 20, gfx::by_role(gfx::role::kAccent));
    f.action("knob", "turn");
    t.say("test.front", whole("test.front", f));

    t.click(20, 15); // the button: a part of an area that hears the pointer is pressed as itself
    CHECK(back.actions.empty());
    REQUIRE(back.pressed.size() == 2);
    CHECK(back.pressed[0].first.area == "test.back");
    CHECK(back.pressed[0].first.x == 20);
    CHECK(back.pressed[0].first.y == 15);
    CHECK(back.pressed[0].second != 0);
    CHECK(back.pressed[1].second == back.pressed[0].second);

    t.click(300, 150); // the plate
    REQUIRE(back.pressed.size() == 4);
    CHECK(back.pressed[2].first.x == 300);
    CHECK(back.pressed[2].first.y == 150);

    t.click(150, 90); // in the front area, where it painted nothing: what lies behind takes it
    REQUIRE(back.pressed.size() == 6);
    CHECK(back.pressed[4].first.x == 150);
    CHECK(back.pressed[4].first.y == 90);

    t.click(115, 65); // the lid names nothing, in an area deaf to the pointer: the back takes it
    REQUIRE(back.pressed.size() == 8);
    CHECK(back.pressed[6].first.x == 115);
    CHECK(front.pressed.empty());

    t.click(255, 65); // the knob names an action: its owner is asked it
    REQUIRE(front.actions.size() == 1);
    CHECK(front.actions[0].area == "test.front");
    CHECK(front.actions[0].action == "turn");
    CHECK(back.pressed.size() == 8);

    t.click(500, 300); // outside every area
    CHECK(back.pressed.size() == 8);

    // A press in cells lands at its cell's centre.
    t.light.queued.events.emplace_back(zengine::input::PointerButton{
        1, true, 1, 1, zengine::input::space::kCells, zengine::input::mod::kNone});
    t.pump();
    REQUIRE(back.pressed.size() == 9);
    CHECK(back.pressed[8].first.x == gfx::kCellPx + gfx::kCellPx / 2);
    CHECK(back.pressed[8].first.y == gfx::kCellPx + gfx::kCellPx / 2);
}

TEST_CASE("keys, their text and their releases go from the OS window to the owner of each area "
          "placed to hear them, and to no other") {
    LitWindow t;
    LitHeard& keys = t.own("test.keys", 0, 0, 100, 100, "night", 0, /*keys=*/true);
    LitHeard& deaf = t.own("test.deaf", 0, 100, 100, 100, "night", 1);
    t.light.queued.events.emplace_back(zengine::input::KeyPressed{
        zengine::input::scan::kQ, "Q", zengine::input::mod::kNone});
    t.light.queued.events.emplace_back(zengine::input::TextEntered{"q"});
    t.light.queued.events.emplace_back(zengine::input::KeyReleased{
        zengine::input::scan::kQ, "Q", zengine::input::mod::kNone});
    t.pump();
    CHECK(keys.said == std::vector<std::string>{"key Q", "text q", "up Q"});
    CHECK(deaf.said.empty());
}

TEST_CASE("the OS window gives a release, another button, a move and the wheel to the owner of "
          "the area they land in, at its own pixel, only when it was placed to hear the pointer, "
          "and a held press its moves and its release wherever they land") {
    namespace in = zengine::input;
    LitWindow t;
    LitHeard& back = t.own("test.back", 0, 0, 400, 200, "night", 0, false, /*pointer=*/true);
    LitHeard& front = t.own("test.front", 100, 50, 100, 50, "night", 1, false, /*pointer=*/true);
    LitHeard& deaf = t.own("test.deaf", 0, 300, 400, 100, "night", 2);
    auto& q = t.light.queued.events;
    q.emplace_back(in::PointerButton{1, false, 120, 70, in::space::kPixels, in::mod::kNone});
    q.emplace_back(in::PointerMoved{130, 80, 2, 3, in::space::kPixels, in::mod::kNone});
    q.emplace_back(in::PointerMoved{140, 85, 10, 5, in::space::kPixels, in::mod::kNone});
    q.emplace_back(in::PointerWheel{0.0, -1.5, 20, 30, in::space::kPixels, in::mod::kNone});
    q.emplace_back(in::PointerButton{2, true, 50, 350, in::space::kPixels, in::mod::kNone});
    q.emplace_back(in::PointerMoved{60, 360, 10, 10, in::space::kPixels, in::mod::kNone});
    q.emplace_back(in::PointerWheel{1.0, 0.0, 60, 360, in::space::kPixels, in::mod::kNone});
    q.emplace_back(in::PointerButton{2, false, 60, 360, in::space::kPixels, in::mod::kNone});
    q.emplace_back(in::PointerButton{1, false, 500, 500, in::space::kPixels, in::mod::kNone});
    t.pump();

    // The front area's: the release at its own pixel, no press holding it, and two moves read in
    // one beat as one.
    REQUIRE(front.pressed.size() == 1);
    CHECK(front.pressed[0].first.button == 1);
    CHECK_FALSE(front.pressed[0].first.pressed);
    CHECK(front.pressed[0].first.x == 20);
    CHECK(front.pressed[0].first.y == 20);
    CHECK(front.pressed[0].second == 0);
    REQUIRE(front.moves.size() == 1);
    CHECK(front.moves[0].first.x == 40);
    CHECK(front.moves[0].first.y == 35);
    CHECK(front.moves[0].first.dx == 12);
    CHECK(front.moves[0].first.dy == 8);
    CHECK(front.wheels.empty());
    // The back area's: the wheel.
    REQUIRE(back.wheels.size() == 1);
    CHECK(back.wheels[0].dy == doctest::Approx(-1.5));
    CHECK(back.wheels[0].x == 20);
    CHECK(back.wheels[0].y == 30);
    CHECK(back.pressed.empty());
    // Placed without the pointer, or outside every area: nothing.
    CHECK(deaf.pressed.empty());
    CHECK(deaf.moves.empty());
    CHECK(deaf.wheels.empty());

    // THE RIGHT BUTTON HELD IN THE BACK AREA: its moves go there wherever they land, in cells at
    // their cell's centre and in pixels, and so does its release, under the press's number.
    q.emplace_back(in::PointerButton{3, true, 10, 12, in::space::kPixels, in::mod::kNone});
    q.emplace_back(in::PointerMoved{150, 70, 1, 1, in::space::kPixels, in::mod::kNone});
    t.pump();
    q.emplace_back(in::PointerMoved{1, 1, 2, -1, in::space::kCells, in::mod::kNone});
    t.pump();
    q.emplace_back(in::PointerButton{3, false, 120, 350, in::space::kPixels, in::mod::kNone});
    t.pump();
    REQUIRE(back.pressed.size() == 2);
    CHECK(back.pressed[0].first.button == 3);
    CHECK(back.pressed[0].first.pressed);
    CHECK(back.pressed[0].first.x == 10);
    CHECK(back.pressed[0].first.y == 12);
    const std::uint64_t press = back.pressed[0].second;
    CHECK(press != 0);
    REQUIRE(back.moves.size() == 2);
    CHECK(back.moves[0].first.x == 150); // over the front area, held by the back's press
    CHECK(back.moves[0].second == press);
    CHECK(back.moves[1].first.x == gfx::kCellPx + gfx::kCellPx / 2);
    CHECK(back.moves[1].first.y == gfx::kCellPx + gfx::kCellPx / 2);
    CHECK(back.moves[1].first.dx == 2 * gfx::kCellPx);
    CHECK(back.moves[1].first.dy == -gfx::kCellPx);
    CHECK_FALSE(back.pressed[1].first.pressed);
    CHECK(back.pressed[1].first.x == 120);
    CHECK(back.pressed[1].first.y == 350);
    CHECK(back.pressed[1].second == press);
    CHECK(front.moves.size() == 1);
    CHECK(deaf.pressed.empty());
}

TEST_CASE("the OS window publishes a close its desktop asked for as its office, once for each ask, "
          "on the beat or a pump; a terminal is never asked") {
    LitWindow t(nullptr, true);
    PlatformAsker& ask = t.platform;
    t.pump();
    CHECK(ask.heard.closes == 0);
    t.light.queued.close = true;
    t.pump();
    CHECK(ask.heard.closes == 1);
    CHECK(t.r.count(0, gfx::SurfaceCloseRequested::zen_name, sdl_office()) == 1);
    t.pump(); // a beat that asks nothing publishes nothing
    CHECK(ask.heard.closes == 1);
    SUBCASE("a pump is the beat's work, a close included") {
        t.light.queued.close = true;
        (void)ask.ask(gfx::PumpSurface{});
        CHECK(ask.heard.closes == 2);
        (void)ask.ask(gfx::PumpSurface{});
        CHECK(ask.heard.closes == 2);
    }
    SUBCASE("a second ask is a second close") {
        t.light.queued.close = true;
        t.pump();
        CHECK(ask.heard.closes == 2);
    }

    gw::TerminalMedium<FakeTerminal> terminal;
    CHECK_FALSE(terminal.close_requested());
    CHECK_FALSE(terminal.take_close());
}

TEST_CASE("the OS window resized offers itself again in its new size in pixels, under the same "
          "incarnation, and draws its picture again at any new size") {
    LitWindow t;
    t.own("test.lit", 0, 0, 100, 100, "night");
    const auto offers = t.r.read<gfx::WindowOffered>(0, sdl_office());
    REQUIRE_FALSE(offers.empty());
    CHECK(offers.back().width == gw::kDefaultWidth);
    CHECK(offers.back().height == gw::kDefaultHeight);
    const std::int64_t epoch = offers.back().epoch;

    std::size_t mark = t.r.log.size();
    std::size_t frames = t.light.frames.size();
    t.light.size = gw::PixelSize{gw::kDefaultWidth + 5, gw::kDefaultHeight + 5}; // in a cell
    t.pump();
    const auto pixels = t.r.read<gfx::WindowOffered>(mark, sdl_office());
    REQUIRE(pixels.size() == 1); // every pixel is its room, none floored to a cell
    CHECK(pixels[0].width == gw::kDefaultWidth + 5);
    CHECK(pixels[0].height == gw::kDefaultHeight + 5);
    CHECK(t.light.frames.size() == frames + 1);

    mark = t.r.log.size();
    frames = t.light.frames.size();
    t.light.size = gw::PixelSize{1200, 600};
    t.pump();
    const auto again = t.r.read<gfx::WindowOffered>(mark, sdl_office());
    REQUIRE(again.size() == 1);
    CHECK(again[0].window == gfx::kSdlWindow);
    CHECK(again[0].width == 1200);
    CHECK(again[0].height == 600);
    CHECK(again[0].epoch == epoch);
    CHECK(again[0].cell_px == gfx::kCellPx); // it shows every pixel...
    CHECK(again[0].text_advance_px == t.light.face.advance); // ...and sets text in its face
    CHECK(again[0].text_line_px == t.light.face.line);
    CHECK(t.light.frames.size() > frames);
    CHECK(t.light.frames.back().width == 1200);
    // READ AS TEXT, a row a line of its face.
    CHECK(t.shown().rows.size() == static_cast<std::size_t>(600 / t.light.face.line));

    mark = t.r.log.size();
    t.pump(); // nothing changed: nothing offered, nothing drawn
    CHECK(t.r.read<gfx::WindowOffered>(mark, sdl_office()).empty());
}

TEST_CASE("an element said in the OS window is drawn by its code, and a change of it is painted "
          "again") {
    LitWindow t;
    LitHeard& o = t.own("test.board", 0, 0, 30 * gfx::kCellPx, 10 * gfx::kCellPx, "night");
    t.say("test.board",
          gfx::ElementPut{"test.board", "grid", "SnakeBoardData", 1,
                          loom::bequeath_item(snake::SnakeBoardData{3, 1, gfx::kCellPx, {0}, -1}),
                          false});
    CHECK(o.refused.empty());
    const LightLog::Area* a = t.light.area("test.board");
    REQUIRE(a != nullptr);
    REQUIRE_FALSE(a->nodes.empty());
    const std::vector<light::NodePrimitives> before = a->nodes;
    CHECK(t.shown().elements == 1);

    t.say("test.board",
          gfx::ElementChange{1, 0, gfx::body_of(snake::SnakeStep{1, 0, -1, false}), false});
    CHECK(o.refused.empty());
    CHECK(t.light.area("test.board")->nodes != before);
}

TEST_CASE("the OS window started again from what its last instance kept shows it, under the same "
          "incarnation, and nothing is drawn again") {
    std::optional<loom::Value> kept;
    gfx::WindowShown before;
    {
        LitWindow t;
        t.own("test.lit", 0, 0, 40 * gfx::kCellPx, 4 * gfx::kCellPx, "day");
        gfx::Sketch s;
        s.text("t", 0, gfx::kCellPx, "kept", gfx::by_role(gfx::role::kFill));
        t.say("test.lit", whole("test.lit", s));
        before = t.shown();
        kept = t.instance->snapshot();
    }
    REQUIRE(kept.has_value());
    LitWindow again(&*kept); // a host with no owner of the area at all
    const gfx::WindowShown after = again.shown();
    CHECK(after.epoch == before.epoch);
    CHECK(after.rows == before.rows);
    CHECK(any_row_has(after, "kept"));
    const LightLog::Area* a = again.light.area("test.lit");
    REQUIRE(a != nullptr);
    REQUIRE(a->nodes.size() == 1);
    CHECK(a->nodes[0].texts[0].rgba == ink_of(light::day_look(), gfx::role::kFill));
    CHECK(again.r.count(0, "Drawn") == 0);
    CHECK(again.light.claims == 1);
}

TEST_CASE("the OS window's page answers for every shape it accepts, and names no other") {
    LitWindow t;
    const std::vector<std::string> described = described_shapes_of(t.r.bus, t.window);
    REQUIRE_FALSE(described.empty());
    CHECK(check_office_manual(t.r.bus, sdl_office()) >= described.size());
    check_page_names_accepted(zengine::manual::pages::zengine_window_sdl::kManual, described);
}

TEST_CASE("the OS window has its backend read back the last picture it presented: none before one, "
          "and never a drawing held back since, nor without a node removed since") {
    // Nothing presented, nothing to read back; with no OS window, nothing is presented.
    LightLog quiet;
    quiet.drawing = false;
    gw::LightMedium<FakeSdlBackend> medium(FakeSdlBackend{&quiet});
    CHECK_FALSE(medium.capture().has_value());
    medium.claim();
    medium.show({});
    CHECK(medium.showings() == 0);
    CHECK_FALSE(medium.capture().has_value());
    CHECK(quiet.captured.empty());
    quiet.drawing = true;
    medium.clear();
    medium.show({});
    REQUIRE(medium.showings() == 1);
    REQUIRE(medium.capture().has_value());
    CHECK(quiet.captured.back() == quiet.frames.back());

    LitWindow t;
    t.own("test.lit", 0, 0, 400, 200, "night");
    gfx::Sketch one;
    one.text("t", 0, 0, "one", gfx::by_role(gfx::role::kFill));
    one.fill_rect("gone", 0, 24, 48, 12, gfx::by_role(gfx::role::kAccent));
    t.say("test.lit", whole("test.lit", one));
    const LightLog::Frame presented = t.light.frames.back();
    REQUIRE(t.light.area("test.lit") != nullptr);
    REQUIRE(t.light.area("test.lit")->nodes.size() == 2);
    const std::size_t frames = t.light.frames.size();

    // Held back: its text changed and a node removed, with more of the beat to follow.
    gfx::Sketch two;
    two.text("t", 0, 0, "two", gfx::by_role(gfx::role::kFill));
    t.say("test.lit", gfx::Drawn{"test.lit", false, two.nodes(), {"gone"}, {}, {}, true});
    REQUIRE(t.light.frames.size() == frames);

    // What is read back is the picture presented, whole: its text, and the node gone since.
    const auto first = t.instance->capture();
    REQUIRE(first.has_value());
    REQUIRE(t.light.captured.size() == 1);
    CHECK(t.light.captured[0] == presented);
    CHECK(*first == LightLog::picture_of(presented.ground));

    // Presented, the picture read back is the one shown now.
    t.say("test.lit", gfx::Drawn{"test.lit", false, two.nodes(), {}, {}, {}, false});
    REQUIRE(t.light.frames.size() == frames + 1);
    REQUIRE(t.instance->capture().has_value());
    CHECK(t.light.captured.back() == t.light.frames.back());
    REQUIRE(t.light.captured.back().areas.size() == 1);
    CHECK(t.light.captured.back().areas[0].paths == std::vector<std::string>{"t"});

    // A backend that cannot read its picture back answers none, and so does the window.
    t.light.reading = false;
    CHECK_FALSE(t.instance->capture().has_value());

    SUBCASE("on the bus: the picture now and by chunk, a picture asked for after this one answered "
            "by the next presented, and none when the backend cannot read back") {
        LitWindow b(nullptr, true);
        PlatformAsker& ask = b.platform;
        b.own("test.lit", 0, 0, 400, 200, "night");
        b.say("test.lit", whole("test.lit", one));
        const std::uint64_t now = ask.ask(gfx::SurfaceCaptureRequested{});
        const std::optional<gfx::SurfaceCaptured> got = ask.captured(now);
        REQUIRE(got.has_value());
        CHECK(got->ok);
        CHECK(got->capture == 1);
        CHECK(got->frame == b.shown().showings);
        CHECK(got->format == "image/bmp");
        CHECK(got->cell_px == gfx::kCellPx);
        CHECK(got->width == 1);
        CHECK(got->height == 1);
        const gw::CapturedWindowPicture expected =
            LightLog::picture_of(b.light.frames.back().ground);
        CHECK(got->bytes == static_cast<std::int64_t>(expected.bytes.size()));
        CHECK(ask.fetch(got->capture, got->bytes) == expected.bytes);

        const std::int64_t shown = b.shown().showings;
        const std::uint64_t later = ask.ask(gfx::SurfaceCaptureRequested{shown});
        b.pump(); // a beat that presents nothing passes no frame
        CHECK(!ask.captured(later).has_value());
        b.say("test.lit", gfx::Drawn{"test.lit", false, two.nodes(), {}, {}, {}, true});
        CHECK(!ask.captured(later).has_value());
        b.say("test.lit", gfx::Drawn{"test.lit", false, two.nodes(), {"gone"}, {}, {}, false});
        const std::optional<gfx::SurfaceCaptured> due = ask.captured(later);
        REQUIRE(due.has_value());
        CHECK(due->ok);
        CHECK(due->capture == 2);
        CHECK(due->frame == shown + 1);
        CHECK(b.light.captured.back() == b.light.frames.back());

        b.light.reading = false;
        const std::uint64_t unread = ask.ask(gfx::SurfaceCaptureRequested{});
        REQUIRE(ask.captured(unread).has_value());
        CHECK_FALSE(ask.captured(unread)->ok);
        CHECK(ask.captured(unread)->refusal == "this medium has no picture to read back right now");
        const std::uint64_t kept = ask.ask(gfx::SurfaceCaptureChunkRequested{2, 0});
        CHECK(!ask.refused(kept).has_value()); // a refused capture leaves the retained one
    }
}

TEST_CASE("the OS window's clipboard and its place on the desktop are its backend's: a copy handed "
          "on, the clipboard read only when asked, its placement read, and a remembered place") {
    LitWindow t;
    t.own("test.lit", 0, 0, 400, 200, "night");
    FakeSdlWindow& w = *t.instance;
    w.clipboard_copy("soup");
    CHECK(t.light.copies == std::vector<std::string>{"soup"});
    t.pump();
    CHECK(t.light.reads == 0); // the beat reads no clipboard
    CHECK_FALSE(w.clipboard_text().has_value());
    t.light.clipboard = "stock";
    CHECK(w.clipboard_text() == std::optional<std::string>("stock"));
    t.light.clipboard = "";
    CHECK(w.clipboard_text() == std::optional<std::string>(""));
    CHECK(t.light.reads == 3);

    CHECK_FALSE(w.placement().has_value());
    t.light.placement = gw::WindowPlacementValue{-1200, 40, true};
    CHECK(w.placement() == t.light.placement);
    w.place(gw::WindowPlacementValue{300, 200, false});
    REQUIRE(t.light.placed.size() == 1);
    CHECK(t.light.placed[0] == gw::WindowPlacementValue{300, 200, false});

    SUBCASE("on the bus: a copy handed on, and a paste answered from the clipboard read then") {
        LitWindow b(nullptr, true);
        PlatformAsker& ask = b.platform;
        ask.copy("broth");
        CHECK(b.light.copies == std::vector<std::string>{"broth"});
        const std::uint64_t none = ask.ask(gfx::ClipboardTextRequested{});
        b.light.clipboard = "stock";
        const std::uint64_t some = ask.ask(gfx::ClipboardTextRequested{});
        b.light.clipboard = "";
        const std::uint64_t empty = ask.ask(gfx::ClipboardTextRequested{});
        REQUIRE(ask.heard.texts.size() == 3);
        CHECK(ask.heard.texts[0].first == none);
        CHECK_FALSE(ask.heard.texts[0].second.readable);
        CHECK(ask.heard.texts[1].first == some);
        CHECK(ask.heard.texts[1].second.readable);
        CHECK(ask.heard.texts[1].second.text == "stock");
        CHECK(ask.heard.texts[2].first == empty);
        CHECK(ask.heard.texts[2].second.readable);
        CHECK(ask.heard.texts[2].second.text.empty());
        CHECK(b.light.reads == 3);
    }
    SUBCASE("on the bus: its place said on activation before its offer, again only when it moved, "
            "before the offer when its size moved too, and after a place offered back") {
        LitWindow b(nullptr, true, gw::WindowPlacementValue{40, 30, false});
        PlatformAsker& ask = b.platform;
        REQUIRE(ask.heard.placements.size() == 1);
        CHECK(placed_at(ask.heard.placements[0], 40, 30, false));
        REQUIRE(ask.heard.order.size() >= 2);
        CHECK(ask.heard.order[0] == "SurfacePlacement");
        CHECK(ask.heard.order[1] == "WindowOffered");
        CHECK(b.r.count(0, gfx::SurfacePlacement::zen_name, sdl_office()) == 1);

        b.pump();
        (void)ask.ask(gfx::PumpSurface{});
        CHECK(ask.heard.placements.size() == 1); // where it was: nothing said

        // Dragged and resized at once: its place first, then its room.
        b.light.placement = gw::WindowPlacementValue{90, 70, false};
        b.light.size = gw::PixelSize{1200, 600};
        const std::size_t from = ask.heard.order.size();
        b.pump();
        REQUIRE(ask.heard.order.size() == from + 2);
        CHECK(ask.heard.order[from] == "SurfacePlacement");
        CHECK(ask.heard.order[from + 1] == "WindowOffered");
        CHECK(placed_at(ask.heard.placements.back(), 90, 70, false));
        CHECK(ask.heard.offers.back().width == 1200);

        // A place offered back is the backend's to take; where the window is after is said.
        (void)ask.ask(gfx::SurfacePlacementRemembered{90, 70, false});
        REQUIRE(b.light.placed.size() == 1);
        CHECK(b.light.placed[0] == gw::WindowPlacementValue{90, 70, false});
        CHECK(ask.heard.placements.size() == 2); // it sits where it sat: nothing new
        b.light.placement = gw::WindowPlacementValue{300, 200, true};
        (void)ask.ask(gfx::SurfacePlacementRemembered{300, 200, true});
        REQUIRE(ask.heard.placements.size() == 3);
        CHECK(placed_at(ask.heard.placements.back(), 300, 200, true));
        CHECK(ask.heard.closes == 0);
    }
}

TEST_CASE("an OS window a weaver made narrower and a pixel taller is not grown back while its "
          "areas are placed again for it one at a time") {
    LitWindow t;
    t.own("test.desk", 0, 0, gw::kDefaultWidth, gw::kDefaultHeight, "night");
    t.own("test.over", 0, 0, gw::kDefaultWidth, gw::kDefaultHeight, "night", 1);
    REQUIRE(t.light.grown.empty());
    t.light.size = gw::PixelSize{gw::kDefaultWidth - 156, gw::kDefaultHeight + 1};
    t.pump();
    // THE FIRST AREA PLACED FOR THE NEW ROOM reaches a pixel further down and less far across;
    // the other still reaches across the old room, no further than it did.
    for (const char* area : {"test.desk", "test.over"}) {
        t.r.to_role(gfx::kGraphicsRole,
                    gfx::AreaPlaced{area, gfx::kSdlWindow, area, 0, 0, gw::kDefaultWidth - 156,
                                    gw::kDefaultHeight + 1, 0, false});
        t.r.drain();
        CHECK(t.light.grown.empty());
    }
}

TEST_CASE("the OS window asks its backend to grow once for each new reach of its areas past it, and "
          "maximizes a remembered maximize on the beat after its next picture, once") {
    LitWindow t;
    t.own("test.small", 0, 0, 400, 200, "night");
    CHECK(t.light.grown.empty()); // within the window: nothing asked
    t.own("test.wide", 100, 50, 1900, 950, "night", 1); // to 2000 x 1000, past 1600 x 900
    REQUIRE(t.light.grown.size() == 1);
    CHECK(t.light.grown[0] == gw::PixelSize{2000, 1000});
    gfx::Sketch s;
    s.fill_rect("r", 0, 0, 24, 12, gfx::by_role(gfx::role::kFill));
    t.say("test.wide", whole("test.wide", s));
    t.pump();
    CHECK(t.light.grown.size() == 1); // more pictures, the same reach: not asked again
    // A window a weaver made smaller is not grown back while its areas stay where they are...
    t.light.size = gw::PixelSize{1200, 700};
    t.pump();
    CHECK(t.light.grown.size() == 1);
    // ...and an area placed further out asks again, along the axis it reaches further on: the
    // width the weaver chose stays.
    t.own("test.tall", 0, 0, 100, 1200, "night", 2);
    REQUIRE(t.light.grown.size() == 2);
    CHECK(t.light.grown[1] == gw::PixelSize{1200, 1200});

    // A remembered maximize waits for the next picture, and lands on the beat after it, once.
    t.instance->place(gw::WindowPlacementValue{50, 60, true});
    CHECK(t.light.placed.back() == gw::WindowPlacementValue{50, 60, true});
    t.pump();
    CHECK(t.light.maximized == 0); // no picture yet
    t.say("test.small", whole("test.small", s));
    CHECK(t.light.maximized == 0); // not on the picture's own beat
    t.pump();
    CHECK(t.light.maximized == 1);
    t.pump();
    CHECK(t.light.maximized == 1);
    // A later want, not maximized, replaces one still waiting.
    t.instance->place(gw::WindowPlacementValue{50, 60, true});
    t.instance->place(gw::WindowPlacementValue{70, 80, false});
    t.say("test.small", whole("test.small", s));
    t.pump();
    CHECK(t.light.maximized == 1);
}

#if defined(GRAPHICS_HAS_SDL)

namespace {

/// The OS windows SDL holds in this process, by id.
std::vector<SDL_WindowID> os_windows() {
    std::vector<SDL_WindowID> out;
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        return out;
    }
    int n = 0;
    SDL_Window** windows = SDL_GetWindows(&n);
    for (int i = 0; i < n; ++i) {
        out.push_back(SDL_GetWindowID(windows[i]));
    }
    SDL_free(windows);
    return out;
}

void sdl_video_driver(const char* name) {
#if defined(_WIN32)
    ::_putenv_s("SDL_VIDEO_DRIVER", name);
    ::_putenv_s("SDL_VIDEODRIVER", name);
#else
    ::setenv("SDL_VIDEO_DRIVER", name, 1);
    ::setenv("SDL_VIDEODRIVER", name, 1);
#endif
}

/// The OS window's own weave, loaded through the kernel beside the directory, an owner drawing in
/// it with the day skin.
struct SdlWeave {
    Rig r;
    LitHeard heard;
    loom::WeaveId window;
    loom::WeaveId owner;
    explicit SdlWeave(bool keys = false, bool pointer = false) {
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        window = r.load("zengine-window-sdl", WINDOW_SDL_SO, sdl_office());
        REQUIRE(window.valid());
        auto made = std::make_unique<LitOwner>(heard);
        LitOwner* raw = made.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        owner = r.bus.register_weave(std::move(made), std::move(g), "test.lit");
        raw->zen_set_self(owner);
        r.to_role(gfx::kGraphicsRole, gfx::SkinOffered{"day", PAINT_DAY_SO});
        gfx::AreaPlaced placed{"test.lit", gfx::kSdlWindow, "test.lit", 0,
                               0, 40 * gfx::kCellPx, 4 * gfx::kCellPx, 0, keys};
        placed.pointer = pointer;
        r.to_role(gfx::kGraphicsRole, placed);
        r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{"test.lit", "day"});
        r.drain();
        gfx::Sketch s;
        s.fill_rect("plate", 0, 0, 200, 40, gfx::by_role(gfx::role::kGround));
        s.text("t", 0, gfx::kCellPx, "lit", gfx::by_role(gfx::role::kFill));
        r.bus.office_send_to_role_as(owner, "test.lit", sdl_office(),
                                     loom::Message(loom::to_value(whole("test.lit", s))));
        r.drain();
    }
    gfx::WindowShown shown() {
        r.bus.send_as_to_role(r.ears, sdl_office(),
                              loom::Message(loom::to_value(gfx::WindowShowAsked{}), r.ears,
                                            r.ears, r.next_corr++));
        r.drain();
        return r.heard.shown.empty() ? gfx::WindowShown{} : r.heard.shown.back();
    }
    /// The window's own beat: it empties SDL's queue.
    void pump() {
        r.bus.send(window, loom::Message(loom::to_value(
                               zengine::timer::TimerFired{gw::kPumpTimerId})));
        r.drain();
    }
};

/// How many events SDL's queue holds.
int queued_events() {
    return SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
}

} // namespace

TEST_CASE("the OS window opens one window on the desktop, draws in it, keeps that one window "
          "across a reload, holds one again once replaced, and closes it when it goes") {
    REQUIRE(os_windows().empty());
    {
        SdlWeave w;
        const gfx::WindowShown first = w.shown();
        CHECK(first.window == gfx::kSdlWindow);
        CHECK(any_row_has(first, "lit"));
        CHECK(first.showings > 0);
        CHECK(first.painter_calls > 0);
        CHECK(first.refusals == 0);
        const std::vector<SDL_WindowID> opened = os_windows();
        REQUIRE(opened.size() == 1);

        const auto reloaded = w.r.command(loom::ReloadWeave{"zengine-window-sdl", WINDOW_SDL_SO});
        REQUIRE_MESSAGE(reloaded.kind == 1, reloaded.text);
        const gfx::WindowShown after = w.shown();
        CHECK(after.epoch == first.epoch);
        CHECK(after.rows == first.rows);
        CHECK(os_windows() == opened); // the same window, handed over, not made again

        const auto swapped = w.r.command(loom::SwapWeave{sdl_office(), "zengine-window-sdl",
                                                         WINDOW_SDL_SO, /*graceful=*/false});
        REQUIRE_MESSAGE(swapped.kind == 0, swapped.text);
        CHECK(w.shown().epoch != first.epoch);
        // Replaced, the window goes before its successor comes: it closes its OS window, and the
        // successor opens one of its own -- one window, never two.
        CHECK(os_windows().size() == 1);
    }
    CHECK(os_windows().empty());
    CHECK(SDL_WasInit(SDL_INIT_VIDEO) == 0); // and SDL is given back whole
}

TEST_CASE("the OS window with no display to open on keeps and composes its picture, and holds no "
          "SDL and no claim on SDL's queue") {
    sdl_video_driver("zengine-no-such-driver");
    {
        SdlWeave w;
        const gfx::WindowShown s = w.shown();
        CHECK(s.window == gfx::kSdlWindow);
        CHECK(any_row_has(s, "lit"));
        // ITS FACE MEASURED with no window to draw in: the embedded face's, not SDL's debug face.
        REQUIRE_FALSE(w.heard.rooms.empty());
        const gfx::TextMeasure face = w.heard.rooms.back().measure();
        CHECK(face.advance > 0);
        CHECK_FALSE(face == gw::kDebugFace);
        CHECK(s.rows.size() == static_cast<std::size_t>(gw::kDefaultHeight / face.line));
        CHECK(s.showings == 0); // nothing was drawn, so nothing was shown
        CHECK(os_windows().empty());
        CHECK(SDL_WasInit(SDL_INIT_VIDEO) == 0);
        // With no window to read for, the queue is left to an SDL Input reader beside it.
        CHECK_FALSE(zengine::input::window_holds_sdl_input());
    }
    sdl_video_driver("dummy");
}

TEST_CASE("the OS window claims SDL's queue while it is loaded, reloaded or replaced, and gives "
          "the claim back when it goes") {
    namespace in = zengine::input;
    sdl_video_driver("dummy"); // a real SDL with no display, run inside the lane or out of it
    REQUIRE_FALSE(in::window_holds_sdl_input());
    {
        SdlWeave w;
        CHECK(in::window_sdl_input_claims() == 1);

        // A reload makes the successor before it destroys its predecessor: one claim, not none.
        const auto reloaded = w.r.command(loom::ReloadWeave{"zengine-window-sdl", WINDOW_SDL_SO});
        REQUIRE_MESSAGE(reloaded.kind == 1, reloaded.text);
        CHECK(in::window_sdl_input_claims() == 1);

        const auto swapped = w.r.command(loom::SwapWeave{sdl_office(), "zengine-window-sdl",
                                                         WINDOW_SDL_SO, /*graceful=*/false});
        REQUIRE_MESSAGE(swapped.kind == 0, swapped.text);
        CHECK(in::window_sdl_input_claims() == 1);
    }
    CHECK_FALSE(in::window_holds_sdl_input());
}

TEST_CASE("the OS window empties SDL's own queue: a key's release, a move, another button and the "
          "wheel reach the owner placed to hear them, and a close request is taken off the queue") {
    sdl_video_driver("dummy"); // a real SDL with no display, run inside the lane or out of it
    SdlWeave w(/*keys=*/true, /*pointer=*/true);
    w.pump(); // what the window's own opening queued
    const auto push = [](SDL_Event e) { REQUIRE(SDL_PushEvent(&e)); };
    SDL_Event up{};
    up.type = SDL_EVENT_KEY_UP;
    up.key.scancode = SDL_SCANCODE_Q;
    up.key.down = false;
    push(up);
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.x = 30.0f;
    motion.motion.y = 20.0f;
    motion.motion.xrel = 4.0f;
    motion.motion.yrel = -1.0f;
    push(motion);
    SDL_Event right{};
    right.type = SDL_EVENT_MOUSE_BUTTON_UP;
    right.button.button = SDL_BUTTON_RIGHT;
    right.button.down = false;
    right.button.x = 31.0f;
    right.button.y = 21.0f;
    push(right);
    SDL_Event wheel{};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.y = 1.0f;
    wheel.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    wheel.wheel.mouse_x = 32.0f;
    wheel.wheel.mouse_y = 22.0f;
    push(wheel);
    SDL_Event close{};
    close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    push(close);

    w.pump();
    CHECK(w.heard.said == std::vector<std::string>{"up Q"});
    REQUIRE(w.heard.moves.size() == 1);
    CHECK(w.heard.moves[0].first.x == 30);
    CHECK(w.heard.moves[0].first.y == 20);
    CHECK(w.heard.moves[0].first.dx == 4);
    CHECK(w.heard.moves[0].first.dy == -1);
    REQUIRE(w.heard.pressed.size() == 1);
    CHECK(w.heard.pressed[0].first.button == 3);
    CHECK_FALSE(w.heard.pressed[0].first.pressed);
    CHECK(w.heard.pressed[0].first.x == 31);
    REQUIRE(w.heard.wheels.size() == 1);
    CHECK(w.heard.wheels[0].dy == doctest::Approx(-1.0)); // flipped, put back
    CHECK(w.heard.wheels[0].x == 32);
    CHECK(queued_events() == 0); // the close too was the window's to take
}

namespace {

using RealSdlWindow = gw::SdlWindowT<gw::ProcessSdlBackend>;

/// The OS window over its own SDL backend, mounted in process at its office beside the directory as
/// `LitWindow` mounts it over the suite's, an owner placed in it with the day skin: a case may ask
/// it what only C++ in its own process can, its clipboard, its place and its picture.
struct RealLit {
    Rig r;
    LitHeard heard;
    loom::WeaveId window;
    loom::WeaveId owner;
    RealSdlWindow* instance = nullptr;
    RealLit() {
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        auto w = std::make_unique<RealSdlWindow>();
        instance = w.get();
        loom::Grant g = loom::emit_default_grant(*instance);
        window = r.bus.register_weave(std::move(w), std::move(g), sdl_office());
        instance->zen_set_self(window);
        const loom::WeaveId door = zengine::testing::mount_door(r.bus);
        zengine::testing::order_activation(r.bus, door, window, 1);
        r.to_role(gfx::kGraphicsRole, gfx::SkinOffered{"day", PAINT_DAY_SO});
        auto made = std::make_unique<LitOwner>(heard);
        LitOwner* raw = made.get();
        loom::Grant og = loom::emit_default_grant(*raw);
        owner = r.bus.register_weave(std::move(made), std::move(og), "test.lit");
        raw->zen_set_self(owner);
        r.drain();
    }
    /// The owner's area at (x, y), w by h pixels.
    void place_area(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) {
        r.to_role(gfx::kGraphicsRole,
                  gfx::AreaPlaced{"test.lit", gfx::kSdlWindow, "test.lit", x, y, w, h, 0, false});
        r.to_role(gfx::kGraphicsRole, gfx::SkinChosen{"test.lit", "day"});
        r.drain();
    }
    void draw(const gfx::Sketch& s) {
        r.bus.office_send_to_role_as(owner, "test.lit", sdl_office(),
                                     loom::Message(loom::to_value(whole("test.lit", s))));
        r.drain();
    }
    void pump() {
        r.bus.send(window, loom::Message(loom::to_value(
                               zengine::timer::TimerFired{gw::kPumpTimerId})));
        r.drain();
    }
    gfx::WindowShown shown() {
        r.bus.send_as_to_role(r.ears, sdl_office(),
                              loom::Message(loom::to_value(gfx::WindowShowAsked{}), r.ears,
                                            r.ears, r.next_corr++));
        r.drain();
        return r.heard.shown.empty() ? gfx::WindowShown{} : r.heard.shown.back();
    }
};

/// SDL's own window for the one OS window the process holds.
SDL_Window* the_os_window() {
    const std::vector<SDL_WindowID> ids = os_windows();
    return ids.size() == 1 ? SDL_GetWindowFromID(ids[0]) : nullptr;
}

/// The primary display's usable area as a platform that keeps some of it reports it, for one case.
struct UsableBounds {
    explicit UsableBounds(const char* rect) { SDL_SetHint(SDL_HINT_DISPLAY_USABLE_BOUNDS, rect); }
    ~UsableBounds() { SDL_ResetHint(SDL_HINT_DISPLAY_USABLE_BOUNDS); }
    UsableBounds(const UsableBounds&) = delete;
    UsableBounds& operator=(const UsableBounds&) = delete;
};

} // namespace

TEST_CASE("the OS window over SDL opens centred on its display's usable area, its top-left there "
          "when it is larger, grows within it for an area placed past it, and goes to a "
          "remembered place kept whole there") {
    sdl_video_driver("dummy"); // a real SDL with no display, run inside the lane or out of it
    // A usable area smaller than the window: SDL alone centres such a window on the whole
    // display, past the usable area's corner; the window keeps its top-left on it.
    UsableBounds usable("100,50,1000,600");
    RealLit t;
    SDL_Window* os = the_os_window();
    REQUIRE(os != nullptr);
    int x = 0, y = 0, w = 0, h = 0;
    REQUIRE(SDL_GetWindowPosition(os, &x, &y));
    CHECK(x == 100);
    CHECK(y == 50);
    CHECK(t.instance->placement() == gw::WindowPlacementValue{100, 50, false});

    // Room enough: an area past the window grows it to the area, and the window, which nobody
    // has placed, is centred again.
    SDL_SetHint(SDL_HINT_DISPLAY_USABLE_BOUNDS, "100,50,3000,2000");
    t.place_area(0, 0, 2400, 1500);
    REQUIRE(SDL_GetWindowSize(os, &w, &h));
    CHECK(w == 2400);
    CHECK(h == 1500);
    CHECK(t.instance->placement() == gw::WindowPlacementValue{400, 300, false});
    t.pump();
    REQUIRE_FALSE(t.heard.rooms.empty());
    CHECK(t.shown().rows.size() ==
          static_cast<std::size_t>(1500 / t.heard.rooms.back().measure().line));

    // A remembered place far off the display comes back whole on it...
    t.instance->place(gw::WindowPlacementValue{-5000, 99999, false});
    REQUIRE(SDL_GetWindowPosition(os, &x, &y));
    CHECK(x == 100);
    CHECK(y == 550);
    CHECK(t.instance->placement() == gw::WindowPlacementValue{100, 550, false});
    // ...and from there the window grows where it stands, moved in only as far as it must.
    t.place_area(0, 0, 2900, 1900);
    REQUIRE(SDL_GetWindowSize(os, &w, &h));
    CHECK(w == 2900);
    CHECK(h == 1900);
    CHECK(t.instance->placement() == gw::WindowPlacementValue{100, 150, false});
}

TEST_CASE("the OS window over SDL copies to SDL's clipboard and reads it only when asked, and "
          "reads back the picture it presented as a BMP of the pixels it drew") {
    sdl_video_driver("dummy"); // a real SDL with no display, run inside the lane or out of it
    RealLit t;
    t.place_area(0, 0, 400, 200);
    gfx::Sketch s;
    s.fill_rect("red", 24, 12, 48, 24, gfx::exact(gfx::role::kAccent, 0xff0000));
    s.fill_rect("blue", 100, 50, 10, 10, gfx::exact(gfx::role::kAccent, 0x0000ff));
    t.draw(s);

    // The clipboard: emptied, it reads as empty; what the window copies is SDL's, and what anyone
    // copied is read back when asked.
    t.instance->clipboard_copy("");
    CHECK(t.instance->clipboard_text() == std::optional<std::string>(""));
    t.instance->clipboard_copy("soup");
    char* direct = SDL_GetClipboardText();
    REQUIRE(direct != nullptr);
    CHECK(std::string(direct) == "soup");
    SDL_free(direct);
    CHECK(t.instance->clipboard_text() == std::optional<std::string>("soup"));
    REQUIRE(SDL_SetClipboardText("stock"));
    CHECK(t.instance->clipboard_text() == std::optional<std::string>("stock"));

    // The picture: the window's size in pixels, a BMP, each pixel what the window drew there.
    const std::int64_t showings = t.shown().showings;
    const auto p = t.instance->capture();
    REQUIRE(p.has_value());
    CHECK(p->format == "image/bmp");
    CHECK(p->cell_px == gfx::kCellPx);
    int w = 0, h = 0;
    REQUIRE(SDL_GetWindowSizeInPixels(the_os_window(), &w, &h));
    CHECK(p->width == w);
    CHECK(p->height == h);
    const std::size_t row = (static_cast<std::size_t>(w) * 3 + 3) & ~std::size_t{3};
    REQUIRE(p->bytes.size() == 54 + row * static_cast<std::size_t>(h));
    CHECK(p->bytes.substr(0, 2) == "BM");
    const auto rgb = [&](int px, int py) { // the BMP's blue, green and red at a window pixel
        const std::size_t at = 54 + static_cast<std::size_t>(h - 1 - py) * row +
                               static_cast<std::size_t>(px) * 3;
        const auto byte = [&](std::size_t i) {
            return static_cast<std::uint32_t>(static_cast<unsigned char>(p->bytes[at + i]));
        };
        return (byte(2) << 16) | (byte(1) << 8) | byte(0);
    };
    const std::uint32_t ground = static_cast<std::uint32_t>(light::day_look().ground);
    CHECK(rgb(30, 20) == 0xff0000u);
    CHECK(rgb(105, 55) == 0x0000ffu);
    CHECK(rgb(300, 150) == ground);  // in the area, where nothing was drawn
    CHECK(rgb(1000, 600) == ground); // outside every area
    CHECK(t.shown().showings == showings); // read back, not shown again
}

TEST_CASE("the OS window over SDL with no display has no clipboard to read, no placement, nothing "
          "to move and no picture to read back") {
    sdl_video_driver("zengine-no-such-driver");
    {
        RealLit t;
        t.place_area(0, 0, 400, 200);
        t.instance->clipboard_copy("soup");
        CHECK_FALSE(t.instance->clipboard_text().has_value());
        CHECK_FALSE(t.instance->placement().has_value());
        t.instance->place(gw::WindowPlacementValue{10, 20, true});
        CHECK_FALSE(t.instance->placement().has_value());
        CHECK_FALSE(t.instance->capture().has_value());
        CHECK(os_windows().empty());
        CHECK(SDL_WasInit(SDL_INIT_VIDEO) == 0);
    }
    sdl_video_driver("dummy");
}

#endif // GRAPHICS_HAS_SDL
