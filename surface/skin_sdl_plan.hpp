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
// that is worth naming: a layer's rectangles and labels come back as ONE flat
// list of opaque quads, so the SDL edge cannot tell a glyph from a rect.
// "Rectangles drawn, labels dropped" is therefore not sayable at the edge at
// all; the only place that distinction exists is in this header, where every
// lane's suite is already looking.
//
// AND SINCE WIND-2a THE ORDER BETWEEN PLANES IS HERE TOO. `plan_canvas` returns one
// `PlanLayer` per published layer, each holding its own quads and its own real-face
// regions, so "which of these two presentations is in front" is answered in pure code
// every lane pins rather than by the sequence of loops somebody wrote at the edge.

#include "cells.hpp"
#include "region.hpp"
#include "skin_sdl_glyphs.hpp"
#include "snake/vocabulary.hpp"
#include "vocabulary.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
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

    friend bool operator==(const PlanRect&, const PlanRect&) = default;
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
/// cleared to before its glyph is drawn. See `plan_layer_quads` on why a label takes
/// its whole cell rather than sitting on top of whatever was under it.
inline constexpr PlanInk kCanvasBackground{18, 18, 24};

/// WHAT THIS MEDIUM PUTS UNDER SELECTED TEXT (TEXT-0) — a band the glyphs are drawn over,
/// which is the graphical answer to the terminal's reverse video: the text keeps its own
/// ink and the GROUND under exactly the selected span changes. It is a medium constant and
/// not a role, because a selection is not something a publisher styles — the publisher said
/// WHICH text is selected (the region's `sel_*` fields) and each medium answers with the
/// one voice it has. A distinctly-hued dark blue, so every ink in `ink_for_role` stays
/// legible on it and it can never be mistaken for a row's own `kMuted` ground.
inline constexpr PlanInk kSelectionBand{40, 64, 112};

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

