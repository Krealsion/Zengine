// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SCREEN_HPP
#define ZENGINE_WORKSHOP_SCREEN_HPP

// The Workshop screen: the session facts, the maker's gestures over them, the
// inspector's rows, and the one function that turns all of it into a published
// canvas.
// Workshop law: agents/workshop/geometry.md (+24 registers; agents/workshop.md routes)

#include "attention.hpp" // what is true right now, held and dismissed
#include "complete.hpp"
#include "context.hpp" // what can be done with a pointed subject
#include "document.hpp"
#include "editor.hpp" // the source editor's buffer, byte law and tab geometry
#include "keymap.hpp"
#include "marks.hpp" // the places a maker may want to come back to
#include "panel.hpp"
#include "property.hpp"
#include "setup.hpp"
#include "vocabulary.hpp"

#include "component/text_box.hpp" // the editable line, the caret in it, and its window
#include "input/vocabulary.hpp"  // `space` -- which medium's numbers a pointer reported in
#include "surface/pointing.hpp"  // and what that medium's layout makes of them
#include "surface/region.hpp"    // and how much prose a bounded region of it holds
#include "surface/vocabulary.hpp"
#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <zen/terminal/transcript.hpp> // the participant's own record, rendered HERE

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <string>
#include <vector>

namespace zengine::workshop {

// ---- The one screen's layout, in canvas cells ------------------------------------------

/// The smallest surface this screen is laid out on -- and, deliberately, the extent it uses
/// when nothing tells it otherwise.
// WL-GEO-02 -- agents/workshop/geometry.md
inline constexpr std::int64_t kScreenMinW = 78;
inline constexpr std::int64_t kScreenMinH = 22;

/// The largest surface this screen will lay out.
// WL-SESSION-07 -- agents/workshop/session.md
inline constexpr std::int64_t kScreenMaxW = 640;
inline constexpr std::int64_t kScreenMaxH = 400;

/// THE ROWS RESERVED AT THE TOP OF THE SCREEN, and they are the first thing a maker reads.
// WL-GEO-03 -- agents/workshop/geometry.md; WL-FRONT-03 -- agents/workshop/planes.md
inline constexpr std::int64_t kTopRows = 2;

inline constexpr std::int64_t kWorkspaceX = 0; ///< the workspace's origin ON THE CANVAS...
inline constexpr std::int64_t kWorkspaceY = kTopRows; ///< ...under the top band, which owns row 0
inline constexpr std::int64_t kWorkspaceMinW = 12; ///< narrow enough to make a share visibly shrink

/// THE SIDE REGION (`placement::kSideRegion`): the column beside the workspace, FIXED, and
/// anchored to the right edge rather than to a column number.
// WL-GEO-03 -- agents/workshop/geometry.md
inline constexpr std::int64_t kPanelCols = 28;
inline constexpr std::int64_t kPanelGap = 2; ///< cells between the workspace's edge and it

/// The rows INSIDE the side region's bounds: one, the `OBJECTS` heading.
// WL-GEO-03 -- agents/workshop/geometry.md
inline constexpr std::int64_t kSideY = kWorkspaceY; ///< the region's top edge: the body's own
inline constexpr std::int64_t kInfoHeadingRows = 1; ///< prose rows the `OBJECTS` heading keeps

/// The band under the workspace.
// WL-GEO-03 -- agents/workshop/geometry.md
// WL-FRONT-03 -- agents/workshop/planes.md
// WL-RGN-03 -- agents/workshop/regions.md
inline constexpr std::int64_t kBottomRows = 4;

static_assert(kTopRows + kBottomRows == 6,
              "QR-14 re-homed the reserved chrome and may not add to it: the workspace's "
              "extent is what a share resolves against");

// ---- THE OVERLAY STACK (`placement::kOverlayStack`) -----------------------------------------
// WL-PANE-04 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t kStackX = 0;
inline constexpr std::int64_t kStackY = kWorkspaceY; ///< directly under the screen's title row
/// THE MINIMUM WIDTH, and the base the surplus is measured from: wide enough for a build
/// recipe's tail on the 78x22 composition, where it is also the whole of the workspace.
inline constexpr std::int64_t kStackW = 48;
inline constexpr std::int64_t kStackRows = 9; ///< every panel placed here is this tall, for now
inline constexpr std::int64_t kStackGap = 1;  ///< a blank row between stacked panels

/// How many rows of the picker carry anything: the header, then one per catalog entry. It
/// has no instance and takes no slot -- it opens over the stack's FIRST slot, because it is
/// a question rather than a thing.
///
/// IT PAINTS A WHOLE PANEL'S WORTH OF ROWS ANYWAY, and that is a live finding rather than a
/// preference. A picker three rows tall over a panel nine rows tall left the panel's
/// last six rows showing underneath it, and in a character medium there is no edge between
/// them: the graphical Workshop read `Info  closed  objects and properties` and then, on the
/// next line and in the same box, `exit  --  asks 0 ever`. One panel, saying two unrelated
/// things. Covering the whole slot costs some blank rows while a question is open and buys a
/// screen that cannot be misread; leaving a gap and marking its edge would be a second
/// overlay convention for the same job.
inline constexpr std::int64_t kPickerRows = 1 + static_cast<std::int64_t>(kPanelKinds);

// ---- The terminal overlay's own furniture, in canvas cells -----------------------------

/// THE WIDTH THE PANE ASKS FOR at the smallest screen, before the room answers. It is a WANT
/// and not a floor.
// WL-GEO-02 -- agents/workshop/geometry.md
inline constexpr std::int64_t kTerminalWantW = 56;
inline constexpr std::int64_t kTerminalMinH = 13;
/// Header, the standing statement, the omission marker, the input line: the rows a pane
/// spends on being a pane, whatever is in it. Everything else is transcript.
inline constexpr std::int64_t kTerminalChrome = 4;
/// The narrowest prose the pane will claim to hold, whatever a medium's metric says.
// WL-TERM-03 -- agents/workshop/terminal.md
inline constexpr std::int64_t kTerminalMinCols = 8;

/// THE SCREEN'S FURNITURE, DERIVED IN ONE PLACE.
// WL-GEO-05 -- agents/workshop/geometry.md
struct Screen {
    std::int64_t w = kScreenMinW;  ///< the canvas extent this screen paints, in cells
    std::int64_t h = kScreenMinH;
    std::int64_t panel_x = 0;      ///< the object list and the inspector
    std::int64_t room_w = 0;       ///< the widest the workspace may be on this screen...
    std::int64_t room_h = 0;       ///< ...and the tallest
    std::int64_t notice_y = 0;
    std::int64_t help_y = 0;       ///< the first of two help lines
    std::int64_t terminal_x = 0;
    std::int64_t terminal_y = 0;
    std::int64_t terminal_w = 0;    ///< the pane's PLACEMENT, in cells, always
    std::int64_t terminal_h = 0;
    /// THE PANE'S INTERIOR, IN PROSE, and the only numbers anything downstream may
    /// use to decide what it can show.
    // WL-TERM-03 -- agents/workshop/terminal.md
    std::int64_t terminal_cols = 0;  ///< characters that fit across the pane
    std::size_t terminal_lines = 0;  ///< rows of prose the pane holds, chrome included
    std::size_t terminal_rows = 0;   ///< ...of which these many carry the transcript
    /// THE METRIC THIS SCREEN WAS RESOLVED WITH, carried rather than looked up.
    // WL-GEO-08 -- agents/workshop/geometry.md
    std::int64_t text_advance_px = 0;
    std::int64_t text_line_px = 0;
    /// ...AND HOW BIG ONE CANVAS CELL OF THAT MEDIUM IS, in its own device pixels.
    // WL-GEO-08 -- agents/workshop/geometry.md
    std::int64_t cell_px = 0;
};

/// The furniture for a surface of this extent -- TOTAL over every std::int64_t, because the
/// extent it is given came off the bus.
// WL-GEO-02, WL-GEO-03, WL-GEO-04, WL-GEO-05 -- agents/workshop/geometry.md
inline constexpr Screen screen_of(std::int64_t want_w, std::int64_t want_h,
                                  std::int64_t text_advance_px = 0,
                                  std::int64_t text_line_px = 0,
                                  std::int64_t cell_px = 0) noexcept {
    Screen s;
    // THE MEDIUM'S DEVICE UNIT IS TAKEN AS REPORTED and clamped at nothing:
    // non-positive is already the vocabulary's "my device unit IS the cell", which is the
    // reading that changes nothing, and above zero there is no number to refuse -- the
    // arithmetic that spends it saturates by its own contract.
    s.cell_px = cell_px > 0 ? cell_px : 0;
    s.w = want_w < kScreenMinW ? kScreenMinW : (want_w > kScreenMaxW ? kScreenMaxW : want_w);
    s.h = want_h < kScreenMinH ? kScreenMinH : (want_h > kScreenMaxH ? kScreenMaxH : want_h);
    s.panel_x = s.w - kPanelCols;
    s.room_w = s.panel_x - kPanelGap;
    s.room_h = s.h - kWorkspaceY - kBottomRows;
    // THE BOTTOM BAND'S FIRST TWO ROWS, DERIVED FROM ITS HEIGHT RATHER THAN COUNTED BACK
    // FROM THE SCREEN'S FOOT. They used to be `h - 4` and `h - 2` against a five-row
    // band whose first row was the setup line; the identity moved to the top band, so the
    // notice is the band's own first row and the legend follows it. Written as the band's
    // origin plus an offset, so a band that changes height cannot leave these two pointing
    // at rows it no longer owns.
    s.notice_y = s.h - kBottomRows;
    s.help_y = s.notice_y + 1;
    // THE PANE, INSIDE THE ROOM THE SCREEN JUST RESERVED. `room_w` is two lines up
    // and it is the whole of the fix: the pane's right edge is the workspace's right edge, so
    // the reserved side column is not the pane's to spend and does not have to know it. What
    // the pane WANTS is rule unchanged (half of every pair of columns the surface
    // gains); what it GETS is the smaller of that want and the room. The two differ only
    // below 94 columns, where the want exceeds the whole room and the pane simply is the
    // room -- and `room_w` is never less than the minimum screen's 48, so the clamp has no
    // degenerate branch to guard.
    const std::int64_t pane_want = kTerminalWantW + (s.w - kScreenMinW) / 2;
    s.terminal_w = pane_want < s.room_w ? pane_want : s.room_w;
    s.terminal_h = kTerminalMinH + (s.h - kScreenMinH) / 2;
    s.terminal_x = s.room_w - s.terminal_w;
    s.terminal_y = s.h - s.terminal_h;
    const surface::RegionFit fit = surface::fit_region(s.terminal_x, s.terminal_y, s.terminal_w,
                                                      s.terminal_h, text_advance_px,
                                                      text_line_px);
    // A FLOOR THE PANE CANNOT DROP THROUGH. The metric arrives on the bus, so a medium
    // could report a line height taller than the whole pane -- and a pane with no rows is
    // indistinguishable from a broken tool, which is the same argument `entries_that_fit`
    // makes for always showing one entry. Below the floor the pane is a cramped pane; it is
    // never an empty box, and it never publishes a negative row count for someone else to
    // subtract from.
    s.terminal_cols = fit.columns > kTerminalMinCols ? fit.columns : kTerminalMinCols;
    const std::int64_t lines =
        fit.rows > kTerminalChrome + 1 ? fit.rows : kTerminalChrome + 1;
    s.terminal_lines = static_cast<std::size_t>(lines);
    s.terminal_rows = static_cast<std::size_t>(lines - kTerminalChrome);
    // The metric AS THE FIT RESOLVED IT, not as it arrived: `fit_region` already
    // spelled a non-positive advance or line height as "text is a cell" and
    // answered zero for both, so a screen never carries half a metric.
    s.text_advance_px = fit.advance_px;
    s.text_line_px = fit.line_px;
    return s;
}

/// The minimum screen, and the one the terminal projection keeps. Named because the
/// assertions under it are this phase's own regression test in the type system: every number
/// the 78x22 composition was written with is still exactly what this screen resolves to.
inline constexpr Screen kMinScreen = screen_of(kScreenMinW, kScreenMinH);

static_assert(kMinScreen.panel_x == 50, "the panel column has not moved on the minimum screen");
static_assert(kMinScreen.room_w == 48, "the workspace's documented default width");
static_assert(kMinScreen.room_h == 16, "the workspace's documented default height");
static_assert(kMinScreen.notice_y == 18 && kMinScreen.help_y == 19, "the bottom band");
// THE MINIMUM COMPOSITION'S THREE REGIONS, WRITTEN OUT.
static_assert(kTopRows == 2 && kWorkspaceY == 2, "the top band owns rows 0 and 1");
static_assert(kWorkspaceY + kMinScreen.room_h == kMinScreen.h - kBottomRows,
              "the workspace's floor IS the bottom band's top -- no cell between them, and "
              "none reserved twice");
// THE PANE'S CORNER AND EXTENT ON THE MINIMUM SCREEN.
static_assert(kMinScreen.terminal_x == 0 && kMinScreen.terminal_y == 9, "the pane's corner");
static_assert(kMinScreen.terminal_w == 48 && kMinScreen.terminal_h == 13, "the pane's extent");
static_assert(kMinScreen.terminal_x + kMinScreen.terminal_w == kMinScreen.room_w,
              "the pane's right edge is the WORKSPACE's right edge, not the screen's (HD-10)");
static_assert(kMinScreen.terminal_rows == 9, "the transcript rows the pane has always had");
// WITH NO TEXT METRIC THE PANE IS EXACTLY THE PANE IT WAS, and these two say so in the type
// system: a character IS a cell, so the interior and the placement are the same numbers, and
// every golden this repository holds over a terminal medium is unmoved.
static_assert(kMinScreen.terminal_cols == kMinScreen.terminal_w, "no metric: a character is a cell");
static_assert(kMinScreen.terminal_lines == static_cast<std::size_t>(kMinScreen.terminal_h),
              "no metric: a row is a cell row");

// ---- PLACEMENT RESOLVED: a place, on a screen, is a rectangle ---------------------------
// WL-GEO-03 -- agents/workshop/geometry.md; WL-PANE-04 -- agents/workshop/panes-and-windows.md

/// THE BOUNDS A PLACE RESOLVES TO on this screen, in canvas cells.
// WL-GEO-03 -- agents/workshop/geometry.md; WL-PANE-04 -- agents/workshop/panes-and-windows.md
inline constexpr ui::Rect placement_bounds(std::int64_t where, std::size_t slot,
                                           const Screen& sc) noexcept {
    if (where == placement::kTopBand) {
        // THE TWO ROWS THE SCREEN RESERVES AT THE TOP, WHOLE, AND THE SLOT IS NOTHING TO IT
        // -- the side region's rule at the other edge (the band has room for one pane, and
        // panel.hpp asserts it). It is the rectangle `paint` used to write the layout
        // selector and the standing identity into directly, said once, here, so that the
        // pane which now stands on it takes exactly the predecessor's rectangle as the
        // answer it gets when its maker has said nothing.
        return ui::Rect{0, 0, sc.w, kTopRows};
    }
    if (where == placement::kSideRegion) {
        // From the top of the canvas to the bottom of the workspace: the column beside the
        // material, ending where the bottom band begins.
        return ui::Rect{sc.panel_x, kSideY, kPanelCols, kWorkspaceY + sc.room_h - kSideY};
    }
    const std::int64_t n = slot >= static_cast<std::size_t>(kScreenMaxH)
                               ? kScreenMaxH
                               : static_cast<std::int64_t>(slot);
    // THE WIDTH IS THE MINIMUM PLUS HALF THE ROOM'S SURPLUS OVER IT, floored. The
    // floor is the whole of the difference at an odd surplus and it is deliberate: rounding
    // up would take the odd column from the maker, and at 79 columns of surface -- a room of
    // 49, a surplus of exactly one -- that is the difference between a panel that leaves a
    // reachable column and one that does not. `room_w` is clamped to at least
    // `kMinScreen.room_w`, which IS `kStackW`, so the subtraction is never negative and this
    // needs no guard; the x is 0, so `x + w <= room_w` holds at every extent, and strictly
    // below it wherever there is any surplus at all.
    return ui::Rect{kStackX, kStackY + n * (kStackRows + kStackGap),
                    kStackW + (sc.room_w - kStackW) / 2, kStackRows};
}

// ---- THE FINE LATTICE, AS A RECTANGLE --------------------------------------------------
// WL-GEO-06 -- agents/workshop/geometry.md

/// A rectangle on the canvas's fine lattice, in sub-units.
struct FineRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const FineRect&, const FineRect&) = default;

    constexpr bool empty() const noexcept { return w <= 0 || h <= 0; }

    /// DOES A POINTER AT THIS SUB-UNIT POSITION, REPORTED AT THIS GRAIN, LAND ON
    /// THIS RECTANGLE.
    // WL-GEO-07 -- agents/workshop/geometry.md
    constexpr bool contains_at(std::int64_t sx, std::int64_t sy,
                               std::int64_t grain) const noexcept {
        return surface::sub_span_contains(x, w, sx, grain) &&
               surface::sub_span_contains(y, h, sy, grain);
    }
};

/// A cell rectangle on the fine lattice — exact, saturating, the one door a
/// developer default walks through on its way to being arrangement truth.
inline constexpr FineRect fine_of_cells(const ui::Rect& r) noexcept {
    return FineRect{surface::subs_of_cells(r.x), surface::subs_of_cells(r.y),
                    surface::subs_of_cells(r.w), surface::subs_of_cells(r.h)};
}

/// THE CELLS A FINE RECTANGLE COVERS — the cell-grain quantization law as a
/// rectangle: [floor(left), floor(right)) per axis.
// WL-GEO-06 -- agents/workshop/geometry.md
inline constexpr ui::Rect cells_covered(const FineRect& f) noexcept {
    const std::int64_t x0 = surface::cell_of_subs(f.x);
    const std::int64_t y0 = surface::cell_of_subs(f.y);
    if (f.w <= 0 || f.h <= 0) {
        return ui::Rect{x0, y0, 0, 0};
    }
    return ui::Rect{x0, y0, surface::cell_of_subs(surface::add_cells(f.x, f.w)) - x0,
                    surface::cell_of_subs(surface::add_cells(f.y, f.h)) - y0};
}

/// A fine rectangle, decomposed onto a published `SurfaceRect` (cells plus
/// remainders, the wire's one spelling of a fine value).
inline constexpr surface::SurfaceRect wire_rect_of(const FineRect& f,
                                                   std::int64_t role) noexcept {
    const std::int64_t cx = surface::cell_of_subs(f.x);
    const std::int64_t cy = surface::cell_of_subs(f.y);
    const std::int64_t cw = surface::cell_of_subs(f.w > 0 ? f.w : 0);
    const std::int64_t ch = surface::cell_of_subs(f.h > 0 ? f.h : 0);
    return surface::SurfaceRect{cx,
                                cy,
                                cw,
                                ch,
                                role,
                                f.x - surface::subs_of_cells(cx),
                                f.y - surface::subs_of_cells(cy),
                                (f.w > 0 ? f.w : 0) - surface::subs_of_cells(cw),
                                (f.h > 0 ? f.h : 0) - surface::subs_of_cells(ch)};
}

/// The part of a fine rectangle this canvas has — `clip_to_canvas`, one lattice
/// finer, against the same canvas expressed in sub-units.
inline constexpr FineRect clip_to_canvas_fine(const FineRect& r, const Screen& sc) noexcept {
    const std::int64_t cw = surface::subs_of_cells(sc.w);
    const std::int64_t ch = surface::subs_of_cells(sc.h);
    const std::int64_t x0 = r.x < 0 ? 0 : r.x;
    const std::int64_t y0 = r.y < 0 ? 0 : r.y;
    const std::int64_t x1 =
        surface::add_cells(r.x, r.w) < cw ? surface::add_cells(r.x, r.w) : cw;
    const std::int64_t y1 =
        surface::add_cells(r.y, r.h) < ch ? surface::add_cells(r.y, r.h) : ch;
    if (x1 <= x0 || y1 <= y0) {
        return FineRect{};
    }
    return FineRect{x0, y0, x1 - x0, y1 - y0};
}

// ---- THE CHROME A PANE WEARS, AND THE INTERIOR IT LEAVES --------------------------------
// WL-CHROME-01, WL-CHROME-02, WL-CHROME-06 -- agents/workshop/chrome.md

/// THE COARSEST HONEST BOUNDARY, and the one every medium can show: one canvas cell. It is
/// what a character medium spends, what a cell-projected interior spends in any medium, and
/// the ceiling `chrome_outer_of` reserves for a surface sized by its own content.
inline constexpr std::int64_t kChromeCells = 1;
inline constexpr std::int64_t kChromeSubs = surface::subs_of_cells(kChromeCells);

/// ONE UNIT OF THE ACTIVE FACE, in sub-units -- what this screen's chrome costs before the
/// interior's own presentation gets a say (`pane_inside` below is where it gets one).
inline constexpr std::int64_t chrome_grain(const Screen& sc) noexcept {
    return surface::subs_of_one_device(sc.cell_px);
}

/// The rectangle inside a pane's chrome: `outer` less `chrome_subs` on every side, empty
/// when the outer rectangle cannot hold both edges.
// WL-CHROME-01, WL-CHROME-05 -- agents/workshop/chrome.md
inline constexpr FineRect pane_interior(const FineRect& outer,
                                        std::int64_t chrome_subs) noexcept {
    const std::int64_t w = outer.w - 2 * chrome_subs;
    const std::int64_t h = outer.h - 2 * chrome_subs;
    if (w <= 0 || h <= 0) {
        return FineRect{};
    }
    return FineRect{surface::add_cells(outer.x, chrome_subs),
                    surface::add_cells(outer.y, chrome_subs), w, h};
}

/// THE INTERIOR OF A PANE AND THE PRESENTATION IT GETS, RESOLVED TOGETHER.
// WL-CHROME-05 -- agents/workshop/chrome.md
struct PaneInside {
    FineRect rect{};              ///< the interior: `outer` less the chrome on every side
    surface::RegionFit fit{};     ///< ...resolved with the ACTIVE medium's own text metric
    std::int64_t chrome_subs = 0; ///< what one side of that boundary cost, in sub-units
};

namespace detail {

/// One candidate: inset by this much, and fit what is left. Total over every rectangle.
inline PaneInside pane_inside_at(const FineRect& outer, const Screen& sc,
                                 std::int64_t chrome_subs) {
    PaneInside p;
    p.chrome_subs = chrome_subs;
    p.rect = pane_interior(outer, chrome_subs);
    if (p.rect.w <= 0 || p.rect.h <= 0) {
        return p;
    }
    p.fit = surface::fit_region_subs(p.rect.x, p.rect.y, p.rect.w, p.rect.h,
                                     sc.text_advance_px, sc.text_line_px);
    return p;
}

} // namespace detail

/// THE ONE CALL. A pane's outer rectangle in, its interior and that interior's resolution
/// out -- and the boundary between them is the finest one the face in front of the maker
/// will actually present.
// WL-CHROME-01, WL-CHROME-03, WL-CHROME-04, WL-CHROME-07 -- agents/workshop/chrome.md
// WL-MAKER-05 -- agents/workshop/maker-pane.md
inline PaneInside pane_inside(const FineRect& outer, const Screen& sc) {
    const std::int64_t fine = chrome_grain(sc);
    if (fine < kChromeSubs) {
        const PaneInside thin = detail::pane_inside_at(outer, sc, fine);
        if (thin.fit.graphical()) {
            return thin;
        }
    }
    const PaneInside cell = detail::pane_inside_at(outer, sc, kChromeSubs);
    if (cell.rect.w > 0 && cell.rect.h > 0) {
        return cell;
    }
    return detail::pane_inside_at(outer, sc, 0);
}

/// The rectangle inside a pane's chrome, for a consumer that wants only the geometry.
inline FineRect pane_interior(const FineRect& outer, const Screen& sc) {
    return pane_inside(outer, sc).rect;
}

/// The same subtraction read BACKWARDS, in whole cells: the outer extent a surface sized by
/// its own content needs in order to hold that content INSIDE its chrome. One consumer (the
// WL-CHROME-06 -- agents/workshop/chrome.md; WL-CTX-03 -- agents/workshop/contextual.md
inline constexpr ui::Rect chrome_outer_of(std::int64_t x, std::int64_t y, std::int64_t w,
                                          std::int64_t h) noexcept {
    return ui::Rect{x, y, w + 2 * kChromeCells, h + 2 * kChromeCells};
}

/// HOW MUCH ROOM ONE COARSE GROW GIVES A PANE, in canvas cells on both axes.
// WL-ARR-10, WL-ARR-11 -- agents/workshop/arrangement.md
inline constexpr std::int64_t kCoarseStepCells = 4;

static_assert(kStackRows + kCoarseStepCells - 2 * kChromeCells - 1 >= 8,
              "one coarse grow must give a default stack pane's BODY the eight rows the "
              "tightest shipped form needs: the slot's rows, plus the step, less the chrome "
              "on both sides, less the pane's own title row");

// ---- THE CHROME'S VOICE: three roles, and the vocabulary is the one that exists ---------
// WL-CHROME-07, WL-CHROME-08 -- agents/workshop/chrome.md
inline constexpr std::int64_t kPaneChrome = surface::role::kFill;
inline constexpr std::int64_t kPaneChromeSelected = surface::role::kAccent;
inline constexpr std::int64_t kTransientChrome = surface::role::kMuted;

// ---- AUTHORED INTENT, PROJECTED ONTO THIS SCREEN -------------------------------------
// WL-PANE-08, WL-PANE-09 -- agents/workshop/panes-and-windows.md

/// WHAT ONE PANE'S AUTHORED INTENT RESOLVES TO ON THIS SCREEN.
// WL-PANE-09, WL-PANE-10 -- agents/workshop/panes-and-windows.md
struct PaneProjection {
    bool projected = true;
    /// WHAT THE AUTHORED INTENT ASKS FOR, before the canvas gets a say — in sub-units.
    /// May run past the screen's right or bottom edge, which is legal authored
    /// intent and is not rewritten.
    FineRect resolved{};
    /// ...AND THE PART OF IT THIS CANVAS ACTUALLY HAS. Empty when nothing of the pane is on
    /// screen, which is what `off-room` means and how it is told from `waiting`.
    FineRect visible{};
};

/// The part of a rectangle this canvas has. A pure intersection, and the one place a pane's
/// rectangle meets the screen's edge.
inline constexpr ui::Rect clip_to_canvas(const ui::Rect& r, const Screen& sc) noexcept {
    const std::int64_t x0 = r.x < 0 ? 0 : r.x;
    const std::int64_t y0 = r.y < 0 ? 0 : r.y;
    const std::int64_t x1 = r.x + r.w < sc.w ? r.x + r.w : sc.w;
    const std::int64_t y1 = r.y + r.h < sc.h ? r.y + r.h : sc.h;
    if (x1 <= x0 || y1 <= y0) {
        return ui::Rect{};
    }
    return ui::Rect{x0, y0, x1 - x0, y1 - y0};
}


/// CAN THIS MEDIUM PROJECT THE AUTHORED UNIT? A pane with either axis in pixels is not
/// presented in any current build: the unit is a fact about the authored row, and fixed
/// placement is not permission to present an unsupported unit as though it were understood.
// WL-SETUP-06 -- agents/workshop/setup-file.md
inline bool pane_unit_projectable(const SetupPane* authored) noexcept {
    if (authored == nullptr) {
        return true;
    }
    return authored->width.mode != pane_unit::kPixels &&
           authored->height.mode != pane_unit::kPixels;
}

/// THE DEVELOPER'S ANSWER, THEN THE MAKER'S, PER AXIS -- and then the canvas.
// WL-GEO-06 -- agents/workshop/geometry.md
// WL-PANE-01, WL-PANE-08, WL-PANE-11 -- agents/workshop/panes-and-windows.md
inline PaneProjection project_pane(std::int64_t where, std::size_t slot,
                                   const SetupPane* authored, const Screen& sc) {
    PaneProjection out;
    // THE DEVELOPER'S ANSWER IS CELL-LATTICE AND ENTERS THE FINE LATTICE EXACTLY
    //: `placement_bounds` keeps thinking in the screen's own cells, and the
    // multiply here is where its rectangle becomes arrangement truth a maker's
    // override lays over, per axis, in the same sub-units the override carries.
    out.resolved = fine_of_cells(placement_bounds(where, slot, sc));
    // THE UNIT IS ASKED FIRST AND FOR EVERY PLACEMENT. A refusal is WHOLE --
    // no rectangle, resolved or visible -- so every consumer that already reads an empty
    // rectangle as "nowhere" is right about a pixel-sized pane with no branch of its own.
    if (!pane_unit_projectable(authored)) {
        return PaneProjection{false, FineRect{}, FineRect{}};
    }
    // THE MAKER'S ANSWER IS SPENT WHEREVER THE PLACE IS THEIRS TO AUTHOR. This
    // used to name the overlay stack, which was the same set said as a list while the stack
    // was the only movable place -- and a list is what a fourth place would be added to by
    // somebody who remembered. `place_is_authorable` (panel.hpp) is the exclusion itself:
    // the side region is the SCREEN's and everything else takes an override. Nothing about
    // the overlay stack changed; the top band joined it.
    if (place_is_authorable(where) && authored != nullptr) {
        if (authored->place.mode == pane_unit::kSubcells) {
            out.resolved.x = authored->place.x;
            out.resolved.y = authored->place.y;
        }
        if (authored->width.mode == pane_unit::kSubcells) {
            out.resolved.w = authored->width.amount;
        }
        if (authored->height.mode == pane_unit::kSubcells) {
            out.resolved.h = authored->height.amount;
        }
    }
    out.visible = clip_to_canvas_fine(out.resolved, sc);
    return out;
}

/// What the one narrow path answers with: whether this kind is open, where its kind is
/// placed, and the rectangle it occupies if it is open at all.
struct PanelBounds {
    bool open = false;
    /// THE KIND'S DECLARED PLACE, open or not — a fact about the catalog rather than about
    /// this session, so it is answerable for a panel nobody has opened.
    std::int64_t placed_in = placement::kOverlayStack;
    /// EMPTY WHEN THE PANEL IS NOT OPEN, deliberately.
    // WL-ARR-04 -- agents/workshop/arrangement.md
    // WL-PANE-09 -- agents/workshop/panes-and-windows.md
    FineRect rect{};
    /// ...and what the authored intent ASKED for, unclipped. Read by the state classifier,
    /// which has to tell "partly cut off" from "not on this screen at all".
    FineRect resolved{};
    /// FALSE WHEN THIS MEDIUM CANNOT PROJECT THE AUTHORED UNIT. `rect` is then empty too,
    /// so nothing paints, nothing is met and no room is granted -- but the reason is a
    /// different one from off-room and a maker is told which.
    bool projected = true;
};

/// WHERE AN OPEN PANEL IS RIGHT NOW — the one narrow path, and the only thing that knows how
/// a slot is earned.
// WL-PANE-03, WL-PANE-07, WL-PANE-09 -- agents/workshop/panes-and-windows.md
// WL-MAKER-05 -- agents/workshop/maker-pane.md
inline PanelBounds bounds_of(const Panels& panels, const Setup& setup, std::int64_t kind,
                             const Screen& sc) {
    std::size_t slot = 0;
    for (const Panel& p : panels.open) {
        const std::int64_t where = placement_of(p.kind);
        const SetupPane* authored = nullptr;
        for (const SetupPane& row : setup.panes) {
            const std::optional<std::int64_t> named = resolve_pane(row.ref, panels);
            if (named.has_value() && *named == p.kind) {
                authored = &row;
                break;
            }
        }
        if (p.kind == kind) {
            const PaneProjection got = project_pane(where, slot, authored, sc);
            return PanelBounds{true, where, got.visible, got.resolved, got.projected};
        }
        if (where == placement::kOverlayStack &&
            (authored == nullptr || authored->place.mode == pane_unit::kDefault)) {
            ++slot;
        }
    }
    return PanelBounds{false, placement_of(kind), FineRect{}, FineRect{}, true};
}

// The two places fit the SMALLEST screen this composition is honest on, which is where they
// are tightest.
// WL-GEO-04 -- agents/workshop/geometry.md
inline constexpr ui::Rect kMinSide = placement_bounds(placement::kSideRegion, 0, kMinScreen);
inline constexpr ui::Rect kMinStack = placement_bounds(placement::kOverlayStack, 0, kMinScreen);

static_assert(kMinStack.x + kMinStack.w <= kMinSide.x - kPanelGap,
              "the two places do not overlap: a stacked panel never reaches the side region");
static_assert(kMinStack.x + kMinStack.w == kMinScreen.room_w,
              "the stack is exactly the minimum screen's workspace width -- it covers the top "
              "of the workspace and nothing else");
// AND THE HALF-SHARE NEVER SPENDS WHAT IS NOT THE STACK'S.
static_assert(kMinStack.w == kStackW, "the minimum composition is byte-identical");
static_assert(placement_bounds(placement::kOverlayStack, 0, screen_of(79, 22)).w == kStackW,
              "an odd surplus of one is FLOORED: the odd column stays the maker's");
static_assert(placement_bounds(placement::kOverlayStack, 0, screen_of(200, 60)).w == 109,
              "48 + (170 - 48)/2 -- the half-share, spelled out");
static_assert(placement_bounds(placement::kOverlayStack, 3, screen_of(200, 60)).w ==
                  placement_bounds(placement::kOverlayStack, 0, screen_of(200, 60)).w,
              "the width is a fact about the SCREEN, not about which slot a panel sits in");
static_assert(kMinStack.y + kMinStack.h <= kMinScreen.notice_y,
              "the stack's first slot stays clear of the notice line");
static_assert(kMinSide.x + kMinSide.w == kMinScreen.w,
              "the side region reaches the screen's right edge");
static_assert(kMinSide.y + kMinSide.h == kWorkspaceY + kMinScreen.room_h,
              "the side region ends where the workspace does, above the bottom band");
// AND THE TERMINAL PANE OBEYS THE SAME LAW AS THE STACK.
static_assert(kMinScreen.terminal_x + kMinScreen.terminal_w <= kMinSide.x - kPanelGap,
              "the two places do not overlap: the terminal pane never reaches the side region");
static_assert(kPickerRows + 2 * kChromeCells <= kStackRows,
              "the picker still fits a panel's slot INSIDE its own chrome (WUX-5): the "
              "declared floor is the compile-time catalog, and the slot must seat it plus "
              "the boundary. A runtime-widened population outgrowing the slot is a "
              "different and already-answered question -- `list_window` counts what it hid");

/// WHERE THE PICKER OPENS: the stack's first slot, and it is a function rather than a repeated
/// expression so that the mode that PAINTS there and the pointer that must not see THROUGH it
/// read one answer. The picker has no catalog row to declare a place in -- it is a mode -- so
/// this is the one presentation that names its own place, and now it names it once.
inline constexpr FineRect picker_bounds(const Screen& sc) noexcept {
    // Cell-lattice geometry on the fine lattice, exactly — the picker is screen
    // furniture and never moves by less than a cell; what is fine is the machinery
    // it shares with the panes (frames, prose places, occupancy).
    return fine_of_cells(placement_bounds(placement::kOverlayStack, 0, sc));
}

/// THE OVERLAY COLUMN: the stack's first slot's corner and width, from its top to the
/// workspace's bottom -- the floor `stack_capacity` itself respects, one row above the
/// setup line, so nothing placed here can erase the line naming the arrangement.
// WL-KEY-10 -- agents/workshop/keyboard.md
inline constexpr FineRect overlay_column(const Screen& sc) noexcept {
    const ui::Rect slot = placement_bounds(placement::kOverlayStack, 0, sc);
    return fine_of_cells(ui::Rect{slot.x, slot.y, slot.w, kWorkspaceY + sc.room_h - slot.y});
}

/// HOW MANY OVERLAY SLOTS THIS SCREEN ACTUALLY HAS ROOM FOR -- the one
/// answer to "may another panel be presented", asked before anything reaches
/// `Panels::open`.
// WL-PANE-03, WL-PANE-04 -- agents/workshop/panes-and-windows.md
// WL-EDIT-13 -- agents/workshop/editor.md
inline constexpr std::size_t stack_slots_that_fit(const Screen& sc) noexcept {
    const std::int64_t floor_y = kWorkspaceY + sc.room_h;
    std::size_t fit = 0;
    while (fit < kMaxSetupPanes) {
        const ui::Rect b = placement_bounds(placement::kOverlayStack, fit, sc);
        if (b.y + b.h > floor_y) {
            break;
        }
        ++fit;
    }
    return fit;
}

/// The same answer in the shape `reconcile` takes, so no call site spells the
/// conversion itself.
inline constexpr StackCapacity stack_capacity(const Screen& sc) noexcept {
    return StackCapacity{stack_slots_that_fit(sc)};
}

static_assert(kWorkspaceY + kMinScreen.room_h == kMinScreen.notice_y,
              "the overlay floor is the workspace's bottom, which is the bottom band's own "
              "top row: a slot allowed past it would erase the row the tool speaks in");
static_assert(stack_slots_that_fit(kMinScreen) == 1,
              "the minimum composition has room for exactly one overlay panel");

// ---- PLACEMENT SPENT ON THE POINTER: a place a maker can see is a place a hand meets ------
// WL-PANE-05 -- agents/workshop/panes-and-windows.md; WL-PRESS-04 -- agents/workshop/press-chain.md

/// THE ANSWER `Occupancy` GIVES WHEN WHAT IT MET IS NOT A PANEL AT ALL -- the picker, which
/// is a presentation with no kind.
///
/// NEGATIVE, for `role::kNone`'s and `kNoCaret`'s reason exactly: a panel kind is
/// non-negative by construction (`panel::kBuilder` is 0 and `kFirstRuntimeKind` is 1024), so
/// the sentinel cannot collide with a kind a later catalog might mean, and a consumer that
/// forgot to test it would fall outside every lookup rather than into the first one.
inline constexpr std::int64_t kNoKind = -1;

/// The canvas cell a reported pointer position lands on, whatever medium
/// reported it -- or nothing, for a space this application cannot place.
// WL-GEO-07 -- agents/workshop/geometry.md
struct PointedAt {
    bool understood = false;
    surface::CanvasPoint cell;
    /// THE SAME MOMENT, ONE LATTICE FINER: the position in sub-units, and
    /// the GRAIN the reporting medium can honestly distinguish — one window pixel
    /// or one terminal cell, in sub-units. The cell above is exactly
    // WL-GEO-07 -- agents/workshop/geometry.md
    surface::CanvasPoint sub;
    std::int64_t grain = surface::kCellGrainSubs;
};

inline PointedAt canvas_point_of(std::int64_t space, std::int64_t x, std::int64_t y) noexcept {
    if (space == input::space::kCells) {
        return PointedAt{true, surface::canvas_of_terminal_cells(x, y),
                         surface::canvas_subs_of_terminal_cells(x, y),
                         surface::kCellGrainSubs};
    }
    if (space == input::space::kPixels) {
        return PointedAt{true, surface::canvas_of_window_pixels(x, y),
                         surface::canvas_subs_of_window_pixels(x, y),
                         surface::kPixelGrainSubs};
    }
    return PointedAt{};
}

/// WHAT A MAKER'S HAND MEETS AT A CANVAS CELL: nothing, or the presentation occupying it.
struct Occupancy {
    bool occupied = false;
    /// The name a maker reads on those cells -- the catalog's own for a panel, the picker's
    /// own for the picker. Empty when nothing is there, and never a kind a caller has to
    /// switch on: what it is FOR is a sentence.
    // WL-PANE-05 -- agents/workshop/panes-and-windows.md
    std::string what;
    /// WHICH PRESENTATION, as a handle -- `kNoKind` for the picker and for nothing at all.
    // WL-PRESS-04 -- agents/workshop/press-chain.md
    std::int64_t kind = kNoKind;
};

/// DOES ANY VISIBLE PRESENTATION OCCUPY THIS CANVAS CELL — the one question the pointer asks
/// before it asks the document anything.
// WL-FRONT-02, WL-FRONT-05 -- agents/workshop/planes.md
// WL-PRESS-04, WL-PRESS-05 -- agents/workshop/press-chain.md
// WL-FOCUS-03 -- agents/workshop/focus.md
// WL-PANE-05 -- agents/workshop/panes-and-windows.md
// WL-TAB-09 -- agents/workshop/tab-run.md
inline Occupancy occupied_at(const Panels& panels, const Setup& setup, const Screen& sc,
                             const PointedAt& at) {
    // EVERY TEST BELOW IS THE POINTER'S OWN GRAIN AGAINST FINE GEOMETRY — the
    // aligned-span law, so the cells and pixels a pane paints are exactly the ones on
    // which it answers. For whole-cell rectangles this is the cell containment this
    // walk has always performed.
    if (panels.picker.open && picker_bounds(sc).contains_at(at.sub.x, at.sub.y, at.grain)) {
        return Occupancy{true, kPickerName, kNoKind};
    }
    const std::vector<std::int64_t> order = effective_pane_order(setup, panels);
    for (std::size_t i = order.size(); i > 0; --i) {
        const std::int64_t kind = order[i - 1];
        if (bounds_of(panels, setup, kind, sc).rect.contains_at(at.sub.x, at.sub.y, at.grain)) {
            // `kind_name` AND NOT `panel_kind(kind).name`. The total lookup answers
            // `Builder` for anything outside the compile-time catalog, so an external pane
            // would tell a maker their hand was on the build tool -- the same lie
            // `resolve_pane` is fallible to prevent, arriving through the pointer instead
            // of through a file. Built-ins are unchanged: `kind_name` reads the same row.
            return Occupancy{true, kind_name(panels, kind), kind};
        }
    }
    return Occupancy{};
}

/// The same walk for a CELL-GRAIN probe: which presentation occupies this canvas cell —
/// a well-formed question a terminal pointer asks natively and a cell-lattice consumer
/// (a suite case included) may ask directly. One line, so the two spellings cannot
// WL-FRONT-02, WL-FRONT-05 -- agents/workshop/planes.md
// WL-PRESS-04, WL-PRESS-05 -- agents/workshop/press-chain.md
// WL-FOCUS-03 -- agents/workshop/focus.md
// WL-PANE-05 -- agents/workshop/panes-and-windows.md
// WL-TAB-09 -- agents/workshop/tab-run.md
inline Occupancy occupied_at(const Panels& panels, const Setup& setup, const Screen& sc,
                             std::int64_t cx, std::int64_t cy) {
    return occupied_at(panels, setup, sc,
                       PointedAt{true, surface::CanvasPoint{cx, cy},
                                 surface::CanvasPoint{surface::subs_of_cells(cx),
                                                      surface::subs_of_cells(cy)},
                                 surface::kCellGrainSubs});
}

/// The workspace extent a fresh session opens on: the whole of the minimum screen's room.
inline constexpr std::int64_t kWorkspaceW = kMinScreen.room_w;
inline constexpr std::int64_t kWorkspaceH = kMinScreen.room_h;

/// What the size handle looks like. One character, because it occupies one cell,
/// and one that none of the medium's role glyphs already use (`.` workspace,
/// `#` body, `*` ring, `!` alert) -- an affordance a maker cannot tell from the
/// furniture is not an affordance.
inline constexpr const char* kHandleGlyph = "+";

/// A drag in progress. Session, emphatically not content.
// WL-DOC-09 -- agents/workshop/document.md
struct Drag {
    bool active = false;
    bool resizing = false; ///< the maker took hold of the size handle, not the body
    std::int64_t id = 0;
    std::int64_t grab_dx = 0;
    std::int64_t grab_dy = 0;
};


/// THE TERMINAL OVERLAY'S VIEW OF A PARTICIPANT — session, emphatically, and a SNAPSHOT.
// WL-TERM-01, WL-TERM-03, WL-TERM-08 -- agents/workshop/terminal.md
// WL-TEXT-01 -- agents/workshop/text-box.md
struct TerminalPane {
    bool open = false;         ///< shift+space, and nothing else, decides this
    bool attached = false;     ///< is there a participant at all (a host may mount none)
    loom::WeaveId id{};        ///< the participant's identity, for the header
    /// The line being typed, before Return authors anything — AND THE CARET IN IT,
    /// AND WHICH PART OF IT THE ROW IS SHOWING.
    // WL-TEXT-01 -- agents/workshop/text-box.md
    component::TextBox input;
    std::vector<loom::TranscriptEntry> shown; ///< the newest entries that FIT, oldest first
    std::uint64_t earlier = 0; ///< kept by the participant, above the top of this pane
    std::uint64_t dropped = 0; ///< evicted from the transcript entirely -- gone, not scrolled
    /// WHAT THE PARTICIPANT COULD SAY NEXT, for the line above. Derived from
    /// `input` and the participant's own vocabulary, recomputed whenever the line
    /// changes, and holding no fact that is not readable from those two -- so it is a
    /// snapshot in exactly the sense `shown` is, and for the same reason.
    Completion completion;
    /// THE ONE PIECE OF COMPLETION STATE THAT IS NOT DERIVED: the maker pressed Escape
    /// and does not want the list for this part of the line.
    // WL-TERM-05 -- agents/workshop/terminal.md
    bool dismissed = false;
    LineSlot dismissed_at = LineSlot::Verb;
    /// ...and the other direction: the maker pressed the completion key on an EMPTY line,
    /// which is the one place discovery needs a gesture.
    // WL-TERM-05 -- agents/workshop/terminal.md
    bool asked = false;
};

// ---- PANE MANAGEMENT: what a maker is ARRANGING, and how ------------------------------

/// THE EIGHT MANIPULATION AFFORDANCES of a rectangle, and there is not a ninth.
// WL-ARR-05 -- agents/workshop/arrangement.md
namespace pane_edge {
inline constexpr std::int64_t kLeft = 0;
inline constexpr std::int64_t kRight = 1;
inline constexpr std::int64_t kTop = 2;
inline constexpr std::int64_t kBottom = 3;
inline constexpr std::int64_t kTopLeft = 4;
inline constexpr std::int64_t kTopRight = 5;
inline constexpr std::int64_t kBottomLeft = 6;
inline constexpr std::int64_t kBottomRight = 7;
inline constexpr std::int64_t kCount = 8;
} // namespace pane_edge

/// NO EDGE. Negative, for `role::kNone`'s reason: every edge is a non-negative index into a
/// table, so an absence cannot collide with one.
inline constexpr std::int64_t kNoPaneEdge = -1;

/// The edge a maker reads, and the mark they read it BY.
// WL-ARR-09 -- agents/workshop/arrangement.md
inline constexpr const char* pane_edge_name(std::int64_t edge) noexcept {
    switch (edge) {
    case pane_edge::kLeft: return "left";
    case pane_edge::kRight: return "right";
    case pane_edge::kTop: return "top";
    case pane_edge::kBottom: return "bottom";
    case pane_edge::kTopLeft: return "top-left";
    case pane_edge::kTopRight: return "top-right";
    case pane_edge::kBottomLeft: return "bottom-left";
    case pane_edge::kBottomRight: return "bottom-right";
    default: return "none";
    }
}

/// Plain ASCII, because this canvas is plain ASCII by contract and a glyph a medium cannot
/// draw is a mark a maker cannot read (`detail::kElided`'s reason).
inline constexpr const char* pane_edge_mark(std::int64_t edge) noexcept {
    switch (edge) {
    case pane_edge::kLeft: return "<";
    case pane_edge::kRight: return ">";
    case pane_edge::kTop: return "^";
    case pane_edge::kBottom: return "v";
    case pane_edge::kTopLeft: return "<^";
    case pane_edge::kTopRight: return "^>";
    case pane_edge::kBottomLeft: return "<v";
    case pane_edge::kBottomRight: return "v>";
    default: return "-";
    }
}

/// HOW DEEP AN EDGE'S GRAB BAND REACHES INTO THE PANE, in sub-units: one cell —
/// exactly the ring the affordances have always occupied.
// WL-ARR-01 -- agents/workshop/arrangement.md
inline constexpr std::int64_t kPaneEdgeBandSubs = surface::kCellSubs;

/// THE ONE CELL-SIZED MARK AN AFFORDANCE IS DRAWN ON — at the pane's own fine
/// edges.
// WL-ARR-09 -- agents/workshop/arrangement.md
inline FineRect pane_edge_cell(const FineRect& r, std::int64_t edge) noexcept {
    const std::int64_t cell = surface::kCellSubs;
    const std::int64_t x0 = r.x;
    const std::int64_t x1 = r.w > cell ? r.x + r.w - cell : r.x;
    const std::int64_t y0 = r.y;
    const std::int64_t y1 = r.h > cell ? r.y + r.h - cell : r.y;
    const std::int64_t xm = r.w > cell ? r.x + (r.w - cell) / 2 : r.x;
    const std::int64_t ym = r.h > cell ? r.y + (r.h - cell) / 2 : r.y;
    switch (edge) {
    case pane_edge::kLeft: return FineRect{x0, ym, cell, cell};
    case pane_edge::kRight: return FineRect{x1, ym, cell, cell};
    case pane_edge::kTop: return FineRect{xm, y0, cell, cell};
    case pane_edge::kBottom: return FineRect{xm, y1, cell, cell};
    case pane_edge::kTopLeft: return FineRect{x0, y0, cell, cell};
    case pane_edge::kTopRight: return FineRect{x1, y0, cell, cell};
    case pane_edge::kBottomLeft: return FineRect{x0, y1, cell, cell};
    case pane_edge::kBottomRight: return FineRect{x1, y1, cell, cell};
    default: return FineRect{};
    }
}

/// ONE CHARACTER, for the cell an affordance is drawn on. The two-character spelling
/// `pane_edge_mark` returns is PROSE -- it reads in a heading and would not fit in the one
/// cell a corner has. `+` is `kHandleGlyph`, this tool's existing word for "take hold here",
/// and the four corners share it because their POSITIONS already tell them apart.
inline constexpr const char* pane_edge_glyph(std::int64_t edge) noexcept {
    switch (edge) {
    case pane_edge::kLeft: return "<";
    case pane_edge::kRight: return ">";
    case pane_edge::kTop: return "^";
    case pane_edge::kBottom: return "v";
    default: return kHandleGlyph;
    }
}

/// WHICH AFFORDANCE OF THIS RECTANGLE A POINTER IS ON, or `kNoPaneEdge` — at the
/// pointer's own grain.
// WL-ARR-01 -- agents/workshop/arrangement.md; WL-GEO-07 -- agents/workshop/geometry.md
inline std::int64_t pane_edge_at(const FineRect& r, std::int64_t sx, std::int64_t sy,
                                 std::int64_t grain) noexcept {
    if (!r.contains_at(sx, sy, grain)) {
        return kNoPaneEdge;
    }
    const std::int64_t band_w = r.w < kPaneEdgeBandSubs ? r.w : kPaneEdgeBandSubs;
    const std::int64_t band_h = r.h < kPaneEdgeBandSubs ? r.h : kPaneEdgeBandSubs;
    const bool left = surface::sub_span_contains(r.x, band_w, sx, grain);
    const bool right =
        surface::sub_span_contains(surface::add_cells(r.x, r.w - band_w), band_w, sx, grain);
    const bool top = surface::sub_span_contains(r.y, band_h, sy, grain);
    const bool bottom =
        surface::sub_span_contains(surface::add_cells(r.y, r.h - band_h), band_h, sy, grain);
    if (top && left) {
        return pane_edge::kTopLeft;
    }
    if (top && right) {
        return pane_edge::kTopRight;
    }
    if (bottom && left) {
        return pane_edge::kBottomLeft;
    }
    if (bottom && right) {
        return pane_edge::kBottomRight;
    }
    if (left) {
        return pane_edge::kLeft;
    }
    if (right) {
        return pane_edge::kRight;
    }
    if (top) {
        return pane_edge::kTop;
    }
    if (bottom) {
        return pane_edge::kBottom;
    }
    return kNoPaneEdge;
}

/// THE ARRANGEMENT STATE: WHICH SCOPE A MAKER IS ARRANGING, AND WHICH PANE THE
/// VOCABULARY ADDRESSES.
// WL-ARR-03, WL-ARR-07 -- agents/workshop/arrangement.md
struct PaneArrange {
    bool open = false;
    bool desk = false;
    PaneRef pane;
    bool resetting = false;

    bool addressed() const { return !pane.provider.empty(); }
};

/// THE PANE EDITOR'S OWN STATE: which pane it is DESCRIBING, and where a maker's
/// hands are inside it.
// WL-PED-02, WL-PED-03, WL-PED-04 -- agents/workshop/pane-manager.md
struct PaneEditor {
    PaneRef subject;               ///< the pane described; an empty provider is "none"
    std::size_t cursor = 0;        ///< the PANES list's cursor; bounded at use
    std::vector<Row> rows;         ///< the subject's rows, in the order the body paints them
    std::size_t row_cursor = 0;    ///< which of those rows the keys are on
    bool on_rows = false;          ///< the keys are in the rows (true) or the PANES list
    double wheel_accum = 0.0;      /// < fractional wheel notches not yet worth a row

    bool addressed() const { return !subject.provider.empty(); }
};

/// THE PANE CREATOR'S NAME PROMPT: open or not, and the line being typed.
// WL-MAKER-11 -- agents/workshop/maker-pane.md
struct PaneNaming {
    bool open = false;
    component::TextBox line;
};

/// A PANE GESTURE IN FLIGHT. Session, emphatically not content.
// WL-ARR-01 -- agents/workshop/arrangement.md
struct PaneGesture {
    bool active = false;
    PaneRef pane;
    bool sizing = false;
    std::int64_t edge = kNoPaneEdge;
    std::int64_t grab_dx = 0; ///< move: where inside the pane's rectangle the hand took hold
    std::int64_t grab_dy = 0;
    std::int64_t from_x = 0;  ///< size: the sub-unit position the press landed on
    std::int64_t from_y = 0;
    std::int64_t base_x = 0;  ///< size: the pane's window at that moment — place...
    std::int64_t base_y = 0;
    std::int64_t base_w = 0;  ///< ...and extent
    std::int64_t base_h = 0;
};

/// WHICH EDITABLE LINE A TEXT-SELECTION DRAG IS SWEEPING.
// WL-TEXT-14 -- agents/workshop/text-box.md
namespace text_drag_place {
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kTerminalLine = 1;  ///< the Terminal pane's editable line
inline constexpr std::int64_t kPropertyDraft = 2; ///< the Inspector's live draft row
inline constexpr std::int64_t kEditorBody = 3;    ///< the source editor's document body
inline constexpr std::int64_t kPaneEditorDraft = 4; /// < the Pane Editor's live draft row
} // namespace text_drag_place

struct TextDrag {
    bool active = false;
    std::int64_t place = text_drag_place::kNone;
};

/// HOW LONG A DOUBLE-CLICK MAY TAKE.
// WL-PTR-01 -- agents/workshop/pointer.md; WL-TAB-10 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kDoubleClickMs = 400;

/// WHAT THE LAST PRESS ON AN EDITABLE LINE NAMED, so the next one can be a double.
// WL-PTR-01, WL-PTR-03 -- agents/workshop/pointer.md; WL-TAB-10 -- agents/workshop/tab-run.md
struct ClickMemory {
    bool armed = false;
    std::int64_t place = text_drag_place::kNone;
    std::uint64_t epoch = 0;        ///< the draft the press landed in (`draft_epoch`)
    std::size_t word_begin = 0;     ///< the word it named, in bytes of the whole text...
    std::size_t word_end = 0;       ///< ...end exclusive; equal ends mean no word
    std::int64_t at_ms = 0;         ///< `interaction_now_ms()` when it landed
};

/// IS THIS PRESS THE SECOND HALF OF A DOUBLE-CLICK? Pure, total, and the ONE place
/// the question is decided.
// WL-PTR-01, WL-PTR-03 -- agents/workshop/pointer.md
inline bool doubles_a_click(const ClickMemory& prior, std::int64_t place, std::uint64_t epoch,
                            const component::WordSpan& word, std::int64_t now_ms) noexcept {
    if (!prior.armed || !word.present()) {
        return false;
    }
    if (prior.place != place || prior.epoch != epoch) {
        return false;
    }
    if (prior.word_begin != word.begin || prior.word_end != word.end) {
        return false;
    }
    const std::int64_t since = now_ms - prior.at_ms;
    return since >= 0 && since <= kDoubleClickMs;
}

/// WHAT THE LAST PRESS ON A LAYOUT TAB NAMED, so the next one can be a double.
// WL-TAB-10 -- agents/workshop/tab-run.md
struct TabClickMemory {
    bool armed = false;
    std::size_t at = 0;      ///< the position the press landed on
    std::int64_t at_ms = 0;  ///< `interaction_now_ms()` when it landed
};

/// IS THIS PRESS THE SECOND HALF OF A DOUBLE-CLICK ON THE SAME TAB? Pure, total.
// WL-TAB-10 -- agents/workshop/tab-run.md
inline bool doubles_a_tab_click(const TabClickMemory& prior, std::size_t at,
                                std::int64_t now_ms) noexcept {
    if (!prior.armed || prior.at != at) {
        return false;
    }
    const std::int64_t since = now_ms - prior.at_ms;
    return since >= 0 && since <= kDoubleClickMs;
}

/// WHICH LAYOUT TAB A REORDER DRAG IS CARRYING -- the fourth gesture record.
// WL-TAB-11 -- agents/workshop/tab-run.md
struct LayoutTabDrag {
    bool active = false;
};

/// The arming a press leaves behind -- written from the same three facts the test above
/// reads, so an arming that could not qualify cannot be written.
inline ClickMemory click_landed(std::int64_t place, std::uint64_t epoch,
                                const component::WordSpan& word, std::int64_t now_ms) noexcept {
    return ClickMemory{true, place, epoch, word.begin, word.end, now_ms};
}

/// WHICH FITTED ROW A POINTER MAY INSPECT -- the surfaces whose painter still holds
/// the whole of what it cut.
// WL-PTR-08 -- agents/workshop/pointer.md
namespace reveal_place {
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kFilesLocation = 1; ///< the project browser's header row
inline constexpr std::int64_t kFilesRow = 2;      ///< one listed name in it
inline constexpr std::int64_t kInfoObject = 3;    ///< one row of the Info panel's OBJECTS
inline constexpr std::int64_t kInfoProperty = 4;  ///< ...and one of its PROPERTIES
} // namespace reveal_place

/// WHAT THE POINTER IS CURRENTLY REVEALING, or nothing.
// WL-PTR-04, WL-PTR-05 -- agents/workshop/pointer.md
struct Revealed {
    std::int64_t place = reveal_place::kNone;
    std::size_t item = 0;    ///< which object, property or listed name
    std::string text;        ///< the WHOLE row as the painter held it when this began
    std::int64_t offset = 0; ///< how many bytes of its head are currently scrolled away
    bool present() const noexcept { return place != reveal_place::kNone; }
    /// Is this the same reading? Asked by the one writer, so a motion that changed nothing
    /// about the picture does not republish one.
    bool same_as(const Revealed& other) const noexcept {
        return place == other.place && item == other.item && offset == other.offset &&
               text == other.text;
    }
};

/// THE FULL HOTKEY VIEW'S ONE FACT: whether it is open.
// WL-KEY-10, WL-KEY-11 -- agents/workshop/keyboard.md
struct HotkeysView {
    bool open = false;
};

/// The session: what a maker is currently doing, as opposed to what they have authored.
/// Kept out of `WorkshopDoc` deliberately, so the two kinds of fact cannot be mistaken for
/// each other -- selection is not content, and neither is the window it is looked at through.
struct Session {
    std::int64_t selected = 0;              ///< the selected object's IDENTITY (0 = none)
    /// HOW MUCH ROOM THE SURFACE SAID IT HAS, in canvas cells -- session, and the most
    /// session-like fact in this struct.
    // WL-GEO-08 -- agents/workshop/geometry.md
    std::int64_t screen_w = kScreenMinW;
    std::int64_t screen_h = kScreenMinH;
    /// AND HOW BIG ONE CHARACTER OF THAT SURFACE IS, in its own device pixels -- the other
    /// half of the same sentence.
    // WL-GEO-08 -- agents/workshop/geometry.md
    std::int64_t text_advance_px = 0;
    std::int64_t text_line_px = 0;
    /// AND HOW BIG ONE CANVAS CELL OF THAT SURFACE IS, in its own device pixels --
    /// the medium's answer about GEOMETRY, beside its answer about type.
    // WL-GEO-08 -- agents/workshop/geometry.md
    std::int64_t cell_px = 0;
    /// THE NORMAL WINDOW'S ROOM -- what a session save remembers as the viewport.
    // WL-SESSION-09 -- agents/workshop/session.md
    std::int64_t normal_w = kScreenMinW;
    std::int64_t normal_h = kScreenMinH;
    /// THE WINDOW'S DESKTOP PLACEMENT, AS LAST REPORTED.
    // WL-SESSION-08 -- agents/workshop/session.md
    bool placement_known = false;
    std::int64_t place_x = 0;
    std::int64_t place_y = 0;
    bool place_maximized = false;
    std::int64_t workspace_w = kWorkspaceW; ///< what a share of the workspace currently means
    std::int64_t workspace_h = kWorkspaceH;
    std::size_t cursor = 0;   ///< which inspector row the maker is on
    std::vector<Row> rows;    ///< the inspector, rebuilt when the selection changes
    Drag drag;                ///< a pointer drag in flight, if any
    /// THE LAST THING WORKSHOP HAD TO SAY, and that is all it is.
    // WL-ATTN-01 -- agents/workshop/attention.md
    std::string notice;
    bool notice_is_bad = false; ///< whether that thing was a refusal
    /// WHAT IS TRUE RIGHT NOW AND HAS NO LIVE OWNER TO DERIVE IT FROM (attention.hpp).
    // WL-ATTN-01 -- agents/workshop/attention.md
    HeldConditions conditions;
    /// ...and the view a maker reads them in, plus the ones they have hidden this session.
    /// Presentation only: it holds a mode flag, a cursor and a set of hidden statements,
    /// and nothing it shows.
    AttentionView attention;
    /// THE CONTEXTUAL-ACTION SURFACE: open, the captured subject, the open group
    /// and a cursor -- an identity and a cursor, never a snapshot (context.hpp). Opening
    /// it changes no selection and no keyboard candidate; the subject it holds is spent
    /// through the owner operations at the moment a row is chosen, and nowhere else.
    ContextMenu context;
    TerminalPane terminal;    ///< the terminal overlay, when a maker has opened it
    /// THE DYNAMIC PANELS a maker has opened, and the picker they opened them from
    /// (panel.hpp).
    Panels panels;
    /// THE AUTHORED SETUP THIS SESSION IS SHOWING, its copy of the one in its file, and the
    /// one-line editor over its name (setup.hpp).
    // WL-LAYOUT-01 -- agents/workshop/layouts.md
    SetupState setup;
    /// WHICH SCOPE A MAKER IS ARRANGING AND WHICH PANE THE VOCABULARY ADDRESSES.
    // WL-ARR-07 -- agents/workshop/arrangement.md
    PaneArrange arrange;
    /// ...and the pane gesture their pointer is holding, if any. Deliberately NOT `drag`:
    /// a document object and a pane are two different things to be holding, and one
    /// variable for both would make "a release ends the gesture it began" a question about
    /// which kind of thing was underneath rather than a fact about the press.
    PaneGesture pane_drag;
    /// THE PANE EDITOR'S SUBJECT AND CURSORS -- see `PaneEditor`. Session and not
    /// pane state, so that closing the editor's own presentation forgets nothing a maker
    /// chose, and never persisted, because a subject is a fact about a maker's attention.
    PaneEditor pane_editor;
    /// THE PANE CREATOR'S NAME PROMPT -- see `PaneNaming`. A mode, beside the
    /// layout-name editor's for the same reason: a maker's hand halfway through a word.
    PaneNaming pane_naming;
    /// THE SOURCE DOCUMENT THIS SESSION IS EDITING (editor.hpp) -- the path, the multiline
    /// buffer with its caret/selection/history, the saved copy the dirty answer derives
    /// from, and the viewport. Session and not pane state, emphatically: the Editor PANE
    // WL-EDIT-01 -- agents/workshop/editor.md
    EditorState editor;
    /// ...and the text selection their pointer is sweeping, if any. The third
    /// gesture record, for the two records' own reason; see `TextDrag`.
    TextDrag text_drag;
    /// ...and what their LAST press on an editable line named, so the next one can be a
    /// double-click. See `ClickMemory`: an identity and an instant, no place.
    ClickMemory click;
    /// ...and what their LAST press on a LAYOUT TAB named, so the next one can be a
    /// double-click. See `TabClickMemory`.
    TabClickMemory tab_click;
    /// ...and the layout tab their pointer is dragging along the run, if any. The
    /// fourth gesture record, for the other three's own reason; see `LayoutTabDrag`.
    LayoutTabDrag tab_drag;
    /// ...and which clipped row their pointer is currently reading past the ellipsis
    /// See `Revealed`: presentation only, and the most transient state here.
    Revealed reveal;
    /// THE CLIPBOARD THIS WORKSHOP'S TEXT BOXES OPERATE ON — session in the plainest sense.
    // WL-TEXT-08 -- agents/workshop/text-box.md
    component::Clipboard clipboard;
    /// THE EFFECTIVE BINDING TRUTH: declaration defaults plus the maker's
    /// authored overrides, plus the legend preference.
    // WL-KEY-02 -- agents/workshop/keyboard.md
    Keymap keymap;
    /// ...and the full hotkey view over it, when a maker has opened one.
    HotkeysView hotkeys;
    /// WHETHER THE ARRANGEABLE PANES PAINT THEIR TITLE ROWS -- a presentation preference.
    // WL-FOCUS-11 -- agents/workshop/focus.md
    bool pane_titles = true;
    /// THE AUTHORED RECIPE CATALOG THIS SESSION HAS MOVED TO -- empty until a
    /// maker replaces one, and the emptiness is the whole of the fact.
    // WL-PROJ-09 -- agents/workshop/project.md
    std::string recipes_moved_to;

    /// THE PLACES THIS RUN KNOWS ARE WORTH RETURNING TO (`marks.hpp`).
    // WL-FILES-05 -- agents/workshop/files.md
    LocationMarks marks;
};

/// This session's screen furniture. The one call; see `Screen`.
// WL-GEO-02, WL-GEO-03, WL-GEO-04, WL-GEO-05 -- agents/workshop/geometry.md
inline constexpr Screen screen_of(const Session& s) noexcept {
    return screen_of(s.screen_w, s.screen_h, s.text_advance_px, s.text_line_px, s.cell_px);
}

/// DOES THE SOURCE EDITOR HAVE THE KEYBOARD RIGHT NOW?
// WL-FOCUS-01, WL-FOCUS-02 -- agents/workshop/focus.md
inline bool editor_has_keyboard(const Session& s) {
    if (s.panels.keyboard != panel::kEditor || !s.editor.open_document() ||
        !s.panels.has(panel::kEditor)) {
        return false;
    }
    const FineRect where =
        bounds_of(s.panels, s.setup.active, panel::kEditor, screen_of(s)).rect;
    return where.w > 0 && where.h > 0;
}

/// DOES THE PROJECT BROWSER HAVE THE KEYBOARD RIGHT NOW?
// WL-FOCUS-01, WL-FOCUS-02, WL-FOCUS-04 -- agents/workshop/focus.md
// WL-FILES-07 -- agents/workshop/files.md
inline bool files_has_keyboard(const Session& s) {
    if (s.panels.keyboard != panel::kProjectFiles || !s.panels.has(panel::kProjectFiles)) {
        return false;
    }
    const FilesPane& pane = s.panels.files;
    if (!pane.listing.known && pane.current_dir.empty() && !s.marks.somewhere_to_go()) {
        return false;
    }
    const FineRect where =
        bounds_of(s.panels, s.setup.active, panel::kProjectFiles, screen_of(s)).rect;
    return where.w > 0 && where.h > 0;
}

/// IS THE PANE EDITOR THE PANE A MAKER LAST PRESSED INTO, WITH SOMETHING TO SHOW?
// WL-FOCUS-01 -- agents/workshop/focus.md; WL-PED-07 -- agents/workshop/pane-manager.md
inline bool pane_editor_has_keyboard(const Session& s) {
    if (s.panels.keyboard != panel::kPaneEditor || !s.panels.has(panel::kPaneEditor)) {
        return false;
    }
    const FineRect where =
        bounds_of(s.panels, s.setup.active, panel::kPaneEditor, screen_of(s)).rect;
    return where.w > 0 && where.h > 0;
}

/// IS A DRAFT LIVE ON ONE OF THE PANE EDITOR'S ROWS? Its own question, kept apart from the
/// Info panel's `draft_live` on purpose: the two drafts are about different subjects, and
/// the refusals Info spends its answer on (a press on the object list rebuilds Info's rows)
/// are not true of a draft that a change of document selection cannot touch.
inline bool pane_editor_draft_live(const Session& s) {
    for (const Row& r : s.pane_editor.rows) {
        if (r.editing()) {
            return true;
        }
    }
    return false;
}

/// THE CHAIN BELOW THE CONTEXTUAL SURFACE -- the branches a key falls to once no mode above
/// them claims it. Split out of `keyboard_context` because the contextual surface needs
/// exactly this half as a VALUE: what the keys would mean when the menu closes.
// WL-KEY-03 -- agents/workshop/keyboard.md; WL-FOCUS-06 -- agents/workshop/focus.md
// WL-ATTN-09 -- agents/workshop/attention.md; WL-CTX-06 -- agents/workshop/contextual.md
// WL-PED-07 -- agents/workshop/pane-manager.md
inline KeyContext keyboard_context_beneath_menu(const Session& s) {
    if (s.setup.naming.open) {
        return KeyContext::kNaming;
    }
    // THE PANE CREATOR'S NAME PROMPT IS THE LAYOUT-NAME EDITOR'S TWIN, in the same
    // position and for the same reason: a maker typing a name has the keyboard whole. The
    // two cannot be open at once -- each is reachable only from a context the other owns
    // -- and the order is written down anyway, because an ordering that rests on a
    // reachability proof is one refactor from being silently wrong.
    if (s.pane_naming.open) {
        return KeyContext::kPaneNaming;
    }
    if (s.panels.picker.open) {
        return KeyContext::kPicker;
    }
    // THE CURRENT-CONDITION VIEW IS A MODE, IN THE PICKER'S OWN PLACE: below the
    // Terminal and the arrangement scopes, above a focused pane and a live draft. It owns
    // the keyboard while it is open for the picker's reason -- it is a list with a cursor
    // and a gesture on the selected row -- and it is deliberately NOT keys-modal like the
    // hotkey view, because its gestures are real application actions that every help
    // surface and the maker's own keymap file must be able to see.
    if (s.attention.open) {
        return KeyContext::kAttention;
    }
    if (is_runtime_kind(keyboard_pane(s.panels))) {
        return KeyContext::kPane;
    }
    // THE SOURCE EDITOR IS A PLACE IN THE FOCUSED PANE'S FAMILY: the candidate is the
    // same last-pressed memory an external pane rides, so the two cannot both be the
    // answer, and whichever the maker pointed the keys at LAST is the one that speaks --
    // own symmetry, with the same resolved-fresh discipline behind it.
    if (editor_has_keyboard(s)) {
        return KeyContext::kEditor;
    }
    // AND THE PROJECT BROWSER IS THE THIRD MEMBER OF THAT FAMILY, on the same terms. The
    // three share one candidate field, so at most one of them can be the answer and the
    // order between these two branches decides nothing -- it is written down anyway,
    // because an ordering that rests on a mutual-exclusion proof is one refactor from
    // being silently wrong.
    if (files_has_keyboard(s)) {
        return KeyContext::kFiles;
    }
    // AND THE PANE EDITOR IS THE FOURTH MEMBER, on the same candidate field and
    // the same terms. A draft open on one of ITS rows takes the keys as text exactly as the
    // Info panel's draft does one branch down -- `kDraft` is one context whichever inspector
    // the row belongs to, and `editing_key` asks which by asking this chain.
    if (pane_editor_has_keyboard(s)) {
        return pane_editor_draft_live(s) ? KeyContext::kDraft : KeyContext::kPaneEditor;
    }
    for (const Row& r : s.rows) {
        if (r.editing()) {
            return KeyContext::kDraft;
        }
    }
    return KeyContext::kCommand;
}

/// WHERE THE KEYBOARD CURRENTLY GOES, AS ONE VALUE -- the routing chain, spelled once. It
/// is resolved fresh from live session state at every spend and stored nowhere: there is
/// no context stack, and a mode that closes stops being the answer with nothing to clear.
// WL-KEY-03 -- agents/workshop/keyboard.md; WL-FOCUS-06, WL-FOCUS-09 -- agents/workshop/focus.md
// WL-ARR-14 -- agents/workshop/arrangement.md; WL-CTX-08 -- agents/workshop/contextual.md
inline KeyContext keyboard_context(const Session& s) {
    if (s.terminal.open) {
        return KeyContext::kTerminal;
    }
    if (s.arrange.open) {
        if (s.arrange.resetting) {
            return KeyContext::kArrangeReset;
        }
        return s.arrange.desk ? KeyContext::kArrangeDesk : KeyContext::kArrangePane;
    }
    // THE CONTEXTUAL-ACTION SURFACE IS A MODE AT THE TOP OF THE PICKER'S BAND:
    // below the Terminal and the arrangement scopes, above everything a press or a draft
    // could otherwise reach. It must answer before a focused pane and a live draft or its
    // own navigation keys would leak into the thing beneath it -- the first-refusal rule
    // -- and it sits above the other transient overlays because it is opened by the LATER
    // deliberate gesture whenever both are somehow open at once.
    if (s.context.open) {
        return KeyContext::kContext;
    }
    return keyboard_context_beneath_menu(s);
}

/// MAY ESCAPE'S FINAL FALLTHROUGH SHED THE PANE SELECTION IN THIS CONTEXT?
// WL-ARR-13, WL-ARR-14 -- agents/workshop/arrangement.md
inline bool escape_may_shed_selection(KeyContext c) {
    return c == KeyContext::kFiles || c == KeyContext::kPaneEditor || c == KeyContext::kCommand;
}

// ---- Spelling the effective bindings -----------------------------------------------------
// WL-KEY-02 -- agents/workshop/keyboard.md

/// The effective gesture of one action, in the screen's compact voice (`^s`, `shift+h`,
/// `enter`). The one call every claim site makes.
inline std::string hotkey_text(const Keymap& k, Act a) {
    return gesture_text(k.gesture_of(a));
}

/// Four direction actions said as one word WHEN THAT WORD IS TRUE: `arrows` exactly while
/// all four sit on their arrow defaults, their own spellings otherwise. The old headings
/// hand-folded four gestures into `arrows`; the fold survives only as long as it is a
/// fact.
inline std::string arrows_text(const Keymap& k, Act left, Act right, Act up, Act down) {
    const bool folded = k.gesture_of(left) == Gesture{input::scan::kLeft, input::mod::kNone} &&
                        k.gesture_of(right) ==
                            Gesture{input::scan::kRight, input::mod::kNone} &&
                        k.gesture_of(up) == Gesture{input::scan::kUp, input::mod::kNone} &&
                        k.gesture_of(down) == Gesture{input::scan::kDown, input::mod::kNone};
    if (folded) {
        return "arrows";
    }
    return hotkey_text(k, left) + "/" + hotkey_text(k, right) + "/" + hotkey_text(k, up) +
           "/" + hotkey_text(k, down);
}

/// The `gesture label` pairs requestable in this context, one string each, in the order
/// the band should spend room on them: the context's own rows first, then what is
/// answered above the mode chain.
// WL-KEY-09 -- agents/workshop/keyboard.md
inline std::vector<std::string> help_pairs(const Keymap& k, KeyContext ctx) {
    std::vector<std::string> out;
    // A fold: the run of actions it covers (in catalog order, keyed on the first), the
    // gestures that make it true, and the folded pair it becomes.
    struct Fold {
        Act first;
        Act rest[3];
        std::size_t others;
        Gesture wants[4];
        const char* pair;
    };
    namespace sc = input::scan;
    namespace mo = input::mod;
    static const Fold kFolds[] = {
        {Act::kObjectLeft,
         {Act::kObjectDown, Act::kObjectUp, Act::kObjectRight},
         3,
         {{sc::kH, mo::kNone}, {sc::kJ, mo::kNone}, {sc::kK, mo::kNone}, {sc::kL, mo::kNone}},
         "hjkl move"},
        {Act::kObjectNarrower,
         {Act::kObjectTaller, Act::kObjectShorter, Act::kObjectWider},
         3,
         {{sc::kH, mo::kShift},
          {sc::kJ, mo::kShift},
          {sc::kK, mo::kShift},
          {sc::kL, mo::kShift}},
         "shift+hjkl size"},
        {Act::kInfoUp,
         {Act::kInfoDown, Act::kNone, Act::kNone},
         1,
         {{sc::kUp, mo::kNone}, {sc::kDown, mo::kNone}, {}, {}},
         "up/down row"},
        {Act::kWorkspaceNarrower,
         {Act::kWorkspaceWider, Act::kNone, Act::kNone},
         1,
         {{sc::kLeftBracket, mo::kNone}, {sc::kRightBracket, mo::kNone}, {}, {}},
         "[ ] workspace"},
    };
    const auto fold_holding = [&k](const Fold& f) {
        if (k.gesture_of(f.first) != f.wants[0]) {
            return false;
        }
        for (std::size_t i = 0; i < f.others; ++i) {
            if (k.gesture_of(f.rest[i]) != f.wants[i + 1]) {
                return false;
            }
        }
        return true;
    };
    const auto folded_member = [&](Act a, const Fold*& holds) {
        for (const Fold& f : kFolds) {
            if (!fold_holding(f)) {
                continue;
            }
            if (f.first == a) {
                holds = &f;
                return true;
            }
            for (std::size_t i = 0; i < f.others; ++i) {
                if (f.rest[i] == a) {
                    holds = nullptr; // covered by the fold its first member emitted
                    return true;
                }
            }
        }
        return false;
    };
    const auto take = [&](bool concrete) {
        for (const ActionRow& row : kActionCatalog) {
            const bool is_concrete = row.context != KeyContext::kGlobal &&
                                     row.context != KeyContext::kNoText &&
                                     row.context != KeyContext::kNoEditor;
            if (is_concrete != concrete || !active_in(row.context, ctx)) {
                continue;
            }
            // A ROW WITH NO GESTURE TEACHES NO KEY. The legend's whole job is
            // `gesture label` pairs, and its scarcest resource is columns; a pair whose
            // gesture half is `?` spends them saying that a key does not exist. The action
            // is still reachable -- from the surface that names it, and from a maker's own
            // binding, which puts the row back here the moment there is one to spell.
            if (!is_bound(k.row_gesture(row))) {
                continue;
            }
            const Fold* holds = nullptr;
            if (ctx == KeyContext::kCommand && folded_member(row.act, holds)) {
                if (holds != nullptr) {
                    out.push_back(holds->pair);
                }
                continue;
            }
            std::string pair = gesture_text(k.row_gesture(row));
            pair += " ";
            pair += row.label;
            // TWO ROWS OF ONE ACTION CAN COME TO ONE SPELLING (an override moves them
            // both), and one meaning said twice in one band is noise, not truth.
            bool repeated = false;
            for (const std::string& seen : out) {
                if (seen == pair) {
                    repeated = true;
                    break;
                }
            }
            if (!repeated) {
                out.push_back(std::move(pair));
            }
        }
    };
    take(true);
    take(false);
    return out;
}

// The band's legend rows are packed from `help_pairs` by `help_rows` below `detail` --
// against however many rows the band's budget composition granted the legend, which is
// what stopped being a constant two.

/// TAKE THE ROOM A SURFACE OFFERED, and re-fit the workspace to it. Answers whether anything
/// actually changed, so a caller can decline to repaint over a surface that merely repeated
/// itself.
// WL-DOC-17 -- agents/workshop/document.md; WL-GEO-08 -- agents/workshop/geometry.md
inline bool adopt_screen(Session& s, std::int64_t want_w, std::int64_t want_h,
                         std::int64_t want_advance_px = 0, std::int64_t want_line_px = 0,
                         std::int64_t want_cell_px = 0) {
    const std::int64_t advance = want_advance_px > 0 ? want_advance_px : 0;
    const std::int64_t line = want_line_px > 0 ? want_line_px : 0;
    // THE CANVAS'S DEVICE UNIT NEEDS NO CEILING OF ITS OWN. It arrives on the bus
    // like every other field of the shape, so a negative number is data rather than an
    // error — and non-positive is already the vocabulary's "my device unit IS the cell",
    // which is the reading that changes nothing. Above zero there is no number to refuse:
    // `surface::device_of_subs` and `subs_exact_in_device` are total over every positive
    // multiplier by their own saturation, and inventing a plausibility bound here would be
    // this application deciding how big somebody else's pixel is allowed to be.
    const std::int64_t cell = want_cell_px > 0 ? want_cell_px : 0;
    const Screen fresh = screen_of(want_w, want_h, advance, line);
    if (fresh.w == s.screen_w && fresh.h == s.screen_h && advance == s.text_advance_px &&
        line == s.text_line_px && cell == s.cell_px) {
        return false;
    }
    s.screen_w = fresh.w;
    s.screen_h = fresh.h;
    s.text_advance_px = advance;
    s.text_line_px = line;
    s.cell_px = cell;
    s.workspace_w = fresh.room_w;
    s.workspace_h = fresh.room_h;
    return true;
}

/// The workspace as a viewport, and the document resolved against it — the ONE
/// call that turns authored intent into geometry in this application.
// WL-DOC-05, WL-DOC-12, WL-DOC-18 -- agents/workshop/document.md
inline ui::Scene workspace_scene(const WorkshopDoc& d, const Session& s) {
    return ui::resolve(d.elements, ui::Viewport{s.workspace_w, s.workspace_h});
}

/// The inspector for one authored object: the properties, plus the facts that
/// are not properties.
// WL-DOC-05 -- agents/workshop/document.md; WL-INFO-06 -- agents/workshop/info-body.md
inline std::vector<Row> inspector_rows(WorkshopDoc& d, const Session& s) {
    std::vector<Row> rows;
    const std::int64_t id = s.selected;
    if (doc::find(d, id) == nullptr) {
        return rows;
    }
    const std::int64_t ww = s.workspace_w;
    const std::int64_t wh = s.workspace_h;

    rows.push_back(Row::show("Identity", [id] { return "#" + std::to_string(id); }));
    rows.push_back(Row::edit("Name", doc::name_of(d, id)));
    // Context comes BEFORE the four numbers it gives meaning to, because that is
    // the order the reading has to happen in: `X 2` is not an answer until you
    // know 2 of what. It is one more `Row::edit` over one more property, and
    // that is the measurement -- a relationship is not a different kind of thing
    // needing a different kind of editor. It is
    // NOT labelled `Parent`: nothing here is a parent, and a familiar word that
    // implies ownership, clipping and cascade-delete would be the tool telling a
    // maker something the document does not do.
    rows.push_back(Row::edit("Context", doc::context_of(d, id)));
    rows.push_back(Row::edit("X", doc::x_of(d, id)));
    rows.push_back(Row::edit("Y", doc::y_of(d, id)));
    rows.push_back(Row::edit("Width", doc::width_of(d, id)));
    rows.push_back(Row::edit("Height", doc::height_of(d, id)));
    rows.push_back(Row::show("Resolved", [&d, id, ww, wh] {
        const ui::Scene scene = ui::resolve(d.elements, ui::Viewport{ww, wh});
        const ui::Placed* placed = ui::placed_for(scene, id);
        if (placed == nullptr) {
            return std::string("-");
        }
        return std::to_string(placed->rect.w) + " x " + std::to_string(placed->rect.h) + " cells";
    }));
    return rows;
}

/// Where the cursor belongs on a freshly built inspector: the first row a maker
/// can actually author. Landing it on `Identity` instead would open onto a row
/// whose only possible answer to "edit this" is a refusal.
inline std::size_t first_editable(const std::vector<Row>& rows) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].editable()) {
            return i;
        }
    }
    return 0;
}

/// Rebuild the inspector for the current selection and put the cursor somewhere
/// useful. One gesture, so the running weave and the suite cannot come to
/// disagree about what a fresh inspector is -- and rebuilding rather than
/// patching is why nothing in this package has a "refresh the inspector" call.
inline void refocus(WorkshopDoc& d, Session& s) {
    s.rows = inspector_rows(d, s);
    s.cursor = first_editable(s.rows);
}

/// REBUILD THE INSPECTOR WITHOUT TAKING A MAKER'S HANDS OFF IT.
// WL-INFO-06 -- agents/workshop/info-body.md
inline void refocus_keeping_draft(WorkshopDoc& d, Session& s) {
    const std::vector<Row> was = std::move(s.rows);
    const std::size_t cursor = s.cursor;
    s.rows = inspector_rows(d, s);
    for (std::size_t i = 0; i < s.rows.size() && i < was.size(); ++i) {
        if (s.rows[i].label() == was[i].label()) {
            s.rows[i].resume(was[i]);
        }
    }
    // AND THE CURSOR STAYS WHERE THE MAKER LEFT IT. It is the other half of "their hands are
    // still on it": a resize that moved the highlight to the first editable row would make a
    // maker who was reading Height look at Name instead, for no reason they could see.
    s.cursor = cursor < s.rows.size() ? cursor : first_editable(s.rows);
}

/// Where an identity sits in DOCUMENT ORDER, or `elements.size()` for one this
/// document does not have.
///
/// One copy, because there were about to be three. The post-delete selection
/// rule needs it and so does the object list's visible window, and "where is
/// this object in the file" is exactly the kind of small answer that goes stale
/// when it is written twice, at the smallest scale that lesson comes in.
inline std::size_t position_of(const WorkshopDoc& d, std::int64_t id) {
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        if (d.elements[i].id == id) {
            return i;
        }
    }
    return d.elements.size();
}

namespace detail {

/// Left-align in a fixed width; longer text is cut. Workshop's own layout job --
/// the canvas has no notion of a column. Its one caller pads the inspector's
/// label column to nine, and the longest label this tool has is `Resolved`, so
/// the cut is arithmetic that never fires rather than a bound anybody is
/// standing on; `fit` below is the one for text whose length a DOCUMENT decides.
inline std::string pad(std::string text, std::size_t width) {
    if (text.size() > width) {
        text.resize(width);
        return text;
    }
    text.append(width - text.size(), ' ');
    return text;
}

/// The mark a bounded presentation leaves where it could not show everything.
/// Three plain characters, because this canvas is plain ASCII by contract
/// (`SurfaceLabel`: "plain means plain") and a glyph a medium cannot draw is a
/// mark a maker cannot read.
inline constexpr const char* kElided = "...";

/// Fit `text` into `width` cells, AND SAY SO when it did not fit.
// WL-INFO-05 -- agents/workshop/info-body.md
// WL-RGN-05 -- agents/workshop/regions.md
// WL-TEXT-05 -- agents/workshop/text-box.md
inline std::string fit(std::string text, std::int64_t width) {
    if (width <= 0) {
        return {};
    }
    const std::size_t room = static_cast<std::size_t>(width);
    if (text.size() <= room) {
        return text; // it fits, so nothing about it changes -- not even its role
    }
    const std::size_t mark = std::char_traits<char>::length(kElided);
    if (room <= mark) {
        return std::string(kElided).substr(0, room);
    }
    text.resize(room - mark);
    text += kElided;
    return text;
}

/// HOW MUCH OF A PATH IS THE CUE THAT SAYS WHICH FILESYSTEM IT IS ON -- `/`, `C:/`,
/// `//server/`, or nothing at all for a spelling that has no root.
// WL-PROJ-10 -- agents/workshop/project.md
inline std::size_t path_root_cue(const std::string& p) {
    if (p.size() >= 2 && p[0] == '/' && p[1] == '/') {
        const std::size_t at = p.find('/', 2); // `//server/` -- the name AND its separator
        return at == std::string::npos ? p.size() : at + 1;
    }
    if (!p.empty() && p[0] == '/') {
        return 1;
    }
    if (p.size() >= 3 && p[1] == ':' && p[2] == '/') {
        return 3;
    }
    return 0;
}

/// FIT A PATH, KEEPING THE END THAT SAYS WHICH FILE OR DIRECTORY IT IS.
// WL-PROJ-10 -- agents/workshop/project.md; WL-TAB-03 -- agents/workshop/tab-run.md
inline std::string fit_path(const std::string& path, std::int64_t width) {
    if (width <= 0) {
        return {};
    }
    const std::size_t room = static_cast<std::size_t>(width);
    if (path.size() <= room) {
        return path; // it fits, so nothing about it changes
    }
    const std::size_t mark = std::char_traits<char>::length(kElided);
    const std::size_t root = path_root_cue(path);
    if (root + mark + 1 > room) {
        return fit(path, width); // no room for root + mark + one cell of tail
    }
    std::string tail = path.substr(path.size() - (room - root - mark));
    const std::size_t boundary = tail.find('/');
    if (boundary != std::string::npos && boundary + 1 < tail.size()) {
        tail = tail.substr(boundary); // start the tail at a whole component
    }
    return path.substr(0, root) + kElided + tail;
}

// ---- Reading past the ellipsis -----------------------------------------------------------
// WL-PTR-06 -- agents/workshop/pointer.md

/// THE FURTHEST A ROW MAY BE SCROLLED: exactly enough to bring the last byte into view, and
/// never one further. It is what makes the right edge of the row mean "the end", rather than
/// meaning "somewhere past the end" with blank cells after it.
inline std::size_t reveal_max_offset(const std::string& full, std::int64_t columns) {
    const std::size_t mark = std::char_traits<char>::length(kElided);
    if (columns <= 0 || static_cast<std::size_t>(columns) <= mark) {
        return 0;
    }
    const std::size_t room = static_cast<std::size_t>(columns) - mark;
    return full.size() > room ? full.size() - room : 0;
}

/// WHICH OFFSET THE POINTER IS ASKING FOR, from the column it is on.
// WL-PTR-06 -- agents/workshop/pointer.md
inline std::int64_t reveal_offset_at_column(const std::string& full, std::int64_t columns,
                                            std::int64_t column) {
    const std::int64_t furthest = static_cast<std::int64_t>(reveal_max_offset(full, columns));
    if (furthest <= 0 || columns <= 1 || column <= 0) {
        return 0;
    }
    const std::int64_t last = columns - 1;
    return column >= last ? furthest : (furthest * column) / last;
}

/// ONE ROW, SHOWN FROM `offset` BYTES IN. Total at every width and every offset, and never
/// wider than `columns`: the mark is spent first and `fit` bounds whatever is left, so the
/// widest answer this can give is exactly the width the fitted answer would have taken.
// WL-PTR-04, WL-PTR-06 -- agents/workshop/pointer.md
inline std::string revealed_row(const std::string& full, std::int64_t columns,
                                std::int64_t offset) {
    if (columns <= 0) {
        return {};
    }
    const std::size_t furthest = reveal_max_offset(full, columns);
    if (offset <= 0 || furthest == 0) {
        return fit(full, columns);
    }
    const std::size_t want = static_cast<std::size_t>(offset);
    return std::string(kElided) +
           fit(full.substr(want < furthest ? want : furthest),
               columns - static_cast<std::int64_t>(std::char_traits<char>::length(kElided)));
}

/// WHAT A PAINTER PUTS ON A REVEALABLE ROW -- its ordinary answer, unless the pointer is on
/// THIS row of THIS surface showing THIS string.
// WL-PTR-04, WL-PTR-05, WL-PTR-08 -- agents/workshop/pointer.md
inline std::string reveal_shown(const Revealed& rev, std::int64_t place, std::size_t item,
                                const std::string& full, std::string rest,
                                std::int64_t columns) {
    if (rev.place != place || rev.item != item || rev.offset <= 0 || rev.text != full) {
        return rest;
    }
    return revealed_row(full, columns, rev.offset);
}

/// How far a wrapped continuation row is indented, so a reader can tell one sentence
/// running on from a new one starting. Two cells: enough to be visible under the sigils
/// every transcript line begins with (`> `, `-- `, `!! `, `^ `, `v `), and cheap enough that
/// a pane loses almost none of its width to it.
inline constexpr std::int64_t kWrapIndent = 2;

/// FIT `text` INTO AS MANY ROWS AS IT NEEDS, at most `width` cells each.
// WL-TERM-07 -- agents/workshop/terminal.md
inline std::vector<std::string> wrap(const std::string& text, std::int64_t width) {
    std::vector<std::string> rows;
    if (width <= 0) {
        return rows;
    }
    const std::size_t room = static_cast<std::size_t>(width);
    const std::size_t indent =
        width > kWrapIndent + 1 ? static_cast<std::size_t>(kWrapIndent) : 0;
    std::size_t at = 0;
    while (true) {
        const std::string lead(rows.empty() ? 0 : indent, ' ');
        const std::size_t take = room - lead.size();
        if (text.size() - at <= take) {
            rows.push_back(lead + text.substr(at));
            return rows;
        }
        // The last space at or before the first character that does not fit. Landing ON that
        // character means the whole run fits and the break is clean, which is why the search
        // starts there rather than one earlier.
        std::size_t cut = at + take;
        bool broke = false;
        for (std::size_t i = cut; i > at; --i) {
            if (text[i] == ' ') {
                cut = i;
                broke = true;
                break;
            }
        }
        rows.push_back(lead + text.substr(at, cut - at));
        at = cut;
        if (broke) {
            while (at < text.size() && text[at] == ' ') {
                ++at;
            }
        }
        if (at >= text.size()) {
            return rows;
        }
    }
}

/// One cell along, without leaving the number line.
///
/// A nudge's proposal is COMPUTED rather than typed, and that widens its input
/// domain the same way sharing `resolve_extent` widens its own:
/// `x + 1` is well defined for every value a setter produced and undefined for
/// the largest one a poke can write (`WorkshopDoc` is ZEN_EXPOSE()d). So the step
/// saturates -- the neighbour of the last representable cell is itself -- and the
/// result then goes through the ordinary refusal like any other proposal. The
/// plain lane cannot see the difference; a sanitizer can, and the report records
/// the run that does.
inline std::int64_t step(std::int64_t v, std::int64_t by) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (by > 0) {
        return v > kMax - by ? v : v + by;
    }
    if (by < 0) {
        return v < kMin - by ? v : v + by;
    }
    return v;
}

/// `a - b`, without leaving the number line — `step`'s partner, and needed for
/// the same reason. A resize's proposal is a DIFFERENCE (`pointer - the object's
/// own edge`), and both terms are values this weave does not own: the pointer
/// comes off the wire and the edge comes off a poke-writable document. The
/// saturated end is far outside any workspace, which already means "nothing
/// reachable there".
inline std::int64_t minus(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (b < 0) {
        return a > kMax + b ? kMax : a - b;
    }
    return a < kMin + b ? kMin : a - b;
}

} // namespace detail

/// The band's legend rows, as the legend projects them, budget-composed.
// WL-KEY-09 -- agents/workshop/keyboard.md; WL-RGN-03 -- agents/workshop/regions.md
inline std::vector<std::string> help_rows(const Keymap& k, KeyContext ctx,
                                          std::int64_t width, std::size_t rows) {
    std::vector<std::string> out;
    if (rows == 0) {
        return out;
    }
    const std::int64_t legend = k.resolved_legend();
    if (legend == legend_mode::kHidden) {
        return out;
    }
    if (legend == legend_mode::kCompact) {
        out.push_back(detail::fit(hotkey_text(k, Act::kHotkeys) + " hotkeys", width));
        return out;
    }
    const std::vector<std::string> pairs = help_pairs(k, ctx);
    std::string row;
    std::size_t taken = 0;
    for (const std::string& pair : pairs) {
        const std::string grown = row.empty() ? pair : row + " | " + pair;
        if (static_cast<std::int64_t>(grown.size()) <= width) {
            row = grown;
            ++taken;
            continue;
        }
        if (out.size() + 1 >= rows) {
            break; // this is the last row the legend was granted: the mark below says so
        }
        out.push_back(std::move(row));
        row.clear();
        if (static_cast<std::int64_t>(pair.size()) <= width) {
            row = pair;
            ++taken;
        }
    }
    // WHAT DID NOT FIT IS MARKED, NOT SWALLOWED: the next pair is written into the cut so
    // `detail::fit`'s mark says there was more -- a help row that silently loses its last
    // hints is the failure that mark exists to prevent, and the full list is one
    // keystroke away in every legend mode.
    if (taken < pairs.size()) {
        const std::string& next = pairs[taken];
        row = detail::fit(row.empty() ? next : row + " | " + next, width);
    }
    if (!row.empty()) {
        out.push_back(std::move(row));
    }
    return out;
}

/// A PANE WINDOW PROPOSAL, IN SUB-UNITS: what one resize gesture asks the whole
/// window to become.
// WL-ARR-05, WL-ARR-06 -- agents/workshop/arrangement.md
struct PaneWindowProposal {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    /// TRUE EXACTLY WHEN THE EDGE NAMES THE LEFT — the horizontal axis must author the
    /// place's `x` for the right-edge anchor to hold. False keeps `x` unproposed.
    bool place_moved_x = false;
    /// TRUE EXACTLY WHEN THE EDGE NAMES THE TOP — the vertical twin, for `y`.
    bool place_moved_y = false;
};

inline PaneWindowProposal pane_window_proposal(std::int64_t edge, std::int64_t base_x,
                                               std::int64_t base_y, std::int64_t base_w,
                                               std::int64_t base_h, std::int64_t dx,
                                               std::int64_t dy) noexcept {
    PaneWindowProposal out{base_x, base_y, base_w, base_h, false, false};
    const bool wide = edge == pane_edge::kLeft || edge == pane_edge::kRight ||
                      edge == pane_edge::kTopLeft || edge == pane_edge::kTopRight ||
                      edge == pane_edge::kBottomLeft || edge == pane_edge::kBottomRight;
    const bool tall = edge == pane_edge::kTop || edge == pane_edge::kBottom ||
                      edge == pane_edge::kTopLeft || edge == pane_edge::kTopRight ||
                      edge == pane_edge::kBottomLeft || edge == pane_edge::kBottomRight;
    const bool leftwards = edge == pane_edge::kLeft || edge == pane_edge::kTopLeft ||
                           edge == pane_edge::kBottomLeft;
    const bool upwards = edge == pane_edge::kTop || edge == pane_edge::kTopLeft ||
                         edge == pane_edge::kTopRight;
    if (wide) {
        out.w = detail::step(base_w, leftwards ? detail::minus(0, dx) : dx);
        if (leftwards) {
            // The RIGHT edge is the anchor: base_x + base_w == x' + w', rearranged.
            out.x = detail::minus(detail::step(base_x, base_w), out.w);
            out.place_moved_x = true;
        }
    }
    if (tall) {
        out.h = detail::step(base_h, upwards ? detail::minus(0, dy) : dy);
        if (upwards) {
            // The BOTTOM edge is the anchor.
            out.y = detail::minus(detail::step(base_y, base_h), out.h);
            out.place_moved_y = true;
        }
    }
    return out;
}

// ---- Direct manipulation, and the boundary policy it needs -----------------------------
// WL-DOC-08 -- agents/workshop/document.md

/// One boundary a hand can stop at, in words a maker can read. Separate
/// sentences from the refusals in document.hpp on purpose: "stopped at the
/// workspace edge" and "the workspace starts at 0" are different events, and a
/// maker who cannot tell them apart cannot tell whether anything was written.
inline constexpr const char* kAtWorkspaceStart = "stopped at the workspace edge";
inline constexpr const char* kAtSmallest = "stopped at the smallest size";
inline constexpr const char* kAtLargest = "stopped at the largest size";
/// "of its context" and not "the workspace": a share of another object stops at
/// the whole of THAT object, and the wall is the same wall either way -- the
/// vocabulary's, not the workspace's (100% of anything is all of it).
inline constexpr const char* kAtWholeContext = "a share stops at the whole of its context";

/// What one act of DIRECT MANIPULATION did — a hand's outcome, which is not the
/// same shape as a value's outcome.
// WL-DOC-08 -- agents/workshop/document.md
struct Handled {
    Written written;
    std::string boundary;

    bool accepted() const { return written.accepted; }
    bool clamped() const { return !boundary.empty(); }

    static Handled of(Written w) { return Handled{std::move(w), {}}; }
};

// ---- The maker's gestures over one session ---------------------------------------------
//
// Session-level operations: each composes a document operation (which can refuse)
// with the selection bookkeeping that keeps the canvas, the object list and the
// inspector talking about the same object. They live here rather than in the
// weave because a gesture whose only witness is a keystroke is a gesture no suite
// can pin -- workshop.cpp binds keys and pointers to these, and nothing else.

/// Create one new authored object and select it.
// WL-DOC-10 -- agents/workshop/document.md
inline std::int64_t create(WorkshopDoc& d, Session& s) {
    const std::int64_t id = doc::add_default(d);
    if (id == 0) {
        return 0;
    }
    s.selected = id;
    refocus(d, s);
    return id;
}

/// Delete the selected object.
// WL-DOC-10 -- agents/workshop/document.md; WL-CTX-07 -- agents/workshop/contextual.md
inline Written delete_selected(WorkshopDoc& d, Session& s) {
    const std::int64_t id = s.selected;
    const std::size_t at = position_of(d, id);
    const Written removed = doc::remove(d, id);
    if (!removed.accepted) {
        return removed;
    }
    if (d.elements.empty()) {
        s.selected = 0;
    } else {
        s.selected = (at < d.elements.size() ? d.elements[at] : d.elements.back()).id;
    }
    refocus(d, s);
    return Written::ok();
}

/// Put an object where a HAND asked for it, IN WORKSPACE CELLS — the one place a
/// proposed position meets the boundary policy, and the only door `nudge` and
/// `drag_to` use.
// WL-DOC-06, WL-DOC-08 -- agents/workshop/document.md
inline Handled place(WorkshopDoc& d, const ui::Scene& scene, std::int64_t id, std::int64_t gx,
                     std::int64_t gy) {
    const ui::Element* e = doc::find(d, id);
    if (e == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    Handled done;
    if (gx < doc::kFirstCell) {
        gx = doc::kFirstCell;
        done.boundary = kAtWorkspaceStart;
    }
    if (gy < doc::kFirstCell) {
        gy = doc::kFirstCell;
        done.boundary = kAtWorkspaceStart;
    }
    const ui::Rect frame = ui::frame_in(scene, *e);
    done.written = doc::move(d, id, detail::minus(gx, frame.x), detail::minus(gy, frame.y));
    return done;
}

/// Step the selected object one cell — the keyboard's move gesture, and the only
/// one the canonical POSIX lane can perform at all (that lane produces no pointer
/// events; see workshop.cpp).
// WL-DOC-06 -- agents/workshop/document.md
inline Handled nudge(WorkshopDoc& d, Session& s, std::int64_t ddx, std::int64_t ddy) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, s.selected);
    if (placed == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    return place(d, scene, s.selected, detail::step(placed->rect.x, ddx),
                 detail::step(placed->rect.y, ddy));
}

// ---- The size a hand asked for, as an authored extent ----------------------------------

/// The authored extent a maker's HAND asks for, when it asks for a resolved size.
// WL-DOC-07 -- agents/workshop/document.md
inline ui::Extent extent_from_drag(const ui::Extent& current, std::int64_t want,
                                   std::int64_t span, std::string& boundary) {
    if (current.mode == ui::kExtentPercent) {
        const std::int64_t least = ui::resolve_extent(ui::Extent{ui::kExtentPercent, 1}, span);
        const std::int64_t most = ui::resolve_extent(ui::Extent{ui::kExtentPercent, 100}, span);
        if (want < least) {
            want = least;
            boundary = kAtSmallest;
        } else if (want > most) {
            want = most;
            // The wall a share meets at the far end is not the workspace being a
            // wall -- placement has no such limit and a cells extent has none
            // either. It is the vocabulary: a share OF something cannot be more
            // than the whole of it, so 100% is where this mode stops.
            boundary = kAtWholeContext;
        }
        if (ui::resolve_extent(current, span) == want) {
            return current; // this share already says exactly that: do not re-author it
        }
        for (std::int64_t pct = 1; pct <= 100; ++pct) {
            const ui::Extent candidate{ui::kExtentPercent, pct};
            if (ui::resolve_extent(candidate, span) >= want) {
                return candidate;
            }
        }
        return ui::Extent{ui::kExtentPercent, 100};
    }
    // Cells, and anything a poke wrote that is neither: an absolute size, whose
    // limits are the document's and have nothing to do with the workspace. An
    // object may be authored WIDER than the workspace for the same reason one may
    // be positioned past its right edge -- the canvas clips, and a maker who did
    // that has not made a mistake.
    if (want < ui::kMinCells) {
        want = ui::kMinCells;
        boundary = kAtSmallest;
    } else if (want > doc::kMaxCells) {
        want = doc::kMaxCells;
        boundary = kAtLargest;
    }
    return ui::Extent{ui::kExtentCells, want};
}

/// Author a new size from a proposal in RESOLVED cells — the shape both the
/// pointer and the keyboard arrive in, and the one place either of them becomes
/// an authored extent.
// WL-DOC-07 -- agents/workshop/document.md
inline Handled size_to(WorkshopDoc& d, const Session& s, std::int64_t id, std::int64_t want_w,
                       std::int64_t want_h) {
    const ui::Element* e = doc::find(d, id);
    if (e == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    const ui::Rect frame = ui::frame_in(workspace_scene(d, s), *e);
    Handled done;
    const ui::Extent w = extent_from_drag(e->width, want_w, frame.w, done.boundary);
    const ui::Extent h = extent_from_drag(e->height, want_h, frame.h, done.boundary);
    done.written = doc::resize(d, id, w, h);
    return done;
}

/// Grow or shrink the selected object by whole RESOLVED cells — the keyboard's
/// resize gesture, and the canonical lane's only one.
// WL-DOC-07 -- agents/workshop/document.md
inline Handled grow(WorkshopDoc& d, Session& s, std::int64_t dw, std::int64_t dh) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, s.selected);
    if (placed == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    return size_to(d, s, s.selected, detail::step(placed->rect.w, dw),
                   detail::step(placed->rect.h, dh));
}

// ---- The one resize affordance ---------------------------------------------------------

/// Where the selected object's size handle is, in WORKSPACE cells.
// WL-DOC-09 -- agents/workshop/document.md
struct Handle {
    bool shown = false;
    std::int64_t id = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
};

inline Handle size_handle(const WorkshopDoc& d, const Session& s) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, s.selected);
    if (placed == nullptr) {
        return Handle{};
    }
    // Asked without performing the addition: `rect.x + rect.w` is not
    // representable for every rect a poked extent can produce, and an overflow
    // here would put the grip somewhere the object is not.
    const std::int64_t back = detail::minus(0, placed->rect.x);
    const std::int64_t room = detail::minus(s.workspace_w, placed->rect.x);
    const std::int64_t up = detail::minus(0, placed->rect.y);
    const std::int64_t down = detail::minus(s.workspace_h, placed->rect.y);
    if (placed->rect.w < back || placed->rect.w >= room || placed->rect.h < up ||
        placed->rect.h >= down) {
        return Handle{};
    }
    return Handle{true, s.selected, placed->rect.x + placed->rect.w,
                  placed->rect.y + placed->rect.h};
}

/// Take hold of whatever authored object is under a workspace cell. Returns the
/// identity taken hold of, or 0 for empty space.
// WL-DOC-09 -- agents/workshop/document.md
inline std::int64_t begin_drag(const WorkshopDoc& d, Session& s, std::int64_t cx,
                               std::int64_t cy) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* under = ui::hit(scene, cx, cy);
    if (under == nullptr) {
        s.drag = Drag{};
        return 0;
    }
    s.drag = Drag{true, false, under->id, detail::minus(cx, under->rect.x),
                  detail::minus(cy, under->rect.y)};
    return under->id;
}

/// What a press takes hold of: the selected object's SIZE HANDLE if the press
/// landed on it, otherwise whatever object's body is under the cell. Returns the
/// identity taken hold of, or 0.
// WL-PANE-05 -- agents/workshop/panes-and-windows.md; WL-PRESS-04 -- agents/workshop/press-chain.md
inline std::int64_t take_hold(WorkshopDoc& d, Session& s, std::int64_t cx, std::int64_t cy) {
    const Handle handle = size_handle(d, s);
    if (handle.shown && handle.x == cx && handle.y == cy) {
        s.drag = Drag{true, true, handle.id, 0, 0};
        return handle.id;
    }
    return begin_drag(d, s, cx, cy);
}

/// THE OBJECT UNDER A WORKSPACE CELL, AND NOTHING ELSE -- `take_hold`'s pure half.
// WL-CTX-01 -- agents/workshop/contextual.md
inline std::int64_t object_at(const WorkshopDoc& d, const Session& s, std::int64_t cx,
                              std::int64_t cy) {
    // The scene must outlive the answer read from it -- `hit` returns a pointer into it
    // (`begin_drag`'s own spelling).
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* under = ui::hit(scene, cx, cy);
    return under == nullptr ? 0 : under->id;
}

/// Where the gesture in flight now proposes the object should BE, or how big it
/// should be — committed through the document's one position operation or its one
/// size operation.
// WL-DOC-09 -- agents/workshop/document.md
inline Handled drag_to(WorkshopDoc& d, const Session& s, std::int64_t cx, std::int64_t cy) {
    if (!s.drag.active) {
        return Handled::of(Written::no("nothing is being dragged"));
    }
    const ui::Scene scene = workspace_scene(d, s);
    if (s.drag.resizing) {
        // The object's RESOLVED left/top edge, because the pointer and the
        // handle are both in workspace cells. Reading the authored `e->x` would
        // ask for a size measured from the wrong corner the moment the object
        // had a context -- the two are the same number only at the root.
        const ui::Placed* placed = ui::placed_for(scene, s.drag.id);
        if (placed == nullptr) {
            return Handled::of(Written::no("no such object"));
        }
        return size_to(d, s, s.drag.id, detail::minus(cx, placed->rect.x),
                       detail::minus(cy, placed->rect.y));
    }
    return place(d, scene, s.drag.id, detail::minus(cx, s.drag.grab_dx),
                 detail::minus(cy, s.drag.grab_dy));
}

inline void end_drag(Session& s) { s.drag = Drag{}; }

// ---- Where a pointer is, in workspace cells --------------------------------------------


/// WHERE A POINTER LANDED INSIDE A BOUNDED TEXT REGION, in that region's own prose.
// WL-INFO-04 -- agents/workshop/info-body.md
struct ProseAt {
    bool understood = false;
    std::int64_t column = 0;
    std::int64_t row = 0;
};

inline ProseAt prose_at(std::int64_t space, std::int64_t x, std::int64_t y,
                        std::int64_t region_x, std::int64_t region_y,
                        const surface::RegionFit& fit) noexcept {
    if (space == input::space::kPixels) {
        return ProseAt{true, surface::prose_column_of_pixel(x, region_x, fit),
                       surface::prose_row_of_pixel(y, region_y, fit)};
    }
    if (space == input::space::kCells) {
        const surface::CanvasPoint at = surface::canvas_of_terminal_cells(x, y);
        return ProseAt{true, surface::sub_px(at.x, region_x), surface::sub_px(at.y, region_y)};
    }
    return ProseAt{};
}

/// The workspace cell a CANVAS cell lands on -- Workshop's own composition, and
/// nothing else.
// WL-GEO-06 -- agents/workshop/geometry.md
inline std::int64_t workspace_cell_x(std::int64_t canvas_x) noexcept {
    return detail::minus(canvas_x, kWorkspaceX);
}
inline std::int64_t workspace_cell_y(std::int64_t canvas_y) noexcept {
    return detail::minus(canvas_y, kWorkspaceY);
}

// ---- What the OBJECTS panel can show, and what it must SAY it cannot ---------------------
// WL-INFO-03 -- agents/workshop/info-body.md

/// Which members of an ordered collection a bounded place is showing, and how
/// many it is leaving out on each side of them.
// WL-INFO-03 -- agents/workshop/info-body.md
struct ListWindow {
    std::size_t first = 0;  ///< the first member shown, as a position in the collection's order
    std::size_t count = 0;  ///< how many are shown, contiguously, in that order
    std::size_t before = 0; ///< how many the place left out ahead of them
    std::size_t after = 0;  ///< how many it left out behind them
};

/// What `rows` lines can honestly show of `total` members while the
/// `selected_at`'th is selected.
// WL-EDIT-10 -- agents/workshop/editor.md
// WL-INFO-03 -- agents/workshop/info-body.md
// WL-TAB-08 -- agents/workshop/tab-run.md
inline ListWindow list_window(std::size_t total, std::size_t selected_at, std::size_t rows) {
    ListWindow w;
    if (total == 0 || rows == 0) {
        w.after = total; // no room at all: everything there is, is missing
        return w;
    }
    if (total <= rows) {
        w.count = total; // rule 1 -- and this is the only case a small document takes
        return w;
    }
    if (rows < 3) {
        // Too few lines to seat one object between two markers, so no window can
        // obey rules 2 and 3 together. It spends what it has on the omission,
        // because the one thing this panel may not do is drop objects quietly.
        //
        // IT WAS UNREACHABLE AT `kListRows = 5` AND IT IS REACHABLE NOW. A share of
        // one or two rows is what a short panel gives a list whose population wants more,
        // so a body of three or four prose rows lands here -- and what a maker then reads is
        // `... 20 more` where the names would be, which is the honest answer: this place
        // cannot show you an object AND tell you what it is hiding, so it tells you.
        w.after = total;
        return w;
    }
    if (selected_at >= total) {
        selected_at = 0; // nothing selected, or a selection that outlived its object
    }
    // One marker's worth of room. Both single-marker windows are this wide, and
    // both leave a non-empty count because `total > rows`.
    const std::size_t one_marker = rows - 1;
    if (selected_at < one_marker) {
        w.count = one_marker;
        w.after = total - w.count;
        return w;
    }
    const std::size_t tail = total - one_marker;
    if (selected_at >= tail) {
        w.first = tail;
        w.count = one_marker;
        w.before = tail;
        return w;
    }
    // The selection is far enough from both ends that both walls are real, so
    // both are said. `first` is the earliest run that reaches the selection,
    // which is at least 2 here (selected_at >= rows - 1 and count == rows - 2),
    // so neither subtraction can leave the number line.
    w.count = rows - 2;
    w.first = selected_at + 1 - w.count;
    w.before = w.first;
    w.after = total - w.first - w.count;
    return w;
}

/// What one omission marker says. `... 2 earlier` / `... 4 more`: a count and a direction.
// WL-INFO-03 -- agents/workshop/info-body.md
inline std::string omitted_text(std::size_t how_many, const char* which) {
    return "... " + std::to_string(how_many) + " " + which;
}

// ---- Rendering one participant's record ------------------------------------------------

/// WHERE A SUBMITTED MESSAGE WAS ADDRESSED, in the SAME three sigils the command line reads.
// WL-TERM-02 -- agents/workshop/terminal.md
inline std::string terminal_address(const loom::TranscriptEntry& e) {
    switch (e.addressing) {
    case loom::Addressing::Weave: return "#" + std::to_string(e.target.value);
    case loom::Addressing::Role: return "@" + e.role;
    case loom::Addressing::Publish: return "* (" + std::to_string(e.recipients) + " queued)";
    }
    return "?";
}

inline std::string terminal_shape(const loom::TranscriptEntry& e) {
    return e.shape + " v" + std::to_string(e.version);
}

/// ONE TRANSCRIPT ENTRY AS ONE LINE.
// WL-TERM-07 -- agents/workshop/terminal.md
inline std::string terminal_line(const loom::TranscriptEntry& e) {
    switch (e.kind) {
    case loom::TranscriptKind::LocalCommand: return "> " + e.text;
    case loom::TranscriptKind::LocalRefusal: return "!! " + e.text;
    case loom::TranscriptKind::LocalNotice: return "-- " + e.text;
    case loom::TranscriptKind::Submitted:
        return "^ " + terminal_shape(e) + " -> " + terminal_address(e) + "  SUBMITTED";
    case loom::TranscriptKind::Received:
        return "v " + terminal_shape(e) + " from #" + std::to_string(e.sender.value);
    case loom::TranscriptKind::AnswerReceived:
        return "v " + terminal_shape(e) + " from #" + std::to_string(e.sender.value) +
               "  [Loom: answers ask " + std::to_string(e.answers) + "]";
    }
    return e.text;
}

/// WHAT `^` MEANS, said once, on a row the pane always shows.
// WL-TERM-07 -- agents/workshop/terminal.md
inline std::string terminal_legend() {
    return "SUBMITTED = authored; a sender is not told its fate";
}

/// ONE TRANSCRIPT ENTRY AS THE ROWS A PANE THIS WIDE SPENDS ON IT.
// WL-TERM-07 -- agents/workshop/terminal.md
inline std::vector<std::string> terminal_wrapped(const loom::TranscriptEntry& e,
                                                 std::int64_t width) {
    return detail::wrap(terminal_line(e), width);
}

/// HOW MANY OF THE NEWEST ENTRIES A PANE THIS WIDE AND THIS TALL CAN SHOW WHOLE.
// WL-TERM-03 -- agents/workshop/terminal.md
inline std::size_t entries_that_fit(const std::vector<loom::TranscriptEntry>& entries,
                                    std::int64_t width, std::size_t rows) {
    std::size_t taken = 0;
    std::size_t used = 0;
    for (std::size_t i = entries.size(); i > 0; --i) {
        const std::size_t cost = terminal_wrapped(entries[i - 1], width).size();
        if (taken > 0 && used + cost > rows) {
            break;
        }
        used += cost;
        ++taken;
        if (used >= rows) {
            break;
        }
    }
    return taken;
}

/// WHAT THE PANE IS NOT SHOWING, in two numbers that are two different facts.
// WL-TERM-03 -- agents/workshop/terminal.md
inline std::string terminal_omission(const TerminalPane& t) {
    if (t.earlier == 0 && t.dropped == 0) {
        return "[the whole of this session's record is on screen]";
    }
    std::string text = "... " + std::to_string(t.earlier) + " earlier";
    if (t.dropped > 0) {
        text += ", " + std::to_string(t.dropped) + " dropped for good";
    }
    return text;
}

// ---- The editable line, resolved ONCE ---------------------------------------------------

/// THE PROMPT, in columns: the `> ` before the editable text.
// WL-TERM-09 -- agents/workshop/terminal.md
inline constexpr std::int64_t kTerminalPromptCols = 2;

/// THE COLUMN THE INSERTION POINT SITS IN, kept out of the editable line's own budget.
// WL-TERM-09 -- agents/workshop/terminal.md; WL-TEXT-05 -- agents/workshop/text-box.md
inline constexpr std::int64_t kTerminalCaretCols = 1;

/// WHERE THE PANE'S EDITABLE LINE IS — the pane's region, the row inside it, and the column
/// its first byte starts at.
// WL-TERM-09 -- agents/workshop/terminal.md
struct TerminalInputPlace {
    std::int64_t region_x = 0; ///< the pane's own cell origin — a region coordinate
    std::int64_t region_y = 0;
    surface::RegionFit fit{};
    std::int64_t prose_row = 0;   ///< the pane's LAST prose row: the line being typed
    std::int64_t first_column = kTerminalPromptCols; ///< where the line's first byte sits
    /// COLUMNS THE VISIBLE PART OF THE LINE MAY OCCUPY — prompt excluded, and the
    /// caret's own column excluded too.
    // WL-TERM-09 -- agents/workshop/terminal.md; WL-TEXT-04, WL-TEXT-05 -- agents/workshop/text-box.md
    std::int64_t columns = 0;
};

inline constexpr TerminalInputPlace terminal_input_place(const Screen& sc) noexcept {
    TerminalInputPlace p;
    p.region_x = sc.terminal_x;
    p.region_y = sc.terminal_y;
    p.fit = surface::fit_region(sc.terminal_x, sc.terminal_y, sc.terminal_w, sc.terminal_h,
                                sc.text_advance_px, sc.text_line_px);
    p.prose_row = static_cast<std::int64_t>(sc.terminal_lines) - 1;
    p.columns = sc.terminal_cols - kTerminalPromptCols - kTerminalCaretCols;
    if (p.columns < 0) {
        p.columns = 0; // a pane too narrow for its own prompt shows no line, and says so
    }
    return p;
}

/// THE PROSE COLUMN THE CARET SITS AT, on the row this pane draws the line on.
// WL-TEXT-13 -- agents/workshop/text-box.md
inline std::int64_t terminal_caret_column(const TerminalInputPlace& p,
                                          const component::TextBox& box) noexcept {
    return surface::add_cells(p.first_column, static_cast<std::int64_t>(box.caret_column()));
}

/// THE BYTE INDEX A PROSE COLUMN NAMES, clamped into the line the pane is showing.
// WL-TEXT-13 -- agents/workshop/text-box.md
inline std::size_t terminal_caret_of_column(const TerminalInputPlace& p,
                                            const component::TextBox& box,
                                            std::int64_t column) noexcept {
    return box.position_at_column(surface::sub_px(column, p.first_column));
}

/// A PROSE COLUMN AS A COLUMN OF THE LINE ITSELF — the prompt taken off and NOTHING
/// clamped, which is what a selection DRAG needs and a press does not.
// WL-TEXT-14 -- agents/workshop/text-box.md
inline constexpr std::int64_t terminal_value_column(const TerminalInputPlace& p,
                                                    std::int64_t column) noexcept {
    return surface::sub_px(column, p.first_column);
}

/// THE VISIBLE SELECTION AS PROSE COLUMNS OF THE PANE'S ROW — the prompt added to
/// the component's own answer, `terminal_caret_column`'s shape for a span.
// WL-TEXT-13 -- agents/workshop/text-box.md
struct TerminalSelectionSpan {
    std::int64_t begin = 0;
    std::int64_t end = 0;
    bool present = false;
};
inline TerminalSelectionSpan terminal_selection_columns(const TerminalInputPlace& p,
                                                        const component::TextBox& box) noexcept {
    const component::TextBox::VisibleSpan vis = box.visible_selection(p.columns);
    if (!vis.present()) {
        return TerminalSelectionSpan{};
    }
    return TerminalSelectionSpan{surface::add_cells(p.first_column, vis.begin),
                                 surface::add_cells(p.first_column, vis.end), true};
}

/// IS THIS PROSE POSITION ON THE EDITABLE LINE AT ALL?
// WL-PRESS-03 -- agents/workshop/press-chain.md; WL-TERM-09 -- agents/workshop/terminal.md
inline constexpr bool terminal_input_hit(const TerminalInputPlace& p, std::int64_t column,
                                         std::int64_t row) noexcept {
    return row == p.prose_row && column >= 0 && column <= p.fit.columns;
}

// ---- The completion list, inside the pane it belongs to ---------------------------------

/// WHERE THE COMPLETION LIST SITS, in canvas cells, and how much prose it holds.
// WL-TERM-06 -- agents/workshop/terminal.md
struct CompletionPlace {
    std::int64_t x = 0; ///< canvas cells, exactly like every other placement here
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::size_t rows = 0; ///< prose rows the list can actually show, heading included
    bool visible = false;
};

/// The cell row, relative to the pane's top, that the pane's prose row `n` begins on.
///
/// With no metric a prose row IS a cell row and this is the identity -- which is what makes
/// the whole arrangement fall back to the arithmetic every terminal golden already holds.
inline constexpr std::int64_t pane_prose_top_cell(const Screen& sc, std::int64_t prose_row) noexcept {
    if (sc.text_advance_px <= 0 || sc.text_line_px <= 0) {
        return prose_row > 0 ? prose_row : 0;
    }
    const std::int64_t top =
        surface::add_cells(surface::kTextInsetPx, surface::mul_px(prose_row, sc.text_line_px));
    return surface::floor_div_px(top, surface::kCanvasCellPx);
}

/// The list is at least this tall in prose rows before it is worth showing at all — and it
/// is ONE, because a heading with nothing under it is a complete answer rather than an
/// empty box.
// WL-TERM-05 -- agents/workshop/terminal.md
inline constexpr std::size_t kCompletionMinRows = 1;

/// How much of the pane the list may take. The pane is a record a maker is reading and a
/// line they are writing; a list that grew to fill it would answer the second question by
/// erasing the first. Half, rounded down, is the same share rule the pane itself takes from
/// a growing screen.
inline constexpr CompletionPlace completion_place(const Screen& sc, std::size_t wanted) noexcept {
    CompletionPlace p;
    if (wanted == 0 || sc.terminal_lines < kTerminalChrome + 1) {
        return p;
    }
    // The list ends where the pane's second-from-last prose row begins -- the omission
    // marker's row -- so the marker and the input line below it are never covered.
    const std::int64_t bottom_cell =
        pane_prose_top_cell(sc, static_cast<std::int64_t>(sc.terminal_lines) - 2);
    // ...and starts no higher than the pane's second cell row, so the header naming the
    // identity whose record this is stays visible whatever is being typed.
    const std::int64_t highest = 1;
    if (bottom_cell <= highest) {
        return p; // the pane has no room between its own two ends
    }
    const std::int64_t room_cells = bottom_cell - highest;
    const std::int64_t half = room_cells / 2 > 0 ? room_cells / 2 : 1;
    // Cells enough for `wanted` prose rows, in whichever lattice this medium has.
    const std::int64_t want_cells =
        (sc.text_advance_px <= 0 || sc.text_line_px <= 0)
            ? static_cast<std::int64_t>(wanted)
            : surface::floor_div_px(
                  surface::add_cells(surface::mul_px(static_cast<std::int64_t>(wanted),
                                                     sc.text_line_px),
                                     2 * surface::kTextInsetPx + surface::kCanvasCellPx - 1),
                  surface::kCanvasCellPx);
    const std::int64_t h = want_cells < half ? want_cells : half;
    if (h <= 0) {
        return p;
    }
    p.x = sc.terminal_x;
    p.w = sc.terminal_w;
    p.h = h;
    p.y = surface::add_cells(sc.terminal_y, bottom_cell - h);
    const surface::RegionFit fit =
        surface::fit_region(p.x, p.y, p.w, p.h, sc.text_advance_px, sc.text_line_px);
    p.rows = fit.rows > 0 ? static_cast<std::size_t>(fit.rows) : 0;
    p.visible = p.rows >= kCompletionMinRows;
    return p;
}

/// WHICH CANDIDATE THE FIRST VISIBLE ROW SHOWS — the windowing, written once.
// WL-TERM-05, WL-TERM-06 -- agents/workshop/terminal.md
// WL-GEO-01 -- agents/workshop/geometry.md
// WL-INFO-03 -- agents/workshop/info-body.md
inline constexpr std::size_t completion_first_shown(std::size_t selected,
                                                    std::size_t capacity) noexcept {
    if (capacity <= 1) {
        return 0; // no room for a candidate row at all: the heading is the whole list
    }
    const std::size_t room = capacity - 1; // the heading always costs one
    return selected >= room ? selected - room + 1 : 0;
}

/// THE LIST AS ROWS -- heading first, then as many candidates as the place holds, with the
/// selected one marked. Windowed around the selection, and the heading says which slice it
/// is showing; the `>` marker says which one on a medium with no colour at all.
// WL-TERM-05, WL-TERM-06 -- agents/workshop/terminal.md
inline std::vector<surface::SurfaceTextRow> completion_rows(const Completion& comp,
                                                            std::size_t capacity,
                                                            std::int64_t width) {
    std::vector<surface::SurfaceTextRow> rows;
    if (capacity == 0) {
        return rows;
    }
    const std::size_t room = capacity - 1; // the heading always costs one
    const std::size_t first = completion_first_shown(comp.selected, capacity);
    const std::size_t last = comp.candidates.size() < first + room ? comp.candidates.size()
                                                                   : first + room;
    std::string heading = comp.heading;
    if (comp.candidates.size() > room) {
        // WHICH SLICE, SAID OUT LOUD -- including the slice that is nothing at all, which is
        // what a pane too short for a single candidate row shows. "none of 5" is a worse
        // picture than five rows and a far better sentence than five rows' worth of silence.
        heading = (room == 0 ? std::string("none")
                             : std::to_string(first + 1) + "-" + std::to_string(last)) +
                  " of " + std::to_string(comp.candidates.size()) + "  " + heading;
    }
    rows.push_back(surface::SurfaceTextRow{detail::fit(heading, width), surface::role::kMuted,
                                           surface::role::kNone});
    for (std::size_t i = first; i < last; ++i) {
        const Candidate& c = comp.candidates[i];
        const bool chosen = i == comp.selected;
        std::string text = (chosen ? "> " : "  ") + c.display;
        if (!c.detail.empty()) {
            // The detail is what a candidate MEANS, and it is the first thing a narrow pane
            // gives up: `detail::fit` cuts the whole row, so a list in a small window shows
            // names and a list in a large one shows names and meanings.
            text += "   " + c.detail;
        }
        rows.push_back(surface::SurfaceTextRow{
            detail::fit(text, width), chosen ? surface::role::kAccent : surface::role::kFill,
            chosen ? surface::role::kMuted : surface::role::kNone});
    }
    return rows;
}

/// The overlay, painted OVER the finished screen.
// WL-TERM-03, WL-TERM-07 -- agents/workshop/terminal.md; WL-GEO-01 -- agents/workshop/geometry.md
inline void paint_terminal(surface::SurfaceLayer& layer, const TerminalPane& t,
                           const Screen& sc, const Keymap& keymap) {
    if (!t.open) {
        return;
    }
    layer.rects.push_back(surface::SurfaceRect{sc.terminal_x, sc.terminal_y, sc.terminal_w,
                                           sc.terminal_h, surface::role::kMuted});

    surface::SurfaceTextRegion pane;
    pane.x = sc.terminal_x;
    pane.y = sc.terminal_y;
    pane.w = sc.terminal_w;
    pane.h = sc.terminal_h;
    pane.rows.resize(sc.terminal_lines);
    const auto row = [&pane, &sc](std::size_t line, const std::string& text, std::int64_t role) {
        if (line >= pane.rows.size()) {
            return; // the pane is smaller than its own chrome: the floor already refused this
        }
        pane.rows[line] = surface::SurfaceTextRow{detail::fit(text, sc.terminal_cols), role};
    };

    // The header NAMES THE IDENTITY whose record this is. A presentation may hold controls for
    // more than one identity, and the moment it stops saying which one it is showing is the
    // moment the two look like one thing with two windows.
    row(0,
        t.attached ? "TERMINAL -- weave #" + std::to_string(t.id.value) + "  (" +
                         hotkey_text(keymap, Act::kTerminalToggle) + " closes)"
                   : "TERMINAL -- no participant was mounted on this bus",
        surface::role::kAccent);

    row(1, terminal_legend(), surface::role::kMuted);

    // THE TRANSCRIPT, WRAPPED -- one entry becomes as many rows as its sentence needs, and the
    // pane is a list of ROWS from here down rather than a list of entries. `refresh_terminal`
    // chose `shown` with the same arithmetic (`entries_that_fit`) against the same
    // `terminal_cols`, so this loop is where that choice is CARRIED OUT rather than where it is
    // made; the truncation below can only fire for a single entry taller than the whole pane,
    // which is the case that function names.
    std::vector<std::string> lines;
    for (const loom::TranscriptEntry& e : t.shown) {
        for (std::string& line : terminal_wrapped(e, sc.terminal_cols)) {
            lines.push_back(std::move(line));
        }
    }
    if (lines.size() > sc.terminal_rows) {
        lines.resize(sc.terminal_rows);
    }

    for (std::size_t i = 0; i < sc.terminal_rows; ++i) {
        row(2 + i, i < lines.size() ? lines[i] : std::string(), surface::role::kFill);
    }
    row(sc.terminal_lines - 2, terminal_omission(t), surface::role::kMuted);
    // THE LINE BEING TYPED, AND THE CARET SAID SEPARATELY FROM IT.
    //
    // Earlier the caret was a `_` this function appended, which was truthful only because
    // the caret could only ever be at the end. It can be anywhere now, so the position is
    // published as a fact ABOUT the region (`caret_row`/`caret_col`) and each medium answers
    // it in its own type: a window fills a bar between two characters, and the cell
    // projection inserts `_` at the same column -- which, for a caret at the end of the line,
    // is byte-for-byte the row this function used to write itself.
    //
    // AND WHILE THERE IS NOTHING ON IT, IT NAMES THE GESTURE THAT ANSWERS "what can I
    // say here". It is on this row rather than in the legend because it is
    // about what to do NEXT rather than about what a word means, and because it
    // erases itself: the moment a maker types anything the line has their text on it
    // and the list is doing the same job better. A tool whose discovery gesture is
    // itself undiscoverable has moved the problem rather than solved it.
    //
    // AND IT IS A WINDOW ONTO THE LINE RATHER THAN THE WHOLE OF IT. `visible` is the
    // slice the row has room for; the authored command is untouched behind it, and nothing
    // in the row says how much is off either side -- there is no marker, no arrow and no
    // ellipsis, because the caret staying put is what tells a maker the line moved and an
    // indicator would be a second thing to keep true. Note that `detail::fit`'s `...` can no
    // longer fire on this row: the slice is at most `columns` and the prompt is exactly the
    // difference, so the row is always short enough. That marker's job here has been taken
    // over by a window a maker can move.
    const TerminalInputPlace typing = terminal_input_place(sc);
    const bool prompting = t.input.empty() && !t.completion.open;
    row(sc.terminal_lines - 1,
        prompting ? ">    " + hotkey_text(keymap, Act::kTerminalComplete) +
                        ": what can this terminal say?"
                  : "> " + t.input.visible(typing.columns),
        t.attached ? surface::role::kAccent : surface::role::kAlert);
    // ONE MEASURER: the column comes from the same resolution the row was written against,
    // and the same one a press is answered with, so a caret cannot land where the text is
    // not and a click cannot land where the caret would not. that resolution
    // includes WHICH PART of the line is on the row, and all three read the one answer
    // `TerminalInput` holds rather than each deciding for itself.
    pane.caret_row = typing.prose_row;
    pane.caret_col = terminal_caret_column(typing, t.input);
    // AND THE SELECTION, THE SAME WAY: the visible part of the component's own
    // range, prompt-shifted by the same helper family the caret goes through, published as
    // the region's selection so each medium answers in its own voice — reverse video in a
    // cell, a band under the glyphs in a window. A selection scrolled wholly off the slice
    // publishes nothing, which is the truthful picture of a row that shows none of it.
    const TerminalSelectionSpan marked = terminal_selection_columns(typing, t.input);
    if (marked.present) {
        pane.sel_begin_row = typing.prose_row;
        pane.sel_begin_col = marked.begin;
        pane.sel_end_row = typing.prose_row;
        pane.sel_end_col = marked.end;
    }

    layer.texts.push_back(std::move(pane));

    // THE COMPLETION LIST, LAST, SO IT IS ON TOP OF THE PANE IT BELONGS TO. Painter's order
    // across `texts` is list order, the same rule every other list on a canvas already
    // states, so "the list covers the transcript" needs no z-order and no framework -- it
    // needs the push to come second.
    //
    // AND ONE MEASURER, AGAIN. `completion_place` decides how many rows there are and
    // `completion_rows` fills exactly that many; nothing upstream was told a number it could
    // disagree with, which is why the list can say "3-5 of 9" and be right.
    if (!t.completion.open || t.dismissed) {
        return;
    }
    const CompletionPlace place =
        completion_place(sc, t.completion.candidates.size() + 1 /*the heading*/);
    if (!place.visible) {
        return; // a pane too small to hold a heading and a candidate shows neither
    }
    surface::SurfaceTextRegion list;
    list.x = place.x;
    list.y = place.y;
    list.w = place.w;
    list.h = place.h;
    list.rows = completion_rows(t.completion, place.rows, sc.terminal_cols);
    layer.texts.push_back(std::move(list));
}

// ---- The dynamic panels, painted -------------------------------------------------------
// WL-CHROME-04 -- agents/workshop/chrome.md; WL-PANE-05 -- agents/workshop/panes-and-windows.md

/// THE BACKDROP OF A PANEL: its whole bounds, in one rect.
// WL-CHROME-04 -- agents/workshop/chrome.md; WL-PANE-05 -- agents/workshop/panes-and-windows.md
inline void paint_panel_frame(surface::SurfaceLayer& layer, const FineRect& b,
                              std::int64_t role) {
    layer.rects.push_back(wire_rect_of(b, role));
}

/// A PANEL WHOSE WHOLE BODY IS ONE BOUNDED REGION OF PROSE, RESOLVED ONCE.
// WL-CHROME-05 -- agents/workshop/chrome.md; WL-RGN-01 -- agents/workshop/regions.md
struct PanelProsePlace {
    bool present = false;
    std::int64_t rows = 0;    ///< prose rows of the ACTIVE medium's type that fit the panel
    std::int64_t columns = 0; ///< ...and how many characters fit across one of them
    /// THE RESOLUTION ITSELF, carried so a press inverse over this surface spends the fit
    /// the painter was handed rather than resolving the same rectangle a second time --
    /// `ExternalBodyPlace`'s own field, for `ExternalBodyPlace`'s own reason.
    surface::RegionFit fit{};
    /// THE INTERIOR THE FIT WAS RESOLVED FOR, so the region a painter publishes is
    /// built from the rectangle that was actually measured rather than from a second
    /// subtraction beside it. How thick this surface's chrome was is `chrome_subs`.
    FineRect inside{};
    std::int64_t chrome_subs = 0;
};

/// The one call. TOTAL over the rectangle, because a closed panel answers with an empty one
/// (`bounds_of`) and a screen may be small enough to hold no row at all. Fine bounds fit at
/// their fine place — the same sub-unit entry the medium resolves the same rectangle with.
// WL-CHROME-05 -- agents/workshop/chrome.md; WL-RGN-01 -- agents/workshop/regions.md
inline PanelProsePlace panel_prose_place(const FineRect& b, const Screen& sc) {
    PanelProsePlace p;
    const PaneInside inside = pane_inside(b, sc);
    p.inside = inside.rect;
    p.chrome_subs = inside.chrome_subs;
    if (inside.rect.w <= 0 || inside.rect.h <= 0) {
        return p;
    }
    p.fit = inside.fit;
    p.rows = p.fit.rows;
    p.columns = p.fit.columns;
    p.present = p.rows > 0 && p.columns > 0;
    return p;
}

/// The region a `PanelProsePlace` was resolved for, empty and ready for its rows — the fine
/// bounds decomposed onto the wire's cells-plus-remainder spelling.
// WL-RGN-01 -- agents/workshop/regions.md
inline surface::SurfaceTextRegion panel_prose_region(const PanelProsePlace& place) {
    surface::SurfaceTextRegion region;
    const surface::SurfaceRect wire = wire_rect_of(place.inside, surface::role::kFill);
    region.x = wire.x;
    region.y = wire.y;
    region.w = wire.w;
    region.h = wire.h;
    region.sub_x = wire.sub_x;
    region.sub_y = wire.sub_y;
    region.sub_w = wire.sub_w;
    region.sub_h = wire.sub_h;
    return region;
}

/// A panel's own field: a fixed-width label and its value, so the values line up down the
/// panel and a maker reads a column rather than a paragraph.
inline std::string panel_field(const char* label, const std::string& value) {
    return detail::pad(label, 9) + value;
}

/// A field whose value is longer than a row: wrapped across a fixed row budget, and MARKED
/// when the budget ran out before the sentence did.
// WL-RGN-02 -- agents/workshop/regions.md
inline std::vector<std::string> panel_block(const char* label, const std::string& value,
                                            std::size_t rows, std::int64_t width) {
    std::vector<std::string> lines = detail::wrap(panel_field(label, value), width);
    if (lines.size() > rows) {
        lines.resize(rows);
        lines.back() = detail::fit(lines.back() + " " + detail::kElided, width);
    }
    while (lines.size() < rows) {
        lines.push_back(std::string());
    }
    return lines;
}

/// THE BUILDER PANEL — Workshop's presentation of a weave it does not own.
// WL-RGN-02 -- agents/workshop/regions.md
inline void paint_builder(surface::SurfaceLayer& layer, const BuilderPane& pane,
                          const FineRect& b, const Screen& sc,
                          const ProjectFrontier& frontier = {},
                          const std::string& catalog_moved_to = std::string(),
                          std::int64_t chrome = kPaneChrome) {
    paint_panel_frame(layer, b, chrome);
    // THE PANEL IS ONE REGION AND ITS ROWS ARE COMPOSED AGAINST THE BUDGET. The
    // Builder was the last consumer of the cell-lattice row spelling, and the recorded
    // reason was never typography: nine facts do not fit five rows, and until the panel
    // had a COMPOSITION PRIORITY there was no honest way to choose which five. The
    // priority is written below, on each fact, and the rule is:
    //
    //     what survives longest is what a maker is ACTING on -- the office's identity,
    //     the live build's activity/result, the frontier the project is waiting on, what
    //     `b` will build next, the realization outcome, the compiler's own words --
    //     and what yields first is static metadata (the exit code's row, the command
    //     echo) and the tail of the output block.
    //
    // The DISPLAY order never changes with the budget: a shorter face shows the same
    // rows in the same order minus the ones that did not fit, so growing the window
    // reveals more truth rather than switching to a different panel. A character medium's
    // nine-row budget selects every fact, byte-for-byte the composition this panel has
    // painted.
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // no room for one row of this medium's type: say nothing at all
    }
    const std::int64_t columns = place.columns;
    struct Fact {
        std::string text;
        std::int64_t role = surface::role::kFill;
        std::int64_t priority = 0; ///< smaller survives longer; all distinct
    };
    std::vector<Fact> facts; // display order, priorities deciding survival

    // THE HEADER NAMES THE OFFICE IT IS PRESENTING, AND NOTHING ELSE. The same
    // discipline the terminal pane's header follows: a presentation that shows somebody
    // else's facts without saying whose is a presentation that will eventually be read as
    // its own.
    //
    // ITS FOUR SHORTCUTS ARE GONE. `b`/`B`, `c`, `f` and the picker's removal key are
    // ordinary `kActionCatalog` rows in command mode, so the band's legend and the full
    // hotkey view already say every one of them -- in the maker's own bindings -- and this
    // pane was spending a third of its widest row restating them. The keymap made the claims
    // truthful; this makes them singular. Project Files reached the same answer first
    // ("THE GESTURES ARE NOT PAINTED HERE") and the argument is the same one: the pane
    // spends its rows on the project rather than on instructions.
    facts.push_back(Fact{std::string("BUILDER @") + builder::kBuilderRole,
                         surface::role::kAccent, 0});

    const auto publish = [&](std::vector<Fact> chosen, const std::string& said_detail) {
        // KEEP WHAT THE BUDGET SEATS, IN DISPLAY ORDER. The priorities are distinct, so
        // "the `place.rows` smallest" is one threshold; the said block is wrapped LAST,
        // into exactly the rows that survived, so its elision mark tells the truth about
        // this budget rather than about the nine-row one. A dropped fact is dropped WHOLE
        // -- nothing substitutes for it, and the rows that remain neither move nor reword.
        std::vector<std::int64_t> priorities;
        priorities.reserve(chosen.size());
        for (const Fact& f : chosen) {
            priorities.push_back(f.priority);
        }
        std::sort(priorities.begin(), priorities.end());
        const std::size_t seats = static_cast<std::size_t>(place.rows);
        const std::int64_t cut =
            priorities.size() > seats ? priorities[seats] : priorities.back() + 1;
        std::size_t said_kept = 0;
        for (const Fact& f : chosen) {
            if (f.priority < cut && f.text.empty()) {
                ++said_kept; // a said placeholder: counted now, written below
            }
        }
        std::vector<std::string> said;
        if (said_kept > 0) {
            said = panel_block("said", said_detail.empty() ? std::string("--") : said_detail,
                               said_kept, columns);
        }
        surface::SurfaceTextRegion region = panel_prose_region(place);
        std::size_t said_at = 0;
        for (Fact& f : chosen) {
            if (f.priority >= cut) {
                continue;
            }
            std::string text = f.text.empty() ? said[said_at++] : std::move(f.text);
            region.rows.push_back(surface::SurfaceTextRow{
                detail::fit(std::move(text), columns), f.role});
        }
        layer.texts.push_back(std::move(region));
    };

    if (!pane.heard) {
        // NOT THE SAME AS "NEVER BUILT", and the panel must not show it as though it were.
        // This is a fact about this panel -- it has asked and is waiting -- and the recipe's
        // own history is not knowable from here until the tool says it.
        facts.push_back(Fact{panel_field("recipe", "(the Builder has not answered yet)"),
                             surface::role::kMuted, 1});
        publish(std::move(facts), std::string());
        return;
    }

    const builder::BuildStatus& s = pane.shown;
    // WHAT THE MAKER HAS PICKED OUT, AND HOW MANY THERE ARE TO PICK FROM.
    //
    // IT IS THE CHOICE AND NOT THE LAST BUILD, and when they differ the choice is the
    // truer row: it is what `b` will do next, which is the question a maker looking at a
    // Builder panel is actually asking. What the last build was about is on the rows
    // below it, where an outcome belongs.
    //
    // AN EMPTY CATALOG IS SAID PLAINLY. A project may hold no recipes -- there is no
    // recipes file, or it names none -- and a panel that showed a blank name would look
    // like one that had not heard yet, which is the distinction `heard` exists to keep.
    const std::size_t held = pane.known.recipes.size();
    if (held == 0) {
        facts.push_back(Fact{panel_field("recipe", "(this project has no build recipes)"),
                             surface::role::kMuted, 3});
    } else {
        const std::size_t at = pane.chosen < held ? pane.chosen : std::size_t{0};
        facts.push_back(
            Fact{panel_field("recipe", pane.known.recipes[at].recipe + " -> " +
                                           pane.known.recipes[at].artifact + "  (" +
                                           std::to_string(at + 1) + "/" +
                                           std::to_string(held) + ")"),
                 surface::role::kFill, 3});
    }
    // ---- WHICH AUTHORED CATALOG THIS SESSION MOVED TO, WHILE IT HAS -------------------
    //
    // THE ROW EXISTS EXACTLY WHILE THE FACT HAS MOVED, which is the `project` row's rule
    // and is taken for the same reason: this panel is exactly full. Nine facts and nine
    // rows of a character medium, measured -- so a tenth UNCONDITIONAL row would spend the
    // third `said` row of every session, including every session that never changes a
    // catalog, to restate what the host's own banner already said correctly at launch.
    // What a replacement changes is that the banner STOPS being true, and that is the
    // moment this row appears. It costs one `said` row while it holds (`shift`, below),
    // and its priority puts it last, so a face whose budget seats five keeps the same five
    // rows it seated before this phase.
    //
    // THE PATH IS ABSOLUTE, AND IT IS CUT BY THE MEASURER THAT KEEPS ITS TAIL. An earlier phase put
    // a project-relative spelling here because the browser could not reach outside the
    // project and an ordinary fit removes a path's filename -- the half that says which
    // catalog this is. Free navigation removed the first half of that premise, so a based spelling
    // with no stated base became a wrong-looking name for the right file, and the answer is
    // the absolute path plus `detail::fit_path`: root cue, a mark where the middle was
    // removed, and the tail intact. Nothing here reformats a path, shortens it to a
    // basename, or widens the panel to hold one.
    const bool moved_catalog = !catalog_moved_to.empty();
    if (moved_catalog) {
        facts.push_back(Fact{panel_field("catalog",
                                         detail::fit_path(catalog_moved_to, columns - 9)),
                             surface::role::kMuted, 10});
    }
    // ---- WHAT THE PROJECT IS WAITING ON, WHILE IT IS ----------------------------------
    //
    // THE ROW EXISTS EXACTLY WHILE THE FRONTIER DOES, and it costs the third `said` row,
    // which is the row this panel can best afford exactly here: a maker whose project is
    // WAITING has no build output yet, and one whose frontier build FAILED still reads two
    // rows of the compiler's ending plus the whole stream on the bus. When nothing is
    // waiting the composition is byte-for-byte the earlier one, because absence of a pending
    // frontier is the whole answer and this panel will not invent a "nothing blocked" to
    // fill a row. Under a constrained budget the row OUTLIVES everything but the header
    // and the live activity row -- it is the actionable pressure this panel exists to
    // surface, and a face that hid it while showing the command echo would be showing the
    // less useful truth.
    //
    // THREE FACTS, ONE ROW, TWO OWNERS. The artifact and the blocked count are the
    // realization owner's, read alive through the host at this paint; which recipes can
    // produce the artifact is the tool's own published catalog, joined here BY STEM --
    // the one edge the catalog allows. One producing recipe is named; several are counted
    // (`f` names them, and `c` shows each beside the artifact it makes); none is said
    // plainly, because a frontier this project cannot produce is a different problem.
    const std::size_t shift = (frontier.waiting ? 1u : 0u) + (moved_catalog ? 1u : 0u);
    if (frontier.waiting) {
        std::size_t makers = 0;
        const builder::RecipeSummary* maker = nullptr;
        for (const builder::RecipeSummary& known : pane.known.recipes) {
            if (known.artifact == frontier.artifact) {
                ++makers;
                maker = &known;
            }
        }
        std::string said = "waiting " + frontier.artifact + " (";
        if (makers == 0) {
            said += "no recipe";
        } else if (makers == 1) {
            said += maker->recipe;
        } else {
            said += std::to_string(makers) + " recipes";
        }
        said += ", blocks " + std::to_string(frontier.blocked) + ")";
        facts.push_back(Fact{panel_field("project", said), surface::role::kAccent, 2});
    }
    // WHAT THIS PANEL IS WATCHING beats what it was last told. `awaiting` is the panel's own
    // fact and it is the truer one while it holds: the tool's last OUTCOME is still the
    // previous build's, and showing that while a new one is running would answer "what
    // happened on the last build" with a sentence about the wrong build.
    //
    // THE OPERATION AND THE OUTPUT COUNT SHARE THIS ROW, and they are on the panel
    // for one reason: they are what make a running build VISIBLE rather than asserted. A
    // maker who presses `b`, moves a rectangle, opens Info and comes back to a Builder that
    // says `running -- op #1, 37 out` has watched Workshop stay alive while a real child
    // process ran, and has watched the count climb while doing it. A build that had frozen
    // the pump could not have produced either number, because nothing would have been
    // delivered to change them. They stay on the row after it ends, so the evidence does not
    // vanish at the moment it becomes a result. It is the LIVE row, so under a constrained
    // budget it outlives everything but the header.
    const bool named_op = s.op != 0;
    const std::string carried =
        named_op ? " -- op #" + std::to_string(s.op) + ", " + std::to_string(s.chunks) + " out"
                 : std::string();
    const bool unanswered = pane.awaiting && s.outcome != builder::outcome::kRunning;
    facts.push_back(
        Fact{unanswered ? panel_field("last", "asked -- waiting for it to start")
                        : panel_field("last",
                                      std::string(builder::name_of_outcome(s.outcome)) +
                                          carried),
             unanswered || s.outcome == builder::outcome::kRunning
                 ? surface::role::kAccent
                 : (s.outcome == builder::outcome::kFailed ||
                            s.outcome == builder::outcome::kNotStarted ||
                            s.outcome == builder::outcome::kNoArtifact ||
                            s.outcome == builder::outcome::kUnknownRecipe
                        ? surface::role::kAlert
                        : surface::role::kFill),
             1});
    // THE EXIT STATUS IS ONLY SHOWN WHEN THERE WAS ONE. A `0` printed after a build that
    // never started reads as success, which is the exact wrong answer at the exact moment a
    // maker most needs the right one.
    //
    // THE TOOL'S OWN COUNTER SHARES THE ROW, and it is on the panel at all because it is the
    // number that proves the tool outlives its presentation: close this panel, reopen it,
    // build again, and it reads 2 -- which a panel that owned the state could not say. It
    // shares rather than taking its own because the rows below are worth more to a maker
    // whose build just failed, and this one has a column to spare. Static metadata: under a
    // constrained budget it yields to every outcome row and to the first `said` row.
    facts.push_back(
        Fact{panel_field("exit", detail::pad(s.outcome == builder::outcome::kSucceeded ||
                                                     s.outcome == builder::outcome::kFailed
                                                 ? std::to_string(s.status)
                                                 : std::string("--"),
                                             11) +
                                     "asks " + std::to_string(s.builds) + " ever"),
             surface::role::kMuted, 6});
    // WHAT WAS ACTUALLY RUN, as the runner reported it. Empty until something has been run,
    // because the tool holds no command and this panel will not invent one to fill a row.
    // The first fact a constrained budget gives up: it is an echo of the maker's own act.
    facts.push_back(Fact{panel_field("ran", s.command.empty()
                                                ? std::string("(nothing has run yet)")
                                                : s.command),
                         surface::role::kMuted, 7});
    // ---- THE SECOND OUTCOME, ON ITS OWN ROW ---------------------------------------------
    //
    // A BUILD OUTCOME AND A REALIZATION OUTCOME ARE TWO ANSWERS AND THIS PANEL SHOWS TWO.
    // The alternative -- one "status" row that says whichever of them is more recent -- is
    // exactly the conflation the Builder's own two fields exist to prevent, and it is worst
    // in the case a maker most needs: a build that WORKED whose realization was REFUSED.
    // The row is present even when nothing was asked, because an absent row reads as an
    // absent question rather than as an unasked one.
    facts.push_back(
        Fact{panel_field("realize",
                         s.realization == builder::realization::kNotAsked
                             ? std::string("-- (B builds and realizes)")
                             : std::string(builder::name_of_realization(s.realization)) +
                                   (s.realized_detail.empty() ? std::string()
                                                              : " -- " + s.realized_detail)),
             s.realization == builder::realization::kRefused
                 ? surface::role::kAlert
                 : (s.realization == builder::realization::kRealized ? surface::role::kFill
                                                                     : surface::role::kMuted),
             4});
    // THREE ROWS FOR WHAT THE BUILD SAID, because this is the row budget a maker spends when
    // something has gone wrong, and one row of a compiler's answer is a row of nothing.
    //
    // THEY TOOK THE FOOTER'S ROW and the footer is gone rather than shortened: it
    // said `[ Build ] press b`, which the header now says beside the two keys the catalog added,
    // and a panel that spends a row of a compiler's answer on repeating its own header is
    // spending the wrong row.
    //
    // ...AND WHILE THE PROJECT IS WAITING, THE `project` ROW HOLDS THE THIRD OF THEM
    //. The trade is argued where the row is painted, above. The rows are
    // PLACEHOLDERS here (empty text, `publish` wraps the detail into exactly the rows that
    // survive the budget, so the elision mark tells the truth about THIS face): the first
    // of them outlives the exit and command rows -- the compiler's own words are what a
    // maker acts on when something went wrong -- and the rest go first.
    const std::size_t said_max = 3 - shift;
    const std::int64_t said_priorities[3] = {5, 8, 9};
    for (std::size_t i = 0; i < said_max; ++i) {
        facts.push_back(Fact{std::string(), surface::role::kMuted, said_priorities[i]});
    }
    publish(std::move(facts), s.detail);
}

// ---- WHAT STATE ONE PANE IS IN -- the recovery invariant, as one word -----------------
// WL-PANE-10 -- agents/workshop/panes-and-windows.md

namespace pane_state {
inline constexpr std::int64_t kClosed = 0;
inline constexpr std::int64_t kUnresolved = 1;
inline constexpr std::int64_t kRefused = 2;
inline constexpr std::int64_t kWaiting = 3;
inline constexpr std::int64_t kOffRoom = 4;
inline constexpr std::int64_t kCovered = 5;
inline constexpr std::int64_t kOpen = 6;
} // namespace pane_state

/// The word a maker reads. Total over the integer, for `panel_kind`'s reason.
inline const char* pane_state_word(std::int64_t state) {
    switch (state) {
    case pane_state::kUnresolved: return "unresolved";
    case pane_state::kRefused: return "refused";
    case pane_state::kWaiting: return "waiting";
    case pane_state::kOffRoom: return "off-room";
    case pane_state::kCovered: return "covered";
    case pane_state::kOpen: return "open";
    default: return "closed";
    }
}

/// WHAT A MAKER CAN DO ABOUT ONE STATE -- the remedy column of the table above, as a
/// function.
// WL-PANE-10 -- agents/workshop/panes-and-windows.md
inline const char* pane_state_remedy(std::int64_t state) {
    switch (state) {
    case pane_state::kClosed: return "open it from the picker";
    case pane_state::kUnresolved: return "check the spelling, or the provider is not loaded";
    case pane_state::kRefused: return "reset its size, or open it on the other medium";
    case pane_state::kWaiting: return "make the window taller, or place it yourself";
    case pane_state::kOffRoom: return "reset its place";
    case pane_state::kCovered: return "raise it";
    default: return "";
    }
}

/// HOW WIDE THE STATE COLUMN IS.
// WL-PANE-10 -- agents/workshop/panes-and-windows.md
inline constexpr std::size_t kPaneStateCols = 11;

/// HOW WIDE THE NAME COLUMN IS -- and it is a bound a party outside this build can
/// reach, which is what makes it a constant rather than the `10` it used to be.
// WL-PED-01 -- agents/workshop/pane-manager.md
inline constexpr std::size_t kPickerNameCols = 13;

/// IS EVERY VISIBLE CELL OF THIS PANE BEHIND ANOTHER ONE?
// WL-PANE-10 -- agents/workshop/panes-and-windows.md; WL-FRONT-05 -- agents/workshop/planes.md
inline bool pane_is_covered(const Panels& panels, const Setup& setup, const Screen& sc,
                            std::int64_t kind, const FineRect& mine) {
    if (mine.w <= 0 || mine.h <= 0) {
        return false; // nothing visible is OFF-ROOM, which is a different word
    }
    const std::vector<std::int64_t> order = effective_pane_order(setup, panels);
    std::size_t me = order.size();
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == kind) {
            me = i;
            break;
        }
    }
    if (me == order.size()) {
        return false;
    }
    std::vector<FineRect> ahead;
    for (std::size_t i = me + 1; i < order.size(); ++i) {
        const FineRect r = bounds_of(panels, setup, order[i], sc).rect;
        if (r.w > 0 && r.h > 0) {
            ahead.push_back(r);
        }
    }
    if (ahead.empty()) {
        return false;
    }
    // EXACT ON THE FINE LATTICE, BY EDGE COMPRESSION. The union of a handful of
    // rectangles is constant between their edges, so the question "is every sub-unit of
    // mine behind the union" needs one representative point per edge-bounded stripe —
    // never a walk of the lattice, which at this resolution would be forty-eight squared
    // points per cell of what used to be one. A pane peeking out by a single sub-unit
    // produces a stripe whose representative is visible, so a maker's sliver still means
    // `open` — one thing a maker can see is enough, exactly as it always was.
    std::vector<std::int64_t> xs{mine.x, surface::add_cells(mine.x, mine.w)};
    std::vector<std::int64_t> ys{mine.y, surface::add_cells(mine.y, mine.h)};
    for (const FineRect& r : ahead) {
        xs.push_back(r.x);
        xs.push_back(surface::add_cells(r.x, r.w));
        ys.push_back(r.y);
        ys.push_back(surface::add_cells(r.y, r.h));
    }
    std::sort(xs.begin(), xs.end());
    std::sort(ys.begin(), ys.end());
    const std::int64_t right = surface::add_cells(mine.x, mine.w);
    const std::int64_t bottom = surface::add_cells(mine.y, mine.h);
    for (std::size_t yi = 0; yi + 1 < ys.size(); ++yi) {
        const std::int64_t y = ys[yi];
        if (y < mine.y || y >= bottom || ys[yi + 1] == y) {
            continue;
        }
        for (std::size_t xi = 0; xi + 1 < xs.size(); ++xi) {
            const std::int64_t x = xs[xi];
            if (x < mine.x || x >= right || xs[xi + 1] == x) {
                continue;
            }
            bool hidden = false;
            for (const FineRect& r : ahead) {
                if (x >= r.x && x < surface::add_cells(r.x, r.w) && y >= r.y &&
                    y < surface::add_cells(r.y, r.h)) {
                    hidden = true;
                    break;
                }
            }
            if (!hidden) {
                return false; // one place a maker can see is enough
            }
        }
    }
    return true;
}

/// THE ONE STATE CLASSIFIER. Asked of an inventory row -- which is the union of the catalog
/// and everything the setup names -- so every authored pane gets exactly one answer and no
/// row is silently omitted because the runtime catalog lacks it.
inline std::int64_t pane_state_of(const Panels& panels, const Setup& setup, const Screen& sc,
                                  const CatalogRow& row) {
    if (!has_pane(setup, row.ref)) {
        return pane_state::kClosed;
    }
    if (row.kind == kNoPaneKind || !resolvable(row.ref, panels)) {
        return pane_state::kUnresolved;
    }
    // A UNIT OUTRANKS A WANT OF ROOM, and this is where that precedence is spent. A pane
    // with a pixel axis AND no tile left is refused rather than waiting: a taller window
    // would give it the tile and it still would not be presented, so telling the maker to
    // make the window taller would be a true sentence about the wrong problem.
    if (!pane_unit_projectable(pane_of(setup, row.ref))) {
        return pane_state::kRefused;
    }
    const PanelBounds where = bounds_of(panels, setup, row.kind, sc);
    if (!where.open) {
        // Named, resolved, projectable and not presented -- which is what `waiting` has
        // always meant here. `seat_panes` is the only thing that produces it and it is
        // medium-independent, which is why this branch does not consult one.
        return pane_state::kWaiting;
    }
    if (!where.projected) {
        return pane_state::kRefused;
    }
    if (where.rect.w <= 0 || where.rect.h <= 0) {
        return pane_state::kOffRoom;
    }
    if (pane_is_covered(panels, setup, sc, row.kind, where.rect)) {
        return pane_state::kCovered;
    }
    return pane_state::kOpen;
}

/// The one row-body spelling, so the painter and any reader of the picker's
/// columns spend the same two column widths.
// WL-PED-01 -- agents/workshop/pane-manager.md
inline std::string picker_entry_text(const std::string& name, const char* state,
                                     const std::string& tail) {
    return detail::pad(detail::fit(name, static_cast<std::int64_t>(kPickerNameCols)),
                       kPickerNameCols) +
           detail::pad(state, kPaneStateCols) + tail;
}

/// The `+ panel` picker: the catalog, where a maker's cursor is in it, and WHICH KINDS ARE
/// ALREADY OPEN, in a fixed column so the list reads down; it asks for the stack's first
/// slot through `picker_bounds` rather than knowing where that is.
inline void paint_picker(surface::SurfaceLayer& layer, const Panels& panels, const Setup& setup,
                         const Screen& sc, const Keymap& keymap) {
    const PanelPicker& picker = panels.picker;
    if (!picker.open) {
        return;
    }
    const FineRect b = picker_bounds(sc);
    paint_panel_frame(layer, b, kTransientChrome);
    // THE PICKER IS ONE BOUNDED REGION OF PROSE, and the budget it spends is the
    // ACTIVE medium's row count rather than the slot's cell count. The two are the same
    // number in a character medium and they are not in one that sets real type -- nine cells
    // of slot is nine rows of a terminal and five rows of an 18-pixel face -- which is the
    // same pair of honest projections the Info panel's body has had.
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a slot with no room for a row says nothing rather than lying about the room
    }
    surface::SurfaceTextRegion region = panel_prose_region(place);
    const auto say = [&region, &place](const std::string& text, std::int64_t role) {
        region.rows.push_back(
            surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    say("+ PANEL -- " + hotkey_text(keymap, Act::kPickerUp) + "/" +
            hotkey_text(keymap, Act::kPickerDown) + ", " +
            hotkey_text(keymap, Act::kPickerChoose) + " opens or removes",
        surface::role::kAccent);
    // THE POPULATION IS THE COMBINED CATALOG AND THE BUDGET IS THE SLOT'S.
    // Before this the list was `kPanelKinds` long and the picker's height was a
    // constant derived from it, which is a catalog census standing in for a
    // capacity -- it was right for exactly as long as no catalog could outgrow
    // the box, and a runtime offer is precisely a catalog that can. So the rows
    // under the heading are `list_window`'s to spend: the OBJECTS list's own
    // function, its own three rules and its own wording (`omitted_text`), which
    // is the second consumer the rule was established with and the fourth
    // overall. There is no second scrolling algorithm here and the picker did not
    // get taller.
    //
    // AND THE POPULATION IS THE SHARED INVENTORY -- the catalog UNION every
    // reference the setup names -- so a pane a maker authored and this build cannot resolve
    // has a row here too, and can be removed with the gesture that removes any other.
    const std::vector<CatalogRow> rows = inventory_rows(setup, panels);
    const std::size_t budget =
        place.rows > 1 ? static_cast<std::size_t>(place.rows - 1) : 0;
    const ListWindow win = list_window(rows.size(), picker.cursor, budget);
    if (win.before > 0) {
        say("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
    }
    for (std::size_t i = win.first; i < win.first + win.count; ++i) {
        const bool here = i == picker.cursor;
        say(std::string(here ? "> " : "  ") +
                picker_entry_text(rows[i].name,
                                  pane_state_word(pane_state_of(panels, setup, sc, rows[i])),
                                  rows[i].summary),
            here ? surface::role::kAccent : surface::role::kFill);
    }
    if (win.after > 0) {
        say("  " + omitted_text(win.after, "more"), surface::role::kMuted);
    }
    // THE REST OF THE SLOT IS THE REGION'S OWN EMPTINESS, and nobody writes it.
    // A region owns what is inside its bounds, so its cell projection already pads every row
    // it was not given -- the spaces that erase the panel underneath in a character medium are
    // `project_one_text_region`'s, and the graphical medium clears the same rectangle once
    // rather than a row at a time. What used to be a loop padding out to `b.h` is now the
    // primitive's contract, which is why this painter no longer has one. See kPickerRows for
    // why the whole slot is covered at all.
    layer.texts.push_back(std::move(region));
}


// ---- SAYING A PANE'S GEOMETRY IN THE FACE'S OWN LANGUAGE ------------------------------
// WL-GEO-09, WL-GEO-10 -- agents/workshop/geometry.md

/// WHAT ONE FINE VALUE IS IN THE ACTIVE MEDIUM'S UNIT, AND WHETHER THAT IS THE
/// AUTHORED NUMBER ITSELF. `exact` false means the amount shown is this medium's
/// floor of a value it cannot say -- a projection, which the readout marks.
struct GeometrySpelling {
    std::string amount;
    bool exact = true;
};

/// THE UNIT WORD FOR A MEDIUM THAT REPORTED `cell_px` -- `px` where the medium named
/// a device pixel, `cells` where it said its device unit IS the cell (every terminal,
/// and a run no medium has spoken to yet). A WORD rather than a symbol, because it is
/// the noun a maker would use about the thing they are looking at.
inline const char* geometry_unit(std::int64_t cell_px) {
    return cell_px > 0 ? "px" : "cells";
}

/// ONE FINE COORDINATE OR EXTENT, SPELLED FOR THIS MEDIUM.
// WL-GEO-09, WL-GEO-10 -- agents/workshop/geometry.md
inline GeometrySpelling geometry_spelling(std::int64_t subs, std::int64_t cell_px) {
    return GeometrySpelling{std::to_string(surface::device_of_subs(subs, cell_px)),
                            surface::subs_exact_in_device(subs, cell_px)};
}

/// THE MARK AN INEXACT SPELLING WEARS. ASCII, because the shipped graphical face's
/// letterform covers printable ASCII and nothing else -- an `almost equal` sign would
/// render there as the unknown-glyph box, which is a worse lie than the one it was
/// added to prevent.
inline constexpr const char* kProjectedMark = "~";

/// The clause a line carries when any number on it is a projection. It appears ONLY
/// when something on that line actually is one, so a maker working in the unit their
/// own gestures author never reads it: the distinction is inspectable rather than
/// permanently lectured.
inline constexpr const char* kProjectedNote = " (~ projected)";

/// ONE FINE VALUE, WITH ITS MARK. `any_projected` accumulates, so a caller decides
/// once whether the line it is building owes the clause above.
inline std::string geometry_amount_text(std::int64_t subs, std::int64_t cell_px,
                                        bool& any_projected) {
    const GeometrySpelling spelled = geometry_spelling(subs, cell_px);
    if (spelled.exact) {
        return spelled.amount;
    }
    any_projected = true;
    return std::string(kProjectedMark) + spelled.amount;
}

/// THE SAME SPELLING READ BACKWARDS: a whole number a maker TYPED in the active
/// face's unit, as a fine value.
// WL-PED-06 -- agents/workshop/pane-manager.md
inline constexpr std::int64_t subs_of_device_amount(std::int64_t amount,
                                                    std::int64_t cell_px) noexcept {
    const std::int64_t bound = (std::numeric_limits<std::int64_t>::max)() / surface::kCellSubs;
    const std::int64_t a = amount > bound ? bound : (amount < -bound ? -bound : amount);
    if (cell_px <= 0) {
        return a * surface::kCellSubs;
    }
    const std::int64_t num = a * surface::kCellSubs;
    if (num < 0) {
        return -((-num + cell_px - 1) / cell_px);
    }
    return (num + cell_px - 1) / cell_px;
}

static_assert(subs_of_device_amount(10, 0) == 10 * surface::kCellSubs,
              "on a cell medium a typed cell count is that many whole cells");
static_assert(subs_of_device_amount(120, surface::kCanvasCellPx) == 10 * surface::kCellSubs,
              "on the shipped window a typed pixel count is exact where the grain divides");

/// WHAT A MAKER TYPED FOR ONE GEOMETRY AMOUNT: `10`, `10 cells`, `120px` -- a whole number,
/// optionally followed by THIS face's unit word.
// WL-PED-06 -- agents/workshop/pane-manager.md
struct FaceAmount {
    bool accepted = false;
    std::int64_t subs = 0;
    std::string refusal;
};

inline FaceAmount parse_face_amount(std::string_view text, std::int64_t cell_px) {
    FaceAmount out;
    const auto trim = [](std::string_view v) {
        while (!v.empty() && v.front() == ' ') {
            v.remove_prefix(1);
        }
        while (!v.empty() && v.back() == ' ') {
            v.remove_suffix(1);
        }
        return v;
    };
    std::string_view body = trim(text);
    const std::string_view unit = geometry_unit(cell_px);
    const std::string_view other = cell_px > 0 ? "cells" : "px";
    if (body.size() > other.size() &&
        body.substr(body.size() - other.size()) == other) {
        out.refusal = "this face reads " + std::string(unit) + ", not " + std::string(other);
        return out;
    }
    if (body.size() > unit.size() && body.substr(body.size() - unit.size()) == unit) {
        body = trim(body.substr(0, body.size() - unit.size()));
    }
    const std::optional<std::int64_t> amount = TextForm<std::int64_t>::parse(body);
    if (!amount) {
        out.refusal = "not a whole number of " + std::string(unit) + " (`-` resets it)";
        return out;
    }
    out.accepted = true;
    out.subs = subs_of_device_amount(*amount, cell_px);
    return out;
}

/// A WHOLE FINE RECTANGLE, IN THE ACTIVE MEDIUM'S UNIT -- `@x,y WxH unit`.
inline std::string fine_rect_text(const FineRect& r, std::int64_t cell_px) {
    bool projected = false;
    std::string text = "@" + geometry_amount_text(r.x, cell_px, projected) + "," +
                       geometry_amount_text(r.y, cell_px, projected) + " " +
                       geometry_amount_text(r.w, cell_px, projected) + "x" +
                       geometry_amount_text(r.h, cell_px, projected) + " " +
                       geometry_unit(cell_px);
    if (projected) {
        text += kProjectedNote;
    }
    return text;
}

/// WHAT A MAKER AUTHORED FOR ONE PANE'S WINDOW, in the active medium's own unit.
// WL-GEO-09 -- agents/workshop/geometry.md
inline std::string pane_window_text(const SetupPane* row, std::int64_t cell_px) {
    if (row == nullptr) {
        return "--";
    }
    bool projected = false;
    const auto axis = [cell_px, &projected](const PaneSize& s) -> std::string {
        if (s.mode == pane_unit::kSubcells) {
            return geometry_amount_text(s.amount, cell_px, projected);
        }
        if (s.mode == pane_unit::kPixels) {
            return std::to_string(s.amount) + "px";
        }
        return std::string("-");
    };
    std::string text;
    if (row->place.mode == pane_unit::kSubcells) {
        text += "@" + geometry_amount_text(row->place.x, cell_px, projected) + "," +
                geometry_amount_text(row->place.y, cell_px, projected) + " ";
    }
    text += axis(row->width) + "x" + axis(row->height);
    // THE UNIT IS SAID ONCE, AND ONLY WHERE A NUMBER IN IT WAS PRINTED. A row default
    // on every axis has said nothing measurable, and appending `cells` to `-x-` would
    // be naming the unit of a number that is not there.
    if (row->place.mode == pane_unit::kSubcells || row->width.mode == pane_unit::kSubcells ||
        row->height.mode == pane_unit::kSubcells) {
        text += " " + std::string(geometry_unit(cell_px));
    }
    text += " f" + std::to_string(row->front);
    if (projected) {
        text += kProjectedNote;
    }
    return text;
}

/// IS ANY PART OF THIS PANE'S WINDOW STILL THE CODE'S ANSWER RATHER THAN THE MAKER'S?
// WL-GEO-12 -- agents/workshop/geometry.md
inline bool pane_window_partly_default(const SetupPane* row) {
    if (row == nullptr) {
        return false;
    }
    return row->place.mode == pane_unit::kDefault || row->width.mode == pane_unit::kDefault ||
           row->height.mode == pane_unit::kDefault;
}


// ---- A SURFACE SIZED BY WHAT IT SAYS, PLACED ---------------------------------------------

/// WHERE A SURFACE SIZED BY ITS OWN CONTENT OPENS, asked at an anchor: the arithmetic
/// `context_bounds` has spent, quarried out so the full hotkey view spends the same sentence.
// WL-CTX-03 -- agents/workshop/contextual.md; WL-KEY-10 -- agents/workshop/keyboard.md
inline FineRect popup_bounds_at(std::int64_t want_cols, std::int64_t want_rows,
                                std::int64_t x, std::int64_t y, const Screen& sc) {
    const surface::RegionCells cells =
        surface::region_cells_for(want_cols, want_rows, sc.text_advance_px, sc.text_line_px);
    const ui::Rect outer = chrome_outer_of(0, 0, cells.w, cells.h);
    const std::int64_t floor_y = kWorkspaceY + sc.room_h;
    const std::int64_t w = outer.w > sc.w ? sc.w : outer.w;
    const std::int64_t room_rows = floor_y - kStackY;
    const std::int64_t h = outer.h > room_rows ? room_rows : outer.h;
    if (x + w > sc.w) {
        x = sc.w - w;
    }
    if (x < 0) {
        x = 0;
    }
    if (y + h > floor_y) {
        y = floor_y - h;
    }
    if (y < kStackY) {
        y = kStackY;
    }
    return fine_of_cells(ui::Rect{x, y, w, h});
}

// ---- THE FULL HOTKEY VIEW -------------------------------------------------------------
// WL-KEY-11 -- agents/workshop/keyboard.md

/// What to call the context beneath the view, in the heading's voice.
inline std::string keyboard_context_name(const Session& s, KeyContext ctx) {
    switch (ctx) {
    case KeyContext::kTerminal: return "the terminal line";
    case KeyContext::kNaming: return "naming a layout";
    case KeyContext::kPaneNaming: return "naming a new pane";
    case KeyContext::kPicker: return "the + panel picker";
    case KeyContext::kAttention: return "what needs attention";
    case KeyContext::kContext: return "the contextual actions";
    case KeyContext::kArrangePane: {
        return s.arrange.addressed() ? "arranging " + ref_text(s.arrange.pane)
                                     : "arranging a pane";
    }
    case KeyContext::kArrangeDesk: return "arranging the desk";
    case KeyContext::kArrangeReset: return "arranging -- reset";
    case KeyContext::kDraft: return "editing a property";
    case KeyContext::kEditor: return "the source editor";
    case KeyContext::kFiles: return "the project browser";
    case KeyContext::kPaneEditor: return "the Pane Manager";
    case KeyContext::kPane: {
        const std::int64_t typing = keyboard_pane(s.panels);
        const RuntimePane* row =
            typing == kNoPaneKind ? nullptr : s.panels.runtime.of_kind(typing);
        return row != nullptr ? "pane " + row->name + " @" + row->provider
                              : "a focused pane";
    }
    default: return "command mode";
    }
}

/// ONE ROW OF THE VIEW AS IT IS PRESENTED: what a maker reads, and the role it is said in.
struct HotkeyRow {
    std::string text;
    std::int64_t role;
};

/// THE ROWS, COMPOSED WHOLE -- the view's one composition, spent by its extent and by its
/// painter alike.
// WL-KEY-10, WL-KEY-11 -- agents/workshop/keyboard.md
inline std::vector<HotkeyRow> hotkeys_rows(const Session& s) {
    const KeyContext ctx = keyboard_context(s);
    const Keymap& k = s.keymap;
    std::vector<HotkeyRow> rows;
    const auto entry = [&rows](const std::string& gesture, const std::string& label) {
        rows.push_back(HotkeyRow{"  " + detail::pad(gesture, 14) + label,
                                 surface::role::kFill});
    };
    const auto group = [&rows](const std::string& name) {
        if (rows.size() > 1) { // a blank row between groups; none under the heading
            rows.push_back(HotkeyRow{std::string(), surface::role::kMuted});
        }
        rows.push_back(HotkeyRow{name, surface::role::kAccent});
    };

    rows.push_back(HotkeyRow{"HOTKEYS -- " + hotkey_text(k, Act::kHotkeys) + " or esc closes",
                             surface::role::kAccent});
    group(keyboard_context_name(s, ctx));
    if (ctx == KeyContext::kPane) {
        // THE HONEST WHOLE OF A PANE'S KEY STORY. Workshop forwards every ordinary key
        // and every character uninterpreted and is deliberately never told what they
        // mean (the seam's own doctrine), so the one truthful sentence is ownership --
        // pretending to know a provider's bindings would be a claim made out of silence.
        rows.push_back(HotkeyRow{"  every ordinary key and character goes to the pane;",
                                 surface::role::kFill});
        rows.push_back(HotkeyRow{"  what each one means there is the provider's own.",
                                 surface::role::kFill});
    } else {
        for (const ActionRow& row : kActionCatalog) {
            if (row.context == ctx) {
                entry(gesture_text(k.row_gesture(row)), row.label);
            }
        }
    }
    bool above = false;
    for (const ActionRow& row : kActionCatalog) {
        const bool is_class =
            row.context == KeyContext::kGlobal || row.context == KeyContext::kNoText ||
            row.context == KeyContext::kNoEditor;
        if (!is_class || !active_in(row.context, ctx)) {
            continue;
        }
        if (!above) {
            group("answered above every mode");
            above = true;
        }
        entry(gesture_text(k.row_gesture(row)), row.label);
    }
    if (ctx == KeyContext::kTerminal || ctx == KeyContext::kNaming ||
        ctx == KeyContext::kPaneNaming || ctx == KeyContext::kDraft) {
        group("the text box's own keys (not remappable)");
        for (const component::EditingGesture& g : component::kEditingVocabulary) {
            entry(gesture_text(Gesture{g.scancode, g.modifiers}), g.label);
        }
    }
    // THE EDITOR'S OWN MECHANICS, from its declaration rows (editor.hpp) exactly as the
    // component's come from theirs: shown for discovery, marked not remappable, their
    // executable truth being `EditorBuffer::consume` and never this keymap.
    if (ctx == KeyContext::kEditor) {
        group("the editor's own keys (not remappable)");
        for (const component::EditingGesture& g : kEditorVocabulary) {
            entry(gesture_text(Gesture{g.scancode, g.modifiers}), g.label);
        }
    }
    return rows;
}

/// WHERE THE FULL HOTKEY VIEW OPENS, AND HOW BIG IT IS.
// WL-CTX-03 -- agents/workshop/contextual.md; WL-KEY-10 -- agents/workshop/keyboard.md
inline FineRect hotkeys_bounds(const Session& s, const Screen& sc) {
    const std::vector<HotkeyRow> rows = hotkeys_rows(s);
    std::int64_t want_cols = 0;
    for (const HotkeyRow& row : rows) {
        const std::int64_t len = static_cast<std::int64_t>(row.text.size());
        want_cols = len > want_cols ? len : want_cols;
    }
    const std::int64_t want_rows = static_cast<std::int64_t>(rows.size());
    const ui::Rect corner = cells_covered(overlay_column(sc));
    std::int64_t x = corner.x;
    std::int64_t y = corner.y;
    const std::int64_t chosen = selected_pane(s.panels);
    if (chosen != kNoPaneKind) {
        const FineRect anchor = bounds_of(s.panels, s.setup.active, chosen, sc).rect;
        if (anchor.w > 0 && anchor.h > 0) {
            // THE ANCHOR IS A CELL CORNER. The view is screen furniture and never moves by
            // less than a cell -- `picker_bounds`'s own rule -- so the pane's fine top-left
            // is read at the cell grain it is drawn on, which is also where its visible
            // boundary is.
            const ui::Rect at = cells_covered(anchor);
            x = at.x;
            y = at.y;
        }
    }
    return popup_bounds_at(want_cols, want_rows, x, y, sc);
}

inline void paint_hotkeys(surface::SurfaceLayer& layer, const Session& s, const Screen& sc) {
    if (!s.hotkeys.open) {
        return;
    }
    const FineRect b = hotkeys_bounds(s, sc);
    paint_panel_frame(layer, b, kTransientChrome);
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a place with no room for a row says nothing rather than lying about the room
    }
    // THE SAME ROWS THE EXTENT WAS MEASURED FROM, so on a character medium every one
    // of them lands whole on its own row, and on the shipped face the same holds with the
    // face's slack to spare.
    const std::vector<HotkeyRow> rows = hotkeys_rows(s);
    surface::SurfaceTextRegion region = panel_prose_region(place);
    const auto say = [&region, &place](const std::string& text, std::int64_t role) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    // NO CURSOR, SO NO WINDOW TO KEEP IT IN: where the room is smaller than the list, the
    // list is cut at the room and the cut is counted, the completion list's own wording --
    // this place cannot show a row AND tell a maker what it is hiding, so it tells them.
    // The heading is row 0 of the composition and survives a one-row room; the marker
    // takes a row only where one is spare.
    const std::size_t budget = static_cast<std::size_t>(place.rows);
    const bool cut = rows.size() > budget;
    std::size_t shown = rows.size();
    if (cut) {
        shown = budget > 1 ? budget - 1 : budget;
    }
    for (std::size_t i = 0; i < shown; ++i) {
        say(rows[i].text, rows[i].role);
    }
    if (cut && budget > 1) {
        say("  " + omitted_text(rows.size() - shown, "more"), surface::role::kMuted);
    }
    layer.texts.push_back(std::move(region));
}

// ---- WHAT IS TRUE RIGHT NOW, PROJECTED ---------------------------------------------------

/// KEYS. Durable dotted strings, `ActionRow::id`'s own kind of name, spelled once so an
/// owner's `establish` and a reader's `find` cannot drift. The two per-subject families
/// carry the subject in the key, because a key identifies exactly one condition and two
/// panes refusing content are two conditions.
// WL-ATTN-01 -- agents/workshop/attention.md
inline constexpr const char* kKeymapWallKey = "workshop.keymap-refused";
inline constexpr const char* kPrefsWallKey = "workshop.prefs-refused";
inline constexpr const char* kMarksWallKey = "workshop.marks-refused";
inline constexpr const char* kMarksSkippedKey = "workshop.marks-skipped";
/// A SESSION FILE THIS RUN COULD NOT READ, and therefore will not write over.
/// The refusal itself is said once on the notice row, where it belongs -- it is about
/// this launch. What STANDS all run, and has a maker action, is the consequence: this
/// Workshop is not keeping your session, and your old file is still there.
inline constexpr const char* kSessionWallKey = "workshop.session-refused";
/// A PANE-DEFINITION FILE THIS RUN COULD NOT READ: the marks wall's shape, one
/// durable fact over. True from the refusal until the process ends, with a maker action
/// (fix or move the file), and it is also load-bearing: while it stands, nothing this run
/// makes may be written over those bytes.
inline constexpr const char* kPaneWallKey = "workshop.pane-refused";
inline constexpr const char* kLegacyShadowedKeyPrefix = "workshop.legacy-shadowed.";
inline std::string pane_content_key(const PaneRef& ref) {
    return "pane.content-refused." + ref_text(ref);
}
inline std::string pane_window_key(const PaneRef& ref) {
    return "pane.not-presented." + ref_text(ref);
}
inline constexpr const char* kFrontierKey = "project.frontier-waiting";

/// EVERY CONDITION THAT IS CURRENTLY TRUE AND WORTH AMBIENT ATTENTION, ranked.
// WL-ATTN-03, WL-ATTN-04, WL-ATTN-05 -- agents/workshop/attention.md
inline std::vector<Condition> attention_conditions(const Session& s,
                                                   const ProjectFrontier& frontier = {}) {
    std::vector<Condition> out = s.conditions.rows;
    const Screen sc = screen_of(s);

    // DERIVED: a provider's update this pane could not keep. The pane holds it (`refusal`
    // is the body's sentence, `refusal_why` the reason), clears it on the next valid
    // content, and knows nothing about this projection -- so the condition disappears
    // because the pane recovered, with no retraction call anywhere in the path. That is
    // the measured defect this replaced, closed by construction rather than by discipline.
    for (const ExternalPane& pane : s.panels.external) {
        if (pane.refusal.empty()) {
            continue;
        }
        const RuntimePane* named = s.panels.runtime.of_kind(pane.kind);
        if (named == nullptr) {
            continue; // a pane whose catalog row has gone has no subject to name
        }
        const PaneRef ref{named->provider, named->pane};
        out.push_back(Condition{pane_content_key(ref),
                                "`" + ref_text(ref) + "` refused an update",
                                pane.refusal_why.empty() ? pane.refusal : pane.refusal_why,
                                surface::role::kAlert, std::string()});
    }

    // DERIVED: a pane the maker authored, that this build can resolve, and of which no cell
    // is on the screen. The word and the remedy are `pane_state`'s own -- one enumeration,
    // one classifier, and the remedy column that was already written beside it.
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panels)) {
        const std::int64_t state = pane_state_of(s.panels, s.setup.active, sc, row);
        if (state != pane_state::kRefused && state != pane_state::kWaiting &&
            state != pane_state::kOffRoom) {
            continue;
        }
        out.push_back(Condition{pane_window_key(row.ref),
                                "`" + ref_text(row.ref) + "` " + pane_state_word(state),
                                std::string(pane_state_remedy(state)), surface::role::kAccent,
                                "workshop.manage"});
    }

    // DERIVED: realization is stopped at a row waiting on the maker. Informative and
    // actionable, and deliberately NOT an error -- "waiting to be built is not a failure
    // and is not silence either" is the host's own sentence about this exact state.
    if (frontier.waiting) {
        std::string detail = "realization is stopped at `" + frontier.artifact + "`";
        if (frontier.blocked > 0) {
            detail += " with " + std::to_string(frontier.blocked) + " authored row" +
                      (frontier.blocked == 1 ? "" : "s") + " behind it";
        }
        out.push_back(Condition{kFrontierKey, "project waiting on `" + frontier.artifact + "`",
                                detail, surface::role::kAccent, "builder.frontier"});
    }

    std::sort(out.begin(), out.end(), ranks_before);
    return out;
}

/// ...LESS THE ONES THIS SESSION HAS HIDDEN. The one function every presentation spends, so
/// the compact indicator, the view, the cursor bound and the dismissal all agree about which
/// list they are talking about -- the one-geometry rule, applied to a population.
inline std::vector<Condition> attention_shown(const Session& s,
                                              const ProjectFrontier& frontier = {}) {
    std::vector<Condition> out;
    for (Condition& c : attention_conditions(s, frontier)) {
        if (!s.attention.hides(c)) {
            out.push_back(std::move(c));
        }
    }
    return out;
}

/// THE COMPACT LINE, or empty when nothing currently deserves attention.
// WL-ATTN-06 -- agents/workshop/attention.md
inline std::string attention_compact(const std::vector<Condition>& shown) {
    if (shown.empty()) {
        return std::string();
    }
    std::string line = shown.front().compact;
    if (shown.size() > 1) {
        line += " (+" + std::to_string(shown.size() - 1) + " more)";
    }
    return line;
}

/// WHERE THE CURRENT-CONDITION VIEW OPENS: the overlay column, for the hotkey view's old
/// reason.
// WL-ATTN-09 -- agents/workshop/attention.md; WL-KEY-10 -- agents/workshop/keyboard.md
inline constexpr FineRect attention_bounds(const Screen& sc) noexcept {
    return overlay_column(sc);
}

/// THE VIEW: every currently-true, non-dismissed condition, in the owner's own words.
// WL-ATTN-09 -- agents/workshop/attention.md
inline void paint_attention(surface::SurfaceLayer& layer, const Session& s, const Screen& sc,
                            const ProjectFrontier& frontier) {
    if (!s.attention.open) {
        return;
    }
    const FineRect b = attention_bounds(sc);
    paint_panel_frame(layer, b, kTransientChrome);
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a slot with no room for a row says nothing rather than lying about the room
    }
    surface::SurfaceTextRegion region = panel_prose_region(place);
    const auto say = [&region, &place](const std::string& text, std::int64_t role) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    const std::vector<Condition> shown = attention_shown(s, frontier);
    say("ATTENTION -- " + std::to_string(shown.size()) +
            (shown.size() == 1 ? " condition, " : " conditions, ") +
            hotkey_text(s.keymap, Act::kAttentionDismiss) + " hides one, " +
            hotkey_text(s.keymap, Act::kAttentionClose) + " closes",
        surface::role::kAccent);
    const std::size_t budget =
        place.rows > 1 ? static_cast<std::size_t>(place.rows - 1) : 0;
    if (shown.empty()) {
        // NOTHING IS WRONG, SAID IN WORDS. A maker who opened this deliberately is owed an
        // answer, and an empty box is not one. (The compact indicator is already absent --
        // this surface is reachable whether or not anything is true.)
        if (budget > 0) {
            say("  nothing needs your attention right now", surface::role::kMuted);
        }
        layer.texts.push_back(std::move(region));
        return;
    }
    // THE CURSOR'S OWN BLOCK IS COMPOSED AND RESERVED BEFORE THE LIST IS WINDOWED, and that
    // ordering is the whole of this painter's honesty.
    //
    // A window computed over the COMPACT rows alone is right until the row it is keeping in
    // view spends three more beneath it -- and what then falls off the bottom of the region
    // is the omission marker, which is the one row that was there to say something had been
    // dropped. A region PADS what it was not given and SILENTLY DROPS what will not fit
    // (region.hpp's projection), so the over-spend is invisible in both media. A bound that
    // grows when it is exceeded is not a bound, so the block comes out of the budget first
    // and `list_window`'s three rules then run over what is left.
    //
    // THE CURSOR IS RESOLVED ONCE, HERE, and every question below spends the same answer.
    // The population is DERIVED, so it can shrink between a keystroke and a repaint with no
    // gesture in between -- and a painter that clamped in one place and compared the raw
    // value in another would compose an explanation for a row it then never marks.
    const std::size_t cursor =
        s.attention.cursor < shown.size() ? s.attention.cursor : shown.size() - 1;
    const Condition& at = shown[cursor];
    std::vector<std::string> block =
        detail::wrap(at.detail, place.columns - detail::kWrapIndent);
    if (!at.action.empty()) {
        // AN ACTION IS A NAME AND ITS GESTURE IS THE KEYMAP'S, resolved at this paint. The
        // row says what a maker could press somewhere else; nothing here can press it.
        for (const ActionRow& row : kActionCatalog) {
            if (at.action == row.id) {
                block.push_back("try: " + gesture_text(s.keymap.row_gesture(row)) + " " +
                                row.label);
                break;
            }
        }
    }
    // THE LIST KEEPS `list_window`'S OWN FLOOR OF THREE, so the cursor's row is always in
    // the window it is being explained inside of. Below four rows there is no explanation at
    // all rather than an explanation with no statement over it, and what the reserve cannot
    // hold is counted in the same words every other omission on this screen is counted in.
    const std::size_t reserve = budget >= 4 ? (block.size() < budget - 3 ? block.size()
                                                                        : budget - 3)
                                            : 0;
    const ListWindow win = list_window(shown.size(), cursor, budget - reserve);
    if (win.before > 0) {
        say("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
    }
    for (std::size_t i = win.first; i < win.first + win.count; ++i) {
        const Condition& c = shown[i];
        const bool here = i == cursor;
        say(std::string(here ? "> " : "  ") + c.compact, c.role);
        if (!here || reserve == 0) {
            continue;
        }
        const std::size_t said = block.size() > reserve ? reserve - 1 : block.size();
        for (std::size_t line = 0; line < said; ++line) {
            say("    " + block[line], surface::role::kMuted);
        }
        if (said < block.size()) {
            say("    " + omitted_text(block.size() - said, "more"), surface::role::kMuted);
        }
    }
    if (win.after > 0) {
        say("  " + omitted_text(win.after, "more"), surface::role::kMuted);
    }
    layer.texts.push_back(std::move(region));
}

// ---- WHAT CAN I DO WITH THIS, PRESENTED --------------------------------------------------
// WL-CTX-03, WL-CTX-04 -- agents/workshop/contextual.md



/// One entry as its row reads: a group descends and says so, an action is its declared
/// label -- `row_of_id`'s answer, never a second spelling.
inline std::string context_entry_text(const ContextEntry& entry) {
    if (entry.is_group) {
        return std::string(entry.group) + " >";
    }
    return entry.row != nullptr ? entry.row->label : std::string();
}

/// THE WIDEST THE POPUP MAY GROW, in prose columns -- the stack panel's own width, the
/// established panel measure of this screen. Content chooses the extent BELOW this bound;
/// a heading longer than the room falls to `detail::fit`'s mark, the ordinary answer for
/// prose that outgrows its material.
inline constexpr std::int64_t kContextMaxCols = kStackW;

/// The label column of one level: the widest entry text, so annotations start in one
/// column down the whole menu rather than ragged after each label.
inline std::int64_t context_label_columns(const std::vector<ContextEntry>& rows) {
    std::int64_t widest = 0;
    for (const ContextEntry& entry : rows) {
        const std::int64_t len = static_cast<std::int64_t>(context_entry_text(entry).size());
        widest = len > widest ? len : widest;
    }
    return widest;
}

/// THE GESTURE WORTH TEACHING BESIDE ONE ENTRY, or "".
// WL-CTX-06 -- agents/workshop/contextual.md; WL-TAB-12 -- agents/workshop/tab-run.md
inline std::string context_annotation(const Session& s, const ContextEntry& entry) {
    if (entry.is_group || entry.row == nullptr) {
        return std::string(); // folders are not actions and have no gesture to teach
    }
    const KeyContext beneath = keyboard_context_beneath_menu(s);
    bool requestable = false;
    for (const ActionRow& row : kActionCatalog) {
        if (row.act == entry.row->act && active_in(row.context, beneath)) {
            requestable = true;
            break;
        }
    }
    if (!requestable) {
        return std::string();
    }
    //...AND NEITHER DOES A ROW WITH NO GESTURE. The four layout-tab operations
    // are reached from this very menu and from no key; annotating them with `?` would
    // teach a maker a binding that does not exist, in the one surface whose annotation
    // exists to teach the faster way of doing what they just chose.
    if (!is_bound(s.keymap.gesture_of(entry.row->act))) {
        return std::string();
    }
    if (entry.row->act == Act::kObjectDelete && s.context.object != s.selected) {
        return std::string();
    }
    // ⚠ AND THE SAME REFINEMENT FOR A LAYOUT TAB, found by the live TUI witness
    // rather than by a case. `^w` closes the LIVE layout; this menu's row closes the
    // CAPTURED one. The two are the same act exactly when the tab a maker pointed at is
    // the one they are standing on, so the annotation is shown then and only then --
    // anything else teaches a key that acts on a different layout than the row it sits
    // beside, which is `object.delete`'s own reason one subject over.
    if (s.context.subject == context_subject::kLayout &&
        s.context.layout != s.setup.active_at) {
        return std::string();
    }
    return hotkey_text(s.keymap, entry.row->act);
}

/// One population row as composed: the entry's text, and -- where one is truthful -- the
/// effective gesture at the level's annotation column, visually subordinate by position.
/// The painter and the extent both spend THIS spelling; a second copy of the composition
/// would be the two-geometries defect.
inline std::string context_row_text(const Session& s, const ContextEntry& entry,
                                    std::int64_t label_columns) {
    const std::string text = context_entry_text(entry);
    const std::string gesture = context_annotation(s, entry);
    if (gesture.empty()) {
        return text;
    }
    return detail::pad(text, static_cast<std::size_t>(label_columns + 2)) + gesture;
}

/// WHERE THE CONTEXTUAL SURFACE OPENS: beside the press that asked, sized by what
/// it has to say.
// WL-CTX-03 -- agents/workshop/contextual.md
inline FineRect context_bounds(const Session& s, const Screen& sc) {
    const ContextMenu& menu = s.context;
    const std::vector<ContextEntry> rows = context_population(menu.subject, menu.group);
    const std::int64_t label_cols = context_label_columns(rows);
    std::int64_t want_cols = 0;
    for (const ContextEntry& entry : rows) {
        const std::int64_t len =
            2 + static_cast<std::int64_t>(context_row_text(s, entry, label_cols).size());
        want_cols = len > want_cols ? len : want_cols;
    }
    want_cols = want_cols > kContextMaxCols ? kContextMaxCols : want_cols;
    const std::int64_t want_rows = static_cast<std::int64_t>(rows.size());
    std::int64_t x = menu.anchor_x;
    std::int64_t y = menu.anchor_y;
    if (!menu.anchored) {
        const ui::Rect slot = placement_bounds(placement::kOverlayStack, 0, sc);
        x = slot.x;
        y = slot.y;
    }
    return popup_bounds_at(want_cols, want_rows, x, y, sc);
}


/// The cursor, bounded through the population's own size -- the attention view's rule,
/// resolved once and spent by every question (the population is derived, so it can move
/// between a keystroke and a repaint with no gesture in between).
inline constexpr std::size_t context_cursor_bound(std::size_t cursor,
                                                  std::size_t population) noexcept {
    if (cursor < population) {
        return cursor;
    }
    return population == 0 ? 0 : population - 1;
}

inline void paint_context(surface::SurfaceLayer& layer, const Session& s, const Screen& sc) {
    if (!s.context.open) {
        return;
    }
    const FineRect b = context_bounds(s, sc);
    paint_panel_frame(layer, b, kTransientChrome);
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a popup with no room for a row says nothing rather than lying about the room
    }
    surface::SurfaceTextRegion region = panel_prose_region(place);
    const auto say = [&region, &place](const std::string& text, std::int64_t role) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    // THE FIRST ROW IS AN ACTION. Nothing announces that a menu of actions
    // contains actions, and nothing restates the two gestures the band's legend is
    // already saying in the maker's own bindings for as long as this surface is open.
    const ContextMenu& menu = s.context;
    const std::vector<ContextEntry> rows = context_population(menu.subject, menu.group);
    const std::int64_t label_cols = context_label_columns(rows);
    const std::size_t budget = static_cast<std::size_t>(place.rows);
    const std::size_t cursor = context_cursor_bound(menu.cursor, rows.size());
    const ListWindow win = list_window(rows.size(), cursor, budget);
    if (win.before > 0) {
        say("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
    }
    for (std::size_t i = win.first; i < win.first + win.count; ++i) {
        const bool here = i == cursor;
        say(std::string(here ? "> " : "  ") + context_row_text(s, rows[i], label_cols),
            here ? surface::role::kAccent : surface::role::kFill);
    }
    if (win.after > 0) {
        say("  " + omitted_text(win.after, "more"), surface::role::kMuted);
    }
    layer.texts.push_back(std::move(region));
}

/// WHERE A PRESS LANDED ON THE OPEN CONTEXTUAL SURFACE -- the painter's inverse, over the
/// same composition (`info_body_at`'s family: it answers WHERE and nothing about what
/// that means; the weave decides what a hit does).
// WL-CTX-08 -- agents/workshop/contextual.md
struct ContextPressAt {
    bool inside = false;
    bool entry = false;
    std::size_t index = 0;
};

inline ContextPressAt context_press_at(const Session& s, const Screen& sc, std::int64_t space,
                                       std::int64_t x, std::int64_t y, const PointedAt& at) {
    ContextPressAt out;
    if (!s.context.open) {
        return out;
    }
    const FineRect b = context_bounds(s, sc);
    if (!b.contains_at(at.sub.x, at.sub.y, at.grain)) {
        return out;
    }
    out.inside = true;
    // THE SAME CALL THE PAINTER MAKES, and not a re-derivation beside it: the
    // painter asks `panel_prose_place` for this rectangle and so does this, so the chrome
    // inset, the metric and the row budget are one answer rather than two that agree
    // today. `prose_at` takes the region's CELL origin (the number on the published
    // `SurfaceTextRegion`), so the wire spelling of the same interior is what it is handed.
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return out;
    }
    const surface::SurfaceTextRegion wire = panel_prose_region(place);
    const ProseAt where = prose_at(space, x, y, wire.x, wire.y, place.fit);
    if (!where.understood || where.column < 0 || where.column >= place.columns ||
        where.row < 0 || where.row >= place.rows) {
        return out;
    }
    // PAINTED ROW i IS POPULATION ROW i. No heading is reserved any more, so there is no
    // offset here and none in the painter -- the one arithmetic that could have made a
    // press choose a different row from the one under it is simply gone.
    const std::vector<ContextEntry> rows =
        context_population(s.context.subject, s.context.group);
    const std::size_t budget = static_cast<std::size_t>(place.rows);
    const std::size_t cursor = context_cursor_bound(s.context.cursor, rows.size());
    const ListWindow win = list_window(rows.size(), cursor, budget);
    std::int64_t offset = where.row;
    if (win.before > 0) {
        if (offset == 0) {
            return out; // the `... n earlier` marker
        }
        --offset;
    }
    if (static_cast<std::size_t>(offset) >= win.count) {
        return out; // the `... n more` marker, or the region's own emptiness
    }
    out.entry = true;
    out.index = win.first + static_cast<std::size_t>(offset);
    return out;
}

// ---- The Info panel's BODY, resolved ONCE ----

/// THE CURSOR MARK AND THE LABEL, in columns: `>` (or a space) and the padded property name.
// WL-INFO-02 -- agents/workshop/info-body.md
inline constexpr std::int64_t kPropertyMarkCols = 1;
inline constexpr std::int64_t kPropertyLabelCols = 9;

/// THE COLUMN THE INSERTION POINT SITS IN, kept out of the value's own budget.
// WL-INFO-02 -- agents/workshop/info-body.md
inline constexpr std::int64_t kPropertyCaretCols = 1;

/// "This prose row shows no property" — a marker row, the `PROPERTIES` heading, an object
/// row, a blank row, or a row nobody has.
///
/// A count-sized sentinel rather than a signed index, because every other property position in
/// this file is a `std::size_t` into `Session::rows` and converting at the boundary is where
/// an off-by-one hides. `position_of` uses `elements.size()` for the same job one shape over;
/// this one cannot, because the body's own row population is not the collection being indexed.
inline constexpr std::size_t kNoProperty = static_cast<std::size_t>(-1);

/// "This prose row shows no object" — a marker row, the `PROPERTIES` heading, a property
/// row, a blank row, or a row nobody has. `kNoProperty`'s twin, one list over.
inline constexpr std::size_t kNoObject = static_cast<std::size_t>(-1);

/// "This member is not on screen" — the window is not showing it.
///
/// NEGATIVE, for `role::kNone`'s and `kNoCaret`'s reason: a prose row index is non-negative by
/// construction, so an absence spelled this way cannot collide with a row anybody meant.
inline constexpr std::int64_t kNoProseRow = -1;

/// HOW MANY PROSE ROWS EACH LIST GETS, and the whole of composition policy.
// WL-INFO-07 -- agents/workshop/info-body.md
struct BodyShare {
    std::size_t objects = 0;    ///< prose rows the OBJECTS list may spend, markers included
    std::size_t properties = 0; ///< prose rows the property list may spend, markers included
};

/// TOTAL over all three counts, because the budget comes from a metric that arrived on the bus
/// and the two demands come from a document a file can have written.
inline BodyShare share_body_rows(std::size_t budget, std::size_t want_objects,
                                 std::size_t want_properties) {
    BodyShare s;
    if (budget == 0) {
        return s;
    }
    if (want_properties <= budget && want_objects <= budget - want_properties) {
        s.objects = want_objects; // both whole; the rest of the body stays spare
        s.properties = want_properties;
        return s;
    }
    const std::size_t half = budget / 2;
    if (want_objects <= half) {
        s.objects = want_objects; // it needs less than its share, so it takes what it needs
        s.properties = budget - s.objects;
    } else if (want_properties <= budget - half) {
        s.properties = want_properties;
        s.objects = budget - s.properties;
    } else {
        s.objects = half; // both want more than half: the contested room is shared
        s.properties = budget - half;
    }
    return s;
}

/// THE SMALLEST BODY THAT CAN SAY ANYTHING: one object row, the `PROPERTIES` heading, one
/// property row.
// WL-INFO-01 -- agents/workshop/info-body.md
inline constexpr std::size_t kInfoBodyMinRows = 3;

// ---- The Info panel's ACTION CONTROLS -----------------------------------------------------
// WL-CTRL-01 -- agents/workshop/info-controls.md

/// The two acts a maker can reach without knowing a key. Indices into one table, in the order
/// the footer paints them.
inline constexpr std::size_t kActionCreate = 0;
inline constexpr std::size_t kActionDelete = 1;
inline constexpr std::size_t kActionCount = 2;

/// "This prose row carries no control" — `kNoProperty`'s and `kNoObject`'s twin, one run over.
inline constexpr std::size_t kNoAction = static_cast<std::size_t>(-1);

/// PROSE ROWS THE FOOTER COSTS: one per control, and the count is the table's.
// WL-CTRL-01 -- agents/workshop/info-controls.md
inline constexpr std::size_t kActionRows = kActionCount;

/// WHY AN ACTION CANNOT RUN RIGHT NOW, or that it can.
// WL-CTRL-03, WL-CTRL-04 -- agents/workshop/info-controls.md
enum class Availability {
    kAvailable, ///< press it and the operation runs
    kNoTarget,  ///< there is no object for this act to be about
    kDraftLive, ///< the maker has unfinished work this act would destroy
};

inline constexpr bool available(Availability a) noexcept {
    return a == Availability::kAvailable;
}

/// IS A PROPERTY DRAFT LIVE?
// WL-CTRL-03 -- agents/workshop/info-controls.md; WL-PED-07 -- agents/workshop/pane-manager.md
inline bool draft_live(const Session& s) {
    for (const Row& row : s.rows) {
        if (row.editing()) {
            return true;
        }
    }
    return false;
}

/// AVAILABILITY OVER THE TWO FACTS IT DEPENDS ON, so the whole rule is readable in one place
/// and testable without a document. The overload below is what a painter and a press call.
inline constexpr Availability action_availability(std::size_t which, bool editing,
                                                  bool has_target) noexcept {
    if (editing) {
        return Availability::kDraftLive; // both controls; the reason is about the MAKER
    }
    if (which == kActionDelete && !has_target) {
        return Availability::kNoTarget;
    }
    return Availability::kAvailable;
}

/// THE SAME QUESTION ABOUT THE DOCUMENT AND SESSION EVERYTHING ELSE HERE IS DERIVED FROM.
// WL-CTRL-03 -- agents/workshop/info-controls.md
inline Availability action_availability(std::size_t which, const WorkshopDoc& d,
                                        const Session& s) {
    return action_availability(which, draft_live(s), doc::find(d, s.selected) != nullptr);
}

/// THE NAME A MAKER READS. Application words, in the application's file: a control does not
/// know the key that also performs its act (`n`, `d`), and a shortcut hint is not written
/// here -- the two help lines at the bottom of the screen are where this tool says what its
/// keys do, and a second copy beside every control is a second thing to keep true.
inline constexpr const char* action_label(std::size_t which) noexcept {
    return which == kActionCreate ? "Create" : "Delete";
}

/// ONE CONTROL AS PROSE — and the availability is said in CHARACTERS, not in colour.
// WL-CTRL-04 -- agents/workshop/info-controls.md
inline std::string action_row_text(std::size_t which, bool pressable, std::int64_t columns) {
    const std::string open = pressable ? "[ " : "( ";
    const std::string close = pressable ? " ]" : " )";
    return detail::fit(open + action_label(which) + close, columns);
}

/// WHERE THE INFO PANEL'S TWO LISTS ARE, HOW MANY ROWS EACH GETS, AND WHICH MEMBERS ARE SHOWN.
// WL-INFO-01 -- agents/workshop/info-body.md
struct InfoBodyPlace {
    bool present = false;
    /// THE PANEL'S WHOLE RECTANGLE: the `OBJECTS` heading is the region's first
    /// prose row and the body begins under it, so the region and the panel are one
    /// rectangle and `kInfoHeadingRows` is subtracted from the PROSE budget, not the cells.
    // WL-INFO-08 -- agents/workshop/info-body.md
    std::int64_t region_x = 0;
    std::int64_t region_y = 0;
    std::int64_t region_w = 0;
    std::int64_t region_h = 0;
    std::int64_t region_sub_x = 0;
    std::int64_t region_sub_y = 0;
    std::int64_t region_sub_w = 0;
    std::int64_t region_sub_h = 0;
    surface::RegionFit fit{}; ///< what this medium makes of those bounds
    /// COLUMNS ONE BODY ROW HAS — `fit.columns`, named so a reader does not have to know which
    /// of the fit's numbers is the prose width. An OBJECT row is fitted to this whole width;
    /// a PROPERTY row spends part of it on the mark and the name.
    std::int64_t columns = 0;
    /// COLUMNS A PROPERTY VALUE MAY OCCUPY — the mark, the label and the caret's own column
    /// taken off. It is the ONE capacity for a value: the slice the painter cuts of a live
    /// draft, the window `keep_caret_visible` reconciles, the width a RESTING value is fitted
    /// to, and the room a press is answered against are all this number.
    std::int64_t value_columns = 0;
    /// PROSE ROWS THE WHOLE BODY HOLDS -- both lists, the `PROPERTIES` heading and the
    /// spare rows together, with the `OBJECTS` heading's rows already reserved off the top.
    std::size_t capacity = 0;
    std::size_t objects_rows = 0;    ///< of those, the OBJECTS list's share
    std::size_t properties_rows = 0; ///< of those, the property list's share
    ListWindow objects{};            ///< which objects are shown, and what is left out
    ListWindow properties{};         ///< which properties are shown, and what is left out
    /// THE PROSE ROW CARRYING `PROPERTIES`. It MOVES: it is exactly the number of rows the
    /// object list was given, so a panel that can show more objects pushes it down and a panel
    /// that can show fewer pulls it up. `kRowsY = 8` was this number when it could not move.
    std::int64_t heading_row = kNoProseRow;
    /// THE FIRST OF THE `kActionRows` CONTROL ROWS, and the one number the footer needs.
    // WL-CTRL-01 -- agents/workshop/info-controls.md
    std::int64_t action_row = kNoProseRow;
};

/// THE PROPERTY ROW THAT MUST STAY ON SCREEN: the one being edited, or the cursor's.
// WL-INFO-04 -- agents/workshop/info-body.md
inline std::size_t inspector_focus(const Session& s) {
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (s.rows[i].editing()) {
            return i;
        }
    }
    return s.cursor;
}

/// WHAT A LIST ASKS THE BODY FOR: one row per member, and never zero.
// WL-INFO-07 -- agents/workshop/info-body.md
inline constexpr std::size_t list_demand(std::size_t members) noexcept {
    return members == 0 ? 1 : members;
}

/// THE BODY, RESOLVED. `total_objects`/`selected_at` and `total_properties`/`focus` are the two
/// populations and the two members that must stay on screen. They are arguments rather than a
/// document and a session so this is pure over the four numbers the composition depends on.
// WL-INFO-01, WL-INFO-08 -- agents/workshop/info-body.md
// WL-CHROME-05 -- agents/workshop/chrome.md
// WL-CTRL-01 -- agents/workshop/info-controls.md
inline InfoBodyPlace info_body_place(const FineRect& outer, const Screen& sc,
                                     std::size_t total_objects, std::size_t selected_at,
                                     std::size_t total_properties, std::size_t focus) {
    InfoBodyPlace p;
    const PaneInside inside = pane_inside(outer, sc);
    const FineRect panel = inside.rect;
    if (panel.w <= 0 || panel.h <= 0) {
        return p; // no panel at all, or none left inside its own chrome
    }
    // THE REGION IS THE WHOLE PANEL AND THE `OBJECTS` HEADING IS ITS FIRST PROSE ROW
    // -- `external_body_place`'s ordering exactly, and for its reason: the heading
    // is reserved out of the PROSE budget before either list is offered anything, so the
    // body's own rows still begin at zero and a press on the heading names nothing. In a
    // character medium one prose row is one cell row and the arithmetic is byte-for-byte
    // what `kInfoBodyY = 1` used to subtract; in a medium that sets type, the heading is a
    // row of that type like everything under it. The region fields and the fit are filled
    // for every non-empty panel -- present or not -- because the heading is sayable in a
    // panel whose body is not seatable, and the painter must not resolve the rectangle a
    // second time to say it.
    const surface::SurfaceRect wire = wire_rect_of(panel, surface::role::kFill);
    p.region_x = wire.x;
    p.region_y = wire.y;
    p.region_w = wire.w;
    p.region_h = wire.h;
    p.region_sub_x = wire.sub_x;
    p.region_sub_y = wire.sub_y;
    p.region_sub_w = wire.sub_w;
    p.region_sub_h = wire.sub_h;
    p.fit = inside.fit;
    p.columns = p.fit.columns;
    const std::int64_t used = kPropertyMarkCols + kPropertyLabelCols;
    if (surface::cell_of_subs(surface::add_cells(panel.x, panel.w)) -
            surface::cell_of_subs(panel.x) <=
        used) {
        return p; // no room for a value beside a name
    }
    p.value_columns = p.fit.columns - used - kPropertyCaretCols;
    if (p.value_columns < 0) {
        p.value_columns = 0;
    }
    p.capacity = p.fit.rows > kInfoHeadingRows
                     ? static_cast<std::size_t>(p.fit.rows - kInfoHeadingRows)
                     : 0;
    if (p.capacity < kInfoBodyMinRows + kActionRows) {
        return p; // not enough to seat a row of each list, the heading, and the controls
    }
    // ONE ROW OFF THE TOP FOR THE HEADING AND `kActionRows` OFF THE FOOT FOR THE CONTROLS,
    // before either list is offered anything. Both are chrome and both are bought at the same
    // price as a row of material, which is the same rule `list_window` follows for its own
    // markers: a bound that grows when it is exceeded is not a bound.
    //
    // THE WHOLE COMPOSITION POLICY IS THIS ONE SUBTRACTION AND THE ONE CALL UNDER IT.
    // The controls are a FIXED demand and the lists are VARIABLE ones, so they are not three
    // claimants on `share_body_rows`: sharing is what two parties do when they both want more
    // than there is, and a control wants exactly one row at every size this panel has. Giving
    // the footer a share would have made it grow into a tall panel's spare room for no reason
    // anybody could state. So the fixed demand comes off the top of the budget and the
    // variable ones share what is left -- and every property the suite pinned survives it, because
    // a budget reduced by a constant is still a budget: growing the panel still grows both
    // shares, a list that fits still gets exactly what it needs, and spare room is still spare.
    //
    // There is no `-2 for buttons` anywhere else in this file. This line is the reservation,
    // `action_row` below is where the reserved rows are, and the painter and the press both
    // ask for that number rather than recomputing it.
    const BodyShare share = share_body_rows(p.capacity - 1 - kActionRows,
                                            list_demand(total_objects),
                                            list_demand(total_properties));
    p.objects_rows = share.objects;
    p.properties_rows = share.properties;
    p.objects = list_window(total_objects, selected_at, p.objects_rows);
    p.properties = list_window(total_properties, focus, p.properties_rows);
    p.heading_row = static_cast<std::int64_t>(p.objects_rows);
    p.action_row = static_cast<std::int64_t>(p.capacity - kActionRows);
    p.present = true;
    return p;
}

/// The same resolution for the document and session a painter is holding. One call, so nothing
/// can resolve the body against a population, a selection or a focus the rest of the screen
/// does not have.
inline InfoBodyPlace info_body_place(const FineRect& panel, const Screen& sc,
                                     const WorkshopDoc& d, const Session& s) {
    return info_body_place(panel, sc, d.elements.size(), position_of(d, s.selected),
                           s.rows.size(), inspector_focus(s));
}

/// The cell-lattice doors to the same two resolutions — one multiply each, so a
/// caller holding a whole-cell rectangle (the side region is one at every extent)
/// asks the identical question the fine door answers.
inline InfoBodyPlace info_body_place(const ui::Rect& panel, const Screen& sc,
                                     std::size_t total_objects, std::size_t selected_at,
                                     std::size_t total_properties, std::size_t focus) {
    return info_body_place(fine_of_cells(panel), sc, total_objects, selected_at,
                           total_properties, focus);
}

inline InfoBodyPlace info_body_place(const ui::Rect& panel, const Screen& sc,
                                     const WorkshopDoc& d, const Session& s) {
    return info_body_place(fine_of_cells(panel), sc, d, s);
}

/// WHERE A POINTER FACT LANDED IN THE INFO PANEL'S BODY — the resolve-and-locate answer the
/// three body handlers all begin from.
// WL-PRESS-03 -- agents/workshop/press-chain.md
struct InfoBodyAt {
    /// THE PANEL IS OPEN, THE BODY RESOLVED, AND THE POSITION UNDERSTOOD — the one bit that
    /// says the two fields below are worth asking anything.
    // WL-PRESS-03 -- agents/workshop/press-chain.md
    bool present = false;
    InfoBodyPlace body{}; ///< the body the painter resolved, not a second reading of it
    ProseAt at{};         ///< where the fact landed, in BODY rows: 0 is the row under `OBJECTS`
};

/// The preamble itself: the Info panel's body resolved from the same `bounds_of` the painter
/// used, and this pointer fact located in it by the same `prose_at` every region press goes
/// through. Three copies of these six lines lived in `info_press`, `actions_press` and
// WL-FOCUS-03 -- agents/workshop/focus.md
// WL-INFO-08 -- agents/workshop/info-body.md
// WL-PRESS-03 -- agents/workshop/press-chain.md
inline InfoBodyAt info_body_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                               std::int64_t x, std::int64_t y) {
    const Screen sc = screen_of(s);
    const PanelBounds info = bounds_of(s.panels, s.setup.active, panel::kInfo, sc);
    if (!info.open) {
        return InfoBodyAt{};
    }
    InfoBodyAt where;
    where.body = info_body_place(info.rect, sc, d, s);
    where.at = prose_at(space, x, y, where.body.region_x, where.body.region_y, where.body.fit);
    // THE HEADING ROW IS SUBTRACTED HERE BECAUSE IT WAS RESERVED THERE.
    // `info_body_place` keeps `kInfoHeadingRows` out of the body's budget before either
    // list is offered anything, so the row a handler means by 0 is the region's prose row
    // 1 -- doing that subtraction in one direction and forgetting it in the other is
    // precisely the off-by-one `external_press_at` already guards against, one panel over.
    // A press ON the heading names no body row and falls to the panel's occupancy answer.
    where.at.row -= kInfoHeadingRows;
    where.present = where.body.present && where.at.understood && where.at.row >= 0;
    return where;
}

// ---- One windowed list's rows, mapped both ways ------------------------------------------
// WL-INFO-04 -- agents/workshop/info-body.md

/// WHICH PROSE ROW SHOWS ITEM `index` OF A LIST THAT BEGINS AT `first_row`, or `kNoProseRow`
/// when the window is not showing it.
// WL-INFO-04 -- agents/workshop/info-body.md
inline std::int64_t prose_row_in_window(const ListWindow& w, std::int64_t first_row,
                                        std::size_t index) {
    if (index < w.first || index - w.first >= w.count) {
        return kNoProseRow;
    }
    return first_row + static_cast<std::int64_t>(index - w.first) + (w.before > 0 ? 1 : 0);
}

/// WHICH ITEM A PROSE ROW SHOWS, or `count` positions past the window's own end for a marker
/// row, a row outside the list's run, or a row nobody has. The inverse of the function above,
/// and its only inverse; callers turn "not an item" into their own sentinel.
inline bool item_at_prose_row(const ListWindow& w, std::int64_t first_row, std::size_t rows,
                              std::int64_t row, std::size_t& out) {
    if (row < first_row || row >= first_row + static_cast<std::int64_t>(rows)) {
        return false;
    }
    const std::int64_t at = row - first_row - (w.before > 0 ? 1 : 0);
    if (at < 0 || at >= static_cast<std::int64_t>(w.count)) {
        return false; // an omission marker, or past the last item shown
    }
    out = w.first + static_cast<std::size_t>(at);
    return true;
}

/// WHICH PROSE ROW OF THE BODY SHOWS OBJECT `index`, and which object a prose row shows. The
/// object list begins at the body's first row, so its `first_row` is zero.
inline std::int64_t prose_row_of_object(const InfoBodyPlace& p, std::size_t index) {
    return p.present ? prose_row_in_window(p.objects, 0, index) : kNoProseRow;
}

inline std::size_t object_at_prose_row(const InfoBodyPlace& p, std::int64_t row) {
    std::size_t at = 0;
    if (!p.present || !item_at_prose_row(p.objects, 0, p.objects_rows, row, at)) {
        return kNoObject;
    }
    return at;
}

/// WHICH PROSE ROW OF THE BODY SHOWS PROPERTY `index`, and which property a prose row shows.
/// The property list begins one row under the `PROPERTIES` heading, which is itself one row
/// under the object list's last row -- so both answers move when the composition does, and
/// they move together because they are the same two calls.
inline std::int64_t prose_row_of_property(const InfoBodyPlace& p, std::size_t index) {
    if (!p.present) {
        return kNoProseRow;
    }
    return prose_row_in_window(p.properties, p.heading_row + 1, index);
}

inline std::size_t property_at_prose_row(const InfoBodyPlace& p, std::int64_t row) {
    std::size_t at = 0;
    if (!p.present ||
        !item_at_prose_row(p.properties, p.heading_row + 1, p.properties_rows, row, at)) {
        return kNoProperty;
    }
    return at;
}

/// WHICH PROSE ROW OF THE BODY CARRIES CONTROL `which`, and which control a prose row carries.
// WL-CTRL-02 -- agents/workshop/info-controls.md
inline std::int64_t prose_row_of_action(const InfoBodyPlace& p, std::size_t which) {
    if (!p.present || which >= kActionCount) {
        return kNoProseRow;
    }
    return p.action_row + static_cast<std::int64_t>(which);
}

inline std::size_t action_at_prose_row(const InfoBodyPlace& p, std::int64_t row) {
    if (!p.present || row < p.action_row ||
        row >= p.action_row + static_cast<std::int64_t>(kActionRows)) {
        return kNoAction;
    }
    return static_cast<std::size_t>(row - p.action_row);
}

/// WHICH CONTROL A PRESS INSIDE THE BODY NAMES, or `kNoAction`.
// WL-PRESS-03 -- agents/workshop/press-chain.md
inline std::size_t action_press_at(const InfoBodyPlace& p, std::int64_t column,
                                   std::int64_t row) {
    if (!p.present || column < 0 || column > p.fit.columns) {
        return kNoAction;
    }
    return action_at_prose_row(p, row);
}

/// ONE SEMANTIC OBJECT ROW AS PROSE — the selection mark, the identity, and as much of the
/// authored name as the body has room for.
// WL-INFO-09 -- agents/workshop/info-body.md
inline std::string object_row_full(const ui::Element& e, bool chosen) {
    return std::string(chosen ? "> " : "  ") + "#" + std::to_string(e.id) + " " + e.label;
}

inline std::string object_row_text(const ui::Element& e, bool chosen, std::int64_t columns) {
    return detail::fit(object_row_full(e, chosen), columns);
}

/// ONE SEMANTIC PROPERTY ROW AS PROSE — the mark, the name, and as much of the value as the
/// body has room for.
// WL-INFO-05 -- agents/workshop/info-body.md
inline std::string property_row_prefix(const Row& row, bool here) {
    return std::string(here ? ">" : " ") +
           detail::pad(row.label(), static_cast<std::size_t>(kPropertyLabelCols));
}

/// THE WHOLE OF WHAT A RESTING ROW WOULD SAY WITH UNLIMITED ROOM -- the mark, the
/// name and the value entire. A LIVE DRAFT HAS NO SUCH ROW, deliberately: a draft is
/// windowed by its own component against its own caret, and there is nothing here to reveal
/// that moving the caret does not already show.
inline std::string property_row_full(const Row& row, bool here) {
    return property_row_prefix(row, here) + row.value();
}

inline std::string property_row_text(const Row& row, bool here, std::int64_t value_columns) {
    std::string text = property_row_prefix(row, here);
    if (row.editing()) {
        return text + row.editor().visible(value_columns);
    }
    return text + detail::fit(row.value(), value_columns);
}

/// THE CARET'S COLUMN IN A BODY ROW: the mark and the name, plus the component's own answer.
// WL-TEXT-13 -- agents/workshop/text-box.md
inline std::int64_t property_caret_column(const Row& row) {
    return kPropertyMarkCols + kPropertyLabelCols +
           static_cast<std::int64_t>(row.editor().caret_column());
}

/// THE DRAFT'S VISIBLE SELECTION AS PROSE COLUMNS OF ITS BODY ROW —
/// `property_caret_column`'s shape for a span, and `terminal_selection_columns`' twin.
// WL-TEXT-13 -- agents/workshop/text-box.md
inline TerminalSelectionSpan property_selection_columns(const Row& row,
                                                        std::int64_t value_columns) {
    const component::TextBox::VisibleSpan vis = row.editor().visible_selection(value_columns);
    if (!vis.present()) {
        return TerminalSelectionSpan{};
    }
    return TerminalSelectionSpan{kPropertyMarkCols + kPropertyLabelCols + vis.begin,
                                 kPropertyMarkCols + kPropertyLabelCols + vis.end, true};
}

/// IS THIS PROSE POSITION ON THE BODY ROW SHOWING PROPERTY `index` AT ALL?
// WL-PRESS-03 -- agents/workshop/press-chain.md
inline bool property_row_hit(const InfoBodyPlace& p, std::size_t index, std::int64_t column,
                             std::int64_t row) {
    return p.present && column >= 0 && column <= p.fit.columns &&
           property_at_prose_row(p, row) == index && index != kNoProperty;
}

/// WHICH OBJECT A PRESS INSIDE THE BODY NAMES, or `kNoObject`.
// WL-INFO-09 -- agents/workshop/info-body.md; WL-PRESS-03 -- agents/workshop/press-chain.md
inline std::size_t object_press_at(const InfoBodyPlace& p, std::int64_t column,
                                   std::int64_t row) {
    if (!p.present || column < 0 || column > p.fit.columns) {
        return kNoObject;
    }
    return object_at_prose_row(p, row);
}

/// A PRESSED COLUMN AS A COLUMN OF THE VALUE. Negative to the left of the value, which
/// `TextBox::position_at_column` reads as "the start of what is shown".
inline constexpr std::int64_t property_value_column(std::int64_t row_column) noexcept {
    return row_column - (kPropertyMarkCols + kPropertyLabelCols);
}

/// THE INFO PANEL — the OBJECTS list and the PROPERTIES inspector, in the column they have
/// always occupied.
// WL-INFO-01, WL-INFO-08 -- agents/workshop/info-body.md
inline void paint_info(surface::SurfaceLayer& layer, const WorkshopDoc& d, const Session& s,
                       const FineRect& b, const Screen& sc,
                       std::int64_t chrome = kPaneChrome) {
    // THE BACKDROP FIRST, so everything below is written over it and nothing authored
    // survives underneath it. One rect, the whole of `b`, and the same call the other two
    // presentations make -- and the part of it the body does not cover is this
    // panel's visible boundary.
    paint_panel_frame(layer, b, chrome);

    // THE BODY IS ONE BOUNDED REGION AND IT HOLDS BOTH LISTS.
    //
    // Everything under `OBJECTS` belongs to it: how many object names there are, where
    // `PROPERTIES` falls, how many properties there are, how wide a value may be, where the
    // caret is, and what neither list is showing. A region is the only shape on this canvas
    // that can be set in the active medium's own type and the only one that can carry an
    // insertion point, and both lists want the first.
    //
    // NOTHING BELOW MULTIPLIES A FONT METRIC. `info_body_place` asked `fit_region` once; the
    // loops spend `objects`/`properties`, `columns` and `value_columns` and know nothing about
    // pixels, faces, insets or line heights. That is what makes "the graphical body shows
    // eleven objects and the terminal body shows twenty" one publisher rather than two.
    //
    // AND NO ROW IS PAINTED THAT THE BODY CANNOT HOLD. Earlier the property loop ran over
    // every property and wrote a label per row, so a population taller than the panel ran off
    // its bottom edge; earlier the object loop was bounded, but by a CONSTANT rather than
    // by the room. Both bounds are windows now, both omissions are counted on the side they
    // happened, and both come from the OBJECTS list's own two functions.
    const InfoBodyPlace body = info_body_place(b, sc, d, s);
    if (body.fit.rows <= 0 || body.fit.columns <= 0) {
        return; // no room for one row of this medium's type: say nothing at all
    }
    surface::SurfaceTextRegion region;
    region.x = body.region_x;
    region.y = body.region_y;
    region.w = body.region_w;
    region.h = body.region_h;
    region.sub_x = body.region_sub_x;
    region.sub_y = body.region_sub_y;
    region.sub_w = body.region_sub_w;
    region.sub_h = body.region_sub_h;
    // `OBJECTS` IS THE REGION'S FIRST PROSE ROW. It used to be an ordinary label on
    // the panel's cell row 0, kept OUT of the body's region because that row was shared
    // with the screen's own terminal hint; the shared top row is retired, the rectangle is
    // whole-panel and wholly this panel's, and the heading is set in whatever type the
    // active medium owns, like everything under it.
    region.rows.push_back(surface::SurfaceTextRow{detail::fit("OBJECTS", body.fit.columns),
                                                  surface::role::kAccent});
    // A PANEL WITH ROOM FOR THE HEADING AND NOTHING ELSE STILL SAYS WHAT IT IS -- the
    // external panes' own rule, one panel over: a rectangle showing a maker nothing at all
    // is the worse of the two answers.
    if (!body.present) {
        layer.texts.push_back(std::move(region));
        return; // no room under the heading: the heading, and no invented room
    }
    // A ROW MAY BE SET ON A GROUND, AND ALMOST NONE OF THEM IS. The ground is
    // defaulted rather than spelled at every call because `role::kNone` is not a value a row
    // could be wrong about -- it is the absence of one, and the picture it draws is the
    // picture every row of this body drew earlier. That is the opposite of
    // `first_visible`, where a default would have let a call site keep an old spelling and be
    // silently right until the first line long enough to scroll: here the two sites that pass
    // one are the whole of what this phase changed, and the default is what makes them read
    // as the exception they are.
    const auto say_row = [&region](std::string text, std::int64_t role,
                                   std::int64_t ground = surface::role::kNone) {
        region.rows.push_back(surface::SurfaceTextRow{std::move(text), role, ground});
    };
    // The markers are in the panel's own muted role because they are the tool's furniture and
    // not authored material: nothing here mints an identity, invents a name, or reorders a
    // document to make a screen fit.
    const auto say_omission = [&](std::size_t how_many, const char* which) {
        if (how_many > 0) {
            say_row(detail::fit(omitted_text(how_many, which), body.columns),
                    surface::role::kMuted);
        }
    };
    // ---- THE FOOTER, WRITTEN ONCE AND EMITTED ON EVERY PATH OUT OF THIS PAINTER -----------
    //
    // The body has two early exits -- an empty document's `(nothing selected)` and the
    // ordinary end -- and a maker in either of those states is exactly the maker who most
    // needs to see that `Create` exists. So the footer is a closure both of them call rather
    // than two copies, and this painter now finishes in one place.
    //
    // THE BLANK ROWS ARE THE SPARE ROOM, and they are written rather than left off because the
    // controls are anchored to `action_row` and a region's rows are positional. It is the same
    // padding the object list's share already gets when it has less to say than it was given.
    const auto say_footer = [&] {
        // `action_row` is a BODY row; the region's rows carry the heading above the body,
        // so every positional bound below is offset by the rows the heading keeps.
        while (region.rows.size() <
               static_cast<std::size_t>(kInfoHeadingRows + body.action_row)) {
            say_row(std::string(), surface::role::kFill);
        }
        for (std::size_t which = 0; which < kActionCount; ++which) {
            const bool pressable = available(action_availability(which, d, s));
            // THE ROLE IS THE SECOND SIGNAL AND NEVER THE ONLY ONE. `kMuted` is this panel's
            // existing word for "furniture, not the maker's material", which is what a control
            // a maker cannot use currently is; the brackets in the text carry the same fact to
            // a medium with no ink to spend. No role was added and none was widened.
            //
            // AND A CONTROL A MAKER CAN USE SITS ON SOMETHING, which is the THIRD
            // signal and still not the only one: `[ ... ]` is what a medium with no ground at
            // all reads, and it is unchanged. The ground is `kMuted` -- the same value the
            // Terminal's completion list has spent on its selected row -- and the
            // reason is a legibility fact each MEDIUM owns rather than a semantic one: it is
            // the one ground in either palette that every ink in `sgr_for_role` /
            // `ink_for_role` reads on, so a publisher may set a row on it without knowing
            // what ink the row's own role resolved to. (Pairing a role with its OWN ground is
            // the mistake this avoids -- `kFill` on `kFill` is white on white in a terminal.)
            //
            // AN UNAVAILABLE CONTROL IS GIVEN NO GROUND AT ALL, which is the whole of what
            // makes the ground say "actionable" rather than "a control is here". Handing it
            // the same slab and a quieter ink would make availability a matter of degree, and
            // a maker would be reading two shades of grey to learn a fact the brackets state
            // outright.
            say_row(action_row_text(which, pressable, body.columns),
                    pressable ? surface::role::kFill : surface::role::kMuted,
                    pressable ? surface::role::kMuted : surface::role::kNone);
        }
        layer.texts.push_back(std::move(region));
    };

    // ---- the objects, named by identity, pointing at the same selection the ring does ----
    //
    // An empty document SAYS it is empty. A maker can reach this state with their own hand by
    // deleting their work, and a panel that merely goes blank is indistinguishable from a tool
    // that has broken. It also says what to do next, because the answer is one key and the
    // alternative is a maker who thinks they have destroyed it. It is a ROW of the body rather
    // than a label beside it, so it is bounded and set in type like everything else here.
    if (d.elements.empty()) {
        say_row(detail::fit("(none) -- n makes one", body.columns), surface::role::kMuted);
    } else {
        say_omission(body.objects.before, "earlier");
        for (std::size_t n = 0; n < body.objects.count; ++n) {
            const std::size_t index = body.objects.first + n;
            const ui::Element& e = d.elements[index];
            const bool chosen = e.id == s.selected;
            // A NAME LONGER THAN THE COLUMN MAY BE READ PAST. The identity keeps the
            // row; what the pointer scrolls is the same string this row was already cutting,
            // and the document is as `const` here as it ever was.
            say_row(detail::reveal_shown(s.reveal, reveal_place::kInfoObject, index,
                                         object_row_full(e, chosen),
                                         object_row_text(e, chosen, body.columns),
                                         body.columns),
                    chosen ? surface::role::kAccent : surface::role::kFill);
        }
        say_omission(body.objects.after, "more");
    }
    // The object list's share is spent whether or not it had that much to say, because the
    // heading below it is at a row the composition chose and not at the row this loop happened
    // to reach. A list that says less than its share leaves blank rows under itself.
    while (region.rows.size() < static_cast<std::size_t>(kInfoHeadingRows) + body.objects_rows) {
        say_row(std::string(), surface::role::kFill);
    }

    // ---- `PROPERTIES`, a row of the body, at the row the composition put it ----
    //
    // AND IT IS SET ON A GROUND, because accent ink alone was not enough to say
    // "a section begins here". It was predicted and a live run confirmed it: the row
    // immediately above `PROPERTIES` is the SELECTED object, which is accent ink too, so the
    // heading and the thing it is not were the same colour on adjacent rows. A ground is the
    // one signal in this vocabulary that says "this row, all of it" -- which is what a
    // boundary is -- and it takes no room, so spent separator row stays spent.
    //
    // THE SAME `kMuted` THE CONTROLS BELOW USE, and that is agreement rather than sharing:
    // the two consumers arrive at one value because each medium offers exactly one ground
    // every ink reads on, not because a heading and a control mean the same thing. What
    // distinguishes them is what each already carried -- accent ink and a section's name
    // against fill ink and a bracketed verb -- so no role was added, none was widened, and
    // there is no `kSectionGround` constant pretending the two decisions are one.
    say_row("PROPERTIES", surface::role::kAccent, surface::role::kMuted);

    // ---- the properties ----
    if (s.rows.empty()) {
        say_row(detail::fit("(nothing selected)", body.columns), surface::role::kMuted);
        say_footer();
        return;
    }
    say_omission(body.properties.before, "earlier");
    for (std::size_t n = 0; n < body.properties.count; ++n) {
        const std::size_t i = body.properties.first + n;
        const Row& row = s.rows[i];
        const bool here = i == s.cursor;
        std::int64_t role = surface::role::kFill;
        if (row.editing()) {
            role = surface::role::kAlert; // a live draft is never quiet
        } else if (!row.editable()) {
            role = surface::role::kMuted; // not the maker's to author
        }
        // A RESTING VALUE LONGER THAN ITS COLUMN MAY BE READ PAST; a LIVE DRAFT
        // may not, and that exclusion is the feature rather than a gap -- a draft is
        // already windowed against its own caret, and a pointer scrolling it would be a
        // second window over one line, fighting the one `keep_caret_visible` reconciles.
        say_row(row.editing() ? property_row_text(row, here, body.value_columns)
                              : detail::reveal_shown(
                                    s.reveal, reveal_place::kInfoProperty, i,
                                    property_row_full(row, here),
                                    property_row_text(row, here, body.value_columns),
                                    body.columns),
                role);
        if (row.editing()) {
            // ONE MEASURER, TWICE OVER. The prose ROW is `prose_row_of_property` -- the same
            // function `property_at_prose_row` inverts for a press -- and the COLUMN is
            // `property_caret_column`, the same offset the row's own text was built with. So
            // a caret cannot land where the text is not, and a click cannot land where the
            // caret would not, on either axis.
            region.caret_row = kInfoHeadingRows + prose_row_of_property(body, i);
            region.caret_col = property_caret_column(row);
            // AND THE DRAFT'S SELECTION, THROUGH THE SAME TWO ANSWERS: the same
            // prose row, and the same value offset the caret's column was built with — so
            // the highlight cannot land where the caret would not, on either axis, in
            // either medium (a band under the glyphs where the body is real type, reverse
            // video over exactly the selected characters where it is cells).
            const TerminalSelectionSpan marked =
                property_selection_columns(row, body.value_columns);
            if (marked.present) {
                region.sel_begin_row = region.caret_row;
                region.sel_begin_col = marked.begin;
                region.sel_end_row = region.caret_row;
                region.sel_end_col = marked.end;
            }
        }
    }
    say_omission(body.properties.after, "more");
    say_footer();
}

// ---- AN EXTERNAL PANE'S BODY: one header row of Workshop's, and a region ---------------

/// One header row, Workshop's own, so the provenance of what follows is legible.
// WL-FOCUS-11 -- agents/workshop/focus.md; WL-PANE-06 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t kExternalHeaderRows = 1;

/// HOW MANY HEADER ROWS THIS PANE'S PRESENTATION RESERVES RIGHT NOW -- the ONE
/// resolution of the title preference, asked by the painter, the press path and the room
/// grant alike. Three parties spending three private answers to this question is a maker
// WL-FOCUS-11 -- agents/workshop/focus.md
inline std::int64_t external_title_rows(const Panels& panels, std::int64_t kind,
                                        bool titles_shown) noexcept {
    return (titles_shown || keyboard_pane(panels) == kind) ? kExternalHeaderRows : 0;
}

/// WHAT A PANE SAYS BEFORE ITS PROVIDER HAS SAID ANYTHING.
inline constexpr const char* kExternalWaiting = "(waiting for the provider)";

/// WHAT A PANE SAYS AFTER AN UPDATE IT COULD NOT KEEP. Workshop's sentence,
/// Workshop's bytes -- nothing of the refused message is echoed, because the
/// thing that was wrong with it was its content.
inline constexpr const char* kExternalRefused =
    "(the last update did not fit this pane's room -- none of it was kept)";

/// THE BODY OF AN EXTERNAL PANEL, RESOLVED ONCE. Where it is, and how much prose the
/// ACTIVE medium fits in it -- which is exactly the budget the provider is granted.
struct ExternalBodyPlace {
    bool present = false;
    /// The panel's bounds as the wire spells them: whole cells (the FLOOR of the fine
    /// coordinate — what a character medium's lattice shows) plus the sub-cell
    /// remainders. `fit` is resolved from the fine value, so the pixel geometry is the
    /// pane's own; the cell halves are what the press inverse and the cell projection
    /// spend.
    std::int64_t region_x = 0;
    std::int64_t region_y = 0;
    std::int64_t region_w = 0;
    std::int64_t region_h = 0;
    std::int64_t region_sub_x = 0;
    std::int64_t region_sub_y = 0;
    std::int64_t region_sub_w = 0;
    std::int64_t region_sub_h = 0;
    surface::RegionFit fit{};
    /// THE HEADER ROWS THIS RESOLUTION RESERVED -- carried so the painter and the press
    /// path spend the number the budget was computed with, never a re-derivation.
    std::int64_t header_rows = 0;
    std::int64_t rows = 0;    ///< prose rows -- the `PaneRoom` budget's first half
    std::int64_t columns = 0; ///< ...and its second
};

/// The body under an external panel's header row: the panel's whole bounds, less that row's
/// share of the PROSE the active medium fits in them.
// WL-CHROME-05 -- agents/workshop/chrome.md
// WL-EDIT-12 -- agents/workshop/editor.md
// WL-PANE-06 -- agents/workshop/panes-and-windows.md
inline ExternalBodyPlace external_body_place(const FineRect& panel, const Screen& sc,
                                             std::int64_t header_rows) {
    ExternalBodyPlace p;
    const PaneInside inside = pane_inside(panel, sc);
    const FineRect inner = inside.rect;
    if (inner.w <= 0 || inner.h <= 0) {
        return p;
    }
    const surface::SurfaceRect wire = wire_rect_of(inner, surface::role::kFill);
    p.region_x = wire.x;
    p.region_y = wire.y;
    p.region_w = wire.w;
    p.region_h = wire.h;
    p.region_sub_x = wire.sub_x;
    p.region_sub_y = wire.sub_y;
    p.region_sub_w = wire.sub_w;
    p.region_sub_h = wire.sub_h;
    p.fit = inside.fit;
    p.header_rows = header_rows;
    p.rows = p.fit.rows > header_rows ? p.fit.rows - header_rows : 0;
    p.columns = p.fit.columns;
    p.present = p.rows > 0 && p.columns > 0;
    return p;
}

/// WHERE A PRESS LANDED IN AN EXTERNAL PANE'S GRANTED ROOM -- the `PaneRoom`
/// lattice, and nothing a provider was not already handed.
// WL-PRESS-04 -- agents/workshop/press-chain.md
struct ExternalPressAt {
    bool named = false;
    std::int64_t row = 0;    ///< a prose row of the BODY: 0 is the row under the header
    std::int64_t column = 0; ///< ...and a prose column of the same region
};

/// LOCATE A PRESS IN THE ROOM A PANE WAS GRANTED, from the rectangle the painter used.
// WL-PRESS-04 -- agents/workshop/press-chain.md
inline ExternalPressAt external_press_at(const Panels& panels, const Setup& setup,
                                         const Screen& sc, std::int64_t kind, bool titles,
                                         std::int64_t space, std::int64_t x, std::int64_t y) {
    const PanelBounds where = bounds_of(panels, setup, kind, sc);
    if (!where.open) {
        return ExternalPressAt{};
    }
    const ExternalBodyPlace body =
        external_body_place(where.rect, sc, external_title_rows(panels, kind, titles));
    if (!body.present) {
        return ExternalPressAt{};
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    if (!at.understood) {
        return ExternalPressAt{};
    }
    const std::int64_t row = at.row - body.header_rows;
    if (row < 0 || row >= body.rows || at.column < 0 || at.column >= body.columns) {
        return ExternalPressAt{};
    }
    return ExternalPressAt{true, row, at.column};
}

/// IT SAYS WHETHER TYPING GOES HERE, which is a repair with a live cost behind it.
// WL-FOCUS-10 -- agents/workshop/focus.md
inline constexpr const char* kTypingHere = "> ";
inline constexpr const char* kTypingElsewhere = "  ";

/// THE HEADER: what this pane is, and WHOSE it is -- both halves validated at admission,
/// neither echoed raw -- and whether typing goes here, said by a mark that costs no columns.
// WL-EDIT-12 -- agents/workshop/editor.md; WL-FOCUS-10 -- agents/workshop/focus.md
inline std::string external_header(const RuntimePane& row, bool typing) {
    return std::string(typing ? kTypingHere : kTypingElsewhere) + row.name + " @" +
           row.provider;
}

/// ONE EXTERNAL PANEL: Workshop's backdrop, Workshop's header, and ONE region carrying
/// whatever that office last validly said inside the room it was granted.
// WL-FOCUS-10 -- agents/workshop/focus.md; WL-PANE-06 -- agents/workshop/panes-and-windows.md
inline void paint_external(surface::SurfaceLayer& layer, const Panels& panels, std::int64_t kind,
                           const FineRect& b, const Screen& sc, bool titles,
                           std::int64_t chrome = kPaneChrome) {
    paint_panel_frame(layer, b, chrome);
    const RuntimePane* row = panels.runtime.of_kind(kind);
    if (row == nullptr) {
        return; // an open kind with no catalog row cannot happen; drawing a lie could
    }
    const ExternalBodyPlace body =
        external_body_place(b, sc, external_title_rows(panels, kind, titles));
    if (body.fit.rows <= 0 || body.fit.columns <= 0) {
        return; // no room for one row of this medium's type: say nothing at all
    }
    surface::SurfaceTextRegion region;
    region.x = body.region_x;
    region.y = body.region_y;
    region.w = body.region_w;
    region.h = body.region_h;
    region.sub_x = body.region_sub_x;
    region.sub_y = body.region_sub_y;
    region.sub_w = body.region_sub_w;
    region.sub_h = body.region_sub_h;
    // WORKSHOP'S HEADER IS THE REGION'S FIRST ROW, so the provenance line and the
    // provider's sentences are the same kind of text in whatever face this medium owns. It is
    // fitted to the region's own columns, which is what marks its cut. the row
    // exists exactly when the resolution reserved one: hidden titles return it to the
    // provider, and the pane holding the keyboard keeps its title -- and with it the `> `
    // mark -- whatever the preference says (`external_title_rows`).
    if (body.header_rows > 0) {
        region.rows.push_back(surface::SurfaceTextRow{
            detail::fit(external_header(*row, keyboard_pane(panels) == kind), body.columns),
            surface::role::kAccent});
    }
    // A PANE WITH ROOM FOR THE HEADER AND NOTHING ELSE STILL SAYS WHOSE IT IS. `present` is
    // the question "was a body granted", and it is asked AFTER the header is written rather
    // than before it -- a rectangle showing a maker nothing at all is the worse of the two
    // answers, and it is what this painter gave for one frame when the header stopped being
    // a cell row of its own.
    if (!body.present) {
        if (!region.rows.empty()) {
            layer.texts.push_back(std::move(region));
        }
        return; // no room under the heading: the heading, and no invented room
    }
    const ExternalPane* pane = panels.external_pane(kind);
    if (pane == nullptr) {
        if (!region.rows.empty()) {
            layer.texts.push_back(std::move(region));
        }
        return;
    }
    if (!pane->refusal.empty()) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(pane->refusal, body.columns),
                                                      surface::role::kAlert});
    } else if (!pane->heard) {
        region.rows.push_back(surface::SurfaceTextRow{
            detail::fit(kExternalWaiting, body.columns), surface::role::kMuted});
    } else {
        region.rows.insert(region.rows.end(), pane->shown.begin(), pane->shown.end());
    }
    layer.texts.push_back(std::move(region));
}

// ---- THE SOURCE EDITOR'S PANE: one document, projected through a viewport ---------------
// WL-EDIT-12 -- agents/workshop/editor.md

inline constexpr std::int64_t kEditorHeaderRows = 1;

/// ONE COLUMN OF EVERY BODY ROW THE TEXT MAY NOT USE -- `kTerminalCaretCols`' rule, for
/// its reason.
// WL-EDIT-08 -- agents/workshop/editor.md
inline constexpr std::int64_t kEditorCaretCols = 1;

/// The editor body's resolved place on this screen: the pane's rectangle less its header,
/// as prose. Absent whenever the pane is closed, off-room, or too small for one row.
inline ExternalBodyPlace editor_body(const Session& s, const Screen& sc) {
    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kEditor, sc);
    if (!where.open) {
        return ExternalBodyPlace{};
    }
    return external_body_place(where.rect, sc, kEditorHeaderRows);
}

/// The columns of the body a LINE may spend -- the body's columns less the caret's one.
inline constexpr std::int64_t editor_text_columns(const ExternalBodyPlace& body) noexcept {
    const std::int64_t text = body.columns - kEditorCaretCols;
    return text > 0 ? text : 0;
}

/// THE HEADER: whether the buffer matches the file, where the caret is, and what is
/// being edited -- in the order the facts must survive `detail::fit`'s TAIL cut.
// WL-EDIT-12 -- agents/workshop/editor.md
inline std::string editor_header(const EditorState& e, bool typing) {
    std::string head = std::string(typing ? kTypingHere : kTypingElsewhere);
    if (!e.open_document()) {
        return head + "Editor -- no source open";
    }
    head += std::string("Editor ") + (e.dirty() ? "UNSAVED" : "saved");
    head += " L" + std::to_string(e.buffer.caret_row() + 1) + ":C" +
            std::to_string(visual_col_of(e.buffer.line(e.buffer.caret_row()),
                                         e.buffer.caret_byte()) +
                           1);
    head += "/" + std::to_string(e.buffer.line_count());
    head += " -- " + e.path;
    return head;
}

/// KEEP THE VIEWPORT TRUE AGAINST THE ROOM AND THE DOCUMENT IT HAS NOW -- the editor's
/// member of the once-per-repaint reconcile family (`refresh_terminal`'s argument, two
// WL-EDIT-09 -- agents/workshop/editor.md
inline void reconcile_editor_view(Session& s) {
    EditorState& e = s.editor;
    if (!e.open_document()) {
        return;
    }
    const ExternalBodyPlace body = editor_body(s, screen_of(s));
    if (!body.present) {
        return; // no presented body: the viewport keeps its answer for the room to come
    }
    const bool resized = body.rows != e.last_rows || body.columns != e.last_cols;
    e.last_rows = body.rows;
    e.last_cols = body.columns;
    const std::size_t rows = static_cast<std::size_t>(body.rows);
    const std::size_t total = e.buffer.line_count();
    const std::size_t furthest_row = total > rows ? total - rows : 0;
    if (e.first_row > furthest_row) {
        e.first_row = furthest_row; // rule 1's vertical half: no blank rows below while
    }                               // lines are hidden above
    if (e.first_col < 0) {
        e.first_col = 0;
    }
    if (!e.follow_caret && !resized) {
        return;
    }
    e.follow_caret = false;
    const std::size_t cr = e.buffer.caret_row();
    if (cr < e.first_row) {
        e.first_row = cr;
    }
    if (cr >= e.first_row + rows) {
        e.first_row = cr + 1 - rows;
    }
    const std::int64_t text_cols = editor_text_columns(body);
    if (text_cols <= 0) {
        return;
    }
    const std::string& line = e.buffer.line(cr);
    const std::int64_t vis = visual_col_of(line, e.buffer.caret_byte());
    // Rule 1's horizontal half, measured on the caret's own line: no blank room at the
    // right while its text is hidden at the left, so erasing a long line back down
    // recovers the room it freed.
    const std::int64_t need = visual_len(line) + kEditorCaretCols;
    const std::int64_t furthest_col = need > text_cols ? need - text_cols : 0;
    if (e.first_col > furthest_col) {
        e.first_col = furthest_col;
    }
    if (vis < e.first_col) {
        e.first_col = vis;
    }
    if (vis - e.first_col > text_cols) {
        e.first_col = vis - text_cols;
    }
}

/// WHERE A PRESS LANDED IN THE EDITOR'S BODY -- `external_press_at`'s shape for the one
/// built-in whose body is a document.
// WL-EDIT-08 -- agents/workshop/editor.md
struct EditorPressAt {
    bool named = false;
    std::int64_t row = 0;    ///< a prose row of the BODY: 0 is the row under the header
    std::int64_t column = 0; ///< a displayed column of the viewport's window
};

inline EditorPressAt editor_press_at(const Session& s, const Screen& sc, std::int64_t space,
                                     std::int64_t x, std::int64_t y) {
    const ExternalBodyPlace body = editor_body(s, sc);
    if (!body.present) {
        return EditorPressAt{};
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    if (!at.understood) {
        return EditorPressAt{};
    }
    const std::int64_t row = at.row - body.header_rows;
    if (row < 0 || row >= body.rows || at.column < 0 || at.column > body.columns) {
        return EditorPressAt{};
    }
    return EditorPressAt{true, row, at.column};
}

/// IS THIS POSITION OVER THE EDITOR'S TEXT BODY -- the wheel's one question. The header
/// row is not the body; the column is not asked, because a wheel aimed at the pane's
/// body is aimed at the document however far right of its last character it sits.
inline bool over_editor_body(const Session& s, const Screen& sc, std::int64_t space,
                             std::int64_t x, std::int64_t y) {
    const ExternalBodyPlace body = editor_body(s, sc);
    if (!body.present) {
        return false;
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    return at.understood && at.row >= body.header_rows && at.row < body.fit.rows;
}

/// THE EDITOR, PAINTED: the frame, the header, and the document through the viewport --
/// one region, so the caret and the selection are the REGION's and each medium answers
/// in cells).
// WL-EDIT-12 -- agents/workshop/editor.md
inline void paint_editor(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                         const Screen& sc, std::int64_t chrome = kPaneChrome) {
    paint_panel_frame(layer, b, chrome);
    const ExternalBodyPlace body = external_body_place(b, sc, kEditorHeaderRows);
    if (body.fit.rows <= 0 || body.fit.columns <= 0) {
        return; // no room for one row of this medium's type: say nothing at all
    }
    surface::SurfaceTextRegion region;
    region.x = body.region_x;
    region.y = body.region_y;
    region.w = body.region_w;
    region.h = body.region_h;
    region.sub_x = body.region_sub_x;
    region.sub_y = body.region_sub_y;
    region.sub_w = body.region_sub_w;
    region.sub_h = body.region_sub_h;
    const EditorState& e = s.editor;
    if (body.header_rows > 0) {
        region.rows.push_back(surface::SurfaceTextRow{
            detail::fit(editor_header(e, editor_has_keyboard(s)), body.columns),
            surface::role::kAccent});
    }
    if (!body.present) {
        if (!region.rows.empty()) {
            layer.texts.push_back(std::move(region));
        }
        return; // room for the heading and nothing else: the heading, honestly
    }
    if (!e.open_document()) {
        // THE ABSENCE IS THE HEADER'S SENTENCE AND IS NOT SAID TWICE. This row
        // used to read `no source open -- e opens the Builder's chosen recipe`: half of it
        // repeated `editor_header`'s own `Editor -- no source open`, and the other half
        // taught a key `document.open`'s catalog row already owns. The header still states
        // the absence, the hotkey view still teaches the gesture, and the pane gives the
        // row back to the document it is waiting for.
        layer.texts.push_back(std::move(region));
        return;
    }
    const std::int64_t text_cols = editor_text_columns(body);
    const std::size_t total = e.buffer.line_count();
    const std::size_t rows = static_cast<std::size_t>(body.rows);
    const std::size_t last = e.first_row + rows < total ? e.first_row + rows : total;
    for (std::size_t r = e.first_row; r < last; ++r) {
        region.rows.push_back(surface::SurfaceTextRow{
            expanded_slice(e.buffer.line(r), e.first_col, text_cols), surface::role::kFill});
    }
    // THE CARET, WHEN ITS ROW IS IN THE WINDOW -- region prose coordinates, the header
    // counted, the column in the viewport's own displayed lattice.
    const std::size_t cr = e.buffer.caret_row();
    if (cr >= e.first_row && cr < last) {
        const std::int64_t vis = visual_col_of(e.buffer.line(cr), e.buffer.caret_byte());
        std::int64_t col = vis - e.first_col;
        if (col >= 0 && col <= text_cols) {
            region.caret_row = body.header_rows + static_cast<std::int64_t>(cr - e.first_row);
            region.caret_col = col;
        }
    }
    // THE SELECTION, CLAMPED INTO THE WINDOW. The range travels in reading order on the
    // region (begin inclusive, end exclusive) and `selection_span_of_row` does the
    // per-row arithmetic in both media; what this clamps is only the part outside the
    // viewport, which no medium could show.
    if (e.buffer.has_selection()) {
        const EditorPos from = e.buffer.selection_begin();
        const EditorPos to = e.buffer.selection_end();
        if (from.row < last && to.row >= e.first_row) {
            std::int64_t brow;
            std::int64_t bcol;
            if (from.row < e.first_row) {
                brow = body.header_rows;
                bcol = 0;
            } else {
                brow = body.header_rows + static_cast<std::int64_t>(from.row - e.first_row);
                const std::int64_t v =
                    visual_col_of(e.buffer.line(from.row), from.byte) - e.first_col;
                bcol = v < 0 ? 0 : (v > text_cols ? text_cols : v);
            }
            std::int64_t erow;
            std::int64_t ecol;
            if (to.row >= last) {
                erow = body.header_rows + static_cast<std::int64_t>(last - e.first_row);
                ecol = 0;
            } else {
                erow = body.header_rows + static_cast<std::int64_t>(to.row - e.first_row);
                const std::int64_t v =
                    visual_col_of(e.buffer.line(to.row), to.byte) - e.first_col;
                ecol = v < 0 ? 0 : (v > text_cols ? text_cols : v);
            }
            if (erow > brow || ecol > bcol) {
                region.sel_begin_row = brow;
                region.sel_begin_col = bcol;
                region.sel_end_row = erow;
                region.sel_end_col = ecol;
            }
        }
    }
    layer.texts.push_back(std::move(region));
}

// ---- THE PROJECT BROWSER, PRESENTED -----------------------------------------------------

inline constexpr std::int64_t kFilesHeaderRows = 1;

/// How many rows the wheel is worth in a cursor-windowed list -- the editor's number, for
/// its reason.
// WL-EDIT-10 -- agents/workshop/editor.md
inline constexpr std::int64_t kListWheelRows = 3;
inline constexpr std::int64_t kFilesWheelRows = kListWheelRows;

/// TURN NOTCHES INTO WHOLE ROWS, CARRYING THE FRACTION.
// WL-EDIT-10 -- agents/workshop/editor.md
inline std::int64_t spend_wheel(double& accum, double dy, std::int64_t rows_per_notch) {
    accum += dy * static_cast<double>(rows_per_notch);
    const std::int64_t rows = static_cast<std::int64_t>(accum);
    accum -= static_cast<double>(rows);
    return rows;
}

/// The browser body's resolved place on this screen: the pane's rectangle less its header.
/// Absent whenever the pane is closed, off-room, or too small for one row.
inline ExternalBodyPlace files_body(const Session& s, const Screen& sc) {
    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kProjectFiles, sc);
    if (!where.open) {
        return ExternalBodyPlace{};
    }
    return external_body_place(where.rect, sc, kFilesHeaderRows);
}

/// THE HEADER: where the maker is, whether this place is one they might have meant, and
/// how far into the listing -- in the order the facts must survive the cut, which is the
// WL-PROJ-10 -- agents/workshop/project.md
inline std::string files_header_prefix(const FilesPane& pane, const std::string& why,
                                       bool typing) {
    const std::size_t total = pane.listing.rows.size();
    std::string out = "Files";
    if (typing) {
        out += " *"; // the keys are here -- the Editor header's own mark, same voice
    }
    if (!pane.listing.known) {
        out += " --";
    } else if (total == 0) {
        out += " empty";
    } else {
        const std::size_t at = pane.cursor < total ? pane.cursor + 1 : total;
        out += " " + std::to_string(at) + "/" + std::to_string(total);
        // A BOUND CLAIMS WHAT IT READ. The walk stopped at the ceiling, so what
        // stands beside the count is the fact that counting STOPPED -- never a total this
        // browser never reached, and never a fraction of one.
        if (pane.listing.bounded) {
            out += "+ (stopped counting)";
        }
    }
    if (!why.empty()) {
        out += "  " + why;
    }
    out += "  ";
    return out;
}

/// WHERE THE BROWSER IS, AS A LOCATION -- absolute, or the one word for the absence.
inline std::string files_location(const FilesPane& pane) {
    return pane.current_dir.empty() ? std::string("nowhere") : pane.current_dir;
}

/// THE HEADER WITH NOTHING TAKEN OFF: everything the painter is holding for this row.
inline std::string files_header_full(const FilesPane& pane, const std::string& why,
                                     bool typing) {
    return files_header_prefix(pane, why, typing) + files_location(pane);
}

inline std::string files_header(const FilesPane& pane, const std::string& why, bool typing,
                                std::int64_t columns) {
    const std::string out = files_header_prefix(pane, why, typing);
    return out +
           detail::fit_path(files_location(pane), columns - static_cast<std::int64_t>(out.size()));
}

/// ONE ROW'S TEXT: the name, a directory marked as one, and a name this application cannot
/// carry marked as that. The two marks are deliberately different words, because they are
// WL-FILES-04, WL-FILES-10 -- agents/workshop/files.md
inline std::string files_row_text(const FileRow& row) {
    std::string out = shown_name(row.name);
    if (row.directory) {
        out += "/";
        if (row.linked) {
            out += "  (link)";
        }
    }
    if (!row.openable) {
        out += "  (name this Workshop cannot open)";
    }
    return out;
}

/// THE WHOLE ROW, CURSOR MARK INCLUDED -- what the browser is holding for one listed
/// name before the body's width has any say in it.
inline std::string files_row_full(const FileRow& row, bool here) {
    return std::string(here ? "> " : "  ") + files_row_text(row);
}

/// WHERE A PRESS LANDED IN THE BROWSER'S BODY -- `editor_press_at`'s shape, answering a
/// row of the WINDOW rather than a position in a document. The header is subtracted here
/// because the resolution reserved it there.
struct FilesPressAt {
    bool named = false;
    std::int64_t row = 0; ///< a prose row of the BODY: 0 is the row under the header
};

inline FilesPressAt files_press_at(const Session& s, const Screen& sc, std::int64_t space,
                                   std::int64_t x, std::int64_t y) {
    const ExternalBodyPlace body = files_body(s, sc);
    if (!body.present) {
        return FilesPressAt{};
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    if (!at.understood) {
        return FilesPressAt{};
    }
    const std::int64_t row = at.row - body.header_rows;
    if (row < 0 || row >= body.rows) {
        return FilesPressAt{};
    }
    return FilesPressAt{true, row};
}

/// IS THIS POSITION OVER THE BROWSER'S BODY -- the wheel's one question, `over_editor_body`
/// exactly: the header row is not the body, and the column is not asked.
inline bool over_files_body(const Session& s, const Screen& sc, std::int64_t space,
                            std::int64_t x, std::int64_t y) {
    const ExternalBodyPlace body = files_body(s, sc);
    if (!body.present) {
        return false;
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    return at.understood && at.row >= body.header_rows && at.row < body.fit.rows;
}

/// WHICH LISTING ROW A BODY ROW SHOWS, for the press inverse -- the SAME window the painter
/// walks, resolved from the same three numbers. It is a function rather than a remembered
// WL-PRESS-03 -- agents/workshop/press-chain.md
inline bool files_row_of_body_row(const FilesPane& pane, std::int64_t body_rows,
                                  std::int64_t body_row, std::size_t& out) {
    if (body_rows <= 0 || body_row < 0) {
        return false;
    }
    const std::size_t total = pane.listing.rows.size();
    const ListWindow win = list_window(total, pane.cursor, static_cast<std::size_t>(body_rows));
    const std::int64_t first_entry_row = win.before > 0 ? 1 : 0;
    const std::int64_t offset = body_row - first_entry_row;
    if (offset < 0 || offset >= static_cast<std::int64_t>(win.count)) {
        return false;
    }
    out = win.first + static_cast<std::size_t>(offset);
    return out < total;
}

// ---- Which revealable row the pointer is on ----------------------------------------------
// WL-PTR-05, WL-PTR-08 -- agents/workshop/pointer.md

/// WHAT THE POINTER IS OVER, if it is over a revealable row at all.
struct RevealAt {
    bool present = false;
    std::int64_t place = reveal_place::kNone;
    std::size_t item = 0;
    std::string text;        ///< the WHOLE row -- what the painter holds before its width
    std::string rest;        ///< ...and what it shows when nobody is pointing
    std::int64_t columns = 0;
    std::int64_t column = 0; ///< where along the row's own prose the hand is
    /// IS ANYTHING ACTUALLY HIDDEN HERE? Presentation differing from what it presents is the
    /// whole of the question, so a fitted row, a path-fitted row and a row cut by some later
    /// measurer all answer it the same way without this having to know which cut it.
    bool clipped() const noexcept { return present && rest != text; }
};

inline RevealAt files_reveal_at(const Session& s, const Screen& sc, std::int64_t space,
                                std::int64_t x, std::int64_t y) {
    const ExternalBodyPlace body = files_body(s, sc);
    if (!body.present) {
        return RevealAt{};
    }
    const ProseAt at = prose_at(space, x, y, body.region_x, body.region_y, body.fit);
    if (!at.understood || at.column < 0 || at.column >= body.columns) {
        return RevealAt{};
    }
    const FilesPane& pane = s.panels.files;
    if (body.header_rows > 0 && at.row >= 0 && at.row < body.header_rows) {
        const std::string why = provenance_words(s.marks.provenance(pane.current_dir));
        const bool typing = files_has_keyboard(s);
        return RevealAt{true,
                        reveal_place::kFilesLocation,
                        0,
                        files_header_full(pane, why, typing),
                        detail::fit(files_header(pane, why, typing, body.columns), body.columns),
                        body.columns,
                        at.column};
    }
    if (!pane.listing.known || pane.listing.rows.empty()) {
        return RevealAt{}; // a refusal or an emptiness is the browser's own sentence
    }
    std::size_t index = 0;
    if (!files_row_of_body_row(pane, body.rows, at.row - body.header_rows, index)) {
        return RevealAt{}; // a marker row, the padding, or no row at all
    }
    const std::string full = files_row_full(pane.listing.rows[index], index == pane.cursor);
    return RevealAt{true,          reveal_place::kFilesRow, index, full,
                    detail::fit(full, body.columns), body.columns, at.column};
}

inline RevealAt info_reveal_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                               std::int64_t x, std::int64_t y) {
    const InfoBodyAt where = info_body_at(d, s, space, x, y);
    if (!where.present || where.at.column < 0 || where.at.column >= where.body.columns) {
        return RevealAt{};
    }
    const std::size_t object = object_at_prose_row(where.body, where.at.row);
    if (object != kNoObject && object < d.elements.size()) {
        const ui::Element& e = d.elements[object];
        const std::string full = object_row_full(e, e.id == s.selected);
        return RevealAt{true,
                        reveal_place::kInfoObject,
                        object,
                        full,
                        detail::fit(full, where.body.columns),
                        where.body.columns,
                        where.at.column};
    }
    const std::size_t property = property_at_prose_row(where.body, where.at.row);
    if (property == kNoProperty || property >= s.rows.size()) {
        return RevealAt{};
    }
    const Row& row = s.rows[property];
    if (row.editing()) {
        return RevealAt{}; // a draft owns its own window; see `paint_info`
    }
    const bool here = property == s.cursor;
    return RevealAt{true,
                    reveal_place::kInfoProperty,
                    property,
                    property_row_full(row, here),
                    property_row_text(row, here, where.body.value_columns),
                    where.body.columns,
                    where.at.column};
}

/// THE ONE ANSWER A MOTION SPENDS: which revealable row -- of any surface -- this position is
/// on. The occupancy walk is asked FIRST and once, so a surface can only answer for cells it
/// actually owns on this screen.
inline RevealAt reveal_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                          std::int64_t x, std::int64_t y) {
    const Screen sc = screen_of(s);
    const PointedAt at = canvas_point_of(space, x, y);
    if (!at.understood) {
        return RevealAt{};
    }
    const Occupancy here = occupied_at(s.panels, s.setup.active, sc, at);
    if (!here.occupied) {
        return RevealAt{};
    }
    if (here.kind == panel::kProjectFiles) {
        return files_reveal_at(s, sc, space, x, y);
    }
    if (here.kind == panel::kInfo) {
        return info_reveal_at(d, s, space, x, y);
    }
    return RevealAt{};
}

/// WHAT THE SESSION SHOULD HOLD FOR THIS POSITION -- the record `Revealed` keeps, or an empty
/// one where there is nothing to read past. Pure: the weave compares it with what it already
/// had and repaints only on a difference, so a hand moving along one row costs one repaint per
/// column it actually changes and a hand moving over furniture costs none.
inline Revealed reveal_for(const WorkshopDoc& d, const Session& s, std::int64_t space,
                           std::int64_t x, std::int64_t y) {
    const RevealAt at = reveal_at(d, s, space, x, y);
    if (!at.clipped()) {
        return Revealed{};
    }
    return Revealed{at.place, at.item, at.text,
                    detail::reveal_offset_at_column(at.text, at.columns, at.column)};
}

/// THE PROJECT BROWSER, PAINTED: the frame, the header, and one directory's rows through
/// the shared list window. Every branch here says something -- an absent project, a
/// directory that would not open, and an empty directory are three different facts and a
/// pane that showed blankness for all three would be hiding two of them.
inline void paint_files(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                        const Screen& sc, const Keymap& k,
                        std::int64_t chrome = kPaneChrome) {
    paint_panel_frame(layer, b, chrome);
    const ExternalBodyPlace body = external_body_place(b, sc, kFilesHeaderRows);
    if (body.fit.rows <= 0 || body.fit.columns <= 0) {
        return; // no room for one row of this medium's type: say nothing at all
    }
    surface::SurfaceTextRegion region;
    region.x = body.region_x;
    region.y = body.region_y;
    region.w = body.region_w;
    region.h = body.region_h;
    region.sub_x = body.region_sub_x;
    region.sub_y = body.region_sub_y;
    region.sub_w = body.region_sub_w;
    region.sub_h = body.region_sub_h;
    const FilesPane& pane = s.panels.files;
    const auto say = [&region, &body](const std::string& text, std::int64_t role) {
        region.rows.push_back(surface::SurfaceTextRow{detail::fit(text, body.columns), role});
    };
    if (body.header_rows > 0) {
        // THE LOCATION IS THE ROW A POINTER MAY READ PAST. The full spelling is what
        // this painter is already holding; the fitted one is the same answer it has always
        // given; `reveal_shown` chooses between them and the choice is the pointer's.
        const std::string why = provenance_words(s.marks.provenance(pane.current_dir));
        const bool typing = files_has_keyboard(s);
        say(detail::reveal_shown(s.reveal, reveal_place::kFilesLocation, 0,
                                 files_header_full(pane, why, typing),
                                 files_header(pane, why, typing, body.columns), body.columns),
            surface::role::kAccent);
    }
    if (!body.present) {
        if (!region.rows.empty()) {
            layer.texts.push_back(std::move(region));
        }
        return; // room for the heading and nothing else: the heading, honestly
    }
    if (!pane.listing.known) {
        say(pane.listing.refusal.empty() ? std::string("nothing has been listed yet")
                                         : pane.listing.refusal,
            surface::role::kMuted);
        layer.texts.push_back(std::move(region));
        return;
    }
    if (pane.listing.rows.empty()) {
        say("this directory is empty", surface::role::kMuted);
        layer.texts.push_back(std::move(region));
        return;
    }
    const std::size_t rows = static_cast<std::size_t>(body.rows);
    const ListWindow win = list_window(pane.listing.rows.size(), pane.cursor, rows);
    if (win.before > 0) {
        say("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
    }
    for (std::size_t i = win.first; i < win.first + win.count; ++i) {
        const bool here = i == pane.cursor;
        const FileRow& row = pane.listing.rows[i];
        const std::string full = files_row_full(row, here);
        say(detail::reveal_shown(s.reveal, reveal_place::kFilesRow, i, full, full,
                                 body.columns),
            here ? surface::role::kAccent
                 : (row.openable ? surface::role::kFill : surface::role::kMuted));
    }
    if (win.after > 0) {
        say("  " + omitted_text(win.after, "more"), surface::role::kMuted);
    }
    // THE GESTURES ARE NOT PAINTED HERE. What Return and Backspace do in this pane is the
    // band's to say, from the one action truth (`help_rows` over `KeyContext::kFiles`), so
    // a maker who remapped them reads their own bindings rather than this file's guess --
    // and the pane spends every row it has on the project instead of on instructions.
    (void)k;
    layer.texts.push_back(std::move(region));
}

/// THE AFFORDANCE RINGS ARE THE ARRANGEMENT STATE MADE VISIBLE.
// WL-ARR-09 -- agents/workshop/arrangement.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
// WL-FRONT-01 -- agents/workshop/planes.md
inline void paint_pane_affordances(surface::SurfaceLayer& layer, const Session& s,
                                   const Screen& sc) {
    if (!s.arrange.open) {
        return;
    }
    const auto ring = [&](const PaneRef& ref, bool emphasized) {
        const std::optional<std::int64_t> kind = resolve_pane(ref, s.panels);
        // EVERY PANE WHOSE PLACE IS THE MAKER'S TO AUTHOR WEARS HANDLES. This
        // named the overlay stack while the stack was the only such place, which made the
        // ring a list rather than the rule it is; `place_is_authorable` is the same
        // exclusion the arrangement admission already spoke -- the side column is the
        // screen's, and a pane whose geometry no gesture can change must not advertise
        // eight grips that all refuse.
        if (!kind.has_value() || !place_is_authorable(placement_of(*kind))) {
            return;
        }
        const PanelBounds where = bounds_of(s.panels, s.setup.active, *kind, sc);
        if (!where.open || where.rect.w <= 0 || where.rect.h <= 0) {
            return;
        }
        const bool held = s.pane_drag.active && s.pane_drag.sizing && s.pane_drag.pane == ref;
        for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
            const FineRect at = pane_edge_cell(where.rect, edge);
            const bool chosen = held ? s.pane_drag.edge == edge : emphasized;
            // THE WIRE SPELLING, cells plus remainders (`wire_rect_of`'s decomposition):
            // a label's x/y ARE canvas cells, and the fine-lattice construction that handed
            // them raw sub-units put every mark off the canvas -- rings that hit
            // correctly and painted nowhere, the exact see/grab split one geometry forbids.
            const std::int64_t cx = surface::cell_of_subs(at.x);
            const std::int64_t cy = surface::cell_of_subs(at.y);
            layer.labels.push_back(surface::SurfaceLabel{
                cx, cy, std::string(pane_edge_glyph(edge)),
                chosen ? surface::role::kAccent : surface::role::kMuted,
                at.x - surface::subs_of_cells(cx), at.y - surface::subs_of_cells(cy)});
        }
    };
    if (!s.arrange.desk) {
        if (s.arrange.addressed()) {
            ring(s.arrange.pane, true);
        }
        return;
    }
    for (const SetupPane& row : s.setup.active.panes) {
        ring(row.ref, s.arrange.addressed() && row.ref == s.arrange.pane);
    }
}

namespace detail {

/// ONE PLANE FOR ONE PRESENTATION — offered unconditionally, and taken back if that
/// presentation turns out to draw nothing.
// WL-FRONT-01 -- agents/workshop/planes.md
template <typename Paint>
inline void on_own_layer(surface::SurfaceCanvas& c, Paint&& paint_it) {
    c.layers.emplace_back();
    paint_it(c.layers.back());
    const surface::SurfaceLayer& drawn = c.layers.back();
    if (drawn.rects.empty() && drawn.labels.empty() && drawn.texts.empty()) {
        c.layers.pop_back();
    }
}

} // namespace detail

// ---- THE SETUP LINE: which arrangement this is, and whether it is written down -----------

/// What the one-line name editor puts before and after the name a maker is typing. The
/// hint is spelled from the effective keymap, like every other gesture claim.
inline constexpr const char* kSetupNamePrompt = "layout name> ";
inline std::string setup_name_hint(const Keymap& k) {
    return "  " + hotkey_text(k, Act::kNamingCommit) + " renames  " +
           hotkey_text(k, Act::kNamingCancel) + " cancels";
}

/// The two gestures the setup line advertises, on the line the thing they act on is on --
/// the `[+ panel]  p` precedent.
// WL-KEY-02 -- agents/workshop/keyboard.md
inline std::string setup_hints(const Keymap& k) {
    return hotkey_text(k, Act::kSetupSave) + " save  " +
           hotkey_text(k, Act::kSetupRestore) + " restore";
}

/// The fewest columns the name editor will claim for the name itself, so that a surface
/// narrow enough for the chrome to exceed it still shows some of what is being typed.
inline constexpr std::int64_t kSetupNameMinCols = 8;

/// THE ROWS THE SCREEN RESERVES AT THE TOP, AS A RECTANGLE -- and that is the
/// LAYOUTS PANE'S DEVELOPER DEFAULT rather than a band's private geometry.
// WL-FRONT-03 -- agents/workshop/planes.md
inline constexpr ui::Rect top_band_bounds(const Screen& sc) noexcept {
    return placement_bounds(placement::kTopBand, 0, sc);
}

/// WHERE THE LAYOUTS PANE'S CONTENT GOES AND WHAT FITS IN IT -- one resolution,
/// spent by the painter, by the tab press inverse and by the name editor's own window.
// WL-TAB-01, WL-TAB-05 -- agents/workshop/tab-run.md
// WL-CHROME-05 -- agents/workshop/chrome.md
// WL-PRESS-05 -- agents/workshop/press-chain.md
inline ExternalBodyPlace layouts_body(const Session& s, const Screen& sc) {
    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kLayouts, sc);
    if (!where.open) {
        return ExternalBodyPlace{};
    }
    return external_body_place(where.rect, sc, 0);
}

/// THE BOTTOM BAND'S RECTANGLE AND ITS FIT -- what the tool just said, and what the keys
/// mean right now, composed against whatever the ACTIVE medium answers for these cells
/// through the same `fit_region` every bounded region resolves with.
// WL-FRONT-02, WL-FRONT-03 -- agents/workshop/planes.md; WL-RGN-03 -- agents/workshop/regions.md
inline constexpr ui::Rect band_bounds(const Screen& sc) noexcept {
    return ui::Rect{0, sc.h - kBottomRows, sc.w, kBottomRows};
}

inline constexpr surface::RegionFit band_fit(const Screen& sc) noexcept {
    const ui::Rect b = band_bounds(sc);
    return surface::fit_region(b.x, b.y, b.w, b.h, sc.text_advance_px, sc.text_line_px);
}

/// HOW MUCH OF THE NAME THE ONE-LINE EDITOR CAN SHOW at this extent -- the one measurer, so
/// the window the `component::TextBox` is kept against and the slice the painter cuts are the
/// same number. A second copy of this arithmetic is how a caret comes to sit off the end of
// WL-TEXT-03 -- agents/workshop/text-box.md
inline std::int64_t setup_name_columns(const Session& s, const Screen& sc) {
    const std::int64_t chrome =
        static_cast<std::int64_t>(std::char_traits<char>::length(kSetupNamePrompt)) +
        static_cast<std::int64_t>(setup_name_hint(s.keymap).size()) + 1;
    const std::int64_t room = layouts_body(s, sc).columns - chrome;
    return room > kSetupNameMinCols ? room : kSetupNameMinCols;
}

// ---- THE `setup:` SLOT: what the ACTIVE layout's association is --------------------------
// WL-TAB-02 -- agents/workshop/tab-run.md

/// The word before the association, and the three the association is said in. Spelled as
/// their own constants because the row's own budget is DERIVED from their widths below --
/// the reservation and the words cannot drift apart if the reservation is measured from
/// them.
inline constexpr const char* kSetupSlot = "setup: ";
inline constexpr const char* kSetupLinkNone = "none";
inline constexpr const char* kSetupLinkCurrent = "current";
inline constexpr const char* kSetupLinkModified = "modified";

/// The separator this row puts between its facts, and the one place its width is known.
inline constexpr const char* kStatusJoin = " | ";
inline constexpr std::int64_t kStatusJoinCols =
    static_cast<std::int64_t>(std::char_traits<char>::length(kStatusJoin));

/// THE ACTIVE LAYOUT'S ASSOCIATION, AS THE ROW SAYS IT -- the standing half of the status,
/// composed against a budget for the PATH alone.
// WL-TAB-02, WL-TAB-03 -- agents/workshop/tab-run.md
inline std::string setup_link_text(const SetupState& setup, std::int64_t path_columns) {
    const std::int64_t status = link_status(setup.active, setup.active_link);
    std::string line = kSetupSlot;
    if (status == setup_link::kNone) {
        line += kSetupLinkNone;
        return line;
    }
    line += detail::fit_path(setup.active_link.path, path_columns);
    line += kStatusJoin;
    line += status == setup_link::kCurrent ? kSetupLinkCurrent : kSetupLinkModified;
    return line;
}

/// WHAT THE ROW SAYS AFTER THE ASSOCIATION: the unresolved count, then the two gestures.
// WL-MAKER-04 -- agents/workshop/maker-pane.md; WL-TAB-03 -- agents/workshop/tab-run.md
inline std::string setup_rest_text(const SetupState& setup, const Panels& panels,
                                   const Keymap& keymap) {
    std::string line;
    // THE SESSION'S WHOLE RESOLUTION TABLE IS ASKED, AND THIS IS THE LINE THAT MADE IT A
    // REQUIRED ARGUMENT (the maker-made pane joined the table later). A pane a
    // maker can SEE must not be counted as unresolved on the row directly beneath it, and
    // the built-in-only resolver would have said exactly that about every admitted external
    // offer -- silently, and only in the configuration where somebody had actually loaded
    // a provider.
    const std::vector<PaneRef> waiting = unresolved_panes(setup.active, panels);
    if (!waiting.empty()) {
        // UNRESOLVED, NEVER UNAVAILABLE. Workshop knows that it cannot present these
        // references; it knows nothing whatever about whoever could, and a word implying
        // otherwise would be a claim made out of silence.
        line += kStatusJoin + std::to_string(waiting.size()) + " unresolved";
    }
    line += kStatusJoin;
    line += setup_hints(keymap);
    return line;
}

/// The workspace's extent, as the band states it -- the one fact the retired shared top
/// row carried that nothing else says. It is a STATUS fact (what a share of the
/// workspace currently resolves against), so it lives beside the setup identity in the
/// band's own voice rather than as a heading of its own.
inline std::string workspace_text(const Session& s) {
    return "workspace " + std::to_string(s.workspace_w) + "x" +
           std::to_string(s.workspace_h) + " cells";
}

// ---- THE LAYOUT TABS: the left of the status row -----------------------------------------

/// One painted tab: which layout it is, and exactly which bytes of the row are its own.
// WL-TAB-07 -- agents/workshop/tab-run.md
struct LayoutTab {
    std::size_t at = 0;       ///< the layout's position in the maker's order
    std::int64_t column = 0;  ///< where its bytes begin in the composed row
    std::int64_t columns = 0; ///< how many bytes they are
    bool active = false;
};

/// The tab run as it will be painted: the text, the tabs inside it, what it left out, and
/// where the create affordance landed if there was room for one.
struct LayoutTabRun {
    std::string text;
    std::vector<LayoutTab> tabs;
    std::size_t before = 0; ///< layouts omitted ahead of the first painted one
    std::size_t after = 0;  ///< layouts omitted after the last painted one
    std::int64_t create_column = 0;  ///< where `+` begins in the composed row...
    std::int64_t create_columns = 0; ///< ...and how many bytes it is; 0 means unpainted
};

/// What one end of a fitted tab run says about the layouts it could not paint. `<2` and
/// `3>`: a count and a direction.
// WL-TAB-08 -- agents/workshop/tab-run.md
inline std::string layouts_omitted_text(std::size_t how_many, bool ahead) {
    return ahead ? " " + std::to_string(how_many) + ">" : "<" + std::to_string(how_many);
}

/// The cell a tab opens with and the cell it closes with -- the live layout's pair and every
/// other layout's, ONE CELL EACH SIDE either way.
// WL-TAB-06 -- agents/workshop/tab-run.md
inline constexpr char kLayoutLiveOpen = '>';
inline constexpr char kLayoutLiveClose = '<';
inline constexpr char kLayoutTabPad = ' ';

/// What ONE layout contributes to the run: its two marker cells and the AUTHORED name between
/// them, bare. No quoting, no escaping and no substitution -- the bytes a maker typed.
inline std::string layout_tab_text(const SetupState& setup, std::size_t at) {
    const bool live = at == setup.active_at;
    std::string tab;
    tab += live ? kLayoutLiveOpen : kLayoutTabPad;
    tab += layout_at(setup, at).name;
    tab += live ? kLayoutLiveClose : kLayoutTabPad;
    return tab;
}

/// The room the ACTIVE LAYOUT'S ASSOCIATION must keep whatever the tab run wants.
// WL-TAB-03 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kElidedCols =
    static_cast<std::int64_t>(std::char_traits<char>::length(detail::kElided));

inline constexpr std::int64_t kSetupStatusCols =
    kStatusJoinCols +                                   // `" | "` before the slot
    static_cast<std::int64_t>(std::char_traits<char>::length(kSetupSlot)) +
    kElidedCols +                                       // the least a path can honestly say
    kStatusJoinCols +                                   // `" | "` before the verdict
    static_cast<std::int64_t>(std::char_traits<char>::length(kSetupLinkModified)) +
    kElidedCols;                                        // the row's own cut mark

/// The fewest columns the tab run keeps even where the reservation would leave it less, so a
/// surface too narrow for both still says which layout is live. `setup_name_columns`' own
/// floor, one region over.
inline constexpr std::int64_t kLayoutTabMinCols = 8;

inline std::int64_t layout_tab_columns(std::int64_t row_columns) noexcept {
    const std::int64_t room = row_columns - kSetupStatusCols;
    return room > kLayoutTabMinCols ? room : kLayoutTabMinCols;
}

/// THE POINTER'S SPELLING OF `layout.new`: one cell of ink at the end of the run.
// WL-TAB-04 -- agents/workshop/tab-run.md
inline constexpr char kLayoutCreate = '+';
inline constexpr std::int64_t kLayoutCreateCols = 2; // one pad cell and the mark

/// THE VISIBLE TAB WINDOW, DERIVED AT EVERY PAINT AND STORED NOWHERE.
// WL-TAB-05, WL-TAB-08 -- agents/workshop/tab-run.md
inline LayoutTabRun layout_tab_run(const SetupState& setup, std::int64_t columns) {
    LayoutTabRun run;
    const std::size_t n = layout_count(setup);
    if (columns <= 0) {
        run.after = n; // no room at all: everything there is, is missing
        return run;
    }
    const std::size_t live = setup.active_at < n ? setup.active_at : 0;
    std::vector<std::string> text;
    text.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        text.push_back(layout_tab_text(setup, i));
    }
    // The cost of painting `[first, last]` -- the tabs plus whichever markers that window
    // would need. Asked afresh for every candidate window, because taking one more tab can
    // RETIRE the marker on that side and pay for itself.
    const auto cost = [&](std::size_t first, std::size_t last) {
        std::int64_t total = 0;
        for (std::size_t i = first; i <= last; ++i) {
            total += static_cast<std::int64_t>(text[i].size());
        }
        if (first > 0) {
            total += static_cast<std::int64_t>(layouts_omitted_text(first, false).size());
        }
        if (last + 1 < n) {
            total += static_cast<std::int64_t>(layouts_omitted_text(n - last - 1, true).size());
        }
        return total;
    };
    std::size_t first = live;
    std::size_t last = live;
    bool rightward = true;
    for (bool grew = true; grew;) {
        grew = false;
        // Two chances a round -- the preferred side, then the other -- so a window that has
        // run out of room on one side keeps growing on the other, and the preference
        // alternates so the live layout ends up inside the run rather than pinned to an end.
        for (int tries = 0; tries < 2 && !grew; ++tries) {
            if (rightward) {
                if (last + 1 < n && cost(first, last + 1) <= columns) {
                    ++last;
                    grew = true;
                }
            } else if (first > 0 && cost(first - 1, last) <= columns) {
                --first;
                grew = true;
            }
            rightward = !rightward;
        }
    }
    run.before = first;
    run.after = n - last - 1;
    // BOTH MARKERS ARE PAID FOR OUT OF THE SAME BUDGET AS THE TABS, and each is RESERVED
    // before a tab is written rather than appended after them. Rule 3 says an omission spends
    // columns of this budget, and a marker written once the budget was already gone would be
    // a bound that grows when it is exceeded -- the exact thing the rule refuses.
    //
    // AND RULE 2 OUTRANKS RULE 3 AT THE BOTTOM OF THE RANGE. Where the room will not hold a
    // marker AND something of the live layout, the marker is not written: which layout is
    // live is what a maker cannot do without, and a run that spent its last cells saying how
    // many it could not show would have stopped answering the question it exists for. The
    // COUNTS are still on the answer (`before`/`after`) whatever the text could carry.
    const std::string head =
        run.before > 0 ? layouts_omitted_text(run.before, false) : std::string();
    const std::string tail =
        run.after > 0 ? layouts_omitted_text(run.after, true) : std::string();
    const std::int64_t head_cost =
        static_cast<std::int64_t>(head.size()) < columns
            ? static_cast<std::int64_t>(head.size())
            : 0;
    const std::int64_t tail_cost =
        head_cost + static_cast<std::int64_t>(tail.size()) < columns
            ? static_cast<std::int64_t>(tail.size())
            : 0;
    if (head_cost > 0) {
        run.text += head;
    }
    for (std::size_t i = first; i <= last; ++i) {
        LayoutTab tab;
        tab.at = i;
        tab.active = i == live;
        tab.column = static_cast<std::int64_t>(run.text.size());
        // THE LIVE TAB IS CUT RATHER THAN DROPPED where even it alone cannot fit, because
        // "which layout am I in" is the one thing this run may not stop saying (rule 2).
        // `detail::fit` marks the cut, and the span recorded is what was actually written.
        std::string shown = text[i];
        if (tab.column + static_cast<std::int64_t>(shown.size()) > columns - tail_cost) {
            shown = detail::fit(std::move(shown), columns - tail_cost - tab.column);
        }
        tab.columns = static_cast<std::int64_t>(shown.size());
        run.text += shown;
        run.tabs.push_back(tab);
    }
    if (tail_cost > 0) {
        run.text += tail;
    }
    //...AND THE CREATE AFFORDANCE OUT OF WHAT IS GENUINELY LEFT. Last, and out of
    // the same budget: an affordance written once the budget was already gone would be the
    // bound-that-grows rule 3 refuses, and it would push the association's own reservation
    // off a narrow row to advertise a key that still works.
    const std::int64_t written = static_cast<std::int64_t>(run.text.size());
    if (written + kLayoutCreateCols <= columns) {
        run.create_column = written + 1; // the pad cell belongs to the gap, not to the mark
        run.create_columns = 1;
        run.text += kLayoutTabPad;
        run.text += kLayoutCreate;
    }
    return run;
}

/// THE BAND'S STATUS ROW, WHOLE: the tabs on the left, the status on the right, and where
/// every painted tab's bytes are.
// WL-TAB-05 -- agents/workshop/tab-run.md
struct BandStatus {
    std::string text;
    std::vector<LayoutTab> tabs;
    std::size_t before = 0;
    std::size_t after = 0;
    std::int64_t create_column = 0;
    std::int64_t create_columns = 0;
};

inline BandStatus band_status(const Session& s, const ExternalBodyPlace& place) {
    BandStatus out;
    if (!place.present) {
        return out;
    }
    const LayoutTabRun run = layout_tab_run(s.setup, layout_tab_columns(place.columns));
    out.before = run.before;
    out.after = run.after;
    const std::int64_t left = static_cast<std::int64_t>(run.text.size());
    std::string rest = setup_rest_text(s.setup, s.panels, s.keymap);
    // THE WORKSPACE FACT FOLDS IN WHERE THE TOP BAND HAS NO SECOND ROW FOR IT -- the band's
    // fold, unchanged in kind and re-measured against the band it is now on. A
    // character medium gives the fact its own row; the shipped face's single row carries
    // both. It folds into the CUTTABLE half, because a room's size is the one fact here a
    // maker can also read by looking at their window.
    if (place.rows < 2) {
        rest += kStatusJoin + workspace_text(s);
    }
    // WHAT IS LEFT FOR THE ARTIFACT'S NAME: what the tabs did not take, less the words of
    // the sentence, less everything that follows it.
    //
    // ⚠ THE PATH IS THE PART THAT SHRINKS, AND IT SHRINKS FOR THE WHOLE ROW. Taking the
    // remainder for the path alone reads as generous and starves the dynamic truth behind
    // it: at the 78-column minimum a real temporary path swallowed every cell after the
    // verdict, and a maker with an unresolved pane stopped being told so -- measured by the
    // suite, not reasoned about. So the path yields to the unresolved count and to the two
    // gestures as well, and only what THEN does not fit is cut from the right, which is the
    // ordering §9 asks for: the verdict is reserved, the tail degrades, the path absorbs.
    const std::int64_t path_columns = place.columns - left - kSetupStatusCols + kElidedCols -
                                      static_cast<std::int64_t>(rest.size());
    std::string standing = setup_link_text(
        s.setup, path_columns > kElidedCols ? path_columns : kElidedCols);
    // THE STATUS IS RIGHT-ADJUSTED WHERE THERE IS ROOM TO ADJUST IT. The run is the
    // row's left and the status is its right, so the gap between them is the row's own slack
    // -- which pins the association to the screen's edge instead of letting it drift with
    // however many tabs happen to exist. Combined with equal-width marker, that
    // makes the right-hand sentence perfectly still: neither switching layouts nor adding
    // one moves a cell of it while the row still fits.
    std::string line = run.text;
    const std::int64_t joined =
        left + kStatusJoinCols + static_cast<std::int64_t>(standing.size() + rest.size());
    if (joined < place.columns) {
        line.append(static_cast<std::size_t>(place.columns - joined + kStatusJoinCols), ' ');
    } else {
        line += kStatusJoin;
    }
    line += standing;
    line += rest;
    out.text = detail::fit(std::move(line), place.columns);
    // A SPAN THE ROW'S OWN CUT REMOVED IS NOT A TAB ANY MORE. The reservation above makes
    // this unreachable at every honest extent -- the tabs are composed against the row less
    // the association's own room -- and it is written anyway, because a span that outlived
    // the bytes it describes is exactly the stale geometry a press must never be answered
    // from. The create affordance is judged by the same rule and for the same reason.
    const std::int64_t painted = static_cast<std::int64_t>(out.text.size());
    for (const LayoutTab& tab : run.tabs) {
        if (tab.column + tab.columns <= painted) {
            out.tabs.push_back(tab);
        }
    }
    if (run.create_columns > 0 && run.create_column + run.create_columns <= painted) {
        out.create_column = run.create_column;
        out.create_columns = run.create_columns;
    }
    return out;
}

/// THE SAME COMPOSITION, RESOLVED FROM THE SESSION -- for every consumer that holds a
/// screen rather than the interior the painter was handed. Two spellings of one answer,
/// because the painter already has the rectangle it is drawing into and re-deriving it
/// there would be the second resolution; everyone else asks for it here.
inline BandStatus band_status(const Session& s, const Screen& sc) {
    return band_status(s, layouts_body(s, sc));
}

/// WHICH ROW OF THE LAYOUTS PANE THE TAB RUN IS PAINTED ON, or `kNoBandRow` when it is not
/// painted at all -- the composition's own answer, so a press can never be resolved against
/// a run this budget did not write.
// WL-TAB-05 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kNoBandRow = -1;

inline std::int64_t band_tab_row(const Session& s, const Screen& sc) {
    if (!layouts_body(s, sc).present || s.setup.naming.open) {
        return kNoBandRow;
    }
    // THE IDENTITY IS THE PANE'S FIRST ROW WHENEVER THE PANE HAS ONE (re-homed when the band
    // became a pane). It used to share a band with the notice, so at a one-row budget the tool's
    // voice outranked it and there was no tab row at all; the notice lives at the foot now,
    // and nothing in this pane can displace the selector but the name editor taking its row.
    return 0;
}

/// WHICH LAYOUT A PRESS LANDED ON, or none -- the exact inverse of what was painted.
// WL-TAB-09 -- agents/workshop/tab-run.md
struct LayoutTabPress {
    bool hit = false;      ///< this press was answered by the run
    std::size_t at = 0;    ///< the layout it landed on, when `create` is false
    bool create = false;   /// < it landed on the `+` affordance instead
};

inline LayoutTabPress band_tab_at(const Session& s, const Screen& sc, std::int64_t space,
                                  std::int64_t x, std::int64_t y) {
    const std::int64_t row = band_tab_row(s, sc);
    if (row == kNoBandRow) {
        return {};
    }
    // ⚠ THE PRESS IS RESOLVED AGAINST THE RECTANGLE THE TABS ARE PAINTED IN, which since
    // the conversion is the Layouts pane's INTERIOR -- the maker's authored place and size, less
    // its chrome. A stale origin here would answer a press at the rectangle the band used
    // to own and ignore the row a maker can actually see, which is the same one-row lie
    // the split made unsayable at the other end and the reason the origin is taken from the
    // same `layouts_body` the painter publishes at.
    //
    // AND THIS IS A PANE-LOCAL INVERSE NOW, NOT A GLOBAL QUESTION. Nothing calls it until
    // ordinary occupancy has already answered `Layouts` for the point, so a pane authored
    // in front of this one takes the press before this arithmetic is ever spent.
    const ExternalBodyPlace place = layouts_body(s, sc);
    const ProseAt at = prose_at(space, x, y, place.region_x, place.region_y, place.fit);
    if (!at.understood || at.row != row) {
        return {};
    }
    const BandStatus band = band_status(s, place);
    for (const LayoutTab& tab : band.tabs) {
        if (at.column >= tab.column && at.column < tab.column + tab.columns) {
            return LayoutTabPress{true, tab.at, false};
        }
    }
    if (band.create_columns > 0 && at.column >= band.create_column &&
        at.column < band.create_column + band.create_columns) {
        return LayoutTabPress{true, 0, true};
    }
    return {};
}

// ---- THE LAYOUTS PANE AND THE BOTTOM BAND, EACH COMPOSED AGAINST ITS BUDGET ---------------
// WL-TAB-01 -- agents/workshop/tab-run.md

/// THE LAYOUTS PANE, PAINTED: the layout selector and the standing identity beside
/// it, the workspace fact under them where the medium fits a second row, and the setup-name
/// editor's caret and selection while a maker is typing a name.
// WL-TAB-01, WL-TAB-05 -- agents/workshop/tab-run.md
inline void paint_layouts(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                          const Screen& sc, std::int64_t chrome = kPaneChrome) {
    paint_panel_frame(layer, b, chrome);
    const ExternalBodyPlace place = external_body_place(b, sc, 0);
    surface::SurfaceTextRegion band;
    band.x = place.region_x;
    band.y = place.region_y;
    band.w = place.region_w;
    band.h = place.region_h;
    band.sub_x = place.region_sub_x;
    band.sub_y = place.region_sub_y;
    band.sub_w = place.region_sub_w;
    band.sub_h = place.region_sub_h;
    const std::int64_t budget = place.rows;
    const std::int64_t columns = place.columns;
    if (!place.present) {
        return; // no room for one row of this medium's type: say nothing at all
    }

    const bool naming = s.setup.naming.open;
    std::string identity;
    std::int64_t caret_col = surface::kNoCaret;
    std::int64_t sel_begin = 0;
    std::int64_t sel_end = 0;
    if (naming) {
        const std::int64_t cols = setup_name_columns(s, sc);
        const std::string shown = s.setup.naming.line.visible(cols);
        const component::TextBox::VisibleSpan vis =
            s.setup.naming.line.visible_selection(cols);
        const std::int64_t prompt =
            static_cast<std::int64_t>(std::char_traits<char>::length(kSetupNamePrompt));
        const std::int64_t at =
            static_cast<std::int64_t>(s.setup.naming.line.caret_column());
        caret_col = prompt + (at < static_cast<std::int64_t>(shown.size())
                                  ? at
                                  : static_cast<std::int64_t>(shown.size()));
        if (vis.present()) {
            sel_begin = prompt + vis.begin;
            sel_end = prompt + vis.end;
        }
        identity = detail::fit(std::string(kSetupNamePrompt) + shown +
                                   setup_name_hint(s.keymap),
                               columns);
    } else {
        // THE LAYOUT TABS AND THE STATUS ARE ONE COMPOSITION, and the painter takes
        // it whole -- the workspace fold and the row's own cut included, so the spans
        // `band_tab_at` answers a press from are the spans that were written here. It is
        // composed against THE PLACE THIS PAINTER RESOLVED rather than against a
        // second reading of the session's own geometry: one rectangle in, one row out.
        identity = band_status(s, place).text;
    }

    band.rows.push_back(surface::SurfaceTextRow{std::move(identity), surface::role::kMuted});
    if (caret_col != surface::kNoCaret) {
        band.caret_row = 0;
        band.caret_col = caret_col;
    }
    if (sel_end > sel_begin) {
        band.sel_begin_row = 0;
        band.sel_begin_col = sel_begin;
        band.sel_end_row = 0;
        band.sel_end_col = sel_end;
    }
    // THE WORKSPACE FACT GETS ITS OWN ROW WHERE THERE IS ONE, and folds into the identity
    // row where there is not (`band_status`). It yields to a name being typed for the reason
    // it has always yielded: a maker mid-name is reading their own words.
    if (budget >= 2 && !naming) {
        band.rows.push_back(
            surface::SurfaceTextRow{detail::fit(workspace_text(s), columns),
                                    surface::role::kMuted});
    }
    layer.texts.push_back(std::move(band));
}

// ---- A MAKER-MADE PANE, PRESENTED: authored regions on an offered interior -----------------
// WL-MAKER-05 -- agents/workshop/maker-pane.md

/// The part of one fine rectangle inside another -- `clip_to_canvas_fine` against a
/// rectangle instead of the canvas. A region authored past its pane's interior is legal
/// intent and is clipped HERE, at presentation, exactly as an off-room pane is clipped by
/// the canvas; the authored value is untouched by it.
inline constexpr FineRect clip_to_fine(const FineRect& r, const FineRect& within) noexcept {
    const std::int64_t x0 = r.x > within.x ? r.x : within.x;
    const std::int64_t y0 = r.y > within.y ? r.y : within.y;
    const std::int64_t rx1 = surface::add_cells(r.x, r.w);
    const std::int64_t ry1 = surface::add_cells(r.y, r.h);
    const std::int64_t wx1 = surface::add_cells(within.x, within.w);
    const std::int64_t wy1 = surface::add_cells(within.y, within.h);
    const std::int64_t x1 = rx1 < wx1 ? rx1 : wx1;
    const std::int64_t y1 = ry1 < wy1 ? ry1 : wy1;
    if (x1 <= x0 || y1 <= y0) {
        return FineRect{};
    }
    return FineRect{x0, y0, x1 - x0, y1 - y0};
}

/// WHAT ONE AUTHORED REGION RESOLVES TO INSIDE ONE OFFERED INTERIOR, on this screen.
struct RegionPresentation {
    bool present = false;     ///< some of the region lies inside the interior
    FineRect asked{};         ///< the authored rectangle on the canvas: interior origin + place
    FineRect shown{};         ///< the part inside the interior -- what is painted and read
    bool clipped = false;     ///< the interior cut some of it away
    surface::RegionFit fit{}; ///< `shown`, resolved with the active face's metric
};

/// THE ONE RESOLUTION OF A REGION: interior origin plus authored place, clipped to the
/// interior, fitted with the face's metric. Pure, total, and the same call the painter,
/// the region mark and the Pane Manager's RESOLVED rows spend -- one measurer.
inline RegionPresentation present_region(const TextRegion& r, const FineRect& interior,
                                         const Screen& sc) {
    RegionPresentation p;
    if (interior.empty()) {
        return p;
    }
    p.asked = FineRect{surface::add_cells(interior.x, r.x), surface::add_cells(interior.y, r.y),
                       r.w, r.h};
    p.shown = clip_to_fine(p.asked, interior);
    p.clipped = !(p.shown == p.asked);
    if (p.shown.empty()) {
        return p;
    }
    p.fit = surface::fit_region_subs(p.shown.x, p.shown.y, p.shown.w, p.shown.h,
                                     sc.text_advance_px, sc.text_line_px);
    p.present = true;
    return p;
}

/// A published region over a fine rectangle, empty and ready for its rows -- the fine
/// bounds decomposed onto the wire's cells-plus-remainder spelling.
inline surface::SurfaceTextRegion region_over(const FineRect& r) {
    surface::SurfaceTextRegion region;
    const surface::SurfaceRect wire = wire_rect_of(r, surface::role::kFill);
    region.x = wire.x;
    region.y = wire.y;
    region.w = wire.w;
    region.h = wire.h;
    region.sub_x = wire.sub_x;
    region.sub_y = wire.sub_y;
    region.sub_w = wire.sub_w;
    region.sub_h = wire.sub_h;
    return region;
}

/// The region a reference and an id name in the OPEN definition, or nothing: nothing when
/// no definition is open, when the reference is not the open definition's, or when the id
/// is not one of its regions. Every reader of a region goes through here, so a subject
/// whose definition closed underneath it reads `--` rather than a stale value.
inline const TextRegion* maker_region(const Session& s, const PaneRef& ref, std::int64_t id) {
    const MakerPane& m = s.panels.maker;
    if (!m.open() || !(maker_pane_ref(m.definition.name) == ref)) {
        return nullptr;
    }
    return region_of(m.definition, id);
}

/// THE MAKER-MADE PANE'S INTERIOR RIGHT NOW, or an empty rectangle: the ordinary pane path's
/// answer for its handle, less the chrome. One call, so the painter, the mark and the rows
/// cannot resolve it three ways.
inline FineRect maker_pane_interior(const Session& s, const Screen& sc) {
    const PanelBounds where = bounds_of(s.panels, s.setup.active, kMakerPaneKind, sc);
    if (!where.open || where.rect.empty()) {
        return FineRect{};
    }
    return pane_inside(where.rect, sc).rect;
}

/// ONE AUTHORED AXIS OF A REGION AS A MAKER READS IT -- the amount in the face's own unit
/// (`geometry_amount_text`, the pane rows' own grammar), marked where this face cannot say
/// the authored number exactly.
inline std::string region_axis_text(const Session& s, const PaneRef& ref, std::int64_t id,
                                    std::size_t axis) {
    const TextRegion* r = maker_region(s, ref, id);
    if (r == nullptr) {
        return "--";
    }
    const std::int64_t v = axis == 0 ? r->x : axis == 1 ? r->y : axis == 2 ? r->w : r->h;
    bool projected = false;
    std::string out = geometry_amount_text(v, s.cell_px, projected) + " " +
                      std::string(geometry_unit(s.cell_px));
    if (projected) {
        out += kProjectedNote;
    }
    return out;
}

/// WRITE ONE AUTHORED AXIS OF A REGION FROM WHAT A MAKER TYPED -- a whole number in the
/// face's own unit, through the definition's own door (`author_region_axis`), which judges
/// the fine value in its own words. A region has no `default` mode, so `-` is refused in
/// words rather than read as a reset that does not exist.
inline Written write_region_axis(Session& s, const PaneRef& ref, std::int64_t id,
                                 std::size_t axis, const std::string& text) {
    if (maker_region(s, ref, id) == nullptr) {
        return Written::no(ref_text(ref) + " is not the open pane definition -- nothing to author");
    }
    std::string_view body = text;
    while (!body.empty() && body.front() == ' ') {
        body.remove_prefix(1);
    }
    while (!body.empty() && body.back() == ' ') {
        body.remove_suffix(1);
    }
    if (body == "-") {
        return Written::no("a region has no default to reset to -- type a whole number of " +
                           std::string(geometry_unit(s.cell_px)));
    }
    const FaceAmount typed = parse_face_amount(body, s.cell_px);
    if (!typed.accepted) {
        return Written::no(typed.refusal);
    }
    return author_region_axis(s.panels.maker.definition, id, axis, typed.subs);
}

/// WRITE WHAT A TEXT REGION SAYS, through the definition's own door.
inline Written write_region_text(Session& s, const PaneRef& ref, std::int64_t id,
                                 std::string text) {
    if (maker_region(s, ref, id) == nullptr) {
        return Written::no(ref_text(ref) + " is not the open pane definition -- nothing to author");
    }
    return set_region_text(s.panels.maker.definition, id, std::move(text));
}

/// THE REGION AS THIS SCREEN RESOLVED IT, relative to the pane's interior and in the face's
/// unit -- so it reads beside the authored X/Y/Width/Height and differs from them exactly
/// where the interior clipped it. `-` when the pane is not presented.
inline std::string region_resolved_text(const Session& s, const PaneRef& ref, std::int64_t id) {
    const TextRegion* r = maker_region(s, ref, id);
    if (r == nullptr) {
        return "--";
    }
    const Screen sc = screen_of(s);
    const FineRect interior = maker_pane_interior(s, sc);
    const RegionPresentation p = present_region(*r, interior, sc);
    if (!p.present) {
        return "- (the pane is not presented, or the region lies outside it)";
    }
    const FineRect local{p.shown.x - interior.x, p.shown.y - interior.y, p.shown.w, p.shown.h};
    std::string out = fine_rect_text(local, s.cell_px);
    if (p.clipped) {
        out += " (clipped by the pane)";
    }
    return out;
}

/// WHAT THE FACE MADE OF THE REGION: rows and columns of type, the cell projection, or no
/// room -- a readout of the medium's answer, never a claim about the definition.
inline std::string region_shown_text(const Session& s, const PaneRef& ref, std::int64_t id) {
    const TextRegion* r = maker_region(s, ref, id);
    if (r == nullptr) {
        return "--";
    }
    const Screen sc = screen_of(s);
    const RegionPresentation p = present_region(*r, maker_pane_interior(s, sc), sc);
    if (!p.present || p.fit.rows <= 0 || p.fit.columns <= 0) {
        return "no room -- nothing of it is drawn on this face";
    }
    return std::to_string(p.fit.rows) + (p.fit.rows == 1 ? " row x " : " rows x ") +
           std::to_string(p.fit.columns) + (p.fit.columns == 1 ? " column, " : " columns, ") +
           (p.fit.graphical() ? "presented in type" : "presented as cells");
}

/// THE ONLY HONEST INTERIOR FOR A PANE THAT IS NOT MADE OF DATA: a read-only capture of the
/// resolved body -- where it is, how much prose the face fits in it, in which presentation
/// -- and the plain statement that no authored interior exists. A built-in's interior is
/// its painter and a provider's is its own; neither is decomposed, inferred or promised.
inline std::string interior_capture_text(const Session& s, const PaneRef& ref) {
    const std::optional<std::int64_t> kind = resolve_pane(ref, s.panels);
    if (!kind.has_value()) {
        if (ref.provider == kMakerPaneProvider) {
            return "no open definition is named " + ref.pane + " -- nothing to show";
        }
        return "unresolved -- nothing to inspect";
    }
    const Screen sc = screen_of(s);
    const PanelBounds where = bounds_of(s.panels, s.setup.active, *kind, sc);
    const char* whose = is_runtime_kind(*kind) ? "a provider's own" : "code-backed";
    if (!where.open || where.rect.empty()) {
        return std::string(whose) + " -- not presented; no authored interior";
    }
    const PanelProsePlace place = panel_prose_place(where.rect, sc);
    if (!place.present) {
        return std::string(whose) + " -- body " + fine_rect_text(place.inside, s.cell_px) +
               ", no room for a row; no authored interior";
    }
    return std::string(whose) + " -- body " + fine_rect_text(place.inside, s.cell_px) + ", " +
           std::to_string(place.rows) + (place.rows == 1 ? " row x " : " rows x ") +
           std::to_string(place.columns) + (place.columns == 1 ? " column " : " columns ") +
           (place.fit.graphical() ? "in type" : "as cells") + "; no authored interior";
}

/// THE MAKER-MADE PANE, PAINTED: the frame, one region owning the whole interior (so the
/// material beneath the pane is cleared and the ring shows, `paint_panel_frame`'s own
/// arithmetic), then one `kGroundOwn` region per authored region.
// WL-MAKER-05 -- agents/workshop/maker-pane.md
inline void paint_maker_pane(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                             const Screen& sc, std::int64_t chrome = kPaneChrome) {
    paint_panel_frame(layer, b, chrome);
    const PaneInside inside = pane_inside(b, sc);
    if (inside.rect.empty()) {
        return;
    }
    layer.texts.push_back(region_over(inside.rect));
    const MakerPane& m = s.panels.maker;
    if (!m.open()) {
        return;
    }
    for (const TextRegion& r : m.definition.regions) {
        const RegionPresentation p = present_region(r, inside.rect, sc);
        if (!p.present || p.fit.rows <= 0 || p.fit.columns <= 0) {
            continue;
        }
        surface::SurfaceTextRegion region = region_over(p.shown);
        region.rows.push_back(
            surface::SurfaceTextRow{detail::fit(r.text, p.fit.columns), surface::role::kFill});
        layer.texts.push_back(std::move(region));
    }
}

/// THE ROLE THE PANE CREATOR'S REGION MARK IS DRAWN IN: the one thing being pointed at, the
/// word the document's selection ring and the selected pane's chrome already speak.
inline constexpr std::int64_t kRegionMark = surface::role::kAccent;

/// WHICH REGION THE PANE CREATOR IS WORKING ON RIGHT NOW, or nothing -- the open
/// definition's first region, while the Pane Manager is on this desk and has the maker's
/// pane as its subject. Derived at every ask, held nowhere: close the manager, choose
/// another subject, or discard the pane, and the answer is nothing with nothing to clear.
inline const TextRegion* creator_subject_region(const Session& s) {
    const MakerPane& m = s.panels.maker;
    if (!m.open() || m.definition.regions.empty() || !s.panels.has(panel::kPaneEditor)) {
        return nullptr;
    }
    if (!s.pane_editor.addressed() ||
        !(s.pane_editor.subject == maker_pane_ref(m.definition.name))) {
        return nullptr;
    }
    return &m.definition.regions.front();
}

/// THE REGION MARK: the exact rectangle the region resolved to, filled in the mark's role,
/// with the region's own text written OVER it.
// WL-MAKER-06 -- agents/workshop/maker-pane.md
inline void paint_creator_region_mark(surface::SurfaceLayer& layer, const Session& s,
                                      const Screen& sc) {
    const TextRegion* r = creator_subject_region(s);
    if (r == nullptr) {
        return;
    }
    const PanelBounds where = bounds_of(s.panels, s.setup.active, kMakerPaneKind, sc);
    if (!where.open || where.rect.empty() ||
        pane_is_covered(s.panels, s.setup.active, sc, kMakerPaneKind, where.rect)) {
        return;
    }
    const RegionPresentation p = present_region(*r, pane_inside(where.rect, sc).rect, sc);
    if (!p.present) {
        return;
    }
    layer.rects.push_back(wire_rect_of(p.shown, kRegionMark));
    if (p.fit.rows > 0 && p.fit.columns > 0) {
        surface::SurfaceTextRegion over = region_over(p.shown);
        over.ground = surface::kGroundBeneath;
        over.rows.push_back(
            surface::SurfaceTextRow{detail::fit(r->text, p.fit.columns), surface::role::kFill});
        layer.texts.push_back(std::move(over));
    }
}

/// THE PANE CREATOR'S NAME PROMPT, as the Pane Manager's heading spells it while a name is
/// being typed, and the columns the typed line may spend beside it (the prompt, then the
/// caret's own column reserved, `kTerminalCaretCols`' rule).
inline constexpr const char* kPaneNamePrompt = "new pane> ";
inline std::int64_t pane_name_columns(std::int64_t heading_columns) {
    const std::int64_t taken =
        static_cast<std::int64_t>(std::char_traits<char>::length(kPaneNamePrompt)) + 1;
    return heading_columns > taken ? heading_columns - taken : 0;
}

// ---- THE PANE EDITOR: a Workshop pane as a SUBJECT, inspected and edited -----------------

/// Prose rows the `PANES` heading keeps -- `kInfoHeadingRows`' twin, one pane over.
inline constexpr std::int64_t kPaneEditorHeadingRows = 1;

/// THE INVENTORY ROW THE SUBJECT NAMES RIGHT NOW, or nothing -- a fresh view over the SAME
/// population the picker walks (`inventory_rows`: the catalog, the admitted runtime panes,
/// and every reference the active setup names). Nothing is cached: a subject is checked
/// against the world at the moment somebody asks.
inline std::optional<CatalogRow> pane_editor_subject_row(const Session& s) {
    if (!s.pane_editor.addressed()) {
        return std::nullopt;
    }
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panels)) {
        if (row.ref == s.pane_editor.subject) {
            return row;
        }
    }
    return std::nullopt;
}

/// THE WINDOW A TYPED EDIT MEASURES THE OTHER AXIS FROM: authored where authored, resolved
/// where reactive -- `managed_window_base`'s spelling (weave.hpp), quarried out so the
// WL-PED-05 -- agents/workshop/pane-manager.md
inline FineRect pane_window_base(const Session& s, const PaneRef& ref) {
    FineRect out;
    const std::optional<std::int64_t> kind = resolve_pane(ref, s.panels);
    if (kind.has_value()) {
        out = bounds_of(s.panels, s.setup.active, *kind, screen_of(s)).resolved;
    }
    const SetupPane* row = pane_of(s.setup.active, ref);
    if (row != nullptr && row->place.mode == pane_unit::kSubcells) {
        out.x = row->place.x;
        out.y = row->place.y;
    }
    if (row != nullptr && row->width.mode == pane_unit::kSubcells) {
        out.w = row->width.amount;
    }
    if (row != nullptr && row->height.mode == pane_unit::kSubcells) {
        out.h = row->height.amount;
    }
    return out;
}

/// MAY THIS PANE'S GEOMETRY BE TYPED RIGHT NOW, and if not, why not -- the arrangement's
/// admission (`arrange_geometry_ready`, weave.hpp) less the one refusal a typed value does
/// not need.
// WL-PED-06 -- agents/workshop/pane-manager.md; WL-PANE-08 -- agents/workshop/panes-and-windows.md
inline Written pane_geometry_typeable(const Session& s, const PaneRef& ref) {
    if (!has_pane(s.setup.active, ref)) {
        return Written::no(ref_text(ref) + " is not in this layout -- open it first");
    }
    const std::optional<std::int64_t> kind = resolve_pane(ref, s.panels);
    if (!kind.has_value()) {
        return Written::no(ref_text(ref) +
                           " is unresolved -- its window cannot be measured; `-` resets an "
                           "axis and the order keys still work");
    }
    if (placement_of(*kind) == placement::kSideRegion) {
        return Written::no(kind_name(s.panels, *kind) +
                           " is in the reserved side column -- the screen owns its place");
    }
    const PanelBounds where = bounds_of(s.panels, s.setup.active, *kind, screen_of(s));
    if (!where.open) {
        return Written::no(kind_name(s.panels, *kind) +
                           " has no room on this screen yet -- `-` resets an axis");
    }
    return Written::ok();
}

/// ONE AUTHORED AXIS AS A MAKER READS IT: the amount in the face's own unit, `-` for the
/// developer's answer, and the pixel spelling for the unit no medium here projects --
/// `pane_window_text`'s per-axis grammar, one axis at a time.
inline std::string pane_axis_text(const Session& s, const PaneRef& ref, std::size_t axis) {
    const SetupPane* row = pane_of(s.setup.active, ref);
    if (row == nullptr) {
        return "--";
    }
    bool projected = false;
    std::string out;
    if (axis < 2) {
        if (row->place.mode != pane_unit::kSubcells) {
            return "-";
        }
        out = geometry_amount_text(axis == 0 ? row->place.x : row->place.y, s.cell_px,
                                   projected);
    } else {
        const PaneSize& size = axis == 2 ? row->width : row->height;
        if (size.mode == pane_unit::kPixels) {
            return std::to_string(size.amount) + "px";
        }
        if (size.mode != pane_unit::kSubcells) {
            return "-";
        }
        out = geometry_amount_text(size.amount, s.cell_px, projected);
    }
    out += " " + std::string(geometry_unit(s.cell_px));
    if (projected) {
        out += kProjectedNote;
    }
    return out;
}

/// WRITE ONE AUTHORED AXIS FROM WHAT A MAKER TYPED -- through the gesture door, one axis
/// proposed and the other left exactly as it stands (`author_pane_window`), or
/// through that axis's reset door for `-`.
// WL-PED-05 -- agents/workshop/pane-manager.md
inline Written write_pane_axis(Session& s, const PaneRef& ref, std::size_t axis,
                               const std::string& text) {
    std::string_view body = text;
    while (!body.empty() && body.front() == ' ') {
        body.remove_prefix(1);
    }
    while (!body.empty() && body.back() == ' ') {
        body.remove_suffix(1);
    }
    if (body == "-") {
        if (!has_pane(s.setup.active, ref)) {
            return Written::no(ref_text(ref) + " is not in this layout -- open it first");
        }
        bool moved = false;
        const char* what = "";
        if (axis < 2) {
            moved = reset_pane_place(s.setup.active, ref);
            what = "place";
        } else if (axis == 2) {
            moved = reset_pane_width(s.setup.active, ref);
            what = "width";
        } else {
            moved = reset_pane_height(s.setup.active, ref);
            what = "height";
        }
        if (!moved) {
            return Written::no(ref_text(ref) + " already takes the developer's " + what);
        }
        return Written::ok();
    }
    const Written ready = pane_geometry_typeable(s, ref);
    if (!ready.accepted) {
        return ready;
    }
    const FaceAmount typed = parse_face_amount(body, s.cell_px);
    if (!typed.accepted) {
        return Written::no(typed.refusal);
    }
    const FineRect from = pane_window_base(s, ref);
    PaneAxisProposal horizontal;
    PaneAxisProposal vertical;
    horizontal.base = from.x;
    vertical.base = from.y;
    switch (axis) {
    case 0: horizontal.position = typed.subs; break;
    case 1: vertical.position = typed.subs; break;
    case 2: horizontal.extent = PaneSize{pane_unit::kSubcells, typed.subs}; break;
    default: vertical.extent = PaneSize{pane_unit::kSubcells, typed.subs}; break;
    }
    return author_pane_window(s.setup.active, ref, horizontal, vertical).written;
}

/// THE SUBJECT'S ROWS: its identity, then AUTHORED, then RESOLVED. Every closure reads the
// WL-PED-04 -- agents/workshop/pane-manager.md
inline std::vector<Row> pane_editor_rows(Session& s) {
    std::vector<Row> rows;
    if (!s.pane_editor.addressed()) {
        return rows;
    }
    Session* sp = &s;
    const PaneRef ref = s.pane_editor.subject;
    const auto found = [sp, ref]() -> std::optional<CatalogRow> {
        for (const CatalogRow& row : inventory_rows(sp->setup.active, sp->panels)) {
            if (row.ref == ref) {
                return row;
            }
        }
        return std::nullopt;
    };
    rows.push_back(Row::show("Name", [found, ref] {
        const std::optional<CatalogRow> row = found();
        return row && row->kind != kNoPaneKind ? row->name : ref.pane;
    }));
    rows.push_back(Row::show("Identity", [ref] { return ref_text(ref); }));
    rows.push_back(Row::show("Provider", [found, ref] {
        const std::optional<CatalogRow> row = found();
        if (ref.provider == kMakerPaneProvider) {
            // A MAKER-MADE PANE'S NAMESPACE, said as what it is: Workshop's own, with no
            // office behind it to be loaded or missing. An unresolved one names the one
            // thing that would resolve it -- a definition file with this name.
            if (!row || row->kind == kNoPaneKind) {
                return ref.provider + " (a pane a maker made -- no open definition is named " +
                       ref.pane + "; --pane <file> opens one)";
            }
            return ref.provider + " (made here -- Pane Creator)";
        }
        if (!row || row->kind == kNoPaneKind) {
            return ref.provider + " (unresolved -- no office here offers it)";
        }
        if (is_runtime_kind(row->kind)) {
            return ref.provider + " (offered this session)";
        }
        return ref.provider + " (built in)";
    }));
    rows.push_back(Row::show("Summary", [found, ref] {
        const std::optional<CatalogRow> row = found();
        return row && row->kind != kNoPaneKind ? row->summary : std::string("--");
    }));
    rows.push_back(Row::section("AUTHORED"));
    static const char* const kAxisLabels[] = {"X", "Y", "Width", "Height"};
    for (std::size_t axis = 0; axis < 4; ++axis) {
        rows.push_back(Row::edit(
            kAxisLabels[axis],
            Property<std::string>([sp, ref, axis] { return pane_axis_text(*sp, ref, axis); },
                                  [sp, ref, axis](std::string text) {
                                      return write_pane_axis(*sp, ref, axis, text);
                                  })));
    }
    rows.push_back(Row::show("Front", [sp, ref] {
        const SetupPane* row = pane_of(sp->setup.active, ref);
        if (row == nullptr) {
            return std::string("--");
        }
        return "f" + std::to_string(row->front) + " of " +
               std::to_string(sp->setup.active.panes.size()) + " -- f/b/r/l order it";
    }));
    rows.push_back(Row::show("Open", [sp, ref] {
        return has_pane(sp->setup.active, ref) ? std::string("yes -- o removes it")
                                               : std::string("no -- o opens it");
    }));
    rows.push_back(Row::section("RESOLVED"));
    rows.push_back(Row::show("Window", [sp, ref] {
        const std::optional<std::int64_t> kind = resolve_pane(ref, sp->panels);
        if (!kind.has_value()) {
            return std::string("-");
        }
        const PanelBounds where =
            bounds_of(sp->panels, sp->setup.active, *kind, screen_of(*sp));
        if (!where.open) {
            return std::string("-");
        }
        if (!where.projected) {
            return std::string("refused -- a pixel axis projects on no medium here");
        }
        return fine_rect_text(where.resolved, sp->cell_px);
    }));
    rows.push_back(Row::show("State", [sp, found] {
        const std::optional<CatalogRow> row = found();
        if (!row) {
            return std::string("-- not in this build's vocabulary nor this layout");
        }
        const std::int64_t state =
            pane_state_of(sp->panels, sp->setup.active, screen_of(*sp), *row);
        std::string out = pane_state_word(state);
        const char* remedy = pane_state_remedy(state);
        if (remedy[0] != '\0') {
            out += std::string(" -- ") + remedy;
        }
        return out;
    }));
    // ---- INTERIOR: what is INSIDE the subject, said honestly for each kind ---------------
    //
    // A MAKER-MADE PANE EXPOSES ITS REGIONS, because regions are what it is made of: the
    // Pane Creator's rows over its one text region -- the text and four fine-lattice
    // numbers, AUTHORED through the definition's own doors -- and the freshly RESOLVED
    // facts beside them. EVERY OTHER PANE IS CODE-BACKED OR SOMEBODY ELSE'S, and the only
    // honest interior this surface can show for one is a read-only CAPTURE of the resolved
    // body: where it is, how many rows of type it holds, in which presentation. No
    // decompilation, no inferred controls, no pretence that a painter is a definition.
    //
    // Which arm a subject gets is decided at REBUILD (when the subject is chosen, or when a
    // definition opens, closes or is replaced) and the rows then read fresh: a maker pane
    // whose definition has since closed reads `--` in every row rather than a stale value.
    rows.push_back(Row::section("INTERIOR"));
    const std::optional<std::int64_t> resolved_now = resolve_pane(ref, s.panels);
    if (resolved_now.has_value() && is_maker_kind(*resolved_now) &&
        !s.panels.maker.definition.regions.empty()) {
        const std::int64_t region_id = s.panels.maker.definition.regions.front().id;
        rows.push_back(Row::show("Region", [sp, ref, region_id] {
            return maker_region(*sp, ref, region_id) == nullptr
                       ? std::string("--")
                       : "#" + std::to_string(region_id) + " text -- the Pane Creator's subject";
        }));
        rows.push_back(Row::edit(
            "Text",
            Property<std::string>(
                [sp, ref, region_id] {
                    const TextRegion* r = maker_region(*sp, ref, region_id);
                    return r == nullptr ? std::string("--") : r->text;
                },
                [sp, ref, region_id](std::string text) {
                    return write_region_text(*sp, ref, region_id, std::move(text));
                })));
        for (std::size_t axis = 0; axis < 4; ++axis) {
            rows.push_back(Row::edit(
                kAxisLabels[axis],
                Property<std::string>(
                    [sp, ref, region_id, axis] {
                        return region_axis_text(*sp, ref, region_id, axis);
                    },
                    [sp, ref, region_id, axis](std::string text) {
                        return write_region_axis(*sp, ref, region_id, axis, text);
                    })));
        }
        rows.push_back(Row::show("Resolved", [sp, ref, region_id] {
            return region_resolved_text(*sp, ref, region_id);
        }));
        rows.push_back(Row::show("Shown", [sp, ref, region_id] {
            return region_shown_text(*sp, ref, region_id);
        }));
    } else {
        rows.push_back(Row::show("Interior", [sp, ref] { return interior_capture_text(*sp, ref); }));
    }
    return rows;
}

/// THE ROW THAT MUST STAY ON SCREEN: the editing one, else the cursor's -- `inspector_focus`
/// for this inspector, and for its reason.
inline std::size_t pane_editor_focus(const Session& s) {
    for (std::size_t i = 0; i < s.pane_editor.rows.size(); ++i) {
        if (s.pane_editor.rows[i].editing()) {
            return i;
        }
    }
    return s.pane_editor.row_cursor;
}

/// WHERE THE PANE EDITOR'S TWO LISTS ARE, HOW MANY ROWS EACH GETS, AND WHICH MEMBERS ARE
/// SHOWN -- `InfoBodyPlace` without the footer. The `PANES` heading is the region's first
/// prose row; the pane list begins at body row 0; the subject's rows begin at
/// `panes_rows`.
struct PaneEditorBodyPlace {
    bool present = false;
    std::int64_t region_x = 0;
    std::int64_t region_y = 0;
    std::int64_t region_w = 0;
    std::int64_t region_h = 0;
    std::int64_t region_sub_x = 0;
    std::int64_t region_sub_y = 0;
    std::int64_t region_sub_w = 0;
    std::int64_t region_sub_h = 0;
    surface::RegionFit fit{};
    std::int64_t columns = 0;
    std::int64_t value_columns = 0;
    std::size_t capacity = 0;
    std::size_t panes_rows = 0;
    std::size_t field_rows = 0;
    ListWindow panes{};
    ListWindow fields{};
};

inline PaneEditorBodyPlace pane_editor_body_place(const FineRect& outer, const Screen& sc,
                                                  std::size_t total_panes,
                                                  std::size_t pane_cursor,
                                                  std::size_t total_fields,
                                                  std::size_t field_focus) {
    PaneEditorBodyPlace p;
    const PaneInside inside = pane_inside(outer, sc);
    const FineRect panel = inside.rect;
    if (panel.w <= 0 || panel.h <= 0) {
        return p;
    }
    const surface::SurfaceRect wire = wire_rect_of(panel, surface::role::kFill);
    p.region_x = wire.x;
    p.region_y = wire.y;
    p.region_w = wire.w;
    p.region_h = wire.h;
    p.region_sub_x = wire.sub_x;
    p.region_sub_y = wire.sub_y;
    p.region_sub_w = wire.sub_w;
    p.region_sub_h = wire.sub_h;
    p.fit = inside.fit;
    p.columns = p.fit.columns;
    const std::int64_t used = kPropertyMarkCols + kPropertyLabelCols;
    if (surface::cell_of_subs(surface::add_cells(panel.x, panel.w)) -
            surface::cell_of_subs(panel.x) <=
        used) {
        return p;
    }
    p.value_columns = p.fit.columns - used - kPropertyCaretCols;
    if (p.value_columns < 0) {
        p.value_columns = 0;
    }
    p.capacity = p.fit.rows > kPaneEditorHeadingRows
                     ? static_cast<std::size_t>(p.fit.rows - kPaneEditorHeadingRows)
                     : 0;
    if (p.capacity < 2) {
        return p; // one row of each list is the smallest body that says anything
    }
    const BodyShare share =
        share_body_rows(p.capacity, list_demand(total_panes), list_demand(total_fields));
    p.panes_rows = share.objects;
    p.field_rows = share.properties;
    p.panes = list_window(total_panes, pane_cursor, p.panes_rows);
    p.fields = list_window(total_fields, field_focus, p.field_rows);
    p.present = true;
    return p;
}

/// The same resolution for the session a painter is holding -- one call, so no press can
/// resolve the body against a population or a focus the paint did not.
inline PaneEditorBodyPlace pane_editor_body(const Session& s, const Screen& sc,
                                            const FineRect& outer) {
    return pane_editor_body_place(outer, sc,
                                  inventory_rows(s.setup.active, s.panels).size(),
                                  s.pane_editor.cursor, s.pane_editor.rows.size(),
                                  pane_editor_focus(s));
}

inline std::int64_t prose_row_of_editor_pane(const PaneEditorBodyPlace& p, std::size_t index) {
    return p.present ? prose_row_in_window(p.panes, 0, index) : kNoProseRow;
}

inline std::size_t editor_pane_at_prose_row(const PaneEditorBodyPlace& p, std::int64_t row) {
    std::size_t at = 0;
    if (!p.present || !item_at_prose_row(p.panes, 0, p.panes_rows, row, at)) {
        return kNoObject;
    }
    return at;
}

inline std::int64_t prose_row_of_field(const PaneEditorBodyPlace& p, std::size_t index) {
    if (!p.present) {
        return kNoProseRow;
    }
    return prose_row_in_window(p.fields, static_cast<std::int64_t>(p.panes_rows), index);
}

inline std::size_t field_at_prose_row(const PaneEditorBodyPlace& p, std::int64_t row) {
    std::size_t at = 0;
    if (!p.present || !item_at_prose_row(p.fields, static_cast<std::int64_t>(p.panes_rows),
                                         p.field_rows, row, at)) {
        return kNoProperty;
    }
    return at;
}

/// WHERE A POINTER FACT LANDED IN THE PANE EDITOR'S BODY -- `InfoBodyAt`'s shape: the pane
/// open, the body resolved through the same `bounds_of` the painter used, the position
/// located by the same `prose_at`, the heading subtracted here because it was reserved
/// there.
struct PaneEditorAt {
    bool present = false;
    PaneEditorBodyPlace body{};
    ProseAt at{};
};

inline PaneEditorAt pane_editor_at(const Session& s, std::int64_t space, std::int64_t x,
                                   std::int64_t y) {
    const Screen sc = screen_of(s);
    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kPaneEditor, sc);
    if (!where.open) {
        return PaneEditorAt{};
    }
    PaneEditorAt out;
    out.body = pane_editor_body(s, sc, where.rect);
    out.at = prose_at(space, x, y, out.body.region_x, out.body.region_y, out.body.fit);
    out.at.row -= kPaneEditorHeadingRows;
    out.present = out.body.present && out.at.understood && out.at.row >= 0;
    return out;
}

/// THE PANE EDITOR, PAINTED: the inventory with the subject marked, then the subject's rows
/// with a live draft's caret and selection on them. One region, in the active medium's own
/// type; the picker's row spelling for a pane, the property row's spelling for a fact.
inline void paint_pane_editor(surface::SurfaceLayer& layer, const Session& s,
                              const FineRect& b, const Screen& sc,
                              std::int64_t chrome = kPaneChrome) {
    paint_panel_frame(layer, b, chrome);
    const std::vector<CatalogRow> panes = inventory_rows(s.setup.active, s.panels);
    const PaneEditor& ed = s.pane_editor;
    const PaneEditorBodyPlace body = pane_editor_body(s, sc, b);
    if (body.fit.rows <= 0 || body.fit.columns <= 0) {
        return;
    }
    surface::SurfaceTextRegion region;
    region.x = body.region_x;
    region.y = body.region_y;
    region.w = body.region_w;
    region.h = body.region_h;
    region.sub_x = body.region_sub_x;
    region.sub_y = body.region_sub_y;
    region.sub_w = body.region_sub_w;
    region.sub_h = body.region_sub_h;
    // THE HEADING SAYS WHAT THIS IS AND WHETHER THE KEYS ARE HERE -- the Files header's
    // `*`, for reason: arrows that stopped meaning command mode's arrows are
    // arrows a maker is entitled to read the reason for.
    std::string heading = "PANE MANAGER";
    if (pane_editor_has_keyboard(s)) {
        heading += " *";
    }
    // THE PANE CREATOR'S NAME PROMPT TAKES THE HEADING ROW WHILE IT IS OPEN: the
    // layout-name editor's own composition -- a prompt, the line's visible window, and the
    // caret and selection as the REGION's own so each face answers in its voice.
    if (s.pane_naming.open) {
        const std::int64_t cols = pane_name_columns(body.fit.columns);
        const std::string shown = s.pane_naming.line.visible(cols);
        const component::TextBox::VisibleSpan vis = s.pane_naming.line.visible_selection(cols);
        const std::int64_t prompt =
            static_cast<std::int64_t>(std::char_traits<char>::length(kPaneNamePrompt));
        const std::int64_t at = static_cast<std::int64_t>(s.pane_naming.line.caret_column());
        region.caret_row = 0;
        region.caret_col = prompt + (at < static_cast<std::int64_t>(shown.size())
                                         ? at
                                         : static_cast<std::int64_t>(shown.size()));
        if (vis.present()) {
            region.sel_begin_row = 0;
            region.sel_begin_col = prompt + vis.begin;
            region.sel_end_row = 0;
            region.sel_end_col = prompt + vis.end;
        }
        heading = std::string(kPaneNamePrompt) + shown;
    }
    region.rows.push_back(
        surface::SurfaceTextRow{detail::fit(heading, body.fit.columns), surface::role::kAccent});
    if (!body.present) {
        layer.texts.push_back(std::move(region));
        return;
    }
    const auto say_row = [&region](std::string text, std::int64_t role,
                                   std::int64_t ground = surface::role::kNone) {
        region.rows.push_back(surface::SurfaceTextRow{std::move(text), role, ground});
    };
    const auto say_omission = [&](std::size_t how_many, const char* which) {
        if (how_many > 0) {
            say_row(detail::fit(omitted_text(how_many, which), body.columns),
                    surface::role::kMuted);
        }
    };
    // ---- the PANES list: the picker's population, the picker's row, plus a subject mark --
    say_omission(body.panes.before, "earlier");
    for (std::size_t n = 0; n < body.panes.count; ++n) {
        const std::size_t i = body.panes.first + n;
        const CatalogRow& row = panes[i];
        const bool here = !ed.on_rows && i == ed.cursor;
        const bool subject = ed.addressed() && row.ref == ed.subject;
        say_row(std::string(here ? ">" : " ") + (subject ? "*" : " ") +
                    detail::fit(picker_entry_text(
                                    row.name,
                                    pane_state_word(pane_state_of(s.panels, s.setup.active,
                                                                  sc, row)),
                                    row.summary),
                                body.columns - 2),
                here || subject ? surface::role::kAccent : surface::role::kFill);
    }
    say_omission(body.panes.after, "more");
    while (region.rows.size() <
           static_cast<std::size_t>(kPaneEditorHeadingRows) + body.panes_rows) {
        say_row(std::string(), surface::role::kFill);
    }
    // ---- the subject's rows ------------------------------------------------------------
    if (ed.rows.empty()) {
        say_row(detail::fit(ed.addressed() ? "(the subject is gone -- choose a pane above)"
                                           : "(no subject -- choose a pane above)",
                            body.columns),
                surface::role::kMuted);
        layer.texts.push_back(std::move(region));
        return;
    }
    say_omission(body.fields.before, "earlier");
    for (std::size_t n = 0; n < body.fields.count; ++n) {
        const std::size_t i = body.fields.first + n;
        const Row& row = ed.rows[i];
        if (row.section()) {
            // A SECTION IS A BOUNDARY, said the way `PROPERTIES` is said: accent ink on
            // the one ground every ink reads on.
            say_row(detail::fit(row.label(), body.columns), surface::role::kAccent,
                    surface::role::kMuted);
            continue;
        }
        const bool here = ed.on_rows && i == ed.row_cursor;
        std::int64_t role = surface::role::kFill;
        if (row.editing()) {
            role = surface::role::kAlert;
        } else if (!row.editable()) {
            role = surface::role::kMuted;
        }
        say_row(property_row_text(row, here, body.value_columns), role);
        if (row.editing()) {
            region.caret_row = kPaneEditorHeadingRows + prose_row_of_field(body, i);
            region.caret_col = property_caret_column(row);
            const TerminalSelectionSpan marked =
                property_selection_columns(row, body.value_columns);
            if (marked.present) {
                region.sel_begin_row = region.caret_row;
                region.sel_begin_col = marked.begin;
                region.sel_end_row = region.caret_row;
                region.sel_end_col = marked.end;
            }
        }
    }
    say_omission(body.fields.after, "more");
    layer.texts.push_back(std::move(region));
}

// ⚠ `paint_panels` STANDS HERE AND NOT ABOVE, and the reason is the conversion itself
//. This file defines everything before it is used -- there is not one forward
// declaration in it -- and the walk that reaches every pane's painter now reaches
// `paint_layouts`, whose composition is the status row's and belongs beside the status
// row's other halves. So the walk moved down to meet its last painter rather than the
// composition moving up away from what it composes.

/// EVERY PRESENTED PANE, BACK TO FRONT — ONE COMPLETE LAYER EACH.
// WL-FRONT-01, WL-FRONT-05, WL-FRONT-07 -- agents/workshop/planes.md
// WL-MAKER-05 -- agents/workshop/maker-pane.md
inline void paint_panels(surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                         const Screen& sc, const ProjectFrontier& frontier = {}) {
    const Panels& panels = s.panels;
    const std::int64_t lifted = selected_pane(panels);
    for (const std::int64_t kind : effective_pane_order(s.setup.active, panels)) {
        const Panel p{kind};
        const FineRect b = bounds_of(panels, s.setup.active, p.kind, sc).rect;
        if (b.w <= 0 || b.h <= 0) {
            continue;
        }
        const std::int64_t chrome = p.kind == lifted ? kPaneChromeSelected : kPaneChrome;
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            if (p.kind == panel::kBuilder) {
                paint_builder(layer, panels.builder, b, sc, frontier, s.recipes_moved_to,
                              chrome);
            } else if (p.kind == panel::kInfo) {
                paint_info(layer, d, s, b, sc, chrome);
            } else if (p.kind == panel::kEditor) {
                paint_editor(layer, s, b, sc, chrome);
            } else if (p.kind == panel::kProjectFiles) {
                paint_files(layer, s, b, sc, s.keymap, chrome);
            } else if (p.kind == panel::kLayouts) {
                // THE LAYOUT RUN, THE SETUP ASSOCIATION AND THE WORKSPACE FACT --
                // one more arm, in the one walk, and that is the whole of what the
                // conversion cost this function. What it BUYS is the two lines above it:
                // the rectangle is `bounds_of`'s, the order is `effective_pane_order`'s,
                // and a pane a maker put in front of this one is drawn over it.
                paint_layouts(layer, s, b, sc, chrome);
            } else if (p.kind == panel::kPaneEditor) {
                paint_pane_editor(layer, s, b, sc, chrome);
            } else if (is_maker_kind(p.kind)) {
                // THE MAKER'S OWN PANE -- one more arm in the one walk, and that
                // is the whole of what a pane made of DATA costs this function. Its
                // rectangle is `bounds_of`'s, its order is `effective_pane_order`'s, its
                // chrome is the same chrome, and the only thing this arm decides is which
                // painter: the one that reads an authored interior instead of composing one.
                paint_maker_pane(layer, s, b, sc, chrome);
            } else if (is_runtime_kind(p.kind)) {
                // ONE GENERIC ARM FOR EVERY EXTERNAL PANE, and there is no second one to
                // add. The branch above chooses a PAINTER, which placement named as the one
                // thing about a panel kind that genuinely cannot be shared -- and this arm
                // is the case where it can be, because every external pane is presented
                // identically: a header Workshop writes and a region the provider fills. A
                // second provider costs this function nothing at all.
                paint_external(layer, panels, p.kind, b, sc, s.pane_titles, chrome);
            }
        });
    }
    // THE PANE CREATOR'S REGION MARK: over the panes, in the affordances' own
    // position and for their reason -- it says which rectangle of the maker's pane the
    // rows they are editing describe, derived from the same resolution that painted it, and
    // it is drawn on a plane of its own so the pane's own interior cannot cover it.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_creator_region_mark(layer, s, sc);
    });
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_pane_affordances(layer, s, sc);
    });
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_picker(layer, panels, s.setup.active, sc, s.keymap);
    });
    // THE CURRENT-CONDITION VIEW, IN THE PICKER'S OWN PLANE: over the panes it
    // covers, under the screen's own chrome. The band keeps speaking while it is open --
    // what a maker is READING is what is currently true, and what the band SAYS is what
    // just happened, and those are two different sentences that must not cover each other.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_attention(layer, s, sc, frontier);
    });
    // THE CONTEXTUAL-ACTION SURFACE, LAST IN THE BAND: over the picker and the
    // attention view, because it is the band's later, more deliberate gesture -- and it
    // takes the band's keys first for the same reason (`keyboard_context`), so what is
    // frontmost and what answers agree.
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_context(layer, s, sc);
    });
}

/// THE BOTTOM BAND AS ONE PUBLISHED REGION: what the tool just said, and what the keys mean
/// right now.
inline surface::SurfaceTextRegion band_region(const Session& s, const Screen& sc) {
    const ui::Rect b = band_bounds(sc);
    const surface::RegionFit fit = band_fit(sc);
    surface::SurfaceTextRegion band;
    band.x = b.x;
    band.y = b.y;
    band.w = b.w;
    band.h = b.h;
    const std::int64_t budget = fit.rows;
    const std::int64_t columns = fit.columns;
    if (budget <= 0 || columns <= 0) {
        return band;
    }

    const std::string notice = s.notice.empty() ? std::string() : detail::fit(s.notice, columns);
    const std::int64_t notice_role =
        s.notice_is_bad ? surface::role::kAlert : surface::role::kFill;

    // THE LEGEND TAKES WHAT THE NOTICE LEAVES, which is this band's whole composition policy
    // now that the identity has its own band. A character medium's four rows are the
    // notice and three of legend where the context has that many pairs; the shipped face's
    // two are the notice and one, which is exactly the pair it read before the split. No
    // reserved row is spare: the band spent the old blank row on the workspace fact and this
    // keeps that discipline rather than handing one back.
    //
    // While an external pane holds the keyboard and the legend is FULL, the first legend row
    // still says so -- that sentence is keyboard-ownership truth, not a binding list,
    // and where the legend has one row the sentence takes it and the chorded survivors follow
    // in whatever room is left.
    const std::size_t legend_rows =
        budget >= 2 ? static_cast<std::size_t>(budget - 1) : 0;
    std::vector<std::string> legend;
    if (legend_rows > 0) {
        const KeyContext ctx = keyboard_context(s);
        const std::int64_t typing = keyboard_pane(s.panels);
        const RuntimePane* typed_into =
            typing == kNoPaneKind ? nullptr : s.panels.runtime.of_kind(typing);
        // THE SOURCE EDITOR IS THE SECOND KEYBOARD-TAKING PANE, and it gets the same
        // sentence for the same measured reason: keystrokes landing somewhere
        // the screen does not name is the lie this row exists to refuse.
        std::string said;
        if (typed_into != nullptr && s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "typing goes to " + typed_into->name + " @" + typed_into->provider +
                   " -- press elsewhere for Workshop's keys";
        } else if (ctx == KeyContext::kEditor &&
                   s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "typing goes to the source editor -- press elsewhere for Workshop's keys";
        } else if (ctx == KeyContext::kFiles &&
                   s.keymap.resolved_legend() == legend_mode::kFull) {
            // THE BROWSER TAKES KEYS WITHOUT TAKING TEXT, so the sentence says KEYS. The
            // row exists for the same measured reason the two above it do: a maker whose
            // arrows have stopped meaning what they mean in command mode is entitled to
            // read why on the screen rather than infer it from a gesture that did nothing.
            said = "keys go to Project Files -- press elsewhere for Workshop's keys";
        } else if (ctx == KeyContext::kPaneEditor &&
                   s.keymap.resolved_legend() == legend_mode::kFull) {
            said = "keys go to the Pane Manager -- press elsewhere for Workshop's keys";
        }
        if (!said.empty()) {
            if (legend_rows == 1) {
                const std::int64_t rest =
                    columns - static_cast<std::int64_t>(said.size()) - 3;
                const std::vector<std::string> pairs = help_rows(s.keymap, ctx, rest, 1);
                legend.push_back(detail::fit(
                    pairs.empty() ? said : said + " | " + pairs.front(), columns));
            } else {
                legend.push_back(detail::fit(said, columns));
                const std::vector<std::string> pairs =
                    help_rows(s.keymap, ctx, columns, legend_rows - 1);
                for (const std::string& row : pairs) {
                    legend.push_back(row);
                }
            }
        } else {
            legend = help_rows(s.keymap, ctx, columns, legend_rows);
        }
    }

    const auto push = [&band](std::string text, std::int64_t role) {
        band.rows.push_back(surface::SurfaceTextRow{std::move(text), role});
    };
    if (budget >= 2) {
        push(notice, notice_role);
        for (std::string& row : legend) {
            push(std::move(row), surface::role::kMuted);
        }
    } else if (!notice.empty()) {
        // One row: the tool's own voice while it has something to say. The identity line is
        // not a candidate here any more -- it has a band of its own that this budget cannot
        // take away.
        push(notice, notice_role);
    } else {
        const std::vector<std::string> pairs =
            help_rows(s.keymap, keyboard_context(s), columns, 1);
        if (!pairs.empty()) {
            push(pairs.front(), surface::role::kMuted);
        }
    }
    return band;
}

/// The whole screen as one published canvas — an ORDERED LIST OF PLANES.
// WL-FRONT-01, WL-FRONT-07 -- agents/workshop/planes.md
// WL-ATTN-04 -- agents/workshop/attention.md
// WL-DOC-18 -- agents/workshop/document.md
// WL-RGN-05 -- agents/workshop/regions.md
inline surface::SurfaceCanvas paint(const WorkshopDoc& d, const Session& s,
                                    const ProjectFrontier& frontier = {}) {
    const Screen sc = screen_of(s);
    surface::SurfaceCanvas c;
    c.width = sc.w;
    c.height = sc.h;

    // THE WORKSPACE PLANE: what a maker authored, as this workspace places it.
    // It is written whole before any pane is, because a pane is a presentation IN FRONT of
    // the document -- which is what `occupied_at` has answered and what the
    // picture now agrees with instead of merely being told.
    //
    // THE SCREEN'S OWN CHROME IS NOT HERE. It is a plane of its own, added after the panes,
    // for a reason worth stating where both are decided: the bottom band is where the tool
    // SPEAKS, and a panel's backdrop painted over it would take the notice that just told a
    // maker what happened and erase it under the furniture it describes. (The shared top
    // row that used to be this note's other half is retired -- see the band below.)
    //
    // A REFERENCE INTO `c.layers` IS SPENT BEFORE ANY OTHER LAYER IS ADDED. That is not a
    // coincidence to be preserved by care: `paint_panels` is the first thing that grows the
    // vector, and the two lambdas below are the only writers before it.
    c.layers.emplace_back();
    surface::SurfaceLayer* on = &c.layers.back();

    const auto rect = [&on](std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h,
                            std::int64_t role) {
        on->rects.push_back(surface::SurfaceRect{x, y, w, h, role});
    };
    const auto label = [&on](std::int64_t x, std::int64_t y, std::string text,
                             std::int64_t role) {
        on->labels.push_back(surface::SurfaceLabel{x, y, std::move(text), role});
    };

    // The workspace, as a thing with edges a maker can see. Its extent is a
    // session fact, so resizing it visibly changes what a share resolves to
    // while changing no authored value at all.
    rect(kWorkspaceX, kWorkspaceY, s.workspace_w, s.workspace_h, surface::role::kMuted);

    // The scene: the authored elements, as this workspace places them. Painting
    // walks the SCENE, not the document -- so a rectangle on screen is by
    // construction a rectangle the hit test can find.
    const ui::Scene scene = workspace_scene(d, s);
    for (const ui::Placed& p : scene.items) {
        const std::int64_t x = kWorkspaceX + p.rect.x;
        const std::int64_t y = kWorkspaceY + p.rect.y;
        if (p.id == s.selected) {
            rect(x - 1, y - 1, p.rect.w + 2, p.rect.h + 2, surface::role::kAccent);
        }
        rect(x, y, p.rect.w, p.rect.h, surface::role::kFill);
        // The label, written on the object, clipped to the workspace rather than
        // allowed to run into the panel beside it. The label is authored, so it
        // is read from the element and not from the observation of it.
        //
        // AND IT IS SEMANTIC TYPE ON MATERIAL SOMEBODY ELSE OWNS, which is the
        // one place in this tool where that sentence has to be argued rather than assumed.
        // The name is semantic -- it is the maker's word for this object and its exact cell
        // occupancy is no part of what they authored -- so it belongs in a bounded region.
        // Its rectangle, though, is already full: the object's body is authored MATERIAL,
        // drawn one line up as a `SurfaceRect`. An ordinary region over it erases that
        // material in both media, and rows carrying the object's role as a GROUND leave a
        // `12h - 4 - 18*rows` pixel band the strips cannot reach (10 px across the foot of a
        // default 12x4 object; `12h - 4 == 18k` has no integer solutions, so SOME remainder
        // exists at every height). Both were built and run live, twice -- once at first and
        // once again to re-measure them. `surface::kGroundBeneath` is the third
        // answer: the region keeps its bounds, so the name is fitted and cut against them,
        // and gives up the ground, so nothing under it is painted over.
        //
        // THE BOUND IS THE OBJECT'S OWN RESOLVED WIDTH, clipped by the workspace's
        // right edge -- and earlier it was only the second of those. The name used to be
        // given `workspace_w - x` cells, so a name longer than the object it names ran out of
        // it and across the backdrop; the re-measure preserved that deliberately and then MEASURED
        // what it costs, which is the paragraph below. The room is the material's, because
        // this is type ON material and material the object does not have is not this name's
        // room to spend. The workspace clip stays because it answers a different question --
        // an object may be authored wider than the room to the edge, and its name is still
        // not the panel's to write into.
        //
        // ...OR ONE COLUMN, WHICHEVER IS MORE, for the row floor's reason said about the
        // other axis (below): a zero-WIDTH object is reachable from a poke or a hand-built
        // document exactly as a zero-height one is, and one cell of room leaves `detail::fit`
        // a mark to put there rather than leaving the object with no trace at all.
        //
        // WHAT A MEDIUM STILL GETS TO SAY IS HOW MANY CHARACTERS THOSE CELLS HOLD, and that
        // half is and unchanged: `fit_region` answers 12 columns in cells and 17
        // columns of a 13pt face for a 12-cell object, so a name is marked when it genuinely
        // did not fit rather than when it would not have fitted as bitmap cells.
        //
        // AND ITS HEIGHT IS THE OBJECT'S, which is what makes a one-cell object honest for
        // free. `fit_region` sends a region with no room for a row of the medium's face back
        // to the cell projection, so an object a maker sized to one cell shows its
        // name in cells -- the same picture a terminal shows -- rather than 18 pixels of type
        // hanging out of a 12-pixel object. No `if (h < N)` was written here; the rule is the
        // one both media already resolve with.
        //
        // ...OR ONE ROW, WHICHEVER IS MORE, and that floor is not a fudge: a name is written
        // ON a row, so the room it needs is a row, and an object whose resolved height is
        // zero still has the row its origin is on. `check_extent` refuses an authored height
        // below one cell, so this is reachable only from a poke or a hand-built document --
        // but it WAS reachable earlier and such an object's name was the only trace of
        // it on the workspace, and a region with no bounds shows nothing and says nothing
        // about it. Measured: without the floor, three zero-height objects lost their names
        // outright. The floor restores byte-for-byte the run of cells the label drew, in
        // every medium, because one cell of room is a cell region either way.
        //
        // THE CUT IS MARKED, and earlier it was not. `resize` here was a silent
        // truncation of a string a MAKER chose (up to `doc::kMaxNameLen`), which is the exact
        // defect found in the picker's name column and repaired the same way: a shorter
        // name that looks finished is a lie about the document. `detail::fit` marks it.
        //
        // AND WHY THE ROOM IS THE MATERIAL'S, WRITTEN HERE BECAUSE IT IS THIS CALL SITE'S.
        // The re-measure found the cost of the old bound in a medium that paints roles as ink: the
        // name is `kMuted` so it reads quietly on the object's `kFill` body, and the workspace
        // backdrop a few statements up is ALSO `kMuted` -- so every character past the
        // object's own edge was the backdrop's exact colour and could not be read at all. Six
        // cells of material and a thirty-two byte name meant 9 characters legible and 23
        // invisible, measured on the pristine tree. Earlier the overhang was legible
        // only for a reason nobody chose: every label cell was cleared to the canvas
        // background first, which is the same hole in the workspace that it was in the object.
        //
        // NO ROLE FIXES THAT, WHICH IS WHY THE ANSWER IS THE BOUND. This medium's inks are
        // kFill 176, kAccent 112/232/240, kMuted 96 and kAlert red: nothing reads on BOTH a
        // `kFill` body and a `kMuted` backdrop, `kAccent` means "the one thing being pointed
        // at" and would make every object shout, and a fifth role is exactly what
        // `surface/vocabulary.hpp` refuses. Contrast is a palette question and the palette is
        // the medium's -- which is the whole reason a publisher ships roles. So the repair is
        // not a colour and not a ground: it is that a name never leaves the material it names,
        // and where it does not fit that material it says so with `detail::fit`'s mark. The
        // authored name is untouched by any of it, and widening the object reveals more of the
        // same authored bytes -- which is the property the whole arrangement is for.
        const ui::Element* authored = doc::find(d, p.id);
        const std::int64_t columns = p.rect.w > 1 ? p.rect.w : 1;
        const std::int64_t to_edge = s.workspace_w - p.rect.x;
        const std::int64_t room = columns < to_edge ? columns : to_edge;
        if (authored != nullptr && room > 0) {
            const std::int64_t rows = p.rect.h > 1 ? p.rect.h : 1;
            const surface::RegionFit fit =
                surface::fit_region(x, y, room, rows, sc.text_advance_px, sc.text_line_px);
            surface::SurfaceTextRegion named;
            named.x = x;
            named.y = y;
            named.w = room;
            named.h = rows;
            named.ground = surface::kGroundBeneath;
            named.rows.push_back(surface::SurfaceTextRow{
                detail::fit(authored->label, fit.columns), surface::role::kMuted});
            on->texts.push_back(std::move(named));
        }
    }

    // The size handle, over everything in the workspace, as a GLYPH rather than
    // as another rectangle. That is not decoration: the ring already paints this
    // exact cell in the accent role, so a rect here would be invisible, and the
    // affordance has to be distinguishable from the ring, from the object's body
    // and from the workspace at a glance. `SurfaceLabel` carries arbitrary text
    // over the rects, so the generic canvas vocabulary already had what this
    // needed -- no role was added, and nothing in surface/ or ui/ changed.
    // (Honest cost: a Skin with no text stack draws no handle. Both shipped
    // media have one -- a terminal's own font, and the SDL medium's bitmap face
    // in surface/skin_sdl_glyphs.hpp -- so nothing declines it today.)
    const Handle handle = size_handle(d, s);
    if (handle.shown) {
        label(kWorkspaceX + handle.x, kWorkspaceY + handle.y, kHandleGlyph,
              surface::role::kAccent);
    }

    // THE DYNAMIC PANELS -- every one of them, INCLUDING the OBJECTS and PROPERTIES columns
    // a maker has always read on the right. Each takes a PLANE of its own, in canonical
    // front order, so a later-ranked pane covers an earlier one kind for kind.
    //
    // THIS ONE CALL IS THE WHOLE OF A REMOVABLE INFO AT THIS LEVEL. What used to be forty lines of
    // furniture painted unconditionally here is now a panel like any other: present because a
    // fresh session opens it, absent the moment a maker removes it, and painted by whoever
    // owns that kind rather than by `paint`.
    paint_panels(c, d, s, sc, frontier);

    // AND THE SCREEN'S OWN CHROME OVER THEM, on its own plane -- which is a
    // budget-composed region rather than one label per cell row, and is ONE of
    // them: the bottom band, where the tool speaks and where the keys are explained. See the
    // note at the top of this function for why it is in front rather than behind: a band is
    // where the tool SPEAKS, and a panel backdrop drawn over one would erase the notice that
    // just told a maker what happened.
    //
    // ⚠ THE TOP BAND IS NOT HERE ANY MORE. The layout selector, the setup
    // association and the workspace fact were the other half of this plane and are an
    // ordinary pane now -- painted by `paint_panels` above, in canonical front order, over
    // and under whatever a maker arranged around them. The ROWS they defaulted to are still
    // reserved (`kTopRows`, and `room_h` is byte-identical either way); what changed is that
    // something authorable stands on them instead of something this function drew.
    //
    // THE OLD SHARED TOP ROW IS STILL RETIRED, AND ITS CELL IS SPENT NOW.
    // Canvas row 0 carried four one-cell voices -- the workspace's extent, the picker and
    // window hints, the terminal hint -- each structurally unable to hold a row of a real
    // face. The band conversion moved those facts into the band and left the row EMPTY, because the
    // workspace's extent is what a share resolves against and a chrome retirement must not
    // resize a maker's document. The split spends that cell, and one more from the bottom band,
    // on a top band two cells tall -- which is what a face needs for one row of type. The
    // reserved total is what it was, so the workspace still did not move.
    //
    // ⚠ THE BOTTOM BAND BELONGS TO THE OVERLAY WHILE THAT IS OPEN, AND THE LAYOUTS PANE
    // DOES NOT. The Terminal is anchored to the bottom-right corner and covers most of the
    // screen's width at every extent, so bottom-band rows painted underneath it would
    // survive only in the cells to its left -- a sentence beheaded mid-word with nothing to
    // say so. The Layouts pane's default rows are ones the overlay cannot reach:
    // `terminal_y` is `h - terminal_h`, which is 9 at the minimum screen and grows with the
    // surface, so it is never less than `kTopRows`. A maker in the Terminal therefore keeps
    // reading which layout they are in, which is the honest answer rather than a courtesy --
    // those rows are not covered, so hiding them would be a lie about occlusion. A maker who
    // MOVED the pane under the overlay is covered by it and correctly so, which is a thing
    // this screen could not say at all until the conversion.
    //
    // A REGION TAKES ITS RECTANGLE, and that is a deliberate widening over the labels it
    // replaced: the old rows cleared only the cells their characters landed on, and a band
    // clears all of its rows across the canvas. A pane a maker authors over the bottom band
    // is covered BY it, because the panes are in front of the DOCUMENT and not in front of
    // the tool's own voice, and the band occupies no pointer space at all.
    //
    // ⚠ THAT LAST EXEMPTION USED TO HAVE AN EXCEPTION AND NO LONGER DOES. The top
    // band painted in front of every pane and answered presses on the layout tabs alone, so
    // a pane dragged under it was visually erased and still met the hand -- see-here,
    // press-there, at exactly the boundary one geometry exists to forbid. Both halves are gone: the
    // tabs are a pane's interior, and `occupied_at` answers that pane for those cells like
    // any other.
    if (!s.terminal.open) {
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            layer.texts.push_back(band_region(s, sc));
        });
    }

    if (s.terminal.open) {
        // THE FINAL MODAL PLANE, and that is the whole of what "overlay" means here. A pane
        // in the last layer covers whatever it lands on -- and the screen underneath is
        // composed exactly as it was before this phase, with no row budget taken from it and
        // no constant moved. A closed pane appends no layer at all.
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            paint_terminal(layer, s.terminal, sc, s.keymap);
        });
    }

    // THE HOTKEY VIEW, LATER STILL: a maker can open it OVER the Terminal to read
    // the Terminal line's own keys, so it must be readable above the pane whose context it
    // is describing. It is the one plane after the Terminal's, and it is a projection --
    // the screen beneath it, the Terminal included, is composed exactly as if it were
    // closed, which is also why the context it reports is the context beneath it.
    if (s.hotkeys.open) {
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            paint_hotkeys(layer, s, sc);
        });
    }

    return c;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_HPP
