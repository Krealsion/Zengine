// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// zengine-tile-defense-bench: the tile defense played by a script to its busiest wave, through each
// window mounted in process in turn (the GUI window over SDL's backend, the TUI window over a
// terminal that keeps its bytes), each step of that wave timed from its beat to its picture shown;
// with `--pictures <dir>`, pictures of both windows mid-wave, the terminal's drawn from its bytes in
// xterm's colours. Reference: examples/tile-defense/README.md, "Measure it".

#include "game.hpp"
#include "host_shapes.hpp"
#include "rules.hpp"
#include "vocabulary.hpp"

#include "graphics/vocabulary.hpp"
#include "graphics/terminal_inks.hpp"
#include "graphics/window/window_tui.hpp"
#if defined(TILE_DEFENSE_HAS_SDL)
#include "graphics/window/window_sdl.hpp"
#include "graphics/window/window_sdl_backend.hpp"
#endif

#include <zen/host/lifecycle_wiring.hpp>
#include <zen/kernel/admission.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace gfx = zengine::graphics;
namespace gw = zengine::graphics::window;
namespace td = zengine::tile_defense;
namespace in = zengine::input;

// ---- the bench's host: a door that activates what it mounts, and ears ----------------------------

using td::BenchActivate;
using td::BenchDoorState;
using td::BenchEarsState;
/// The host's lifecycle door: an in-process weave started as Loom's control door starts one.
class BenchDoor : public loom::WeaveBase<BenchDoor, BenchDoorState, loom::Accept<BenchActivate>,
                                         loom::Emit<loom::Activated>> {
public:
    explicit BenchDoor(loom::LifecycleAuthority authority) : authority_(authority) {}
    void on(const BenchActivate& o, loom::Mail& mail) {
        ++state_.announced;
        mail.announce_lifecycle(authority_, loom::WeaveId{static_cast<std::uint64_t>(o.target)},
                                loom::Activated{state_.announced}, state_.announced);
    }

private:
    loom::LifecycleAuthority authority_;
};

/// What the bench hears: the manager's answers and the window's report.
class BenchEars : public loom::WeaveBase<BenchEars, BenchEarsState,
                                         loom::Accept<loom::Result, loom::Refused, gfx::WindowShown>,
                                         loom::Emit<loom::LoadWeave, gfx::WindowShowAsked>> {
public:
    void on(const loom::Result&, loom::Mail&) { ++state_.heard; }
    void on(const loom::Refused& r, loom::Mail&) { refused.push_back(r.reason); }
    void on(const gfx::WindowShown& s, loom::Mail&) { shown = s; }
    std::vector<std::string> refused;
    gfx::WindowShown shown;
};

/// A terminal that keeps what it is written, 80 by 26 cells, typing nothing.
struct KeptBytes {
    std::string written;
    std::int64_t writes = 0;
};
class KeepingTerminal {
public:
    KeepingTerminal() = default;
    explicit KeepingTerminal(KeptBytes* kept) : kept_(kept) {}
    void claim() {}
    std::int64_t now_ms() const { return 0; }
    bool utf8() const { return true; }
    gw::TerminalSize size() const { return gw::TerminalSize{80, 26}; }
    void write(std::string_view bytes) {
        kept_->written.append(bytes);
        ++kept_->writes;
    }
    std::vector<in::InputEvent> poll() { return {}; }

private:
    KeptBytes* kept_ = nullptr;
};

std::string exe_dir() {
#if defined(_WIN32)
    std::string path(MAX_PATH, '\0');
    const DWORD n = ::GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(n);
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? "." : path.substr(0, slash);
#else
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return ".";
    }
    buf[n] = '\0';
    const std::string path(buf);
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
#endif
}

constexpr const char* kSuffix =
#if defined(_WIN32)
    ".dll";
#else
    ".so";
#endif

// ---- the terminal's picture: its bytes drawn as xterm shows them --------------------------------

