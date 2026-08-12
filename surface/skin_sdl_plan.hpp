// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_SKIN_SDL_PLAN_HPP
#define ZENGINE_SURFACE_SKIN_SDL_PLAN_HPP

// The SDL Skin's brain, SDL-free (the zen-ui-pixel move): a SnakeVisual
// becomes a list of colored rectangles and a window geometry, as pure math.
// Every lane pins this — including the ones that never build SDL — so the
// only thing the SDL edge (skin_sdl.cpp) adds is executing the plan against a
// real renderer, and the only thing that can be wrong THERE is plumbing.
//
// The plan speaks pixels because the medium does; the INTENT it consumes has
// no pixel anywhere in it. That direction — medium-specific numbers derived
// from medium-agnostic intent, never the reverse — is the agnosticism the
// phase exists to prove.
//
// The general canvas is here for the same reason, and gains one property from it
// that is worth naming: `plan_canvas` returns rectangles and labels as ONE flat
// list of opaque quads, so the SDL edge cannot tell a glyph from a rect.
// "Rectangles drawn, labels dropped" is therefore not sayable at the edge at
// all; the only place that distinction exists is in this header, where every
// lane's suite is already looking.

#include "cells.hpp"
#include "skin_sdl_glyphs.hpp"
#include "snake/vocabulary.hpp"
#include "vocabulary.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace zengine::surface {

/// One filled rectangle, window coordinates, opaque color.
struct PlanRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

struct PlanSize {
    std::int64_t w = 0;
    std::int64_t h = 0;
};

inline constexpr std::int64_t kCellPx = 24;  ///< one grid cell, square
inline constexpr std::int64_t kMarginPx = 12; ///< board inset from the window edge
inline constexpr std::int64_t kCellGapPx = 1; ///< breathing room inside a cell

/// The window sized to the board: the visual's grid dictates geometry, the
/// medium never dictates the visual (a growth mid-game is a window resize).
inline PlanSize window_size_of(const zengine::snake::SnakeVisual& v) {
    return PlanSize{v.width * kCellPx + 2 * kMarginPx, v.height * kCellPx + 2 * kMarginPx};
}

/// The frame as rectangles, background first (painter's order):
///   - background: near-black while alive, a dark red the moment the run dies
///     (the whole window is the death banner — no font required);
///   - food: gold, inset deeper than the snake so it reads as a pickup;
///   - body: green; head: brighter green, drawn last.
/// The off-board food sentinel (kNoFood) simply plans no food rectangle.
inline std::vector<PlanRect> plan_frame(const zengine::snake::SnakeVisual& v) {
    std::vector<PlanRect> out;
    out.reserve(v.snake.size() + 2);
    const PlanSize win = window_size_of(v);

    if (v.alive) {
        out.push_back(PlanRect{0, 0, win.w, win.h, 18, 18, 24});
    } else {
        out.push_back(PlanRect{0, 0, win.w, win.h, 88, 16, 16});
    }

    const auto cell = [](std::int64_t cx, std::int64_t cy, std::int64_t inset) {
        return PlanRect{kMarginPx + cx * kCellPx + inset, kMarginPx + cy * kCellPx + inset,
                        kCellPx - 2 * inset, kCellPx - 2 * inset, 0, 0, 0};
    };

    if (v.food.x >= 0 && v.food.x < v.width && v.food.y >= 0 && v.food.y < v.height) {
        PlanRect f = cell(v.food.x, v.food.y, kCellPx / 4);
        f.r = 232;
        f.g = 196;
        f.b = 64;
        out.push_back(f);
    }

    for (std::size_t i = v.snake.size(); i > 1; --i) {
        PlanRect s = cell(v.snake[i - 1].x, v.snake[i - 1].y, kCellGapPx);
        s.r = 64;
        s.g = 168;
        s.b = 80;
        out.push_back(s);
    }
    if (!v.snake.empty()) {
        PlanRect h = cell(v.snake.front().x, v.snake.front().y, kCellGapPx);
        h.r = 112;
        h.g = 232;
        h.b = 128;
        out.push_back(h);
    }
    return out;
}

// ---- The general canvas -----------------------------------------------------------
//
// Everything below turns a `SurfaceCanvas` into the same `PlanRect` list the
// snake frame already produces. See the header note on why labels and rects
// arrive as one indistinguishable list.

