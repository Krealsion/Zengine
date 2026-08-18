// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SCREEN_HPP
#define ZENGINE_WORKSHOP_SCREEN_HPP

// The Workshop screen: the session facts, the maker's gestures over them, the
// inspector's rows, and the one function that turns all of it into a published
// canvas.
//
// THE GESTURES LIVE HERE, not in the weave, and on purpose: a gesture whose
// only witness is a keystroke is a gesture no suite can pin. `workshop.cpp`
// binds keys and pointer events to these functions and does nothing else with
// the document, so what the suite drives is what a maker's hand drives.
//
// PURE, and that is the point of it being its own header: paint() takes a
// document and a session and returns a SurfaceCanvas. No terminal, no window, no
// weave, no bus. So the suite pins an entire Workshop screen -- the rectangle,
// the selection ring, the object list, the inspector, a refusal -- as a value it
// can assert on, and the only thing left for the live run to prove is that the
// bus and the Skin carry it, which is a different claim from "the screen is
// right".
//
// THE AUTHORED MATERIAL IS RESOLVED IN EXACTLY ONE PLACE, and that place is not
// this file. `workspace_scene()` below builds the viewport and calls
// `ui::resolve` once; the canvas, the inspector's resolved reading and the hit
// test all read the Scene it returns. Three separate call sites doing their own
// extent arithmetic agree only because one person wrote all three -- which is a
// coincidence, not a guarantee.
//
// The SCREEN'S OWN FURNITURE is still constants, and that is a different thing
// from the authored material and stays a Workshop decision: where the object
// list sits beside the workspace is this application's composition, not
// something a maker authors and not something a package should decide for it.
// SurfaceCanvas paints a picture; whoever publishes one has already decided what
// the picture is. A relational layout engine (stacks, weights, "beside") is the
// Loom's loom::Widget + px_layout, a different model that stays where it is --
// the two are not competitors, and neither replaces the other
// (README.md#ui--the-authoredresolved-vocabulary).

#include "complete.hpp"
#include "document.hpp"
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

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace zengine::workshop {

// ---- The one screen's layout, in canvas cells ------------------------------------------
//
// THE SCREEN'S EXTENT IS RUNTIME, AND EVERYTHING ELSE HERE IS EITHER FIXED OR DERIVED FROM
// IT. Until G-2 it was one pair of constants, because a canvas publisher had no way to learn
// how much room its medium had; the Surface package now says so (`surface::SurfaceExtent`),
// so a larger window is a larger Workshop rather than a larger copy of one.
//
// WHAT THE EXTRA ROOM IS SPENT ON is this application's composition and nobody else's, and
// the WORKSPACE takes the extra columns and rows. Two overlays then take a HALF-SHARE of the
// surplus back out of the room: the terminal pane while it is open (G-2, bounded by HD-10),
// and an overlay-stack slot whenever one is (WIND-1). Both are the same sentence about the
// same surplus -- every two columns the surface gains are one column of workspace and one
// column of overlay -- and neither ever takes a whole row. The panel column beside the
// workspace keeps its width, the inspector keeps its rows, and the bottom band keeps its
// shape -- their sizes are decisions about how much of each thing is worth showing, not
// shares of a screen, and turning them into shares would be inventing a layout policy this
// phase has no evidence for.
//
// EVERY CONSTANT BELOW THAT SURVIVED IS ONE OF TWO KINDS: a MINIMUM (the smallest surface
// this composition is honest on, and the base a half-share is measured from), or a FIXED
// SIZE (something whose right size does not depend on how much room there is). `Screen`
// holds what is derived, `screen_of` is the one place the derivation happens, and the
// static_asserts underneath it pin that the minimum screen is byte-for-byte the 78x22
// composition that existed before this phase.

/// The smallest surface this screen is laid out on -- and, deliberately, the extent it uses
/// when nothing tells it otherwise. It is not a taste: at 78x22 every piece of furniture
/// below is still on the canvas with the workspace at its documented 48x16, which is the
/// composition WT-0 measured as fully spoken for. A medium that offers less is not refused;
/// its publisher simply keeps painting this, and the medium clips, which is what a terminal
/// too small for its output has always done.
inline constexpr std::int64_t kScreenMinW = 78;
inline constexpr std::int64_t kScreenMinH = 22;

/// The largest surface this screen will lay out, and the reason is arithmetic rather than
/// taste: an extent arrives from a MEDIUM, over the bus, as a `ZEN_SHAPE` whose fields are
/// whatever the sender put in them. `paint` allocates nothing, but the terminal Skin's
/// rasterizer allocates `w * h` cells from the canvas it is handed, so a published extent of
/// 10^18 is a multiply that leaves the number line before anything is drawn. This is W-1's
/// lesson at the other end of the same telescope: a value that used to be a constant
/// somebody chose is now an input, so it needs a total function rather than a comment.
/// 640x400 cells is a 7680x4800-pixel window in the graphical medium -- larger than any
/// display this runs on -- and 256,000 cells, which every consumer of it handles in
/// microseconds.
inline constexpr std::int64_t kScreenMaxW = 640;
inline constexpr std::int64_t kScreenMaxH = 400;

inline constexpr std::int64_t kWorkspaceX = 0; ///< the workspace's origin ON THE CANVAS...
inline constexpr std::int64_t kWorkspaceY = 1; ///< ...which authored coordinates are relative to
inline constexpr std::int64_t kWorkspaceMinW = 12; ///< narrow enough to make a share visibly shrink

/// THE SIDE REGION (`placement::kSideRegion`): the column beside the workspace, FIXED, and
/// anchored to the right edge rather than to a column number. Its width is how much of an
/// object's name and an inspector row is worth showing, which is a fact about the rows and
/// not about the screen -- so a wider surface gives the workspace more room and gives this
/// exactly as much as it had.
///
/// THE SCREEN RESERVES IT; THE PLACEMENT PATH SPENDS IT. `screen_of` below turns this width
/// into `Screen::panel_x`, which exists whether or not any panel is in the column and is
/// what the workspace measures itself against; `placement_bounds` turns that reservation into
/// the rectangle the panel placed here occupies. Two steps rather than one, because they
/// answer different questions -- how much room the workspace has is a fact about the SCREEN
/// and must not depend on what a maker has open, and that is exactly why hiding Info moves
/// nothing.
///
/// IT IS RESERVED WHETHER OR NOT INFO IS OPEN (PNL-0), and that is a decision rather than an
/// oversight. Removing the Info panel leaves these 28 columns empty, and empty they stay.
///
/// The alternative -- giving the workspace the vacated room -- is not a layout improvement
/// that this phase declined for tidiness; it is a change to WHAT THE DOCUMENT LOOKS LIKE.
/// The workspace's extent is what a share resolves against, so a workspace that grew when a
/// panel closed would make every `%`-width object on the screen change size because a maker
/// removed a list of names. A panel's presence must not be visible in the picture of the
/// document. That is the same rule the authored/resolved split has been enforcing since W-1,
/// arriving from a new direction, and it is why "the space is simply unused" is the truthful
/// answer here rather than the lazy one.
inline constexpr std::int64_t kPanelCols = 28;
inline constexpr std::int64_t kPanelGap = 2; ///< cells between the workspace's edge and it

/// The rows INSIDE the side region's bounds. There used to be four of them and now there
/// are two, which is the measurable half of HD-7: `kListRows = 5` and `kRowsY = 8` were a
/// fixed OBJECTS height and a fixed PROPERTIES origin, and both are gone. How many objects
/// the panel shows and where the inspector begins under them are answers about the ROOM the
/// active medium reports, resolved by `info_body_place`, and a constant cannot hold either.
///
/// WHAT SURVIVES IS WHERE THE PANEL'S OWN CHROME IS. `kSideY` is the region's top edge and
/// `kInfoBodyY` is the first row under the `OBJECTS` heading -- the heading stays an ordinary
/// label on the panel's row 0 because that row is SHARED with the screen's own
/// `shift+space terminal` hint, which is not this panel's to cover. Everything below it
/// belongs to one bounded region.
inline constexpr std::int64_t kSideY = 0; ///< the region's top edge: the canvas's own
inline constexpr std::int64_t kInfoBodyY = 1; ///< the body's first row, under `OBJECTS`

/// The band under the workspace: the setup line, the notice, a spare row, and the two help
/// lines. FIXED for the same reason the panel is -- it holds a known number of sentences.
/// (The first row was spare until WS-0 gave it the setup line.)
inline constexpr std::int64_t kBottomRows = 5;

/// THE NOTICE'S OWN ROOM, IN CELLS: its row and the spare one beneath it (TYPE-0).
///
/// TWO, and the number is a measurement rather than a margin. The notice is one sentence and
/// wants one row -- but a bounded region is set in the ACTIVE medium's type, and a face whose
/// line is 18 device pixels does not fit in a 12-pixel cell: `fit_region` answers zero rows
/// for a one-cell region and hands it back to the cell projection (HD-5). Two cells is the
/// smallest room that holds one row of this repository's face, and the second of them is the
/// spare row `kBottomRows` already reserves and nothing else paints. Nothing moved to make it.
inline constexpr std::int64_t kNoticeRows = 2;

// ---- THE OVERLAY STACK (`placement::kOverlayStack`) -----------------------------------------
//
// THE SIMPLEST TRUTHFUL PLACEMENT THIS GEOMETRY SUPPORTS, and it is not a good one. A panel
// placed here is an OVERLAY anchored to the canvas's top-left corner, drawn after the
// workspace and over it, stacked downwards if there is ever more than one. The terminal
// overlay's mechanism exactly (a backdrop rect, then rows padded to the pane's full width
// so a character medium's spaces erase what is underneath), pointed at the other corner.
//
// WHY AN OVERLAY RATHER THAN THE COLUMN. The side region is spoken for: it holds OBJECTS at
// a fixed five rows and PROPERTIES immediately under it at a height that CHANGES with the
// selection, so "below PROPERTIES" is a row number that moves when a maker selects a
// different object -- a panel whose top edge slides is worse than one that covers something.
// So the honest remaining choice is to put it over the workspace and say so. That is also
// why the side region holds exactly one panel and panel.hpp asserts it.
//
// PNL-0 DID NOT MOVE THE BUILDER HERE-OR-THERE BY WHAT ELSE IS OPEN, and PNL-1 has not
// either. A Builder that took the column when Info was absent would be a panel whose PLACE
// depended on which other panels were open -- a layout policy, and a fiddly one, arriving as
// a side effect. A place is a fact about a KIND (panel.hpp's catalog); what varies at
// runtime is only which slot of this stack an open panel is in.
//
// AND IT IS AWKWARD ON PURPOSE-ADJACENT GROUNDS: it covers the material a maker is
// building. That awkwardness is the evidence rather than the embarrassment -- inventing
// docking would be answering a demand ahead of anybody feeling it. What using it actually
// felt like is in the reports.
//
// A WIDER ROOM IS SHARED BY THE PANE AND THE MAKER (WIND-1). The width below is the
// MINIMUM and no longer the whole answer: `placement_bounds` resolves a slot to
// `kStackW + (room_w - kStackW)/2`, so the surplus a bigger surface gives the workspace is
// split evenly between the panel and the material underneath it. It is the terminal pane's
// own rule (`pane_want`, three lines into `screen_of`) pointed at the other corner, and the
// two bases are the same number -- `kMinStack.x + kMinStack.w == kMinScreen.room_w` has been
// asserted under `placement_bounds` since PNL-1. What it buys is measured: at 200x60 an
// external pane's granted columns go from 48 to 109 without a threshold. What it costs is
// that every added cell is the panel's for PAINT and for the POINTER both, so the honest
// half of the sentence is that the maker keeps the OTHER half -- 1, 9, 21, 61 and 281 free
// columns of the panel's own rows at 79, 96, 120, 200 and 640 columns of surface. A full
// width would leave zero at every extent, which is the rule this one was chosen against.
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
/// preference (PNL-0). A picker three rows tall over a panel nine rows tall left the panel's
/// last six rows showing underneath it, and in a character medium there is no edge between
/// them: the graphical Workshop read `Info  closed  objects and properties` and then, on the
/// next line and in the same box, `exit  --  asks 0 ever`. One panel, saying two unrelated
/// things. Covering the whole slot costs some blank rows while a question is open and buys a
/// screen that cannot be misread; leaving a gap and marking its edge would be a second
/// overlay convention for the same job.
inline constexpr std::int64_t kPickerRows = 1 + static_cast<std::int64_t>(kPanelKinds);

// ---- The terminal overlay's own furniture, in canvas cells -----------------------------
//
// ANCHORED TO THE BOTTOM-RIGHT CORNER OF THE ROOM, AND THE ROOM IS NOT THE SCREEN (HD-10).
// Its bottom edge IS the screen's; its right edge is the WORKSPACE'S right edge -- the same
// `room_w` the workspace itself is measured against, three lines up in `screen_of`. Nothing
// is rearranged to make room for it: it is an OVERLAY, drawn last, and while it is open it
// covers the furniture underneath rather than pushing it aside. What it may not cover is the
// reserved column beside the workspace, because that column is reserved BY THE SCREEN and not
// by whoever happens to be standing in it.
//
// UNTIL HD-10 ITS RIGHT EDGE WAS THE SCREEN'S, and that was the one placement in this file
// that spent room `screen_of` had already reserved for something else. The cost was measured
// and it was not cosmetic: at EVERY extent this composition lays out, the pane covered the
// full 28-column width of the side region and between 8 and 37 of its rows -- so the Info
// panel published its object list, its properties and its `[ Create ]` / `[ Delete ]` footer,
// and a later region erased them in canvas-coloured ground at the same moment. A maker
// reading the result could not tell whether those rows had been omitted, hidden or destroyed.
// The other place with a rectangle -- the overlay stack -- never had that fault, because
// `kMinStack.x + kMinStack.w == kMinScreen.room_w` is asserted under `placement_bounds` and
// has been since PNL-1. HD-10 did not invent that law; it brought the one presentation older
// than `placement_bounds` under it.
//
// WHAT G-2 CHANGED is that there is now sometimes more room, and the pane takes HALF of it.
// Half, because the two things competing for a bigger surface are the workspace a maker is
// building in and the record they are reading, and neither deserves all of it: every two
// columns the surface gains are one column of workspace and one column of pane. HD-10 bounds
// that appetite by the room rather than repealing it -- the want is the same arithmetic, the
// room is the ceiling, and the two agree from 94 columns up, so the ceiling binds only on the
// narrowest screens, which are exactly the ones with no half to take. The alternative rules
// were both worse -- a FIXED pane wastes the room the phase exists to make usable, and a pane
// that keeps its gutters swallows a growing share of the screen until it is the tool.
//
// The old honest limit here -- "a canvas has no notion of the medium's size, and giving it
// one is a Surface question, not a Workshop one" -- was answered rather than removed. The
// Surface package now says it (`surface::SurfaceExtent`), Workshop hears it, and this
// arithmetic is what a Workshop screen makes of the answer.

/// THE WIDTH THE PANE ASKS FOR at the smallest screen, before the room answers. It is a WANT
/// and not a floor, and the name says so because the value cannot: at the three narrowest
/// extents this composition lays out (78, 79 and 80 columns) the room is smaller than this
/// and the pane gets the room. `kTerminalMinH` beside it IS a floor -- nothing bounds the
/// pane vertically, because the reservation this phase is about is horizontal.
inline constexpr std::int64_t kTerminalWantW = 56;
inline constexpr std::int64_t kTerminalMinH = 13;
/// Header, the standing statement, the omission marker, the input line: the rows a pane
/// spends on being a pane, whatever is in it. Everything else is transcript.
inline constexpr std::int64_t kTerminalChrome = 4;
/// The narrowest prose the pane will claim to hold, whatever a medium's metric says.
///
/// The metric arrives on the bus, so an advance of a million pixels is a sentence a
/// publisher can say; a pane that answered "zero columns" to it would hand `wrap` a width of
/// zero and `fit` a length of zero, and the record would vanish rather than degrade. Eight
/// is enough for `> abc_` and the sigils, which is the smallest thing this pane is still
/// FOR.
inline constexpr std::int64_t kTerminalMinCols = 8;

/// THE SCREEN'S FURNITURE, DERIVED IN ONE PLACE.
///
/// It is a value rather than a set of constants, and it is recomputed rather than cached,
/// for exactly the reason `workspace_scene()` below is a function: a screen laid out against
/// an extent it no longer has is the stale-number lie this whole tool is arranged against.
/// It is a dozen integers and no allocation.
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
    /// use to decide what it can show. They are the pane's own cell bounds resolved
    /// against whatever text metric the active medium published (HD-1) -- equal to
    /// `terminal_w`/`terminal_h` in a medium whose character is a cell, and larger
    /// or smaller in one that sets real type.
    std::int64_t terminal_cols = 0;  ///< characters that fit across the pane
    std::size_t terminal_lines = 0;  ///< rows of prose the pane holds, chrome included
    std::size_t terminal_rows = 0;   ///< ...of which these many carry the transcript
    /// THE METRIC THIS SCREEN WAS RESOLVED WITH, carried rather than looked up
    /// (HD-2). A second bounded region INSIDE the pane -- the completion list --
    /// has to know where the pane's prose rows fall in CELLS, which is a question
    /// only the metric answers. Carrying it here is what keeps that a re-derivation
    /// from the same numbers rather than a second path to them: everything that
    /// resolves the pane's geometry already holds a `Screen`.
    std::int64_t text_advance_px = 0;
    std::int64_t text_line_px = 0;
};

