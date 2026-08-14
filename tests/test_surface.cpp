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

#include "surface/pointing.hpp"
#include "surface/region.hpp"
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
#include <limits>
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

    /// The one thing a Medium is ASKED. A fake answers whatever a case set, so the
    /// shell's own rule -- publish on change, never publish "no opinion" -- can be
    /// driven without a window.
    SurfaceExtent room{};
    SurfaceExtent extent() const { return room; }
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

/// An ordinary accepter of the one fact that travels medium -> publisher.
class RoomEars : public loom::WeaveBase<RoomEars, ReadyState, loom::Accept<SurfaceExtent>,
                                        loom::Emit<>> {
public:
    explicit RoomEars(std::vector<SurfaceExtent>& heard) : heard_(&heard) {}
    void on(const SurfaceExtent& e, loom::Mail&) { heard_->push_back(e); }

private:
    std::vector<SurfaceExtent>* heard_;
};

/// mount(), keeping the pointer, so a case can move the fake medium's answer the way a
/// person moves a window edge. `loom::mount` hands back only an id, and what these cases
/// need to reach is the MEDIUM the shell is asking.
inline SkinT<FakeMedium>* mount_fake_skin(loom::Switchboard& bus, std::vector<std::string>& log,
                                          loom::WeaveId& id) {
    auto weave = std::make_unique<SkinT<FakeMedium>>(FakeMedium{&log});
    SkinT<FakeMedium>* raw = weave.get();
    loom::Grant grant = loom::emit_default_grant(*raw);
    loom::allow_poke_answers(grant);
    id = bus.register_weave(std::move(weave), std::move(grant));
    raw->zen_set_self(id);
    return raw;
}

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

    // THE ONE SHAPE THAT TRAVELS MEDIUM -> PUBLISHER, at version 2: the room, and
    // since HD-1 the size of one character of the medium's own type. Pinned in the
    // order they are declared, because this is a wire: a medium and an application
    // in two separately-loaded libraries agree about these four numbers or they
    // agree about nothing.
    CHECK(schema_of<SurfaceExtent>()->content_id() == SchemaBuilder("SurfaceExtent", 2)
                                                          .field("width", Kind::Int)
                                                          .field("height", Kind::Int)
                                                          .field("text_advance_px", Kind::Int)
                                                          .field("text_line_px", Kind::Int)
                                                          .build()
                                                          ->content_id());

    // ZERO IS A VALUE, NOT AN ABSENCE, and the default says so: an extent built
    // with no metric means "text is a cell", which is what every terminal medium
    // means always and what a window medium means before its face opens.
    const SurfaceExtent fresh;
    CHECK(fresh.text_advance_px == 0);
    CHECK(fresh.text_line_px == 0);
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

    // HD-1's bounded text region: a row carries what it says and how loudly, and
    // NOT where it is -- a row's place is its index in its region, which is what
    // makes the region a bounded presentation rather than a second coordinate
    // system. Pinned here so that "a row gained an x" is a red in this file rather
    // than a discovery on a wire.
    //
    // VERSION 2 SINCE HD-2, for the field that made a selected row sayable. The
    // ground is the LAST field, deliberately: an existing publisher's two fields
    // are where they were, so the only thing a reader has to check is that the new
    // one is at the end.
    const auto text_row = SchemaBuilder("SurfaceTextRow", 2)
                              .field("text", Kind::Text)
                              .field("role", Kind::Int)
                              .field("background", Kind::Int)
                              .build();
    CHECK(schema_of<SurfaceTextRow>()->content_id() == text_row->content_id());

    // AND THE TWO SHAPES THAT CARRY IT MOVED WITH IT, which is the whole reason
    // this case builds them by hand out of each other rather than checking three
    // independent spellings. A region's identity is computed from its field TYPES,
    // one of which is a list of the row above; the canvas's from a list of
    // regions. So a row gaining a field changed all three content-ids, and all
    // three declared versions had to follow -- a version that did not would be two
    // different wire shapes wearing one number.
    const auto text_region = SchemaBuilder("SurfaceTextRegion", 2)
                                 .field("x", Kind::Int)
                                 .field("y", Kind::Int)
                                 .field("w", Kind::Int)
                                 .field("h", Kind::Int)
                                 .list("rows", loom::type_message(text_row))
                                 .build();
    CHECK(schema_of<SurfaceTextRegion>()->content_id() == text_region->content_id());

    // The canvas carries lists of the three above, so its identity depends on
    // theirs -- which is the property that makes a drift anywhere in this
    // vocabulary a red here rather than a surprise on a wire.
    //
    // VERSION 3, because a published schema is immutable and evolving a shape is
    // publishing a new one. Version 2 was the region list it gained in HD-1; this
    // one it gained no field of its own at all, and changed anyway.
    const auto canvas = SchemaBuilder("SurfaceCanvas", 3)
                            .field("width", Kind::Int)
                            .field("height", Kind::Int)
                            .list("rects", loom::type_message(rect))
                            .list("labels", loom::type_message(label))
                            .list("texts", loom::type_message(text_region))
                            .build();
    CHECK(schema_of<SurfaceCanvas>()->content_id() == canvas->content_id());

    // A ROW WITH NO GROUND IS THE DEFAULT, and `role::kNone` is not one of the four
    // roles -- it is negative precisely so that a later vocabulary adding a fifth
    // role cannot collide with it, and so that the unknown-role fallback (kFill)
    // can never swallow it silently.
    const SurfaceTextRow plain;
    CHECK(plain.background == role::kNone);
    CHECK(role::kNone < 0);
    CHECK(role::kNone != role::kFill);
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

