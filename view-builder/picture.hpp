// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_BUILDER_PICTURE_HPP
#define ZENGINE_VIEW_BUILDER_PICTURE_HPP

// The View Builder's own picture: its bar, the kinds it makes, the element list and the selected
// element's values in boxes, beside a design canvas where the view is drawn by its own picture
// code at its own pixels. Over that canvas the builder draws marks alone, never an element.
// Law: agents/view.md.

#include "view-builder/model.hpp"
#include "view-builder/vocabulary.hpp"
#include "view/view.hpp"

#include "component/text_box.hpp"
#include "surface/pointing.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/pane_canvas_vocabulary.hpp"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace zengine::view_builder {
namespace ws = zengine::workshop;
namespace ink = zengine::surface::role;

/// A rectangle of the picture in local subunits.
struct Area {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    bool empty() const noexcept { return w <= 0 || h <= 0; }
    bool contains(std::int64_t px, std::int64_t py, std::int64_t grain) const {
        return surface::sub_span_contains(x, w, px, grain) && surface::sub_span_contains(y, h, py, grain);
    }
    Area within(const Area& o) const {
        const auto left = std::max(x, o.x), top = std::max(y, o.y);
        const auto right = std::min(x + w, o.x + o.w), bottom = std::min(y + h, o.y + o.h);
        return {left, top, right - left, bottom - top};
    }
};

/// One pressable place in a picture, and what pressing it means.
struct Hit {
    Area at;
    std::string action;
    std::vector<std::string> args;
};

struct Picture {
    ws::PaneCanvasContent content;
    std::vector<Hit> hits;
    std::int64_t grain = 1;
    /// Where the view is drawn: nothing of it outside this area.
    Area design;
    /// The view's own room as the canvas places it: its pixel 0,0 is this corner, the design
    /// area's corner less the pan, and it reaches to the design area's far edges.
    Area view;
    const Hit* hit(std::int64_t x, std::int64_t y) const {
        for (auto at = hits.rbegin(); at != hits.rend(); ++at)
            if (at->at.contains(x, y, grain)) return &*at;
        return nullptr;
    }
};

/// A value with a box, being typed into: which value, of which element, and its text as typed.
struct Box {
    std::string field;                  ///< id, label, text, x, y, w or h; name, path or intent
    std::optional<std::size_t> element; ///< the element the value is one of; none for the view's
    component::TextBox text;
};

/// The builder's own presentation, never saved nor kept across a reload: the box being typed
/// into, the element the pointer rests on, the label a carried value would land on, a kind being
/// dragged from the palette, a New or Open waiting for its second press, the first field a
/// choice of fields shows, how far the design canvas is panned, and the edges a snap met.
struct Presentation {
    std::optional<Box> box;
    std::optional<std::size_t> hovered, landing;
    std::size_t choices_from = 0;
    struct Ghost {
        view::Kind kind = view::Kind::label;
        std::int64_t x = 0, y = 0;
    };
    std::optional<Ghost> ghost;
    std::string armed;
    /// How far into the view the design canvas looks, in whole pixels from its top left corner.
    std::int64_t pan_x = 0, pan_y = 0;
    /// The edge of another element a place by hand came to, in the view's pixels, while held.
    std::optional<std::int64_t> met_x, met_y;
};

/// The values an element shows in boxes, in the order Tab walks them.
inline std::vector<std::string> element_fields(const view::Element& e) {
    if (e.kind == view::Kind::number) return {"id", "label", "text", "x", "y", "w", "h"};
    return {"id", "label", "x", "y", "w", "h"};
}

/// An element's value as its box shows it.
inline std::string value_of(const view::Element& e, const std::string& field) {
    if (field == "id") return e.id;
    if (field == "label") return e.label;
    if (field == "text") return e.text;
    if (field == "x") return std::to_string(e.x);
    if (field == "y") return std::to_string(e.y);
    if (field == "w") return std::to_string(e.w);
    if (field == "h") return std::to_string(e.h);
    return {};
}

