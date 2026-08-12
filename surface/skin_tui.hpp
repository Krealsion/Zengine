// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

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

#include "cells.hpp"
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
#include <vector>

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

/// This medium's ink for each semantic canvas role. The mapping lives HERE and
/// nowhere else — that is the whole point of shipping roles instead of colours:
/// a publisher says "alert", the terminal says red, a themed window says
/// whatever it likes, and neither has to agree with the other. An unknown role
/// paints as `kFill` (vocabulary.hpp's stated fallback) rather than disappearing.
inline const char* sgr_for_role(int role) noexcept {
    switch (role) {
    case 1: return "\x1b[36m";    // kAccent — cyan: the thing being pointed at
    case 2: return "\x1b[90m";    // kMuted  — bright black: present, quiet
    case 3: return "\x1b[31;1m";  // kAlert  — bold red: must be seen
    default: return "\x1b[37m";   // kFill and anything unknown — plain ink
    }
}

/// And this medium's GLYPH for each role — because colour alone would be a lie
/// on a monochrome terminal, where four roles would paint four identical `#`s.
/// A publisher ships intent; a medium is responsible for making that intent
/// distinguishable in the medium it actually owns, and one character per cell is
/// all the ink this one has. (This is the same authority the styles already
/// exercise over snake's glyphs — it is why role is semantic and not RGB.)
inline char glyph_for_role(int role) noexcept {
    switch (role) {
    case 1: return '*'; // kAccent
    case 2: return '.'; // kMuted
    case 3: return '!'; // kAlert
    default: return '#'; // kFill and anything unknown
    }
}

/// The general canvas, rasterized to the terminal: one cell = one character
/// column, `role` = an SGR colour, labels drawn over the rects. PURE — a
/// SurfaceCanvas in, the exact bytes out, no Sink and no terminal in sight —
/// which is what lets the suite pin a whole Workshop screen as a golden string.
///
/// NOT part of a Style. The two styles are the old snake drawers' looks, ported
/// byte-faithful, and a canvas has no drawer to be faithful to; giving it a
/// per-style appearance now would be inventing two looks in order to have a
/// choice nobody asked for. One canvas rasterizer, shared — and the day a
/// canvas genuinely wants a style, THAT is when it becomes one.
///
/// Elements are clipped to the extent, per the vocabulary's contract: a rect
/// hanging off the edge is drawn as much as fits, and a label is cut at the
/// right edge on a BYTE boundary — this renderer is byte-per-cell, so a
/// multi-byte codepoint would be split, which is why the house charset rule
/// (plain ASCII intent) is the publisher's side of the same bargain.
inline std::string canvas_body(const zengine::surface::SurfaceCanvas& c) {
    const std::int64_t w = c.width > 0 ? c.width : 0;
    const std::int64_t h = c.height > 0 ? c.height : 0;
    if (w == 0 || h == 0) {
        return {};
    }
    // Two parallel grids: what to draw, and in what role. Painter's order falls
    // out of overwriting — later rects win, labels win over every rect.
    // std::vector<char>, not std::string, and the reason is testability rather
    // than taste. A below-extent write is invisible to the golden bytes by
    // construction (the render loop reads only in-range cells), so the bottom-edge
    // guard can only ever be watched by a sanitizer — and a std::string keeps
    // spare capacity, so the slip landed inside its own allocation and ASan stayed
    // green too (measured, with the guard deleted). A sized vector allocates what
    // it was asked for, so the same slip becomes a real heap overflow. The guard
    // below is the correctness; this is what lets anything prove it is still there.
    const std::size_t cells = static_cast<std::size_t>(w * h);
    std::vector<char> glyphs(cells, ' ');
    std::vector<char> roles(cells, static_cast<char>(-1)); // -1 = untouched background

    const auto put = [&](std::int64_t x, std::int64_t y, char g, std::int64_t role) {
        if (x < 0 || y < 0 || x >= w || y >= h) {
            return;
        }
        const std::size_t i = static_cast<std::size_t>(y * w + x);
        glyphs[i] = g;
        roles[i] = static_cast<char>(role);
    };

    // CLIPPED BEFORE ITERATING. `put` already refuses every cell off the canvas,
    // so the visible picture is the same either way -- but a canvas is a
    // ZEN_SHAPE, so `r.w` is a number a publisher chose, and walking it was the
    // publisher deciding how long this Skin runs. Both halves of that were
    // measured: a rect 100,000,000 cells wide on a 4x2 canvas cost 75 ms to produce
    // 38 bytes, and `r.x + dx` at the top of the number line was signed overflow
    // (UBSan, on committed code -- no test fed it such a canvas, so the standing
    // lane had nothing to catch). Both are gone by asking surface/cells.hpp for
    // the span first; see there for why the rule is shared with the SDL plan.
    for (const zengine::surface::SurfaceRect& r : c.rects) {
        const char g = glyph_for_role(static_cast<int>(r.role));
        const CellSpan xs = clip_span(r.x, r.w, w);
        const CellSpan ys = clip_span(r.y, r.h, h);
        for (std::int64_t y = ys.begin; y < ys.end; ++y) {
            for (std::int64_t x = xs.begin; x < xs.end; ++x) {
                put(x, y, g, r.role);
            }
        }
    }
    for (const zengine::surface::SurfaceLabel& l : c.labels) {
        for (std::size_t i = 0; i < l.text.size(); ++i) {
            put(add_cells(l.x, static_cast<std::int64_t>(i)), l.y, l.text[i], l.role);
        }
    }

    std::string out;
    out.reserve(cells * 3);
    for (std::int64_t y = 0; y < h; ++y) {
        out += "\x1b[2K";
        // The role whose SGR is in effect, starting at a value NO role and not
        // even the background can equal -- so the first cell of every row always
        // states its own ink. With -1 here (the background's own marker) a row
        // that begins with untouched background emitted no SGR at all and drew in
        // whatever the terminal happened to be wearing: the canvas would have
        // been describing a picture it did not fully determine.
        int open = -2;
        for (std::int64_t x = 0; x < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y * w + x);
            const int role = static_cast<int>(roles[i]);
            if (role != open) {
                out += role < 0 ? "\x1b[0m" : sgr_for_role(role);
                open = role;
            }
            out += glyphs[i];
        }
        if (open >= 0) {
            out += "\x1b[0m";
        }
        out += "\r\n";
    }
    return out;
}

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

    /// A canvas lands in exactly the same place a board does — the canvas rows
    /// from row 3 down, claimed on the first frame. Same layout convention, one
    /// different body.
    void canvas(const zengine::surface::SurfaceCanvas& c, bool first) {
        std::string out = "\x1b[3;1H";
        if (first) {
            out += "\x1b[0J";
        }
        out += canvas_body(c);
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

    /// NO OPINION ABOUT HOW MUCH ROOM THERE IS — this projection's policy, stated
    /// rather than defaulted.
    ///
    /// A window Skin owns a drawable whose size is its own to read; a terminal
    /// Skin owns no such thing. It writes into a stream, from row 3 down, and how
    /// many rows and columns are on the other end of that stream is the
    /// TERMINAL'S fact, not this medium's — it belongs to a `Sink` that may be a
    /// std::string in a suite, a pipe, or a real console, and only the last of
    /// those has an answer at all.
    ///
    /// So this projection keeps the extent its publisher already authors, and a
    /// terminal too small simply shows less of it — which is what it did before
    /// G-2 and what a terminal has always done to output too wide for it. Saying
    /// {0,0} here is not a stub: it is the honest sentence "I have no opinion",
    /// and `SkinT::report_extent` publishes nothing for it, so a publisher hears
    /// no claim rather than a wrong one.
    ///
    /// THE TRIGGER for changing that is a real terminal size arriving with a real
    /// consumer for it: a `Sink` that can be ASKED its extent (and a rule for what
    /// a resize of a live terminal means, which SIGWINCH-less Windows consoles and
    /// pipes answer differently). G-2 deliberately did not invent one to be
    /// symmetrical with the window medium.
    SurfaceExtent extent() const { return SurfaceExtent{}; }

    Sink& sink() { return sink_; }

private:
    Sink sink_;
};

