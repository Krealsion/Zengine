// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The tile defense played headless, through both windows' painting: its game and its stage loaded
// by the kernel from their libraries, the graphics directory beside them, and a window mounted in
// process over the suite's terminal, the suite's pixel backend, or -- where the build has SDL --
// SDL's own under its dummy driver. Keys and presses go in through the window, as a player's do;
// what comes out is read where each window shows it: the terminal's cells and its bytes, the light
// a GUI window hands its backend, and the pixels SDL draws. It fails where the drawing breaks.

#include "doctest.h"

#include "graphics_rig.hpp"
#include "lifecycle_door.hpp"
#include "window_backends.hpp"

#include "examples/tile-defense/art.hpp"
#include "examples/tile-defense/picture.hpp"
#include "examples/tile-defense/rules.hpp"
#include "examples/tile-defense/vocabulary.hpp"

#include "graphics/light_paint.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"

#if defined(GRAPHICS_HAS_SDL)
#include "graphics/window/window_sdl_backend.hpp"
#endif

#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace gfx = zengine::graphics;
namespace gw = zengine::graphics::window;
namespace td = zengine::tile_defense;
namespace in = zengine::input;
namespace light = zengine::graphics::light;
using zengine::testing::graphics_rig::Rig;
using zengine::testing::window_backends::FakeSdlBackend;
using zengine::testing::window_backends::FakeSdlWindow;
using zengine::testing::window_backends::FakeTerminal;
using zengine::testing::window_backends::FakeTuiWindow;
using zengine::testing::window_backends::LightLog;
using zengine::testing::window_backends::TermLog;

/// Where the suite builds its towers: an arrow tower beside the road's first long leg down, and a
/// fireball tower beside its climb back up.
constexpr std::int64_t kArrowCol = 10, kArrowRow = 7;
constexpr std::int64_t kFireCol = 20, kFireRow = 8;

/// What ends the game, as the host's door hears it: its player's quit, and a window's close box.
struct Ends {
    int quits = 0;
    int closes = 0;
};
struct TileEndsState {
    std::int64_t heard = 0;
    ZEN_SHAPE(TileEndsState, 1, ZEN_FIELD(heard));
};
class EndsEars : public loom::WeaveBase<EndsEars, TileEndsState,
                                        loom::Accept<td::TileQuit, gfx::SurfaceCloseRequested>,
                                        loom::Emit<>> {
public:
    explicit EndsEars(Ends& ends) : ends_(&ends) {}
    void on(const td::TileQuit&, loom::Mail& mail) {
        if (mail.authored_from_role(td::kGameRole)) {
            ++ends_->quits;
        }
    }
    void on(const gfx::SurfaceCloseRequested&, loom::Mail&) { ++ends_->closes; }

private:
    Ends* ends_;
};

/// THE GAME, ITS STAGE AND THE DIRECTORY, beside a window of kind `W` mounted in process at its
/// role over `device`, activated as Loom's control door activates it; and ears for its end.
template <class W>
struct Played {
    Rig r;
    W* window = nullptr;
    loom::WeaveId window_id;
    std::string role;
    Ends ends;

    /// Over `device`, or with none the window's own.
    template <class... Device>
    explicit Played(const std::string& window_name, Device... device)
        : role(std::string(gfx::kWindowRolePrefix) + window_name) {
        REQUIRE(r.load("tile-defense-stage", TILE_DEFENSE_STAGE_SO, td::kStageRole).valid());
        REQUIRE(r.load("zengine-graphics", GRAPHICS_SO, gfx::kGraphicsRole).valid());
        auto w = std::make_unique<W>(std::move(device)...);
        window = w.get();
        loom::Grant g = loom::emit_default_grant(*window);
        window_id = r.bus.register_weave(std::move(w), std::move(g), role);
        window->zen_set_self(window_id);
        const loom::WeaveId door = zengine::testing::mount_door(r.bus);
        zengine::testing::order_activation(r.bus, door, window_id, 1);
        r.drain();
        REQUIRE(r.load("tile-defense", TILE_DEFENSE_SO, td::kGameRole).valid());
        (void)loom::mount<EndsEars>(r.bus, ends);
        r.drain();
    }

