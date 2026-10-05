// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_CANVAS_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_CANVAS_VOCABULARY_HPP

#include "surface/region.hpp"
#include "surface/vocabulary.hpp"
#include <zen/weave/shape.hpp>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

// Optional pane-local graphics, in local canvas pixels. The host owns placement, clipping and
// pointer custody; the provider owns hit testing and the meaning of its picture. No screen
// coordinates cross. `kPaneCanvasUnit` is one canvas cell: a label byte's advance, and the grain
// of a medium whose device unit is the cell.
inline constexpr std::int64_t kPaneCanvasUnit = surface::kCanvasCellPx;
inline constexpr std::size_t kPaneCanvasMaxRects = 4096;
inline constexpr std::size_t kPaneCanvasMaxLabels = 2048;
inline constexpr std::size_t kPaneCanvasMaxLabelBytes = 4096;
inline constexpr std::size_t kPaneCanvasMaxTexts = 2048;
inline constexpr std::size_t kPaneCanvasMaxTextRunBytes = 4096;
inline constexpr std::size_t kPaneCanvasMaxTextBytes = 131072;

struct PaneCanvasRect {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t role = surface::role::kFill;
    ZEN_SHAPE(PaneCanvasRect, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(role));
};

// One canvas cell per printable ASCII byte, unscaled. Offscreen glyphs are omitted whole.
struct PaneCanvasLabel {
    std::int64_t x = 0, y = 0;
    std::string text;
    std::int64_t role = surface::role::kFill;
    ZEN_SHAPE(PaneCanvasLabel, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text), ZEN_FIELD(role));
};

// One line in the medium's measured prose face. x/y are the local region origin, before
// its inset. Caret and selection are source-byte columns (ASCII); selection is [begin,end).
// Whole glyphs/rows are omitted at the room edge, without moving the surviving glyphs.
struct PaneCanvasText {
    std::int64_t x = 0, y = 0;
    std::string text;
    std::int64_t role = surface::role::kFill;
    std::int64_t caret_col = surface::kNoCaret;
    std::int64_t sel_begin_col = surface::kNoSelection, sel_end_col = surface::kNoSelection;
    ZEN_SHAPE(PaneCanvasText, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(text), ZEN_FIELD(role),
              ZEN_FIELD(caret_col), ZEN_FIELD(sel_begin_col), ZEN_FIELD(sel_end_col));
};

// Width, height and grain are canvas pixels: the grain is one device unit of the medium in front
// (one pixel in a window, `kPaneCanvasUnit` in a terminal). A positive grant is a capability for
// this presentation and this exact provider incarnation. Zero extent revokes its room.
struct PaneCanvasRoom {
    std::string pane;
    std::int64_t grant = 0, width = 0, height = 0, grain = kPaneCanvasUnit;
    bool graphical = false;
    std::int64_t text_advance_px = 0, text_line_px = 0;
    ZEN_SHAPE(PaneCanvasRoom, 3, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(width),
              ZEN_FIELD(height), ZEN_FIELD(grain), ZEN_FIELD(graphical),
              ZEN_FIELD(text_advance_px), ZEN_FIELD(text_line_px));
};

// A whole replacement picture, rects first and labels above. Positive picture numbers strictly
// increase within a grant. Offscreen coordinates are legal; invalid or over-budget pictures
// are rejected whole and the last good picture stays visible. There is no implicit scenegraph.
struct PaneCanvasContent {
    std::string pane;
    std::int64_t grant = 0, picture = 0;
    std::vector<PaneCanvasRect> rects;
    std::vector<PaneCanvasLabel> labels;
    std::vector<PaneCanvasText> texts = {};
    ZEN_SHAPE(PaneCanvasContent, 3, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(picture),
              ZEN_FIELD(rects), ZEN_FIELD(labels), ZEN_FIELD(texts));
};

struct PaneCanvasRejected {
    std::string pane;
    std::int64_t grant = 0, picture = 0;
    std::string reason;
    ZEN_SHAPE(PaneCanvasRejected, 1, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(picture),
              ZEN_FIELD(reason));
};

namespace canvas_pointer {
inline constexpr std::int64_t kPress = 1, kMove = 2, kRelease = 3, kLost = 4, kWheel = 5;
}

// Press and wheel name the fenced picture actually shown. A held gesture keeps its press's
// picture, grant and gesture number through motion and release/loss, even as the pane repaints.
// x/y are local canvas pixels and may leave the room during a drag. Lost ends custody; its position
// is the last position reported. A new room ends old custody. Button is 1/2/3; wheel uses 0.
struct PaneCanvasPointer {
    std::string pane;
    std::int64_t grant = 0, picture = 0, gesture = 0;
    std::int64_t phase = canvas_pointer::kPress, button = 0, x = 0, y = 0, modifiers = 0;
    double dx = 0, dy = 0;
    bool keys_went_here = false;
    ZEN_SHAPE(PaneCanvasPointer, 2, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(picture),
              ZEN_FIELD(gesture), ZEN_FIELD(phase), ZEN_FIELD(button), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(modifiers), ZEN_FIELD(dx), ZEN_FIELD(dy),
              ZEN_FIELD(keys_went_here));
};