/// The terminal modes a TUI Skin claims, as bytes — pure, so the claim is a
/// value a suite can read rather than a side effect only a live terminal sees.
///
/// WHO OWNS POINTER REPORTING, decided here. A terminal reports a
/// pointer only if something asks it to, in-band, on the OUTPUT stream. The
/// output stream is the Skin's — it already claims the alternate screen and the
/// cursor and gives them back — so pointer reporting is claimed and released on
/// exactly the same lifetime, by the same RAII, with no coordination surface
/// invented between the two packages. Input never writes a byte to the
/// terminal; it only parses what arrives. That the two need not talk is the
/// reason this is the smallest truthful owner: the Skin turning reporting on
/// requires telling Input nothing, because Input parses an SGR report whenever
/// one shows up and one only shows up if a Skin asked.
///
/// The consequence, stated because it is a real product fact: on the POSIX
/// lane there is no pointer without a TUI Skin loaded. A Workshop with no Skin
/// has no screen either, so nothing is lost that was not already gone.
///
/// `1002` is button-event tracking — press, release, and motion WHILE A BUTTON
/// IS HELD. That is exactly a drag and nothing else; `1003` would report every
/// idle motion and pay for a gesture nobody makes. `1006` asks for SGR
/// coordinates, which are the only encoding that survives past column 223 and
/// the only one that spells press and release distinctly.
///
/// LEAVE UNDOES ENTER, in reverse order, and that is asserted rather than
/// eyeballed. What it cannot promise is survival of an uncatchable death: a
/// process killed with SIGKILL restores nothing, and a terminal left in
/// reporting mode prints mouse escapes at its shell until `reset`. That is the
/// same exposure the alternate screen already carries and it is not new here.
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

/// The OUTPUT side of the terminal, claimed for exactly the Skin's lifetime —
/// alternate screen, hidden cursor, pointer reporting where it is in-band, and
/// (on Windows) VT processing so the ANSI is real, plus the UTF-8 codepage
/// lever — restored whole on destruction. Degrades gracefully with no console
/// (stdout redirected, headless ctest): the ceremony is skipped but frames
/// still go to stdout, exactly the old drawers' posture — bytes belong to
/// whoever redirected them. This is the host's old Screen class, relocated: the
/// INPUT side of the terminal stays the Input weave's, and the two never touch
/// the same console state.
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

#endif // ZENGINE_SURFACE_SKIN_TUI_HPP
