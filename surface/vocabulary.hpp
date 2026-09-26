// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_VOCABULARY_HPP
#define ZENGINE_SURFACE_VOCABULARY_HPP

// The Surface package's message vocabulary: a weave publishes visual intent, and the Skin -- the
// replaceable weave holding the singleton `zengine.skin` role -- claims the medium and paints it.
// A Skin claims its medium in its constructor and releases it in its destructor; loading a
// second Skin into the held role is refused. Reference: docs/reference/surface.md.

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::surface {

/// One line of plain text for a named slot ("status", "score"): no escape codes and no markup,
/// because how a slot looks is the Skin's business. A slot the Skin does not know is dropped.
struct SurfaceText {
    std::string slot;
    std::string text;
    ZEN_SHAPE(SurfaceText, 1, ZEN_FIELD(slot), ZEN_FIELD(text));
};

/// The visual role of a canvas element: semantic, never a colour -- each Skin picks the ink, so one
/// canvas reads correctly in a monochrome terminal and in a window. A role the active Skin does
/// not know paints as `kFill` rather than vanishing.
namespace role {
inline constexpr std::int64_t kFill = 0;   ///< ordinary authored material
inline constexpr std::int64_t kAccent = 1; ///< the one thing being pointed at
inline constexpr std::int64_t kMuted = 2;  ///< present, deliberately quiet
inline constexpr std::int64_t kAlert = 3;  ///< something the maker must see
inline constexpr std::int64_t kGround = 4; ///< opaque, empty material beneath content

/// No role at all: the absence of a background, not an ink role. Negative, so it cannot collide
/// with a role a later vocabulary adds (which an older Skin would silently paint as `kFill`).
/// Never pass it to a Skin's role-to-ink table; test for it first.
inline constexpr std::int64_t kNone = -1;
} // namespace role

/// One filled rectangle, in canvas cells: a character column in a terminal, `kCanvasCellPx`
/// pixels in the shipped window. `sub_*` refine a coordinate on the same lattice (`kCellSubs`);
/// zero is the whole-cell picture. A layer's rects paint in list order, so a publisher puts a
/// rect behind another by publishing it earlier; there is no z field.
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
    ZEN_SHAPE(SurfaceRect, 2, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(role), ZEN_FIELD(sub_x), ZEN_FIELD(sub_y), ZEN_FIELD(sub_w),
              ZEN_FIELD(sub_h));
};

/// One run of plain text anchored at a canvas cell, drawn over its layer's rects: one cell per
/// byte in every medium. A Skin says which bytes it has a glyph for and draws something visible
/// for the rest; no Skin drops a character silently.
struct SurfaceLabel {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::string text;
    std::int64_t role = role::kFill;
    std::int64_t sub_x = 0; ///< sub-cell remainders of the anchor; a label has no fine extent
    std::int64_t sub_y = 0;
    ZEN_SHAPE(SurfaceLabel, 2, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text), ZEN_FIELD(role),
              ZEN_FIELD(sub_x), ZEN_FIELD(sub_y));
};

/// One row of prose in a bounded text region: plain text, a role, and a background to set it on.
/// A row's place is its index in its region's list. `background` is a role, not a colour, and
/// `role::kNone` (the default) shows whatever the region sits on. Colour alone is not enough on
/// a monochrome terminal, so a row marked as selected also says so in its text. Plain ASCII: the
/// cell projection is one cell per byte and would split a multi-byte sequence.
struct SurfaceTextRow {
    std::string text;
    std::int64_t role = role::kFill;
    std::int64_t background = role::kNone;
    ZEN_SHAPE(SurfaceTextRow, 2, ZEN_FIELD(text), ZEN_FIELD(role), ZEN_FIELD(background));
};

/// A region with no caret. Negative, as `role::kNone` is, so it cannot collide with a row index.
inline constexpr std::int64_t kNoCaret = -1;

/// Whose rectangle a region is. `kGroundOwn` (the default): the region clears its whole bounds
/// before a row is drawn, so nothing shows beneath it. `kGroundBeneath`: the region writes on
/// material published beneath it -- the same bounds and fit, with no fill and no padding. It is
/// not a row's `background`, and not transparency: no blend, no opacity, no order of its own. A
/// medium tests for `kGroundBeneath` exactly and reads any other value as `kGroundOwn`.
inline constexpr std::int64_t kGroundOwn = 0;
inline constexpr std::int64_t kGroundBeneath = 1;

/// The caret in a medium whose character is a cell: this glyph, inserted at the caret's column.
inline constexpr char kCaretGlyph = '_';

