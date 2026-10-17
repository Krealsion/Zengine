// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_DRAWING_HPP
#define ZENGINE_WORKSHOP_PANE_DRAWING_HPP

// Between a pane's drawing and Workshop's rows, both ways. A drawing (`graphics::Sketch`) read
// onto rows: its text painted cell by cell as a window of characters paints it
// (graphics/term_paint.hpp), each record a part of the row its text stands on, the action a press
// on it asks beside it. And rows drawn (`rows_drawing`), each run a named part holds a node asking
// for it by name, for the window showing a pane's area as the pane keeps it (`WindowDrawing`).
// Reference: workshop/docs/workshop-panes.md, "The pane facets".

#include "component/row_map.hpp"
#include "graphics/canvas.hpp"
#include "graphics/ops.hpp"
#include "graphics/painting.hpp"
#include "graphics/term_paint.hpp"
#include "graphics/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <zen/weave.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
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

/// A PANE'S ROWS AS A GRAPHICS WINDOW DRAWS THEM, and where a press reaches each part they name.
struct RowsDrawing {
    graphics::Sketch sketch;
    /// Each named part's place: a cell only it holds, else one where a press reaches it.
    std::map<std::string, RowCell> places;
};

/// ROWS AND THEIR PARTS DRAWN, in a room `columns` wide: each row's text where a window of
/// characters paints it, in its role over its ground, cut where a part begins or ends; each run a
/// part holds is a node whose record asks the part's name, held where parts overlap by the one a
/// press on Workshop's rows reaches (the last the pane reads). A lit row's nodes are named apart,
/// so its ground lies beneath them. Rows past the window's bounds on nodes or records are not
/// drawn. The parts are `PaneRowPart`s or `v2::PaneRowPart`s; the row's text says availability.
template <class Part>
RowsDrawing rows_drawing(const std::vector<surface::SurfaceTextRow>& rows,
                         const std::vector<Part>& parts, std::int64_t columns) {
    struct Run {
        std::string node, words, action;
        std::int64_t x = 0;
    };
    RowsDrawing out;
    std::size_t nodes = 0, records = 0;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const auto row = static_cast<std::int64_t>(r);
        const std::string& text = rows[r].text;
        const auto said = static_cast<std::int64_t>(text.size());
        std::vector<std::size_t> here;
        std::int64_t extent = said;
        for (std::size_t k = 0; k < parts.size(); ++k) {
            if (parts[k].row == row && parts[k].column >= 0 && parts[k].columns > 0) {
                here.push_back(k);
                extent = (std::max)(extent, parts[k].column + parts[k].columns);
            }
        }
        extent = (std::min)(extent, (std::max)(said, columns));
        std::vector<std::int64_t> owner(static_cast<std::size_t>(extent), -1);
        std::vector<int> cover(owner.size(), 0);
        for (const std::size_t k : here) {
            const std::int64_t end = (std::min)(extent, parts[k].column + parts[k].columns);
            for (std::int64_t c = parts[k].column; c < end; ++c) {
                owner[static_cast<std::size_t>(c)] = static_cast<std::int64_t>(k);
                ++cover[static_cast<std::size_t>(c)];
            }
        }
        const bool lit = rows[r].background != surface::role::kNone;
        const std::string prefix = (lit ? "lit" : "row") + std::to_string(r) + ".";
        std::vector<Run> runs;
        std::size_t asks = 0;
        for (std::int64_t a = 0; a < extent;) {
            const std::int64_t k = owner[static_cast<std::size_t>(a)];
            std::int64_t b = a + 1;
            while (b < extent && owner[static_cast<std::size_t>(b)] == k) {
                ++b;
            }
            const std::int64_t end = k < 0 ? (std::min)(b, said) : b;
            if (end > a) {
                Run one;
                one.node = prefix + std::to_string(runs.size());
                one.x = a;
                one.words = a < said ? text.substr(static_cast<std::size_t>(a),
                                                   static_cast<std::size_t>(end - a))
                                     : std::string();
                one.words.resize(static_cast<std::size_t>(end - a), ' ');
                one.action = k >= 0 ? parts[static_cast<std::size_t>(k)].name : std::string();
                asks += one.action.empty() ? std::size_t{0} : std::size_t{1};
                runs.push_back(std::move(one));
            }
            a = b;
        }
        const std::size_t drawn = runs.size() + (lit ? 1 : 0);
        if (nodes + drawn > graphics::kMaxNodesPerArea ||
            records + asks > graphics::kMaxElementsPerArea) {
            break;
        }
        nodes += drawn;
        records += asks;
        if (lit) {
            out.sketch.fill_rect("ground" + std::to_string(r), 0, row * graphics::kCellPx,
                                 (std::max)(extent, columns) * graphics::kCellPx, graphics::kCellPx,
                                 graphics::by_role(rows[r].background));
        }
        for (const Run& one : runs) {
            out.sketch.text(one.node, one.x * graphics::kCellPx, row * graphics::kCellPx,
                            one.words, graphics::by_role(rows[r].role));
            if (!one.action.empty()) {
                out.sketch.action(one.node, one.action);
            }
        }
        for (const std::size_t k : here) {
            if (parts[k].name.empty() || out.places.count(parts[k].name) != 0) {
                continue;
            }
            std::optional<std::int64_t> alone, reached;
            const std::int64_t end = (std::min)(extent, parts[k].column + parts[k].columns);
            for (std::int64_t c = parts[k].column; c < end; ++c) {
                const auto at = static_cast<std::size_t>(c);
                if (!alone && cover[at] == 1) alone = c;
                if (!reached && owner[at] == static_cast<std::int64_t>(k)) reached = c;
            }
            if (alone || reached) {
                out.places[parts[k].name] = RowCell{row, alone ? *alone : *reached, true};
            }
        }
    }
    return out;
}

