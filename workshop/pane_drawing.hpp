// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_DRAWING_HPP
#define ZENGINE_WORKSHOP_PANE_DRAWING_HPP

// Between a pane's drawing and Workshop's rows, both ways. A drawing (`graphics::Sketch`) read
// onto rows: its text painted cell by cell as a window of characters paints it
// (graphics/term_paint.hpp), each record a part of the row its text stands on, the action a press
// on it asks beside it. And rows drawn (`rows_drawing`), a node asking for each run a named part
// holds, the caret in front, for the window showing a pane's area as it keeps it (`WindowDrawing`).
// Reference: workshop/docs/workshop-panes.md, "The pane facets".

#include "component/row_map.hpp"
#include "graphics/canvas.hpp"
#include "graphics/ops.hpp"
#include "graphics/painting.hpp"
#include "graphics/term_paint.hpp"
#include "graphics/vocabulary.hpp"
#include "surface/region.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_canvas_vocabulary.hpp"
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

/// A node's operands with its place read from a drawing set in `text` onto the cell lattice: a
/// group's offset and clip and a text's start, a character's advance a cell across and a line a
/// cell down. Any other node is as drawn.
inline loom::Bytes on_cells(const graphics::Node& n, const graphics::TextMeasure& text) {
    const auto across = [&](std::int64_t v) {
        return surface::floor_div_px(v, text.advance) * graphics::kCellPx;
    };
    const auto down = [&](std::int64_t v) {
        return surface::floor_div_px(v, text.line) * graphics::kCellPx;
    };
    if (n.op == graphics::op::kGroup) {
        if (auto g = graphics::from_body<graphics::GroupArgs>(n.args)) {
            g->x = across(g->x);
            g->y = down(g->y);
            g->clip_w = across(g->clip_w);
            g->clip_h = down(g->clip_h);
            return graphics::body_of(*g);
        }
    } else if (n.op == graphics::op::kText) {
        if (auto t = graphics::from_body<graphics::TextArgs>(n.args)) {
            t->x = across(t->x);
            t->y = down(t->y);
            return graphics::body_of(*t);
        }
    }
    return n.args;
}

