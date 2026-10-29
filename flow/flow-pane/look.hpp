// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_LOOK_HPP
#define ZENGINE_FLOW_PANE_LOOK_HPP
// Flow's look: its colours, each exact beside the theme's role a medium without colour shows; the
// measures of the room a picture is laid out in; the marks a picture is made of; and the drawing a
// graphics window is given of them -- the marks in paint order, each place a press means something
// painted again over its place under a record that names it, and a run's caret and selection in
// front. Reference: flow/docs/zengine.flow.md.
#include "graphics/canvas.hpp"
#include "graphics/ops.hpp"
#include "graphics/pixels.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/pane_parts.hpp"
#include "workshop/pane_picture.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::flow_pane {
namespace ws = zengine::workshop;
namespace gfx = zengine::graphics;
namespace ink = zengine::graphics::role;

/// FLOW'S COLOURS, exact where a medium shows colour and the role beside each where it does not.
/// Every surface is a ground, so a terminal paints none of them; what a word means is its role.
namespace hue {
inline gfx::Paint of(std::int64_t role, std::int64_t rgb) { return gfx::exact(role, rgb); }
inline gfx::Paint body() { return of(ink::kGround, 0x262b35); }    ///< the pane's ground
inline gfx::Paint field() { return of(ink::kGround, 0x1b1f26); }   ///< a field, a chip, the Next line
inline gfx::Paint well() { return of(ink::kGround, 0x1a1d24); }    ///< the graph's ground
inline gfx::Paint band() { return of(ink::kGround, 0x2b303b); }    ///< a card's head, the rule's row
inline gfx::Paint card() { return of(ink::kGround, 0x323846); }    ///< a card's body
inline gfx::Paint badge() { return of(ink::kGround, 0x3b4252); }   ///< a value on a wire
inline gfx::Paint chosen() { return of(ink::kGround, 0x323d48); }  ///< the row the preview shows
/// The three lanes, each its part's colour at a twentieth over the graph's ground.
inline gfx::Paint when_lane() { return of(ink::kGround, 0x242629); }
inline gfx::Paint do_lane() { return of(ink::kGround, 0x20252d); }
inline gfx::Paint then_lane() { return of(ink::kGround, 0x20252c); }
/// A part of the rule's sentence: a message, a kept field, a step, at a seventh over its row.
inline gfx::Paint message_chip() { return of(ink::kGround, 0x464646); }
inline gfx::Paint kept_chip() { return of(ink::kGround, 0x39444d); }
inline gfx::Paint step_chip() { return of(ink::kGround, 0x384450); }
inline gfx::Paint message_rim() { return of(ink::kMuted, 0x8b7e63); }
inline gfx::Paint kept_rim() { return of(ink::kMuted, 0x5d767b); }
inline gfx::Paint step_rim() { return of(ink::kMuted, 0x5a7886); }
inline gfx::Paint rule() { return of(ink::kMuted, 0x343b49); }     ///< a line between parts
inline gfx::Paint frame() { return of(ink::kMuted, 0x434c5e); }    ///< the preview's outline
inline gfx::Paint edge() { return of(ink::kMuted, 0x4a5366); }     ///< an outline
inline gfx::Paint thread() { return of(ink::kMuted, 0x6b7385); }   ///< the thread from a write to a say
inline gfx::Paint text() { return of(ink::kFill, 0xe5e9f0); }
inline gfx::Paint soft() { return of(ink::kFill, 0xc0c7d3); }
inline gfx::Paint quiet() { return of(ink::kMuted, 0x9aa3b2); }
inline gfx::Paint faint() { return of(ink::kMuted, 0x7d869a); }
inline gfx::Paint message() { return of(ink::kAccent, 0xebcb8b); } ///< a message: gold
inline gfx::Paint wire() { return of(ink::kAccent, 0x88c0d0); }    ///< wires, ports and steps
inline gfx::Paint kept() { return of(ink::kAccent, 0x8fbcbb); }    ///< what the rule keeps
inline gfx::Paint done() { return of(ink::kFill, 0xa3be8c); }      ///< ready, running, done
inline gfx::Paint warn() { return of(ink::kAlert, 0xd08770); }     ///< what must come first
/// A gold button's face, and the words on it: an accent where a medium has no colour.
inline gfx::Paint gold() { return of(ink::kGround, 0xebcb8b); }
inline gfx::Paint on_gold() { return of(ink::kAccent, 0x1b1f26); }
} // namespace hue