/// ONE LAYER as one flat list of opaque quads, in painter's order.
///
/// Rectangles first in list order, then every label's cells over them — the same
/// order `canvas_body` rasterizes each layer for a terminal, so the two media agree
/// about what is on top without either knowing the other exists. The order BETWEEN
/// layers is `plan_canvas`'s, below, for the same reason: one owner per question.
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
inline std::vector<PlanRect> plan_layer_quads(const SurfaceLayer& layer, std::int64_t width,
                                             std::int64_t height,
                                             const SurfaceExtent& metric = SurfaceExtent{}) {
    std::vector<PlanRect> out;
    const std::int64_t w = canvas_extent(width);
    const std::int64_t h = canvas_extent(height);
    if (w == 0 || h == 0) {
        return out; // an empty canvas is a legitimate picture: it draws nothing
    }

    // THE QUAD DOOR CLIPS TO THE CANVAS'S OWN PIXELS (WUX-2). The cell paths below
    // never exceeded them, so nothing they emit changes; a fine coordinate can put a
    // quad astride the canvas edge, and this plan's standing invariant — no pixel
    // outside the canvas is ever emitted — has to hold there too, in one place.
    const std::int64_t w_px = w * kCanvasCellPx;
    const std::int64_t h_px = h * kCanvasCellPx;
    const auto quad = [&out, w_px, h_px](std::int64_t x, std::int64_t y, std::int64_t pw,
                                         std::int64_t ph, PlanInk ink) {
        const std::int64_t x0 = x > 0 ? x : 0;
        const std::int64_t y0 = y > 0 ? y : 0;
        const std::int64_t x1 = add_cells(x, pw) < w_px ? add_cells(x, pw) : w_px;
        const std::int64_t y1 = add_cells(y, ph) < h_px ? add_cells(y, ph) : h_px;
        if (x1 <= x0 || y1 <= y0) {
            return;
        }
        out.push_back(PlanRect{x0, y0, x1 - x0, y1 - y0, ink.r, ink.g, ink.b});
    };

    for (const SurfaceRect& r : layer.rects) {
        // THE ONE QUANTIZATION LAW AT THE PIXEL GRAIN (WUX-2): each fine EDGE through
        // `px_of_subs`, never an extent through a separate multiply — so the pixels a
        // rect occupies are exactly the pixels a fit resolves and a hit test compares.
        // For a rect with zero remainders these are byte-for-byte the cell-clipped
        // quads this loop always emitted, and the quad door above clips the rest.
        if (r.w < 0 || r.h < 0 || (r.w == 0 && sub_rem(r.sub_w) == 0) ||
            (r.h == 0 && sub_rem(r.sub_h) == 0)) {
            continue; // a negative extent is nothing, and so is a zero one with no remainder
        }
        const std::int64_t sx = subs_of_wire(r.x, r.sub_x);
        const std::int64_t sy = subs_of_wire(r.y, r.sub_y);
        const std::int64_t sw = add_cells(subs_of_cells(r.w), sub_rem(r.sub_w));
        const std::int64_t sh = add_cells(subs_of_cells(r.h), sub_rem(r.sub_h));
        const std::int64_t x_px = px_of_subs(sx);
        const std::int64_t y_px = px_of_subs(sy);
        quad(x_px, y_px, px_of_subs(add_cells(sx, sw)) - x_px,
             px_of_subs(add_cells(sy, sh)) - y_px, ink_for_role(r.role));
    }

    // THE BITMAP FACE ALSO DRAWS THE TEXT REGIONS, WHENEVER THERE IS NO REAL ONE.
    //
    // A medium publishing no text metric has said "text is a cell" (see
    // SurfaceExtent), and this is that sentence carried out: the regions go
    // through region.hpp's cell projection -- the same one the terminal Skins
    // use -- and come out as ordinary labels rasterized by the same glyph loop
    // as every other label. So a window whose font failed to open degrades to
    // exactly the pane this medium drew before HD-1, rather than to a blank
    // rectangle, and it does so through shared code rather than through a
    // second renderer that would have to be kept in step.
    //
    // With a real metric MOST regions are absent from this list: they are the SDL
    // edge's to draw in type (`plan_layer_regions`), and rasterizing them here as
    // well would paint the same words twice at two sizes.
    //
    // MOST, NOT ALL, SINCE HD-5, and the partition is now one predicate rather
    // than a test on the metric alone: a region belongs to whichever list
    // `fit_region` says its BOUNDS support. A face's line is not a cell -- this
    // repository's is 18 device pixels against a 12-pixel cell -- so a region one
    // cell tall (the Inspector's editable row) holds no row of type at all, and
    // before HD-5 it was in neither list and was therefore drawn by nobody. The
    // two lists still partition the work exactly; what changed is what they
    // partition on. See region.hpp's `fit_region`.
    const std::vector<ProjectedRow> projected = project_text_regions(layer, metric);

    // A GROUND IS THE CELL'S OWN QUAD, which is the whole of what this face has to
    // change to honour one (HD-2): a label cell is already cleared before its glyph
    // is drawn, so a row that asked to sit on something is that same clear in a
    // different ink. `role::kNone` -- every label, and every region row that did not
    // ask -- is the canvas background, which is exactly the quad that was there
    // before this parameter existed.
    //
    // ...UNLESS THE ROW'S REGION GAVE UP THE GROUND (TYPE-1), which is the one case
    // where a cell's quad is not drawn at all. The three-way is resolved here and
    // exactly once: the ROW's ground if it named one, otherwise the canvas's own if
    // the region took its rectangle, otherwise NOTHING -- the glyph lands on whatever
    // this layer already drew there, which is what "type on material" means in the
    // fidelity a bitmap face has. Both arguments are passed at every call site rather
    // than defaulted, because "does this text take its cells" is precisely the
    // question a publisher must have answered and a caller must not be able to skip.
    // ...AND A SELECTED CELL'S QUAD IS THE SELECTION BAND (TEXT-0), whatever the row's own
    // ground was going to be — the same precedence the terminal's reverse video has, in the
    // ink this face uses for it. A selected cell of a `kGroundBeneath` row is banded too:
    // giving up the rectangle is about the ROW's ground, and a selection is a fact about
    // exactly these cells that must not become invisible because the material under them is
    // somebody else's.
    const auto draw_label = [&](const SurfaceLabel& l, std::int64_t background,
                                std::int64_t region_ground, std::int64_t sel_begin,
                                std::int64_t sel_end) {
        // THE ANCHOR MAY BE FINE (WUX-2): the label's pixel origin is the one
        // quantization law at the pixel grain, and every byte advances a whole cell
        // from it. A label with zero remainders lands on exactly the cell pixels this
        // loop always produced, and the quad door clips a fine straddle at the edge.
        const std::int64_t label_y = px_of_subs(subs_of_wire(l.y, l.sub_y));
        if (add_cells(label_y, kCanvasCellPx) <= 0 || label_y >= h_px) {
            return; // no pixel row of this canvas belongs to it
        }
        const PlanInk ink = ink_for_role(l.role);
        const bool takes_the_cell = background >= 0 || region_ground != kGroundBeneath;
        const PlanInk under = background < 0 ? kCanvasBackground : ink_for_role(background);
        const std::int64_t label_x = px_of_subs(subs_of_wire(l.x, l.sub_x));
        for (std::size_t i = 0; i < l.text.size(); ++i) {
            const std::int64_t cell_x =
                add_cells(label_x, static_cast<std::int64_t>(i) * kCanvasCellPx);
            if (add_cells(cell_x, kCanvasCellPx) <= 0) {
                continue; // before the canvas starts; a later character may land
            }
            if (cell_x >= w_px) {
                break; // every remaining character is further right still
            }
            const std::int64_t cell_y = label_y;
            const bool in_selection =
                static_cast<std::int64_t>(i) >= sel_begin && static_cast<std::int64_t>(i) < sel_end;
            if (in_selection) {
                quad(cell_x, cell_y, kCanvasCellPx, kCanvasCellPx, kSelectionBand);
            } else if (takes_the_cell) {
                quad(cell_x, cell_y, kCanvasCellPx, kCanvasCellPx, under);
            }
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
    };

    for (const SurfaceLabel& l : layer.labels) {
        // A LABEL TAKES ITS CELL, always: `SurfaceLabel` has no ground of its own to
        // name and no region to have given one up, so it is the ordinary answer on
        // both counts — and no cell of a bare label is ever selected, because a
        // selection is a region's fact and a label is not a region.
        draw_label(l, role::kNone, kGroundOwn, 0, 0);
    }
    for (const ProjectedRow& p : projected) {
        // Last IN THIS LAYER: a region is the topmost thing its own presentation draws.
        // A later layer still covers it -- see `plan_canvas` below.
        draw_label(p.label, p.background, p.ground, p.sel_begin, p.sel_end);
    }
    return out;
}

// ---- Bounded regions, resolved for a medium that owns a real face --------------------
//
// EVERYTHING BELOW IS ABOUT REGIONS, NOT ABOUT TERMINALS. Nothing here knows what
// a transcript is, and that is the point of it living in the plan: the conversion
// from a cell rectangle to a pixel viewport, the clip against the surface, the
// local origin the interior is drawn in, and the row pitch are the four numbers
// ANY bounded interior needs. A background, a control, a chart or a second panel
// that wanted a finer inside would ask for exactly these and would not have to be
// extracted from a terminal renderer first.
//
// The rasterization itself is not here, and cannot be: a real font is a real
// dependency, and this header is pinned by lanes that build no SDL at all. What
// the SDL edge is left with is "set this viewport, draw these strings at these
// local coordinates in these inks" -- which is the same division skin_sdl.cpp
// already lives by, one shape further out.

/// One row of a resolved region: what it says, in which ink, and on what.
///
/// `background` IS ALWAYS A REAL INK HERE, never an absence — `role::kNone`
/// resolves to the region's own ground before it reaches this struct. That is the
/// difference between the wire and the plan: a publisher says "no ground of my
/// own", and a plan says "this exact colour", because by the time a medium is
/// drawing there is no such thing as a strip with no colour behind it. A row whose
/// ground equals its region's is a row the medium need not fill separately, which
/// is precisely how the renderer tells "selected" from "ordinary" without a second
/// field to keep in step.
struct PlanTextRow {
    std::string text;
    PlanInk ink{};
    PlanInk background = kCanvasBackground;

    friend bool operator==(const PlanTextRow&, const PlanTextRow&) = default;
};

/// A REGION READY TO DRAW: where to clip, where the first character starts inside
/// that clip, how far apart the rows are, and the rows.
///
/// `view` is the CLIPPED viewport in window pixels — what a medium may paint on.
/// `origin_x`/`origin_y` are LOCAL to it and may be negative, which is precisely
/// what carries a region that starts above or to the left of the surface: the
/// clip moved, the text did not.
/// A REGION TAKES ITS RECTANGLE, exactly as a label takes its cell.
///
/// `background` is what the whole clipped viewport is cleared to before a single
/// row is drawn, and it is the same argument `plan_canvas` already makes one
/// granularity down: a label cell there is cleared to the canvas background so
/// that text never has to compete with a fill it happens to be sitting on. A
/// region is that at region scale -- and it is not an aesthetic choice, it is
/// what makes a region an OVERLAY. Within one layer every rect is drawn and then every
/// label, so a label belonging to a panel underneath is drawn AFTER the backdrop
/// rect of a pane in the same plane; before HD-1 the pane's own full-width labels
/// cleared those cells one at a time and the question never arose. Measured live
/// the first time a region drew real type: the object list and the inspector
/// showed straight through the Terminal, with the Terminal's sentences on top of
/// them.
///
/// The cell projection carries the identical rule by a different mechanism --
/// `project_text_regions` pads every row to the region's full width, and a space
/// erases in a character medium -- so both media end at the same picture without
/// either knowing the other exists.
/// A REGION'S CARET, RESOLVED TO A RECTANGLE (HD-3).
///
/// Local to its region's clipped viewport, exactly as `origin_x`/`origin_y` are,
/// and for the same reason: the renderer draws inside the viewport and never
/// holds a window coordinate. `present` is false whenever the publisher named no
/// caret or named one outside the prose this region actually has — resolved HERE
/// rather than in the renderer, because "is there a caret to draw" is arithmetic
/// over the same fit the rows were resolved with, and the renderer's copy of that
/// arithmetic would be the second answer this package exists not to have.
struct PlanCaret {
    bool present = false;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    PlanInk ink{};

    friend bool operator==(const PlanCaret&, const PlanCaret&) = default;
};

/// ONE ROW'S WORTH OF SELECTION, RESOLVED TO A RECTANGLE (TEXT-0). Local to its region's
/// clipped viewport, exactly as `PlanCaret` is and for the same reason: the renderer draws
/// inside the viewport and never holds a window coordinate. The ink is not on it because
/// there is exactly one — `kSelectionBand` — and a field restating a constant per row would
/// be a second place for the one answer to live.
struct PlanSelectionBand {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const PlanSelectionBand&, const PlanSelectionBand&) = default;
};

