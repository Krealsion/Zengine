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

#include "document.hpp"
#include "panel.hpp"
#include "property.hpp"
#include "vocabulary.hpp"

#include "input/vocabulary.hpp"  // `space` -- which medium's numbers a pointer reported in
#include "surface/pointing.hpp"  // and what that medium's layout makes of them
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
// it is spent in exactly two places: the WORKSPACE takes the extra columns and rows, and the
// terminal overlay takes half of them back while it is open. The panel column beside the
// workspace keeps its width, the inspector keeps its rows, and the bottom band keeps its
// shape -- their sizes are decisions about how much of each thing is worth showing, not
// shares of a screen, and turning them into shares would be inventing a layout policy this
// phase has no evidence for.
//
// EVERY CONSTANT BELOW THAT SURVIVED IS ONE OF TWO KINDS: a MINIMUM (the smallest surface
// this composition is honest on), or a FIXED SIZE (something whose right size does not
// depend on how much room there is). `Screen` holds what is derived, `screen_of` is the one
// place the derivation happens, and the static_asserts underneath it pin that the minimum
// screen is byte-for-byte the 78x22 composition that existed before this phase.

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

/// The rows INSIDE the side region's bounds: where the object list starts, how many names
/// it shows, and where the inspector starts under it. They are the same three numbers they
/// have always been, because the region begins at the top of the canvas -- what PNL-1
/// changed is what they are measured FROM, so the panel painted here reads its own column's
/// x and y off the bounds it was handed and knows nothing about the screen around it.
inline constexpr std::int64_t kSideY = 0; ///< the region's top edge: the canvas's own
inline constexpr std::int64_t kListY = 1;
inline constexpr std::int64_t kListRows = 5;
inline constexpr std::int64_t kRowsY = 8;

/// The band under the workspace: a spare row, the notice, a spare row, and the two help
/// lines. FIXED for the same reason the panel is -- it holds a known number of sentences.
inline constexpr std::int64_t kBottomRows = 5;

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
inline constexpr std::int64_t kStackX = 0;
inline constexpr std::int64_t kStackY = kWorkspaceY; ///< directly under the screen's title row
inline constexpr std::int64_t kStackW = 48;          ///< wide enough for a build recipe's tail
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
// ANCHORED TO THE CANVAS'S BOTTOM-RIGHT CORNER, and that is still the whole of the geometry:
// the pane's right edge IS the screen's and its bottom edge IS the screen's. Nothing is
// rearranged to make room -- it is an OVERLAY, drawn last, and while it is open it covers the
// furniture underneath rather than pushing it aside.
//
// WHAT G-2 CHANGED is that there is now sometimes more room, and the pane takes HALF of it.
// Half, because the two things competing for a bigger surface are the workspace a maker is
// building in and the record they are reading, and neither deserves all of it: at the
// minimum extent the pane is the same 56x13 it has always been, and every two columns the
// surface gains are one column of workspace and one column of pane. The alternative rules
// were both worse -- a FIXED pane wastes the room the phase exists to make usable, and a
// pane that keeps its gutters swallows a growing share of the screen until it is the tool.
//
// The old honest limit here -- "a canvas has no notion of the medium's size, and giving it
// one is a Surface question, not a Workshop one" -- was answered rather than removed. The
// Surface package now says it (`surface::SurfaceExtent`), Workshop hears it, and this
// arithmetic is what a Workshop screen makes of the answer.
inline constexpr std::int64_t kTerminalMinW = 56;
inline constexpr std::int64_t kTerminalMinH = 13;
/// Header, the standing statement, the omission marker, the input line: the rows a pane
/// spends on being a pane, whatever is in it. Everything else is transcript.
inline constexpr std::int64_t kTerminalChrome = 4;

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
    std::int64_t terminal_w = 0;
    std::int64_t terminal_h = 0;
    std::size_t terminal_rows = 0; ///< transcript rows the pane has room for
};

