// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_VIEW_HPP
#define ZENGINE_FLOW_PANE_VIEW_HPP
// Flow's picture (flow/docs/zengine.flow.md, "Shows"): the bar -- Run, Apply and Stop, the rule's
// state, the File and View menus and the gear -- the pages, and on the graph page the rule read as
// one sentence, the rail that finds what to compose, the graph in three lanes (When docked at the
// left, Then at the right, Do the open canvas between), the Next line and the Events. Laid out in
// the room's own measures, so one picture serves a window of light and a terminal alike.
#include "flow/flow-pane/find.hpp"
#include "flow/flow-pane/look.hpp"
#include "flow/flow-pane/model.hpp"
#include "flow/flow-pane/next.hpp"
#include "flow/flow-pane/observed.hpp"
#include "flow/flow-pane/vocabulary.hpp"
#include "graphics/pixels.hpp"
#include "graphics/role.hpp"
#include "workshop/pane_canvas_drawing.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/pane_parts.hpp"
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace zengine::flow_pane {
/// Flow's authored grid: `unit` to one character of the medium's text, the unit its saved node
/// places and its pan are in. Presentation scales it to the room's pixels (`GridProjection`).
inline constexpr std::int64_t unit = 48;
/// The place that stands at the Do canvas's corner while the view is not panned: where the first
/// node a trigger is given is placed across, and two rows above it.
inline constexpr std::int64_t kPlaceX = 1056, kPlaceY = 96;

// Workspace coordinates use an authored square grid. Only presentation spends
// the medium's independent horizontal advance and padded text-row height.
struct GridProjection {
    std::int64_t advance = unit, row = unit, padding = 0;
    explicit GridProjection(const ws::CanvasRoom& room) {
        const auto metrics = ws::canvas_text_metrics(room);
        advance = std::max<std::int64_t>(1, metrics.advance);
        row = std::max<std::int64_t>(1, gfx::add_cells(metrics.line, 2 * metrics.inset));
        const auto pad = 2 * metrics.inset * unit;
        padding = pad / advance + (pad % advance != 0 ? 1 : 0);
    }
    static std::int64_t scaled(std::int64_t value, std::int64_t factor, std::int64_t divisor) {
        // Saturation only affects coordinates already far outside an ordinary room.
        // A hostile metric must not turn clipping into signed multiplication overflow.
        if (value >= 0) return gfx::mul_px(value, factor) / divisor;
        const auto magnitude = value == (std::numeric_limits<std::int64_t>::min)()
                                   ? (std::numeric_limits<std::int64_t>::max)()
                                   : -value;
        return -gfx::mul_px(magnitude, factor) / divisor;
    }
    std::int64_t x(std::int64_t value) const { return scaled(value, advance, unit); }
    std::int64_t y(std::int64_t value) const { return scaled(value, row, unit); }
    std::int64_t grid_x(std::int64_t value) const { return scaled(value, unit, advance); }
    std::int64_t grid_y(std::int64_t value) const { return scaled(value, unit, row); }
};

/// FLOW'S PICTURE: what it draws and where a press means something (`Drawing`), the draft's
/// revision it shows, and on the graph page the Do canvas and the graph's box in the area's pixels,
/// the view it shows the canvas at, and where the selected node and the whole graph stand there.
struct Picture : Drawing {
    std::int64_t revision = 0;
    std::optional<Box> canvas, graph;
    std::int64_t pan_x = 0, pan_y = 0, zoom = 100;
    std::optional<std::pair<std::int64_t, std::int64_t>> node_extent, graph_extent;
    /// Where the When and Then columns stand under their heads, and each card in them by its name.
    std::optional<Box> docks;
    std::map<std::string, Box> cards;
    /// Where the chosen port stands on its step's card, and that card, for the box that opens there.
    std::optional<std::pair<std::int64_t, std::int64_t>> port_at;
    std::optional<Box> port_card;
    /// What each editor in place edits on the graph, by its action: where one opens that nothing
    /// pressed opened.
    std::map<std::string, Box> subjects;
    /// WHERE THE POINTER CAN REST, each by the key `Model::hover` keeps (a later one over an earlier),
    /// each wire by the points of its line, a pointer within `reach` pixels of one resting on it, and
    /// every key drawn lit.
    struct Rest {
        std::string key;
        Box box;
    };
    struct Line {
        std::string key;
        std::vector<std::pair<std::int64_t, std::int64_t>> points;
    };
    std::vector<Rest> rests;
    std::vector<Line> lines;
    std::int64_t reach = 1;
    std::set<std::string> lit;
    /// Every key drawn lit for what the last message changed.
    std::set<std::string> happened;
    /// Where the box of what resting tells stands, a pointer inside it resting on nothing.
    std::optional<Box> tip;
    /// Every run of text it draws, in paint order.
    std::vector<std::string> texts() const {
        std::vector<std::string> out;
        for (const auto& k : marks)
            if (k.op == Mark::Op::Text) out.push_back(k.words);
        return out;
    }
};

/// WHAT THE POINTER RESTS ON at (x, y): a port, a field's row or a step's answer first, the
/// smallest places; then a wire within reach; then whatever rests on top there. None for nothing.
inline std::optional<std::string> rest_at(const Picture& p, std::int64_t x, std::int64_t y) {
    const auto inside = [&](const Box& b) { return x >= b.x && x < b.right() && y >= b.y && y < b.bottom(); };
    if (p.tip && inside(*p.tip)) return std::nullopt;
    const auto port = [](const std::string& key) {
        return key.rfind("port:", 0) == 0 || key.rfind("answer:", 0) == 0 || key.rfind("field:", 0) == 0;
    };
    for (auto it = p.rests.rbegin(); it != p.rests.rend(); ++it)
        if (port(it->key) && inside(it->box)) return it->key;
    const auto px = static_cast<double>(x), py = static_cast<double>(y);
    const auto reach = static_cast<double>(p.reach);
    for (const auto& line : p.lines)
        for (std::size_t i = 1; i < line.points.size(); ++i) {
            const auto& [x0, y0] = line.points[i - 1];
            const auto& [x1, y1] = line.points[i];
            const auto ax = static_cast<double>(x0), ay = static_cast<double>(y0);
            const auto bx = static_cast<double>(x1), by = static_cast<double>(y1);
            const auto dx = bx - ax, dy = by - ay, length = dx * dx + dy * dy;
            const auto t = length > 0 ? std::clamp(((px - ax) * dx + (py - ay) * dy) / length, 0.0, 1.0) : 0.0;
            const auto ex = ax + t * dx - px, ey = ay + t * dy - py;
            if (ex * ex + ey * ey <= reach * reach) return line.key;
        }
    for (auto it = p.rests.rbegin(); it != p.rests.rend(); ++it)
        if (inside(it->box)) return it->key;
    return std::nullopt;
}

/// The most a picture draws before it is the bounded recovery picture instead.
inline constexpr std::size_t kMaxMarks = 2048;
inline constexpr std::size_t kMaxMarkTextBytes = 131072;

/// WHERE A NODE'S PLACE STANDS in a picture's pixels, and the place a point of the canvas stands for.
/// The canvas's origin: three characters in from its left edge, so a node placed there stands clear
/// of the When lane's ports.
inline std::int64_t canvas_left(const Picture& p, const GridProjection& grid) {
    return gfx::add_cells(p.canvas.value_or(Box{}).x, grid.x(3 * unit));
}
inline std::pair<std::int64_t, std::int64_t> point_of(const Picture& p, const GridProjection& grid,
                                                      std::int64_t x, std::int64_t y) {
    const Box c = p.canvas.value_or(Box{});
    const auto scale = [&](std::int64_t v) { return GridProjection::scaled(v, p.zoom, 100); };
    return {gfx::add_cells(canvas_left(p, grid), grid.x(gfx::add_cells(scale(gfx::sub_px(x, kPlaceX)), p.pan_x))),
            gfx::add_cells(c.y, grid.y(gfx::add_cells(scale(gfx::sub_px(y, kPlaceY)), p.pan_y)))};
}
inline std::pair<std::int64_t, std::int64_t> place_at(const Picture& p, const ws::CanvasRoom& room,
                                                      std::int64_t x, std::int64_t y) {
    const GridProjection grid(room);
    const auto zoom = std::max<std::int64_t>(1, p.zoom);
    const Box c = p.canvas.value_or(Box{});
    const auto bound = [](std::int64_t v) { return std::clamp<std::int64_t>(v, -10000000, 10000000); };
    return {bound((grid.grid_x(x - canvas_left(p, grid)) - p.pan_x) * 100 / zoom + kPlaceX),
            bound((grid.grid_y(y - c.y) - p.pan_y) * 100 / zoom + kPlaceY)};
}

namespace view_detail {

/// A CONTROL'S FACE: plain, gold (what to do now), outlined in gold or in the kept colour, or quiet.
enum class Face { Plain, Gold, Gilt, Teal, Quiet };
enum class Icon { None, Play, Square, Chevron, Gear };

/// WHAT DRAWS A PICTURE: its marks and places, laid out in the room's measures.
struct Pen {
    Picture& p;
    Measure m;
    const ws::CanvasRoom& room;
    void rect(Box b, gfx::Paint fill, gfx::Paint stroke = gfx::no_paint(), std::int64_t radius = 0,
              bool dashed = false, std::optional<Box> clip = std::nullopt) {
        if (b.empty()) return;
        Mark k;
        k.op = Mark::Op::Rect;
        k.x = b.x; k.y = b.y; k.w = b.w; k.h = b.h;
        k.fill = fill; k.stroke = stroke; k.radius = radius; k.dashed = dashed; k.clip = clip;
        p.marks.push_back(std::move(k));
    }
    void dot(Box b, gfx::Paint fill, gfx::Paint stroke = gfx::no_paint(),
             std::optional<Box> clip = std::nullopt) {
        if (b.empty()) return;
        Mark k;
        k.op = Mark::Op::Ellipse;
        k.x = b.x; k.y = b.y; k.w = b.w; k.h = b.h;
        k.fill = fill; k.stroke = stroke; k.clip = clip;
        p.marks.push_back(std::move(k));
    }
    void line(std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1, gfx::Paint ink,
              std::optional<Box> clip = std::nullopt) {
        Mark k;
        k.op = Mark::Op::Line;
        k.x = x0; k.y = y0; k.w = x1 - x0; k.h = y1 - y0;
        k.stroke = ink; k.clip = clip;
        p.marks.push_back(std::move(k));
    }
    void path(std::string d, Box bounds, gfx::Paint fill, gfx::Paint stroke,
              std::optional<Box> clip = std::nullopt) {
        Mark k;
        k.op = Mark::Op::Path;
        k.x = bounds.x; k.y = bounds.y; k.w = bounds.w; k.h = bounds.h;
        k.words = std::move(d); k.fill = fill; k.stroke = stroke; k.clip = clip;
        p.marks.push_back(std::move(k));
    }
    /// A run of text, its padded region at (x, y); the mark, to give it a caret.
    Mark& text(std::int64_t x, std::int64_t y, std::string words, gfx::Paint ink,
               std::optional<Box> clip = std::nullopt) {
        Mark k;
        k.op = Mark::Op::Text;
        k.x = x; k.y = y;
        k.words = clean(std::move(words));
        k.w = m.words(k.words.size()); k.h = m.row;
        k.fill = ink; k.clip = clip;
        p.marks.push_back(std::move(k));
        return p.marks.back();
    }
    std::int64_t width(const std::string& words) const { return m.words(words.size()); }
    void press(Box b, std::string action, std::vector<std::string> args = {}, std::int64_t subject = 0) {
        if (b.empty()) return;
        p.hits.push_back({b.x, b.y, b.w, b.h, std::move(action), std::move(args), subject});
    }
    /// The words a control shows: its label, in brackets where a character is a cell.
    std::string label(const std::string& words, Icon icon) const {
        if (m.light) return words;
        if (icon == Icon::Gear) return "[More]";
        return "[" + words + (icon == Icon::Chevron ? " v" : "") + "]";
    }
    std::int64_t button_width(const std::string& words, Icon icon = Icon::None) const {
        const auto said = label(words, icon);
        if (!m.light) return width(said);
        if (icon == Icon::Gear) return m.tall;
        const std::int64_t mark = icon == Icon::Play || icon == Icon::Square ? 16 : icon == Icon::Chevron ? 12 : 0;
        return 20 + mark + width(said) - 2 * m.inset;
    }
    /// A CONTROL: its face where the room is light, its words in brackets where a character is a
    /// cell; the place it is pressed at; how wide it stands.
    std::int64_t button(std::int64_t x, std::int64_t y, const std::string& words, std::string action,
                        std::vector<std::string> args = {}, Face face = Face::Plain,
                        Icon icon = Icon::None, std::optional<Box> clip = std::nullopt) {
        const auto w = button_width(words, icon);
        const auto said = label(words, icon);
        gfx::Paint ink = hue::text(), rim = hue::edge(), fill = gfx::no_paint();
        switch (face) {
        case Face::Plain: break;
        case Face::Gold: fill = hue::gold(); rim = hue::of(ink::kAccent, 0xebcb8b); ink = hue::on_gold(); break;
        case Face::Gilt: rim = hue::message(); ink = hue::message(); break;
        case Face::Teal: rim = hue::kept(); ink = hue::kept(); break;
        case Face::Quiet: ink = hue::quiet(); break;
        }
        if (m.light) {
            rect({x, y, w, m.tall}, fill, rim, 4, false, clip);
            const auto ty = y + (m.tall - m.row) / 2;
            std::int64_t tx = x + 10;
            const auto cy = y + m.tall / 2;
            if (icon == Icon::Play) {
                path("M" + num(tx) + " " + num(cy - 5) + " L" + num(tx + 8) + " " + num(cy) + " L" +
                         num(tx) + " " + num(cy + 5) + " Z",
                     {tx, cy - 5, 9, 11}, ink, gfx::no_paint(), clip);
                tx += 16;
            } else if (icon == Icon::Square) {
                rect({tx, cy - 4, 8, 8}, ink, gfx::no_paint(), 0, false, clip);
                tx += 16;
            } else if (icon == Icon::Gear) {
                const auto gx = x + (w - 14) / 2, gy = y + (m.tall - 14) / 2;
                path(gear(gx, gy), {gx, gy, 15, 15}, gfx::no_paint(), ink, clip);
                dot({gx + 5, gy + 5, 5, 5}, gfx::no_paint(), ink, clip);
                press({x, y, w, m.tall}, std::move(action), std::move(args));
                return w;
            }
            text(tx - m.inset, ty, said, ink, clip);
            if (icon == Icon::Chevron) {
                const auto cx = tx - 2 * m.inset + width(said) + 4;
                path("M" + num(cx) + " " + num(cy - 2) + " L" + num(cx + 7) + " " + num(cy - 2) + " L" +
                         num(static_cast<double>(cx) + 3.5) + " " + num(cy + 2) + " Z",
                     {cx, cy - 2, 8, 5}, ink, gfx::no_paint(), clip);
            }
            press({x, y, w, m.tall}, std::move(action), std::move(args));
        } else {
            text(x, y, said, face == Face::Gold || face == Face::Gilt || face == Face::Teal ? hue::message() : ink,
                 clip);
            press({x, y, w, m.row}, std::move(action), std::move(args));
        }
        return w;
    }
};

/// THE ROWS OF A CARD: its box, where its head's words stand, and each row's words and middle.
struct CardRows {
    Box box;
    std::int64_t head = 0;
    std::vector<std::int64_t> rows, middles;
};
inline CardRows card_rows(const Measure& m, std::int64_t x, std::int64_t y, std::int64_t w, std::size_t n) {
    CardRows out;
    const auto count = static_cast<std::int64_t>(n);
    if (m.light) {
        out.box = {x, y, w, m.card * (count + 1)};
        out.head = y + (m.card - m.row) / 2;
        for (std::int64_t i = 0; i < count; ++i) {
            out.rows.push_back(y + m.card * (i + 1) + (m.card - m.row) / 2);
            out.middles.push_back(y + m.card * (i + 1) + m.card / 2);
        }
    } else {
        out.box = {x, y, w, m.row * (count + 2)};
        out.head = y;
        for (std::int64_t i = 0; i < count; ++i) {
            out.rows.push_back(y + m.row * (i + 1));
            out.middles.push_back(y + m.row * (i + 1) + m.row / 2);
        }
    }
    return out;
}
/// A CARD'S BODY: rounded, its head a band, where light; an outline in characters, its head's words
/// on its top edge, where a character is a cell.
inline void card(Pen& pen, const Box& b, gfx::Paint rim, std::optional<Box> clip) {
    if (pen.m.light) {
        pen.rect(b, hue::card(), rim, 4, false, clip);
        pen.rect({b.x + 1, b.y + 1, b.w - 2, pen.m.card - 1}, hue::band(), gfx::no_paint(), 3, false, clip);
        pen.rect({b.x + 1, b.y + pen.m.card / 2, b.w - 2, pen.m.card - pen.m.card / 2}, hue::band(),
                 gfx::no_paint(), 0, false, clip);
    } else {
        pen.rect(b, hue::well(), rim, 0, false, clip);
    }
}
/// A dashed place still to fill, where light, its dashes bright while lit; where a character is a
/// cell, its words alone.
inline void open_place(Pen& pen, const Box& b, std::optional<Box> clip, bool lit = false) {
    if (pen.m.light) pen.rect(b, gfx::no_paint(), lit ? hue::text() : hue::faint(), 4, true, clip);
}
/// A CARD OR A STEP LIT: a line round it, where light, bright for what the pointer rests on.
inline void glow(Pen& pen, const Box& b, std::optional<Box> clip, gfx::Paint ink = hue::text()) {
    if (pen.m.light) pen.rect({b.x - 2, b.y - 2, b.w + 4, b.h + 4}, gfx::no_paint(), ink, 6, false, clip);
}
/// A PORT: a dot at a card's edge -- a ring until wired, where light; an `o` on the edge where a
/// character is a cell -- bright, and ringed where light, while lit.
inline void port(Pen& pen, std::int64_t x, std::int64_t middle, gfx::Paint ink, bool filled,
                 std::optional<Box> clip, bool lit = false) {
    if (pen.m.light) {
        if (lit) pen.dot({x - 8, middle - 8, 16, 16}, gfx::no_paint(), hue::text(), clip);
        pen.dot({x - 5, middle - 5, 10, 10}, filled ? (lit ? hue::text() : ink) : hue::well(),
                lit ? hue::text() : ink, clip);
    } else {
        pen.text(x, middle - pen.m.row / 2, "o", lit ? hue::text() : ink, clip);
    }
}
/// THE POINTS OF A WIRE'S LINE as `wire` draws it, for the pointer to rest on: the curve's, where
/// light; its three runs' corners where a character is a cell.
inline std::vector<std::pair<std::int64_t, std::int64_t>> wire_points(const Measure& m, std::int64_t x0, std::int64_t y0,
                                                                      std::int64_t x1, std::int64_t y1) {
    std::vector<std::pair<std::int64_t, std::int64_t>> out;
    if (m.light) {
        const double mx = (static_cast<double>(x0) + static_cast<double>(x1)) / 2.0;
        for (int i = 0; i <= 24; ++i) {
            const double t = i / 24.0, u = 1.0 - t;
            const double x = u * u * u * static_cast<double>(x0) + 3 * u * u * t * mx + 3 * u * t * t * mx +
                             t * t * t * static_cast<double>(x1);
            const double y = (u * u * u + 3 * u * u * t) * static_cast<double>(y0) +
                             (3 * u * t * t + t * t * t) * static_cast<double>(y1);
            out.emplace_back(static_cast<std::int64_t>(x), static_cast<std::int64_t>(y));
        }
        return out;
    }
    const auto a = x0 + m.ch, z = x1 - m.ch;
    const auto mid = a + ((z - a) / 2 / m.ch) * m.ch;
    if (z > a) out = {{a, y0}, {mid, y0}, {mid, y1}, {z, y1}};
    return out;
}
/// A WIRE from an answer to a port, leaving and arriving level: a curve twice drawn, where light --
/// three times while lit -- and three straight runs where a character is a cell.
inline void wire(Pen& pen, std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1,
                 gfx::Paint ink, std::optional<Box> clip, bool lit = false) {
    if (pen.m.light) {
        const Box b{std::min(x0, x1), std::min(y0, y1) - 2, std::abs(x1 - x0) + 1, std::abs(y1 - y0) + 4};
        if (lit) pen.path(curve(x0, y0 - 1, x1, y1 - 1), b, gfx::no_paint(), ink, clip);
        pen.path(curve(x0, y0, x1, y1), b, gfx::no_paint(), ink, clip);
        pen.path(curve(x0, y0 + 1, x1, y1 + 1), b, gfx::no_paint(), ink, clip);
        return;
    }
    const auto a = x0 + pen.m.ch, z = x1 - pen.m.ch;
    const auto mid = a + ((z - a) / 2 / pen.m.ch) * pen.m.ch;
    if (z > a) {
        pen.line(a, y0, mid, y0, ink, clip);
        if (y0 != y1) pen.line(mid, y0, mid, y1, ink, clip);
        pen.line(mid, y1, z, y1, ink, clip);
    }
}
/// A VALUE ON A WIRE, at its middle: a pill, where light; in brackets where a character is a cell.
inline void badge(Pen& pen, std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1,
                  const std::string& value, std::optional<Box> clip) {
    const auto cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    if (pen.m.light) {
        const auto words = pen.width(value);
        const auto w = std::max<std::int64_t>(22, words + 6);
        pen.rect({cx - w / 2, cy - pen.m.row / 2, w, pen.m.row}, hue::badge(), gfx::no_paint(), pen.m.row / 2,
                 false, clip);
        pen.text(cx - words / 2, cy - pen.m.row / 2, value, hue::text(), clip);
    } else {
        const auto said = "(" + value + ")";
        pen.text(cx - pen.width(said) / 2, cy - pen.m.row / 2, said, hue::text(), clip);
    }
}

/// WHAT IS LIT: what the pointer rests on and what goes with it -- a part of the sentence and the
/// cards, steps and places it stands for; a card or a step and the parts that stand for it, every
/// kept field's part for the card of what it keeps -- each by its key, the parts by their place; and
/// what the last message changed (`changed_by_last`).
struct Lighting {
    std::set<std::string> pointed;
    std::set<std::size_t> chips;
    std::set<std::string> happened;
    bool on(const std::string& key) const { return pointed.count(key) != 0; }
    bool done(const std::string& key) const { return happened.count(key) != 0; }
};

/// WHAT THE LAST MESSAGE CHANGED, while the rule runs, against the message before it of the same
/// trigger, or everything where none came before: the card of the message that came; each wire whose
/// value changed -- a field of the message, a kept field as it was before, a step's answer where a
/// wire into that step changed -- and the steps they reach; what the rule writes where the value it
/// keeps changed, and its wire where the answer did; each card of a message it said. Each by the key
/// the picture lights it by; nothing while the rule is stopped or before a message reaches it.
inline std::set<std::string> changed_by_last(const Model& m) {
    std::set<std::string> out;
    const auto& def = m.workspace.graph.project.definition;
    const auto& seen = m.observed.arrivals;
    if (!m.running || def.on.empty() || seen.empty()) return out;
    const auto& on = def.on.at(m.trigger());
    const auto& last = seen.back();
    if (last.message != on.message->name()) return out;
    const Arrival* before = seen.size() > 1 ? &seen[seen.size() - 2] : nullptr;
    if (before && before->message != last.message) before = nullptr;
    const auto value = [](const std::optional<loom::Value>& v, const std::string& name) -> std::optional<std::string> {
        if (!v || !v->get(name)) return std::nullopt;
        return message_draft::summary(v->get(name));
    };
    out.insert("card:message:" + on.message->name());
    const auto& nodes = on.body.nodes;
    std::vector<bool> moved(nodes.size(), false);
    for (std::size_t n = 0; n < nodes.size(); ++n) {
        const auto id = std::to_string(place_of(m, n));
        for (std::size_t a = 0; a < nodes[n].arguments.size(); ++a) {
            const auto& b = nodes[n].arguments[a];
            bool now = false;
            if (b.from() == op::Binding::From::Input && !unwired(b)) {
                const auto& name = b.input_name();
                now = def.state->find(name) ? !before || value(before->before, name) != value(last.before, name)
                                            : !before || value(before->carried, name) != value(last.carried, name);
            } else if (b.from() == op::Binding::From::Node && b.node_index() < n) {
                now = moved[b.node_index()];
            }
            if (!now) continue;
            out.insert("wire:" + id + "." + std::to_string(a));
            moved[n] = true;
        }
        if (moved[n]) out.insert("step:" + id);
    }
    if (value(last.before, on.output) != value(last.after, on.output)) out.insert("card:write:" + on.message->name());
    if (!before || value(before->after, on.output) != value(last.after, on.output)) out.insert("wire:write");
    for (const auto& emit : on.emits)
        for (const auto& said : last.said)
            if (said.message == emit.message->name())
                out.insert("card:say:" + on.message->name() + ":" + emit.message->name());
    return out;
}
/// THE TWO ENDS OF A WIRE, by key: the answer or the field it leaves, and the port it reaches.
inline std::vector<std::string> wire_ends(const Model& m, const std::string& key) {
    const auto& def = m.workspace.graph.project.definition;
    if (def.on.empty() || key.rfind("wire:", 0) != 0) return {};
    const auto& on = def.on.at(m.trigger());
    const auto leaves = [&](const op::Binding& b) {
        return b.from() == op::Binding::From::Node ? "answer:" + std::to_string(place_of(m, b.node_index()))
                                                   : "field:" + b.input_name();
    };
    if (key == "wire:write")
        return {leaves(op::Binding::node(on.body.result_node)), std::string("port:write")};
    const auto dot = key.find('.');
    if (dot == std::string::npos) return {};
    try {
        const auto step = step_at(m, flow::integer(key.substr(5, dot - 5)));
        const auto port = flow::index_of(key.substr(dot + 1));
        if (!step || port >= on.body.nodes.at(*step).arguments.size()) return {};
        return {leaves(on.body.nodes[*step].arguments[port]), "port:" + key.substr(5)};
    } catch (const std::exception&) {
        return {};
    }
}
inline Lighting lighting(const Model& model, const std::vector<Chip>& chips) {
    Lighting out;
    out.happened = changed_by_last(model);
    if (!model.hover) return out;
    const auto& key = *model.hover;
    out.pointed.insert(key);
    if (key.rfind("wire:", 0) == 0) {
        for (const auto& end : wire_ends(model, key)) out.pointed.insert(end);
        return out;
    }
    if (key.rfind("chip:", 0) == 0) {
        std::size_t i = chips.size();
        try {
            i = flow::index_of(key.substr(5));
        } catch (const std::exception&) {
        }
        if (i < chips.size()) {
            out.chips.insert(i);
            out.pointed.insert(chips[i].subjects.begin(), chips[i].subjects.end());
        }
        return out;
    }
    for (std::size_t i = 0; i < chips.size(); ++i) {
        const auto& said = chips[i].subjects;
        if (std::find(said.begin(), said.end(), key) != said.end() ||
            (key == "card:kept" && chips[i].kind == Chip::Kind::Kept))
            out.chips.insert(i);
    }
    return out;
}

/// THE BAR: Run, Apply or Restart, and Stop, the rule's state beside them, and the File and View
/// menus and the gear at its right.
inline void bar(Pen& pen, const Model& model, std::int64_t height) {
    const auto& m = pen.m;
    const auto& def = model.workspace.graph.project.definition;
    const auto y = m.light ? (height - m.tall) / 2 : 0;
    const bool ready = !def.on.empty() && model.workspace.graph.problems(model.palette).empty();
    const bool restart = model.running && model.running_definition &&
                         !next_detail::restart_reason(def, *model.running_definition).empty();
    const bool changed = model.running && model.running_definition &&
                         model.running_definition->revision != def.revision;
    std::int64_t x = m.light ? 10 : 0;
    const auto step = m.light ? 8 : m.ch;
    x += pen.button(x, y, model.running ? "Running" : "Run", "run", {},
                    !model.running && ready ? Face::Gold : model.running ? Face::Quiet : Face::Plain, Icon::Play) + step;
    if (restart)
        x += pen.button(x, y, "Restart", "restart", {}, Face::Gilt) + step;
    else
        x += pen.button(x, y, "Apply", "apply", {}, changed ? Face::Plain : Face::Quiet) + step;
    x += pen.button(x, y, "Stop", "stop", {}, model.running ? Face::Plain : Face::Quiet, Icon::Square) + step;
    // THE MENUS AND THE GEAR, from the right.
    auto right = pen.p.width - (m.light ? 10 : 0);
    const auto gear_w = pen.button_width("", Icon::Gear);
    const auto view_w = pen.button_width("View", Icon::Chevron);
    const auto file_w = pen.button_width("File", Icon::Chevron);
    const auto menus = gear_w + view_w + file_w + 2 * step;
    if (right - menus > x) {
        right -= gear_w;
        pen.button(right, y, "", "menu", {"more"}, Face::Plain, Icon::Gear);
        right -= step + view_w;
        pen.button(right, y, "View", "menu", {"view"}, Face::Plain, Icon::Chevron);
        right -= step + file_w;
        pen.button(right, y, "File", "menu", {"file"}, Face::Plain, Icon::Chevron);
        right -= step;
    }
    // THE RULE'S STATE: its name, running or stopped, and whether what runs is behind the draft.
    std::vector<std::string> said{def.name};
    if (model.running && model.running_definition) {
        said.push_back("running");
        if (restart) said.push_back("changed, needs a restart");
        else if (changed) said.push_back("changed, not applied");
    } else {
        said.push_back("stopped");
    }
    const auto ink = model.running ? hue::done() : hue::quiet();
    if (right - x < m.chars(8)) return;
    const Box space{x, y, right - x, m.light ? m.tall : m.row};
    if (m.light) {
        std::int64_t w = 20;
        for (std::size_t i = 0; i < said.size(); ++i) w += pen.width(said[i]) - 2 * m.inset + (i > 0 ? 14 : 0);
        const Box chip{x, y + (m.tall - m.row - 2) / 2, std::min(w, space.w), m.row + 2};
        pen.rect(chip, hue::field(), gfx::no_paint(), chip.h / 2, false, space);
        std::int64_t tx = x + 10 - m.inset;
        for (std::size_t i = 0; i < said.size(); ++i) {
            if (i > 0) {
                pen.dot({tx + m.inset + 4, chip.y + chip.h / 2 - 1, 3, 3}, ink, gfx::no_paint(), chip);
                tx += 14;
            }
            pen.text(tx, chip.y + 1, said[i], ink, chip);
            tx += pen.width(said[i]) - 2 * m.inset;
        }
    } else {
        std::string line;
        for (std::size_t i = 0; i < said.size(); ++i) line += (i > 0 ? " - " : "") + said[i];
        pen.text(x, y, line, ink, space);
    }
}

/// THE PAGES, a tab each, the one shown underlined.
inline void tabs(Pen& pen, const Model& model, std::int64_t y, std::int64_t height) {
    const auto& m = pen.m;
    static constexpr std::pair<Page, const char*> kPages[] = {
        {Page::Graph, "Graph"}, {Page::State, "State"}, {Page::Messages, "Messages"},
        {Page::Library, "Library"}, {Page::Events, "Events"}};
    static constexpr const char* kActions[] = {"page-graph", "page-state", "page-messages",
                                               "page-library", "page-events"};
    std::int64_t x = m.light ? 8 : 0;
    for (std::size_t i = 0; i < std::size(kPages); ++i) {
        const bool on = model.page == kPages[i].first &&
                        !(model.dialog && !in_place(model.dialog->action)) && !model.dropped;
        const std::string words = kPages[i].second;
        if (m.light) {
            const auto w = pen.width(words) - 2 * m.inset + 20;
            pen.text(x + 10 - m.inset, y + (height - m.row) / 2, words, on ? hue::text() : hue::quiet());
            if (on) pen.rect({x, y + height - 3, w, 2}, hue::gold());
            pen.press({x, y, w, height}, kActions[i]);
            x += w + 2;
        } else {
            const auto said = "[" + words + "]";
            pen.text(x, y, said, on ? hue::message() : hue::text());
            pen.press({x, y, pen.width(said), m.row}, kActions[i]);
            x += pen.width(said) + m.ch;
        }
    }
    if (m.light) pen.rect({0, y + height - 1, pen.p.width, 1}, hue::rule());
}

/// THE RULE IN ONE SENTENCE, on a band of its own: each chosen part a chip of its colour, each part
/// still to choose dashed and pressed as the Next line's button is; a part lit, bright round it where
/// light and in angle brackets where a character is a cell.
inline void rule_row(Pen& pen, const Model& model, const Box& row, const Lighting& lit) {
    const auto& m = pen.m;
    if (m.light) {
        pen.rect(row, hue::band());
        pen.rect({row.x, row.bottom() - 1, row.w, 1}, hue::rule());
    }
    const Box inside{row.x + (m.light ? 12 : 0), row.y, row.w - (m.light ? 24 : 0), row.h};
    std::int64_t x = inside.x;
    const auto ty = row.y + (row.h - m.row) / 2;
    const auto say = [&](const std::string& words, gfx::Paint ink) {
        pen.text(x - (m.light ? m.inset : 0), ty, words, ink, inside);
        x += pen.width(words) - (m.light ? 2 * m.inset : 0) + (m.light ? 6 : m.ch);
    };
    say("The rule:", hue::quiet());
    const auto chips = sentence(model);
    for (std::size_t i = 0; i < chips.size(); ++i) {
        const auto& chip = chips[i];
        if (chip.kind == Chip::Kind::Plain) {
            say(chip.text, hue::text());
            continue;
        }
        const bool on = lit.chips.count(i) != 0;
        const bool to_choose = chip.kind == Chip::Kind::Open || chip.kind == Chip::Kind::StepOpen;
        const auto key = "chip:" + std::to_string(i);
        if (on) pen.p.lit.insert(key);
        gfx::Paint fill = gfx::no_paint(), rim = hue::faint(), ink = hue::quiet();
        bool dashed = false;
        switch (chip.kind) {
        case Chip::Kind::Message: fill = hue::message_chip(); rim = hue::message_rim(); ink = hue::message(); break;
        case Chip::Kind::Kept: fill = hue::kept_chip(); rim = hue::kept_rim(); ink = hue::kept(); break;
        case Chip::Kind::Step: fill = hue::step_chip(); rim = hue::step_rim(); ink = hue::wire(); break;
        case Chip::Kind::StepOpen: rim = hue::wire(); ink = hue::wire(); dashed = true; break;
        case Chip::Kind::Open: dashed = true; break;
        case Chip::Kind::Plain: break;
        }
        Box at;
        if (m.light) {
            const auto w = pen.width(chip.text) - 2 * m.inset + 12;
            at = Box{x, ty - 1, w, m.row + 2};
            if (on) pen.rect({at.x - 2, at.y - 2, at.w + 4, at.h + 4}, gfx::no_paint(), hue::text(), 5, false, inside);
            pen.rect(at, fill, rim, 3, dashed, inside);
            pen.text(x + 6 - m.inset, ty, chip.text, ink, inside);
            x += w + 6;
        } else {
            const auto said = (on ? "<" : "[") + chip.text + (on ? ">" : "]");
            at = Box{x, ty, pen.width(said), m.row};
            pen.text(x, ty, said, chip.kind == Chip::Kind::Open ? hue::quiet() : hue::message(), inside);
            x += pen.width(said) + m.ch;
        }
        at = at.cut(inside);
        pen.p.rests.push_back({key, at});
        if (to_choose) pen.press(at, "chip", {std::to_string(i)});
    }
}

/// The door's rows as the rail lists them: each operator under its name's first word, then the
/// Sources, the Conversions and the Forms -- with a port selected, the Sources first, which fill a
/// port from nothing; a row a power, as the door answered it.
struct RailRow {
    std::string heading;           ///< a heading's words, or none for a power's row
    const workshop::PowerRow* row = nullptr;
};
inline std::vector<RailRow> rail_rows(const Model& model) {
    std::vector<RailRow> out;
    const auto& rows = model.discovered.rows;
    std::vector<std::string> spaces;
    for (const auto& r : rows) {
        if (r.kind != workshop::kOperatorKind) continue;
        const auto dot = r.identity.find('.');
        const auto space = dot == std::string::npos ? std::string("other") : r.identity.substr(0, dot);
        if (std::find(spaces.begin(), spaces.end(), space) == spaces.end()) spaces.push_back(space);
    }
    const auto upper = [](std::string s) {
        for (auto& c : s)
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        return s;
    };
    const auto kind_group = [&](const char* kind, const char* heading) {
        bool headed = false;
        for (const auto& r : rows) {
            if (r.kind != kind) continue;
            if (!headed) out.push_back({heading, nullptr});
            headed = true;
            out.push_back({{}, &r});
        }
    };
    const bool filling = selected_port(model).has_value();
    if (filling) kind_group(workshop::kSourceKind, "SOURCES");
    for (const auto& space : spaces) {
        out.push_back({upper(space), nullptr});
        for (const auto& r : rows) {
            if (r.kind != workshop::kOperatorKind) continue;
            const auto dot = r.identity.find('.');
            if ((dot == std::string::npos ? std::string("other") : r.identity.substr(0, dot)) == space)
                out.push_back({{}, &r});
        }
    }
    if (!filling) kind_group(workshop::kSourceKind, "SOURCES");
    kind_group(workshop::kConversionKind, "CONVERSIONS");
    kind_group(workshop::kFormKind, "FORMS");
    return out;
}
/// WORDS BROKEN TO `columns`, at a space where one stands past half the line, in at most `most`
/// lines: the last holds whatever is left, for its place to cut.
inline std::vector<std::string> wrapped(std::string rest, std::size_t columns, std::size_t most) {
    std::vector<std::string> out;
    for (std::size_t n = 0; !rest.empty() && n < most; ++n) {
        if (rest.size() <= columns || n + 1 == most) {
            out.push_back(rest);
            break;
        }
        auto cut = rest.rfind(' ', columns);
        if (cut == std::string::npos || cut < columns / 2) cut = columns;
        out.push_back(rest.substr(0, cut));
        rest = rest.substr(cut < rest.size() && rest[cut] == ' ' ? cut + 1 : cut);
    }
    return out;
}

/// THE RAIL: find by what it is for, what is in scope -- or, with a port selected, what could fill
/// it, or with a wire in hand, what takes it -- the door's rows grouped, each one's words broken to
/// two lines, and the preview of the one chosen with what Add does.
inline void rail(Pen& pen, const Model& model, const Box& rail) {
    const auto& m = pen.m;
    const auto& graph = model.workspace.graph;
    const auto& def = graph.project.definition;
    const auto x = rail.x;
    std::int64_t y = rail.y;
    const auto line_h = m.light ? m.row : m.row;
    // THE SEARCH LINE, the one field outside a dialog: typing reaches it with no gesture.
    {
        auto search = model.search;
        if (m.light) {
            pen.text(x - m.inset, y + (m.tall - m.row) / 2, "find", hue::message(), rail);
            const auto bx = x + pen.width("find") + 4;
            const Box box{bx, y, rail.right() - bx, m.tall};
            pen.rect(box, hue::field(), hue::edge(), 3, false, rail);
            const auto columns = std::max<std::int64_t>(1, m.fits(box.w - 12));
            search.keep_caret_visible(columns);
            auto& run = pen.text(box.x + 6 - m.inset + 2, y + (m.tall - m.row) / 2, search.visible(columns),
                                 hue::text(), box);
            if (!model.wire_search) {
                run.caret_col = static_cast<std::int64_t>(search.caret_column());
                const auto selection = search.visible_selection(columns);
                if (selection.present()) {
                    run.sel_begin_col = selection.begin;
                    run.sel_end_col = selection.end;
                }
            }
            if (search.text().empty())
                pen.text(box.x + 6 - m.inset + 2 + m.ch, y + (m.tall - m.row) / 2, "what is it for? add, larger",
                         hue::faint(), box);
            y += m.tall + 10;
        } else {
            const std::string prefix = "find: ";
            const auto columns = std::max<std::int64_t>(1, m.fits(rail.w) - static_cast<std::int64_t>(prefix.size()) - 1);
            search.keep_caret_visible(columns);
            const bool after = search.caret_column() >= search.visible(columns).size();
            auto& run = pen.text(x, y, prefix + search.visible(columns) + (after ? " " : ""), hue::text(), rail);
            if (!model.wire_search) {
                const auto prefix_columns = static_cast<std::int64_t>(prefix.size());
                run.caret_col = prefix_columns + static_cast<std::int64_t>(search.caret_column());
                const auto selection = search.visible_selection(columns);
                if (selection.present()) {
                    run.sel_begin_col = prefix_columns + selection.begin;
                    run.sel_end_col = prefix_columns + selection.end;
                }
            }
            y += m.row;
        }
    }
    // A heading stands in the list, which keeps it above the preview; it breaks to two lines.
    const auto heading_columns = static_cast<std::size_t>(std::max<std::int64_t>(8, rail.w / m.ch));
    const auto heading = [&](const std::string& words, gfx::Paint ink = hue::message()) {
        for (const auto& line : wrapped(words, heading_columns, 2)) {
            pen.text(x - (m.light ? m.inset : 0), y, line, ink);
            y += line_h;
        }
    };
    const auto held = held_wire(model);
    // WHAT THE PREVIEW TAKES at the rail's foot, so the list above it stops there.
    const auto* shown = previewed(model);
    const auto slot = body_slot_node(model);
    const auto chosen_port = selected_port(model);
    std::vector<std::pair<std::string, std::vector<std::string>>> offers; // label, then action and args
    std::string note;
    if (shown) {
        if (def.on.empty()) {
            note = "A rule needs its message and what it keeps before its first step.";
        } else if (shown->kind == workshop::kFormKind) {
            offers.push_back({"Add to the rule", {"add-fold"}});
            if (model.node && *model.node < def.on.at(model.trigger()).body.nodes.size())
                offers.push_back({"Add before " + step_name(def.on.at(model.trigger()).body.nodes, *model.node),
                                  {"add-fold", std::to_string(*model.node)}});
        } else if (slot) {
            auto in = static_cast<std::int64_t>(shown->inputs.content_id);
            auto out = static_cast<std::int64_t>(shown->outputs.content_id);
            if (model.slot_reference && model.slot_reference->slot == *model.body_slot &&
                model.slot_reference->ref.identity == shown->identity) {
                in = static_cast<std::int64_t>(model.slot_reference->ref.authored_in);
                out = static_cast<std::int64_t>(model.slot_reference->ref.authored_out);
            }
            for (const auto& choice : fold_choices(model, *shown))
                offers.push_back({"count " + choice.count + ", acc " + choice.accumulator,
                                  {"fold-body", std::to_string(*slot), shown->identity, std::to_string(in),
                                   std::to_string(out), choice.count, choice.accumulator}});
        } else {
            offers.push_back({"Add to the rule", {"add-found", shown->kind, shown->identity}});
            if (chosen_port)
                offers.push_back({"Add into " + port_words(model, *chosen_port),
                                  {"add-into", shown->kind, shown->identity, std::to_string(chosen_port->node),
                                   std::to_string(chosen_port->port)}});
            else if (held)
                // A WIRE IN HAND: the step added after what it reads, wired at a port of its type.
                for (const auto& [port, name] : ports_taking(model, *shown, held->type))
                    offers.push_back({"Add, wired into " + name,
                                      {"add-from", shown->kind, shown->identity, std::to_string(port)}});
            else if (model.node && *model.node < def.on.at(model.trigger()).body.nodes.size())
                offers.push_back({"Add before " + step_name(def.on.at(model.trigger()).body.nodes, *model.node),
                                  {"add-found", shown->kind, shown->identity, std::to_string(*model.node)}});
        }
    }
    // ITS LINES, each wrapped to the rail: what it is and whose, its ports, what it is for.
    std::int64_t preview_h = 0;
    std::vector<std::pair<std::string, gfx::Paint>> said;
    if (shown) {
        const auto columns = static_cast<std::size_t>(std::max<std::int64_t>(8, m.fits(rail.w - (m.light ? 16 : 0))));
        const auto wrap = [&](const std::string& words, gfx::Paint ink, std::size_t most) {
            for (auto& line : wrapped(words, columns, most)) said.emplace_back(std::move(line), ink);
        };
        wrap(classification_of(*shown), hue::quiet(), 2);
        wrap(shown->signature, hue::wire(), 2);
        wrap(shown->about.empty() ? std::string("(its contributor says nothing of what it is for)") : shown->about,
             shown->about.empty() ? hue::quiet() : hue::text(), 3);
        if (!note.empty()) wrap(note, hue::quiet(), 3);
        const auto lines = static_cast<std::int64_t>(1 + said.size());
        preview_h = lines * line_h + static_cast<std::int64_t>(offers.size()) * (m.light ? m.tall + 4 : m.row) +
                    (m.light ? 20 : 0);
        preview_h = std::min(preview_h, std::max<std::int64_t>(0, rail.bottom() - y - 2 * line_h));
    }
    const Box list{rail.x, y, rail.w, std::max<std::int64_t>(0, rail.bottom() - preview_h - y - (shown ? 8 : 0))};
    const auto first_list = pen.p.marks.size();
    if (slot) {
        // A FOLD'S BODY SLOT lists what a fold could spend: one answer, an Int port for the count
        // and another of the answer's type for the accumulator. The preview says which port is which.
        heading("For " + step_name(def.on.at(model.trigger()).body.nodes, *slot) + "'s body");
        pen.text(x - (m.light ? m.inset : 0), y, "(acc: T, count: Int) -> T", hue::quiet(), list);
        y += line_h;
    } else if (chosen_port) {
        // A SELECTED PORT lists what could fill it, in the order a maker reaches for it: what is in
        // scope, a typed constant, then the door's Sources and operators that yield its type.
        const auto n = std::to_string(chosen_port->node), a = std::to_string(chosen_port->port);
        heading("For " + port_words(model, *chosen_port) + " : " + chosen_port->type);
        heading("In scope");
        const auto scope = in_scope(model, *chosen_port);
        const auto indent = [&](const std::string& words) { return m.light ? words : "  " + words; };
        const auto ix = x + (m.light ? 8 - m.inset : 0);
        for (const auto& candidate : scope) {
            if (y >= list.bottom()) break;
            const auto name = indent(candidate.name);
            const auto ink = candidate.from == InScope::From::Kept ? hue::kept() : hue::wire();
            pen.text(ix, y, name, ink, list);
            pen.text(ix + pen.width(name) - (m.light ? 2 * m.inset : 0) + m.ch, y, whence(candidate), hue::quiet(), list);
            pen.press(Box{rail.x, y, rail.w, line_h}.cut(list), "scope", {n, a, candidate.bind});
            y += line_h;
        }
        if (scope.empty() && y < list.bottom()) {
            pen.text(ix, y, indent("none of this type"), hue::quiet(), list);
            y += line_h;
        }
        if (takes_constant(*chosen_port) && y < list.bottom()) {
            heading("Constant");
            y += pen.button(x, y, std::string(loom::name_of(chosen_port->field.type.kind)) + " constant", "constant",
                            {n, a}, Face::Plain, Icon::None, list) > 0 ? (m.light ? m.tall + 4 : m.row) : 0;
        }
    } else if (held) {
        // A WIRE IN HAND lists what takes it: the door's Sources and operators with a port of its
        // type, each added wired from it in the preview.
        heading("Wire from " + held->words + " : " + held->type);
    } else if (!def.on.empty()) {
        heading("In scope");
        const auto& on = def.on.at(model.trigger());
        const auto row = [&](const loom::Field& f, bool kept) {
            if (y >= list.bottom()) return;
            const auto tx = x - (m.light ? m.inset : 0);
            auto at = tx;
            if (m.light) {
                pen.text(tx, y, f.name, kept ? hue::kept() : hue::wire(), list);
                at += pen.width(f.name) - 2 * m.inset;
                const std::string type = std::string(" : ") + loom::name_of(f.type.kind);
                pen.text(at, y, type, hue::text(), list);
                at += pen.width(type) - 2 * m.inset;
            } else {
                const auto spelled = f.name + " : " + loom::name_of(f.type.kind);
                pen.text(tx, y, spelled, kept ? hue::kept() : hue::wire(), list);
                at += pen.width(spelled);
            }
            pen.text(at, y, kept ? " kept" : " from the message", hue::quiet(), list);
            pen.press(Box{rail.x, y, rail.w, line_h}.cut(list), "source-field", {f.name});
            y += line_h;
        };
        for (const auto& f : on.message->fields())
            if (!def.state->find(f.name)) row(f, false);
        for (const auto& f : def.state->fields()) row(f, true);
    }
    y += m.light ? 8 : 0;
    // THE DOOR'S ROWS, grouped, in the order it gave them within a group: every power its
    // contributor offers. A row previews; the preview's Add places it.
    if (y < list.bottom()) heading("Operators");
    if (!model.discovered_read) {
        if (y < list.bottom()) heading("(asking the door)", hue::quiet());
    } else if (!model.discovered.ok) {
        if (y < list.bottom()) heading(model.discovered.reason, hue::warn());
    } else {
        const auto rows = rail_rows(model);
        // A row's words stand from 8 pixels in to 4 short of the rail's edge.
        const auto about_columns = static_cast<std::size_t>(std::max<std::int64_t>(8, (rail.w - 12) / m.ch));
        std::size_t skipped = 0;
        if (rows.empty() && y < list.bottom()) heading("none offered match", hue::quiet());
        for (const auto& r : rows) {
            if (y >= list.bottom()) break;
            if (skipped++ < model.first_row) continue;
            if (!r.row) {
                pen.text(x - (m.light ? m.inset : 0), y, r.heading, hue::quiet(), list);
                y += line_h;
                continue;
            }
            const bool on = is_previewed(model, *r.row);
            const auto composite = r.row->construction == ws::kCompositeConstruction ? " (composite)" : "";
            if (m.light) {
                // Its name, then what it is for broken to two lines, never cut on the first.
                const auto about = wrapped(r.row->about.empty() ? std::string("says nothing of what it is for")
                                                                : r.row->about,
                                           about_columns, 2);
                const auto h = static_cast<std::int64_t>(1 + about.size()) * m.row + 4;
                if (on) pen.rect({rail.x, y, rail.w, h}, hue::chosen(), hue::wire(), 3, false, list);
                pen.text(rail.x + 8 - m.inset, y + 2, power_name(r.row->identity) + composite, hue::text(), list);
                for (std::size_t k = 0; k < about.size(); ++k)
                    pen.text(rail.x + 8 - m.inset, y + 2 + m.row * static_cast<std::int64_t>(k + 1), about[k],
                             r.row->about.empty() ? hue::faint() : hue::quiet(), Box{rail.x, y, rail.w - 4, h}.cut(list));
                pen.press(Box{rail.x, y, rail.w, h}.cut(list), "found", {r.row->kind, r.row->identity});
                y += h + 2;
            } else {
                pen.text(x, y, "  " + power_name(r.row->identity) + composite, on ? hue::message() : hue::text(), list);
                pen.press(Box{rail.x, y, rail.w, line_h}.cut(list), "found", {r.row->kind, r.row->identity});
                y += line_h;
            }
        }
        const auto listed = static_cast<std::int64_t>(model.discovered.rows.size());
        if (model.discovered.total > listed && y < list.bottom())
            heading("+ " + std::to_string(model.discovered.total - listed) + " more match -- narrow the search",
                    hue::quiet());
    }
    for (auto i = first_list; i < pen.p.marks.size(); ++i)
        if (!pen.p.marks[i].clip) pen.p.marks[i].clip = list;
    if (!shown || preview_h <= 0) return;
    // THE PREVIEW: the door's row expanded -- what it is, its ports and what its contributor says
    // it is for -- and what Add does with it. Reading it evaluates nothing.
    const Box box{rail.x, rail.bottom() - preview_h, rail.w, preview_h};
    if (m.light) pen.rect(box, hue::band(), hue::frame(), 4);
    const auto px = box.x + (m.light ? 8 - m.inset : 0);
    std::int64_t py = box.y + (m.light ? 6 : 0);
    pen.text(px, py, power_name(shown->identity), m.light ? hue::text() : hue::message(), box);
    py += line_h;
    for (const auto& [words, ink] : said) {
        pen.text(px, py, words, ink, box);
        py += line_h;
    }
    for (const auto& [label, act] : offers) {
        if (py + (m.light ? m.tall : m.row) > box.bottom()) break;
        py += m.light ? 4 : 0;
        std::vector<std::string> args(act.begin() + 1, act.end());
        pen.button(box.x + (m.light ? 8 : 0), py, label, act.front(), std::move(args),
                   label == "Add to the rule" ? Face::Gold : Face::Plain, Icon::None, box);
        py += m.light ? m.tall : m.row;
    }
}

/// THE GRAPH IN THREE LANES: When docked at the left -- the message that wakes the rule and what it
/// keeps -- Then docked at the right -- what it writes and what it says -- and Do, the open canvas
/// between, where steps are placed. Wires carry values left to right; while the rule runs each
/// shows the last value it carried.
inline void lanes(Pen& pen, const Model& model, const Box& g, const Lighting& lit) {
    const auto& m = pen.m;
    const auto& graph = model.workspace.graph;
    const auto& def = graph.project.definition;
    const bool light = m.light;
    const maker::On* on = def.on.empty() ? nullptr : &def.on.at(model.trigger());
    const auto dock = light ? std::clamp<std::int64_t>(g.w * 30 / 100, 200, 252)
                            : std::clamp<std::int64_t>(g.w * 28 / 100, m.chars(12), m.chars(24));
    const Box when{g.x, g.y, dock, g.h}, then{g.right() - dock, g.y, dock, g.h};
    const Box lane{when.right(), g.y, then.x - when.right(), g.h};
    const auto head_h = light ? m.row + 12 : m.row;
    const Box canvas{lane.x, g.y + head_h, lane.w, g.h - head_h};
    pen.p.canvas = canvas;
    pen.p.graph = g;
    pen.p.pan_x = model.workspace.pan_x;
    pen.p.pan_y = model.workspace.pan_y;
    pen.p.zoom = model.workspace.zoom;
    if (light) {
        pen.rect(g, hue::well(), hue::rule(), 4);
        pen.rect({when.x + 1, g.y + 1, when.w - 4, g.h - 2}, hue::when_lane());
        pen.rect({lane.x + 2, g.y + 1, lane.w - 4, g.h - 2}, hue::do_lane());
        pen.rect({then.x + 3, g.y + 1, then.w - 4, g.h - 2}, hue::then_lane());
    }
    // THE SELECTED STEP'S CONTROL at the right of the DO lane's head -- Use as result while another
    // step is the result -- and each head's words where they stand clear of it.
    auto do_end = lane.right();
    if (on && model.node && *model.node < on->body.nodes.size() && on->body.result_node != *model.node) {
        const auto n = std::to_string(*model.node);
        auto right = lane.right() - (light ? 8 : m.ch);
        const auto y_head = g.y + (light ? (head_h - m.tall) / 2 : 0);
        right -= pen.button_width("Use as result");
        pen.button(right, y_head, "Use as result", "result", {n}, Face::Plain, Icon::None, lane);
        do_end = right - (light ? 6 : m.ch);
    }
    const auto header = [&](const Box& in, const std::string& word, gfx::Paint ink, const std::string& rest,
                            std::int64_t end) {
        const auto hx = in.x + (light ? 14 - m.inset : m.ch);
        const auto hy = g.y + (light ? 6 : 0);
        pen.text(hx, hy, word, ink, in);
        const auto rx = hx + pen.width(word) - (light ? 2 * m.inset : 0) + m.ch;
        if (rx + pen.width(rest) - (light ? m.inset : 0) <= end) pen.text(rx, hy, rest, hue::quiet(), in);
    };
    header(when, "WHEN", hue::message(), "a message arrives", when.right());
    header(lane, "DO", hue::wire(), "steps on its values", do_end);
    header(then, "THEN", hue::kept(), "what changes", then.right());
    const auto inset = light ? std::int64_t{14} : m.ch;
    const auto cw = dock - 2 * inset;
    const Box when_room{when.x, g.y + head_h, when.w, g.h - head_h};
    const Box then_room{then.x, g.y + head_h, then.w, g.h - head_h};
    // A CARD THE MAKER MOVED stands where they put it, down its own column and inside it; the rest
    // stand where the column stacks them. Its head chooses it, and a drag from there moves it.
    const GridProjection dock_grid(pen.room);
    pen.p.docks = Box{when.x, when_room.y, g.w, when_room.h};
    const auto stood_at = [&](const std::string& name, std::int64_t stacked, std::int64_t h, const Box& room) {
        const auto at = model.dock_y(name);
        if (!at) return stacked;
        return std::clamp(gfx::add_cells(room.y, dock_grid.y(*at)), room.y, std::max(room.y, room.bottom() - h));
    };
    const auto handle = [&](const CardRows& rows, const std::string& name, const Box& room) {
        pen.p.cards[name] = rows.box;
        pen.press(Box{rows.box.x, rows.box.y, rows.box.w, light ? m.card : m.row}.cut(room), "card", {name});
    };
    // A CARD'S RIM: bright while lit, the wire's colour while chosen; a card lit glows, and the
    // pointer rests on it.
    const auto edge_of = [&](const std::string& name) {
        if (lit.on("card:" + name)) return hue::text();
        if (lit.done("card:" + name)) return hue::done();
        return model.dock && *model.dock == name ? hue::wire() : hue::edge();
    };
    const auto card_at = [&](const CardRows& rows, const std::string& name, const Box& room) {
        pen.p.rests.push_back({"card:" + name, rows.box.cut(room)});
        if (lit.done("card:" + name)) pen.p.happened.insert("card:" + name);
        if (lit.on("card:" + name)) {
            pen.p.lit.insert("card:" + name);
            glow(pen, rows.box, room);
        } else if (lit.done("card:" + name)) {
            glow(pen, rows.box, room, hue::done());
        }
    };
    const auto place_at = [&](const std::string& name, const Box& box, const Box& room) {
        pen.p.rests.push_back({"place:" + name, box.cut(room)});
        if (lit.on("place:" + name)) pen.p.lit.insert("place:" + name);
        return lit.on("place:" + name);
    };
    pen.p.reach = light ? 5 : std::max<std::int64_t>(1, m.row / 2);
    const auto seen = carried(model.observed);
    const bool live = model.running && seen.known();
    // Where each wire leaves: a field of the message, a field kept, a step's answer.
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> from_field, from_kept, from_node;
    std::int64_t y = when_room.y + (light ? 12 : 0);
    // THE MESSAGE THAT WAKES IT, or the one declared to, or a place to drop one.
    const loom::Schema* message = on ? on->message.get() : def.accepts.empty() ? nullptr : def.accepts.front().get();
    if (!message) {
        const Box box{when.x + inset, y, cw, light ? 150 : m.row * 4};
        open_place(pen, box, when_room, place_at("message", box, when_room));
        pen.press(box.cut(when_room), "drop-zone", {"when"});
        // Centred where they fit; from the box's edge where they do not, so a cut takes their end.
        const auto cx = [&](const std::string& words) {
            return box.x + std::max<std::int64_t>(0, (box.w - pen.width(words)) / 2);
        };
        auto ty = box.y + (light ? 28 : 0);
        pen.text(cx("Drop a message here"), ty, "Drop a message here", hue::text(), when_room);
        ty += m.row;
        for (const char* words : {"a button's message", "starts a rule"}) {
            pen.text(cx(words), ty, words, hue::quiet(), when_room);
            ty += m.row;
        }
        ty += light ? 10 : 0;
        const auto bw = pen.button_width("New message");
        pen.button(box.x + std::max<std::int64_t>(0, (box.w - bw) / 2), ty, "New message", "ask-message", {}, Face::Gilt, Icon::None, when_room);
        pen.p.subjects["message"] = box;
        y = box.bottom() + (light ? 16 : m.row);
    } else {
        std::vector<loom::Field> fields;
        for (const auto& f : message->fields())
            if (!def.state->find(f.name)) fields.push_back(f);
        const auto name = "message:" + message->name();
        const auto stacked = card_rows(m, when.x + inset, y, cw, fields.size() + 1);
        const auto rows = card_rows(m, when.x + inset, stood_at(name, y, stacked.box.h, when_room), cw, fields.size() + 1);
        card(pen, rows.box, edge_of(name), when_room);
        card_at(rows, name, when_room);
        pen.text(rows.box.x + (light ? 8 - m.inset : m.ch), rows.head, message->name(), hue::message(), when_room);
        pen.text(rows.box.x + (light ? 8 - m.inset : m.ch), rows.rows[0],
                 on ? "wakes the rule" : "waits for a kept field", hue::quiet(), when_room);
        for (std::size_t i = 0; i < fields.size(); ++i) {
            const auto ry = rows.rows[i + 1];
            pen.text(rows.box.x + (light ? 8 - m.inset : m.ch), ry, fields[i].name, hue::wire(), when_room);
            const std::string type = loom::name_of(fields[i].type.kind);
            pen.text(rows.box.right() - pen.width(type) - (light ? 14 - m.inset : 2 * m.ch), ry, type, hue::quiet(), when_room);
            pen.press(Box{rows.box.x, ry, rows.box.w, m.row}.cut(when_room), "field", {fields[i].name});
            pen.p.rests.push_back({"field:" + fields[i].name, Box{rows.box.x, ry, rows.box.w, m.row}.cut(when_room)});
            from_field[fields[i].name] = {rows.box.right() - (light ? 0 : m.ch), rows.middles[i + 1]};
        }
        handle(rows, name, when_room);
        pen.p.subjects["message"] = rows.box;
        y = stacked.box.bottom() + (light ? 16 : 0);
    }
    // WHAT IT KEEPS BETWEEN MESSAGES, or a place to keep something.
    if (def.state->fields().empty()) {
        if (message) {
            const Box box{when.x + inset, y, cw, light ? 56 : m.row * 2};
            open_place(pen, box, when_room, place_at("keep", box, when_room));
            const auto bw = pen.button_width("+ keep a field");
            pen.button(box.x + std::max<std::int64_t>(0, (box.w - bw) / 2), box.y + (box.h - (light ? m.tall : m.row)) / 2, "+ keep a field",
                       "ask-keep", {}, Face::Teal, Icon::None, when_room);
            pen.p.subjects["keep"] = box;
            y = box.bottom() + (light ? 16 : m.row);
        }
    } else {
        const auto& fields = def.state->fields();
        const auto stacked = card_rows(m, when.x + inset, y, cw, fields.size());
        const auto rows = card_rows(m, when.x + inset, stood_at("kept", y, stacked.box.h, when_room), cw, fields.size());
        card(pen, rows.box, edge_of("kept"), when_room);
        card_at(rows, "kept", when_room);
        pen.text(rows.box.x + (light ? 8 - m.inset : m.ch), rows.head, "kept between messages", hue::quiet(), when_room);
        for (std::size_t i = 0; i < fields.size(); ++i) {
            const auto ry = rows.rows[i];
            pen.text(rows.box.x + (light ? 8 - m.inset : m.ch), ry, fields[i].name, hue::kept(), when_room);
            const auto* cell = graph.project.state.get(fields[i].name);
            const std::string type = std::string(loom::name_of(fields[i].type.kind)) +
                                     (cell ? " = " + message_draft::summary(cell) : std::string());
            pen.text(rows.box.right() - pen.width(type) - (light ? 14 - m.inset : 2 * m.ch), ry, type, hue::quiet(), when_room);
            pen.press(Box{rows.box.x, ry, rows.box.w, m.row}.cut(when_room), "field", {fields[i].name});
            pen.p.rests.push_back({"field:" + fields[i].name, Box{rows.box.x, ry, rows.box.w, m.row}.cut(when_room)});
            from_kept[fields[i].name] = {rows.box.right() - (light ? 0 : m.ch), rows.middles[i]};
        }
        handle(rows, "kept", when_room);
        pen.p.subjects["keep"] = rows.box;
        y = stacked.box.bottom() + (light ? 16 : m.row);
    }
    // THE OTHER MESSAGES THAT WAKE A TRIGGER, each to show in its place.
    if (def.on.size() > 1) {
        pen.text(when.x + inset - (light ? m.inset : 0), y, "also wakes on", hue::quiet(), when_room);
        y += m.row;
        for (std::size_t t = 0; t < def.on.size(); ++t) {
            if (t == model.trigger()) continue;
            pen.text(when.x + inset - (light ? m.inset : 0), y, def.on[t].message->name(), hue::message(), when_room);
            pen.press(Box{when.x, y, when.w, m.row}.cut(when_room), "select-trigger", {std::to_string(t)});
            y += m.row;
        }
    }
    const auto lit_port = [&](const std::string& key) {
        if (!lit.on(key)) return false;
        pen.p.lit.insert(key);
        return true;
    };
    for (const auto& [name, at] : from_field)
        port(pen, at.first, at.second, hue::wire(), true, when_room, lit_port("field:" + name));
    for (const auto& [name, at] : from_kept)
        port(pen, at.first, at.second, hue::kept(), true, when_room, lit_port("field:" + name));
    // WHAT IT WRITES AND WHAT IT SAYS.
    std::optional<std::pair<std::int64_t, std::int64_t>> write_port;
    y = then_room.y + (light ? 12 : 0);
    if (on) {
        const bool nodes = !on->body.nodes.empty();
        const auto write_name = "write:" + on->message->name();
        const auto write_stacked = card_rows(m, then.x + inset, y, cw, 1);
        const auto rows = card_rows(m, then.x + inset, stood_at(write_name, y, write_stacked.box.h, then_room), cw, 1);
        // Where each card it says stands: stacked after the write and its thread, or where it was put.
        const bool threaded = nodes || !on->emits.empty();
        const auto thread_h = light ? std::int64_t{26} : m.row;
        y = write_stacked.box.bottom() + (threaded ? thread_h : 0);
        std::vector<CardRows> says;
        std::vector<std::string> say_names;
        for (const auto& emit : on->emits) {
            say_names.push_back("say:" + on->message->name() + ":" + emit.message->name());
            const auto stacked = card_rows(m, then.x + inset, y, cw, 1);
            says.push_back(card_rows(m, then.x + inset, stood_at(say_names.back(), y, stacked.box.h, then_room), cw, 1));
            y = stacked.box.bottom() + (light ? 10 : 0);
        }
        if (nodes) {
            card(pen, rows.box, edge_of(write_name), then_room);
            card_at(rows, write_name, then_room);
        } else {
            open_place(pen, rows.box, then_room);
            if (!light) pen.rect(rows.box, hue::well(), hue::faint(), 0, false, then_room);
        }
        const auto tx = rows.box.x + (light ? 14 - m.inset : 2 * m.ch);
        pen.text(tx, rows.head, on->output, hue::kept(), then_room);
        pen.text(tx + pen.width(on->output) - (light ? 2 * m.inset : 0) + m.ch, rows.head,
                 nodes ? "becomes the answer" : "becomes ...", hue::text(), then_room);
        const auto* now = graph.project.state.get(on->output);
        pen.text(tx, rows.rows[0],
                 !nodes ? std::string("wire an answer here")
                 : model.running && now ? "now " + message_draft::summary(now)
                                        : std::string("when the rule runs"),
                 hue::quiet(), then_room);
        pen.press(rows.box.cut(then_room), "write", {std::to_string(model.trigger())});
        if (nodes) handle(rows, write_name, then_room);
        pen.p.subjects["say"] = rows.box;
        write_port = std::pair{rows.box.x, light ? rows.box.y + m.card / 2 : rows.box.y + m.row / 2};
        // THE THREAD from what it writes down to the first thing it says, where that stands below.
        if (threaded) {
            const auto top = rows.box.bottom();
            const auto end = says.empty() ? top + thread_h : says.front().box.y;
            if (end - top >= (light ? std::int64_t{8} : m.row)) {
                const auto thread_x = rows.box.x + (light ? 22 : m.ch);
                if (light) pen.rect({thread_x, top, 2, end - top}, hue::thread(), gfx::no_paint(), 0, false, then_room);
                pen.text(thread_x + (light ? 10 - m.inset : m.ch), top + (light ? 2 : 0), "then", hue::quiet(), then_room);
            }
        }
        for (std::size_t e = 0; e < on->emits.size(); ++e) {
            const auto& emit = on->emits[e];
            const auto& said = says[e];
            card(pen, said.box, edge_of(say_names[e]), then_room);
            card_at(said, say_names[e], then_room);
            const auto sx = said.box.x + (light ? 8 - m.inset : m.ch);
            pen.text(sx, said.head, "say", hue::text(), then_room);
            pen.text(sx + pen.width("say") - (light ? 2 * m.inset : 0) + m.ch, said.head, emit.message->name(),
                     hue::message(), then_room);
            std::string fields;
            const Said* heard = nullptr;
            if (model.running && !model.observed.arrivals.empty())
                for (const auto& s : model.observed.arrivals.back().said)
                    if (s.message == emit.message->name()) heard = &s;
            if (heard) {
                for (const auto& [name, value] : heard->fields) fields += (fields.empty() ? "" : ", ") + name + " = " + value;
            } else {
                for (const auto& f : emit.message->fields()) fields += (fields.empty() ? "" : ", ") + f.name;
                fields += ", after the write";
            }
            pen.text(sx, said.rows[0], fields, heard ? hue::text() : hue::quiet(), then_room);
            pen.press(said.box.cut(then_room), "say", {std::to_string(e)});
            handle(said, say_names[e], then_room);
            pen.p.subjects["say"] = said.box;
            const auto xw = pen.button_width("x");
            pen.button(said.box.right() - xw - (light ? 4 : 0), said.head - (light ? 3 : 0), "x", "emit-remove",
                       {std::to_string(e)}, Face::Quiet, Icon::None, then_room);
        }
        if (nodes && on->emits.empty()) {
            const Box box{then.x + inset, y, cw, light ? 56 : m.row * 2};
            open_place(pen, box, then_room);
            const auto label = "+ say " + def.name + "." + say_name(on->output);
            const auto bw = pen.button_width(label);
            pen.button(box.x + std::max<std::int64_t>(0, (box.w - bw) / 2), box.y + (box.h - (light ? m.tall : m.row)) / 2,
                       label, "ask-say", {}, Face::Gilt, Icon::None, then_room);
            pen.p.subjects["say"] = box;
        }
    }
    // THE STEPS ON THE OPEN CANVAS, each a card at its place.
    if (!on) return;
    const GridProjection grid(pen.room);
    const auto scale = [&](std::int64_t v) { return GridProjection::scaled(v, model.workspace.zoom, 100); };
    const auto node_w = std::max<std::int64_t>(m.chars(14), grid.x(scale(26 * unit)));
    const auto slot = body_slot_node(model);
    const auto chosen_port = selected_port(model);
    struct Placed {
        CardRows rows;
        std::int64_t id = 0;
        std::size_t extra = 0;
    };
    std::vector<Placed> placed(on->body.nodes.size());
    if (on->body.nodes.empty()) {
        const auto wide = m.chars(32); // the longer line's 28 characters, and room beside them
        const Box box{canvas.x + std::max<std::int64_t>(0, (canvas.w - wide) / 2), canvas.y + (light ? 60 : m.row),
                      std::min(canvas.w, wide), light ? 122 : m.row * 3};
        open_place(pen, box, canvas, place_at("do", box, canvas));
        // Centred where they fit; from the box's edge where they do not, so a cut takes their end.
        const auto cx = [&](const std::string& words) {
            return box.x + std::max<std::int64_t>(0, (box.w - pen.width(words)) / 2);
        };
        pen.text(cx("Find an operator in the rail"), box.y + (light ? 36 : 0), "Find an operator in the rail",
                 hue::text(), canvas);
        pen.text(cx("type what it is for: add"), box.y + (light ? 36 : 0) + m.row, "type what it is for: add",
                 hue::quiet(), canvas);
    }
    for (std::size_t n = 0; n < on->body.nodes.size(); ++n) {
        const auto& node = on->body.nodes[n];
        std::int64_t id = 0, px = 0, py = 0;
        for (const auto& place : graph.places)
            if (place.trigger == model.workspace.active_trigger && place.node == static_cast<std::int64_t>(n)) {
                id = place.id;
                std::tie(px, py) = point_of(pen.p, grid, place.x, place.y);
            }
        // A node stands on the room's device grain, so its words and rows round to none of its own.
        px = gfx::floor_div_px(px, m.grain) * m.grain;
        py = gfx::floor_div_px(py, m.grain) * m.grain;
        const std::size_t extra = node.fold ? 1 : 0;
        const auto rows = card_rows(m, px, py, node_w, node.arguments.size() + extra + 1);
        placed[n] = {rows, id, extra};
        const bool selected = model.node && *model.node == n;
        const auto bottom = rows.box.bottom();
        if (selected) pen.p.node_extent = std::pair{rows.box.y, bottom};
        pen.p.graph_extent = std::pair{pen.p.graph_extent ? std::min(pen.p.graph_extent->first, rows.box.y) : rows.box.y,
                                       pen.p.graph_extent ? std::max(pen.p.graph_extent->second, bottom) : bottom};
        // A node wholly off the canvas draws nothing and takes no press.
        if (!rows.box.meets(canvas)) continue;
        const Box inside = rows.box.cut(canvas);
        const auto step_key = "step:" + std::to_string(id);
        card(pen, rows.box,
             lit.on(step_key) ? hue::text() : lit.done(step_key) ? hue::done() : selected ? hue::wire() : hue::edge(),
             canvas);
        pen.p.rests.push_back({step_key, inside});
        if (lit.done(step_key)) pen.p.happened.insert(step_key);
        if (lit.on(step_key)) {
            pen.p.lit.insert(step_key);
            glow(pen, rows.box, canvas);
        } else if (lit.done(step_key)) {
            glow(pen, rows.box, canvas, hue::done());
        }
        const auto tx = rows.box.x + (light ? 8 - m.inset : m.ch);
        // THE CHOSEN STEP'S x on its corner takes it away; the head's words stop short of it.
        const auto xw = pen.button_width("x");
        const auto x_room = selected ? xw + (light ? 4 : 0) : 0;
        const Box head{rows.box.x, rows.box.y, rows.box.w - (light ? 4 : m.ch) - x_room, light ? m.card : m.row};
        pen.text(tx, rows.head, step_name(on->body.nodes, n), !light && selected ? hue::message() : hue::wire(),
                 head.cut(canvas));
        pen.press(Box{rows.box.x, rows.box.y, rows.box.w, light ? m.card : m.row}.cut(canvas), "node", {std::to_string(n)}, id);
        if (selected)
            pen.button(rows.box.right() - xw - (light ? 4 : 0), rows.head - (light ? 3 : 0), "x", "remove",
                       {std::to_string(n)}, Face::Quiet, Icon::None, canvas);
        if (node.fold) {
            const bool open = slot && *slot == n;
            const auto words = body_slot_text(node);
            pen.text(tx, rows.rows[0], light ? "body: " + (node.identity.empty() ? std::string("[choose]") : words) : words,
                     open ? hue::message() : hue::text(), inside);
            pen.press(Box{rows.box.x, rows.rows[0], rows.box.w, m.row}.cut(canvas), "body-slot", {std::to_string(n)}, id);
        }
        std::vector<std::string> names;
        try {
            // Held while its fields are read: a fold's ports are made afresh, and no one else holds them.
            const auto ports = graph.node_ports(model.trigger(), n, model.palette);
            for (const auto& f : ports.inputs->fields()) names.push_back(f.name);
        } catch (const std::exception&) {
        }
        for (std::size_t a = 0; a < node.arguments.size(); ++a) {
            const auto ry = rows.rows[a + extra];
            const auto& b = node.arguments[a];
            const bool held_over = model.wire_drawn && model.wire_drawn->lit && model.wire_drawn->lit->place == id &&
                                   model.wire_drawn->lit->port == a;
            const bool marked = held_over || (chosen_port && chosen_port->node == n && chosen_port->port == a);
            // A WIRE IN THE HAND THAT DOES NOT FIT the port under it marks it in the warning's colour.
            const auto mark = held_over && !model.wire_drawn->why.empty() ? hue::warn() : hue::message();
            const auto port_name = a < names.size() ? names[a] : std::to_string(a);
            if (light) {
                pen.text(tx + 6, ry, port_name, marked ? mark : hue::text(), inside);
                std::string from;
                if (unwired(b)) {
                    try {
                        from = loom::name_of(graph.node_ports(model.trigger(), n, model.palette).inputs->fields().at(a).type.kind);
                    } catch (const std::exception&) {
                        from = "?";
                    }
                } else {
                    from = "<- " + source_words(on->body.nodes, b);
                }
                pen.text(rows.box.right() - pen.width(from) - (8 - m.inset), ry, from, hue::quiet(), inside);
            } else {
                // Where a character is a cell, the port is the `o` its row begins with, on the edge.
                const auto reads = unwired(b) ? std::string(" [unwired]") : " <- " + source_words(on->body.nodes, b);
                pen.text(rows.box.x, ry, "o " + port_name + reads, marked ? mark : hue::text(), inside);
            }
            pen.press(Box{rows.box.x, ry, rows.box.w, m.row}.cut(canvas), "port", {std::to_string(n), std::to_string(a)}, id);
            pen.p.rests.push_back({"port:" + std::to_string(id) + "." + std::to_string(a),
                                   Box{rows.box.x, ry, rows.box.w, m.row}.cut(canvas)});
            if (chosen_port && chosen_port->node == n && chosen_port->port == a) {
                pen.p.port_at = std::pair{rows.box.x, rows.middles[a + extra]};
                pen.p.port_card = rows.box;
            }
        }
        const auto ay = rows.rows.back();
        if (light) {
            pen.text(tx, ay, "answer", hue::text(), inside);
            std::string type = "?";
            try {
                type = loom::name_of(graph.node_answer_type(model.trigger(), n, model.palette).kind);
            } catch (const std::exception&) {
            }
            pen.text(rows.box.right() - pen.width(type) - (14 - m.inset), ay, type, hue::quiet(), inside);
        } else {
            pen.text(tx, ay, on->body.result_node == n ? "[answer -> " + on->output + "]" : std::string("[answer]"),
                     hue::message(), inside);
        }
        pen.press(Box{rows.box.x, ay, rows.box.w, m.row}.cut(canvas), "source-node", {std::to_string(n)}, id);
        pen.p.rests.push_back({"answer:" + std::to_string(id), Box{rows.box.x, ay, rows.box.w, m.row}.cut(canvas)});
        from_node[std::to_string(n)] = {rows.box.right() - (light ? 0 : m.ch), rows.middles.back()};
    }
    // THE WIRES, and the value each last carried while the rule runs, then the ports over them.
    const auto value_of = [&](const op::Binding& b) -> std::optional<std::string> {
        if (!live) return std::nullopt;
        if (b.from() == op::Binding::From::Input) {
            const auto& name = b.input_name();
            const auto& in = def.state->find(name) ? seen.before : seen.message;
            if (const auto it = in.find(name); it != in.end()) return it->second;
        } else if (b.from() == op::Binding::From::Node && b.node_index() == on->body.result_node) {
            if (const auto it = seen.after.find(on->output); it != seen.after.end()) return it->second;
        }
        return std::nullopt;
    };
    for (std::size_t n = 0; n < on->body.nodes.size(); ++n) {
        const auto& node = on->body.nodes[n];
        const auto& rows = placed[n].rows;
        if (!rows.box.meets(canvas)) continue;
        for (std::size_t a = 0; a < node.arguments.size(); ++a) {
            const auto& b = node.arguments[a];
            if (unwired(b) || b.from() == op::Binding::From::Constant) continue;
            std::optional<std::pair<std::int64_t, std::int64_t>> from;
            gfx::Paint ink = hue::wire();
            if (b.from() == op::Binding::From::Node) {
                if (const auto it = from_node.find(std::to_string(b.node_index())); it != from_node.end()) from = it->second;
            } else if (def.state->find(b.input_name())) {
                if (const auto it = from_kept.find(b.input_name()); it != from_kept.end()) from = it->second;
                ink = hue::kept();
            } else if (const auto it = from_field.find(b.input_name()); it != from_field.end()) {
                from = it->second;
            }
            if (!from) continue;
            const auto to_x = rows.box.x + (light ? 0 : 0), to_y = rows.middles[a + placed[n].extra];
            // A WIRE LIT: its own, its step's, or the card it leaves's.
            const auto key = "wire:" + std::to_string(placed[n].id) + "." + std::to_string(a);
            const bool kept_wire = b.from() == op::Binding::From::Input && def.state->find(b.input_name());
            const bool field_wire = b.from() == op::Binding::From::Input && !kept_wire;
            const bool shine = lit.on(key) || lit.on("step:" + std::to_string(placed[n].id)) ||
                               (kept_wire && lit.on("card:kept")) ||
                               (field_wire && lit.on("card:message:" + on->message->name()));
            if (shine) pen.p.lit.insert(key);
            if (lit.done(key)) pen.p.happened.insert(key);
            pen.p.lines.push_back({key, wire_points(m, from->first, from->second, to_x, to_y)});
            wire(pen, from->first, from->second, to_x, to_y, shine ? hue::text() : lit.done(key) ? hue::done() : ink, g,
                 shine || lit.done(key));
            if (const auto v = value_of(b)) badge(pen, from->first, from->second, to_x, to_y, *v, g);
        }
    }
    if (write_port && !on->body.nodes.empty() && on->body.result_node < on->body.nodes.size()) {
        if (const auto it = from_node.find(std::to_string(on->body.result_node)); it != from_node.end()) {
            const bool shine = lit.on("wire:write") || lit.on("card:write:" + on->message->name());
            if (shine) pen.p.lit.insert("wire:write");
            if (lit.done("wire:write")) pen.p.happened.insert("wire:write");
            pen.p.lines.push_back(
                {"wire:write", wire_points(m, it->second.first, it->second.second, write_port->first, write_port->second)});
            wire(pen, it->second.first, it->second.second, write_port->first, write_port->second,
                 shine ? hue::text() : lit.done("wire:write") ? hue::done() : hue::wire(), g,
                 shine || lit.done("wire:write"));
            if (live)
                if (const auto v = seen.after.find(on->output); v != seen.after.end())
                    badge(pen, it->second.first, it->second.second, write_port->first, write_port->second, v->second, g);
        }
    }
    for (std::size_t n = 0; n < on->body.nodes.size(); ++n) {
        const auto& node = on->body.nodes[n];
        const auto& rows = placed[n].rows;
        if (!rows.box.meets(canvas)) continue;
        const auto at = std::to_string(placed[n].id);
        if (light)
            for (std::size_t a = 0; a < node.arguments.size(); ++a)
                port(pen, rows.box.x, rows.middles[a + placed[n].extra], hue::wire(), !unwired(node.arguments[a]), canvas,
                     lit_port("port:" + at + "." + std::to_string(a)));
        port(pen, rows.box.right() - (light ? 0 : m.ch), rows.middles.back(), hue::wire(),
             on->body.result_node == n, canvas, lit_port("answer:" + at));
    }
    if (write_port)
        port(pen, write_port->first, write_port->second, hue::kept(), !on->body.nodes.empty(), then_room,
             lit_port("port:write"));
    // A WIRE IN THE HAND, drawn from where it was pressed to the hand, in the ink of wires.
    if (const auto& w = model.wire_drawn) wire(pen, w->from_x, w->from_y, w->to_x, w->to_y, hue::wire(), g);
}

/// WHAT RESTING ON A PORT OR A WIRE TELLS, a line or three: a port's name and kind and what it
/// reads, or for an answer or a field what reads it; a wire's two ends and the value it last carried.
/// None for anything else.
inline std::vector<std::string> told(const Model& m) {
    const auto& def = m.workspace.graph.project.definition;
    if (!m.hover || def.on.empty()) return {};
    const auto& key = *m.hover;
    const auto& graph = m.workspace.graph;
    const auto& on = def.on.at(m.trigger());
    const auto& nodes = on.body.nodes;
    const auto kind = [](const loom::TypeRef& t) { return std::string(loom::name_of(t.kind)); };
    const auto same = [](const op::Binding& a, const op::Binding& b) {
        if (a.from() != b.from()) return false;
        if (a.from() == op::Binding::From::Node) return a.node_index() == b.node_index();
        return a.from() == op::Binding::From::Input && a.input_name() == b.input_name();
    };
    // WHAT READS `source`: each port wired from it, and what the rule keeps where it is the answer.
    const auto readers = [&](const op::Binding& source) {
        std::vector<std::string> out;
        std::string ports;
        for (std::size_t n = 0; n < nodes.size(); ++n) {
            std::vector<std::string> names;
            try {
                for (const auto& f : graph.node_ports(m.trigger(), n, m.palette).inputs->fields()) names.push_back(f.name);
            } catch (const std::exception&) {
            }
            for (std::size_t a = 0; a < nodes[n].arguments.size(); ++a)
                if (same(nodes[n].arguments[a], source))
                    ports += (ports.empty() ? "" : ", ") + step_name(nodes, n) + "'s " +
                             (a < names.size() ? names[a] : std::to_string(a));
        }
        const bool answer = source.from() == op::Binding::From::Node && source.node_index() == on.body.result_node;
        if (!ports.empty()) out.push_back("read by " + ports);
        if (answer) out.push_back(on.output + " becomes it");
        if (out.empty()) out.push_back("nothing reads it yet");
        return out;
    };
    const auto seen = carried(m.observed);
    const auto value = [&](const op::Binding& b) -> std::string {
        if (!seen.known()) return "nothing carried yet";
        std::optional<std::string> v;
        if (b.from() == op::Binding::From::Input) {
            const auto& in = def.state->find(b.input_name()) ? seen.before : seen.message;
            if (const auto it = in.find(b.input_name()); it != in.end()) v = it->second;
        } else if (b.from() == op::Binding::From::Node && b.node_index() == on.body.result_node) {
            if (const auto it = seen.after.find(on.output); it != seen.after.end()) v = it->second;
        }
        return v ? "last carried " + *v : "not seen: Flow sees what arrives and what is kept";
    };
    try {
        if (key.rfind("field:", 0) == 0) {
            const auto name = key.substr(6);
            const auto* kept = def.state->find(name);
            const auto* field = kept ? kept : on.message->find(name);
            if (!field) return {};
            auto out = std::vector<std::string>{name + " : " + kind(field->type) + (kept ? ", kept" : ", from the message")};
            for (auto& line : readers(op::Binding::input(name))) out.push_back(std::move(line));
            return out;
        }
        if (key == "wire:write") {
            const auto b = op::Binding::node(on.body.result_node);
            return {step_name(nodes, on.body.result_node) + "'s answer -> " + on.output, value(b)};
        }
        const auto colon = key.find(':');
        const auto dot = key.find('.');
        const auto head = key.substr(0, colon);
        if (head != "port" && head != "answer" && head != "wire") return {};
        const auto step = step_at(m, flow::integer(key.substr(colon + 1, dot == std::string::npos ? std::string::npos
                                                                                                   : dot - colon - 1)));
        if (!step || *step >= nodes.size()) return {};
        const auto n = *step;
        if (head == "answer") {
            auto out = std::vector<std::string>{step_name(nodes, n) + "'s answer : " +
                                                kind(graph.node_answer_type(m.trigger(), n, m.palette))};
            for (auto& line : readers(op::Binding::node(n))) out.push_back(std::move(line));
            return out;
        }
        const auto a = flow::index_of(key.substr(dot + 1));
        const auto& field = graph.node_ports(m.trigger(), n, m.palette).inputs->fields().at(a);
        const auto& b = nodes.at(n).arguments.at(a);
        const auto port = step_name(nodes, n) + "'s " + field.name;
        if (head == "wire") return {source_words(nodes, b) + " -> " + port, value(b)};
        std::string reads = "is " + flow::binding_text(b);
        if (unwired(b)) reads = "nothing wired yet";
        else if (b.from() == op::Binding::From::Input)
            reads = "reads " + b.input_name() + (def.state->find(b.input_name()) ? ", kept" : ", from the message");
        else if (b.from() == op::Binding::From::Node)
            reads = "reads " + source_words(nodes, b) + "'s answer";
        return {field.name + " : " + kind(field.type), reads};
    } catch (const std::exception&) {
        return {};
    }
}

/// WHAT RESTING TELLS, in a box below and right of where the pointer came to rest -- left of it or
/// above it where `area` has no room there, so it never stands under the pointer -- and a wire in the
/// hand over a port it does not fit, why, beside the hand. The box is a part of its own, so it stands
/// over the parts beneath it, and a press on it does nothing.
inline void tell(Pen& pen, const Model& model, const Box& area) {
    const auto& m = pen.m;
    std::vector<std::string> lines;
    std::int64_t x = 0, y = 0;
    bool refused = false;
    if (model.wire_drawn && !model.wire_drawn->why.empty()) {
        lines = wrapped(model.wire_drawn->why, 64, 3);
        x = model.wire_drawn->to_x;
        y = model.wire_drawn->to_y;
        refused = true;
    } else if (model.hover) {
        lines = told(model);
        x = model.hover_x;
        y = model.hover_y;
    }
    if (lines.empty()) return;
    std::int64_t w = 0;
    for (const auto& line : lines) w = std::max(w, pen.width(line));
    w = std::min(area.w, w + (m.light ? 12 : 2 * m.ch));
    const auto h = std::min(area.h, static_cast<std::int64_t>(lines.size()) * m.row + (m.light ? 10 : 2 * m.row));
    const auto gap = m.light ? std::int64_t{14} : m.ch;
    auto bx = x + gap, by = y + (m.light ? gap : m.row);
    if (bx + w > area.right()) bx = x - gap - w;
    if (by + h > area.bottom()) by = y - h - (m.light ? 6 : m.row);
    const auto snap = [&](std::int64_t v) { return gfx::floor_div_px(v, m.grain) * m.grain; };
    bx = snap(std::clamp(bx, area.x, std::max(area.x, area.right() - w)));
    by = snap(std::clamp(by, area.y, std::max(area.y, area.bottom() - h)));
    const Box box{bx, by, w, h};
    pen.p.tip = box;
    pen.press(box, "tip", {"box"});
    pen.rect(box, m.light ? hue::field() : hue::well(), refused ? hue::warn() : hue::edge(), m.light ? 4 : 0);
    auto ty = by + (m.light ? 5 : m.row);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto ink = refused ? hue::warn() : i == 0 ? hue::text() : hue::quiet();
        pen.text(bx + (m.light ? 6 - m.inset : m.ch), ty, lines[i], ink, box);
        ty += m.row;
    }
}

/// THE NEXT LINE: the one thing the rule still needs, the press that does it, and a hint.
inline void next_line(Pen& pen, const Model& model, const Box& box) {
    const auto& m = pen.m;
    const auto step = next_step(model);
    const auto tone = step.tone == NextStep::Tone::Good ? hue::done()
                      : step.tone == NextStep::Tone::Warn ? hue::warn()
                                                          : hue::message();
    std::int64_t x = box.x;
    const auto ty = box.y + (box.h - m.row) / 2;
    if (m.light) {
        pen.rect(box, hue::field(), step.tone == NextStep::Tone::Warn ? hue::warn() : hue::rule(), 4);
        x += 10;
        pen.dot({x, box.y + box.h / 2 - 5, 10, 10}, tone, gfx::no_paint(), box);
        x += 18;
    } else {
        pen.text(x, ty, "*", tone, box);
        x += 2 * m.ch;
    }
    const auto say = [&](const std::string& words, gfx::Paint ink) {
        if (words.empty()) return;
        pen.text(x - (m.light ? m.inset : 0), ty, words, ink, box);
        x += pen.width(words) - (m.light ? 2 * m.inset : 0) + (m.light ? 8 : m.ch);
    };
    say(step.lead, tone);
    say(step.text, hue::text());
    if (!step.button.empty() && x < box.right()) {
        x += pen.button(x, box.y + (box.h - (m.light ? m.tall : m.row)) / 2, step.button, step.action, step.args,
                        Face::Gold, Icon::None, box) +
             (m.light ? 10 : m.ch);
    }
    say(step.hint, hue::quiet());
}

/// THE EVENTS: one line a message, newest first -- what came in, what changed, what was said.
inline void events_band(Pen& pen, const Model& model, const Box& box, std::size_t lines) {
    const auto& m = pen.m;
    std::int64_t y = box.y;
    if (m.light) {
        pen.rect({box.x, box.y, box.w, 1}, hue::rule());
        y += 8;
    }
    const auto x = box.x + (m.light ? 10 - m.inset : 0);
    pen.text(x, y, "Events", hue::message(), box);
    const std::string order = "newest first";
    pen.text(box.right() - pen.width(order) - (m.light ? 10 - m.inset : 0), y, order, hue::quiet(), box);
    y += m.row;
    // This run's arrivals, newest first, then the runs' before it, as many as the band has lines.
    std::vector<std::string> shown;
    for (auto at = model.observed.arrivals.rbegin(); at != model.observed.arrivals.rend() && shown.size() < lines; ++at)
        shown.push_back(line_of(*at));
    for (auto at = model.earlier.rbegin(); at != model.earlier.rend() && shown.size() < lines; ++at)
        shown.push_back(*at);
    if (model.observed.arrivals.empty() && model.earlier.empty()) {
        const auto& def = model.workspace.graph.project.definition;
        pen.text(x, y,
                 model.running ? "nothing yet: send " + (def.on.empty() ? std::string("its message")
                                                                        : def.on.at(model.trigger()).message->name())
                               : std::string("nothing yet: the rule is not running"),
                 hue::quiet(), box);
        return;
    }
    for (const auto& line : shown) {
        pen.text(x, y, line, hue::text(), box);
        y += m.row;
    }
}

/// ONE TYPED FIELD OF A DIALOG at (x, y) inside `within`: its label and a field to its right where
/// light, `> Label: text` where a character is a cell, the caret and selection drawn in the field
/// the keys are in; a press on its row chooses it.
inline void entry_row(Pen& pen, const Box& within, std::int64_t x, std::int64_t y, const Entry& e, std::size_t i,
                      bool on) {
    const auto& m = pen.m;
    const auto prefix = (m.light ? std::string() : std::string(on ? "> " : "  ")) + e.label + ": ";
    auto text = e.text;
    if (m.light) {
        pen.text(x, y, prefix, on ? hue::text() : hue::quiet(), within);
        const auto fx = x + pen.width(prefix);
        const Box field{fx, y - 3, within.right() - fx - 12, m.tall};
        pen.rect(field, hue::field(), on ? hue::wire() : hue::edge(), 3, false, within);
        const auto columns = std::max<std::int64_t>(1, m.fits(field.w - 8));
        text.keep_caret_visible(columns);
        auto& run = pen.text(field.x + 4, y, text.visible(columns), hue::text(), field);
        if (on) {
            run.caret_col = static_cast<std::int64_t>(text.caret_column());
            const auto selection = text.visible_selection(columns);
            if (selection.present()) {
                run.sel_begin_col = selection.begin;
                run.sel_end_col = selection.end;
            }
        }
    } else {
        const auto columns = std::max(std::int64_t{1}, m.fits(within.w) - static_cast<std::int64_t>(prefix.size()) - 1);
        text.keep_caret_visible(columns);
        // A selected entry's caret after its text stands on a blank of its own.
        const bool after = on && text.caret_column() >= text.visible(columns).size();
        auto& run = pen.text(x, y, prefix + text.visible(columns) + (after ? " " : ""), hue::text(), within);
        if (on) {
            const auto prefix_columns = static_cast<std::int64_t>(prefix.size());
            run.caret_col = prefix_columns + static_cast<std::int64_t>(text.caret_column());
            const auto selection = text.visible_selection(columns);
            if (selection.present()) {
                run.sel_begin_col = prefix_columns + selection.begin;
                run.sel_end_col = prefix_columns + selection.end;
            }
        }
    }
    pen.press(Box{within.x, y - (m.light ? 3 : 0), within.w, m.light ? m.tall : m.row}.cut(within), "dialog-field",
              {std::to_string(i)});
}

/// AN EDITOR IN PLACE: the open dialog as a small box beside what opened it -- under it where the
/// area has room, else over it -- or, where nothing pressed opened it, beside what it edits on the
/// graph; the page stays drawn round it. A field chosen from words is a row of them, the one held
/// lit; a typed one a field with its caret. A press inside it lands on it.
inline void editor(Pen& pen, const Model& model, const Box& area) {
    const auto& m = pen.m;
    const auto& d = *model.dialog;
    const bool light = m.light;
    const auto pad = light ? std::int64_t{10} : std::int64_t{0};
    const auto line = light ? m.tall + 10 : 2 * m.row;
    const auto control_h = light ? m.tall + 6 : m.row;
    const auto w = std::min(area.w, light ? std::max<std::int64_t>(m.chars(40), 360) : m.chars(48));
    const auto offered = suggestions(model);
    const auto h = std::min(area.h, 2 * pad + m.row + (light ? 10 : m.row) +
                                        static_cast<std::int64_t>(d.entries.size()) * line + control_h +
                                        static_cast<std::int64_t>(offered.size()) * m.row);
    std::optional<Box> at;
    if (d.beside) at = Box{d.beside->x, d.beside->y, d.beside->w, d.beside->h};
    else if (const auto it = pen.p.subjects.find(d.action); it != pen.p.subjects.end()) at = it->second;
    std::int64_t bx = area.x + (area.w - w) / 2, by = area.y + (area.h - h) / 2;
    if (at) {
        const auto gap = light ? std::int64_t{6} : std::int64_t{0};
        bx = at->x;
        by = at->bottom() + gap;
        if (by + h > area.bottom()) by = at->y - h - gap;
    }
    const auto snap = [&](std::int64_t v) { return gfx::floor_div_px(v, m.grain) * m.grain; };
    bx = snap(std::clamp(bx, area.x, std::max(area.x, area.right() - w)));
    by = snap(std::clamp(by, area.y, std::max(area.y, area.bottom() - h)));
    const Box box{bx, by, w, h};
    pen.rect(box, light ? hue::band() : hue::well(), hue::wire(), light ? 4 : 0);
    pen.press(box, "dialog-box", {"box"});
    const auto x = bx + (light ? pad + 2 - m.inset : m.ch);
    std::int64_t y = by + pad;
    pen.text(x, y, d.title, hue::message(), box);
    y += m.row + (light ? 10 : m.row);
    const Box inside{bx + (light ? 2 : m.ch), by, w - (light ? 4 : 2 * m.ch), h};
    for (std::size_t i = 0; i < d.entries.size(); ++i) {
        const auto& e = d.entries[i];
        const bool on = i == d.selected;
        if (e.choices.empty()) {
            entry_row(pen, inside, x, y, e, i, on);
            // THE NAMES OFFERED as it is typed, beneath it: each its name and whose it is, the one
            // the keys are on marked; a press takes it.
            if (on && !offered.empty()) {
                auto oy = y + (light ? m.tall : m.row);
                for (std::size_t k = 0; k < offered.size(); ++k) {
                    const bool marked = d.pick && *d.pick == k;
                    const Box row{inside.x, oy, inside.w, m.row};
                    if (marked && light) pen.rect(row, hue::chosen(), gfx::no_paint(), 2, false, box);
                    const auto said = (light ? std::string() : std::string(marked ? "> " : "  ")) + offered[k].name;
                    pen.text(x + (light ? m.ch : 0), oy, said, marked ? hue::message() : hue::text(), box);
                    pen.text(x + (light ? m.ch : 0) + pen.width(said) - (light ? 2 * m.inset : 0) + m.ch, oy,
                             offered[k].whose, hue::quiet(), box);
                    pen.press(row.cut(box), "dialog-suggest", {std::to_string(i), offered[k].name});
                    oy += m.row;
                }
                y += static_cast<std::int64_t>(offered.size()) * m.row;
            }
        } else {
            const auto prefix = (light ? std::string() : std::string(on ? "> " : "  ")) + e.label + ": ";
            pen.text(x, y, prefix, on ? hue::text() : hue::quiet(), box);
            auto cx = x + pen.width(prefix) - (light ? 2 * m.inset : 0);
            for (const auto& choice : e.choices) {
                const bool held = e.text.text() == choice;
                cx += pen.button(cx, y - (light ? 3 : 0), choice, "dialog-choose", {std::to_string(i), choice},
                                 held ? Face::Teal : Face::Plain, Icon::None, box) +
                      (light ? 6 : m.ch);
            }
        }
        y += line;
    }
    const auto step = light ? 8 : m.chars(2);
    auto cx = x + (light ? m.inset : 0);
    cx += pen.button(cx, y, "Confirm", "dialog-confirm", {}, Face::Gold, Icon::None, box) + step;
    pen.button(cx, y, "Cancel", "dialog-cancel", {}, Face::Plain, Icon::None, box);
}

/// THE BOX AT THE CHOSEN PORT: what is in scope of its type, each a button by its own name and
/// where it comes from; then one value of the port's own kind -- a number typed and checked as it
/// is typed for an Int, yes or no for a Bool. It stands beside the port's step, inside `within`,
/// on the side with room; a press inside it lands on it.
inline void port_box(Pen& pen, const Model& model, const Box& within) {
    const auto& m = pen.m;
    const auto port = selected_port(model);
    if (!port || !pen.p.port_at || !pen.p.port_card) return;
    const auto n = std::to_string(port->node), a = std::to_string(port->port);
    const auto offers = in_scope(model, *port);
    const auto kind = port->field.type.kind;
    const bool number = kind == loom::Kind::Int, truth = kind == loom::Kind::Bool;
    const auto typed = model.port_value.text();
    const bool wrong = number && !typed.empty() && !whole_number(typed);
    const auto line = m.light ? m.tall + 4 : m.row;
    const auto pad = m.light ? std::int64_t{8} : std::int64_t{0};
    std::int64_t inner = m.row + (offers.empty() ? m.row : static_cast<std::int64_t>(offers.size()) * line);
    if (number) inner += m.row + line + (wrong ? m.row : 0);
    if (truth) inner += m.row + line;
    const auto w = std::min(within.w, m.chars(30));
    const auto h = std::min(within.h, inner + 2 * pad);
    // Beside the step: on its left where there is room, else on its right, else under the port.
    const auto& card = *pen.p.port_card;
    const auto gap = m.light ? std::int64_t{12} : m.ch;
    std::int64_t bx = card.x - gap - w, by = pen.p.port_at->second - m.row / 2 - pad;
    if (bx < within.x) bx = card.right() + gap;
    if (bx + w > within.right()) {
        bx = card.x;
        by = pen.p.port_at->second + m.row;
    }
    const auto snap = [&](std::int64_t v) { return gfx::floor_div_px(v, m.grain) * m.grain; };
    bx = snap(std::clamp(bx, within.x, std::max(within.x, within.right() - w)));
    by = snap(std::clamp(by, within.y, std::max(within.y, within.bottom() - h)));
    const Box box{bx, by, w, h};
    pen.rect(box, m.light ? hue::band() : hue::well(), hue::wire(), m.light ? 4 : 0);
    pen.press(box, "port-box", {n, a});
    const auto x = bx + (m.light ? pad - m.inset : m.ch);
    std::int64_t y = by + pad;
    pen.text(x, y, port->field.name + " : " + loom::name_of(kind), hue::message(), box);
    const auto xw = pen.button_width("x");
    pen.button(box.right() - xw - (m.light ? 4 : m.ch), y - (m.light ? 3 : 0), "x", "port-close", {n, a},
               Face::Quiet, Icon::None, box);
    y += m.row;
    const auto bx0 = bx + (m.light ? pad : m.ch);
    if (offers.empty()) {
        pen.text(x, y, "nothing in scope is " + std::string(loom::name_of(kind)) + " yet", hue::quiet(), box);
        y += m.row;
    }
    for (const auto& offer : offers) {
        const auto said = offer.from == InScope::From::Kept      ? "kept"
                          : offer.from == InScope::From::Message ? "from the message"
                                                                 : "a step's answer";
        const auto bw = pen.button(bx0, y, offer.name, "scope", {n, a, offer.bind},
                                   offer.from == InScope::From::Kept ? Face::Teal : Face::Plain, Icon::None, box);
        pen.text(bx0 + bw + (m.light ? 8 - m.inset : m.ch), y + (m.light ? (m.tall - m.row) / 2 : 0), said,
                 hue::quiet(), box);
        y += line;
    }
    if (number) {
        pen.text(x, y, "or a number", hue::quiet(), box);
        y += m.row;
        const auto use_w = pen.button_width("Use");
        const Box field{bx0, y, std::max<std::int64_t>(m.ch, box.right() - bx0 - use_w - 2 * gap),
                        m.light ? m.tall : m.row};
        if (m.light) pen.rect(field, hue::field(), wrong ? hue::warn() : hue::wire(), 3, false, box);
        auto value = model.port_value;
        const auto columns = std::max<std::int64_t>(1, m.fits(field.w - (m.light ? 8 : 2 * m.ch)));
        value.keep_caret_visible(columns);
        const auto shown = m.light ? value.visible(columns) : "[" + value.visible(columns) + "]";
        auto& run = pen.text(field.x + (m.light ? 4 - m.inset : 0), y + (m.light ? (m.tall - m.row) / 2 : 0), shown,
                             wrong ? hue::warn() : hue::text(), field.cut(box));
        run.caret_col = static_cast<std::int64_t>(value.caret_column()) + (m.light ? 0 : 1);
        const auto selection = value.visible_selection(columns);
        if (selection.present()) {
            run.sel_begin_col = selection.begin + (m.light ? 0 : 1);
            run.sel_end_col = selection.end + (m.light ? 0 : 1);
        }
        pen.button(field.right() + gap, y, "Use", "port-set", {n, a}, wrong ? Face::Quiet : Face::Gold, Icon::None, box);
        y += line;
        if (wrong) pen.text(x, y, "a whole number, like 12", hue::warn(), box);
    }
    if (truth) {
        pen.text(x, y, "or", hue::quiet(), box);
        y += m.row;
        const auto yes = pen.button(bx0, y, "yes", "port-set", {n, a, "true"}, Face::Plain, Icon::None, box);
        pen.button(bx0 + yes + gap, y, "no", "port-set", {n, a, "false"}, Face::Plain, Icon::None, box);
    }
}

/// THE SEARCH AT THE HAND: a box where the wire was let go, kept inside the canvas, saying what the
/// wire carries, the search line typed into, and the rows a choice makes from, the one the keys are
/// on marked. A press inside it lands on it.
inline void wire_list(Pen& pen, const Model& model, const Box& within) {
    const auto& m = pen.m;
    const auto& search = *model.wire_search;
    const auto rows = wire_rows(model);
    const auto choice = wire_choice(model, rows);
    const auto room_rows = (within.h - (m.light ? 16 : 0)) / m.row - 2;
    const std::size_t visible = static_cast<std::size_t>(std::clamp<std::int64_t>(room_rows, 1, 10));
    std::string carries;
    if (search.from.source) {
        const auto type = source_type(model, *search.from.source);
        const auto& def = model.workspace.graph.project.definition;
        const auto from = def.on.empty() ? std::string("it")
                                         : source_words(def.on.at(model.trigger()).body.nodes, *search.from.source);
        carries = "From " + from + " : " + type.value_or("?") + " -- what takes it";
    } else if (const auto port = wire_port(model)) {
        carries = "Into " + port_words(model, *port) + " : " + port->type + " -- what fills it";
    } else if (!search.from.port) {
        carries = "A step here -- what is it for?";
    }
    // As wide as what it says it carries, so a name is never cut from it.
    const auto w = std::min(within.w, std::max(m.chars(36), pen.width(carries) + 2 * m.ch));
    const auto listed = std::max<std::size_t>(1, std::min(rows.size(), visible));
    const auto h = static_cast<std::int64_t>(listed + 2) * m.row + (m.light ? 16 : 0);
    const auto snap = [&](std::int64_t v) { return gfx::floor_div_px(v, m.grain) * m.grain; };
    const auto bx = snap(std::max(within.x, std::min(search.x, within.right() - w)));
    const auto by = snap(std::max(within.y, std::min(search.y, within.bottom() - h)));
    const Box box{bx, by, w, h};
    pen.rect(box, m.light ? hue::band() : hue::well(), hue::wire(), m.light ? 4 : 0);
    pen.press(box, "wire-list", {"box"});
    const auto x = bx + (m.light ? 8 - m.inset : m.ch);
    std::int64_t y = by + (m.light ? 8 : 0);
    pen.text(x, y, carries, hue::message(), box);
    y += m.row;
    {
        const std::string prefix = "find: ";
        auto text = model.search;
        const auto columns = std::max<std::int64_t>(1, m.fits(w) - static_cast<std::int64_t>(prefix.size()) - 2);
        text.keep_caret_visible(columns);
        const bool after = !m.light && text.caret_column() >= text.visible(columns).size();
        auto& run = pen.text(x, y, prefix + text.visible(columns) + (after ? " " : ""), hue::text(), box);
        const auto prefix_columns = static_cast<std::int64_t>(prefix.size());
        run.caret_col = prefix_columns + static_cast<std::int64_t>(text.caret_column());
        const auto selection = text.visible_selection(columns);
        if (selection.present()) {
            run.sel_begin_col = prefix_columns + selection.begin;
            run.sel_end_col = prefix_columns + selection.end;
        }
    }
    y += m.row;
    if (rows.empty()) pen.text(x, y, search.answered ? "none offered match" : "(asking the door)", hue::quiet(), box);
    // The list scrolls to keep the row the keys are on in sight.
    const std::size_t first = choice && *choice >= visible ? *choice + 1 - visible : 0;
    for (std::size_t i = first; i < rows.size() && i < first + visible; ++i) {
        const auto& row = rows[i];
        const bool on = choice && *choice == i;
        const auto ink = !row.kind.empty() ? (on ? hue::message() : hue::text())
                         : row.label.rfind("+ ", 0) == 0 ? hue::quiet()
                                                         : hue::message();
        if (on && m.light) pen.rect({bx + 2, y, w - 4, m.row}, hue::chosen(), gfx::no_paint(), 2, false, box);
        // The row the keys are on is marked where its indent stands.
        const bool indented = row.label.rfind("  ", 0) == 0;
        pen.text(x, y, on && indented ? "> " + row.label.substr(2) : row.label, ink, box);
        if (!row.kind.empty()) pen.press({bx, y, w, m.row}, "wire-row", {std::to_string(i)});
        y += m.row;
    }
}

} // namespace view_detail