struct PlanTextRegion {
    RegionViewport view{};
    std::int64_t origin_x = 0;
    std::int64_t origin_y = 0;
    std::int64_t line_px = 0;
    PlanInk background = kCanvasBackground;
    /// ...AND WHETHER THAT GROUND IS PAINTED AT ALL (TYPE-1). `kGroundOwn` is every
    /// region before TYPE-1 and is what makes a region an overlay; `kGroundBeneath`
    /// is the publisher saying the rectangle is not its to take, so the renderer
    /// fills nothing and the material this layer already drew shows through.
    ///
    /// It travels beside `background` rather than replacing it because a ROW may
    /// still name a ground of its own inside such a region, and the renderer's rule
    /// for that is unchanged and needs both: a row paints a strip when its ground
    /// differs from the one the region already put down. A region that put nothing
    /// down leaves `background` at the canvas ground, which no role resolves to, so
    /// a row that named nothing resolves equal to it and paints nothing -- and a row
    /// that named something differs from it and paints its strip.
    std::int64_t ground = kGroundOwn;
    std::vector<PlanTextRow> rows;
    PlanCaret caret{};
    /// THE SELECTED SPANS, one band per row the selection touches, DRAWN AFTER the row
    /// grounds and BEFORE the text — so the glyphs keep their ink and sit on the band,
    /// which is this face's reverse video (TEXT-0). Resolved here, from the same
    /// `selection_span_of_row` the cell projection spends and the same `RegionFit` the
    /// rows were cut with, because a renderer's own copy of span arithmetic would be the
    /// second answer this package exists not to have.
    std::vector<PlanSelectionBand> selection;

