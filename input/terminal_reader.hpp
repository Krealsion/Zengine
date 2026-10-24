// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_TERMINAL_READER_HPP
#define ZENGINE_INPUT_TERMINAL_READER_HPP

// The terminal's and the Win32 console's input edge: a reader owns the platform's input-side state
// (raw mode on POSIX; the console input mode on Windows) while it holds the terminal, and hands
// each native event to the pure translators in translate.hpp. With no console (a pipe, a test run)
// it yields nothing. It yields the terminal to a window as `window_claim.hpp` says, putting back
// the mode it found when an OS window comes. Read by the Input weave and by a terminal window.
// Reference: input/docs/input.md.

#include "translate.hpp"
#include "window_claim.hpp" // the windows' claims on the process's input

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
#include <string>
#include <vector>

namespace zengine::input {

#if defined(_WIN32)

class ConsoleReader {
public:
    explicit ConsoleReader(bool window_owner = false) : window_owner_(window_owner) {
        (void)settle();
    }
    ~ConsoleReader() { give_back(); }
    ConsoleReader(const ConsoleReader&) = delete;
    ConsoleReader& operator=(const ConsoleReader&) = delete;

    /// ReadConsoleInputW, not ...A: the W record's uChar.UnicodeChar is the
    /// layout's own answer to "what did this keystroke type", and it is the only
    /// reason `%` can reach an application without anyone deducing it from a key
    /// identity. The A record would narrow that answer to the console codepage
    /// before this process ever saw it.
    std::vector<InputEvent> poll() {
        std::vector<InputEvent> out;
        if (!settle()) {
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
    /// Whether it reads now: it takes the console while it may, and gives back the mode it set
    /// while an OS window holds input. A terminal window's mode it never touches.
    bool settle() {
        const bool terminal_window = window_holds_terminal_input();
        if (terminal_reader_may_use_terminal(window_owner_, terminal_window,
                                             window_holds_sdl_input())) {
            if (!ok_ && !absent_) {
                in_ = ::GetStdHandle(STD_INPUT_HANDLE);
                ok_ = in_ != INVALID_HANDLE_VALUE && ::GetConsoleMode(in_, &saved_) != 0;
                absent_ = !ok_;
                if (ok_) {
                    ::SetConsoleMode(in_, ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT);
                }
            }
            return ok_;
        }
        if (!terminal_window) {
            give_back();
        }
        return false;
    }
    /// The mode it found put back, unless a terminal window holds the console, whose mode it is.
    void give_back() {
        if (ok_ && (window_owner_ || !window_holds_terminal_input())) {
            ::SetConsoleMode(in_, saved_);
            ok_ = false;
        }
    }

    HANDLE in_ = nullptr;
    DWORD saved_ = 0;
    KeyTrack keys_;
    PointerTrack track_;
    bool window_owner_ = false;
    bool ok_ = false;     // it holds the console, in the mode it set
    bool absent_ = false; // there is no console to take
};

using PlatformReader = ConsoleReader;

#else

class TerminalReader {
public:
    explicit TerminalReader(bool window_owner = false) : window_owner_(window_owner) {
        (void)settle();
    }
    ~TerminalReader() { give_back(); }
    TerminalReader(const TerminalReader&) = delete;
    TerminalReader& operator=(const TerminalReader&) = delete;

    /// One read, into a parser that outlives it: the kernel splits an escape sequence wherever it
    /// likes, so a mouse report cut in half is rejoined across two polls instead of typed. A read
    /// that yields nothing is the parser's only clock: `idle()` releases a pending lone ESC as
    /// the Escape key (translate.hpp).
    std::vector<InputEvent> poll() {
        if (!settle()) {
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
    /// Whether it reads now: it takes the terminal while it may, and gives back the mode it set
    /// while an OS window holds input. A terminal window's mode it never touches.
    bool settle() {
        const bool terminal_window = window_holds_terminal_input();
        if (terminal_reader_may_use_terminal(window_owner_, terminal_window,
                                             window_holds_sdl_input())) {
            if (!ok_ && !absent_) {
                ok_ = ::tcgetattr(STDIN_FILENO, &saved_) == 0;
                absent_ = !ok_;
                if (ok_) {
                    termios raw = saved_;
                    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG));
                    raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
                    raw.c_cc[VMIN] = 0; // read() is a poll; the pump owns the cadence
                    raw.c_cc[VTIME] = 0;
                    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
                }
            }
            return ok_;
        }
        if (!terminal_window) {
            give_back();
        }
        return false;
    }
    /// The mode it found put back, unless a terminal window holds the terminal, whose mode it is.
    void give_back() {
        if (ok_ && (window_owner_ || !window_holds_terminal_input())) {
            ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_);
            ok_ = false;
        }
    }

    termios saved_{};
    TerminalParser parser_;
    bool window_owner_ = false;
    bool ok_ = false;     // it holds the terminal, in the mode it set
    bool absent_ = false; // there is no terminal to take
};

using PlatformReader = TerminalReader;

#endif

} // namespace zengine::input

#endif // ZENGINE_INPUT_TERMINAL_READER_HPP
