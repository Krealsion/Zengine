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
#include "workshop/pane_canvas_vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// A graphics window's room as a pane draws in it: `width` by `height` pixels, in the window's unit
/// and its text. A window that shows every pixel (`cell_px` above zero) is a graphical room a pixel
/// a grain, its text set in `text`; one whose unit is the cell is a room of cells, a character a
/// cell.
inline PaneCanvasRoom canvas_window_room(const std::string& pane, std::int64_t width,
                                         std::int64_t height, std::int64_t cell_px,
                                         const graphics::TextMeasure& text) {
    if (cell_px <= 0) {
        return PaneCanvasRoom{pane, 1, width, height, graphics::kCellPx, false, 0, 0};
    }
    return PaneCanvasRoom{pane, 1, width, height, 1, true, text.advance, text.line};
}

/// A CANVAS PICTURE AS A WINDOW DRAWS IT, and why the picture itself could not be: a drawing past
/// an area's bounds is a ground and that sentence alone, never one the window refuses.
struct CanvasSketch {
    graphics::Sketch sketch;
    std::string over; ///< why the picture was not drawn, or ""
};

/// THE PICTURE, drawn for the room it was composed in, over the room's ground, the whole of it, so
/// nothing behind the area shows through. Its nodes stand in two groups -- `picture`, what it
/// shows, and `parts`, each named part's place painted again -- and are named by their place in
/// their group, so a node a change adds goes after its siblings and paints where the picture paints
/// it; a run of text's words stand inset in its box where the room is graphical. A part's group is
/// recorded with the part's name as its action; a part with no name is pressed nowhere.
inline CanvasSketch canvas_sketch(const v4::PaneCanvasContent& c, const PaneCanvasRoom& room) {
    struct Drawn {
        const char* op;
        std::int64_t x, y, w, h;
        std::int64_t role;
        const std::string* words;
        std::int64_t inset = 0; ///< where its words begin inside its box
    };
    const auto metrics = canvas_text_metrics(room);
    const auto across = [&](std::size_t bytes, std::int64_t advance) {
        return static_cast<std::int64_t>(bytes) * advance + 2 * metrics.inset;
    };
    std::vector<Drawn> drawn;
    drawn.reserve(c.rects.size() + c.labels.size() + c.texts.size());
    for (const PaneCanvasRect& r : c.rects) {
        drawn.push_back({graphics::op::kRect, r.x, r.y, r.w, r.h, r.role, nullptr});
    }
    for (const PaneCanvasLabel& l : c.labels) {
        drawn.push_back({graphics::op::kText, l.x, l.y, across(l.text.size(), kPaneCanvasUnit),
                         kPaneCanvasUnit, l.role, &l.text});
    }
    for (const PaneCanvasText& t : c.texts) {
        drawn.push_back({graphics::op::kText, t.x, t.y, across(t.text.size(), metrics.advance),
                         metrics.line + 2 * metrics.inset, t.role, &t.text, metrics.inset});
    }
    const auto meets = [](const Drawn& d, const PaneCanvasPart& p) {
        return d.x < p.x + p.w && p.x < d.x + d.w && d.y < p.y + p.h && p.y < d.y + d.h;
    };

    // What a window would keep, judged before anything is drawn: two groups, the picture, and
    // each named part's group with what it paints again, counted no further than a bound.
    const auto pressed = [](const PaneCanvasPart& p) { return !p.name.empty() && p.w > 0 && p.h > 0; };
    std::size_t nodes = 3 + drawn.size(), records = 0;
    for (const PaneCanvasPart& p : c.parts) records += pressed(p) ? std::size_t{1} : std::size_t{0};
    for (auto p = c.parts.begin(); p != c.parts.end() && records <= graphics::kMaxElementsPerArea &&
                                   nodes <= graphics::kMaxNodesPerArea;
         ++p) {
        if (!pressed(*p)) continue;
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
    for (const PaneCanvasPart& p : c.parts) {
        if (!pressed(p)) continue;
        const std::string group = "parts/" + std::to_string(at++);
        out.sketch.group(group, p.x, p.y, p.w, p.h);
        out.sketch.action(group, p.name);
        std::size_t k = 0;
        for (const Drawn& d : drawn) {
            if (meets(d, p)) put(group + "/" + std::to_string(k++), d, p.x, p.y);
        }
    }
    return out;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_CANVAS_DRAWING_HPP
