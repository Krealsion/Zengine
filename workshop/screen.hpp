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
#include "desktop_seam_vocabulary.hpp"    // ...and the effective keymap a presenter shows
#include "context.hpp" // what can be done with a pointed subject
#include "keymap.hpp"
#include "panes.hpp"
#include "property.hpp"
#include "settings_seam_vocabulary.hpp" // where a lock holds a setting
#include "setup.hpp"
#include "live_desk.hpp" // the live layout as the layouts' owner last sent it
#include "vocabulary.hpp"

#include "component/text_box.hpp" // the editable line, the caret in it, and its window
#include "graphics/canvas.hpp"   // the desk drawn for a graphics window
#include "graphics/pixels.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"  // `space` -- which medium's numbers a pointer reported in
#include "workshop/screen_planes.hpp"
#include "workshop/pane_text_fit.hpp"
#include "pane_text_fit.hpp"

#include <zen/terminal/session.hpp> // the participant this host holds, for the door

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

// ---- The one screen's layout, in canvas pixels -----------------------------------------

/// The smallest surface this screen is laid out on, in canvas pixels -- 78 by 22 cells -- and,
/// deliberately, the extent it uses when nothing tells it otherwise.
// WL-GEO-02 -- agents/workshop/geometry.md
inline constexpr std::int64_t kScreenMinW = 78 * graphics::kCellPx;
inline constexpr std::int64_t kScreenMinH = 22 * graphics::kCellPx;

/// The largest surface this screen will lay out, in canvas pixels: 640 by 400 cells.
// WL-SESSION-07 -- agents/workshop/window-file.md
inline constexpr std::int64_t kScreenMaxW = 640 * graphics::kCellPx;
inline constexpr std::int64_t kScreenMaxH = 400 * graphics::kCellPx;

/// THE ROWS OF TEXT RESERVED AT THE TOP OF THE SCREEN, and they are the first thing a weaver
/// reads. The band is as tall as these rows of the active medium's type need (`screen_of`).
// WL-GEO-03 -- agents/workshop/geometry.md; WL-FRONT-03 -- agents/workshop/planes.md
inline constexpr std::int64_t kTopRows = 2;

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
/// THE FALLBACK HEIGHT for providers without a preferred body size: `workshop.stack-rows`, from
/// `kMinStackRows` to `kMaxStackRows`; this is its default.
inline constexpr std::int64_t kStackRows = 9;
inline constexpr std::int64_t kMinStackRows = 5;
inline constexpr std::int64_t kMaxStackRows = 40;
inline constexpr std::int64_t kStackGap = 1;  ///< a blank row between stacked panes

static_assert(kMinStackRows <= kStackRows && kStackRows <= kMaxStackRows,
              "the default is a value its setting takes");


/// THE SCREEN'S FURNITURE, DERIVED IN ONE PLACE, in canvas pixels.
// WL-GEO-05 -- agents/workshop/geometry.md
struct Screen {
    std::int64_t w = kScreenMinW;  ///< the canvas extent this screen paints
    std::int64_t h = kScreenMinH;
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
    /// The overlay stack's fallback height, in cells, as the session's settings choose it.
    // WL-GEO-03 -- agents/workshop/geometry.md
    std::int64_t stack_rows = kStackRows;
};

/// How tall `rows` rows of text stand in a band, inside `chrome` on each side: the face's lines
/// and inset where the medium sets type, whole cells where text is a cell.
inline constexpr std::int64_t band_px_for(std::int64_t rows, std::int64_t text_advance_px,
                                          std::int64_t text_line_px,
                                          std::int64_t chrome) noexcept {
    return graphics::add_cells(region_px_for(1, rows, text_advance_px, text_line_px).h,
                              2 * chrome);
}

