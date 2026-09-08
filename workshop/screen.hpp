// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SCREEN_HPP
#define ZENGINE_WORKSHOP_SCREEN_HPP

// The Workshop screen: the session facts, the maker's gestures over them, the
// inspector's rows, and the one function that turns all of it into a published
// canvas.
// Workshop law: agents/workshop/geometry.md (+24 registers; agents/workshop.md routes)

#include "attention.hpp" // what is true right now, held and dismissed
#include "attention_seam_vocabulary.hpp" // ...and how it crosses to the pane that shows it
#include "document_seam_vocabulary.hpp"  // ...and how the object document does
#include "terminal_seam_vocabulary.hpp"   // ...and how the terminal participant's record does
#include "complete.hpp"
#include "context.hpp" // what can be done with a pointed subject
#include "document.hpp"
#include "editor.hpp" // the source editor's buffer, byte law and tab geometry
#include "keymap.hpp"
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

// ---- The one screen's layout, in canvas cells ------------------------------------------

/// The smallest surface this screen is laid out on -- and, deliberately, the extent it uses
/// when nothing tells it otherwise.
// WL-GEO-02 -- agents/workshop/geometry.md
inline constexpr std::int64_t kScreenMinW = 78;
inline constexpr std::int64_t kScreenMinH = 22;

/// The largest surface this screen will lay out.
// WL-SESSION-07 -- agents/workshop/session-restore.md
inline constexpr std::int64_t kScreenMaxW = 640;
inline constexpr std::int64_t kScreenMaxH = 400;

/// THE ROWS RESERVED AT THE TOP OF THE SCREEN, and they are the first thing a maker reads.
// WL-GEO-03 -- agents/workshop/geometry.md; WL-FRONT-03 -- agents/workshop/planes.md
inline constexpr std::int64_t kTopRows = 2;

inline constexpr std::int64_t kWorkspaceX = 0; ///< the workspace's origin ON THE CANVAS...
inline constexpr std::int64_t kWorkspaceY = kTopRows; ///< ...under the top band, which owns row 0
inline constexpr std::int64_t kWorkspaceMinW = 12; ///< narrow enough to make a share visibly shrink

/// THE RIGHT COLUMN (`placement::kSideRegion`): a PLACE at the screen's right edge, fixed
/// width, anchored to the edge rather than to a column number -- AND RESERVING NOTHING. It
/// took 28 columns plus a two-cell gap off the room for as long as it was a reservation, held
/// empty whether or not a pane stood in them. The room runs underneath it whole now, and a
/// pane standing here covers room rather than owning it -- which is what a stacked panel has
/// always done to the room it is over.
// WL-GEO-03 -- agents/workshop/geometry.md
inline constexpr std::int64_t kPanelCols = 28;

/// The side region's top edge. ⭐ `kInfoHeadingRows` STOOD BESIDE IT AND IS RETIRED: it was
/// the row the `OBJECTS` heading kept, and the heading is `Zengine/info-pane/`'s now. Nothing
/// this host compiles knows what a pane in this place puts on its first row.
// WL-GEO-03 -- agents/workshop/geometry.md
inline constexpr std::int64_t kSideY = kWorkspaceY; ///< the region's top edge: the body's own

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

// ⭐ THE TERMINAL OVERLAY'S FURNITURE WAS HERE AND IS GONE (VD-24). `kTerminalWantW`,
// `kTerminalMinH`, `kTerminalChrome` and `kTerminalMinCols` sized one particular tool's
// rectangle out of the screen's own arithmetic -- the last built-in that had a place before
// a maker gave it one. A pane's rectangle comes from the arrangement a maker authored, like
// every other pane's, and its interior comes from the room Workshop grants it.

