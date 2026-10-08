// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SCREEN_HPP
#define ZENGINE_WORKSHOP_SCREEN_HPP

// The Workshop screen: the session facts, the weaver's gestures over them, and the one function
// that turns them into a published canvas.
// Workshop law: agents/workshop/geometry.md (+24 registers; agents/workshop.md routes)

#include "attention.hpp" // what is true right now, held and dismissed
#include "attention_seam_vocabulary.hpp" // ...and how it crosses to the pane that shows it
#include "inspection_seam_vocabulary.hpp" // ...and how a pane as an inspector's subject does
#include "terminal_seam_vocabulary.hpp"   // ...and how the terminal participant's record does
#include "desktop_seam_vocabulary.hpp"    // ...and the effective keymap a presenter shows
#include "complete.hpp"
#include "context.hpp" // what can be done with a pointed subject
#include "keymap.hpp"
#include "panes.hpp"
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

#include <zen/terminal/session.hpp> // the participant this host holds, for the door

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <string>
#include <vector>

namespace zengine::workshop {

// ---- The one screen's layout, in canvas pixels -----------------------------------------

/// The smallest surface this screen is laid out on, in canvas pixels -- 78 by 22 cells -- and,
/// deliberately, the extent it uses when nothing tells it otherwise.
// WL-GEO-02 -- agents/workshop/geometry.md
inline constexpr std::int64_t kScreenMinW = 78 * surface::kCanvasCellPx;
inline constexpr std::int64_t kScreenMinH = 22 * surface::kCanvasCellPx;

/// The largest surface this screen will lay out, in canvas pixels: 640 by 400 cells.
// WL-SESSION-07 -- agents/workshop/session-restore.md
inline constexpr std::int64_t kScreenMaxW = 640 * surface::kCanvasCellPx;
inline constexpr std::int64_t kScreenMaxH = 400 * surface::kCanvasCellPx;

/// THE ROWS OF TEXT RESERVED AT THE TOP OF THE SCREEN, and they are the first thing a weaver
/// reads. The band is as tall as these rows of the active medium's type need (`screen_of`).
// WL-GEO-03 -- agents/workshop/geometry.md; WL-FRONT-03 -- agents/workshop/planes.md
inline constexpr std::int64_t kTopRows = 2;

/// The right column (`placement::kSideRegion`): a place at the screen's right edge, fixed width
/// in cells, reserving nothing -- a pane standing here covers room, as a stacked one does.
// WL-GEO-03 -- agents/workshop/geometry.md
inline constexpr std::int64_t kSideCols = 28;

/// The rows of text in the band under the workspace, fitted the same way.
// WL-GEO-03 -- agents/workshop/geometry.md
// WL-FRONT-03 -- agents/workshop/planes.md
// WL-RGN-03 -- agents/workshop/regions.md
inline constexpr std::int64_t kBottomRows = 4;

static_assert(kTopRows + kBottomRows == 6,
              "the reserved chrome is six rows of text and may not grow: the workspace's "
              "extent is what a share resolves against");

// ---- THE OVERLAY STACK (`placement::kOverlayStack`), in cells -------------------------------
// WL-PANE-04 -- agents/workshop/panes-and-windows.md
/// THE MINIMUM WIDTH, and the base the surplus is measured from: wide enough for a build
/// recipe's tail on the 78x22 composition, where it is also the whole of the workspace.
inline constexpr std::int64_t kStackW = 48;
inline constexpr std::int64_t kStackRows = 9; ///< fallback height for providers without a preferred body size
inline constexpr std::int64_t kStackGap = 1;  ///< a blank row between stacked panes


/// THE SCREEN'S FURNITURE, DERIVED IN ONE PLACE, in canvas pixels.
// WL-GEO-05 -- agents/workshop/geometry.md
struct Screen {
    std::int64_t w = kScreenMinW;  ///< the canvas extent this screen paints
    std::int64_t h = kScreenMinH;
    std::int64_t side_x = 0;      ///< the right column's left edge: a place, not a reservation
    std::int64_t room_y = 0;      ///< the workspace's top: the top band's floor
    std::int64_t room_w = 0;       ///< the widest the workspace may be on this screen...
    std::int64_t room_h = 0;       ///< ...and the tallest
    std::int64_t notice_y = 0;     ///< the bottom band's top, where the tool speaks
    /// THE METRIC THIS SCREEN WAS RESOLVED WITH, carried rather than looked up.
    // WL-GEO-08 -- agents/workshop/geometry.md
    std::int64_t text_advance_px = 0;
    std::int64_t text_line_px = 0;
    /// ...AND HOW BIG ONE CANVAS CELL OF THAT MEDIUM IS, in its own device pixels.
    // WL-GEO-08 -- agents/workshop/geometry.md
    std::int64_t cell_px = 0;
};

/// How tall `rows` rows of text stand in a band, inside `chrome` on each side: the face's lines
/// and inset where the medium sets type, whole cells where text is a cell.
inline constexpr std::int64_t band_px_for(std::int64_t rows, std::int64_t text_advance_px,
                                          std::int64_t text_line_px,
                                          std::int64_t chrome) noexcept {
    return surface::add_cells(surface::region_px_for(1, rows, text_advance_px, text_line_px).h,
                              2 * chrome);
}

/// The furniture for a surface of this extent -- TOTAL over every std::int64_t, because the
/// extent it is given came off the bus.
// WL-GEO-02, WL-GEO-03, WL-GEO-04, WL-GEO-05 -- agents/workshop/geometry.md
inline constexpr Screen screen_of(std::int64_t want_w, std::int64_t want_h,
                                  std::int64_t text_advance_px = 0,
                                  std::int64_t text_line_px = 0,
                                  std::int64_t cell_px = 0) noexcept {
    Screen s;
    // The medium's device unit is taken as reported: non-positive already means "my device unit
    // is the cell".
    s.cell_px = cell_px > 0 ? cell_px : 0;
    s.w = want_w < kScreenMinW ? kScreenMinW : (want_w > kScreenMaxW ? kScreenMaxW : want_w);
    s.h = want_h < kScreenMinH ? kScreenMinH : (want_h > kScreenMaxH ? kScreenMaxH : want_h);
    s.side_x = s.w - surface::px_of_cells(kSideCols);
    // The room is the surface: nothing reserves the right column, so the room runs under it whole
    // and what a share means never depends on which panes are open
    // (docs/history/decisions/the-reserved-column.md).
    s.room_w = s.w;
    // The fit is resolved here because the screen carries the metric, which arrives on the bus.
    const surface::RegionFit fit =
        surface::fit_region(0, 0, s.w, s.h, text_advance_px, text_line_px);
    // The metric as the fit resolved it: a non-positive advance or line already means "text is a
    // cell", so a screen never carries half a metric.
    s.text_advance_px = fit.advance_px;
    s.text_line_px = fit.line_px;
    // The bands are as tall as the text they hold: the Layouts pane's rows inside one device
    // unit of chrome at the top, and the tool's rows at the bottom. Where text is a cell they are
    // whole cells, the Layouts pane drawing no boundary in its two. A metric whose bands would
    // leave no room of a cell is not spent on them: the cell bands stand instead.
    const bool type = s.text_advance_px > 0 && s.text_line_px > 0;
    std::int64_t top = surface::px_of_cells(kTopRows);
    std::int64_t band = surface::px_of_cells(kBottomRows);
    if (type) {
        const std::int64_t typed_top = band_px_for(kTopRows, s.text_advance_px, s.text_line_px,
                                                   surface::px_of_one_device(s.cell_px));
        const std::int64_t typed_band =
            band_px_for(kBottomRows, s.text_advance_px, s.text_line_px, 0);
        if (surface::add_cells(typed_top, typed_band) <= s.h - surface::kCanvasCellPx) {
            top = typed_top;
            band = typed_band;
        }
    }
    s.room_y = top;
    // The height loses its bands: the top and bottom rows are chrome this screen paints,
    // coverable by no pane.
    s.notice_y = s.h - band;
    s.room_h = s.notice_y - s.room_y;
    return s;
}

/// The minimum screen with no metric, the one the terminal projection keeps; the assertions pin
/// every number the 78x22 composition was written with, in pixels.
inline constexpr Screen kMinScreen = screen_of(kScreenMinW, kScreenMinH);

static_assert(kMinScreen.side_x == 50 * surface::kCanvasCellPx,
              "the right column has not moved on the minimum screen");
static_assert(kMinScreen.room_w == kMinScreen.w,
              "the room IS the surface: nothing comes off its width. The 48 this read before "
              "was 78 less the right column's 28 and the two-cell gap beside it, and those "
              "thirty columns are the room's");
static_assert(kMinScreen.room_h == 16 * surface::kCanvasCellPx,
              "the workspace's documented default height");
static_assert(kMinScreen.notice_y == 18 * surface::kCanvasCellPx, "the bottom band");
// THE MINIMUM COMPOSITION'S THREE REGIONS, WRITTEN OUT.
static_assert(kMinScreen.room_y == 2 * surface::kCanvasCellPx,
              "where text is a cell, the top band owns rows 0 and 1");
static_assert(kMinScreen.room_y + kMinScreen.room_h == kMinScreen.notice_y,
              "the workspace's floor IS the bottom band's top -- no pixel between them, and "
              "none reserved twice");
static_assert(screen_of(kScreenMinW, kScreenMinH, 8, 18, surface::kCanvasCellPx).room_y ==
                  2 * 18 + 2 * surface::kTextInsetPx + 2,
              "where a face sets type, the top band is the Layouts pane's two rows of it, "
              "inside a boundary of one device pixel");
static_assert(screen_of(kScreenMinW, kScreenMinH, 8, 18, surface::kCanvasCellPx).notice_y ==
                  kScreenMinH - (4 * 18 + 2 * surface::kTextInsetPx),
              "...and the bottom band is the tool's four");


// ---- THE PIXEL LATTICE, AS A RECTANGLE --------------------------------------------------
// WL-GEO-06 -- agents/workshop/geometry.md

/// A rectangle on the canvas, in canvas pixels.
struct PixelRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const PixelRect&, const PixelRect&) = default;

    constexpr bool empty() const noexcept { return w <= 0 || h <= 0; }

    /// DOES A POINTER AT THIS PIXEL, REPORTED AT THIS GRAIN, LAND ON THIS RECTANGLE.
    // WL-GEO-07 -- agents/workshop/geometry.md
    constexpr bool contains_at(std::int64_t sx, std::int64_t sy,
                               std::int64_t grain) const noexcept {
        return surface::px_span_contains(x, w, sx, grain) &&
               surface::px_span_contains(y, h, sy, grain);
    }
};

