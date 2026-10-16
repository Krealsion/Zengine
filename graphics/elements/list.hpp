// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENTS_LIST_HPP
#define ZENGINE_GRAPHICS_ELEMENTS_LIST_HPP

// A list an owner draws into a Sketch: its rows, one chosen, a window of them shown. Each shown
// row is drawn at `<path>/<index>` with its face as the record there, so a reader reads which row
// a part is and a press lands on it by name with the row's own action. The chosen row says so in
// its words (`> `) as well as its ink, for a medium without colour.

#include "graphics/canvas.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::graphics {

/// What one drawn row means to a reader: its box in the list's space, its words, the paints its
/// state chose, and whether it is chosen and pointed at.
struct ListRowFace {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::string text;
    Paint fill;
    Paint ink;
    bool selected = false;
    bool hovered = false;
    ZEN_SHAPE(ListRowFace, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(text), ZEN_FIELD(fill), ZEN_FIELD(ink), ZEN_FIELD(selected),
              ZEN_FIELD(hovered));
};

/// Rows of the owner's own type, one chosen (or none, -1), shown `visible_rows` at a time from
/// `scroll_offset` (every row when that is 0). Choosing a row keeps it in the window.
template <class Row>
class List {
public:
    explicit List(std::string path) : path_(std::move(path)) {}

    void set_origin(std::int64_t x, std::int64_t y) {
        x_ = x;
        y_ = y;
    }
    void set_row_size(std::int64_t w, std::int64_t h) {
        w_ = w;
        row_h_ = h;
    }
    void set_visible_rows(std::int64_t rows) {
        visible_ = std::max<std::int64_t>(0, rows);
        settle();
    }
    /// New rows: the choice and the pointed row stay where they still name a row.
    void set_rows(std::vector<Row> rows) {
        rows_ = std::move(rows);
        if (selected_ >= count()) {
            selected_ = count() - 1;
        }
        if (hovered_ >= count()) {
            hovered_ = -1;
        }
        settle();
    }
    /// Chooses row `i`, or none for an index that names no row.
    void select(std::int64_t i) {
        selected_ = i >= 0 && i < count() ? i : -1;
        settle();
    }
    /// The next row; the first when none is chosen.
    void select_next() {
        if (!rows_.empty()) {
            select(selected_ < 0 ? 0 : std::min(selected_ + 1, count() - 1));
        }
    }
    /// The row before; the last when none is chosen.
    void select_prev() {
        if (!rows_.empty()) {
            select(selected_ < 0 ? count() - 1 : std::max<std::int64_t>(selected_ - 1, 0));
        }
    }
    void hover(std::int64_t i) { hovered_ = i >= 0 && i < count() ? i : -1; }
    /// Shows the window from row `first`, held between the first row and the last full window.
    void scroll_to(std::int64_t first) {
        scroll_ = std::clamp<std::int64_t>(first, 0, max_scroll());
    }

    const std::string& path() const { return path_; }
    std::int64_t selected() const { return selected_; }
    std::int64_t hovered() const { return hovered_; }
    std::int64_t scroll_offset() const { return scroll_; }
    std::int64_t visible_rows() const { return visible_; }
    std::size_t size() const { return rows_.size(); }
    bool empty() const { return rows_.empty(); }
    const Row& at(std::size_t i) const { return rows_.at(i); }
    std::string row_path(std::size_t i) const { return path_ + "/" + std::to_string(i); }

    /// Draws the shown rows, each in the words `format(row)` gives; a press on row `i` asks
    /// `action_for(i, row)` of the area's owner, and nothing when that is empty.
    template <class Format, class ActionFor>
    void sketch(Sketch& s, Format&& format, ActionFor&& action_for) const {
        const std::int64_t end = visible_ == 0 ? count() : std::min(count(), scroll_ + visible_);
        const std::int64_t window = visible_ == 0 ? count() : visible_;
        s.group(path_, x_, y_, w_, window * row_h_);
        for (std::int64_t i = scroll_; i < end; ++i) {
            const auto row = static_cast<std::size_t>(i);
            ListRowFace f;
            f.y = (i - scroll_) * row_h_;
            f.w = w_;
            f.h = row_h_;
            f.text = std::string(format(rows_[row]));
            f.selected = i == selected_;
            f.hovered = i == hovered_;
            f.fill = by_role(role::kGround);
            f.ink = by_role(f.selected || f.hovered ? role::kAccent : role::kFill);
            const std::string p = row_path(row);
            s.group(p, f.x, f.y, f.w, f.h);
            s.fill_rect(p + "/face", 0, 0, f.w, f.h, f.fill);
            s.text(p + "/text", 0, 0, (f.selected ? "> " : "  ") + f.text, f.ink);
            s.record(p, f, std::string(action_for(row, rows_[row])));
        }
    }
    /// Draws the shown rows with no action: a press on one sends nothing.
    template <class Format>
    void sketch(Sketch& s, Format&& format) const {
        sketch(s, std::forward<Format>(format),
               [](std::size_t, const Row&) { return std::string(); });
    }

private:
    std::int64_t count() const { return static_cast<std::int64_t>(rows_.size()); }
    std::int64_t max_scroll() const {
        return visible_ == 0 ? 0 : std::max<std::int64_t>(0, count() - visible_);
    }
    /// The window held in range, and moved to show the chosen row.
    void settle() {
        if (visible_ > 0 && selected_ >= 0) {
            if (selected_ < scroll_) {
                scroll_ = selected_;
            } else if (selected_ >= scroll_ + visible_) {
                scroll_ = selected_ - visible_ + 1;
            }
        }
        scroll_ = std::clamp<std::int64_t>(scroll_, 0, max_scroll());
    }

    std::string path_;
    std::vector<Row> rows_;
    std::int64_t selected_ = -1;
    std::int64_t hovered_ = -1;
    std::int64_t scroll_ = 0;
    std::int64_t visible_ = 0;
    std::int64_t x_ = 0, y_ = 0, w_ = 0, row_h_ = kCellPx;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_ELEMENTS_LIST_HPP
