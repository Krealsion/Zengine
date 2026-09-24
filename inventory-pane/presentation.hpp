// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_PANE_PRESENTATION_HPP
#define ZENGINE_INVENTORY_PANE_PRESENTATION_HPP
#include "slots.hpp"
#include "browser.hpp"
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
/// What main Inventory shows of the folder it is browsing. `show` is false for a collection
/// without folders, which then looks exactly as it did before folders existed.
struct Browse {
    bool show = false;
    std::string owner;
    std::vector<std::pair<std::string, std::string>> crumbs; // (folder, name), the root first
    std::vector<std::pair<const inventory::InventoryFolderState*, std::size_t>> folders; // with member counts
    std::size_t placed = 0; // members of this folder placed in portable views
    std::string moving;     // the row key of an item picked to move
};
/// "[Up] [Move here] Root > ... > Commands > Drafts  +1 in views": every control and crumb is a
/// span of the picture; an elided crumb is not a target.
inline void location(std::vector<surface::SurfaceTextRow>& rows, View& v, const Browse& b) {
    const auto row = static_cast<std::int64_t>(rows.size());
    if (row >= v.rows || b.crumbs.empty()) return;
    const auto width = static_cast<std::size_t>(std::max<std::int64_t>(0, v.columns));
    std::string text;
    std::vector<std::tuple<std::size_t, std::size_t, std::string>> spans;
    const auto add = [&](const std::string& part, const std::string& meaning) {
        if (!meaning.empty()) spans.emplace_back(text.size(), part.size(), meaning);
        text += part;
    };
    add(b.crumbs.size() > 1 ? "[Up]" : "(Up)", b.crumbs.size() > 1 ? kUpControl : "");
    if (!b.moving.empty()) { text += " "; add("[Move here]", kMoveHereControl); }
    text += " ";
    // The root always shows; keep as many of the nearest folders as fit and elide the middle.
    const auto below = b.crumbs.size() - 1; // folders under the root on this path
    const auto measure = [&](std::size_t keep) {
        std::size_t n = b.crumbs.front().second.size() + (keep < below ? 6 : 0); // " > ..."
        for (std::size_t i = b.crumbs.size() - keep; i < b.crumbs.size(); ++i) n += 3 + b.crumbs[i].second.size();
        return n;
    };
    std::size_t keep = below;
    while (keep > 1 && text.size() + measure(keep) > width) --keep;
    for (std::size_t i = 0; i < b.crumbs.size(); ++i) {
        if (i && i < b.crumbs.size() - keep) { if (i == 1) text += " > ..."; continue; }
        if (i) text += " > ";
        add(b.crumbs[i].second, crumb(b.owner, b.crumbs[i].first));
    }
    // Crumbs are where to go; the count of members placed elsewhere is only information, so it
    // shortens and then leaves before a crumb is cut.
    if (b.placed) {
        const auto count = std::to_string(b.placed);
        if (text.size() + 12 + count.size() <= width) text += "  +" + count + " in views";
        else if (text.size() + 2 + count.size() <= width) text += " +" + count;
    }
    const auto drawn = workshop::pane_text::drawable(workshop::pane_text::fit(text, v.columns));
    const auto solid = component::solid_columns(drawn, text.size());
    for (const auto& [first, size, meaning] : spans)
        v.map.span(row, static_cast<std::int64_t>(first), static_cast<std::int64_t>(size), solid, meaning);
    rows.push_back({drawn, surface::role::kFill, surface::role::kNone});
}
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
// Five text rows high, with the wider text-cell aspect compensated horizontally.
// Adjacent tiles advance by one less than their extent, sharing the border.
inline constexpr std::int64_t kSlotColumns = 9, kSlotRows = 5;
inline std::string centered(std::string text, std::size_t width) {
    text = workshop::pane_text::fit(text, static_cast<std::int64_t>(width));
    const auto left = (width - text.size()) / 2;
    return std::string(left, ' ') + text + std::string(width - left - text.size(), ' ');
}
inline std::pair<std::string,std::string> tile_name(const std::string& name) {
    std::size_t cut=std::min<std::size_t>(7,name.size());
    if(name.size()>7) {
        const auto space=name.rfind(' ',7);
        if(space!=std::string::npos && space>0) cut=space;
    }
    auto rest=cut;
    while(rest<name.size() && name[rest]==' ') ++rest;
    return {name.substr(0,cut),workshop::pane_text::fit(name.substr(rest),7)};
}
inline std::vector<surface::SurfaceTextRow> render(const InventoryViews& s, const std::string& id,
    View& v, const std::vector<inventory::InventorySummary>& entries, const std::string& notice,
    const std::string& edit_label = {}, const std::string& edit_text = {}, const Browse& browse = {}) {
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
    push((id == "inventory" ? "INVENTORY " : std::string(active(s,id)?"ON ":"OFF ") + kind + " ") + std::to_string(entries.size()) +
         (id == "inventory" ? std::string(" | sort: ") + (v.order == 0 ? "added" : v.order == 1 ? "name" : "type") : "") +
         (id == "inventory" ? std::string(" | hotkeys ") + (active(s, id) ? "ON" : "OFF") : ""), role::kAccent);
    const auto it = std::find_if(entries.begin(), entries.end(), [&](const auto& e) { return key(e.reference) == v.selected; });
    const auto index = it == entries.end() ? 0u : static_cast<std::size_t>(it - entries.begin());
    if (!edit_label.empty()) {
        push(edit_label, role::kAccent);
        push(edit_text, role::kFill);
        push(notice.empty() ? "Enter saves; Escape cancels" : notice, role::kMuted);
    } else if (kind != "inventory") {
        const auto capacity = kind == "row" ? (v.columns-1)/(kSlotColumns-1) :
            kind == "column" ? (v.rows-3)/(kSlotRows-1) : 1;
        if (v.columns < kSlotColumns || v.rows < kSlotRows+1 || capacity < 1) {
            push("Resize", role::kMuted); // no invisible/clipped tile is interactive
            return rows;
        }
        const auto count = static_cast<std::size_t>(capacity);
        const auto first = index / count * count;
        const auto width = kind == "row" ? capacity*(kSlotColumns-1)+1 : kSlotColumns;
        const auto height = kind == "column" ? capacity*(kSlotRows-1)+1 : kSlotRows;
        std::vector<std::string> grid(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width),' '));
        for (std::size_t slot=0; slot<count; ++slot) {
            const auto x=kind=="row"?static_cast<std::int64_t>(slot)*(kSlotColumns-1):0;
            const auto y=kind=="column"?static_cast<std::int64_t>(slot)*(kSlotRows-1):0;
            auto put=[&](std::int64_t dy,const std::string& text) { grid[static_cast<std::size_t>(y+dy)].replace(static_cast<std::size_t>(x),text.size(),text); };
            put(0,"+-------+"); put(4,"+-------+");
            const auto i=first+slot;
            std::string label="", hint="";
            bool selected=false;
            if(i<entries.size()) {
                const auto& e=entries[i]; label=e.label; selected=key(e.reference)==v.selected;
                if(const auto* b=binding(s,e.reference)) hint=workshop::gesture_word({b->scancode,b->modifiers})+(b->enabled?"":" x");
                for(std::int64_t dy=1;dy<kSlotRows-1;++dy)
                    v.map.span(y+dy+1,x+1,kSlotColumns-2,v.columns,key(e.reference));
            }
            put(1,"|"+centered(hint,7)+"|");
            const auto name=tile_name(label);
            put(2,"|"+centered(label.empty()?"+":name.first,7)+"|");
            put(3,"|"+centered(name.second,7)+"|");
            if(selected) { grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x+1)]='*'; }
        }
        for(const auto& line:grid) push(line,role::kFill);
        if(it!=entries.end()) push(it->label,role::kAccent);
        if(first || entries.size()>first+count)
            push(std::to_string(first)+" earlier | "+std::to_string(entries.size()>first+count?entries.size()-first-count:0)+" later",role::kMuted);
        push(notice.empty()?"Drag moves | right-click actions":notice,role::kMuted);
    } else {
        // Folders first (by name), then entries in the chosen order; one selection over both.
        const bool folders = browse.show && id == "inventory";
        if (folders && v.rows > 3) location(rows, v, browse);
        const auto nf = folders ? browse.folders.size() : 0;
        const auto total = nf + entries.size();
        std::size_t at = it == entries.end() ? 0 : nf + index;
        for (std::size_t f = 0; f < nf; ++f)
            if (folder_row(browse.owner, browse.folders[f].first->folder.folder) == v.selected) at = f;
        const auto fixed = static_cast<std::int64_t>(rows.size()) + (v.rows > 2 ? 1 : 0);
        const auto budget = static_cast<std::size_t>(std::max<std::int64_t>(0, v.rows - fixed));
        const auto window = component::cursor_window(total, at, at, budget);
        if (window.before && window.marker_rows()) push("... " + std::to_string(window.before) + " earlier", role::kMuted);
        for (std::size_t i = window.first; i < window.end(); ++i) {
            if (i < nf) {
                const auto& [folder, members] = browse.folders[i];
                const auto meaning = folder_row(browse.owner, folder->folder.folder);
                v.map.row(static_cast<std::int64_t>(rows.size()), meaning);
                push(std::string(meaning == v.selected ? "> " : "  ") + folder->name + "/  " +
                     (members ? "(" + std::to_string(members) + ")" : std::string("(empty)")) +
                     (meaning == browse.moving ? " [moving]" : ""), role::kAccent);
                continue;
            }
            const auto& e = entries[i - nf];
            v.map.row(static_cast<std::int64_t>(rows.size()), key(e.reference));
            push(std::string(key(e.reference) == v.selected ? "> " : "  ") + caption(s, e) +
                 (e.capture_slot ? " [capture slot]" : "") + (kind == "single" ? "" : " : " + e.schema) +
                 (key(e.reference) == browse.moving ? " [moving]" : ""), role::kFill);
        }
        if (window.after && window.marker_rows()) push("... " + std::to_string(window.after) + " later", role::kMuted);
        if (total == 0) push(folders && browse.crumbs.size() > 1 ? "Empty folder: drop entries here; Ctrl+D new folder"
                                                                 : "Drop entries here; right-click view actions", role::kMuted);
        push(!notice.empty() ? notice : folders ? "Enter opens | Backspace up | right-click actions"
                                                : "Drag moves | right-click live & actions", role::kMuted);
    }
    return rows;
}
}
#endif