/// A cell rectangle in canvas pixels — exact, saturating, the one door a cell-counted layout
/// walks through on its way to being arrangement truth.
inline constexpr PixelRect pixels_of_cells(const ui::Rect& r) noexcept {
    return PixelRect{surface::px_of_cells(r.x), surface::px_of_cells(r.y),
                     surface::px_of_cells(r.w), surface::px_of_cells(r.h)};
}

/// THE CELLS A PIXEL RECTANGLE COVERS — the cell-grain quantization law as a rectangle:
/// [floor(left), floor(right)) per axis.
// WL-GEO-06 -- agents/workshop/geometry.md
inline constexpr ui::Rect cells_covered(const PixelRect& f) noexcept {
    const std::int64_t x0 = surface::cell_of_pixel(f.x);
    const std::int64_t y0 = surface::cell_of_pixel(f.y);
    if (f.w <= 0 || f.h <= 0) {
        return ui::Rect{x0, y0, 0, 0};
    }
    return ui::Rect{x0, y0, surface::cell_of_pixel(surface::add_cells(f.x, f.w)) - x0,
                    surface::cell_of_pixel(surface::add_cells(f.y, f.h)) - y0};
}

/// A pixel rectangle as a published `SurfaceRect`; a negative extent publishes as none.
inline constexpr surface::SurfaceRect wire_rect_of(const PixelRect& f,
                                                   std::int64_t role) noexcept {
    return surface::SurfaceRect{f.x, f.y, f.w > 0 ? f.w : 0, f.h > 0 ? f.h : 0, role};
}

/// AN AUTHORED PLACE ON THE CANVAS: a place is measured from the room's top-left, which stands
/// at (0, `room_y`) -- directly under the top band, whatever height that band was fitted to.
// WL-PANE-11 -- agents/workshop/panes-and-windows.md
inline constexpr PixelRect canvas_of_room(PixelRect r, const Screen& sc) noexcept {
    r.y = surface::add_cells(r.y, sc.room_y);
    return r;
}

/// ...AND A RECTANGLE ON THE CANVAS AS A PLACE IN THE ROOM, for what a hand or a readout measures.
// WL-PANE-11 -- agents/workshop/panes-and-windows.md
inline constexpr PixelRect room_of_canvas(PixelRect r, const Screen& sc) noexcept {
    r.y = surface::add_cells(r.y, -sc.room_y);
    return r;
}

/// The part of a pixel rectangle this canvas has.
inline constexpr PixelRect clip_to_canvas_px(const PixelRect& r, const Screen& sc) noexcept {
    const std::int64_t x0 = r.x < 0 ? 0 : r.x;
    const std::int64_t y0 = r.y < 0 ? 0 : r.y;
    const std::int64_t x1 =
        surface::add_cells(r.x, r.w) < sc.w ? surface::add_cells(r.x, r.w) : sc.w;
    const std::int64_t y1 =
        surface::add_cells(r.y, r.h) < sc.h ? surface::add_cells(r.y, r.h) : sc.h;
    if (x1 <= x0 || y1 <= y0) {
        return PixelRect{};
    }
    return PixelRect{x0, y0, x1 - x0, y1 - y0};
}


// ---- PLACEMENT RESOLVED: a place, on a screen, is a rectangle ---------------------------
// WL-GEO-03 -- agents/workshop/geometry.md; WL-PANE-04 -- agents/workshop/panes-and-windows.md

/// THE BOUNDS A PLACE RESOLVES TO on this screen, in canvas pixels.
// WL-GEO-03 -- agents/workshop/geometry.md; WL-PANE-04 -- agents/workshop/panes-and-windows.md
inline constexpr PixelRect placement_bounds(std::int64_t where, std::size_t slot,
                                            const Screen& sc) noexcept {
    if (where == placement::kTopBand) {
        // The top band, whole; the slot is nothing to it (one pane fits, and panes.hpp asserts
        // it).
        return PixelRect{0, 0, sc.w, sc.room_y};
    }
    if (where == placement::kSideRegion) {
        // From the workspace's top to its floor, against the right edge: over the material.
        return PixelRect{sc.side_x, sc.room_y, surface::px_of_cells(kSideCols), sc.room_h};
    }
    const std::int64_t n = slot >= static_cast<std::size_t>(kScreenMaxH)
                               ? kScreenMaxH
                               : static_cast<std::int64_t>(slot);
    // The width is the minimum plus half the room's surplus over it, floored so the odd pixel
    // stays the weaver's; `x + w < room_w` at every extent.
    const std::int64_t stack_w = surface::px_of_cells(kStackW);
    return PixelRect{0, sc.room_y + n * surface::px_of_cells(kStackRows + kStackGap),
                     stack_w + (sc.room_w - stack_w) / 2, surface::px_of_cells(kStackRows)};
}

// ---- THE CHROME A PANE WEARS, AND THE INTERIOR IT LEAVES --------------------------------
// WL-CHROME-01, WL-CHROME-02, WL-CHROME-06 -- agents/workshop/chrome.md

/// The coarsest honest boundary, one every medium can show: one canvas cell, and the ceiling
/// `chrome_outer_of` reserves for a surface sized by its own content.
inline constexpr std::int64_t kChromeCells = 1;
inline constexpr std::int64_t kChromePx = surface::px_of_cells(kChromeCells);

/// ONE UNIT OF THE ACTIVE FACE, in canvas pixels -- what this screen's chrome costs before the
/// interior's own presentation gets a say (`pane_inside` below is where it gets one).
inline constexpr std::int64_t chrome_grain(const Screen& sc) noexcept {
    return surface::px_of_one_device(sc.cell_px);
}

/// The rectangle inside a pane's chrome: `outer` less `chrome_px` on every side, empty
/// when the outer rectangle cannot hold both edges.
// WL-CHROME-01, WL-CHROME-05 -- agents/workshop/chrome.md
inline constexpr PixelRect pane_interior(const PixelRect& outer,
                                        std::int64_t chrome_px) noexcept {
    const std::int64_t w = outer.w - 2 * chrome_px;
    const std::int64_t h = outer.h - 2 * chrome_px;
    if (w <= 0 || h <= 0) {
        return PixelRect{};
    }
    return PixelRect{surface::add_cells(outer.x, chrome_px),
                    surface::add_cells(outer.y, chrome_px), w, h};
}