/// THE ROOM'S MEASURES, as a picture is laid out in them: a character's advance and a line's
/// height, the inset that pads a run of text, and the spacing a medium of light gives what a
/// medium of characters packs a cell apart.
struct Measure {
    bool light = false;         ///< the room shows every pixel and sets its text in a face
    std::int64_t ch = gfx::kCellPx, line = gfx::kCellPx, inset = 0;
    std::int64_t row = gfx::kCellPx;  ///< a run of text's height: its line and the inset twice
    std::int64_t pad = 0, gap = gfx::kCellPx;
    std::int64_t tall = gfx::kCellPx; ///< a control's height
    std::int64_t card = gfx::kCellPx; ///< a card's row
    std::int64_t grain = gfx::kCellPx;
    explicit Measure(const ws::CanvasRoom& room) {
        const auto m = ws::canvas_text_metrics(room);
        light = m.graphical;
        ch = std::max<std::int64_t>(1, m.advance);
        line = std::max<std::int64_t>(1, m.line);
        inset = m.inset;
        grain = std::max<std::int64_t>(1, m.grain);
        row = gfx::add_cells(line, 2 * inset);
        if (light) {
            pad = 6;
            gap = 8;
            tall = gfx::add_cells(row, 6);
            card = gfx::add_cells(row, 8);
        } else {
            pad = 0;
            gap = ch;
            tall = row;
            card = row;
        }
    }
    /// How wide `n` characters stand, padded as a run is.
    std::int64_t words(std::size_t n) const {
        return gfx::add_cells(gfx::mul_px(static_cast<std::int64_t>(n), ch), 2 * inset);
    }
    std::int64_t chars(std::int64_t n) const { return gfx::mul_px(n, ch); }
    /// How many characters fit `px` pixels of a run's room.
    std::int64_t fits(std::int64_t px) const {
        return std::max<std::int64_t>(0, (px - 2 * inset) / ch);
    }
};

/// A rectangle of the area, in its pixels.
struct Box {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
    std::int64_t right() const { return gfx::add_cells(x, w); }
    std::int64_t bottom() const { return gfx::add_cells(y, h); }
    bool meets(const Box& o) const {
        return !empty() && !o.empty() && x < o.right() && o.x < right() && y < o.bottom() &&
               o.y < bottom();
    }
    bool holds(const Box& o) const {
        return o.x >= x && o.y >= y && o.right() <= right() && o.bottom() <= bottom();
    }
    Box cut(const Box& o) const {
        const auto x0 = std::max(x, o.x), y0 = std::max(y, o.y);
        const auto x1 = std::min(right(), o.right()), y1 = std::min(bottom(), o.bottom());
        return {x0, y0, std::max<std::int64_t>(0, x1 - x0), std::max<std::int64_t>(0, y1 - y0)};
    }
};

/// ONE MARK OF A PICTURE, in paint order: a rectangle (rounded in a room of light, and outlined in
/// dashes there when `dashed`), an ellipse, a line from (x, y) to (x + w, y + h), a path, or a run
/// of text -- its padded region's origin, its words, and its caret and selection by byte column.
/// A clip keeps it inside a part of the picture.
struct Mark {
    enum class Op { Rect, Ellipse, Line, Path, Text };
    Op op = Op::Rect;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    gfx::Paint fill = gfx::no_paint(), stroke = gfx::no_paint(); ///< a run's ink is its fill
    std::int64_t radius = 0;
    bool dashed = false;
    std::string words; ///< a run's words, or a path's data in the area's pixels
    std::int64_t caret_col = -1, sel_begin_col = -1, sel_end_col = -1;
    std::optional<Box> clip;
    /// Where it stands: its box, a line's from its two ends.
    Box bounds() const {
        if (op != Op::Line) return {x, y, w, h};
        const auto x1 = gfx::add_cells(x, w), y1 = gfx::add_cells(y, h);
        return {std::min(x, x1), std::min(y, y1), std::max<std::int64_t>(1, std::abs(w) + 1),
                std::max<std::int64_t>(1, std::abs(h) + 1)};
    }
};

