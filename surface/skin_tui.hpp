#ifndef ZENGINE_SURFACE_SKIN_TUI_HPP
#define ZENGINE_SURFACE_SKIN_TUI_HPP

// The terminal medium: the whole TUI drawing path in one header, split the
// zen-ui-pixel way so every byte of it is pinnable headless —
//
//   ClassicStyle / BlockStyle  pure functions: a SnakeVisual becomes the exact
//                              ANSI string the old snake drawers painted
//                              (ported byte-faithful; the styles ARE those two
//                              drawers, keeping the swap moment unmistakable).
//   TuiMedium<Style, Sink>     the layout convention that used to be spread
//                              across host, score weave, and drawers: row 1 is
//                              the status slot, row 2 the score slot, row 3
//                              down is the canvas; first frame claims the
//                              canvas with an erase-below.
//   TuiTerminal                the real Sink, and the CLAIM: alternate screen,
//                              hidden cursor (and on Windows, VT processing +
//                              UTF-8 codepage) engaged in the constructor,
//                              restored whole in the destructor — the host's
//                              old Screen class, moved to where it now
//                              belongs: whoever paints owns the terminal.
//
// The suite injects a string Sink and pins the exact bytes; the two .so skins
// (skin_tui_classic.cpp / skin_tui_block.cpp) plug in TuiTerminal and ship.

#include "skin.hpp"

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

/// The classic look: one character per cell, an ASCII border, monochrome,
/// banner underneath. The old snake-drawer-classic frame, ported byte-faithful
/// with exactly two deliberate deltas: the cursor-home/erase prefix moved to
/// the medium (layout, not style), and the banner now says "skin" — the thing
/// it names was renamed, and the pixels follow the truth.
struct ClassicStyle {
    static std::string board(const zengine::snake::SnakeVisual& v) {
        std::string out;
        out.reserve(static_cast<std::size_t>((v.width + 4) * (v.height + 6)));
        out += "+";
        out.append(static_cast<std::size_t>(v.width), '-');
        out += "+\r\n";
        for (std::int64_t y = 0; y < v.height; ++y) {
            out += '|';
            for (std::int64_t x = 0; x < v.width; ++x) {
                out += glyph(v, x, y);
            }
            out += "|\r\n";
        }
        out += "+";
        out.append(static_cast<std::size_t>(v.width), '-');
        out += "+\r\n";
        out += "\x1b[2K  classic skin - score " + std::to_string(v.score);
        out += v.alive ? " - alive" : " - DEAD (n = new game)";
        out += "\r\n";
        return out;
    }

private:
    static char glyph(const zengine::snake::SnakeVisual& v, std::int64_t x, std::int64_t y) {
        if (!v.snake.empty() && v.snake.front().x == x && v.snake.front().y == y) {
            return 'O';
        }
        for (std::size_t i = 1; i < v.snake.size(); ++i) {
            if (v.snake[i].x == x && v.snake[i].y == y) {
                return 'o';
            }
        }
        if (v.food.x == x && v.food.y == y) {
            return '*';
        }
        return ' ';
    }
};

/// The block look: double-width cells, no border (colored rules instead), SGR
/// color and inverse video, banner ABOVE the board — deliberately different
/// code so a live swap is unmistakable. The old snake-drawer-block frame,
/// same two deltas as ClassicStyle and no others.
struct BlockStyle {
    static std::string board(const zengine::snake::SnakeVisual& v) {
        const std::size_t cols = static_cast<std::size_t>(v.width) * 2;
        std::string out;
        out.reserve(cols * static_cast<std::size_t>(v.height + 6) * 2);
        out += "\x1b[2K\x1b[7m BLOCK SKIN \x1b[0m  score ";
        out += std::to_string(v.score);
        if (!v.alive) {
            out += "  \x1b[31;7m DEAD - n starts over \x1b[0m";
        }
        out += "\r\n\x1b[36m";
        out.append(cols, '=');
        out += "\x1b[0m\r\n";
        for (std::int64_t y = 0; y < v.height; ++y) {
            out += "\x1b[2K";
            for (std::int64_t x = 0; x < v.width; ++x) {
                out += cell(v, x, y);
            }
            out += "\r\n";
        }
        out += "\x1b[36m";
        out.append(cols, '=');
        out += "\x1b[0m\r\n";
        return out;
    }

private:
    static std::string cell(const zengine::snake::SnakeVisual& v, std::int64_t x, std::int64_t y) {
        if (!v.snake.empty() && v.snake.front().x == x && v.snake.front().y == y) {
            return "\x1b[32;7m@@\x1b[0m";
        }
        for (std::size_t i = 1; i < v.snake.size(); ++i) {
            if (v.snake[i].x == x && v.snake[i].y == y) {
                return "\x1b[32m##\x1b[0m";
            }
        }
        if (v.food.x == x && v.food.y == y) {
            return "\x1b[33;7m()\x1b[0m";
        }
        return "  ";
    }
};

/// The terminal layout — the shared convention, now in exactly one place:
/// rows 1 and 2 are the "status" and "score" slots, the canvas starts at row
/// 3 and claims everything below it on the Skin's first frame. Slots the
/// layout has no row for are dropped (see vocabulary.hpp).
template <class Style, class Sink>
class TuiMedium {
public:
    TuiMedium() = default;
    explicit TuiMedium(Sink sink) : sink_(std::move(sink)) {}

    void frame(const zengine::snake::SnakeVisual& v, bool first) {
        std::string out = "\x1b[3;1H";
        if (first) {
            out += "\x1b[0J";
        }
        out += Style::board(v);
        sink_.write(out);
    }

    void note(std::string_view slot, std::string_view text) {
        int row = 0;
        if (slot == kSlotStatus) {
            row = 1;
        } else if (slot == kSlotScore) {
            row = 2;
        } else {
            return;
        }
        std::string out = "\x1b[";
        out += std::to_string(row);
        out += ";1H\x1b[2K ";
        out += text;
        sink_.write(out);
    }

    /// A terminal needs no servicing between writes; the pump is the window
    /// media's lifeline (see vocabulary.hpp), honestly idle here.
    void pump() {}

    Sink& sink() { return sink_; }

private:
    Sink sink_;
};

/// The OUTPUT side of the terminal, claimed for exactly the Skin's lifetime —
/// alternate screen, hidden cursor, and (on Windows) VT processing so the
/// ANSI is real, plus the UTF-8 codepage lever — restored whole on
/// destruction. Degrades gracefully with no console (stdout redirected,
/// headless ctest): the ceremony is skipped but frames still go to stdout,
/// exactly the old drawers' posture — bytes belong to whoever redirected
/// them. This is the host's old Screen class, relocated: the INPUT side of
/// the terminal stays the Input weave's, and the two never touch the same
/// console state.
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

private:
    static void enter() {
        std::fputs("\x1b[?1049h\x1b[?25l\x1b[2J", stdout);
        std::fflush(stdout);
    }
    static void leave() {
        std::fputs("\x1b[?25h\x1b[?1049l", stdout);
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

#endif // ZENGINE_SURFACE_SKIN_TUI_HPP