TEST_CASE("golden: a text region rasterizes to cells, over everything, bounded") {
    // HD-1's honesty proof as bytes. A terminal owns no font, so a bounded text
    // region IS cells here -- one row per cell row, cut at the region's width,
    // dropped past its height -- and this is the projection every character medium
    // performs. It is deliberately the same arithmetic `paint_terminal` used to
    // perform for itself before regions existed.
    SurfaceCanvas c;
    c.width = 6;
    c.height = 4;
    c.rects.push_back(SurfaceRect{0, 0, 6, 4, role::kMuted});
    // A label UNDER the region, in the cells the region owns: the region wins,
    // because a region is a grant of bounds and the topmost thing on a canvas.
    c.labels.push_back(SurfaceLabel{1, 1, "ZZZZ", role::kAlert});

    SurfaceTextRegion r;
    r.x = 1;
    r.y = 1;
    r.w = 4;
    r.h = 2;
    r.rows.push_back(SurfaceTextRow{"ab", role::kAccent});
    r.rows.push_back(SurfaceTextRow{"toolong", role::kFill});
    r.rows.push_back(SurfaceTextRow{"never", role::kAlert}); // past the region's height
    c.texts.push_back(r);

    TuiMedium<ClassicStyle, StringSink> m;
    m.canvas(c, /*first=*/true);
    CHECK(m.sink().out ==
          "\x1b[3;1H\x1b[0J"
          "\x1b[2K\x1b[90m......\x1b[0m\r\n"
          // `ab  ` is ONE run: the padding a row is widened with carries that row's
          // own role, exactly as `pad(fit(...))` did before regions existed.
          "\x1b[2K\x1b[90m.\x1b[36mab  \x1b[90m.\x1b[0m\r\n"
          "\x1b[2K\x1b[90m.\x1b[37mtool\x1b[90m.\x1b[0m\r\n"
          "\x1b[2K\x1b[90m......\x1b[0m\r\n");

    // AND THE SAME CANVAS THROUGH THE GRAPHICAL MEDIUM'S BITMAP FACE, which is the
    // other half of "the fallback is the cell projection": with no metric the SDL
    // plan draws exactly these labels, so a window with no font shows the picture
    // a terminal shows.
    SurfaceCanvas same_as_labels;
    same_as_labels.width = c.width;
    same_as_labels.height = c.height;
    same_as_labels.rects = c.rects;
    same_as_labels.labels = c.labels;
    for (const ProjectedRow& p : project_text_regions(c)) {
        same_as_labels.labels.push_back(p.label);
    }
    CHECK(plan_canvas(c) == plan_canvas(same_as_labels));
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
// Tier 2c — the SDL canvas plan: labels reach pixels
// ============================================================================
//
// The whole label path is here rather than behind SURFACE_HAS_SDL on purpose.
// `plan_canvas` is the only place that knows a label from a rect -- the SDL edge
// receives one flat quad list -- so these cases are what stands between the
// medium and "rectangles drawn, labels dropped", and they must run on every
// lane, including the Windows stranger lane that builds no SDL at all.
//
// The first case belongs to BOTH media: the terminal Skin's clipping was found
// unsafe while this one's was written, so the guard is shared and so is its
// proof.

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
    // FOUND IN COMMITTED CODE, in the TERMINAL Skin, and both halves were
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

    // THE LABELS-REACH-PIXELS CASE. Ink exists where the label was published and nowhere else on
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
    // A dropped label at character granularity is what this policy refuses: a
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

// ============================================================================
// Tier 2d — where a reported pointer lands on the canvas
// ============================================================================
//
// The pixel-to-cell rule and the terminal's canvas origin are BOTH this
// package's, because both are decided by a Skin's own layout. They are pinned
// here, on every lane, because they are pure arithmetic and because the lane
// that builds no SDL is exactly the lane most likely to be the one that breaks
// them.

TEST_CASE("pointing: a window pixel lands on the cell a maker is looking at") {
    constexpr std::int64_t kCell = kCanvasCellPx;

    // The three that decide the boundary policy, stated in terms of the CELL
    // SIZE rather than of 12 -- there is one owner of that number and this test
    // consults it rather than making a second copy.
    CHECK(canvas_of_window_pixels(0, 0) == CanvasPoint{0, 0});
    CHECK(canvas_of_window_pixels(kCell - 1, kCell - 1) == CanvasPoint{0, 0});
    CHECK(canvas_of_window_pixels(kCell, kCell) == CanvasPoint{1, 1});
    CHECK(canvas_of_window_pixels(2 * kCell + kCell / 2, 5 * kCell) == CanvasPoint{2, 5});

    // OUTSIDE IS A REAL ANSWER, and it is where truncation would lie. C++
    // integer division rounds toward zero, so a plain `/` sends pixel -1 to cell
    // 0 -- one pixel left of the window reading as the window's first column,
    // which downstream becomes "the click selected the leftmost object". Floored,
    // the cell boundaries are evenly spaced across zero and -1 is cell -1.
    CHECK(canvas_of_window_pixels(-1, -1) == CanvasPoint{-1, -1});
    CHECK(canvas_of_window_pixels(-kCell, -kCell) == CanvasPoint{-1, -1});
    CHECK(canvas_of_window_pixels(-kCell - 1, -kCell - 1) == CanvasPoint{-2, -2});

    // And it is total over the whole number line: these are wire values.
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    CHECK(canvas_of_window_pixels(kMax, kMax) == CanvasPoint{kMax / kCell, kMax / kCell});
    const CanvasPoint low = canvas_of_window_pixels(kMin, kMin);
    CHECK(low.x < 0);
    CHECK(low.y < 0);
}

TEST_CASE("pointing: a terminal cell is a canvas cell, two rows up") {
    // The terminal Skins write the canvas from terminal row 3 (`\x1b[3;1H`),
    // because rows 1 and 2 are the SurfaceText slots. So the only difference
    // between a terminal position and a canvas position is those two rows -- and
    // the column is not shifted at all.
    CHECK(canvas_of_terminal_cells(0, kTuiCanvasTopRow) == CanvasPoint{0, 0});
    CHECK(canvas_of_terminal_cells(7, kTuiCanvasTopRow + 4) == CanvasPoint{7, 4});
    CHECK(canvas_of_terminal_cells(0, 0) == CanvasPoint{0, -kTuiCanvasTopRow});

    // Saturating, for the same reason every other wire-fed subtraction here is.
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    CHECK(canvas_of_terminal_cells(kMin, kMin) == CanvasPoint{kMin, kMin});
}

TEST_CASE("pointing: the two media disagree about the numbers and agree about the cell") {
    // The property that actually matters, as one assertion: whatever medium a
    // maker is looking through, pointing at canvas cell (c) reports something
    // that projects back to (c). Nothing downstream should be able to tell.
    for (std::int64_t cx = 0; cx < 5; ++cx) {
        for (std::int64_t cy = 0; cy < 5; ++cy) {
            const CanvasPoint want{cx, cy};
            CHECK(canvas_of_terminal_cells(cx, cy + kTuiCanvasTopRow) == want);
            // every pixel of that cell, not merely its corner
            for (std::int64_t p = 0; p < kCanvasCellPx; ++p) {
                CHECK(canvas_of_window_pixels(cx * kCanvasCellPx + p,
                                              cy * kCanvasCellPx + p) == want);
            }
        }
    }
}

// ============================================================================
// Tier 2b — bounded regions: one resolution, two consumers (HD-1)
// ============================================================================
//
// EVERYTHING IN THIS BLOCK IS PURE, AND THAT PLACEMENT IS THE CLAIM. The lane
// that builds no SDL at all -- the Windows stranger lane -- still proves how much
// prose a region holds, where its pixels are, where its interior starts, what a
// pointer lands on inside it and what a character medium makes of it. Only the
// rasterization needs a font, and the rasterization is the part that cannot be
// wrong about anything except plumbing.

TEST_CASE("region: with no text metric a region is exactly its own cells") {
    // THE HONESTY HINGE OF THE WHOLE PHASE. A medium that publishes no metric has
    // said "text is a cell", and this is what that sentence resolves to: the
    // region's own bounds, no inset, no pixel arithmetic anybody has to trust.
    // Every terminal Skin lives here, and so does the graphical one before its
    // font opens and after a font has failed to open.
    const RegionFit f = fit_region(3, 4, 20, 6, 0, 0);
    CHECK(f.columns == 20);
    CHECK(f.rows == 6);
    CHECK(f.origin_x == 0);
    CHECK(f.origin_y == 0);
    CHECK_FALSE(f.graphical());
    CHECK(f.view == RegionViewport{3 * kCanvasCellPx, 4 * kCanvasCellPx, 20 * kCanvasCellPx,
                                   6 * kCanvasCellPx});

    // A metric is two numbers or it is none: half an answer describes half a line
    // of text, so either missing half means cells.
    CHECK_FALSE(fit_region(0, 0, 20, 6, 8, 0).graphical());
    CHECK_FALSE(fit_region(0, 0, 20, 6, 0, 18).graphical());
    CHECK(fit_region(0, 0, 20, 6, 8, 0).columns == 20);
    CHECK(fit_region(0, 0, 20, 6, 0, 18).rows == 6);

    // A negative advance is not a size. It is a number off the wire, and it means
    // the same thing zero does rather than meaning an error.
    CHECK_FALSE(fit_region(0, 0, 20, 6, -8, -18).graphical());
    CHECK(fit_region(0, 0, 20, 6, -8, -18).columns == 20);
}

TEST_CASE("region: a real metric divides the region's pixels, inset and all") {
    // The minimum Terminal pane, at the face HD-1 ships: 56x13 cells is 672x156
    // device pixels, and an 8px advance with an 18px line divides what is left
    // after the inset comes off BOTH sides.
    const RegionFit f = fit_region(22, 9, 56, 13, 8, 18);
    REQUIRE(f.graphical());
    CHECK(f.view == RegionViewport{22 * 12, 9 * 12, 672, 156});
    CHECK(f.origin_x == kTextInsetPx);
    CHECK(f.origin_y == kTextInsetPx);
    CHECK(f.columns == (672 - 2 * kTextInsetPx) / 8);
    CHECK(f.rows == (156 - 2 * kTextInsetPx) / 18);
    CHECK(f.advance_px == 8);
    CHECK(f.line_px == 18);

    // THE INSET IS IN THE CAPACITY, WHICH IS THE WHOLE POINT OF IT LIVING HERE.
    // Padding a renderer consumed privately would leave a publisher wrapping
    // against 84 columns and a medium drawing 83 of them, and the difference
    // would appear as the last character of every long row falling off the edge.
    CHECK(f.columns == 83);
    CHECK(f.columns < 672 / 8);
}

TEST_CASE("region: a metric off the wire cannot make the arithmetic misbehave") {
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();

    // A line taller than the region, or a character wider than it: nothing fits,
    // said as zero rather than as a negative somebody downstream would subtract.
    CHECK(fit_region(0, 0, 4, 2, 8, 4000).rows == 0);
    CHECK(fit_region(0, 0, 4, 2, 4000, 18).columns == 0);
    CHECK(fit_region(0, 0, 4, 2, kMax, kMax).columns == 0);
    CHECK(fit_region(0, 0, 4, 2, kMax, kMax).rows == 0);

    // A region with no bounds, or bounds off the number line: still an answer.
    CHECK(fit_region(0, 0, -5, -5, 8, 18).columns == 0);
    CHECK(fit_region(0, 0, -5, -5, 0, 0).columns == 0);
    CHECK(fit_region(kMax, kMax, kMax, kMax, 8, 18).view.w > 0);
    CHECK(fit_region(kMin, kMin, kMin, kMin, 8, 18).view.w == 0);

    // The inset can be larger than the region itself. Two cells is 24 pixels and
    // survives; a region so small that the inset eats it answers zero, not a
    // negative width.
    CHECK(fit_region(0, 0, 1, 1, 8, 18).columns == (12 - 2 * kTextInsetPx) / 8);
    CHECK(fit_region(0, 0, 1, 1, 8, 18).rows == 0);
}

TEST_CASE("region: the clip is the surface's business and never the capacity's") {
    // A region wholly on the surface is untouched.
    const RegionViewport whole{100, 50, 200, 80};
    CHECK(clip_viewport(whole, 936, 264) == whole);

    // Hanging off the right and the bottom: less is painted.
    CHECK(clip_viewport(whole, 200, 100) == RegionViewport{100, 50, 100, 50});

    // Starting above and to the left: the clip moves, and a medium is expected to
    // carry the difference in its LOCAL origin rather than in the capacity.
    CHECK(clip_viewport(RegionViewport{-30, -10, 100, 40}, 936, 264) ==
          RegionViewport{0, 0, 70, 30});

    // Entirely off: an empty clip, and `empty()` says so rather than a caller
    // having to compare two numbers.
    CHECK(clip_viewport(RegionViewport{2000, 0, 100, 40}, 936, 264).empty());
    CHECK(clip_viewport(RegionViewport{0, 2000, 100, 40}, 936, 264).empty());

    // AND THE CAPACITY IS UNMOVED BY ANY OF IT. This is the separation that keeps
    // the omission marker true: a window two pixels too small paints less and the
    // pane still says the same thing about what it is showing, because what it is
    // showing was decided from authored bounds.
    const RegionFit f = fit_region(22, 9, 56, 13, 8, 18);
    const RegionViewport squeezed = clip_viewport(f.view, 400, 200);
    CHECK(squeezed.w == 400 - 264);
    CHECK(squeezed.h == 200 - 108);
    CHECK(squeezed.w < f.view.w);
    CHECK(fit_region(22, 9, 56, 13, 8, 18).columns == f.columns);
}

TEST_CASE("region: the cell projection is what the pane used to do, exactly") {
    SurfaceCanvas c;
    c.width = 40;
    c.height = 10;
    SurfaceTextRegion r;
    r.x = 2;
    r.y = 3;
    r.w = 8;
    r.h = 4;
    r.rows.push_back(SurfaceTextRow{"abc", role::kAccent});
    r.rows.push_back(SurfaceTextRow{"a much longer row than fits", role::kAlert});
    c.texts.push_back(r);

    const std::vector<ProjectedRow> projected = project_text_regions(c);
    REQUIRE(projected.size() == 4); // EVERY cell row of the region, including the empty ones
    std::vector<SurfaceLabel> out;
    for (const ProjectedRow& p : projected) {
        CHECK(p.background == role::kNone); // no row here asked for a ground
        out.push_back(p.label);
    }

    // Padded to the region's width -- which is what CLEARS the furniture underneath
    // in a medium whose ink is one character per cell, and which is the job
    // `paint_terminal` used to do for itself.
    CHECK(out[0].text == "abc     ");
    CHECK(out[0].x == 2);
    CHECK(out[0].y == 3);
    CHECK(out[0].role == role::kAccent);

    // Cut at the region's width, on a byte boundary: one cell per byte, as ever.
    CHECK(out[1].text == "a much l");
    CHECK(out[1].role == role::kAlert);

    // The rows with nothing behind them are still written, still the full width,
    // and quiet -- a short session must not render as an overlay with holes
    // punched through it into the workspace behind.
    CHECK(out[2].text == "        ");
    CHECK(out[3].text == "        ");
    CHECK(out[3].y == 6);
    CHECK(out[2].role == role::kFill);

    // More rows than the region is tall: the extra ones are simply not there.
    c.texts[0].rows.resize(99, SurfaceTextRow{"x", role::kFill});
    CHECK(project_text_regions(c).size() == 4);

    // A region with no bounds projects nothing, and says nothing about it.
    c.texts[0].w = 0;
    CHECK(project_text_regions(c).empty());
}

TEST_CASE("region plan: a region resolves to a viewport, a local origin and rows") {
    SurfaceCanvas c;
    c.width = 78;
    c.height = 22;
    SurfaceTextRegion r;
    r.x = 22;
    r.y = 9;
    r.w = 56;
    r.h = 13;
    r.rows.push_back(SurfaceTextRow{"TERMINAL", role::kAccent});
    r.rows.push_back(SurfaceTextRow{"", role::kFill});
    r.rows.push_back(SurfaceTextRow{"> _", role::kAccent});
    c.texts.push_back(r);

    const SurfaceExtent metric{78, 22, 8, 18};
    const std::vector<PlanTextRegion> plan = plan_text_regions(c, metric, PlanSize{936, 264});
    REQUIRE(plan.size() == 1);
    CHECK(plan[0].view == RegionViewport{264, 108, 672, 156});
    CHECK(plan[0].origin_x == kTextInsetPx);
    CHECK(plan[0].origin_y == kTextInsetPx);
    CHECK(plan[0].line_px == 18);
    REQUIRE(plan[0].rows.size() == 3);
    CHECK(plan[0].rows[0].text == "TERMINAL");
    CHECK(plan[0].rows[0].ink == ink_for_role(role::kAccent));
    CHECK(plan[0].rows[2].ink == ink_for_role(role::kAccent));

    // WITH NO METRIC THERE IS NO GRAPHICAL REGION AT ALL, and `plan_canvas` has the
    // words instead. One list or the other; never both, so the same sentence can
    // never be painted twice at two sizes.
    CHECK(plan_text_regions(c, SurfaceExtent{78, 22, 0, 0}, PlanSize{936, 264}).empty());
    CHECK(plan_canvas(c, SurfaceExtent{78, 22, 8, 18}).size() ==
          plan_canvas(SurfaceCanvas{c.width, c.height, c.rects, c.labels, {}}).size());
}

TEST_CASE("region plan: the plan bounds its own work, and carries the clip in its origin") {
    SurfaceCanvas c;
    c.width = 78;
    c.height = 22;
    SurfaceTextRegion r;
    r.x = 22;
    r.y = 9;
    r.w = 56;
    r.h = 13;
    r.rows.resize(400, SurfaceTextRow{std::string(4000, 'x'), role::kFill});
    c.texts.push_back(r);

    const SurfaceExtent metric{78, 22, 8, 18};
    const std::vector<PlanTextRegion> plan = plan_text_regions(c, metric, PlanSize{936, 264});
    REQUIRE(plan.size() == 1);
    // NEVER MORE ROWS THAN THE FIT SAID, and never a longer row than fits either.
    // A publisher that oversends cannot make this medium draw what it claimed it
    // could not show, and a published row of four thousand characters costs four
    // thousand characters of nothing rather than four thousand characters of font
    // engine.
    const RegionFit fit = fit_region(r, metric);
    CHECK(static_cast<std::int64_t>(plan[0].rows.size()) == fit.rows);
    CHECK(static_cast<std::int64_t>(plan[0].rows[0].text.size()) == fit.columns);

    // A region hanging off the top-left: the CLIP moved and the text did not, which
    // is what the local origin going negative means.
    c.texts[0].x = -1;
    c.texts[0].y = -1;
    const std::vector<PlanTextRegion> off = plan_text_regions(c, metric, PlanSize{936, 264});
    REQUIRE(off.size() == 1);
    CHECK(off[0].view.x == 0);
    CHECK(off[0].view.y == 0);
    CHECK(off[0].origin_x == kTextInsetPx - kCanvasCellPx);
    CHECK(off[0].origin_y == kTextInsetPx - kCanvasCellPx);

    // A region entirely off the surface is not in the plan at all.
    c.texts[0].x = 9000;
    c.texts[0].y = 9000;
    CHECK(plan_text_regions(c, metric, PlanSize{936, 264}).empty());
}

// ============================================================================
// HD-2 — a row may sit on something
// ============================================================================

TEST_CASE("region: a row's ground travels the cell projection unresolved") {
    // THE PROJECTION DOES NOT DECIDE WHAT A GROUND LOOKS LIKE, and that is the whole
    // reason it hands the role out beside the label rather than folding it in: what a
    // medium makes of a ground is the medium's own answer, and the two media that consume
    // this projection answer completely differently.
    SurfaceCanvas c;
    c.width = 40;
    c.height = 10;
    SurfaceTextRegion r;
    r.x = 2;
    r.y = 3;
    r.w = 6;
    r.h = 3;
    r.rows.push_back(SurfaceTextRow{"a", role::kFill, role::kNone});
    r.rows.push_back(SurfaceTextRow{"b", role::kAccent, role::kMuted});
    c.texts.push_back(r);

    const std::vector<ProjectedRow> out = project_text_regions(c);
    REQUIRE(out.size() == 3);
    CHECK(out[0].background == role::kNone);
    CHECK(out[1].background == role::kMuted);
    CHECK(out[1].label.role == role::kAccent);
    // A ROW WITH NOTHING BEHIND IT ASKS FOR NOTHING. The region's blank rows are the
    // region's own emptiness, not a row that was selected and forgot to say so.
    CHECK(out[2].background == role::kNone);
    // ...and the ground does not change what a row SAYS: still padded to the width.
    CHECK(out[1].label.text == "b     ");
}

TEST_CASE("region plan: a ground resolves against the region it is in, once") {
    // ON THE WIRE `kNone` IS AN ABSENCE; IN A PLAN IT IS A COLOUR. By the time a medium
    // is drawing there is no such thing as a strip with nothing behind it, so the plan
    // resolves the absence to the region's own ground -- which is exactly what lets the
    // renderer tell "selected" from "ordinary" by comparing two inks it already has.
    SurfaceCanvas c;
    c.width = 80;
    c.height = 24;
    SurfaceTextRegion r;
    r.x = 2;
    r.y = 2;
    r.w = 20;
    r.h = 4;
    r.rows.push_back(SurfaceTextRow{"plain", role::kFill, role::kNone});
    r.rows.push_back(SurfaceTextRow{"chosen", role::kAccent, role::kMuted});
    c.texts.push_back(r);

    const std::vector<PlanTextRegion> plan =
        plan_text_regions(c, SurfaceExtent{80, 24, 8, 18}, PlanSize{960, 288});
    REQUIRE(plan.size() == 1);
    REQUIRE(plan[0].rows.size() == 2);
    CHECK(plan[0].rows[0].background == plan[0].background); // the region's own ground
    CHECK(plan[0].rows[1].background == ink_for_role(role::kMuted));
    CHECK(plan[0].rows[1].ink == ink_for_role(role::kAccent));
    // The comparison the renderer makes is the one that decides whether to fill a strip.
    CHECK(plan[0].rows[0].background == kCanvasBackground);
    CHECK_FALSE(plan[0].rows[1].background == plan[0].background);
}

TEST_CASE("canvas plan: the bitmap face paints a ground as the cell's own quad") {
    // A LABEL CELL WAS ALREADY CLEARED BEFORE ITS GLYPH WAS DRAWN, so a ground is that
    // same clear in a different ink -- no new pass, no new shape, and the fallback face
    // keeps drawing the same picture the terminal does.
    SurfaceCanvas c;
    c.width = 10;
    c.height = 4;
    SurfaceTextRegion r;
    r.x = 0;
    r.y = 0;
    r.w = 3;
    r.h = 2;
    r.rows.push_back(SurfaceTextRow{"ab", role::kFill, role::kMuted});
    r.rows.push_back(SurfaceTextRow{"cd", role::kFill, role::kNone});
    c.texts.push_back(r);

    const std::vector<PlanRect> quads = plan_canvas(c);
    const PlanInk muted = ink_for_role(role::kMuted);
    const auto quad_at = [&](std::int64_t x, std::int64_t y) {
        for (const PlanRect& q : quads) {
            if (q.x == x && q.y == y && q.w == kCanvasCellPx && q.h == kCanvasCellPx) {
                return PlanInk{q.r, q.g, q.b};
            }
        }
        return PlanInk{1, 2, 3}; // no whole-cell quad here at all
    };
    // Every cell of the grounded row wears it -- including the one padded to the region's
    // width, which is what makes a selection bar a BAR rather than the length of its text.
    CHECK(quad_at(0, 0) == muted);
    CHECK(quad_at(kCanvasCellPx, 0) == muted);
    CHECK(quad_at(2 * kCanvasCellPx, 0) == muted);
    // ...and the row that asked for nothing is the canvas background, exactly as before.
    CHECK(quad_at(0, kCanvasCellPx) == kCanvasBackground);
}

TEST_CASE("golden: the terminal medium says a ground in SGR, once, and puts it back") {
    // THE CHARACTER MEDIUM'S HONEST ANSWER. One attribute per cell became two, and the
    // second is emitted only where a row asked for one -- which is what makes this
    // addition invisible to every canvas that does not use it.
    SurfaceCanvas c;
    c.width = 6;
    c.height = 3;
    SurfaceTextRegion r;
    r.x = 0;
    r.y = 0;
    r.w = 4;
    r.h = 2;
    r.rows.push_back(SurfaceTextRow{"ab", role::kAccent, role::kMuted});
    r.rows.push_back(SurfaceTextRow{"cd", role::kFill, role::kNone});
    c.texts.push_back(r);

    const std::string body = canvas_body(c);
    // The selected row: its ink, then its ground, then the characters -- and the ground
    // is stated once for the whole run rather than once per cell.
    CHECK(body.find("\x1b[36m\x1b[100mab") != std::string::npos);
    std::size_t grounds = 0;
    for (std::size_t at = body.find("\x1b[100m"); at != std::string::npos;
         at = body.find("\x1b[100m", at + 1)) {
        ++grounds;
    }
    CHECK(grounds == 1);
    // AND IT IS PUT BACK. `\x1b[0m` at the end of the row is all-attributes, which is what
    // stops the bar bleeding into the row underneath.
    CHECK(body.find("\x1b[100mab  \x1b[0m") != std::string::npos);

    // THE SAME CANVAS WITH NO GROUND EMITS NOT ONE BACKGROUND BYTE. This is the assertion
    // that says the third grid costs nothing to a picture that does not use it -- and the
    // whole-screen goldens elsewhere in this file are the same claim at scale.
    c.texts[0].rows[0].background = role::kNone;
    const std::string plain = canvas_body(c);
    CHECK(plain.find("\x1b[100m") == std::string::npos);
    CHECK(plain.find("\x1b[49m") == std::string::npos);
    CHECK(plain.find("\x1b[4") == std::string::npos); // no background SGR of any colour
}

TEST_CASE("pointing: a pixel inside a region lands on a prose column and row") {
    // The same floored division `cell_of_pixel` performs, one step finer, and
    // resolved with the fit the rows were DRAWN with rather than with a metric
    // read separately.
    const RegionFit fit = fit_region(22, 9, 56, 13, 8, 18);
    const std::int64_t x0 = 22 * kCanvasCellPx + kTextInsetPx;
    const std::int64_t y0 = 9 * kCanvasCellPx + kTextInsetPx;

    CHECK(prose_column_of_pixel(x0, 22, fit) == 0);
    CHECK(prose_column_of_pixel(x0 + 7, 22, fit) == 0);
    CHECK(prose_column_of_pixel(x0 + 8, 22, fit) == 1);
    CHECK(prose_row_of_pixel(y0, 9, fit) == 0);
    CHECK(prose_row_of_pixel(y0 + 17, 9, fit) == 0);
    CHECK(prose_row_of_pixel(y0 + 18, 9, fit) == 1);

    // FLOORED, so a pixel to the LEFT of the region's prose is column -1 and not
    // column 0. Truncating division would put a press just outside the pane onto
    // its first character.
    CHECK(prose_column_of_pixel(x0 - 1, 22, fit) == -1);
    CHECK(prose_row_of_pixel(y0 - 1, 9, fit) == -1);

    // A PROJECTION, NOT A HIT TEST: past the region is an answer, not an error.
    CHECK(prose_column_of_pixel(x0 + 8 * 500, 22, fit) == 500);
    CHECK(prose_column_of_pixel(x0 + 8 * 500, 22, fit) > fit.columns);

    // Under a cell-projection fit it degrades to the cell answer, which is the
    // truthful one for a medium whose character IS a cell.
    const RegionFit cells = fit_region(22, 9, 56, 13, 0, 0);
    CHECK(prose_column_of_pixel(22 * kCanvasCellPx + 5, 22, cells) == 0);
    CHECK(prose_column_of_pixel(23 * kCanvasCellPx, 22, cells) == 1);
    CHECK(prose_row_of_pixel(10 * kCanvasCellPx, 9, cells) == 1);

    // Total over the number line, both fits: these are wire values on both sides.
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    CHECK(prose_column_of_pixel(kMin, kMax, fit) < 0);
    CHECK(prose_column_of_pixel(kMax, kMin, fit) > 0);
    CHECK(prose_row_of_pixel(kMin, kMax, cells) < 0);
    CHECK(prose_row_of_pixel(kMax, kMin, cells) > 0);
}

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

TEST_CASE("how many whole cells a drawable has room for - floored, and total") {
    // G-2's one new piece of medium arithmetic, and it is here rather than behind the SDL
    // gate for the reason every other pure thing in this file is: the lane that builds no SDL
    // must still be able to say what a window of N pixels means in cells.

    // The plain reading, and the one a person resizing a window meets.
    CHECK(extent_of_drawable(PlanSize{936, 264}).width == 78);
    CHECK(extent_of_drawable(PlanSize{936, 264}).height == 22);
    CHECK(extent_of_drawable(PlanSize{1200, 400}).width == 100);
    CHECK(extent_of_drawable(PlanSize{1200, 400}).height == 33);

    // FLOORED, and this is the assertion that says why: three quarters of a cell is not room
    // for a cell, and a publisher told otherwise authors a row whose bottom it cannot see.
    for (std::int64_t spare = 0; spare < kCanvasCellPx; ++spare) {
        const SurfaceExtent e = extent_of_drawable(PlanSize{10 * kCanvasCellPx + spare,
                                                            4 * kCanvasCellPx + spare});
        CHECK(e.width == 10);
        CHECK(e.height == 4);
    }

    // A surface with no room says so, and a NEGATIVE one -- which is not a size any window
    // has, and is exactly what an int64 field can hold -- says the same thing rather than
    // dividing its way to a negative extent.
    CHECK(extent_of_drawable(PlanSize{}).width == 0);
    CHECK(extent_of_drawable(PlanSize{0, 400}).width == 0);
    CHECK(extent_of_drawable(PlanSize{-1, -1}).width == 0);
    CHECK(extent_of_drawable(PlanSize{-1, -1}).height == 0);
    CHECK(extent_of_drawable(PlanSize{(std::numeric_limits<std::int64_t>::min)(), 1}).width == 0);

    // THE TWO DIRECTIONS AGREE. A canvas of N cells asks for a window of exactly N cells'
    // worth of pixels, and that window has room for exactly N cells -- which is what makes
    // the loop between a publisher and a medium a fixed point rather than a fight.
    for (std::int64_t n = 1; n < 200; ++n) {
        SurfaceCanvas c;
        c.width = n;
        c.height = n;
        CHECK(extent_of_drawable(canvas_window_size(c)).width == n);
        CHECK(extent_of_drawable(canvas_window_size(c)).height == n);
    }
}

TEST_CASE("the shell says how much room there is - on change, and never says none") {
    // THE ONE MESSAGE THAT TRAVELS MEDIUM -> PUBLISHER, and the whole of its policy.
    loom::Switchboard bus;
    std::vector<std::string> log;
    std::vector<SurfaceExtent> heard;
    loom::WeaveId skin{};
    SkinT<FakeMedium>* raw = mount_fake_skin(bus, log, skin);
    (void)loom::mount<RoomEars>(bus, heard);

    SurfaceCanvas c;
    c.width = 8;
    c.height = 2;

    // A MEDIUM WITH NO OPINION SAYS NOTHING. A terminal skin answers this way forever and a
    // window skin answers this way until its window exists; publishing {0,0} would tell a
    // publisher there is no room, which is a different sentence and a false one.
    bus.send(skin, loom::Message(loom::to_value(c)));
    bus.send(skin, loom::Message(loom::to_value(PumpSurface{})));
    bus.pump();
    CHECK(heard.empty());

    // The surface came up: said once.
    raw->medium().room = SurfaceExtent{78, 22};
    bus.send(skin, loom::Message(loom::to_value(PumpSurface{})));
    bus.pump();
    REQUIRE(heard.size() == 1);
    CHECK(heard[0].width == 78);
    CHECK(heard[0].height == 22);

    // ...AND NOT AGAIN WHILE IT IS THE SAME. The beat is 10ms; without this guard a still
    // window would publish a hundred times a second at a publisher that repaints on each one.
    for (int i = 0; i < 20; ++i) {
        bus.send(skin, loom::Message(loom::to_value(PumpSurface{})));
        bus.send(skin, loom::Message(loom::to_value(c)));
    }
    bus.pump();
    CHECK(heard.size() == 1);

    // A hand on the window edge: one sentence per size it passes through.
    raw->medium().room = SurfaceExtent{90, 22};
    bus.send(skin, loom::Message(loom::to_value(PumpSurface{})));
    bus.pump();
    raw->medium().room = SurfaceExtent{100, 33};
    bus.send(skin, loom::Message(loom::to_value(PumpSurface{})));
    bus.pump();
    REQUIRE(heard.size() == 3);
    CHECK(heard[2].width == 100);
    CHECK(heard[2].height == 33);

    // A SURFACE THAT WENT AWAY IS STILL NOT "NO ROOM". Nothing is published, and the value is
    // remembered -- so a medium that loses its surface and gets the same one back says so
    // again rather than going quiet about a change a publisher needs.
    raw->medium().room = SurfaceExtent{};
    bus.send(skin, loom::Message(loom::to_value(PumpSurface{})));
    bus.pump();
    CHECK(heard.size() == 3);
    raw->medium().room = SurfaceExtent{100, 33};
    bus.send(skin, loom::Message(loom::to_value(PumpSurface{})));
    bus.pump();
    REQUIRE(heard.size() == 4);
    CHECK(heard[3].width == 100);
}

TEST_CASE("a terminal medium has no opinion about how much room there is") {
    // THE TUI PROJECTION'S POLICY, asserted rather than described. A terminal skin writes into
    // a stream from row 3 down and owns no drawable whose size is its to read, so it declines
    // the question -- and `report_extent` turns a declined question into SILENCE, which is
    // what keeps a terminal Workshop painting exactly the screen it painted before G-2.
    TuiMedium<ClassicStyle, StringSink> medium;
    CHECK(medium.extent().width == 0);
    CHECK(medium.extent().height == 0);

    // AND NO OPINION ABOUT TYPE EITHER (HD-1). A terminal's character IS its cell; it owns no
    // face whose metric would be its to report, and answering anything else would be this
    // medium claiming a capability it does not have. Zero here is the same word the extent
    // uses -- "text is a cell" -- and it is the truth in a terminal rather than a placeholder.
    CHECK(medium.extent().text_advance_px == 0);
    CHECK(medium.extent().text_line_px == 0);

    loom::Switchboard bus;
    std::vector<SurfaceExtent> heard;
    const loom::WeaveId skin = loom::mount<SkinT<TuiMedium<ClassicStyle, StringSink>>>(bus);
    (void)loom::mount<RoomEars>(bus, heard);

    SurfaceCanvas c;
    c.width = 78;
    c.height = 22;
    for (int i = 0; i < 5; ++i) {
        bus.send(skin, loom::Message(loom::to_value(c)));
        bus.send(skin, loom::Message(loom::to_value(PumpSurface{})));
    }
    bus.pump();
    CHECK(heard.empty());
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
    // THE BUG THIS EXISTS FOR. Sharing one `hello_once` between them means
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

    // AND THE TALLY IS ALREADY THERE. The successor does not have to
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
    // The line lands. It lands ONCE, not twice: the skin said its hello at its
    // own activation, back when it was loaded — so this text is not its first
    // message and does not trigger a second hello, and the speaker has nothing
    // to re-speak to. (With a hello spoken on the first message this reads 2
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
// Tier 4a — the terminal MODES a Skin claims, including pointer reporting
// ============================================================================

TEST_CASE("the Skin's terminal claim includes pointer reporting, and leave undoes enter") {
    // A terminal reports a pointer only if something asks it to, in
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
    // the same exposure the alternate screen has always carried, with one more
    // mode on it, and nothing here pretends otherwise.
    CHECK(kTuiPointerOn == std::string("\x1b[?1002h\x1b[?1006h"));
    CHECK(kTuiPointerOff == std::string("\x1b[?1006l\x1b[?1002l"));
}

// ============================================================================
// Tier 4b — the SDL skin, where this lane built it (dummy video driver)
// ============================================================================

#if defined(SURFACE_HAS_SDL)

#include <SDL3/SDL.h>

namespace {

/// fd-2 recorder: what a Skin said on stderr while this object was alive.
///
/// Hush's twin, pointed at the other stream and keeping what it catches. It
/// exists because the claim is that a failure is SAID, and the
/// only honest way to assert "said" is to read what came out of the process.
class Caught {
public:
    Caught() {
        std::fflush(stderr);
#if defined(_WIN32)
        saved_ = ::_dup(2);
        if (::_pipe(pipe_, 65536, _O_BINARY) == 0) {
            ::_dup2(pipe_[1], 2);
        }
#else
        saved_ = ::dup(STDERR_FILENO);
        if (::pipe(pipe_) == 0) {
            ::dup2(pipe_[1], STDERR_FILENO);
        }
#endif
    }
    ~Caught() { restore(); }
    Caught(const Caught&) = delete;
    Caught& operator=(const Caught&) = delete;

    /// Everything written to stderr so far, and put the real stderr back.
    std::string text() {
        restore();
        return caught_;
    }

private:
    void restore() {
        if (saved_ < 0) {
            return;
        }
        std::fflush(stderr);
#if defined(_WIN32)
        ::_dup2(saved_, 2);
        ::_close(saved_);
        ::_close(pipe_[1]);
        char buf[4096];
        int n = 0;
        while ((n = ::_read(pipe_[0], buf, sizeof(buf))) > 0) {
            caught_.append(buf, static_cast<std::size_t>(n));
        }
        ::_close(pipe_[0]);
#else
        ::dup2(saved_, STDERR_FILENO);
        ::close(saved_);
        ::close(pipe_[1]);
        char buf[4096];
        ssize_t n = 0;
        while ((n = ::read(pipe_[0], buf, sizeof(buf))) > 0) {
            caught_.append(buf, static_cast<std::size_t>(n));
        }
        ::close(pipe_[0]);
#endif
        saved_ = -1;
    }

    int saved_ = -1;
    int pipe_[2] = {-1, -1};
    std::string caught_;
};

void choose_video_driver(const char* name) {
#if defined(_WIN32)
    ::_putenv_s("SDL_VIDEO_DRIVER", name);
    ::_putenv_s("SDL_VIDEODRIVER", name);
#else
    ::setenv("SDL_VIDEO_DRIVER", name, 1);
    ::setenv("SDL_VIDEODRIVER", name, 1);
#endif
}

} // namespace

TEST_CASE("a Skin that cannot open its surface SAYS SO, in SDL's own words") {
    // Asserted rather than described. "Politely dark" is the tempting posture:
    // SDL_Init fails, the medium disables itself, every frame after that is
    // consumed and nothing is ever shown -- a correct degradation and a terrible
    // diagnosis. Real time has been spent on it, because on this machine's WSL
    // the fetched SDL3 has only the dummy and offscreen drivers and the only
    // symptom available was "no window".
    //
    // The failure is FORCED, not waited for: a video driver that does not exist
    // is a real SDL_Init failure with a real SDL_GetError() behind it, and it
    // needs no machine configuration to be changed and nothing left behind.
    //
    // This case must run BEFORE the one below, because SDL_Init is refcounted
    // per process and a video subsystem someone else already brought up would
    // succeed whatever this asks for. It is also why the assertion is on the
    // DIAGNOSTIC and not merely on "no window": a run in which the failure did
    // not actually happen produces no diagnostic and is a red, so this cannot
    // pass vacuously.
    choose_video_driver("zengine-no-such-video-driver");

    Rig r;
    std::string said;
    loom::WeaveId skin{};
    {
        Caught stderr_of_the_skin;
        skin = r.load("zengine-skin-sdl", SKIN_SO_SDL, kSkinRole);
        said = stderr_of_the_skin.text();
    }

    CHECK(said.find("zengine-skin-sdl:") != std::string::npos);
    CHECK(said.find("SDL_Init(SDL_INIT_VIDEO) failed") != std::string::npos);
    // SDL'S OWN REASON, not a house sentence about it. The exact wording is
    // SDL's to choose and this does not pin it; what it pins is that something
    // beyond the house prefix and the call's name came out, which is the whole
    // difference between "it failed" and "here is why".
    const std::size_t colon = said.rfind(": ");
    REQUIRE(colon != std::string::npos);
    CHECK(said.size() > colon + 3);
    CHECK(said.find("(SDL gave no reason)") == std::string::npos);

    // And the medium is still a good citizen about it: the weave loaded, holds
    // the role, and consumes intent without a window rather than taking the
    // process down. Politely dark is still the right DEGRADATION; what this case
    // is about is that it must not be a silent one.
    Hush hush;
    r.intent(skin, SurfaceText{kSlotStatus, "still running"});
    CHECK(r.poke(skin, loom::PokeRead{"texts"}).text == "1");
}

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

TEST_CASE("the SDL skin services its own window and takes nothing off the queue") {
    // THE DEFECT THIS CASE EXISTS FOR. G-1 read "the queue has one owner" as a rule about who
    // may CALL SDL and emptied this medium's pump(), leaving a window's liveness to be a side
    // effect of whichever INPUT weave the host happened to boot. Run the SDL skin with the
    // terminal reader -- two independent flags the host argues must stay independent -- and
    // nothing called into SDL at all: the window came up, never processed another OS message,
    // and Windows flagged it Not Responding. Found live, in the graphical Workshop.
    //
    // One-owner is a rule about who REMOVES. `SDL_PumpEvents` removes nothing, and both halves
    // of that are asserted below: the pump ran (it complained about what it found), and every
    // event that was on the queue is still on it, in order, for the reader that owns it.
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
    r.tick(); // the window is created on the first frame, and pump() has one to service

    // Put events on THE queue -- one SDL, shared between this process and the weave library.
    // Enough of them that a queue nobody drains is a measurement and not a coincidence.
    constexpr int kPushed = 1200;
    for (int i = 0; i < kPushed; ++i) {
        SDL_Event ev{};
        ev.type = SDL_EVENT_USER;
        ev.user.code = i;
        REQUIRE(SDL_PushEvent(&ev));
    }
    const int before =
        SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
    REQUIRE(before >= kPushed);

    std::string said;
    {
        Caught caught;
        r.bus.send_to_role(kSkinRole, loom::Message(loom::to_value(PumpSurface{})));
        r.pump();
        said = caught.text();
    }

    // THE PUMP RAN. An emptied pump() is silent here, which is exactly how the defect was
    // shaped -- and the complaint is the honest half of the pairing that caused it: a window
    // that draws while a different ear is listening, said out loud with the flag that fixes it.
    CHECK(said.find("nothing is taking them") != std::string::npos);
    CHECK(said.find("--input zengine-input-sdl") != std::string::npos);

    // ...AND IT TOOK NOTHING. Not one event, and not one out of order: the reader that owns
    // this queue finds exactly what was put on it.
    CHECK(SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) == before);
    int seen = 0;
    SDL_Event got{};
    while (SDL_PollEvent(&got)) {
        if (got.type == SDL_EVENT_USER) {
            CHECK(got.user.code == seen);
            ++seen;
        }
    }
    CHECK(seen == kPushed);

    // Said ONCE per incarnation: a complaint on every beat is the noise that teaches a person
    // to stop reading stderr.
    for (int i = 0; i < kPushed; ++i) {
        SDL_Event ev{};
        ev.type = SDL_EVENT_USER;
        REQUIRE(SDL_PushEvent(&ev));
    }
    std::string again;
    {
        Caught caught;
        r.bus.send_to_role(kSkinRole, loom::Message(loom::to_value(PumpSurface{})));
        r.pump();
        again = caught.text();
    }
    CHECK(again.find("nothing is taking them") == std::string::npos);
    CHECK(r.poke(skin, loom::PokeRead{"pumps"}).text == "2");
    while (SDL_PollEvent(&got)) {
    }
}

TEST_CASE("the SDL window is the person's to resize, and says how much room it has") {
    // G-2's live half, under the dummy driver: everything below is a real SDL window, a real
    // renderer and the real weave library -- only the photons are missing. The window this
    // case inspects was created inside the loaded .so; there is one shared SDL3 in this
    // process, so `SDL_GetWindows` finds it, which is what makes this an observation of the
    // shipped medium rather than of a copy of its arithmetic.
#if defined(_WIN32)
    ::_putenv_s("SDL_VIDEO_DRIVER", "dummy");
    ::_putenv_s("SDL_VIDEODRIVER", "dummy");
#else
    ::setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    ::setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
    Rig r;
    std::vector<SurfaceExtent> heard;
    (void)loom::mount<RoomEars>(r.bus, heard);
    const loom::WeaveId skin = r.load("zengine-skin-sdl", SKIN_SO_SDL, kSkinRole);

    // The opening picture: Workshop's own minimum screen, which is what creates the window.
    SurfaceCanvas c;
    c.width = 78;
    c.height = 22;
    r.intent(skin, c);

    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    REQUIRE(windows != nullptr);
    REQUIRE(count == 1);
    SDL_Window* win = windows[0];
    SDL_free(windows);

    // RESIZABLE. Before G-2 this window was created with no flags at all and a person could
    // not take hold of its edge; the whole phase is downstream of this bit.
    CHECK((SDL_GetWindowFlags(win) & SDL_WINDOW_RESIZABLE) != 0);

    // ...WITH A FLOOR, and the floor is the first picture's own size. 78x22 cells at
    // kCanvasCellPx is what a Workshop asks for, and it is what this window will never be
    // dragged below.
    int min_w = 0;
    int min_h = 0;
    REQUIRE(SDL_GetWindowMinimumSize(win, &min_w, &min_h));
    CHECK(min_w == 78 * kCanvasCellPx);
    CHECK(min_h == 22 * kCanvasCellPx);

    // AND IT SAID SO, in cells, unprompted.
    REQUIRE(heard.size() == 1);
    CHECK(heard[0].width == 78);
    CHECK(heard[0].height == 22);

    // A HAND ON THE WINDOW EDGE. Nothing tells the weave this happened -- there is no resize
    // message it accepts and it takes nothing off the event queue -- so the beat is what
    // notices, and PumpSurface is that beat's hands.
    REQUIRE(SDL_SetWindowSize(win, 1200, 400));
    r.intent(skin, PumpSurface{});
    REQUIRE(heard.size() == 2);
    CHECK(heard[1].width == 100);
    CHECK(heard[1].height == 33);

    // THE PUBLISHER'S ANSWER DOES NOT SHRINK THE WINDOW BACK. This is the half that would
    // have made a resizable window unusable: a canvas that fills the room, handed to a medium
    // that sizes the window to the canvas, nibbles the window down to a whole number of cells
    // on every single frame.
    c.width = 100;
    c.height = 33;
    r.intent(skin, c);
    int now_w = 0;
    int now_h = 0;
    REQUIRE(SDL_GetWindowSize(win, &now_w, &now_h));
    CHECK(now_w == 1200);
    CHECK(now_h == 400);
    CHECK(heard.size() == 2); // and nothing changed, so nothing was said

    // SMALLER AGAIN, within the minimum: the same numbers, in reverse.
    REQUIRE(SDL_SetWindowSize(win, 78 * kCanvasCellPx, 22 * kCanvasCellPx));
    r.intent(skin, PumpSurface{});
    REQUIRE(heard.size() == 3);
    CHECK(heard[2].width == 78);
    CHECK(heard[2].height == 22);

    // A PICTURE THAT GENUINELY DOES NOT FIT STILL GROWS THE WINDOW -- the rule that keeps a
    // board (whose publisher hears nothing) whole. Same medium, same function, no per-shape
    // special case.
    c.width = 120;
    c.height = 22;
    r.intent(skin, c);
    REQUIRE(SDL_GetWindowSize(win, &now_w, &now_h));
    CHECK(now_w == 120 * kCanvasCellPx);
    CHECK(now_h == 22 * kCanvasCellPx);
}

TEST_CASE("the SDL skin opens a real face and publishes what it MEASURED") {
    // HD-1's live half, under the dummy driver: a real window, a real renderer, a
    // real SDL_ttf, a real FreeType and the real weave library -- only the photons
    // are missing. What this case cannot do is judge whether the letters are
    // legible; that is a person's job and it is done in the report. What it CAN do
    // is prove the seam: the medium opened the face it carries, measured it, and
    // said the result out loud.
    choose_video_driver("dummy");
    Rig r;
    std::vector<SurfaceExtent> heard;
    (void)loom::mount<RoomEars>(r.bus, heard);
    const loom::WeaveId skin = r.load("zengine-skin-sdl", SKIN_SO_SDL, kSkinRole);

    SurfaceCanvas c;
    c.width = 78;
    c.height = 22;
    // A bounded region, published exactly as Workshop's Terminal publishes one.
    SurfaceTextRegion pane;
    pane.x = 22;
    pane.y = 9;
    pane.w = 56;
    pane.h = 13;
    pane.rows.push_back(SurfaceTextRow{"TERMINAL -- weave #3", role::kAccent});
    pane.rows.push_back(SurfaceTextRow{"gjpqy Ill1 O0o ceao weave", role::kFill});
    c.texts.push_back(pane);
    r.intent(skin, c);

    REQUIRE(heard.size() == 1);
    CHECK(heard[0].width == 78);
    CHECK(heard[0].height == 22);

    // MEASURED, NOT AUTHORED. The only number a person chose is the point size; the
    // advance and the line height are what the opened face answered, so this case
    // asserts their SHAPE rather than their values -- a face is allowed to be
    // rasterized differently by a different FreeType, and a test that pinned 8 and
    // 18 would be pinning this machine.
    const std::int64_t advance = heard[0].text_advance_px;
    const std::int64_t line = heard[0].text_line_px;
    CHECK(advance > 0);
    CHECK(line > 0);
    CHECK(line > advance);        // a monospace line is taller than a character is wide
    CHECK(advance < kCanvasCellPx); // ...and at this size, narrower than a cell
    CHECK(line < 4 * kCanvasCellPx);

    // AND THE PUBLISHER CAN DO ARITHMETIC WITH IT. This is the whole reason the
    // metric travels: `fit_region` here and `fit_region` in Workshop are the same
    // function reading the same two numbers, so "how much prose fits" has one
    // answer in this process.
    const RegionFit fit = fit_region(pane, heard[0]);
    CHECK(fit.graphical());
    CHECK(fit.columns > pane.w);   // real type is narrower than a cell: MORE columns
    CHECK(fit.rows < pane.h);      // ...and taller than one: fewer rows
    CHECK(fit.rows >= 1);

    // A SECOND FRAME SAYS NOTHING NEW. The metric is a fact about the face, not
    // about the frame, so a still window with an open font is silent.
    r.intent(skin, c);
    CHECK(heard.size() == 1);

    // DRAWING A REGION LEAVES THE RENDERER WITH NO VIEWPORT OF ITS OWN (HD-2's repair of
    // an HD-1 defect). SDL keeps two states here and `SDL_GetRenderViewport` flattens
    // them -- a renderer with no viewport answers with the whole target's rectangle -- so
    // save-and-restore through that call alone turned the implicit state into an explicit
    // one, after which SDL stopped growing it when the window did. The picture was a
    // Workshop dragged larger whose panels were still clipped to the old window's width;
    // reproduced on the pristine tree first, with nothing typed. `SDL_RenderViewportSet`
    // is SDL's own answer to the question, and this is that answer.
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    REQUIRE(windows != nullptr);
    REQUIRE(count == 1);
    SDL_Renderer* ren = SDL_GetRenderer(windows[0]);
    SDL_free(windows);
    REQUIRE(ren != nullptr);
    CHECK_FALSE(SDL_RenderViewportSet(ren));

    // ...and it is still false after a canvas with NO region at all, which is the control:
    // a pass that came from never having drawn one would prove nothing.
    SurfaceCanvas plain;
    plain.width = 78;
    plain.height = 22;
    r.intent(skin, plain);
    CHECK_FALSE(SDL_RenderViewportSet(ren));
}

#endif // SURFACE_HAS_SDL
