// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-app: an app's own host (app/docs/app.md). `zengine-app <app folder>` runs the folder's
// plan with no Workshop, each artifact waiting on this console until it is decided there in Loom's
// `authority` grammar, and goes on until the app's window is closed or `quit` is said here.

#include "app/host.hpp"

#include <zen/host/line_input.hpp>

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

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

/// The directory this program stands in.
std::string exe_dir() {
#if defined(_WIN32)
    // In the UTF-8 code page a path's bytes can outnumber its UTF-16 units, so the buffer grows
    // until the whole path fits: up to three bytes for each of a path's 32,767 units at most.
    std::string path(MAX_PATH, '\0');
    for (;;) {
        const DWORD n =
            ::GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
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
    std::string path(buf);
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
#endif
}

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
    zengine::app::AppHost host(
        bus,
        zengine::app::AppPlace{folder.generic_string(), artifact_dir(exe_dir()), kLibrarySuffix},
        std::cout);
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