/// THE INTERIOR OF A PANE AND THE PRESENTATION IT GETS, RESOLVED TOGETHER.
// WL-CHROME-05 -- agents/workshop/chrome.md
struct PaneInside {
    PixelRect rect{};              ///< the interior: `outer` less the chrome on every side
    surface::RegionFit fit{};     ///< ...resolved with the ACTIVE medium's own text metric
    std::int64_t chrome_px = 0;    ///< what one side of that boundary cost
};

namespace detail {

/// One candidate: inset by this much, and fit what is left. Total over every rectangle.
PaneInside pane_inside_at(const PixelRect& outer, const Screen& sc,
                                 std::int64_t chrome_px);

} // namespace detail

/// The one call: a pane's outer rectangle in, its interior and that interior's resolution out,
/// across the finest boundary the face in front of the weaver will present.
PaneInside pane_inside(const PixelRect& outer, const Screen& sc);

/// The rectangle inside a pane's chrome, for a consumer that wants only the geometry.
PixelRect pane_interior(const PixelRect& outer, const Screen& sc);

/// The same subtraction read backwards: the outer extent a surface sized by its own content
/// needs to hold that content inside its chrome.
// WL-CHROME-06 -- agents/workshop/chrome.md; WL-CTX-03 -- agents/workshop/contextual.md
inline constexpr PixelRect chrome_outer_of(std::int64_t x, std::int64_t y, std::int64_t w,
                                           std::int64_t h) noexcept {
    return PixelRect{x, y, w + 2 * kChromePx, h + 2 * kChromePx};
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
    /// What the authored intent asks for before the canvas gets a say, in pixels; it may run
    /// past the screen's edge, which is legal intent and is not rewritten.
    PixelRect resolved{};
    /// ...AND THE PART OF IT THIS CANVAS ACTUALLY HAS. Empty when nothing of the pane is on
    /// screen, which is what `off-room` means and how it is told from `waiting`.
    PixelRect visible{};
};

/// THE DEVELOPER'S ANSWER, THEN THE WEAVER'S, PER AXIS -- and then the canvas.
PaneProjection project_pane(std::int64_t where, std::size_t slot,
                            const SetupPane* authored, const Screen& sc,
                            const RuntimePane* preference = nullptr, std::int64_t stack_y = -1);

/// What the one narrow path answers with: whether this kind is open, where its kind is
/// placed, and the rectangle it occupies if it is open at all.
struct PaneBounds {
    bool open = false;
    /// THE KIND'S DECLARED PLACE, open or not — a fact about the catalog rather than about
    /// this session, so it is answerable for a pane nobody has opened.
    std::int64_t placed_in = placement::kOverlayStack;
    /// EMPTY WHEN THE PANE IS NOT OPEN, deliberately.
    // WL-ARR-04 -- agents/workshop/arrangement.md
    // WL-PANE-09 -- agents/workshop/panes-and-windows.md
    PixelRect rect{};
    /// ...and what the authored intent ASKED for, unclipped. Read by the state classifier,
    /// which has to tell "partly cut off" from "not on this screen at all".
    PixelRect resolved{};
};

/// WHERE AN OPEN PANE IS RIGHT NOW — the one narrow path, and the only thing that knows how
/// a slot is earned.
PaneBounds bounds_of(const Panes& panes, const Setup& setup, std::int64_t kind,
                             const Screen& sc);

// The two places fit the SMALLEST screen this composition is honest on, which is where they
// are tightest.
// WL-GEO-04 -- agents/workshop/geometry.md
inline constexpr PixelRect kMinSide = placement_bounds(placement::kSideRegion, 0, kMinScreen);
inline constexpr PixelRect kMinStack =
    placement_bounds(placement::kOverlayStack, 0, kMinScreen);

// The two places may meet: a pane covering a pane is what an overlay is for. What the
// half-share promises is that a slot never covers the whole room.
static_assert(kMinStack.x + kMinStack.w < kMinScreen.room_w,
              "a stacked pane leaves reachable workspace to its right at every extent -- "
              "which at the smallest screen it did NOT before: 48 of 48 left nothing, and 63 "
              "of 78 leaves fifteen");
// AND THE HALF-SHARE IS THE SAME ARITHMETIC IT WAS, over a bigger room.
static_assert(kMinStack.w == 63 * surface::kCanvasCellPx,
              "48 + (78 - 48)/2 cells -- the half-share on the minimum screen, spelled out");
static_assert(placement_bounds(placement::kOverlayStack, 0, screen_of(kScreenMinW + 1, kScreenMinH))
                      .w == 63 * surface::kCanvasCellPx,
              "an odd surplus is FLOORED: 576 + (937 - 576)/2 is 756, not 757 -- the odd pixel "
              "stays the weaver's");
static_assert(placement_bounds(placement::kOverlayStack, 0, screen_of(2400, 720)).w ==
                  576 + (2400 - 576) / 2,
              "the half-share, spelled out in pixels");
static_assert(placement_bounds(placement::kOverlayStack, 3, screen_of(2400, 720)).w ==
                  placement_bounds(placement::kOverlayStack, 0, screen_of(2400, 720)).w,
              "the width is a fact about the SCREEN, not about which slot a pane sits in");
static_assert(kMinStack.y + kMinStack.h <= kMinScreen.notice_y,
              "the stack's first slot stays clear of the notice line");
static_assert(kMinSide.x + kMinSide.w == kMinScreen.w,
              "the side region reaches the screen's right edge");
static_assert(kMinSide.y + kMinSide.h == kMinScreen.room_y + kMinScreen.room_h,
              "the side region ends where the workspace does, above the bottom band");

/// THE OVERLAY COLUMN: the stack's first slot's corner and width, from its top to the
/// workspace's bottom -- the floor `stack_capacity` itself respects, one row above the
/// setup line, so nothing placed here can erase the line naming the arrangement.
inline constexpr PixelRect overlay_column(const Screen& sc) noexcept {
    const PixelRect slot = placement_bounds(placement::kOverlayStack, 0, sc);
    return PixelRect{slot.x, slot.y, slot.w, sc.room_y + sc.room_h - slot.y};
}

/// How many overlay slots of the fallback height one pass down the column holds on this screen:
/// a fact the desk's presentation claim carries, so a managed open sees the room move.
// WL-PANE-03, WL-PANE-04 -- agents/workshop/panes-and-windows.md
// WL-EDIT-13 -- agents/workshop/editor.md
inline constexpr std::size_t stack_slots_that_fit(const Screen& sc) noexcept {
    const std::int64_t floor_y = sc.room_y + sc.room_h;
    std::size_t fit = 0;
    while (fit < kMaxSetupPanes) {
        const PixelRect b = placement_bounds(placement::kOverlayStack, fit, sc);
        if (b.y + b.h > floor_y) {
            break;
        }
        ++fit;
    }
    return fit;
}

/// The column's room in the shape a pane is sized by (`preferred_extent`), so no call site
/// spells the conversion itself.
inline constexpr StackCapacity stack_capacity(const Screen& sc) noexcept {
    const bool graphical = sc.text_advance_px > 0 && sc.text_line_px > 0;
    const auto line = graphical ? std::min(sc.text_line_px, std::int64_t{8192})
                                : surface::kCanvasCellPx;
    const auto column = graphical ? std::min(sc.text_advance_px, std::int64_t{8192})
                                  : surface::kCanvasCellPx;
    const auto border = graphical ? chrome_grain(sc) + surface::kTextInsetPx : kChromePx;
    const auto text_row = graphical ? line + 2 * surface::kTextInsetPx : surface::kCanvasCellPx;
    return StackCapacity{stack_slots_that_fit(sc), sc.room_h, sc.room_w, line, column, border,
                         surface::px_of_cells(kStackRows), surface::px_of_cells(kStackGap),
                         graphical ? chrome_grain(sc) : kChromePx, text_row, text_row,
                         chrome_grain(sc)};
}

static_assert(kMinScreen.room_y + kMinScreen.room_h == kMinScreen.notice_y,
              "the overlay floor is the workspace's bottom, which is the bottom band's own "
              "top row: a slot allowed past it would erase the row the tool speaks in");
static_assert(stack_slots_that_fit(kMinScreen) == 1,
              "the minimum composition's column holds one pane of the fallback height, and the "
              "next begins the column again at its top");

// ---- PLACEMENT SPENT ON THE POINTER: a place a weaver can see is a place a hand meets ------
// WL-PANE-05 -- agents/workshop/panes-and-windows.md; WL-PRESS-04 -- agents/workshop/press-chain.md

/// What `Occupancy` answers when it met no pane: the bare room. Negative, so it cannot collide
/// with a kind, and a consumer that forgot to test it falls outside every lookup.
inline constexpr std::int64_t kNoKind = -1;

