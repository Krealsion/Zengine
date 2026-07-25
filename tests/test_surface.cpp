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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "surface/skin.hpp"
#include "surface/skin_sdl_plan.hpp"
#include "surface/skin_tui.hpp"
#include "surface/vocabulary.hpp"

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
#include <string>
#include <string_view>
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
    CHECK(r.poke(block, loom::PokeRead{"frames"}).text == "0");
    CHECK(r.poke(block, loom::PokeRead{"texts"}).text == "0");

    // The next real frame wakes the new skin; its hello gets the tally
    // re-published — the score line is back on screen with NO new meal.
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
    // The full live round: the line lands (texts 1) — and since it was the
    // fresh skin's FIRST message, the skin hellos, the speaker re-speaks
    // (exactly what the host's operator does), and the line lands again.
    CHECK(spoke == 2);
    CHECK(r.poke(skin, loom::PokeRead{"texts"}).text == "2");

    int mute_spoke = 0;
    loom::Grant mute;
    mute.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, r.manager);
    const loom::WeaveId muted = loom::mount_granted<Speaker>(r.bus, std::move(mute), mute_spoke);
    r.bus.send(muted, loom::Message(loom::to_value(SurfaceReady{})));
    r.pump();
    CHECK(mute_spoke == 1);                                   // it DID speak...
    CHECK(r.poke(skin, loom::PokeRead{"texts"}).text == "2"); // ...into a closed door
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
