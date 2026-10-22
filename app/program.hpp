// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_APP_PROGRAM_HPP
#define ZENGINE_APP_PROGRAM_HPP

// Where the running program stands, and the app host's program beside it (app/docs/app.md):
// `zengine-app` stands beside Workshop's program, in a build tree and in the installed package, so
// what Workshop writes or starts for an app names the program beside the one running.

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
#include <string>

namespace zengine::app {

/// The app host's program, as its file is named.
inline constexpr const char* kAppProgram =
#if defined(_WIN32)
    "zengine-app.exe";
#else
    "zengine-app";
#endif

/// The directory the running program stands in; `.` when the system cannot say.
inline std::string program_dir() {
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

/// The app host's program beside the running one.
inline std::string app_program() { return program_dir() + "/" + kAppProgram; }

} // namespace zengine::app

#endif // ZENGINE_APP_PROGRAM_HPP
