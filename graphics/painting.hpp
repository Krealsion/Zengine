// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_PAINTING_HPP
#define ZENGINE_GRAPHICS_PAINTING_HPP

// What a window hands its painter and what comes back: a node to paint, its colours resolved, and
// what it painted. Reference: graphics/docs/graphics.md.

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
/// its own), what it draws, the colours its look names as the window resolved them, in the order
/// `colors_of` gives (graphics/ops.hpp), and for a text the font, size and measure the window
/// resolved its look to (`face`, the face's family; none, all zero, where the task's measure
/// stands) and its weight (`text_weight::kOfStyle` where the window resolved none). A painter reads
/// its colours from `inks` alone, and a text's measure from `text` where it holds one. A node
/// handed only to be read (`read_only`) -- the sheet a node to paint draws from -- is painted
/// nothing.
struct PaintNode {
    std::string path;
    std::vector<std::int64_t> key;
    Operands draw;
    std::vector<PaintInk> inks;
    TextMeasured text{};
    std::string face{};
    std::int64_t weight = text_weight::kOfStyle;
    bool read_only = false;
    /// Version 6: it draws by eleven operators, and may be handed only to be read.
    ZEN_SHAPE(PaintNode, 6, ZEN_FIELD(path), ZEN_FIELD(key), ZEN_FIELD(draw), ZEN_FIELD(inks),
              ZEN_FIELD(text), ZEN_FIELD(face), ZEN_FIELD(weight), ZEN_FIELD(read_only));

    /// The weight a text is set at: the one the window resolved, else regular.
    std::int64_t weight_set() const {
        return weight != text_weight::kOfStyle ? weight : text_weight::kRegular;
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
    /// Version 7: its nodes draw by eleven operators.
    ZEN_SHAPE(PaintTask, 7, ZEN_FIELD(nodes), ZEN_FIELD(grain), ZEN_FIELD(text_advance),
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

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_PAINTING_HPP