/// The canvas pixel a reported pointer position lands on, whatever medium reported it, and the
/// cell that pixel is in -- or nothing, for a space this application cannot place.
// WL-GEO-07 -- agents/workshop/geometry.md
struct PointedAt {
    bool understood = false;
    surface::CanvasPoint cell;
    /// The same moment in canvas pixels, and the grain the reporting medium can honestly
    /// distinguish (one window pixel or one terminal cell).
    // WL-GEO-07 -- agents/workshop/geometry.md
    surface::CanvasPoint px;
    std::int64_t grain = surface::kCellGrainPx;
};

PointedAt canvas_point_of(std::int64_t space, std::int64_t x, std::int64_t y) noexcept;

/// WHAT A WEAVER'S HAND MEETS AT A CANVAS CELL: nothing, or the presentation occupying it.
struct Occupancy {
    bool occupied = false;
    /// The name a weaver reads on those cells, empty when nothing is there: a sentence, never a
    /// kind to switch on.
    // WL-PANE-05 -- agents/workshop/panes-and-windows.md
    std::string what;
    /// WHICH PRESENTATION, as a handle -- `kNoKind` for nothing at all.
    // WL-PRESS-04 -- agents/workshop/press-chain.md
    std::int64_t kind = kNoKind;
};

/// DOES ANY VISIBLE PRESENTATION OCCUPY THIS CANVAS CELL — the one question the pointer asks
/// before it asks the document anything.
Occupancy occupied_at(const Panes& panes, const Setup& setup, const Screen& sc,
                             const PointedAt& at);

/// The same walk for a cell-grain probe: the question a terminal pointer asks natively.
Occupancy occupied_at(const Panes& panes, const Setup& setup, const Screen& sc,
                             std::int64_t cx, std::int64_t cy);

// ---- PANE MANAGEMENT: what a weaver is ARRANGING, and how ------------------------------

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

/// The edge a weaver reads, and the mark they read it BY.
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
/// draw is a mark a weaver cannot read (`detail::kElided`'s reason).
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

/// HOW DEEP AN EDGE'S GRAB BAND REACHES INTO THE PANE, in canvas pixels: one cell —
/// exactly the ring the affordances have always occupied.
// WL-ARR-01 -- agents/workshop/arrangement.md
inline constexpr std::int64_t kPaneEdgeBandPx = surface::kCanvasCellPx;

/// HOW NEAR A LINE AN EDGE THE HAND MOVES COMES TO MEET IT, in canvas pixels: under a cell, so a
/// terminal hand a cell away is not pulled across it, and at least half a cell, so a terminal hand
/// at the nearest cell meets a line that falls inside it.
// WL-ARR-17 -- agents/workshop/arrangement-snap.md
inline constexpr std::int64_t kPaneSnapReachPx = 8;

static_assert(kPaneSnapReachPx < surface::kCanvasCellPx &&
                  2 * kPaneSnapReachPx >= surface::kCanvasCellPx,
              "a snap reaches under a cell, so a terminal hand is never pulled a whole cell, and "
              "at least half one, so every line falls within reach of a terminal hand's cell");

/// THE ONE CELL-SIZED MARK AN AFFORDANCE IS DRAWN ON — at the pane's own
/// edges.
PixelRect pane_edge_cell(const PixelRect& r, std::int64_t edge) noexcept;

/// What "take hold here" looks like: one character, used by none of the medium's role glyphs
/// (`.` room, `#` body, `*` ring, `!` alert).
inline constexpr const char* kHandleGlyph = "+";

/// One character for the cell an affordance is drawn on: `pane_edge_mark`'s two-character spelling
/// is prose. The corners share `kHandleGlyph`; their positions tell them apart.
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
std::int64_t pane_edge_at(const PixelRect& r, std::int64_t sx, std::int64_t sy,
                                 std::int64_t grain) noexcept;

/// THE ARRANGEMENT STATE: WHICH SCOPE A WEAVER IS ARRANGING, AND WHICH PANE THE
/// VOCABULARY ADDRESSES.
// WL-ARR-03, WL-ARR-07 -- agents/workshop/arrangement.md
struct PaneArrange {
    bool open = false;
    bool desk = false;
    PaneRef pane;
    bool resetting = false;

    bool addressed() const { return !pane.provider.empty(); }
};

/// The inspector's subject: a pane an inspector named, the owner's rows over it, and the name this
/// host gives what they address. Written by one door (`on(InspectPaneRequested)`); the name moves
/// only when the pane, the live desk or the rows' interior arm does. Session, never persisted.
// WL-INFO-14 -- agents/workshop/info-body.md
struct InspectedPane {
    PaneRef ref;               ///< the pane an inspector asked for; an empty provider is "none"
    std::vector<Row> rows;     ///< the owner's rows over it (`pane_subject_rows`)
    std::int64_t name = 0;     ///< what the picture carries and a commit returns; 0 is unnamed
    std::int64_t minted = 0;   ///< the last name handed out; names are never handed out twice
    std::uint64_t desk = 0;    ///< `SetupState::put_live` when `name` was given

    bool addressed() const { return !ref.provider.empty(); }
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
    std::int64_t from_x = 0;  ///< size: the pixel the press landed on
    std::int64_t from_y = 0;
    std::int64_t base_x = 0;  ///< size: the pane's window at that moment — place...
    std::int64_t base_y = 0;
    std::int64_t base_w = 0;  ///< ...and extent
    std::int64_t base_h = 0;
    /// The lines the last motion's snap met, in the room, which the affordance plane marks while
    /// the gesture is held; none on an axis that met none.
    // WL-ARR-17 -- agents/workshop/arrangement-snap.md
    std::optional<std::int64_t> met_x;
    std::optional<std::int64_t> met_y;
};

/// WHICH EDITABLE LINE A TEXT-SELECTION DRAG IS SWEEPING.
// WL-TEXT-14 -- agents/workshop/text-box.md
namespace text_drag_place {
inline constexpr std::int64_t kNone = 0;
// 1-4 were retired places; the one left keeps its number: session routing, never durable.
/// An external pane's body: the press named a row of its granted room, and every motion until the
/// release crosses as `PaneDragged`, resolved against that body at each motion.
// WL-TEXT-14 -- agents/workshop/text-box.md
inline constexpr std::int64_t kExternalPane = 5;
} // namespace text_drag_place

struct TextDrag {
    bool active = false;
    std::int64_t place = text_drag_place::kNone;
    /// The pane a `kExternalPane` sweep belongs to, by handle, re-checked at every motion.
    std::int64_t kind = kNoPaneKind;
};

/// HOW LONG A DOUBLE-CLICK MAY TAKE.
// WL-PTR-01 -- agents/workshop/pointer.md; WL-TAB-10 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kDoubleClickMs = 400;

/// WHAT THE LAST PRESS ON A LAYOUT TAB NAMED, so the next one can be a double.
// WL-PTR-01 -- agents/workshop/pointer.md; WL-TAB-10 -- agents/workshop/tab-run.md
struct TabClickMemory {
    bool armed = false;
    std::size_t at = 0;      ///< the position the press landed on
    std::int64_t at_ms = 0;  ///< `interaction_now_ms()` when it landed
};

/// IS THIS PRESS THE SECOND HALF OF A DOUBLE-CLICK ON THE SAME TAB? Pure, total, and the ONE
/// place the question is decided.
bool doubles_a_tab_click(const TabClickMemory& prior, std::size_t at,
                                std::int64_t now_ms) noexcept;

/// WHICH LAYOUT TAB A REORDER DRAG IS CARRYING -- the fourth gesture record.
// WL-TAB-11 -- agents/workshop/tab-run.md
struct LayoutTabDrag {
    bool active = false;
};


