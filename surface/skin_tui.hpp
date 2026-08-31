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
// A Sink is anything with:
//
//   void write(std::string_view);   // put these bytes on my stream
//   TerminalSize size() const;      // how big the terminal on the other end is,
//                                   // in cells; {0,0} = there is none to ask
//
// `size()` is REQUIRED of a Sink rather than detected on one, and that is why the
// contract is spelled here at all (TUI-0). A Sink that quietly lacked the method
// would be a Sink whose terminal is permanently unmeasurable — which is an
// ordinary, honest state a pipe reaches every day — so the mistake would look
// exactly like the truth, on every lane, forever. Requiring it makes a forgetful
// Sink a compile error instead of a silent fixed-size TUI.
//
// The suite injects a string Sink and pins the exact bytes; the two .so skins
// (skin_tui_classic.cpp / skin_tui_block.cpp) plug in TuiTerminal and ship.

#include "cells.hpp"
#include "pointing.hpp"
#include "region.hpp"
#include "skin.hpp"
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
#include <optional>
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

/// This medium's GROUND for each role — the same table one attribute over, and
/// the terminal's honest answer to a row that asked to be set on something (HD-2).
///
/// SGR 40–47 are the eight background colours and 100–107 their bright halves,
/// which is the whole of what an ANSI terminal has to say here; `role::kNone` is
/// not in this table at all, because it is the ABSENCE of a ground and is spelled
/// by not emitting anything (`\x1b[49m`, the default background, restores it).
///
/// The pairs are chosen so a row's own ink stays legible on top of its ground —
/// `kMuted` is the selection ground precisely because every foreground in
/// `sgr_for_role` reads on it. That is a MEDIUM's judgement about its own
/// palette, which is what the role vocabulary exists to keep out of publishers.
inline const char* sgr_bg_for_role(int role) noexcept {
    switch (role) {
    case 1: return "\x1b[46m";  // kAccent — cyan ground
    case 2: return "\x1b[100m"; // kMuted  — bright black: the selection bar
    case 3: return "\x1b[41m";  // kAlert  — red ground
    default: return "\x1b[47m"; // kFill and anything unknown — plain ground
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
/// LAYERS ARE EXECUTED IN LIST ORDER, ONE COMPLETE PLANE AT A TIME (WIND-2a) — so a
/// label in a later layer covers a text row in an earlier one, which is the fact this
/// medium could not express before and the reason the canvas gained the shape.
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
    // SIGNED, EXPLICITLY. These two hold a sentinel of -1 and are read back with a
    // `< 0` test; plain `char` is unsigned on some targets (ARM by default), where
    // -1 would come back as 255, the test would be false, and an untouched cell
    // would paint in the unknown-role fallback instead of resetting. Nothing this
    // repository builds on today is such a target, which is exactly why it is worth
    // spelling rather than relying on.
    std::vector<signed char> roles(cells, static_cast<signed char>(-1)); // -1 = untouched
    // A THIRD GRID, AND ONLY A TEXT REGION'S ROWS EVER WRITE IT (HD-2). Rects and
    // labels have no ground to say -- `SurfaceRect` IS a ground and a
    // `SurfaceLabel` deliberately has none -- so every cell they touch carries
    // `role::kNone` and this grid emits nothing at all for them. That is what
    // makes the addition byte-invisible to every canvas that does not use it,
    // which the unchanged goldens are the proof of.
    std::vector<signed char> grounds(cells, static_cast<signed char>(zengine::surface::role::kNone));
    // A FOURTH GRID, AND ONLY A TEXT REGION'S SELECTED SPAN EVER WRITES IT (TEXT-0). It is a
    // separate channel rather than a fifth ground value because a selection composes with
    // every ink and every ground a row already has: the terminal's own word for "these exact
    // cells, whatever they are wearing" is reverse video, which swaps the two attributes the
    // other grids chose instead of competing with either. Rects and labels never set it, so a
    // canvas with no selection emits not one byte of it — the goldens are the proof.
    std::vector<signed char> selected(cells, static_cast<signed char>(0));

    const auto put = [&](std::int64_t x, std::int64_t y, char g, std::int64_t role,
                         std::int64_t ground = zengine::surface::role::kNone,
                         bool in_selection = false) {
        if (x < 0 || y < 0 || x >= w || y >= h) {
            return;
        }
        const std::size_t i = static_cast<std::size_t>(y * w + x);
        glyphs[i] = g;
        roles[i] = static_cast<signed char>(role);
        grounds[i] = static_cast<signed char>(ground);
        selected[i] = static_cast<signed char>(in_selection ? 1 : 0);
    };

    const auto write_label = [&](const zengine::surface::SurfaceLabel& l,
                                 std::int64_t ground = zengine::surface::role::kNone,
                                 std::int64_t sel_begin = 0, std::int64_t sel_end = 0) {
        for (std::size_t i = 0; i < l.text.size(); ++i) {
            const std::int64_t col = static_cast<std::int64_t>(i);
            put(add_cells(l.x, col), l.y, l.text[i], l.role, ground,
                col >= sel_begin && col < sel_end);
        }
    };

    // ONE WHOLE LAYER, THEN THE NEXT ONE OVER IT (WIND-2a). The two grids ARE the
    // painter's order -- a later write simply overwrites -- so executing the layers in
    // list order, and each layer's three kinds in their own order inside it, is the
    // complete implementation of the canvas's two-level law. Nothing is sorted, nothing
    // is composited, and a one-layer canvas produces byte-for-byte the picture this
    // function produced when the three lists were the canvas's own.
    for (const zengine::surface::SurfaceLayer& layer : c.layers) {
        // CLIPPED BEFORE ITERATING. `put` already refuses every cell off the canvas,
        // so the visible picture is the same either way -- but a canvas is a
        // ZEN_SHAPE, so `r.w` is a number a publisher chose, and walking it was the
        // publisher deciding how long this Skin runs. Both halves of that were
        // measured: a rect 100,000,000 cells wide on a 4x2 canvas cost 75 ms to produce
        // 38 bytes, and `r.x + dx` at the top of the number line was signed overflow
        // (UBSan, on committed code -- no test fed it such a canvas, so the standing
        // lane had nothing to catch). Both are gone by asking surface/cells.hpp for
        // the span first; see there for why the rule is shared with the SDL plan.
        for (const zengine::surface::SurfaceRect& r : layer.rects) {
            const char g = glyph_for_role(static_cast<int>(r.role));
            // THE ONE QUANTIZATION LAW AT THE CELL GRAIN (WUX-2): a fine rectangle
            // covers the cells its floored edges span, so a right edge that crosses a
            // cell boundary earns that cell. `r.x` is already the left edge's floor (a
            // remainder is 0..47 by decomposition, and a garbage one reads as zero),
            // and the carry below is the right edge's. Zero remainders leave every
            // span exactly what it always was — the TUI's picture of whole-cell
            // geometry has not moved by a byte.
            const std::int64_t carry_w =
                (zengine::surface::sub_rem(r.sub_x) + zengine::surface::sub_rem(r.sub_w)) /
                zengine::surface::kCellSubs;
            const std::int64_t carry_h =
                (zengine::surface::sub_rem(r.sub_y) + zengine::surface::sub_rem(r.sub_h)) /
                zengine::surface::kCellSubs;
            const CellSpan xs = clip_span(r.x, r.w >= 0 ? add_cells(r.w, carry_w) : r.w, w);
            const CellSpan ys = clip_span(r.y, r.h >= 0 ? add_cells(r.h, carry_h) : r.h, h);
            for (std::int64_t y = ys.begin; y < ys.end; ++y) {
                for (std::int64_t x = xs.begin; x < xs.end; ++x) {
                    put(x, y, g, r.role);
                }
            }
        }
        for (const zengine::surface::SurfaceLabel& l : layer.labels) {
            write_label(l);
        }
        // A TEXT REGION IS CELLS HERE, AND THAT IS THE HONEST ANSWER RATHER THAN THE
        // CHEAP ONE. A terminal's character is its cell; it owns no font it could set
        // a finer interior in, and inventing a pixel to divide would be this medium
        // claiming a capability it does not have. So the projection is the one in
        // region.hpp -- one row per cell row, cut at the region's width, dropped past
        // its height -- and it lands through the SAME `put` every label goes through,
        // last IN THIS LAYER, because a region is the topmost thing its own presentation
        // draws. A LATER layer still covers it, which is the whole of WIND-2a.
        for (const ProjectedRow& p : project_text_regions(layer)) {
            write_label(p.label, p.background, p.sel_begin, p.sel_end);
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
        // AND THE GROUND IN EFFECT, tracked separately because it changes
        // separately -- but reset TOGETHER, because `\x1b[0m` is all-attributes and
        // clears a ground that is still meant to be showing. So an ink change to
        // the untouched background re-states the ground after it, and a run of
        // cells that carry a ground and no role gets one `\x1b[0m` and one ground
        // rather than one per cell. With no row asking for a ground this whole
        // branch never fires and the bytes are the ones every golden already holds.
        int open_bg = zengine::surface::role::kNone;
        // AND WHETHER REVERSE VIDEO IS IN EFFECT (TEXT-0), tracked like the ground and for
        // the ground's reason: `\x1b[0m` is all-attributes and clears it, so a reset
        // re-states a selection that is still meant to be showing, and a run of selected
        // cells costs one `\x1b[7m` rather than one per cell. With no selected cell on the
        // canvas this branch never fires and the bytes are the ones every golden holds.
        bool open_sel = false;
        for (std::int64_t x = 0; x < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y * w + x);
            const int role = static_cast<int>(roles[i]);
            const int ground = static_cast<int>(grounds[i]);
            const bool in_selection = selected[i] != 0;
            if (role != open) {
                out += role < 0 ? "\x1b[0m" : sgr_for_role(role);
                if (role < 0) {
                    open_bg = zengine::surface::role::kNone; // the reset took the ground too
                    open_sel = false;                        // ...and the selection with it
                }
                open = role;
            }
            if (ground != open_bg) {
                out += ground < 0 ? "\x1b[49m" : sgr_bg_for_role(ground);
                open_bg = ground;
            }
            if (in_selection != open_sel) {
                out += in_selection ? "\x1b[7m" : "\x1b[27m";
                open_sel = in_selection;
            }
            out += glyphs[i];
        }
        if (open >= 0 || open_bg >= 0 || open_sel) {
            out += "\x1b[0m";
        }
        out += "\r\n";
    }
    return out;
}

/// THE ROWS A TUI SKIN SPENDS ON BEING A TUI SKIN — the whole of the difference
/// between "how big is this terminal" and "how much canvas fits in it" (TUI-0).
///
/// TWO OF THEM ARE FURNITURE, and they are not counted again here: row 1 is the status
/// slot and row 2 the score slot, which is exactly what `kTuiCanvasTopRow` already says
/// (pointing.hpp) and why `frame` and `canvas` below both begin at row 3. That constant
/// is the pointer path's answer to "where does canvas row 0 land", and this is the same
/// fact read from the other end — so it is consulted rather than restated. A second `2`
/// here would be a second opinion about the same two rows.
///
/// THE THIRD IS ARITHMETIC ABOUT THE LAST ROW, and it is the one worth writing down.
/// `canvas_body` ends EVERY row with CRLF, the last one included, so a canvas whose
/// final row lands on the terminal's final row moves the cursor one row past the
/// bottom — and a line feed at the bottom of a terminal SCROLLS. One row of the picture
/// would leave at the top of every single frame, and the two slots above would be the
/// first things off the screen.
///
/// The other way to buy that row back is to stop feeding after the last row, and it is
/// deliberately not taken: those bytes are what every terminal golden in this
/// repository pins, and one row of a forty-row terminal is a cheaper thing to spend
/// than the meaning of a byte-exact projection.
inline constexpr std::int64_t kTuiScrollGuardRows = 1; ///< where the last row's CRLF lands
inline constexpr std::int64_t kTuiReservedRows = kTuiCanvasTopRow + kTuiScrollGuardRows;

/// WHAT A TERMINAL OF THIS SIZE HAS ROOM FOR, AS A CANVAS EXTENT. Pure, so every lane
/// pins it — including the ones with no terminal anywhere near them, which is the whole
/// reason the measurement and the arithmetic are two functions rather than one.
///
/// A CHARACTER IS A CELL HERE, so the columns pass through untouched and the text
/// metric is ZERO on both axes. That is not a placeholder and not a measurement this
/// medium failed to take: `SurfaceExtent`'s own vocabulary spells zero as "this medium
/// presents text in cells", which is the truth in a terminal and the thing every
/// consumer of the metric already knows how to read. A TUI answering in pixels would be
/// claiming a face it does not own.
///
/// AND SO IS THE CANVAS ITSELF (WUX-6): `cell_px` is zero, which the vocabulary spells
/// "this medium's device unit IS the canvas cell". That is a terminal's permanent
/// answer rather than a starting one -- a terminal has no finer unit to report, ever --
/// which is why it is the same zero the metric already carries and not a second kind of
/// absence. A maker arranging a pane here reads CELLS because cells are what this
/// medium can distinguish.
///
/// AN UNMEASURED TERMINAL AND A TERMINAL WITH NO ROOM LEFT BOTH ANSWER `{}`, and they
/// are two different sentences — "nobody could tell me" and "there is not one row over"
/// — that this medium has no way to say apart to its shell. `SkinT::report_extent`
/// turns either into SILENCE, and silence leaves a publisher on whatever extent it
/// already had, which for a fresh Workshop is its own documented minimum. The third
/// sentence, "there is no room", is the one nobody may say: it is what publishing zero
/// would mean, and it is false in both cases.
inline constexpr SurfaceExtent tui_canvas_extent(const TerminalSize& t) noexcept {
    if (!t.measured() || t.rows <= kTuiReservedRows) {
        return SurfaceExtent{};
    }
    return SurfaceExtent{t.cols, t.rows - kTuiReservedRows, 0, 0, 0};
}

/// STANDARD BASE64, because OSC 52 speaks nothing else. Pure and total; no padding
/// subtleties beyond the two `=` forms, and no alternate alphabets — the sequence's
/// consumers are terminals, and terminals read RFC 4648 or read nothing.
inline std::string tui_base64(const std::string& bytes) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);
    std::size_t i = 0;
    for (; i + 3 <= bytes.size(); i += 3) {
        const std::uint32_t n = (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i])) << 16) |
                                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i + 1])) << 8) |
                                static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i + 2]));
        out.push_back(kAlphabet[(n >> 18) & 0x3Fu]);
        out.push_back(kAlphabet[(n >> 12) & 0x3Fu]);
        out.push_back(kAlphabet[(n >> 6) & 0x3Fu]);
        out.push_back(kAlphabet[n & 0x3Fu]);
    }
    const std::size_t left = bytes.size() - i;
    if (left == 1) {
        const std::uint32_t n = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i])) << 16;
        out.push_back(kAlphabet[(n >> 18) & 0x3Fu]);
        out.push_back(kAlphabet[(n >> 12) & 0x3Fu]);
        out += "==";
    } else if (left == 2) {
        const std::uint32_t n = (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i])) << 16) |
                                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i + 1])) << 8);
        out.push_back(kAlphabet[(n >> 18) & 0x3Fu]);
        out.push_back(kAlphabet[(n >> 12) & 0x3Fu]);
        out.push_back(kAlphabet[(n >> 6) & 0x3Fu]);
        out.push_back('=');
    }
    return out;
}