    /// The window's own beat: what the suite queued for it read and given to the areas.
    void pump() {
        r.bus.send(window_id, loom::Message(loom::to_value(gfx::PumpSurface{})));
        r.drain();
    }
    /// `n` steps of the world, as its beat gives them.
    void steps(std::int64_t n) {
        r.to_role(td::kGameRole, td::TileTick{n});
        r.drain();
    }
    gfx::WindowShown shown() {
        r.bus.send_as_to_role(r.ears, role,
                              loom::Message(loom::to_value(gfx::WindowShowAsked{}), r.ears, r.ears,
                                            r.next_corr++));
        r.drain();
        return r.heard.shown.empty() ? gfx::WindowShown{} : r.heard.shown.back();
    }
};

in::KeyPressed key(std::int64_t scancode) { return in::KeyPressed{scancode, "", in::mod::kNone}; }

/// The cell a terminal row shows at a column: its character's UTF-8, "" past the row's end.
std::string cell(const std::vector<std::string>& rows, std::int64_t col, std::int64_t row) {
    if (row < 0 || static_cast<std::size_t>(row) >= rows.size()) {
        return {};
    }
    const std::string& r = rows[static_cast<std::size_t>(row)];
    std::int64_t c = 0;
    for (std::size_t at = 0; at < r.size();) {
        std::size_t end = at + 1;
        while (end < r.size() && (static_cast<unsigned char>(r[end]) & 0xC0U) == 0x80U) {
            ++end;
        }
        if (c == col) {
            return r.substr(at, end - at);
        }
        ++c;
        at = end;
    }
    return {};
}

/// The last ground a terminal's bytes set (an SGR code of 40 to 49 or 100 to 107); 0 for none.
int last_ground(const std::string& bytes) {
    int last = 0;
    for (std::size_t at = bytes.find("\x1b["); at != std::string::npos;
         at = bytes.find("\x1b[", at + 2)) {
        std::size_t end = at + 2;
        int code = 0;
        while (end < bytes.size() && bytes[end] >= '0' && bytes[end] <= '9') {
            code = code * 10 + (bytes[end] - '0');
            ++end;
        }
        if (end < bytes.size() && bytes[end] == 'm' && end > at + 2 &&
            ((code >= 40 && code <= 49) || (code >= 100 && code <= 107))) {
            last = code;
        }
    }
    return last;
}

/// Whether a character is one of the Braille block's, a spark's dots.
bool braille(const std::string& g) {
    return g.size() == 3 && static_cast<unsigned char>(g[0]) == 0xE2 &&
           (static_cast<unsigned char>(g[1]) == 0xA0 || static_cast<unsigned char>(g[1]) == 0xA1 ||
            static_cast<unsigned char>(g[1]) == 0xA2 || static_cast<unsigned char>(g[1]) == 0xA3);
}

/// What a played wave showed, step by step.
struct Seen {
    bool walker_on_road = false, walker_off_road = false;
    bool arrow = false, fireball = false, spark = false, burst = false;
};

#if defined(GRAPHICS_HAS_SDL)
/// A pixel of a 24-bit bottom-up BMP, as 0xRRGGBB; none outside it.
std::optional<std::uint32_t> bmp_pixel(const std::string& bmp, std::int64_t x, std::int64_t y) {
    if (bmp.size() < 54) {
        return std::nullopt;
    }
    const auto u32 = [&](std::size_t at) {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(bmp[at])) |
               static_cast<std::uint32_t>(static_cast<unsigned char>(bmp[at + 1])) << 8 |
               static_cast<std::uint32_t>(static_cast<unsigned char>(bmp[at + 2])) << 16 |
               static_cast<std::uint32_t>(static_cast<unsigned char>(bmp[at + 3])) << 24;
    };
    const std::int64_t offset = u32(10), w = u32(18), h = u32(22);
    if (x < 0 || y < 0 || x >= w || y >= h) {
        return std::nullopt;
    }
    const std::int64_t stride = (w * 3 + 3) / 4 * 4;
    const auto at = static_cast<std::size_t>(offset + (h - 1 - y) * stride + x * 3);
    if (at + 2 >= bmp.size()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bmp[at + 2])) << 16 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(bmp[at + 1])) << 8 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(bmp[at]));
}
#endif

/// A light node of the last frame by its path in the game's area; null where it shows none.
const light::NodePrimitives* node_of(const LightLog& log, const std::string& path) {
    if (log.frames.empty()) {
        return nullptr;
    }
    for (const LightLog::Area& a : log.frames.back().areas) {
        if (a.area != td::kGameArea) {
            continue;
        }
        for (std::size_t i = 0; i < a.paths.size(); ++i) {
            if (a.paths[i] == path) {
                return &a.nodes[i];
            }
        }
    }
    return nullptr;
}