/// A partial graph would look like a different graph. A view too dense to show is this bounded
/// recovery picture instead, keeping save/export and view controls available.
inline Picture recovery_picture(Picture view, const ws::CanvasRoom& room) {
    using namespace view_detail;
    const auto revision = view.revision;
    view = Picture{};
    view.revision = revision;
    view.width = room.width;
    view.height = room.height;
    view.grain = std::max<std::int64_t>(1, room.grain);
    Pen pen{view, Measure(room), room};
    view.light = pen.m.light;
    const auto x0 = pen.m.light ? 10 : 0;
    std::int64_t y = pen.m.light ? 10 : 0;
    pen.text(x0, y, "View limit reached: pan/zoom or choose a smaller graph.", hue::warn());
    y += pen.m.row;
    pen.text(x0, y, "Draft intact. Save or export it before changing the graph.", hue::text());
    y += 2 * pen.m.row;
    std::int64_t x = x0;
    const auto step = pen.m.light ? 8 : pen.m.ch;
    const auto line_h = pen.m.light ? pen.m.tall + 6 : pen.m.row;
    for (const auto& [title, action] : std::vector<std::pair<std::string, std::string>>{
             {"Save", "ask-save"}, {"Export", "ask-export"}, {"Graph", "page-graph"}, {"State", "page-state"},
             {"Messages", "page-messages"}, {"Events", "page-events"}, {"Reset view", "fit"},
             {"Zoom in", "zoom-in"}, {"Zoom out", "zoom-out"}}) {
        const auto w = pen.button_width(title);
        if (x + w > room.width) {
            x = x0;
            y += line_h;
        }
        if (w > room.width || y + line_h > room.height) break;
        x += pen.button(x, y, title, action) + step;
    }
    keep_in_room(view, room);
    return view;
}