/// A PLACE A PRESS MEANS SOMETHING ON: the action it asks, its arguments, and the node's stable
/// place it stands for, if any.
struct Hit {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::string action;
    std::vector<std::string> args;
    std::int64_t subject = 0;
    bool contains(std::int64_t px, std::int64_t py, std::int64_t grain) const {
        return gfx::px_span_contains(x, w, px, grain) && gfx::px_span_contains(y, h, py, grain);
    }
};

/// Printable ASCII as itself; a control byte as '?', as a window draws any other.
inline std::string clean(std::string text) {
    for (auto& c : text)
        if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) c = '?';
    return text;
}

/// A number as path data spells it: whole, or to two places.
inline std::string num(std::int64_t v) { return std::to_string(v); }
inline std::string num(double v) {
    char out[32];
    const double whole = static_cast<double>(static_cast<std::int64_t>(v));
    if (whole == v)
        std::snprintf(out, sizeof out, "%lld", static_cast<long long>(v));
    else
        std::snprintf(out, sizeof out, "%.2f", v);
    return out;
}

/// A rectangle's outline with rounded corners, as path data: its far edges on its last pixels.
inline std::string rounded(const Box& b, std::int64_t r) {
    const double x0 = static_cast<double>(b.x), y0 = static_cast<double>(b.y);
    const double x1 = static_cast<double>(b.right() - 1), y1 = static_cast<double>(b.bottom() - 1);
    const double k = static_cast<double>(std::clamp<std::int64_t>(r, 0, std::min(b.w, b.h) / 2));
    std::string d = "M" + num(x0 + k) + " " + num(y0) + " H" + num(x1 - k);
    d += " Q" + num(x1) + " " + num(y0) + " " + num(x1) + " " + num(y0 + k);
    d += " V" + num(y1 - k) + " Q" + num(x1) + " " + num(y1) + " " + num(x1 - k) + " " + num(y1);
    d += " H" + num(x0 + k) + " Q" + num(x0) + " " + num(y1) + " " + num(x0) + " " + num(y1 - k);
    d += " V" + num(y0 + k) + " Q" + num(x0) + " " + num(y0) + " " + num(x0 + k) + " " + num(y0) +
         " Z";
    return d;
}

/// A rectangle's outline in dashes, as path data: four pixels drawn and three left, along each
/// straight edge.
inline std::string dashes(const Box& b, std::int64_t r) {
    std::string d;
    const auto run = [&](std::int64_t a0, std::int64_t a1, std::int64_t at, bool across) {
        for (std::int64_t a = a0; a < a1; a += 7) {
            const auto end = std::min(a1, a + 4);
            if (across)
                d += "M" + num(static_cast<double>(a)) + " " + num(static_cast<double>(at)) + " L" +
                     num(static_cast<double>(end)) + " " + num(static_cast<double>(at)) + " ";
            else
                d += "M" + num(static_cast<double>(at)) + " " + num(static_cast<double>(a)) + " L" +
                     num(static_cast<double>(at)) + " " + num(static_cast<double>(end)) + " ";
        }
    };
    const auto x1 = b.right() - 1, y1 = b.bottom() - 1;
    run(b.x + r, x1 - r, b.y, true);
    run(b.x + r, x1 - r, y1, true);
    run(b.y + r, y1 - r, b.x, false);
    run(b.y + r, y1 - r, x1, false);
    return d;
}