/// The session: what a weaver is currently doing, as opposed to what they authored -- kept apart
/// from every file a weaver owns, so selection is never mistaken for content.
struct Session {
    /// How much room the surface said it has, in canvas pixels.
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
    // WL-SESSION-09 -- agents/workshop/session-restore.md
    std::int64_t normal_w = kScreenMinW;
    std::int64_t normal_h = kScreenMinH;
    /// THE WINDOW'S DESKTOP PLACEMENT, AS LAST REPORTED.
    // WL-SESSION-08 -- agents/workshop/session-restore.md
    bool placement_known = false;
    std::int64_t place_x = 0;
    std::int64_t place_y = 0;
    bool place_maximized = false;
    /// THE LAST THING WORKSHOP HAD TO SAY, and that is all it is.
    // WL-ATTN-01 -- agents/workshop/attention.md
    std::string notice;
    bool notice_is_bad = false; ///< whether that thing was a refusal
    /// WHAT IS TRUE RIGHT NOW AND HAS NO LIVE OWNER TO DERIVE IT FROM (attention.hpp).
    // WL-ATTN-01 -- agents/workshop/attention.md
    HeldConditions conditions;
    /// The contextual-action surface: an identity and a cursor, never a snapshot (context.hpp).
    /// Opening it moves no selection or keyboard candidate.
    ContextMenu context;
    /// ...AND A PANE'S OWN MENU, PRESENTED BY THE PRESENTER PARTICIPANT on a popup this host
    /// granted (context.hpp). At most one of the two is open: each is a surface the weaver's next
    /// keys and presses go to, and the later one withdraws or closes the earlier.
    PresentedMenu presented;
    /// THE DYNAMIC PANES a weaver has opened (panes.hpp).
    Panes panes;
    /// THE AUTHORED SETUP THIS SESSION IS SHOWING, its copy of the one in its file, and the
    /// one-line editor over its name (setup.hpp).
    // WL-LAYOUT-01 -- agents/workshop/layouts.md
    SetupState setup;
    /// WHICH SCOPE A WEAVER IS ARRANGING AND WHICH PANE THE VOCABULARY ADDRESSES.
    // WL-ARR-07 -- agents/workshop/arrangement.md
    PaneArrange arrange;
    /// ...and the pane gesture their pointer is holding, if any: its own record, so a release ends
    /// the gesture its press began.
    PaneGesture pane_drag;
    /// The inspector's subject (`InspectedPane`): session, never persisted.
    InspectedPane inspected;
    /// ...and the text selection their pointer is sweeping, if any. The third
    /// gesture record, for the two records' own reason; see `TextDrag`.
    TextDrag text_drag;
    /// ...and what their LAST press on a LAYOUT TAB named, so the next one can be a
    /// double-click. See `TabClickMemory`.
    TabClickMemory tab_click;
    /// ...and the layout tab their pointer is dragging along the run, if any. The
    /// fourth gesture record, for the other three's own reason; see `LayoutTabDrag`.
    LayoutTabDrag tab_drag;
    /// THE CLIPBOARD THIS WORKSHOP'S TEXT BOXES OPERATE ON — session in the plainest sense.
    // WL-TEXT-08 -- agents/workshop/text-box.md
    component::Clipboard clipboard;
    /// THE EFFECTIVE BINDING TRUTH: declaration defaults plus the weaver's
    /// authored overrides, plus the legend preference.
    // WL-KEY-02 -- agents/workshop/keyboard.md
    Keymap keymap;
    /// WHETHER THE ARRANGEABLE PANES PAINT THEIR TITLE ROWS -- a presentation preference.
    // WL-FOCUS-11 -- agents/workshop/focus.md
    bool pane_titles = true;
};

/// This session's screen furniture. The one call; see `Screen`.
// WL-GEO-02, WL-GEO-03, WL-GEO-04, WL-GEO-05 -- agents/workshop/geometry.md
inline constexpr Screen screen_of(const Session& s) noexcept {
    return screen_of(s.screen_w, s.screen_h, s.text_advance_px, s.text_line_px, s.cell_px);
}

/// THE CHAIN BELOW THE CONTEXTUAL SURFACE -- the branches a key falls to once no mode above
/// them claims it. Split out of `keyboard_context` because the contextual surface needs
/// exactly this half as a VALUE: what the keys would mean when the menu closes.
KeyContext keyboard_context_beneath_menu(const Session& s);

/// WHERE THE KEYBOARD CURRENTLY GOES, AS ONE VALUE -- the routing chain, spelled once. It
/// is resolved fresh from live session state at every spend and stored nowhere: there is
/// no context stack, and a mode that closes stops being the answer with nothing to clear.
KeyContext keyboard_context(const Session& s);

/// The pane an ordinary key goes to right now, or `kNoPaneKind`: the context is a pane's and no
/// hotkey view has the keys. Not `keyboard_pane`, which names the pane the keys return to. A
/// press's `keys_went_here` and the band's typing sentence are both this answer.
std::int64_t typing_pane(const Session& s);

/// MAY ESCAPE'S FINAL FALLTHROUGH SHED THE PANE SELECTION IN THIS CONTEXT?
bool default_row_context(KeyContext c);

// ---- Spelling the effective bindings -----------------------------------------------------

/// The effective gesture of one action, in the screen's compact voice (`^s`, `shift+h`,
/// `enter`). The one call every claim site makes.
std::string hotkey_text(const Keymap& k, Act a);

/// Four direction actions said as one word when true: `arrows` while all four sit on their arrow
/// defaults, their own spellings otherwise.
std::string arrows_text(const Keymap& k, Act left, Act right, Act up, Act down);

/// The `gesture label` pairs requestable in this context, in the order the band spends room on
/// them: the context's own rows (for `kPane`, the keyboard pane's declared ones), then what is
/// answered above the mode chain.
// WL-KEY-15 -- agents/workshop/keyboard.md
std::vector<std::string> help_pairs(const Keymap& k, KeyContext ctx,
                                    std::int64_t pane = kNoPaneKind);

// The band's legend rows are packed from `help_pairs` by `help_rows`, into however many rows the
// band's budget granted.

/// Take the room a surface offered and re-fit the workspace to it; answers whether anything
/// changed, so a caller can decline to repaint.
bool adopt_screen(Session& s, std::int64_t want_w, std::int64_t want_h,
                         std::int64_t want_advance_px = 0, std::int64_t want_line_px = 0,
                         std::int64_t want_cell_px = 0);


namespace detail {

/// Left-align in a fixed width; longer text is cut. `fit` is for text whose length a document
/// decides.
std::string pad(std::string text, std::size_t width);

/// The mark a bounded presentation leaves where it could not show everything: plain ASCII, because
/// the canvas is plain by contract.
inline constexpr const char* kElided = "...";

/// How far a wrapped continuation row is indented, when the room can hold an indent at all.
inline constexpr std::int64_t kWrapIndent = 2;

/// Fit `text` into `width` cells, AND SAY SO when it did not fit.
std::string fit(std::string text, std::int64_t width);

/// HOW MUCH OF A PATH IS THE CUE THAT SAYS WHICH FILESYSTEM IT IS ON -- `/`, `C:/`,
/// `//server/`, or nothing at all for a spelling that has no root.
std::size_t path_root_cue(const std::string& p);

/// FIT A PATH, KEEPING THE END THAT SAYS WHICH FILE OR DIRECTORY IT IS.
std::string fit_path(const std::string& path, std::int64_t width);

/// FIT `text` INTO AS MANY ROWS AS IT NEEDS, at most `width` cells each.
std::vector<std::string> wrap(const std::string& text, std::int64_t width);

/// One cell along, without leaving the number line: a nudge's proposal is computed, so the step
/// saturates and the result meets the ordinary refusal.
std::int64_t step(std::int64_t v, std::int64_t by) noexcept;

/// `a - b` without leaving the number line: a resize's proposal is a difference of two values this
/// weave does not own, so it saturates, far outside any workspace.
std::int64_t minus(std::int64_t a, std::int64_t b) noexcept;

} // namespace detail

/// The band's legend rows, as the legend projects them, budget-composed. `pane` is the
/// keyboard pane's handle when `ctx` is `kPane`, so its declared rows are packed too.
std::vector<std::string> help_rows(const Keymap& k, KeyContext ctx,
                                          std::int64_t width, std::size_t rows,
                                          std::int64_t pane = kNoPaneKind);

/// A PANE WINDOW PROPOSAL, IN CANVAS PIXELS: what one resize gesture asks the whole
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

/// WHAT A WINDOW PROPOSAL WRITES through the gesture door (`author_pane_window`), measured from the
/// window it was proposed against: a place on an axis the proposal moved and changed, an extent on
/// an axis it changed. The hand's write and the desk a snap measures are made from the same axes.
struct PaneWindowAxes {
    PaneAxisProposal horizontal;
    PaneAxisProposal vertical;
};
// WL-ARR-17 -- agents/workshop/arrangement-snap.md
PaneWindowAxes pane_window_axes(const PaneWindowProposal& want, const PixelRect& base);

/// THE LINES AN EDGE THE HAND MOVES MAY MEET, in the room: the room's own edges and every edge
/// of every other pane on the screen.
// WL-ARR-17 -- agents/workshop/arrangement-snap.md
struct PaneSnapLines {
    std::vector<std::int64_t> xs;
    std::vector<std::int64_t> ys;
};

