// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_SKIN_TUI_HPP
#define ZENGINE_SURFACE_SKIN_TUI_HPP

// The terminal medium, whole and pinnable headless: ClassicStyle and BlockStyle turn a
// SnakeVisual into the old snake drawers' exact bytes; TuiMedium lays rows 1-2 out as the status
// and score slots and the canvas from row 3; TuiTerminal is the real Sink. A Sink has
// `write(std::string_view)` and `TerminalSize size() const`, required rather than detected: a
// Sink lacking `size()` would look exactly like an unmeasurable terminal, on every lane.
// Surface law: agents/surface.md

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

/// The classic look: one character per cell, an ASCII border, monochrome, banner underneath --
/// the old classic drawer's frame, byte for byte but for the cursor-home prefix (the medium's
/// now) and the banner's word "skin".
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

/// This medium's ink for each canvas role; an unknown role paints as `kFill`.
inline const char* sgr_for_role(int role) noexcept {
    switch (role) {
    case 1: return "\x1b[36m";    // kAccent — cyan: the thing being pointed at
    case 2: return "\x1b[90m";    // kMuted  — bright black: present, quiet
    case 3: return "\x1b[31;1m";  // kAlert  — bold red: must be seen
    case 4: return "\x1b[30m";    // kGround — black: opaque empty material
    default: return "\x1b[37m";   // kFill and anything unknown — plain ink
    }
}

/// This medium's ground for each role: SGR 40-47 and 100-107 are all an ANSI terminal has.
/// `role::kNone` is not in the table: it is the absence of a ground, restored by `\x1b[49m`. A
/// publisher still chooses contrasting roles -- an ink on its own ground is invisible.
inline const char* sgr_bg_for_role(int role) noexcept {
    switch (role) {
    case 1: return "\x1b[46m";  // kAccent — cyan ground
    case 2: return "\x1b[100m"; // kMuted  — bright black: the selection bar
    case 3: return "\x1b[41m";  // kAlert  — red ground
    case 4: return "\x1b[40m";  // kGround — black ground
    default: return "\x1b[47m"; // kFill and anything unknown — plain ground
    }
}

/// This medium's glyph for each role: colour alone would be a lie on a monochrome terminal, so
/// roles need distinct glyphs. An empty ground is a space, replacing earlier material.
inline char glyph_for_role(int role) noexcept {
    switch (role) {
    case 1: return '*'; // kAccent
    case 2: return '.'; // kMuted
    case 3: return '!'; // kAlert
    case 4: return ' '; // kGround
    default: return '#'; // kFill and anything unknown
    }
}

/// The four grids a canvas rasterizes to, at the cell grain: glyph, role, ground, selection.
/// Built once and read by `canvas_body` and `canvas_cells`, so a capture cannot disagree with
/// the picture by a byte.
struct CanvasGrids {
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::size_t cells = 0;
    std::vector<char> glyphs;
    std::vector<signed char> roles;
    std::vector<signed char> grounds;
    std::vector<signed char> selected;
};