/// WHAT A TERMINAL MEDIUM DOES WITH A COPY: `OSC 52 ; c ; <base64> BEL` — the in-band
/// set-clipboard sequence, written to the same stream the alternate screen and pointer
/// reporting already travel, because the OUTPUT side of the terminal is the Skin's (the rule
/// `kTuiPointerOn` states). Pure, so the suite pins the exact bytes.
///
/// THE HONEST LIMIT, STATED RATHER THAN DRESSED UP: whether the terminal on the far end
/// honours OSC 52 is a per-terminal, per-configuration fact this medium has no way to ask —
/// modern emulators largely do, stock xterm wants `allowWindowOps`, and a pipe is not a
/// terminal at all. Writing the sequence where it is not honoured costs nothing and does
/// nothing; what this medium therefore never claims is that the SYSTEM clipboard took the
/// text. Inside the process the copy is already true either way — the application heard the
/// same `ClipboardCopy` this medium did (vocabulary.hpp) — and READING a system clipboard
/// has no truthful terminal route at all (the OSC 52 query is disabled almost everywhere for
/// exactly the reason it should be), so paste on this medium means what the process itself
/// has copied. That asymmetry is the medium's, and it is the strongest truthful answer a
/// terminal has.
inline std::string tui_clipboard_sequence(const std::string& text) {
    return "\x1b]52;c;" + tui_base64(text) + "\x07";
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
        const std::int64_t rows = c.height > 0 ? c.height : 0;
        std::string out = "\x1b[3;1H";
        if (first) {
            out += "\x1b[0J";
        }
        out += canvas_body(c);
        // GIVE BACK THE ROWS THIS CANVAS STOPPED USING (TUI-0). The first frame CLAIMS
        // everything below row 3; a later frame SHORTER than the one before it has to
        // hand the difference back, or the tail of the taller picture stays on the
        // screen underneath the shorter one — which is exactly what a maker sees when
        // they drag a terminal's bottom edge upwards and the canvas follows it in.
        //
        // The cursor is one row past the last row just written (every row ends with a
        // feed), so erase-below erases precisely the difference and nothing else.
        //
        // ONLY ON A SHRINK, and that is what keeps this honest rather than merely
        // convenient: a steady frame writes the bytes it has always written, so every
        // golden in this repository is unmoved and the erase can only appear where
        // something genuinely needed erasing. It costs one integer, a plain member for
        // `SkinT::reported_`'s reason — the screen belongs to an INCARNATION, and a
        // fresh one begins on an alternate screen its own constructor just cleared,
        // having painted nothing into it.
        //
        // There is no damage tracking here and no dirty-region system: a terminal
        // canvas repaints itself whole every frame already, so the only thing that can
        // go stale is the part it stopped painting at all.
        if (!first && rows < painted_rows_) {
            out += "\x1b[0J";
        }
        painted_rows_ = rows;
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

    /// A maker copied text: offer it to the terminal's clipboard, in the one voice a
    /// terminal has for that. See `tui_clipboard_sequence` for exactly what is and is not
    /// being claimed.
    void clipboard_copy(const std::string& text) { sink_.write(tui_clipboard_sequence(text)); }

    /// A maker asked to paste: this medium CANNOT SAY what the system clipboard holds, and
    /// says so (QR-11). Reading it has no truthful terminal route — the OSC 52 query is
    /// disabled almost everywhere, for exactly the reason it should be
    /// (`tui_clipboard_sequence`'s honesty note) — so the answer is the standing nullopt,
    /// never a guess dressed as a read. The asker then pastes what this process itself
    /// last copied, which is the strongest truthful paste a terminal has, and is why
    /// copy-here-paste-there keeps working on this medium with no platform claim anywhere.
    std::optional<std::string> clipboard_text() { return std::nullopt; }

    /// A TERMINAL APPLICATION HAS NO DESKTOP PLACEMENT FACT AT ALL (WUX-3). The window a
    /// maker sees belongs to the terminal emulator, which tells its guests nothing about
    /// where it sits and takes no instructions about it — so this medium answers the
    /// honest absence, `SkinT::report_placement` publishes nothing for it, and a
    /// remembered placement offered back is received and truthfully not acted on. Neither
    /// is a stub waiting to be filled in: they are what a terminal IS, said in one line
    /// each, exactly as `clipboard_text`'s nullopt is.
    std::optional<SurfacePlacement> placement() { return std::nullopt; }
    void place(const SurfacePlacementRemembered&) {}

    /// HOW MUCH ROOM THERE IS — ASKED OF THE SINK, BECAUSE THE SINK IS THE TERMINAL.
    ///
    /// G-2 left this answering `{0,0}` forever and named the trigger for changing it:
    /// "a real terminal size arriving with a real consumer for it: a `Sink` that can be
    /// ASKED its extent". Both halves arrived. HD-1 through HD-6 made every bounded
    /// region in Workshop spend the room its medium reports — the Inspector's property
    /// body, the pane's prose, the omission markers, a TextBox's window — so a terminal
    /// keeping its size to itself became the one medium withholding cells a publisher
    /// would have used. And the layer that owns the terminal is the layer that can be
    /// asked about it, which is the Sink (TUI-0).
    ///
    /// A window Skin owns a drawable whose size is its own to read; this one owns a
    /// stream and asks the operating system about the far end of it. The distinction
    /// that survives is about WHO answers, not about whether anyone can: `TuiTerminal`
    /// holds a real console and answers, a std::string in a suite holds nothing and says
    /// so, and a pipe is a far end that is not a terminal at all. One call, three honest
    /// answers.
    ///
    /// STILL NO OPINION WHEN THERE IS NOTHING TO HAVE ONE ABOUT. `tui_canvas_extent`
    /// turns an unmeasurable terminal back into `{0,0}`, `SkinT::report_extent` publishes
    /// nothing for it, and a publisher hears no claim rather than a wrong one — so a
    /// redirected, piped, captured or headless run is byte-for-byte the run it was
    /// before this phase.
    SurfaceExtent extent() const { return tui_canvas_extent(sink_.size()); }

    Sink& sink() { return sink_; }

private:
    Sink sink_;
    /// How many rows the canvas this medium last painted had — per incarnation, never
    /// state, and read by exactly one branch. See `canvas` above.
    std::int64_t painted_rows_ = 0;
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

    /// HOW BIG THE TERMINAL IS, at the moment of asking (TUI-0).
    ///
    /// GATED ON THE SAME `ok_` THE CLAIM IS, so the two facts cannot drift apart: a run
    /// that found no terminal to take does not have one to measure. On POSIX that is
    /// `isatty(STDOUT)`; on Windows it is a console mode this handle actually has. So a
    /// redirected, piped or captured run answers "no terminal" here for precisely the
    /// reason it drew no alternate screen, rather than for a second reason that might
    /// one day disagree with the first.
    ///
    /// The measurement itself is `native_terminal_size()`, in its own header, because it
    /// is the one thing in this file that has to know which operating system it is on.
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

#endif // ZENGINE_SURFACE_SKIN_TUI_HPP