PaneSnapLines pane_snap_lines(const Panes& panes, const Setup& setup, const Screen& sc,
                              std::int64_t held);

/// A HAND'S PROPOSAL, SNAPPED, and the line each axis met.
// WL-ARR-17 -- agents/workshop/arrangement-snap.md
struct SnappedWindow {
    PaneWindowProposal want;
    std::optional<std::int64_t> met_x;
    std::optional<std::int64_t> met_y;
};

/// Each edge the hand moves -- both edges of both axes for a move (`kNoPaneEdge`), the pulled
/// edges for a resize -- comes to the nearest line within `kPaneSnapReachPx`, the opposite edge
/// held; a snap that would leave the place or the extent outside a pane's rules is not taken.
SnappedWindow snap_pane_window(const PaneWindowProposal& want, std::int64_t edge,
                               const PaneSnapLines& lines) noexcept;

// ---- Where a pointer is, in workspace cells --------------------------------------------


/// WHERE A POINTER LANDED INSIDE A BOUNDED TEXT REGION, in that region's own prose.
// WL-PRESS-03 -- agents/workshop/press-chain.md
struct ProseAt {
    bool understood = false;
    std::int64_t column = 0;
    std::int64_t row = 0;
};

ProseAt prose_at(std::int64_t space, std::int64_t x, std::int64_t y,
                 const surface::RegionFit& fit) noexcept;

// ---- A bounded list: what it shows, and what it must say it cannot ---------------------------
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

// ---- The dynamic panes, painted -------------------------------------------------------

/// THE BACKDROP OF A PANE: its whole bounds, in one rect.
void paint_pane_frame(surface::SurfaceLayer& layer, const PixelRect& b,
                              std::int64_t role);

/// THE POPUP'S OR A PANE'S BODY AS ONE BOUNDED REGION OF PROSE, RESOLVED ONCE.
// WL-CHROME-05 -- agents/workshop/chrome.md; WL-RGN-01 -- agents/workshop/regions.md
struct ProsePlace {
    bool present = false;
    std::int64_t rows = 0;    ///< prose rows of the ACTIVE medium's type that fit the rectangle
    std::int64_t columns = 0; ///< ...and how many characters fit across one of them
    /// The resolution itself, so a press inverse spends the fit the painter was handed.
    surface::RegionFit fit{};
    /// The interior the fit was resolved for: the published region is the rectangle measured.
    PixelRect inside{};
    std::int64_t chrome_px = 0;
};

/// The one call, total over the rectangle: a closed pane answers with an empty one. Bounds fit
/// where they are.
ProsePlace prose_place(const PixelRect& b, const Screen& sc);

/// The region a `ProsePlace` was resolved for, empty and ready for its rows.
surface::SurfaceTextRegion prose_region(const ProsePlace& place);

// ---- WHAT STATE ONE PANE IS IN -- the recovery invariant, as one word -----------------
// WL-PANE-10 -- agents/workshop/panes-and-windows.md

namespace pane_state {
inline constexpr std::int64_t kClosed = 0;
inline constexpr std::int64_t kUnresolved = 1;
inline constexpr std::int64_t kOffRoom = 4;
inline constexpr std::int64_t kCovered = 5;
inline constexpr std::int64_t kOpen = 6;
} // namespace pane_state

/// The word a weaver reads. Total over the integer, for `builtin_pane`'s reason.
const char* pane_state_word(std::int64_t state);

/// WHAT A WEAVER CAN DO ABOUT ONE STATE -- the remedy column of the table above, as a
/// function.
const char* pane_state_remedy(std::int64_t state);

/// IS EVERY VISIBLE CELL OF THIS PANE BEHIND ANOTHER ONE?
bool pane_is_covered(const Panes& panes, const Setup& setup, const Screen& sc,
                            std::int64_t kind, const PixelRect& mine);

/// THE ONE STATE CLASSIFIER. Asked of an inventory row -- which is the union of the catalog
/// and everything the setup names -- so every authored pane gets exactly one answer and no
/// row is silently omitted because the runtime catalog lacks it. A caller that only asks
/// whether a pane is on the screen at all passes `judge_cover` false, and a covered pane then
/// answers `open`, sparing the comparison against every pane in front of it.
std::int64_t pane_state_of(const Panes& panes, const Setup& setup, const Screen& sc,
                                  const CatalogRow& row, bool judge_cover = true);



// ---- SAYING A PANE'S GEOMETRY IN THE FACE'S OWN LANGUAGE ------------------------------
// WL-GEO-09, WL-GEO-10 -- agents/workshop/geometry.md

/// WHAT ONE PIXEL VALUE IS IN THE ACTIVE MEDIUM'S UNIT, AND WHETHER THAT IS THE
/// AUTHORED NUMBER ITSELF. `exact` false means the amount shown is this medium's
/// floor of a value it cannot say -- a projection, which the readout marks.
struct GeometrySpelling {
    std::string amount;
    bool exact = true;
};

/// The unit word for a medium that reported `cell_px`: `px` where it named a device pixel, `cells`
/// where its unit is the cell.
const char* geometry_unit(std::int64_t cell_px);

/// ONE PIXEL COORDINATE OR EXTENT, SPELLED FOR THIS MEDIUM.
GeometrySpelling geometry_spelling(std::int64_t px, std::int64_t cell_px);

/// The mark an inexact spelling wears: ASCII, because the shipped face covers printable ASCII only.
inline constexpr const char* kProjectedMark = "~";

/// The clause a line carries only when a number on it is a projection.
inline constexpr const char* kProjectedNote = " (~ projected)";

/// ONE PIXEL VALUE, WITH ITS MARK. `any_projected` accumulates, so a caller decides
/// once whether the line it is building owes the clause above.
std::string geometry_amount_text(std::int64_t px, std::int64_t cell_px,
                                        bool& any_projected);

/// THE SAME SPELLING READ BACKWARDS: a whole number a weaver TYPED in the active
/// face's unit, as canvas pixels: exact on the window, whole cells where the unit is the cell.
// WL-PED-06 -- agents/workshop/pane-manager.md
inline constexpr std::int64_t px_of_device_amount(std::int64_t amount,
                                                    std::int64_t cell_px) noexcept {
    const std::int64_t bound = (std::numeric_limits<std::int64_t>::max)() / surface::kCanvasCellPx;
    const std::int64_t a = amount > bound ? bound : (amount < -bound ? -bound : amount);
    if (cell_px <= 0) {
        return a * surface::kCanvasCellPx;
    }
    const std::int64_t num = a * surface::kCanvasCellPx;
    if (num < 0) {
        return -((-num + cell_px - 1) / cell_px);
    }
    return (num + cell_px - 1) / cell_px;
}

static_assert(px_of_device_amount(10, 0) == 10 * surface::kCanvasCellPx,
              "on a cell medium a typed cell count is that many whole cells");
static_assert(px_of_device_amount(120, surface::kCanvasCellPx) == 10 * surface::kCanvasCellPx,
              "on the shipped window a typed pixel count is exact where the grain divides");

/// WHAT A WEAVER TYPED FOR ONE GEOMETRY AMOUNT: `10`, `10 cells`, `120px` -- a whole number,
/// optionally followed by THIS face's unit word.
// WL-PED-06 -- agents/workshop/pane-manager.md
struct FaceAmount {
    bool accepted = false;
    std::int64_t px = 0;
    std::string refusal;
};

FaceAmount parse_face_amount(std::string_view text, std::int64_t cell_px);

/// A WHOLE PIXEL RECTANGLE, IN THE ACTIVE MEDIUM'S UNIT -- `@x,y WxH unit`.
std::string pixel_rect_text(const PixelRect& r, std::int64_t cell_px);

/// WHAT A WEAVER AUTHORED FOR ONE PANE'S WINDOW, in the active medium's own unit.
std::string pane_window_text(const SetupPane* row, std::int64_t cell_px);

/// IS ANY PART OF THIS PANE'S WINDOW STILL THE CODE'S ANSWER RATHER THAN THE WEAVER'S?
bool pane_window_partly_default(const SetupPane* row);


// ---- A SURFACE SIZED BY WHAT IT SAYS, PLACED ---------------------------------------------

/// Where a surface sized by its own content opens, asked at an anchor.
PixelRect popup_bounds_at(std::int64_t want_cols, std::int64_t want_rows,
                                std::int64_t x, std::int64_t y, const Screen& sc);

// ---- THE EFFECTIVE KEYMAP, AS A VALUE -----------------------------------------------------

/// What to call a keyboard context, in a group heading's voice.
std::string keyboard_context_name(const Session& s, KeyContext ctx);

