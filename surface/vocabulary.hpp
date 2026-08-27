// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_VOCABULARY_HPP
#define ZENGINE_SURFACE_VOCABULARY_HPP

// The Surface package's message vocabulary — deliberately tiny.
//
// The rule this package installs: no game, world, or panel weave talks to the
// terminal, a window, or a renderer. They PUBLISH visual intent; a **Skin** —
// an ordinary, replaceable loadable weave holding the `zengine.skin` role —
// claims the actual surface and turns intent into output. Swap the Skin and
// the same intent lands on a different medium; that replaceability is the
// whole point, and the suite proves it with a terminal Skin and an SDL Skin
// consuming identical messages.
//
// V1 intent is two shapes deep, and the first is borrowed:
//   - `SnakeVisual` (snake's own locked shape) is the first real canvas
//     payload. The Skins accept it directly — a V1 coupling, named face-up:
//     the general medium-agnostic canvas vocabulary that would dissolve it is
//     a later phase, not this one. What V1 proves is the PATTERN (intent in,
//     pixels/characters out, zero medium-specific fields in the intent).
//   - `SurfaceText` (here) is the one intent this package adds: a line of
//     plain text for a named slot. The host's status line and the score
//     weave's tally are its first publishers.
//
// V2 adds the GENERAL canvas the note above promised — `SurfaceCanvas` — but
// it does NOT dissolve the SnakeVisual coupling, and that restraint is
// deliberate. Workshop is the live consumer that pulled it: a maker tool
// has to paint an authored rectangle somewhere, and the alternatives were a
// second world beside the Skins (a Workshop-only painter) or teaching the
// Skins a Workshop-only shape. Both are worse than one general canvas that any
// Zengine app can publish. Re-expressing snake's own frame as a canvas is a
// separate, evidence-carrying move (the golden frames ARE the old drawers) and
// is not part of this addition — a general shape existing is not permission to
// migrate a proven one through it.
//
// Claiming and releasing the surface are NOT messages. A Skin claims its
// medium in its constructor and releases it in its destructor (the Input
// package's reader move: load takes the terminal's hand, unload gives it
// back), and the role system is the arbiter of "exactly one active Skin":
// roles are singleton — loading a second Skin into `zengine.skin` while one
// holds it is a clean Refused, pinned in the suite. The Weave Manager's
// swap already delivers the unload first, so a swap is release-then-claim by
// construction. A surface-arbiter weave (multiple surfaces, negotiation) is a
// later package's ground; V1 needs none.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::surface {

/// One line of PLAIN text for a named slot ("status", "score", ...). Plain
/// means plain: no escape codes, no markup — how a slot looks is the Skin's
/// business, which is exactly what lets the same intent land in a terminal
/// row, a window title, or anything a future Skin dreams up. Slots unknown to
/// the active Skin are dropped without ceremony (pub-sub: intent is an offer,
/// not a command).
struct SurfaceText {
    std::string slot;
    std::string text;
    ZEN_SHAPE(SurfaceText, 1, ZEN_FIELD(slot), ZEN_FIELD(text));
};

/// The visual ROLE of a canvas element — semantic, never a colour. The Skin
/// picks the actual ink, so one canvas reads correctly in a monochrome terminal
/// and in a themed window and a publisher never learns which medium it landed
/// on. (The Loom's PxRole stance, one layer down: this vocabulary DOES carry
/// geometry, because a canvas is geometry — what it still refuses to carry is
/// anything medium-specific. Cells, not pixels; roles, not RGB.) A role the
/// active Skin does not know falls back to `kFill` rather than vanishing: an
/// unknown role is still a rectangle somebody meant to be seen, and dropping it
/// would be the silent-blank fate this house refuses. Same posture as an
/// unknown text slot, opposite resolution — a slot has no place to go, a rect
/// does.
namespace role {
inline constexpr std::int64_t kFill = 0;   ///< ordinary authored material
inline constexpr std::int64_t kAccent = 1; ///< the one thing being pointed at
inline constexpr std::int64_t kMuted = 2;  ///< present, deliberately quiet
inline constexpr std::int64_t kAlert = 3;  ///< something the maker must see

/// NO ROLE AT ALL — the ABSENCE of one, and deliberately not a fifth role.
///
/// It exists because a background is the one place in this vocabulary where
/// "nothing" is a real, common and different answer from "ordinary": a row with
/// no background shows whatever ground it is sitting on, which is not the same
/// picture as a row painted in `kFill`. Every other field here names ink and has
/// no use for it, so it is only ever read where a shape says so.
///
/// It is NEGATIVE on purpose. The unknown-role fallback is `kFill` (see above),
/// so a positive sentinel would be indistinguishable from a role a later
/// vocabulary added — and the failure would be silent, in the widening
/// direction. Nothing may pass this to a Skin's role→ink table; a consumer tests
/// for it first.
inline constexpr std::int64_t kNone = -1;
} // namespace role