/// THE SCREEN'S FURNITURE, DERIVED IN ONE PLACE.
// WL-GEO-05 -- agents/workshop/geometry.md
struct Screen {
    std::int64_t w = kScreenMinW;  ///< the canvas extent this screen paints, in cells
    std::int64_t h = kScreenMinH;
    std::int64_t panel_x = 0;      ///< the right column's left edge: a place, not a reservation
    std::int64_t room_w = 0;       ///< the widest the workspace may be on this screen...
    std::int64_t room_h = 0;       ///< ...and the tallest
    std::int64_t notice_y = 0;
    std::int64_t help_y = 0;       ///< the first of two help lines
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
    // THE ROOM IS THE SURFACE. This line subtracted the right column and its gap for as long
    // as that column was reserved; nothing reserves it now. `panel_x` above is still the x a
    // pane placed at the right edge resolves to -- a PLACE -- and the room runs under it,
    // whole, so a maker who takes that pane off the desk gets thirty columns of workspace
    // back instead of thirty columns of nothing.
    //
    // ⚠ AND EVERY %-WIDE OBJECT RESOLVES AGAINST THE BIGGER NUMBER, once, at this version.
    // `workspace_w` follows `room_w` (screen_bindings.cpp `resize_screen`), so a 60% object on
    // a 160-column surface is 96 cells where it was 78. That is the move `the-reserved-column`
    // refused when Info became removable, and it is refused here for the same reason it was
    // then: what a share means may not depend on which panes are open. It does not depend on
    // that here -- the room is the surface at every moment, whatever stands on it.
    s.room_w = s.w;
    // THE HEIGHT STILL LOSES ITS BANDS, and the asymmetry is the point: the top and bottom
    // rows are chrome this screen paints itself, under no pane and coverable by none, while
    // the right column is a place panes are put in.
    s.room_h = s.h - kWorkspaceY - kBottomRows;
    // THE BOTTOM BAND'S FIRST TWO ROWS, DERIVED FROM ITS HEIGHT RATHER THAN COUNTED BACK
    // FROM THE SCREEN'S FOOT. They used to be `h - 4` and `h - 2` against a five-row
    // band whose first row was the setup line; the identity moved to the top band, so the
    // notice is the band's own first row and the legend follows it. Written as the band's
    // origin plus an offset, so a band that changes height cannot leave these two pointing
    // at rows it no longer owns.
    s.notice_y = s.h - kBottomRows;
    s.help_y = s.notice_y + 1;
    // ⚠ HD-10 IS OVER, AND THIS IS WHERE IT ENDED. The reservation retired in PR #21 was
    // doing two jobs -- "these columns are nobody's to spend" and "the terminal cannot
    // silently erase what stands there" -- and only the first was retired there, which left
    // the terminal overlay covering whatever stood at the right column with no boundary to
    // read it by. The founder was offered chrome, paint order or a ceiling and chose none of
    // them: the Terminal wears a pane's boundary BY CONSTRUCTION now, so there is no
    // mechanism to invent and no rectangle to reserve. The seven lines of arithmetic that
    // stood here -- want, height, corner, fit, the two floors and the row split -- are the
    // pane's own room, granted by `external_body_place` like every other pane's.
    //
    // AND THE FIT IS STILL RESOLVED HERE, because the screen carries the METRIC: it is the
    // one fact every region on this canvas measures itself with, and it arrives on the bus.
    const surface::RegionFit fit =
        surface::fit_region(0, 0, s.w, s.h, text_advance_px, text_line_px);
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

static_assert(kMinScreen.panel_x == 50, "the right column has not moved on the minimum screen");
static_assert(kMinScreen.room_w == kMinScreen.w,
              "the room IS the surface: nothing comes off its width. The 48 this read before "
              "was 78 less the right column's 28 and the two-cell gap beside it, and those "
              "thirty columns are the room's");
static_assert(kMinScreen.room_h == 16, "the workspace's documented default height");
static_assert(kMinScreen.notice_y == 18 && kMinScreen.help_y == 19, "the bottom band");
// THE MINIMUM COMPOSITION'S THREE REGIONS, WRITTEN OUT.
static_assert(kTopRows == 2 && kWorkspaceY == 2, "the top band owns rows 0 and 1");
static_assert(kWorkspaceY + kMinScreen.room_h == kMinScreen.h - kBottomRows,
              "the workspace's floor IS the bottom band's top -- no cell between them, and "
              "none reserved twice");


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
        // From the top of the workspace to its floor, against the right edge: the column OVER
        // the material rather than beside it, ending where the bottom band begins. The
        // rectangle is what it always was; what it no longer is, is subtracted from the room
        // it stands on.
        return ui::Rect{sc.panel_x, kSideY, kPanelCols, kWorkspaceY + sc.room_h - kSideY};
    }
    const std::int64_t n = slot >= static_cast<std::size_t>(kScreenMaxH)
                               ? kScreenMaxH
                               : static_cast<std::int64_t>(slot);
    // THE WIDTH IS THE MINIMUM PLUS HALF THE ROOM'S SURPLUS OVER IT, floored. The floor is
    // deliberate: rounding up would take the odd column from the maker. `room_w` is the
    // surface's own width and is never below `kScreenMinW`, which is thirty cells above
    // `kStackW`, so the subtraction is never negative and this needs no guard; the x is 0, so
    // `x + w < room_w` holds STRICTLY at every extent -- including the smallest, where the
    // slot used to be the whole room and left the maker nothing to its right.
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