/// A DRAWING ON THE ROWS OF A ROOM `rows` BY `columns`, set in `measure`: each text node's
/// characters where a window of characters paints them (offset and clipped by the groups along its
/// path), a character an advance of `measure` and a row a line of it, a row in the role of the
/// first text drawn on it; and each record a part, recorded in `map`, on the row its subtree's
/// first shown text stands on, over the columns its text covers there. What falls outside the room
/// is neither said nor named, and the operators other than text are the window's alone.
inline std::vector<surface::SurfaceTextRow> drawn_rows(const graphics::Sketch& sketch,
                                                       std::int64_t rows, std::int64_t columns,
                                                       component::RowMap<DrawnPart>& map,
                                                       const graphics::TextMeasure& measure = {}) {
    std::vector<surface::SurfaceTextRow> out;
    if (rows <= 0 || columns <= 0) {
        return out;
    }
    const bool lattice = measure == graphics::TextMeasure{};
    std::vector<graphics::PaintNode> nodes;
    nodes.reserve(sketch.nodes().size());
    for (const graphics::Node& n : sketch.nodes()) {
        nodes.push_back(
            graphics::PaintNode{n.path, {}, n.op, lattice ? n.args : on_cells(n, measure)});
    }
    const std::map<std::string, graphics::term::Space> at = graphics::term::spaces(nodes);
    const graphics::term::Painter painter{graphics::term::Look{}};

    // The cells each text node shows on the first row it shows any, by its path.
    struct Run {
        std::int64_t row = 0, first = 0, end = 0;
    };
    std::map<std::string, Run> shown;
    std::vector<bool> roled;
    for (const graphics::PaintNode& n : nodes) {
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

/// THE ROOM A PANE'S ROWS ARE DRAWN IN: its area's size in pixels and how the window showing it
/// sets text, so a row stands a line of it and a character an advance of it.
struct RowsRoom {
    std::int64_t width = 0, height = 0;
    graphics::TextMeasure text;
    /// The characters a row holds and the rows the room holds.
    std::int64_t columns() const { return width / text.advance; }
    std::int64_t rows() const { return height / text.line; }
};

/// A PANE'S ROWS AS A GRAPHICS WINDOW DRAWS THEM, and where a press reaches each part they name.
struct RowsDrawing {
    graphics::Sketch sketch;
    /// Each named part's place: a cell only it holds, else one where a press reaches it.
    std::map<std::string, RowCell> places;
};

/// ROWS AND THEIR PARTS DRAWN, `columns` wide in `room`: the room's ground, all of it, then each
/// row a line of the window's text and a character an advance of it, in its role over its ground
/// (a lit row's nodes named apart above it), cut where a part begins or ends; each run a part holds
/// a node asking the part's name, held where parts overlap by the one a press on Workshop's rows
/// reaches; `caret` in front. No row past `max_rows` (above 0) or the window's bounds is drawn. The
/// parts are `PaneRowPart`s or `v2::PaneRowPart`s; the row's text says availability.
template <class Part>
RowsDrawing rows_drawing(const std::vector<surface::SurfaceTextRow>& rows,
                         const std::vector<Part>& parts, const RowsRoom& room,
                         std::int64_t columns, const RowsCaret& caret = {},
                         std::int64_t max_rows = 0) {
    struct Run {
        std::string node, words, action;
        std::int64_t x = 0;
    };
    RowsDrawing out;
    const std::int64_t advance = room.text.advance;
    const std::int64_t line = room.text.line;
    out.sketch.fill_rect("room", 0, 0, room.width, room.height,
                         graphics::by_role(surface::role::kGround));
    std::size_t nodes = 1, records = 0;
    const std::size_t shown =
        max_rows > 0 ? (std::min)(rows.size(), static_cast<std::size_t>(max_rows)) : rows.size();
    std::size_t r = 0;
    for (; r < shown; ++r) {
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
            out.sketch.fill_rect("ground" + std::to_string(r), 0, row * line,
                                 (std::max)(extent, columns) * advance, line,
                                 graphics::by_role(rows[r].background));
        }
        for (const Run& one : runs) {
            out.sketch.text(one.node, one.x * advance, row * line, one.words,
                            graphics::by_role(rows[r].role));
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

    // THE CARET AND THE SELECTION, in front of the rows: each shown row's selected columns again
    // in the accent role, and the caret a bar at the left of the cell it sits before, as a window
    // that sets type draws one. Each mark asks what the part beneath it asks (the last part said
    // that covers its cell), so a press on it reaches the row it stands on. Marks stand on drawn
    // rows alone, and one that would take the area past the window's bounds is not drawn.
    const auto asked_at = [&](std::int64_t row, std::int64_t column) {
        std::string asks;
        for (const Part& p : parts) {
            if (p.row == row && column >= p.column && column < p.column + p.columns) {
                asks = p.name;
            }
        }
        return asks;
    };
    const auto room_for = [&](const std::string& asks) {
        return nodes < graphics::kMaxNodesPerArea &&
               (asks.empty() || records < graphics::kMaxElementsPerArea);
    };
    const auto ask = [&](const std::string& node, const std::string& asks) {
        ++nodes;
        if (!asks.empty()) {
            out.sketch.action(node, asks);
            ++records;
        }
    };
    surface::SurfaceTextRegion selected; // the selection's per-row arithmetic is surface's
    selected.sel_begin_row = caret.sel_begin_row;
    selected.sel_begin_col = caret.sel_begin_col;
    selected.sel_end_row = caret.sel_end_row;
    selected.sel_end_col = caret.sel_end_col;
    const auto marked = static_cast<std::int64_t>(r); // the rows drawn
    for (std::int64_t m = 0; m < marked; ++m) {
        const std::string& text = rows[static_cast<std::size_t>(m)].text;
        const auto length = (std::min)(static_cast<std::int64_t>(text.size()), columns);
        const surface::RowSpan span = surface::selection_span_of_row(selected, m, length);
        std::size_t run = 0;
        for (std::int64_t a = span.begin; a < span.end;) {
            const std::string asks = asked_at(m, a);
            std::int64_t b = a + 1;
            while (b < span.end && asked_at(m, b) == asks) {
                ++b;
            }
            const std::string node = "sel" + std::to_string(m) + "." + std::to_string(run++);
            if (!room_for(asks)) {
                return out;
            }
            const auto from = static_cast<std::size_t>(a);
            out.sketch.text(node, a * advance, m * line,
                            text.substr(from, static_cast<std::size_t>(b - a)),
                            graphics::by_role(surface::role::kAccent));
            ask(node, asks);
            a = b;
        }
    }
    if (caret.row < 0 || caret.row >= marked || caret.column < 0 || columns <= 0) {
        return out;
    }
    const std::int64_t column = (std::min)(caret.column, columns - 1);
    const std::string asks = asked_at(caret.row, column);
    if (room_for(asks)) {
        out.sketch.line("caret", column * advance, caret.row * line, column * advance,
                        caret.row * line + line - 1, graphics::by_role(surface::role::kAccent));
        ask("caret", asks);
    }
    return out;
}

/// WHERE A POINT OF A PANE'S AREA FALLS ON THE ROWS DRAWN THERE (`rows_drawing`, set by `text`):
/// the row and column of the character holding it, shown while it lies within `rows` by `columns`.
inline RowCell window_cell(std::int64_t x, std::int64_t y, std::int64_t rows,
                           std::int64_t columns, const graphics::TextMeasure& text) {
    RowCell out;
    out.row = surface::floor_div_px(y, text.line);
    out.column = surface::floor_div_px(x, text.advance);
    out.shown = out.row >= 0 && out.row < rows && out.column >= 0 && out.column < columns;
    return out;
}

/// ONE POINTER MOMENT A GRAPHICS WINDOW GAVE A PANE'S AREA, as the pane reads it: its phase in the
/// canvas pointer's words (`canvas_pointer::kPress` ... `kWheel`, a hold the window ended being
/// `kLost`), the button, the point in the area's pixels and the cell of the rows drawn there
/// holding it, the modifiers, the wheel's turn, and the press's number, which the press, its moves
/// and its release carry and a continuation about the press cites.
struct WindowPointer {
    std::int64_t phase = canvas_pointer::kPress;
    std::int64_t button = 0;
    std::int64_t x = 0, y = 0;
    RowCell at;
    std::int64_t modifiers = 0;
    double dx = 0, dy = 0;
    std::uint64_t press = 0;
};

/// THE GRAPHICS WINDOW THAT SHOWS A PANE'S AREA, as the office drawing there keeps it: the canvas
/// it draws with (`graphics::Canvas`) and the drawing it last sent. A drawing the window refused
/// leaves what it shows as it was. A refused change was read against a drawing the window does not
/// hold, so the drawing last sent goes again at once, whole; a refused whole drawing, which the
/// window would refuse again, does not, nor does that drawing unchanged, and the next goes whole.
class WindowDrawing {
public:
    WindowDrawing(std::string office, std::string area)
        : canvas_(std::move(office), std::move(area)) {}

    const std::string& area() const noexcept { return canvas_.area(); }
    /// A graphics window shows the area.
    bool shown() const noexcept { return !canvas_.window().empty(); }
    /// The office of the window that shows the area, which a drawing sent there addresses.
    std::string window_office() const { return canvas_.window_office(); }
    /// The office that answers a paste asked from the area: the window showing it, else the Skin.
    std::string clipboard_office() const {
        return shown() ? canvas_.window_office() : std::string(surface::kSkinRole);
    }
    /// The room that window gives the area, in pixels, and how it sets text there.
    std::int64_t width() const noexcept { return canvas_.width(); }
    std::int64_t height() const noexcept { return canvas_.height(); }
    const graphics::TextMeasure& text() const noexcept { return canvas_.text(); }
    /// The window's device pixels in one cell: none where its unit is the cell.
    std::int64_t cell_px() const noexcept { return canvas_.cell_px(); }
    /// The rows drawn there, as `rows_drawing` takes them: the whole room, set by the window.
    RowsRoom rows_room() const { return RowsRoom{width(), height(), text()}; }
    /// The window refused the whole drawing last sent, and no room or other drawing came since.
    bool refused_whole() const noexcept { return refused_whole_; }

    /// THE ROOM A WINDOW TELLS OF THE AREA, as that window's office: true when the window, its
    /// incarnation, its size or its measure changed or the area is empty, so it holds none of the
    /// drawing and the next goes whole.
    bool room(const graphics::AreaRoom& r, const loom::Mail& mail) {
        if (r.area != area() ||
            !mail.authored_from_role(std::string(graphics::kWindowOfficePrefix) + r.window) ||
            !canvas_.room(r)) {
            return false;
        }
        fresh_ = true;
        refused_whole_ = false;
        return true;
    }

    /// Whether `mail` is the word of the window that shows the area, about `about`.
    bool from_window(const std::string& about, const loom::Mail& mail) const {
        return about == area() && shown() && mail.authored_from_role(canvas_.window_office());
    }

    /// A POINTER MOMENT THE WINDOW SHOWING THE AREA GAVE IT, as that window's office, read onto
    /// the rows drawn there, `rows` by `columns`: a button pressed or let go, a move, the wheel, or
    /// a hold the window ended. None for another area or speaker.
    std::optional<WindowPointer> pointer(const graphics::AreaButtonChanged& b,
                                         const loom::Mail& mail, std::int64_t rows,
                                         std::int64_t columns) const {
        if (!from_window(b.area, mail)) {
            return std::nullopt;
        }
        WindowPointer p = at(b.x, b.y, rows, columns, mail);
        p.phase = b.pressed ? canvas_pointer::kPress : canvas_pointer::kRelease;
        p.button = b.button;
        p.modifiers = b.modifiers;
        return p;
    }
    std::optional<WindowPointer> pointer(const graphics::AreaPointerMoved& m,
                                         const loom::Mail& mail, std::int64_t rows,
                                         std::int64_t columns) const {
        if (!from_window(m.area, mail)) {
            return std::nullopt;
        }
        WindowPointer p = at(m.x, m.y, rows, columns, mail);
        p.phase = canvas_pointer::kMove;
        p.modifiers = m.modifiers;
        return p;
    }
    std::optional<WindowPointer> pointer(const graphics::AreaWheelTurned& w, const loom::Mail& mail,
                                         std::int64_t rows, std::int64_t columns) const {
        if (!from_window(w.area, mail)) {
            return std::nullopt;
        }
        WindowPointer p = at(w.x, w.y, rows, columns, mail);
        p.phase = canvas_pointer::kWheel;
        p.modifiers = w.modifiers;
        p.dx = w.dx;
        p.dy = w.dy;
        return p;
    }
    std::optional<WindowPointer> pointer(const graphics::AreaHoldEnded& e,
                                         const loom::Mail& mail) const {
        if (!from_window(e.area, mail)) {
            return std::nullopt;
        }
        WindowPointer p;
        p.phase = canvas_pointer::kLost;
        p.button = e.button;
        return p;
    }

    /// DRAW `sketch` IN THE WINDOW THAT SHOWS THE AREA: what changed since the drawing it holds,
    /// nothing when nothing did, or the whole of it when it holds none. Nothing while no window
    /// shows the area, nor while it refused this drawing whole.
    void draw(const graphics::Sketch& sketch, loom::Mail& mail) {
        if (!shown() || (refused_whole_ && unchanged(sketch))) {
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
        } else if (!last_.nodes().empty()) {
            refused_whole_ = true;
        }
        return true;
    }

private:
    WindowPointer at(std::int64_t x, std::int64_t y, std::int64_t rows, std::int64_t columns,
                     const loom::Mail& mail) const {
        WindowPointer p;
        p.x = x;
        p.y = y;
        p.at = window_cell(x, y, rows, columns, text());
        p.press = mail.correlation();
        return p;
    }

    void send(loom::Mail& mail) {
        drew_whole_ = fresh_;
        fresh_ = false;
        refused_whole_ = false;
        canvas_.draw(last_);
        canvas_.show(mail);
    }

    bool unchanged(const graphics::Sketch& sketch) const {
        const graphics::Drawn d = graphics::difference(last_, sketch);
        return d.put.empty() && d.removed.empty() && d.said.empty() && d.unsaid.empty();
    }

    graphics::Canvas canvas_;
    graphics::Sketch last_;
    /// The window holds none of the drawing; the drawing last sent went whole, or none was sent;
    /// the window refused that whole drawing.
    bool fresh_ = true, drew_whole_ = true, refused_whole_ = false;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DRAWING_HPP