/// One filled rectangle, in CANVAS CELLS — the canvas's own square unit, which
/// each Skin resolves into its medium (one character column per cell in a
/// terminal, `kCanvasCellPx` pixels in a window). A cell is the honest common
/// unit: it is the coarsest thing a terminal can address, so a canvas authored
/// in cells lands somewhere real in every medium instead of being pixel-exact
/// in one and rounded into mush in the other.
///
/// SINCE WUX-2 A COORDINATE MAY CARRY A SUB-CELL REMAINDER (`sub_*`, in
/// 1/`kCellSubs` of a cell — see that constant below for the whole model). The
/// cell fields still mean exactly what they always did, the remainders default
/// to zero, and a publisher that thinks in whole cells publishes exactly the
/// bytes it always published. A remainder refines the one lattice; it is not a
/// second coordinate system, and each medium resolves it at its own grain — a
/// window spends it in pixels, a terminal floors it away at its projection.
///
/// Painter's order: `SurfaceLayer::rects` is drawn back-to-front in list
/// order, so a publisher expresses "behind" by publishing earlier. There is no
/// z field and no explicit stacking policy — list order already says it, and a
/// second way to say the same thing is how two orderings come to disagree.
struct SurfaceRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::int64_t role = role::kFill;
    std::int64_t sub_x = 0; ///< sub-cell remainder of x, in 1/kCellSubs cells; 0..kCellSubs-1
    std::int64_t sub_y = 0;
    std::int64_t sub_w = 0; ///< sub-cell remainder of w — the extent may be fine too
    std::int64_t sub_h = 0;
    // v2: the four sub-cell remainders (WUX-2). Everything a version-1 publisher
    // said is said identically by zeros here.
    ZEN_SHAPE(SurfaceRect, 2, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(role), ZEN_FIELD(sub_x), ZEN_FIELD(sub_y), ZEN_FIELD(sub_w),
              ZEN_FIELD(sub_h));
};

/// One run of PLAIN text anchored at a canvas cell, drawn over every rect.
/// Plain means plain, exactly as in SurfaceText: no escapes, no markup.
///
/// One cell per BYTE, in every medium: the terminal skins index `text[i]` and
/// the SDL skin draws one glyph per byte, so a canvas describes one picture
/// rather than one per backend. A Skin states in its own docs which bytes it has
/// a glyph for and what it draws for the rest; what no Skin may do is drop a
/// character silently, because a publisher cannot see that happen.
struct SurfaceLabel {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::string text;
    std::int64_t role = role::kFill;
    std::int64_t sub_x = 0; ///< sub-cell remainder of the anchor (WUX-2); a run has no fine EXTENT
    std::int64_t sub_y = 0; ///< — one cell per byte is the label's whole width contract
    // v2: the anchor's sub-cell remainders (WUX-2), zero for every v1 publisher.
    ZEN_SHAPE(SurfaceLabel, 2, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text), ZEN_FIELD(role),
              ZEN_FIELD(sub_x), ZEN_FIELD(sub_y));
};

/// ONE ROW OF PROSE inside a bounded text region. Plain text, a semantic role,
/// and — since HD-2 — a semantic GROUND to set it on. Deliberately nothing else:
/// no x, no y, no width. A row's PLACE is its index in its region's list, which
/// is what makes the region a bounded presentation rather than a second
/// coordinate system.
///
/// `background` IS THE WHOLE OF HD-2's VOCABULARY ADDITION, and it is here rather
/// than anywhere richer because of what a person cannot otherwise be told: which
/// row of a list they are on. A selected row, a pressed control and an error
/// highlight are all one question — "this row, not those rows" — and ink alone
/// cannot answer it, because ink alone is already spoken for by what a row MEANS
/// (`role`). `role::kNone`, the default, is the absence of a ground: the row
/// shows whatever its region is sitting on, which is exactly the picture every
/// row drew before this field existed.
///
/// IT IS A ROLE, NOT A COLOUR, for the same reason `role` is — the Skin owns the
/// palette, so one selected row reads correctly in a monochrome terminal and in a
/// themed window. And it is emphatically not a theme system: there is one new
/// value (`kNone`) and no new role, so the number of things a medium must know
/// how to paint has not changed.
///
/// COLOUR ALONE IS NOT ENOUGH IN A CHARACTER MEDIUM, and this vocabulary already
/// knows it — `glyph_for_role` exists in the terminal Skin precisely because four
/// roles would otherwise paint four identical cells on a monochrome terminal. A
/// background inherits that argument whole, which is why a publisher marking a
/// row as selected should also SAY so in the row's own text rather than relying
/// on this field to carry the meaning by itself.
///
/// Plain ASCII, the same house rule `SurfaceLabel` states, and for the same
/// reason: the cell projection is one cell per BYTE, so a multi-byte sequence
/// would be split there. A graphical medium that sets real type may well render
/// such bytes as real characters; that divergence is not a promise this
/// vocabulary makes, and no first-party publisher produces one.
struct SurfaceTextRow {
    std::string text;
    std::int64_t role = role::kFill;
    std::int64_t background = role::kNone;
    ZEN_SHAPE(SurfaceTextRow, 2, ZEN_FIELD(text), ZEN_FIELD(role), ZEN_FIELD(background));
};

