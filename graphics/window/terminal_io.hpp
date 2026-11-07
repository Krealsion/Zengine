// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_TERMINAL_IO_HPP
#define ZENGINE_GRAPHICS_WINDOW_TERMINAL_IO_HPP

// A terminal's output side and its size, as whoever paints it claims it: the one place this
// repository asks an operating system how big a terminal is (the only `#if defined(_WIN32)` the
// terminal medium needs), the modes a painter takes and gives back, the ink it shows a colour in, the
// copy sequence, and the claim itself, held for exactly its owner's lifetime. Read by the terminal
// window, which asks the size each time it looks. On POSIX a resize also writes a pipe
// (`ResizeWatch`) that a host's one wait holds, so a resize wakes the painter that asks; on Windows
// a resize is a console input record, which wakes it the same way. Reference:
// graphics/docs/zengine.window.terminal.md.

#include <array>
#include <cstdlib>
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
#include <cerrno>
#include <csignal>
#include <fcntl.h>
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

#if defined(__unix__) || defined(__APPLE__)

/// A RESIZE, WRITTEN TO A PIPE: while one stands, a `SIGWINCH` writes a byte to its read end's
/// pipe, so a host's wait on that end wakes whoever asks the size. It stands for exactly its
/// owner's life, and puts back the handler it found when it goes, so an unloaded weave leaves no
/// handler of its own behind; one already there is still called. One at a time: a second gives
/// nothing to wait on.
class ResizeWatch {
public:
    ResizeWatch() {
        if (write_fd() >= 0) {
            return; // another stands
        }
        int fds[2] = {-1, -1};
        if (::pipe(fds) != 0) {
            return;
        }
        for (const int fd : fds) {
            (void)::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
            (void)::fcntl(fd, F_SETFD, FD_CLOEXEC);
        }
        struct sigaction on {};
        on.sa_handler = &ResizeWatch::on_resize;
        on.sa_flags = SA_RESTART;
        sigemptyset(&on.sa_mask);
        write_fd() = fds[1];
        if (::sigaction(SIGWINCH, &on, &previous()) != 0) {
            write_fd() = -1;
            ::close(fds[0]);
            ::close(fds[1]);
            return;
        }
        read_ = fds[0];
        write_ = fds[1];
    }
    ~ResizeWatch() {
        if (read_ < 0) {
            return;
        }
        (void)::sigaction(SIGWINCH, &previous(), nullptr);
        write_fd() = -1;
        ::close(read_);
        ::close(write_);
    }
    ResizeWatch(const ResizeWatch&) = delete;
    ResizeWatch& operator=(const ResizeWatch&) = delete;

    /// The read end, or -1 when no watch stands here.
    int fd() const noexcept { return read_; }
    /// What the resizes since the last look wrote, read away.
    void drain() const noexcept {
        if (read_ < 0) {
            return;
        }
        char buf[64];
        while (::read(read_, buf, sizeof buf) > 0) {
        }
    }

private:
    static int& write_fd() noexcept {
        static int fd = -1;
        return fd;
    }
    static struct sigaction& previous() noexcept {
        static struct sigaction was {};
        return was;
    }
    static void on_resize(int sig) {
        const int saved = errno;
        if (write_fd() >= 0) {
            const char one = 1;
            (void)!::write(write_fd(), &one, 1);
        }
        const struct sigaction& was = previous();
        if ((was.sa_flags & SA_SIGINFO) == 0 && was.sa_handler != SIG_DFL &&
            was.sa_handler != SIG_IGN && was.sa_handler != nullptr) {
            was.sa_handler(sig);
        }
        errno = saved;
    }

    int read_ = -1;
    int write_ = -1;
};

#endif

/// The sixteen inks a terminal shows, by name, in the order their codes count (30 to 37, then the
/// bright ones, 90 to 97), each with the colour a terminal shows it in by default (xterm's).
/// The `cells` word for the terminal's default colours: its default ink (SGR 39) and ground (49).
inline constexpr std::string_view kTerminalDefault = "terminal default";
inline constexpr std::array<const char*, 8> kTerminalInkNames = {
    "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"};
inline constexpr std::array<std::uint32_t, 16> kTerminalInkColors = {
    0x000000, 0xcd0000, 0x00cd00, 0xcdcd00, 0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
    0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00, 0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff};

