// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_TERMINAL_IO_HPP
#define ZENGINE_GRAPHICS_WINDOW_TERMINAL_IO_HPP

// A terminal's output side and its size, as whoever paints it claims it: the one place this
// repository asks an operating system how big a terminal is (the only `#if defined(_WIN32)` the
// terminal medium needs), the modes a painter takes and gives back, the ink for each role, the
// copy sequence, and the claim itself, held for exactly its owner's lifetime. Read by the terminal
// window. Asked, not watched: no SIGWINCH handler and no thread, since a painter asks on its beat,
// which is prompt enough. Reference: graphics/docs/zengine.window.terminal.md.

#include "graphics/role.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

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
#else
#include <unistd.h>
#endif

namespace zengine::graphics::window {

/// A terminal's visible window, in character cells. Non-positive means there is no terminal to
/// ask -- a pipe, a file, CI, no console, an unsupported platform. The absence is the value, not a
/// flag beside it: a flag fails open when an API answers successfully with a degenerate window.
/// `measured()` tests it.
struct TerminalSize {
    std::int64_t cols = 0;
    std::int64_t rows = 0;

    /// Whether an operating system actually answered with a window that exists.
    constexpr bool measured() const noexcept { return cols > 0 && rows > 0; }
};

/// Ask this process's standard output how big the terminal on the other end is: stdout is the
/// stream a terminal painter paints into, so a piped run has no terminal to fit. No environment
/// variable is consulted (`COLUMNS`/`LINES` are a shell's stale idea). POSIX:
/// `ioctl(TIOCGWINSZ)`, which fails on a non-terminal. Windows: `GetConsoleScreenBufferInfo`'s
/// visible window (`srWindow`), never `dwSize`, the 9,001-row scrollback buffer. Anywhere else:
/// `{}`.
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

/// This medium's ink for each role; an unknown role paints as `kFill`. Plain ink is the
/// terminal's own text colour, as its own ground is no ground byte, so plain text reads on
/// whatever the terminal wears, light or dark, and reverse video swaps the two.
inline const char* sgr_for_role(int r) noexcept {
    switch (r) {
    case role::kAccent: return "\x1b[36m";   // cyan: the thing being pointed at
    case role::kMuted: return "\x1b[90m";    // bright black: present, quiet
    case role::kAlert: return "\x1b[31;1m";  // bold red: must be seen
    case role::kGround: return "\x1b[30m";   // black: opaque empty material
    case role::kGuide: return "\x1b[95m";    // bright magenta: a guest's mark
    default: return "\x1b[39m"; // kFill, kMediumGround (no ink) and anything unknown: plain ink
    }
}

/// The terminal modes a painter claims, as bytes a suite can read. Pointer reporting is asked
/// for on the output stream, the painter's, so it lives and dies with the alternate screen and
/// Input need not be told: it parses an SGR report whenever one arrives (on POSIX, no pointer
/// without a painter holding the terminal). `1002` reports presses, releases and drags, never idle
/// motion; `1006` is SGR coordinates, the only encoding past column 223. A SIGKILLed process
/// restores nothing.
// WL-PTR-09 -- agents/workshop/pointer.md
inline constexpr const char* kTuiPointerOn = "\x1b[?1002h\x1b[?1006h";
inline constexpr const char* kTuiPointerOff = "\x1b[?1006l\x1b[?1002l";

/// What a painter writes when it takes the terminal. `pointer` is false on a backend whose
/// pointer arrives some other way -- the Win32 console delivers mouse INPUT_RECORDs to the Input
/// weave's own reader, so asking it for in-band reports would be asking twice for one thing.
inline std::string tui_enter_sequence(bool pointer) {
    std::string s = "\x1b[?1049h\x1b[?25l\x1b[2J";
    if (pointer) {
        s += kTuiPointerOn;
    }
    return s;
}

/// What it writes when it gives the terminal back: everything `enter` claimed, released in the
/// opposite order.
inline std::string tui_leave_sequence(bool pointer) {
    std::string s;
    if (pointer) {
        s += kTuiPointerOff;
    }
    s += "\x1b[?25h\x1b[?1049l";
    return s;
}

/// Whether this build's terminal delivers its pointer in band. POSIX: yes, as SGR reports on
/// stdin. Win32 console: no -- the console hands the Input weave's reader real MOUSE_EVENT
/// records instead.
inline constexpr bool kTuiPointerIsInBand =
#if defined(_WIN32)
    false;
#else
    true;
#endif

/// Bytes in standard base64 (RFC 4648, padded with `=`), the one alphabet OSC 52 reads.
inline std::string terminal_base64(std::string_view bytes) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto at = [&bytes](std::size_t i) {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i]));
    };
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);
    std::size_t i = 0;
    for (; i + 3 <= bytes.size(); i += 3) {
        const std::uint32_t n = (at(i) << 16) | (at(i + 1) << 8) | at(i + 2);
        out.push_back(kAlphabet[(n >> 18) & 0x3Fu]);
        out.push_back(kAlphabet[(n >> 12) & 0x3Fu]);
        out.push_back(kAlphabet[(n >> 6) & 0x3Fu]);
        out.push_back(kAlphabet[n & 0x3Fu]);
    }
    const std::size_t left = bytes.size() - i;
    if (left == 0) {
        return out;
    }
    const std::uint32_t n = (at(i) << 16) | (left == 2 ? at(i + 1) << 8 : 0u);
    out.push_back(kAlphabet[(n >> 18) & 0x3Fu]);
    out.push_back(kAlphabet[(n >> 12) & 0x3Fu]);
    out.push_back(left == 2 ? kAlphabet[(n >> 6) & 0x3Fu] : '=');
    out.push_back('=');
    return out;
}