/// THE GEAR, as the mockup draws it in fourteen pixels, at (x, y): its teeth's outline.
inline std::string gear(std::int64_t x, std::int64_t y) {
    static constexpr std::array<std::pair<double, double>, 32> kTeeth{{
        {11.46, 5.89}, {13.24, 6.12}, {13.24, 7.88}, {11.46, 8.11}, {10.94, 9.37}, {12.03, 10.79},
        {10.79, 12.03}, {9.37, 10.94}, {8.11, 11.46}, {7.88, 13.24}, {6.12, 13.24}, {5.89, 11.46},
        {4.63, 10.94}, {3.21, 12.03}, {1.97, 10.79}, {3.06, 9.37}, {2.54, 8.11}, {0.76, 7.88},
        {0.76, 6.12}, {2.54, 5.89}, {3.06, 4.63}, {1.97, 3.21}, {3.21, 1.97}, {4.63, 3.06},
        {5.89, 2.54}, {6.12, 0.76}, {7.88, 0.76}, {8.11, 2.54}, {9.37, 3.06}, {10.79, 1.97},
        {12.03, 3.21}, {10.94, 4.63}}};
    std::string d;
    for (std::size_t i = 0; i < kTeeth.size(); ++i)
        d += (i == 0 ? "M" : " L") + num(static_cast<double>(x) + kTeeth[i].first) + " " +
             num(static_cast<double>(y) + kTeeth[i].second);
    return d + " Z";
}

/// A cubic wire from one port to another, leaving and arriving level, as path data.
inline std::string curve(std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1) {
    const double mx = (static_cast<double>(x0) + static_cast<double>(x1)) / 2.0;
    return "M" + num(static_cast<double>(x0)) + " " + num(static_cast<double>(y0)) + " C" +
           num(mx) + " " + num(static_cast<double>(y0)) + " " + num(mx) + " " +
           num(static_cast<double>(y1)) + " " + num(static_cast<double>(x1)) + " " +
           num(static_cast<double>(y1));
}

/// WHAT A PICTURE DRAWS, AND WHERE A PRESS MEANS SOMETHING: the marks in paint order, the places
/// in the order a press reads them (the later over the earlier), and the room's measures.
struct Drawing {
    std::vector<Mark> marks;
    std::vector<Hit> hits;
    std::int64_t width = 0, height = 0, grain = 1;
    bool light = false;
    /// The places a press reaches, the last first.
    const Hit* hit(std::int64_t x, std::int64_t y) const {
        for (auto at = hits.rbegin(); at != hits.rend(); ++at)
            if (at->contains(x, y, grain)) return &*at;
        return nullptr;
    }
};

/// THE MARKS AS THE ROOM KEEPS THEM: each run of text held to whole glyphs and rows inside its
/// clip and the room, a run shortened marked with dots where it was cut; each rectangle cut to
/// its clip; any other mark kept whole inside its clip, left out wholly outside it, and kept with
/// its clip where it crosses one. What the room cannot show at all goes.
inline void keep_in_room(Drawing& d, const ws::CanvasRoom& room) {
    const Box whole{0, 0, room.width, room.height};
    std::vector<Mark> kept;
    kept.reserve(d.marks.size());
    for (auto& m : d.marks) {
        const Box clip = m.clip ? m.clip->cut(whole) : whole;
        if (m.op == Mark::Op::Text) {
            ws::CanvasText run{m.x, m.y, m.words, m.fill.role, m.caret_col, m.sel_begin_col,
                               m.sel_end_col};
            const auto original = m.words.size();
            // A clip keeps a run's glyphs, whose padding may stand the inset left of it.
            const auto inset = ws::canvas_text_metrics(room).inset;
            const Box glyphs = Box{clip.x - inset, clip.y, clip.w + inset, clip.h}.cut(whole);
            auto layout = ws::clip_canvas_text(run, {glyphs.x, glyphs.y, glyphs.w, glyphs.h}, room);
            if (!layout.visible()) continue;
            const bool left_cut = layout.first_column > 0;
            const bool right_cut = layout.first_column + layout.text.text.size() < original;
            auto words = std::move(layout.text.text);
            const auto mark = std::min(std::size_t{3}, words.size());
            if (left_cut) words.replace(0, mark, mark, '.');
            if (right_cut) words.replace(words.size() - mark, mark, mark, '.');
            m.x = layout.text.x;
            m.y = layout.text.y;
            m.words = std::move(words);
            m.caret_col = layout.text.caret_col;
            m.sel_begin_col = layout.text.sel_begin_col;
            m.sel_end_col = layout.text.sel_end_col;
            m.w = layout.bounds.w;
            m.h = layout.bounds.h;
            m.clip.reset();
            if (m.words.empty() && m.caret_col < 0) continue;
            kept.push_back(std::move(m));
            continue;
        }
        const Box at = m.bounds();
        if (!at.meets(clip)) continue;
        if (m.op == Mark::Op::Rect) {
            const Box inside = Box{m.x, m.y, m.w, m.h}.cut(clip);
            if (inside.empty()) continue;
            // A rectangle cut loses its rounding where it was cut: its corners are not its own.
            if (inside.x != m.x || inside.y != m.y || inside.w != m.w || inside.h != m.h)
                m.radius = 0;
            m.x = inside.x;
            m.y = inside.y;
            m.w = inside.w;
            m.h = inside.h;
            m.clip.reset();
        } else if (clip.holds(at)) {
            m.clip.reset();
        } else {
            m.clip = clip;
        }
        kept.push_back(std::move(m));
    }
    d.marks = std::move(kept);
    std::vector<Hit> hits;
    for (auto h : d.hits) {
        const Box at = Box{h.x, h.y, h.w, h.h}.cut(whole);
        if (at.empty()) continue;
        h.x = at.x;
        h.y = at.y;
        h.w = at.w;
        h.h = at.h;
        hits.push_back(std::move(h));
    }
    d.hits = std::move(hits);
    d.width = room.width;
    d.height = room.height;
}

