// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_TEXT_AREA_HPP
#define ZENGINE_COMPONENT_TEXT_AREA_HPP

// Text longer than its place, read only: wrapped by width, scrolled, its text selected and copied,
// over the text core (`text.hpp`). The scroll and the selection are places in the text, never rows
// or pixels, so a text wrapped again at another width keeps both. No medium: a window draws it
// with a bar and scrolls it freely (`graphics/elements/text_face.hpp`), and a row of cells spells
// it here (`TextArea::cells`), with a marker for the lines above and below.
// Reference: component/docs/component.md.

#include "component/text.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::component {

/// HOW ONE PARAGRAPH OF AN AREA READS: the owner's number for its look (a theme role, to a pane),
/// and how many columns its further lines stand in when it wraps. A paragraph is a line of the
/// text as written, up to its `\n`.
struct TextLook {
    std::int64_t role = 0;
    std::int64_t hang = 0;
    bool operator==(const TextLook&) const = default;
};

/// LONG TEXT, READ ONLY: the text as written, how each paragraph reads, a selection and the place
/// the scroll stands at. Every operation keeps the places on character boundaries in the text.
class TextArea {
public:
    /// A line's fraction a scroll keeps beside its place: a window scrolls by pixels, and a line
    /// of any height is this many units tall.
    static constexpr std::int64_t kUnitsPerLine = 4096;

    /// ONE LINE AS A WIDTH WRAPS IT: the bytes of the text it shows, `end` exclusive (a break's
    /// space is in neither line), the paragraph it belongs to, and the blank columns it stands in.
    struct Line {
        std::size_t begin = 0, end = 0;
        std::size_t paragraph = 0;
        std::int64_t indent = 0;
        bool operator==(const Line&) const = default;
    };

    const std::string& text() const noexcept { return text_; }
    const TextPlaces& places() const noexcept { return places_; }
    bool has_selection() const noexcept { return places_.has_selection(); }
    std::string selected_text() const { return component::selected_text(text_, places_); }
    /// The place the scroll stands at: the line holding it is the first shown.
    std::size_t top() const noexcept { return top_; }
    /// ...and how far into that line a window has scrolled, in `kUnitsPerLine`.
    std::int64_t within() const noexcept { return within_; }
    /// How paragraph `p` reads: its look, or the plain one where none was given.
    TextLook look(std::size_t p) const { return p < looks_.size() ? looks_[p] : TextLook{}; }

    /// NEW TEXT, THE SCROLL AND THE SELECTION KEPT: each place held inside the new text and on its
    /// boundaries, so text that grew or was laid out again stays where the weaver was reading.
    void set(std::string text, std::vector<TextLook> looks = {}) {
        text_ = std::move(text);
        looks_ = std::move(looks);
        places_.settle(text_);
        top_ = character_boundary(text_, top_);
        if (top_ >= text_.size()) {
            within_ = 0;
        }
    }

    /// No text, no selection, the scroll at the top.
    void clear() noexcept {
        text_.clear();
        looks_.clear();
        places_ = TextPlaces{};
        top_ = 0;
        within_ = 0;
    }

    // ---- The wrap -----------------------------------------------------------------------------

    /// THE TEXT AS `columns` COLUMNS WRAP IT: each paragraph from its first line, a further line
    /// standing in its paragraph's hang; a line broken at its last space that fits, the space spent
    /// by the break, and a word longer than the line cut where the line ends. An empty paragraph is
    /// one empty line; no text is none.
    std::vector<Line> lines(std::int64_t columns) const {
        std::vector<Line> out;
        if (columns <= 0 || text_.empty()) {
            return out;
        }
        std::size_t from = 0, paragraph = 0;
        while (true) {
            const std::size_t nl = text_.find('\n', from);
            const std::size_t to = nl == std::string::npos ? text_.size() : nl;
            wrap(from, to, paragraph, columns, out);
            if (nl == std::string::npos) {
                return out;
            }
            from = nl + 1;
            ++paragraph;
        }
    }