/// A REGION THAT HAS NO CARET SAYS SO WITH THIS — negative for the reason
/// `role::kNone` is: a prose row index is non-negative by construction, so the
/// sentinel cannot collide with any row a publisher might one day mean, and a
/// consumer reading it as a row would land far outside the region rather than on
/// its first line.
inline constexpr std::int64_t kNoCaret = -1;

/// WHAT A BOUNDED REGION'S RECTANGLE IS MADE OF -- the two, and only two, answers
/// (TYPE-1).
///
/// `kGroundOwn` is what every region drew before this constant existed and is the
/// default: the region OWNS its rectangle and clears the whole of it before a row
/// is drawn, so nothing may be underneath. That is not an implementation detail --
/// it is what makes a region honest about the room it was granted, and it is why
/// ordinary tool prose, panels, lists and panes are regions.
///
/// `kGroundBeneath` is the one thing this vocabulary previously could not say:
/// SEMANTIC TYPE ON MATERIAL SOMEBODY ELSE OWNS. The region keeps its bounds -- they
/// are still what its rows are fitted and cut against -- but it gives up the ground:
/// it draws its rows and nothing else, so whatever was published beneath it shows
/// wherever a glyph does not. A maker's name written across an authored object is
/// the consumer that earned it: the name is semantic (its cell occupancy is no part
/// of what a maker authored) and its container is authored MATERIAL, so neither
/// existing answer was true.
///
/// IT IS NOT A ROW'S `background`, AND THE TWO MUST NOT BE READ AS ONE FIELD. A row
/// that names no background defers to its region; a region has nothing to defer to,
/// so its two answers are about OWNERSHIP rather than about ink -- it either takes
/// the rectangle or it does not. That is why this is not spelled with `role::kNone`
/// and why the default here is the opposite of the default there.
///
/// AND IT IS NOT TRANSPARENCY, ALPHA OR COMPOSITING. There is no blend, no opacity,
/// no order of its own and no second rectangle: the region is in exactly the plane
/// its publisher put it in, and `kGroundBeneath` removes one fill. Everything a
/// medium already knew about painter's order still decides what "beneath" is.
/// AN UNKNOWN GROUND IS `kGroundOwn`, which is the same posture `role` takes one field
/// up and is chosen for the same reason: a value this vocabulary does not know is still a
/// region somebody meant to be seen, and the safe reading is the one every region had
/// before this constant existed. So a medium tests for `kGroundBeneath` EXACTLY and treats
/// everything else as owning its room -- never the other way round, which would let a
/// number nobody chose make a panel see-through.
inline constexpr std::int64_t kGroundOwn = 0;
inline constexpr std::int64_t kGroundBeneath = 1;

/// WHAT A CARET IS IN A MEDIUM WHOSE CHARACTER IS A CELL.
///
/// One character, INSERTED at the caret's column — which is exactly the picture
/// the Workshop Terminal drew before HD-3, when the caret could only ever be at
/// the end of the line and the pane appended this byte itself. Making the cell
/// projection do it is what keeps the character medium's answer honest for a
/// caret that can now be anywhere, without teaching it what a pixel is.
inline constexpr char kCaretGlyph = '_';

/// A REGION THAT HAS NO SELECTION SAYS SO WITH THIS — negative for `kNoCaret`'s
/// reason exactly: a prose row index is non-negative by construction, so the
/// absence cannot collide with a row anybody might one day mean.
inline constexpr std::int64_t kNoSelection = -1;