    friend bool operator==(const PlanTextRegion&, const PlanTextRegion&) = default;
};

/// WHERE A REGION'S CARET IS, in pixels local to a resolved region — the whole of
/// HD-3's caret arithmetic, pure and total.
///
/// `advance_px`/`line_px` come off the `RegionFit` rather than off the metric,
/// which is what makes "the caret is positioned by the same number that drew the
/// text" a fact rather than a convention: `fit_region` is the one function that
/// turns a metric into a capacity, and this reads what it decided.
///
/// A CARET AT COLUMN `columns` IS LEGITIMATE and is not off the end. It is where
/// the caret sits after the last character a row can show, and the inset is
/// exactly what makes it fit: the interior is `columns * advance_px` wide inside
/// a `2 * kTextInsetPx` margin, so a `kCaretWidthPx`-wide bar at the far column
/// lands inside the region's own viewport as long as the two constants match.
inline constexpr PlanCaret plan_caret(const RegionFit& fit, std::int64_t local_origin_x,
                                      std::int64_t local_origin_y, std::int64_t caret_row,
                                      std::int64_t caret_col, PlanInk ink) noexcept {
    PlanCaret c;
    if (caret_row < 0 || caret_row >= fit.rows || caret_col < 0 || caret_col > fit.columns) {
        return c; // no caret, or one this region's prose does not have room for
    }
    c.present = true;
    c.x = add_cells(local_origin_x, mul_px(caret_col, fit.advance_px));
    c.y = add_cells(local_origin_y, mul_px(caret_row, fit.line_px));
    c.w = kCaretWidthPx;
    c.h = fit.line_px;
    c.ink = ink;
    return c;
}