    /// THE LINE HOLDING `place`: the last whose first byte is at or before it.
    static std::size_t line_of(const std::vector<Line>& lines, std::size_t place) noexcept {
        std::size_t found = 0;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].begin > place) {
                break;
            }
            found = i;
        }
        return found;
    }

    /// THE PLACE A COLUMN OF A SHOWN LINE NAMES: the line's own bytes from its indent on, a column
    /// in the indent its first, one past its words its end, snapped back to a character's start.
    std::size_t place_in_line(const Line& line, std::int64_t column) const noexcept {
        const std::int64_t into = column - line.indent;
        if (into <= 0) {
            return line.begin;
        }
        const std::size_t ahead = static_cast<std::size_t>(into);
        const std::size_t at = ahead < line.end - line.begin ? line.begin + ahead : line.end;
        return character_boundary(text_, at);
    }

    // ---- The scroll, in a row of cells ----------------------------------------------------------

    /// THE FURTHEST FIRST LINE `rows` ROWS OF CELLS SHOW, `total` lines long: the last lines
    /// under the marker for those above, none below.
    static std::size_t last_first(std::size_t total, std::int64_t rows) noexcept {
        if (rows <= 0 || total <= static_cast<std::size_t>(rows)) {
            return 0;
        }
        const std::size_t shown = rows >= 2 ? static_cast<std::size_t>(rows - 1) : 1;
        return total - shown;
    }

    /// THE SCROLL MOVED `by` LINES, `columns` wide in `rows` rows of cells, held inside the text.
    void scroll_lines(std::int64_t by, std::int64_t columns, std::int64_t rows) {
        const std::vector<Line> all = lines(columns);
        if (all.empty()) {
            top_ = 0;
            within_ = 0;
            return;
        }
        const auto last = static_cast<std::int64_t>(last_first(all.size(), rows));
        std::int64_t first = static_cast<std::int64_t>(line_of(all, top_));
        first = first > last ? last : first;
        first += by;
        first = first < 0 ? 0 : (first > last ? last : first);
        top_ = all[static_cast<std::size_t>(first)].begin;
        within_ = 0;
    }

    /// The scroll at the top of the text.
    void scroll_home() noexcept {
        top_ = 0;
        within_ = 0;
    }

    // ---- The scroll, in a window -----------------------------------------------------------------

    /// HOW FAR DOWN THE TEXT A WINDOW `shown` UNITS TALL IS SCROLLED, in `kUnitsPerLine`, held so the
    /// last line's foot is the window's foot at most.
    std::int64_t offset(const std::vector<Line>& all, std::int64_t shown) const noexcept {
        const std::int64_t most = furthest(all, shown);
        const std::int64_t at =
            static_cast<std::int64_t>(line_of(all, top_)) * kUnitsPerLine + within_;
        return at < most ? at : most;
    }

    /// THE SCROLL MOVED `by` UNITS IN A WINDOW `shown` UNITS TALL, `columns` wide: freely, a line
    /// part shown, and held inside the text.
    void scroll_by(std::int64_t by, std::int64_t columns, std::int64_t shown) {
        const std::vector<Line> all = lines(columns);
        scroll_to(offset(all, shown) + by, all, shown);
    }

    /// THE SCROLL SET `to` UNITS DOWN A WINDOW `shown` UNITS TALL, held inside the text: where a
    /// bar dragged or pressed puts it.
    void scroll_to(std::int64_t to, const std::vector<Line>& all, std::int64_t shown) {
        if (all.empty()) {
            top_ = 0;
            within_ = 0;
            return;
        }
        const std::int64_t most = furthest(all, shown);
        to = to < 0 ? 0 : (to > most ? most : to);
        const auto line = static_cast<std::size_t>(to / kUnitsPerLine);
        top_ = all[line < all.size() ? line : all.size() - 1].begin;
        within_ = to % kUnitsPerLine;
    }

    /// How far a window `shown` units tall can scroll down `all`, in units.
    static std::int64_t furthest(const std::vector<Line>& all, std::int64_t shown) noexcept {
        const std::int64_t whole = static_cast<std::int64_t>(all.size()) * kUnitsPerLine;
        return whole > shown ? whole - shown : 0;
    }

    // ---- The selection ---------------------------------------------------------------------------

    /// A PRESS AT `place`: the caret there and nothing selected -- the gesture that starts one.
    void press(std::size_t place) noexcept {
        places_.caret = character_boundary(text_, place);
        places_.anchor = places_.caret;
    }

    /// A DRAG OR A SHIFTED PRESS REACHING `place`: the caret moves and the anchor stays.
    void drag_to(std::size_t place) noexcept { places_.caret = character_boundary(text_, place); }

    /// THE WORD AT `place` SELECTED, a word a run of bytes between spaces and line ends; true when
    /// there was one, else the press's answer.
    bool select_word_at(std::size_t place) noexcept {
        std::size_t begin = place < text_.size() ? place : text_.size();
        std::size_t end = begin;
        while (begin > 0 && !gap(text_[begin - 1])) {
            --begin;
        }
        while (end < text_.size() && !gap(text_[end])) {
            ++end;
        }
        if (end <= begin) {
            press(place);
            return false;
        }
        places_.anchor = begin;
        places_.caret = end;
        return true;
    }

    /// EVERYTHING SELECTED, the caret at the end.
    void select_all() noexcept {
        places_.anchor = 0;
        places_.caret = text_.size();
    }

    /// Nothing selected, the caret where it stands.
    void clear_selection() noexcept { places_.anchor = places_.caret; }

    /// THE SELECTION COPIED, AS WRITTEN: never the wrap's breaks, indents or markers.
    void copy(Clipboard& clip) const { copy_selection(text_, places_, clip); }

    /// THE KEYS AN AREA ANSWERS ITSELF: Ctrl+C copies and Ctrl+A selects all. Any other key is the
    /// owner's, the scroll's keys among them, since an owner names its own actions.
    bool consume(std::int64_t scancode, std::int64_t modifiers, Clipboard& clip) {
        if (modifiers != mod::kCtrl) {
            return false;
        }
        if (scancode == key::kC) {
            copy(clip);
            return true;
        }
        if (scancode == key::kA) {
            select_all();
            return true;
        }
        return false;
    }

    // ---- In cells --------------------------------------------------------------------------------

    /// ONE ROW OF THE AREA SPELLED IN CELLS: its text from the area's first column, the paragraph's
    /// look, the selected columns (end exclusive, equal for none), and the bytes of the text it
    /// shows; a marker row shows none (`line` is -1).
    struct CellRow {
        std::string text;
        TextLook look;
        bool marker = false;
        std::int64_t line = -1;
        std::int64_t selection_begin = 0, selection_end = 0;
        bool operator==(const CellRow&) const = default;
    };

    /// THE AREA IN `rows` ROWS OF `columns` CELLS: the lines from the scroll's, `N more above...`
    /// atop where lines are hidden above and `... N more below` at the foot where lines are hidden
    /// below, a line's count of each. `first` is the first line shown, `lines` the whole count.
    struct Cells {
        std::vector<CellRow> rows;
        std::vector<Line> wrapped;
        std::size_t first = 0, above = 0, below = 0;
    };

    Cells cells(std::int64_t rows, std::int64_t columns) const {
        Cells out;
        out.wrapped = lines(columns);
        if (rows <= 0 || columns <= 0) {
            return out;
        }
        const std::size_t total = out.wrapped.size();
        std::size_t first = line_of(out.wrapped, top_);
        const std::size_t last = last_first(total, rows);
        first = first > last ? last : first;
        if (total <= static_cast<std::size_t>(rows)) {
            first = 0;
        }
        const bool top_marker = first > 0 && rows >= 2;
        std::int64_t room = rows - (top_marker ? 1 : 0);
        const bool foot_marker = first + static_cast<std::size_t>(room) < total && room >= 2;
        room -= foot_marker ? 1 : 0;
        const std::size_t end = first + static_cast<std::size_t>(room) < total
                                    ? first + static_cast<std::size_t>(room)
                                    : total;
        out.first = first;
        out.above = first;
        out.below = total - end;
        if (top_marker) {
            out.rows.push_back(marker(std::to_string(first) + " more above...", columns));
        }
        for (std::size_t i = first; i < end; ++i) {
            out.rows.push_back(cell_row(out.wrapped[i], i, columns));
        }
        if (foot_marker) {
            out.rows.push_back(marker("... " + std::to_string(out.below) + " more below", columns));
        }
        return out;
    }

    /// THE PLACE A PRESS ON ROW `row`, COLUMN `column` OF `cells` NAMES: none on a marker or past
    /// the rows.
    std::optional<std::size_t> place_at(const Cells& cells, std::int64_t row,
                                        std::int64_t column) const {
        if (row < 0 || row >= static_cast<std::int64_t>(cells.rows.size())) {
            return std::nullopt;
        }
        const CellRow& r = cells.rows[static_cast<std::size_t>(row)];
        if (r.marker || r.line < 0) {
            return std::nullopt;
        }
        return place_in_line(cells.wrapped[static_cast<std::size_t>(r.line)], column);
    }

