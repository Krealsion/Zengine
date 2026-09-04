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
// WL-PANE-15 -- agents/workshop/panes-and-windows.md
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
PaneInside pane_inside_at(const FineRect& outer, const Screen& sc,
                                 std::int64_t chrome_subs);

} // namespace detail

/// THE ONE CALL. A pane's outer rectangle in, its interior and that interior's resolution
/// out -- and the boundary between them is the finest one the face in front of the maker
/// will actually present.
PaneInside pane_inside(const FineRect& outer, const Screen& sc);

/// The rectangle inside a pane's chrome, for a consumer that wants only the geometry.
FineRect pane_interior(const FineRect& outer, const Screen& sc);

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
bool pane_unit_projectable(const SetupPane* authored) noexcept;

/// THE DEVELOPER'S ANSWER, THEN THE MAKER'S, PER AXIS -- and then the canvas.
PaneProjection project_pane(std::int64_t where, std::size_t slot,
                                   const SetupPane* authored, const Screen& sc);

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
PanelBounds bounds_of(const Panels& panels, const Setup& setup, std::int64_t kind,
                             const Screen& sc);

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
// WL-PANE-15 -- agents/workshop/panes-and-windows.md
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

PointedAt canvas_point_of(std::int64_t space, std::int64_t x, std::int64_t y) noexcept;

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
Occupancy occupied_at(const Panels& panels, const Setup& setup, const Screen& sc,
                             const PointedAt& at);

/// The same walk for a CELL-GRAIN probe: which presentation occupies this canvas cell —
/// a well-formed question a terminal pointer asks natively and a cell-lattice consumer
/// (a suite case included) may ask directly. One line, so the two spellings cannot
Occupancy occupied_at(const Panels& panels, const Setup& setup, const Screen& sc,
                             std::int64_t cx, std::int64_t cy);

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
FineRect pane_edge_cell(const FineRect& r, std::int64_t edge) noexcept;

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
std::int64_t pane_edge_at(const FineRect& r, std::int64_t sx, std::int64_t sy,
                                 std::int64_t grain) noexcept;

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
bool doubles_a_click(const ClickMemory& prior, std::int64_t place, std::uint64_t epoch,
                            const component::WordSpan& word, std::int64_t now_ms) noexcept;

/// WHAT THE LAST PRESS ON A LAYOUT TAB NAMED, so the next one can be a double.
// WL-TAB-10 -- agents/workshop/tab-run.md
struct TabClickMemory {
    bool armed = false;
    std::size_t at = 0;      ///< the position the press landed on
    std::int64_t at_ms = 0;  ///< `interaction_now_ms()` when it landed
};

/// IS THIS PRESS THE SECOND HALF OF A DOUBLE-CLICK ON THE SAME TAB? Pure, total.
bool doubles_a_tab_click(const TabClickMemory& prior, std::size_t at,
                                std::int64_t now_ms) noexcept;

/// WHICH LAYOUT TAB A REORDER DRAG IS CARRYING -- the fourth gesture record.
// WL-TAB-11 -- agents/workshop/tab-run.md
struct LayoutTabDrag {
    bool active = false;
};

/// The arming a press leaves behind -- written from the same three facts the test above
/// reads, so an arming that could not qualify cannot be written.
ClickMemory click_landed(std::int64_t place, std::uint64_t epoch,
                                const component::WordSpan& word, std::int64_t now_ms) noexcept;

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
bool editor_has_keyboard(const Session& s);

/// DOES THE PROJECT BROWSER HAVE THE KEYBOARD RIGHT NOW?
bool files_has_keyboard(const Session& s);

/// IS THE PANE EDITOR THE PANE A MAKER LAST PRESSED INTO, WITH SOMETHING TO SHOW?
bool pane_editor_has_keyboard(const Session& s);

/// IS A DRAFT LIVE ON ONE OF THE PANE EDITOR'S ROWS? Its own question, kept apart from the
/// Info panel's `draft_live` on purpose: the two drafts are about different subjects, and
/// the refusals Info spends its answer on (a press on the object list rebuilds Info's rows)
/// are not true of a draft that a change of document selection cannot touch.
bool pane_editor_draft_live(const Session& s);

/// THE CHAIN BELOW THE CONTEXTUAL SURFACE -- the branches a key falls to once no mode above
/// them claims it. Split out of `keyboard_context` because the contextual surface needs
/// exactly this half as a VALUE: what the keys would mean when the menu closes.
KeyContext keyboard_context_beneath_menu(const Session& s);

/// WHERE THE KEYBOARD CURRENTLY GOES, AS ONE VALUE -- the routing chain, spelled once. It
/// is resolved fresh from live session state at every spend and stored nowhere: there is
/// no context stack, and a mode that closes stops being the answer with nothing to clear.
KeyContext keyboard_context(const Session& s);

/// MAY ESCAPE'S FINAL FALLTHROUGH SHED THE PANE SELECTION IN THIS CONTEXT?
bool escape_may_shed_selection(KeyContext c);

// ---- Spelling the effective bindings -----------------------------------------------------

/// The effective gesture of one action, in the screen's compact voice (`^s`, `shift+h`,
/// `enter`). The one call every claim site makes.
std::string hotkey_text(const Keymap& k, Act a);

/// Four direction actions said as one word WHEN THAT WORD IS TRUE: `arrows` exactly while
/// all four sit on their arrow defaults, their own spellings otherwise. The old headings
/// hand-folded four gestures into `arrows`; the fold survives only as long as it is a
/// fact.
std::string arrows_text(const Keymap& k, Act left, Act right, Act up, Act down);

/// The `gesture label` pairs requestable in this context, one string each, in the order
/// the band should spend room on them: the context's own rows first, then what is
/// answered above the mode chain.
std::vector<std::string> help_pairs(const Keymap& k, KeyContext ctx);

// The band's legend rows are packed from `help_pairs` by `help_rows` below `detail` --
// against however many rows the band's budget composition granted the legend, which is
// what stopped being a constant two.

/// TAKE THE ROOM A SURFACE OFFERED, and re-fit the workspace to it. Answers whether anything
/// actually changed, so a caller can decline to repaint over a surface that merely repeated
/// itself.
bool adopt_screen(Session& s, std::int64_t want_w, std::int64_t want_h,
                         std::int64_t want_advance_px = 0, std::int64_t want_line_px = 0,
                         std::int64_t want_cell_px = 0);

/// The workspace as a viewport, and the document resolved against it — the ONE
/// call that turns authored intent into geometry in this application.
ui::Scene workspace_scene(const WorkshopDoc& d, const Session& s);

/// The inspector for one authored object: the properties, plus the facts that
/// are not properties.
std::vector<Row> inspector_rows(WorkshopDoc& d, const Session& s);

/// Where the cursor belongs on a freshly built inspector: the first row a maker
/// can actually author. Landing it on `Identity` instead would open onto a row
/// whose only possible answer to "edit this" is a refusal.
std::size_t first_editable(const std::vector<Row>& rows);

/// Rebuild the inspector for the current selection and put the cursor somewhere
/// useful. One gesture, so the running weave and the suite cannot come to
/// disagree about what a fresh inspector is -- and rebuilding rather than
/// patching is why nothing in this package has a "refresh the inspector" call.
void refocus(WorkshopDoc& d, Session& s);