/// Every binding in force, grouped by where it is answered, read off `Session::keymap` -- the one
/// truth dispatch reads. `file` and `word` are the keymap file and what loading it came to. A
/// projection: it holds and decides nothing (WL-DESK-11).
KeymapShown keymap_shown(const Session& s, const std::string& file, const std::string& word);

// ---- WHAT IS TRUE RIGHT NOW, PROJECTED ---------------------------------------------------

/// Condition keys: durable dotted strings, spelled once so `establish` and `find` cannot drift. A
/// per-subject family carries the subject in the key: two panes refusing are two conditions.
// WL-ATTN-01 -- agents/workshop/attention.md
inline constexpr const char* kKeymapWallKey = "workshop.keymap-refused";
/// ...AND THE ONE FOR AN EDIT THAT IS LIVE BUT NOT WRITTEN: the next launch will not have it.
inline constexpr const char* kKeymapUnwrittenKey = "workshop.keymap-unwritten";
inline constexpr const char* kPrefsWallKey = "workshop.prefs-refused";
/// A session file this run could not read and so will not write over; the refusal is said once,
/// and this stands all run.
inline constexpr const char* kSessionWallKey = "workshop.session-refused";
inline constexpr const char* kLegacyShadowedKeyPrefix = "workshop.legacy-shadowed.";
std::string pane_content_key(const PaneRef& ref);
std::string pane_window_key(const PaneRef& ref);
inline constexpr const char* kFrontierKey = "project.frontier-waiting";

/// EVERY CONDITION THAT IS CURRENTLY TRUE AND WORTH AMBIENT ATTENTION, ranked.
std::vector<Condition> attention_conditions(const Session& s,
                                                   const ProjectFrontier& frontier = {});

/// Every current condition as the sentence that crosses the pane seam: its four fields, and its
/// action resolved into words against the effective keymap (empty when none answers).
std::vector<StandingCondition> standing_conditions(const Session& s,
                                                   const ProjectFrontier& frontier = {});

/// ARE THESE THE SAME SENTENCES? Field by field, in order, because the order is part of
/// what is being said (WL-ATTN-07). What this answers is "is there news", and the caller
/// stays silent when there is not.
bool same_conditions(const std::vector<StandingCondition>& a,
                     const std::vector<StandingCondition>& b);

/// THE PARTICIPANT'S RECORD, AS THE SEAM CARRIES IT: derived at the moment of the ask, holding
/// no `loom::TerminalSession`, no `Transcript` and no entry the renderer could not read.
TranscriptShown transcript_shown(const loom::TerminalSession* me);

/// ...and the comparison that decides whether saying it again would be news.
bool same_transcript(const TranscriptShown& a, const TranscriptShown& b);

/// `LineSlot` AS THE SEAM SPELLS IT.
const char* slot_name(LineSlot slot) noexcept;

// ---- WHAT CAN I DO WITH THIS, PRESENTED --------------------------------------------------



/// One entry as its row reads: a group descends and says so, an action is its declared
/// label -- `row_of_id`'s answer, never a second spelling.
std::string context_entry_text(const ContextEntry& entry);

/// The widest the popup may grow, in prose columns: the stack pane's width. Content chooses the
/// extent below it.
inline constexpr std::int64_t kContextMaxCols = kStackW;

/// The label column of one level: the widest entry text, so annotations start in one
/// column down the whole menu rather than ragged after each label.
std::int64_t context_label_columns(const std::vector<ContextEntry>& rows);

/// THE GESTURE WORTH TEACHING BESIDE ONE ENTRY, or "".
std::string context_annotation(const Session& s, const ContextEntry& entry);

/// One population row as composed: the entry's text and, where truthful, the effective gesture at
/// the level's annotation column. The painter and the extent both spend this spelling.
std::string context_row_text(const Session& s, const ContextEntry& entry,
                                    std::int64_t label_columns);

/// WHERE THE CONTEXTUAL SURFACE OPENS: beside the press that asked, sized by what
/// it has to say.
PixelRect context_bounds(const Session& s, const Screen& sc);


/// The cursor, bounded by the population's size: the population is derived, so it can move
/// between a keystroke and a repaint.
inline constexpr std::size_t context_cursor_bound(std::size_t cursor,
                                                  std::size_t population) noexcept {
    if (cursor < population) {
        return cursor;
    }
    return population == 0 ? 0 : population - 1;
}

void paint_context(surface::SurfaceLayer& layer, const Session& s, const Screen& sc);

/// THE NAME OF EACH LINE `paint_context` PAINTS, in its order: the id of the action a line shows,
/// or the name of the group it opens, and "" for a marker.
std::vector<std::string> context_line_names(const Session& s, const Screen& sc);

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

// ---- A PANE'S MENU, AS ITS PRESENTER SHOWED IT -------------------------------------------------
//
// The fixed display machinery a presenter participant draws through (presenter_vocabulary.hpp):
// the popup's frame and place are this host's, what its lines say is the presenter's.

/// THE MOST A PRESENTED MENU CAN SHOW AT AN ANCHOR on this screen -- the room `MenuGranted` hands
/// the presenter: the biggest popup that fits there, read as rows and columns of prose.
// WL-CTX-09 -- agents/workshop/pane-menu.md
ProsePlace presented_room(bool anchored, std::int64_t x, std::int64_t y, const Screen& sc);

/// WHERE THE PRESENTED MENU OPENS: beside its anchor, sized by the lines the presenter showed;
/// empty while it has shown none.
// WL-CTX-09 -- agents/workshop/pane-menu.md
PixelRect presented_bounds(const Session& s, const Screen& sc);

// WL-CTX-09 -- agents/workshop/pane-menu.md
void paint_presented(surface::SurfaceLayer& layer, const Session& s, const Screen& sc);

/// WHERE A PRESS LANDED ON THE PRESENTED MENU -- inside it or not, and which line (-1 for none:
/// the frame, or a line the room could not show). The painter's inverse, over the same place.
// WL-CTX-09 -- agents/workshop/pane-menu.md
struct PresentedPressAt {
    bool inside = false;
    std::int64_t line = -1;
};

// WL-CTX-09 -- agents/workshop/pane-menu.md
PresentedPressAt presented_press_at(const Session& s, const Screen& sc, std::int64_t space,
                                    std::int64_t x, std::int64_t y, const PointedAt& at);

// ---- AN EXTERNAL PANE'S BODY: one header row of Workshop's, and a region ---------------

/// One header row, Workshop's own, so the provenance of what follows is legible.
// WL-FOCUS-11 -- agents/workshop/focus.md; WL-PANE-06 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t kExternalHeaderRows = 1;

/// How many header rows this pane's presentation reserves now: the one resolution of the title
/// preference, spent by the painter, the press path and the room grant alike.
std::int64_t external_title_rows(const Panes& panes, std::int64_t kind,
                                        bool titles_shown) noexcept;

/// WHAT A PANE SAYS BEFORE ITS PROVIDER HAS SAID ANYTHING.
// WL-PANE-16 -- agents/workshop/panes-and-windows.md
inline constexpr const char* kExternalWaiting = "(waiting for the provider)";

/// What a pane says after an update it could not keep: Workshop's sentence, echoing nothing of it.
inline constexpr const char* kExternalRefused =
    "(the last update did not fit this pane's room -- none of it was kept)";

/// ...and after a picture it could not keep, which is refused for what it holds, never for its room.
inline constexpr const char* kExternalPictureRefused =
    "(the last picture was refused -- none of it was drawn)";

/// What a picture still standing wears while a later one stands refused: it is older than what
/// its pane last drew.
inline constexpr const char* kExternalRefusedMark = "(update refused)";

/// THE BODY OF AN EXTERNAL PANE, RESOLVED ONCE. Where it is, and how much prose the
/// ACTIVE medium fits in it -- which is exactly the budget the provider is granted.
struct ExternalBodyPlace {
    bool present = false;
    /// The body's bounds in canvas pixels, as the published region states them; `fit` is
    /// resolved from them.
    std::int64_t region_x = 0;
    std::int64_t region_y = 0;
    std::int64_t region_w = 0;
    std::int64_t region_h = 0;
    surface::RegionFit fit{};
    /// The header rows this resolution reserved: the painter and the press path spend this number.
    std::int64_t header_rows = 0;
    std::int64_t rows = 0;    ///< prose rows -- the `PaneRoom` budget's first half
    std::int64_t columns = 0; ///< ...and its second
};

