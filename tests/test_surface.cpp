// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Surface suite — the Skin pattern, proven headless.
//
// Four tiers, deliberately ordered:
//   1. CONTRACT pins — the surface vocabulary's ZEN_SHAPE spellings derive
//      schemas content-id-identical to their SchemaBuilder spellings.
//   2. GOLDEN BYTES — the terminal medium as pure string math: a known
//      SnakeVisual becomes the exact ANSI frame (both styles, alive and dead,
//      first frame and steady state), and text slots land on their exact
//      rows. The bytes are literals in this file, not re-derivations — the
//      pin would catch the styles drifting from the old drawers' look.
//   2b. THE SDL PLAN — the SDL skin's brain as pure math on EVERY lane,
//      including the ones that never build SDL: rectangles, painter's order,
//      geometry, the death tint, the food sentinel, the title projection.
//   3. THE WEAVE through a real bus — a fake medium records what the shell
//      delegates; the hello-once law and the counters are pinned without a
//      terminal or a window in sight.
//   4. THE REAL LIBRARIES through the real Kernel — a skin .so claims the
//      zengine.skin role and paints real intent from the real world .so; the
//      score tally SURVIVES the painter being replaced (the SurfaceReady
//      republish handshake); a second skin is cleanly REFUSED the held role
//      (ownership is enforced ground); and the -fno-gnu-unique linkage pins
//      ride the skin pair, the first weave libraries to share TWO packages'
//      vocabulary headers. Where the SDL skin is built, the same intent
//      messages drive it under SDL's dummy driver — the agnosticism proof
//      through the real gate.
//
// What headless CANNOT prove is photons: a human sees the alternate screen
// and the real window in the live runs. Stated so this green means exactly
// what it says.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "surface/skin.hpp"
#include "surface/skin_sdl_plan.hpp"
#include "surface/skin_tui.hpp"
#include "surface/vocabulary.hpp"

#include "lifecycle_door.hpp"

#include "snake/vocabulary.hpp"

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
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace zengine::surface;
using zengine::snake::Pos;
using zengine::snake::SnakeVisual;
using loom::schema_of;

namespace {

// ---- tier 2 rig ----------------------------------------------------------------

struct StringSink {
    std::string out;
    void write(std::string_view s) { out += s; }
};

SnakeVisual small_visual() {
    SnakeVisual v;
    v.width = 4;
    v.height = 3;
    v.snake = {Pos{1, 1}, Pos{0, 1}};
    v.food = Pos{3, 0};
    v.alive = true;
    v.score = 7;
    return v;
}

// ---- tier 3 rig ----------------------------------------------------------------

/// Records what the shell delegates. What the real media do to a terminal or
/// a window, this one does to a log.
struct FakeMedium {
    std::vector<std::string>* log = nullptr;
    void frame(const SnakeVisual& v, bool first) {
        log->push_back("frame w=" + std::to_string(v.width) + " first=" + (first ? "1" : "0"));
    }
    void canvas(const SurfaceCanvas& c, bool first) {
        log->push_back("canvas w=" + std::to_string(c.width) + " rects=" +
                       std::to_string(c.rects.size()) + " labels=" +
                       std::to_string(c.labels.size()) + " first=" + (first ? "1" : "0"));
    }
    void note(std::string_view slot, std::string_view text) {
        log->push_back("note " + std::string(slot) + "=" + std::string(text));
    }
    void pump() { log->push_back("pump"); }
};

struct ReadyState {
    std::int64_t heard = 0;
    ZEN_SHAPE(ReadyState, 1, ZEN_FIELD(heard));
};

/// An ordinary accepter of the skins' hello — what any text publisher looks
/// like to the bus.
class ReadyEars : public loom::WeaveBase<ReadyEars, ReadyState, loom::Accept<SurfaceReady>,
                                         loom::Emit<>> {
public:
    explicit ReadyEars(int& count) : count_(&count) {}
    void on(const SurfaceReady&, loom::Mail&) { ++(*count_); }

private:
    int* count_;
};

/// What a skin asked the Timer package for, field by field.
struct BeatAsk {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role;
};

struct CatcherState {
    std::int64_t asks = 0;
    ZEN_SHAPE(CatcherState, 1, ZEN_FIELD(asks));
};

/// A stand-in TimerService: holds the timer role and records the exact asks.
/// It never beats — the case below delivers the firings by hand, so the
/// skin's side of the contract is pinned without a clock in sight.
class BeatCatcher
    : public loom::WeaveBase<BeatCatcher, CatcherState,
                             loom::Accept<zengine::timer::StartRoleTimer>, loom::Emit<>> {
public:
    explicit BeatCatcher(std::vector<BeatAsk>& asks) : asks_(&asks) {}
    void on(const zengine::timer::StartRoleTimer& s, loom::Mail&) {
        ++state_.asks;
        asks_->push_back(BeatAsk{s.id, s.delay_ms, s.repeat, s.role});
    }

private:
    std::vector<BeatAsk>* asks_;
};

/// mount(), plus a role binding (the weave sugar has no role parameter; the
/// catcher must HOLD zengine.timer so the skins' role-addressed asks land).
template <class W, class... Args>
loom::WeaveId mount_into_role(loom::Switchboard& bus, std::string role, Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    loom::Grant grant = loom::emit_default_grant(*raw);
    loom::allow_poke_answers(grant);
    const loom::WeaveId id =
        bus.register_weave(std::move(weave), std::move(grant), std::move(role));
    raw->zen_set_self(id);
    return id;
}

// ---- tier 4 rig (the test_snake rig, minus the lockstep pilot) ------------------

struct Recorded {
    struct Answer {
        std::uint64_t corr = 0;
        int kind = 0; // 0 Result, 1 Ack, 2 Refused
        std::string text;
    };
    std::vector<Answer> answers;

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

class Probe : public loom::WeaveBase<Probe, ProbeState,
                                     loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                     loom::Emit<>> {
public:
    explicit Probe(Recorded& rec) : rec_(&rec) {}
    void on(const loom::Result& r, loom::Mail& mail) { note(mail, 0, r.value); }
    void on(const loom::Ack&, loom::Mail& mail) { note(mail, 1, ""); }
    void on(const loom::Refused& r, loom::Mail& mail) { note(mail, 2, r.reason); }

private:
    void note(loom::Mail& mail, int kind, std::string text) {
        ++state_.noted;
        rec_->answers.push_back(Recorded::Answer{mail.correlation(), kind, std::move(text)});
    }
    Recorded* rec_;
};

/// fd-1 silencer: the real skins paint stdout when frames arrive; the suite's
/// own output must stay legible. Scoped exactly around pumps.
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
        reach.allow(loom::ListLoaded::zen_name, loom::ListLoaded::zen_version, manager);
        return loom::mount_granted<Probe>(bus, std::move(reach), rec);
    }

    void pump() {
        Hush hush;
        bus.pump();
    }

    template <class Cmd>
    Recorded::Answer command(const Cmd& cmd) {
        const std::uint64_t corr = next_corr++;
        bus.send_as(probe, manager, loom::Message(loom::to_value(cmd), probe, probe, corr));
        pump();
        const Recorded::Answer* a = rec.find(corr);
        REQUIRE(a != nullptr);
        return *a;
    }

    template <class Poke>
    Recorded::Answer poke(loom::WeaveId target, const Poke& p) {
        const std::uint64_t corr = next_corr++;
        bus.send(target, loom::Message(loom::to_value(p), loom::WeaveId{}, probe, corr));
        pump();
        const Recorded::Answer* a = rec.find(corr);
        REQUIRE(a != nullptr);
        return *a;
    }

