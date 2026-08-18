// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_REGION_HPP
#define ZENGINE_SURFACE_REGION_HPP

// A BOUNDED REGION OF A CANVAS, RESOLVED — the arithmetic that lets one part of a
// picture be finer than the cell grid the rest of it is drawn on.
//
// WHAT THIS HEADER IS FOR. `SurfaceTextRegion` is placed in cells and filled with
// rows a medium sets in its own type. Two parties have to agree, exactly, about
// what that means in numbers: the PUBLISHER, which decides how much prose fits
// and therefore what it is not showing, and the MEDIUM, which decides where the
// pixels go. If they resolve it separately they will one day resolve it
// differently — and the first symptom is not a misdrawn pane, it is an omission
// marker that says "... 12 earlier" when the truth is fourteen. So the
// resolution is ONE function, here, pure, and both sides call it.
//
// IT IS NOT A TEXT REGION'S HEADER. Nothing below mentions the Terminal, a
// transcript, prose, or any consumer. What it knows is: a rectangle in cells, a
// medium's text metric, and how to turn the pair into a pixel viewport, a local
// origin, and a capacity. A future background, control, or graphical primitive
// that wants a bounded interior finer than a cell wants exactly these numbers,
// and should get them from here rather than from a copy that agrees today.
//
// ZERO METRIC IS THE CELL PROJECTION, everywhere, and it is a real answer rather
// than a fallback: a terminal's text IS a cell, and so is the SDL medium's while
// its bitmap face is what is drawing. `fit_region` under a zero metric returns
// exactly the region's own cell bounds, which is how a graphical medium that has
// no font and a character medium that has no pixel end up describing the same
// picture without either being asked to pretend.
//
// PIXELS APPEAR HERE, AND ONLY AS A DERIVED UNIT. `kCanvasCellPx` is consulted,
// never copied, and every product is saturated before it can leave the number
// line — a region's bounds arrive on the bus inside a `ZEN_SHAPE`, so `w` is a
// number a publisher chose and `w * kCanvasCellPx` is undefined behaviour
// produced by data. Same domain, same discipline, same reason as cells.hpp.

#include "cells.hpp"
#include "vocabulary.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace zengine::surface {

/// THE BREATHING ROOM INSIDE A TEXT REGION, in device pixels, on every side.
///
/// It is here and not in a renderer, which is the whole point of its existing at
/// all. Padding consumed privately by whoever draws is the exact defect this
/// phase is arranged against: the publisher would wrap against the region's full
/// width, the renderer would draw into a narrower one, and the difference would
/// appear as a character falling off the right-hand edge of every long row. So
/// the inset is subtracted in `fit_region`, which is what tells the publisher how
/// many columns there are, and added in the same function's origin, which is what
/// tells the medium where to start drawing. One number, two consumers, no way to
/// apply it once.
///
/// Two pixels rather than a cell, because a cell is twelve and a text region's
/// rows are already shorter than the cell they nominally sit on; and because a
/// coarse inset would have to come out of the row budget, where it would be a
/// visible row of the record rather than a millimetre of air.
inline constexpr std::int64_t kTextInsetPx = 2;

/// HOW WIDE A CARET BAR IS, in device pixels, in a medium that sets real type.
///
/// It is here beside the inset rather than in a renderer for the identical
/// reason: the width is part of the arithmetic that says where a caret is, so a
/// hit test, a plan and a renderer must not each have their own. Two pixels, the
/// same as the inset, because a caret has to be visible against a stem of the
/// face beside it and must not be mistaken for one — at this repository's
/// measured 8-pixel advance a bar of one pixel is a stem and a bar of four is a
/// block cursor, which is a different thing that says "overwrite".
///
/// A BAR, NEVER A BLOCK, and that is a semantic statement rather than taste: the
/// caret this vocabulary carries is an INSERTION POINT — it sits BETWEEN two
/// characters — so a mark that covers one of them would be claiming the wrong
/// thing about what the next keystroke does.
inline constexpr std::int64_t kCaretWidthPx = 2;