inline CanvasGrids rasterize_canvas(const zengine::surface::SurfaceCanvas& c) {
    CanvasGrids grids;
    const std::int64_t w = c.width > 0 ? c.width : 0;
    const std::int64_t h = c.height > 0 ? c.height : 0;
    grids.w = w;
    grids.h = h;
    if (w == 0 || h == 0) {
        return grids;
    }
    // Two parallel grids, painter's order by overwriting: later rects win, labels over rects.
    // A sized vector, not a std::string: a write below the extent then overflows the heap, so a
    // sanitizer can see the bottom-edge guard is still there (spare string capacity hid it).
    const std::size_t cells = static_cast<std::size_t>(w * h);
    grids.cells = cells;
    std::vector<char>& glyphs = grids.glyphs;
    glyphs.assign(cells, ' ');
    // Signed explicitly: these hold a -1 sentinel read back with `< 0`, and plain `char` is
    // unsigned on some targets (ARM), where an untouched cell would paint as the fallback role.
    std::vector<signed char>& roles = grids.roles;
    roles.assign(cells, static_cast<signed char>(-1)); // -1 = untouched
    // The third grid holds explicit row grounds and opaque kGround rectangles.
    // Ordinary material rectangles and labels replace a whole cell without
    // claiming a background. A beneath region keeps the prior ground wherever
    // its row asks for none; an owned region clears it.
    std::vector<signed char>& grounds = grids.grounds;
    grounds.assign(cells, static_cast<signed char>(zengine::surface::role::kNone));
    // The fourth grid, written only by a region's selected span: a separate channel, because
    // reverse video composes with every ink and ground. No selection, no byte of it.
    std::vector<signed char>& selected = grids.selected;
    selected.assign(cells, static_cast<signed char>(0));

    const auto put = [&](std::int64_t x, std::int64_t y, char g, std::int64_t role,
                         std::int64_t ground = zengine::surface::role::kNone,
                         bool in_selection = false, bool keep_ground = false) {
        if (x < 0 || y < 0 || x >= w || y >= h) {
            return;
        }
        const std::size_t i = static_cast<std::size_t>(y * w + x);
        glyphs[i] = g;
        roles[i] = static_cast<signed char>(role);
        if (!keep_ground || ground != zengine::surface::role::kNone)
            grounds[i] = static_cast<signed char>(ground);
        selected[i] = static_cast<signed char>(in_selection ? 1 : 0);
    };

    const auto write_label = [&](const zengine::surface::SurfaceLabel& l,
                                 std::int64_t ground = zengine::surface::role::kNone,
                                 std::int64_t sel_begin = 0, std::int64_t sel_end = 0,
                                 bool keep_ground = false) {
        for (std::size_t i = 0; i < l.text.size(); ++i) {
            const std::int64_t col = static_cast<std::int64_t>(i);
            put(add_cells(l.x, col), l.y, l.text[i], l.role, ground,
                col >= sel_begin && col < sel_end, keep_ground);
        }
    };

    // One whole layer, then the next over it: the grids are painter's order (a later write
    // overwrites), so executing the layers and each layer's kinds in order is the whole law.
    for (const zengine::surface::SurfaceLayer& layer : c.layers) {
        // Clipped before iterating (surface/cells.hpp): a rect's size is a publisher's number,
        // and walking it let a publisher decide how long this Skin runs (measured: 75 ms for a
        // 100,000,000-cell rect on a 4x2 canvas) and overflowed at the top of the number line.
        for (const zengine::surface::SurfaceRect& r : layer.rects) {
            const char g = glyph_for_role(static_cast<int>(r.role));
            // A fine rect covers the cells its floored edges span: `r.x` is the left edge's
            // floor, and the carry below is the right edge's. Zero remainders move no byte.
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
                    put(x, y, g, r.role, r.role == zengine::surface::role::kGround
                        ? r.role : zengine::surface::role::kNone);
                }
            }
        }
        for (const zengine::surface::SurfaceLabel& l : layer.labels) {
            write_label(l);
        }
        // A region is cells here, projected by region.hpp -- one row per cell row, cut at its
        // width, dropped past its height -- through the same `put` as a label, last in its
        // layer: a terminal owns no face to set a finer interior in.
        for (const ProjectedRow& p : project_text_regions(layer)) {
            write_label(p.label, p.background, p.sel_begin, p.sel_end,
                        p.ground == zengine::surface::kGroundBeneath);
        }
    }

    return grids;
}

/// A canvas's cells as plain text -- the glyphs `canvas_body` paints, less the ink: `height`
/// rows of `width` bytes, each ending in a newline. A terminal's capture (`text/cells`).
inline std::string canvas_cells(const zengine::surface::SurfaceCanvas& c) {
    const CanvasGrids g = rasterize_canvas(c);
    std::string out;
    out.reserve(static_cast<std::size_t>((g.w + 1) * g.h));
    for (std::int64_t y = 0; y < g.h; ++y) {
        out.append(g.glyphs.data() + static_cast<std::size_t>(y * g.w),
                   static_cast<std::size_t>(g.w));
        out += '\n';
    }
    return out;
}

