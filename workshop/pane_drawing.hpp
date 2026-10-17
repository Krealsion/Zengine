// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_DRAWING_HPP
#define ZENGINE_WORKSHOP_PANE_DRAWING_HPP

// Between a pane's drawing and Workshop's rows, both ways. A drawing (`graphics::Sketch`) read
// onto rows: its text painted cell by cell as a window of characters paints it
// (graphics/term_paint.hpp), each element record a part of the row its text stands on, the action
// a press on it asks beside it. And rows drawn: a pane that says rows shows them in a graphics
// window as a drawing, each run of columns a named part holds a node that asks for it by name.
// Reference: workshop/docs/workshop-panes.md, "The pane facets".

#include "component/row_map.hpp"
#include "graphics/canvas.hpp"
#include "graphics/ops.hpp"
#include "graphics/painting.hpp"
#include "graphics/term_paint.hpp"
#include "graphics/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"

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

/// WHERE A PRESS ON A PART OF DRAWN ROWS LANDS: the part's name, the row it stands on and the
/// first column of it that a press reaches.
struct SketchedPart {
    std::string name;
    std::int64_t row = 0, column = 0;
};

/// ROWS AS A DRAWING, `graphics::kCellPx` pixels a cell: each row's text, its blanks after the last
/// character left out, a node `rows/<i>` in the row's role; and, in front of them, each run of a
/// row's columns a press reaches one part on -- the last of `parts`, in the order a press reads
/// them, that covers it -- a node `parts/<row>.<run>` over the run, recorded, where the part is
/// named, as asking its name. Past `graphics::kMaxElementsPerArea` runs the rest are their rows'
/// text alone. Returns where a press on each part drawn so lands.
inline std::vector<SketchedPart> rows_sketch(const std::vector<surface::SurfaceTextRow>& rows,
                                             const std::vector<v2::PaneRowPart>& parts,
                                             graphics::Sketch& out) {
    out.group("rows", 0, 0);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::string& text = rows[i].text;
        const std::size_t last = text.find_last_not_of(' ');
        if (last != std::string::npos) {
            out.text("rows/" + std::to_string(i), 0,
                     static_cast<std::int64_t>(i) * graphics::kCellPx, text.substr(0, last + 1),
                     graphics::by_role(rows[i].role));
        }
    }
    out.group("parts", 0, 0);
    std::vector<SketchedPart> landed;
    std::size_t recorded = 0;
    const auto limit = static_cast<std::int64_t>(graphics::kMaxArgsBytes);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const auto row = static_cast<std::int64_t>(r);
        std::vector<std::int64_t> owner; // the part a press on each column reaches, or -1
        for (std::size_t k = 0; k < parts.size(); ++k) {
            const v2::PaneRowPart& p = parts[k];
            if (p.row != row || p.column < 0 || p.columns <= 0 || p.column >= limit) continue;
            const std::int64_t end = (std::min)(p.column + p.columns, limit);
            if (static_cast<std::int64_t>(owner.size()) < end) {
                owner.resize(static_cast<std::size_t>(end), -1);
            }
            for (std::int64_t c = p.column; c < end; ++c) {
                owner[static_cast<std::size_t>(c)] = static_cast<std::int64_t>(k);
            }
        }
        const std::string& text = rows[r].text;
        std::size_t run = 0;
        for (std::size_t c = 0; c < owner.size() && recorded < graphics::kMaxElementsPerArea;) {
            std::size_t end = c + 1;
            while (end < owner.size() && owner[end] == owner[c]) ++end;
            if (owner[c] >= 0 && !parts[static_cast<std::size_t>(owner[c])].name.empty()) {
                const std::string& name = parts[static_cast<std::size_t>(owner[c])].name;
                std::string words = c < text.size() ? text.substr(c, end - c) : std::string();
                words.resize(end - c, ' '); // a press reaches the run's blanks too
                const std::string path = "parts/" + std::to_string(r) + "." + std::to_string(run++);
                out.text(path, static_cast<std::int64_t>(c) * graphics::kCellPx,
                         row * graphics::kCellPx, words, graphics::by_role(rows[r].role));
                out.action(path, name);
                ++recorded;
                if (std::none_of(landed.begin(), landed.end(),
                                 [&](const SketchedPart& l) { return l.name == name; })) {
                    landed.push_back(SketchedPart{name, row, static_cast<std::int64_t>(c)});
                }
            }
            c = end;
        }
    }
    return landed;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DRAWING_HPP
