// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_TERMINAL_SIZE_HPP
#define ZENGINE_SURFACE_TERMINAL_SIZE_HPP

// THE ONE PLACE THIS REPOSITORY ASKS AN OPERATING SYSTEM HOW BIG A TERMINAL IS.
//
// It is a header of its own, and small on purpose: everything above it —
// `TuiTerminal`, `TuiMedium`, the Skin shell, Workshop — is platform-neutral, and
// the `#if defined(_WIN32)` below is the only one any of them needs. That is the
// shape the SDL medium already has one floor up (a window whose size is its own
// to read), arriving at the medium that owns a stream instead of a drawable.
//
// WHAT IT ANSWERS IS THE TERMINAL'S OWN SIZE, in its own character cells, and
// nothing about what a canvas may do with it. The layout convention — which rows
// a TUI Skin spends on being a TUI Skin — belongs to the medium that holds it and
// is applied in skin_tui.hpp, so this function stays a measurement rather than a
// policy. Two questions, two owners, one call site each.
//
// IT IS ASKED, NOT WATCHED. There is no SIGWINCH handler here, no thread and no
// registration: a caller asks when it is convenient to ask, which for a Skin is
// its ordinary pump beat. A resize does not need to interrupt a program; it needs
// to be noticed promptly, and 10ms is prompt. (`SkinT::report_extent` is what
// turns a stream of identical answers into silence — see skin.hpp.)

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

/// A terminal's visible window, in character cells.
///
/// NON-POSITIVE IS "THERE IS NO TERMINAL TO ASK", and it is deliberately the same
/// word `SurfaceExtent{}` already uses for "no opinion" — one spelling for absence,
/// used twice, so a value can never be half-present. Redirected output, a pipe, a
/// file, a CI runner, a process with no console and a platform whose API this
/// repository does not implement all arrive here as the same `{}`.
///
/// THE ABSENCE IS THE VALUE AND NOT A FLAG BESIDE IT. A `bool measured` would be a
/// marker whose mere presence reads as truth, and the failure mode of such a marker
/// is that it fails OPEN: a query that returned `{measured = true, cols = 0}` — which
/// is what a console API answering successfully with a degenerate window looks like —
/// would be believed by anything that tested the flag instead of the number. Every
/// caller here tests the number, and `measured()` is that test written once.
struct TerminalSize {
    std::int64_t cols = 0;
    std::int64_t rows = 0;

    /// Whether an operating system actually answered with a window that exists.
    constexpr bool measured() const noexcept { return cols > 0 && rows > 0; }
};

/// ASK THIS PROCESS'S OWN STANDARD OUTPUT HOW BIG THE TERMINAL ON THE OTHER END IS.
///
/// Standard output rather than standard input or `/dev/tty`, because standard output
/// is the stream a TUI Skin actually paints into: a run whose output is a pipe has no
/// terminal to fit, whatever is attached to its keyboard. It opens nothing, spawns
/// nothing, and consults no environment variable — `COLUMNS`/`LINES` are a shell's
/// idea of a size at the moment it exported them, not the terminal's idea of its size
/// now, and a stale one is worse than an honest absence.
///
/// POSIX: `ioctl(TIOCGWINSZ)`, the kernel's own record of the window size, which is
/// what a terminal emulator writes when a person drags an edge and what the kernel
/// raises SIGWINCH about. `isatty` is not consulted here — a descriptor that is not a
/// terminal fails the ioctl, which is the same answer arrived at with one call
/// instead of two.
///
/// WINDOWS: `GetConsoleScreenBufferInfo`, and the VISIBLE WINDOW (`srWindow`) rather
/// than `dwSize`. That distinction is the whole trap on this platform: `dwSize` is the
/// SCROLLBACK BUFFER, which on a stock console is 9,001 rows tall and has nothing to
/// do with how much a person can see. A medium that believed it would report a
/// nine-thousand-row surface into a thirty-row window.
///
/// NEITHER: a platform this repository has no console API for answers `{}`, which is
/// the honest sentence rather than a guess, and every consumer already has a path for
/// it because a pipe takes the same one.
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
    // Inclusive bounds, so the count is the difference plus one. Computed in the API's
    // own SHORTs widened to this repository's integer, and negative or empty windows
    // fall out as non-positive -- which `measured()` reads as absence, so a degenerate
    // console needs no second rule.
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