/// A region with no selection; negative for `kNoCaret`'s reason.
inline constexpr std::int64_t kNoSelection = -1;

/// A bounded region of prose, placed in canvas cells like a rect and filled with rows the medium
/// sets in its own text metric: a terminal draws one row per cell row, cut at `w` and dropped
/// past `h`; a window with a real face draws at its own advance and line height, inside the
/// rectangle the bounds resolve to. How many rows and columns fit is not on this shape: the
/// publisher asks `fit_region` (surface/region.hpp), the function the medium itself uses, and
/// sends what fits. A row longer than the region is the medium's to cut.
///
/// `caret_*` and `sel_*` are positions in the region's own prose lattice -- a row and a column
/// into `rows` -- never pixels or cells. A selection runs from begin (inclusive) to end
/// (exclusive) in reading order; `selection_span_of_row` is its per-row arithmetic, and a range
/// that is absent, empty or out of order shows nothing. It is one range meaning selection: not
/// styling, not focus, not several selections.
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
    std::int64_t sub_x = 0; ///< sub-cell remainders of the bounds; the prose lattice
    std::int64_t sub_y = 0; ///< (rows, columns, caret, selection) is untouched by them
    std::int64_t sub_w = 0;
    std::int64_t sub_h = 0;
    ZEN_SHAPE(SurfaceTextRegion, 6, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(rows), ZEN_FIELD(caret_row), ZEN_FIELD(caret_col), ZEN_FIELD(ground),
              ZEN_FIELD(sel_begin_row), ZEN_FIELD(sel_begin_col), ZEN_FIELD(sel_end_row),
              ZEN_FIELD(sel_end_col), ZEN_FIELD(sub_x), ZEN_FIELD(sub_y), ZEN_FIELD(sub_w),
              ZEN_FIELD(sub_h));
};

/// One painter's plane: its rects in list order, then its labels, then its text regions, drawn
/// as a complete picture before the next plane goes over it. A layer is a position in the
/// canvas's list and nothing more -- no transform, opacity, clipping, identity or z -- and a
/// nested value, never a message. It is not a compositor and must not become one.
struct SurfaceLayer {
    std::vector<SurfaceRect> rects;
    std::vector<SurfaceLabel> labels;
    std::vector<SurfaceTextRegion> texts;
    ZEN_SHAPE(SurfaceLayer, 4, ZEN_FIELD(rects), ZEN_FIELD(labels), ZEN_FIELD(texts));
};

/// A whole picture: an extent in whole cells and its planes, `layers[0]` back-most. A drawing,
/// not a layout: no parent/child, anchors or percentages -- whoever publishes has decided where
/// things go. Elements outside the extent are the Skin's to clip. A canvas with no layers is a
/// picture of nothing, and clears the previous one.
struct SurfaceCanvas {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::vector<SurfaceLayer> layers;
    ZEN_SHAPE(SurfaceCanvas, 8, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(layers));
};

/// One canvas cell in the shipped graphical Skin, in pixels. Another medium's cell may differ: a
/// consumer spells geometry with the size its medium reports (`SurfaceExtent::cell_px`).
inline constexpr std::int64_t kCanvasCellPx = 12;

/// Sub-cell units per canvas cell: the lattice's resolution. A geometry shape's coordinate is
/// whole cells plus a `sub_*` remainder in [0, kCellSubs); a remainder outside it reads as zero.
/// A medium whose device unit is `g` sub-units shows the span [L, R) on units
/// [floor(L/g), floor(R/g)), and a pointer's hit test floors the same way (surface/pointing.hpp).
/// Forty-eight is finer than the shipped Skin's pixel (four sub-units) and is no medium's own
/// scale, so authored geometry stays medium-independent.
inline constexpr std::int64_t kCellSubs = 48;
static_assert(kCellSubs % kCanvasCellPx == 0,
              "the shipped graphical cell embeds exactly: one pixel is a whole number of "
              "sub-units TODAY (a lattice fact worth noticing when it changes, not a "
              "requirement a future medium must meet)");