/// REBUILD THE INSPECTOR WITHOUT TAKING A MAKER'S HANDS OFF IT.
void refocus_keeping_draft(WorkshopDoc& d, Session& s);

/// Where an identity sits in DOCUMENT ORDER, or `elements.size()` for one this
/// document does not have.
///
/// One copy, because there were about to be three. The post-delete selection
/// rule needs it and so does the object list's visible window, and "where is
/// this object in the file" is exactly the kind of small answer that goes stale
/// when it is written twice, at the smallest scale that lesson comes in.
std::size_t position_of(const WorkshopDoc& d, std::int64_t id);

namespace detail {

/// Left-align in a fixed width; longer text is cut. Workshop's own layout job --
/// the canvas has no notion of a column. Its one caller pads the inspector's
/// label column to nine, and the longest label this tool has is `Resolved`, so
/// the cut is arithmetic that never fires rather than a bound anybody is
/// standing on; `fit` below is the one for text whose length a DOCUMENT decides.
std::string pad(std::string text, std::size_t width);

/// The mark a bounded presentation leaves where it could not show everything.
/// Three plain characters, because this canvas is plain ASCII by contract
/// (`SurfaceLabel`: "plain means plain") and a glyph a medium cannot draw is a
/// mark a maker cannot read.
inline constexpr const char* kElided = "...";

/// Fit `text` into `width` cells, AND SAY SO when it did not fit.
std::string fit(std::string text, std::int64_t width);

/// HOW MUCH OF A PATH IS THE CUE THAT SAYS WHICH FILESYSTEM IT IS ON -- `/`, `C:/`,
/// `//server/`, or nothing at all for a spelling that has no root.
std::size_t path_root_cue(const std::string& p);

/// FIT A PATH, KEEPING THE END THAT SAYS WHICH FILE OR DIRECTORY IT IS.
std::string fit_path(const std::string& path, std::int64_t width);

// ---- Reading past the ellipsis -----------------------------------------------------------

/// THE FURTHEST A ROW MAY BE SCROLLED: exactly enough to bring the last byte into view, and
/// never one further. It is what makes the right edge of the row mean "the end", rather than
/// meaning "somewhere past the end" with blank cells after it.
std::size_t reveal_max_offset(const std::string& full, std::int64_t columns);

/// WHICH OFFSET THE POINTER IS ASKING FOR, from the column it is on.
std::int64_t reveal_offset_at_column(const std::string& full, std::int64_t columns,
                                            std::int64_t column);

/// ONE ROW, SHOWN FROM `offset` BYTES IN. Total at every width and every offset, and never
/// wider than `columns`: the mark is spent first and `fit` bounds whatever is left, so the
/// widest answer this can give is exactly the width the fitted answer would have taken.
std::string revealed_row(const std::string& full, std::int64_t columns,
                                std::int64_t offset);

/// WHAT A PAINTER PUTS ON A REVEALABLE ROW -- its ordinary answer, unless the pointer is on
/// THIS row of THIS surface showing THIS string.
std::string reveal_shown(const Revealed& rev, std::int64_t place, std::size_t item,
                                const std::string& full, std::string rest,
                                std::int64_t columns);

/// How far a wrapped continuation row is indented, so a reader can tell one sentence
/// running on from a new one starting. Two cells: enough to be visible under the sigils
/// every transcript line begins with (`> `, `-- `, `!! `, `^ `, `v `), and cheap enough that
/// a pane loses almost none of its width to it.
inline constexpr std::int64_t kWrapIndent = 2;

/// FIT `text` INTO AS MANY ROWS AS IT NEEDS, at most `width` cells each.
std::vector<std::string> wrap(const std::string& text, std::int64_t width);

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
std::int64_t step(std::int64_t v, std::int64_t by) noexcept;

/// `a - b`, without leaving the number line — `step`'s partner, and needed for
/// the same reason. A resize's proposal is a DIFFERENCE (`pointer - the object's
/// own edge`), and both terms are values this weave does not own: the pointer
/// comes off the wire and the edge comes off a poke-writable document. The
/// saturated end is far outside any workspace, which already means "nothing
/// reachable there".
std::int64_t minus(std::int64_t a, std::int64_t b) noexcept;

} // namespace detail

/// The band's legend rows, as the legend projects them, budget-composed.
std::vector<std::string> help_rows(const Keymap& k, KeyContext ctx,
                                          std::int64_t width, std::size_t rows);

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

PaneWindowProposal pane_window_proposal(std::int64_t edge, std::int64_t base_x,
                                               std::int64_t base_y, std::int64_t base_w,
                                               std::int64_t base_h, std::int64_t dx,
                                               std::int64_t dy) noexcept;

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
std::int64_t create(WorkshopDoc& d, Session& s);

/// Delete the selected object.
Written delete_selected(WorkshopDoc& d, Session& s);

/// Put an object where a HAND asked for it, IN WORKSPACE CELLS — the one place a
/// proposed position meets the boundary policy, and the only door `nudge` and
/// `drag_to` use.
Handled place(WorkshopDoc& d, const ui::Scene& scene, std::int64_t id, std::int64_t gx,
                     std::int64_t gy);

/// Step the selected object one cell — the keyboard's move gesture, and the only
/// one the canonical POSIX lane can perform at all (that lane produces no pointer
/// events; see workshop.cpp).
Handled nudge(WorkshopDoc& d, Session& s, std::int64_t ddx, std::int64_t ddy);

// ---- The size a hand asked for, as an authored extent ----------------------------------

/// The authored extent a maker's HAND asks for, when it asks for a resolved size.
ui::Extent extent_from_drag(const ui::Extent& current, std::int64_t want,
                                   std::int64_t span, std::string& boundary);

/// Author a new size from a proposal in RESOLVED cells — the shape both the
/// pointer and the keyboard arrive in, and the one place either of them becomes
/// an authored extent.
Handled size_to(WorkshopDoc& d, const Session& s, std::int64_t id, std::int64_t want_w,
                       std::int64_t want_h);

/// Grow or shrink the selected object by whole RESOLVED cells — the keyboard's
/// resize gesture, and the canonical lane's only one.
Handled grow(WorkshopDoc& d, Session& s, std::int64_t dw, std::int64_t dh);

// ---- The one resize affordance ---------------------------------------------------------

/// Where the selected object's size handle is, in WORKSPACE cells.
// WL-DOC-09 -- agents/workshop/document.md
struct Handle {
    bool shown = false;
    std::int64_t id = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
};

Handle size_handle(const WorkshopDoc& d, const Session& s);

/// Take hold of whatever authored object is under a workspace cell. Returns the
/// identity taken hold of, or 0 for empty space.
std::int64_t begin_drag(const WorkshopDoc& d, Session& s, std::int64_t cx,
                               std::int64_t cy);

/// What a press takes hold of: the selected object's SIZE HANDLE if the press
/// landed on it, otherwise whatever object's body is under the cell. Returns the
/// identity taken hold of, or 0.
std::int64_t take_hold(WorkshopDoc& d, Session& s, std::int64_t cx, std::int64_t cy);

/// THE OBJECT UNDER A WORKSPACE CELL, AND NOTHING ELSE -- `take_hold`'s pure half.
std::int64_t object_at(const WorkshopDoc& d, const Session& s, std::int64_t cx,
                              std::int64_t cy);