namespace view_detail {
/// THE PICTURE KEPT IN ITS ROOM, or the recovery picture where it draws more than its bounds.
inline Picture finish(Picture view, const ws::CanvasRoom& room) {
    keep_in_room(view, room);
    std::size_t bytes = 0;
    for (const auto& k : view.marks) bytes += k.words.size();
    if (view.marks.size() <= kMaxMarks && bytes <= kMaxMarkTextBytes) return view;
    return recovery_picture(std::move(view), room);
}
} // namespace view_detail

inline Picture picture(const Model& model, const ws::CanvasRoom& room) {
    using namespace view_detail;
    Picture out;
    out.revision = model.workspace.graph.project.definition.revision;
    out.width = room.width;
    out.height = room.height;
    out.grain = std::max<std::int64_t>(1, room.grain);
    Pen pen{out, Measure(room), room};
    const auto& m = pen.m;
    out.light = m.light;
    const auto W = room.width, H = room.height;
    if (W < m.chars(40) || H < m.row * 10 || m.ch > W || m.row > H) {
        pen.text(0, 0, "Flow needs more room", hue::text());
        pen.text(0, m.row, "Resize this pane", hue::quiet());
        return finish(std::move(out), room);
    }
    const bool light = m.light;
    const auto margin = light ? std::int64_t{10} : std::int64_t{0};
    const auto bar_h = light ? m.tall + 16 : m.row;
    bar(pen, model, bar_h);
    const auto tabs_h = light ? m.row + 10 : m.row;
    tabs(pen, model, bar_h, tabs_h);
    const auto foot = H - m.row - (light ? 4 : 0);
    pen.text(margin, foot, model.notice, light ? hue::soft() : hue::message(), Box{0, foot, W, m.row});
    const auto top = bar_h + tabs_h;
    const Box body{margin, top + (light ? 10 : 0), W - 2 * margin, std::max<std::int64_t>(0, foot - top - (light ? 14 : 0))};
    const auto line_h = light ? m.row : m.row;
    const auto control_h = light ? m.tall + 6 : m.row;
    // AN EDITOR IN PLACE is drawn over the page it was opened on, which stays drawn round it and is
    // pressed nowhere: a press outside the editor puts it down, so only the editor's parts are named.
    const auto done = [&]() {
        if (model.dialog && in_place(model.dialog->action)) {
            out.hits.clear();
            editor(pen, model, Box{0, top, W, std::max<std::int64_t>(0, foot - top)});
        }
        return finish(std::move(out), room);
    };
    if (model.dialog && !in_place(model.dialog->action)) {
        const auto& dialog = *model.dialog;
        const Box card_box{body.x, body.y, std::min(body.w, m.chars(90)), body.h};
        if (light) pen.rect(card_box, hue::band(), hue::edge(), 4);
        const auto x = card_box.x + (light ? 12 - m.inset : 0);
        std::int64_t y = card_box.y + (light ? 10 : 0);
        pen.text(x, y, dialog.title, hue::message(), card_box);
        y += 2 * line_h;
        for (std::size_t i = 0; i < dialog.entries.size(); ++i) {
            entry_row(pen, card_box, x, y, dialog.entries[i], i, i == dialog.selected);
            y += light ? m.tall + 10 : 2 * m.row;
        }
        const auto step = light ? 8 : m.chars(2);
        auto bx = x + (light ? m.inset : 0);
        bx += pen.button(bx, y, "Confirm", "dialog-confirm", {}, Face::Gold, Icon::None, card_box) + step;
        pen.button(bx, y, "Cancel", "dialog-cancel", {}, Face::Plain, Icon::None, card_box);
        y += control_h + (light ? 4 : m.row);
        pen.text(x, y, "Tab changes field; Enter confirms; Escape cancels", hue::quiet(), card_box);
        return finish(std::move(out), room);
    }
    if (model.dropped) {
        // A DROPPED VALUE'S PAGE: what came, and what it can become here; nothing is done until the
        // maker chooses, and Escape or Cancel puts it down.
        const auto summary = summarize_drop(model.dropped->value);
        std::int64_t y = body.y;
        pen.text(body.x, y, summary.title, hue::message(), body);
        y += line_h;
        for (const auto& row : summary.rows) {
            if (y >= body.y + 6 * line_h) break;
            pen.text(body.x + m.ch, y, row, hue::quiet(), body);
            y += line_h;
        }
        if (!summary.why.empty()) {
            pen.text(body.x + m.ch, y, "Not declarable: " + summary.why, hue::warn(), body);
            y += line_h;
        }
        y += line_h;
        for (const auto& offer : drop_offers(model)) {
            if (y + control_h > body.bottom()) break;
            pen.button(body.x, y, offer.title, offer.action, offer.args, Face::Plain, Icon::None, body);
            y += control_h;
        }
        return done();
    }
    const auto& graph = model.workspace.graph;
    const auto& def = graph.project.definition;
    if (model.page == Page::Graph) {
        const auto rule_h = light ? m.row + 16 : m.row;
        const auto lit = lighting(model, sentence(model));
        rule_row(pen, model, Box{0, top, W, rule_h}, lit);
        const auto y0 = top + rule_h + (light ? 10 : 0);
        const auto next_h = light ? m.tall + 16 : m.row;
        std::size_t lines = std::min<std::size_t>(
            light ? 4 : 2, std::max<std::size_t>(1, model.observed.arrivals.size() + model.earlier.size()));
        const auto events_h = [&](std::size_t n) {
            return light ? 8 + m.row * static_cast<std::int64_t>(1 + n) + 4 : m.row * static_cast<std::int64_t>(1 + n);
        };
        const auto floor_h = light ? std::int64_t{180} : m.row * 6;
        const auto gaps = light ? 20 : 0;
        std::int64_t band_h = events_h(lines);
        while (lines > 1 && foot - y0 - next_h - band_h - gaps < floor_h) band_h = events_h(--lines);
        if (foot - y0 - next_h - band_h - gaps < floor_h) band_h = 0;
        const auto graph_h = std::max<std::int64_t>(m.row * 3, foot - y0 - next_h - band_h - gaps - (light ? 4 : 0));
        const auto rail_w = light ? std::clamp<std::int64_t>(W * 24 / 100, 200, 280)
                                  : std::min(std::clamp<std::int64_t>(W * 30 / 100, m.chars(22), m.chars(48)), W / 3);
        const Box rail_box{margin, y0, rail_w, graph_h};
        const Box graph_box{margin + rail_w + margin + (light ? 0 : m.ch), y0,
                            W - 2 * margin - rail_w - margin - (light ? 0 : m.ch), graph_h};
        rail(pen, model, rail_box);
        lanes(pen, model, graph_box, lit);
        if (!model.wire_search && !model.wire_drawn && !model.dialog && out.canvas) port_box(pen, model, *out.canvas);
        if (model.wire_search && out.canvas) wire_list(pen, model, *out.canvas);
        if (!model.dialog && !model.wire_search) tell(pen, model, Box{0, y0, W, graph_h});
        out.pan_x = model.workspace.pan_x;
        out.pan_y = model.workspace.pan_y;
        out.zoom = model.workspace.zoom;
        const auto ny = y0 + graph_h + (light ? 10 : 0);
        next_line(pen, model, Box{margin, ny, W - 2 * margin, next_h});
        if (band_h > 0) events_band(pen, model, Box{margin, ny + next_h + (light ? 10 : 0), W - 2 * margin, band_h}, lines);
        return done();
    }
    if (model.page == Page::Events) {
        // A long event -- a refusal in its owner's words -- continues on the rows beneath it,
        // indented, rather than ending where the room does.
        const auto columns = std::max<std::size_t>(24, static_cast<std::size_t>(std::max<std::int64_t>(0, m.fits(body.w) - 1)));
        std::int64_t y = body.y;
        const auto lines = [&](const std::string& text, gfx::Paint ink) {
            std::string rest = text;
            while (y + line_h <= body.bottom()) {
                if (rest.size() <= columns) {
                    pen.text(body.x, y, rest, ink, body);
                    y += line_h;
                    return;
                }
                if (y + 2 * line_h > body.bottom()) { // the last row the room has: shortened, and marked so
                    pen.text(body.x, y, rest.substr(0, columns - 3) + "...", ink, body);
                    y += line_h;
                    return;
                }
                auto cut = rest.rfind(' ', columns);
                if (cut == std::string::npos || cut < columns / 2) cut = columns;
                pen.text(body.x, y, rest.substr(0, cut), ink, body);
                y += line_h;
                rest = "    " + rest.substr(cut < rest.size() && rest[cut] == ' ' ? cut + 1 : cut);
            }
        };
        if (!model.events.empty()) lines("Events, newest first", hue::message());
        for (std::size_t i = model.first_row; i < model.events.size() && y < body.bottom(); ++i)
            lines(model.events[i], hue::text());
        for (const auto& error : model.lacks()) {
            if (y >= body.bottom()) break;
            lines(error, hue::message());
        }
        if (model.events.empty() && y < body.bottom())
            pen.text(body.x, y, "Run and send a message. Observed results appear here.", hue::quiet(), body);
        return done();
    }
    std::int64_t y = body.y;
    const auto step = light ? 8 : m.ch;
    if (model.page == Page::State) {
        auto x = body.x;
        x += pen.button(x, y, "Add state field", "ask-state-field") + step;
        pen.button(x, y, "Keep state", "keep-state");
        y += control_h + (light ? 6 : m.row);
    }
    if (model.page == Page::Messages) {
        auto x = body.x;
        x += pen.button(x, y, "New message", "ask-message") + step;
        pen.button(x, y, "Add field", "ask-message-field");
        y += control_h;
        for (std::size_t i = 0; i < def.accepts.size() && y + control_h <= body.bottom(); ++i) {
            pen.button(body.x, y, def.accepts[i]->name(), "message-open", {std::to_string(i)});
            y += control_h;
        }
        y += line_h;
        // THE MESSAGES THIS DEFINITION MAY PUBLISH, inside its namespace; a trigger's Emit sends one.
        pen.text(body.x, y + (light ? (m.tall - m.row) / 2 : 0), "Emitted", hue::message(), body);
        x = body.x + m.chars(10);
        x += pen.button(x, y, "New emitted", "ask-emitted-message") + step;
        pen.button(x, y, "Add emitted field", "ask-emitted-field");
        y += control_h;
        for (std::size_t i = 0; i < def.emits.size() && y + line_h <= body.bottom(); ++i) {
            std::string fields;
            for (const auto& f : def.emits[i]->fields())
                fields += (fields.empty() ? "" : ", ") + f.name + ": " + loom::name_of(f.type.kind);
            pen.text(body.x + m.ch, y, def.emits[i]->name() + " {" + fields + "}", hue::text(), body);
            pen.press({body.x, y, body.w, line_h}, "emitted-row", {std::to_string(i)});
            y += line_h;
        }
        y += line_h;
    }
    if (model.page == Page::Library) {
        auto x = body.x;
        x += pen.button(x, y, "Import", "ask-library-open") + step;
        pen.button(x, y, "Export", "ask-library-save");
        y += control_h;
        for (const auto& p : model.workspace.library.entries()) {
            if (y + control_h > body.bottom()) break;
            pen.button(body.x, y, p.title + (p.draft.ready() ? "" : " (unfinished)"), "preset-open", {p.title});
            y += control_h;
        }
        y += line_h;
        pen.text(body.x, y, "Retained forms", hue::message(), body);
        y += line_h;
        for (std::size_t i = 0; i < model.workspace.forms.size() && y + control_h <= body.bottom(); ++i) {
            const auto& saved = model.workspace.forms[i];
            std::size_t before = 0;
            for (std::size_t k = 0; k < i; ++k)
                if (model.workspace.forms[k].title == saved.title) ++before;
            pen.button(body.x, y, saved.title + (before > 0 ? " (" + std::to_string(before + 1) + ")" : std::string()),
                       "draft-open", {std::to_string(i)});
            y += control_h;
        }
        y += line_h;
    }
    if (model.form) {
        pen.text(body.x, y, model.form_title, hue::message(), body);
        y += line_h;
        auto x = body.x;
        x += pen.button(x, y, "Save example", "ask-preset") + step;
        if (!model.form_state) pen.button(x, y, "Send", "send");
        y += control_h;
        const auto rows = model.form->rows();
        const auto unset_w = pen.button_width("unset");
        for (std::size_t i = model.first_row; i < rows.size() && y + line_h <= body.bottom(); ++i) {
            const auto& row = rows[i];
            pen.text(body.x, y, row.label + " : " + loom::name_of(row.type.kind) + " = " + row.summary, hue::text(),
                     Box{body.x, y, std::max<std::int64_t>(m.ch, body.w - unset_w - step), line_h});
            pen.press({body.x, y, std::max<std::int64_t>(m.ch, body.w - unset_w - step), line_h}, "value-row",
                      {std::to_string(i)});
            pen.button(body.right() - unset_w, y - (light ? (m.tall - m.row) / 2 : 0), "unset", "unset", {std::to_string(i)},
                       Face::Quiet);
            y += light ? m.tall + 2 : line_h;
        }
        if (!model.form->ready() && y < body.bottom())
            pen.text(body.x, y, "Unfinished draft: save it now, complete before sending.", hue::message(), body);
    }
    return done();
}