/// The largest cell coordinate that survives being multiplied into pixels. Not a
/// policy about how big a canvas may be — exactly how many cells a pixel number
/// can hold.
inline constexpr std::int64_t kMaxCellsInPixels =
    (std::numeric_limits<std::int64_t>::max)() / kCanvasCellPx;

/// A cell coordinate in device pixels, saturating at both ends.
inline constexpr std::int64_t px_of_cells(std::int64_t cells) noexcept {
    if (cells >= kMaxCellsInPixels) {
        return kMaxCellsInPixels * kCanvasCellPx;
    }
    if (cells <= -kMaxCellsInPixels) {
        return -kMaxCellsInPixels * kCanvasCellPx;
    }
    return cells * kCanvasCellPx;
}

/// `a - b` without leaving the number line — cells.hpp's `add_cells`, the other
/// way round. It is its own function rather than `add_cells(a, -b)` because
/// negating `INT64_MIN` is itself the undefined behaviour being avoided.
inline constexpr std::int64_t sub_px(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (b < 0) {
        return a > kMax + b ? kMax : a - b;
    }
    if (b > 0) {
        return a < kMin + b ? kMin : a - b;
    }
    return a;
}

/// `a * b` for non-negative operands, saturating at the top of the number line.
///
/// The same discipline `px_of_cells` applies to a constant factor, for a factor
/// that arrives on the bus: a medium publishes `text_line_px`, so "which cell row
/// does prose row N begin on" is a multiply by a number a publisher chose. A
/// non-positive operand answers zero, because neither a count of rows nor a row
/// pitch has a meaning below one and saturating downwards would invent a
/// coordinate.
inline constexpr std::int64_t mul_px(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    if (a <= 0 || b <= 0) {
        return 0;
    }
    return a > kMax / b ? kMax : a * b;
}

/// `v / d` FLOORED, for a strictly positive divisor — the same rule
/// `cell_of_pixel` states for the cell grid, written once for any metric.
///
/// C++ integer division truncates toward zero, so a plain `/` sends pixel -1 to
/// column 0: a pointer one pixel to the LEFT of a region would report as being in
/// its first column. Flooring is what makes the boundaries evenly spaced across
/// zero, which is what an eye already assumes.
inline constexpr std::int64_t floor_div_px(std::int64_t v, std::int64_t d) noexcept {
    if (d <= 0) {
        return 0;
    }
    const std::int64_t q = v / d;
    return (v % d < 0) ? q - 1 : q;
}

/// A REGION'S OUTER RECTANGLE IN A GRAPHICAL MEDIUM, in device pixels.
///
/// The whole rectangle the region was granted — not what is currently visible,
/// which is `clip_viewport`'s answer. A medium sets its clip from this and then
/// draws in coordinates local to `x`/`y`, which is the part that stays true when
/// a second kind of thing wants a bounded interior.
struct RegionViewport {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    constexpr bool empty() const noexcept { return w <= 0 || h <= 0; }

    friend bool operator==(const RegionViewport&, const RegionViewport&) = default;
};

/// A REGION, RESOLVED: where its pixels are, where its first character starts,
/// and how much prose fits.
///
/// `columns`/`rows` are the capacity BOTH sides must use — the publisher to
/// decide what it can show and what it must say it is omitting, the medium to
/// know it will never be handed more than it can draw. `origin_x`/`origin_y` are
/// LOCAL to `view`, so a medium adds them to its viewport's corner and never
/// needs a global coordinate to draw one row.
///
/// `advance_px`/`line_px` are carried back out deliberately: they are the metric
/// this fit was resolved WITH, so a medium that positions row `i` at
/// `origin_y + i * line_px` is using the same number the capacity was computed
/// from rather than re-reading a field that may have moved on.
struct RegionFit {
    RegionViewport view{};
    std::int64_t origin_x = 0;
    std::int64_t origin_y = 0;
    std::int64_t columns = 0;
    std::int64_t rows = 0;
    std::int64_t advance_px = 0; ///< 0 = this fit is the cell projection
    std::int64_t line_px = 0;