/// A terminal's screen as its bytes leave it: each cell's character, ink and ground (0xRRGGBB).
struct Screen {
    std::int64_t cols = 80, rows = 26;
    struct Cell {
        std::string glyph = " ";
        std::uint32_t ink = 0xe5e5e5, ground = 0x000000;
        bool bold = false;
    };
    std::vector<Cell> cells = std::vector<Cell>(80 * 26);
    std::int64_t col = 0, row = 0;
    std::uint32_t ink = 0xe5e5e5, ground = 0x000000;
    bool bold = false;

    static std::uint32_t xterm(int code) {
        const int i = code >= 90 ? code - 90 + 8 : code - 30;
        return gw::kTerminalInkColors[static_cast<std::size_t>(std::clamp(i, 0, 15))];
    }
    void sgr(const std::string& params) {
        std::size_t at = 0;
        while (at <= params.size()) {
            const std::size_t end = std::min(params.find(';', at), params.size());
            const int code = end > at ? std::stoi(params.substr(at, end - at)) : 0;
            if (code == 0) {
                ink = 0xe5e5e5;
                ground = 0x000000;
                bold = false;
            } else if (code == 1) {
                bold = true;
            } else if (code == 22) {
                bold = false;
            } else if (code == 39) {
                ink = 0xe5e5e5;
            } else if (code == 49) {
                ground = 0x000000;
            } else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97)) {
                ink = xterm(code);
            } else if ((code >= 40 && code <= 47) || (code >= 100 && code <= 107)) {
                ground = xterm(code - 10);
            }
            at = end + 1;
        }
    }
    /// The bytes laid over the screen: cursor moves, colours and characters.
    void take(const std::string& bytes) {
        std::size_t i = 0;
        while (i < bytes.size()) {
            if (bytes[i] == '\x1b' && i + 1 < bytes.size() && bytes[i + 1] == '[') {
                std::size_t j = i + 2;
                while (j < bytes.size() && !((bytes[j] >= '@' && bytes[j] <= '~'))) {
                    ++j;
                }
                if (j >= bytes.size()) {
                    break;
                }
                const std::string params = bytes.substr(i + 2, j - i - 2);
                if (bytes[j] == 'm') {
                    sgr(params);
                } else if (bytes[j] == 'H') {
                    const std::size_t semi = params.find(';');
                    row = (semi == std::string::npos ? 1 : std::stoll(params.substr(0, semi))) - 1;
                    col = (semi == std::string::npos ? 1 : std::stoll(params.substr(semi + 1))) - 1;
                }
                i = j + 1;
                continue;
            }
            std::size_t end = i + 1;
            while (end < bytes.size() && (static_cast<unsigned char>(bytes[end]) & 0xC0U) == 0x80U) {
                ++end;
            }
            if (col >= 0 && row >= 0 && col < cols && row < rows) {
                cells[static_cast<std::size_t>(row * cols + col)] =
                    Cell{bytes.substr(i, end - i), ink, ground, bold};
            }
            ++col;
            i = end;
        }
    }
};

#if defined(TILE_DEFENSE_HAS_SDL)
/// The screen drawn as a picture, `cell_w` by `cell_h` pixels a cell, in the GUI window's face.
bool write_screen_bmp(const Screen& s, const std::string& path, gw::SdlFace& face) {
    const gfx::TextMeasure m = face.measure();
    const int cw = static_cast<int>(m.advance), ch = static_cast<int>(m.line);
    SDL_Surface* surface = SDL_CreateSurface(static_cast<int>(s.cols) * cw,
                                             static_cast<int>(s.rows) * ch, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        return false;
    }
    SDL_Renderer* r = SDL_CreateSoftwareRenderer(surface);
    for (std::int64_t row = 0; row < s.rows; ++row) {
        for (std::int64_t col = 0; col < s.cols; ++col) {
            const Screen::Cell& c = s.cells[static_cast<std::size_t>(row * s.cols + col)];
            SDL_SetRenderDrawColor(r, static_cast<Uint8>(c.ground >> 16), static_cast<Uint8>(c.ground >> 8),
                                   static_cast<Uint8>(c.ground), 255);
            const SDL_FRect box{static_cast<float>(col * cw), static_cast<float>(row * ch),
                                static_cast<float>(cw), static_cast<float>(ch)};
            SDL_RenderFillRect(r, &box);
            if (c.glyph != " " && !c.glyph.empty()) {
                face.draw(r, box.x, box.y, (c.ink << 8) | 0xffu, c.glyph, cw, ch, 0,
                          c.bold ? gfx::text_weight::kBold : gfx::text_weight::kRegular);
            }
        }
    }
    SDL_RenderPresent(r);
    SDL_Surface* bgr = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_BGR24);
    bool ok = false;
    if (bgr != nullptr) {
        const std::string bmp = gw::bmp_of_bgr24(bgr->w, bgr->h, bgr->pitch,
                                                 static_cast<const std::uint8_t*>(bgr->pixels));
        std::ofstream(path, std::ios::binary) << bmp;
        ok = !bmp.empty();
        SDL_DestroySurface(bgr);
    }
    face.drop();
    SDL_DestroyRenderer(r);
    SDL_DestroySurface(surface);
    return ok;
}
#endif