std::uint64_t version_of(const LightLog& log, const std::string& path) {
    for (const LightLog::Area& a : log.frames.back().areas) {
        for (std::size_t i = 0; i < a.paths.size(); ++i) {
            if (a.area == td::kGameArea && a.paths[i] == path) {
                return a.versions[i];
            }
        }
    }
    return 0;
}

} // namespace

TEST_CASE("the tile defense in the terminal: its board a cell a tile, lined up, its map on its "
          "grounds, and a wave played by keys and presses -- towers drawing and flaring, walkers "
          "on the road, arrows pointing the way they fly, a fireball trailing sparks and bursting") {
    TermLog term;
    term.size = {80, 26};
    Played<FakeTuiWindow> p(gfx::kTuiWindow, FakeTerminal{&term});
    const td::Art art = td::make_art();
    const gfx::TileGrid map = td::map_of(art);

    // THE BOARD, LINED UP: each tile its one cell, the castle where the road ends, a tree, a rock
    // and the grass's marks where the map puts them, and the count beneath.
    std::vector<std::string> rows = p.shown().rows;
    REQUIRE(rows.size() >= static_cast<std::size_t>(td::kRows + td::kHudLines));
    CHECK(cell(rows, td::kCastleCol, td::kCastleRow) == "#");
    std::int64_t trees = 0, marks = 0;
    for (std::int64_t r = 0; r < td::kRows; ++r) {
        for (std::int64_t c = 0; c < td::kCols; ++c) {
            const gfx::FrameId f = map.at(c, r);
            if (f == art.tree) {
                ++trees;
                CHECK(cell(rows, c, r) == "^");
            } else if (f == art.grass[1] && !braille(cell(rows, c, r))) {
                ++marks; // a mark of the grass, where the cursor's reach does not dot it
                CHECK(cell(rows, c, r) == ",");
            }
        }
    }
    CHECK(trees > 0);
    CHECK(marks > 0);
    CHECK(rows[td::kRows].rfind("Gold 150   Lives 20   Wave 0/8", 0) == 0);
    CHECK(rows[td::kRows + 1].find("[F] Fireball 80") != std::string::npos);

    // ITS GROUNDS: the road on yellow and the grass on green, each showing that wrote a ground
    // ending on the terminal's own.
    CHECK(term.written.find("\x1b[43m") != std::string::npos);
    CHECK(term.written.find("\x1b[42m") != std::string::npos);
    std::size_t grounded = 0;
    for (const std::string& w : term.writes) {
        if (w.find("\x1b[42m") != std::string::npos || w.find("\x1b[43m") != std::string::npos) {
            ++grounded;
            CHECK(last_ground(w) == 49);
        }
    }
    CHECK(grounded > 0);

    // BUILT BY KEYS AND PRESSES: the cursor moved to a tile and Return, then F and a press.
    term.moments.emplace_back(key(in::scan::kA));
    for (std::int64_t i = 0; i < kArrowCol - 4; ++i) {
        term.moments.emplace_back(key(in::scan::kRight));
    }
    term.moments.emplace_back(key(in::scan::kDown));
    term.moments.emplace_back(key(in::scan::kReturn));
    term.moments.emplace_back(key(in::scan::kF));
    term.moments.emplace_back(
        in::PointerButton{1, true, kFireCol, kFireRow, in::space::kCells, in::mod::kNone});
    term.moments.emplace_back(
        in::PointerButton{1, false, kFireCol, kFireRow, in::space::kCells, in::mod::kNone});
    p.pump();
    rows = p.shown().rows;
    CHECK(cell(rows, kArrowCol, kArrowRow) == "A");
    CHECK(cell(rows, kFireCol, kFireRow) == "F");
    CHECK(rows[td::kRows].rfind("Gold 20 ", 0) == 0);

    // A WAVE, SENT BY ITS WORDS BENEATH THE BOARD, pressed: "[Space] Wave".
    const std::string choices = rows[td::kRows + 1];
    const auto wave_at = static_cast<std::int64_t>(choices.find("[Space] Wave"));
    REQUIRE(wave_at > 0);
    term.moments.emplace_back(in::PointerButton{1, true, wave_at + 2, td::kRows + 1,
                                                in::space::kCells, in::mod::kNone});
    p.pump();
    CHECK(p.shown().rows[td::kRows].find("Wave 1/8") != std::string::npos);

    const std::int64_t layers = p.shown().nodes;
    Seen seen;
    for (int beat = 0; beat < 1200; ++beat) {
        p.steps(2);
        rows = p.shown().rows;
        for (std::int64_t r = 0; r < td::kRows; ++r) {
            for (std::int64_t c = 0; c < td::kCols; ++c) {
                const std::string g = cell(rows, c, r);
                if (g == "r" || g == "R" || g == "B") {
                    (td::is_road(c, r) ? seen.walker_on_road : seen.walker_off_road) = true;
                } else if (g == "-" || g == "|" || g == "/" || g == "\\") {
                    seen.arrow = true;
                } else if (g == "*" || g == "o" || g == "O") {
                    seen.fireball = seen.fireball || !(map.at(c, r) == art.rock);
                } else if (g == "@") {
                    seen.burst = true;
                } else if (braille(g)) {
                    seen.spark = true;
                }
            }
        }
        if (rows[td::kRows + 2].find("held") != std::string::npos) {
            break;
        }
    }
    CHECK(seen.walker_on_road);
    CHECK_FALSE(seen.walker_off_road);
    CHECK(seen.arrow);
    CHECK(seen.fireball);
    CHECK(seen.spark);
    CHECK(seen.burst);
    CHECK(rows[td::kRows + 2].find("Wave 1 held") != std::string::npos);
    const gfx::WindowShown shown = p.shown();
    CHECK(shown.refusals == 0);
    CHECK(shown.nodes == layers); // a layer a node, however much it holds

    // Q, AS THE PLAYER PRESSES IT: the game says its player quit, which ends the host.
    term.moments.emplace_back(key(in::scan::kQ));
    p.pump();
    CHECK(p.ends.quits == 1);
}