// THE TWO PLACES MAY NOW MEET, and that is the decision rather than an oversight: the stack's
// half-share is measured against a room that no longer stops short of the right column, so on
// the smallest screen a slot runs to column 62 and the right column begins at 50. A panel
// covering a pane is what an overlay is for; what the half-share still promises is that a
// slot never covers the WHOLE room, and the line under this one is that promise.
static_assert(kMinStack.x + kMinStack.w < kMinScreen.room_w,
              "a stacked panel leaves reachable workspace to its right at every extent -- "
              "which at the smallest screen it did NOT before: 48 of 48 left nothing, and 63 "
              "of 78 leaves fifteen");
// AND THE HALF-SHARE IS THE SAME ARITHMETIC IT WAS, over a bigger room.
static_assert(kMinStack.w == kStackW + (kMinScreen.room_w - kStackW) / 2 && kMinStack.w == 63,
              "48 + (78 - 48)/2 -- the half-share on the minimum screen, spelled out");
static_assert(placement_bounds(placement::kOverlayStack, 0, screen_of(79, 22)).w == 63,
              "an odd surplus is FLOORED: 48 + (79 - 48)/2 is 63, not 64 -- the odd column "
              "stays the maker's");
static_assert(placement_bounds(placement::kOverlayStack, 0, screen_of(200, 60)).w == 124,
              "48 + (200 - 48)/2 -- the half-share, spelled out");
static_assert(placement_bounds(placement::kOverlayStack, 3, screen_of(200, 60)).w ==
                  placement_bounds(placement::kOverlayStack, 0, screen_of(200, 60)).w,
              "the width is a fact about the SCREEN, not about which slot a panel sits in");
static_assert(kMinStack.y + kMinStack.h <= kMinScreen.notice_y,
              "the stack's first slot stays clear of the notice line");
static_assert(kMinSide.x + kMinSide.w == kMinScreen.w,
              "the side region reaches the screen's right edge");
static_assert(kMinSide.y + kMinSide.h == kWorkspaceY + kMinScreen.room_h,
              "the side region ends where the workspace does, above the bottom band");
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
/// non-negative by construction (every `panel::k*` is, and `kFirstRuntimeKind` is 1024), so
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


// ⭐ `TerminalPane` WAS HERE AND IS GONE (VD-24). It held the overlay's open bit, the line
// being typed, the transcript snapshot, the completion list and the two flags that are not
// derived from either -- a whole tool's session state, inside the host's. Every field of it
// lives in `terminal-pane/` now; what crosses is a picture and two asks
// (`workshop/terminal_seam_vocabulary.hpp`).

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
// ⚠ `kTerminalLine` WAS 1 AND IS GONE (VD-24) -- the Terminal's line is a pane's now, and a
// pane sweeps its own selection out of the presses and motions it is already sent. The
// remaining values keep their numbers: they are a session's live routing and never durable,
// but renumbering them would be a diff nobody could read for no gain anybody could measure.
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