// ---- one window's run -------------------------------------------------------------------------

/// What one window's busiest wave cost.
struct Costs {
    std::string window;
    std::vector<double> step_us;    ///< each step's beat to its picture shown
    std::vector<std::int64_t> drawn_bytes; ///< each step's drawings, as Loom carries them
    std::int64_t showings = 0, device_bytes = 0;
    std::int64_t textures_steady = 0;
    std::int64_t nodes = 0;
    std::int64_t walkers = 0, shots = 0, sparks = 0; ///< the most at once
    std::int64_t wave = 0;
};

/// Where the scripted player builds, in its order: towers along the road's legs.
constexpr std::pair<std::int64_t, std::int64_t> kPlan[] = {
    {10, 7}, {20, 8}, {6, 5}, {16, 12}, {26, 7}, {30, 11}, {10, 12}, {22, 3}, {33, 14},
    {6, 13}, {26, 13}, {37, 11}, {14, 17}, {30, 3}, {20, 17}, {33, 7}};

/// The steps of the busiest wave a picture is taken after: thirty-two, three steps apart, once the
/// wave is in full cry; and what takes it -- the window, and the picture's number.
constexpr std::int64_t kFirstPicture = 1150, kPictureEvery = 3, kPictures = 32;
template <class Window>
using Picture = std::function<void(Window&, std::int64_t)>;