/// THE GRAPHICS WINDOW THAT SHOWS A PANE'S AREA, as the office drawing there keeps it: the canvas
/// it draws with (`graphics::Canvas`) and the drawing it last sent. A drawing the window refused
/// leaves what it shows as it was. A refused change was read against a drawing the window does not
/// hold, so the drawing last sent goes again at once, whole; a refused whole drawing, which the
/// window would refuse again, does not, and the next drawing goes whole.
class WindowDrawing {
public:
    WindowDrawing(std::string office, std::string area)
        : canvas_(std::move(office), std::move(area)) {}

    const std::string& area() const noexcept { return canvas_.area(); }
    /// A graphics window shows the area.
    bool shown() const noexcept { return !canvas_.window().empty(); }

    /// THE ROOM A WINDOW TELLS OF THE AREA, as that window's office: true when the window, its
    /// incarnation or its size changed, so it holds none of the drawing and the next goes whole.
    bool room(const graphics::AreaRoom& r, const loom::Mail& mail) {
        if (r.area != area() ||
            !mail.authored_from_role(std::string(graphics::kWindowOfficePrefix) + r.window) ||
            !canvas_.room(r)) {
            return false;
        }
        fresh_ = true;
        return true;
    }

    /// Whether `mail` is the word of the window that shows the area, about `about`.
    bool from_window(const std::string& about, const loom::Mail& mail) const {
        return about == area() && shown() && mail.authored_from_role(canvas_.window_office());
    }

    /// DRAW `sketch` IN THE WINDOW THAT SHOWS THE AREA: what changed since the drawing it holds,
    /// nothing when nothing did, or the whole of it when it holds none. Nothing while no window
    /// shows the area.
    void draw(const graphics::Sketch& sketch, loom::Mail& mail) {
        if (!shown()) {
            return;
        }
        last_ = sketch;
        send(mail);
    }

    /// A DRAWING THE WINDOW REFUSED, said by the window that shows the area: true when it was.
    bool refused(const graphics::DrawnRefused& refusal, loom::Mail& mail) {
        if (!from_window(refusal.area, mail)) {
            return false;
        }
        canvas_.forget();
        fresh_ = true;
        if (!drew_whole_) {
            send(mail);
        }
        return true;
    }

private:
    void send(loom::Mail& mail) {
        drew_whole_ = fresh_;
        fresh_ = false;
        canvas_.draw(last_);
        canvas_.show(mail);
    }

    graphics::Canvas canvas_;
    graphics::Sketch last_;
    /// The window holds none of the drawing; the drawing last sent went whole, or none was sent.
    bool fresh_ = true, drew_whole_ = true;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DRAWING_HPP