// ⭐ `reveal_place`, `Revealed` AND THE FOUR REVEAL FUNCTIONS LEFT WITH THE INFO PANEL. Reading
// past an ellipsis was one feature and it was Info's alone: a pointer resting on a truncated
// OBJECTS or PROPERTIES row scrolled that row under the hand. It needs the row's UNFITTED text,
// which is the pane's now -- what crosses the seam is rows already cut to the room the pane was
// granted. `screen_reveal.cpp` carries the whole argument where the code was, including why the
// pane protocol is not given a hover so this host could keep it (VD-22).


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
    // WL-SESSION-09 -- agents/workshop/session-restore.md
    std::int64_t normal_w = kScreenMinW;
    std::int64_t normal_h = kScreenMinH;
    /// THE WINDOW'S DESKTOP PLACEMENT, AS LAST REPORTED.
    // WL-SESSION-08 -- agents/workshop/session-restore.md
    bool placement_known = false;
    std::int64_t place_x = 0;
    std::int64_t place_y = 0;
    bool place_maximized = false;
    std::int64_t workspace_w = kWorkspaceW; ///< what a share of the workspace currently means
    std::int64_t workspace_h = kWorkspaceH;
    std::vector<Row> rows;    ///< the inspector, rebuilt when the selection changes
    Drag drag;                ///< a pointer drag in flight, if any
    /// THE LAST THING WORKSHOP HAD TO SAY, and that is all it is.
    // WL-ATTN-01 -- agents/workshop/attention.md
    std::string notice;
    bool notice_is_bad = false; ///< whether that thing was a refusal
    /// WHAT IS TRUE RIGHT NOW AND HAS NO LIVE OWNER TO DERIVE IT FROM (attention.hpp).
    // WL-ATTN-01 -- agents/workshop/attention.md
    HeldConditions conditions;
    /// THE CONTEXTUAL-ACTION SURFACE: open, the captured subject, the open group
    /// and a cursor -- an identity and a cursor, never a snapshot (context.hpp). Opening
    /// it changes no selection and no keyboard candidate; the subject it holds is spent
    /// through the owner operations at the moment a row is chosen, and nowhere else.
    ContextMenu context;
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
};

/// This session's screen furniture. The one call; see `Screen`.
// WL-GEO-02, WL-GEO-03, WL-GEO-04, WL-GEO-05 -- agents/workshop/geometry.md
inline constexpr Screen screen_of(const Session& s) noexcept {
    return screen_of(s.screen_w, s.screen_h, s.text_advance_px, s.text_line_px, s.cell_px);
}

/// DOES THE SOURCE EDITOR HAVE THE KEYBOARD RIGHT NOW?
bool editor_has_keyboard(const Session& s);

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
/// answered above the mode chain. For `kPane` the context's own rows are the keyboard
/// pane's declared ones (`pane`, its runtime handle; `kNoPaneKind` names no pane).
// WL-KEY-15 -- agents/workshop/keyboard.md
std::vector<std::string> help_pairs(const Keymap& k, KeyContext ctx,
                                    std::int64_t pane = kNoPaneKind);

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

/// Rebuild the inspector for the current selection. One gesture, so the running weave and the
/// suite cannot come to disagree about what a fresh inspector is -- and rebuilding rather than
/// patching is why nothing in this package has a "refresh the inspector" call.
///
/// ⭐ IT USED TO HAVE A TWIN AND A CURSOR. `refocus_keeping_draft` carried a live draft and the
/// maker's row across a rebuild, and `first_editable` chose where a fresh one landed; both were
/// about state the Info panel held in this host and the Info WEAVE holds now. What the host
/// still owns is the derived rows themselves, which are a fact about the selection.
void refocus(WorkshopDoc& d, Session& s);

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

/// How far a wrapped continuation row is indented, when the room can hold an indent at all.
inline constexpr std::int64_t kWrapIndent = 2;

/// Fit `text` into `width` cells, AND SAY SO when it did not fit.
std::string fit(std::string text, std::int64_t width);

/// HOW MUCH OF A PATH IS THE CUE THAT SAYS WHICH FILESYSTEM IT IS ON -- `/`, `C:/`,
/// `//server/`, or nothing at all for a spelling that has no root.
std::size_t path_root_cue(const std::string& p);

