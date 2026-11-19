// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// zengine-tile-defense, the game's host: it owns the boot list and its loop's rest, and nothing
// else -- it draws nothing, knows no rules, reads no keys and keeps no time. It loads the stage,
// the graphics directory, one window -- the GUI window, or with `--tui` the TUI window in the
// terminal it was started from -- the game and the Timer, whose loading starts time; and it ends
// when the game says its player quit or the window says its close box was pressed.
// Reference: examples/tile-defense/README.md.

#include "host_shapes.hpp"
#include "vocabulary.hpp"

#include "graphics/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/host/reactor.hpp>
#include <zen/kernel/admission.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

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

#include <cstdint>
#include <cstdio>
#include <memory>
#include <utility>
#include <string>
#include <vector>

namespace {

namespace gfx = zengine::graphics;
namespace td = zengine::tile_defense;
namespace timer = zengine::timer;

/// What the door was told: whether to stop, and the loads that were refused.
struct DoorHeard {
    bool stop = false;
    std::vector<std::string> refused;
};

/// THE HOST'S DOOR: it asks the manager to load each weave, and hears the end -- the game's quit,
/// or the window's close box -- and a load the manager refused.
class Door : public loom::WeaveBase<Door, td::TileDoorState,
                                    loom::Accept<loom::Result, loom::Refused, td::TileQuit,
                                                 gfx::SurfaceCloseRequested>,
                                    loom::Emit<loom::LoadWeave>> {
public:
    Door(DoorHeard& heard, std::string window_role)
        : heard_(&heard), window_role_(std::move(window_role)) {}
    void on(const loom::Result&, loom::Mail&) { ++state_.heard; }
    void on(const loom::Refused& r, loom::Mail&) {
        heard_->refused.push_back(r.reason);
        heard_->stop = true;
    }
    void on(const td::TileQuit&, loom::Mail& mail) {
        if (mail.authored_from_role(td::kGameRole)) {
            heard_->stop = true;
        }
    }
    void on(const gfx::SurfaceCloseRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(window_role_)) {
            heard_->stop = true;
        }
    }

private:
    DoorHeard* heard_;
    std::string window_role_;
};

std::string exe_dir() {
#if defined(_WIN32)
    std::string path(MAX_PATH, '\0');
    for (;;) {
        const DWORD n = ::GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (n == 0) {
            return ".";
        }
        if (n < path.size()) {
            path.resize(n);
            break;
        }
        if (path.size() > std::size_t{3} * 32767) {
            return ".";
        }
        path.resize(path.size() * 2);
    }
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

/// The platform's loadable-library suffix.
constexpr const char* kWeaveSuffix =
#if defined(_WIN32)
    ".dll";
#else
    ".so";
#endif

} // namespace

int main(int argc, char** argv) {
    bool tui = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--tui") {
            tui = true;
        } else {
            std::fprintf(stderr, "usage: zengine-tile-defense [--tui]\n");
            return 2;
        }
    }
    const std::string window_name = tui ? gfx::kTuiWindow : gfx::kGuiWindow;
    const std::string window_role = std::string(gfx::kWindowRolePrefix) + window_name;
    const std::string window_weave = tui ? "zengine-window-tui" : "zengine-window-sdl";
    const std::string dir = exe_dir();
    const auto so = [&dir](const std::string& stem) { return dir + "/" + stem + kWeaveSuffix; };

    loom::Switchboard bus;
    loom::Kernel kernel(bus, loom::trust_every_artifact(
                                 "the tile defense loads only its own build's output"));
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    DoorHeard heard;
    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    const loom::WeaveId door = loom::mount_granted<Door>(bus, std::move(reach), heard, window_role);

    // THE BOOT LIST, in its order: the stage first, so it hears the directory's and the window's
    // offers; then the directory and the window; the game; and the Timer last, whose loading
    // starts time.
    std::uint64_t corr = 1;
    const auto load = [&](const std::string& stem, const std::string& role) {
        bus.send_as(door, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{stem, so(stem), {role}}), door,
                                  door, corr++));
    };
    load("tile-defense-stage", td::kStageRole);
    load("zengine-graphics", gfx::kGraphicsRole);
    load(window_weave, window_role);
    load("tile-defense", td::kGameRole);
    load("zengine-timer", timer::kTimerRole);

    // The whole game runs in this loop: a turn, then one wait on the window and the Timer's time.
    loom::host::Reactor wait(bus);
    wait.trust(window_role);
    wait.trust(timer::kTimerRole);
    while (!heard.stop) {
        (void)bus.pump_pending();
        if (heard.stop) {
            break;
        }
        const bool idle = bus.pending() == 0;
        const loom::host::Reactor::Woke woke = idle ? wait.after_turn() : wait.after_turn(0);
        if (idle && bus.pending() == 0 && woke.roles == 0 && woke.due == 0) {
            std::fprintf(stderr, "zengine-tile-defense - nothing waits to speak: exiting.\n");
            break;
        }
    }
    for (const std::string& why : heard.refused) {
        std::fprintf(stderr, "zengine-tile-defense - a load was refused: %s\n", why.c_str());
    }
    return heard.refused.empty() ? 0 : 1;
}
