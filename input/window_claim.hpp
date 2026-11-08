// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INPUT_WINDOW_CLAIM_HPP
#define ZENGINE_INPUT_WINDOW_CLAIM_HPP

// A window's claim on an input edge it shares with an Input reader in the same process: the
// TUI window's on the terminal's input, the GUI window's on SDL's queue, which also leaves the
// console the run started from to the person at it. Window and reader share no variable, so a
// claim lives in the environment as `<pid>:<count>`: a child ignores a claim under another pid,
// and the count keeps it across a reload in place, which builds the new window first.
// Reference: input/docs/zengine.input.md.

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

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>

namespace zengine::input {

inline constexpr const char* kWindowTuiInputVariable = "ZENGINE_WINDOW_TUI_INPUT_PID";
inline constexpr const char* kWindowSdlInputVariable = "ZENGINE_WINDOW_SDL_INPUT_PID";

namespace detail {

inline std::string window_claim_pid() {
#if defined(_WIN32)
    return std::to_string(::GetCurrentProcessId());
#else
    return std::to_string(::getpid());
#endif
}

/// The variable's text, or nothing when it is unset (or longer than any claim, on Windows).
inline std::string window_claim_text(const char* variable) {
#if defined(_WIN32)
    char buf[64];
    const DWORD n = ::GetEnvironmentVariableA(variable, buf, static_cast<DWORD>(sizeof(buf)));
    return n < sizeof(buf) ? std::string(buf, n) : std::string();
#else
    const char* text = ::getenv(variable);
    return text != nullptr ? std::string(text) : std::string();
#endif
}

inline void write_window_claim(const char* variable, std::int64_t count) {
    const std::string text = window_claim_pid() + ":" + std::to_string(count);
#if defined(_WIN32)
    ::SetEnvironmentVariableA(variable, text.c_str());
#else
    ::setenv(variable, text.c_str(), 1);
#endif
}

inline void erase_window_claim(const char* variable) {
#if defined(_WIN32)
    ::SetEnvironmentVariableA(variable, nullptr);
#else
    ::unsetenv(variable);
#endif
}

/// How many windows in this process hold the claim `variable` names: the count after this
/// process's own pid; 0 for another pid's claim, a count that is not a positive number, or none.
inline std::int64_t window_claims(const char* variable) {
    const std::string text = window_claim_text(variable);
    const std::string prefix = window_claim_pid() + ":";
    if (std::string_view(text).substr(0, prefix.size()) != prefix) {
        return 0;
    }
    const char* first = text.data() + prefix.size();
    const char* last = text.data() + text.size();
    std::int64_t count = 0;
    const std::from_chars_result read = std::from_chars(first, last, count);
    return read.ec == std::errc() && read.ptr == last && count > 0 ? count : 0;
}

inline void claim_window(const char* variable) {
    write_window_claim(variable, window_claims(variable) + 1);
}

inline void release_window(const char* variable) {
    const std::int64_t n = window_claims(variable);
    if (n > 1) {
        write_window_claim(variable, n - 1);
    } else {
        erase_window_claim(variable);
    }
}

} // namespace detail

// ---- the TUI window's claim on the terminal's input ---------------------------------------

/// How many TUI windows in this process hold its terminal's input.
inline std::int64_t window_tui_input_claims() {
    return detail::window_claims(kWindowTuiInputVariable);
}

inline bool window_holds_terminal_input() { return window_tui_input_claims() > 0; }

/// A TUI window's, once when it is made: one more claim on this process's terminal input.
inline void claim_window_tui_input() { detail::claim_window(kWindowTuiInputVariable); }

/// A TUI window's, once when it goes: its claim given back, the variable gone with the last.
inline void release_window_tui_input() {
    detail::release_window(kWindowTuiInputVariable);
}

/// A reader touches the terminal when it is the TUI window's own, or when no window in this
/// process holds input: neither a TUI window holding the terminal nor a GUI window, which
/// reads its own events and leaves the console the run started from to the person at it.
inline bool terminal_reader_may_use_terminal(bool window_owner, bool tui_window_holds,
                                             bool gui_window_holds) {
    return window_owner || (!tui_window_holds && !gui_window_holds);
}

// ---- the GUI window's claim on SDL's event queue ------------------------------------------------

/// How many GUI windows in this process hold SDL's event queue.
inline std::int64_t window_sdl_input_claims() {
    return detail::window_claims(kWindowSdlInputVariable);
}

inline bool window_holds_sdl_input() { return window_sdl_input_claims() > 0; }

/// A GUI window's, once when it is made: one more claim on this process's SDL queue.
inline void claim_window_sdl_input() { detail::claim_window(kWindowSdlInputVariable); }

/// A GUI window's, once when it goes or finds it has no window to read: its claim given back.
inline void release_window_sdl_input() { detail::release_window(kWindowSdlInputVariable); }

/// The SDL Input reader drains SDL's queue only while no GUI window in this process holds it.
inline bool sdl_reader_may_use_sdl() { return !window_holds_sdl_input(); }

} // namespace zengine::input

#endif // ZENGINE_INPUT_WINDOW_CLAIM_HPP