/// FIT A PATH, KEEPING THE END THAT SAYS WHICH FILE OR DIRECTORY IT IS.
std::string fit_path(const std::string& path, std::int64_t width);

// ---- Reading past the ellipsis -----------------------------------------------------------


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

/// The band's legend rows, as the legend projects them, budget-composed. `pane` is the
/// keyboard pane's handle when `ctx` is `kPane`, so its declared rows are packed too.
std::vector<std::string> help_rows(const Keymap& k, KeyContext ctx,
                                          std::int64_t width, std::size_t rows,
                                          std::int64_t pane = kNoPaneKind);

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
// WL-PRESS-03 -- agents/workshop/press-chain.md
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

// ⭐ THE PARTICIPANT'S RENDERER WAS DECLARED HERE AND IS GONE (VD-24).
// `terminal_address`, `terminal_shape`, `terminal_line`, `terminal_legend`,
// `terminal_wrapped`, `entries_that_fit` and `terminal_omission` turned one participant's
// record into rows a pane this wide could hold. Every one of them was PRESENTATION -- what
// a sentence says and how many rows it costs -- and they moved to `terminal-pane/` whole,
// where the pane that knows its own width can run them. What is left on this side is the
// derivation of the picture (`transcript_shown`) and the comparison that decides whether
// saying it again would be news (`same_transcript`).

// ---- The editable line, resolved ONCE ---------------------------------------------------

// ⭐ AND SO DID THE EDITABLE LINE'S PLACEMENT. `kTerminalPromptCols`, `kTerminalCaretCols`,
// `TerminalInputPlace`, `terminal_input_place`, `terminal_caret_column`,
// `terminal_caret_of_column`, `terminal_value_column`, `terminal_selection_columns` and
// `terminal_input_hit` were the ONE MEASURER of the overlay's input row -- the answer the
// painter wrote against, the press was located with and the caret was published at. A pane
// measures its own row: it is told its room and it decides which of its rows is the prompt.

/// THE VISIBLE SELECTION AS PROSE COLUMNS OF A ROW -- a component's own answer, shifted by
/// whatever the row puts in front of the text.
///
/// ⚠ IT WAS `TerminalSelectionSpan` AND THE TERMINAL IT WAS NAMED FOR IS GONE. Two consumers
/// remain and neither is a terminal: the Inspector's property draft
/// (`property_selection_columns`) and the Pane Editor's. The type was never the overlay's --
/// it is what any row with a prompt in front of its text answers -- so it kept its shape and
/// lost the name of the first thing that needed it.
// WL-TEXT-13 -- agents/workshop/text-box.md
struct TextSelectionSpan {
    std::int64_t begin = 0;
    std::int64_t end = 0;
    bool present = false;
};

// ---- The completion list, inside the pane it belongs to ---------------------------------

// ⭐ AND THE COMPLETION LIST'S PLACEMENT WENT WITH THEM. `CompletionPlace`,
// `kCompletionMinRows`, `completion_place`, `completion_first_shown`, `completion_rows` and
// `paint_terminal` put a SECOND bounded region on top of the pane's own -- which is the one
// thing the pane protocol cannot express: a pane publishes ONE list of rows
// (`PaneContent`), and Workshop assembles ONE region from it. So the list is rows INSIDE the
// pane now, above the input row it belongs to, and it takes room from the transcript rather
// than covering it. A maker sees the difference; it is named in the register.

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

/// THE COMPACT LINE, or empty when nothing currently deserves attention.
///
/// ⚠ IT SPENDS EVERY CURRENT CONDITION, NOT THE UNDISMISSED ONES, and that is a change the
/// migration made rather than a simplification. `attention_shown` was the one population the
/// chip, the view, the cursor and the dismissal all agreed on, and it could be, because one
/// party held all four. The dismissal set is the Attention PANE's now (`attention-pane/`,
/// `AttentionPaneState::dismissed`) and this host has no way to learn it that would not be a
/// second sentence across the seam. So the chip says what is TRUE and the pane says what
/// this maker has chosen to look at -- which is also the honest reading of WL-ATTN-08's own
/// sentence, that dismiss is not resolve and changes nothing that is true.
std::string attention_compact(const std::vector<Condition>& shown);