template <class Window, class... Device>
Costs run(const std::string& window_name, const Picture<Window>& picture,
          const std::function<void()>& after_step, Device... device) {
    Costs costs;
    costs.window = window_name;
    loom::Switchboard bus;
    loom::Kernel kernel(bus, loom::trust_every_artifact("the bench loads only its own build's output"));
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);
    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    reach.allow_to_any(gfx::WindowShowAsked::zen_name, gfx::WindowShowAsked::zen_version);
    auto ears_made = std::make_unique<BenchEars>();
    BenchEars* ears = ears_made.get();
    const loom::WeaveId ears_id = bus.register_weave(std::move(ears_made), std::move(reach));
    ears->zen_set_self(ears_id);
    const loom::WeaveId door = loom::mount<BenchDoor>(bus, loom::host_lifecycle_authority(bus));
    const std::string dir = exe_dir();
    std::uint64_t corr = 1;
    const auto load = [&](const std::string& stem, const std::string& role) {
        bus.send_as(ears_id, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{stem, dir + "/" + stem + kSuffix, {role}}),
                                  ears_id, ears_id, corr++));
        bus.drain_until_idle();
    };
    const auto mount = [&](auto made, const std::string& role) {
        auto* raw = made.get();
        loom::Grant g = loom::emit_default_grant(*raw);
        const loom::WeaveId id = bus.register_weave(std::move(made), std::move(g), role);
        raw->zen_set_self(id);
        bus.send(door, loom::Message(loom::to_value(BenchActivate{static_cast<std::int64_t>(id.value)})));
        bus.drain_until_idle();
        return std::make_pair(raw, id);
    };
    load("tile-defense-stage", td::kStageRole);
    load("zengine-graphics", gfx::kGraphicsRole);
    const std::string window_role = std::string(gfx::kWindowRolePrefix) + window_name;
    auto [window, window_id] = mount(std::make_unique<Window>(std::move(device)...), window_role);
    auto [game, game_id] = mount(std::make_unique<td::TileDefense>(), td::kGameRole);
    (void)window;
    if (!ears->refused.empty()) {
        std::fprintf(stderr, "bench: a load was refused: %s\n", ears->refused.front().c_str());
        return costs;
    }
    const auto shown = [&]() {
        bus.send_as_to_role(ears_id, window_role,
                            loom::Message(loom::to_value(gfx::WindowShowAsked{}), ears_id, ears_id, corr++));
        bus.drain_until_idle();
        return ears->shown;
    };
    const auto key = [&](std::int64_t scancode) {
        bus.send_to_role(td::kGameRole, loom::Message(loom::to_value(
                                            gfx::AreaKeyPressed{td::kGameArea, scancode, ""})));
        bus.drain_until_idle();
    };
    const auto build_at = [&](std::int64_t c, std::int64_t r) {
        const td::TileWorld& w = game->world();
        while (w.cursor_col < c) key(in::scan::kRight);
        while (w.cursor_col > c) key(in::scan::kLeft);
        while (w.cursor_row < r) key(in::scan::kDown);
        while (w.cursor_row > r) key(in::scan::kUp);
        key(in::scan::kReturn);
    };
    // Drawings as Loom carries them, each step's bytes counted.
    std::int64_t drawn = 0;
    bool counting = false;
    const loom::ObserverId tap = bus.add_observer([&](const loom::BusEvent& e) {
        if (counting && e.kind == loom::EventKind::Delivered && e.payload != nullptr &&
            e.schema_name == loom::schema_of<gfx::Drawn>()->name()) {
            drawn += static_cast<std::int64_t>(loom::serialize(*e.payload).size());
        }
    });
    std::size_t planned = 0;
    std::int64_t steady_textures = -1;
    std::int64_t picture_steps = 0;
    for (std::int64_t step = 0; step < 200000; ++step) {
        const td::TileWorld& w = game->world();
        if (w.lives <= 0 || (w.wave >= td::kWaves && !w.walking)) {
            break;
        }
        // the player: build what the gold allows, the arrow tower and the fireball tower in turn
        if (planned < std::size(kPlan)) {
            const std::int64_t kind = static_cast<std::int64_t>(planned % 2);
            if (w.gold >= td::kTowers[static_cast<std::size_t>(kind)].cost) {
                key(kind == td::kArrowTower ? in::scan::kA : in::scan::kF);
                build_at(kPlan[planned].first, kPlan[planned].second);
                ++planned;
            }
        }
        if (!w.walking) {
            key(in::scan::kSpace);
        }
        const bool busiest = w.wave == td::kWaves && w.walking;
        if (busiest && steady_textures < 0) {
            const gfx::WindowShown s = shown();
            steady_textures = s.textures;
            costs.showings = -s.showings;
            costs.device_bytes = -s.device_bytes;
        }
        counting = busiest;
        drawn = 0;
        const auto t0 = std::chrono::steady_clock::now();
        bus.send_to_role(td::kGameRole, loom::Message(loom::to_value(td::TileTick{1})));
        bus.drain_until_idle();
        const auto t1 = std::chrono::steady_clock::now();
        if (busiest) {
            costs.step_us.push_back(
                std::chrono::duration<double, std::micro>(t1 - t0).count());
            costs.drawn_bytes.push_back(drawn);
            costs.walkers = std::max(costs.walkers, static_cast<std::int64_t>(w.enemies.size()));
            costs.shots = std::max(costs.shots, static_cast<std::int64_t>(w.shots.size()));
            costs.sparks = std::max(costs.sparks, static_cast<std::int64_t>(w.sparks.size()));
            ++picture_steps;
        }
        if (after_step) {
            after_step();
        }
        const std::int64_t since = picture_steps - kFirstPicture;
        if (picture && busiest && since >= 0 && since < kPictures * kPictureEvery &&
            since % kPictureEvery == 0) {
            picture(*window, since / kPictureEvery);
        }
    }
    counting = false;
    bus.remove_observer(tap);
    const gfx::WindowShown s = shown();
    costs.showings += s.showings;
    costs.device_bytes += s.device_bytes;
    costs.textures_steady = steady_textures < 0 ? 0 : s.textures - steady_textures;
    costs.nodes = s.nodes;
    costs.wave = game->world().wave;
    (void)game_id;
    (void)window_id;
    return costs;
}

