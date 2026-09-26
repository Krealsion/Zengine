// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_SKIN_SDL_PLAN_HPP
#define ZENGINE_SURFACE_SKIN_SDL_PLAN_HPP

// The SDL Skin's plan, SDL-free: a canvas or a SnakeVisual becomes window geometry, opaque
// quads and real-face text regions, as pure arithmetic every lane pins, SDL built or not. The
// SDL edge (skin_sdl.cpp) executes the plan, so the order between planes and the split between
// quads and type are decided here, and plumbing is all the edge can get wrong.
// Surface law: agents/surface.md

#include "cells.hpp"
#include "region.hpp"
#include "skin_sdl_glyphs.hpp"
#include "snake/vocabulary.hpp"
#include "vocabulary.hpp"

#include <cstdint>
#include <limits>
#include <optional>
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

/// The frame as rectangles in painter's order: the background (dark red once the run has died,
/// so the whole window is the death banner), the food inset deeper than the snake, the body,
/// then the head. Food off the board (kNoFood) plans nothing.
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

/// This medium's ink per canvas role; an unknown role paints as `kFill`. In the plan rather than
/// at the SDL edge, so every lane can pin which ink a role gets.
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
    case role::kGround: return PlanInk{0, 0, 0};
    default: return PlanInk{176, 176, 188};
    }
}

/// What a cell shows where nothing was published, and what a label's cell is cleared to before
/// its glyph is drawn (`plan_layer_quads`).
inline constexpr PlanInk kCanvasBackground{18, 18, 24};

/// The band under selected text, this medium's answer to the terminal's reverse video: the
/// glyphs keep their ink. A medium constant, not a role (a publisher says which text is
/// selected, not how it looks); a dark blue every `ink_for_role` ink stays legible on, never
/// mistaken for a `kMuted` ground.
inline constexpr PlanInk kSelectionBand{40, 64, 112};

/// One glyph pixel, in device pixels. Chosen so a glyph fills a canvas cell
/// exactly: no resampling, no partial cells, and a label's Nth character sits in
/// the canvas cell the publisher named and nowhere else.
inline constexpr std::int64_t kGlyphScale = kCanvasCellPx / kGlyphCols;
static_assert(kCanvasCellPx % kGlyphCols == 0,
              "a canvas cell must be a whole number of glyph pixels wide");
static_assert(kCanvasCellPx % kGlyphRows == 0,
              "a canvas cell must be a whole number of glyph pixels tall");

/// The largest canvas extent this medium considers, in cells: not a policy, but what a pixel
/// number can hold, so every `cell * kCanvasCellPx` in this file is safe by construction.
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

/// How many whole canvas cells a drawable of this many pixels has room for. Floored: a partial
/// cell is not room, and the remainder is background (`canvas` clears the whole drawable). No
/// room answers zero, which `SkinT::report_extent` turns into silence.
inline constexpr SurfaceExtent extent_of_drawable(const PlanSize& px) noexcept {
    return SurfaceExtent{px.w > 0 ? px.w / kCanvasCellPx : 0,
                         px.h > 0 ? px.h / kCanvasCellPx : 0};
}