/// Where the gesture in flight now proposes the object should BE, or how big it
/// should be — committed through the document's one position operation or its one
/// size operation.
Handled drag_to(WorkshopDoc& d, const Session& s, std::int64_t cx, std::int64_t cy);

void end_drag(Session& s);

// ---- Where a pointer is, in workspace cells --------------------------------------------


/// WHERE A POINTER LANDED INSIDE A BOUNDED TEXT REGION, in that region's own prose.
// WL-INFO-04 -- agents/workshop/info-body.md
struct ProseAt {
    bool understood = false;
    std::int64_t column = 0;
    std::int64_t row = 0;
};

ProseAt prose_at(std::int64_t space, std::int64_t x, std::int64_t y,
                        std::int64_t region_x, std::int64_t region_y,
                        const surface::RegionFit& fit) noexcept;

/// The workspace cell a CANVAS cell lands on -- Workshop's own composition, and
/// nothing else.
std::int64_t workspace_cell_x(std::int64_t canvas_x) noexcept;
std::int64_t workspace_cell_y(std::int64_t canvas_y) noexcept;

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
ListWindow list_window(std::size_t total, std::size_t selected_at, std::size_t rows);

/// What one omission marker says. `... 2 earlier` / `... 4 more`: a count and a direction.
std::string omitted_text(std::size_t how_many, const char* which);

// ---- Rendering one participant's record ------------------------------------------------

/// WHERE A SUBMITTED MESSAGE WAS ADDRESSED, in the SAME three sigils the command line reads.
std::string terminal_address(const loom::TranscriptEntry& e);

std::string terminal_shape(const loom::TranscriptEntry& e);

/// ONE TRANSCRIPT ENTRY AS ONE LINE.
std::string terminal_line(const loom::TranscriptEntry& e);

/// WHAT `^` MEANS, said once, on a row the pane always shows.
std::string terminal_legend();

/// ONE TRANSCRIPT ENTRY AS THE ROWS A PANE THIS WIDE SPENDS ON IT.
std::vector<std::string> terminal_wrapped(const loom::TranscriptEntry& e,
                                                 std::int64_t width);

/// HOW MANY OF THE NEWEST ENTRIES A PANE THIS WIDE AND THIS TALL CAN SHOW WHOLE.
std::size_t entries_that_fit(const std::vector<loom::TranscriptEntry>& entries,
                                    std::int64_t width, std::size_t rows);

/// WHAT THE PANE IS NOT SHOWING, in two numbers that are two different facts.
std::string terminal_omission(const TerminalPane& t);

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
std::int64_t terminal_caret_column(const TerminalInputPlace& p,
                                          const component::TextBox& box) noexcept;

/// THE BYTE INDEX A PROSE COLUMN NAMES, clamped into the line the pane is showing.
std::size_t terminal_caret_of_column(const TerminalInputPlace& p,
                                            const component::TextBox& box,
                                            std::int64_t column) noexcept;

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
TerminalSelectionSpan terminal_selection_columns(const TerminalInputPlace& p,
                                                        const component::TextBox& box) noexcept;

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
std::vector<surface::SurfaceTextRow> completion_rows(const Completion& comp,
                                                            std::size_t capacity,
                                                            std::int64_t width);

/// The overlay, painted OVER the finished screen.
void paint_terminal(surface::SurfaceLayer& layer, const TerminalPane& t,
                           const Screen& sc, const Keymap& keymap);

// ---- The dynamic panels, painted -------------------------------------------------------

/// THE BACKDROP OF A PANEL: its whole bounds, in one rect.
void paint_panel_frame(surface::SurfaceLayer& layer, const FineRect& b,
                              std::int64_t role);

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
PanelProsePlace panel_prose_place(const FineRect& b, const Screen& sc);

/// The region a `PanelProsePlace` was resolved for, empty and ready for its rows — the fine
/// bounds decomposed onto the wire's cells-plus-remainder spelling.
surface::SurfaceTextRegion panel_prose_region(const PanelProsePlace& place);

/// A panel's own field: a fixed-width label and its value, so the values line up down the
/// panel and a maker reads a column rather than a paragraph.
std::string panel_field(const char* label, const std::string& value);

/// A field whose value is longer than a row: wrapped across a fixed row budget, and MARKED
/// when the budget ran out before the sentence did.
std::vector<std::string> panel_block(const char* label, const std::string& value,
                                            std::size_t rows, std::int64_t width);

/// THE BUILDER PANEL — Workshop's presentation of a weave it does not own.
void paint_builder(surface::SurfaceLayer& layer, const BuilderPane& pane,
                          const FineRect& b, const Screen& sc,
                          const ProjectFrontier& frontier = {},
                          const std::string& catalog_moved_to = std::string(),
                          std::int64_t chrome = kPaneChrome);

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
const char* pane_state_word(std::int64_t state);

/// WHAT A MAKER CAN DO ABOUT ONE STATE -- the remedy column of the table above, as a
/// function.
const char* pane_state_remedy(std::int64_t state);

/// HOW WIDE THE STATE COLUMN IS.
// WL-PANE-10 -- agents/workshop/panes-and-windows.md
inline constexpr std::size_t kPaneStateCols = 11;

/// HOW WIDE THE NAME COLUMN IS -- and it is a bound a party outside this build can
/// reach, which is what makes it a constant rather than the `10` it used to be.
// WL-PED-01 -- agents/workshop/pane-manager.md
inline constexpr std::size_t kPickerNameCols = 13;

/// IS EVERY VISIBLE CELL OF THIS PANE BEHIND ANOTHER ONE?
bool pane_is_covered(const Panels& panels, const Setup& setup, const Screen& sc,
                            std::int64_t kind, const FineRect& mine);

/// THE ONE STATE CLASSIFIER. Asked of an inventory row -- which is the union of the catalog
/// and everything the setup names -- so every authored pane gets exactly one answer and no
/// row is silently omitted because the runtime catalog lacks it.
std::int64_t pane_state_of(const Panels& panels, const Setup& setup, const Screen& sc,
                                  const CatalogRow& row);

/// The one row-body spelling, so the painter and any reader of the picker's
/// columns spend the same two column widths.
std::string picker_entry_text(const std::string& name, const char* state,
                                     const std::string& tail);

/// The `+ panel` picker: the catalog, where a maker's cursor is in it, and WHICH KINDS ARE
/// ALREADY OPEN, in a fixed column so the list reads down; it asks for the stack's first
/// slot through `picker_bounds` rather than knowing where that is.
void paint_picker(surface::SurfaceLayer& layer, const Panels& panels, const Setup& setup,
                         const Screen& sc, const Keymap& keymap);


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
const char* geometry_unit(std::int64_t cell_px);

/// ONE FINE COORDINATE OR EXTENT, SPELLED FOR THIS MEDIUM.
GeometrySpelling geometry_spelling(std::int64_t subs, std::int64_t cell_px);

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
std::string geometry_amount_text(std::int64_t subs, std::int64_t cell_px,
                                        bool& any_projected);

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

FaceAmount parse_face_amount(std::string_view text, std::int64_t cell_px);

/// A WHOLE FINE RECTANGLE, IN THE ACTIVE MEDIUM'S UNIT -- `@x,y WxH unit`.
std::string fine_rect_text(const FineRect& r, std::int64_t cell_px);