    loom::WeaveId load(const char* name, const char* path, const char* role) {
        const Recorded::Answer a = command(loom::LoadWeave{name, path, role});
        REQUIRE_MESSAGE(a.kind == 0, "load refused: ", a.text);
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(a.text))};
    }

    void tick() {
        bus.send_to_role(zengine::snake::kWorldRole,
                         loom::Message(loom::to_value(zengine::snake::SnakeTick{})));
        pump();
    }

    /// Root-send an intent directly to a weave (the gate still guards the
    /// door — root chooses the target, never skips conformance).
    template <class T>
    void intent(loom::WeaveId target, const T& msg) {
        bus.send(target, loom::Message(loom::to_value(msg)));
        pump();
    }
};

} // namespace

// ============================================================================
// Tier 1 — the vocabulary, pinned by content-id
// ============================================================================

TEST_CASE("contract: the surface shapes derive their locked spellings exactly") {
    using loom::Kind;
    using loom::SchemaBuilder;
    const auto text = SchemaBuilder("SurfaceText", 1)
                          .field("slot", Kind::Text)
                          .field("text", Kind::Text)
                          .build();
    CHECK(schema_of<SurfaceText>()->content_id() == text->content_id());
    CHECK(schema_of<SurfaceReady>()->content_id() ==
          SchemaBuilder("SurfaceReady", 1).build()->content_id());
    CHECK(schema_of<PumpSurface>()->content_id() ==
          SchemaBuilder("PumpSurface", 1).build()->content_id());
}

TEST_CASE("contract: the canvas shapes derive their declared spellings exactly") {
    using loom::Kind;
    using loom::SchemaBuilder;
    const auto rect = SchemaBuilder("SurfaceRect", 1)
                          .field("x", Kind::Int)
                          .field("y", Kind::Int)
                          .field("w", Kind::Int)
                          .field("h", Kind::Int)
                          .field("role", Kind::Int)
                          .build();
    CHECK(schema_of<SurfaceRect>()->content_id() == rect->content_id());

    const auto label = SchemaBuilder("SurfaceLabel", 1)
                           .field("x", Kind::Int)
                           .field("y", Kind::Int)
                           .field("text", Kind::Text)
                           .field("role", Kind::Int)
                           .build();
    CHECK(schema_of<SurfaceLabel>()->content_id() == label->content_id());

    // The canvas carries lists of the two above, so its identity depends on
    // theirs -- which is the property that makes a drift anywhere in this
    // vocabulary a red here rather than a surprise on a wire.
    const auto canvas = SchemaBuilder("SurfaceCanvas", 1)
                            .field("width", Kind::Int)
                            .field("height", Kind::Int)
                            .list("rects", loom::type_message(rect))
                            .list("labels", loom::type_message(label))
                            .build();
    CHECK(schema_of<SurfaceCanvas>()->content_id() == canvas->content_id());
}

// ============================================================================
// Tier 2 — the terminal medium as golden bytes
// ============================================================================

TEST_CASE("golden: the classic style paints the exact frame, first and steady") {
    TuiMedium<ClassicStyle, StringSink> m;
    const SnakeVisual v = small_visual();

    m.frame(v, /*first=*/true);
    CHECK(m.sink().out ==
          "\x1b[3;1H\x1b[0J"
          "+----+\r\n"
          "|   *|\r\n"
          "|oO  |\r\n"
          "|    |\r\n"
          "+----+\r\n"
          "\x1b[2K  classic skin - score 7 - alive\r\n");

    m.sink().out.clear();
    m.frame(v, /*first=*/false);
    CHECK(m.sink().out ==
          "\x1b[3;1H"
          "+----+\r\n"
          "|   *|\r\n"
          "|oO  |\r\n"
          "|    |\r\n"
          "+----+\r\n"
          "\x1b[2K  classic skin - score 7 - alive\r\n");

    m.sink().out.clear();
    SnakeVisual dead = v;
    dead.alive = false;
    m.frame(dead, /*first=*/false);
    CHECK(m.sink().out.find(" - DEAD (n = new game)\r\n") != std::string::npos);
}

TEST_CASE("golden: the block style paints the exact frame, and death changes the banner") {
    TuiMedium<BlockStyle, StringSink> m;
    SnakeVisual v;
    v.width = 2;
    v.height = 1;
    v.snake = {Pos{0, 0}};
    v.food = Pos{1, 0};
    v.alive = true;
    v.score = 0;

    m.frame(v, /*first=*/true);
    CHECK(m.sink().out ==
          "\x1b[3;1H\x1b[0J"
          "\x1b[2K\x1b[7m BLOCK SKIN \x1b[0m  score 0\r\n"
          "\x1b[36m====\x1b[0m\r\n"
          "\x1b[2K\x1b[32;7m@@\x1b[0m\x1b[33;7m()\x1b[0m\r\n"
          "\x1b[36m====\x1b[0m\r\n");

    m.sink().out.clear();
    v.alive = false;
    m.frame(v, /*first=*/false);
    CHECK(m.sink().out.find("  \x1b[31;7m DEAD - n starts over \x1b[0m\r\n") !=
          std::string::npos);
}

TEST_CASE("golden: text slots land on their rows; unknown slots are dropped") {
    TuiMedium<ClassicStyle, StringSink> m;

    m.note(kSlotStatus, "hello there");
    CHECK(m.sink().out == "\x1b[1;1H\x1b[2K hello there");

    m.sink().out.clear();
    m.note(kSlotScore, "tally: 3");
    CHECK(m.sink().out == "\x1b[2;1H\x1b[2K tally: 3");

    m.sink().out.clear();
    m.note("weather", "sunny");
    CHECK(m.sink().out.empty());
}

TEST_CASE("golden: a canvas rasterizes to exact bytes -- roles, paint order, labels over rects") {
    SurfaceCanvas c;
    c.width = 6;
    c.height = 3;
    // Painter's order is list order: the muted backdrop first, the fill over it.
    c.rects.push_back(SurfaceRect{0, 0, 6, 3, role::kMuted});
    c.rects.push_back(SurfaceRect{1, 1, 3, 1, role::kFill});
    // A label wins over every rect it crosses.
    c.labels.push_back(SurfaceLabel{2, 1, "ab", role::kAlert});

    TuiMedium<ClassicStyle, StringSink> m;
    m.canvas(c, /*first=*/true);
    // One SGR per RUN, not per cell: the ink changes only where the role does
    // (`ab` is one run of two characters), and a row ends by putting the terminal
    // back where it found it.
    CHECK(m.sink().out ==
          "\x1b[3;1H\x1b[0J"
          "\x1b[2K\x1b[90m......\x1b[0m\r\n"
          "\x1b[2K\x1b[90m.\x1b[37m#\x1b[31;1mab\x1b[90m..\x1b[0m\r\n"
          "\x1b[2K\x1b[90m......\x1b[0m\r\n");

    // Steady state drops only the erase-below: the same layout convention a
    // board follows, so a canvas and a frame claim the surface identically.
    m.sink().out.clear();
    m.canvas(c, /*first=*/false);
    CHECK(m.sink().out.rfind("\x1b[3;1H\x1b[2K", 0) == 0);
    CHECK(m.sink().out.find("\x1b[0J") == std::string::npos);
}