/// A BOUNDED REGION OF PROSE: placed in canvas CELLS like everything else here,
/// and filled with rows the active medium sets in its OWN text metric.
///
/// THIS WAS THE ONE PLACE A CANVAS ADMITTED THAT A MEDIUM MAY BE FINER THAN A
/// CELL; since WUX-2 the admission has a second, equally narrow half — the
/// geometry shapes may carry a sub-cell remainder on their bounds (see
/// `kCellSubs`). `x/y/w/h` are cells, so where a region sits is the same kind
/// of fact as where a rect sits and every medium can honour it, and `sub_*`
/// refine that place on the one shared lattice. What happens INSIDE is the
/// medium's: a terminal draws one row per cell row, truncated and clipped to
/// the cells the bounds cover; a window that owns a real face draws the rows at
/// its own advance and line height, inside the pixel rectangle those bounds
/// resolve to. Neither is pretending. The terminal is not asked to invent a
/// pixel, and the window is not asked to round its type onto a twelve-pixel
/// lattice.
///
/// HOW MANY ROWS AND COLUMNS FIT IS NOT ON THIS SHAPE, and that absence is the
/// load-bearing part. The medium publishes its text metric on `SurfaceExtent`;
/// the publisher resolves that metric against these cell bounds — through
/// `surface/region.hpp`, one function, the same one the medium resolves with —
/// and sends exactly the rows it decided fit. Two parties measuring would be two
/// answers, and a pane whose omission marker says "... 12 earlier" while its
/// renderer spends a different number of rows is a pane that lies.
///
/// A row longer than the region is the MEDIUM's to cut, exactly as an oversized
/// rect is: publishers truncate because the cell projection needs them to, and a
/// graphical medium clips as well because it cannot have its overflow predicted
/// by anybody else.
/// VERSION 5, AND THE LADDER OF REASONS IS WORTH KEEPING. Version 2 was
/// bookkeeping rather than ceremony: this shape's wire identity is computed
/// from its field types, one of which is a list of `SurfaceTextRow`, so a row
/// gaining a background changed what a region IS on the wire even though nothing
/// here was edited. Version 3 is the ordinary kind — HD-3 added the two caret
/// fields below — and version 4 is TYPE-1's `ground`. Version 5 is TEXT-0's
/// selection, below. Either way, leaving the declared version alone would have
/// meant two different content-ids wearing one version number, which is the
/// exact failure a version exists to prevent. The same sentence one layer out
/// has moved `SurfaceLayer` and `SurfaceCanvas` in step every time.
///
/// A REGION MAY HAVE A CARET, AND IT IS SAID IN THE REGION'S OWN PROSE LATTICE
/// (HD-3). `caret_row`/`caret_col` are a row index and a column index into this
/// region's rows — the same two numbers a row's text is written in — and never a
/// pixel, a cell or a coordinate on the canvas. That is what lets each medium
/// answer for itself with the metric it already resolved: a window that sets real
/// type fills a bar at `origin_x + caret_col * advance_px`, and a medium whose
/// character IS a cell puts a character there instead. Neither is asked to
/// convert anything the other did.
///
/// `kNoCaret` IS NEGATIVE ON PURPOSE, the same argument `role::kNone` makes one
/// field up: the absence of a caret must not be a value a later vocabulary could
/// collide with, and a row index is non-negative by construction, so the whole
/// negative half of the number line is free and unambiguous. A publisher that
/// says nothing gets exactly the picture every region drew before this existed.
///
/// A REGION MAY ALSO HAVE A SELECTED RANGE, SAID IN THE SAME LATTICE (TEXT-0).
/// `sel_begin_*`/`sel_end_*` are two caret-like positions — begin inclusive, end
/// exclusive, in READING ORDER — and the range between them is the text a
/// maker's next gesture acts on: what typing replaces, what copy takes, what
/// delete removes. On one row that is the columns `[begin_col, end_col)` of that
/// row; across rows it is the begin row from `begin_col` to that row's own end,
/// every row between whole, and the end row up to `end_col` — the shape a
/// multiline editor needs, carried now so the single-line publisher and the
/// future one speak one vocabulary. `surface/region.hpp` owns that per-row
/// arithmetic in ONE function both media consume.
///
/// WHICH END THE CARET IS AT IS NOT RESTATED HERE. A publisher whose selection
/// has an active end says so with the caret fields it already has; the range is
/// normalized so a medium never re-derives reading order from gesture history it
/// cannot see. A range that is absent (`kNoSelection`), empty, or not in reading
/// order shows nothing — a medium draws what a publisher meant, and a value no
/// publisher could mean is the absence, never a guess.
///
/// WHAT THE PAIR IS NOT. Not per-span styling (there is one range and it means
/// selection, not "these words are blue"), not focus, not multiple selections,
/// and not a claim the medium must track anything — it is a fact about THIS
/// picture, republished whenever the picture is.
struct SurfaceTextRegion {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::vector<SurfaceTextRow> rows;
    std::int64_t caret_row = kNoCaret; ///< the prose row it is on; kNoCaret = there is none
    std::int64_t caret_col = 0;        ///< ...and the prose column it sits BEFORE
    std::int64_t ground = kGroundOwn;  ///< whose rectangle this is; see kGroundOwn above
    std::int64_t sel_begin_row = kNoSelection; ///< reading-order start; kNoSelection = none
    std::int64_t sel_begin_col = 0;            ///< inclusive, a caret-like position
    std::int64_t sel_end_row = kNoSelection;   ///< reading-order end row
    std::int64_t sel_end_col = 0;              ///< exclusive, a caret-like position
    std::int64_t sub_x = 0; ///< sub-cell remainders of the BOUNDS (WUX-2); the prose lattice
    std::int64_t sub_y = 0; ///< (rows, columns, caret, selection) is untouched by them
    std::int64_t sub_w = 0;
    std::int64_t sub_h = 0;
    // v6: the bounds' sub-cell remainders (WUX-2) — the same compose-upward bump
    // every region field has cost, and zero for every v5 publisher.
    ZEN_SHAPE(SurfaceTextRegion, 6, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(rows), ZEN_FIELD(caret_row), ZEN_FIELD(caret_col), ZEN_FIELD(ground),
              ZEN_FIELD(sel_begin_row), ZEN_FIELD(sel_begin_col), ZEN_FIELD(sel_end_row),
              ZEN_FIELD(sel_end_col), ZEN_FIELD(sub_x), ZEN_FIELD(sub_y), ZEN_FIELD(sub_w),
              ZEN_FIELD(sub_h));
};