/// The furniture for a surface of this extent -- TOTAL over every std::int64_t, because the
/// extent it is given came off the bus.
inline constexpr Screen screen_of(std::int64_t want_w, std::int64_t want_h) noexcept {
    Screen s;
    s.w = want_w < kScreenMinW ? kScreenMinW : (want_w > kScreenMaxW ? kScreenMaxW : want_w);
    s.h = want_h < kScreenMinH ? kScreenMinH : (want_h > kScreenMaxH ? kScreenMaxH : want_h);
    s.panel_x = s.w - kPanelCols;
    s.room_w = s.panel_x - kPanelGap;
    s.room_h = s.h - kWorkspaceY - kBottomRows;
    s.notice_y = s.h - 4;
    s.help_y = s.h - 2;
    s.terminal_w = kTerminalMinW + (s.w - kScreenMinW) / 2;
    s.terminal_h = kTerminalMinH + (s.h - kScreenMinH) / 2;
    s.terminal_x = s.w - s.terminal_w;
    s.terminal_y = s.h - s.terminal_h;
    s.terminal_rows = static_cast<std::size_t>(s.terminal_h - kTerminalChrome);
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
static_assert(kMinScreen.terminal_x == 22 && kMinScreen.terminal_y == 9, "the pane's corner");
static_assert(kMinScreen.terminal_w == 56 && kMinScreen.terminal_h == 13, "the pane's extent");
static_assert(kMinScreen.terminal_rows == 9, "the transcript rows the pane has always had");

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
    return ui::Rect{kStackX, kStackY + n * (kStackRows + kStackGap), kStackW, kStackRows};
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
    ui::Rect rect{};
};

/// WHERE AN OPEN PANEL IS RIGHT NOW — the one narrow path, and the only thing that knows how
/// a slot is earned.
///
/// A panel takes the next slot in the stack only if it is PLACED in the stack, so an Info
/// ahead of a Builder in the open list never pushes it down a slot it does not occupy. That
/// rule used to be a counter inside the painting loop that named a kind; it is stated here
/// once, and a third kind is counted by it without being mentioned in it.
inline PanelBounds bounds_of(const Panels& panels, std::int64_t kind, const Screen& sc) noexcept {
    std::size_t slot = 0;
    for (const Panel& p : panels.open) {
        const std::int64_t where = placement_of(p.kind);
        if (p.kind == kind) {
            return PanelBounds{true, where, placement_bounds(where, slot, sc)};
        }
        if (where == placement::kOverlayStack) {
            ++slot;
        }
    }
    return PanelBounds{false, placement_of(kind), ui::Rect{}};
}

// The two places fit the SMALLEST screen this composition is honest on, which is the only
// extent they have to fit: a wider surface gives the workspace more room and gives each of
// these exactly as much as it had, the same rule the bottom band follows. Asserted over the
// RESOLVED rectangles rather than over the constants behind them, which is what PNL-1 bought
// -- "these two places do not overlap" is now one comparison of two rectangles instead of a
// hand-checked relation between four separate numbers.
inline constexpr ui::Rect kMinSide = placement_bounds(placement::kSideRegion, 0, kMinScreen);
inline constexpr ui::Rect kMinStack = placement_bounds(placement::kOverlayStack, 0, kMinScreen);

static_assert(kMinStack.x + kMinStack.w <= kMinSide.x - kPanelGap,
              "the two places do not overlap: a stacked panel never reaches the side region");
static_assert(kMinStack.x + kMinStack.w == kMinScreen.room_w,
              "the stack is exactly the minimum screen's workspace width -- it covers the top "
              "of the workspace and nothing else");
static_assert(kMinStack.y + kMinStack.h <= kMinScreen.notice_y,
              "the stack's first slot stays clear of the notice line");
static_assert(kMinSide.x + kMinSide.w == kMinScreen.w,
              "the side region reaches the screen's right edge");
static_assert(kMinSide.y + kMinSide.h == kWorkspaceY + kMinScreen.room_h,
              "the side region ends where the workspace does, above the bottom band");
static_assert(kPickerRows <= kStackRows, "the picker is never taller than a panel");