    /// Is the interior expressed in the medium's own type, rather than in cells?
    constexpr bool graphical() const noexcept { return advance_px > 0 && line_px > 0; }

    friend bool operator==(const RegionFit&, const RegionFit&) = default;
};

/// A CELL RECTANGLE AS A PIXEL VIEWPORT. One multiply per number, saturated.
inline constexpr RegionViewport viewport_of_cells(std::int64_t x, std::int64_t y, std::int64_t w,
                                                  std::int64_t h) noexcept {
    return RegionViewport{px_of_cells(x), px_of_cells(y), px_of_cells(w > 0 ? w : 0),
                          px_of_cells(h > 0 ? h : 0)};
}

/// THE ONE RESOLUTION. A region's cell bounds plus a medium's text metric become
/// a viewport, a local origin, and a capacity — and every party that needs any of
/// those three asks this function for all of them.
///
/// TOTAL over every std::int64_t on every argument, because all five arrive from
/// the bus: four on a `SurfaceTextRegion` and the metric on a `SurfaceExtent`. A
/// non-positive advance or line height is not an error, it is the sentence "text
/// is a cell" (see `SurfaceExtent`), and it resolves to the region's own cell
/// bounds with no inset — which is exactly what a character medium draws and
/// exactly what a graphical medium with no font draws.
///
/// A REGION TOO SMALL FOR THE MEDIUM'S OWN TYPE IS A CELL REGION IN THAT MEDIUM
/// (HD-5), and that is the same sentence one step further rather than a new rule.
/// A face's line is not a cell: this repository's measures 18 device pixels
/// against a 12-pixel cell, so a region ONE CELL TALL holds `(12 - 2*inset) / 18`
/// = zero rows of it. Before HD-5 such a region resolved to a graphical fit with
/// no capacity, and both media then drew NOTHING — `plan_layer_regions` skips a
/// fit with no rows and `plan_canvas` had already decided the regions were the
/// other list's. A bounded region that silently vanishes is the one answer this
/// header exists to make impossible, so the fallback is here, in the ONE function
/// both sides call: a medium that cannot set a region in type describes it in
/// cells, exactly as a medium with no face does, and the publisher asking for the
/// capacity is told the same thing. The Inspector's editable row is one cell
/// tall and reaches this; the Terminal pane and its completion list are not and
/// do not.
inline constexpr RegionFit fit_region(std::int64_t x, std::int64_t y, std::int64_t w,
                                      std::int64_t h, std::int64_t text_advance_px,
                                      std::int64_t text_line_px) noexcept {
    RegionFit f;
    f.view = viewport_of_cells(x, y, w, h);
    if (text_advance_px > 0 && text_line_px > 0) {
        const std::int64_t inner_w = f.view.w - 2 * kTextInsetPx;
        const std::int64_t inner_h = f.view.h - 2 * kTextInsetPx;
        const std::int64_t columns = inner_w > 0 ? inner_w / text_advance_px : 0;
        const std::int64_t rows = inner_h > 0 ? inner_h / text_line_px : 0;
        if (columns > 0 && rows > 0) {
            f.advance_px = text_advance_px;
            f.line_px = text_line_px;
            f.origin_x = kTextInsetPx;
            f.origin_y = kTextInsetPx;
            f.columns = columns;
            f.rows = rows;
            return f;
        }
    }
    f.columns = w > 0 ? w : 0; // one cell per character: the region's own bounds
    f.rows = h > 0 ? h : 0;
    return f;
}

/// The same resolution, from the shapes themselves.
inline constexpr RegionFit fit_region(const SurfaceTextRegion& r,
                                      const SurfaceExtent& metric) noexcept {
    return fit_region(r.x, r.y, r.w, r.h, metric.text_advance_px, metric.text_line_px);
}