TEST_CASE("the tile defense in a GUI window: its sheet held once and never painted again, its map "
          "forty by twenty tiles of sixteen pixels blitted from it, and a wave's sprites and "
          "sparks where the world puts them") {
    LightLog light;
    light.keeps_last = true;
    Played<FakeSdlWindow> p(gfx::kGuiWindow, FakeSdlBackend{&light});

    // THE WINDOW SHAPED TO THE PICTURE: forty tiles across, twenty down and three lines beneath.
    REQUIRE_FALSE(light.shaped.empty());
    CHECK(light.shaped.back().first == "Tile defense");
    CHECK(light.shaped.back().second.width == td::kCols * td::kTileWantPx);
    CHECK(light.shaped.back().second.height ==
          td::kRows * td::kTileWantPx + light.face.line / 2 + td::kHudLines * light.face.line);

    const light::NodePrimitives* sheet = node_of(light, td::kSheetPath);
    REQUIRE(sheet != nullptr);
    REQUIRE(sheet->sheets.size() == 1);
    CHECK(sheet->sheets[0].pw == 8 * td::kFramePx);
    const light::NodePrimitives* map = node_of(light, "board/map");
    REQUIRE(map != nullptr);
    CHECK(map->sheet == td::kSheetPath);
    REQUIRE(map->blits.size() == static_cast<std::size_t>(td::kCols * td::kRows));
    CHECK(map->blits[0].w == td::kTileWantPx);
    CHECK(map->blits[0].h == td::kTileWantPx);
    CHECK(map->blits.back().x == (td::kCols - 1) * td::kTileWantPx);
    CHECK(map->blits.back().y == (td::kRows - 1) * td::kTileWantPx);
    const std::uint64_t sheet_painted = version_of(light, td::kSheetPath);
    const std::uint64_t map_painted = version_of(light, "board/map");

    // A press builds a fireball tower on its tile, sixteen pixels a tile.
    light.queued.events.emplace_back(in::KeyPressed{in::scan::kF, "", in::mod::kNone});
    light.queued.events.emplace_back(in::PointerButton{
        1, true, kFireCol * 16 + 3, kFireRow * 16 + 12, in::space::kPixels, in::mod::kNone});
    light.queued.events.emplace_back(in::KeyPressed{in::scan::kSpace, "", in::mod::kNone});
    p.pump();
    const light::NodePrimitives* towers = node_of(light, "board/towers");
    REQUIRE(towers != nullptr);
    REQUIRE(towers->blits.size() == 1);
    CHECK(towers->blits[0].x == kFireCol * 16);
    CHECK(towers->blits[0].y == kFireRow * 16);

    // ITS ANIMATIONS: the tower's brazier in its frames, burning and flaring; the walkers, the
    // fireball and its sparks as the world puts them.
    bool walkers = false, fireball = false, sparks = false;
    std::vector<std::int32_t> tower_frames;
    for (int beat = 0; beat < 700; ++beat) {
        p.steps(1);
        const light::NodePrimitives* w = node_of(light, "board/walkers");
        const light::NodePrimitives* s = node_of(light, "board/shots");
        const light::NodePrimitives* k = node_of(light, "board/sparks");
        const light::NodePrimitives* t = node_of(light, "board/towers");
        walkers = walkers || (w != nullptr && !w->blits.empty());
        fireball = fireball || (s != nullptr && !s->blits.empty());
        sparks = sparks || (k != nullptr && k->quads.size() > 3 && k->quads[0].w == 2);
        if (t != nullptr && t->blits.size() == 1 &&
            std::find(tower_frames.begin(), tower_frames.end(), t->blits[0].sx) ==
                tower_frames.end()) {
            tower_frames.push_back(t->blits[0].sx);
        }
        if (walkers && fireball && sparks && tower_frames.size() >= 4) {
            break;
        }
    }
    CHECK(walkers);
    CHECK(fireball);
    CHECK(sparks);
    CHECK(tower_frames.size() == 4); // three of burning, one of flaring
    // ...and the sheet and the map, unchanged, never painted again.
    CHECK(version_of(light, td::kSheetPath) == sheet_painted);
    CHECK(version_of(light, "board/map") == map_painted);
    CHECK(p.shown().refusals == 0);

    // THE CLOSE BOX: the window says its close was asked, which ends the host.
    light.queued.close = true;
    p.pump();
    CHECK(p.ends.closes == 1);
}