/// This medium's ink per semantic canvas role — the counterpart of the TUI's SGR
/// table, and the proof the role vocabulary was worth having: two media, two
/// completely unrelated palettes, one unchanged publisher. Unknown roles paint as
/// `kFill`, per vocabulary.hpp.
///
/// It lives in the plan rather than at the SDL edge so that "which ink did this
/// role get" is a fact every lane can pin, SDL built or not.
struct PlanInk {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    friend bool operator==(const PlanInk&, const PlanInk&) = default;
};

inline constexpr PlanInk ink_for_role(std::int64_t role) noexcept {
    switch (role) {
    case role::kAccent: return PlanInk{112, 232, 240};
    case role::kMuted: return PlanInk{96, 96, 108};
    case role::kAlert: return PlanInk{232, 72, 72};
    default: return PlanInk{176, 176, 188};
    }
}

/// What a cell shows where nothing was published — and what a label's own cell is
/// cleared to before its glyph is drawn. See `plan_canvas` on why a label takes
/// its whole cell rather than sitting on top of whatever was under it.
inline constexpr PlanInk kCanvasBackground{18, 18, 24};

/// One glyph pixel, in device pixels. Chosen so a glyph fills a canvas cell
/// exactly: no resampling, no partial cells, and a label's Nth character sits in
/// the canvas cell the publisher named and nowhere else.
inline constexpr std::int64_t kGlyphScale = kCanvasCellPx / kGlyphCols;
static_assert(kCanvasCellPx % kGlyphCols == 0,
              "a canvas cell must be a whole number of glyph pixels wide");
static_assert(kCanvasCellPx % kGlyphRows == 0,
              "a canvas cell must be a whole number of glyph pixels tall");

/// The largest canvas extent this medium will consider, in cells.
///
/// Not a policy about how big a canvas may be — it is exactly how many cells a
/// pixel number can hold. `cell * kCanvasCellPx` is the first arithmetic every
/// element goes through, and a published `width` of 10^18 makes that multiply
/// undefined before anything has been drawn or refused. Clamping the extent here
/// makes every pixel product in this file safe by construction rather than by
/// each caller remembering.
inline constexpr std::int64_t kMaxCanvasCells =
    (std::numeric_limits<std::int64_t>::max)() / kCanvasCellPx;

/// The canvas extent this plan will actually work in, in cells: what was
/// published, floored at nothing and capped at what pixels can express.
inline constexpr std::int64_t canvas_extent(std::int64_t published) noexcept {
    if (published <= 0) {
        return 0;
    }
    return published < kMaxCanvasCells ? published : kMaxCanvasCells;
}

/// The window this canvas asks for: its extent in cells, in pixels.
inline constexpr PlanSize canvas_window_size(const SurfaceCanvas& c) noexcept {
    return PlanSize{canvas_extent(c.width) * kCanvasCellPx,
                    canvas_extent(c.height) * kCanvasCellPx};
}

/// The other direction, and the only one that is new in G-2: how many WHOLE
/// canvas cells a drawable of this many pixels has room for.
///
/// FLOORED, and it has to be: a cell that is only three quarters on the surface
/// is not room for a cell, and a publisher told otherwise would author a row it
/// could not see the bottom of. The remainder — up to `kCanvasCellPx - 1` pixels
/// on each axis — is simply background, which is why `canvas` clears the whole
/// drawable before it draws anything.
///
/// It lives here rather than at the SDL edge for the reason everything else in
/// this header does: it is pure arithmetic, so every lane pins it, including the
/// ones that build no SDL at all. Negative or zero pixels answer zero — a
/// surface with no room says so honestly, and `SkinT::report_extent` turns that
/// into silence rather than a claim.
inline constexpr SurfaceExtent extent_of_drawable(const PlanSize& px) noexcept {
    return SurfaceExtent{px.w > 0 ? px.w / kCanvasCellPx : 0,
                         px.h > 0 ? px.h / kCanvasCellPx : 0};
}