/// THE NAME A PART OF THE PICTURE IS PRESSED BY: a node by its place, `node:<id>`, and its argument
/// `port:<id>.<n>` (`#<index>` where the node has no place yet); a control by what it does,
/// `control:<action>`; any other place by what it does and what it stands for, `<action>:<args>`.
inline std::string part_name(const Hit& hit) {
    const auto node = [&]() {
        return hit.subject != 0 ? std::to_string(hit.subject)
                                : "#" + (hit.args.empty() ? std::string() : hit.args[0]);
    };
    if (hit.action == "node") return "node:" + node();
    if (hit.action == "port" && hit.args.size() == 2) return "port:" + node() + "." + hit.args[1];
    if (hit.args.empty()) return "control:" + hit.action;
    std::string name = hit.action;
    for (const auto& arg : hit.args) name += ":" + arg;
    return name;
}

/// THE PLACES OF A PICTURE AS IT IS SAID: each place a press means something, in the order a press
/// reads them, under its name (`part_name`), or unnamed where an earlier place took its name.
inline ws::CanvasPicture named(const Drawing& d) {
    ws::PartNames<ws::CanvasPart> gathered;
    for (const auto& h : d.hits) (void)gathered.add(ws::CanvasPart{part_name(h), h.x, h.y, h.w, h.h});
    ws::CanvasPicture out;
    out.parts = gathered.take();
    return out;
}

/// The drawing a window keeps of a picture, and why it would not keep the picture itself.
struct Sketched {
    gfx::Sketch sketch;
    std::string over; ///< why the picture is more than a window keeps, or ""
};