/// ONE ORDERED PAINTER PLANE: the three primitive kinds, drawn as one complete
/// picture before the next plane is drawn over it (WIND-2a).
///
/// WHY IT EXISTS, AND IT IS ONE SENTENCE: a publisher that has decided which of
/// two presentations is IN FRONT had no way to say so. The three lists used to
/// sit on the canvas itself, so painter's order across KINDS was global — every
/// rect, then every label, then every region — and a text region belonging to a
/// pane a maker had sent to the BACK still covered a label belonging to the pane
/// they had sent to the FRONT. Workshop's hit test answered with the front pane
/// and the medium painted the back one. The order was authored and unsayable.
///
/// WHAT IT IS NOT, and the list is the design. There is no coordinate transform,
/// no opacity, no blending, no clipping tree, no layer identity, name, handle or
/// persisted key, no numeric z or depth, no sorting, no ties, no epochs, no
/// accumulating counter, no hit testing and no window-manager behaviour. A layer
/// is a position in a vector. The publisher supplies an ALREADY ORDERED list and
/// the Skin executes it in that order — which is the same "list order is painter's
/// order" rule `SurfaceRect` has always stated, applied once more, one level out.
///
/// SO THIS IS NOT A COMPOSITOR AND MUST NOT BECOME ONE. Every field a compositor
/// has is a fact a publisher would then have to hold, and this vocabulary's whole
/// claim is that a publisher holds the PICTURE and the medium holds the pixels.
///
/// A NESTED VALUE, NEVER A MESSAGE. Nothing sends a `SurfaceLayer`; it exists
/// because `SurfaceCanvas` carries a list of them, exactly as `SurfaceTextRow`
/// exists because a region carries a list of those. It is not in any ordinary
/// vocabulary and no grant names it.
struct SurfaceLayer {
    std::vector<SurfaceRect> rects;
    std::vector<SurfaceLabel> labels;
    std::vector<SurfaceTextRegion> texts;
    // v3: a layer IS a list of regions, so a region gaining its selection fields (TEXT-0)
    // changed what a layer is on the wire — the same bookkeeping bump v2 was for the ground.
    // v4: all three lists' shapes gained their sub-cell remainders (WUX-2).
    ZEN_SHAPE(SurfaceLayer, 4, ZEN_FIELD(rects), ZEN_FIELD(labels), ZEN_FIELD(texts));
};

/// A whole canvas: an extent in cells, and the ordered planes that fill it.
/// The complete general drawing intent — and deliberately no more than that.
/// It is not a layout system and not a widget tree: it carries no parent/child
/// relationship, no anchors, no percentages, no policy. Whoever publishes it
/// has already decided where things go; the Skin only resolves cells into its
/// medium. That boundary is the whole reason this shape can stay this small,
/// and the reason the Loom's geometry-free semantic tree (loom::Widget) remains
/// a different, higher thing rather than something this competes with: that
/// tree describes intent a renderer must LAY OUT, this describes a picture a
/// medium must PAINT.
///
/// Elements outside the extent are the Skin's to clip. An empty canvas (no
/// layers at all) is a legitimate picture — it means "nothing", not "no intent" —
/// and clears whatever the previous canvas drew. So is a canvas of layers that
/// are themselves empty.
///
/// PAINTER'S ORDER IS TWO LEVELS AND THE WHOLE OF IT IS THIS (WIND-2a):
///
///     layers[0]              back-most
///     layers[n-1]            front-most
///     inside one layer       rects in list order, then labels over them, then
///                            text regions over those
///     between two layers     the complete earlier layer, then the complete
///                            later one over it
///
/// A region is the topmost thing IN ITS OWN LAYER because a region is an overlay:
/// it is granted bounds and owns what is inside them, which is exactly what would
/// be untrue if a label of the same presentation could land on top of one. What
/// changed in WIND-2a is that this stopped being a claim about the whole canvas —
/// a region belonging to a presentation somebody put BEHIND another one is behind
/// it, kind for kind, and no primitive had to gain a field to say so.
///
/// VERSION 8 SINCE WUX-2, and only WIND-2a's 5 ever changed the fields written
/// here. Versions 2, 3, 4, 6, 7 and 8 it gained nothing at all and changed
/// anyway, because its identity is derived from what it carries — a canvas is a
/// list of layers is a list of regions, so the region's ground (v6), its
/// selection (v7) and the geometry shapes' sub-cell remainders (v8, WUX-2) each
/// moved this number without an edit on this struct. There is no old-version
/// reader, no compatibility root list, no implicit base layer beside the
/// explicit ones and no second spelling of one picture — two valid ways to say
/// the same thing is how two orderings come to disagree, which is the defect
/// versioning exists to end. (The canvas EXTENT stays whole cells: how much
/// room a picture claims is the same coarse fact a medium reports, and a
/// fractional canvas edge is a picture nobody can honour.)
struct SurfaceCanvas {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::vector<SurfaceLayer> layers;
    ZEN_SHAPE(SurfaceCanvas, 8, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(layers));
};