TEST_CASE("canvas: elements are clipped to the extent, and an empty canvas is a picture") {
    // THE RIGHT-EDGE CLIP IS THE OBSERVABLE ONE, so this canvas is three rows
    // tall on purpose. The grid is row-major, so an unclipped write past the
    // right edge does not fall off the end -- it lands at the START OF THE NEXT
    // ROW, which is visible. (The BOTTOM-edge clip cannot be shown this way: a
    // write below the extent goes to cells nothing renders, so there the guard is
    // memory safety rather than appearance. The sanitizer lane is what watches
    // that one; a mutation to the y-bound alone is green here and says so.)
    SurfaceCanvas c;
    c.width = 4;
    c.height = 3;
    // A rect hanging off the LEFT and TOP, and one hanging off the RIGHT.
    c.rects.push_back(SurfaceRect{-2, -1, 3, 2, role::kFill});
    c.rects.push_back(SurfaceRect{2, 0, 4, 1, role::kMuted});
    // A label running off the right edge, on its own row.
    c.labels.push_back(SurfaceLabel{2, 1, "abcd", role::kAccent});
    // And one hanging off the BOTTOM. It contributes nothing a reader can see --
    // that is the point of it. It exists so that the bottom-edge guard has
    // something to guard against in this suite's data at all: without an element
    // down here, deleting that guard is not merely invisible, it is INERT, and a
    // sanitizer lane would have nothing to catch either.
    c.rects.push_back(SurfaceRect{0, 2, 2, 3, role::kAlert});

    TuiMedium<ClassicStyle, StringSink> m;
    m.canvas(c, /*first=*/false);
    CHECK(m.sink().out ==
          "\x1b[3;1H"
          // row 0: the first rect's surviving cell at x=0, then the second's two.
          "\x1b[2K\x1b[37m#\x1b[0m \x1b[90m..\x1b[0m\r\n"
          // row 1: NOTHING wrapped here from row 0's overflow -- two background
          // cells, then the label's two surviving characters.
          "\x1b[2K\x1b[0m  \x1b[36mab\x1b[0m\r\n"
          // row 2: the bottom rect's ONE surviving row, and nothing wrapped from
          // row 1's label.
          "\x1b[2K\x1b[31;1m!!\x1b[0m  \r\n");

    // An extent of nothing produces no rows at all -- and does not crash, which
    // is the half of this case that matters.
    SurfaceCanvas empty;
    TuiMedium<ClassicStyle, StringSink> m2;
    m2.canvas(empty, /*first=*/false);
    CHECK(m2.sink().out == "\x1b[3;1H");

    // An extent with nothing IN it clears its rows: "nothing" is a picture. The
    // leading reset is load-bearing -- a background row states its own ink rather
    // than drawing in whatever the terminal was already wearing.
    SurfaceCanvas blank;
    blank.width = 2;
    blank.height = 1;
    TuiMedium<ClassicStyle, StringSink> m3;
    m3.canvas(blank, /*first=*/false);
    CHECK(m3.sink().out == "\x1b[3;1H\x1b[2K\x1b[0m  \r\n");
}

TEST_CASE("canvas: an unknown role paints as kFill rather than vanishing") {
    SurfaceCanvas c;
    c.width = 2;
    c.height = 1;
    c.rects.push_back(SurfaceRect{0, 0, 2, 1, 99}); // a role no Skin knows

    TuiMedium<ClassicStyle, StringSink> m;
    m.canvas(c, /*first=*/false);
    // Drawn, in kFill's ink and glyph: vocabulary.hpp's stated fallback, and the
    // opposite resolution from an unknown text slot (which has no row to go to).
    CHECK(m.sink().out == "\x1b[3;1H\x1b[2K\x1b[37m##\x1b[0m\r\n");
}

// ============================================================================
// Tier 2b — the SDL plan as pure math (every lane, SDL built or not)
// ============================================================================

TEST_CASE("plan: geometry follows the board, painter's order holds, colors are the plan's") {
    const SnakeVisual v = small_visual();
    const PlanSize win = window_size_of(v);
    CHECK(win.w == 120);
    CHECK(win.h == 96);

    const std::vector<PlanRect> rects = plan_frame(v);
    REQUIRE(rects.size() == 4);

    // Background first, the whole window, near-black while alive.
    CHECK(rects[0].x == 0);
    CHECK(rects[0].y == 0);
    CHECK(rects[0].w == 120);
    CHECK(rects[0].h == 96);
    CHECK(rects[0].r == 18);
    CHECK(rects[0].g == 18);
    CHECK(rects[0].b == 24);

    // Food next, inset deeper than the snake.
    CHECK(rects[1].x == 90);
    CHECK(rects[1].y == 18);
    CHECK(rects[1].w == 12);
    CHECK(rects[1].h == 12);
    CHECK(rects[1].r == 232);

    // Body before head; head drawn last and brighter.
    CHECK(rects[2].x == 13);
    CHECK(rects[2].y == 37);
    CHECK(rects[2].w == 22);
    CHECK(rects[2].g == 168);
    CHECK(rects[3].x == 37);
    CHECK(rects[3].y == 37);
    CHECK(rects[3].g == 232);
}

TEST_CASE("plan: death tints the window, the food sentinel plans no food, the title projects") {
    SnakeVisual v = small_visual();
    v.alive = false;
    v.food = Pos{-1, -1}; // the off-board sentinel: nothing to plan
    const std::vector<PlanRect> rects = plan_frame(v);
    REQUIRE(rects.size() == 3); // background + body + head, no food
    CHECK(rects[0].r == 88);
    CHECK(rects[0].g == 16);
    CHECK(rects[0].b == 16);

    CHECK(title_of("", "") == "zengine [sdl skin]");
    CHECK(title_of("paused", "") == "zengine [sdl skin] | paused");
    CHECK(title_of("running", "eaten: 2") == "zengine [sdl skin] | running | eaten: 2");
}

// ============================================================================
// Tier 2c — the SDL canvas plan: labels reach pixels (G-0 / P8)
// ============================================================================
//
// The whole label path is here rather than behind SURFACE_HAS_SDL on purpose.
// `plan_canvas` is the only place that knows a label from a rect -- the SDL edge
// receives one flat quad list -- so these cases are what stands between the
// medium and P8, and they must run on every lane, including the Windows stranger
// lane that builds no SDL at all.
//
// The first case belongs to BOTH media: G-0 found the terminal Skin's clipping
// unsafe while writing this one's, so the guard is shared and so is its proof.

namespace {

/// Rasterize a plan back into a pixel grid, so a case can assert what a screen
/// would actually show rather than which quads were emitted. `ink` is anything
/// that is not the canvas background; a cell a label took reads as background
/// even where its glyph has no ink, which is the point of `plan_canvas` clearing
/// a label's own cell.
class Raster {
public:
    explicit Raster(const SurfaceCanvas& c)
        : w_(canvas_extent(c.width) * kCanvasCellPx), h_(canvas_extent(c.height) * kCanvasCellPx),
          px_(static_cast<std::size_t>(w_ * h_), PlanInk{0, 0, 0}) {
        for (const PlanRect& r : plan_canvas(c)) {
            // A quad outside the surface it claims to be drawing on is the defect
            // this rasterizer exists to catch, so it is an assertion and not a
            // clamp -- clamping here would hide exactly what a sanitizer sees.
            REQUIRE(r.x >= 0);
            REQUIRE(r.y >= 0);
            REQUIRE(r.w > 0);
            REQUIRE(r.h > 0);
            REQUIRE(r.x + r.w <= w_);
            REQUIRE(r.y + r.h <= h_);
            for (std::int64_t y = r.y; y < r.y + r.h; ++y) {
                for (std::int64_t x = r.x; x < r.x + r.w; ++x) {
                    px_[static_cast<std::size_t>(y * w_ + x)] = PlanInk{r.r, r.g, r.b};
                }
            }
        }
    }