/// THE PICTURE AS A GRAPHICS WINDOW IS GIVEN IT, over the area's ground: its marks in a `picture`
/// group, each by its place there; each named place in a `parts` group, under a record asking its
/// name, the marks meeting it painted again there so a press lands on it wherever it shows; each
/// run's selection and caret in a `marks` group. Rounded corners, dashes and curves are paths where
/// the room is light; a room of characters is given rectangles and lines (`Mark`, `Measure`).
inline Sketched sketch_of(const Drawing& d, const ws::CanvasRoom& room) {
    Sketched out;
    const Measure m(room);
    // EACH PLACE UNDER ITS NAME, as Workshop gathers a pane's parts: one a name another took, or
    // the judge refuses, stays a place unnamed, and past the bound the earliest go.
    std::vector<ws::CanvasPart> parts = named(d).parts;
    std::erase_if(parts, [](const ws::CanvasPart& p) { return p.w <= 0 || p.h <= 0; });
    // A mark as nodes, at `path`, drawn `dx`, `dy` back from where the picture stands it.
    const auto put = [&](gfx::Sketch& s, const std::string& path, const Mark& k, std::int64_t dx,
                         std::int64_t dy) {
        const auto x = k.x - dx, y = k.y - dy;
        switch (k.op) {
        case Mark::Op::Rect:
            // EVERY MARK KEEPS ONE NAME, whatever it draws: a window paints a name it has not held
            // after every other, so a name that came and went would paint over the words beside it.
            if (m.light && k.dashed) {
                const Box b{x, y, k.w, k.h};
                s.group(path, 0, 0);
                if (!k.fill.none()) s.path(path + "/0", rounded(b, k.radius), k.fill, gfx::no_paint());
                if (!k.stroke.none()) s.path(path + "/1", dashes(b, k.radius), gfx::no_paint(), k.stroke);
            } else if (m.light && k.radius > 0) {
                s.path(path, rounded(Box{x, y, k.w, k.h}, k.radius), k.fill, k.stroke);
            } else {
                s.rect(path, x, y, k.w, k.h, k.fill, k.dashed ? gfx::no_paint() : k.stroke);
            }
            break;
        case Mark::Op::Ellipse:
            s.ellipse(path, x, y, k.w, k.h, k.fill, k.stroke);
            break;
        case Mark::Op::Line:
            s.line(path, x, y, gfx::add_cells(x, k.w), gfx::add_cells(y, k.h), k.stroke);
            break;
        case Mark::Op::Path: {
            // A path's data stands in the area's pixels: drawn back by an offset, it is drawn
            // under a group that moves it.
            if (dx == 0 && dy == 0) {
                s.path(path, k.words, k.fill, k.stroke);
            } else {
                s.group(path, -dx, -dy);
                s.path(path + "/0", k.words, k.fill, k.stroke);
            }
            break;
        }
        case Mark::Op::Text:
            s.text(path, x + m.inset, y + m.inset, k.words, k.fill);
            break;
        }
    };
    // What a window would keep, judged before anything is drawn: each mark's nodes as `put` draws
    // them, in the picture and again in each place it meets.
    const auto count = [&](const Mark& k, bool moved) -> std::size_t {
        if (k.op == Mark::Op::Rect && m.light && k.dashed)
            return 1 + (k.fill.none() ? 0u : 1u) + (k.stroke.none() ? 0u : 1u);
        if (k.op == Mark::Op::Path && moved) return 2;
        return 1;
    };
    // WHAT A PLACE CARRIES: each mark that meets it, but a run of words only where its whole row
    // stands inside the place, since a window draws a run whole once its row begins inside a group,
    // and a run a place only grazes would stand there over what was drawn after it.
    const auto carries = [](const Box& place, const Mark& k) {
        if (!k.bounds().meets(place)) return false;
        return k.op != Mark::Op::Text || (k.y >= place.y && k.y + k.h <= place.bottom());
    };
    std::size_t nodes = 3, records = 0;
    for (const auto& k : d.marks)
        nodes += k.clip && k.op != Mark::Op::Text && k.op != Mark::Op::Rect ? 1 + count(k, true) : count(k, false);
    for (const auto& p : parts) {
        const Box at{p.x, p.y, p.w, p.h};
        records += p.name.empty() ? std::size_t{0} : std::size_t{1};
        ++nodes;
        for (const auto& k : d.marks) nodes += carries(at, k) ? count(k, true) : std::size_t{0};
        if (nodes > gfx::kMaxNodesPerArea || records > gfx::kMaxElementsPerArea) break;
    }
    std::size_t carets = 0;
    for (const auto& k : d.marks)
        if (k.op == Mark::Op::Text)
            carets += (k.caret_col >= 0 ? std::size_t{2} : std::size_t{0}) +
                      (k.sel_begin_col >= 0 ? std::size_t{2} : std::size_t{0});
    nodes += carets + (carets > 0 ? 1 : 0);
    if (nodes > gfx::kMaxNodesPerArea || records > gfx::kMaxElementsPerArea) {
        // A drawing past an area's bounds is a ground and that sentence alone, never one the window
        // refuses.
        out.over = "this picture is more than a graphics window keeps, " +
                   std::to_string(gfx::kMaxNodesPerArea) + " nodes and " +
                   std::to_string(gfx::kMaxElementsPerArea) + " pressed parts";
        out.sketch.fill_rect("ground", 0, 0, room.width, room.height, hue::body());
        out.sketch.group("picture", 0, 0);
        out.sketch.text("picture/0", 0, 0, out.over, hue::warn());
        return out;
    }
    out.sketch.fill_rect("ground", 0, 0, room.width, room.height, hue::body());
    out.sketch.group("picture", 0, 0);
    for (std::size_t i = 0; i < d.marks.size(); ++i) {
        const auto& k = d.marks[i];
        const auto path = "picture/" + std::to_string(i);
        if (k.clip && k.op != Mark::Op::Text && k.op != Mark::Op::Rect) {
            out.sketch.group(path, k.clip->x, k.clip->y, k.clip->w, k.clip->h);
            put(out.sketch, path + "/0", k, k.clip->x, k.clip->y);
        } else {
            put(out.sketch, path, k, 0, 0);
        }
    }
    out.sketch.group("parts", 0, 0);
    std::size_t at = 0;
    for (const auto& p : parts) {
        const Box place{p.x, p.y, p.w, p.h};
        const std::string group = "parts/" + std::to_string(at++);
        out.sketch.group(group, p.x, p.y, p.w, p.h);
        // A place unnamed asks nothing, so where it lies over another the window gives no press
        // to the one beneath, as the pane reads one.
        if (!p.name.empty()) out.sketch.action(group, p.name);
        std::size_t n = 0;
        for (const auto& k : d.marks) {
            if (!carries(place, k)) continue;
            put(out.sketch, group + "/" + std::to_string(n++), k, p.x, p.y);
        }
    }
    if (carets > 0) out.sketch.group("marks", 0, 0);
    for (std::size_t i = 0; i < d.marks.size(); ++i) {
        const auto& k = d.marks[i];
        if (k.op != Mark::Op::Text) continue;
        const auto gx = k.x + m.inset, gy = k.y + m.inset;
        const auto length = static_cast<std::int64_t>(k.words.size());
        // Each asks what the last named place under it asks, so a press on it reaches what lies
        // beneath.
        const auto asks = [&](std::int64_t px, std::int64_t py) {
            std::string name;
            for (const auto& p : parts)
                if (!p.name.empty() && px >= p.x && px < p.x + p.w && py >= p.y && py < p.y + p.h)
                    name = p.name;
            return name;
        };
        const auto from = std::max<std::int64_t>(0, k.sel_begin_col);
        const auto to = std::min(length, k.sel_end_col);
        if (k.sel_begin_col >= 0 && from < to) {
            const auto node = "marks/sel" + std::to_string(i);
            const auto x = gx + from * m.ch;
            out.sketch.text(node, x, gy,
                            k.words.substr(static_cast<std::size_t>(from),
                                           static_cast<std::size_t>(to - from)),
                            gfx::by_role(ink::kAccent));
            if (const auto name = asks(x, gy); !name.empty()) out.sketch.action(node, name);
        }
        if (k.caret_col >= 0 && k.caret_col <= length) {
            const auto node = "marks/caret" + std::to_string(i);
            const auto x = gx + k.caret_col * m.ch;
            const auto c = static_cast<std::size_t>(k.caret_col);
            if (!m.light && c < k.words.size() && k.words[c] != ' ')
                out.sketch.text(node, x, gy, k.words.substr(c, 1), gfx::by_role(ink::kAccent));
            else
                out.sketch.line(node, x, gy, x, gy + m.line - 1, gfx::by_role(ink::kAccent));
            if (const auto name = asks(x, gy); !name.empty()) out.sketch.action(node, name);
        }
    }
    return out;
}

} // namespace zengine::flow_pane
#endif