/// How much room the active surface has, in canvas cells: a fact only the medium holds, offered
/// to publishers (one that ignores it keeps its own extent, and the Skin clips). Published when
/// it changes; a medium with no answer publishes nothing rather than zeroes.
///
/// `text_advance_px` and `text_line_px` are one character's advance and the row pitch, in the
/// medium's device pixels, when it sets real type; zero means text is a cell. The medium
/// measures, the publisher fits: one measurer, so a publisher's "12 more" stays true. `cell_px`
/// is the medium's device pixels per canvas cell; zero means its device unit is the cell. It is
/// not the text metric: a window whose font failed publishes `{w, h, 0, 0, kCanvasCellPx}`.
/// No font family, size or DPI: the result of measuring, never its mechanism.
struct SurfaceExtent {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::int64_t text_advance_px = 0;
    std::int64_t text_line_px = 0;
    std::int64_t cell_px = 0;
    ZEN_SHAPE(SurfaceExtent, 3, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(text_advance_px),
              ZEN_FIELD(text_line_px), ZEN_FIELD(cell_px));
};

/// The active Skin's hello, published once per incarnation on its first message. Text
/// publishers re-publish their current line when they hear it, so a new Skin starts complete.
struct SurfaceReady {
    ZEN_SHAPE(SurfaceReady, 1);
};

/// Give the active Skin a turn to service its medium, such as a window's event queue. A Skin
/// keeps its own beat (`kPumpTimerId`); this is the same work on request, for suites and for
/// hosts with no Timer.
struct PumpSurface {
    ZEN_SHAPE(PumpSurface, 1);
};

/// The surface was asked to close, as by a window's close box: a lifecycle fact, not an input
/// moment, so it is never confused with a maker's quit key. Whoever hears it applies its own quit
/// policy, which may be to stay. It names no window: `kSkinRole` is a singleton. No terminal Skin
/// sends it; under SDL the Input reader publishes it, because it owns the one event queue.
struct SurfaceCloseRequested {
    ZEN_SHAPE(SurfaceCloseRequested, 1);
};

/// Where the active surface's window sits on its desktop, in the platform's desktop units. A
/// publisher treats `x`/`y` as opaque: it may remember them and offer them back
/// (`SurfacePlacementRemembered`), never interpret them. They are always the normal
/// (unmaximized) window's top-left, remembered across a maximized stretch; `maximized` is the
/// current state. Published when it changes, before the extent when both change. A medium with
/// no desktop window (every terminal) publishes nothing: `(0,0)` is a real place.
struct SurfacePlacement {
    std::int64_t x = 0;
    std::int64_t y = 0;
    bool maximized = false;
    ZEN_SHAPE(SurfacePlacement, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(maximized));
};

/// A remembered placement, offered back to `kSkinRole` once by a publisher restoring a session:
/// a want, not an instruction. The medium judges it against the displays that exist now: a
/// reachable position is restored as sent, a stranded one is moved onto the nearest display,
/// and with no display truth nothing moves (docs/reference/surface.md). What comes back is the
/// next `SurfacePlacement`, never an echo; a terminal medium does nothing.
struct SurfacePlacementRemembered {
    std::int64_t x = 0;
    std::int64_t y = 0;
    bool maximized = false;
    ZEN_SHAPE(SurfacePlacementRemembered, 1, ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(maximized));
};

/// A maker copied this text. A publication, because several parties hear it: the Skin sets the
/// platform clipboard as far as its medium can (on a terminal, OSC 52, with no claim that it
/// took), and every participant that mirrors a clipboard keeps copy-and-paste true in-process.
struct ClipboardCopy {
    std::string text;
    ZEN_SHAPE(ClipboardCopy, 1, ZEN_FIELD(text));
};

/// What does the platform clipboard hold now? Sent to `kSkinRole` because a maker pressed paste,
/// and for no other reason: nothing observes the system clipboard, and this ask's answer is the
/// one road its text has onto the bus. The asker settles the answer as any ask
/// (`loom::AskBook`) and applies it to the draft that asked, or discards it.
struct ClipboardTextRequested {
    ZEN_SHAPE(ClipboardTextRequested, 1);
};

/// The medium's answer. `readable=false` (every terminal) means it cannot say, and the asker
/// pastes what this process last copied; `readable=true` with empty `text` means the platform
/// holds no text, and the paste inserts nothing. Two fields, so an empty platform clipboard never
/// pastes a stale copy.
struct ClipboardText {
    bool readable = false;
    std::string text;
    ZEN_SHAPE(ClipboardText, 1, ZEN_FIELD(readable), ZEN_FIELD(text));
};

// ---- A capture: what the active surface presented, through its own medium ---------------
//
// A picture is evidence of presentation at one frame and of nothing else: it proves nothing
// about queued work. `after_frame` defers the answer until a later frame is painted -- anyone's
// paint -- which is not the frame that shows a given input: ordering after input is the
// injector's host's (a fence, or a link ask's `settle`). One picture is retained at a time and
// fetched by chunk, one request may wait for its frame at a time, and a picture over
// `kMaxCaptureBytes` is refused, never cut.

