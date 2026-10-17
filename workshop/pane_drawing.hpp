// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_DRAWING_HPP
#define ZENGINE_WORKSHOP_PANE_DRAWING_HPP

// A pane's drawing (`graphics::Sketch`) read onto Workshop's rows: its text painted cell by cell
// as a window of characters paints it (graphics/term_paint.hpp), and each element record a part
// of the row its text stands on, the action a press on it asks beside it. The pane draws one
// picture; this is what Workshop's rows make of it. Reference: workshop/docs/workshop-panes.md,
// "The pane facets".

#include "component/row_map.hpp"
#include "graphics/canvas.hpp"
#include "graphics/ops.hpp"
#include "graphics/painting.hpp"
#include "graphics/term_paint.hpp"
#include "graphics/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace zengine::workshop {

/// WHAT ONE PART OF A DRAWING MEANS: the path of the record that names it, which is its name, and
/// the action a press on it asks ("" for none). A picture numbered by these moves its number when
/// a part's action changes, as when its place does.
struct DrawnPart {
    std::string path;
    std::string action;
    bool operator==(const DrawnPart&) const = default;
};

/// A DRAWING ON THE ROWS OF A ROOM `rows` BY `columns`: each text node's characters where a window
/// of characters paints them (`graphics::kCellPx` pixels a cell, offset and clipped by the groups
/// along its path), a row in the role of the first text drawn on it; and each record a part,
/// recorded in `map`, on the row its subtree's first shown text stands on, over the columns its
/// text covers there. What falls outside the room is neither said nor named, and the operators
/// other than text are the window's alone.
inline std::vector<surface::SurfaceTextRow> drawn_rows(const graphics::Sketch& sketch,
                                                       std::int64_t rows, std::int64_t columns,
                                                       component::RowMap<DrawnPart>& map) {
    std::vector<surface::SurfaceTextRow> out;
    if (rows <= 0 || columns <= 0) {
        return out;
    }
    std::vector<graphics::PaintNode> nodes;
    nodes.reserve(sketch.nodes().size());
    for (const graphics::Node& n : sketch.nodes()) {
        nodes.push_back(graphics::PaintNode{n.path, {}, n.op, n.args});
    }
    const std::map<std::string, graphics::term::Space> at = graphics::term::spaces(nodes);
    const graphics::term::Painter painter{graphics::term::Look{}};

    // The cells each text node shows on the first row it shows any, by its path.
    struct Run {
        std::int64_t row = 0, first = 0, end = 0;
    };
    std::map<std::string, Run> shown;
    std::vector<bool> roled;
    for (const graphics::Node& n : sketch.nodes()) {
        if (n.op != graphics::op::kText) {
            continue;
        }
        std::vector<graphics::term::Cell> cells;
        painter.paint(n.op, n.args, at.at(n.path), cells);
        std::optional<Run> run;
        for (const graphics::term::Cell& c : cells) {
            if (c.row < 0 || c.col < 0 || c.row >= rows || c.col >= columns) {
                continue;
            }
            const auto r = static_cast<std::size_t>(c.row);
            const auto col = static_cast<std::size_t>(c.col);
            if (out.size() <= r) {
                out.resize(r + 1);
                roled.resize(r + 1, false);
            }
            std::string& text = out[r].text;
            if (text.size() <= col) {
                text.resize(col + 1, ' ');
            }
            text[col] = c.glyph;
            if (!roled[r]) {
                out[r].role = c.role;
                roled[r] = true;
            }
            if (!run) {
                run = Run{c.row, c.col, c.col + 1};
            } else if (c.row == run->row) {
                run->first = (std::min)(run->first, c.col);
                run->end = (std::max)(run->end, c.col + 1);
            }
        }
        if (run) {
            shown[n.path] = *run;
        }
    }

    for (const graphics::ElementRecord& record : sketch.records()) {
        if (record.path.empty()) {
            continue;
        }
        std::optional<Run> part;
        for (const graphics::Node& n : sketch.nodes()) {
            const auto it = graphics::under(n.path, record.path) ? shown.find(n.path) : shown.end();
            if (it == shown.end()) {
                continue;
            }
            if (!part) {
                part = it->second;
            } else if (it->second.row == part->row) {
                part->first = (std::min)(part->first, it->second.first);
                part->end = (std::max)(part->end, it->second.end);
            }
        }
        if (part) {
            (void)map.span(part->row, part->first, part->end - part->first, columns,
                           DrawnPart{record.path, record.action});
        }
    }
    return out;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DRAWING_HPP