/// The whole canvas as one flat list of opaque quads, in painter's order.
///
/// Rectangles first in list order, then every label's cells over them — the same
/// order `canvas_body` rasterizes for a terminal, so the two media agree about
/// what is on top without either knowing the other exists.
///
/// A LABEL TAKES ITS CELL. The terminal's `put` overwrites both the glyph AND the
/// role of every cell a label lands on, so a label cell there shows the label's
/// character on the terminal's own background and the rect underneath is simply
/// gone from that cell. The closest thing this medium can do is clear each label
/// cell to the canvas background and draw the glyph over it, which is what it
/// does: text never has to compete with a fill it happens to be sitting on.
///
/// CLIPPING IS PER CELL, against the canvas extent and nothing else — the same
/// rule and the same bounds the terminal applies, so a label hanging off any edge
/// (or starting at a negative coordinate) loses exactly the cells that are not on
/// the canvas and keeps the rest.
///
/// The shape of the work is one pass: each rect is a single quad, and each label
/// cell is one background quad plus one quad per horizontal run of ink in its
/// glyph — at most three per glyph row. Nothing is allocated per character, no
/// texture is created, and there is no state to cache between frames.
inline std::vector<PlanRect> plan_canvas(const SurfaceCanvas& c) {
    std::vector<PlanRect> out;
    const std::int64_t w = canvas_extent(c.width);
    const std::int64_t h = canvas_extent(c.height);
    if (w == 0 || h == 0) {
        return out; // an empty canvas is a legitimate picture: it draws nothing
    }

    const auto quad = [&out](std::int64_t x, std::int64_t y, std::int64_t pw, std::int64_t ph,
                             PlanInk ink) {
        out.push_back(PlanRect{x, y, pw, ph, ink.r, ink.g, ink.b});
    };

    for (const SurfaceRect& r : c.rects) {
        // Clipped in CELLS, before any pixel arithmetic — which is what keeps the
        // multiply below inside the number line whatever the wire said. Same
        // helper the terminal Skin clips with, so the two media cannot come to
        // disagree about what is on the canvas.
        const CellSpan xs = clip_span(r.x, r.w, w);
        const CellSpan ys = clip_span(r.y, r.h, h);
        if (xs.empty() || ys.empty()) {
            continue;
        }
        quad(xs.begin * kCanvasCellPx, ys.begin * kCanvasCellPx, xs.count() * kCanvasCellPx,
             ys.count() * kCanvasCellPx, ink_for_role(r.role));
    }

    for (const SurfaceLabel& l : c.labels) {
        if (l.y < 0 || l.y >= h) {
            continue; // no row of this canvas belongs to it
        }
        const PlanInk ink = ink_for_role(l.role);
        const std::int64_t cell_y = l.y * kCanvasCellPx;
        for (std::size_t i = 0; i < l.text.size(); ++i) {
            const std::int64_t cx = add_cells(l.x, static_cast<std::int64_t>(i));
            if (cx < 0) {
                continue; // before the canvas starts; a later character may land
            }
            if (cx >= w) {
                break; // every remaining character is further right still
            }
            const std::int64_t cell_x = cx * kCanvasCellPx;
            quad(cell_x, cell_y, kCanvasCellPx, kCanvasCellPx, kCanvasBackground);
            const Glyph& g = glyph_of(static_cast<unsigned char>(l.text[i]));
            for (int gy = 0; gy < kGlyphRows; ++gy) {
                int gx = 0;
                while (gx < kGlyphCols) {
                    if ((g.row[gy] & (1u << gx)) == 0) {
                        ++gx;
                        continue;
                    }
                    const int run_start = gx;
                    while (gx < kGlyphCols && (g.row[gy] & (1u << gx)) != 0) {
                        ++gx;
                    }
                    quad(cell_x + run_start * kGlyphScale, cell_y + gy * kGlyphScale,
                         (gx - run_start) * kGlyphScale, kGlyphScale, ink);
                }
            }
        }
    }
    return out;
}

/// The window title carries the text slots — a real, visible projection of
/// SurfaceText that costs no font stack. ASCII by the house charset rule.
inline std::string title_of(const std::string& status, const std::string& score) {
    std::string t = "zengine [sdl skin]";
    if (!status.empty()) {
        t += " | " + status;
    }
    if (!score.empty()) {
        t += " | " + score;
    }
    return t;
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_SKIN_SDL_PLAN_HPP