/// The furniture for a surface of this extent -- TOTAL over every std::int64_t, because the
/// extent it is given came off the bus.
///
/// THE TEXT METRIC IS AN ARGUMENT, NOT A LOOKUP, and that is what makes this function the
/// single source of the pane's capacity (HD-1). Every party that needs to know how much
/// prose the pane holds -- the snapshot that chooses which entries fit, the omission marker
/// that says how many did not, and the painter that spends the rows -- calls THIS, with the
/// session's metric, and gets one answer. Before it, `terminal_w` was both the placement and
/// the capacity because a cell was both; a medium that sets type separates them, and the
/// separation has to happen in one place or the pane starts lying about what it left out.
inline constexpr Screen screen_of(std::int64_t want_w, std::int64_t want_h,
                                  std::int64_t text_advance_px = 0,
                                  std::int64_t text_line_px = 0) noexcept {
    Screen s;
    s.w = want_w < kScreenMinW ? kScreenMinW : (want_w > kScreenMaxW ? kScreenMaxW : want_w);
    s.h = want_h < kScreenMinH ? kScreenMinH : (want_h > kScreenMaxH ? kScreenMaxH : want_h);
    s.panel_x = s.w - kPanelCols;
    s.room_w = s.panel_x - kPanelGap;
    s.room_h = s.h - kWorkspaceY - kBottomRows;
    s.notice_y = s.h - 4;
    s.help_y = s.h - 2;
    // THE PANE, INSIDE THE ROOM THE SCREEN JUST RESERVED (HD-10). `room_w` is two lines up
    // and it is the whole of the fix: the pane's right edge is the workspace's right edge, so
    // the reserved side column is not the pane's to spend and does not have to know it. What
    // the pane WANTS is G-2's rule unchanged (half of every pair of columns the surface
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
static_assert(kMinScreen.notice_y == 18 && kMinScreen.help_y == 20, "the bottom band");
// THE PANE'S CORNER AND EXTENT ON THE MINIMUM SCREEN, and both moved in HD-10 -- the only two
// numbers in this file that did. It used to be 56x13 at (22, 9), with its right edge on the
// screen's and 28 columns of it standing in the reserved side column; it is 48x13 at (0, 9)
// now, which is the whole of the workspace's width and none of anybody else's. What it costs
// is eight columns of prose at the three narrowest extents, and what it buys is that the
// panel beside it is never erased. The height, the chrome and the transcript rows are
// untouched: this reservation is horizontal.
static_assert(kMinScreen.terminal_x == 0 && kMinScreen.terminal_y == 9, "the pane's corner");
static_assert(kMinScreen.terminal_w == 48 && kMinScreen.terminal_h == 13, "the pane's extent");
static_assert(kMinScreen.terminal_x + kMinScreen.terminal_w == kMinScreen.room_w,
              "the pane's right edge is the WORKSPACE's right edge, not the screen's (HD-10)");
static_assert(kMinScreen.terminal_rows == 9, "the transcript rows the pane has always had");
// WITH NO TEXT METRIC THE PANE IS EXACTLY THE PANE IT WAS, and these two say so in the type
// system: a character IS a cell, so the interior and the placement are the same numbers, and
// every golden this repository holds over a terminal medium is unmoved by HD-1.
static_assert(kMinScreen.terminal_cols == kMinScreen.terminal_w, "no metric: a character is a cell");
static_assert(kMinScreen.terminal_lines == static_cast<std::size_t>(kMinScreen.terminal_h),
              "no metric: a row is a cell row");

// ---- PLACEMENT RESOLVED: a place, on a screen, is a rectangle ---------------------------
//
// THE ONE PLACE A PANEL'S GEOMETRY IS WORKED OUT (PNL-1). Before this, each panel kind's
// painter carried its own: the Builder wrote its rows at the stack's x and padded them to
// the stack's width, Info wrote its labels at `Screen::panel_x`, and the picker knew all
// three of the stack's numbers. So "where is this panel, and what bounds belong to it" was a
// question you answered by reading two painters, and a third kind would have arrived with a
// third set of constants and a third chance to overlap something.
//
//     panel kind  ->  placement intent (panel.hpp)  ->  placement_bounds()  ->  the
//                                                              painter is HANDED that rect
//
// WHAT THIS IS NOT: a docking framework, an anchor system, a constraint solver, or a way for
// a kind to ask for somewhere neither place is. `placement_bounds` has two branches because
// Workshop has two places, and a third would be a phase with evidence for a third place.
// What a third KIND costs, meanwhile, is a catalog row -- it declares one of these two and
// is handed a rectangle, and nothing in this section changes.
//
// THE RECTANGLES ARE CANVAS CELLS, not workspace cells. The document's own scene is resolved
// against the workspace and offset by `kWorkspaceX/kWorkspaceY` at paint time; a panel's
// bounds are already where they are on the canvas. Both are `ui::Rect` because both are
// resolved observations of the same kind -- but a panel rect handed to `ui::hit` would be
// asking a question about the wrong space.

/// THE BOUNDS A PLACE RESOLVES TO on this screen, in canvas cells.
///
/// `slot` is a panel's position in the overlay stack, counted over the panels actually
/// placed there; the side region has room for exactly one panel (panel.hpp asserts it) and
/// ignores the slot entirely.
///
/// TOTAL, like every other function here that takes a number it did not choose. `Screen` is
/// already clamped, and the slot is clamped to the tallest screen this composition lays out
/// -- a slot below that is off every screen anyway, and without the clamp a large enough
/// slot is a signed multiply that leaves the number line. Nothing reachable passes one (the
/// open list is bounded by the catalog), which is exactly the argument W-1 measured wrong
/// once already.
///
/// AND IT IS A PURE FUNCTION OF PLACE, SLOT AND SCREEN (WIND-1) -- not of the panel's kind,
/// its provider, what it is showing, how wide it was a frame ago, or whether a maker
/// selected it. That is what keeps the width below one law rather than a policy: the same
/// rectangle answers the painter, the pointer, the capacity count and an external pane's
/// grant, and none of them can be told a different number.
inline constexpr ui::Rect placement_bounds(std::int64_t where, std::size_t slot,
                                           const Screen& sc) noexcept {
    if (where == placement::kSideRegion) {
        // From the top of the canvas to the bottom of the workspace: the column beside the
        // material, ending where the bottom band begins.
        return ui::Rect{sc.panel_x, kSideY, kPanelCols, kWorkspaceY + sc.room_h - kSideY};
    }
    const std::int64_t n = slot >= static_cast<std::size_t>(kScreenMaxH)
                               ? kScreenMaxH
                               : static_cast<std::int64_t>(slot);
    // THE WIDTH IS THE MINIMUM PLUS HALF THE ROOM'S SURPLUS OVER IT, floored (WIND-1). The
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

// ---- AUTHORED INTENT, PROJECTED ONTO THIS SCREEN (WIND-2) ----------------------------
//
// ONE OVERRIDE-AWARE RESOLVER, AND EVERYTHING CONSUMES ITS ANSWER: the painter, the
// pointer, the selected pane's affordances, an external pane's body and the room that body
// is granted. There is no second geometry here for the same reason PNL-1 removed the last
// one -- what is painted and what is met must be the same rectangle, and a second copy is
// right until the first thing moves.
//
// THE OVERRIDE IS SPENT ON THE OVERLAY STACK AND NOWHERE ELSE, and that is a law rather
// than a fallback. `screen_of` RESERVES the side column (`panel_x`, `room_w`) whether or
// not Info is open, and `room_w` is what a share of the workspace resolves against -- so a
// movable Info would change the resolved size of every share-width object in a maker's
// document, which is exactly the outcome PNL-0 refuses when it leaves the vacated column
// empty. A side-region row's authored geometry is therefore RETAINED in the file, never
// rewritten, and never spent; pane management refuses to author one and says why.

/// WHAT ONE PANE'S AUTHORED INTENT RESOLVES TO ON THIS SCREEN.
///
/// `projected` is false when this medium cannot honour an authored unit, and it is the
/// third thing a pane can be after "open" and "waiting" -- see `pane_state` below. The
/// authored value is untouched by it: the intent survives the refusal, exactly as
/// `reconcile`'s law already says of a setup legal on a tall screen and shown on a short
/// one.
struct PaneProjection {
    bool projected = true;
    /// WHAT THE AUTHORED INTENT ASKS FOR, before the canvas gets a say. May run past the
    /// screen's right or bottom edge, which is legal authored intent and is not rewritten.
    ui::Rect resolved{};
    /// ...AND THE PART OF IT THIS CANVAS ACTUALLY HAS. Empty when nothing of the pane is on
    /// screen, which is what `off-room` means and how it is told from `waiting`.
    ui::Rect visible{};
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

/// THE DEVELOPER'S ANSWER, THEN THE MAKER'S, PER AXIS -- and then the canvas.
///
/// `authored` is the setup row, or nothing for a pane the setup does not name. The order is
/// the whole of the model:
///
///     place default    ->  `placement_bounds(place, seated slot, Screen)`, unchanged
///     place cells      ->  the absolute authored x/y, NOT an offset from the default
///     width default    ->  the developer's reactive answer for that axis (WIND-1's
///     height default        half-share for the overlay stack, untouched)
///     width cells      ->  the authored count, for THAT AXIS ONLY
///     height cells
///
/// EACH AXIS IS INDEPENDENT, so a place-only edit leaves a default width still reacting to
/// the room and a width-only edit moves neither the place nor the height. Freezing all
/// three on the first edit would convert two developer defaults into maker decisions the
/// maker never made, which is `default_setup`'s own argument said about geometry.
///
/// ---- Why a pixel amount is refused HERE, on every medium in this build ----
///
/// No medium publishes a trustworthy per-axis device-pixel scale for a canvas cell, and the
/// two near-misses are traps rather than answers:
///
///   `RegionFit::graphical()` (a positive TEXT metric) identifies a medium that sets real
///   TYPE, which is a different fact -- `skin_sdl.cpp`'s own comment says a window whose
///   font failed to open publishes `{w, h, 0, 0}` while still laying its canvas out at
///   `kCanvasCellPx` device pixels per cell.
///
///   `kCanvasCellPx` itself is a constant of THE ONE GRAPHICAL SKIN THAT EXISTS, and
///   `surface/pointing.hpp` says so in the sharpest terms this tree uses. Workshop is
///   allowed to apply it to a POINTER only because the event arrives carrying
///   `input::space::kPixels` -- a stamp on that moment, made by the backend. `SurfaceExtent`
///   carries no such stamp, and there is no way to ask the active Skin what its presentation
///   context is.
///
/// So a pixel axis is a LEGAL, PERMANENTLY RETAINED authored value with an honest
/// per-projection refusal, and the refusal is WHOLE: a pane with either axis in pixels is
/// not presented at all, rather than presented at the default width with an honoured
/// height. Two precedents, both load-bearing -- `doc::resize` checks both extents before
/// writing either, and `PaneContent` is judged whole before a byte is retained. A per-axis
/// fallback would be exactly the silent default this contract refuses.
///
/// The future rule, once a real per-axis scale is published, is `cells = max(1, pixels /
/// scale)`, floored and independently per axis. That is documentation and not code: WIND-2
/// invents no scale and widens no shape in `surface/`.
///
/// `pane_unit_projectable` is that refusal as ONE predicate, because two parties ask it: the
/// projection below, and the state classifier, which has to answer `refused` for a pane
/// whose reactive tile also ran out -- a question about a unit outranks a question about
/// room, and a second copy of the test is how the two would come to disagree.
///
/// IT TAKES NO PLACEMENT, AND THAT IS WIND-2a's CORRECTION (findings 2.6). WIND-2 spelled
/// it `where != kOverlayStack -> true`, which read as "a fixed pane has no geometry to
/// project, so nothing to refuse" and was a different sentence from the one the phase
/// wrote down: *a pane with either axis in pixels is not presented, in every current
/// build*. A setup could therefore carry `width: 240px` for Info and Info went on being
/// presented at the developer's width, silently ignoring an authored value in a unit this
/// build has already decided it cannot honour -- the exact per-axis silent default the
/// whole-refusal rule exists to prevent, arriving through the one placement that was
/// exempt from it.
///
/// SO THE UNIT IS A FACT ABOUT THE AUTHORED ROW AND THE PLACEMENT IS NOT PART OF IT. Fixed
/// placement is still fixed: Info's PLACE and SIZE remain the screen's, a side-region row's
/// retained cell geometry stays inert, and management still refuses to author one and says
/// which reservation it hit. What fixed placement is not is permission to present an
/// unsupported unit as though it were understood. Dropping the parameter rather than
/// ignoring it is what makes the old spelling unsayable at the call sites.
inline bool pane_unit_projectable(const SetupPane* authored) noexcept {
    if (authored == nullptr) {
        return true;
    }
    return authored->width.mode != pane_unit::kPixels &&
           authored->height.mode != pane_unit::kPixels;
}

inline PaneProjection project_pane(std::int64_t where, std::size_t slot,
                                   const SetupPane* authored, const Screen& sc) {
    PaneProjection out;
    out.resolved = placement_bounds(where, slot, sc);
    // THE UNIT IS ASKED FIRST AND FOR EVERY PLACEMENT (WIND-2a). A refusal is WHOLE --
    // no rectangle, resolved or visible -- so every consumer that already reads an empty
    // rectangle as "nowhere" is right about a pixel-sized pane with no branch of its own.
    if (!pane_unit_projectable(authored)) {
        return PaneProjection{false, ui::Rect{}, ui::Rect{}};
    }
    if (where == placement::kOverlayStack && authored != nullptr) {
        if (authored->place.mode == pane_unit::kCells) {
            out.resolved.x = authored->place.x;
            out.resolved.y = authored->place.y;
        }
        if (authored->width.mode == pane_unit::kCells) {
            out.resolved.w = authored->width.amount;
        }
        if (authored->height.mode == pane_unit::kCells) {
            out.resolved.h = authored->height.amount;
        }
    }
    out.visible = clip_to_canvas(out.resolved, sc);
    return out;
}

/// What the one narrow path answers with: whether this kind is open, where its kind is
/// placed, and the rectangle it occupies if it is open at all.
struct PanelBounds {
    bool open = false;
    /// THE KIND'S DECLARED PLACE, open or not — a fact about the catalog rather than about
    /// this session, so it is answerable for a panel nobody has opened.
    std::int64_t placed_in = placement::kOverlayStack;
    /// EMPTY WHEN THE PANEL IS NOT OPEN, deliberately: a caller that forgets to ask `open`
    /// gets a rectangle that contains nothing (`ui::Rect::contains` says so for w/h <= 0)
    /// rather than the first slot's, which would be a closed panel answering as though it
    /// were somewhere.
    ///
    /// IT IS THE VISIBLE RECTANGLE SINCE WIND-2 -- resolved, then clipped to the canvas --
    /// so every consumer that already treated an empty rectangle as "nowhere" answers
    /// correctly for a pane whose authored place is off-room, with no branch of its own.
    ui::Rect rect{};
    /// ...and what the authored intent ASKED for, unclipped. Read by the state classifier,
    /// which has to tell "partly cut off" from "not on this screen at all".
    ui::Rect resolved{};
    /// FALSE WHEN THIS MEDIUM CANNOT PROJECT THE AUTHORED UNIT. `rect` is then empty too,
    /// so nothing paints, nothing is met and no room is granted -- but the reason is a
    /// different one from off-room and a maker is told which.
    bool projected = true;
};

/// WHERE AN OPEN PANEL IS RIGHT NOW — the one narrow path, and the only thing that knows how
/// a slot is earned.
///
/// A panel takes the next slot in the stack only if it is PLACED in the stack, so an Info
/// ahead of a Builder in the open list never pushes it down a slot it does not occupy. That
/// rule used to be a counter inside the painting loop that named a kind; it is stated here
/// once, and a third kind is counted by it without being mentioned in it.
///
/// AND ONLY A REACTIVE PANE EARNS ONE (WIND-2), which is `seat_panes`' rule said in the
/// place the rectangle is actually resolved. A pane the maker PLACED is not in the tiling,
/// so counting a slot for it would push every reactive pane below it down a row for a
/// rectangle that is not in the stack at all.
///
/// THE SETUP IS A REQUIRED ARGUMENT AND IT IS NOT DEFAULTED, for `resolve_pane`'s reason
/// and HD-4's: a default would let a call site keep the three-argument spelling and be
/// silently right until the first authored override, and the symptom would be a maker's
/// moved pane painted in one place and met in another.
inline PanelBounds bounds_of(const Panels& panels, const Setup& setup, std::int64_t kind,
                             const Screen& sc) {
    std::size_t slot = 0;
    for (const Panel& p : panels.open) {
        const std::int64_t where = placement_of(p.kind);
        const SetupPane* authored = nullptr;
        for (const SetupPane& row : setup.panes) {
            const std::optional<std::int64_t> named = resolve_pane(row.ref, panels.runtime);
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
    return PanelBounds{false, placement_of(kind), ui::Rect{}, ui::Rect{}, true};
}

// The two places fit the SMALLEST screen this composition is honest on, which is where they
// are tightest: the side region keeps its reservation at every extent (a wider surface gives
// it exactly as much as it had, the same rule the bottom band follows), and a stack slot
// takes half the room's surplus and so grows strictly slower than the room does (WIND-1).
// Asserted over the RESOLVED rectangles rather than over the constants behind them, which is
// what PNL-1 bought -- "these two places do not overlap" is now one comparison of two
// rectangles instead of a hand-checked relation between four separate numbers.
inline constexpr ui::Rect kMinSide = placement_bounds(placement::kSideRegion, 0, kMinScreen);
inline constexpr ui::Rect kMinStack = placement_bounds(placement::kOverlayStack, 0, kMinScreen);

static_assert(kMinStack.x + kMinStack.w <= kMinSide.x - kPanelGap,
              "the two places do not overlap: a stacked panel never reaches the side region");
static_assert(kMinStack.x + kMinStack.w == kMinScreen.room_w,
              "the stack is exactly the minimum screen's workspace width -- it covers the top "
              "of the workspace and nothing else");
// AND THE HALF-SHARE NEVER SPENDS WHAT IS NOT THE STACK'S (WIND-1). The reserved column is
// the law HD-10 brought the pane under, and the stack has to keep it at every extent rather
// than only at the one the rectangles above are resolved on. Three witnesses in the type
// system -- the minimum, an ODD surplus, and a wide surface -- say the two halves of the
// rule: a slot never reaches past the room, and wherever there is any surplus at all it
// stops strictly short of it, so a column of its own rows is always still the maker's. The
// exhaustive version, over the whole clamped width domain, is in the suite.
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
// AND THE TERMINAL PANE OBEYS THE SAME LAW AS THE STACK, which is what HD-10 bought. The pane
// is not a `placement` -- it is a MODE with a rectangle, older than this section and computed
// in `screen_of` -- so it cannot be asserted through `placement_bounds`; it is asserted here,
// in the same words and against the same rectangle, because the law is about the RESERVED
// COLUMN and not about which function drew the thing that reached into it.
//
// THIS IS THE ONE INVARIANT HD-10 ADDS, and it is deliberately narrow. It does not say that
// regions may not overlap: the completion list covers the pane's own transcript, the picker
// covers the stack slot beneath it, and a mode's pane covers whatever panel is under it --
// three overlaps that are all intentional and all inside one owner's room. What it says is
// that the room reserved BESIDE the workspace is nobody's to spend, which is the one overlap
// that had no owner and no reason.
static_assert(kMinScreen.terminal_x + kMinScreen.terminal_w <= kMinSide.x - kPanelGap,
              "the two places do not overlap: the terminal pane never reaches the side region");
static_assert(kPickerRows <= kStackRows, "the picker is never taller than a panel");

/// WHERE THE PICKER OPENS: the stack's first slot, and it is a function rather than a repeated
/// expression so that the mode that PAINTS there and the pointer that must not see THROUGH it
/// read one answer. The picker has no catalog row to declare a place in -- it is a mode -- so
/// this is the one presentation that names its own place, and now it names it once.
inline constexpr ui::Rect picker_bounds(const Screen& sc) noexcept {
    return placement_bounds(placement::kOverlayStack, 0, sc);
}

/// HOW MANY OVERLAY SLOTS THIS SCREEN ACTUALLY HAS ROOM FOR (WP-0) -- the one
/// answer to "may another panel be presented", asked before anything reaches
/// `Panels::open`.
///
/// THE FLOOR IS THE WORKSPACE'S BOTTOM AND NOT THE NOTICE LINE, and the
/// difference is one row that carries a sentence:
///
///     kWorkspaceY + sc.room_h  ==  sc.notice_y - 1
///
/// -- the row directly under the workspace, which WS-0 spent on the SETUP LINE.
/// A capacity checked against `notice_y` would call a second slot legal at the
/// minimum composition and let it erase the line naming the arrangement a maker
/// is in. The two expressions are equal by `screen_of`'s own arithmetic
/// (`room_h = h - kWorkspaceY - kBottomRows`, `notice_y = h - 4`), asserted
/// under this function, and the one written here is the one that says WHY: the
/// stack lives in the workspace's room and the bottom band is somebody else's.
///
/// THE SLOTS ARE ASKED OF `placement_bounds`, NOT COMPUTED FROM `kStackRows`.
/// There is no second arithmetic here and there must not be: whatever moves a
/// slot's rectangle moves this count with it, which is the same one-measurer rule
/// `occupied_at` follows for the pointer. The loop is bounded by
/// `kMaxSetupPanes` because a setup cannot author more references than that, and
/// a screen this composition lays out never reaches even four.
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

static_assert(kWorkspaceY + kMinScreen.room_h == kMinScreen.notice_y - 1,
              "the overlay floor is the row under the workspace, which is the setup line: "
              "a capacity measured against notice_y would let a second slot erase it");
static_assert(stack_slots_that_fit(kMinScreen) == 1,
              "the minimum composition has room for exactly one overlay panel");

// ---- PLACEMENT SPENT ON THE POINTER: a place a maker can see is a place a hand meets ------
//
// PNL-1 MADE THE DEFECT SAYABLE AND PNL-2 SAYS IT. With bounds resolved in one path, "did this
// press land inside an open panel" became one `contains` call -- and the measured answer was
// that it did not matter, because nothing asked: a press at a canvas cell the Builder was
// visibly covering took hold of the object underneath it, selected that object, and began a
// drag a maker could not see. Two phases old, and not previously expressible.
//
// SO A PANEL'S BOUNDS ARE NOT ONLY PIXELS. The rectangle a kind's place resolves to is the
// rectangle it occupies in every sense this application has: what gets painted there, and what
// a hand meets there. There is no second geometry and no second truth -- `occupied_at` asks
// `bounds_of`, which is the same call `paint_panels` makes for the same panel on the same
// screen, so a panel that moved would take its occupancy with it without anybody remembering.
//
// IT IS A QUESTION, NOT A DISPATCHER. No kind is named below, nothing is registered, nothing
// is captured and nothing is focused: one loop over what is open, one `contains`, and an
// answer that says WHICH presentation was met so a maker can be told rather than left with a
// press that vanished.

/// WHAT A MAKER'S HAND MEETS AT A CANVAS CELL: nothing, or the presentation occupying it.
struct Occupancy {
    bool occupied = false;
    /// The name a maker reads on those cells -- the catalog's own for a panel, the picker's
    /// own for the picker. Empty when nothing is there, and never a kind a caller has to
    /// switch on: what it is FOR is a sentence.
    ///
    /// A `std::string` SINCE WP-0, AND THE CHANGE IS A LIFETIME RATHER THAN A TASTE.
    /// A runtime pane's name lives in a `std::vector<RuntimePane>` that the next accepted
    /// offer may grow and reallocate, so a `const char*` taken out of one would dangle at
    /// the next offer -- a defect whose symptom is a correct-looking notice printed from
    /// freed memory, which is exactly the class the sanitizer lane exists to name (W-3a).
    /// The built-ins' names are static and would have been safe either way; one shape for
    /// both is what stops a reader having to know which half they are holding.
    std::string what;
};

/// DOES ANY VISIBLE PRESENTATION OCCUPY THIS CANVAS CELL — the one question the pointer asks
/// before it asks the document anything.
///
/// IT ANSWERS WITH WHAT IS ON TOP, in painter's order reversed: the picker first because it is
/// painted last over the stack's first slot, then the open panels newest-first. Today no two of
/// those rectangles can overlap -- the side region holds one panel by a static_assert and the
/// stack's slots are disjoint by construction -- so the order changes no answer; it is written
/// this way because the question is "what would a maker say is there", and that is the
/// topmost-not-first discipline a suite helper already had to learn once (PNL-0's `label_at`).
///
/// THE TERMINAL OVERLAY IS NOT HERE, deliberately. It is a MODE and it outranks the pointer
/// entirely rather than by bounds -- while it is open the pointer does nothing anywhere, which
/// is a strictly wider rule than occupancy and is enforced ahead of this call (weave.hpp). A
/// pane that answered here as well would be the same rule written twice, and the second copy is
/// the one that would go stale.
///
/// IT WALKS THE PRESENTATION ORDER SINCE WIND-2, NOT THE OPEN LIST. `presentation_order`
/// is the setup's canonical `front` ranks restricted to what is seated, and this walks it
/// BACKWARD -- so the topmost pane answers first, which is the same law `ui::hit` states
/// for the document one layer down. Before overlap was reachable the two orders were the
/// same list and the choice changed no answer; a maker who raises a pane makes it the
/// answer, which is what raising one means.
inline Occupancy occupied_at(const Panels& panels, const Setup& setup, const Screen& sc,
                             std::int64_t cx, std::int64_t cy) {
    if (panels.picker.open && picker_bounds(sc).contains(cx, cy)) {
        return Occupancy{true, kPickerName};
    }
    const std::vector<std::int64_t> order = presentation_order(setup, panels);
    for (std::size_t i = order.size(); i > 0; --i) {
        const std::int64_t kind = order[i - 1];
        if (bounds_of(panels, setup, kind, sc).rect.contains(cx, cy)) {
            // `kind_name` AND NOT `panel_kind(kind).name` (WP-0). The total lookup answers
            // `Builder` for anything outside the compile-time catalog, so an external pane
            // would tell a maker their hand was on the build tool -- the same lie
            // `resolve_pane` is fallible to prevent, arriving through the pointer instead
            // of through a file. Built-ins are unchanged: `kind_name` reads the same row.
            return Occupancy{true, kind_name(panels, kind)};
        }
    }
    return Occupancy{};
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
///
/// It holds the IDENTITY being manipulated and where inside that object the
/// maker took hold -- and deliberately not a position, and deliberately not a
/// size. The object's authored place and extents are on the object; a second
/// copy of either here would be the shadow model the whole Workshop arc is
/// arranged to avoid, and it would be the copy that goes stale.
///
/// `grab_dx/dy` are in RESOLVED cells — where inside the object's rectangle on
/// the workspace the maker took hold. RESOLVED and not authored, and the two are
/// genuinely different numbers: a resolved position is its context's origin plus
/// the authored offset, and what a hand grabs is what is on screen. The grab is
/// a plain SUBTRACTION and not an inverse of the resolver; turning the hand's
/// answer back into authored truth is a second subtraction -- the context's
/// origin -- performed in `place`.
///
/// `resizing` is the whole of a resize's session cost: ONE bool, because there
/// is one gesture in flight and it is either moving the object or sizing it. A
/// resize
/// stores no grab offset (the handle is one cell and the pointer names the new
/// corner outright), no starting extent and no starting viewport -- the
/// projection reads the CURRENT extent for its mode and the CURRENT workspace for
/// its span, and a proposal computed from the pointer rather than from the
/// previous proposal cannot drift. Nothing here is a Tool, a Manipulator or a
/// TransformSession; one gesture does not earn an interaction framework.
struct Drag {
    bool active = false;
    bool resizing = false; ///< the maker took hold of the size handle, not the body
    std::int64_t id = 0;
    std::int64_t grab_dx = 0;
    std::int64_t grab_dy = 0;
};

// THE LINE A MAKER IS TYPING IS A COMPONENT NOW (HD-5).
//
// `TerminalInput` lived here from HD-3 to HD-4 and is gone: the class -- its text, its caret,
// its horizontal window, its character-safe operations, its two-half invariant and every line
// of its reasoning -- IS `zengine::component::TextBox`, moved out whole and renamed once.
//
// It moved because a second tool needed the same machinery, not because a component library
// wanted a member. HD-4 traced the Inspector's property draft on all nine axes and declined
// the extraction, correctly: at that point the two shared only the character walk they were
// already sharing as free functions, so a `TextBox` would have renamed this class and deleted
// nothing. HD-5 is the day the property editor needs the caret, the window and the pointer
// arithmetic -- which is the day extracting is the SMALLER repair.
//
// WHAT STAYED HERE is everything that is about a Terminal rather than about editing text: the
// prompt's width, where the pane's editable row is, what a completion is allowed to assume,
// the submission grammar, and the participant the line is eventually spoken to. The component
// has never heard of any of it.

/// THE TERMINAL OVERLAY'S VIEW OF A PARTICIPANT — session, emphatically, and a SNAPSHOT.
///
/// Workshop presents an ordinary `loom::TerminalSession` that its host mounted on the one bus
/// this process has. What lives here is the presentation's side of that and nothing else:
/// whether the pane is open, the line a maker is part-way through typing, and a COPY of the
/// entries the pane is currently showing.
///
/// A COPY, and that is a lifetime answer rather than a convenience. The participant is owned
/// by the bus; this pane holds a non-owning pointer to it in the weave and nothing at all
/// here. `Transcript::entries()` returns by value precisely so a presentation may hold the
/// result across anything, including the death of the participant it came from -- so a screen
/// painted from this struct cannot read a freed transcript no matter when it is painted.
///
/// IT IS NOT AN IDENTITY AND CARRIES NO AUTHORITY. `id` is here so the pane can say WHOSE
/// record a maker is reading, because a presentation showing two identities that does not name
/// them is the one thing this composition must never become. Nothing is authored from this
/// struct; every send goes through the participant's own door, stamped by the bus with the
/// participant's own WeaveId and gated against the participant's own grant.
struct TerminalPane {
    bool open = false;         ///< shift+space, and nothing else, decides this
    bool attached = false;     ///< is there a participant at all (a host may mount none)
    loom::WeaveId id{};        ///< the participant's identity, for the header
    /// The line being typed, before Return authors anything — AND THE CARET IN IT (HD-3),
    /// AND WHICH PART OF IT THE ROW IS SHOWING (HD-4). One object, because the three are one
    /// fact: see `component/text_box.hpp` for why none of them is a field beside a
    /// `std::string`.
    ///
    /// THE PANE OWNS IT MECHANICALLY (HD-5). It is a member, not an entity: it has no
    /// identity, nothing registers it, nothing persists it, and closing the pane does not
    /// have to clean it up because there is nothing to clean up. The Inspector's editing row
    /// owns another one, and neither knows the other exists.
    component::TextBox input;
    std::vector<loom::TranscriptEntry> shown; ///< the newest entries that FIT, oldest first
    std::uint64_t earlier = 0; ///< kept by the participant, above the top of this pane
    std::uint64_t dropped = 0; ///< evicted from the transcript entirely -- gone, not scrolled
    /// WHAT THE PARTICIPANT COULD SAY NEXT, for the line above (HD-2). Derived from
    /// `input` and the participant's own vocabulary, recomputed whenever the line
    /// changes, and holding no fact that is not readable from those two -- so it is a
    /// snapshot in exactly the sense `shown` is, and for the same reason.
    Completion completion;
    /// THE ONE PIECE OF COMPLETION STATE THAT IS NOT DERIVED: the maker pressed Escape
    /// and does not want the list for this part of the line. It is remembered against
    /// the SLOT rather than against the text, so typing more of the same word leaves it
    /// dismissed and moving on to the next word brings it back -- a dismissal that
    /// survived one keystroke would be a gesture with no effect, and one that survived
    /// the whole line would make the list unreachable without retyping.
    bool dismissed = false;
    LineSlot dismissed_at = LineSlot::Verb;
    /// ...and the other direction: the maker pressed the completion key on an EMPTY line,
    /// which is the one place discovery needs a gesture (HD-2 §14).
    ///
    /// A LIST OVER AN EMPTY LINE WOULD COVER THE ANSWER THE PANE JUST GAVE. Submitting
    /// clears the line, so "show candidates whenever there are any" put the verb list on
    /// top of the reply to the command that had just been typed -- measured, on the case
    /// that asserts the pane states its whole grammar with nothing elided. So an untouched
    /// line asks nothing and typing is the gesture; this flag is the deliberate way to ask
    /// anyway, and any change to the line ends it.
    bool asked = false;
};

// ---- PANE MANAGEMENT: what a maker is ARRANGING, and how (WIND-2) ---------------------
//
// ALL OF IT IS SESSION AND NONE OF IT REACHES A FILE. Which pane is selected, which step of
// the arrangement a maker is on, which edge they chose and which gesture their hand is
// holding are four facts that die with the process, exactly as `PanelPicker` and
// `SetupNaming` do. What SURVIVES is what the gestures WROTE, which is the authored setup.

namespace pane_manage {
inline constexpr std::int64_t kSelect = 0; ///< choosing which pane; ordering acts from here
inline constexpr std::int64_t kMove = 1;   ///< arrows author an absolute place
inline constexpr std::int64_t kSize = 2;   ///< an edge is chosen; arrows author that axis
inline constexpr std::int64_t kReset = 3;  ///< one key per authored dimension
} // namespace pane_manage

/// THE EIGHT MANIPULATION AFFORDANCES of a rectangle, and there is not a ninth.
///
/// AN EDGE NAMES AN AXIS AND A DIRECTION -- IT IS NOT AN ANCHOR, and that is the phase's
/// one genuinely surprising rule, so it is written where the constants are. A resize writes
/// SIZE and never PLACE (`doc::resize`'s law, carried), so a pane's top-left corner is its
/// authored place and stays exactly where the maker put it whichever edge they pull. What
/// the left edge buys over the right one is the DIRECTION a hand means: pulling left widens,
/// pulling right narrows, and the pane grows from its own corner either way. Making the left
/// edge move the place instead would turn one gesture into two authored writes and put a
/// refused height beside a moved corner, which is the exact refusal-beside-a-successful-write
/// `doc::resize` exists to refuse.
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
///
/// TWO SIGNALS, AND THE CHARACTER IS THE FIRST OF THEM (HD-8's law). A terminal has no
/// ground to tint and a maker may have no colour at all, so the accent role is the SECOND
/// signal and never the only one -- which is why an edge has a name AND a mark rather than a
/// highlight.
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

/// THE ONE CELL AN AFFORDANCE IS DRAWN ON. Derived every time it is wanted and stored
/// nowhere, so what is painted and what is met are the same arithmetic -- `size_handle`'s
/// law, and the reason there is no `click_edge_bounds()` beside a `paint_edge_bounds()`.
/// An edge's mark sits at the MIDDLE of its run, which is the cell furthest from the two
/// corners it shares its run with.
inline ui::Rect pane_edge_cell(const ui::Rect& r, std::int64_t edge) noexcept {
    const std::int64_t x0 = r.x;
    const std::int64_t x1 = r.x + r.w - 1;
    const std::int64_t y0 = r.y;
    const std::int64_t y1 = r.y + r.h - 1;
    const std::int64_t xm = r.x + r.w / 2;
    const std::int64_t ym = r.y + r.h / 2;
    switch (edge) {
    case pane_edge::kLeft: return ui::Rect{x0, ym, 1, 1};
    case pane_edge::kRight: return ui::Rect{x1, ym, 1, 1};
    case pane_edge::kTop: return ui::Rect{xm, y0, 1, 1};
    case pane_edge::kBottom: return ui::Rect{xm, y1, 1, 1};
    case pane_edge::kTopLeft: return ui::Rect{x0, y0, 1, 1};
    case pane_edge::kTopRight: return ui::Rect{x1, y0, 1, 1};
    case pane_edge::kBottomLeft: return ui::Rect{x0, y1, 1, 1};
    case pane_edge::kBottomRight: return ui::Rect{x1, y1, 1, 1};
    default: return ui::Rect{};
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

/// WHICH AFFORDANCE OF THIS RECTANGLE A CANVAS CELL IS ON, or `kNoPaneEdge`.
///
/// THE RING IS THE OUTERMOST CELL, derived every time it is wanted and stored nowhere --
/// `size_handle`'s law, said about eight cells instead of one. A CORNER WINS over the two
/// edges it belongs to, because a corner cell is genuinely in both and a maker aiming at one
/// means the corner; without the precedence a diagonal gesture would be unreachable at
/// exactly the cell it is drawn on.
inline std::int64_t pane_edge_at(const ui::Rect& r, std::int64_t cx, std::int64_t cy) noexcept {
    if (!r.contains(cx, cy)) {
        return kNoPaneEdge;
    }
    const bool left = cx == r.x;
    const bool right = cx == r.x + r.w - 1;
    const bool top = cy == r.y;
    const bool bottom = cy == r.y + r.h - 1;
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

/// WHICH PANE A MAKER IS ARRANGING, AND WHICH STEP THEY ARE ON.
///
/// THE SELECTION IS A `PaneRef` AND NOT A KIND, and that is the identity law spent where it
/// matters most: a runtime kind is session-local and a pane a maker selected may stop
/// resolving between two frames, while the reference they chose is the durable thing they
/// meant. It is also what lets an UNRESOLVED pane be selected at all, which is the whole of
/// the recovery path -- a maker cannot reset the place of a pane they cannot name.
///
/// AN EMPTY `provider` MEANS NOTHING IS SELECTED. `check_pane_key` refuses an empty key, so
/// no reference a file or a catalog can produce is ever equal to it.
struct PaneManagement {
    bool open = false;
    PaneRef selected;
    std::int64_t doing = pane_manage::kSelect;
    std::int64_t edge = pane_edge::kBottomRight;

    bool has_selection() const { return !selected.provider.empty(); }
};

/// A PANE GESTURE IN FLIGHT. Session, emphatically not content.
///
/// ONE PRESS CLAIMS ONE GESTURE UNTIL RELEASE, and everything below is what that costs: the
/// IDENTITY being manipulated, which edge it was taken by, and the two numbers a proposal is
/// measured FROM. It holds no rectangle and no live position -- `Drag`'s own law -- so a
/// motion that crosses another pane, the Terminal, or an ordering change cannot transfer
/// custody, because there is nothing here for a different pane to become.
///
/// `base_w`/`base_h` ARE THE SIZE AT THE MOMENT OF THE PRESS, and they are why a resize is
/// deterministic rather than accumulated: every motion proposes `base + (pointer - press)`,
/// so the same hand position always means the same size no matter how the pointer got there.
struct PaneGesture {
    bool active = false;
    PaneRef pane;
    bool sizing = false;
    std::int64_t edge = kNoPaneEdge;
    std::int64_t grab_dx = 0; ///< move: where inside the pane's rectangle the hand took hold
    std::int64_t grab_dy = 0;
    std::int64_t from_x = 0;  ///< size: the canvas cell the press landed on
    std::int64_t from_y = 0;
    std::int64_t base_w = 0;  ///< size: the pane's resolved size at that moment
    std::int64_t base_h = 0;
};

/// The session: what a maker is currently doing, as opposed to what they have
/// authored. Kept out of WorkshopDoc deliberately (see vocabulary.hpp) so the
/// two kinds of fact cannot be mistaken for each other -- selection is not
/// content, and neither is the size of the window it is being looked at through,
/// and neither is a half-finished drag.
struct Session {
    std::int64_t selected = 0;              ///< the selected object's IDENTITY (0 = none)
    /// HOW MUCH ROOM THE SURFACE SAID IT HAS, in canvas cells -- session, and the most
    /// session-like fact in this struct: it is not authored, it is not derived from anything
    /// authored, and two makers looking at the same document through different windows are
    /// looking at the same document. It starts at the minimum and moves only when a Skin says
    /// so (`surface::SurfaceExtent`), so a run with no medium opinion is exactly the run
    /// Workshop had before G-2.
    std::int64_t screen_w = kScreenMinW;
    std::int64_t screen_h = kScreenMinH;
    /// AND HOW BIG ONE CHARACTER OF THAT SURFACE IS, in its own device pixels -- the other
    /// half of the same sentence, and session for exactly the same reasons (HD-1). Zero is
    /// the honest starting value and the honest steady value for every character medium: it
    /// means "text is a cell", which is what a terminal's text is and what a window's text
    /// is until a real face opens. A run with no medium opinion is therefore, again,
    /// precisely the run Workshop had before.
    std::int64_t text_advance_px = 0;
    std::int64_t text_line_px = 0;
    std::int64_t workspace_w = kWorkspaceW; ///< what a share of the workspace currently means
    std::int64_t workspace_h = kWorkspaceH;
    std::size_t cursor = 0;   ///< which inspector row the maker is on
    std::vector<Row> rows;    ///< the inspector, rebuilt when the selection changes
    Drag drag;                ///< a pointer drag in flight, if any
    std::string notice;       ///< the last thing Workshop had to say
    bool notice_is_bad = false; ///< whether that thing was a refusal
    TerminalPane terminal;    ///< the terminal overlay, when a maker has opened it
    /// THE DYNAMIC PANELS a maker has opened, and the picker they opened them from
    /// (panel.hpp). Session like everything else here, and for the sharpest version of the
    /// reason: what a maker has open is not what a maker has AUTHORED, and a panel list
    /// that rode the document would make "which tools were showing" part of the file --
    /// a persistence decision arriving as a side effect of where a vector was declared.
    Panels panels;
    /// THE AUTHORED SETUP THIS SESSION IS SHOWING, its copy of the one in its file, and the
    /// one-line editor over its name (setup.hpp). WS-0's new fact, and the only member of
    /// this struct that is genuinely AUTHORED rather than session -- which is why it is worth
    /// saying here what it is doing here.
    ///
    /// IT IS BESIDE `panels` BECAUSE IT IS `panels`' TWIN, and the pair is the whole of the
    /// phase's authored/resolved split: `setup.active.panes` is which panes a maker MEANT,
    /// durably and in their own words; `panels.open` is which presentations this build could
    /// make of that intent, on this screen, in this run. `reconcile` is the one path between
    /// them, and the reason they are members of one struct is that a reader who finds one
    /// should find the other before they write a gesture that moves only half of the pair.
    ///
    /// IT IS EMPHATICALLY NOT `WorkshopDoc`, and the boundary that matters is that one. The
    /// document is what a maker MADE; this is what they were looking at while they made it.
    /// The two persist to different files through different functions, and neither save
    /// touches the other's bytes.
    SetupState setup;
    /// WHICH PANE A MAKER IS ARRANGING AND WHICH STEP THEY ARE ON (WIND-2). Session, beside
    /// `panels` and `setup` rather than inside either: it is neither a presentation nor
    /// authored intent -- it is the maker's hand halfway through a sentence, and it dies
    /// with the process exactly as `PanelPicker` and `SetupNaming` do.
    PaneManagement manage;
    /// ...and the pane gesture their pointer is holding, if any. Deliberately NOT `drag`:
    /// a document object and a pane are two different things to be holding, and one
    /// variable for both would make "a release ends the gesture it began" a question about
    /// which kind of thing was underneath rather than a fact about the press.
    PaneGesture pane_drag;
};

/// This session's screen furniture. The one call; see `Screen`.
///
/// IT CARRIES THE METRIC, and that is what makes "the one call" true rather than aspirational
/// (HD-1). Everything in this application that needs to know how wide the pane's prose is
/// reaches it through here, so there is no second path by which a caller could compute the
/// pane's capacity from `terminal_w` and be quietly wrong on a graphical medium.
inline constexpr Screen screen_of(const Session& s) noexcept {
    return screen_of(s.screen_w, s.screen_h, s.text_advance_px, s.text_line_px);
}

/// TAKE THE ROOM A SURFACE OFFERED, and re-fit the workspace to it. Answers whether anything
/// actually changed, so a caller can decline to repaint over a surface that merely repeated
/// itself.
///
/// THE WORKSPACE FILLS THE NEW ROOM, and that is the whole of what "more usable surface"
/// means here -- a bigger window whose workspace stayed 48 cells wide would be a bigger
/// picture of the same tool. It costs a maker's `[` narrowing when the surface changes size,
/// and that is the honest trade rather than an oversight: `[`/`]` says "show me this
/// document in a narrower workspace", and dragging the window is the same sentence said with
/// a hand. The one that happened last wins, and there is no second remembered width for the
/// two of them to disagree about.
///
/// Nothing authored is touched. A share resolves to something else and every authored value
/// is byte-identical, which is the property `[`/`]` was built to demonstrate and this is a
/// second way to reach.
///
/// THE TEXT METRIC IS TAKEN THE SAME WAY AND ON THE SAME TERMS (HD-1): clamped at nothing
/// (a negative advance is not a size, and the vocabulary already spells non-positive as
/// "text is a cell"), and a change in it counts as a change even when the cell extent did
/// not move. That second half is load-bearing -- a font that opens after the first frame
/// changes only the metric, and a guard that compared extents alone would have swallowed the
/// one message that says the pane may now hold real type.
inline bool adopt_screen(Session& s, std::int64_t want_w, std::int64_t want_h,
                         std::int64_t want_advance_px = 0, std::int64_t want_line_px = 0) {
    const std::int64_t advance = want_advance_px > 0 ? want_advance_px : 0;
    const std::int64_t line = want_line_px > 0 ? want_line_px : 0;
    const Screen fresh = screen_of(want_w, want_h, advance, line);
    if (fresh.w == s.screen_w && fresh.h == s.screen_h && advance == s.text_advance_px &&
        line == s.text_line_px) {
        return false;
    }
    s.screen_w = fresh.w;
    s.screen_h = fresh.h;
    s.text_advance_px = advance;
    s.text_line_px = line;
    s.workspace_w = fresh.room_w;
    s.workspace_h = fresh.room_h;
    return true;
}

/// The workspace as a viewport, and the document resolved against it — the ONE
/// call that turns authored intent into geometry in this application.
///
/// It is a function rather than a cached member of Session on purpose. A
/// resolved scene that outlived the workspace it was resolved against is exactly
/// the stale-number lie the authored/resolved split exists to prevent, so the
/// answer is recomputed wherever it is wanted and stored nowhere. It is three
/// integers and a loop over a handful of elements; the cost of being right is
/// nothing.
inline ui::Scene workspace_scene(const WorkshopDoc& d, const Session& s) {
    return ui::resolve(d.elements, ui::Viewport{s.workspace_w, s.workspace_h});
}

/// The inspector for one authored object: the properties, plus the facts that
/// are not properties.
///
/// The whole list is seven calls, and `Width` and `Height` are the two that
/// matter most -- they are the same semantic type, so they share every line of
/// conversion, parsing and refusal wording. A further property of an existing
/// type would be one more line here and nothing else anywhere. That is the old
/// builder's per-row plumbing, replaced -- and the prediction is spent: adding
/// `Context`, a property of a type nothing else uses, costs one line here plus
/// one `TextForm` specialisation, and nothing else in this file.
///
/// The last row is the RESOLVED size, and it is a `show` rather than an `edit`
/// because it is not the maker's to author: it is what the current workspace
/// makes of `Width` and `Height`. A maker looking at `70%` and `33 x 8 cells` is
/// looking at two true things, and the inspector says which is which by what it
/// will let them touch.
///
/// That row reads THE SCENE -- the same resolved scene the canvas is painted
/// from and the same one a click is tested against. Reading its own extent
/// arithmetic instead would make "the inspector agrees with the picture" a
/// property of two functions happening to say the same thing.
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

/// REBUILD THE INSPECTOR WITHOUT TAKING A MAKER'S HANDS OFF IT (HD-5).
///
/// `refocus` above rebuilds the rows AND opens onto the first editable one, which is exactly
/// right whenever the thing being inspected changed -- a new selection, a load, a delete. A
/// new surface extent is not that. The same object is selected, the same properties are on
/// screen, and a maker may be part-way through typing into one of them; the rows still have
/// to be rebuilt (the resolved row closes over the extent it resolves against), so the
/// rebuild has to hand the draft back rather than drop it.
///
/// MEASURED ON THE PRISTINE HD-4 TREE before it was repaired: one `SurfaceExtent` -- a window
/// dragged, which is not a gesture aimed at the inspector at all -- and a half-typed value was
/// gone, silently. Since HD-5 a draft is a caret and a window as well as a string, so the same
/// event would now throw away a maker's place in a long value too.
///
/// BY INDEX AND BY LABEL TOGETHER, which is what makes this safe for the one caller it has:
/// the rows are rebuilt from the SAME document and the SAME selection, so row i is the same
/// property it was, and the label agreeing is the cheap proof. `Row::resume` refuses anything
/// else -- a non-editable row, or a previous row that was not being edited.
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
///
/// THE CANVAS CLIPS AND SAYS NOTHING. `surface::canvas_body`'s `put` drops any
/// cell outside the grid, so a label longer than the room it was given loses its
/// tail with no mark at all. For furniture whose length is a constant somebody
/// checked, that is harmless. For a NOTICE it is not: a notice's length is
/// decided by the document that produced it, and a refusal beheaded at the
/// screen's edge still reads as a finished sentence -- one that means something
/// else, with nothing on screen to say so. Met live with a cycle refusal:
/// shortening that wording answers one producer, and this answers the boundary.
///
/// So the BOUND STAYS -- one line is still one line -- and only the SILENCE
/// goes. This is presentation and nothing else: the semantic message stays whole
/// in `Session::notice`, exactly as its producer wrote it, and a wider screen or
/// a second line would need nothing from anybody but room.
///
/// It is total, including at widths no screen has, and the mark itself is what
/// makes that a real question: `width - 3` underflows below three. A width at or
/// under the mark's own length is therefore spent on as much of the mark as fits,
/// which is still the only honest thing such a width can say.
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

/// How far a wrapped continuation row is indented, so a reader can tell one sentence
/// running on from a new one starting. Two cells: enough to be visible under the sigils
/// every transcript line begins with (`> `, `-- `, `!! `, `^ `, `v `), and cheap enough that
/// a pane loses almost none of its width to it.
inline constexpr std::int64_t kWrapIndent = 2;

/// FIT `text` INTO AS MANY ROWS AS IT NEEDS, at most `width` cells each.
///
/// `fit` above is the answer for a place with exactly ONE row -- it bounds the text and
/// leaves a mark saying it did. This is the answer for a place with SEVERAL, and G-2 exists
/// partly because one of those places had been using the other: the terminal pane's own
/// syntax notice was a hundred and eleven characters and the pane was fifty-six wide, so
/// what a maker who asked how to send a message got was the first fifty-three characters of
/// the answer and `...`. A pane that cannot state its own grammar is a pane that cannot be
/// used, and no amount of extra window would have fixed it -- the truncation was in the
/// fitting, not in the room.
///
/// IT WRAPS AT SPACES, and hard-breaks a word with no space in it, because the alternative
/// -- refusing to break -- silently loses the tail again. Leading spaces on a continuation
/// are eaten (they are the break itself, not content), and a continuation is indented by
/// `kWrapIndent` so the pane still reads as a list of entries rather than as prose.
///
/// TOTAL, including at widths no pane has: a width at or under the indent spends the whole
/// row on text and indents nothing, and every row consumes at least one character, so this
/// terminates for any input.
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

/// A PANE SIZE PROPOSAL, IN CELLS (WIND-2). Saturating on both axes, so a delta arriving off
/// the wire can never leave the number line before the law that judges it gets to see it --
/// which is the one thing `detail::step`/`detail::minus` exist for, and the reason no branch
/// here relies on signed overflow to notice a silly number.
///
/// IT LIVES HERE, BELOW `detail`, ONLY BECAUSE IT SPENDS IT. Everything else about a pane
/// edge is up beside `pane_edge`, where a reader looks for it.
struct PaneSizeProposal {
    std::int64_t w = 0;
    std::int64_t h = 0;
};

inline PaneSizeProposal pane_size_proposal(std::int64_t edge, std::int64_t base_w,
                                           std::int64_t base_h, std::int64_t dx,
                                           std::int64_t dy) noexcept {
    PaneSizeProposal out{base_w, base_h};
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
    }
    if (tall) {
        out.h = detail::step(base_h, upwards ? detail::minus(0, dy) : dy);
    }
    return out;
}

// ---- Direct manipulation, and the boundary policy it needs -----------------------------
//
// A hand reaches a wall in two places -- a drag into the workspace's edge, and a
// resize meeting a limit at BOTH ends of every extent -- so the policy is stated
// here, once, for both gestures:
//
//     A HAND that reaches past what exists           stops at the boundary,
//     (a drag, a nudge, a corner pulled)             authors the boundary value,
//                                                    AND SAYS SO.
//
//     A VALUE a maker WROTE that is not allowed      is refused, the authored
//     (`-1` in X, `500%` in Width)                   state is untouched, and the
//                                                    draft survives so it can be
//                                                    fixed.
//
// The two are different acts and deserve different answers. A hand did not
// propose "-1"; it proposed "further left than there is", and the honest reading
// of that is "as far left as there is". A typed `-1` is a specific claim, and
// silently storing `0` instead would be the tool putting words in a maker's
// mouth -- the exact silent correction the authored/resolved discipline exists to
// prevent, and the reason `doc::` still refuses and never clamps.
//
// The clamp therefore lives HERE and the refusal lives THERE, and the boundary a
// hand stops at is expressed in the document's OWN limits (`doc::kFirstCell`,
// `doc::kMaxCells`, `ui::kMinCells`, and — for a share — whatever the resolver
// says 1% and 100% of this workspace are). So the gesture is not permitted a
// second opinion about what is legal; it is only permitted to stop.

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
///
/// `written` is what the document said about the proposal the gesture finally
/// made. `boundary` is empty unless the gesture had to REDUCE that proposal to
/// reach something authorable at all. Both facts are needed because they are
/// independent: a clamped gesture normally succeeds (something WAS written --
/// the boundary value), and a refusal writes nothing, so "was anything written"
/// and "did the hand hit a wall" cannot be read off one another.
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
///
/// The identity comes from the DOCUMENT's mint, and the new object is immediately
/// the selected one -- which is what makes creation a complete gesture rather
/// than a thing that happens somewhere off screen. The canvas, the object list
/// and the inspector all read the selection, so all three follow from this one
/// assignment; there is no "add it to the list too" step to forget.
/// Returns 0 when the document has no identity left to mint (a document can
/// arrive from a file, and a file can say its mint is spent). Nothing is
/// created and nothing in the session moves -- a gesture that could not happen
/// must not leave the selection somewhere new.
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
///
/// THE POST-DELETE SELECTION RULE, stated once here and tested: the selection
/// moves to whichever object took the deleted one's place in authored order; if
/// the deleted one was last, to the new last; if the document is now empty, to
/// NONE. It is the smallest rule that never leaves a dangling identity and never
/// makes a maker hunt for where they are.
///
/// A refusal (nothing selected, or a selection that has already outlived its
/// object) changes neither the document nor the session.
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
///
/// THE PROPOSAL IS GLOBAL AND THE WRITE IS LOCAL. A hand points at a cell of the
/// workspace; it does not point at "two cells into #1". So the gesture layer
/// takes the hand's answer in the only coordinates a hand has, and projects it
/// into whatever the object's authored coordinates MEAN by subtracting the
/// origin of the frame the resolver says the object is read in. For a
/// root-context object that origin is 0,0 and the projection is the identity,
/// which is why it is easy to miss that it is there at all.
///
/// IT ASKS THE RESOLVER FOR THAT ORIGIN (`ui::frame_in`) rather than working it
/// out. A gesture that reasoned "the source is at 3,2, so subtract 3 and 2"
/// would be a second copy of the geometry, and the second copy is the one that
/// goes stale. It also inherits, free, the answer for the
/// case a source is missing: an empty frame, for an object the resolver did not
/// place, so `doc::move` still judges an ordinary proposal.
///
/// THE CLAMP IS IN GLOBAL CELLS, FOR EVERYBODY. The proposal is reduced to the
/// first cell the workspace has BEFORE the projection, so a hand stops where a
/// maker can see it stop -- at the workspace edge -- whether the object it is
/// dragging measures against the root or against something else. What that stop
/// is authored AS then differs, correctly: at the root it is x = 0, and inside a
/// frame whose origin is at 3 it is x = -3, which is an ordinary authorable
/// offset (see doc::check_coord). The document still judges the
/// result and there is no path here that writes past its refusal.
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
///
/// It proposes a position and lets `place` + `doc::move` decide, exactly as a
/// drag does. Two gestures, one write path.
///
/// It steps the RESOLVED position, not the authored one, and that is not a
/// detail: `place` now speaks workspace cells, and a maker pressing `l` means
/// "one cell to the right on the screen" whatever frame the object is authored
/// in. Stepping the authored offset and stepping the resolved position happen to
/// agree (both add one) -- but only because a frame's origin does not move when
/// its dependent does, and going through the resolved position is what makes the
/// keyboard and the pointer literally one path rather than two that agree.
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
///
/// NOT AN INVERSE, and the name is the first line of the argument.
/// `ui::resolve_extent` is many-to-one -- it floors a share and it clamps an
/// out-of-range one -- so it has no inverse at all. This function does
/// something different and weaker: it AUTHORS A NEW VALUE that this viewport
/// resolves to what the maker asked for. It does not reconstruct the value that
/// was there before and it cannot, because several percentages name the same cell
/// count (at a 48-cell workspace, 59% and 60% are both 28 cells -- 58% is 27, so
/// the range this comment used to name was one wider than the truth). A maker who
/// drags a 60%-wide object out and back gets the same PICTURE and a different
/// NUMBER, and that is a true fact about shares rather than a defect.
///
/// MODE IS PRESERVED. A resize is a direct manipulation of a property the maker
/// already authored, so it writes that property in the spelling they authored it
/// in: a share stays a share, cells stay cells. Converting 60% into 34 cells
/// because cells are easier to compute would silently destroy the only part of
/// the value that survives the next workspace change -- and §24's live evidence
/// is exactly that: after resize the cells object holds its size and the share
/// object still moves with the workspace.
///
/// THE ROUNDING RULE IS NOT A TASTE, IT IS FORCED. The resolver FLOORS, so the
/// projection must take the SMALLEST share this viewport resolves to at least the
/// asked-for size. Choosing the two rules independently is what makes a value
/// walk: `nearest` sends 60% (28 cells at 48) to 58%, which resolves to 27, so
/// merely grabbing an edge and letting go would shrink the object by a cell.
/// `ceil` round-trips exactly wherever a percent is finer than a cell (any
/// workspace of 100 cells or fewer, which is every workspace this tool has), and
/// where it is not, it is still the smallest share that COVERS what was asked.
///
/// AND IT ASKS THE RESOLVER RATHER THAN RE-DERIVING ITS ARITHMETIC. A hundred
/// candidates is nothing, and the payment is large: the projection cannot drift
/// from the resolver, it inherits the resolver's totality over hostile values for
/// free (no `100 * want` to overflow), and "the authored value resolves to what
/// the maker asked for" is true by construction rather than by two functions
/// agreeing. That is the one-place-resolves rule, spent.
///
/// The clamp is the gesture layer's, per this header's boundary policy: `want` is
/// first reduced to the reachable band, and `boundary` says which wall it met.
/// The band is asked of the document's own limits and of the resolver -- never
/// invented here -- so the extent this returns is always one `doc::check_extent`
/// accepts, and `doc::resize` still gets to judge it.
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
///
/// Both extents are projected, then written by ONE `doc::resize`, so the
/// atomicity the document promises is not undone by the gesture proposing twice.
///
/// THE SPAN IS THE CONTEXT'S, and those three words are the whole of what a
/// context costs resizing. `extent_from_drag` asks "which share of this span
/// resolves to what the hand wants", and the span is whatever frame the resolver
/// says this object is read in, asked for with `ui::frame_in`. There is no
/// second projection, no "child resize" path, and no
/// branch on whether an object has a context. The existing operation was simply
/// being handed the wrong context all along, and now it is handed the right one.
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
///
/// It asks for "one more cell than I can see", which is the same question the
/// pointer asks by landing one cell further out, and it goes through the same
/// projection and the same document operation. A maker who grows and shrinks
/// returns to the same size; on a share they may not return to the same NUMBER,
/// because the projection authors the one canonical share for a given cell count.
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
///
/// WORKSHOP PRESENTATION, not authored content. It is not a ui::Element, has no
/// identity of its own, is not a child of anything, and nothing persists it -- it
/// is DERIVED from the selected object's resolved `Placed::rect` every time it is
/// wanted, exactly as the canvas and the hit test are. `zengine::ui` never learns
/// that handles exist; the generic vocabulary already carried this without
/// changing, which is the third phase in a row it has.
///
/// It sits on the selection ring's bottom-right corner cell -- one past the
/// object's own last cell -- so it steals no content cell, it lands where a hand
/// expects a corner grip, and the arithmetic is exact: a pointer AT the handle
/// proposes the size the object already has.
///
/// `shown` is false when it would fall outside the workspace, and that is one
/// fact rather than two: what a maker cannot see, a maker cannot grab. An object
/// authored wider than the workspace is therefore resizable through the inspector
/// and the keyboard but not by hand, which is honest -- the grip is off-screen.
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
///
/// The hit test is the SAME one the canvas is painted from -- `ui::hit` over
/// `workspace_scene` -- so what a maker can see is what they can grab. There is
/// no second geometry test for dragging, which is how a drag and a click cannot
/// come to disagree about which object they are talking about.
///
/// The grabbed point is recorded as an offset from the object's RESOLVED
/// placement — where inside the rectangle on screen the maker took hold — and it
/// is a plain subtraction in the coordinates the pointer already speaks.
///
/// AN AUTHORED POSITION "RELATIVE TO SOMETHING ELSE" IS DRAGGABLE, and the
/// tempting argument that it is not -- the gesture would have to invert the
/// resolver, and the resolver is not invertible -- is true of EXTENTS and false
/// of PLACEMENT. What is not invertible in the resolver is the
/// share arithmetic -- it floors and it clamps, which is why `extent_from_drag`
/// authors a new value rather than recovering the old one. Placement composes by
/// ADDITION, and a sum has an inverse: the hand's global answer minus the
/// context's origin is the authored offset, exactly. So a relative position is
/// draggable without a projection, and it is `place` that performs the one
/// subtraction.
///
/// It does NOT change the selection. Composing that is the caller's gesture: a
/// press on empty space should not silently mean "deselect".
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
///
/// THE PRIORITY LIVES HERE, and that is the point of the function existing. The
/// handle sits one cell outside its own object, so it can sit on top of a
/// neighbour, and something has to say which one a press means. `ui::hit` must
/// not: it answers "which authored element is under this cell", and a handle is
/// not an authored element -- teaching the package about affordances would be
/// teaching a shared vocabulary one application's furniture. So the package still
/// gives the whole answer about the DOCUMENT and Workshop simply consults its own
/// affordance first, in Workshop's own gesture, where a suite can drive it.
inline std::int64_t take_hold(WorkshopDoc& d, Session& s, std::int64_t cx, std::int64_t cy) {
    const Handle handle = size_handle(d, s);
    if (handle.shown && handle.x == cx && handle.y == cy) {
        s.drag = Drag{true, true, handle.id, 0, 0};
        return handle.id;
    }
    return begin_drag(d, s, cx, cy);
}

/// Where the gesture in flight now proposes the object should BE, or how big it
/// should be — committed through the document's one position operation or its one
/// size operation.
///
/// It writes AUTHORED state, and only ever that. Nothing here touches a Rect, a
/// Placed or a Scene: those are the observation, they are rebuilt from the
/// authored state on the next paint, and a gesture that moved or stretched them
/// would have changed the picture without changing the thing.
///
/// The resize arm reads the object's own left/top edge and asks for
/// `pointer - edge` cells, which is exact because the handle sits one cell past
/// the object: a pointer resting ON the handle proposes the size it already has.
/// The subtraction saturates for the same reason `begin_drag`'s does -- both
/// terms are values this weave does not own.
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
//
// This lives here and not in the weave because it is the only NEW arithmetic on
// the pointer path, it is the arithmetic a hostile value could break, and code in
// workshop.cpp's anonymous namespace is code no suite can reach. (The weave's
// message handlers themselves are still unreachable that way -- see the report;
// what this relocation buys is that the part with an undefined-behaviour edge is
// not among them.)
//
// TWO TRANSLATIONS ARE STACKED HERE, and only the second is Workshop's:
//
//   medium position -> CANVAS cell    the Skin's layout, and the Surface
//                                     package's (surface/pointing.hpp). It is
//                                     not a number an application can know.
//   canvas cell -> WORKSPACE cell     Workshop's own composition: where this
//                                     application put its workspace on its screen.
//
// The whole contract, both halves and who owns each, is
// docs/reference/pointer-spaces.md.

/// The canvas cell a reported pointer position lands on, whatever medium
/// reported it -- or nothing, for a space this application cannot place.
///
/// THIS IS THE PAIRING, AND THE PAIRING IS THE HONEST COST. Each medium's
/// transform is the Surface package's (it authored the layout); which transform
/// applies to THIS event is decided from the `space` the backend stamped, and
/// that decision is here because nothing else in the process can make it --
/// nothing can ask the active Skin what its presentation context is. So Workshop
/// states the pairing rather than pretending it is derived, and the statement is
/// exactly one switch in one place. The contract, and the day a second graphical
/// Skin makes this switch unable to tell two layouts apart, are
/// docs/reference/pointer-spaces.md.
///
/// A SPACE THIS APPLICATION DOES NOT RECOGNISE IS IGNORED, never guessed at.
/// That is the whole reason `space` exists: a terminal cell and an SDL pixel are
/// both small non-negative integers, and assuming is how a click lands 12 cells
/// from where a maker pointed.
struct PointedAt {
    bool understood = false;
    surface::CanvasPoint cell;
};

inline PointedAt canvas_point_of(std::int64_t space, std::int64_t x, std::int64_t y) noexcept {
    if (space == input::space::kCells) {
        return PointedAt{true, surface::canvas_of_terminal_cells(x, y)};
    }
    if (space == input::space::kPixels) {
        return PointedAt{true, surface::canvas_of_window_pixels(x, y)};
    }
    return PointedAt{};
}

/// WHERE A POINTER LANDED INSIDE A BOUNDED TEXT REGION, in that region's own prose (HD-3).
///
/// `canvas_point_of` one lattice finer, and the SAME pairing statement: which transform
/// applies is decided from the `space` the backend stamped, because nothing in this process
/// can ask the active Skin what its layout is.
///
/// THE RAW PIXEL IS USED AS A RAW PIXEL AND IS NEVER ROUNDED TO A CELL FIRST. That is the
/// whole reason the sub-cell precision is on the wire (`input::PointerButton`'s kPixels): a
/// press at pixel 271 on a face whose advance is 8 is a column, exactly, and converting to a
/// cell of twelve pixels and back would lose the answer and then invent a worse one.
/// `prose_column_of_pixel`/`prose_row_of_pixel` (surface/pointing.hpp) are the pure
/// arithmetic, pinned since HD-1 and wired here for the first time.
///
/// A CELL MEDIUM'S POSITION IS ALREADY A CHARACTER, so it takes the other route entirely --
/// `canvas_of_terminal_cells` and a subtraction, with no division by a pixel size that its
/// numbers were never in. Feeding a terminal's column to the pixel helper would divide a
/// column by twelve, which is the exact class of mistake `space` exists to prevent.
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
///
/// The saturation stays, and for the unchanged reason: the numbers come off the
/// wire from whichever weave holds the input role, a backend is a weave like any
/// other, and `INT64_MIN - 3` is undefined behaviour produced by data. The
/// saturated end is far outside any canvas, which already means "nothing there".
inline std::int64_t workspace_cell_x(std::int64_t canvas_x) noexcept {
    return detail::minus(canvas_x, kWorkspaceX);
}
inline std::int64_t workspace_cell_y(std::int64_t canvas_y) noexcept {
    return detail::minus(canvas_y, kWorkspaceY);
}

// ---- What the OBJECTS panel can show, and what it must SAY it cannot ---------------------
//
// A bounded place is any number of lines tall and a document is any size, so some
// documents do not fit. That was always true and is still fine. What was not
// fine is what the panel did about it: it stopped after the fifth line and said
// nothing. A maker with six objects saw five of them, with no marker -- and with
// the selection marker on NONE of them, while the status line said `6 objects |
// selected #6` and the inspector said `Identity #6`. Three statements on one
// screen that could not all be true, and the only one a maker could act on was
// the one that was wrong. Reachable by pressing `n` six times; found by a cold
// review, seven phases in, because no run and no case had ever painted a
// six-object document.
//
// It is the argument the empty document already won three lines further down:
// a panel that merely goes blank is indistinguishable from a tool that has
// broken. A panel that quietly stops is worse, because it looks like it worked.
// The Loom console reached the same rule from the other side: retained history
// is BOUNDED, and its truncation is OBSERVABLE.
//
// So the bound stays exactly where it was, and only the silence goes.
//
// HD-7 MOVED THE BOUND AND NOT THE RULE. `kListRows = 5` is gone: how many objects the panel
// shows is `InfoBodyPlace::objects_rows`, resolved from the room the active medium reports.
// This function did not change a line for that -- it always took its capacity as an argument,
// which is why a fixed five and a measured twenty are the same call.

/// Which members of an ordered collection a bounded place is showing, and how
/// many it is leaving out on each side of them.
///
/// WORKSHOP PRESENTATION, like `Handle`: derived every paint and stored nowhere.
/// It is not in `ui/`, because a shared vocabulary should not learn one
/// application's panel height; and it is not in `WorkshopDoc`, because how many
/// objects fit on a screen is not a fact about the document.
///
/// It is a WINDOW, not a page. There is no scroll position, no anchor, no
/// session field and nothing to invalidate: the SELECTION decides what is
/// visible, and the selection is already session state. That is the whole reason
/// this is four numbers instead of a scroll view -- a stored scroll offset would
/// be a second opinion about where a maker is looking, and it is the copy that
/// goes stale.
///
/// TWO CONSUMERS SINCE HD-6, and the second is what turned the words above from
/// "the OBJECTS panel's window" into a rule. The Inspector's property body is
/// bounded for the first time in HD-6 and needed exactly this: an ordered
/// collection, one member that must stay on screen, a capacity that the active
/// medium decides, and every omission counted on the side it happened. It reuses
/// the FUNCTION rather than the shape of it -- see `info_body_place` -- so
/// there is one rule about what a bounded list may hide and one wording for
/// saying so (`omitted_text`), and a change to either moves both sections.
///
/// SINCE HD-7 BOTH CONSUMERS ARE IN ONE REGION and both capacities come from one
/// `fit_region`, which is the first time the two calls have been visibly the same
/// call. Nothing here changed for it. The one thing that DID change is that a
/// capacity below three is now reachable -- see the `rows < 3` branch.
///
/// THE COMPLETION LIST IS NOT A THIRD CONSUMER, and the difference is worth
/// naming rather than glossing: `completion_first_shown` anchors to the TAIL and
/// never draws a `before` marker, because that list spends its first row on a
/// heading which already says `3-5 of 9`. Same problem, genuinely different
/// rule; sharing them would mean one of the two lying about what it is showing.
struct ListWindow {
    std::size_t first = 0;  ///< the first member shown, as a position in the collection's order
    std::size_t count = 0;  ///< how many are shown, contiguously, in that order
    std::size_t before = 0; ///< how many the place left out ahead of them
    std::size_t after = 0;  ///< how many it left out behind them
};

/// What `rows` lines can honestly show of `total` members while the
/// `selected_at`'th is selected.
///
/// THREE RULES, in this order:
///
///   1. A collection that FITS is shown whole, with no marker and no chrome at
///      all. The simple case stays the simple case: zero through `rows`
///      members look exactly as they did before this function existed.
///   2. The SELECTED member is always in the window. It is the object the status
///      line and the inspector are both already naming -- or, for the Inspector,
///      the row a maker's cursor or live draft is on -- so a list that omits it
///      does not merely hide a member: it contradicts the rest of the screen,
///      which is the defect rather than a symptom of it.
///   3. Every member left out is COUNTED, on the side it was left out on, and
///      each count spends one of the `rows`. The markers come out of the budget
///      rather than being extra lines beneath it: a bound that grows when it is
///      exceeded is not a bound.
///
/// DOCUMENT ORDER IS NEVER TOUCHED. The window is a contiguous run of the
/// document as it is written, so the list still reads the way the file reads,
/// the way `paint` paints, and the way `ui::hit` answers. Sorting by identity,
/// name, context, dependency or selection recency would each be this one panel
/// telling a maker a different story about the same document. Document order and
/// dependency order are separated deliberately (ui::resolve orders its own work
/// by dependency and emits in document order), and a presentation choice is not
/// the place to quietly rejoin them.
///
/// IT ANCHORS AT THE TOP for as long as it can, then at the BOTTOM, and takes a
/// middle window only when neither end reaches the selection. So the first
/// screenful of a growing document stays still -- which is what a maker pressing
/// `n` is looking at -- and when it finally must move it moves as little as the
/// selection requires.
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
        // IT WAS UNREACHABLE AT `kListRows = 5` AND IT IS REACHABLE NOW (HD-7). A share of
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

/// What one omission marker says. `... 2 earlier` / `... 4 more`: a count,
/// because "there are more" without a number tells a maker only that they are
/// lost, and a direction, because which end they are at is the difference
/// between scrolling and hunting. Kept as one function so the two markers cannot
/// come to be worded by two different hands.
inline std::string omitted_text(std::size_t how_many, const char* which) {
    return "... " + std::to_string(how_many) + " " + which;
}

// ---- Rendering one participant's record ------------------------------------------------
//
// THE TRANSCRIPT IS A MODEL, NOT OUTPUT, and Loom says so in as many words: entries carry
// structured facts -- kind, shape identity, the bus-stamped sender, the addressing, the
// correlation -- and NO baked prose, so that two renderers cannot come to disagree about what
// a message entry says by disagreeing about a string the core happened to write. These three
// functions are Workshop's renderer, and they are pure, so the suite pins the exact words.
//
// `loom::safe_terminal_text` is deliberately NOT used. It is a rule for a renderer that has a
// real terminal to protect from escape sequences; this one paints into a SurfaceCanvas that
// every Skin rasterizes cell by cell, and showing a maker backslash-escapes for no reason is
// the mistake its own comment names.

/// WHERE A SUBMITTED MESSAGE WAS ADDRESSED, in the SAME three sigils the command line reads.
///
/// Not a coincidence and not a second table: `#N` / `@office` / `*` is Loom's own address
/// grammar (`loom::parse_address`), so a maker can read a line out of the transcript and type
/// it back in. A publication additionally says how many deliveries were QUEUED -- the one
/// delivery fact an ordinary sender is given, and never a count of what landed.
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
///
/// SUBMITTED SAYS SUBMITTED, and no line says more than that. A participant is never told its
/// send's fate -- not delivered, not refused, not dropped for want of a target -- so an
/// outbound line carries what was authored, where it was aimed, and Loom's own word for the
/// only thing that is known. What that word MEANS is said once, by `terminal_legend` below,
/// on a row the pane always shows. Once rather than on every line, because a fifty-cell
/// clause repeated down every row of the pane costs the room the record itself needs -- and
/// wrapping only makes that worse, since a clause that no longer fits takes a second row from
/// the record too. Also because a rule with one owner cannot come to be worded two ways.
///
/// The suite additionally asserts that nothing this renderer produces contains the word
/// "delivered": a renderer is the one place the never-say-delivered contract can be broken by
/// prose alone.
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
///
/// This is the pane's whole copy of the never-say-delivered rule, and it is deliberately a
/// STANDING statement rather than a per-line one. The standalone terminal words it its own
/// way and this words it this way; what makes two renderers safe is that neither is inventing
/// a FACT -- the transcript has no outcome field to read, so no renderer can say "delivered"
/// by accident, only by writing the word.
inline std::string terminal_legend() {
    return "SUBMITTED = authored; a sender is not told its fate";
}

/// ONE TRANSCRIPT ENTRY AS THE ROWS A PANE THIS WIDE SPENDS ON IT.
///
/// The renderer above says what an entry IS; this says how much of a pane it takes, and the
/// separation is the whole answer to G-2's terminal defect. `loom::Transcript` keeps
/// structured facts and Workshop's renderer turns one into a sentence of whatever length
/// that sentence needs; NOTHING upstream is shortened to suit a pane. The pane then spends as
/// many of its own rows as the sentence needs, which is a presentation decision made at the
/// presentation boundary and nowhere else -- exactly the division `Session::notice` and
/// `detail::fit` already keep for the notice line, applied to a place that has more than one
/// row to spend.
///
/// Before this, an entry was `fit` into ONE row: the pane's own syntax notice arrived as its
/// first fifty-three characters and `...`, so the one thing a maker could ask this pane --
/// how do I say something -- was the one thing it could not answer.
inline std::vector<std::string> terminal_wrapped(const loom::TranscriptEntry& e,
                                                 std::int64_t width) {
    return detail::wrap(terminal_line(e), width);
}

/// HOW MANY OF THE NEWEST ENTRIES A PANE THIS WIDE AND THIS TALL CAN SHOW WHOLE.
///
/// `entries` is oldest-first, as `Transcript::tail` hands it over. Counting from the NEWEST
/// backwards is the direction that matters: a pane is a window onto the end of a record, so
/// what must survive a shortage is the most recent thing that happened, and the omission
/// marker says how much did not.
///
/// AT LEAST ONE, WHEN THERE IS ONE. An entry whose sentence is taller than the whole pane
/// would otherwise show nothing at all -- a pane gone blank because its newest line was too
/// long is indistinguishable from a broken tool. It is shown from its beginning and the rows
/// that do not fit are dropped, which is the one place in this pane where something is lost
/// without a mark of its own; it takes an entry of more than `width * rows` characters to
/// reach, and the omission marker is still telling the truth about the ENTRIES.
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
///
/// `earlier` scrolled off the top of the pane and is still in the participant's record;
/// `dropped` was evicted by the transcript's own bound and is gone for good. Z0a's
/// rule, and the reason it is not one number: "you cannot see it here" and "nobody can see it
/// any more" are answers to different questions, and a maker deciding whether to widen a pane
/// or re-run a command needs to know which one they are looking at.
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

// ---- The editable line, resolved ONCE (HD-3) --------------------------------------------
//
// THE GEOMETRY THAT DREW A THING AND THE GEOMETRY THAT HITS IT MUST BE THE SAME GEOMETRY.
// That is the one-measurer rule (G-2, HD-1) arriving at interaction, and it is the same
// argument in a new place: the pane's omission marker is only true if one party measured the
// wrap, and a pointer only lands where a maker aimed if one party measured the input row.
// So there is no `paint_input_bounds()` beside a `click_input_bounds()` here -- there is
// `terminal_input_place`, and the painter, the caret and the press all call it.

/// THE PROMPT, in columns: the `> ` before the editable text.
///
/// It is a constant rather than a `strlen` at each site because it is the offset between
/// "the third column of this row" and "the first byte of the line", which is a fact three
/// different pieces of arithmetic need and none of them owns.
inline constexpr std::int64_t kTerminalPromptCols = 2;

/// THE COLUMN THE INSERTION POINT SITS IN, kept out of the editable line's own budget (HD-4).
///
/// A caret is BETWEEN two characters, so the one after the last character of a full row needs
/// somewhere to be — and the two media answer that differently. A window has the region's
/// inset: `plan_caret` puts a `kCaretWidthPx` bar at column `fit.columns` and it lands inside
/// the viewport, which is a property `surface/region.hpp` states and pins. A CELL medium has
/// no inset and no half-cells: `project_text_regions` inserts the mark as a character and then
/// cuts the row at the region's width, so a mark past the last cell is cut off with it. HD-3
/// recorded exactly that and deferred it, because rescuing it inside the projection would have
/// been inventing a scroll for every consumer at once.
///
/// So the scroll invented HERE pays for it here: the line's own capacity is one column short
/// of the row, which leaves the far caret a cell of its own in a terminal and a column of
/// slack in a window. One rule, both media, and the two therefore scroll to the same place —
/// a capacity that branched on the medium would be a second answer, and the branch would be
/// invisible to anyone reading either projection.
inline constexpr std::int64_t kTerminalCaretCols = 1;

/// WHERE THE PANE'S EDITABLE LINE IS — the pane's region, the row inside it, and the column
/// its first byte starts at.
///
/// `fit` is the pane's own `RegionFit`: the same resolution `screen_of` performed to decide
/// how much prose the pane holds, recomputed from the same `Screen` rather than carried, so
/// there is exactly one function that can be wrong. Everything a caret or a press needs is
/// derivable from these five numbers and nothing else.
struct TerminalInputPlace {
    std::int64_t region_x = 0; ///< the pane's own cell origin — a region coordinate
    std::int64_t region_y = 0;
    surface::RegionFit fit{};
    std::int64_t prose_row = 0;   ///< the pane's LAST prose row: the line being typed
    std::int64_t first_column = kTerminalPromptCols; ///< where the line's first byte sits
    /// COLUMNS THE VISIBLE PART OF THE LINE MAY OCCUPY — prompt excluded, and since HD-4 the
    /// caret's own column excluded too. It is the ONE capacity: the slice the painter cuts,
    /// the window `keep_caret_visible` reconciles and the room a press is answered against
    /// are all this number, and there is deliberately no second count of the columns beside
    /// it for paint, for input or for hit testing.
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
///
/// One column per byte, which is what every other step of this presentation counts
/// (`detail::fit` cuts at a byte, the cell projection is "one cell per byte, as ever") — so
/// this is not a simplification, it is the same measurer.
///
/// SINCE HD-5 IT IS THE PROMPT PLUS THE COMPONENT'S OWN ANSWER, and nothing else. `caret -
/// first_visible` is `TextBox::caret_column()`, computed once inside the thing that owns
/// both numbers; what is left here is the only part that is about a TERMINAL, which is that
/// this pane's prose begins with `> `.
///
/// THE BOX RATHER THAN TWO INDICES, and that is HD-4's parameter lesson arriving at its
/// conclusion. HD-4 gave this function a NON-DEFAULTED `first_visible` precisely so that no
/// call site could keep the old spelling and be silently right until the first line long
/// enough to scroll. Taking the component instead makes that hazard unsayable: there is no
/// argument left to forget, and no way to hand it a caret from one line and a window from
/// another.
inline std::int64_t terminal_caret_column(const TerminalInputPlace& p,
                                          const component::TextBox& box) noexcept {
    return surface::add_cells(p.first_column, static_cast<std::int64_t>(box.caret_column()));
}

/// THE BYTE INDEX A PROSE COLUMN NAMES, clamped into the line the pane is showing.
///
/// The mirror of `terminal_caret_column` and, since HD-5, the same one-line shape: take the
/// prompt off the column, and ask the component what byte the rest of it names. The three
/// boundary answers are `TextBox::position_at_column`'s and are written down there —
///
///     a column before or inside the prompt  -> first_visible (the maker aimed at what they
///                                                             can see)
///     a column past the last byte           -> line length   (the end of the WHOLE line)
///
/// — and on an unscrolled line they are byte-for-byte the three answers HD-3 wrote down,
/// which is why every pre-existing pin still watches what it used to.
///
/// Snapping off a character's middle is `TextBox::place`'s, so this returns a byte index and
/// never pretends to be one.
inline std::size_t terminal_caret_of_column(const TerminalInputPlace& p,
                                            const component::TextBox& box,
                                            std::int64_t column) noexcept {
    return box.position_at_column(surface::sub_px(column, p.first_column));
}

/// IS THIS PROSE POSITION ON THE EDITABLE LINE AT ALL?
///
/// The row is the whole test, and the column is deliberately NOT: a press anywhere along the
/// pane's last prose row is a press on the line, including the inset before the prompt and
/// the empty room past the last character. A maker aiming at the end of a short command
/// clicks in the empty space after it, and refusing that would be refusing the most obvious
/// gesture the row has.
inline constexpr bool terminal_input_hit(const TerminalInputPlace& p, std::int64_t column,
                                         std::int64_t row) noexcept {
    return row == p.prose_row && column >= 0 && column <= p.fit.columns;
}

// ---- The completion list, inside the pane it belongs to (HD-2) --------------------------
//
// A SECOND BOUNDED REGION, PLACED OVER THE FIRST, AND NOT A SECOND PANEL. It is the
// Terminal's own discovery surface: it appears while a line is being composed, covers some
// transcript rows while it is there, and is gone on the next repaint when it is not. Nothing
// about it is a Workshop entity -- no panel kind, no picker row, no placement rule, no
// presence to remember -- because a maker never opens or closes it. It is what the pane is
// SAYING, and the pane already owns its own interior.
//
// IT COVERS ROWS; IT DOES NOT TAKE THEM. The transcript snapshot and the omission marker are
// computed from `terminal_rows` exactly as they were before this existed, so "... 4 earlier"
// counts what the pane could not SHOW rather than what the list happened to be sitting on. A
// list that shrank the transcript budget would be the honest alternative and it would make
// the pane's own sentence depend on how much a maker had typed; covering is the reading in
// which the two facts stay independent.

/// WHERE THE COMPLETION LIST SITS, in canvas cells, and how much prose it holds.
///
/// THE HARD PART IS THAT A REGION IS PLACED IN CELLS AND FILLED IN PROSE ROWS, and on a
/// medium that sets real type those two are different lattices. The pane's input line is its
/// LAST prose row, which in a window begins part-way down some cell; a list anchored to a
/// cell boundary can therefore only guarantee it clears the input line by ending at the top
/// of the cell that row begins in. That is what this computes, and it errs upward on purpose
/// -- a few pixels of the pane showing under the list is a gap, and a few pixels of the list
/// over the input line is a maker who cannot see what they are typing.
///
/// TOTAL over every std::int64_t on both arguments: the metric on the screen arrived on the
/// bus, and `wanted` is a count of candidates a vocabulary produced.
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
///
/// MEASURED, NOT REASONED: with a floor of two, `send * s` showed nothing at all. Five
/// shapes are known, none begins with a lowercase `s`, so there were no candidate rows to
/// pair with the heading and the region was refused for being one row tall — leaving a
/// maker who had just been told nothing to distinguish "your prefix matches nothing here"
/// from "completion is broken". That sentence is the single most useful thing this list
/// ever says, because it is the one that tells a maker the vocabulary does not hold what
/// they are reaching for.
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

/// THE LIST AS ROWS — heading first, then as many candidates as the place holds, with the
/// selected one marked.
///
/// WINDOWED AROUND THE SELECTION, and the heading says which slice it is showing. A list
/// that scrolled without saying so would be the omission lie one region over: a maker on
/// candidate seven of nine, looking at three rows, must be able to tell that from a
/// vocabulary with three entries in it.
///
/// THE MARKER IS NOT DECORATION. `>` is what says "this one" on a medium with no colour at
/// all, which is the same argument `glyph_for_role` makes in the terminal Skin and the
/// reason a background alone would not be enough. The background is the graphical answer to
/// the same question; both are said, so neither has to carry it alone.
/// WHICH CANDIDATE THE FIRST VISIBLE ROW SHOWS — the windowing, written once (HD-3).
///
/// It used to live inside `completion_rows` and had one consumer. A pointer press has to ask
/// the same question backwards ("which candidate is this row?"), and a second copy of this
/// arithmetic is how a maker comes to click one row and select another — a defect that would
/// appear only after the list had scrolled, which is to say only when nobody was looking for
/// it. `capacity` is the whole list's, heading included, exactly as `completion_rows` takes
/// it.
inline constexpr std::size_t completion_first_shown(std::size_t selected,
                                                    std::size_t capacity) noexcept {
    if (capacity <= 1) {
        return 0; // no room for a candidate row at all: the heading is the whole list
    }
    const std::size_t room = capacity - 1; // the heading always costs one
    return selected >= room ? selected - room + 1 : 0;
}

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
///
/// SINCE HD-1 THIS PANE IS THE ONE PLACE IN WORKSHOP THAT PUBLISHES A TEXT REGION, and it is
/// deliberately the only one. A region is a grant of bounds whose interior the active medium
/// may set in real type instead of in cells; the Terminal asked for it because the Terminal
/// is the panel a person READS, and no other panel had to change, learn a metric, or move a
/// coordinate to let it. The backdrop rect underneath is unchanged and still a `SurfaceRect`
/// in cells -- what got finer is the interior, not the furniture.
///
/// EVERY ROW IS STILL WRITTEN, including the ones with nothing in them, and the reason is the
/// one the first live rasterization taught: a row left unsaid shows whatever is under the
/// pane, and a short session rendered as an overlay with holes punched through it into the
/// workspace behind. Region rows carry that for free now -- region.hpp's cell projection
/// fills the region's whole height, so a pane that has less to say is still a pane.
///
/// WHAT IT NO LONGER DOES IS PAD. Padding each row to the pane's width was how a character
/// medium was made to erase; that is the PROJECTION's job now and it is done for every
/// medium at once, which is one fewer thing a publisher has to know about the media it might
/// land on. What stays here is the truncation, because a row longer than the pane is a
/// PRESENTATION decision -- the pane is choosing what it can show -- and the number it
/// truncates to is the same `terminal_cols` its snapshot chose entries with.
inline void paint_terminal(surface::SurfaceLayer& layer, const TerminalPane& t,
                           const Screen& sc) {
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
        t.attached ? "TERMINAL -- weave #" + std::to_string(t.id.value) +
                         "  (shift+space closes)"
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
    // THE LINE BEING TYPED, AND THE CARET SAID SEPARATELY FROM IT (HD-3).
    //
    // Until HD-3 the caret was a `_` this function appended, which was truthful only because
    // the caret could only ever be at the end. It can be anywhere now, so the position is
    // published as a fact ABOUT the region (`caret_row`/`caret_col`) and each medium answers
    // it in its own type: a window fills a bar between two characters, and the cell
    // projection inserts `_` at the same column -- which, for a caret at the end of the line,
    // is byte-for-byte the row this function used to write itself.
    //
    // AND WHILE THERE IS NOTHING ON IT, IT NAMES THE GESTURE THAT ANSWERS "what can I
    // say here" (HD-2). It is on this row rather than in the legend because it is
    // about what to do NEXT rather than about what a word means, and because it
    // erases itself: the moment a maker types anything the line has their text on it
    // and the list is doing the same job better. A tool whose discovery gesture is
    // itself undiscoverable has moved the problem rather than solved it.
    //
    // AND IT IS A WINDOW ONTO THE LINE RATHER THAN THE WHOLE OF IT (HD-4). `visible` is the
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
        prompting ? ">    tab: what can this terminal say?"
                  : "> " + t.input.visible(typing.columns),
        t.attached ? surface::role::kAccent : surface::role::kAlert);
    // ONE MEASURER: the column comes from the same resolution the row was written against,
    // and the same one a press is answered with, so a caret cannot land where the text is
    // not and a click cannot land where the caret would not. Since HD-4 that resolution
    // includes WHICH PART of the line is on the row, and all three read the one answer
    // `TerminalInput` holds rather than each deciding for itself.
    pane.caret_row = typing.prose_row;
    pane.caret_col = terminal_caret_column(typing, t.input);

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
//
// THE SAME MECHANISM THE TERMINAL OVERLAY USES, and deliberately not a second one: a
// backdrop rect for media that draw glyphs rather than cells, then rows padded to the
// panel's full width so that in a character medium a space erases what is underneath. Two
// overlay mechanisms would be two answers to "what does an overlay do about the furniture
// below it", and this file has already answered it once.

/// THE BACKDROP OF A PANEL: its whole bounds, in one rect.
///
/// EVERY PANEL HAS ONE, and PNL-2a is where that became true. BLD-0 predicted that this and
/// `paint_panel_row` were "the shared half of every panel kind, so a second kind writes its
/// content and inherits its shape"; PNL-0 measured that false, because the second kind was a
/// column of bare labels with neither. Only half of that measurement survived contact with a
/// pointer. PNL-2 gave a panel's bounds the maker's HAND as well as their eye -- a press
/// inside them stops there -- and then found the picture disagreeing with the refusal: an
/// object dragged under the Info column painted its body and its selection ring straight
/// THROUGH the panel, with the panel's own words on top of it. One rectangle, visibly empty
/// and demonstrably occupied.
///
/// So a backdrop is what a PLACE looks like when something is in it, and it belongs to every
/// kind that occupies one. It is emphatically NOT a per-cell painted mask: occlusion stays a
/// question about BOUNDS (`occupied_at`), because a mask would make what a maker can press
/// depend on the length of a label.
///
/// `paint_panel_row` below is still the OVERLAY's shape and not every panel's -- that half of
/// BLD-0's prediction is still a prediction. Info writes bare labels, and padding them to the
/// bounds' width would erase the screen-level hint that shares the side region's top row.
inline void paint_panel_frame(surface::SurfaceLayer& layer, const ui::Rect& b) {
    layer.rects.push_back(surface::SurfaceRect{b.x, b.y, b.w, b.h, surface::role::kMuted});
}

/// One row of an overlaid panel, fitted and padded to its bounds' width.
///
/// THE CELL-LATTICE SPELLING OF A PANEL ROW, and since TYPE-0 it has ONE consumer left: the
/// Builder, whose nine rows are a fixed composition against a nine-cell slot (see
/// `paint_builder`). Every other panel says its rows through `panel_prose_place` below, which
/// is what lets a medium that owns a face set them in it.
inline void paint_panel_row(surface::SurfaceLayer& layer, const ui::Rect& b, std::int64_t line,
                            const std::string& text, std::int64_t role) {
    layer.labels.push_back(surface::SurfaceLabel{
        b.x, b.y + line, detail::pad(detail::fit(text, b.w), static_cast<std::size_t>(b.w)),
        role});
}

/// A PANEL WHOSE WHOLE BODY IS ONE BOUNDED REGION OF PROSE, RESOLVED ONCE (TYPE-0).
///
/// It is `info_body_place`'s and `external_body_place`'s one shared sentence, for the panels
/// that have no chrome outside the region: a panel's bounds plus the ACTIVE medium's text
/// metric become a row budget and a column width, through `fit_region`, which is the same one
/// call the medium will resolve the same rectangle with. Nothing here multiplies a metric and
/// nothing downstream is allowed to -- a painter spends `rows` and `columns` and never learns
/// what a pixel is.
///
/// WHY A PANEL IS ONE REGION AND NOT A COLUMN OF LABELS. A label is one cell per byte in every
/// medium; a region is the one shape on this canvas whose interior a medium may set in its own
/// type. So the difference between `paint_panel_row` and this is the difference between a
/// panel a maker reads in a 5x5 bitmap letterform and a panel they read in the face the
/// Inspector beside it already uses. The cell projection of a region is one row per cell row,
/// padded to the region's width and cut at it -- byte-for-byte what `paint_panel_row` wrote --
/// so a character medium cannot tell which spelling a panel chose.
///
/// A REGION TAKES ITS RECTANGLE, which is why this is only for panels whose whole bounds are
/// theirs. The backdrop rect underneath it is erased in both media (spaces in a character
/// medium, the region's own ground in a graphical one) -- exactly what a panel's padded rows
/// already did, and exactly what a presentation sharing its rectangle with something else
/// could not survive.
struct PanelProsePlace {
    bool present = false;
    std::int64_t rows = 0;    ///< prose rows of the ACTIVE medium's type that fit the panel
    std::int64_t columns = 0; ///< ...and how many characters fit across one of them
};

/// The one call. TOTAL over the rectangle, because a closed panel answers with an empty one
/// (`bounds_of`) and a screen may be small enough to hold no row at all.
inline PanelProsePlace panel_prose_place(const ui::Rect& b, const Screen& sc) {
    PanelProsePlace p;
    if (b.w <= 0 || b.h <= 0) {
        return p;
    }
    const surface::RegionFit fit =
        surface::fit_region(b.x, b.y, b.w, b.h, sc.text_advance_px, sc.text_line_px);
    p.rows = fit.rows;
    p.columns = fit.columns;
    p.present = p.rows > 0 && p.columns > 0;
    return p;
}

/// The region a `PanelProsePlace` was resolved for, empty and ready for its rows.
inline surface::SurfaceTextRegion panel_prose_region(const ui::Rect& b) {
    surface::SurfaceTextRegion region;
    region.x = b.x;
    region.y = b.y;
    region.w = b.w;
    region.h = b.h;
    return region;
}

/// A panel's own field: a fixed-width label and its value, so the values line up down the
/// panel and a maker reads a column rather than a paragraph.
inline std::string panel_field(const char* label, const std::string& value) {
    return detail::pad(label, 9) + value;
}

/// A field whose value is longer than a row: wrapped across a fixed row budget, and MARKED
/// when the budget ran out before the sentence did.
///
/// The rows are a fixed number because the panel is, so this cannot grow the furniture --
/// which means it can and does run out of room, and saying so is the whole job. `wrap`
/// already breaks on words and indents its continuations; what it has no way to express is
/// "there was more", and a block silently ending mid-thought is exactly the failure
/// `detail::fit` exists to prevent one row at a time.
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
///
/// Every value on it came off the bus as `builder::BuildStatus`, published by the tool. The
/// panel computes none of them, remembers none of them past a close, and states the one
/// thing that is genuinely its own: whether the tool has answered it yet.
///
/// IT IS HANDED ITS BOUNDS AND DOES NOT KNOW WHERE THEY CAME FROM (PNL-1). Every number
/// below is either a row of its own content or a reading off `b` -- there is no `kStackX` in
/// this function any more, and no `kStackRows`, so what it means to move this panel or to
/// give it a different amount of room is entirely `placement_bounds`'s business.
inline void paint_builder(surface::SurfaceLayer& layer, const BuilderPane& pane,
                          const ui::Rect& b) {
    paint_panel_frame(layer, b);
    const auto row = [&layer, &b](std::int64_t line, const std::string& text,
                                 std::int64_t role) {
        paint_panel_row(layer, b, line, text, role);
    };
    // THE HEADER NAMES THE OFFICE IT IS PRESENTING. The same discipline the terminal pane's
    // header follows: a presentation that shows somebody else's facts without saying whose
    // is a presentation that will eventually be read as its own.
    // `p removes` and not `x closes`: PNL-0 gave panel presence one owner, and it is the
    // picker. A panel's own header advertising its own removal key is exactly the per-panel
    // binding the second kind made untenable.
    row(0, std::string("BUILDER @") + builder::kBuilderRole + " -- b builds, p removes",
        surface::role::kAccent);

    if (!pane.heard) {
        // NOT THE SAME AS "NEVER BUILT", and the panel must not show it as though it were.
        // This is a fact about this panel -- it has asked and is waiting -- and the target's
        // own history is not knowable from here until the tool says it.
        row(1, panel_field("target", "(the Builder has not answered yet)"),
            surface::role::kMuted);
        for (std::int64_t i = 2; i < b.h; ++i) {
            row(i, std::string(), surface::role::kFill);
        }
        return;
    }

    const builder::BuildStatus& s = pane.shown;
    row(1, panel_field("target", s.target), surface::role::kFill);
    // WHAT THIS PANEL IS WATCHING beats what it was last told. `awaiting` is the panel's own
    // fact and it is the truer one while it holds: the tool's last OUTCOME is still the
    // previous build's, and showing that while a new one is running would answer "what
    // happened on the last build" with a sentence about the wrong build.
    //
    // THE OPERATION AND THE OUTPUT COUNT SHARE THIS ROW (ASYNC-1), and they are on the panel
    // for one reason: they are what make a running build VISIBLE rather than asserted. A
    // maker who presses `b`, moves a rectangle, opens Info and comes back to a Builder that
    // says `running -- op #1, 37 out` has watched Workshop stay alive while a real child
    // process ran, and has watched the count climb while doing it. A build that had frozen
    // the pump could not have produced either number, because nothing would have been
    // delivered to change them. They stay on the row after it ends, so the evidence does not
    // vanish at the moment it becomes a result.
    const bool named_op = s.op != 0;
    const std::string carried =
        named_op ? " -- op #" + std::to_string(s.op) + ", " + std::to_string(s.chunks) + " out"
                 : std::string();
    const bool unanswered = pane.awaiting && s.outcome != builder::outcome::kRunning;
    row(2,
        unanswered ? panel_field("last", "asked -- waiting for it to start")
                   : panel_field("last", std::string(builder::name_of_outcome(s.outcome)) +
                                             carried),
        unanswered || s.outcome == builder::outcome::kRunning
            ? surface::role::kAccent
            : (s.outcome == builder::outcome::kFailed ||
                       s.outcome == builder::outcome::kNotStarted ||
                       s.outcome == builder::outcome::kUnknownTarget
                   ? surface::role::kAlert
                   : surface::role::kFill));
    // THE EXIT STATUS IS ONLY SHOWN WHEN THERE WAS ONE. A `0` printed after a build that
    // never started reads as success, which is the exact wrong answer at the exact moment a
    // maker most needs the right one.
    //
    // THE TOOL'S OWN COUNTER SHARES THE ROW, and it is on the panel at all because it is the
    // number that proves the tool outlives its presentation: close this panel, reopen it,
    // build again, and it reads 2 -- which a panel that owned the state could not say. It
    // shares rather than taking its own because the rows below are worth more to a maker
    // whose build just failed, and this one has a column to spare.
    row(3,
        panel_field("exit", detail::pad(s.outcome == builder::outcome::kSucceeded ||
                                                s.outcome == builder::outcome::kFailed
                                            ? std::to_string(s.status)
                                            : std::string("--"),
                                        11) +
                                "asks " + std::to_string(s.builds) + " ever"),
        surface::role::kMuted);
    // WHAT WAS ACTUALLY RUN, as the runner reported it. Empty until something has been run,
    // because the tool holds no command and this panel will not invent one to fill a row.
    row(4, panel_field("recipe", s.recipe.empty() ? std::string("(nothing has run yet)")
                                                  : s.recipe),
        surface::role::kMuted);
    // THREE ROWS FOR WHAT THE BUILD SAID, because this is the row budget a maker spends when
    // something has gone wrong, and one row of a compiler's answer is a row of nothing.
    const std::vector<std::string> said =
        panel_block("said", s.detail.empty() ? std::string("--") : s.detail, 3, b.w);
    for (std::size_t i = 0; i < said.size(); ++i) {
        row(5 + static_cast<std::int64_t>(i), said[i], surface::role::kMuted);
    }
    row(8, "[ Build ]  press b", surface::role::kAccent);
}

/// The `+ panel` picker: the catalog, where a maker's cursor is in it, and WHICH KINDS ARE
/// ALREADY OPEN.
///
/// THE LAST OF THOSE IS PNL-0'S, and it is not decoration. Since the picker is now the one
/// owner of panel presence, Return does one of two opposite things depending on a fact that
/// was previously nowhere on this list — so a picker that showed only names would be asking a
/// maker to remember whether the thing they are about to select is currently there. The state
/// gets its own fixed column so the words line up and the list reads down rather than across.
///
/// IT ASKS FOR THE STACK'S FIRST SLOT rather than knowing where that is (PNL-1). The picker
/// is a mode and not a panel -- it has no catalog row to declare a place in — so this is the
/// one caller that names a place itself, and naming one is all it does. Since PNL-2 it names
/// it through `picker_bounds`, because the pointer has to ask the same question: a box a maker
/// can read through is one defect and a box a maker can press through is another, and both are
/// answered by the same rectangle.
// ---- WHAT STATE ONE PANE IS IN -- the recovery invariant, as one word (WIND-2) --------
//
// SEVEN STATES, ONE CLASSIFIER, AND A DELIBERATE PRECEDENCE. Before WIND-2 there were
// three, and that was enough for exactly as long as no two panes could overlap and no
// authored place could leave the screen. Each of the four new ones is a different thing for
// a maker to DO about it, which is the whole reason they are not collapsed into one
// "not showing" bit:
//
//     closed        the active setup does not name it            -- open it
//     unresolved    named, and this build cannot resolve it      -- a typo, or not installed
//     refused       named, resolved, and this medium cannot      -- reset the size, or open
//                   project an authored unit                        the other medium
//     waiting       named, resolved, and the reactive stack      -- make the window taller,
//                   had no tile left                                or place it yourself
//     off-room      named, resolved, projected, and no cell of   -- reset the place
//                   its rectangle is on this canvas
//     covered       named, resolved, projected, on screen, and   -- raise it
//                   every visible cell is behind another pane
//     open          none of the above                            -- nothing
//
// A WANT OF ROOM IS NOT AN UNSUPPORTED UNIT AND NEITHER IS AN UNRESOLVED REFERENCE, and
// `picker_state_word`'s own note already made the neighbouring distinction for the right
// reason: calling a want of room `unresolved` would blame a provider for a screen.

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

/// HOW WIDE THE STATE COLUMN IS.
///
/// ELEVEN, and it moved from eight in WIND-2 because the honest words outgrew it:
/// `unresolved` is ten bytes and `detail::pad` TRUNCATES at its width, so an eight-column
/// field would have presented it as `unresolv` -- a word that is not a state and not a
/// truncation a reader could recognise. Ten plus one, because a column exactly the length
/// of its longest word butts that word against the summary beside it. The price is measured
/// and visible: at the 78x22 minimum a picker row two cells longer than the slot is FITTED
/// and the cut is MARKED, which is `detail::fit` doing exactly its job.
inline constexpr std::size_t kPaneStateCols = 11;

/// HOW WIDE THE NAME COLUMN IS -- and it is a bound a party outside this build can
/// reach, which is what makes it a constant rather than the `10` it used to be.
///
/// A NAME HERE IS NOT ALWAYS THIS TOOL'S. Workshop's own two are `Builder` and `Info`,
/// so for two phases the cut was arithmetic that never fired; an offered pane's name is
/// admitted at up to THIRTY-TWO bytes (`check_pane_text`), so a provider's name reaching
/// this column three times too long is the ordinary case rather than the odd one. INTR-0
/// was the first to do it, on its first live run, and what a maker read was a shorter
/// name that looked finished.
inline constexpr std::size_t kPickerNameCols = 10;

/// IS EVERY VISIBLE CELL OF THIS PANE BEHIND ANOTHER ONE?
///
/// THE UNION, NOT CONTAINMENT BY ONE PANE. Two panes that each cover half of a third leave
/// nothing of it showing, and a maker cannot see it -- so a test that asked "is it inside
/// some single pane" would call that one `open` and leave the maker with a row that says
/// their pane is fine and a screen on which it is not there.
///
/// PARTIAL COVERAGE IS NOT COVERAGE. One visible cell is enough to be `open`: a maker can
/// see the pane, so the word for it is not the word for a pane they cannot.
///
/// IT ASKS ONLY WHAT IS IN FRONT. `presentation_order` is back-to-front, so the panes that
/// can cover this one are exactly the ones after it -- which is the same sentence
/// `occupied_at` spends when it walks that order backward.
inline bool pane_is_covered(const Panels& panels, const Setup& setup, const Screen& sc,
                            std::int64_t kind, const ui::Rect& mine) {
    if (mine.w <= 0 || mine.h <= 0) {
        return false; // nothing visible is OFF-ROOM, which is a different word
    }
    const std::vector<std::int64_t> order = presentation_order(setup, panels);
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
    std::vector<ui::Rect> ahead;
    for (std::size_t i = me + 1; i < order.size(); ++i) {
        const ui::Rect r = bounds_of(panels, setup, order[i], sc).rect;
        if (r.w > 0 && r.h > 0) {
            ahead.push_back(r);
        }
    }
    if (ahead.empty()) {
        return false;
    }
    for (std::int64_t y = mine.y; y < mine.y + mine.h; ++y) {
        for (std::int64_t x = mine.x; x < mine.x + mine.w; ++x) {
            bool hidden = false;
            for (const ui::Rect& r : ahead) {
                if (r.contains(x, y)) {
                    hidden = true;
                    break;
                }
            }
            if (!hidden) {
                return false; // one cell a maker can see is enough
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
    if (row.kind == kNoPaneKind || !resolvable(row.ref, panels.runtime)) {
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
///
/// THE NAME IS FITTED BEFORE IT IS PADDED (INTR-0), and the two are not the same act.
/// `pad` aligns -- it is what keeps the state column under the state column -- and it
/// truncates in SILENCE, which is right for a column whose longest word is a constant
/// somebody checked and wrong the moment the text belongs to a party this build never
/// compiled. `fit` is this file's answer for text whose length somebody else decides:
/// it leaves the mark. So the name passes through `fit` for the truth and `pad` for the
/// alignment, and the state column has not moved by a cell.
///
/// The STATE is padded and not fitted, deliberately: `pane_state_text` returns one of a
/// closed set of words this build writes, and `kPaneStateCols` is chosen to hold the
/// longest of them. Fitting it would be a mark that can never appear, guarding a bound
/// that is checked at the declaration.
inline std::string picker_entry_text(const std::string& name, const char* state,
                                     const std::string& tail) {
    return detail::pad(detail::fit(name, static_cast<std::int64_t>(kPickerNameCols)),
                       kPickerNameCols) +
           detail::pad(state, kPaneStateCols) + tail;
}

inline void paint_picker(surface::SurfaceLayer& layer, const Panels& panels, const Setup& setup,
                         const Screen& sc) {
    const PanelPicker& picker = panels.picker;
    if (!picker.open) {
        return;
    }
    const ui::Rect b = picker_bounds(sc);
    paint_panel_frame(layer, b);
    // THE PICKER IS ONE BOUNDED REGION OF PROSE (TYPE-0), and the budget it spends is the
    // ACTIVE medium's row count rather than the slot's cell count. The two are the same
    // number in a character medium and they are not in one that sets real type -- nine cells
    // of slot is nine rows of a terminal and five rows of an 18-pixel face -- which is the
    // same pair of honest projections the Info panel's body has had since HD-6.
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a slot with no room for a row says nothing rather than lying about the room
    }
    surface::SurfaceTextRegion region = panel_prose_region(b);
    const auto say = [&region, &place](const std::string& text, std::int64_t role) {
        region.rows.push_back(
            surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    say("+ PANEL -- up/down, enter opens or removes", surface::role::kAccent);
    // THE POPULATION IS THE COMBINED CATALOG AND THE BUDGET IS THE SLOT'S (WP-0).
    // Before this the list was `kPanelKinds` long and the picker's height was a
    // constant derived from it, which is a catalog census standing in for a
    // capacity -- it was right for exactly as long as no catalog could outgrow
    // the box, and a runtime offer is precisely a catalog that can. So the rows
    // under the heading are `list_window`'s to spend: the OBJECTS list's own
    // function, its own three rules and its own wording (`omitted_text`), which
    // is the second consumer HD-6 established the rule with and the fourth
    // overall. There is no second scrolling algorithm here and the picker did not
    // get taller.
    //
    // AND SINCE WIND-2 THE POPULATION IS THE SHARED INVENTORY -- the catalog UNION every
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
    // THE REST OF THE SLOT IS THE REGION'S OWN EMPTINESS, and since TYPE-0 nobody writes it.
    // A region owns what is inside its bounds, so its cell projection already pads every row
    // it was not given -- the spaces that erase the panel underneath in a character medium are
    // `project_one_text_region`'s, and the graphical medium clears the same rectangle once
    // rather than a row at a time. What used to be a loop padding out to `b.h` is now the
    // primitive's contract, which is why this painter no longer has one. See kPickerRows for
    // why the whole slot is covered at all.
    layer.texts.push_back(std::move(region));
}

// ---- PANE MANAGEMENT, PRESENTED (WIND-2) ----------------------------------------------
//
// THE PICKER'S OWN SURFACE, WITH A DIFFERENT PURPOSE. It is the same slot, the same frame,
// the same `list_window`, the same omission wording, the same two padded columns and the
// same shared inventory -- what differs is the HEADING, what the third column says, and what
// Return means. There is no warning pane, no notification queue, no second pane list and no
// independent inventory widget, because there is no question here the picker's list could
// not already answer once its population became the union.
//
// WHY IT IS A SEPARATE MODE AND NOT A SECOND KEY ON THE PICKER. Selecting an open row in the
// picker REMOVES it -- PNL-0's resolution of the `x` key, and still the right one for a
// surface whose single ownership is PRESENCE. The gesture a maker reaches for on an open row
// while they are arranging is `raise`, not `remove`, so the two live in modes with different
// purposes and neither gesture changed meaning. Management binds no toggle at all.

/// WHAT A MAKER AUTHORED FOR ONE PANE'S WINDOW, as one line of prose.
///
/// A DEFAULT IS SAID IN CHARACTERS rather than left blank, because a blank third column reads
/// as "this row has nothing to say" and the whole point of the column is that a reactive pane
/// and an arranged one are different things a maker should be able to tell apart at a glance.
/// A pane the setup does not name has no row of authored intent at all, and says so.
inline std::string pane_window_text(const SetupPane* row) {
    if (row == nullptr) {
        return "--";
    }
    const auto axis = [](const PaneSize& s) -> std::string {
        if (s.mode == pane_unit::kCells) {
            return std::to_string(s.amount);
        }
        if (s.mode == pane_unit::kPixels) {
            return std::to_string(s.amount) + "px";
        }
        return std::string("-");
    };
    std::string text;
    if (row->place.mode == pane_unit::kCells) {
        text += "@" + std::to_string(row->place.x) + "," + std::to_string(row->place.y) + " ";
    }
    text += axis(row->width) + "x" + axis(row->height);
    text += " f" + std::to_string(row->front);
    return text;
}

/// The heading, which is where the SUBMODE and the chosen edge are said. One row, because
/// the surface has one -- and the edge is named AND marked, so a medium with no colour
/// carries the whole answer.
inline std::string management_heading(const PaneManagement& manage, const std::string& what) {
    switch (manage.doing) {
    case pane_manage::kMove:
        return "+ WINDOW move " + what + " -- arrows place, esc back";
    case pane_manage::kSize:
        return std::string("+ WINDOW size ") + pane_edge_mark(manage.edge) + " " +
               pane_edge_name(manage.edge) + " -- tab edge, arrows size, esc back";
    case pane_manage::kReset:
        return "+ WINDOW reset " + what + " -- p place, w width, h height, o order, esc back";
    default:
        return "+ WINDOW -- m move, s size, f/b front/back, r/l raise/lower, 0 reset";
    }
}

inline void paint_management(surface::SurfaceLayer& layer, const Panels& panels,
                             const Setup& setup, const PaneManagement& manage,
                             const Screen& sc) {
    if (!manage.open) {
        return;
    }
    const ui::Rect b = picker_bounds(sc);
    paint_panel_frame(layer, b);
    const std::vector<CatalogRow> rows = inventory_rows(setup, panels);
    std::size_t cursor = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (manage.has_selection() && rows[i].ref == manage.selected) {
            cursor = i;
            break;
        }
    }
    const std::string what =
        manage.has_selection() ? quoted_setup_name(ref_text(manage.selected)) : std::string();
    // ONE BOUNDED REGION OF PROSE, exactly as the picker it shares this slot with (TYPE-0).
    const PanelProsePlace place = panel_prose_place(b, sc);
    if (!place.present) {
        return; // a slot with no room for a row says nothing rather than lying about the room
    }
    surface::SurfaceTextRegion region = panel_prose_region(b);
    const auto say = [&](const std::string& text, std::int64_t role) {
        region.rows.push_back(
            surface::SurfaceTextRow{detail::fit(text, place.columns), role});
    };
    say(management_heading(manage, what), surface::role::kAccent);
    const std::size_t budget =
        place.rows > 1 ? static_cast<std::size_t>(place.rows - 1) : 0;
    const ListWindow win = list_window(rows.size(), cursor, budget);
    if (win.before > 0) {
        say("  " + omitted_text(win.before, "earlier"), surface::role::kMuted);
    }
    for (std::size_t i = win.first; i < win.first + win.count; ++i) {
        const bool here = i == cursor && manage.has_selection();
        say(std::string(here ? "> " : "  ") +
                picker_entry_text(rows[i].name,
                                  pane_state_word(pane_state_of(panels, setup, sc, rows[i])),
                                  pane_window_text(pane_of(setup, rows[i].ref))),
            here ? surface::role::kAccent : surface::role::kFill);
    }
    if (win.after > 0) {
        say("  " + omitted_text(win.after, "more"), surface::role::kMuted);
    }
    layer.texts.push_back(std::move(region));
}

// ---- The Info panel's BODY, resolved ONCE (HD-5, widened by HD-6, widened again by HD-7) --
//
// THE GEOMETRY THAT DRAWS A THING AND THE GEOMETRY THAT HITS IT MUST BE THE SAME GEOMETRY.
// That is the one-measurer rule (G-2, HD-1) as HD-3 brought it to interaction. HD-5 obeyed it
// for the one row a maker was editing; HD-6 obeyed it for the whole property body; HD-7 obeys
// it for the OBJECTS list beside it, because both lists are now rows of ONE bounded region.
// So there is no `paint_property_bounds()` beside a `click_property_bounds()` and no
// `paint_object_bounds()` beside either -- there is `info_body_place`, and the painter, the
// caret, the viewport reconcile, both vertical windows and both presses all call it, with the
// same panel bounds `bounds_of` gave the painter and the same `Screen` everything else is
// resolved from.
//
// WHAT HD-6 CHANGED, AND WHY IT HAD TO BE THE BODY RATHER THAN THE ROW. HD-5 made the editing
// row's VALUE a `SurfaceTextRegion` one cell tall and measured the wall that put it against: a
// region one cell tall holds `(12 - 2*inset) / 18` = ZERO rows of this repository's face, so
// `fit_region` answered with the cell projection and the editor got the mark a cell medium
// makes rather than the bar it was built for. The row could not simply be given two cells --
// a two-cell region covers the property beneath it, which is a property of the object the
// maker is editing, hidden at the moment they are working on it.
//
// So the room is taken ONCE, for all the rows together: `fit_region` answers the whole
// question in one equation -- how many rows of the active medium's type fit in that
// rectangle, and how many characters fit across it -- with the inset already inside it.
// Nothing here multiplies a font metric, and there is no Inspector row height to keep in step
// with a renderer's.
//
//     a graphical medium   25 cells tall = 300 px, less 2*2 inset, / 18 px line  -> 16 rows
//     a cell medium        25 cells tall                                         -> 25 rows
//
// Two honest projections of one body. A semantic row is not one cell tall and never was; what
// was one cell tall is the cell medium's PICTURE of it.
//
// WHAT HD-7 CHANGED: THE BODY IS THE WHOLE PANEL, AND IT HOLDS TWO LISTS.
//
// HD-6 left the panel with one bounded section and one unbounded one. `OBJECTS` was five cell
// rows (`kListRows`) whatever the medium reported -- five at 78x25, five at 120x40, five at
// 240x80 -- while the property body beneath it went from five rows to sixty-four over the same
// range. Both facts were on one screen: at 240x80 the body had fifty-six blank rows under
// eight properties while the list three rows above it said `... 16 more`.
//
// ONE REGION RATHER THAN TWO, and the reason is that two regions cannot share a column without
// somebody inverting the metric. Splitting the panel's CELLS between two regions requires
// knowing how many cells a list's rows need, which is `fit_region` read backwards -- a second
// arithmetic beside the one function that turns a metric into a capacity, and the exact thing
// HD-6 refused. One region asks `fit_region` once, gets a budget in PROSE ROWS, and spends it:
//
//     row 0 .. objects_rows-1        the OBJECTS list, its markers included
//     row objects_rows               `PROPERTIES` -- a heading that MOVES with the composition
//     the next properties_rows       the property list, its markers included
//     everything after that          spare, and it is allowed to stay spare
//
// So the two sections cannot overlap: they are not two rectangles that must be kept apart,
// they are disjoint runs of one row budget. The `OBJECTS` heading stays an ordinary label on
// the panel's row 0, because that row is shared with the screen's own `shift+space terminal`
// hint and a region owns its interior.

/// THE CURSOR MARK AND THE LABEL, in columns: `>` (or a space) and the padded property name.
///
/// Constants rather than a `strlen` at each site for `kTerminalPromptCols`' reason: the sum
/// is the offset between "the first column of this row" and "the first byte of the VALUE",
/// which is a fact the painter, the caret and a press each need and none of them owns.
inline constexpr std::int64_t kPropertyMarkCols = 1;
inline constexpr std::int64_t kPropertyLabelCols = 9;

/// THE COLUMN THE INSERTION POINT SITS IN, kept out of the value's own budget.
///
/// `kTerminalCaretCols`' argument, one editor over, and the same one column: a caret is
/// BETWEEN characters, so the one after the last character of a full row needs somewhere to
/// be, and a cell medium has no half-cells to put it in -- `project_text_regions` inserts the
/// mark as a character and then cuts the row at the region's width, so a mark past the last
/// cell is cut off with it. One rule for both media, deliberately, exactly as there.
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

/// HOW MANY PROSE ROWS EACH LIST GETS, and the whole of HD-7's composition policy.
///
/// THE POLICY, IN ONE SENTENCE: **each list is given the rows its own population needs; what
/// neither needs stays spare; and what they cannot both have is shared equally, with any part
/// of a half a list does not need going to the other.**
///
/// It is max-min fair sharing, which is worth naming because it is a rule with a name rather
/// than a ratio somebody picked. Four things follow from it and each is one of the truths the
/// panel has to keep:
///
///   1. A list that FITS gets exactly what it needs and no more. A maker with two objects and
///      a sixty-row panel does not get a fifty-eight-row object list with two names in it and
///      the properties pushed to the floor -- OBJECTS takes two rows and PROPERTIES sits under
///      them, where an eye that just read a name expects to find what it is a name OF.
///   2. Spare room stays SPARE. Nothing is invented to fill it and no list is inflated into
///      it. It is the honest picture of a panel with more room than material.
///   3. Growing the panel never SHRINKS either list. Both shares are non-decreasing in the
///      budget -- pinned as a property over every budget from zero to two hundred, for four
///      demand pairs, rather than spot-checked at the extents this phase happened to run.
///   4. The 50/50 case is a CONSEQUENCE, not a decision. Two lists that both want more than
///      half end up with half each because that is what "share what is contested equally"
///      produces; the moment either wants less, it takes what it wants and the rest goes to
///      the other. There is no fixed split anywhere in this file.
///
/// AND IT IS IN PROSE ROWS, NOT CELLS, which is the reason it can be this small. Splitting the
/// panel's CELLS between two sections needs to know how many cells a run of rows costs, which
/// is `fit_region` read backwards -- a second arithmetic beside the one function that turns a
/// metric into a capacity. One region asks `fit_region` once and this function spends the
/// answer, so no medium metric appears here at all: it is arithmetic over three counts.
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
///
/// Below it the body is not `present` and the panel paints nothing under its heading, which is
/// what this file already did for a body with no rows at all -- a panel with room for a
/// heading and one line cannot show a maker where they are in either list, and half a picture
/// of two lists is worse than the honest absence of both. Unreachable at any screen this
/// composition supports (the minimum panel is seventeen cells and the shortest body this
/// repository's face resolves to is ten rows); here because the metric arrives on the bus.
///
/// SINCE HD-8 THE FOOTER IS BOUGHT ON TOP OF IT: a body must seat this much material AND
/// `kActionRows` of controls, because a panel that shows a maker a list and no way to act on
/// it is the discoverability problem this phase exists to fix, arriving at the one size where
/// it is hardest to argue with. The sum is the floor `info_body_place` actually tests.
inline constexpr std::size_t kInfoBodyMinRows = 3;

// ---- The Info panel's ACTION CONTROLS (HD-8) ----------------------------------------------
//
// WHAT A CONTROL IS HERE: a row of the body that a maker can PRESS, carrying the name of an
// act this application already performs and a visible answer about whether it can be
// performed now. It is not a widget, it has no identity, it holds no callback and it owns no
// state -- `kActionCount` of them exist, they are a TABLE, and everything below is a function
// of an index into that table plus the document and session everything else on this screen is
// derived from. Removing the Info panel removes all of it, because there is nothing to remove.
//
// WHY THEY ARE ROWS OF THE BODY RATHER THAN A THIRD REGION. Exactly HD-7's argument, and it is
// the one that decided one region over two: splitting the panel's CELLS between sections needs
// `fit_region` read backwards. As rows of the one budget the three runs -- objects, properties,
// controls -- are disjoint by construction, so no press can be claimed by two of them and no
// control can be painted over a property. `info_press`, `objects_press` and `actions_press`
// cannot fight over a press for the same reason the lists cannot overlap.
//
// AND THEY ARE NOT A THIRD LIST. They do not go through `list_window`, they have no omission
// marker and they cannot scroll: a list windows a population a document decides the size of,
// and this is a fixed set of controls the APPLICATION declares. A `... 1 more` under a Create
// button would be this file pretending it had been handed material.

/// The two acts a maker can reach without knowing a key. Indices into one table, in the order
/// the footer paints them.
inline constexpr std::size_t kActionCreate = 0;
inline constexpr std::size_t kActionDelete = 1;
inline constexpr std::size_t kActionCount = 2;

/// "This prose row carries no control" — `kNoProperty`'s and `kNoObject`'s twin, one run over.
inline constexpr std::size_t kNoAction = static_cast<std::size_t>(-1);

/// PROSE ROWS THE FOOTER COSTS: one per control, and the count is the table's.
///
/// One row apiece rather than two side by side, and that is the smallest truthful shape rather
/// than a style: putting two controls on one row means a second axis -- a horizontal split
/// with its own paint answer and its own hit answer -- and this file has exactly one
/// segmentation rule today (a row) with exactly one inverse pair per run. A column of controls
/// needs no new arithmetic at all, and a panel twenty-eight cells wide has no room to spare
/// sideways anyway.
inline constexpr std::size_t kActionRows = kActionCount;

/// WHY AN ACTION CANNOT RUN RIGHT NOW, or that it can.
///
/// TWO REASONS, ONE BIT, TWO OWNERS -- and that is HD-8's finding rather than a shape it
/// inherited. The PRESENTATION needs one bit: a control either reads as pressable or reads as
/// present-but-not-pressable, and no third appearance was earned. The ROUTING needs the
/// distinction, because the two reasons are owned by different parts of this tool:
///
///   kDraftLive   the APPLICATION owns it. `create_object`/`delete_object` know nothing about
///                a live property draft and would happily rebuild the rows out from under
///                one, so the press must be held back BEFORE the operation, and the notice is
///                the one HD-7 already wrote for a press on the object list.
///   kNoTarget    the DOCUMENT owns it. `doc::remove` already refuses `no such object` and
///                changes nothing, so holding the press back here would be a second copy of a
///                refusal that exists -- and a second sentence for one state. The press goes
///                through and the document answers in its own words.
///
/// So the rule is one sentence: **a control never invents a reason; it defers to whoever owns
/// the refusal, and holds a press back only when nobody downstream would.** That is why this
/// is not a `disabled` flag. A flag would have collapsed a fact the application must act on
/// and a fact the document must speak for into one word, and the collapse is invisible until
/// a maker presses a control that then quietly does nothing.
///
/// AND IT IS NOT A PREDICTION OF EVERY REFUSAL. An object something else measures against
/// cannot be deleted (`doc::remove`'s dependents policy), and a document can arrive from a
/// file with its mint spent so that `create` refuses too. Neither makes a control unavailable:
/// answering them here would put a copy of the document's policy in the panel's presentation,
/// re-run on every paint, going stale the first time that policy changed. Availability is
/// whether the act has a TARGET and whether the maker is FREE to act -- never whether the
/// document will say yes.
enum class Availability {
    kAvailable, ///< press it and the operation runs
    kNoTarget,  ///< there is no object for this act to be about
    kDraftLive, ///< the maker has unfinished work this act would destroy
};

inline constexpr bool available(Availability a) noexcept {
    return a == Availability::kAvailable;
}

/// IS A PROPERTY DRAFT LIVE? One copy of a loop this file was about to write a third time.
///
/// `objects_press` had it inline and `actions_press` needs the same question about the same
/// rows, which is the duplication test a helper is earned by (HD-7). It is deliberately not a
/// session FIELD: `Row::editing()` is the fact, a bool beside it would be a second opinion
/// about the same state, and `refocus_keeping_draft` would have to remember to carry it.
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
///
/// The target is asked for BY IDENTITY (`doc::find`) rather than by `s.selected != 0`, because
/// a selection can outlive the object it names -- which is exactly the state `delete_selected`
/// refuses in, so a control that read the raw number would offer a press the document has
/// already decided against.
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
///
///     [ Create ]     pressable
///     ( Delete )     present, and not pressable right now
///
/// THE BRACKETS ARE THE STATEMENT AND THE ROLE IS THE SECOND SIGNAL, which is the same rule
/// the object list's `> ` mark has followed since HD-7 and the same reason: a terminal with no
/// ground to tint must be able to tell a maker the two states apart, so the difference cannot
/// live in a Skin's ink. A graphical medium adds the muted role on top and reads at a glance;
/// a character medium reads the brackets and loses nothing.
///
/// `[ ... ]` is also this tool's existing word for "a thing that can be pressed": the Builder
/// panel has painted `[ Build ]` since BLD-0. What HD-8 changes is that one of them finally is.
///
/// FITTED LIKE EVERY OTHER ROW OF THIS BODY, so a panel too narrow for a label cuts it with
/// the `...` the rest of the canvas uses rather than running off the region's edge.
inline std::string action_row_text(std::size_t which, bool pressable, std::int64_t columns) {
    const std::string open = pressable ? "[ " : "( ";
    const std::string close = pressable ? " ]" : " )";
    return detail::fit(open + action_label(which) + close, columns);
}

/// WHERE THE INFO PANEL'S TWO LISTS ARE, HOW MANY ROWS EACH GETS, AND WHICH MEMBERS ARE SHOWN.
///
/// `present` is false when there is nowhere to put a body: the Info panel is not open (its
/// bounds are empty by `bounds_of`'s own rule), it is too narrow for a mark, a label and a
/// value, or it is too short to seat both lists and the heading between them. A caller that
/// forgets to ask gets a region of no width, which draws nothing and contains no press, rather
/// than a rectangle somewhere it is not.
struct InfoBodyPlace {
    bool present = false;
    std::int64_t region_x = 0; ///< the body's own cell origin — a region coordinate
    std::int64_t region_y = 0;
    std::int64_t region_w = 0;
    std::int64_t region_h = 0;
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
    /// PROSE ROWS THE WHOLE BODY HOLDS, both lists, the heading and the spare rows together.
    std::size_t capacity = 0;
    std::size_t objects_rows = 0;    ///< of those, the OBJECTS list's share
    std::size_t properties_rows = 0; ///< of those, the property list's share
    ListWindow objects{};            ///< which objects are shown, and what is left out
    ListWindow properties{};         ///< which properties are shown, and what is left out
    /// THE PROSE ROW CARRYING `PROPERTIES`. It MOVES: it is exactly the number of rows the
    /// object list was given, so a panel that can show more objects pushes it down and a panel
    /// that can show fewer pulls it up. `kRowsY = 8` was this number when it could not move.
    std::int64_t heading_row = kNoProseRow;
    /// THE FIRST OF THE `kActionRows` CONTROL ROWS, and the one number the footer needs (HD-8).
    ///
    /// It is ANCHORED TO THE FOOT of the body -- `capacity - kActionRows` -- and not to the
    /// row the property list happened to stop at. Both are deterministic; only one puts the
    /// controls in the same place from one paint to the next. A footer that floated up and
    /// down as a maker selected objects with different property counts would be a target that
    /// moves under the hand aiming at it, which is a worse fault than an empty strip above it.
    /// So the spare room, when there is any, falls BETWEEN the properties and the controls.
    std::int64_t action_row = kNoProseRow;
};

/// THE PROPERTY ROW THAT MUST STAY ON SCREEN: the one being edited, or the cursor's.
///
/// They are the same row today and the function is written anyway. `begin_edit` opens a draft
/// on `Session::cursor` and `move_cursor` is reachable only from command mode, which is
/// exactly the state in which no row is being edited -- so the two indices cannot drift apart
/// by any gesture this application has. That is a REACHABILITY proof, and HD-5 already wrote
/// down why one of those is not a thing to build a window on: it is one refactor away from
/// being silently wrong, and the symptom here would be a maker's live draft scrolled off the
/// body while the highlight sat somewhere else. A live draft wins, because it is the thing
/// that cannot be reconstructed by looking.
inline std::size_t inspector_focus(const Session& s) {
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (s.rows[i].editing()) {
            return i;
        }
    }
    return s.cursor;
}

/// WHAT A LIST ASKS THE BODY FOR: one row per member, and never zero.
///
/// An empty list still has a sentence to say -- `(none) -- n makes one` for a document a maker
/// has emptied with their own hand, `(nothing selected)` for an inspector with no object under
/// it -- and a list given no rows would say it silently. Both are ordinary reachable states
/// (`delete_object` empties a document; deleting the last one leaves nothing selected), so
/// they are floors here rather than special cases in the painter.
inline constexpr std::size_t list_demand(std::size_t members) noexcept {
    return members == 0 ? 1 : members;
}

/// THE BODY, RESOLVED. `total_objects`/`selected_at` and `total_properties`/`focus` are the two
/// populations and the two members that must stay on screen. They are arguments rather than a
/// document and a session so this is pure over the four numbers the composition depends on --
/// the overload below is the one a painter calls.
inline InfoBodyPlace info_body_place(const ui::Rect& panel, const Screen& sc,
                                     std::size_t total_objects, std::size_t selected_at,
                                     std::size_t total_properties, std::size_t focus) {
    InfoBodyPlace p;
    const std::int64_t used = kPropertyMarkCols + kPropertyLabelCols;
    if (panel.w <= used || panel.h <= kInfoBodyY) {
        return p; // no panel, no room for a value beside a name, or no rows under the heading
    }
    p.region_x = panel.x;
    p.region_y = surface::add_cells(panel.y, kInfoBodyY);
    p.region_w = panel.w;
    p.region_h = panel.h - kInfoBodyY;
    p.fit = surface::fit_region(p.region_x, p.region_y, p.region_w, p.region_h,
                                sc.text_advance_px, sc.text_line_px);
    p.columns = p.fit.columns;
    p.value_columns = p.fit.columns - used - kPropertyCaretCols;
    if (p.value_columns < 0) {
        p.value_columns = 0;
    }
    p.capacity = p.fit.rows > 0 ? static_cast<std::size_t>(p.fit.rows) : 0;
    if (p.capacity < kInfoBodyMinRows + kActionRows) {
        return p; // not enough to seat a row of each list, the heading, and the controls
    }
    // ONE ROW OFF THE TOP FOR THE HEADING AND `kActionRows` OFF THE FOOT FOR THE CONTROLS,
    // before either list is offered anything. Both are chrome and both are bought at the same
    // price as a row of material, which is the same rule `list_window` follows for its own
    // markers: a bound that grows when it is exceeded is not a bound.
    //
    // THE WHOLE COMPOSITION POLICY IS THIS ONE SUBTRACTION AND THE ONE CALL UNDER IT (HD-8).
    // The controls are a FIXED demand and the lists are VARIABLE ones, so they are not three
    // claimants on `share_body_rows`: sharing is what two parties do when they both want more
    // than there is, and a control wants exactly one row at every size this panel has. Giving
    // the footer a share would have made it grow into a tall panel's spare room for no reason
    // anybody could state. So the fixed demand comes off the top of the budget and the
    // variable ones share what is left -- and every property HD-7 pinned survives it, because
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
inline InfoBodyPlace info_body_place(const ui::Rect& panel, const Screen& sc,
                                     const WorkshopDoc& d, const Session& s) {
    return info_body_place(panel, sc, d.elements.size(), position_of(d, s.selected),
                           s.rows.size(), inspector_focus(s));
}

/// WHERE A POINTER FACT LANDED IN THE INFO PANEL'S BODY — the resolve-and-locate answer the
/// three body handlers all begin from (QR-2).
///
/// IT ANSWERS **WHERE**, AND NOTHING ABOUT WHAT THAT MEANS. There is no routing priority in
/// here, no property, action or object semantics, no refusal and no consumption: those are the
/// three handlers' own, and each still asks its own inverse (`property_row_hit`,
/// `action_press_at`, `object_press_at`) of the place this returns. The extraction is the same
/// one `draft_live` earned in HD-8 — duplicate lines removed, ownership unmoved.
struct InfoBodyAt {
    /// THE PANEL IS OPEN, THE BODY RESOLVED, AND THE POSITION UNDERSTOOD — the one bit that
    /// says the two fields below are worth asking anything. It is deliberately the conjunction
    /// of all three: a closed panel, a panel too small to seat a body and a position in a
    /// `space` this application does not recognise are three different facts about the
    /// picture and exactly one fact about this press, which is that it named nothing here.
    bool present = false;
    InfoBodyPlace body{}; ///< the body the painter resolved, not a second reading of it
    ProseAt at{};         ///< and where the fact landed in ITS prose
};

/// The preamble itself: the Info panel's body resolved from the same `bounds_of` the painter
/// used, and this pointer fact located in it by the same `prose_at` every region press goes
/// through. Three copies of these six lines lived in `info_press`, `actions_press` and
/// `objects_press`; a fourth pressable place would have made it four.
///
/// IT IS DELIBERATELY NOT GENERIC OVER REGIONS. The terminal pane and the completion list
/// resolve differently — their own places, their own conditions — and a helper they shared
/// would be a name over three unrelated resolutions rather than one repeated one.
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
    where.present = where.body.present && where.at.understood;
    return where;
}

// ---- One windowed list's rows, mapped both ways ------------------------------------------
//
// TWO LISTS NOW SHARE ONE PROSE LATTICE, so the arithmetic that turns a member's index into a
// row and back is written ONCE and called twice rather than copied. HD-6 wrote it once for the
// property body and predicted the shape of the second copy exactly: "off by one, only once the
// body has scrolled, which is to say only when nobody is looking."
//
// IT IS A HELPER AND IT IS NOT A COMPONENT. It owns no items, no selection, no capacity and no
// keys; it takes a window somebody else resolved and a row somebody else's list begins at, and
// answers a question about rows. See the report-back's List readiness section -- this is
// exactly the kind of extraction that is earned (duplicate arithmetic removed, ownership
// unmoved) and exactly the kind that must not be called `List`.

/// WHICH PROSE ROW SHOWS ITEM `index` OF A LIST THAT BEGINS AT `first_row`, or `kNoProseRow`
/// when the window is not showing it.
///
/// The `... N earlier` marker spends the list's first row when there is one, so this is not
/// `index - first` and the difference is exactly the defect a second copy of it would be.
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

/// WHICH PROSE ROW OF THE BODY CARRIES CONTROL `which`, and which control a prose row carries
/// (HD-8). The footer's inverse pair, beside the two the lists have — and it is a pair rather
/// than one function used two ways for the reason HD-6 wrote down: the painter positions with
/// the first and a press resolves with the second, and a single copy of the arithmetic is what
/// stops a click landing one row off the control it is aimed at.
///
/// There is no window here, so this is `first + index` and nothing more. That is the visible
/// difference between a fixed set of controls and a list: `prose_row_in_window` exists because
/// a list's first row may be spent on `... N earlier`, and a footer has nothing to omit.
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
///
/// `object_press_at`'s rule, one run down, and the same one-measurer claim: the row a press
/// resolves to is the row `prose_row_of_action` painted. THE COLUMN IS DELIBERATELY NOT PART
/// OF THE TEST, exactly as it is not for an object row or a property row — a press anywhere
/// along a control's row is a press on that control, including the room past `]`. Refusing the
/// room beside a six-letter label on a twenty-eight-cell panel would make the target smaller
/// than the row a maker can see, and the row is what they are aiming at.
inline std::size_t action_press_at(const InfoBodyPlace& p, std::int64_t column,
                                   std::int64_t row) {
    if (!p.present || column < 0 || column > p.fit.columns) {
        return kNoAction;
    }
    return action_at_prose_row(p, row);
}

/// ONE SEMANTIC OBJECT ROW AS PROSE — the selection mark, the identity, and as much of the
/// authored name as the body has room for (HD-7).
///
/// THE MARK IS TEXT AND NOT A COLOUR, deliberately and unchanged: `> ` is what a maker reads
/// in a terminal that has no ground to tint, and the accent role a graphical medium adds is
/// the second signal rather than the only one.
///
/// THE IDENTITY COMES BEFORE THE NAME because a name is not an identity in this document --
/// every object `n` makes is called `panel`, so a list of names alone would be a column of the
/// same word. It is the same reason the notice line says `created #4 -- a new identity, not a
/// new name`.
///
/// AND THE WHOLE ROW IS FITTED, not the name alone. There is no `kObjectNameCols` beside the
/// property row's mark and label columns, because an object row has no fixed column after the
/// name to protect: the name is the last field, so cutting the row at the body's width cuts
/// exactly the name and leaves the mark and the identity -- the two things a maker needs to
/// know WHICH row they are looking at -- intact by construction. The cut says so with the
/// `...` this canvas has used since W-6, and the authored name is untouched: `detail::fit`
/// takes a copy, the document is `const` here, and a wider surface shows more of it.
inline std::string object_row_text(const ui::Element& e, bool chosen, std::int64_t columns) {
    return detail::fit(std::string(chosen ? "> " : "  ") + "#" + std::to_string(e.id) + " " +
                           e.label,
                       columns);
}

/// ONE SEMANTIC PROPERTY ROW AS PROSE — the mark, the name, and as much of the value as the
/// body has room for.
///
/// THE TWO HALVES OF THE VALUE ARE DIFFERENT ACTS AND GET DIFFERENT ANSWERS. A RESTING value
/// is fitted (`detail::fit`), so a value longer than the row ends in the `...` this whole
/// canvas already uses to say "there was more" -- HD-5 left ordinary rows alone and HD-5's own
/// live run then showed `the-quick-brown-fo` at the panel edge with nothing to say it had been
/// cut. A LIVE DRAFT is windowed (`TextBox::visible`), because a draft has an insertion point
/// and a maker is moving it: the caret staying put is what tells them the value moved, and a
/// `...` on a row that is being typed into would be a second thing to keep true whose width
/// would come out of this same one capacity (HD-4's rule, unchanged).
inline std::string property_row_text(const Row& row, bool here, std::int64_t value_columns) {
    std::string text = std::string(here ? ">" : " ") +
                       detail::pad(row.label(), static_cast<std::size_t>(kPropertyLabelCols));
    if (row.editing()) {
        return text + row.editor().visible(value_columns);
    }
    return text + detail::fit(row.value(), value_columns);
}

/// THE CARET'S COLUMN IN A BODY ROW: the mark and the name, plus the component's own answer.
///
/// The component reports a column into the WINDOW it is showing (`caret - first_visible`), so
/// this is the one place the row's prose offset is added -- the same shape
/// `terminal_caret_column` has for the pane's `> ` prompt, and for the same reason.
inline std::int64_t property_caret_column(const Row& row) {
    return kPropertyMarkCols + kPropertyLabelCols +
           static_cast<std::int64_t>(row.editor().caret_column());
}

/// IS THIS PROSE POSITION ON THE BODY ROW SHOWING PROPERTY `index` AT ALL?
///
/// `terminal_input_hit`'s rule, one editor over: the ROW is the whole test and the column is
/// deliberately not. A press anywhere along a property's row is a press on that property,
/// including the empty room past the last character -- a maker aiming at the end of a short
/// value clicks after it, and refusing that would refuse the most obvious gesture the row has.
/// A press to the LEFT of the value, on the mark or the name, is on the row too, and the
/// component clamps it to the start of what is shown; the alternative is a strip of a live
/// draft's own row that answers nothing.
inline bool property_row_hit(const InfoBodyPlace& p, std::size_t index, std::int64_t column,
                             std::int64_t row) {
    return p.present && column >= 0 && column <= p.fit.columns &&
           property_at_prose_row(p, row) == index && index != kNoProperty;
}

/// WHICH OBJECT A PRESS INSIDE THE BODY NAMES, or `kNoObject` (HD-7).
///
/// `property_row_hit`'s rule, one list up, and the same one-measurer claim: the row a press
/// resolves to is the row `prose_row_of_object` positioned, so a maker's finger and the mark
/// they are aiming at cannot come from two hands. The COLUMN is deliberately not part of the
/// test -- a press anywhere along a name's row is a press on that object, including the room
/// past a short name -- and a press on a marker row, on the heading, on a property row or on
/// the body's spare rows names no object at all.
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
///
/// NOTHING HERE IS NEW, and that is the claim PNL-0 is making. Every line below was lifted
/// out of `paint` unchanged: the same column, the same rows, the same words, the same roles.
/// What changed is that it is now reached through `panels.open` instead of unconditionally,
/// so a maker can remove it and get it back.
///
/// IT PRESENTS, AND OWNS NOTHING. The objects come from the document; the selection, the
/// inspector's rows and the cursor come from the session; the column's x comes from the
/// screen. There is no `InfoPane` parameter because there is no Info state — removing this
/// panel destroys nothing and reopening it asks nobody, which is exactly how it differs from
/// the Builder beside it and exactly why it is worth having as the second kind.
///
/// IT IS NOT IN THE STACK, and it still does not use the stack's ROWS. `paint_panel_row` is
/// the OVERLAY's shape — rows padded to a panel's full width — and BLD-0's comment on it ("so
/// a second kind writes its content and inherits its shape") is still a prediction rather
/// than a description here: the second kind is a column of bare labels. What PNL-2a changed
/// is the other half of that sentence. This panel takes a BACKDROP across the whole of its
/// bounds, from the same `paint_panel_frame` the Builder and the picker take theirs from,
/// because a backdrop is not chrome — it is what a place LOOKS like when something is in it,
/// and PNL-2 had already given this rectangle the maker's hand. A region a press cannot
/// reach through and an eye can is one rectangle telling a maker two different things.
///
/// IT IS THE SAME RECTANGLE, NOT A SECOND ONE. The backdrop is painted at `b` — the bounds
/// this painter was handed, which `paint_panels` got from `bounds_of` and which
/// `occupied_at` asks the same function for. There is no geometry here to drift from the
/// occupancy answer, which is why widening this panel moves what it covers and what it
/// refuses in one edit.
///
/// WHAT THE BACKDROP DOES NOT DO IS PAD THE ROWS, and that is deliberate. Padding each label
/// to `b.w` — the shape a stacked panel has — would also erase the `shift+space terminal`
/// hint, which `paint` writes on this region's top row beside OBJECTS and which belongs to
/// the SCREEN rather than to this panel. A rect is drawn under every label by construction,
/// so the hint survives a backdrop; it would not have survived a padded row.
///
/// AND THE INK IS THE ROLE'S, NOT THIS PANEL'S. `paint_panel_frame` says `kMuted` — quiet
/// ground — which is the same role the workspace's own backdrop carries, so in both shipped
/// media this region is currently the same ink as the workspace beside it: `.` in a terminal,
/// the same grey in a window. That is the role vocabulary working as designed rather than an
/// oversight: a publisher ships intent and a Skin owns appearance, so "quiet ground behind
/// furniture" and "quiet ground a maker authors on" are told apart by a MEDIUM that wants to,
/// or by a new role once something has measured that they must be. Neither is this phase's to
/// decide, and what it is worth is the observation that a filled region is no longer a hole.
///
/// EVERY COORDINATE BELOW IS RELATIVE TO `b`. `kInfoBodyY` is a row within this panel; `b.x`
/// is its column. There is no `Screen` here any more, which is the measurable half of PNL-1
/// for this kind: what used to be "the painter reads `sc.panel_x`, the same number `screen_of`
/// gives the workspace to measure against" is now "the painter is told where it is".
inline void paint_info(surface::SurfaceLayer& layer, const WorkshopDoc& d, const Session& s,
                       const ui::Rect& b, const Screen& sc) {
    // THE BACKDROP FIRST, so everything below is written over it and nothing authored
    // survives underneath it. One rect, the whole of `b`, and the same call the other two
    // presentations make.
    paint_panel_frame(layer, b);

    // `OBJECTS` STAYS CHROME ON THE PANEL'S OWN ROW 0, and it is the one thing in this panel
    // that is not a row of the body (HD-7). That row is SHARED: `paint` writes the screen's
    // `shift+space terminal` hint eight cells to the right of it, and a region owns its
    // interior -- a body starting here would blank a sentence that is not this panel's.
    layer.labels.push_back(surface::SurfaceLabel{b.x, b.y + kInfoBodyY - 1, "OBJECTS",
                                             surface::role::kAccent});

    // THE BODY IS ONE BOUNDED REGION AND IT HOLDS BOTH LISTS (HD-7, widening HD-6).
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
    // AND NO ROW IS PAINTED THAT THE BODY CANNOT HOLD. Before HD-6 the property loop ran over
    // every property and wrote a label per row, so a population taller than the panel ran off
    // its bottom edge; before HD-7 the object loop was bounded, but by a CONSTANT rather than
    // by the room. Both bounds are windows now, both omissions are counted on the side they
    // happened, and both come from the OBJECTS list's own two functions.
    const InfoBodyPlace body = info_body_place(b, sc, d, s);
    if (!body.present) {
        return; // a panel with no room under its heading says nothing rather than lying
    }
    surface::SurfaceTextRegion region;
    region.x = body.region_x;
    region.y = body.region_y;
    region.w = body.region_w;
    region.h = body.region_h;
    // A ROW MAY BE SET ON A GROUND, AND ALMOST NONE OF THEM IS (HD-9). The ground is
    // defaulted rather than spelled at every call because `role::kNone` is not a value a row
    // could be wrong about -- it is the absence of one, and the picture it draws is the
    // picture every row of this body drew before HD-9. That is the opposite of HD-4's
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
    // ---- THE FOOTER, WRITTEN ONCE AND EMITTED ON EVERY PATH OUT OF THIS PAINTER (HD-8) ----
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
        while (region.rows.size() < static_cast<std::size_t>(body.action_row)) {
            say_row(std::string(), surface::role::kFill);
        }
        for (std::size_t which = 0; which < kActionCount; ++which) {
            const bool pressable = available(action_availability(which, d, s));
            // THE ROLE IS THE SECOND SIGNAL AND NEVER THE ONLY ONE. `kMuted` is this panel's
            // existing word for "furniture, not the maker's material", which is what a control
            // a maker cannot use currently is; the brackets in the text carry the same fact to
            // a medium with no ink to spend. No role was added and none was widened.
            //
            // AND A CONTROL A MAKER CAN USE SITS ON SOMETHING (HD-9), which is the THIRD
            // signal and still not the only one: `[ ... ]` is what a medium with no ground at
            // all reads, and it is unchanged. The ground is `kMuted` -- the same value the
            // Terminal's completion list has spent on its selected row since HD-2 -- and the
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
            const ui::Element& e = d.elements[body.objects.first + n];
            const bool chosen = e.id == s.selected;
            say_row(object_row_text(e, chosen, body.columns),
                    chosen ? surface::role::kAccent : surface::role::kFill);
        }
        say_omission(body.objects.after, "more");
    }
    // The object list's share is spent whether or not it had that much to say, because the
    // heading below it is at a row the composition chose and not at the row this loop happened
    // to reach. A list that says less than its share leaves blank rows under itself.
    while (region.rows.size() < body.objects_rows) {
        say_row(std::string(), surface::role::kFill);
    }

    // ---- `PROPERTIES`, a row of the body, at the row the composition put it ----
    //
    // AND IT IS SET ON A GROUND (HD-9), because accent ink alone was not enough to say
    // "a section begins here". HD-7 predicted this and a live run confirmed it: the row
    // immediately above `PROPERTIES` is the SELECTED object, which is accent ink too, so the
    // heading and the thing it is not were the same colour on adjacent rows. A ground is the
    // one signal in this vocabulary that says "this row, all of it" -- which is what a
    // boundary is -- and it takes no room, so HD-7's spent separator row stays spent.
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
        say_row(property_row_text(row, here, body.value_columns), role);
        if (row.editing()) {
            // ONE MEASURER, TWICE OVER. The prose ROW is `prose_row_of_property` -- the same
            // function `property_at_prose_row` inverts for a press -- and the COLUMN is
            // `property_caret_column`, the same offset the row's own text was built with. So
            // a caret cannot land where the text is not, and a click cannot land where the
            // caret would not, on either axis.
            region.caret_row = prose_row_of_property(body, i);
            region.caret_col = property_caret_column(row);
        }
    }
    say_omission(body.properties.after, "more");
    say_footer();
}

/// Every open panel, then the picker over them. The one call `paint` makes.
///
/// THE LOOP NO LONGER KNOWS ANY GEOMETRY (PNL-1). It asks the placement path where each open
/// panel is and hands the answer to that kind's painter; the branch that remains chooses a
/// PAINTER, which is the one thing about a panel kind that genuinely cannot be shared -- the
/// Builder draws a tool's status and Info draws a document. Before this, the same branch also
/// carried the placement (one kind got a slot in a stack, the other got a column) and a
/// counter that named a kind to decide which panels earned a slot.
///
/// A THIRD KIND IS A CATALOG ROW AND A LINE HERE, and neither of them is geometry. That is
/// the whole of what PNL-1 set out to be worth: the placement question is answered where a
/// kind is DECLARED rather than where it is drawn, so the third one does not arrive with a
/// third set of constants. What it still cannot do is ask for somewhere neither place is --
/// that is the layout phase, and it now has two named places to argue from.
// ---- AN EXTERNAL PANE'S BODY: one header row of Workshop's, and a region (WP-0) --------
//
// WORKSHOP OWNS EVERY RECTANGLE AND THE PROVIDER OWNS EVERY SENTENCE. That split is the
// whole of the phase's presentation claim, and the shape below is what makes it structural
// rather than promised: the provider is handed two integers and hands back rows, so there is
// no coordinate, no cell, no pixel and no metric anywhere in the conversation for it to
// disagree with this application about.
//
// THE ROOM IS `fit_region`'S ANSWER AND NOBODY MULTIPLIES A METRIC. Same function, same one
// call, same discipline as the Info body (HD-6) and the terminal pane (HD-1): 8 cells of body
// is 5 rows of an 18-pixel face and 8 rows of a cell medium, and the two are honest
// projections of one body rather than two arithmetics that happen to agree today.

/// One header row, Workshop's own, so the provenance of what follows is legible.
///
/// IT IS A PROSE ROW OF THE PANEL'S REGION SINCE TYPE-0, AND IT USED TO BE A CELL ROW ABOVE
/// IT. The difference is what the header is SET IN: a cell row is one label, one cell per
/// byte in every medium, so Workshop's own heading was drawn in the bitmap letterform
/// directly above a provider's rows drawn in a real face -- the tool contradicting itself
/// about its own typography, on the one panel whose whole purpose is to present somebody
/// else's words honestly. One region for the whole panel makes the two the same kind of
/// thing, which is what a header claiming provenance has to be.
///
/// WHAT IT COSTS IS ONE PROSE ROW OF THE PROVIDER'S BUDGET, ON A GRAPHICAL MEDIUM ONLY, AND
/// THE NUMBER IS MEASURED. A nine-cell slot holds nine rows of a character medium and five
/// of an 18-pixel face. Before: the header took a CELL, and `fit_region` over the remaining
/// eight cells answered 5 rows -- so the provider got 5 there and 8 in a terminal. After:
/// `fit_region` over all nine answers 5, less this one, so the provider gets 4 there and 8
/// in a terminal, which is unchanged. The graphical row is the honest price of the header
/// being the same text as the body, and the pane's own omission markers spend it truthfully
/// (INTR-0): a budget that shrank is a budget, and a windowed list says what it left out.
inline constexpr std::int64_t kExternalHeaderRows = 1;

/// WHAT A PANE SAYS BEFORE ITS PROVIDER HAS SAID ANYTHING. The Builder pane's
/// distinction, one provider further out: this is a fact about THIS PANEL -- a
/// room has been granted and nothing valid has answered it -- and it is not a
/// fact about the provider. It is never `unavailable`, because Loom gives
/// Workshop no participant-visible unload notification and a sender's silence
/// does not prove a delivery's fate.
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
    std::int64_t region_x = 0;
    std::int64_t region_y = 0;
    std::int64_t region_w = 0;
    std::int64_t region_h = 0;
    surface::RegionFit fit{};
    std::int64_t rows = 0;    ///< prose rows -- the `PaneRoom` budget's first half
    std::int64_t columns = 0; ///< ...and its second
};

/// The body under an external panel's header row: the panel's whole bounds, less that row's
/// share of the PROSE the active medium fits in them (TYPE-0; it used to be less a CELL).
///
/// TOTAL over the rectangle it is handed, because a closed panel answers with an empty one
/// (`bounds_of`) and a screen may be small enough that a header leaves nothing beneath it.
/// `present` is false in both cases and no room is ever granted from a body that is not
/// there -- which is what keeps `PaneRoom` from carrying a zero somebody downstream would
/// subtract from.
///
/// THE REGION IS THE WHOLE PANEL AND THE HEADER IS ITS FIRST ROW, so `region_y` is the
/// panel's own `y` and the subtraction happens in `rows`. That ordering is the point: the
/// header is reserved from the budget BEFORE the provider is told what it has (HD-8's
/// reservation argument, INTR-0's second occurrence), so a provider is never granted a row
/// Workshop is about to write over.
inline ExternalBodyPlace external_body_place(const ui::Rect& panel, const Screen& sc) {
    ExternalBodyPlace p;
    if (panel.w <= 0 || panel.h <= 0) {
        return p;
    }
    p.region_x = panel.x;
    p.region_y = panel.y;
    p.region_w = panel.w;
    p.region_h = panel.h;
    p.fit = surface::fit_region(p.region_x, p.region_y, p.region_w, p.region_h,
                                sc.text_advance_px, sc.text_line_px);
    p.rows = p.fit.rows > kExternalHeaderRows ? p.fit.rows - kExternalHeaderRows : 0;
    p.columns = p.fit.columns;
    p.present = p.rows > 0 && p.columns > 0;
    return p;
}

/// THE HEADER: what this pane is, and WHOSE it is.
///
/// The Builder panel's rule (`BUILDER @zengine.builder`), which the terminal pane follows
/// too: a presentation showing somebody else's facts without saying whose is a presentation
/// that will eventually be read as its own. Here it is load-bearing rather than good manners
/// -- the rows underneath were written by a party this build has never met, and the office
/// that authored them is the only thing about that party Workshop actually knows.
///
/// BOTH HALVES WERE VALIDATED AT ADMISSION and neither is echoed raw: the name passed
/// `check_pane_text` and the office passed `check_pane_key`, so no control byte and no
/// row-breaking sequence can reach this line. `paint_panel_row` fits it to the panel's width,
/// which marks its own cut.
inline std::string external_header(const RuntimePane& row) {
    return row.name + " @" + row.provider;
}

/// ONE EXTERNAL PANEL: Workshop's backdrop, Workshop's header, and ONE region carrying
/// whatever that office last validly said inside the room it was granted.
///
/// THE CACHED ROWS ARE PAINTED WITHOUT BEING RE-JUDGED, and that is sound rather than lax:
/// nothing enters `ExternalPane::shown` without having passed the CURRENT room's row count,
/// column width and plain-ASCII contract, and every new grant clears the cache before the new
/// room is sent. So the invariant this painter rests on is maintained at the retention
/// boundary where the bytes arrive, not re-derived on the paint path every frame.
///
/// NO CARET. A caret is an insertion point and nothing here is editable; there is no input
/// path to an external pane at all (weave.hpp's occupancy wall consumes every press that
/// lands on one).
inline void paint_external(surface::SurfaceLayer& layer, const Panels& panels, std::int64_t kind,
                           const ui::Rect& b, const Screen& sc) {
    paint_panel_frame(layer, b);
    const RuntimePane* row = panels.runtime.of_kind(kind);
    if (row == nullptr) {
        return; // an open kind with no catalog row cannot happen; drawing a lie could
    }
    const ExternalBodyPlace body = external_body_place(b, sc);
    if (body.fit.rows <= 0 || body.fit.columns <= 0) {
        return; // no room for one row of this medium's type: say nothing at all
    }
    surface::SurfaceTextRegion region;
    region.x = body.region_x;
    region.y = body.region_y;
    region.w = body.region_w;
    region.h = body.region_h;
    // WORKSHOP'S HEADER IS THE REGION'S FIRST ROW (TYPE-0), so the provenance line and the
    // provider's sentences are the same kind of text in whatever face this medium owns. It is
    // fitted to the region's own columns, which is what marks its cut.
    region.rows.push_back(
        surface::SurfaceTextRow{detail::fit(external_header(*row), body.columns),
                                surface::role::kAccent});
    // A PANE WITH ROOM FOR THE HEADER AND NOTHING ELSE STILL SAYS WHOSE IT IS. `present` is
    // the question "was a body granted", and it is asked AFTER the header is written rather
    // than before it -- a rectangle showing a maker nothing at all is the worse of the two
    // answers, and it is what this painter gave for one frame when the header stopped being
    // a cell row of its own.
    if (!body.present) {
        layer.texts.push_back(std::move(region));
        return; // no room under the heading: the heading, and no invented room
    }
    const ExternalPane* pane = panels.external_pane(kind);
    if (pane == nullptr) {
        layer.texts.push_back(std::move(region));
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

/// THE EIGHT AFFORDANCES OF THE SELECTED PANE, drawn only while a maker is arranging.
///
/// GLYPHS OVER THE PANE, not rectangles, and for `size_handle`'s reason exactly: the pane's
/// own backdrop already fills these cells, so a rect here would be invisible, and an
/// affordance a maker cannot tell from the furniture is not an affordance. The chosen one is
/// in the accent role and the other seven are muted -- the SECOND signal, after the marks
/// themselves, never the only one.
///
/// NOTHING IS DRAWN FOR A PANE WITH NO PRESENTATION. A pane that is off-room, unprojectable,
/// waiting or unresolved has no rectangle to ring, which is exactly why the management LIST
/// is the recovery surface and the ring is not: a maker reaches an invisible pane by its row,
/// and the row is always there.
inline void paint_pane_affordances(surface::SurfaceLayer& layer, const Session& s,
                                   const Screen& sc) {
    if (!s.manage.open || !s.manage.has_selection()) {
        return;
    }
    const std::optional<std::int64_t> kind =
        resolve_pane(s.manage.selected, s.panels.runtime);
    if (!kind.has_value()) {
        return;
    }
    const PanelBounds where = bounds_of(s.panels, s.setup.active, *kind, sc);
    if (!where.open || where.rect.w <= 0 || where.rect.h <= 0) {
        return;
    }
    for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
        const ui::Rect at = pane_edge_cell(where.rect, edge);
        const bool chosen = s.manage.doing == pane_manage::kSize && s.manage.edge == edge;
        layer.labels.push_back(surface::SurfaceLabel{
            at.x, at.y, std::string(pane_edge_glyph(edge)),
            chosen ? surface::role::kAccent : surface::role::kMuted});
    }
}

namespace detail {

/// ONE PLANE FOR ONE PRESENTATION — offered unconditionally, and taken back if that
/// presentation turns out to draw nothing (WIND-2a).
///
/// It exists so a presentation's own guard stays inside the presentation. `paint_picker`
/// already knows whether the picker is open, and `paint_pane_affordances` already knows
/// whether the selected pane resolved to a rectangle worth ringing; testing either of
/// those a second time out here, merely to decide whether to allocate a plane, would be a
/// second copy of a condition — which is how a copy and its original come to disagree.
/// So the caller offers a plane to everything and an unused one is dropped.
///
/// A CANVAS THEREFORE CARRIES A LAYER FOR EACH THING ACTUALLY ON IT, which is what makes
/// "one complete layer per presented pane" a readable property of a published canvas
/// rather than a claim about which painters happened to run.
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

/// EVERY PRESENTED PANE, BACK TO FRONT — ONE COMPLETE LAYER EACH.
///
/// IT WALKS THE PRESENTATION ORDER (WIND-2), ASCENDING, so a later-ranked pane is drawn
/// OVER an earlier one -- the exact reverse of the order `occupied_at` walks, from the one
/// helper, so what a maker sees on top is what their hand meets.
///
/// AND SINCE WIND-2a THAT SENTENCE IS TRUE OF THE PICTURE AND NOT ONLY OF THIS LOOP. The
/// canvas used to hold three root lists, so appending a pane's rects, labels and regions
/// here put each KIND into a global band: every rect, then every label, then every region.
/// A pane the maker had sent to the BACK still covered a pane in FRONT of it whenever the
/// two drew different kinds -- measured, with the Builder placed over the Info column: the
/// pointer answered `Builder` and the terminal drew Info's prose in the same cell. A layer
/// per pane is the whole repair; no primitive gained a field and this loop still reads the
/// same order it always did.
///
/// A PANE WITH NOTHING ON SCREEN PAINTS NOTHING AT ALL, and the guard is one comparison
/// rather than three: `bounds_of` answers with an empty rectangle for a pane that is
/// off-room and for one whose authored unit this medium cannot project, so a pane with no
/// cells here never reaches a painter that would push a degenerate `SurfaceRect`.
///
/// THE THREE PRESENTATIONS AFTER THE PANES ARE LATER LAYERS, in the order a maker's
/// attention is in: the selected pane's affordances go over its own content so that every
/// handle is visible, and the picker or the management surface goes over the pane it is
/// covering -- which is what makes the recovery surface readable above an external pane's
/// text rather than underneath it. The Terminal is later still and is `paint`'s to add,
/// because it is a MODE that also decides what the rest of the screen does.
inline void paint_panels(surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                         const Screen& sc) {
    const Panels& panels = s.panels;
    for (const std::int64_t kind : presentation_order(s.setup.active, panels)) {
        const Panel p{kind};
        const ui::Rect b = bounds_of(panels, s.setup.active, p.kind, sc).rect;
        if (b.w <= 0 || b.h <= 0) {
            continue;
        }
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            if (p.kind == panel::kBuilder) {
                paint_builder(layer, panels.builder, b);
            } else if (p.kind == panel::kInfo) {
                paint_info(layer, d, s, b, sc);
            } else if (is_runtime_kind(p.kind)) {
                // ONE GENERIC ARM FOR EVERY EXTERNAL PANE, and there is no second one to
                // add. The branch above chooses a PAINTER, which PNL-1 named as the one
                // thing about a panel kind that genuinely cannot be shared -- and this arm
                // is the case where it can be, because every external pane is presented
                // identically: a header Workshop writes and a region the provider fills. A
                // second provider costs this function nothing at all.
                paint_external(layer, panels, p.kind, b, sc);
            }
        });
    }
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_pane_affordances(layer, s, sc);
    });
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_picker(layer, panels, s.setup.active, sc);
    });
    detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
        paint_management(layer, panels, s.setup.active, s.manage, sc);
    });
}

// ---- THE SETUP LINE: which arrangement this is, and whether it is written down (WS-0) ----
//
// ONE ROW OF THE BOTTOM BAND, the one directly under the workspace, which was blank. The
// band has always been five rows -- a spare, the notice, a spare, and two help lines -- and
// this spends the first spare on the one fact WS-0 adds that a maker needs CONTINUOUSLY
// rather than at the moment they act: which setup they are in, whether it matches its file,
// and whether any of its panes could not be presented.
//
// WHY NOT THE STATUS SLOT. That line is the DOCUMENT's -- object count, selection, document
// path, document saved marker -- and it is already 56 cells with the default path and past
// 78 with any real one, so a setup half appended to it would be clipped by the terminal with
// no mark at all. `SurfaceText` carries no width and the medium clips in silence; a canvas
// label goes through `detail::fit`, which SAYS it cut something. The choice is therefore
// between two silences and one honest elision, and this is the honest one.
//
// WHY NOT A BOUNDED REGION WITH A REAL CARET (HD-3). Because the row is ONE CELL TALL, and
// HD-6 measured what that means: `fit_region` over a one-cell-high rectangle answers ZERO
// rows of a real face, so a graphical medium would draw nothing at all there. A caret said
// in prose needs a region with room for prose. So the editor below writes its caret INTO the
// text as `surface::kCaretGlyph` -- which is byte-for-byte what the cell projection does with
// a region's caret anyway -- and both media show the same row.
//
// IT OCCUPIES NO POINTER SPACE. There is no hit test here, no press, no pointer editing:
// `occupied_at` answers about panels and the picker, and this line is furniture beside the
// notice, exactly as the help lines are.

/// What the one-line name editor puts before and after the name a maker is typing.
inline constexpr const char* kSetupNamePrompt = "setup name> ";
inline constexpr const char* kSetupNameHint = "  enter saves  esc cancels";

/// The two gestures the setup line advertises, on the line the thing they act on is on --
/// the `[+ panel]  p` precedent, and for the same measured reason: the two help lines at the
/// bottom are 68 and 73 cells of a 78-cell minimum screen, so neither has room for a gesture
/// pair, and abbreviating one a maker uses constantly to advertise one they may not would be
/// paying for the new thing with the old.
inline constexpr const char* kSetupHints = "s name/save  r restore";

/// What the line says where a path would be when the host chose none.
inline constexpr const char* kNoSetupFileShown = "no setup file";

/// The fewest columns the name editor will claim for the name itself, so that a surface
/// narrow enough for the chrome to exceed it still shows some of what is being typed.
inline constexpr std::int64_t kSetupNameMinCols = 8;

/// HOW MUCH OF THE NAME THE ONE-LINE EDITOR CAN SHOW at this extent -- the one measurer, so
/// the window the `component::TextBox` is kept against and the slice the painter cuts are the
/// same number. A second copy of this arithmetic is how a caret comes to sit off the end of
/// the row it is drawn on (HD-4).
inline std::int64_t setup_name_columns(const Screen& sc) noexcept {
    const std::int64_t chrome =
        static_cast<std::int64_t>(std::char_traits<char>::length(kSetupNamePrompt)) +
        static_cast<std::int64_t>(std::char_traits<char>::length(kSetupNameHint)) + 1;
    const std::int64_t room = sc.w - chrome;
    return room > kSetupNameMinCols ? room : kSetupNameMinCols;
}

/// The name being typed, windowed, with the caret in it.
inline std::string setup_naming_text(const SetupNaming& naming, const Screen& sc) {
    std::string shown = naming.line.visible(setup_name_columns(sc));
    const std::size_t at = naming.line.caret_column();
    shown.insert(at < shown.size() ? at : shown.size(), 1, surface::kCaretGlyph);
    return std::string(kSetupNamePrompt) + shown + kSetupNameHint;
}

/// WHICH SETUP THIS IS, IN THE ORDER A MAKER NEEDS IT.
///
/// The name first, because it is the identity and must never be the thing that elides; then
/// the saved marker, computed by comparison and never flagged; then the unresolved count,
/// which is the only dynamic truth on the line; then the file; then the two gestures.
///
/// MEASURED AT THE MINIMUM COMPOSITION: with the default name and the default file name and
/// nothing unresolved the whole line is 70 of 78 cells. A setup with an unresolved reference
/// runs 15 cells longer and `detail::fit` marks the cut, which falls on the tail of the
/// static hint rather than on any of the truths above it -- which is why they are in this
/// order rather than in the order they were designed in.
inline std::string setup_status_text(const SetupState& setup, const std::string& path,
                                     const RuntimeCatalog& runtime) {
    std::string line = "setup " + quoted_setup_name(setup.active.name) + " " +
                       (setup.saved() ? "saved" : "UNSAVED");
    // THE RUNTIME CATALOG IS ASKED, AND THIS IS THE LINE THAT MADE IT A REQUIRED ARGUMENT
    // (WP-0). A pane a maker can SEE must not be counted as unresolved on the row directly
    // beneath it, and the built-in-only resolver would have said exactly that about every
    // admitted external offer -- silently, and only in the configuration where somebody had
    // actually loaded a provider.
    const std::vector<PaneRef> waiting = unresolved_panes(setup.active, runtime);
    if (!waiting.empty()) {
        // UNRESOLVED, NEVER UNAVAILABLE. Workshop knows that it cannot present these
        // references; it knows nothing whatever about whoever could, and a word implying
        // otherwise would be a claim made out of silence.
        line += " | " + std::to_string(waiting.size()) + " unresolved";
    }
    line += " | ";
    line += path.empty() ? std::string(kNoSetupFileShown) : path;
    line += " | ";
    line += kSetupHints;
    return line;
}

/// The setup line as it is painted: the editor while a maker is naming, and the identity
/// otherwise. Fitted here, at the presentation boundary and nowhere upstream.
inline std::string setup_line(const Session& s, const std::string& path, const Screen& sc) {
    return detail::fit(s.setup.naming.open ? setup_naming_text(s.setup.naming, sc)
                                           : setup_status_text(s.setup, path,
                                                               s.panels.runtime),
                       sc.w);
}

/// The whole screen as one published canvas — an ORDERED LIST OF PLANES since WIND-2a.
///
///     the workspace       its backdrop, then each authored element as the scene placed it
///                         (with the selected one's ring UNDER it, so the ring reads as a
///                         ring rather than a border the object grew), and the size handle
///     one plane per pane  `presentation_order`, ascending by canonical `front`
///     the affordances     over the selected pane's own content, so no handle is hidden
///     picker / management over the panes they are covering
///     the screen's chrome the shared top row and the bottom band the tool speaks in
///     the Terminal        the final modal plane, when it is open
///
/// Inside any one plane painter's order is list order across its three primitive kinds, as
/// it always was. BETWEEN planes the later one wins WHOLE, which is the fact WIND-2 could
/// not express: paint and `occupied_at` walk one `presentation_order` in opposite
/// directions, and now the medium executes it too.
///
/// The picture is derived from `workspace_scene()` and from nothing else, which
/// is what makes "what you see is what the hit test answers about" structural
/// rather than a claim: the two read one value.
///
/// THE SETUP PATH IS AN ARGUMENT (WS-0), and it is the only thing this function has ever
/// needed that is neither authored content nor session. It is the HOST's choice, exactly as
/// `document_path` is, and the document's path avoids this by only ever being said in the
/// status slot, which the weave composes. The setup's identity is painted ON the canvas, so
/// the canvas has to be told. It is defaulted so that a caller with no host -- every screen
/// case in the suite -- paints a truthful line saying no setup file was chosen.
inline surface::SurfaceCanvas paint(const WorkshopDoc& d, const Session& s,
                                    const std::string& setup_path = std::string()) {
    const Screen sc = screen_of(s);
    surface::SurfaceCanvas c;
    c.width = sc.w;
    c.height = sc.h;

    // THE WORKSPACE PLANE: what a maker authored, as this workspace places it (WIND-2a).
    // It is written whole before any pane is, because a pane is a presentation IN FRONT of
    // the document -- which is what `occupied_at` has answered since PNL-2, and what the
    // picture now agrees with instead of merely being told.
    //
    // THE SCREEN'S OWN CHROME IS NOT HERE. It is a plane of its own, added after the panes,
    // for a reason worth stating where both are decided: the top row is SHARED with the side
    // region by design (HD-7 -- `OBJECTS` names the panel's column and the terminal hint
    // names a mode, on one row that neither owns outright), and the bottom band is where the
    // tool SPEAKS. Painting a panel's backdrop over either would take the notice that just
    // told a maker what happened, and the two gestures that open a panel and a window, and
    // erase them under the furniture they describe.
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
        // AND IT IS SEMANTIC TYPE ON MATERIAL SOMEBODY ELSE OWNS (TYPE-1), which is the
        // one place in this tool where that sentence has to be argued rather than assumed.
        // The name is semantic -- it is the maker's word for this object and its exact cell
        // occupancy is no part of what they authored -- so it belongs in a bounded region.
        // Its rectangle, though, is already full: the object's body is authored MATERIAL,
        // drawn one line up as a `SurfaceRect`. An ordinary region over it erases that
        // material in both media, and rows carrying the object's role as a GROUND leave a
        // `12h - 4 - 18*rows` pixel band the strips cannot reach (10 px across the foot of a
        // default 12x4 object; `12h - 4 == 18k` has no integer solutions, so SOME remainder
        // exists at every height). Both were built and run live, twice -- once by TYPE-0 and
        // once again by TYPE-1 to re-measure them. `surface::kGroundBeneath` is the third
        // answer: the region keeps its bounds, so the name is fitted and cut against them,
        // and gives up the ground, so nothing under it is painted over.
        //
        // THE BOUND IS THE WORKSPACE'S RIGHT EDGE AND NOT THE OBJECT'S, and it always has
        // been: a name longer than the object it names runs out of it and across the
        // workspace rather than being cut at a width the maker chose for the BODY. TYPE-1
        // preserved that deliberately -- the region is `room` cells wide, which is exactly
        // the number `detail::fit` was given before. What a medium now gets to say is how
        // many CHARACTERS those cells hold: `fit_region` answers 47 cells in cells and 70
        // columns of a 13pt face, so a name is marked when it genuinely did not fit rather
        // than when it would not have fitted as bitmap cells.
        //
        // AND ITS HEIGHT IS THE OBJECT'S, which is what makes a one-cell object honest for
        // free. `fit_region` sends a region with no room for a row of the medium's face back
        // to the cell projection (HD-5), so an object a maker sized to one cell shows its
        // name in cells -- the same picture a terminal shows -- rather than 18 pixels of type
        // hanging out of a 12-pixel object. No `if (h < N)` was written here; the rule is the
        // one both media already resolve with.
        //
        // ...OR ONE ROW, WHICHEVER IS MORE, and that floor is not a fudge: a name is written
        // ON a row, so the room it needs is a row, and an object whose resolved height is
        // zero still has the row its origin is on. `check_extent` refuses an authored height
        // below one cell, so this is reachable only from a poke or a hand-built document --
        // but it WAS reachable before TYPE-1 and such an object's name was the only trace of
        // it on the workspace, and a region with no bounds shows nothing and says nothing
        // about it. Measured: without the floor, three zero-height objects lost their names
        // outright. The floor restores byte-for-byte the run of cells the label drew, in
        // every medium, because one cell of room is a cell region either way.
        //
        // THE CUT IS MARKED, and before TYPE-0 it was not. `resize` here was a silent
        // truncation of a string a MAKER chose (up to `doc::kMaxNameLen`), which is the exact
        // defect INTR-0 found in the picker's name column and repaired the same way: a shorter
        // name that looks finished is a lie about the document. `detail::fit` marks it.
        //
        // AND THE MEASURED COST OF ALL OF IT, WRITTEN HERE BECAUSE IT IS THIS CALL SITE'S:
        // A NAME LONGER THAN ITS OBJECT IS UNREADABLE WHERE IT LEAVES ONE, in a medium that
        // paints roles as ink. The name is `kMuted` so it reads quietly on the object's
        // `kFill` body; the workspace backdrop three statements up is ALSO `kMuted`, so the
        // overhang is the workspace's exact colour. Before TYPE-1 it was legible for a reason
        // nobody chose: every label cell was cleared to the canvas background first, which is
        // the same hole in the workspace that it was in the object. Measured live, both
        // trees, same document; TYPE-1's report-back carries the pair of pictures.
        //
        // IT IS NOT FIXED HERE, AND THE REASON IS THAT NO ROLE FIXES IT. This medium's inks
        // are kFill 176, kAccent 112/232/240, kMuted 96 and kAlert red: nothing reads on BOTH
        // a `kFill` body and a `kMuted` backdrop, `kAccent` means "the one thing being pointed
        // at" and would make every object shout, and a fifth role is exactly what
        // `surface/vocabulary.hpp` refuses. Contrast is a palette question and the palette is
        // the medium's -- which is the whole reason a publisher ships roles. The alternatives
        // are all product decisions with a wider blast radius than this phase has evidence
        // for: bound the name to the object's own width (a real behaviour change -- the bound
        // has been the WORKSPACE's right edge since the tool had one), give the workspace a
        // different role, or give this row a `background` (which paints a strip the full width
        // of the region and so claims material the object does not have). Reported, not
        // guessed at.
        const ui::Element* authored = doc::find(d, p.id);
        const std::int64_t room = s.workspace_w - p.rect.x;
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
    // front order, so a later-ranked pane covers an earlier one kind for kind (WIND-2a).
    //
    // THIS ONE CALL IS THE WHOLE OF PNL-0 AT THIS LEVEL. What used to be forty lines of
    // furniture painted unconditionally here is now a panel like any other: present because a
    // fresh session opens it, absent the moment a maker removes it, and painted by whoever
    // owns that kind rather than by `paint`.
    paint_panels(c, d, s, sc);

    // AND THE SCREEN'S OWN CHROME OVER THEM, on its own plane. See the note at the top of
    // this function for why it is in front rather than behind: this is the row the
    // composition shares and the band the tool speaks in.
    c.layers.emplace_back();
    on = &c.layers.back();

    label(0, 0,
          "WORKSPACE " + std::to_string(s.workspace_w) + "x" + std::to_string(s.workspace_h) +
              " cells",
          surface::role::kAccent);

    // HOW TO OPEN THE TERMINAL, and it is up here rather than on the help lines because the
    // help lines have no room -- both are within a few cells of the canvas width already, and
    // abbreviating a gesture a maker uses constantly to advertise one they may never use would
    // be paying for the new thing with the old. This row has twenty-one free cells to its
    // right; the hint is twenty. A mode with no way to discover it is not a feature. Both
    // this and OBJECTS below are measured from the RIGHT edge, so a wider surface moves them
    // together and the twenty-one cells between them are the same twenty-one cells.
    label(sc.w - 20, 0, "shift+space terminal", surface::role::kMuted);

    // THE TWO GESTURES THAT OPEN SOMETHING, on the same row and for the same reason the
    // terminal hint is there: the two help lines at the bottom are within a few cells of the
    // canvas width already, and this row has twenty-nine free cells between the workspace
    // title and OBJECTS. They are LEFT-anchored beside the title rather than right-anchored
    // beside the terminal hint, because a maker looking for something to do with the
    // workspace reads left to right and the stack `p` opens into is on this side.
    //
    // `w` IS HERE BECAUSE A MODE WITH NO WAY TO DISCOVER IT IS NOT A FEATURE (WIND-2a).
    // WIND-2 documented pane management's keys inside the mode and named none of them
    // outside it, so the one gesture that gets a maker in was reachable only by reading the
    // source. It is ONE label with both hints rather than two labels, because what has to
    // be true is a fact about the whole run -- 25 cells beginning at column 24, ending at
    // 48, one clear of `OBJECTS` at the minimum composition's column 50 -- and a single
    // string is the only spelling of that fact that cannot be half-moved.
    label(24, 0, "[+ panel]  p  [window]  w", surface::role::kMuted);

    // THE BOTTOM BAND, AND IT BELONGS TO THE OVERLAY WHILE THAT IS OPEN, which is why these
    // three lines are conditional. The pane is anchored to the bottom-right corner and
    // covers most of the screen's width at every extent, so a notice or a help line painted
    // underneath it would survive only in the cells to its left -- a sentence beheaded
    // mid-word with nothing to say so, which is the exact failure `detail::fit` exists to
    // prevent one line further down. Half a hint beside a pane is worse than no hint, and
    // the pane's own header carries the one gesture that matters while it is open.
    //
    // IT IS THE SCREEN'S FURNITURE, SO IT IS ON THE SCREEN'S PLANE (WIND-2a), and that plane
    // is written AFTER the panes rather than before them -- the always-on-top host chrome
    // this function's opening note selects. No panel's reactive rectangle reaches this band
    // -- `screen_of` reserves it and the assertions above say so -- so for every arrangement
    // the developer composes the picture is byte-identical. What changed is the one case a
    // maker can now author: a pane PLACED over the notice line is covered BY it, because the
    // panes are in front of the DOCUMENT and not in front of the tool's own voice. The band
    // occupies no pointer space, so `occupied_at` still answers the pane for those cells.
    if (!s.terminal.open) {
        // WHICH SETUP THIS IS -- the band's first row, which was blank, and the one fact
        // WS-0 added that a maker needs continuously rather than at the moment they act.
        // Painted with the notice and the help lines rather than with the panels, because it
        // is the screen's own furniture and no panel's: removing every panel does not remove
        // it.
        label(0, sc.notice_y - 1, setup_line(s, setup_path, sc), surface::role::kMuted);

        // The notice, fitted to the one line it has. `Session::notice` keeps the
        // whole sentence -- the fit happens HERE, at the presentation boundary, and
        // nowhere upstream, so no document operation is made less informative
        // because this screen happens to be as wide as it is. What a maker sees is
        // bounded; what Workshop knows is not, and the mark is what tells them the
        // two are different right now. A wider surface therefore needs nothing from
        // anybody but room, which is what this line said before there was any, and
        // a bigger window now spends that room on more of the sentence.
        //
        // IT IS THE ONE PIECE OF SCREEN CHROME THAT COULD MIGRATE (TYPE-0), and the reason is
        // arithmetic rather than importance. The band under the workspace is FIVE cells for
        // FOUR sentences and a spare -- the setup line, the notice, that spare, and two help
        // lines -- and
        // a graphical face's line is taller than a cell: 18 device pixels against 12, so a
        // region N cells tall holds `(12N - 4) / 18` rows of type. One cell holds ZERO, which
        // is why `fit_region` sends a one-cell region back to the cell projection (HD-5) and
        // why every other row of this band would be block text however it was published. The
        // notice is the only one with a neighbour to borrow: the spare row beneath it is
        // reserved by `kBottomRows`, painted by nobody, and two cells hold exactly one row of
        // the face. So the tool's own VOICE is set in the tool's own type, and nothing moved.
        //
        // A REGION TAKES ITS RECTANGLE, and here that is a widening: the label cleared only
        // the cells its characters landed on, and the region clears both rows across the
        // canvas. Over the workspace that is invisible in both media (the band is below it);
        // over a panel in the stack's second slot it erases one row more than the sentence
        // used to. That is the band doing what a band is for, and it happens only while there
        // is something to say -- an empty notice publishes no region at all, exactly as it
        // published no label.
        if (!s.notice.empty()) {
            surface::SurfaceTextRegion notice;
            notice.x = 0;
            notice.y = sc.notice_y;
            notice.w = sc.w;
            notice.h = kNoticeRows;
            const surface::RegionFit fit =
                surface::fit_region(notice.x, notice.y, notice.w, notice.h,
                                    sc.text_advance_px, sc.text_line_px);
            notice.rows.push_back(surface::SurfaceTextRow{
                detail::fit(s.notice, fit.columns),
                s.notice_is_bad ? surface::role::kAlert : surface::role::kFill});
            on->texts.push_back(std::move(notice));
        }
        // Two lines, because the canvas clips at its own width and a help line that
        // silently loses its last hint is worse than no hint. The maker's gestures
        // come first; the tool's own furniture second.
        //
        // It advertises `shift+hjkl` and not `,. width | -= height`: those four
        // literal bindings do not exist, and a help line naming them would be the
        // tool's own instructions telling a maker to press keys that do nothing.
        label(0, sc.help_y,
              "n new | d delete | hjkl move | shift+hjkl size | tab object | q quit",
              surface::role::kMuted);
        label(0, sc.help_y + 1,
              "enter edit | esc cancel | up/down row | [ ] workspace | ^s save | ^o open",
              surface::role::kMuted);
    }

    if (s.terminal.open) {
        // THE FINAL MODAL PLANE, and that is the whole of what "overlay" means here. A pane
        // in the last layer covers whatever it lands on -- and the screen underneath is
        // composed exactly as it was before this phase, with no row budget taken from it and
        // no constant moved. A closed pane appends no layer at all.
        detail::on_own_layer(c, [&](surface::SurfaceLayer& layer) {
            paint_terminal(layer, s.terminal, sc);
        });
    }

    return c;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_HPP