    PlanInk at(std::int64_t x, std::int64_t y) const {
        return px_[static_cast<std::size_t>(y * w_ + x)];
    }

    /// How many pixels of one canvas cell are ink -- 0 means nothing was drawn
    /// there, which is what "a label was dropped" looks like from the outside.
    int ink_in_cell(std::int64_t cx, std::int64_t cy) const {
        int n = 0;
        for (std::int64_t y = 0; y < kCanvasCellPx; ++y) {
            for (std::int64_t x = 0; x < kCanvasCellPx; ++x) {
                const PlanInk p = at(cx * kCanvasCellPx + x, cy * kCanvasCellPx + y);
                if (!(p == kCanvasBackground) && !(p == PlanInk{0, 0, 0})) {
                    ++n;
                }
            }
        }
        return n;
    }

    /// The ink colour of a cell that has some, as the medium would show it.
    PlanInk ink_colour(std::int64_t cx, std::int64_t cy) const {
        for (std::int64_t y = 0; y < kCanvasCellPx; ++y) {
            for (std::int64_t x = 0; x < kCanvasCellPx; ++x) {
                const PlanInk p = at(cx * kCanvasCellPx + x, cy * kCanvasCellPx + y);
                if (!(p == kCanvasBackground) && !(p == PlanInk{0, 0, 0})) {
                    return p;
                }
            }
        }
        return kCanvasBackground;
    }

private:
    std::int64_t w_;
    std::int64_t h_;
    std::vector<PlanInk> px_;
};

SurfaceCanvas canvas_of(std::int64_t w, std::int64_t h) {
    SurfaceCanvas c;
    c.width = w;
    c.height = h;
    return c;
}

/// The characters of one terminal row of `canvas_body`, with the SGR runs taken
/// out -- what a reader of that medium sees, as opposed to how it was coloured.
std::string tui_row(const SurfaceCanvas& c, std::size_t row) {
    const std::string all = canvas_body(c);
    std::vector<std::string> rows;
    std::string current;
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (all[i] == '\x1b') {
            while (i < all.size() && all[i] != 'm' && all[i] != 'K') {
                ++i;
            }
            continue;
        }
        if (all[i] == '\r') {
            continue;
        }
        if (all[i] == '\n') {
            rows.push_back(current);
            current.clear();
            continue;
        }
        current += all[i];
    }
    return row < rows.size() ? rows[row] : std::string{};
}

} // namespace

TEST_CASE("canvas: a published coordinate cannot overflow or outrun the canvas") {
    // FOUND BY G-0 IN COMMITTED CODE, in the TERMINAL Skin, and both halves were
    // reachable from data alone -- a canvas is a ZEN_SHAPE, so its numbers come
    // from whoever published it.
    //
    //   `r.x + dx` and `l.x + i` at the top of the number line were signed
    //   overflow (UBSan says so; the standing lane never caught it because no
    //   test fed the rasterizer such a canvas -- this one now does);
    //
    //   and the rect loop walked every cell the PUBLISHER named before dropping
    //   the ones off the canvas, so a rect 10^8 cells wide cost 75 ms on a 4x2
    //   canvas and the same shape near INT64_MAX did not finish at all.
    //
    // This case is the data those two guards guard against, in the suite that
    // runs everywhere -- and it is why `surface/cells.hpp` is shared with the SDL
    // plan rather than copied into it.
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    SurfaceCanvas c;
    c.width = 4;
    c.height = 2;
    c.rects.push_back(SurfaceRect{kMax, 0, kMax, 1, role::kFill});
    c.rects.push_back(SurfaceRect{kMin, kMin, kMax, kMax, role::kMuted});
    c.rects.push_back(SurfaceRect{0, 0, kMax, kMax, role::kAlert}); // covers the canvas
    c.labels.push_back(SurfaceLabel{kMax, 0, "AB", role::kFill});
    c.labels.push_back(SurfaceLabel{kMin, 1, "AB", role::kFill});

    // It returns, it is bounded, and the one rect that does reach the canvas
    // covers it exactly: clipping changed what is WALKED, never what is DRAWN.
    CHECK(tui_row(c, 0) == "!!!!");
    CHECK(tui_row(c, 1) == "!!!!");

    // The same numbers through the same helper, spelled out.
    CHECK(add_cells(kMax, 1) == kMax);
    CHECK(add_cells(kMin, -1) == kMin);
    CHECK(add_cells(5, 7) == 12);
    CHECK(clip_span(kMax, kMax, 4) == CellSpan{4, 4});
    CHECK(clip_span(kMin, kMax, 4) == CellSpan{0, 0});
    CHECK(clip_span(-2, 5, 4) == CellSpan{0, 3});
    CHECK(clip_span(1, 2, 4) == CellSpan{1, 3});
    CHECK(clip_span(9, 2, 4).empty());
}

TEST_CASE("canvas plan: a label reaches pixels, in the cell the publisher named") {
    SurfaceCanvas c = canvas_of(8, 3);
    c.labels.push_back(SurfaceLabel{2, 1, "AB", role::kFill});
    const Raster r(c);

    // THE P8 CASE. Ink exists where the label was published and nowhere else on
    // that row -- if the medium ever drops labels again, every one of these
    // counts goes to zero.
    CHECK(r.ink_in_cell(2, 1) > 0);
    CHECK(r.ink_in_cell(3, 1) > 0);
    CHECK(r.ink_in_cell(0, 1) == 0);
    CHECK(r.ink_in_cell(1, 1) == 0);
    CHECK(r.ink_in_cell(4, 1) == 0);
    CHECK(r.ink_in_cell(2, 0) == 0);
    CHECK(r.ink_in_cell(2, 2) == 0);

    // One cell per byte, and the cell is exactly the canvas cell: the glyph fills
    // it, so 'A' and 'B' are different pictures in adjacent cells rather than one
    // run of text drifting off the grid.
    CHECK(kGlyphScale == 2);
    CHECK(r.at(2 * kCanvasCellPx, 1 * kCanvasCellPx) == kCanvasBackground);
    CHECK_FALSE(r.ink_in_cell(2, 1) == r.ink_in_cell(3, 1)); // A and B differ
}

TEST_CASE("canvas plan: a label takes its whole cell, and lands over the rects") {
    SurfaceCanvas c = canvas_of(4, 1);
    c.rects.push_back(SurfaceRect{0, 0, 4, 1, role::kAlert});
    c.labels.push_back(SurfaceLabel{1, 0, "i", role::kAccent});
    const Raster r(c);

    // The rect is under everything...
    CHECK(r.at(0, 0) == ink_for_role(role::kAlert));
    CHECK(r.at(3 * kCanvasCellPx, 0) == ink_for_role(role::kAlert));
    // ...but the label's own cell is the label's: the terminal's `put` overwrites
    // both the glyph and the role of a cell a label lands on, so the closest this
    // medium can do is clear the cell and draw the glyph on it. A name written on
    // an object must not have to compete with the object's own fill.
    CHECK(r.at(1 * kCanvasCellPx, 0) == kCanvasBackground);
    CHECK(r.ink_colour(1, 0) == ink_for_role(role::kAccent));
}