/// One canvas cell in a graphical medium. The terminal needs no such number —
/// its cell IS a character — so this lives here as the one place a window-owning
/// Skin gets the conversion, rather than each inventing its own scale.
inline constexpr std::int64_t kCanvasCellPx = 12;

/// HOW FINE THE CANVAS LATTICE IS: sub-cell units per canvas cell (WUX-2).
///
/// The geometry shapes above carry their coordinates as whole cells plus a
/// `sub_*` remainder in 1/kCellSubs of a cell — a FIXED-POINT refinement of the
/// one lattice, with exactly one spelling per value (`0 <= sub_* < kCellSubs`,
/// the floor decomposition). It exists so graphical interaction can be
/// pixel-responsive while authored geometry stays medium-independent: a maker's
/// pane sits at 10 + 24/48 cells, which is a fact about the CANVAS, not about
/// any monitor.
///
/// WHY FORTY-EIGHT. It is strictly finer than the shipped graphical cell
/// (`kCanvasCellPx` = 12 device pixels, so one sub-unit is a quarter of a
/// pixel), which is what makes every pointer position a medium can distinguish
/// representable; the current pixel lattice embeds EXACTLY (12 divides 48, and
/// so would a 2x or 4x face), so a gesture's fine truth round-trips to the
/// pixel it came from; and it is deliberately NOT the pixel count itself, so no
/// medium's device scale ever becomes authored arrangement truth. A future
/// medium whose cell is not a divisor of 48 still resolves the same lattice —
/// its projection floors at its own grain, at most one device unit of wobble.
///
/// THE ONE QUANTIZATION LAW every consumer applies: a presenter whose device
/// unit is `g` sub-units (a terminal cell: g = kCellSubs; a shipped-skin pixel:
/// g = kCellSubs / kCanvasCellPx = 4) shows the fine span [L, R) on device
/// units [floor(L/g), floor(R/g)). Exact-cell geometry — every remainder zero —
/// therefore lands on exactly the cells and pixels it always did, and a
/// pointer's hit test uses the identical flooring, so what a hand meets is what
/// an eye sees on every medium (see surface/pointing.hpp).
inline constexpr std::int64_t kCellSubs = 48;
static_assert(kCellSubs % kCanvasCellPx == 0,
              "the shipped graphical cell embeds exactly: one pixel is a whole number of "
              "sub-units TODAY (a lattice fact worth noticing when it changes, not a "
              "requirement a future medium must meet)");

/// HOW MUCH ROOM THE ACTIVE SURFACE HAS, in canvas cells — the medium answering
/// the one question a publisher cannot answer for itself.
///
/// Every other shape here travels intent -> medium. This one travels the other
/// way, and it is the only fact that does: a canvas is authored in cells and a
/// medium resolves cells into its own units, so how many cells there is room for
/// is a fact ONLY the medium holds. Before G-2 a publisher had to guess, which in
/// practice meant a constant, which in practice meant a window sized to the
/// picture rather than a picture sized to the window. `Workshop`'s own screen
/// header said so in as many words — "a canvas has no notion of the medium's
/// size, and giving it one is a Surface question, not a Workshop one". This is
/// that Surface question, answered.
///
/// IT IS AN OFFER, NOT AN INSTRUCTION, exactly like every other intent on this
/// bus in the other direction. A publisher that ignores it keeps publishing
/// whatever extent it likes and the Skin clips, which is the contract
/// `SurfaceCanvas` already states. What changes is only that a publisher which
/// WANTS to fill its medium now can.
///
/// PUBLISHED WHEN IT CHANGES, and not otherwise — a Skin's own beat is what
/// notices a person dragging a window edge, so the fact arrives on the pump
/// rather than on an event nobody owns. A medium with no answer (a terminal Skin,
/// or a window that does not exist yet) publishes NOTHING rather than publishing
/// zeroes: "I have no opinion" and "there is no room" are different sentences,
/// and only one of them should move a publisher's screen.
///
/// It names no surface for the same reason `SurfaceCloseRequested` names no
/// window: `kSkinRole` is a singleton, so "the active surface" is a complete
/// address.
///
/// V2 ADDS A SECOND UNIT, AND ONLY FOR TEXT. `text_advance_px` and
/// `text_line_px` are how wide one character is and how far apart two rows are,
/// in the medium's own device pixels, when that medium sets text in a real face
/// rather than in cells. They exist because exactly one party may measure in a
/// sizing conversation (G-2's rule) and for TEXT that party has to be the
/// application: the Terminal pane chooses which transcript entries it can show
/// whole and then SAYS how many it left out, and a medium that wrapped on its
/// own behalf would make that sentence false. So the medium measures its face
/// once and publishes the RESULT; the application does the arithmetic.
///
/// ZERO MEANS "TEXT IS A CELL", and it is the honest answer for most media. A
/// terminal publishes no extent at all, so it never says anything about type; a
/// window says zero before its font is open and after a font has failed to open,
/// and in both of those cases the thing it actually draws is the cell-sized
/// bitmap face — so zero is not a placeholder, it is a description. Both numbers
/// travel together: a medium that answered one and not the other would be
/// describing half a line of text.
///
/// WHAT IS DELIBERATELY NOT HERE: a family, a filename, a point size, an ascent,
/// a descent, a hinting mode, a DPI. An application needs the RESULT of
/// measurement in order to decide how much prose fits; it needs none of the
/// mechanism that produced the result, and every one of those fields would be a
/// fact about one backend that a second backend would have to fake.
struct SurfaceExtent {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::int64_t text_advance_px = 0;
    std::int64_t text_line_px = 0;
    ZEN_SHAPE(SurfaceExtent, 2, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(text_advance_px),
              ZEN_FIELD(text_line_px));
};