/// One layer as one flat list of opaque quads in painter's order: rects in list order, then
/// each label's cells, as `canvas_body` rasterizes a layer for a terminal. A label takes its
/// cell -- cleared to the canvas background before its glyph, as a terminal's `put` overwrites
/// one -- and clips per cell against the canvas extent. A glyph row is at most three quads;
/// nothing is allocated per character and nothing is cached between frames.
inline std::vector<PlanRect> plan_layer_quads(const SurfaceLayer& layer, std::int64_t width,
                                             std::int64_t height,
                                             const SurfaceExtent& metric = SurfaceExtent{}) {
    std::vector<PlanRect> out;
    const std::int64_t w = canvas_extent(width);
    const std::int64_t h = canvas_extent(height);
    if (w == 0 || h == 0) {
        return out; // an empty canvas is a legitimate picture: it draws nothing
    }

    // Every quad clips to the canvas's own pixels, here and nowhere else: a fine coordinate can
    // put a quad astride the canvas edge, and no pixel outside the canvas is ever emitted.
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
        // Each fine edge goes through `px_of_subs`, never an extent through a multiply of its
        // own, so a rect occupies exactly the pixels a fit resolves and a hit test compares.
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

    // With no real face this face also draws the text regions, through region.hpp's cell
    // projection (the terminal Skins' own) as ordinary labels. With a real face a region is drawn
    // here only when `fit_region` says its bounds hold no row of the face; every other region is
    // `plan_layer_regions`'s, and the two lists partition the work exactly.
    const std::vector<ProjectedRow> projected = project_text_regions(layer, metric);

    // A cell's quad resolves here, once: a selected cell is the selection band; otherwise the
    // row's own ground if it named one, else the canvas background if the region took its
    // rectangle, else nothing, and the glyph lands on what this layer drew beneath. Both grounds
    // are passed at every call site, never defaulted: whether text takes its cells is the
    // publisher's to have answered.
    const auto draw_label = [&](const SurfaceLabel& l, std::int64_t background,
                                std::int64_t region_ground, std::int64_t sel_begin,
                                std::int64_t sel_end) {
        // The anchor may be fine: its pixel origin is the quantization law at the pixel grain,
        // and every byte advances a whole cell from it.
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
        // A bare label always takes its cell and is never selected: selection is a region's.
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
// What any bounded interior needs: its cell rectangle as a pixel viewport clipped to the
// surface, the local origin its interior is drawn from, and the row pitch. Rasterizing is the
// SDL edge's, since a real font is a dependency the lanes pinning this header do not build.

/// One row of a resolved region. `background` is always a real ink here: `role::kNone` has
/// resolved to the region's own ground, and a row whose ground equals its region's is one the
/// medium need not fill separately.
struct PlanTextRow {
    std::string text;
    PlanInk ink{};
    PlanInk background = kCanvasBackground;

    friend bool operator==(const PlanTextRow&, const PlanTextRow&) = default;
};

/// A region's caret as a rectangle, local to its region's clipped viewport. `present` is false
/// when the publisher named no caret or one outside the prose the region has -- resolved here,
/// from the rows' own fit, so the renderer keeps no second copy of that arithmetic.
struct PlanCaret {
    bool present = false;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    PlanInk ink{};

    friend bool operator==(const PlanCaret&, const PlanCaret&) = default;
};

/// One row's selected span as a rectangle, local to its region's clipped viewport. It carries
/// no ink: there is one, `kSelectionBand`.
struct PlanSelectionBand {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const PlanSelectionBand&, const PlanSelectionBand&) = default;
};

/// A region ready to draw. `view` is the clipped viewport in window pixels; `origin_x`/`origin_y`
/// are local to it and may be negative (a region starting off the surface: the clip moves, the
/// text does not); `line_px` is the row pitch. A region is an overlay: `background` clears the
/// whole viewport before any row, so a label of a panel beneath in the same plane cannot show
/// through -- as the cell projection pads each row to the region's width.
struct PlanTextRegion {
    RegionViewport view{};
    std::int64_t origin_x = 0;
    std::int64_t origin_y = 0;
    std::int64_t line_px = 0;
    PlanInk background = kCanvasBackground;
    /// Whether that ground is painted: `kGroundBeneath` fills nothing, and what this layer drew
    /// shows through. A row naming its own ground still paints a strip where it differs from
    /// `background`, which then stays the canvas ground no role resolves to.
    std::int64_t ground = kGroundOwn;
    std::vector<PlanTextRow> rows;
    PlanCaret caret{};
    /// One band per row the selection touches, drawn after the row grounds and before the
    /// text, from `selection_span_of_row` and the rows' own fit.
    std::vector<PlanSelectionBand> selection;

    friend bool operator==(const PlanTextRegion&, const PlanTextRegion&) = default;
};

/// Where a region's caret is, in pixels local to the resolved region. `advance_px`/`line_px`
/// come off the `RegionFit`, so the caret is placed by the numbers that drew the text. A caret
/// at column `columns`, after the last character a row shows, is legitimate: the inset leaves
/// room for its `kCaretWidthPx` bar inside the viewport.
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

/// Every text region of one layer, resolved against a medium's text metric; empty when text is
/// a cell (`plan_layer_quads` drew them). Cut to what fits twice over -- the rows to the fit,
/// each row to its columns -- so a publisher's excess costs no font work; the medium's clip
/// stops ink leaving the region, which does not bound the work.
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
            // Off the surface, nothing fits, or its bounds hold no row of the face and
            // `plan_layer_quads` drew it as cells; `graphical()` keeps the two lists disjoint.
            continue;
        }
        PlanTextRegion p;
        p.view = clipped;
        p.origin_x = sub_px(add_cells(fit.view.x, fit.origin_x), clipped.x);
        p.origin_y = sub_px(add_cells(fit.view.y, fit.origin_y), clipped.y);
        p.line_px = fit.line_px;
        // Whose rectangle this is, carried through as the publisher answered it.
        p.ground = r.ground == kGroundBeneath ? kGroundBeneath : kGroundOwn;
        // The region's own ground, read once: what a row that names none resolves to.
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
            // The row's selected span over the cut text, placed through the fit that placed
            // the row, so the band and the glyphs are one picture.
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
        // The caret takes the ink of the row it is on; on a row the region was not given, the
        // ordinary fill, since that is still a real position.
        const PlanInk caret_ink =
            (r.caret_row >= 0 && static_cast<std::size_t>(r.caret_row) < p.rows.size())
                ? p.rows[static_cast<std::size_t>(r.caret_row)].ink
                : ink_for_role(role::kFill);
        p.caret = plan_caret(fit, p.origin_x, p.origin_y, r.caret_row, r.caret_col, caret_ink);
        out.push_back(std::move(p));
    }
    return out;
}

