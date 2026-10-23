// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_CANVAS_DRAWING_HPP
#define ZENGINE_WORKSHOP_PANE_CANVAS_DRAWING_HPP

// A pane's canvas picture as a graphics window draws it: its rectangles, labels and runs of text in
// the order Workshop paints them, then each part it names painted again over its own place, clipped
// there, under a group whose record asks the part's name. A window lands a press on what it
// painted, so a part is pressed wherever the picture shows it, and a later part over an earlier one,
// as a pane reads a press on its picture. Reference: graphics/docs/graphics.md.

#include "graphics/canvas.hpp"
#include "graphics/ops.hpp"
#include "graphics/vocabulary.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/pane_picture.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// A graphics window's room as a pane draws in it: `width` by `height` pixels, in the window's unit
/// and its text. A window that shows every pixel (`cell_px` above zero) is a graphical room a pixel
/// a grain, its text set in `text`; one whose unit is the cell is a room of cells, a character a
/// cell.
inline CanvasRoom canvas_window_room(std::int64_t width, std::int64_t height,
                                     std::int64_t cell_px, const graphics::TextMeasure& text) {
    if (cell_px <= 0) {
        return CanvasRoom{width, height, graphics::kCellPx, false, 0, 0};
    }
    return CanvasRoom{width, height, 1, true, text.advance, text.line};
}

/// A CANVAS PICTURE AS A WINDOW DRAWS IT, and why the picture itself could not be: a drawing past
/// an area's bounds is a ground and that sentence alone, never one the window refuses.
struct CanvasSketch {
    graphics::Sketch sketch;
    std::string over; ///< why the picture was not drawn, or ""
};