/// EVERY TEXT REGION OF ONE LAYER, RESOLVED AGAINST A MEDIUM'S TEXT METRIC.
///
/// Empty when the metric is zero — there is no such thing as a graphical region
/// on a medium that sets text in cells, and `plan_layer_quads` has already drawn them
/// as labels in that case. One list or the other, never both.
///
/// TRUNCATED TO WHAT FITS, twice over, and neither is redundant. `fit_region`
/// says how many rows and columns the publisher was working with, so a publisher
/// that sent more rows than it claimed cannot make this medium draw them; and each
/// row is cut to `columns` so a published row of a million characters costs a
/// million characters of nothing rather than a million characters of font
/// engine. The medium's clip is a third guarantee and still not a substitute for
/// either — it stops ink leaving the region, which is not the same as bounding
/// the work.
inline std::vector<PlanTextRegion> plan_layer_regions(const SurfaceLayer& layer,
                                                     const SurfaceExtent& metric,
                                                     const PlanSize& surface) {
    std::vector<PlanTextRegion> out;
    if (metric.text_advance_px <= 0 || metric.text_line_px <= 0) {
        return out; // text is a cell here: plan_layer_quads drew them
    }
    for (const SurfaceTextRegion& r : layer.texts) {
        const RegionFit fit = fit_region(r, metric);
        const RegionViewport clipped = clip_viewport(fit.view, surface.w, surface.h);
        if (clipped.empty() || fit.columns <= 0 || fit.rows <= 0 || !fit.graphical()) {
            // Nothing of it is on the surface, or nothing of it fits, or this medium cannot
            // set THIS region in type at all -- the last is HD-5's, and it is the other half
            // of `plan_layer_quads`'s partition rather than a second policy: a region whose bounds
            // hold no row of the face is drawn there, as cells, by the same glyph loop every
            // other label goes through. Asking `graphical()` rather than the metric is what
            // makes the two lists disjoint and complete for every region on the canvas.
            continue;
        }
        PlanTextRegion p;
        p.view = clipped;
        p.origin_x = sub_px(add_cells(fit.view.x, fit.origin_x), clipped.x);
        p.origin_y = sub_px(add_cells(fit.view.y, fit.origin_y), clipped.y);
        p.line_px = fit.line_px;
        // WHOSE RECTANGLE THIS IS, carried through unresolved (TYPE-1). It is the one
        // thing on a region that is not arithmetic -- the publisher already answered
        // it and this plan neither second-guesses it nor turns it into a colour.
        p.ground = r.ground == kGroundBeneath ? kGroundBeneath : kGroundOwn;
        // THE REGION'S OWN GROUND, read once and named, because it is what a row
        // that asked for none resolves to. Reading `p.background` inside the loop
        // would work today and would silently follow any later line that set it
        // AFTER the rows were built -- an ordering dependency with nothing holding
        // it in place.
        const PlanInk region_ground = p.background;
        const std::size_t take = r.rows.size() < static_cast<std::size_t>(fit.rows)
                                     ? r.rows.size()
                                     : static_cast<std::size_t>(fit.rows);
        const std::size_t width = static_cast<std::size_t>(fit.columns);
        p.rows.reserve(take);
        for (std::size_t i = 0; i < take; ++i) {
            std::string text = r.rows[i].text;
            if (text.size() > width) {
                text.resize(width);
            }
            // THE SELECTED SPAN OF THIS ROW, over the CUT text — the highlight meets the
            // same right-hand cut every byte of the row met, so a selection running past
            // the columns this region holds is covered exactly as far as its text is
            // drawn. The rule itself is region.hpp's one function; what is resolved here
            // is only geometry: columns become pixels through the SAME fit that placed
            // the row, which is what makes the band and the glyphs one picture.
            const RowSpan span = selection_span_of_row(r, static_cast<std::int64_t>(i),
                                                       static_cast<std::int64_t>(text.size()));
            if (span.present()) {
                p.selection.push_back(PlanSelectionBand{
                    add_cells(p.origin_x, mul_px(span.begin, fit.advance_px)),
                    add_cells(p.origin_y, mul_px(static_cast<std::int64_t>(i), fit.line_px)),
                    mul_px(span.end - span.begin, fit.advance_px), fit.line_px});
            }
            const std::int64_t ground = r.rows[i].background;
            p.rows.push_back(PlanTextRow{std::move(text), ink_for_role(r.rows[i].role),
                                         ground < 0 ? region_ground : ink_for_role(ground)});
        }
        // THE CARET TAKES THE INK OF THE ROW IT IS ON, and that is not decoration: a caret
        // marks where the text a maker is typing continues, so it belongs to that text. A
        // caret beyond the rows this region was actually given falls back to ordinary fill
        // rather than to nothing, because a row that was not said is still a row of the
        // region and a caret on it is still a real position.
        const PlanInk caret_ink =
            (r.caret_row >= 0 && static_cast<std::size_t>(r.caret_row) < p.rows.size())
                ? p.rows[static_cast<std::size_t>(r.caret_row)].ink
                : ink_for_role(role::kFill);
        p.caret = plan_caret(fit, p.origin_x, p.origin_y, r.caret_row, r.caret_col, caret_ink);
        out.push_back(std::move(p));
    }
    return out;
}