/// The active Skin's hello: published exactly once per incarnation, on the
/// first message it handles after claiming its surface (a weave runs only on
/// message — the same lazy-first-wake stance as the v2 world's inheritance
/// claim). Text publishers re-publish their current line when they hear it,
/// so a freshly loaded or swapped-in Skin starts complete instead of waiting
/// for each slot's next natural event. Publishers that never hear it (no Skin
/// loaded) lose nothing: their intent was landing on no one anyway.
struct SurfaceReady {
    ZEN_SHAPE(SurfaceReady, 1);
};

/// Give the active Skin execution time — the PumpInput precedent, pointed at
/// output: a weave runs only when a message arrives, and a Skin whose medium
/// is a real OS window must service that window's event queue even when no
/// intent is flowing (a dead-quiet world starves a frame-driven pump, and an
/// unpumped Windows window is flagged unresponsive — found live: the busy
/// cursor). The day this shape's note promised has arrived: a Skin now
/// arranges its OWN beat (kPumpTimerId below, asked of the Timer package on
/// its own ACTIVATION), so no host owes it laps. PumpSurface stays as the same
/// hands on direct request — for suites, diagnostics, and timer-less hosts.
struct PumpSurface {
    ZEN_SHAPE(PumpSurface, 1);
};

/// THE SURFACE THIS APPLICATION IS BEING SHOWN ON HAS BEEN ASKED TO CLOSE.
///
/// A window manager's close box, or the platform's equivalent. It is a
/// LIFECYCLE fact and emphatically not an input moment, and the distinction is
/// the whole reason it is a shape of its own rather than a synthesized
/// `KeyPressed{Q}`: a maker who binds `q` to quit has authored a policy, and an
/// operating system asking a window to go away has not. Whoever hears this
/// applies its own quit policy — including deciding not to.
///
/// IT NAMES NO WINDOW, and that is deliberate rather than unfinished. There is
/// exactly one application surface in this architecture: `kSkinRole` is a
/// singleton, so "the active surface" is a complete address. A window id would
/// either be SDL's (a backend number leaking into a medium-agnostic
/// vocabulary) or a Zen one (a multi-window identity law written from a
/// one-window witness). Neither is earned. When a second surface exists, the
/// fact this shape is missing will be obvious and it can be added then.
///
/// WHO PUBLISHES IT is a per-medium question with an awkward but honest answer.
/// A terminal has no close box and no terminal Skin ever sends this. SDL
/// reports window lifecycle and input through ONE process-global event queue,
/// so the weave that owns that queue — the SDL Input reader — is the only thing
/// in the process that can see the request. Owning the queue does not make the
/// request input; it makes the reader a router for one fact that is not its
/// own, and the fact is spelled here, in the vocabulary that owns the
/// application's surface, so no consumer has to read it as a key.
struct SurfaceCloseRequested {
    ZEN_SHAPE(SurfaceCloseRequested, 1);
};

/// A MAKER COPIED THIS TEXT — an application's offer of it to the medium's clipboard, and
/// to anything else in this process that holds editable text (TEXT-0).
///
/// IT IS INTENT, THE DIRECTION EVERY OTHER SHAPE HERE TRAVELS: the active Skin executes it
/// with whatever its medium honestly has. The SDL medium sets the real platform clipboard.
/// A terminal medium WRITES the OSC 52 set-clipboard sequence to its own stream — the
/// output stream is the Skin's, exactly as the alternate screen is — which terminals that
/// support it honour and the rest ignore; a terminal offers no way to ask which happened,
/// so nothing here claims the system took it. Either way the text is on the BUS, so every
/// participant that mirrors a clipboard (Workshop, a pane provider with a field) heard the
/// same copy — which is what keeps copy-here-paste-there true inside this process even on a
/// medium whose platform clipboard cannot answer.
///
/// WHY IT IS NOT A SEND TO `kSkinRole`: the Skin is one interested party, not the only one.
/// A publication is the honest shape for a fact several unrelated parties mirror.
struct ClipboardCopy {
    std::string text;
    ZEN_SHAPE(ClipboardCopy, 1, ZEN_FIELD(text));
};

