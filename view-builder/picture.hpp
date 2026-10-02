// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_BUILDER_PICTURE_HPP
#define ZENGINE_VIEW_BUILDER_PICTURE_HPP

// The View Builder's own picture: its bar, the element kinds, the element list, the selected
// element's properties, the field a label shows and the intent a button says. It draws lists of
// words, never the view: the view draws itself in its own pane. Law: agents/view.md.

#include "view-builder/model.hpp"
#include "view-builder/vocabulary.hpp"

#include "surface/pointing.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/pane_canvas_vocabulary.hpp"

#include <deque>
#include <string>
#include <vector>

namespace zengine::view_builder {
namespace ws = zengine::workshop;
namespace ink = zengine::surface::role;

/// One pressable place in a picture, and what pressing it means.
struct Hit {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::string action;
    std::vector<std::string> args;
    bool contains(std::int64_t px, std::int64_t py, std::int64_t grain) const {
        return surface::sub_span_contains(x, w, px, grain) && surface::sub_span_contains(y, h, py, grain);
    }
};

struct Picture {
    ws::PaneCanvasContent content;
    std::vector<Hit> hits;
    std::int64_t grain = 1;
    const Hit* hit(std::int64_t x, std::int64_t y) const {
        for (auto at = hits.rbegin(); at != hits.rend(); ++at)
            if (at->contains(x, y, grain)) return &*at;
        return nullptr;
    }
};

inline std::string clean(std::string text) {
    for (auto& c : text)
        if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) c = '?';
    return text;
}

/// A shape as one line: its name and its fields' names and kinds.
inline std::string shape_line(const loom::Schema& shape) {
    std::string fields;
    for (const auto& f : shape.fields())
        fields += (fields.empty() ? "" : ", ") + f.name + ": " + loom::name_of(f.type.kind);
    return shape.name() + " {" + fields + "}";
}