TEST_CASE("canvas plan: later labels win, exactly as the terminal's do") {
    SurfaceCanvas c = canvas_of(2, 1);
    c.labels.push_back(SurfaceLabel{0, 0, "X", role::kMuted});
    c.labels.push_back(SurfaceLabel{0, 0, "X", role::kAlert});
    const Raster r(c);
    CHECK(r.ink_colour(0, 0) == ink_for_role(role::kAlert));
}

TEST_CASE("canvas plan: role decides the ink, and an unknown role is still drawn") {
    SurfaceCanvas c = canvas_of(4, 1);
    c.labels.push_back(SurfaceLabel{0, 0, "A", role::kAccent});
    c.labels.push_back(SurfaceLabel{1, 0, "A", role::kMuted});
    c.labels.push_back(SurfaceLabel{2, 0, "A", role::kAlert});
    c.labels.push_back(SurfaceLabel{3, 0, "A", 99}); // no such role
    const Raster r(c);
    CHECK(r.ink_colour(0, 0) == PlanInk{112, 232, 240});
    CHECK(r.ink_colour(1, 0) == PlanInk{96, 96, 108});
    CHECK(r.ink_colour(2, 0) == PlanInk{232, 72, 72});
    // vocabulary.hpp's stated fallback: an unknown role paints as kFill rather
    // than vanishing.
    CHECK(r.ink_colour(3, 0) == ink_for_role(role::kFill));
}

TEST_CASE("canvas plan: clipping is per cell, against the canvas and nothing else") {
    SUBCASE("a label running off the right edge keeps the cells that fit") {
        SurfaceCanvas c = canvas_of(3, 1);
        c.labels.push_back(SurfaceLabel{1, 0, "ABCDEFGH", role::kFill});
        const Raster r(c); // the Raster's own REQUIREs are the out-of-bounds proof
        CHECK(r.ink_in_cell(1, 0) > 0);
        CHECK(r.ink_in_cell(2, 0) > 0);
    }
    SUBCASE("a negative origin drops the cells before the canvas and keeps the rest") {
        SurfaceCanvas c = canvas_of(3, 1);
        c.labels.push_back(SurfaceLabel{-2, 0, "ABCD", role::kFill});
        const Raster r(c);
        // 'A' and 'B' are off the canvas; 'C' lands in cell 0.
        SurfaceCanvas just_c = canvas_of(3, 1);
        just_c.labels.push_back(SurfaceLabel{0, 0, "C", role::kFill});
        const Raster expected(just_c);
        CHECK(r.ink_in_cell(0, 0) == expected.ink_in_cell(0, 0));
        CHECK(r.ink_in_cell(0, 0) > 0);
    }
    SUBCASE("a label below or above the canvas draws nothing at all") {
        SurfaceCanvas c = canvas_of(3, 2);
        c.labels.push_back(SurfaceLabel{0, 2, "A", role::kFill});  // one row past
        c.labels.push_back(SurfaceLabel{0, -1, "A", role::kFill}); // one row before
        const Raster r(c);
        CHECK(r.ink_in_cell(0, 0) == 0);
        CHECK(r.ink_in_cell(0, 1) == 0);
    }
    SUBCASE("an empty label and an empty canvas are both legitimate pictures") {
        SurfaceCanvas c = canvas_of(2, 1);
        c.labels.push_back(SurfaceLabel{0, 0, "", role::kFill});
        CHECK(Raster(c).ink_in_cell(0, 0) == 0);
        CHECK(plan_canvas(canvas_of(0, 0)).empty());
        CHECK(plan_canvas(canvas_of(-5, -5)).empty());
    }
    SUBCASE("the saturated ends of the number line clip like any other coordinate") {
        constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
        constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
        SurfaceCanvas c = canvas_of(4, 2);
        // Every one of these would be undefined behaviour under a naive
        // `x + i` or `cell * kCanvasCellPx`; all of them must simply clip.
        c.labels.push_back(SurfaceLabel{kMax, 0, "AB", role::kFill});
        c.labels.push_back(SurfaceLabel{kMin, 0, "AB", role::kFill});
        c.labels.push_back(SurfaceLabel{0, kMax, "AB", role::kFill});
        c.labels.push_back(SurfaceLabel{0, kMin, "AB", role::kFill});
        c.rects.push_back(SurfaceRect{kMax, kMax, kMax, kMax, role::kFill});
        c.rects.push_back(SurfaceRect{kMin, kMin, kMax, kMax, role::kFill});
        const Raster r(c);
        CHECK(r.ink_in_cell(0, 0) == 0);
        CHECK(r.ink_in_cell(3, 1) == 0);
        // And an extent no pixel number could hold is capped rather than
        // multiplied into undefined behaviour.
        CHECK(canvas_extent(kMax) == kMaxCanvasCells);
        CHECK(canvas_window_size(canvas_of(78, 22)).w == 78 * kCanvasCellPx);
        CHECK(canvas_window_size(canvas_of(78, 22)).h == 22 * kCanvasCellPx);
    }
}

TEST_CASE("canvas plan: a byte with no glyph is SEEN, never dropped") {
    // P8 at character granularity is the thing this policy exists to refuse: a
    // character that silently disappears is invisible to the publisher, so an
    // unsupported byte draws a box instead.
    SurfaceCanvas c = canvas_of(6, 1);
    c.labels.push_back(SurfaceLabel{0, 0, "A\x01\xC3\xA9 B", role::kFill}); // 'e-acute' in UTF-8
    const Raster r(c);
    CHECK(r.ink_in_cell(0, 0) > 0); // 'A'
    CHECK(r.ink_in_cell(1, 0) > 0); // a control byte -- drawn as the box
    CHECK(r.ink_in_cell(2, 0) > 0); // one cell per BYTE of the sequence...
    CHECK(r.ink_in_cell(3, 0) > 0); // ...so both bytes are visible
    CHECK(r.ink_in_cell(4, 0) == 0); // a space is a real, blank glyph -- not a box
    CHECK(r.ink_in_cell(5, 0) > 0);  // 'B'

    // All three unsupported bytes draw the SAME thing, and it is the box.
    SurfaceCanvas box = canvas_of(1, 1);
    box.labels.push_back(SurfaceLabel{0, 0, "\x01", role::kFill});
    const int box_ink = Raster(box).ink_in_cell(0, 0);
    CHECK(r.ink_in_cell(1, 0) == box_ink);
    CHECK(r.ink_in_cell(2, 0) == box_ink);
    CHECK(r.ink_in_cell(3, 0) == box_ink);
    CHECK(glyph_of(0x01) == kUnknownGlyph);
    CHECK(glyph_of(0xC3) == kUnknownGlyph);
    CHECK(glyph_of(0x7f) == kUnknownGlyph);
    CHECK_FALSE(glyph_of(' ') == kUnknownGlyph);
}

TEST_CASE("canvas plan: the promise is printable ASCII, and every character of it") {
    // The measured Workshop population is 73 distinct bytes; the other 22
    // printable characters are one maker keystroke away, so the floor is the
    // whole range rather than what the tool happens to say today.
    for (unsigned char b = kFirstGlyph; b <= kLastGlyph; ++b) {
        CAPTURE(b);
        CHECK_FALSE(glyph_of(b) == kUnknownGlyph);
    }
    // ...and every one of them is distinguishable from every other, so no two
    // characters can read as the same thing on screen. Space is the one glyph
    // that is legitimately blank.
    for (unsigned char a = kFirstGlyph; a <= kLastGlyph; ++a) {
        for (unsigned char b = static_cast<unsigned char>(a + 1); b <= kLastGlyph; ++b) {
            if (glyph_of(a) == glyph_of(b)) {
                CAPTURE(a);
                CAPTURE(b);
                FAIL_CHECK("two printable characters share one glyph");
            }
        }
    }
    CHECK(glyph_of(' ') == Glyph{});
}