/// WHAT DOES THE PLATFORM CLIPBOARD HOLD RIGHT NOW? — asked of `kSkinRole` because a maker
/// pressed paste, and for no other reason (QR-11).
///
/// CLIPBOARD READ FOLLOWS PASTE INTENT. The system clipboard is ambient host state that may
/// have nothing to do with this application; permission to use its text when a maker asks
/// to paste is not permission to observe it continuously. So there is no standing mirror of
/// the platform's clipboard anywhere in this process, no shape that carries its payload
/// uninvited, and the ONE road foreign clipboard text has onto this bus is the answer to
/// this ask. (TEXT-0's `ClipboardChanged` was that mirror's feed — the SDL reader read the
/// clipboard at startup and on every platform change and published the payload — and QR-11
/// retired it whole.)
///
/// A SEND, NOT A PUBLICATION, and to the Skin's ROLE: the Medium owns the platform surface,
/// so it owns the platform clipboard in BOTH directions — `ClipboardCopy` is the write and
/// this is the read — and the answer goes to the one participant that asked rather than to
/// everyone who might be listening. The asker settles it as any ask is settled: its own
/// book's correlation plus Loom's answer provenance, and applies the text to the draft that
/// requested the paste — or discards it if that draft is gone (`loom::AskBook`).
struct ClipboardTextRequested {
    ZEN_SHAPE(ClipboardTextRequested, 1);
};

/// THE MEDIUM'S ANSWER: what its platform clipboard holds at this moment, or the honest
/// admission that it cannot say (QR-11).
///
/// `readable` is the medium's REACH, and it is a separate field from an empty `text`
/// because they are different sentences a paste must not confuse: the SDL medium answers
/// `readable=true` with the clipboard's current bytes — empty meaning the platform holds no
/// text, which a paste honours by inserting nothing — while a terminal medium answers
/// `readable=false`, because no truthful terminal route reads a system clipboard (the
/// OSC 52 query is disabled almost everywhere, for exactly the reason this shape exists).
/// On an unreadable medium the asker falls back to what this process itself last copied,
/// which is the strongest truthful paste a terminal has. Folding the two into one field
/// would make an EMPTY platform clipboard paste stale mirror text the platform no longer
/// holds.
struct ClipboardText {
    bool readable = false;
    std::string text;
    ZEN_SHAPE(ClipboardText, 1, ZEN_FIELD(readable), ZEN_FIELD(text));
};

/// The role that IS surface ownership. Singleton by the Loom's role rules, so
/// "exactly one active Skin owns the primary surface" is enforced ground, not
/// convention. Address the Skin by role, never by id — the successor after a
/// swap is a different weave; only the role carries intent across.
inline constexpr const char* kSkinRole = "zengine.skin";

/// The two slots V1 publishers actually speak. Nothing reserves them — a slot
/// name is a convention between publisher and Skin, exactly like a schema
/// name — but spelling them once keeps the two sides from drifting.
inline constexpr const char* kSlotStatus = "status";
inline constexpr const char* kSlotScore = "score";

/// The Skin's heartbeat, asked of the Timer package on its own ACTIVATION —
/// and asked AGAIN on TimerReady, because a skin loaded before any timer
/// service exists sends an ask that goes nowhere (rejected at the library/
/// schema seam — see TimerReady in timer/vocabulary.hpp) and must be able to
/// retry: announcing is ONCE, asking is REPEATABLE, and they are deliberately
/// not the same call any more. A repeating role-addressed timer. Role-addressed
/// is the load-bearing half —
/// the beat belongs to kSkinRole, so a swapped-in successor inherits it
/// without asking (on a dead-quiet bus a fresh window-owning skin would
/// otherwise never get its queue serviced — the exact wedge the old
/// host-sent PumpSurface existed to prevent, dead by construction for a SKIN
/// swap). Terminal media no-op the firing, exactly as they no-op'd the pump.
///
/// The succession that holds here is the standing timer's, across holders of
/// kSkinRole. It is not the Timer service's own: swapping `zengine.timer`
/// ends every beat in the system, this one with it, and today nothing
/// re-lights them — the window would go unserviced again for the same reason
/// the world would stop. See Drive in timer/vocabulary.hpp.
inline constexpr const char* kPumpTimerId = "zengine.skin.pump";
inline constexpr std::int64_t kPumpBeatMs = 10;

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_VOCABULARY_HPP
