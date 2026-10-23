// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_PAINTING_HPP
#define ZENGINE_GRAPHICS_PAINTING_HPP

// What crosses the two seams a window calls in its own handler -- the painter's and the element's
// -- as Loom canonical bytes, never as messages: a node to paint and the cells it painted; an event
// for an element's code and what came of it. Reference: graphics/docs/graphics.md.

#include "graphics/vocabulary.hpp"

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::graphics {

/// A node as a painter needs it: its place in the area's paint order (its ancestors' ranks, then
/// its own), its operator and operands' body.
struct PaintNode {
    std::string path;
    std::vector<std::int64_t> key;
    std::string op;
    loom::Bytes args;
    ZEN_SHAPE(PaintNode, 1, ZEN_FIELD(path), ZEN_FIELD(key), ZEN_FIELD(op), ZEN_FIELD(args));
};

/// Window -> painter: paint these nodes, each with every ancestor present, at this many pixels a
/// cell, setting text as the window does: a character every `text_advance` pixels, a line
/// `text_line` tall.
struct PaintTask {
    std::vector<PaintNode> nodes;
    std::int64_t grain = kCellPx;
    std::int64_t text_advance = kCellPx;
    std::int64_t text_line = kCellPx;
    ZEN_SHAPE(PaintTask, 2, ZEN_FIELD(nodes), ZEN_FIELD(grain), ZEN_FIELD(text_advance),
              ZEN_FIELD(text_line));
};

/// One node as painted for a medium of characters: its cells in the area's cells, packed as runs
/// (graphics/term_paint.hpp: `pack`). A group paints nothing of its own.
struct NodePaint {
    std::string path;
    loom::Bytes cells;
    ZEN_SHAPE(NodePaint, 1, ZEN_FIELD(path), ZEN_FIELD(cells));
};

/// Painter -> window: each node painted.
struct PaintResult {
    std::vector<NodePaint> nodes;
    ZEN_SHAPE(PaintResult, 1, ZEN_FIELD(nodes));
};

namespace event {
inline constexpr const char* kPut = "put";       ///< `input` is the data form, whole
inline constexpr const char* kChange = "change"; ///< `input` is a change's body; `change_kind` which
inline constexpr const char* kPress = "press";   ///< `node` is the node pressed
inline constexpr const char* kRedraw = "redraw"; ///< new code, the same state: drawn whole
} // namespace event

/// Window -> element code: one event for the element the window names `handle`, at `path`, with
/// the state the window kept for it ("" before its first put).
struct ElementEvent {
    std::string kind;
    std::int64_t handle = 0;
    std::string path;
    loom::Bytes state;
    std::int64_t change_kind = 0;
    loom::Bytes input;
    std::string node;
    ZEN_SHAPE(ElementEvent, 1, ZEN_FIELD(kind), ZEN_FIELD(handle), ZEN_FIELD(path),
              ZEN_FIELD(state), ZEN_FIELD(change_kind), ZEN_FIELD(input), ZEN_FIELD(node));
};

/// Element code -> window: the element's state after the event, which the window keeps; what
/// changed in its subtree, applied as its drawing; and whether it took a press.
struct ElementOutcome {
    loom::Bytes state;
    Drawn drawn;
    bool took = true;
    ZEN_SHAPE(ElementOutcome, 2, ZEN_FIELD(state), ZEN_FIELD(drawn), ZEN_FIELD(took));
};

/// What element code is: the data form it presents, and the changes it takes in index order.
struct ElementDescribed {
    std::string type;
    std::vector<std::string> changes;
    ZEN_SHAPE(ElementDescribed, 1, ZEN_FIELD(type), ZEN_FIELD(changes));
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_PAINTING_HPP