TEST_CASE("canvas plan: rectangles are clipped to the canvas, like the terminal's") {
    SurfaceCanvas c = canvas_of(3, 2);
    c.rects.push_back(SurfaceRect{-1, -1, 10, 10, role::kFill});
    const std::vector<PlanRect> quads = plan_canvas(c);
    REQUIRE(quads.size() == 1);
    CHECK(quads[0].x == 0);
    CHECK(quads[0].y == 0);
    CHECK(quads[0].w == 3 * kCanvasCellPx);
    CHECK(quads[0].h == 2 * kCanvasCellPx);

    // An empty or inverted rect is nothing, not a negative quad.
    SurfaceCanvas none = canvas_of(3, 2);
    none.rects.push_back(SurfaceRect{1, 1, 0, 5, role::kFill});
    none.rects.push_back(SurfaceRect{1, 1, -4, 5, role::kFill});
    none.rects.push_back(SurfaceRect{9, 9, 5, 5, role::kFill});
    CHECK(plan_canvas(none).empty());
}

TEST_CASE("canvas plan: the two media place the same label in the same cell") {
    // The agnosticism claim, made checkable. The terminal rasterizes to bytes and
    // this medium to quads, so they cannot be compared directly -- but WHICH CELL
    // a character lands in is a fact both must agree on, and it is the fact a
    // publisher relies on when it puts a panel at column 50.
    SurfaceCanvas c = canvas_of(6, 2);
    c.labels.push_back(SurfaceLabel{2, 1, "Hi", role::kFill});
    const Raster r(c);

    // The terminal: row 1, columns 2 and 3 carry 'H' and 'i'.
    CHECK(tui_row(c, 0) == "      ");
    CHECK(tui_row(c, 1) == "  Hi  ");
    // This medium: the same two cells carry ink, and no others.
    for (std::int64_t x = 0; x < 6; ++x) {
        CAPTURE(x);
        CHECK((r.ink_in_cell(x, 1) > 0) == (x == 2 || x == 3));
        CHECK(r.ink_in_cell(x, 0) == 0);
    }
}

// ============================================================================
// Tier 3 — the shell through a real bus
// ============================================================================

TEST_CASE("the shell says hello exactly once, and delegates every intent") {
    loom::Switchboard bus;
    std::vector<std::string> log;
    int hellos = 0;
    const loom::WeaveId skin = loom::mount<SkinT<FakeMedium>>(bus, FakeMedium{&log});
    (void)loom::mount<ReadyEars>(bus, hellos);

    // Nothing accepts a slot nobody holds: publishing into silence is legal.
    CHECK(bus.publish(loom::Message(loom::to_value(zengine::snake::FoodEaten{}))) == 0);

    bus.send(skin, loom::Message(loom::to_value(small_visual())));
    bus.pump();
    REQUIRE(log.size() == 1);
    CHECK(log[0] == "frame w=4 first=1");
    CHECK(hellos == 1);

    bus.send(skin, loom::Message(loom::to_value(SurfaceText{"status", "up"})));
    bus.send(skin, loom::Message(loom::to_value(small_visual())));
    bus.pump();
    REQUIRE(log.size() == 3);
    CHECK(log[1] == "note status=up");
    CHECK(log[2] == "frame w=4 first=0");
    CHECK(hellos == 1); // once per incarnation, not per message
}

TEST_CASE("a canvas is a frame: same hello, same first-flag, same counter") {
    loom::Switchboard bus;
    std::vector<std::string> log;
    int hellos = 0;
    const loom::WeaveId skin = loom::mount<SkinT<FakeMedium>>(bus, FakeMedium{&log});
    (void)loom::mount<ReadyEars>(bus, hellos);

    SurfaceCanvas c;
    c.width = 8;
    c.height = 2;
    c.rects.push_back(SurfaceRect{0, 0, 2, 2, role::kFill});
    c.labels.push_back(SurfaceLabel{3, 0, "hi", role::kAccent});

    bus.send(skin, loom::Message(loom::to_value(c)));
    bus.pump();
    REQUIRE(log.size() == 1);
    CHECK(log[0] == "canvas w=8 rects=1 labels=1 first=1");
    CHECK(hellos == 1); // a canvas claims the surface exactly as a board does

    // The SAME counter: a second painted thing is not a first one, whichever
    // intent produced it. This is what makes `frames == 0` a usable
    // claim-the-surface signal for a medium that sees both.
    bus.send(skin, loom::Message(loom::to_value(small_visual())));
    bus.send(skin, loom::Message(loom::to_value(c)));
    bus.pump();
    REQUIRE(log.size() == 3);
    CHECK(log[1] == "frame w=4 first=0");
    CHECK(log[2] == "canvas w=8 rects=1 labels=1 first=0");
    CHECK(hellos == 1);
}

TEST_CASE("the pump is execution time: serviced, counted, and an honest first hello") {
    loom::Switchboard bus;
    std::vector<std::string> log;
    int hellos = 0;
    const loom::WeaveId skin = loom::mount<SkinT<FakeMedium>>(bus, FakeMedium{&log});
    (void)loom::mount<ReadyEars>(bus, hellos);

    bus.send(skin, loom::Message(loom::to_value(PumpSurface{})));
    bus.pump();
    REQUIRE(log.size() == 1);
    CHECK(log[0] == "pump");
    // On a pumped host the pump is the skin's earliest first message, so the
    // hello — and the text rows it re-summons — no longer waits for a frame.
    CHECK(hellos == 1);

    bus.send(skin, loom::Message(loom::to_value(PumpSurface{})));
    bus.pump();
    CHECK(log.size() == 2);
    CHECK(log[1] == "pump");
    CHECK(hellos == 1); // once per incarnation, still
}