/// TAKE A PICTURE, now or once a later frame has been painted. Answered `SurfaceCaptured`.
struct SurfaceCaptureRequested {
    /// Capture when `frames > after_frame`. -1 (the default) means now, whatever frame stands;
    /// a value at or beyond the current count defers the answer to the paint that passes it.
    std::int64_t after_frame = -1;
    ZEN_SHAPE(SurfaceCaptureRequested, 1, ZEN_FIELD(after_frame));
};

/// THE PICTURE'S IDENTITY AND SHAPE -- not its bytes, which are fetched by chunk. `format` is
/// the medium's honest word: `image/bmp` for a window (24-bit, bottom-up, as a BMP is), or
/// `text/cells` for a terminal (the cell projection, one row per line, `height` rows of `width`
/// bytes plus a newline). `cell_px` is the graphical medium's cell size, so a pixel coordinate
/// in the picture maps to the canvas lattice a pointer moment is spelled in; 0 on a terminal,
/// where a cell IS the unit.
struct SurfaceCaptured {
    bool ok = false;
    std::int64_t capture = 0; ///< this picture's number; the chunk door names it
    std::int64_t frame = 0;   ///< the Skin's frame count when it was taken
    std::int64_t width = 0;   ///< pixels, or cells on a terminal
    std::int64_t height = 0;
    std::int64_t cell_px = 0;
    std::string format;
    std::int64_t bytes = 0; ///< the whole picture's size, in bytes
    std::string refusal;    ///< why not, when `ok` is false
    ZEN_SHAPE(SurfaceCaptured, 1, ZEN_FIELD(ok), ZEN_FIELD(capture), ZEN_FIELD(frame),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(cell_px), ZEN_FIELD(format),
              ZEN_FIELD(bytes), ZEN_FIELD(refusal));
};

/// FETCH ONE CHUNK of the retained picture, from `offset`. Answered `SurfaceCaptureChunk`, or
/// `zen.Refused` when `capture` is no longer the one retained.
struct SurfaceCaptureChunkRequested {
    std::int64_t capture = 0;
    std::int64_t offset = 0;
    ZEN_SHAPE(SurfaceCaptureChunkRequested, 1, ZEN_FIELD(capture), ZEN_FIELD(offset));
};

struct SurfaceCaptureChunk {
    std::int64_t capture = 0;
    std::int64_t offset = 0;
    std::int64_t total = 0; ///< the whole picture's size; `offset + data.size() == total` on the last
    loom::Bytes data;
    ZEN_SHAPE(SurfaceCaptureChunk, 1, ZEN_FIELD(capture), ZEN_FIELD(offset), ZEN_FIELD(total),
              ZEN_FIELD(data));
};

/// The most bytes one chunk carries: small enough for any crossing's frame and any bus's
/// decode budget, large enough that a screen is a few hundred round trips and not thousands.
inline constexpr std::int64_t kCaptureChunkBytes = 32 * 1024;
/// The largest picture a Skin retains. A 4K window at 24 bits is under this; a larger one is
/// refused with its size, never silently cut.
inline constexpr std::int64_t kMaxCaptureBytes = 32 * 1024 * 1024;

/// WHAT A MEDIUM HANDS THE SHELL WHEN ASKED FOR ITS PICTURE. Plain bytes and their shape; the
/// shell numbers and retains it.
struct CapturedPicture {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::int64_t cell_px = 0;
    std::string format;
    std::string bytes;
};

/// The role that is surface ownership: a singleton, so exactly one Skin is active. Address the
/// Skin by role, never by id: a swap's successor is a different weave.
inline constexpr const char* kSkinRole = "zengine.skin";

/// The two slots publishers speak. A slot name is a convention between publisher and Skin, and
/// spelling them once keeps the two sides from drifting.
inline constexpr const char* kSlotStatus = "status";
inline constexpr const char* kSlotScore = "score";

/// The Skin's beat: a repeating timer addressed to `kSkinRole`, asked of the Timer on the Skin's
/// activation and again on `TimerReady` (an ask sent before a Timer exists goes nowhere). It
/// belongs to the role, so a swapped-in Skin inherits it.
inline constexpr const char* kPumpTimerId = "zengine.skin.pump";
inline constexpr std::int64_t kPumpBeatMs = 10;

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_VOCABULARY_HPP