/// An intent's name inside its view's, as its box holds it.
inline std::string intent_name(const view::Description& d, const view::Intent& in) {
    const auto& name = in.shape->name();
    return name.size() > d.name.size() ? name.substr(d.name.size() + 1) : name;
}

/// One field of a choice, laid as `[name]` at a row and a column from the choice's corner.
struct Choice {
    std::size_t field = 0;
    std::int64_t row = 0, col = 0;
};

/// A CHOICE OF FIELDS laid in rows `width` columns wide, from field `from`, in at most `rows`
/// rows: a button that would pass the row's end starts the next, so one too wide for any row has
/// a row of its own.
inline std::vector<Choice> lay_choices(const std::vector<std::string>& fields, std::size_t from, std::int64_t width,
                                       std::int64_t rows) {
    std::vector<Choice> out;
    std::int64_t row = 0, col = 0;
    for (auto i = from; i < fields.size(); ++i) {
        const auto w = static_cast<std::int64_t>(fields[i].size()) + 2;
        if (col > 0 && col + w > width) {
            ++row;
            col = 0;
        }
        if (row >= rows) break;
        out.push_back({i, row, col});
        col += w + 1;
    }
    return out;
}

/// Where an element sits on the canvas, in local subunits: its whole pixels from the corner of the
/// view's room as the canvas places it (`Picture::view`), as the view's own picture places it.
inline Area element_area(const Area& view, const view::Element& e) {
    return {view.x + surface::subs_of_pixel(e.x), view.y + surface::subs_of_pixel(e.y),
            surface::subs_of_pixel(e.w), surface::subs_of_pixel(e.h)};
}

/// How far the design canvas pans, in whole pixels: from the view's top left corner as far as
/// puts the farthest element's far edge at the middle of the canvas.
inline std::pair<std::int64_t, std::int64_t> pan_reach(const view::Description& d, const Area& design) {
    std::int64_t right = 0, bottom = 0;
    for (const auto& e : d.elements) {
        right = std::max(right, e.x + e.w);
        bottom = std::max(bottom, e.y + e.h);
    }
    const auto half = [](std::int64_t subs) { return surface::floor_div_px(subs, surface::kPixelGrainSubs) / 2; };
    return {std::max<std::int64_t>(0, right - half(design.w)), std::max<std::int64_t>(0, bottom - half(design.h))};
}

/// THE SNAP of a place made, moved or resized by hand: an edge the hand moves comes to an edge of
/// another element within `kSnapReach` pixels, else to the nearest line of a grid `kSnapGrid`
/// pixels apart, a cell, so a snapped place sits on a terminal's lattice too. The arrow keys and
/// a typed value place exactly.
inline constexpr std::int64_t kSnapGrid = surface::kCanvasCellPx;
inline constexpr std::int64_t kSnapReach = 6;

/// Which edges of an element the hand moves along one axis: both (a move), the low or the high
/// one (a side or a corner of a resize), or neither.
enum class Edges { both, low, high, none };

/// One axis of a place: where it begins, how far it reaches, and the other element's edge it
/// came to when that, not the grid, placed it.
struct Snapped {
    std::int64_t at = 0, size = 0;
    std::optional<std::int64_t> met;
};