/// WHAT A MAKER AUTHORED FOR ONE PANE'S WINDOW, in the active medium's own unit.
std::string pane_window_text(const SetupPane* row, std::int64_t cell_px);

/// IS ANY PART OF THIS PANE'S WINDOW STILL THE CODE'S ANSWER RATHER THAN THE MAKER'S?
bool pane_window_partly_default(const SetupPane* row);


// ---- A SURFACE SIZED BY WHAT IT SAYS, PLACED ---------------------------------------------

/// WHERE A SURFACE SIZED BY ITS OWN CONTENT OPENS, asked at an anchor: the arithmetic
/// `context_bounds` has spent, quarried out so the full hotkey view spends the same sentence.
FineRect popup_bounds_at(std::int64_t want_cols, std::int64_t want_rows,
                                std::int64_t x, std::int64_t y, const Screen& sc);

// ---- THE FULL HOTKEY VIEW -------------------------------------------------------------

/// What to call the context beneath the view, in the heading's voice.
std::string keyboard_context_name(const Session& s, KeyContext ctx);

/// ONE ROW OF THE VIEW AS IT IS PRESENTED: what a maker reads, and the role it is said in.
struct HotkeyRow {
    std::string text;
    std::int64_t role;
};

/// THE ROWS, COMPOSED WHOLE -- the view's one composition, spent by its extent and by its
/// painter alike.
std::vector<HotkeyRow> hotkeys_rows(const Session& s);

/// WHERE THE FULL HOTKEY VIEW OPENS, AND HOW BIG IT IS.
FineRect hotkeys_bounds(const Session& s, const Screen& sc);

void paint_hotkeys(surface::SurfaceLayer& layer, const Session& s, const Screen& sc);

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
std::string pane_content_key(const PaneRef& ref);
std::string pane_window_key(const PaneRef& ref);
inline constexpr const char* kFrontierKey = "project.frontier-waiting";

/// EVERY CONDITION THAT IS CURRENTLY TRUE AND WORTH AMBIENT ATTENTION, ranked.
std::vector<Condition> attention_conditions(const Session& s,
                                                   const ProjectFrontier& frontier = {});

/// ...LESS THE ONES THIS SESSION HAS HIDDEN. The one function every presentation spends, so
/// the compact indicator, the view, the cursor bound and the dismissal all agree about which
/// list they are talking about -- the one-geometry rule, applied to a population.
std::vector<Condition> attention_shown(const Session& s,
                                              const ProjectFrontier& frontier = {});

/// THE COMPACT LINE, or empty when nothing currently deserves attention.
std::string attention_compact(const std::vector<Condition>& shown);

/// WHERE THE CURRENT-CONDITION VIEW OPENS: the overlay column, for the hotkey view's old
/// reason.
// WL-ATTN-09 -- agents/workshop/attention.md; WL-KEY-10 -- agents/workshop/keyboard.md
inline constexpr FineRect attention_bounds(const Screen& sc) noexcept {
    return overlay_column(sc);
}

/// THE VIEW: every currently-true, non-dismissed condition, in the owner's own words.
void paint_attention(surface::SurfaceLayer& layer, const Session& s, const Screen& sc,
                            const ProjectFrontier& frontier);

// ---- WHAT CAN I DO WITH THIS, PRESENTED --------------------------------------------------



/// One entry as its row reads: a group descends and says so, an action is its declared
/// label -- `row_of_id`'s answer, never a second spelling.
std::string context_entry_text(const ContextEntry& entry);

/// THE WIDEST THE POPUP MAY GROW, in prose columns -- the stack panel's own width, the
/// established panel measure of this screen. Content chooses the extent BELOW this bound;
/// a heading longer than the room falls to `detail::fit`'s mark, the ordinary answer for
/// prose that outgrows its material.
inline constexpr std::int64_t kContextMaxCols = kStackW;

/// The label column of one level: the widest entry text, so annotations start in one
/// column down the whole menu rather than ragged after each label.
std::int64_t context_label_columns(const std::vector<ContextEntry>& rows);

/// THE GESTURE WORTH TEACHING BESIDE ONE ENTRY, or "".
std::string context_annotation(const Session& s, const ContextEntry& entry);

/// One population row as composed: the entry's text, and -- where one is truthful -- the
/// effective gesture at the level's annotation column, visually subordinate by position.
/// The painter and the extent both spend THIS spelling; a second copy of the composition
/// would be the two-geometries defect.
std::string context_row_text(const Session& s, const ContextEntry& entry,
                                    std::int64_t label_columns);

/// WHERE THE CONTEXTUAL SURFACE OPENS: beside the press that asked, sized by what
/// it has to say.
FineRect context_bounds(const Session& s, const Screen& sc);


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

void paint_context(surface::SurfaceLayer& layer, const Session& s, const Screen& sc);

/// WHERE A PRESS LANDED ON THE OPEN CONTEXTUAL SURFACE -- the painter's inverse, over the
/// same composition (`info_body_at`'s family: it answers WHERE and nothing about what
/// that means; the weave decides what a hit does).
// WL-CTX-08 -- agents/workshop/contextual.md
struct ContextPressAt {
    bool inside = false;
    bool entry = false;
    std::size_t index = 0;
};

ContextPressAt context_press_at(const Session& s, const Screen& sc, std::int64_t space,
                                       std::int64_t x, std::int64_t y, const PointedAt& at);

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
BodyShare share_body_rows(std::size_t budget, std::size_t want_objects,
                                 std::size_t want_properties);

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
bool draft_live(const Session& s);

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
Availability action_availability(std::size_t which, const WorkshopDoc& d,
                                        const Session& s);

/// THE NAME A MAKER READS. Application words, in the application's file: a control does not
/// know the key that also performs its act (`n`, `d`), and a shortcut hint is not written
/// here -- the two help lines at the bottom of the screen are where this tool says what its
/// keys do, and a second copy beside every control is a second thing to keep true.
inline constexpr const char* action_label(std::size_t which) noexcept {
    return which == kActionCreate ? "Create" : "Delete";
}

/// ONE CONTROL AS PROSE — and the availability is said in CHARACTERS, not in colour.
std::string action_row_text(std::size_t which, bool pressable, std::int64_t columns);

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
std::size_t inspector_focus(const Session& s);

/// WHAT A LIST ASKS THE BODY FOR: one row per member, and never zero.
// WL-INFO-07 -- agents/workshop/info-body.md
inline constexpr std::size_t list_demand(std::size_t members) noexcept {
    return members == 0 ? 1 : members;
}

/// THE BODY, RESOLVED. `total_objects`/`selected_at` and `total_properties`/`focus` are the two
/// populations and the two members that must stay on screen. They are arguments rather than a
/// document and a session so this is pure over the four numbers the composition depends on.
InfoBodyPlace info_body_place(const FineRect& outer, const Screen& sc,
                                     std::size_t total_objects, std::size_t selected_at,
                                     std::size_t total_properties, std::size_t focus);

/// The same resolution for the document and session a painter is holding. One call, so nothing
/// can resolve the body against a population, a selection or a focus the rest of the screen
/// does not have.
InfoBodyPlace info_body_place(const FineRect& panel, const Screen& sc,
                                     const WorkshopDoc& d, const Session& s);

