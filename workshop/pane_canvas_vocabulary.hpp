// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_CANVAS_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_CANVAS_VOCABULARY_HPP

#include "surface/vocabulary.hpp"
#include <zen/weave/shape.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

// Optional pane-local graphics. The host owns placement, clipping and pointer custody;
// the provider owns hit testing and the meaning of its picture. No screen coordinates cross.
inline constexpr std::int64_t kPaneCanvasUnit = surface::kCellSubs;
inline constexpr std::size_t kPaneCanvasMaxRects = 4096;
inline constexpr std::size_t kPaneCanvasMaxLabels = 2048;
inline constexpr std::size_t kPaneCanvasMaxLabelBytes = 4096;
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

// Width, height and grain are subunits (48 per canvas cell). A positive grant is a capability
// for this presentation and this exact provider incarnation. Zero extent revokes its room.
struct PaneCanvasRoom {
    std::string pane;
    std::int64_t grant = 0, width = 0, height = 0, grain = kPaneCanvasUnit;
    bool graphical = false;
    ZEN_SHAPE(PaneCanvasRoom, 1, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(width),
              ZEN_FIELD(height), ZEN_FIELD(grain), ZEN_FIELD(graphical));
};

// A whole replacement picture, rects first and labels above. Positive picture numbers strictly
// increase within a grant. Offscreen coordinates are legal; invalid or over-budget pictures
// are rejected whole and the last good picture stays visible. There is no implicit scenegraph.
struct PaneCanvasContent {
    std::string pane;
    std::int64_t grant = 0, picture = 0;
    std::vector<PaneCanvasRect> rects;
    std::vector<PaneCanvasLabel> labels;
    ZEN_SHAPE(PaneCanvasContent, 1, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(picture),
              ZEN_FIELD(rects), ZEN_FIELD(labels));
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
// x/y are local subunits and may leave the room during a drag. Lost ends custody; its position
// is the last position reported. A new room ends old custody. Button is 1/2/3; wheel uses 0.
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

} // namespace zengine::workshop
#endif