TEST_CASE("announcing and asking are SEPARATE: the skin retries its beat on TimerReady") {
    // THE BUG THIS EXISTS FOR (R2A-2). These two used to share `hello_once`, so
    // a skin whose ask went nowhere (rejected at the library/schema seam, since
    // with no Timer present nobody accepts StartRoleTimer and the shape is
    // never registered) could never retry: the hello was already spent, and
    // every later trigger returned early from it. A skin loaded before the
    // Timer would have been serviced by nothing, forever, on a bus that was
    // working perfectly. Announcing is ONCE; asking must stay REPEATABLE.
    loom::Switchboard bus;
    std::vector<std::string> log;
    int hellos = 0;
    const loom::WeaveId skin = loom::mount<SkinT<FakeMedium>>(bus, FakeMedium{&log});
    const loom::WeaveId door = zengine::testing::mount_door(bus);
    (void)loom::mount<ReadyEars>(bus, hellos);
    std::vector<BeatAsk> asks;
    (void)mount_into_role<BeatCatcher>(bus, zengine::timer::kTimerRole, asks);

    // An ordinary message ANNOUNCES and does not ask. (Before the split it did
    // both, which is what made the ask unrepeatable.)
    bus.send(skin, loom::Message(loom::to_value(SurfaceText{"status", "up"})));
    bus.pump();
    CHECK(hellos == 1);
    CHECK(asks.empty());

    // The skin's own activation is its first breath: announce (already done)
    // AND ask. The ask's wire content is pinned field by field.
    zengine::testing::order_activation(bus, door, skin, 1);
    bus.pump();
    CHECK(hellos == 1); // still once per incarnation
    REQUIRE(asks.size() == 1);
    CHECK(asks[0].id == kPumpTimerId);
    CHECK(asks[0].delay_ms == kPumpBeatMs);
    CHECK(asks[0].repeat);
    CHECK(asks[0].role == kSkinRole);

    // THE RETRY. TimerReady asks AGAIN — the path that rescues a skin whose
    // activation-time ask found no timer. It is an upsert on the service's
    // side, so this never doubles the beat.
    bus.send(skin, loom::Message(loom::to_value(zengine::timer::TimerReady{})));
    bus.pump();
    REQUIRE(asks.size() == 2);
    CHECK(asks[1].id == kPumpTimerId);
    CHECK(asks[1].role == kSkinRole);
    CHECK(hellos == 1); // and the retry is not a second hello

    // A duplicate activation does nothing at all — the cursor's whole job.
    zengine::testing::order_activation(bus, door, skin, 1);
    bus.pump();
    CHECK(asks.size() == 2);

    // The beat is the pump's twin: serviced and counted the same...
    bus.send(skin, loom::Message(loom::to_value(zengine::timer::TimerFired{kPumpTimerId})));
    bus.pump();
    REQUIRE(log.size() == 2);
    CHECK(log[0] == "note status=up");
    CHECK(log[1] == "pump");

    // ...and an alien id aimed at this role is data, not a drive.
    bus.send(skin, loom::Message(loom::to_value(zengine::timer::TimerFired{"someone.else"})));
    bus.pump();
    CHECK(log.size() == 2);
    CHECK(hellos == 1);
    CHECK(asks.size() == 2);
}

// ============================================================================
// Tier 4 — the real libraries through the real kernel
// ============================================================================

TEST_CASE("a skin .so claims the role and paints the world's real intent") {
    Rig r;
    r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);
    const loom::WeaveId skin = r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC,
                                      kSkinRole);

    r.tick(); // the world seeds and publishes; the skin paints
    CHECK(r.poke(skin, loom::PokeRead{"frames"}).text == "1");
    CHECK(r.poke(skin, loom::PokeRead{"texts"}).text == "0");

    r.intent(skin, SurfaceText{kSlotStatus, "operator says hi"});
    CHECK(r.poke(skin, loom::PokeRead{"texts"}).text == "1");

    // The host's heartbeat reaches the role-holder like any other drive.
    r.bus.send_to_role(kSkinRole, loom::Message(loom::to_value(PumpSurface{})));
    r.pump();
    CHECK(r.poke(skin, loom::PokeRead{"pumps"}).text == "1");
}

TEST_CASE("the tally survives the painter being replaced (the hello handshake)") {
    Rig r;
    r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);
    r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, kSkinRole);
    const loom::WeaveId score = r.load("snake-score", SCORE_SO, "");

    // The score weave witnesses one meal and speaks its tally once.
    r.intent(score, zengine::snake::FoodEaten{});
    CHECK(r.poke(score, loom::PokeRead{"eaten"}).text == "1");

    // Replace the painter mid-game: different code takes the surface.
    const Recorded::Answer swapped = r.command(loom::SwapWeave{
        kSkinRole, "zengine-skin-tui-block", SKIN_SO_TUI_BLOCK, /*graceful=*/false});
    REQUIRE(swapped.kind == 0);
    const loom::WeaveId block{static_cast<std::uint64_t>(std::stoll(swapped.text))};
    CHECK(r.poke(block, loom::PokeRead{"frames"}).text == "0"); // nothing painted yet

    // AND THE TALLY IS ALREADY THERE (R2A-2). The successor does not have to
    // wait for a frame to wake it: its own activation is its first breath, so
    // it announced at load, the score weave heard the hello and re-published,
    // and the line was on screen before anything was drawn. Under the old
    // mechanism this read 0 here and only became 1 after the next tick.
    CHECK(r.poke(block, loom::PokeRead{"texts"}).text == "1");

    // The next real frame paints — and the tally is still one meal, because no
    // meal happened; the line survived the painter, not the score.
    r.tick();
    CHECK(r.poke(block, loom::PokeRead{"frames"}).text == "1");
    CHECK(r.poke(block, loom::PokeRead{"texts"}).text == "1");
    CHECK(r.poke(score, loom::PokeRead{"eaten"}).text == "1"); // no meal happened
}

namespace {

/// A stand-in for the host's operator: a restrictively-granted weave that
/// PUBLISHES a status line from inside a handler — the exact gesture
/// play.cpp's operator makes (including re-speaking on a skin's hello).
struct SpeakerState {
    std::int64_t noop = 0;
    ZEN_SHAPE(SpeakerState, 1, ZEN_FIELD(noop));
};

class Speaker : public loom::WeaveBase<Speaker, SpeakerState, loom::Accept<SurfaceReady>,
                                       loom::Emit<SurfaceText>> {
public:
    explicit Speaker(int& spoke) : spoke_(&spoke) {}
    void on(const SurfaceReady&, loom::Mail& mail) {
        ++(*spoke_);
        mail.publish(SurfaceText{kSlotStatus, "spoken"});
    }

private:
    int* spoke_;
};

} // namespace

TEST_CASE("a granted operator can speak to the surface (the host's grant recipe)") {
    // The host's operator holds a RESTRICTIVE grant (manager commands,
    // target-scoped) — and publishes its status line. A publish is authorized
    // per-recipient against the SENDER's grant, so without an explicit
    // any-target SurfaceText rule the row dies silently while everything else
    // hums (found live, in the pty run — the suite's other publishers are
    // loaded .so's whose permissive grant hid the path). This pins the recipe
    // play.cpp relies on: the restricted reach PLUS the one speaking rule,
    // with the rule-less twin as the negative that makes it a proof.
    Rig r;
    const loom::WeaveId skin = r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC,
                                      kSkinRole);

    int spoke = 0;
    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, r.manager);
    reach.allow_to_any(SurfaceText::zen_name, SurfaceText::zen_version);
    const loom::WeaveId speaking = loom::mount_granted<Speaker>(r.bus, std::move(reach), spoke);
    r.bus.send(speaking, loom::Message(loom::to_value(SurfaceReady{})));
    r.pump();
    // The line lands. It lands ONCE, not twice: since R2A-2 the skin said its
    // hello at its own activation, back when it was loaded — so this text is
    // not its first message and does not trigger a second hello, and the
    // speaker has nothing to re-speak to. (Under the old mechanism this read 2
    // for both, because the skin's hello was still pending here.) The property
    // under test is unchanged and is the whole point: the line lands at all,
    // because the grant carries an any-target SurfaceText rule.
    CHECK(spoke == 1);
    CHECK(r.poke(skin, loom::PokeRead{"texts"}).text == "1");

    int mute_spoke = 0;
    loom::Grant mute;
    mute.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, r.manager);
    const loom::WeaveId muted = loom::mount_granted<Speaker>(r.bus, std::move(mute), mute_spoke);
    r.bus.send(muted, loom::Message(loom::to_value(SurfaceReady{})));
    r.pump();
    CHECK(mute_spoke == 1);                                   // it DID speak...
    CHECK(r.poke(skin, loom::PokeRead{"texts"}).text == "1"); // ...into a closed door
}