/// One plane, ready to execute: its opaque quads, then the regions it sets in real type.
struct PlanLayer {
    std::vector<PlanRect> quads;
    std::vector<PlanTextRegion> regions;

    friend bool operator==(const PlanLayer&, const PlanLayer&) = default;
};

/// The whole canvas as an ordered execution list, one `PlanLayer` per published layer in the
/// publisher's order. It is the SDL edge's only door, so the order between planes is decided
/// here, in code every lane pins, never by the edge's own loops. In each layer a region is in
/// `quads` (as bitmap cells) or `regions` (in type), never both and never neither.
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

// ---- The attention chip: the `score` slot, composed into the picture ---------------------
//
// The slot also lands in the window title (`title_of`), which a maker reading the picture does
// not read; so it is composed into the frame as a compact box in the top-right corner, over
// every plane, through the same region machinery. One voice, `role::kAlert`: a slot carries no
// role, and severity lives in the canvas and in the words.

/// The chip as a canvas layer, in canvas cells: an ordinary `SurfaceTextRegion`, so its fit,
/// cut, ink, ground and clip are this medium's usual machinery. Two cells hold one row of an
/// 18-pixel face (24 pixels less the inset on each side is 20); a taller face takes more. The
/// width is what the text asks for, clamped to the canvas, where the region cuts honestly.
inline SurfaceLayer attention_chip_layer(const std::string& text, std::int64_t canvas_w,
                                         const SurfaceExtent& metric) {
    SurfaceLayer layer;
    if (text.empty() || canvas_w <= 0) {
        return layer; // NOTHING TO SAY, AND NOTHING DRAWN -- empty is the retraction
    }
    const std::int64_t advance =
        metric.text_advance_px > 0 ? metric.text_advance_px : kCanvasCellPx;
    const std::int64_t line = metric.text_line_px > 0 ? metric.text_line_px : kCanvasCellPx;
    const std::int64_t want_w =
        add_cells(mul_px(static_cast<std::int64_t>(text.size()), advance), 2 * kTextInsetPx);
    const std::int64_t cells_w = want_w / kCanvasCellPx + (want_w % kCanvasCellPx != 0 ? 1 : 0);
    const std::int64_t want_h = add_cells(line, 2 * kTextInsetPx);
    const std::int64_t cells_h = want_h / kCanvasCellPx + (want_h % kCanvasCellPx != 0 ? 1 : 0);
    SurfaceTextRegion region;
    region.w = cells_w < canvas_w ? cells_w : canvas_w;
    region.h = cells_h;
    region.x = canvas_w - region.w;
    region.y = 0;
    // The row names its own ground: the region clears to the canvas ground and the row paints
    // its strip over it, so the chip reads as a box with a hairline around it, not a slab.
    region.rows.push_back(SurfaceTextRow{text, role::kFill, role::kAlert});
    layer.texts.push_back(std::move(region));
    return layer;
}