/// THE PART OF A VIEWPORT THAT IS ACTUALLY ON THE SURFACE, in device pixels.
///
/// Separate from `fit_region` on purpose. The FIT is authored geometry and must
/// be identical on both sides of the conversation; the CLIP is a property of the
/// surface a medium happens to have right now, and a window that is two pixels
/// too small must not change how much prose the publisher believed fit. So a
/// region hanging off the edge draws less and still says the same thing about
/// what it is showing.
inline constexpr RegionViewport clip_viewport(const RegionViewport& v, std::int64_t surface_w,
                                              std::int64_t surface_h) noexcept {
    RegionViewport out;
    out.x = v.x > 0 ? v.x : 0;
    out.y = v.y > 0 ? v.y : 0;
    // `add_cells` is cells.hpp's saturating int64 sum; the unit it is adding here
    // is pixels, and the reason for reaching for it is identical — both numbers
    // came off the wire, so the sum must not be allowed to leave the number line.
    const std::int64_t right = add_cells(v.x, v.w);
    const std::int64_t bottom = add_cells(v.y, v.h);
    const std::int64_t clip_r = right < surface_w ? right : surface_w;
    const std::int64_t clip_b = bottom < surface_h ? bottom : surface_h;
    out.w = clip_r > out.x ? clip_r - out.x : 0;
    out.h = clip_b > out.y ? clip_b - out.y : 0;
    return out;
}

/// How much of a published region the cell projection below will materialize,
/// per axis.
///
/// A bound rather than a policy: `w` and `h` are numbers a publisher chose, and
/// projecting a region a hundred million cells wide would allocate a hundred
/// million bytes per row to describe a canvas that can show a few hundred. Every
/// medium clips the labels afterwards anyway, so the cap costs a real picture
/// nothing — it costs a published absurdity the time it would otherwise take.
/// The numbers are far past any surface a person has: no display is sixteen
/// thousand cells across, and `Screen` clamps two orders of magnitude below this.
/// The same lesson cells.hpp records, one shape further out.
inline constexpr std::int64_t kMaxProjectedWidth = 16384;
inline constexpr std::int64_t kMaxProjectedRows = 16384;

/// ONE PROJECTED ROW: the label a cell medium draws, and the ground it draws it
/// on.
///
/// The ground is beside the label rather than on it, and that is deliberate.
/// `SurfaceLabel` is a shape on the wire and a label has no background — giving
/// it one would be widening a published vocabulary to carry a fact only the
/// projection of a different shape produces. So the pairing lives here, in the
/// projection's own return type, where both consumers (the terminal Skin and the
/// SDL medium's bitmap face) read it and nothing else has to know it exists.
struct ProjectedRow {
    SurfaceLabel label;
    std::int64_t background = role::kNone; ///< role::kNone: whatever is underneath
};