/// A copy, in a terminal's one voice for it: OSC 52 set-clipboard, written on the stream the
/// painter owns. Whether the terminal honours it cannot be asked (xterm wants `allowWindowOps`; a
/// pipe is no terminal), so writing it claims nothing about the system's clipboard.
inline std::string terminal_clipboard_sequence(std::string_view text) {
    return "\x1b]52;c;" + terminal_base64(text) + "\x07";
}

/// The terminal's output side, claimed for exactly its owner's lifetime -- alternate screen,
/// hidden cursor, in-band pointer reporting, and on Windows VT processing and the UTF-8 codepage
/// -- and restored whole on destruction. With no console (redirected, headless) the claim is
/// skipped and frames still go to stdout. The input side stays the Input weave's.
class TuiTerminal {
public:
#if defined(_WIN32)
    TuiTerminal() {
        out_ = ::GetStdHandle(STD_OUTPUT_HANDLE);
        ok_ = out_ != INVALID_HANDLE_VALUE && ::GetConsoleMode(out_, &saved_out_) != 0;
        if (!ok_) {
            return;
        }
        ::SetConsoleMode(out_, saved_out_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        saved_cp_ = ::GetConsoleOutputCP();
        ::SetConsoleOutputCP(CP_UTF8);
        enter();
    }
    ~TuiTerminal() {
        if (!ok_) {
            return;
        }
        leave();
        if (saved_cp_ != 0) {
            ::SetConsoleOutputCP(saved_cp_);
        }
        ::SetConsoleMode(out_, saved_out_);
    }
#else
    TuiTerminal() {
        ok_ = ::isatty(STDOUT_FILENO) == 1;
        if (ok_) {
            enter();
        }
    }
    ~TuiTerminal() {
        if (ok_) {
            leave();
        }
    }
#endif
    TuiTerminal(const TuiTerminal&) = delete;
    TuiTerminal& operator=(const TuiTerminal&) = delete;

    void write(std::string_view s) {
        std::fwrite(s.data(), 1, s.size(), stdout);
        std::fflush(stdout);
    }

    /// How big the terminal is now, gated on the same `ok_` as the claim, so a run that found
    /// no terminal to take has none to measure. The OS question is `native_terminal_size()`'s.
    TerminalSize size() const { return ok_ ? native_terminal_size() : TerminalSize{}; }

private:
    static void enter() {
        const std::string s = tui_enter_sequence(kTuiPointerIsInBand);
        std::fwrite(s.data(), 1, s.size(), stdout);
        std::fflush(stdout);
    }
    static void leave() {
        const std::string s = tui_leave_sequence(kTuiPointerIsInBand);
        std::fwrite(s.data(), 1, s.size(), stdout);
        std::fflush(stdout);
    }

#if defined(_WIN32)
    HANDLE out_ = nullptr;
    DWORD saved_out_ = 0;
    UINT saved_cp_ = 0;
#endif
    bool ok_ = false;
};

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_TERMINAL_IO_HPP