/// The furniture for a surface of this extent -- TOTAL over every std::int64_t, because the
/// extent it is given came off the bus.
// WL-GEO-02, WL-GEO-03, WL-GEO-04, WL-GEO-05 -- agents/workshop/geometry.md
inline constexpr Screen screen_of(std::int64_t want_w, std::int64_t want_h,
                                  std::int64_t text_advance_px = 0,
                                  std::int64_t text_line_px = 0,
                                  std::int64_t cell_px = 0,
                                  std::int64_t stack_rows = kStackRows) noexcept {
    Screen s;
    // The medium's device unit is taken as reported: non-positive already means "my device unit
    // is the cell".
    s.cell_px = cell_px > 0 ? cell_px : 0;
    s.w = want_w < kScreenMinW ? kScreenMinW : (want_w > kScreenMaxW ? kScreenMaxW : want_w);
    s.h = want_h < kScreenMinH ? kScreenMinH : (want_h > kScreenMaxH ? kScreenMaxH : want_h);
    s.stack_rows = std::clamp(stack_rows, kMinStackRows, kMaxStackRows);
    // The room is the surface: nothing reserves any of it, so what a share means never depends
    // on which panes are open (docs/history/decisions/the-reserved-column.md).
    s.room_w = s.w;
    // The fit is resolved here because the screen carries the metric, which arrives on the bus.
    const RegionFit fit =
        fit_region(0, 0, s.w, s.h, text_advance_px, text_line_px);
    // The metric as the fit resolved it: a non-positive advance or line already means "text is a
    // cell", so a screen never carries half a metric.
    s.text_advance_px = fit.advance_px;
    s.text_line_px = fit.line_px;
    // The bands are as tall as the text they hold: the Layouts pane's rows inside one device
    // unit of chrome at the top, and the tool's rows at the bottom. Where text is a cell they are
    // whole cells, the Layouts pane drawing no boundary in its two. A metric whose bands would
    // leave no room of a cell is not spent on them: the cell bands stand instead.
    const bool type = s.text_advance_px > 0 && s.text_line_px > 0;
    std::int64_t top = graphics::px_of_cells(kTopRows);
    std::int64_t band = graphics::px_of_cells(kBottomRows);
    if (type) {
        const std::int64_t typed_top = band_px_for(kTopRows, s.text_advance_px, s.text_line_px,
                                                   graphics::px_of_one_device(s.cell_px));
        const std::int64_t typed_band =
            band_px_for(kBottomRows, s.text_advance_px, s.text_line_px, 0);
        if (graphics::add_cells(typed_top, typed_band) <= s.h - graphics::kCellPx) {
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

static_assert(kMinScreen.room_w == kMinScreen.w,
              "the room IS the surface: nothing comes off its width");
static_assert(kMinScreen.room_h == 16 * graphics::kCellPx,
              "the workspace's documented default height");
static_assert(kMinScreen.notice_y == 18 * graphics::kCellPx, "the bottom band");
// THE MINIMUM COMPOSITION'S THREE REGIONS, WRITTEN OUT.
static_assert(kMinScreen.room_y == 2 * graphics::kCellPx,
              "where text is a cell, the top band owns rows 0 and 1");
static_assert(kMinScreen.room_y + kMinScreen.room_h == kMinScreen.notice_y,
              "the workspace's floor IS the bottom band's top -- no pixel between them, and "
              "none reserved twice");
static_assert(screen_of(kScreenMinW, kScreenMinH, 8, 18, graphics::kCellPx).room_y ==
                  2 * 18 + 2 * kTextInsetPx + 2,
              "where a face sets type, the top band is the Layouts pane's two rows of it, "
              "inside a boundary of one device pixel");
static_assert(screen_of(kScreenMinW, kScreenMinH, 8, 18, graphics::kCellPx).notice_y ==
                  kScreenMinH - (4 * 18 + 2 * kTextInsetPx),
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
        return graphics::px_span_contains(x, w, sx, grain) &&
               graphics::px_span_contains(y, h, sy, grain);
    }
};

/// A rectangle of whole cells: what a terminal shows of a pixel rectangle, and what a cell-counted
/// layout says before it becomes pixels. Workshop's own, distinct from the pixel rectangle.
struct CellRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;

    friend bool operator==(const CellRect&, const CellRect&) = default;

    /// Whether the cell is inside. Total over every value: an empty or inverted rectangle
    /// contains nothing, and the far edge is compared in unsigned arithmetic, so a rectangle whose
    /// right edge is past the int64 range answers without signed overflow.
    constexpr bool contains(std::int64_t cx, std::int64_t cy) const noexcept {
        if (w <= 0 || h <= 0 || cx < x || cy < y) {
            return false;
        }
        using U = std::uint64_t;
        return (static_cast<U>(cx) - static_cast<U>(x)) < static_cast<U>(w) &&
               (static_cast<U>(cy) - static_cast<U>(y)) < static_cast<U>(h);
    }
};

/// A cell rectangle in canvas pixels — exact, saturating, the one door a cell-counted layout
/// walks through on its way to being arrangement truth.
inline constexpr PixelRect pixels_of_cells(const CellRect& r) noexcept {
    return PixelRect{graphics::px_of_cells(r.x), graphics::px_of_cells(r.y),
                     graphics::px_of_cells(r.w), graphics::px_of_cells(r.h)};
}

/// THE CELLS A PIXEL RECTANGLE COVERS — the cell-grain quantization law as a rectangle:
/// [floor(left), floor(right)) per axis.
// WL-GEO-06 -- agents/workshop/geometry.md
inline constexpr CellRect cells_covered(const PixelRect& f) noexcept {
    const std::int64_t x0 = graphics::cell_of_pixel(f.x);
    const std::int64_t y0 = graphics::cell_of_pixel(f.y);
    if (f.w <= 0 || f.h <= 0) {
        return CellRect{x0, y0, 0, 0};
    }
    return CellRect{x0, y0, graphics::cell_of_pixel(graphics::add_cells(f.x, f.w)) - x0,
                    graphics::cell_of_pixel(graphics::add_cells(f.y, f.h)) - y0};
}

/// A pixel rectangle as a plane's `ScreenRect`; a negative extent draws as none.
inline constexpr ScreenRect wire_rect_of(const PixelRect& f,
                                         std::int64_t role) noexcept {
    return ScreenRect{f.x, f.y, f.w > 0 ? f.w : 0, f.h > 0 ? f.h : 0, role};
}

/// AN AUTHORED PLACE ON THE CANVAS: a place is measured from the room's top-left, which stands
/// at (0, `room_y`) -- directly under the top band, whatever height that band was fitted to.
// WL-PANE-11 -- agents/workshop/panes-and-windows.md
inline constexpr PixelRect canvas_of_room(PixelRect r, const Screen& sc) noexcept {
    r.y = graphics::add_cells(r.y, sc.room_y);
    return r;
}

/// ...AND A RECTANGLE ON THE CANVAS AS A PLACE IN THE ROOM, for what a hand or a readout measures.
// WL-PANE-11 -- agents/workshop/panes-and-windows.md
inline constexpr PixelRect room_of_canvas(PixelRect r, const Screen& sc) noexcept {
    r.y = graphics::add_cells(r.y, -sc.room_y);
    return r;
}

/// The part of a pixel rectangle this canvas has.
inline constexpr PixelRect clip_to_canvas_px(const PixelRect& r, const Screen& sc) noexcept {
    const std::int64_t x0 = r.x < 0 ? 0 : r.x;
    const std::int64_t y0 = r.y < 0 ? 0 : r.y;
    const std::int64_t x1 =
        graphics::add_cells(r.x, r.w) < sc.w ? graphics::add_cells(r.x, r.w) : sc.w;
    const std::int64_t y1 =
        graphics::add_cells(r.y, r.h) < sc.h ? graphics::add_cells(r.y, r.h) : sc.h;
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
    const std::int64_t n = slot >= static_cast<std::size_t>(kScreenMaxH)
                               ? kScreenMaxH
                               : static_cast<std::int64_t>(slot);
    // The width is the minimum plus half the room's surplus over it, floored so the odd pixel
    // stays the weaver's; `x + w < room_w` at every extent.
    const std::int64_t stack_w = graphics::px_of_cells(kStackW);
    return PixelRect{0, sc.room_y + n * graphics::px_of_cells(sc.stack_rows + kStackGap),
                     stack_w + (sc.room_w - stack_w) / 2, graphics::px_of_cells(sc.stack_rows)};
}

// ---- THE CHROME A PANE WEARS, AND THE INTERIOR IT LEAVES --------------------------------
// WL-CHROME-01, WL-CHROME-02, WL-CHROME-06 -- agents/workshop/chrome.md

/// The coarsest honest boundary, one every medium can show: one canvas cell, and the ceiling
/// `chrome_outer_of` reserves for a surface sized by its own content.
inline constexpr std::int64_t kChromeCells = 1;
inline constexpr std::int64_t kChromePx = graphics::px_of_cells(kChromeCells);

/// ONE UNIT OF THE ACTIVE FACE, in canvas pixels -- what this screen's chrome costs before the
/// interior's own presentation gets a say (`pane_inside` below is where it gets one).
inline constexpr std::int64_t chrome_grain(const Screen& sc) noexcept {
    return graphics::px_of_one_device(sc.cell_px);
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
    return PixelRect{graphics::add_cells(outer.x, chrome_px),
                    graphics::add_cells(outer.y, chrome_px), w, h};
}

/// Which prose column of a bounded text region a canvas pixel is on: subtract where the region's
/// text starts, floor by one character, with the `fit` its rows were drawn with and the device
/// unit (`grain`) of the medium the pixel came from. A projection, not a hit test: a negative
/// column or one past `columns` means "not on this region's prose". Under a cell fit a character
/// is a cell set from the region's edge as the medium drew it, floored to its unit: a window
/// counts from the edge's own pixel, and a terminal from the cell the edge floors to.
inline constexpr std::int64_t prose_column_of_pixel(std::int64_t px, const RegionFit& fit,
                                                    std::int64_t grain = graphics::kCellPx) noexcept {
    if (!fit.graphical()) {
        return graphics::floor_div_px(graphics::sub_px(px, graphics::floor_to_grain(fit.view.x, grain)), graphics::kCellPx);
    }
    return graphics::floor_div_px(graphics::sub_px(px, graphics::add_cells(fit.view.x, fit.origin_x)), fit.advance_px);
}

/// WHICH PROSE ROW OF A BOUNDED TEXT REGION A CANVAS PIXEL IS ON. The other axis
/// of `prose_column_of_pixel`, same rules, same non-answers.
inline constexpr std::int64_t prose_row_of_pixel(std::int64_t py, const RegionFit& fit,
                                                 std::int64_t grain = graphics::kCellPx) noexcept {
    if (!fit.graphical()) {
        return graphics::floor_div_px(graphics::sub_px(py, graphics::floor_to_grain(fit.view.y, grain)), graphics::kCellPx);
    }
    return graphics::floor_div_px(graphics::sub_px(py, graphics::add_cells(fit.view.y, fit.origin_y)), fit.line_px);
}

/// THE INTERIOR OF A PANE AND THE PRESENTATION IT GETS, RESOLVED TOGETHER.
// WL-CHROME-05 -- agents/workshop/chrome.md
struct PaneInside {
    PixelRect rect{};              ///< the interior: `outer` less the chrome on every side
    RegionFit fit{};     ///< ...resolved with the ACTIVE medium's own text metric
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

/// HOW MUCH ROOM ONE COARSE GROW GIVES A PANE, in canvas cells on both axes:
/// `workshop.arrange-step`, from `kMinArrangeStepCells` to `kMaxArrangeStepCells`; this is its
/// default.
// WL-ARR-10, WL-ARR-11 -- agents/workshop/arrangement.md
inline constexpr std::int64_t kCoarseStepCells = 4;
inline constexpr std::int64_t kMinArrangeStepCells = 2;
inline constexpr std::int64_t kMaxArrangeStepCells = 16;

static_assert(kMinArrangeStepCells <= kCoarseStepCells && kCoarseStepCells <= kMaxArrangeStepCells,
              "the default step is a value its setting takes");
static_assert(kStackRows + kCoarseStepCells - 2 * kChromeCells - 1 >= 8,
              "at the defaults, one coarse grow must give a stack pane's BODY the eight rows the "
              "tightest shipped form needs: the slot's rows, plus the step, less the chrome "
              "on both sides, less the pane's own title row");

// ---- THE CHROME'S VOICE: three roles, and the vocabulary is the one that exists ---------
// WL-CHROME-07, WL-CHROME-08 -- agents/workshop/chrome.md
inline constexpr std::int64_t kPaneChrome = graphics::role::kFill;
inline constexpr std::int64_t kPaneChromeSelected = graphics::role::kAccent;
inline constexpr std::int64_t kTransientChrome = graphics::role::kMuted;

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

/// EVERY OPEN PANE'S BOUNDS FROM ONE LAYOUT OF THE DESK: what `bounds_of` answers of each kind,
/// for a caller that asks it of many panes on the desk as it stands. It is the desk at the moment
/// it was laid out; a change to the panes, the setup or the screen is laid out anew.
struct DeskBounds {
    /// Each open kind with its bounds, the first open pane of a kind answering for it.
    std::vector<std::pair<std::int64_t, PaneBounds>> laid;
    /// What `bounds_of` answers of `kind` on the desk laid out: as laid, else not open.
    PaneBounds of(std::int64_t kind) const;
};

DeskBounds desk_bounds(const Panes& panes, const Setup& setup, const Screen& sc);

// The stack fits the SMALLEST screen this composition is honest on, which is where it is
// tightest.
// WL-GEO-04 -- agents/workshop/geometry.md
inline constexpr PixelRect kMinStack =
    placement_bounds(placement::kOverlayStack, 0, kMinScreen);

// The two places may meet: a pane covering a pane is what an overlay is for. What the
// half-share promises is that a slot never covers the whole room.
static_assert(kMinStack.x + kMinStack.w < kMinScreen.room_w,
              "a stacked pane leaves reachable workspace to its right at every extent -- "
              "which at the smallest screen it did NOT before: 48 of 48 left nothing, and 63 "
              "of 78 leaves fifteen");
// AND THE HALF-SHARE IS THE SAME ARITHMETIC IT WAS, over a bigger room.
static_assert(kMinStack.w == 63 * graphics::kCellPx,
              "48 + (78 - 48)/2 cells -- the half-share on the minimum screen, spelled out");
static_assert(placement_bounds(placement::kOverlayStack, 0, screen_of(kScreenMinW + 1, kScreenMinH))
                      .w == 63 * graphics::kCellPx,
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
                                : graphics::kCellPx;
    const auto column = graphical ? std::min(sc.text_advance_px, std::int64_t{8192})
                                  : graphics::kCellPx;
    const auto border = graphical ? chrome_grain(sc) + kTextInsetPx : kChromePx;
    const auto text_row = graphical ? line + 2 * kTextInsetPx : graphics::kCellPx;
    return StackCapacity{stack_slots_that_fit(sc), sc.room_h, sc.room_w, line, column, border,
                         graphics::px_of_cells(sc.stack_rows), graphics::px_of_cells(kStackGap),
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

/// A position on the canvas, in canvas pixels or in cells as the function that made it says. It
/// may be outside the canvas: a projection, not a hit test.
struct CanvasPoint {
    std::int64_t x = 0;
    std::int64_t y = 0;
    friend bool operator==(const CanvasPoint&, const CanvasPoint&) = default;
};

/// The canvas pixel a reported pointer position lands on, in pixels or a window's cells, the canvas
/// starting at the window's origin, and the cell that pixel is in -- or nothing, for a space this
/// application cannot place.
// WL-GEO-07 -- agents/workshop/geometry.md
struct PointedAt {
    bool understood = false;
    CanvasPoint cell;
    /// The same moment in canvas pixels, and the grain the reporting medium can honestly
    /// distinguish (one window pixel or one terminal cell).
    // WL-GEO-07 -- agents/workshop/geometry.md
    CanvasPoint px;
    std::int64_t grain = graphics::kCellGrainPx;
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
inline constexpr std::int64_t kPaneEdgeBandPx = graphics::kCellPx;

/// HOW NEAR A LINE AN EDGE THE HAND MOVES COMES TO MEET IT, in canvas pixels: under a cell, so a
/// terminal hand a cell away is not pulled across it, and at least half a cell, so a terminal hand
/// at the nearest cell meets a line that falls inside it. The reach is `workshop.snap-reach`, from
/// `kMinSnapReachPx` to `kMaxSnapReachPx`; this is its default.
// WL-ARR-17 -- agents/workshop/arrangement-snap.md
inline constexpr std::int64_t kPaneSnapReachPx = 8;
inline constexpr std::int64_t kMinSnapReachPx = 6;
inline constexpr std::int64_t kMaxSnapReachPx = 11;

static_assert(kMaxSnapReachPx < graphics::kCellPx &&
                  2 * kMinSnapReachPx >= graphics::kCellPx &&
                  kMinSnapReachPx <= kPaneSnapReachPx && kPaneSnapReachPx <= kMaxSnapReachPx,
              "every reach the setting takes is under a cell, so a terminal hand is never pulled a "
              "whole cell, and at least half one, so every line falls within reach of a terminal "
              "hand's cell");

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
/// only when the pane, the live desk or whether that desk holds the pane's row does. Session,
/// never persisted.
// WL-INFO-14 -- agents/workshop/info-body.md
struct InspectedPane {
    PaneRef ref;               ///< the pane an inspector asked for; an empty provider is "none"
    std::vector<Row> rows;     ///< the owner's rows over it (`pane_subject_rows`)
    std::int64_t name = 0;     ///< what the picture carries and a commit returns; 0 is unnamed
    std::int64_t minted = 0;   ///< the last name handed out; names are never handed out twice
    std::uint64_t desk = 0;    ///< `LiveDesk::put_live` when `name` was given
    bool member = false;       ///< whether that desk held the pane's row when `name` was given

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

/// WHICH LAYOUT TAB A REORDER DRAG IS CARRYING -- the third gesture record.
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
    // WL-SESSION-09 -- agents/workshop/window-file.md
    std::int64_t normal_w = kScreenMinW;
    std::int64_t normal_h = kScreenMinH;
    /// THE WINDOW'S DESKTOP PLACEMENT, AS LAST REPORTED.
    // WL-SESSION-08 -- agents/workshop/window-file.md
    bool placement_known = false;
    std::int64_t place_x = 0;
    std::int64_t place_y = 0;
    bool place_maximized = false;
    /// THE OS WINDOW'S OWN NORMAL ROOM, as the session file kept it: what a run in another window
    /// writes back; 0 by 0 where none was kept.
    // WL-SESSION-20 -- agents/workshop/window-file.md
    std::int64_t window_room_w = 0;
    std::int64_t window_room_h = 0;
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
    /// THE LIVE LAYOUT THIS SESSION IS SHOWING, as the store last gave it, and the one-line editor
    /// over its name (live_desk.hpp).
    // WL-LAYOUT-01 -- agents/workshop/layouts.md
    LiveDesk setup;
    /// AN INSPECTOR'S COMMIT OF A ROW, as the row's setter left it: the edit of the live layout it
    /// asks, or the setting it writes, for the commit's door to ask of the store.
    // WL-INFO-15 -- agents/workshop/info-body.md
    std::optional<LayoutEdit> pending_edit;
    /// WHICH SCOPE A WEAVER IS ARRANGING AND WHICH PANE THE VOCABULARY ADDRESSES.
    // WL-ARR-07 -- agents/workshop/arrangement.md
    PaneArrange arrange;
    /// ...and the pane gesture their pointer is holding, if any: its own record, so a release ends
    /// the gesture its press began.
    PaneGesture pane_drag;
    /// The inspector's subject (`InspectedPane`): session, never persisted.
    InspectedPane inspected;
    /// ...and what their LAST press on a LAYOUT TAB named, so the next one can be a
    /// double-click. See `TabClickMemory`.
    TabClickMemory tab_click;
    /// ...and the layout tab their pointer is dragging along the run, if any. The
    /// third gesture record, for the other two's own reason; see `LayoutTabDrag`.
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
    /// HOW THE BAND AND THE HOTKEYS VIEW EACH SPELL A KEY -- two presentation preferences, chosen
    /// apart; the manual a pane's `?` shows spells as the Hotkeys view does.
    // WL-KEY-16 -- agents/workshop/key-spelling.md
    KeySpelling band_keys = KeySpelling::kFull;
    KeySpelling hotkeys_keys = KeySpelling::kFull;
    /// WHETHER THE KEYBOARD'S SELECTION IS RINGED PLAIN -- one ring in the chrome's ink -- rather
    /// than in the accent's two: `workshop.focus.ring`.
    // WL-FOCUS-12 -- agents/workshop/focus.md
    bool selection_ring_plain = false;
    /// THE PAINTER SKIN WORKSHOP'S OWN AREA WEARS, and every pane's whose row names none:
    /// `workshop.skin`.
    std::string skin = kSkinClassic;
    /// WHETHER THE TERMINAL'S COMPLETION OFFERS A SHAPE'S REQUIRED FIELDS FIRST:
    /// `workshop.completion-order`.
    // WL-TERM-04 -- agents/workshop/terminal.md
    bool completion_required_first = false;
    /// The arrangement's measures as the weaver set them: the coarse step, the snap's reach and
    /// the stack's fallback height.
    // WL-ARR-10 -- agents/workshop/arrangement.md
    std::int64_t arrange_step_cells = kCoarseStepCells;
    // WL-ARR-17 -- agents/workshop/arrangement-snap.md
    std::int64_t snap_reach_px = kPaneSnapReachPx;
    // WL-GEO-03 -- agents/workshop/geometry.md
    std::int64_t stack_rows = kStackRows;
};

/// This session's screen furniture. The one call; see `Screen`.
// WL-GEO-02, WL-GEO-03, WL-GEO-04, WL-GEO-05 -- agents/workshop/geometry.md
inline constexpr Screen screen_of(const Session& s) noexcept {
    return screen_of(s.screen_w, s.screen_h, s.text_advance_px, s.text_line_px, s.cell_px,
                     s.stack_rows);
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

// ---- Moving the keyboard's selection -----------------------------------------------------

/// THE PANES THE KEYBOARD'S SELECTION WALKS, in reading order: every open pane that takes the
/// keys and stands on the canvas, top to bottom by its rectangle, then left to right, then front
/// to back in the authored order.
// WL-FOCUS-12 -- agents/workshop/focus.md
std::vector<std::int64_t> selectable_panes(const Panes& panes, const Setup& setup,
                                           const Screen& sc);

/// The pane `step` places after `from` in reading order, wrapping; from a pane not in it, the
/// first (a step forward) or the last. None while no pane is selectable.
// WL-FOCUS-12 -- agents/workshop/focus.md
std::optional<std::int64_t> cycle_pane(const Panes& panes, const Setup& setup, const Screen& sc,
                                       std::int64_t from, std::int64_t step);

/// THE NEAREST PANE BEYOND `from` IN A DIRECTION (`Act::kFocusLeft`, `kFocusRight`, `kFocusUp`,
/// `kFocusDown`): candidates whose centre lies past `from`'s edge, else past its centre where
/// panes overlap; the lowest gap along the direction plus twice the misalignment across it
/// wins, a tie the front-most. From a pane not selectable, the first in reading order; none when
/// nothing lies that way.
// WL-FOCUS-12 -- agents/workshop/focus.md
std::optional<std::int64_t> spatial_neighbour(const Panes& panes, const Setup& setup,
                                              const Screen& sc, std::int64_t from, Act dir);

// ---- Spelling the effective bindings -----------------------------------------------------

/// The effective gesture of one action, in the band's spelling (`^s` or `ctrl+s`). The one call
/// every claim site makes.
std::string hotkey_text(const Keymap& k, KeySpelling spelling, Act a);

/// Four direction actions said as one word when true: `arrows` while all four sit on their arrow
/// defaults, their own spellings otherwise.
std::string arrows_text(const Keymap& k, KeySpelling spelling, Act left, Act right, Act up,
                        Act down);

/// The `gesture label` pairs requestable in this context, in the order the band spends room on
/// them: the context's own rows (for `kPane`, the keyboard pane's declared ones), then what is
/// answered above the mode chain.
// WL-KEY-15 -- agents/workshop/keyboard.md
std::vector<std::string> help_pairs(const Keymap& k, KeySpelling spelling, KeyContext ctx,
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
std::vector<std::string> help_rows(const Keymap& k, KeySpelling spelling, KeyContext ctx,
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
/// an axis it changed, and each axis's room -- its span and the window's step -- so the door can
/// say it in its word. The hand's write and the desk a snap measures are made from the same axes.
struct PaneWindowAxes {
    PaneAxisProposal horizontal;
    PaneAxisProposal vertical;
};
// WL-ARR-17 -- agents/workshop/arrangement-snap.md
PaneWindowAxes pane_window_axes(const PaneWindowProposal& want, const PixelRect& base,
                                const Screen& sc);

/// The two axes of a window standing at `base` in the room, proposing nothing yet: each axis's
/// near edge, extent, the room's span along it and the window's step.
inline PaneWindowAxes pane_window_at(const PixelRect& base, const Screen& sc) {
    PaneWindowAxes out;
    out.horizontal.base = base.x;
    out.horizontal.length = base.w;
    out.horizontal.span = sc.room_w;
    out.horizontal.step = chrome_grain(sc);
    out.vertical.base = base.y;
    out.vertical.length = base.h;
    out.vertical.span = sc.room_h;
    out.vertical.step = chrome_grain(sc);
    return out;
}

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
/// edges for a resize -- comes to the nearest line within `reach_px`, the opposite edge held; a
/// snap that would leave the place or the extent outside a pane's rules is not taken.
SnappedWindow snap_pane_window(const PaneWindowProposal& want, std::int64_t edge,
                               const PaneSnapLines& lines,
                               std::int64_t reach_px = kPaneSnapReachPx) noexcept;

// ---- Where a pointer is, in workspace cells --------------------------------------------


/// WHERE A POINTER LANDED INSIDE A BOUNDED TEXT REGION, in that region's own prose.
// WL-PRESS-03 -- agents/workshop/press-chain.md
struct ProseAt {
    bool understood = false;
    std::int64_t column = 0;
    std::int64_t row = 0;
};

ProseAt prose_at(std::int64_t space, std::int64_t x, std::int64_t y,
                 const RegionFit& fit) noexcept;

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
void paint_pane_frame(ScreenPlane& layer, const PixelRect& b,
                              std::int64_t role);

/// THE POPUP'S OR A PANE'S BODY AS ONE BOUNDED REGION OF PROSE, RESOLVED ONCE.
// WL-CHROME-05 -- agents/workshop/chrome.md; WL-RGN-01 -- agents/workshop/regions.md
struct ProsePlace {
    bool present = false;
    std::int64_t rows = 0;    ///< prose rows of the ACTIVE medium's type that fit the rectangle
    std::int64_t columns = 0; ///< ...and how many characters fit across one of them
    /// The resolution itself, so a press inverse spends the fit the painter was handed.
    RegionFit fit{};
    /// The interior the fit was resolved for: the published region is the rectangle measured.
    PixelRect inside{};
    std::int64_t chrome_px = 0;
};

/// The one call, total over the rectangle: a closed pane answers with an empty one. Bounds fit
/// where they are.
ProsePlace prose_place(const PixelRect& b, const Screen& sc);

/// The region a `ProsePlace` was resolved for, empty and ready for its rows.
ScreenRegion prose_region(const ProsePlace& place);

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

/// ...judged from the rectangles of the panes in front of it, already laid out: what
/// `pane_is_covered` judges once it has them, for a caller that lays the whole desk out once.
bool rects_cover(const PixelRect& mine, const std::vector<PixelRect>& ahead);

/// THE ONE STATE CLASSIFIER. Asked of an inventory row -- which is the union of the catalog
/// and everything the setup names -- so every authored pane gets exactly one answer and no
/// row is silently omitted because the runtime catalog lacks it. A caller that only asks
/// whether a pane is on the screen at all passes `judge_cover` false, and a covered pane then
/// answers `open`, sparing the comparison against every pane in front of it.
std::int64_t pane_state_of(const Panes& panes, const Setup& setup, const Screen& sc,
                                  const CatalogRow& row, bool judge_cover = true,
                                  const PaneBounds* laid = nullptr);



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
    const std::int64_t bound = (std::numeric_limits<std::int64_t>::max)() / graphics::kCellPx;
    const std::int64_t a = amount > bound ? bound : (amount < -bound ? -bound : amount);
    if (cell_px <= 0) {
        return a * graphics::kCellPx;
    }
    const std::int64_t num = a * graphics::kCellPx;
    if (num < 0) {
        return -((-num + cell_px - 1) / cell_px);
    }
    return (num + cell_px - 1) / cell_px;
}

static_assert(px_of_device_amount(10, 0) == 10 * graphics::kCellPx,
              "on a cell medium a typed cell count is that many whole cells");
static_assert(px_of_device_amount(120, graphics::kCellPx) == 10 * graphics::kCellPx,
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

/// WHAT A WEAVER AUTHORED FOR ONE PANE'S WINDOW, each axis in its word, its numbers in the active
/// medium's own unit.
std::string pane_window_text(const SetupPane* row, std::int64_t cell_px);

/// IS ANY AXIS OF THIS PANE'S WINDOW SAID IN A WORD OTHER THAN PIXELS -- the code's answer, a
/// point of the room, a share, `fit` or `fill` -- so that its numbers alone do not say where it is?
bool pane_window_reads_words(const SetupPane* row);

/// WHAT A `fit` AXIS STANDS ON, where it is not the pane's latest offer: a pane that offered
/// nothing, or, in the OS window, an offer its latest is within a pixel of; empty otherwise.
std::string pane_fit_note(const Session& s, const PaneRef& ref);


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

void paint_context(ScreenPlane& layer, const Session& s, const Screen& sc);

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
// The popup's frame and place are this host's; the presenter participant draws its lines in an
// area of its own inside the frame (presenter_vocabulary.hpp), which this host places there.

/// THE MOST A PRESENTED MENU CAN SHOW AT AN ANCHOR on this screen -- the room `MenuGranted` hands
/// the presenter: the biggest popup that fits there, read as rows and columns of prose.
// WL-CTX-09 -- agents/workshop/pane-menu.md
ProsePlace presented_room(bool anchored, std::int64_t x, std::int64_t y, const Screen& sc);

/// WHERE THE PRESENTED MENU OPENS: beside its anchor, its frame sized to the lines the presenter
/// said it draws; empty while it has said none.
// WL-CTX-09 -- agents/workshop/pane-menu.md
PixelRect presented_bounds(const Session& s, const Screen& sc);

/// ...AND THE PRESENTER'S AREA FOR IT, inside that frame, in the desk's pixels; empty with it.
// WL-CTX-09 -- agents/workshop/pane-menu.md
PixelRect presented_area_place(const Session& s, const Screen& sc);

/// THE AREA A PRESENTED MENU STANDS IN, named for that menu alone.
std::string presented_menu_area(std::int64_t menu);

/// THE PRESENTED MENU'S FRAME, which the presenter's area stands inside.
// WL-CTX-09 -- agents/workshop/pane-menu.md
void paint_presented(ScreenPlane& layer, const Session& s, const Screen& sc);

/// WHETHER A PRESS THIS HOST HEARD LANDED ON THE PRESENTED MENU'S FRAME: inside the popup, where
/// the presenter's area does not reach.
// WL-CTX-09 -- agents/workshop/pane-menu.md
bool presented_holds(const Session& s, const Screen& sc, const PointedAt& at);

// ---- AN EXTERNAL PANE'S BODY: one header row of Workshop's, and a region ---------------

/// One header row, Workshop's own, so the provenance of what follows is legible.
// WL-FOCUS-11 -- agents/workshop/focus.md; WL-PANE-06 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t kExternalHeaderRows = 1;

/// How many header rows this pane's presentation reserves now: the one resolution of the title
/// preference, spent by the painter, the press path and the room grant alike.
std::int64_t external_title_rows(const Panes& panes, std::int64_t kind,
                                        bool titles_shown) noexcept;

/// WHAT A PANE'S FRAME SAYS IN ITS BODY, behind the pane's own area: `waiting for <name>` until the
/// pane draws there and its ground covers the words, or `<name> draws in no window` for a pane
/// whose office takes no room in a window.
// WL-PANE-16 -- agents/workshop/panes-and-windows.md
inline constexpr const char* kExternalWaiting = "waiting for ";
inline constexpr const char* kExternalNoWindow = " draws in no window";

/// The words a pane's frame says in its body (`kExternalWaiting`, `kExternalNoWindow`).
std::string external_body_words(const ExternalPane& pane, const std::string& name);

/// THE BODY OF AN EXTERNAL PANE, RESOLVED ONCE. Where it is, and how much prose the
/// ACTIVE medium fits in it.
struct ExternalBodyPlace {
    bool present = false;
    /// The body's bounds in canvas pixels, as the published region states them; `fit` is
    /// resolved from them.
    std::int64_t region_x = 0;
    std::int64_t region_y = 0;
    std::int64_t region_w = 0;
    std::int64_t region_h = 0;
    RegionFit fit{};
    /// The header rows this resolution reserved: the painter and the press path spend this number.
    std::int64_t header_rows = 0;
    std::int64_t rows = 0;    ///< prose rows under the header
    std::int64_t columns = 0; ///< ...and their columns
};

/// The body under an external pane's header row: the pane's whole bounds, less that row's
/// share of the PROSE the active medium fits in them.
ExternalBodyPlace external_body_place(const PixelRect& pane_rect, const Screen& sc,
                                             std::int64_t header_rows);

/// WHERE A PRESS LANDS IN AN EXTERNAL PANE'S BODY, as a row and column of the text the active
/// medium fits there: the place the pane reads a press in its area back to.
// WL-PRESS-04 -- agents/workshop/press-chain.md
struct ExternalPressAt {
    bool named = false;
    std::int64_t row = 0;    ///< a prose row of the BODY: 0 is the row under the header
    std::int64_t column = 0; ///< ...and the column of the pressed cell, the character it shows
};

/// LOCATE A PRESS IN A PANE'S BODY, from the rectangle the painter used.
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

/// A PANE'S `?` AND ITS GEAR, at the right end of its header row: the gear two cells before the
/// `?`, which takes the last cell. Printable ASCII, which every face draws, and no glyph a
/// terminal's border is drawn with.
// WL-CHROME-09 -- agents/workshop/chrome.md
inline constexpr const char* kChromeMarks = "= ?";
/// The fewest header columns that keep the title's mark and a few of its letters beside them.
inline constexpr std::int64_t kChromeMarksMinColumns = 12;

/// WHICH MARK A PRESS IN A HEADER ROW LANDED ON.
namespace chrome_mark {
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kManual = 1;   ///< the `?`
inline constexpr std::int64_t kSettings = 2; ///< the gear
} // namespace chrome_mark

/// THE HEADER ROW AS PAINTED, `prefix` before the title: the title fitted to `columns`, and the
/// pane's `?` and gear at its right end when it wears them and the row has room.
// WL-CHROME-09 -- agents/workshop/chrome.md
std::string external_header_row(const RuntimePane& row, bool typing, bool marks,
                                std::int64_t columns, const std::string& prefix = std::string());

/// WHICH OF A PANE'S MARKS A PRESS LANDED ON, from the place the painter used; `kNone` off them,
/// or on a pane that does not wear them.
// WL-CHROME-09 -- agents/workshop/chrome.md
std::int64_t chrome_mark_at(const Panes& panes, const Setup& setup, const Screen& sc,
                            std::int64_t kind, bool titles, std::int64_t space, std::int64_t x,
                            std::int64_t y);

/// ONE EXTERNAL PANE'S FRAME: Workshop's backdrop, Workshop's header, and in its body the words
/// that stand behind the pane's own area (`external_body_words`).
void paint_external(ScreenPlane& layer, const Panes& panes, std::int64_t kind,
                    const PixelRect& b, const Screen& sc, bool titles,
                    std::int64_t chrome = kPaneChrome);

// ---- The arrangement's affordance rings --------------------------------------------------

/// THE KEYBOARD'S SELECTION MADE VISIBLE: in command mode, brackets on the selected pane's corners
/// where text is a cell, rings outside its edge where an edge is finer, so a selection the keys
/// have not entered is told from the pane that has them.
// WL-FOCUS-12 -- agents/workshop/focus.md
void paint_selection_ring(ScreenPlane& layer, const Session& s, const Screen& sc);

/// THE AFFORDANCE RINGS ARE THE ARRANGEMENT STATE MADE VISIBLE.
void paint_pane_affordances(ScreenPlane& layer, const Session& s,
                                   const Screen& sc);

namespace detail {

/// ONE PLANE FOR ONE PRESENTATION — offered unconditionally, and taken back if that
/// presentation turns out to draw nothing.
// WL-FRONT-01 -- agents/workshop/planes.md
template <typename Paint>
inline void on_own_layer(ScreenCanvas& c, Paint&& paint_it) {
    c.layers.emplace_back();
    paint_it(c.layers.back());
    const ScreenPlane& drawn = c.layers.back();
    if (drawn.rects.empty() && drawn.labels.empty() && drawn.texts.empty()) {
        c.layers.pop_back();
    }
}

} // namespace detail

// ---- THE SETUP LINE: which arrangement this is, and whether it is written down -----------

/// What the one-line name editor puts before and after the name a weaver is typing. The
/// hint is spelled from the effective keymap, like every other gesture claim.
inline constexpr const char* kSetupNamePrompt = "layout name> ";
std::string setup_name_hint(const Keymap& k, KeySpelling spelling);

/// The two gestures the setup line advertises, on the line the thing they act on is on.
std::string setup_hints(const Keymap& k, KeySpelling spelling);

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

inline constexpr RegionFit band_fit(const Screen& sc) noexcept {
    const PixelRect b = band_bounds(sc);
    return fit_region(b.x, b.y, b.w, b.h, sc.text_advance_px, sc.text_line_px);
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
std::string setup_link_text(const LiveDesk& setup, std::int64_t path_columns);

/// WHAT THE ROW SAYS AFTER THE ASSOCIATION: the unresolved count, then the two gestures.
std::string setup_rest_text(const LiveDesk& setup, const Panes& panes,
                                   const Keymap& keymap, KeySpelling spelling);

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
std::string layout_tab_text(const LiveDesk& setup, std::size_t at);

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
LayoutTabRun layout_tab_run(const LiveDesk& setup, std::int64_t columns);

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
void paint_layouts(ScreenPlane& layer, const Session& s, const PixelRect& b,
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
/// own. A pane's settings are not among them: an inspector reads those of the layouts' owner.
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
void paint_panes(ScreenCanvas& c, const Session& s,
                         const Screen& sc);

/// THE BOTTOM BAND AS ONE PUBLISHED REGION: what the tool just said, and what the keys mean
/// right now.
ScreenRegion band_region(const Session& s, const Screen& sc);

/// The whole screen as one published canvas, an ordered list of planes: a pure projection of the
/// session and the screen.
ScreenCanvas paint(const Session& s);

// ---- THE DESK IN A GRAPHICS WINDOW: each pane's own area, and Workshop's behind them ----------

/// THE AREA OF WORKSHOP'S OWN IN FRONT OF EVERY PANE'S, where a graphics window shows the carried
/// value and the guide marks; it hears neither the keys nor the pointer, so a press on a mark
/// reaches what lies under it.
inline constexpr const char* kWorkshopOverlayArea = "zengine.workshop.overlay";

/// THE AREA A GRAPHICS WINDOW SHOWS AN OPEN PANE'S BODY IN, by the name its office draws there: an
/// office seating several panes names each `<office>.<pane>` (`PaneCanvasSeat::area_of`), any
/// other its office.
std::string pane_window_area(const Panes& panes, const RuntimePane& row);

/// WHETHER THE SESSION PUTS WORKSHOP'S OWN AREA IN FRONT OF EVERY PANE'S: while arranging or while a
/// menu is open, the surfaces a weaver's next press goes to. The weave adds a carry.
bool desk_in_front(const Session& s) noexcept;

/// THE AREA OF WORKSHOP'S OWN A GRAPHICS WINDOW SHOWS A PANE'S FRAME IN, at the pane's own depth:
/// its edge, its header and the words its body says, just behind the pane's own area. It is named
/// by the pane's reference, `zengine.workshop.frame.<office>.<pane>`, as the pane's own area is.
std::string pane_frame_area(const Panes& panes, std::int64_t kind);

/// WORKSHOP'S DESK AS A GRAPHICS WINDOW DRAWS IT: what stands about the panes -- the arrangement's
/// rings, the selection's, a menu, the band -- each pane's frame standing at its depth in an area
/// of its own (`workshop_frame_drawing`). Behind the panes or in front of them, while a weaver
/// arranges or a menu is open, it draws nothing of a pane, so every pane shows as it stands.
struct WorkshopWindowDrawing {
    graphics::Sketch sketch;
};

WorkshopWindowDrawing workshop_window_drawing(const Session& s);

/// ONE PANE'S FRAME AS A GRAPHICS WINDOW DRAWS IT in `pane_frame_area(panes, kind)`, placed over
/// the pane's bounds (`at`): its edge and header over its ground, and the words its body says
/// behind the pane's own area (`external_body_words`), as the desk draws them. Empty for a pane
/// the desk does not show.
struct WorkshopFrameDrawing {
    graphics::Sketch sketch;
    PixelRect at;
};

WorkshopFrameDrawing workshop_frame_drawing(const Session& s, std::int64_t kind);

/// THE PLANES A REPAINT LAYS ABOVE THE PICTURE -- the carried value, the guide marks -- as the
/// overlay a graphics window shows in front of every pane's area (`kWorkshopOverlayArea`).
graphics::Sketch workshop_overlay_drawing(const Session& s,
                                          const std::vector<ScreenPlane>& overlays);

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_HPP