/// EVERY CURRENT CONDITION AS THE SENTENCE THAT CROSSES THE PANE SEAM.
///
/// The four content fields are the condition's own; the fifth is the ACTION, resolved here
/// into the words a maker reads, because resolving it needs the effective keymap and the
/// keymap is this host's. A condition that names no action carries an empty suggestion, and
/// one that names an action nothing in the catalog answers to carries one too -- the same
/// silence the built-in's painter kept for the same case.
std::vector<StandingCondition> standing_conditions(const Session& s,
                                                   const ProjectFrontier& frontier = {});

/// ARE THESE THE SAME SENTENCES? Field by field, in order, because the order is part of
/// what is being said (WL-ATTN-07). What this answers is "is there news", and the caller
/// stays silent when there is not.
bool same_conditions(const std::vector<StandingCondition>& a,
                     const std::vector<StandingCondition>& b);

/// THE OBJECT DOCUMENT AS THE SENTENCE THAT CROSSES THE PANE SEAM: the object rows, the
/// selection, and the inspector rows of the selected object. Derived from the document and
/// the session exactly as the built-in's painter derived them, and owning nothing.
// WL-DOC-20 -- agents/workshop/document.md
DocumentShown document_shown(const WorkshopDoc& d, const Session& s);

/// THE PARTICIPANT'S RECORD, AS THE SEAM CARRIES IT -- `document_shown`'s exact shape one
/// owner over: derived at the moment of the ask, holding no `loom::TerminalSession`, no
/// `Transcript` and no entry the renderer could not read.
TranscriptShown transcript_shown(const loom::TerminalSession* me);

/// IS THIS THE SAME PICTURE? Field by field, in order, for `same_conditions`' reason.
// WL-DOC-20 -- agents/workshop/document.md
bool same_document(const DocumentShown& a, const DocumentShown& b);

/// ...and the comparison that decides whether saying it again would be news.
bool same_transcript(const TranscriptShown& a, const TranscriptShown& b);

/// `LineSlot` AS THE SEAM SPELLS IT.
const char* slot_name(LineSlot slot) noexcept;

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
/// ⭐ THE PANE MANAGER'S NOW. They were the Info panel's row composition and the Pane Manager
/// borrowed them; the Info pane carries its own copies in its own image, and what is left
/// here has one consumer.
// WL-TEXT-13 -- agents/workshop/text-box.md
inline constexpr std::int64_t kPropertyMarkCols = 1;
inline constexpr std::int64_t kPropertyLabelCols = 9;

/// THE COLUMN THE INSERTION POINT SITS IN, kept out of the value's own budget.
// WL-TEXT-13 -- agents/workshop/text-box.md
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

/// IS A DRAFT LIVE ON `Session::rows`? The rows are the Pane Manager's since the Info panel
/// left, so this is the question a contextual delete asks before it destroys unfinished work.
// WL-PED-07 -- agents/workshop/pane-manager.md
bool draft_live(const Session& s);

// ⭐ THE INFO PANEL'S PRESENTATION AND ITS CONTROLS ARE DECLARED NOWHERE NOW, BECAUSE THEY ARE
// NOT THIS HOST'S. `action_row_text`, `InfoBodyPlace`, `inspector_focus`, the four
// `info_body_place` overloads, `InfoBodyAt`, `info_body_at`, the six prose-row mappers, the
// three press helpers, the object row texts and `paint_info` stood here; so did the footer's
// whole vocabulary -- `kActionCreate`, `kActionDelete`, `kActionCount`, `kActionRows`,
// `kNoAction`, `Availability`, `available`, both `action_availability` overloads and
// `action_label` -- and `kInfoBodyMinRows` with them. They are `Zengine/info-pane/pane.cpp`'s
// composition now, and the rows it makes of it cross as `PaneContent` like every other pane's.
//
// ⚠ THE CONTROLS WERE KEPT ONE STAGE LONGER THAN THE PAINTER AND ARE DELETED HERE. Nothing in
// this host called them after `paint_info` left: the availability rule is the pane's, made
// against facts the pane holds (its own draft, the picture it was shown), and a second copy
// in the host would be a second answer to a question the host is no longer asked.

