// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INFO_VALUE_PICTURE_HPP
#define ZENGINE_INFO_VALUE_PICTURE_HPP

// One Info value view's picture: what the view shows, in words and lists, laid out in the room it
// was granted, with the press map the same pass records. It decides nothing the view owns -- not
// which acts can run, nor what the draft or a pending request holds; the view says those.
// Reference: info/docs/zengine.info.md.

#include "vocabulary.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "component/text_box.hpp"
#include "message-draft/draft.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::info_pane {

/// A FIELD BY STRUCTURE, never by row number: `metadata` is -1 for the item, else the index of a
/// read-only metadata value; `path` is the draft's structural path inside it.
struct FieldRef {
    std::int64_t metadata = -1;
    message_draft::Path path;
    bool operator==(const FieldRef&) const = default;
    /// Is this field `outer` or inside it?
    bool within(const FieldRef& outer) const {
        return metadata == outer.metadata && path.size() >= outer.path.size() &&
               std::equal(outer.path.begin(), outer.path.end(), path.begin());
    }
};

/// WHAT ONE PLACE OF THE PICTURE MEANS: a control (by action id), a field, or the fold mark of a
/// field that holds fields of its own.
struct Meaning {
    enum Kind { kNone, kField, kControl, kFold } kind = kNone;
    std::string control;
    FieldRef field;
    bool operator==(const Meaning&) const = default;
};

/// ONE CONTROL: the act a press on it runs, its word, and whether that act can run now.
struct Control {
    std::string id;
    std::string label;
    bool on = true;
};

/// ONE FIELD ROW: the field it means, its words, and its marks. A row names its own field (`x`,
/// `[1]`), indented by its depth; a field holding fields of its own is a level, which folds, and
/// shut it says what it holds in its summary.
struct FieldLine {
    FieldRef ref;
    std::string label, summary;
    std::size_t depth = 0; ///< how many levels it stands inside
    bool branch = false;   ///< it holds fields of its own
    bool open = false;     ///< ...and shows them
    bool missing = false;  ///< required and absent
    bool metadata = false; ///< read-only capture metadata, drawn muted
    bool here = false;     ///< the selected field
    char mark = ' ';       ///< `*` edited in this draft, `~` changed by the last observation
};

/// WHAT ONE VIEW SHOWS, read from it once per composition.
struct Shown {
    std::int64_t rows = 0, columns = 0;
    bool used = false;          ///< false: an unused slot, which offers only Use
    std::string title, subject; ///< the heading: the view's name and what it holds
    std::string state;          ///< the state line, from four rows
    bool alert = false;         ///< the state line asks for attention
    std::string notice;
    std::string detail;         ///< the detail line, from twelve rows
    std::string typing;         ///< what the line being typed edits, or empty when none is
    std::vector<Control> controls;
    bool holds_value = false;
    std::vector<FieldLine> fields;
    std::size_t selected = 0;   ///< the field the window keeps in sight
};

/// THE CONTROLS A VIEW OFFERS, in the order they read, each for the view to say whether it can run.
/// An unused slot offers only Use. While a replacement waits, the way out of waiting comes first:
/// it is the one act that matters until the answer arrives, and a narrow room must not wrap it away.
inline std::vector<Control> control_row(bool used, bool waiting, bool watching, bool default_view) {
    if (!used) return {{kActionViewUse, "Use this view"}};
    std::vector<Control> out;
    if (waiting) out.push_back({kActionStop, "Stop"});
    for (const Control& item : std::vector<Control>{{kActionSave, "Save"}, {kActionSaveCopy, "Save copy"},
                                                    {kActionRefresh, "Refresh"},
                                                    {kActionWatch, watching ? "Pause" : "Watch"},
                                                    {kActionViewNew, "New"}, {kActionViewFork, "Fork"},
                                                    {kActionSample, "Sample"}, {kActionViewRename, "Rename"}})
        out.push_back(item);
    if (!default_view) out.push_back({kActionViewClose, "Close"});
    else out.push_back({kActionPanes, "Panes"});
    return out;
}