/// The cell-lattice doors to the same two resolutions — one multiply each, so a
/// caller holding a whole-cell rectangle (the side region is one at every extent)
/// asks the identical question the fine door answers.
InfoBodyPlace info_body_place(const ui::Rect& panel, const Screen& sc,
                                     std::size_t total_objects, std::size_t selected_at,
                                     std::size_t total_properties, std::size_t focus);

InfoBodyPlace info_body_place(const ui::Rect& panel, const Screen& sc,
                                     const WorkshopDoc& d, const Session& s);

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
InfoBodyAt info_body_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                               std::int64_t x, std::int64_t y);

// ---- One windowed list's rows, mapped both ways ------------------------------------------

/// WHICH PROSE ROW SHOWS ITEM `index` OF A LIST THAT BEGINS AT `first_row`, or `kNoProseRow`
/// when the window is not showing it.
std::int64_t prose_row_in_window(const ListWindow& w, std::int64_t first_row,
                                        std::size_t index);

/// WHICH ITEM A PROSE ROW SHOWS, or `count` positions past the window's own end for a marker
/// row, a row outside the list's run, or a row nobody has. The inverse of the function above,
/// and its only inverse; callers turn "not an item" into their own sentinel.
bool item_at_prose_row(const ListWindow& w, std::int64_t first_row, std::size_t rows,
                              std::int64_t row, std::size_t& out);

/// WHICH PROSE ROW OF THE BODY SHOWS OBJECT `index`, and which object a prose row shows. The
/// object list begins at the body's first row, so its `first_row` is zero.
std::int64_t prose_row_of_object(const InfoBodyPlace& p, std::size_t index);

std::size_t object_at_prose_row(const InfoBodyPlace& p, std::int64_t row);

/// WHICH PROSE ROW OF THE BODY SHOWS PROPERTY `index`, and which property a prose row shows.
/// The property list begins one row under the `PROPERTIES` heading, which is itself one row
/// under the object list's last row -- so both answers move when the composition does, and
/// they move together because they are the same two calls.
std::int64_t prose_row_of_property(const InfoBodyPlace& p, std::size_t index);

std::size_t property_at_prose_row(const InfoBodyPlace& p, std::int64_t row);

/// WHICH PROSE ROW OF THE BODY CARRIES CONTROL `which`, and which control a prose row carries.
std::int64_t prose_row_of_action(const InfoBodyPlace& p, std::size_t which);

std::size_t action_at_prose_row(const InfoBodyPlace& p, std::int64_t row);

/// WHICH CONTROL A PRESS INSIDE THE BODY NAMES, or `kNoAction`.
std::size_t action_press_at(const InfoBodyPlace& p, std::int64_t column,
                                   std::int64_t row);

/// ONE SEMANTIC OBJECT ROW AS PROSE — the selection mark, the identity, and as much of the
/// authored name as the body has room for.
std::string object_row_full(const ui::Element& e, bool chosen);

std::string object_row_text(const ui::Element& e, bool chosen, std::int64_t columns);

/// ONE SEMANTIC PROPERTY ROW AS PROSE — the mark, the name, and as much of the value as the
/// body has room for.
std::string property_row_prefix(const Row& row, bool here);

/// THE WHOLE OF WHAT A RESTING ROW WOULD SAY WITH UNLIMITED ROOM -- the mark, the
/// name and the value entire. A LIVE DRAFT HAS NO SUCH ROW, deliberately: a draft is
/// windowed by its own component against its own caret, and there is nothing here to reveal
/// that moving the caret does not already show.
std::string property_row_full(const Row& row, bool here);

std::string property_row_text(const Row& row, bool here, std::int64_t value_columns);

/// THE CARET'S COLUMN IN A BODY ROW: the mark and the name, plus the component's own answer.
std::int64_t property_caret_column(const Row& row);

/// THE DRAFT'S VISIBLE SELECTION AS PROSE COLUMNS OF ITS BODY ROW —
/// `property_caret_column`'s shape for a span, and `terminal_selection_columns`' twin.
TerminalSelectionSpan property_selection_columns(const Row& row,
                                                        std::int64_t value_columns);

/// IS THIS PROSE POSITION ON THE BODY ROW SHOWING PROPERTY `index` AT ALL?
bool property_row_hit(const InfoBodyPlace& p, std::size_t index, std::int64_t column,
                             std::int64_t row);

/// WHICH OBJECT A PRESS INSIDE THE BODY NAMES, or `kNoObject`.
std::size_t object_press_at(const InfoBodyPlace& p, std::int64_t column,
                                   std::int64_t row);

/// A PRESSED COLUMN AS A COLUMN OF THE VALUE. Negative to the left of the value, which
/// `TextBox::position_at_column` reads as "the start of what is shown".
inline constexpr std::int64_t property_value_column(std::int64_t row_column) noexcept {
    return row_column - (kPropertyMarkCols + kPropertyLabelCols);
}

/// THE INFO PANEL — the OBJECTS list and the PROPERTIES inspector, in the column they have
/// always occupied.
void paint_info(surface::SurfaceLayer& layer, const WorkshopDoc& d, const Session& s,
                       const FineRect& b, const Screen& sc,
                       std::int64_t chrome = kPaneChrome);

// ---- AN EXTERNAL PANE'S BODY: one header row of Workshop's, and a region ---------------

/// One header row, Workshop's own, so the provenance of what follows is legible.
// WL-FOCUS-11 -- agents/workshop/focus.md; WL-PANE-06 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t kExternalHeaderRows = 1;

/// HOW MANY HEADER ROWS THIS PANE'S PRESENTATION RESERVES RIGHT NOW -- the ONE
/// resolution of the title preference, asked by the painter, the press path and the room
/// grant alike. Three parties spending three private answers to this question is a maker
std::int64_t external_title_rows(const Panels& panels, std::int64_t kind,
                                        bool titles_shown) noexcept;

/// WHAT A PANE SAYS BEFORE ITS PROVIDER HAS SAID ANYTHING.
// WL-PANE-16 -- agents/workshop/panes-and-windows.md
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
ExternalBodyPlace external_body_place(const FineRect& panel, const Screen& sc,
                                             std::int64_t header_rows);

/// WHERE A PRESS LANDED IN AN EXTERNAL PANE'S GRANTED ROOM -- the `PaneRoom`
/// lattice, and nothing a provider was not already handed.
// WL-PRESS-04 -- agents/workshop/press-chain.md
struct ExternalPressAt {
    bool named = false;
    std::int64_t row = 0;    ///< a prose row of the BODY: 0 is the row under the header
    std::int64_t column = 0; ///< ...and a prose column of the same region
};

/// LOCATE A PRESS IN THE ROOM A PANE WAS GRANTED, from the rectangle the painter used.
ExternalPressAt external_press_at(const Panels& panels, const Setup& setup,
                                         const Screen& sc, std::int64_t kind, bool titles,
                                         std::int64_t space, std::int64_t x, std::int64_t y);

/// IT SAYS WHETHER TYPING GOES HERE, which is a repair with a live cost behind it.
// WL-FOCUS-10 -- agents/workshop/focus.md
inline constexpr const char* kTypingHere = "> ";
inline constexpr const char* kTypingElsewhere = "  ";