/// Pan a graph that runs past the bottom of its canvas up, as far as its topmost node allows, so a
/// workspace opened in a short pane shows its nodes whole where the room can hold them.
inline void reveal_graph(Model& model, const ws::CanvasRoom& room) {
    if (room.width <= 0 || room.height <= 0) return;
    const auto drawn = picture(model, room);
    if (!drawn.graph_extent || !drawn.canvas) return;
    const auto [top, bottom] = *drawn.graph_extent;
    if (bottom <= drawn.canvas->bottom()) return;
    const auto dy = std::max(drawn.canvas->bottom() - bottom, drawn.canvas->y - top);
    if (dy >= 0) return;
    model.workspace.pan_y = std::clamp(model.workspace.pan_y + GridProjection(room).grid_y(dy),
                                       std::int64_t{-10000000}, std::int64_t{10000000});
}

/// Pan the graph so the selected node's whole card -- its head, body slot, ports and answer -- sits
/// inside the canvas, as this room draws it. A node just added is then seen whole; one already in
/// view does not move.
inline void reveal_selected(Model& model, const ws::CanvasRoom& room) {
    if (!model.node || room.width <= 0 || room.height <= 0) return;
    const auto drawn = picture(model, room);
    if (!drawn.node_extent || !drawn.canvas) return;
    const auto [top, bottom] = *drawn.node_extent;
    std::int64_t dy = 0;
    if (bottom > drawn.canvas->bottom()) dy = drawn.canvas->bottom() - bottom;
    if (top + dy < drawn.canvas->y) dy = drawn.canvas->y - top;
    if (dy == 0) return;
    const GridProjection grid(room);
    // Into the grid's units, rounded away from zero so the move is never one too short.
    const auto units = dy < 0 ? -grid.grid_y(-dy + grid.row - 1) : grid.grid_y(dy + grid.row - 1);
    model.workspace.pan_y = std::clamp(model.workspace.pan_y + units, std::int64_t{-10000000}, std::int64_t{10000000});
    model.touched();
}

/// FLOW IN A GRAPHICS WINDOW: its picture for the window's room, drawn as the window is given one
/// (`sketch_of`), and the place each part the drawing names stands for, by its name. A view past
/// what a window keeps is the recovery picture.
struct WindowPicture {
    Picture picture;
    gfx::Sketch sketch;
    std::map<std::string, Hit> parts;
};
inline WindowPicture window_picture(const Model& model, const ws::CanvasRoom& room) {
    WindowPicture out{picture(model, room), {}, {}};
    auto drawn = sketch_of(out.picture, room);
    if (!drawn.over.empty()) {
        out.picture = recovery_picture(std::move(out.picture), room);
        drawn = sketch_of(out.picture, room);
    }
    out.sketch = std::move(drawn.sketch);
    std::set<std::string> said;
    for (const auto& record : out.sketch.records()) said.insert(record.action);
    for (const Hit& hit : out.picture.hits)
        if (const auto name = part_name(hit); said.count(name) != 0) out.parts.emplace(name, hit);
    return out;
}
} // namespace zengine::flow_pane
#endif
