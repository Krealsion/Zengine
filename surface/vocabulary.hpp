// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SURFACE_VOCABULARY_HPP
#define ZENGINE_SURFACE_VOCABULARY_HPP

// The Surface package's message vocabulary: a weave publishes visual intent, and the Skin -- the
// replaceable weave holding the singleton `zengine.skin` role -- claims the medium and paints it.
// A Skin claims its medium in its constructor and releases it in its destructor; loading a
// second Skin into the held role is refused. Reference: surface/docs/surface.md; the Skin:
// surface/docs/zengine.skin.md.

#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"

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

/// One filled rectangle, in canvas pixels: one device pixel in the shipped window, and a twelfth
/// of a character cell in a terminal, whose Skin floors each edge to its cells (`kCellPx`).
/// A layer's rects paint in list order, so a publisher puts a rect behind another by publishing
/// it earlier; there is no z field.
struct SurfaceRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::int64_t role = graphics::role::kFill;
    ZEN_SHAPE(SurfaceRect, 3, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(role));
};

/// One run of plain text anchored at a canvas pixel, drawn over its layer's rects: one canvas
/// cell (`kCellPx` pixels) per byte in every medium. A Skin says which bytes it has a glyph
/// for and draws something visible for the rest; no Skin drops a character silently.
struct SurfaceLabel {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::string text;
    std::int64_t role = graphics::role::kFill;
    ZEN_SHAPE(SurfaceLabel, 3, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text), ZEN_FIELD(role));
};

/// One row of prose in a bounded text region: plain text, a role, and a background to set it on.
/// A row's place is its index in its region's list. `background` is a role, not a colour, and
/// `role::kNone` (the default) shows whatever the region sits on. Colour alone is not enough on
/// a monochrome terminal, so a row marked as selected also says so in its text. Plain ASCII: the
/// cell projection is one cell per byte and would split a multi-byte sequence.
struct SurfaceTextRow {
    std::string text;
    std::int64_t role = graphics::role::kFill;
    std::int64_t background = graphics::role::kNone;
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

/// A region with no selection; negative for `kNoCaret`'s reason.
inline constexpr std::int64_t kNoSelection = -1;

/// A bounded region of prose, placed in canvas pixels like a rect and filled with rows the medium
/// sets in its own text metric: a terminal draws one row per cell row of the cells the bounds
/// cover, cut at their width and dropped past their height; a window with a real face draws at
/// its own advance and line height, inside the bounds. How many rows and columns fit is not on this shape: the
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
    ZEN_SHAPE(SurfaceTextRegion, 7, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(rows), ZEN_FIELD(caret_row), ZEN_FIELD(caret_col), ZEN_FIELD(ground),
              ZEN_FIELD(sel_begin_row), ZEN_FIELD(sel_begin_col), ZEN_FIELD(sel_end_row),
              ZEN_FIELD(sel_end_col));
};

/// One painter's plane: its rects in list order, then its labels, then its text regions, drawn
/// as a complete picture before the next plane goes over it. A layer is a position in the
/// canvas's list and nothing more -- no transform, opacity, clipping, identity or z -- and a
/// nested value, never a message. It is not a compositor and must not become one.
struct SurfaceLayer {
    std::vector<SurfaceRect> rects;
    std::vector<SurfaceLabel> labels;
    std::vector<SurfaceTextRegion> texts;
    ZEN_SHAPE(SurfaceLayer, 5, ZEN_FIELD(rects), ZEN_FIELD(labels), ZEN_FIELD(texts));
};

/// A whole picture: an extent in canvas pixels and its planes, `layers[0]` back-most. A drawing,
/// not a layout: no parent/child, anchors or percentages -- whoever publishes has decided where
/// things go. Elements outside the extent are the Skin's to clip. A canvas with no layers is a
/// picture of nothing, and clears the previous one.
struct SurfaceCanvas {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::vector<SurfaceLayer> layers;
    ZEN_SHAPE(SurfaceCanvas, 9, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(layers));
};

/// How much room the active surface has, in canvas pixels: a fact only the medium holds, offered
/// to publishers (one that ignores it keeps its own extent, and the Skin clips). A terminal's
/// room is its cells times `kCellPx`. Published when it changes; a medium with no answer
/// publishes nothing rather than zeroes.
///
/// `text_advance_px` and `text_line_px` are one character's advance and the row pitch, in the
/// medium's device pixels, when it sets real type; zero means text is a cell. The medium
/// measures, the publisher fits: one measurer, so a publisher's "12 more" stays true. `cell_px`
/// is the medium's device pixels per canvas cell; zero means its device unit is the cell. It is
/// not the text metric: a window whose font failed publishes `{w, h, 0, 0, kCellPx}`.
/// No font family, size or DPI: the result of measuring, never its mechanism.
struct SurfaceExtent {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::int64_t text_advance_px = 0;
    std::int64_t text_line_px = 0;
    std::int64_t cell_px = 0;
    ZEN_SHAPE(SurfaceExtent, 4, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(text_advance_px),
              ZEN_FIELD(text_line_px), ZEN_FIELD(cell_px));
};

/// The active Skin's hello, published once per incarnation on its first message. Text
/// publishers re-publish their current line when they hear it, so a new Skin starts complete.
struct SurfaceReady {
    ZEN_SHAPE(SurfaceReady, 1);
};

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