/// The ink a `cells` word names, as SGR's codes: `terminal default`, the terminal's default ink (39), or one
/// of its sixteen (`red` 31, `bright red` 91), each perhaps `bold` (`;1`); "" where the word names
/// none of them.
inline std::string sgr_codes_of_word(std::string_view word) {
    std::string codes;
    bool bold = false;
    constexpr std::string_view kBold = " bold";
    if (word.size() > kBold.size() && word.substr(word.size() - kBold.size()) == kBold) {
        bold = true;
        word.remove_suffix(kBold.size());
    }
    if (word == kTerminalDefault) {
        codes = "39";
    } else {
        int base = 30;
        constexpr std::string_view kBright = "bright ";
        if (word.substr(0, kBright.size()) == kBright) {
            base = 90;
            word.remove_prefix(kBright.size());
        }
        for (std::size_t i = 0; i < kTerminalInkNames.size(); ++i) {
            if (word == kTerminalInkNames[i]) {
                codes = std::to_string(base + static_cast<int>(i));
            }
        }
    }
    if (codes.empty()) {
        return {};
    }
    return bold ? codes + ";1" : codes;
}

/// Which of the sixteen inks is nearest a colour (0xRRGGBBAA, its alpha not counted), by the
/// colours a terminal shows them in by default: its SGR code, 30 to 37 or 90 to 97.
inline int nearest_terminal_ink(std::int64_t rgba) noexcept {
    const auto channel = [](std::uint32_t c, int shift) {
        return static_cast<std::int64_t>((c >> shift) & 0xffu);
    };
    const auto rgb = static_cast<std::uint32_t>((static_cast<std::uint64_t>(rgba) >> 8) & 0xffffffu);
    int best = 0;
    std::int64_t best_distance = -1;
    for (std::size_t i = 0; i < kTerminalInkColors.size(); ++i) {
        const std::uint32_t c = kTerminalInkColors[i];
        const std::int64_t dr = channel(rgb, 16) - channel(c, 16);
        const std::int64_t dg = channel(rgb, 8) - channel(c, 8);
        const std::int64_t db = channel(rgb, 0) - channel(c, 0);
        const std::int64_t d = dr * dr + dg * dg + db * db;
        if (best_distance < 0 || d < best_distance) {
            best_distance = d;
            best = static_cast<int>(i);
        }
    }
    return best < 8 ? 30 + best : 90 + (best - 8);
}

/// What a terminal is written to show a colour, regular or `bold`: the ink its `cells` word names,
/// else the nearest of the sixteen to the colour itself, and its weight said every time (`22`
/// normal, `1` bold), so one ink's weight never carries into the next. The terminal's default ink
/// is shown only where a word asks for it: a nearest is always one of the sixteen, never the
/// terminal's default ink or ground.
inline std::string sgr_for(std::string_view cells, std::int64_t rgba, bool bold = false) {
    std::string codes = sgr_codes_of_word(cells);
    if (codes.empty()) {
        codes = std::to_string(nearest_terminal_ink(rgba));
    }
    const bool word_bold = codes.size() > 2 && codes.compare(codes.size() - 2, 2, ";1") == 0;
    if (word_bold) {
        codes.resize(codes.size() - 2);
    }
    return "\x1b[" + std::string(bold || word_bold ? "1;" : "22;") + codes + "m";
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

/// WHETHER A LOCALE CARRIES UTF-8, as POSIX resolves one from the environment: the first of
/// `LC_ALL`, `LC_CTYPE` and `LANG` that is set and not empty names it, and none is the C locale,
/// which carries ASCII alone. A name whose codeset is UTF-8 (`en_US.UTF-8`, `C.utf8`) carries it.
inline bool terminal_locale_is_utf8(const char* lc_all, const char* lc_ctype, const char* lang) {
    const char* named = nullptr;
    for (const char* v : {lc_all, lc_ctype, lang}) {
        if (v != nullptr && v[0] != '\0') {
            named = v;
            break;
        }
    }
    if (named == nullptr) {
        return false;
    }
    std::string lower;
    for (const char* p = named; *p != '\0'; ++p) {
        const char c = *p;
        if (c != '-' && c != '_') {
            lower.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
        }
    }
    return lower.find("utf8") != std::string::npos;
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

    /// Whether the terminal's encoding carries UTF-8: on Windows its console's output code page,
    /// which the claim sets to UTF-8 and a console that refused keeps; elsewhere the locale the
    /// environment names (`terminal_locale_is_utf8`). Output with no terminal is UTF-8.
    bool utf8() const {
#if defined(_WIN32)
        return !ok_ || ::GetConsoleOutputCP() == CP_UTF8;
#else
        return !ok_ || terminal_locale_is_utf8(std::getenv("LC_ALL"), std::getenv("LC_CTYPE"),
                                               std::getenv("LANG"));
#endif
    }

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