/// One axis of a place by hand, from `at` and `size` wide, its `moving` edges snapped against the
/// other elements' edges on that axis. A snap that would leave the view's rules is not taken.
inline Snapped snap_axis(std::int64_t at, std::int64_t size, Edges moving, const std::vector<std::int64_t>& others) {
    if (moving == Edges::none) return {at, size, std::nullopt};
    const auto end = at + size;
    const auto travelled = [&](std::int64_t travel) -> std::optional<Snapped> {
        Snapped s{at, size, std::nullopt};
        if (moving != Edges::high) s.at = at + travel;
        if (moving == Edges::low) s.size = end - s.at;
        if (moving == Edges::high) s.size = size + travel;
        if (s.at < 0 || s.size < 1 || s.at > view::kMaxPixels || s.size > view::kMaxPixels) return std::nullopt;
        return s;
    };
    std::optional<Snapped> best;
    auto nearest = kSnapReach + 1;
    const auto meet = [&](std::int64_t edge) {
        for (const auto to : others) {
            if (std::abs(to - edge) >= nearest) continue;
            if (auto s = travelled(to - edge)) {
                nearest = std::abs(to - edge);
                s->met = to;
                best = s;
            }
        }
    };
    if (moving != Edges::high) meet(at);
    if (moving != Edges::low) meet(end);
    if (best) return *best;
    const auto edge = moving == Edges::high ? end : at;
    const auto below = surface::floor_div_px(edge, kSnapGrid) * kSnapGrid;
    const bool nearer_below = edge - below < below + kSnapGrid - edge;
    for (const auto line : {nearer_below ? below : below + kSnapGrid, nearer_below ? below + kSnapGrid : below})
        if (auto s = travelled(line - edge)) return *s;
    return {at, size, std::nullopt};
}

/// A place by hand, in whole pixels, and the other elements' edges it came to.
struct Place {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::optional<std::int64_t> met_x, met_y;
};

/// A place by hand snapped on both axes against every element of `d` but `placing`, the one
/// being placed, whose own edges never pull it.
inline Place snap(const view::Description& d, std::optional<std::size_t> placing, const Place& at, Edges along_x,
                  Edges along_y) {
    std::vector<std::int64_t> xs, ys;
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        if (placing && *placing == i) continue;
        const auto& e = d.elements[i];
        xs.insert(xs.end(), {e.x, e.x + e.w});
        ys.insert(ys.end(), {e.y, e.y + e.h});
    }
    const auto x = snap_axis(at.x, at.w, along_x, xs);
    const auto y = snap_axis(at.y, at.h, along_y, ys);
    return {x.at, y.at, x.size, y.size, x.met, y.met};
}

inline std::string clean(std::string text) {
    for (auto& c : text)
        if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) c = '?';
    return text;
}