/// The canvas rasterized to the terminal, as exact bytes: one character per cell, role as SGR
/// ink, layers in list order, one complete plane at a time. One rasterizer for both styles: the
/// styles are the snake drawers' looks, and a canvas has no drawer to be faithful to. Clipped to
/// the extent; a label is cut on a byte, which is why intent is plain ASCII.
inline std::string canvas_body(const zengine::surface::SurfaceCanvas& c) {
    const CanvasGrids g = rasterize_canvas(c);
    const std::int64_t w = g.w;
    const std::int64_t h = g.h;
    if (w == 0 || h == 0) {
        return {};
    }
    const std::size_t cells = g.cells;
    const std::vector<char>& glyphs = g.glyphs;
    const std::vector<signed char>& roles = g.roles;
    const std::vector<signed char>& grounds = g.grounds;
    const std::vector<signed char>& selected = g.selected;
    std::string out;
    out.reserve(cells * 3);
    for (std::int64_t y = 0; y < h; ++y) {
        out += "\x1b[2K";
        // The role in effect starts at a value no role (not even the background's -1) can
        // equal, so every row's first cell states its own ink rather than inheriting one.
        int open = -2;
        // The ground in effect, tracked apart but reset together: `\x1b[0m` also clears a
        // ground still meant to show, so a reset re-states it. With no ground, no byte of this.
        int open_bg = zengine::surface::role::kNone;
        // ...and whether reverse video is in effect, for the ground's reason.
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

/// The rows a TUI Skin spends on being one: the two slots (`kTuiCanvasTopRow`, consulted rather
/// than restated) and one because `canvas_body` ends every row with CRLF, and a feed on a
/// terminal's last row scrolls, taking the slots with it. Not feeding after the last row would
/// buy the row back and move every terminal golden; a row is the cheaper price.
inline constexpr std::int64_t kTuiScrollGuardRows = 1; ///< where the last row's CRLF lands
inline constexpr std::int64_t kTuiReservedRows = kTuiCanvasTopRow + kTuiScrollGuardRows;

/// What a terminal of this size has room for, as a canvas extent; pure, so every lane pins it.
/// A character is a cell here, so the text metric and `cell_px` are zero: "text is a cell" and
/// "my device unit is the cell". An unmeasured terminal and one with no row to spare both
/// answer `{}`, which `SkinT::report_extent` turns into silence; publishing zero would claim
/// there is no room, which is false in both cases.
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

/// A copy, in a terminal's one voice for it: OSC 52 set-clipboard, on the stream the Skin
/// already owns. Whether the terminal honours it cannot be asked (xterm wants `allowWindowOps`;
/// a pipe is no terminal), so nothing claims the system took the text; in the process the copy
/// is true anyway (`ClipboardCopy`). No truthful terminal route reads a system clipboard, so a
/// paste here means what this process copied.
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
        // Give back the rows this canvas stopped using: a frame shorter than the last erases
        // below itself (the cursor is one row past its last row), or a terminal dragged shorter
        // keeps the taller picture's tail. Only on a shrink, so a steady frame writes the bytes
        // every golden pins. `painted_rows_` is per incarnation: a fresh one starts on an
        // alternate screen its constructor just cleared.
        if (!first && rows < painted_rows_) {
            out += "\x1b[0J";
        }
        painted_rows_ = rows;
        last_canvas_ = c;
        sink_.write(out);
    }

    /// THE CELL PROJECTION OF THE LAST CANVAS, as plain rows: what a terminal presented, less
    /// the ink -- a byte per cell, exactly the glyphs `canvas_body` wrote, one row per line.
    /// A medium that has painted no canvas has no picture and says so.
    std::optional<CapturedPicture> capture() {
        if (!last_canvas_.has_value()) {
            return std::nullopt;
        }
        CapturedPicture p;
        p.width = last_canvas_->width > 0 ? last_canvas_->width : 0;
        p.height = last_canvas_->height > 0 ? last_canvas_->height : 0;
        p.cell_px = 0;
        p.format = "text/cells";
        p.bytes = canvas_cells(*last_canvas_);
        return p;
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

    /// A paste: this medium cannot say what the system clipboard holds, and says so, never a
    /// guess dressed as a read. The asker then pastes what this process last copied.
    std::optional<std::string> clipboard_text() { return std::nullopt; }

    /// A terminal application has no desktop placement: the emulator owns the window, tells its
    /// guests nothing and takes no instructions, so the absence is the whole honest answer.
    std::optional<SurfacePlacement> placement() { return std::nullopt; }
    void place(const SurfacePlacementRemembered&) {}

    /// How much room there is, asked of the Sink because the Sink holds the terminal:
    /// `TuiTerminal` answers; a string in a suite and a pipe have no terminal to ask, and an
    /// unmeasurable terminal publishes nothing (`tui_canvas_extent`).
    SurfaceExtent extent() const { return tui_canvas_extent(sink_.size()); }

    Sink& sink() { return sink_; }

private:
    Sink sink_;
    /// How many rows the canvas this medium last painted had — per incarnation, never
    /// state, and read by exactly one branch. See `canvas` above.
    std::int64_t painted_rows_ = 0;
    /// The last canvas painted, kept for `capture`: a terminal cannot be read back, so the
    /// medium keeps what it drew. Per incarnation, never state.
    std::optional<zengine::surface::SurfaceCanvas> last_canvas_;
};

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

#endif // ZENGINE_SURFACE_SKIN_TUI_HPP
