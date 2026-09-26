// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Input weave library for the terminal and the Win32 console: only the platform edge. Each
// reader fetches native events and hands them to the pure translators in translate.hpp; the
// weave (input_weave.hpp) publishes what comes back. A reader owns the platform's input-side
// state for exactly the weave's lifetime (raw mode on POSIX; on Windows the console input mode,
// quick-edit off and mouse records on): construction engages it, destruction restores it.
// Surface law: agents/surface.md

// The output side (alternate screen, VT processing) is the Skin's ground. With no console
// (redirected stdin, headless ctest, a pipe) the mode setup fails, the reader stays disabled,
// and poll() yields nothing. The SDL reader is a separate artifact (input_sdl.cpp).

#include "input_weave.hpp"

#include <zen/kernel/export.hpp>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using namespace zengine::input;

#if defined(_WIN32)

class ConsoleReader {
public:
    ConsoleReader() {
        in_ = ::GetStdHandle(STD_INPUT_HANDLE);
        ok_ = in_ != INVALID_HANDLE_VALUE && ::GetConsoleMode(in_, &saved_) != 0;
        if (!ok_) {
            return;
        }
        ::SetConsoleMode(in_, ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT);
    }
    ~ConsoleReader() {
        if (ok_) {
            ::SetConsoleMode(in_, saved_);
        }
    }
    ConsoleReader(const ConsoleReader&) = delete;
    ConsoleReader& operator=(const ConsoleReader&) = delete;

    /// ReadConsoleInputW, not ...A: the W record's uChar.UnicodeChar is the
    /// layout's own answer to "what did this keystroke type", and it is the only
    /// reason `%` can reach an application without anyone deducing it from a key
    /// identity. The A record would narrow that answer to the console codepage
    /// before this process ever saw it.
    std::vector<InputEvent> poll() {
        std::vector<InputEvent> out;
        if (!ok_) {
            return out;
        }
        DWORD pending = 0;
        while (::GetNumberOfConsoleInputEvents(in_, &pending) != 0 && pending > 0) {
            INPUT_RECORD rec;
            DWORD got = 0;
            if (::ReadConsoleInputW(in_, &rec, 1, &got) == 0 || got == 0) {
                break;
            }
            std::vector<InputEvent> batch;
            if (rec.EventType == KEY_EVENT) {
                const KEY_EVENT_RECORD& k = rec.Event.KeyEvent;
                batch = win32_key_to_events(keys_, static_cast<std::uint16_t>(k.wVirtualKeyCode),
                                            static_cast<std::uint32_t>(k.uChar.UnicodeChar),
                                            k.bKeyDown != 0,
                                            static_cast<std::uint32_t>(k.dwControlKeyState));
            } else if (rec.EventType == MOUSE_EVENT) {
                const MOUSE_EVENT_RECORD& m = rec.Event.MouseEvent;
                batch = win32_mouse_to_events(track_, m.dwMousePosition.X, m.dwMousePosition.Y,
                                              static_cast<std::uint32_t>(m.dwButtonState),
                                              static_cast<std::uint32_t>(m.dwEventFlags),
                                              static_cast<std::uint32_t>(m.dwControlKeyState));
            }
            out.insert(out.end(), batch.begin(), batch.end());
        }
        return out;
    }

private:
    HANDLE in_ = nullptr;
    DWORD saved_ = 0;
    KeyTrack keys_;
    PointerTrack track_;
    bool ok_ = false;
};

using PlatformReader = ConsoleReader;

#else

class TerminalReader {
public:
    TerminalReader() {
        ok_ = ::tcgetattr(STDIN_FILENO, &saved_) == 0;
        if (!ok_) {
            return;
        }
        termios raw = saved_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG));
        raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
        raw.c_cc[VMIN] = 0; // read() is a poll; the pump owns the cadence
        raw.c_cc[VTIME] = 0;
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
    ~TerminalReader() {
        if (ok_) {
            ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_);
        }
    }
    TerminalReader(const TerminalReader&) = delete;
    TerminalReader& operator=(const TerminalReader&) = delete;

    /// One read, into a parser that outlives it: the kernel splits an escape sequence wherever it
    /// likes, so a mouse report cut in half is rejoined across two polls instead of typed. A read
    /// that yields nothing is the parser's only clock: `idle()` releases a pending lone ESC as
    /// the Escape key (translate.hpp).
    std::vector<InputEvent> poll() {
        if (!ok_) {
            return {};
        }
        unsigned char buf[64];
        const ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) {
            return parser_.idle();
        }
        return parser_.feed(buf, static_cast<std::size_t>(n));
    }

private:
    termios saved_{};
    TerminalParser parser_;
    bool ok_ = false;
};

using PlatformReader = TerminalReader;

#endif

using InputWeave = InputWeaveT<PlatformReader>;

} // namespace

ZEN_EXPORT_WEAVE(InputWeave)