TEST_CASE("the surface has one owner: a second skin is refused the held role") {
    Rig r;
    r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, kSkinRole);

    const Recorded::Answer refused =
        r.command(loom::LoadWeave{"zengine-skin-tui-block", SKIN_SO_TUI_BLOCK, kSkinRole});
    CHECK(refused.kind == 2);
    CHECK(refused.text.find("already held") != std::string::npos);

    // The incumbent kept its role and its life; the pretender is not resident.
    const Recorded::Answer listed = r.command(loom::ListLoaded{});
    CHECK(listed.text.find("zengine-skin-tui-classic@zengine.skin") != std::string::npos);
    CHECK(listed.text.find("block") == std::string::npos);
}

// The unload-reload linkage pins, riding the skin pair — the first weave
// libraries sharing TWO packages' vocabulary headers (surface + snake).
// Without -fno-gnu-unique on the weave targets, the second library's schema
// statics would silently alias the first one's DESTROYED statics through
// glibc's program-wide unique-symbol table (see the top-level CMakeLists);
// these hold the law across every load-after-unload shape the demo uses.

TEST_CASE("linkage pin: a later skin is whole when its sibling never loaded") {
    Rig r;
    r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);
    const loom::WeaveId block = r.load("zengine-skin-tui-block", SKIN_SO_TUI_BLOCK, kSkinRole);
    r.tick();
    CHECK(r.poke(block, loom::PokeRead{"frames"}).text == "1");
}

TEST_CASE("linkage pin: load-unload-load across shared vocabulary, third library resident") {
    Rig r;
    r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);
    r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, kSkinRole);
    const Recorded::Answer u = r.command(loom::SwapWeave{kSkinRole, "zengine-skin-tui-block",
                                                         SKIN_SO_TUI_BLOCK, false});
    CHECK_MESSAGE(u.kind == 0, u.text);
}

TEST_CASE("linkage pin: load-unload-load across shared vocabulary, no other library") {
    Rig r;
    r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, kSkinRole);
    const Recorded::Answer u = r.command(loom::SwapWeave{kSkinRole, "zengine-skin-tui-block",
                                                         SKIN_SO_TUI_BLOCK, false});
    CHECK_MESSAGE(u.kind == 0, u.text);
}

// ============================================================================
// Tier 4a — the terminal MODES a Skin claims, including pointer reporting (W-4)
// ============================================================================

TEST_CASE("the Skin's terminal claim includes pointer reporting, and leave undoes enter") {
    // W-4 §19: a terminal reports a pointer only if something asks it to, in
    // band, on the OUTPUT stream -- and the output stream is the Skin's. So
    // pointer reporting is claimed and released on exactly the lifetime that
    // already claims the alternate screen and the cursor, with no coordination
    // surface invented between Surface and Input. Input parses SGR reports
    // whenever they arrive; they only arrive because of these bytes.
    const std::string on = tui_enter_sequence(true);
    const std::string off = tui_leave_sequence(true);

    // What enter claims, named one at a time so a silent drop is a red.
    CHECK(on.find("\x1b[?1049h") != std::string::npos); // alternate screen
    CHECK(on.find("\x1b[?25l") != std::string::npos);   // cursor hidden
    CHECK(on.find("\x1b[2J") != std::string::npos);     // and cleared
    CHECK(on.find("\x1b[?1002h") != std::string::npos); // button-event tracking
    CHECK(on.find("\x1b[?1006h") != std::string::npos); // SGR coordinates

    // 1002, not 1003: press, release and motion WHILE A BUTTON IS HELD is
    // exactly a drag. 1003 would report every idle motion and pay for a gesture
    // nobody makes.
    CHECK(on.find("\x1b[?1003h") == std::string::npos);

    // What leave releases -- everything, in the OPPOSITE order.
    CHECK(off.find("\x1b[?1006l") != std::string::npos);
    CHECK(off.find("\x1b[?1002l") != std::string::npos);
    CHECK(off.find("\x1b[?25h") != std::string::npos);
    CHECK(off.find("\x1b[?1049l") != std::string::npos);
    CHECK(off.find("\x1b[?1006l") < off.find("\x1b[?1049l"));
    CHECK(off.find("\x1b[?1002l") < off.find("\x1b[?25h"));

    // Every mode enter sets, leave clears: `h` and `l` are the only difference,
    // so a mode claimed and never released cannot hide.
    for (const char* mode : {"1049", "25", "1002", "1006"}) {
        const std::string set = std::string("\x1b[?") + mode;
        INFO("mode ", mode);
        const bool claimed =
            on.find(set + "h") != std::string::npos || on.find(set + "l") != std::string::npos;
        const bool released =
            off.find(set + "l") != std::string::npos || off.find(set + "h") != std::string::npos;
        CHECK(claimed);
        CHECK(released);
    }

    // A backend whose pointer is NOT in band (the Win32 console hands the Input
    // weave real MOUSE_EVENT records) asks for no reporting -- asking twice for
    // one thing is not politeness.
    const std::string plain_on = tui_enter_sequence(false);
    const std::string plain_off = tui_leave_sequence(false);
    CHECK(plain_on.find("\x1b[?1002h") == std::string::npos);
    CHECK(plain_on.find("\x1b[?1006h") == std::string::npos);
    CHECK(plain_off.find("\x1b[?1002l") == std::string::npos);
    // ...and the ordinary screen and cursor restoration is untouched either way.
    CHECK(plain_on.find("\x1b[?1049h") != std::string::npos);
    CHECK(plain_off.find("\x1b[?25h") != std::string::npos);
    CHECK(plain_off.find("\x1b[?1049l") != std::string::npos);

    // HONEST LIMIT, asserted as prose because it cannot be asserted as code: a
    // process killed uncatchably restores nothing, and a terminal left in
    // reporting mode prints mouse escapes at its shell until `reset`. That is
    // the same exposure the alternate screen has always carried; W-4 adds one
    // more mode to it and does not pretend otherwise.
    CHECK(kTuiPointerOn == std::string("\x1b[?1002h\x1b[?1006h"));
    CHECK(kTuiPointerOff == std::string("\x1b[?1006l\x1b[?1002l"));
}

// ============================================================================
// Tier 4b — the SDL skin, where this lane built it (dummy video driver)
// ============================================================================

#if defined(SURFACE_HAS_SDL)

TEST_CASE("the same intent drives the SDL skin - a window medium, zero new fields") {
    // Belt to ctest's braces: the dummy driver must be chosen before the .so's
    // SDL_Init runs, and this process is the one loading it.
#if defined(_WIN32)
    ::_putenv_s("SDL_VIDEO_DRIVER", "dummy");
    ::_putenv_s("SDL_VIDEODRIVER", "dummy");
#else
    ::setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    ::setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
    Rig r;
    r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);
    const loom::WeaveId skin = r.load("zengine-skin-sdl", SKIN_SO_SDL, kSkinRole);

    r.tick();
    r.tick();
    CHECK(r.poke(skin, loom::PokeRead{"frames"}).text == "2");

    r.intent(skin, SurfaceText{kSlotScore, "eaten: 1"});
    CHECK(r.poke(skin, loom::PokeRead{"texts"}).text == "1");

    // The heartbeat that keeps a real window answering its OS even when the
    // world goes quiet — under the dummy driver the queue is real, the
    // servicing is real, only the photons are missing.
    r.bus.send_to_role(kSkinRole, loom::Message(loom::to_value(PumpSurface{})));
    r.pump();
    CHECK(r.poke(skin, loom::PokeRead{"pumps"}).text == "1");
}

#endif // SURFACE_HAS_SDL