/// THE PICTURE, drawn for the room it was composed in, over the room's ground, the whole of it, so
/// nothing behind the area shows through. Its nodes stand in three groups -- `picture`, what it
/// shows, `parts`, each part's place painted again, and `marks`, each run's caret and selection in
/// front of both -- and are named by their place in their group, so a node a change adds goes after
/// its siblings and paints where the picture paints it; a padded run of text's words stand inset in
/// its box where the room is graphical, and a run's ground stands under its box. A named part's
/// group is recorded with the part's name as its action, said unavailable (`graphics::PartFace`)
/// where the part says so; a part with no name asks nothing, so where it lies over another, as the
/// pane reads a press, the window gives no press to the one beneath.
inline CanvasSketch canvas_sketch(const CanvasPicture& c, const CanvasRoom& room) {
    struct Drawn {
        const char* op;
        std::int64_t x, y, w, h;
        std::int64_t role;
        const std::string* words;
        std::int64_t inset = 0; ///< where its words begin inside its box
        const CanvasText* run = nullptr; ///< the run it draws, whose caret and selection it marks
    };
    const auto metrics = canvas_text_metrics(room);
    const auto across = [&](std::size_t bytes, std::int64_t advance) {
        return static_cast<std::int64_t>(bytes) * advance + 2 * metrics.inset;
    };
    std::vector<Drawn> drawn;
    drawn.reserve(c.rects.size() + c.labels.size() + c.texts.size());
    for (const CanvasRect& r : c.rects) {
        drawn.push_back({graphics::op::kRect, r.x, r.y, r.w, r.h, r.role, nullptr});
    }
    for (const CanvasLabel& l : c.labels) {
        drawn.push_back({graphics::op::kText, l.x, l.y, across(l.text.size(), kPaneCanvasUnit),
                         kPaneCanvasUnit, l.role, &l.text});
    }
    for (const CanvasText& t : c.texts) {
        // AN UNPADDED RUN'S GLYPHS STAND AT ITS X/Y, its box the medium's inset around them.
        const std::int64_t back = t.padded ? 0 : metrics.inset;
        const Drawn run{graphics::op::kText, t.x - back, t.y - back,
                        across(t.text.size(), metrics.advance), metrics.line + 2 * metrics.inset,
                        t.role, &t.text, metrics.inset, &t};
        if (t.background != graphics::role::kNone) {
            drawn.push_back({graphics::op::kRect, run.x, run.y, run.w, run.h, t.background,
                             nullptr});
        }
        drawn.push_back(run);
    }
    const auto meets = [](const Drawn& d, const CanvasPart& p) {
        return d.x < p.x + p.w && p.x < d.x + d.w && d.y < p.y + p.h && p.y < d.y + d.h;
    };

    // THE MARKS, as the rows drawing marks a row (`rows_drawing`): a run's selected characters
    // again in the accent role, and its caret a bar at the left of the character it sits before,
    // as a window that sets type draws one -- in a room of cells, that character again in the
    // accent role, which a bar there would hide, and a bar where it sits before none. Each asks what
    // the last named part over its place asks, so a press on it reaches what lies beneath.
    struct Mark {
        std::string node;
        std::int64_t x, y;
        std::string words; ///< what it draws again, or none for a bar
        std::string asks;
        bool available = true;
    };
    std::vector<Mark> marks;
    for (std::size_t i = 0; i < drawn.size(); ++i) {
        const CanvasText* t = drawn[i].run;
        if (t == nullptr) continue;
        const std::int64_t gx = drawn[i].x + drawn[i].inset, gy = drawn[i].y + drawn[i].inset;
        const auto length = static_cast<std::int64_t>(t->text.size());
        const auto mark = [&](std::string node, std::int64_t column, std::string words) {
            Mark m{std::move(node), gx + column * metrics.advance, gy, std::move(words), {}, true};
            for (const CanvasPart& p : c.parts) {
                if (p.w > 0 && p.h > 0 && !p.name.empty() && m.x >= p.x && m.x < p.x + p.w &&
                    m.y >= p.y && m.y < p.y + p.h) {
                    m.asks = p.name;
                    m.available = p.available;
                }
            }
            marks.push_back(std::move(m));
        };
        const std::int64_t from = (std::max)(std::int64_t{0}, t->sel_begin_col);
        const std::int64_t to = (std::min)(length, t->sel_end_col);
        if (t->sel_begin_col >= 0 && from < to) {
            mark("sel" + std::to_string(i), from,
                 t->text.substr(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from)));
        }
        if (t->caret_col >= 0 && t->caret_col <= length) {
            const auto at = static_cast<std::size_t>(t->caret_col);
            const bool on = !metrics.graphical && at < t->text.size() && t->text[at] != ' ';
            mark("caret" + std::to_string(i), t->caret_col, on ? t->text.substr(at, 1) : "");
        }
    }

    // What a window would keep, judged before anything is drawn: two groups, the picture, and
    // each part's group with what it paints again, counted no further than a bound.
    const auto placed = [](const CanvasPart& p) { return p.w > 0 && p.h > 0; };
    std::size_t nodes = 3 + drawn.size(), records = 0;
    for (const CanvasPart& p : c.parts) {
        records += placed(p) && !p.name.empty() ? std::size_t{1} : std::size_t{0};
    }
    nodes += marks.empty() ? 0 : 1 + marks.size();
    for (const Mark& m : marks) records += m.asks.empty() ? std::size_t{0} : std::size_t{1};
    for (auto p = c.parts.begin(); p != c.parts.end() && records <= graphics::kMaxElementsPerArea &&
                                   nodes <= graphics::kMaxNodesPerArea;
         ++p) {
        if (!placed(*p)) continue;
        ++nodes;
        for (const Drawn& d : drawn) nodes += meets(d, *p) ? std::size_t{1} : std::size_t{0};
    }
    CanvasSketch out;
    if (nodes > graphics::kMaxNodesPerArea || records > graphics::kMaxElementsPerArea) {
        out.over = "this picture is more than a graphics window keeps, " +
                   std::to_string(graphics::kMaxNodesPerArea) + " nodes and " +
                   std::to_string(graphics::kMaxElementsPerArea) + " pressed parts";
        out.sketch.fill_rect("ground", 0, 0, room.width, room.height,
                             graphics::by_role(graphics::role::kGround));
        out.sketch.group("picture", 0, 0);
        out.sketch.text("picture/0", 0, 0, out.over, graphics::by_role(graphics::role::kAlert));
        return out;
    }

    const auto put = [&](const std::string& path, const Drawn& d, std::int64_t dx,
                         std::int64_t dy) {
        if (d.words == nullptr) {
            out.sketch.fill_rect(path, d.x - dx, d.y - dy, d.w, d.h, graphics::by_role(d.role));
        } else {
            out.sketch.text(path, d.x + d.inset - dx, d.y + d.inset - dy, *d.words,
                            graphics::by_role(d.role));
        }
    };
    out.sketch.fill_rect("ground", 0, 0, room.width, room.height,
                         graphics::by_role(graphics::role::kGround));
    out.sketch.group("picture", 0, 0);
    for (std::size_t i = 0; i < drawn.size(); ++i) {
        put("picture/" + std::to_string(i), drawn[i], 0, 0);
    }
    out.sketch.group("parts", 0, 0);
    std::size_t at = 0;
    for (const CanvasPart& p : c.parts) {
        if (!placed(p)) continue;
        const std::string group = "parts/" + std::to_string(at++);
        out.sketch.group(group, p.x, p.y, p.w, p.h);
        if (!p.name.empty() && p.available) {
            out.sketch.action(group, p.name);
        } else if (!p.name.empty()) {
            out.sketch.record(group, graphics::PartFace{false}, p.name);
        }
        std::size_t k = 0;
        for (const Drawn& d : drawn) {
            if (meets(d, p)) put(group + "/" + std::to_string(k++), d, p.x, p.y);
        }
    }
    if (!marks.empty()) out.sketch.group("marks", 0, 0);
    for (const Mark& m : marks) {
        const std::string node = "marks/" + m.node;
        const graphics::Paint accent = graphics::by_role(graphics::role::kAccent);
        if (m.words.empty()) {
            out.sketch.line(node, m.x, m.y, m.x, m.y + metrics.line - 1, accent);
        } else {
            out.sketch.text(node, m.x, m.y, m.words, accent);
        }
        if (m.asks.empty()) continue;
        if (m.available) {
            out.sketch.action(node, m.asks);
        } else {
            out.sketch.record(node, graphics::PartFace{false}, m.asks);
        }
    }
    return out;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_CANVAS_DRAWING_HPP