/// THE HEADER: what this pane is, and WHOSE it is -- both halves validated at admission,
/// neither echoed raw -- and whether typing goes here, said by a mark that costs no columns.
std::string external_header(const RuntimePane& row, bool typing);

/// ONE EXTERNAL PANEL: Workshop's backdrop, Workshop's header, and ONE region carrying
/// whatever that office last validly said inside the room it was granted.
void paint_external(surface::SurfaceLayer& layer, const Panels& panels, std::int64_t kind,
                           const FineRect& b, const Screen& sc, bool titles,
                           std::int64_t chrome = kPaneChrome);

// ---- THE SOURCE EDITOR'S PANE: one document, projected through a viewport ---------------
// WL-EDIT-12 -- agents/workshop/editor.md

inline constexpr std::int64_t kEditorHeaderRows = 1;

/// ONE COLUMN OF EVERY BODY ROW THE TEXT MAY NOT USE -- `kTerminalCaretCols`' rule, for
/// its reason.
// WL-EDIT-08 -- agents/workshop/editor.md
inline constexpr std::int64_t kEditorCaretCols = 1;

/// The editor body's resolved place on this screen: the pane's rectangle less its header,
/// as prose. Absent whenever the pane is closed, off-room, or too small for one row.
ExternalBodyPlace editor_body(const Session& s, const Screen& sc);

/// The columns of the body a LINE may spend -- the body's columns less the caret's one.
inline constexpr std::int64_t editor_text_columns(const ExternalBodyPlace& body) noexcept {
    const std::int64_t text = body.columns - kEditorCaretCols;
    return text > 0 ? text : 0;
}

/// THE HEADER: whether the buffer matches the file, where the caret is, and what is
/// being edited -- in the order the facts must survive `detail::fit`'s TAIL cut.
std::string editor_header(const EditorState& e, bool typing);

/// KEEP THE VIEWPORT TRUE AGAINST THE ROOM AND THE DOCUMENT IT HAS NOW -- the editor's
/// member of the once-per-repaint reconcile family (`refresh_terminal`'s argument, two
void reconcile_editor_view(Session& s);

/// WHERE A PRESS LANDED IN THE EDITOR'S BODY -- `external_press_at`'s shape for the one
/// built-in whose body is a document.
// WL-EDIT-08 -- agents/workshop/editor.md
struct EditorPressAt {
    bool named = false;
    std::int64_t row = 0;    ///< a prose row of the BODY: 0 is the row under the header
    std::int64_t column = 0; ///< a displayed column of the viewport's window
};

EditorPressAt editor_press_at(const Session& s, const Screen& sc, std::int64_t space,
                                     std::int64_t x, std::int64_t y);

/// IS THIS POSITION OVER THE EDITOR'S TEXT BODY -- the wheel's one question. The header
/// row is not the body; the column is not asked, because a wheel aimed at the pane's
/// body is aimed at the document however far right of its last character it sits.
bool over_editor_body(const Session& s, const Screen& sc, std::int64_t space,
                             std::int64_t x, std::int64_t y);

/// THE EDITOR, PAINTED: the frame, the header, and the document through the viewport --
/// one region, so the caret and the selection are the REGION's and each medium answers
/// in cells).
void paint_editor(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                         const Screen& sc, std::int64_t chrome = kPaneChrome);

// ---- THE PROJECT BROWSER, PRESENTED -----------------------------------------------------

inline constexpr std::int64_t kFilesHeaderRows = 1;

/// How many rows the wheel is worth in a cursor-windowed list -- the editor's number, for
/// its reason.
// WL-EDIT-10 -- agents/workshop/editor.md
inline constexpr std::int64_t kListWheelRows = 3;
inline constexpr std::int64_t kFilesWheelRows = kListWheelRows;

/// TURN NOTCHES INTO WHOLE ROWS, CARRYING THE FRACTION.
std::int64_t spend_wheel(double& accum, double dy, std::int64_t rows_per_notch);

/// The browser body's resolved place on this screen: the pane's rectangle less its header.
/// Absent whenever the pane is closed, off-room, or too small for one row.
ExternalBodyPlace files_body(const Session& s, const Screen& sc);

/// THE HEADER: where the maker is, whether this place is one they might have meant, and
/// how far into the listing -- in the order the facts must survive the cut, which is the
std::string files_header_prefix(const FilesPane& pane, const std::string& why,
                                       bool typing);

/// WHERE THE BROWSER IS, AS A LOCATION -- absolute, or the one word for the absence.
std::string files_location(const FilesPane& pane);

/// THE HEADER WITH NOTHING TAKEN OFF: everything the painter is holding for this row.
std::string files_header_full(const FilesPane& pane, const std::string& why,
                                     bool typing);

std::string files_header(const FilesPane& pane, const std::string& why, bool typing,
                                std::int64_t columns);

/// ONE ROW'S TEXT: the name, a directory marked as one, and a name this application cannot
/// carry marked as that. The two marks are deliberately different words, because they are
std::string files_row_text(const FileRow& row);

/// THE WHOLE ROW, CURSOR MARK INCLUDED -- what the browser is holding for one listed
/// name before the body's width has any say in it.
std::string files_row_full(const FileRow& row, bool here);

/// WHERE A PRESS LANDED IN THE BROWSER'S BODY -- `editor_press_at`'s shape, answering a
/// row of the WINDOW rather than a position in a document. The header is subtracted here
/// because the resolution reserved it there.
struct FilesPressAt {
    bool named = false;
    std::int64_t row = 0; ///< a prose row of the BODY: 0 is the row under the header
};

FilesPressAt files_press_at(const Session& s, const Screen& sc, std::int64_t space,
                                   std::int64_t x, std::int64_t y);

/// IS THIS POSITION OVER THE BROWSER'S BODY -- the wheel's one question, `over_editor_body`
/// exactly: the header row is not the body, and the column is not asked.
bool over_files_body(const Session& s, const Screen& sc, std::int64_t space,
                            std::int64_t x, std::int64_t y);

/// WHICH LISTING ROW A BODY ROW SHOWS, for the press inverse -- the SAME window the painter
/// walks, resolved from the same three numbers. It is a function rather than a remembered
bool files_row_of_body_row(const FilesPane& pane, std::int64_t body_rows,
                                  std::int64_t body_row, std::size_t& out);

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

RevealAt files_reveal_at(const Session& s, const Screen& sc, std::int64_t space,
                                std::int64_t x, std::int64_t y);

RevealAt info_reveal_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                               std::int64_t x, std::int64_t y);

/// THE ONE ANSWER A MOTION SPENDS: which revealable row -- of any surface -- this position is
/// on. The occupancy walk is asked FIRST and once, so a surface can only answer for cells it
/// actually owns on this screen.
RevealAt reveal_at(const WorkshopDoc& d, const Session& s, std::int64_t space,
                          std::int64_t x, std::int64_t y);

/// WHAT THE SESSION SHOULD HOLD FOR THIS POSITION -- the record `Revealed` keeps, or an empty
/// one where there is nothing to read past. Pure: the weave compares it with what it already
/// had and repaints only on a difference, so a hand moving along one row costs one repaint per
/// column it actually changes and a hand moving over furniture costs none.
Revealed reveal_for(const WorkshopDoc& d, const Session& s, std::int64_t space,
                           std::int64_t x, std::int64_t y);