/// WHAT A LIST ASKS THE BODY FOR: one row per member, and never zero.
// WL-INFO-07 -- agents/workshop/info-body.md
inline constexpr std::size_t list_demand(std::size_t members) noexcept {
    return members == 0 ? 1 : members;
}

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

/// Where the cursor belongs on a freshly built row list: the first row a maker can actually
/// author. Landing it on a read-only row instead would open onto a row whose only possible
/// answer to "edit this" is a refusal. The Info panel spent it first; the Pane Manager spends
/// it now, on its own fields.
std::size_t first_editable(const std::vector<Row>& rows);

/// A PRESSED COLUMN AS A COLUMN OF THE VALUE. Negative to the left of the value, which
/// `TextBox::position_at_column` reads as "the start of what is shown".
inline constexpr std::int64_t property_value_column(std::int64_t row_column) noexcept {
    return row_column - (kPropertyMarkCols + kPropertyLabelCols);
}

/// THE DRAFT'S VISIBLE SELECTION AS PROSE COLUMNS OF ITS BODY ROW —
/// `property_caret_column`'s shape for a span.
TextSelectionSpan property_selection_columns(const Row& row,
                                                        std::int64_t value_columns);

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

/// ONE COLUMN OF EVERY BODY ROW THE TEXT MAY NOT USE -- the caret's own column, for
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
/// member of the once-per-repaint reconcile family (`refresh_editor`'s argument, two
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

// ---- A cursor-windowed list's wheel ------------------------------------------------------

/// How many rows the wheel is worth in a cursor-windowed list -- the editor's number, for
/// its reason. Spent by the Editor, the Pane Manager and the picker; the browser that
/// introduced it takes its own copy across the seam now.
// WL-EDIT-10 -- agents/workshop/editor.md
inline constexpr std::int64_t kListWheelRows = 3;

/// TURN NOTCHES INTO WHOLE ROWS, CARRYING THE FRACTION.
std::int64_t spend_wheel(double& accum, double dy, std::int64_t rows_per_notch);

// ---- The arrangement's affordance rings --------------------------------------------------
//
// ⭐ THE SECTION ABOVE THIS ONE WAS "WHICH REVEALABLE ROW THE POINTER IS ON" AND IS GONE with
// the reveal (see the retirement note in `agents/workshop/pointer.md`). Its doc comment and the
// browser's, which left a phase earlier, were spliced together by the deletion; this is the
// comment the surviving declaration always had.

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

/// Prose rows the `PANES` heading keeps -- what `kInfoHeadingRows` was, one pane over.
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

// ---- THE COMPOSITION: every pane back to front, the bottom band, and the screen as planes ----

/// EVERY PRESENTED PANE, BACK TO FRONT — ONE COMPLETE LAYER EACH.
/// ⚠ IT NO LONGER TAKES THE FRONTIER. The parameter existed for one caller inside it -- the
/// current-condition view's painter, which read the frontier to derive one of its rows -- and
/// that view is a pane now, told what is true rather than working it out. Nothing else in the
/// composition ever looked at it.
void paint_panels(surface::SurfaceCanvas& c, const Session& s,
                         const Screen& sc);

/// THE BOTTOM BAND AS ONE PUBLISHED REGION: what the tool just said, and what the keys mean
/// right now.
surface::SurfaceTextRegion band_region(const Session& s, const Screen& sc);

/// The whole screen as one published canvas — an ORDERED LIST OF PLANES.
///
/// ⚠ AND IT NO LONGER TAKES THE FRONTIER EITHER. It was handed one so that it could hand one
/// to `paint_panels`, which handed it to the current-condition view's painter. That view is a
/// pane now; the picture is a pure projection of the document, the session and the screen,
/// and nothing in it derives a fact from the realization owner any more.
surface::SurfaceCanvas paint(const WorkshopDoc& d, const Session& s);

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_HPP