/// The body under an external pane's header row: the pane's whole bounds, less that row's
/// share of the PROSE the active medium fits in them.
ExternalBodyPlace external_body_place(const PixelRect& pane_rect, const Screen& sc,
                                             std::int64_t header_rows);

/// WHERE A PRESS LANDED IN AN EXTERNAL PANE'S GRANTED ROOM -- the `PaneRoom`
/// lattice, and nothing a provider was not already handed.
// WL-PRESS-04 -- agents/workshop/press-chain.md
struct ExternalPressAt {
    bool named = false;
    std::int64_t row = 0;    ///< a prose row of the BODY: 0 is the row under the header
    std::int64_t column = 0; ///< ...and the column of the pressed cell, the character it shows
};

/// LOCATE A PRESS IN THE ROOM A PANE WAS GRANTED, from the rectangle the painter used.
ExternalPressAt external_press_at(const Panes& panes, const Setup& setup,
                                         const Screen& sc, std::int64_t kind, bool titles,
                                         std::int64_t space, std::int64_t x, std::int64_t y);

/// The mark that says whether typing goes here.
// WL-FOCUS-10 -- agents/workshop/focus.md
inline constexpr const char* kTypingHere = "> ";
inline constexpr const char* kTypingElsewhere = "  ";

/// THE HEADER: what this pane is, and WHOSE it is -- both halves validated at admission,
/// neither echoed raw -- and whether typing goes here, said by a mark that costs no columns.
std::string external_header(const RuntimePane& row, bool typing);

/// ONE EXTERNAL PANE: Workshop's backdrop, Workshop's header, and ONE region carrying
/// whatever that office last validly said inside the room it was granted.
void paint_external(surface::SurfaceLayer& layer, const Panes& panes, std::int64_t kind,
                           const PixelRect& b, const Screen& sc, bool titles,
                           std::int64_t chrome = kPaneChrome);

// ---- The arrangement's affordance rings --------------------------------------------------

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

/// What the one-line name editor puts before and after the name a weaver is typing. The
/// hint is spelled from the effective keymap, like every other gesture claim.
inline constexpr const char* kSetupNamePrompt = "layout name> ";
std::string setup_name_hint(const Keymap& k);

/// The two gestures the setup line advertises, on the line the thing they act on is on.
std::string setup_hints(const Keymap& k);

/// The fewest columns the name editor will claim for the name itself, so that a surface
/// narrow enough for the chrome to exceed it still shows some of what is being typed.
inline constexpr std::int64_t kSetupNameMinCols = 8;

/// THE ROWS THE SCREEN RESERVES AT THE TOP, AS A RECTANGLE -- and that is the
/// LAYOUTS PANE'S DEVELOPER DEFAULT rather than a band's private geometry.
// WL-FRONT-03 -- agents/workshop/planes.md
inline constexpr PixelRect top_band_bounds(const Screen& sc) noexcept {
    return placement_bounds(placement::kTopBand, 0, sc);
}

/// WHERE THE LAYOUTS PANE'S CONTENT GOES AND WHAT FITS IN IT -- one resolution,
/// spent by the painter, by the tab press inverse and by the name editor's own window.
ExternalBodyPlace layouts_body(const Session& s, const Screen& sc);

/// THE BOTTOM BAND'S RECTANGLE AND ITS FIT -- what the tool just said, and what the keys
/// mean right now, composed against whatever the ACTIVE medium answers for these pixels
/// through the same `fit_region` every bounded region resolves with.
// WL-FRONT-02, WL-FRONT-03 -- agents/workshop/planes.md; WL-RGN-03 -- agents/workshop/regions.md
inline constexpr PixelRect band_bounds(const Screen& sc) noexcept {
    return PixelRect{0, sc.notice_y, sc.w, sc.h - sc.notice_y};
}

inline constexpr surface::RegionFit band_fit(const Screen& sc) noexcept {
    const PixelRect b = band_bounds(sc);
    return surface::fit_region(b.x, b.y, b.w, b.h, sc.text_advance_px, sc.text_line_px);
}

/// How much of the name the one-line editor can show at this extent: one measurer, so the text
/// box's window and the painter's slice are the same number.
std::int64_t setup_name_columns(const Session& s, const Screen& sc);

// ---- THE `setup:` SLOT: what the ACTIVE layout's association is --------------------------
// WL-TAB-02 -- agents/workshop/tab-run.md

/// The word before the association and the three it is said in: constants, because the row's
/// budget is derived from their widths.
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
std::string setup_rest_text(const SetupState& setup, const Panes& panes,
                                   const Keymap& keymap);

/// The workspace's extent as the band states it: a status fact, beside the setup identity.
std::string workspace_text(const Session& s);

// ---- THE LAYOUT TABS: the left of the status row -----------------------------------------

/// One painted tab: which layout it is, and exactly which bytes of the row are its own.
// WL-TAB-07 -- agents/workshop/tab-run.md
struct LayoutTab {
    std::size_t at = 0;       ///< the layout's position in the weaver's order
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
/// them, bare. No quoting, no escaping and no substitution -- the bytes a weaver typed.
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

/// The fewest columns the tab run keeps, so a surface too narrow for both still says which layout
/// is live.
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

/// The same composition resolved from the session, for a consumer holding a screen rather than the
/// painter's interior.
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
/// editor's caret and selection while a weaver is typing a name.
void paint_layouts(surface::SurfaceLayer& layer, const Session& s, const PixelRect& b,
                          const Screen& sc, std::int64_t chrome = kPaneChrome);

// ---- A PANE'S INTERIOR, CAPTURED ----------------------------------------------------------

/// THE ONLY HONEST INTERIOR OF A PANE: a read-only capture of the resolved body -- where it is,
/// how much prose the face fits in it, in which presentation -- and the plain statement that no
/// authored interior exists. A built-in's interior is its painter and a provider's is its own;
/// neither is decomposed, inferred or promised.
std::string interior_capture_text(const Session& s, const PaneRef& ref);

// ---- A WORKSHOP PANE AS A SUBJECT, inspected and edited through its owners (Info's) ------

/// The window a typed edit measures the other axis from: authored where authored, resolved where
/// reactive -- `managed_window_base`'s rule (weave.hpp).
PixelRect pane_window_base(const Session& s, const PaneRef& ref);

/// MAY THIS PANE'S GEOMETRY BE TYPED RIGHT NOW, and if not, why not -- the arrangement's
/// admission (`arrange_geometry_ready`, weave.hpp) less the one refusal a typed value does
/// not need.
Written pane_geometry_typeable(const Session& s, const PaneRef& ref);

/// ONE AUTHORED AXIS AS A WEAVER READS IT: the amount in the face's own unit, `-` for the
/// developer's answer, and the pixel spelling for the unit no medium here projects --
/// `pane_window_text`'s per-axis grammar, one axis at a time.
std::string pane_axis_text(const Session& s, const PaneRef& ref, std::size_t axis);

/// WRITE ONE AUTHORED AXIS FROM WHAT A WEAVER TYPED -- through the gesture door, one axis
/// proposed and the other left exactly as it stands (`author_pane_window`), or
/// through that axis's reset door for `-`.
Written write_pane_axis(Session& s, const PaneRef& ref, std::size_t axis,
                               const std::string& text);

/// The rows a pane is inspected by -- identity, authored, resolved, interior -- every closure
/// reading fresh and every setter an existing door. The rows name no key: the inspector holds its
/// own.
// WL-INFO-14 -- agents/workshop/info-body.md
std::vector<Row> pane_subject_rows(Session& s, const PaneRef& ref);

/// THE INSPECTOR'S SUBJECT AS THE SEAM CARRIES IT: the reference, the inventory's name for it,
/// the name of the rows and every row read fresh. Pure over the session.
// WL-INFO-14 -- agents/workshop/info-body.md
PaneSubjectShown pane_subject_shown(const Session& s);

/// ...AND WHETHER TWO READINGS SAY THE SAME THING, every field, for the publication's gate.
bool same_pane_subject(const PaneSubjectShown& a, const PaneSubjectShown& b);

// ---- THE COMPOSITION: every pane back to front, the bottom band, and the screen as planes ----

/// Every presented pane, back to front, one complete layer each.
void paint_panes(surface::SurfaceCanvas& c, const Session& s,
                         const Screen& sc);

/// THE BOTTOM BAND AS ONE PUBLISHED REGION: what the tool just said, and what the keys mean
/// right now.
surface::SurfaceTextRegion band_region(const Session& s, const Screen& sc);

/// The whole screen as one published canvas, an ordered list of planes: a pure projection of the
/// session and the screen.
surface::SurfaceCanvas paint(const Session& s);

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_HPP
