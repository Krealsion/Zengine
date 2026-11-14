// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_PAINTING_HPP
#define ZENGINE_GRAPHICS_PAINTING_HPP

// What a window hands its painter and what comes back -- a node to paint, its colours resolved, and
// what it painted -- and what crosses the element seam a window calls in its own handler, as Loom
// canonical bytes, never as messages: an event for an element's code and what came of it.
// Reference: graphics/docs/graphics.md.

#include "graphics/text_style.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::graphics {

/// A colour as the window resolved it for its painter: exact (0xRRGGBBAA, or negative for no paint
/// at all), what a terminal shows where it cannot show it ("" its nearest) and the character a
/// terminal fills a shape with in it.
struct PaintInk {
    std::int64_t rgba = -1;
    std::string cells;
    std::string fill;
    ZEN_SHAPE(PaintInk, 1, ZEN_FIELD(rgba), ZEN_FIELD(cells), ZEN_FIELD(fill));

    bool none() const { return rgba < 0; }
    friend bool operator==(const PaintInk&, const PaintInk&) = default;
};

/// A node as a painter needs it: its place in the area's paint order (its ancestors' ranks, then
/// its own), what it draws, the colours its operands name as the window resolved
/// them, in the order `colors_of` gives (graphics/ops.hpp), and for a text the font, size and
/// measure the window resolved its style, size and font to (`face`, the face's family; none, all
/// zero, where the task's measure stands) and its weight (`text_weight::kOfStyle` where the window
/// resolved none: the operands' own). A painter reads its colours from `inks` alone, and a text's
/// measure from `text` where it holds one.
struct PaintNode {
    std::string path;
    std::vector<std::int64_t> key;
    Operands draw;
    std::vector<PaintInk> inks;
    TextMeasured text{};
    std::string face{};
    std::int64_t weight = text_weight::kOfStyle;
    /// Version 5: it draws by its operands, the operator the case they hold.
    ZEN_SHAPE(PaintNode, 5, ZEN_FIELD(path), ZEN_FIELD(key), ZEN_FIELD(draw), ZEN_FIELD(inks),
              ZEN_FIELD(text), ZEN_FIELD(face), ZEN_FIELD(weight));

    /// The weight a text is set at: the one the window resolved, else the operands' own.
    std::int64_t weight_of(std::int64_t operands) const {
        return weight != text_weight::kOfStyle ? weight : operands;
    }
};

/// Window -> painter: paint these nodes, each with every ancestor present, at this many pixels a
/// cell, setting text as the window does: a character every `text_advance` pixels, a line
/// `text_line` tall. `plain` and `quiet` are the area's text and quiet colours, for what carries
/// no colour of its own (an image set in characters; an operator the painter does not know,
/// painted as its name), and `outline` a terminal's corner, across and down.
struct PaintTask {
    std::vector<PaintNode> nodes;
    std::int64_t grain = kCellPx;
    std::int64_t text_advance = kCellPx;
    std::int64_t text_line = kCellPx;
    PaintInk plain;
    PaintInk quiet;
    std::string outline = "+-|";
    ZEN_SHAPE(PaintTask, 6, ZEN_FIELD(nodes), ZEN_FIELD(grain), ZEN_FIELD(text_advance),
              ZEN_FIELD(text_line), ZEN_FIELD(plain), ZEN_FIELD(quiet), ZEN_FIELD(outline));
};

/// One node as painted: for a medium of characters its cells in the area's cells, packed as runs
/// (graphics/term_paint.hpp: `pack`); for light, its primitives behind their tag
/// (graphics/light_paint.hpp: `pack_node`). A group paints nothing of its own.
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
    ZEN_SHAPE(ElementOutcome, 3, ZEN_FIELD(state), ZEN_FIELD(drawn), ZEN_FIELD(took));
};

/// What element code is: the data form it presents, and the changes it takes in index order.
struct ElementDescribed {
    std::string type;
    std::vector<std::string> changes;
    ZEN_SHAPE(ElementDescribed, 1, ZEN_FIELD(type), ZEN_FIELD(changes));
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_PAINTING_HPP