/// THE PICTURE. Words sit in text rows and columns at the medium's measured advance and padded
/// line height; the design area begins on a cell boundary, so the view's own picture, moved
/// there whole, keeps its lattice in a window and in a terminal alike. The box being typed into
/// keeps its caret in view at the width it is drawn, in `p` itself: a press reads that scroll.
inline Picture picture(const Model& m, Presentation& p, const ws::PaneCanvasRoom& room, std::int64_t number) {
    Picture out;
    out.content.pane = kPane;
    out.content.grant = room.grant;
    out.content.picture = number;
    out.grain = std::max<std::int64_t>(1, room.grain);
    out.content.rects.push_back({0, 0, room.width, room.height, ink::kGround});
    const auto metrics = ws::canvas_text_metrics(room);
    const auto advance = std::max<std::int64_t>(1, metrics.advance);
    const auto line = surface::add_cells(metrics.line, 2 * metrics.inset);
    const auto columns = std::max<std::int64_t>(1, (room.width - 2 * metrics.inset) / advance);
    const auto rows = std::max<std::int64_t>(1, room.height / std::max<std::int64_t>(1, line));
    const auto& d = m.description;
    const bool chosen = m.selected && *m.selected < d.elements.size();

    // THE LAYOUT: the bar across the top; on the left the view's name and file, the kinds and the
    // element list; the design area beside it; the selected element's values in a column of their
    // own on the right where the room is wide, else below the list; the status and the notice
    // across the last three rows.
    const auto cell = surface::kCellSubs;
    const auto up_to_cell = [&](std::int64_t v) { return (v + cell - 1) / cell * cell; };
    const auto rule = room.graphical ? surface::kPixelGrainSubs : out.grain;
    const bool beside = columns >= 110;
    const auto side = beside ? std::clamp<std::int64_t>(columns / 4, 30, 36)
                             : std::min(columns, std::clamp<std::int64_t>(columns * 2 / 5, 30, 40));
    const auto details = beside ? std::clamp<std::int64_t>(columns / 4, 30, 40) : std::int64_t{0};
    const auto details_col = columns - details;
    const auto status = std::max<std::int64_t>(1, rows - 3);
    out.design.x = up_to_cell(side * advance + 2 * metrics.inset + rule);
    out.design.y = up_to_cell(line + rule);
    out.design.w = (beside ? details_col * advance - rule : room.width) - out.design.x;
    out.design.h = status * line - out.design.y;

    // A line keeps to its column, from `from` to `limit`, and a row of a column above the status.
    std::int64_t from = 0, limit = columns, stop = rows;
    const auto put = [&](std::int64_t col, std::int64_t row, std::string text, std::int64_t role = ink::kFill) {
        if (row < 0 || row >= stop) return std::int64_t{0};
        ws::PaneCanvasText run{col * advance, row * line, clean(std::move(text)), role};
        const auto left = from * advance;
        const ws::CanvasTextBox clip{left, 0, std::min(room.width, limit * advance + 2 * metrics.inset) - left, room.height};
        auto placed = ws::clip_canvas_text(run, clip, room);
        if (placed.visible()) out.content.texts.push_back(placed.text);
        return static_cast<std::int64_t>(run.text.size());
    };
    const auto press = [&](std::int64_t col, std::int64_t row, std::int64_t width, std::string action,
                           std::vector<std::string> args = {}) {
        if (row < 0 || row >= stop) return;
        out.hits.push_back({{col * advance, row * line, width * advance + 2 * metrics.inset, line},
                            std::move(action), std::move(args)});
    };
    const auto button = [&](std::int64_t col, std::int64_t row, const std::string& title, std::string action,
                            std::vector<std::string> args = {}) {
        const auto width = put(col, row, "[" + title + "]", ink::kAccent);
        press(col, row, width, std::move(action), std::move(args));
        return col + width + 1;
    };
    // A VALUE IN A BOX, edited where it is: a quiet box with the value on it, its caret shown
    // while it is being typed into. Pressing it is how a weaver starts typing there.
    const auto box = [&](std::int64_t col, std::int64_t row, std::int64_t width, const std::string& field,
                         std::optional<std::size_t> element, const std::string& value) {
        if (row < 1 || row >= status) return col + width + 1;
        const bool typing = p.box && p.box->field == field && p.box->element == element;
        const Area at{col * advance, row * line, width * advance + 2 * metrics.inset, line};
        out.content.rects.push_back({at.x, at.y, at.w, at.h, ink::kMuted});
        std::string shown = value;
        std::int64_t caret = surface::kNoCaret;
        if (typing) {
            auto& text = p.box->text;
            text.keep_caret_visible(width);
            shown = text.visible(width);
            caret = static_cast<std::int64_t>(text.caret_column());
        }
        ws::PaneCanvasText run{at.x, at.y, clean(shown), typing ? ink::kAccent : ink::kFill, caret};
        auto placed = ws::clip_canvas_text(run, {at.x, at.y, at.w, at.h}, room);
        if (placed.visible()) out.content.texts.push_back(placed.text);
        out.hits.push_back({at, "box", {field, element ? std::to_string(*element) : std::string()}});
        return col + width + 1;
    };

    // The bar. A New or Open over an unsaved view waits for its second press.
    std::int64_t col = 0;
    col = button(col, 0, p.armed == "new" ? "New: discard?" : "New", "new");
    col = button(col, 0, p.armed == "open" ? "Open: discard?" : "Open", "open");
    col = button(col, 0, m.dirty ? "Save*" : "Save", "save");
    col = button(col, 0, m.running ? "Running" : "Run", "run");
    col = button(col, 0, "Apply", "apply");
    button(col, 0, "Stop", "stop");

    // WORDS BEFORE A CONTROL give way to it: cut to `fit` columns, ending `...`, so the control
    // after them is drawn whole inside its column, where it can be pressed.
    const auto giving_way = [](std::string words, std::int64_t fit) {
        const auto keep = static_cast<std::size_t>(std::max<std::int64_t>(0, fit));
        if (words.size() <= keep) return words;
        return keep <= 3 ? words.substr(0, keep) : words.substr(0, keep - 3) + "...";
    };

    // THE SELECTED ELEMENT'S VALUES, from column `at` and `width` columns wide, from `row` down:
    // the rows they took.
    const auto values = [&](std::int64_t at, std::int64_t width, std::int64_t row) {
        const auto top = row;
        const auto i = *m.selected;
        const auto index = std::to_string(i);
        const auto& e = d.elements[i];
        const auto head = giving_way(e.id + " (" + view::kind_word(e.kind) + ")", width - 9);
        put(at, row, head + " ", ink::kAccent);
        button(at + static_cast<std::int64_t>(head.size()) + 1, row, "Remove", "remove", {index});
        ++row;
        const auto fields = element_fields(e);
        for (const auto* name : {"id", "label", "text"}) {
            if (std::find(fields.begin(), fields.end(), name) == fields.end()) continue;
            put(at, row, name);
            box(at + 6, row, std::max<std::int64_t>(4, width - 7), name, i, value_of(e, name));
            ++row;
        }
        const auto pair = [&](const char* a, const char* b) {
            put(at, row, a);
            const auto next = box(at + 2, row, 6, a, i, value_of(e, a));
            put(next + 1, row, b);
            box(next + 3, row, 6, b, i, value_of(e, b));
            ++row;
        };
        pair("x", "y");
        pair("w", "h");
        if (e.kind == view::Kind::label) {
            if (m.choosing && m.choosing->element == e.id) {
                // EVERY FIELD IT COULD SHOW, wrapped within the column; when the rows left cannot
                // hold them, the last is More, which shows the next of them and then the first.
                put(at, row++, "show which field of " + m.choosing->shape->name() + "?");
                const auto choices = showable(*m.choosing->shape);
                const auto first = p.choices_from < choices.size() ? p.choices_from : 0;
                const auto left = stop - row;
                auto laid = lay_choices(choices, first, width, left);
                const bool paged = first > 0 || first + laid.size() < choices.size();
                if (paged && left > 1) laid = lay_choices(choices, first, width, left - 1);
                for (const auto& c : laid) button(at + c.col, row + c.row, choices[c.field], "show", {index, choices[c.field]});
                row += laid.empty() ? 0 : laid.back().row + 1;
                if (paged) {
                    const auto next = first + laid.size() < choices.size() ? first + laid.size() : 0;
                    button(at, row++, "More", "choices", {std::to_string(next)});
                }
            } else if (const auto* s = d.shown(e.id)) {
                const auto bound = giving_way("shows " + s->shape->name() + "." + s->field, width - 9);
                const auto said = static_cast<std::int64_t>(bound.size());
                put(at, row, bound + " ");
                button(at + said + 1, row, "Unshow", "unshow", {index});
                press(at, row, said, "shows", {index});
                ++row;
            } else {
                put(at, row, "shows nothing: drag a shape here", ink::kMuted);
                press(at, row, width, "shows", {index});
                ++row;
            }
        } else if (e.kind == view::Kind::button) {
            if (const auto* intent = d.intent(e.id)) {
                // WHAT IT SAYS: its words are the handle a drag carries its shape out by, and a
                // right press offers to carry it; the name after them is a value in a box.
                const auto whole = "says " + d.name + ".";
                const auto boxed = std::max<std::int64_t>(std::min<std::int64_t>(12, width - 2),
                                                          width - static_cast<std::int64_t>(whole.size()) - 2);
                const auto says = giving_way(whole, width - boxed - 2);
                const auto said = static_cast<std::int64_t>(says.size());
                put(at, row, says, ink::kAccent);
                press(at, row, said, "says", {index});
                box(at + said + 1, row, boxed, "intent", i, intent_name(d, *intent));
                ++row;
                std::string carried;
                for (const auto& f : intent->shape->fields())
                    carried += (carried.empty() ? "" : ", ") + f.name + ": " + loom::name_of(f.type.kind);
                for (const auto& part : view::wrap("{" + carried + "}", width - 2, 2)) {
                    put(at, row, "  " + part);
                    press(at, row, width, "says", {index});
                    ++row;
                }
                col = button(at, row, "Make intent", "intent", {index});
                button(col, row, "Drop intent", "drop-intent", {index});
                ++row;
                put(at, row++, "drag `says` out, or right-press it", ink::kMuted);
            } else {
                put(at, row++, "says nothing yet", ink::kMuted);
                button(at, row++, "Make intent", "intent", {index});
            }
        }
        return row - top;
    };

    // THE LEFT COLUMN: the view's name and file, the kinds to drag, then the list.
    limit = side;
    stop = status;
    const auto wide = std::max<std::int64_t>(4, side - 6);
    put(0, 1, "View");
    box(5, 1, wide, "name", std::nullopt, d.name);
    put(0, 2, "File");
    box(5, 2, wide, "path", std::nullopt, m.path);
    col = put(0, 3, "Add ");
    for (const auto kind : {view::Kind::label, view::Kind::number, view::Kind::button}) {
        const std::string word = view::kind_word(kind);
        const std::string title = kind == view::Kind::number ? "Number" : kind == view::Kind::button ? "Button" : "Label";
        col = button(col, 3, title, "kind", {word});
    }
    std::int64_t row = 5;
    put(0, row++, "Elements (" + std::to_string(d.elements.size()) + ")", ink::kAccent);
    // Stacked, the values take the rows below the list, which keeps a few rows whatever they
    // need; the list follows the selected element.
    std::int64_t below = 0;
    if (chosen && !beside) {
        const auto& e = d.elements[*m.selected];
        below = 1 + (e.kind == view::Kind::number ? 3 : 2) + 2;
        below += e.kind == view::Kind::button ? 4 : e.kind == view::Kind::label ? 2 : 0;
        if (e.kind == view::Kind::label && m.choosing && m.choosing->element == e.id) {
            const auto laid = lay_choices(showable(*m.choosing->shape), 0, side, rows);
            below += laid.empty() ? 0 : laid.back().row;
        }
    }
    const auto few = std::min<std::int64_t>(4, std::max<std::int64_t>(1, static_cast<std::int64_t>(d.elements.size())));
    const auto list_end = std::max(row + few, beside ? status : status - below - 1);
    const auto shown_rows = static_cast<std::size_t>(std::max<std::int64_t>(1, list_end - row));
    auto first = std::min(m.first_row, d.elements.size());
    if (chosen) {
        if (*m.selected < first) first = *m.selected;
        if (*m.selected >= first + shown_rows) first = *m.selected + 1 - shown_rows;
    }
    for (std::size_t i = first; i < d.elements.size() && row < list_end; ++i) {
        const auto& e = d.elements[i];
        const bool is_chosen = m.selected && *m.selected == i;
        if ((p.hovered && *p.hovered == i) || (p.landing && *p.landing == i))
            out.content.rects.push_back({0, row * line, side * advance + 2 * metrics.inset, line, ink::kMuted});
        put(0, row, std::string(is_chosen ? "> " : "  ") + e.id + "  " + view::kind_word(e.kind) + "  \"" +
                        e.label + "\"",
            is_chosen ? ink::kAccent : ink::kFill);
        press(0, row, side, "select", {std::to_string(i)});
        ++row;
    }
    if (d.elements.empty()) put(0, row++, "  none yet: drag a kind in", ink::kMuted);
    if (chosen && !beside) (void)values(0, side, std::max(row, list_end) + 1);
    if (beside) {
        // THE DETAILS COLUMN: the selected element's values, or what selecting one shows.
        from = details_col;
        limit = columns;
        if (chosen) (void)values(details_col, details, 1);
        else put(details_col, 1, "select an element to see its values", ink::kMuted);
    }
    from = 0;
    limit = columns;
    stop = rows;

    // THE DESIGN AREA: the view drawn by its own picture code, in a room that reaches from its top
    // left corner to the area's far edges with the medium's metrics, moved there whole, less the
    // pan; nothing of it outside the area; then the builder's marks over it. The pan is kept in
    // `p` within its reach, and moves the view by whole grains, so a terminal pans by cells.
    const auto& area = out.design;
    if (!area.empty()) {
        out.hits.push_back({area, "canvas", {}});
        out.content.rects.push_back({area.x, area.y, area.w, area.h, ink::kGround});
        out.content.rects.push_back({area.x - rule, area.y - rule, area.w + rule, rule, ink::kMuted});
        out.content.rects.push_back({area.x - rule, area.y, rule, area.h, ink::kMuted});
        if (beside) out.content.rects.push_back({area.x + area.w, area.y - rule, rule, area.h + rule, ink::kMuted});
        const auto [reach_x, reach_y] = pan_reach(d, area);
        p.pan_x = std::clamp<std::int64_t>(p.pan_x, 0, reach_x);
        p.pan_y = std::clamp<std::int64_t>(p.pan_y, 0, reach_y);
        const auto grained = [&](std::int64_t px) { return surface::floor_div_px(surface::subs_of_pixel(px), out.grain) * out.grain; };
        const auto left = grained(p.pan_x), top = grained(p.pan_y);
        out.view = {area.x - left, area.y - top, left + area.w, top + area.h};
        const ws::PaneCanvasRoom inner{room.pane, room.grant, out.view.w, out.view.h, room.grain,
                                       room.graphical, room.text_advance_px, room.text_line_px};
        const auto drawn = view::picture(d, {}, view::Presentation{}, inner, number);
        for (auto r : drawn.content.rects) {
            const auto inside = Area{r.x + out.view.x, r.y + out.view.y, r.w, r.h}.within(area);
            if (inside.empty()) continue;
            r.x = inside.x;
            r.y = inside.y;
            r.w = inside.w;
            r.h = inside.h;
            out.content.rects.push_back(r);
        }
        for (auto t : drawn.content.texts) {
            t.x += out.view.x;
            t.y += out.view.y;
            auto placed = ws::clip_canvas_text(t, {area.x, area.y, area.w, area.h}, room);
            if (placed.visible()) out.content.texts.push_back(placed.text);
        }
        const auto mark = [&](const Area& a) {
            const auto inside = a.within(area);
            if (!inside.empty()) out.content.rects.push_back({inside.x, inside.y, inside.w, inside.h, ink::kAccent});
        };
        const auto outline = [&](const Area& a, std::int64_t role, std::int64_t t) {
            for (const Area& side_of : {Area{a.x - t, a.y - t, a.w + 2 * t, t}, Area{a.x - t, a.y + a.h, a.w + 2 * t, t},
                                        Area{a.x - t, a.y, t, a.h}, Area{a.x + a.w, a.y, t, a.h}}) {
                const auto inside = side_of.within(area);
                if (!inside.empty()) out.content.rects.push_back({inside.x, inside.y, inside.w, inside.h, role});
            }
        };
        const auto thin = room.graphical ? surface::kPixelGrainSubs : out.grain;
        for (std::size_t i = 0; i < d.elements.size(); ++i) {
            const auto at = element_area(out.view, d.elements[i]).within(area);
            if (!at.empty()) out.hits.push_back({at, "element", {std::to_string(i)}});
        }
        // The element the pointer rests on, in the fill's ink: a quiet field's own box is muted.
        if (p.hovered && *p.hovered < d.elements.size() && (!chosen || *p.hovered != *m.selected))
            outline(element_area(out.view, d.elements[*p.hovered]), ink::kFill, thin);
        if (p.landing && *p.landing < d.elements.size())
            outline(element_area(out.view, d.elements[*p.landing]), ink::kAccent, 2 * thin);
        if (p.ghost) {
            const auto [w, h] = made_size(p.ghost->kind);
            outline(element_area(out.view, view::Element{"", p.ghost->kind, "", p.ghost->x, p.ghost->y, w, h, ""}),
                    ink::kMuted, thin);
        }
        // THE EDGE A SNAP MET, while the hand holds what it places: a line across the canvas.
        if (p.met_x) mark({out.view.x + surface::subs_of_pixel(*p.met_x), area.y, thin, area.h});
        if (p.met_y) mark({area.x, out.view.y + surface::subs_of_pixel(*p.met_y), area.w, thin});
        if (chosen) {
            // THE SELECTED ELEMENT: marked, with a handle on each side, which moves that side alone,
            // and one at each corner, which moves the two sides it joins; where a side's handle and
            // a corner's meet, the corner's is pressed. Each handle names its sides: -1 the left or
            // top, 1 the right or bottom, 0 neither.
            const auto at = element_area(out.view, d.elements[*m.selected]);
            outline(at, ink::kAccent, thin);
            const auto size = room.graphical ? surface::subs_of_pixel(8) : out.grain;
            const auto down = [&](std::int64_t v) { return surface::floor_div_px(v, out.grain) * out.grain; };
            const std::pair<int, int> sides[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
            for (const auto& [sx, sy] : sides) {
                Area handle;
                if (room.graphical) {
                    const auto cx = sx < 0 ? at.x : sx > 0 ? at.x + at.w : down(at.x + at.w / 2);
                    const auto cy = sy < 0 ? at.y : sy > 0 ? at.y + at.h : down(at.y + at.h / 2);
                    handle = {cx - size / 2, cy - size / 2, size, size};
                } else {
                    // A terminal's corner handle is the element's own corner cell; a side's is the
                    // cell beside that side's middle, outside the element, so it is never a corner's,
                    // or the side's own middle cell where the canvas ends there.
                    const auto next_to = [&](int s, int other, std::int64_t begin, std::int64_t extent,
                                             std::int64_t area_begin, std::int64_t area_extent) {
                        const auto low = down(begin), high = down(begin + extent - 1);
                        if (s == 0) return std::min(down(begin + extent / 2), high);
                        if (other != 0) return s < 0 ? low : high;
                        const auto outside = s < 0 ? low - out.grain : high + out.grain;
                        return outside >= area_begin && outside < area_begin + area_extent ? outside : s < 0 ? low : high;
                    };
                    handle = {next_to(sx, sy, at.x, at.w, area.x, area.w), next_to(sy, sx, at.y, at.h, area.y, area.h),
                              size, size};
                }
                mark(handle);
                const auto inside = handle.within(area);
                if (!inside.empty())
                    out.hits.push_back({inside, "handle", {std::to_string(*m.selected), std::to_string(sx), std::to_string(sy)}});
            }
        }
    }

    // The status, then the notice on the last two rows: a host's answer is often long.
    put(0, status, d.name + (m.running ? " / running" : " / stopped") + (m.path.empty() ? std::string() : " / " + m.path),
        ink::kMuted);
    const auto notice = view::wrap(m.notice, columns, 2);
    for (std::size_t i = 0; i < notice.size(); ++i)
        put(0, status + 1 + static_cast<std::int64_t>(i), notice[i], ink::kAccent);
    // Rectangles a room cannot hold are dropped: a picture is admitted whole or not at all.
    std::erase_if(out.content.rects, [](const auto& r) { return r.w <= 0 || r.h <= 0; });
    return out;
}

} // namespace zengine::view_builder
#endif