/// ONE VIEW'S PICTURE: its rows, and the press map and caret recorded by the pass that composed
/// them. Its view keeps it across incarnations, so the picture keeps numbering upward and no old
/// press matches.
class ValuePicture {
public:
    /// LAY OUT WHAT THE VIEW SHOWS for its pane. `line` is the line being typed, scrolled here to
    /// keep its caret in sight; `first` is the field window's first row, which follows the selection.
    workshop::ComposedRows compose(const Shown& s, component::TextBox& line, std::size_t& first) {
        workshop::ComposedRows out;
        map_.begin();
        caret_ = {};
        const auto width = s.columns;
        const auto push = [&](std::string text, std::int64_t role, std::int64_t ground = graphics::role::kNone) {
            if (static_cast<std::int64_t>(out.rows.size()) < s.rows)
                out.rows.push_back({workshop::pane_text::drawable(workshop::pane_text::fit(std::move(text), width)), role, ground});
        };
        if (!s.used) {
            push(s.title + " -- unused view slot", graphics::role::kAccent, graphics::role::kMuted);
            controls(s, out, 1);
            push("Or drop an Inventory value here. Views close with Close; data stays in Inventory.",
                 graphics::role::kMuted);
            (void)map_.settle();
            return out;
        }
        push(s.title + " | " + s.subject, graphics::role::kAccent, graphics::role::kMuted);
        if (s.rows >= 4) push(s.state, s.alert ? graphics::role::kAlert : graphics::role::kMuted);
        if (!s.typing.empty()) {
            if (!s.notice.empty()) push(s.notice, graphics::role::kAlert);
            push(s.typing, graphics::role::kFill);
            const std::int64_t room = std::max<std::int64_t>(0, width - 2);
            line.keep_caret_visible(room);
            if (room > 0 && static_cast<std::int64_t>(out.rows.size()) < s.rows) {
                const auto row = static_cast<std::int64_t>(out.rows.size());
                caret_.row = row;
                caret_.column = 2 + static_cast<std::int64_t>(line.caret_column());
                if (const auto span = line.visible_selection(room); span.present()) {
                    caret_.sel_begin_row = caret_.sel_end_row = row;
                    caret_.sel_begin_col = 2 + span.begin;
                    caret_.sel_end_col = 2 + span.end;
                }
            }
            push("> " + line.visible(room), graphics::role::kAccent);
            push("Enter keeps it; Escape cancels", graphics::role::kMuted);
            (void)map_.settle();
            return out;
        }
        controls(s, out, s.rows >= 10 ? 2 : 1);
        if (!s.notice.empty()) push(s.notice, graphics::role::kAlert);
        if (s.rows >= 12 && !s.detail.empty()) push(s.detail, graphics::role::kMuted);
        if (!s.holds_value) {
            push("Drag an Inventory entry here for a copy; right-click an entry, Grab live, then click here to link it.",
                 graphics::role::kMuted);
            (void)map_.settle();
            return out;
        }
        const auto budget = static_cast<std::size_t>(std::max<std::int64_t>(0, s.rows - static_cast<std::int64_t>(out.rows.size())));
        const auto window = component::cursor_window(s.fields.size(), s.selected, first, budget);
        first = window.first;
        if (const std::size_t earlier = component::earlier_marker(window); earlier > 0)
            push(workshop::pane_text::omitted_text(earlier, "earlier"), graphics::role::kMuted);
        for (std::size_t i = window.first; i < window.end(); ++i) {
            const FieldLine& f = s.fields[i];
            const auto row = static_cast<std::int64_t>(out.rows.size());
            map_.row(row, Meaning{Meaning::kField, {}, f.ref});
            // A LEVEL'S FOLD MARK, and the marks and indent before it, fold it on a press.
            const std::string indent(2 * f.depth, ' ');
            if (f.branch)
                (void)map_.span(row, 0, static_cast<std::int64_t>(3 + indent.size()), width,
                                Meaning{Meaning::kFold, {}, f.ref});
            const char* fold = !f.branch ? "  " : f.open ? "- " : "+ ";
            push(std::string(f.here ? ">" : " ") + f.mark + indent + fold + f.label +
                     (f.branch && f.open ? std::string() : ": " + f.summary) + (f.missing ? " (required)" : ""),
                 f.metadata ? graphics::role::kMuted : graphics::role::kFill);
        }
        if (const std::size_t more = component::more_marker(window); more > 0)
            push(workshop::pane_text::omitted_text(more, "more"), graphics::role::kMuted);
        (void)map_.settle();
        return out;
    }

    /// THE MEANING UNDER A PLACE of the picture last composed, or nullptr.
    const Meaning* at(std::int64_t row, std::int64_t column) const noexcept { return map_.at(row, column); }
    const component::RowMap<Meaning>& map() const noexcept { return map_; }
    /// WHERE THE LINE BEING TYPED HAS ITS CARET AND SELECTION in the rows last composed, or none.
    const workshop::RowsCaret& caret() const noexcept { return caret_; }

private:
    /// LAY OUT THE CONTROLS GREEDILY OVER `lines` ROWS, keeping `[...]` last so every act stays one
    /// press away. A control the width cannot hold whole is not drawn, so it is never a target.
    void controls(const Shown& s, workshop::ComposedRows& out, std::int64_t lines) {
        const auto& items = s.controls;
        const bool menu = items.size() > 1;
        const std::int64_t reserve = menu ? 6 : 0; // room for " [...]" on whichever line is last
        std::size_t next = 0;
        std::int64_t last_row = -1;
        std::string last_text;
        for (std::int64_t line = 0; line < lines && static_cast<std::int64_t>(out.rows.size()) < s.rows; ++line) {
            std::string text;
            const auto row = static_cast<std::int64_t>(out.rows.size());
            while (next < items.size()) {
                std::string word = items[next].on ? "[" + items[next].label + "]" : "(" + items[next].label + ")";
                const auto start = static_cast<std::int64_t>(text.size()) + (text.empty() ? 0 : 1);
                if (start + static_cast<std::int64_t>(word.size()) > s.columns - reserve) break;
                if (!text.empty()) text += ' ';
                map_.span(row, start, static_cast<std::int64_t>(word.size()), s.columns, Meaning{Meaning::kControl, items[next].id, {}});
                text += word;
                ++next;
            }
            out.rows.push_back({workshop::pane_text::drawable(text), graphics::role::kFill, graphics::role::kNone});
            last_row = row; last_text = text;
            if (next >= items.size()) break;
        }
        if (menu && last_row >= 0) {
            // EVERY ACT, ONE PRESS AWAY: the menu lists what the width could not show.
            const auto start = static_cast<std::int64_t>(last_text.size()) + (last_text.empty() ? 0 : 1);
            if (start + 5 <= s.columns) {
                if (!last_text.empty()) last_text += ' ';
                map_.span(last_row, start, 5, s.columns, Meaning{Meaning::kControl, kActionViewMenu, {}});
                last_text += "[...]";
                out.rows[static_cast<std::size_t>(last_row)].text = workshop::pane_text::drawable(last_text);
            }
        }
    }

    component::RowMap<Meaning> map_;
    workshop::RowsCaret caret_;
};

} // namespace zengine::info_pane
#endif
