// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_TUI_TERMINAL_HPP
#define ZENGINE_SURFACE_TUI_TERMINAL_HPP

// A terminal's output side, as whoever paints it claims it: the modes a painter takes and gives
// back, the ink for each role, and the claim itself, held for exactly its owner's lifetime. Read
// by the terminal Skins and by a terminal window. Surface law: agents/surface.md

#include "terminal_size.hpp"

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

#include <cstdio>
#include <string>
#include <string_view>

namespace zengine::surface {

/// This medium's ink for each canvas role; an unknown role paints as `kFill`. Plain ink is the
/// terminal's own text colour, as its own ground is no ground byte, so plain text reads on
/// whatever the terminal wears, light or dark, and reverse video swaps the two.
inline const char* sgr_for_role(int role) noexcept {
    switch (role) {
    case 1: return "\x1b[36m";    // kAccent — cyan: the thing being pointed at
    case 2: return "\x1b[90m";    // kMuted  — bright black: present, quiet
    case 3: return "\x1b[31;1m";  // kAlert  — bold red: must be seen
    case 4: return "\x1b[30m";    // kGround — black: opaque empty material
    case 6: return "\x1b[95m";    // kGuide — bright magenta: a guest's mark
    default: return "\x1b[39m";   // kFill, kMediumGround (no ink) and anything unknown — plain ink
    }
}

/// The terminal modes a TUI Skin claims, as bytes a suite can read. Pointer reporting is asked
/// for on the output stream, the Skin's, so it lives and dies with the alternate screen and
/// Input need not be told: it parses an SGR report whenever one arrives (on POSIX, no pointer
/// without a TUI Skin). `1002` reports presses, releases and drags, never idle motion; `1006`
/// is SGR coordinates, the only encoding past column 223. A SIGKILLed process restores nothing.
// WL-PTR-09 -- agents/workshop/pointer.md
inline constexpr const char* kTuiPointerOn = "\x1b[?1002h\x1b[?1006h";
inline constexpr const char* kTuiPointerOff = "\x1b[?1006l\x1b[?1002l";

/// What a TUI Skin writes when it takes the terminal. `pointer` is false on a
/// backend whose pointer arrives some other way — the Win32 console delivers
/// mouse INPUT_RECORDs to the Input weave's own reader, so asking it for
/// in-band reports would be asking twice for one thing.
inline std::string tui_enter_sequence(bool pointer) {
    std::string s = "\x1b[?1049h\x1b[?25l\x1b[2J";
    if (pointer) {
        s += kTuiPointerOn;
    }
    return s;
}

/// What it writes when it gives the terminal back: everything `enter` claimed,
/// released in the opposite order.
inline std::string tui_leave_sequence(bool pointer) {
    std::string s;
    if (pointer) {
        s += kTuiPointerOff;
    }
    s += "\x1b[?25h\x1b[?1049l";
    return s;
}

/// Whether this build's terminal delivers its pointer in band. POSIX: yes, as
/// SGR reports on stdin. Win32 console: no — the console hands the Input
/// weave's reader real MOUSE_EVENT records instead.
inline constexpr bool kTuiPointerIsInBand =
#if defined(_WIN32)
    false;
#else
    true;
#endif

/// The terminal's output side, claimed for exactly the Skin's lifetime -- alternate screen,
/// hidden cursor, in-band pointer reporting, and on Windows VT processing and the UTF-8
/// codepage -- and restored whole on destruction. With no console (redirected, headless) the
/// claim is skipped and frames still go to stdout. The input side stays the Input weave's.
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


} // namespace zengine::surface

#endif // ZENGINE_SURFACE_TUI_TERMINAL_HPP