/// A REGION'S ROWS AS ORDINARY CANVAS LABELS — the cell projection, written once
/// and shared by every medium that has cells rather than type.
///
/// This is the terminal Skins' whole implementation of a text region, and it is
/// also what the SDL medium draws when it has no real face. That sharing is the
/// honesty proof for the vocabulary: the lower-fidelity projection is not a stub
/// invented so a terminal could claim support, it is the identical arithmetic the
/// pane performed before regions existed — row `i` at cell `(x, y + i)`, cut at
/// `w`, dropped past `h`.
///
/// EVERY CELL ROW OF THE REGION GETS A LABEL, including the ones with no row
/// behind them, and each is padded to the region's full width. A region is an
/// OVERLAY: it was granted bounds and owns what is inside them, so a row left
/// unwritten must show the region's own emptiness and not whatever was on the
/// canvas underneath. (Painted as spaces, which erase in a character medium and
/// draw nothing in a graphical one — the same trick `paint_terminal` used to do
/// for itself, now done for it.)
///
/// A ROW'S GROUND RIDES ALONG UNCHANGED (HD-2). The projection does not resolve
/// it, does not substitute for it and does not invent one for the rows with
/// nothing behind them: `role::kNone` travels out exactly as it travelled in, and
/// what a medium makes of a ground is the medium's own answer — an SGR background
/// on a terminal, a filled strip in a window, nothing at all where a medium has
/// no way to say it.
///
/// A CARET IS A CHARACTER HERE (HD-3), inserted at its column on its own row,
/// and that is this projection's whole answer to it. A cell medium has no
/// sub-cell position to put a bar at, so the honest lower-fidelity reading of
/// "the next keystroke lands between these two characters" is to put a mark
/// between them — which pushes the rest of the row one cell right and is exactly
/// what the Workshop Terminal did for itself when the caret could only be at the
/// end. Inserted BEFORE the cut, so a caret past the region's width falls off the
/// row like any other character rather than being specially rescued: this
/// projection does not scroll, and inventing a scroll here would be inventing one
/// for every consumer at once.
inline void project_one_text_region(const SurfaceTextRegion& r, std::vector<ProjectedRow>& out) {
    if (r.w <= 0 || r.h <= 0) {
        return; // a region with no bounds shows nothing, and says nothing about it
    }
    const std::size_t width = static_cast<std::size_t>(
        r.w < static_cast<std::int64_t>(kMaxProjectedWidth) ? r.w : kMaxProjectedWidth);
    const std::int64_t lines =
        r.h < static_cast<std::int64_t>(kMaxProjectedRows) ? r.h : kMaxProjectedRows;
    for (std::int64_t i = 0; i < lines; ++i) {
        const std::size_t at = static_cast<std::size_t>(i);
        const bool said = at < r.rows.size();
        std::string text = said ? r.rows[at].text : std::string();
        const std::int64_t role = said ? r.rows[at].role : role::kFill;
        const std::int64_t back = said ? r.rows[at].background : role::kNone;
        if (r.caret_row == i && r.caret_col >= 0 &&
            r.caret_col <= static_cast<std::int64_t>(text.size())) {
            text.insert(static_cast<std::size_t>(r.caret_col), 1, kCaretGlyph);
        }
        if (text.size() > width) {
            text.resize(width); // cut on a byte boundary: one cell per byte, as ever
        } else {
            text.append(width - text.size(), ' ');
        }
        out.push_back(
            ProjectedRow{SurfaceLabel{r.x, add_cells(r.y, i), std::move(text), role}, back});
    }
}

/// EVERY REGION OF ONE LAYER, AS CELLS — a character medium's whole answer for that
/// layer, because a character medium has no second list to partition against.
///
/// A LAYER AND NOT A CANVAS SINCE WIND-2a, and the argument is the whole of that phase.
/// A canvas-wide version of this function is a FLATTENER: it returns every region of
/// every plane as one run, which is precisely the global band that let a back-ranked
/// region cover a front-ranked label. There is deliberately no overload taking a
/// `SurfaceCanvas`, so no consumer — renderer, plan or test helper — can ask for the
/// order this vocabulary stopped having.
inline std::vector<ProjectedRow> project_text_regions(const SurfaceLayer& l) {
    std::vector<ProjectedRow> out;
    for (const SurfaceTextRegion& r : l.texts) {
        project_one_text_region(r, out);
    }
    return out;
}

/// THE REGIONS THIS MEDIUM CANNOT SET IN ITS OWN TYPE, as cells (HD-5).
///
/// The partition a graphical medium draws from, and it is one predicate rather than a global
/// test: a region belongs to `plan_layer_regions` when `fit_region` says its bounds hold type,
/// and to this list when they do not. With a zero metric that is EVERY region, byte-for-byte
/// what the single-argument overload above returns, which is why no canvas this repository
/// paints in a character medium moves.
///
/// Before HD-5 the split was made once for the whole canvas — regions were cells when the
/// medium had no face and type when it had one — and a region too small for the face was
/// therefore in neither list. See `fit_region` for the measurement that made that reachable.
///
/// PER LAYER SINCE WIND-2a, for the overload above's reason exactly: the partition is
/// between two lists of ONE plane, and a canvas-wide answer would put every plane's typed
/// regions after every plane's cells.
inline std::vector<ProjectedRow> project_text_regions(const SurfaceLayer& l,
                                                     const SurfaceExtent& metric) {
    std::vector<ProjectedRow> out;
    for (const SurfaceTextRegion& r : l.texts) {
        if (fit_region(r, metric).graphical()) {
            continue; // this medium sets this one in type: plan_layer_regions has it
        }
        project_one_text_region(r, out);
    }
    return out;
}

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_REGION_HPP