/// The chip ready to execute, through the two calls `plan_canvas` makes for every plane: one
/// door, so what a suite asserts is what the edge draws.
inline PlanLayer plan_attention_chip(const std::string& text, const SurfaceCanvas& c,
                                     const SurfaceExtent& metric, const PlanSize& surface) {
    const SurfaceLayer chip = attention_chip_layer(text, c.width, metric);
    return PlanLayer{plan_layer_quads(chip, c.width, c.height, metric),
                     plan_layer_regions(chip, metric, surface)};
}

// ---- Restoring a remembered desktop placement -----------------------------------------
//
// `SurfacePlacementRemembered`'s judgment as pure arithmetic every lane pins: the SDL edge
// supplies the displays and the window's size, and applies the answer.

/// One display's USABLE area — its bounds less the platform's own reservations (taskbar,
/// dock, menu bar) — in the same desktop units window positions are spoken in.
struct DesktopSpan {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
};

/// A position in those units. Distinct from `PlanSize` because a desktop coordinate is
/// signed territory — a monitor left of the primary lives at negative x, legitimately.
struct DesktopPoint {
    std::int64_t x = 0;
    std::int64_t y = 0;
};

/// How much of a window must show for a remembered position to be reachable: this much of its
/// width, within its top strip (its first this-many rows), on one display's usable area. The
/// top strip is what a hand drags a window by, so its visibility decides.
inline constexpr std::int64_t kPlacementGraspPx = 32;

/// A reachable position restores verbatim, deliberate overhangs included; a stranded one is
/// clamped into the usable area of the nearest display (most overlap, else nearest centre),
/// top-left first, so the grab strip comes back and an oversized window overflows right and
/// down; with no display truth there is no answer, since an uninformed move is a blind replay.
/// Reachable: the top strip meets one display vertically at all, with `kPlacementGraspPx` of
/// width showing (all of a narrower window). It moves a window and never sizes one.
inline std::optional<DesktopPoint> placement_within(std::int64_t x, std::int64_t y,
                                                    std::int64_t w, std::int64_t h,
                                                    const std::vector<DesktopSpan>& usable) {
    if (usable.empty()) {
        return std::nullopt;
    }
    const std::int64_t need_w = w < kPlacementGraspPx ? w : kPlacementGraspPx;
    const std::int64_t strip_h = h < kPlacementGraspPx ? h : kPlacementGraspPx;
    // Reachable on some single display, verbatim.
    for (const DesktopSpan& s : usable) {
        const std::int64_t vis_w =
            (x + w < s.x + s.w ? x + w : s.x + s.w) - (x > s.x ? x : s.x);
        const std::int64_t vis_h =
            (y + strip_h < s.y + s.h ? y + strip_h : s.y + s.h) - (y > s.y ? y : s.y);
        if (vis_w >= need_w && vis_h >= 1) {
            return DesktopPoint{x, y};
        }
    }
    // Stranded: pick the nearest display — most overlap with the remembered rectangle,
    // else smallest center-to-center distance — and clamp the top-left into its usable
    // area.
    const DesktopSpan* home = &usable.front();
    std::int64_t best_overlap = -1;
    std::int64_t best_distance = std::numeric_limits<std::int64_t>::max();
    for (const DesktopSpan& s : usable) {
        const std::int64_t ox = (x + w < s.x + s.w ? x + w : s.x + s.w) - (x > s.x ? x : s.x);
        const std::int64_t oy = (y + h < s.y + s.h ? y + h : s.y + s.h) - (y > s.y ? y : s.y);
        const std::int64_t overlap = (ox > 0 && oy > 0) ? ox * oy : 0;
        // Centers, times two, so the arithmetic stays integral.
        const std::int64_t dx = (2 * x + w) - (2 * s.x + s.w);
        const std::int64_t dy = (2 * y + h) - (2 * s.y + s.h);
        const std::int64_t distance = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (overlap > best_overlap ||
            (overlap == best_overlap && distance < best_distance)) {
            best_overlap = overlap;
            best_distance = distance;
            home = &s;
        }
    }
    const auto clamp_into = [](std::int64_t v, std::int64_t lo, std::int64_t hi) {
        // hi may sit below lo when the window outsizes the span; the top-left edge wins.
        if (hi < lo) {
            return lo;
        }
        return v < lo ? lo : (v > hi ? hi : v);
    };
    return DesktopPoint{clamp_into(x, home->x, home->x + home->w - w),
                        clamp_into(y, home->y, home->y + home->h - h)};
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