/// THE PROJECT BROWSER, PAINTED: the frame, the header, and one directory's rows through
/// the shared list window. Every branch here says something -- an absent project, a
/// directory that would not open, and an empty directory are three different facts and a
/// pane that showed blankness for all three would be hiding two of them.
void paint_files(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                        const Screen& sc, const Keymap& k,
                        std::int64_t chrome = kPaneChrome);

/// THE AFFORDANCE RINGS ARE THE ARRANGEMENT STATE MADE VISIBLE.
void paint_pane_affordances(surface::SurfaceLayer& layer, const Session& s,
                                   const Screen& sc);

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
std::string setup_name_hint(const Keymap& k);

/// The two gestures the setup line advertises, on the line the thing they act on is on --
/// the `[+ panel]  p` precedent.
std::string setup_hints(const Keymap& k);

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
ExternalBodyPlace layouts_body(const Session& s, const Screen& sc);

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
std::int64_t setup_name_columns(const Session& s, const Screen& sc);

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
std::string setup_link_text(const SetupState& setup, std::int64_t path_columns);

/// WHAT THE ROW SAYS AFTER THE ASSOCIATION: the unresolved count, then the two gestures.
std::string setup_rest_text(const SetupState& setup, const Panels& panels,
                                   const Keymap& keymap);

/// The workspace's extent, as the band states it -- the one fact the retired shared top
/// row carried that nothing else says. It is a STATUS fact (what a share of the
/// workspace currently resolves against), so it lives beside the setup identity in the
/// band's own voice rather than as a heading of its own.
std::string workspace_text(const Session& s);

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
std::string layouts_omitted_text(std::size_t how_many, bool ahead);

/// The cell a tab opens with and the cell it closes with -- the live layout's pair and every
/// other layout's, ONE CELL EACH SIDE either way.
// WL-TAB-06 -- agents/workshop/tab-run.md
inline constexpr char kLayoutLiveOpen = '>';
inline constexpr char kLayoutLiveClose = '<';
inline constexpr char kLayoutTabPad = ' ';

/// What ONE layout contributes to the run: its two marker cells and the AUTHORED name between
/// them, bare. No quoting, no escaping and no substitution -- the bytes a maker typed.
std::string layout_tab_text(const SetupState& setup, std::size_t at);

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

std::int64_t layout_tab_columns(std::int64_t row_columns) noexcept;

/// THE POINTER'S SPELLING OF `layout.new`: one cell of ink at the end of the run.
// WL-TAB-04 -- agents/workshop/tab-run.md
inline constexpr char kLayoutCreate = '+';
inline constexpr std::int64_t kLayoutCreateCols = 2; // one pad cell and the mark

/// THE VISIBLE TAB WINDOW, DERIVED AT EVERY PAINT AND STORED NOWHERE.
LayoutTabRun layout_tab_run(const SetupState& setup, std::int64_t columns);

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

BandStatus band_status(const Session& s, const ExternalBodyPlace& place);

/// THE SAME COMPOSITION, RESOLVED FROM THE SESSION -- for every consumer that holds a
/// screen rather than the interior the painter was handed. Two spellings of one answer,
/// because the painter already has the rectangle it is drawing into and re-deriving it
/// there would be the second resolution; everyone else asks for it here.
BandStatus band_status(const Session& s, const Screen& sc);

/// WHICH ROW OF THE LAYOUTS PANE THE TAB RUN IS PAINTED ON, or `kNoBandRow` when it is not
/// painted at all -- the composition's own answer, so a press can never be resolved against
/// a run this budget did not write.
// WL-TAB-05 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kNoBandRow = -1;

std::int64_t band_tab_row(const Session& s, const Screen& sc);

/// WHICH LAYOUT A PRESS LANDED ON, or none -- the exact inverse of what was painted.
// WL-TAB-09 -- agents/workshop/tab-run.md
struct LayoutTabPress {
    bool hit = false;      ///< this press was answered by the run
    std::size_t at = 0;    ///< the layout it landed on, when `create` is false
    bool create = false;   /// < it landed on the `+` affordance instead
};

LayoutTabPress band_tab_at(const Session& s, const Screen& sc, std::int64_t space,
                                  std::int64_t x, std::int64_t y);

// ---- THE LAYOUTS PANE AND THE BOTTOM BAND, EACH COMPOSED AGAINST ITS BUDGET ---------------

/// THE LAYOUTS PANE, PAINTED: the layout selector and the standing identity beside
/// it, the workspace fact under them where the medium fits a second row, and the setup-name
/// editor's caret and selection while a maker is typing a name.
void paint_layouts(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                          const Screen& sc, std::int64_t chrome = kPaneChrome);

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
RegionPresentation present_region(const TextRegion& r, const FineRect& interior,
                                         const Screen& sc);

/// A published region over a fine rectangle, empty and ready for its rows -- the fine
/// bounds decomposed onto the wire's cells-plus-remainder spelling.
surface::SurfaceTextRegion region_over(const FineRect& r);

/// The region a reference and an id name in the OPEN definition, or nothing: nothing when
/// no definition is open, when the reference is not the open definition's, or when the id
/// is not one of its regions. Every reader of a region goes through here, so a subject
/// whose definition closed underneath it reads `--` rather than a stale value.
const TextRegion* maker_region(const Session& s, const PaneRef& ref, std::int64_t id);

/// THE MAKER-MADE PANE'S INTERIOR RIGHT NOW, or an empty rectangle: the ordinary pane path's
/// answer for its handle, less the chrome. One call, so the painter, the mark and the rows
/// cannot resolve it three ways.
FineRect maker_pane_interior(const Session& s, const Screen& sc);

/// ONE AUTHORED AXIS OF A REGION AS A MAKER READS IT -- the amount in the face's own unit
/// (`geometry_amount_text`, the pane rows' own grammar), marked where this face cannot say
/// the authored number exactly.
std::string region_axis_text(const Session& s, const PaneRef& ref, std::int64_t id,
                                    std::size_t axis);

/// WRITE ONE AUTHORED AXIS OF A REGION FROM WHAT A MAKER TYPED -- a whole number in the
/// face's own unit, through the definition's own door (`author_region_axis`), which judges
/// the fine value in its own words. A region has no `default` mode, so `-` is refused in
/// words rather than read as a reset that does not exist.
Written write_region_axis(Session& s, const PaneRef& ref, std::int64_t id,
                                 std::size_t axis, const std::string& text);

/// WRITE WHAT A TEXT REGION SAYS, through the definition's own door.
Written write_region_text(Session& s, const PaneRef& ref, std::int64_t id,
                                 std::string text);

/// THE REGION AS THIS SCREEN RESOLVED IT, relative to the pane's interior and in the face's
/// unit -- so it reads beside the authored X/Y/Width/Height and differs from them exactly
/// where the interior clipped it. `-` when the pane is not presented.
std::string region_resolved_text(const Session& s, const PaneRef& ref, std::int64_t id);

/// WHAT THE FACE MADE OF THE REGION: rows and columns of type, the cell projection, or no
/// room -- a readout of the medium's answer, never a claim about the definition.
std::string region_shown_text(const Session& s, const PaneRef& ref, std::int64_t id);