/// WHERE THE PICKER OPENS: the stack's first slot, and it is a function rather than a repeated
/// expression so that the mode that PAINTS there and the pointer that must not see THROUGH it
/// read one answer. The picker has no catalog row to declare a place in -- it is a mode -- so
/// this is the one presentation that names its own place, and now it names it once.
inline constexpr ui::Rect picker_bounds(const Screen& sc) noexcept {
    return placement_bounds(placement::kOverlayStack, 0, sc);
}

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
    const char* what = "";
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
inline Occupancy occupied_at(const Panels& panels, const Screen& sc, std::int64_t cx,
                             std::int64_t cy) noexcept {
    if (panels.picker.open && picker_bounds(sc).contains(cx, cy)) {
        return Occupancy{true, kPickerName};
    }
    for (std::size_t i = panels.open.size(); i > 0; --i) {
        const std::int64_t kind = panels.open[i - 1].kind;
        if (bounds_of(panels, kind, sc).rect.contains(cx, cy)) {
            return Occupancy{true, panel_kind(kind).name};
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
    std::string input;         ///< the line being typed, before Return authors anything
    std::vector<loom::TranscriptEntry> shown; ///< the newest entries that FIT, oldest first
    std::uint64_t earlier = 0; ///< kept by the participant, above the top of this pane
    std::uint64_t dropped = 0; ///< evicted from the transcript entirely -- gone, not scrolled
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
};

/// This session's screen furniture. The one call; see `Screen`.
inline constexpr Screen screen_of(const Session& s) noexcept {
    return screen_of(s.screen_w, s.screen_h);
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
inline bool adopt_screen(Session& s, std::int64_t want_w, std::int64_t want_h) {
    const Screen fresh = screen_of(want_w, want_h);
    if (fresh.w == s.screen_w && fresh.h == s.screen_h) {
        return false;
    }
    s.screen_w = fresh.w;
    s.screen_h = fresh.h;
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
// The panel is `kListRows` lines tall and a document is any size, so some
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

/// Which authored objects the OBJECTS panel is showing, and how many it is
/// leaving out on each side of them.
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
struct ListWindow {
    std::size_t first = 0;  ///< the first object shown, as a position in DOCUMENT order
    std::size_t count = 0;  ///< how many are shown, contiguously, in that order
    std::size_t before = 0; ///< how many the panel left out ahead of them
    std::size_t after = 0;  ///< how many it left out behind them
};

/// What `rows` lines can honestly show of `total` objects while the
/// `selected_at`'th is selected.
///
/// THREE RULES, in this order:
///
///   1. A document that FITS is shown whole, with no marker and no chrome at
///      all. The simple case stays the simple case: zero through `kListRows`
///      objects look exactly as they did before this function existed.
///   2. The SELECTED object is always in the window. It is the object the status
///      line and the inspector are both already naming, so a list that omits it
///      does not merely hide an object -- it contradicts the rest of the screen,
///      which is the defect rather than a symptom of it.
///   3. Every object left out is COUNTED, on the side it was left out on, and
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
        // Unreachable at kListRows = 5; here so the arithmetic below may assume
        // the room it needs rather than underflow looking for it.
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

/// The overlay, painted OVER the finished screen.
///
/// Every row is a label padded to the pane's full width, which is what CLEARS the furniture
/// underneath in a medium whose ink is one character per cell -- a space is a glyph like any
/// other, and painter's order makes the last writer win. The backdrop rect underneath is for
/// media that draw glyphs rather than cells: there a space draws nothing, so without it the
/// pane would be text floating over the workspace. Both media are served, and neither the
/// canvas vocabulary nor any Skin needed a new role, a new shape or a new slot to serve them.
inline void paint_terminal(surface::SurfaceCanvas& c, const TerminalPane& t, const Screen& sc) {
    if (!t.open) {
        return;
    }
    c.rects.push_back(surface::SurfaceRect{sc.terminal_x, sc.terminal_y, sc.terminal_w,
                                           sc.terminal_h, surface::role::kMuted});
    const auto row = [&c, &sc](std::int64_t line, const std::string& text, std::int64_t role) {
        c.labels.push_back(surface::SurfaceLabel{
            sc.terminal_x, sc.terminal_y + line,
            detail::pad(detail::fit(text, sc.terminal_w),
                        static_cast<std::size_t>(sc.terminal_w)),
            role});
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
    // chose `shown` with the same arithmetic (`entries_that_fit`), so this loop is where that
    // choice is CARRIED OUT rather than where it is made; the truncation below can only fire
    // for a single entry taller than the whole pane, which is the case that function names.
    std::vector<std::string> lines;
    for (const loom::TranscriptEntry& e : t.shown) {
        for (std::string& line : terminal_wrapped(e, sc.terminal_w)) {
            lines.push_back(std::move(line));
        }
    }
    if (lines.size() > sc.terminal_rows) {
        lines.resize(sc.terminal_rows);
    }

    // EVERY transcript row is written, including the ones with nothing in them. A row left
    // unwritten shows the backdrop's own glyph -- `.` in a terminal -- so a short session
    // rendered as a pane with holes punched through it into the workspace behind. Seen in the
    // first live rasterization, and fixed here rather than by giving the backdrop a quieter
    // glyph, because the pane's shape should not depend on how much it happens to be showing.
    for (std::size_t i = 0; i < sc.terminal_rows; ++i) {
        const std::int64_t line = 2 + static_cast<std::int64_t>(i);
        row(line, i < lines.size() ? lines[i] : std::string(), surface::role::kFill);
    }
    row(sc.terminal_h - 2, terminal_omission(t), surface::role::kMuted);
    // The cursor is a character, for the same reason the size handle is: this canvas has no
    // notion of a caret, and a maker needs to see where the next keystroke lands.
    row(sc.terminal_h - 1, "> " + t.input + "_",
        t.attached ? surface::role::kAccent : surface::role::kAlert);
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
inline void paint_panel_frame(surface::SurfaceCanvas& c, const ui::Rect& b) {
    c.rects.push_back(surface::SurfaceRect{b.x, b.y, b.w, b.h, surface::role::kMuted});
}

/// One row of an overlaid panel, fitted and padded to its bounds' width.
inline void paint_panel_row(surface::SurfaceCanvas& c, const ui::Rect& b, std::int64_t line,
                            const std::string& text, std::int64_t role) {
    c.labels.push_back(surface::SurfaceLabel{
        b.x, b.y + line, detail::pad(detail::fit(text, b.w), static_cast<std::size_t>(b.w)),
        role});
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
inline void paint_builder(surface::SurfaceCanvas& c, const BuilderPane& pane,
                          const ui::Rect& b) {
    paint_panel_frame(c, b);
    const auto row = [&c, &b](std::int64_t line, const std::string& text, std::int64_t role) {
        paint_panel_row(c, b, line, text, role);
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
    row(2,
        pane.awaiting ? panel_field("last", "asked -- waiting for it to finish")
                      : panel_field("last", builder::name_of_outcome(s.outcome)),
        pane.awaiting ? surface::role::kAccent
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
inline void paint_picker(surface::SurfaceCanvas& c, const Panels& panels, const Screen& sc) {
    const PanelPicker& picker = panels.picker;
    if (!picker.open) {
        return;
    }
    const ui::Rect b = picker_bounds(sc);
    paint_panel_frame(c, b);
    paint_panel_row(c, b, 0, "+ PANEL -- up/down, enter opens or removes",
                    surface::role::kAccent);
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        const bool here = i == picker.cursor;
        paint_panel_row(c, b, 1 + static_cast<std::int64_t>(i),
                        std::string(here ? "> " : "  ") + detail::pad(kPanelCatalog[i].name, 10) +
                            detail::pad(panels.has(kPanelCatalog[i].kind) ? "open" : "closed", 8) +
                            kPanelCatalog[i].summary,
                        here ? surface::role::kAccent : surface::role::kFill);
    }
    // THE REST OF THE SLOT, PADDED AND BLANK. In a character medium those spaces are what
    // erase the panel underneath; without them the panel's own rows read as more of this
    // list. See kPickerRows.
    for (std::int64_t row = kPickerRows; row < b.h; ++row) {
        paint_panel_row(c, b, row, std::string(), surface::role::kFill);
    }
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
/// EVERY COORDINATE BELOW IS RELATIVE TO `b`. `kListY` and `kRowsY` are rows within this
/// panel; `b.x` is its column. There is no `Screen` here any more, which is the measurable
/// half of PNL-1 for this kind: what used to be "the painter reads `sc.panel_x`, the same
/// number `screen_of` gives the workspace to measure against" is now "the painter is told
/// where it is".
inline void paint_info(surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                       const ui::Rect& b) {
    // THE BACKDROP FIRST, so everything below is written over it and nothing authored
    // survives underneath it. One rect, the whole of `b`, and the same call the other two
    // presentations make.
    paint_panel_frame(c, b);
    const auto label = [&c](std::int64_t x, std::int64_t y, std::string text, std::int64_t role) {
        c.labels.push_back(surface::SurfaceLabel{x, y, std::move(text), role});
    };

    // The object list: the same objects, named by identity, pointing at the same
    // selection the ring in the workspace does -- and, when there are more of
    // them than the panel is tall, SAYING how many it is not showing and on
    // which side. The markers are in the panel's own muted role because they are
    // the tool's furniture and not authored material: nothing here mints an
    // identity, invents a name, or reorders a document to make a screen fit.
    label(b.x, b.y + kListY - 1, "OBJECTS", surface::role::kAccent);
    const ListWindow window = list_window(d.elements.size(), position_of(d, s.selected),
                                          static_cast<std::size_t>(kListRows));
    std::int64_t line = 0;
    if (window.before > 0) {
        label(b.x, b.y + kListY + line, omitted_text(window.before, "earlier"),
              surface::role::kMuted);
        ++line;
    }
    for (std::size_t i = 0; i < window.count; ++i) {
        const ui::Element& e = d.elements[window.first + i];
        const bool chosen = e.id == s.selected;
        label(b.x, b.y + kListY + line,
              std::string(chosen ? "> " : "  ") + "#" + std::to_string(e.id) + " " + e.label,
              chosen ? surface::role::kAccent : surface::role::kFill);
        ++line;
    }
    if (window.after > 0) {
        label(b.x, b.y + kListY + line, omitted_text(window.after, "more"),
              surface::role::kMuted);
        ++line;
    }
    // An empty document SAYS it is empty. A maker can reach this state with their
    // own hand by deleting their work, and a panel that merely goes blank is
    // indistinguishable from a tool that has broken. It also says what to do
    // next, because the answer is one
    // key and the alternative is a maker who thinks they have destroyed it.
    if (d.elements.empty()) {
        label(b.x, b.y + kListY, "(none) -- n makes one", surface::role::kMuted);
    }

    // The inspector.
    label(b.x, b.y + kRowsY - 1, "PROPERTIES", surface::role::kAccent);
    if (s.rows.empty()) {
        label(b.x, b.y + kRowsY, "(nothing selected)", surface::role::kMuted);
    }
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        const Row& row = s.rows[i];
        const bool here = i == s.cursor;
        std::int64_t role = surface::role::kFill;
        if (row.editing()) {
            role = surface::role::kAlert; // a live draft is never quiet
        } else if (!row.editable()) {
            role = surface::role::kMuted; // not the maker's to author
        }
        label(b.x, b.y + kRowsY + static_cast<std::int64_t>(i),
              std::string(here ? ">" : " ") + detail::pad(row.label(), 9) + row.display(), role);
    }
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
inline void paint_panels(surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                         const Screen& sc) {
    const Panels& panels = s.panels;
    for (const Panel& p : panels.open) {
        const ui::Rect b = bounds_of(panels, p.kind, sc).rect;
        if (p.kind == panel::kBuilder) {
            paint_builder(c, panels.builder, b);
        } else if (p.kind == panel::kInfo) {
            paint_info(c, d, s, b);
        }
    }
    paint_picker(c, panels, sc);
}

/// The whole screen as one published canvas.
///
/// Painter's order, which is list order: the workspace backdrop, then each
/// authored element as the scene placed it (with the selected one's ring UNDER
/// it, so the ring reads as a ring rather than a border the object grew), then
/// every label over everything.
///
/// The picture is derived from `workspace_scene()` and from nothing else, which
/// is what makes "what you see is what the hit test answers about" structural
/// rather than a claim: the two read one value.
inline surface::SurfaceCanvas paint(const WorkshopDoc& d, const Session& s) {
    const Screen sc = screen_of(s);
    surface::SurfaceCanvas c;
    c.width = sc.w;
    c.height = sc.h;

    const auto rect = [&c](std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h,
                           std::int64_t role) {
        c.rects.push_back(surface::SurfaceRect{x, y, w, h, role});
    };
    const auto label = [&c](std::int64_t x, std::int64_t y, std::string text, std::int64_t role) {
        c.labels.push_back(surface::SurfaceLabel{x, y, std::move(text), role});
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
        const ui::Element* authored = doc::find(d, p.id);
        const std::int64_t room = s.workspace_w - p.rect.x;
        if (authored != nullptr && room > 0) {
            std::string shown = authored->label;
            if (static_cast<std::int64_t>(shown.size()) > room) {
                shown.resize(static_cast<std::size_t>(room));
            }
            label(x, y, shown, surface::role::kMuted);
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

    // HOW TO OPEN A PANEL, on the same row and for the same reason the terminal hint is
    // there: the two help lines at the bottom are within a few cells of the canvas width
    // already, and this row has twenty-nine free cells between the workspace title and
    // OBJECTS. It is LEFT-anchored beside the title rather than right-anchored beside the
    // terminal hint, because a maker looking for something to do with the workspace reads
    // left to right and the stack it opens into is on this side.
    label(24, 0, "[+ panel]  p", surface::role::kMuted);

    // THE DYNAMIC PANELS -- every one of them, INCLUDING the OBJECTS and PROPERTIES columns
    // a maker has always read on the right. They come after the scene and the size handle so
    // that in a character medium a stacked panel's padded rows erase the authored material
    // behind them rather than being erased by it, and before the notice and help lines, which
    // no panel reaches.
    //
    // THIS ONE CALL IS THE WHOLE OF PNL-0 AT THIS LEVEL. What used to be forty lines of
    // furniture painted unconditionally here is now a panel like any other: present because a
    // fresh session opens it, absent the moment a maker removes it, and painted by whoever
    // owns that kind rather than by `paint`.
    paint_panels(c, d, s, sc);

    // THE BOTTOM BAND BELONGS TO THE OVERLAY WHILE IT IS OPEN, and that is why these three
    // lines are conditional. The pane is anchored to the bottom-right corner and covers most
    // of the screen's width at every extent, so a notice or a help line painted underneath it
    // would survive only in the cells to its left -- a sentence beheaded mid-word with nothing
    // to say so, which is the exact failure `detail::fit` exists to prevent one line further
    // down. Half a hint beside a pane is worse than no hint, and the pane's own header carries
    // the one gesture that matters while it is open.
    if (s.terminal.open) {
        // LAST, and that is the whole of what "overlay" means here. Painter's order is list
        // order, so a pane appended after everything covers whatever it lands on -- and the
        // screen underneath is composed exactly as it was before this phase, with no row budget
        // taken from it and no constant moved. A closed pane appends nothing at all.
        paint_terminal(c, s.terminal, sc);
        return c;
    }

    // The notice, fitted to the one line it has. `Session::notice` keeps the
    // whole sentence -- the fit happens HERE, at the presentation boundary, and
    // nowhere upstream, so no document operation is made less informative
    // because this screen happens to be as wide as it is. What a maker sees is
    // bounded; what Workshop knows is not, and the mark is what tells them the
    // two are different right now. A wider surface therefore needs nothing from
    // anybody but room, which is what this line said before there was any, and
    // a bigger window now spends that room on more of the sentence.
    if (!s.notice.empty()) {
        label(0, sc.notice_y, detail::fit(s.notice, sc.w),
              s.notice_is_bad ? surface::role::kAlert : surface::role::kFill);
    }
    // Two lines, because the canvas clips at its own width and a help line that
    // silently loses its last hint is worse than no hint. The maker's gestures
    // come first; the tool's own furniture second.
    //
    // It advertises `shift+hjkl` and not `,. width | -= height`: those four
    // literal bindings do not exist, and a help line naming them would be the
    // tool's own instructions telling a maker to press keys that do nothing.
    label(0, sc.help_y, "n new | d delete | hjkl move | shift+hjkl size | tab object | q quit",
          surface::role::kMuted);
    label(0, sc.help_y + 1,
          "enter edit | esc cancel | up/down row | [ ] workspace | ^s save | ^o open",
          surface::role::kMuted);

    return c;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_HPP
