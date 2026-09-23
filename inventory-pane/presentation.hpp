// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_PANE_PRESENTATION_HPP
#define ZENGINE_INVENTORY_PANE_PRESENTATION_HPP
#include "slots.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/keymap.hpp"
#include "component/row_map.hpp"
#include "component/list_window.hpp"
namespace zengine::inventory_pane {
struct View {
    std::int64_t rows = 0, columns = 0;
    component::RowMap<std::string> map;
    std::string selected;
    int order = 0;
    double wheel = 0;
};
inline bool active(const InventoryViews& s, const std::string& id) {
    for (const auto& v : s.views) if (v.id == id) return v.active;
    return id == "inventory" && s.inventory_active;
}
inline std::string caption(const InventoryViews& s, const inventory::InventorySummary& e) {
    auto text = e.label;
    if (const auto* b = binding(s, e.reference))
        text = "[" + workshop::gesture_word({b->scancode, b->modifiers}) + (b->enabled ? "] " : " off] ") + text;
    return text;
}
inline std::vector<surface::SurfaceTextRow> render(const InventoryViews& s, const std::string& id,
    View& v, const std::vector<inventory::InventorySummary>& entries, const std::string& notice,
    const std::string& edit_label = {}, const std::string& edit_text = {}) {
    namespace ws = workshop;
    namespace role = surface::role;
    v.map.begin();
    std::vector<surface::SurfaceTextRow> rows;
    const auto push = [&](std::string text, std::int64_t style) {
        if (static_cast<std::int64_t>(rows.size()) < v.rows)
            rows.push_back({ws::pane_text::drawable(ws::pane_text::fit(text, v.columns)), style, role::kNone});
    };
    std::string kind = "inventory";
    for (const auto& x : s.views) if (x.id == id) kind = x.kind;
    push((id == "inventory" ? "INVENTORY " : kind + " ") + std::to_string(entries.size()) +
         (id == "inventory" ? std::string(" | sort: ") + (v.order == 0 ? "added" : v.order == 1 ? "name" : "type") : "") +
         " | hotkeys " + (active(s, id) ? "ON" : "OFF"), role::kAccent);
    const auto it = std::find_if(entries.begin(), entries.end(), [&](const auto& e) { return key(e.reference) == v.selected; });
    const auto index = it == entries.end() ? 0u : static_cast<std::size_t>(it - entries.begin());
    if (!edit_label.empty()) {
        push(edit_label + edit_text, role::kFill);
        push(notice.empty() ? "Enter saves; Escape cancels" : notice, role::kMuted);
    } else if (kind == "row") {
        const auto capacity = static_cast<std::size_t>(std::max<std::int64_t>(1, v.columns / 24));
        const auto count = std::max<std::size_t>(1, std::min(entries.size(), capacity));
        const auto width = std::max<std::int64_t>(1, v.columns / static_cast<std::int64_t>(count));
        const auto first = index / count * count;
        std::string line;
        for (std::size_t i = first; i < std::min(entries.size(), first + count); ++i) {
            const auto text = ws::pane_text::fit((key(entries[i].reference) == v.selected ? ">" : " ") + caption(s, entries[i]), width - 1);
            if (v.rows > 1 && !text.empty()) v.map.span(1, static_cast<std::int64_t>(line.size()), width, v.columns, key(entries[i].reference));
            line += text; line.append(static_cast<std::size_t>(width) - text.size(), ' ');
        }
        push(line.empty() ? "Drop entries here" : line, role::kFill);
        push(std::to_string(first) + " earlier | " + std::to_string(entries.size() > first + count ? entries.size() - first - count : 0) + " later | arrows/wheel", role::kMuted);
        push(notice.empty() ? "Drag reorders | right-click actions" : notice, role::kMuted);
    } else {
        const auto budget = static_cast<std::size_t>(std::max<std::int64_t>(0, v.rows - (v.rows > 2 ? 2 : 1)));
        const auto window = component::cursor_window(entries.size(), index, index, budget);
        if (window.before && window.marker_rows()) push("... " + std::to_string(window.before) + " earlier", role::kMuted);
        for (std::size_t i = window.first; i < window.end(); ++i) {
            v.map.row(static_cast<std::int64_t>(rows.size()), key(entries[i].reference));
            push(std::string(key(entries[i].reference) == v.selected ? "> " : "  ") + caption(s, entries[i]) +
                 (entries[i].capture_slot ? " [capture slot]" : "") + (kind == "single" ? "" : " : " + entries[i].schema), role::kFill);
        }
        if (window.after && window.marker_rows()) push("... " + std::to_string(window.after) + " later", role::kMuted);
        if (entries.empty()) push("Drop entries here; right-click view actions", role::kMuted);
        push(notice.empty() ? "Drag moves | right-click live & actions" : notice, role::kMuted);
    }
    return rows;
}
}
#endif