/// THE PICTURE, laid out in text rows and columns and placed at the medium's measured advance and
/// padded line height, so a column of words lines up in a window and in a terminal alike.
inline Picture picture(const Model& m, const ws::PaneCanvasRoom& room, std::int64_t number) {
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
    const auto put = [&](std::int64_t col, std::int64_t row, std::string text, std::int64_t role = ink::kFill,
                         std::int64_t caret = surface::kNoCaret) {
        if (row < 0 || row >= rows) return std::int64_t{0};
        ws::PaneCanvasText run{col * advance, row * line, clean(std::move(text)), role, caret};
        auto placed = ws::clip_canvas_text(run, {0, 0, room.width, room.height}, room);
        if (placed.visible()) out.content.texts.push_back(placed.text);
        return static_cast<std::int64_t>(run.text.size());
    };
    const auto press = [&](std::int64_t col, std::int64_t row, std::int64_t width, std::string action,
                           std::vector<std::string> args = {}) {
        if (row < 0 || row >= rows) return;
        out.hits.push_back({col * advance, row * line, width * advance + 2 * metrics.inset, line,
                            std::move(action), std::move(args)});
    };
    const auto button = [&](std::int64_t col, std::int64_t row, const std::string& title, std::string action,
                            std::vector<std::string> args = {}) {
        const auto width = put(col, row, "[" + title + "]", ink::kAccent);
        press(col, row, width, std::move(action), std::move(args));
        return col + width + 1;
    };
    // The bar, then what a weaver adds, then the view's name.
    std::int64_t col = 0, row = 0;
    const auto bar = [&](const std::string& title, const std::string& action) {
        if (col + static_cast<std::int64_t>(title.size()) + 2 > columns) {
            col = 0;
            ++row;
        }
        col = button(col, row, title, action);
    };
    bar("New", "ask-new");
    bar("Open", "ask-open");
    bar(m.dirty ? "Save*" : "Save", "ask-save");
    bar(m.running ? "Running" : "Run", "run");
    bar("Apply", "apply");
    bar("Stop", "stop");
    ++row;
    col = button(put(0, row, "View " + m.description.name + " ", ink::kAccent), row, "Rename", "ask-rename");
    ++row;
    col = put(0, row, "Add ");
    col = button(col, row, "Number", "add", {"number"});
    col = button(col, row, "Button", "add", {"button"});
    col = button(col, row, "Label", "add", {"label"});
    row += 2;
    const auto bottom = rows - 3;
    if (m.dialog) {
        put(0, row++, m.dialog->title, ink::kAccent);
        for (std::size_t i = 0; i < m.dialog->entries.size() && row < bottom; ++i) {
            const auto& e = m.dialog->entries[i];
            const auto prefix = (i == m.dialog->selected ? "> " : "  ") + e.label + ": ";
            auto text = e.text;
            const auto width = std::max<std::int64_t>(1, columns - static_cast<std::int64_t>(prefix.size()) - 1);
            text.keep_caret_visible(width);
            put(0, row, prefix + text.visible(width), ink::kFill,
                i == m.dialog->selected ? static_cast<std::int64_t>(prefix.size()) + static_cast<std::int64_t>(text.caret_column())
                                        : surface::kNoCaret);
            press(0, row, columns, "dialog-field", {std::to_string(i)});
            ++row;
        }
        ++row;
        col = button(0, row, "Confirm", "dialog-confirm");
        button(col, row, "Cancel", "dialog-cancel");
        put(0, row + 1, "Tab changes field; Enter confirms; Escape cancels", ink::kMuted);
    } else {
        put(0, row++, "Elements (" + std::to_string(m.description.elements.size()) + ")", ink::kAccent);
        const auto& d = m.description;
        // The list: one row per element, its kind, label, place and size. Selecting one shows its
        // properties beneath; a shape or a field carried onto a label's row is what it shows.
        const auto list_end = std::min<std::int64_t>(bottom - 6, row + static_cast<std::int64_t>(d.elements.size()));
        for (std::size_t i = m.first_row; i < d.elements.size() && row < list_end; ++i) {
            const auto& e = d.elements[i];
            const bool chosen = m.selected && *m.selected == i;
            const auto text = std::string(chosen ? "> " : "  ") + e.id + "  " + view::kind_word(e.kind) + "  \"" +
                              e.label + "\"  " + std::to_string(e.x) + "," + std::to_string(e.y) + " " +
                              std::to_string(e.w) + "x" + std::to_string(e.h);
            put(0, row, text, chosen ? ink::kAccent : ink::kFill);
            press(0, row, columns, "select", {std::to_string(i)});
            ++row;
        }
        if (d.elements.empty()) put(0, row++, "  none yet: add a number field, a button and a label", ink::kMuted);
        ++row;
        if (m.selected && *m.selected < d.elements.size() && row < bottom) {
            const auto i = std::to_string(*m.selected);
            const auto& e = d.elements[*m.selected];
            col = put(0, row, e.id + " (" + view::kind_word(e.kind) + ") ", ink::kAccent);
            col = button(col, row, "Edit", "ask-element", {i});
            col = button(col, row, "Up", "up", {i});
            col = button(col, row, "Down", "down", {i});
            button(col, row, "Remove", "remove", {i});
            ++row;
            if (e.kind == view::Kind::label) {
                if (m.choosing && m.choosing->element == e.id) {
                    col = put(0, row, "show which field of " + m.choosing->shape->name() + "? ");
                    for (const auto& f : showable(*m.choosing->shape)) col = button(col, row, f, "show", {i, f});
                } else if (const auto* s = d.shown(e.id)) {
                    col = put(0, row, "shows " + s->shape->name() + "." + s->field + " ");
                    button(col, row, "Unshow", "unshow", {i});
                } else {
                    put(0, row, "shows nothing: carry a shape or a field here", ink::kMuted);
                }
                press(0, row, columns, "shows", {i});
            } else if (e.kind == view::Kind::button) {
                if (const auto* intent = d.intent(e.id)) {
                    put(0, row, "says " + shape_line(*intent->shape));
                    press(0, row, columns, "says", {i});
                    ++row;
                    col = button(0, row, "Make intent from fields", "ask-intent", {i});
                    button(col, row, "Drop intent", "drop-intent", {i});
                    put(0, row + 1, "right-press `says` to carry its shape onto Flow", ink::kMuted);
                } else {
                    put(0, row, "says nothing yet", ink::kMuted);
                    ++row;
                    button(0, row, "Make intent from fields", "ask-intent", {i});
                }
            } else if (!e.text.empty()) {
                put(0, row, "starts with " + e.text, ink::kMuted);
            }
        }
    }
    // The notice takes the last two rows when it needs them: a host's answer is often long.
    auto notice = m.notice, rest = std::string();
    if (static_cast<std::int64_t>(notice.size()) > columns) {
        auto cut = notice.rfind(' ', static_cast<std::size_t>(columns));
        if (cut == std::string::npos || cut == 0) cut = static_cast<std::size_t>(columns);
        rest = notice.substr(cut + (notice[cut] == ' ' ? 1 : 0));
        notice.resize(cut);
    }
    const auto status = rest.empty() ? rows - 2 : rows - 3;
    put(0, status, m.description.name + (m.running ? " / running" : " / stopped") +
                       (m.path.empty() ? std::string() : " / " + m.path), ink::kMuted);
    put(0, status + 1, notice, ink::kAccent);
    if (!rest.empty()) put(0, rows - 1, rest, ink::kAccent);
    return out;
}

} // namespace zengine::view_builder
#endif
