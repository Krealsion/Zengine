// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-app: an app's own host (app/docs/app.md). `zengine-app <app folder>` runs the folder's
// plan with no Workshop, each artifact waiting on this console until it is decided there in Loom's
// `authority` grammar, and goes on until the app's window is closed or `quit` is said here.

#include "app/host.hpp"
#include "app/program.hpp"

#include <zen/host/line_input.hpp>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

constexpr const char* kLibrarySuffix =
#if defined(_WIN32)
    ".dll";
#elif defined(__APPLE__)
    ".dylib";
#else
    ".so";
#endif

/// WHERE THE ARTIFACTS ARE: beside this program, where a build stages them, else the installed
/// package's artifact directory, which stands at `ZENGINE_APP_ARTIFACTS` from this program's.
std::string artifact_dir(const std::string& here) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::exists(fs::path(here) / (std::string("zengine-app-weave") + kLibrarySuffix), ec)) {
        return here;
    }
    return (fs::path(here) / ZENGINE_APP_ARTIFACTS).lexically_normal().generic_string();
}

/// How long the console waits for a line when the bus has nothing to do, in milliseconds.
constexpr int kIdleWaitMs = 50;

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fputs("usage: zengine-app <app folder>\n", stderr);
        return 2;
    }
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path folder = fs::absolute(fs::path(argv[1]), ec).lexically_normal();
    if (ec || !fs::is_directory(folder, ec)) {
        std::fprintf(stderr, "zengine-app - %s is not a folder\n", argv[1]);
        return 2;
    }

    loom::Switchboard bus;
    const std::string artifacts = artifact_dir(zengine::app::program_dir());
    zengine::app::AppHost host(
        bus, zengine::app::AppPlace{folder.generic_string(), artifacts, kLibrarySuffix}, std::cout);
    std::string why;
    if (!host.open(&why)) {
        std::cout << "zengine-app - " << why << "\nzengine-app - nothing was loaded.\n";
        return 3;
    }
    host.begin();

    // The console: a turn of the bus, then a line if one is whole. The bus is never drained,
    // since a live Timer re-arms its beat inside its own handler (`pump_pending`, MSG-09).
    loom::host::LineInput console;
    bool console_open = true;
    while (!host.ended()) {
        bus.pump_pending();
        host.turned();
        if (host.ended()) {
            break;
        }
        const bool idle = bus.pending() == 0;
        // A load in flight is answered within turns of the bus: the next line waits for it, so a
        // line typed ahead is read against the plan as that load left it.
        if (!idle && host.executor().state() == zengine::workshop::load::Realization::Loading) {
            continue;
        }
        if (!console_open) {
            if (idle) {
                // Nothing will speak again and nobody can answer: a plan still waiting on the
                // console did not run, which the exit says.
                const bool waiting = host.executor().state() ==
                                     zengine::workshop::load::Realization::Waiting;
                std::cout << "zengine-app - the console's input ended and nothing here will "
                             "speak again: ending"
                          << (waiting ? ", its plan still waiting on " + host.executor().waiting_on()
                                      : std::string())
                          << ".\n";
                std::cout.flush();
                return waiting ? 5 : host.exit_code();
            }
            continue;
        }
        std::string line;
        switch (console.read(&line, idle ? kIdleWaitMs : 0)) {
        case loom::host::LineInput::Status::Line: host.command(line); break;
        case loom::host::LineInput::Status::TooLong:
            std::cout << "zengine-app - line " << console.too_long().line << " is longer than "
                      << loom::host::kMaxCommandBytes << " bytes and was not read\n";
            break;
        case loom::host::LineInput::Status::Closed:
            console_open = false;
            std::cout << "zengine-app - the console's input ended; the app runs on until its "
                         "window is closed\n";
            break;
        case loom::host::LineInput::Status::Idle: break;
        }
        std::cout.flush();
    }
    std::cout.flush();
    return host.exit_code();
}