private:
    static bool gap(char c) noexcept { return c == ' ' || c == '\n'; }

    void wrap(std::size_t from, std::size_t to, std::size_t paragraph, std::int64_t columns,
              std::vector<Line>& out) const {
        if (from >= to) {
            out.push_back(Line{from, from, paragraph, 0});
            return;
        }
        const std::int64_t hang = look(paragraph).hang;
        std::size_t at = from;
        bool first = true;
        while (at < to) {
            const std::int64_t indent = !first && hang > 0 && hang < columns ? hang : 0;
            const auto take = static_cast<std::size_t>(columns - indent);
            if (to - at <= take) {
                out.push_back(Line{at, to, paragraph, indent});
                return;
            }
            std::size_t cut = at + take;
            std::size_t next = 0;
            bool spaced = false;
            for (std::size_t i = cut; i > at; --i) {
                if (text_[i] == ' ') {
                    cut = i;
                    spaced = true;
                    break;
                }
            }
            if (spaced) {
                next = cut + 1;
            } else {
                cut = character_boundary(text_, cut);
                if (cut <= at) {
                    cut = character_after(text_, at);
                }
                next = cut;
            }
            out.push_back(Line{at, cut, paragraph, indent});
            at = next;
            first = false;
        }
    }

    CellRow cell_row(const Line& line, std::size_t index, std::int64_t columns) const {
        CellRow row;
        row.look = look(line.paragraph);
        row.line = static_cast<std::int64_t>(index);
        row.text = std::string(static_cast<std::size_t>(line.indent), ' ') +
                   text_.substr(line.begin, line.end - line.begin);
        if (static_cast<std::int64_t>(row.text.size()) > columns) {
            row.text.resize(static_cast<std::size_t>(columns));
        }
        if (places_.has_selection()) {
            const std::size_t b = places_.begin() > line.begin ? places_.begin() : line.begin;
            const std::size_t e = places_.end() < line.end ? places_.end() : line.end;
            if (e > b) {
                row.selection_begin = line.indent + static_cast<std::int64_t>(b - line.begin);
                row.selection_end = line.indent + static_cast<std::int64_t>(e - line.begin);
            }
        }
        return row;
    }

    static CellRow marker(std::string words, std::int64_t columns) {
        CellRow row;
        row.marker = true;
        row.text = std::move(words);
        if (static_cast<std::int64_t>(row.text.size()) > columns) {
            row.text.resize(static_cast<std::size_t>(columns));
        }
        return row;
    }

    std::string text_;
    std::vector<TextLook> looks_;
    TextPlaces places_;
    std::size_t top_ = 0;
    std::int64_t within_ = 0;
};

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_TEXT_AREA_HPP