#if defined(GRAPHICS_HAS_SDL)
TEST_CASE("the tile defense through SDL's own drawing: the castle's and the road's pixels where "
          "their tiles are, and the sheet's texture made once, none more however many pictures "
          "follow") {
    using RealSdlWindow = gw::SdlWindowT<gw::ProcessSdlBackend>;
    Played<RealSdlWindow> p(gfx::kGuiWindow);
    p.pump();
    p.steps(1);
    const std::int64_t made = p.shown().textures;
    CHECK(made == 1); // the sheet's, and nothing else is an image

    const std::optional<gw::CapturedWindowPicture> picture = p.window->capture();
    REQUIRE(picture.has_value());
    REQUIRE(picture->format == "image/bmp");
    // the castle's stone, a road tile's earth, each in its tile's pixels
    const auto stone = bmp_pixel(picture->bytes, td::kCastleCol * 16 + 3, td::kCastleRow * 16 + 6);
    REQUIRE(stone.has_value());
    CHECK(*stone == 0x9aa0b4u);
    const auto earth = bmp_pixel(picture->bytes, 3 * 16 + 1, 3 * 16 + 1);
    REQUIRE(earth.has_value());
    CHECK(((*earth >> 16) & 0xffu) > ((*earth >> 8) & 0xffu)); // more red than green: earth
    CHECK(((*earth >> 8) & 0xffu) > (*earth & 0xffu));         // more green than blue

    // A WAVE OF PICTURES, sent as the window gives the game its keys: the sheet's pixels never
    // changed, so no texture was made for them.
    p.r.to_role(td::kGameRole, gfx::AreaKeyPressed{td::kGameArea, in::scan::kSpace, "space"});
    for (int i = 0; i < 120; ++i) {
        p.steps(1);
    }
    CHECK(p.shown().textures == made);
    CHECK(p.shown().showings > 100);

    // ANOTHER PALETTE CHOSEN: every node of the area painted again, the sheet's pixels as they were,
    // so its texture is kept.
    p.r.to_role(gfx::kGraphicsRole, gfx::themes::day());
    p.r.to_role(gfx::kGraphicsRole, gfx::PaletteChosen{td::kGameArea, "day"});
    p.r.drain();
    p.steps(1);
    const gfx::WindowShown repainted = p.shown();
    CHECK(repainted.painter_calls > 0);
    CHECK(repainted.textures == made);
}
#endif