/// THE ONLY HONEST INTERIOR FOR A PANE THAT IS NOT MADE OF DATA: a read-only capture of the
/// resolved body -- where it is, how much prose the face fits in it, in which presentation
/// -- and the plain statement that no authored interior exists. A built-in's interior is
/// its painter and a provider's is its own; neither is decomposed, inferred or promised.
std::string interior_capture_text(const Session& s, const PaneRef& ref);

/// THE MAKER-MADE PANE, PAINTED: the frame, one region owning the whole interior (so the
/// material beneath the pane is cleared and the ring shows, `paint_panel_frame`'s own
/// arithmetic), then one `kGroundOwn` region per authored region.
void paint_maker_pane(surface::SurfaceLayer& layer, const Session& s, const FineRect& b,
                             const Screen& sc, std::int64_t chrome = kPaneChrome);

/// THE ROLE THE PANE CREATOR'S REGION MARK IS DRAWN IN: the one thing being pointed at, the
/// word the document's selection ring and the selected pane's chrome already speak.
inline constexpr std::int64_t kRegionMark = surface::role::kAccent;

/// WHICH REGION THE PANE CREATOR IS WORKING ON RIGHT NOW, or nothing -- the open
/// definition's first region, while the Pane Manager is on this desk and has the maker's
/// pane as its subject. Derived at every ask, held nowhere: close the manager, choose
/// another subject, or discard the pane, and the answer is nothing with nothing to clear.
const TextRegion* creator_subject_region(const Session& s);

/// THE REGION MARK: the exact rectangle the region resolved to, filled in the mark's role,
/// with the region's own text written OVER it.
void paint_creator_region_mark(surface::SurfaceLayer& layer, const Session& s,
                                      const Screen& sc);

/// THE PANE CREATOR'S NAME PROMPT, as the Pane Manager's heading spells it while a name is
/// being typed, and the columns the typed line may spend beside it (the prompt, then the
/// caret's own column reserved, `kTerminalCaretCols`' rule).
inline constexpr const char* kPaneNamePrompt = "new pane> ";
std::int64_t pane_name_columns(std::int64_t heading_columns);

// ---- THE PANE EDITOR: a Workshop pane as a SUBJECT, inspected and edited -----------------

/// Prose rows the `PANES` heading keeps -- `kInfoHeadingRows`' twin, one pane over.
inline constexpr std::int64_t kPaneEditorHeadingRows = 1;

/// THE INVENTORY ROW THE SUBJECT NAMES RIGHT NOW, or nothing -- a fresh view over the SAME
/// population the picker walks (`inventory_rows`: the catalog, the admitted runtime panes,
/// and every reference the active setup names). Nothing is cached: a subject is checked
/// against the world at the moment somebody asks.
std::optional<CatalogRow> pane_editor_subject_row(const Session& s);

/// THE WINDOW A TYPED EDIT MEASURES THE OTHER AXIS FROM: authored where authored, resolved
/// where reactive -- `managed_window_base`'s spelling (weave.hpp), quarried out so the
FineRect pane_window_base(const Session& s, const PaneRef& ref);

/// MAY THIS PANE'S GEOMETRY BE TYPED RIGHT NOW, and if not, why not -- the arrangement's
/// admission (`arrange_geometry_ready`, weave.hpp) less the one refusal a typed value does
/// not need.
Written pane_geometry_typeable(const Session& s, const PaneRef& ref);

/// ONE AUTHORED AXIS AS A MAKER READS IT: the amount in the face's own unit, `-` for the
/// developer's answer, and the pixel spelling for the unit no medium here projects --
/// `pane_window_text`'s per-axis grammar, one axis at a time.
std::string pane_axis_text(const Session& s, const PaneRef& ref, std::size_t axis);

/// WRITE ONE AUTHORED AXIS FROM WHAT A MAKER TYPED -- through the gesture door, one axis
/// proposed and the other left exactly as it stands (`author_pane_window`), or
/// through that axis's reset door for `-`.
Written write_pane_axis(Session& s, const PaneRef& ref, std::size_t axis,
                               const std::string& text);

/// THE SUBJECT'S ROWS: its identity, then AUTHORED, then RESOLVED. Every closure reads the
std::vector<Row> pane_editor_rows(Session& s);

/// THE ROW THAT MUST STAY ON SCREEN: the editing one, else the cursor's -- `inspector_focus`
/// for this inspector, and for its reason.
std::size_t pane_editor_focus(const Session& s);

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

PaneEditorBodyPlace pane_editor_body_place(const FineRect& outer, const Screen& sc,
                                                  std::size_t total_panes,
                                                  std::size_t pane_cursor,
                                                  std::size_t total_fields,
                                                  std::size_t field_focus);

/// The same resolution for the session a painter is holding -- one call, so no press can
/// resolve the body against a population or a focus the paint did not.
PaneEditorBodyPlace pane_editor_body(const Session& s, const Screen& sc,
                                            const FineRect& outer);

std::int64_t prose_row_of_editor_pane(const PaneEditorBodyPlace& p, std::size_t index);

std::size_t editor_pane_at_prose_row(const PaneEditorBodyPlace& p, std::int64_t row);

std::int64_t prose_row_of_field(const PaneEditorBodyPlace& p, std::size_t index);

std::size_t field_at_prose_row(const PaneEditorBodyPlace& p, std::int64_t row);

/// WHERE A POINTER FACT LANDED IN THE PANE EDITOR'S BODY -- `InfoBodyAt`'s shape: the pane
/// open, the body resolved through the same `bounds_of` the painter used, the position
/// located by the same `prose_at`, the heading subtracted here because it was reserved
/// there.
struct PaneEditorAt {
    bool present = false;
    PaneEditorBodyPlace body{};
    ProseAt at{};
};

PaneEditorAt pane_editor_at(const Session& s, std::int64_t space, std::int64_t x,
                                   std::int64_t y);

/// THE PANE EDITOR, PAINTED: the inventory with the subject marked, then the subject's rows
/// with a live draft's caret and selection on them. One region, in the active medium's own
/// type; the picker's row spelling for a pane, the property row's spelling for a fact.
void paint_pane_editor(surface::SurfaceLayer& layer, const Session& s,
                              const FineRect& b, const Screen& sc,
                              std::int64_t chrome = kPaneChrome);

// ⚠ `paint_panels` STANDS HERE AND NOT ABOVE, and the reason is the conversion itself
//. This file defines everything before it is used -- there is not one forward
// declaration in it -- and the walk that reaches every pane's painter now reaches
// `paint_layouts`, whose composition is the status row's and belongs beside the status
// row's other halves. So the walk moved down to meet its last painter rather than the
// composition moving up away from what it composes.

/// EVERY PRESENTED PANE, BACK TO FRONT — ONE COMPLETE LAYER EACH.
void paint_panels(surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                         const Screen& sc, const ProjectFrontier& frontier = {});

/// THE BOTTOM BAND AS ONE PUBLISHED REGION: what the tool just said, and what the keys mean
/// right now.
surface::SurfaceTextRegion band_region(const Session& s, const Screen& sc);

/// The whole screen as one published canvas — an ORDERED LIST OF PLANES.
surface::SurfaceCanvas paint(const WorkshopDoc& d, const Session& s,
                                    const ProjectFrontier& frontier = {});

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_HPP
