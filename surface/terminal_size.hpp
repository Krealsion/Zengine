// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_TERMINAL_SIZE_HPP
#define ZENGINE_SURFACE_TERMINAL_SIZE_HPP

// The one place this repository asks an operating system how big a terminal is: the only
// `#if defined(_WIN32)` the terminal medium needs. It measures the terminal's own size in
// character cells; which rows a TUI Skin spends is skin_tui.hpp's. Asked, not watched: no
// SIGWINCH handler and no thread, since a Skin asks on its beat, which is prompt enough.
// Reference: docs/reference/surface.md.

#include <cstdint>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace zengine::surface {

/// A terminal's visible window, in character cells. Non-positive means there is no terminal to
/// ask -- a pipe, a file, CI, no console, an unsupported platform -- the same `{}` a
/// `SurfaceExtent` uses for no opinion. The absence is the value, not a flag beside it: a flag
/// fails open when an API answers successfully with a degenerate window. `measured()` tests it.
struct TerminalSize {
    std::int64_t cols = 0;
    std::int64_t rows = 0;

    /// Whether an operating system actually answered with a window that exists.
    constexpr bool measured() const noexcept { return cols > 0 && rows > 0; }
};

/// Ask this process's standard output how big the terminal on the other end is: stdout is the
/// stream a TUI Skin paints into, so a piped run has no terminal to fit. No environment variable
/// is consulted (`COLUMNS`/`LINES` are a shell's stale idea). POSIX: `ioctl(TIOCGWINSZ)`, which
/// fails on a non-terminal. Windows: `GetConsoleScreenBufferInfo`'s visible window (`srWindow`),
/// never `dwSize`, the 9,001-row scrollback buffer. Anywhere else: `{}`.
inline TerminalSize native_terminal_size() noexcept {
#if defined(_WIN32)
    const HANDLE out = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == nullptr || out == INVALID_HANDLE_VALUE) {
        return TerminalSize{};
    }
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (::GetConsoleScreenBufferInfo(out, &info) == 0) {
        return TerminalSize{}; // not a console (a pipe, a file, a detached process)
    }
    // Inclusive bounds, so the count is the difference plus one; a negative or empty window
    // falls out non-positive, which `measured()` already reads as absence.
    return TerminalSize{static_cast<std::int64_t>(info.srWindow.Right) -
                            static_cast<std::int64_t>(info.srWindow.Left) + 1,
                        static_cast<std::int64_t>(info.srWindow.Bottom) -
                            static_cast<std::int64_t>(info.srWindow.Top) + 1};
#elif defined(__unix__) || defined(__APPLE__)
    struct winsize ws {};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0) {
        return TerminalSize{}; // redirected, piped, or no controlling terminal
    }
    return TerminalSize{static_cast<std::int64_t>(ws.ws_col),
                        static_cast<std::int64_t>(ws.ws_row)};
#else
    return TerminalSize{};
#endif
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_TERMINAL_SIZE_HPP