// Where the pointer rests over a canvas, for a provider that accepts it: the picture handed to
// the medium and a local place in canvas pixels, or `over` false once the pointer left the room
// it last named.
// `carrying` says a carried value is over it. Presentation only: no gesture, focus or key moves.
struct PaneCanvasHover {
    std::string pane;
    std::int64_t grant = 0, picture = 0, x = 0, y = 0;
    bool over = true, carrying = false;
    ZEN_SHAPE(PaneCanvasHover, 2, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(picture),
              ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(over), ZEN_FIELD(carrying));
};

// ---- The canvas in sub-units, as a provider built before the pixel door still speaks it ------
//
// The same doors at their earlier versions, whose numbers are sub-units: 48 to the canvas cell,
// four to the canvas pixel. Workshop meets a provider whose holder accepts only these by granting
// its room and sending its pointer and hover times `kPaneCanvasLegacySubs`, and by reading its
// picture at the floor of that, the floor the window always painted it at.

/// Sub-units per canvas pixel on the earlier canvas doors.
inline constexpr std::int64_t kPaneCanvasLegacySubs = 4;

namespace v2 {

struct PaneCanvasRoom {
    std::string pane;
    std::int64_t grant = 0, width = 0, height = 0, grain = 0;
    bool graphical = false;
    std::int64_t text_advance_px = 0, text_line_px = 0;
    ZEN_SHAPE(PaneCanvasRoom, 2, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(width),
              ZEN_FIELD(height), ZEN_FIELD(grain), ZEN_FIELD(graphical),
              ZEN_FIELD(text_advance_px), ZEN_FIELD(text_line_px));
};

struct PaneCanvasContent {
    std::string pane;
    std::int64_t grant = 0, picture = 0;
    std::vector<PaneCanvasRect> rects;
    std::vector<PaneCanvasLabel> labels;
    std::vector<PaneCanvasText> texts = {};
    ZEN_SHAPE(PaneCanvasContent, 2, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(picture),
              ZEN_FIELD(rects), ZEN_FIELD(labels), ZEN_FIELD(texts));
};

} // namespace v2

namespace v1 {

struct PaneCanvasPointer {
    std::string pane;
    std::int64_t grant = 0, picture = 0, gesture = 0;
    std::int64_t phase = canvas_pointer::kPress, button = 0, x = 0, y = 0, modifiers = 0;
    double dx = 0, dy = 0;
    bool keys_went_here = false;
    ZEN_SHAPE(PaneCanvasPointer, 1, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(picture),
              ZEN_FIELD(gesture), ZEN_FIELD(phase), ZEN_FIELD(button), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(modifiers), ZEN_FIELD(dx), ZEN_FIELD(dy),
              ZEN_FIELD(keys_went_here));
};

struct PaneCanvasHover {
    std::string pane;
    std::int64_t grant = 0, picture = 0, x = 0, y = 0;
    bool over = true, carrying = false;
    ZEN_SHAPE(PaneCanvasHover, 1, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(picture),
              ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(over), ZEN_FIELD(carrying));
};

} // namespace v1

/// A sub-unit coordinate of an earlier picture as a canvas pixel: floored, as the window drew it.
inline constexpr std::int64_t px_of_legacy_subs(std::int64_t subs) noexcept {
    return surface::floor_div_px(subs, kPaneCanvasLegacySubs);
}

/// A canvas pixel as an earlier door's sub-units: exact.
inline constexpr std::int64_t legacy_subs_of_px(std::int64_t px) noexcept {
    return surface::mul_px(px < 0 ? 0 : px, kPaneCanvasLegacySubs) -
           surface::mul_px(px < 0 ? -px : 0, kPaneCanvasLegacySubs);
}

/// An earlier picture in canvas pixels: every edge floored, so a rect keeps the pixels it was
/// painted on and an anchor the pixel its glyph started at.
inline PaneCanvasContent canvas_content_of_legacy(const v2::PaneCanvasContent& old) {
    PaneCanvasContent out;
    out.pane = old.pane;
    out.grant = old.grant;
    out.picture = old.picture;
    out.rects.reserve(old.rects.size());
    for (const PaneCanvasRect& r : old.rects) {
        const std::int64_t x = px_of_legacy_subs(r.x);
        const std::int64_t y = px_of_legacy_subs(r.y);
        const std::int64_t w =
            r.w > 0 ? px_of_legacy_subs(surface::add_cells(r.x, r.w)) - x : r.w;
        const std::int64_t h =
            r.h > 0 ? px_of_legacy_subs(surface::add_cells(r.y, r.h)) - y : r.h;
        out.rects.push_back(PaneCanvasRect{x, y, w, h, r.role});
    }
    out.labels.reserve(old.labels.size());
    for (const PaneCanvasLabel& l : old.labels) {
        out.labels.push_back(PaneCanvasLabel{px_of_legacy_subs(l.x), px_of_legacy_subs(l.y),
                                             l.text, l.role});
    }
    out.texts.reserve(old.texts.size());
    for (PaneCanvasText t : old.texts) {
        t.x = px_of_legacy_subs(t.x);
        t.y = px_of_legacy_subs(t.y);
        out.texts.push_back(std::move(t));
    }
    return out;
}

} // namespace zengine::workshop
#endif