/// ONE PLANE, READY TO EXECUTE: its opaque quads, then the regions it sets in real type.
///
/// The two lists PARTITION that layer's work and are ordered against each other, which is
/// the fact the SDL edge needs and the fact it must not be left to reconstruct.
struct PlanLayer {
    std::vector<PlanRect> quads;
    std::vector<PlanTextRegion> regions;

    friend bool operator==(const PlanLayer&, const PlanLayer&) = default;
};

/// THE WHOLE CANVAS AS AN ORDERED EXECUTION LIST — one `PlanLayer` per published layer,
/// in the publisher's own order (WIND-2a).
///
/// THIS IS THE ONLY DOOR THE SDL EDGE USES, and that is the phase's structural claim about
/// this medium. Before WIND-2a the edge drew every layer's quads and then every layer's
/// real-face regions, because the plan handed it two canvas-wide lists and the ordering
/// between them was a convention nobody could see. Two global bands is exactly the defect
/// the terminal medium had, arriving under a different type -- so the plan now carries the
/// interleaving itself and the edge has no second list to get the order wrong with.
///
/// A LAYER'S TWO LISTS ARE STILL DISJOINT AND STILL COMPLETE, per HD-5: a region belongs
/// to `quads` (as bitmap cells) or to `regions` (in real type) according to whether its own
/// bounds hold a row of the face, never to both and never to neither.
inline std::vector<PlanLayer> plan_canvas(const SurfaceCanvas& c, const SurfaceExtent& metric,
                                          const PlanSize& surface) {
    std::vector<PlanLayer> out;
    out.reserve(c.layers.size());
    for (const SurfaceLayer& layer : c.layers) {
        out.push_back(PlanLayer{plan_layer_quads(layer, c.width, c.height, metric),
                                plan_layer_regions(layer, metric, surface)});
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