double quantile(std::vector<double> v, double q) {
    if (v.empty()) {
        return 0;
    }
    std::sort(v.begin(), v.end());
    const auto at = static_cast<std::size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
    return v[std::min(at, v.size() - 1)];
}

void say(const Costs& c) {
    std::vector<double> bytes(c.drawn_bytes.begin(), c.drawn_bytes.end());
    std::printf("%s: wave %lld, %zu steps of the busiest; step %.1f us median, %.1f us p95; drawn "
                "%.0f B median, %.0f B p95; %lld showings, %lld device bytes (%.0f B a showing); "
                "%lld textures made once steady; %lld nodes; at most %lld walkers, %lld shots, "
                "%lld sparks\n",
                c.window.c_str(), static_cast<long long>(c.wave), c.step_us.size(),
                quantile(c.step_us, 0.5), quantile(c.step_us, 0.95), quantile(bytes, 0.5),
                quantile(bytes, 0.95), static_cast<long long>(c.showings),
                static_cast<long long>(c.device_bytes),
                c.showings > 0 ? static_cast<double>(c.device_bytes) / static_cast<double>(c.showings) : 0.0,
                static_cast<long long>(c.textures_steady), static_cast<long long>(c.nodes),
                static_cast<long long>(c.walkers), static_cast<long long>(c.shots),
                static_cast<long long>(c.sparks));
}

} // namespace

int main(int argc, char** argv) {
    std::string which = "both", pictures;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--tui" || arg == "--gui") {
            which = arg.substr(2);
        } else if (arg == "--pictures" && i + 1 < argc) {
            pictures = argv[++i];
        } else {
            std::fprintf(stderr, "usage: zengine-tile-defense-bench [--tui|--gui] [--pictures <dir>]\n");
            return 2;
        }
    }
    int failed = 0;
    if (which != "gui") {
        // THE TERMINAL: what it is written laid over its screen after each step, so a picture of it
        // is what a terminal shows of those bytes.
        KeptBytes kept;
        Screen screen;
        using TuiWindow = gw::WindowT<KeepingTerminal>;
        Picture<TuiWindow> picture;
#if defined(TILE_DEFENSE_HAS_SDL)
        gw::SdlFace face;
        if (!pictures.empty()) {
            face.open();
            picture = [&](TuiWindow&, std::int64_t n) {
                (void)write_screen_bmp(screen, pictures + "/tui-" + std::to_string(n) + ".bmp", face);
            };
        }
#endif
        const auto after = [&]() {
            if (!pictures.empty()) {
                screen.take(kept.written);
            }
            kept.written.clear();
        };
        Costs c = run<TuiWindow>(gfx::kTuiWindow, picture, after, KeepingTerminal{&kept});
        say(c);
        failed += c.step_us.empty() ? 1 : 0;
    }
#if defined(TILE_DEFENSE_HAS_SDL)
    if (which != "tui") {
        // THE GUI WINDOW: its picture as SDL drew it, read back.
        using GuiWindow = gw::SdlWindowT<gw::ProcessSdlBackend>;
        Picture<GuiWindow> picture;
        if (!pictures.empty()) {
            picture = [&](GuiWindow& w, std::int64_t n) {
                if (const auto shot = w.capture(); shot && shot->format == "image/bmp") {
                    std::ofstream(pictures + "/gui-" + std::to_string(n) + ".bmp", std::ios::binary)
                        << shot->bytes;
                }
            };
        }
        Costs c = run<GuiWindow>(gfx::kGuiWindow, picture, std::function<void()>());
        say(c);
        failed += c.step_us.empty() ? 1 : 0;
    }
#endif
    return failed == 0 ? 0 : 1;
}
