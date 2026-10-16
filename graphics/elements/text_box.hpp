// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENTS_TEXT_BOX_HPP
#define ZENGINE_GRAPHICS_ELEMENTS_TEXT_BOX_HPP

// A one-line text box an owner draws into a Sketch and edits from the keys its area hears: an
// outlined box, the part of its text around the cursor, its placeholder while it is empty and
// unfocused, and a cursor while it is focused. The cursor is a byte offset into UTF-8 text that
// always stands between whole characters. Its face is the record at its path, carrying the action
// a press asks of the owner, which focuses it.

#include "graphics/canvas.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace zengine::graphics {

/// What a drawn text box means to a reader: its box in its parent's space, its whole text, its
/// placeholder, its cursor (a byte offset into the text), its state, and the paints it chose.
struct TextBoxFace {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::string text;
    std::string placeholder;
    std::int64_t cursor = 0;
    bool focused = false;
    bool enabled = true;
    Paint fill;
    Paint ink;
    Paint border;
    ZEN_SHAPE(TextBoxFace, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(text), ZEN_FIELD(placeholder), ZEN_FIELD(cursor), ZEN_FIELD(focused),
              ZEN_FIELD(enabled), ZEN_FIELD(fill), ZEN_FIELD(ink), ZEN_FIELD(border));
};

/// One line of text and a cursor in it. The editing calls a key makes do nothing while it is
/// disabled, and a disabled box is never focused; `set_text` and `clear` change it either way.
class TextBox {
public:
    explicit TextBox(std::string path, std::string placeholder = {})
        : path_(std::move(path)), placeholder_(std::move(placeholder)) {}

    void set_bounds(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) {
        x_ = x;
        y_ = y;
        w_ = w;
        h_ = h;
    }
    /// New text, the cursor after its end.
    void set_text(std::string text) {
        text_ = std::move(text);
        cursor_ = text_.size();
    }
    void set_placeholder(std::string placeholder) { placeholder_ = std::move(placeholder); }
    void set_focused(bool focused) { focused_ = focused && enabled_; }
    void set_enabled(bool enabled) {
        enabled_ = enabled;
        focused_ = focused_ && enabled;
    }
    void clear() {
        text_.clear();
        cursor_ = 0;
    }

    /// Text typed at the cursor, which moves past it.
    void insert(std::string_view typed) {
        if (enabled_) {
            text_.insert(cursor_, typed);
            cursor_ += typed.size();
        }
    }
    /// Removes the character before the cursor.
    void backspace() {
        if (enabled_ && cursor_ > 0) {
            const std::size_t from = before(cursor_);
            text_.erase(from, cursor_ - from);
            cursor_ = from;
        }
    }
    /// Removes the character after the cursor.
    void delete_forward() {
        if (enabled_ && cursor_ < text_.size()) {
            text_.erase(cursor_, after(cursor_) - cursor_);
        }
    }
    void move_left() {
        if (enabled_) {
            cursor_ = before(cursor_);
        }
    }
    void move_right() {
        if (enabled_) {
            cursor_ = after(cursor_);
        }
    }
    void move_home() {
        if (enabled_) {
            cursor_ = 0;
        }
    }
    void move_end() {
        if (enabled_) {
            cursor_ = text_.size();
        }
    }

    const std::string& path() const { return path_; }
    const std::string& text() const { return text_; }
    const std::string& placeholder() const { return placeholder_; }
    std::int64_t cursor() const { return static_cast<std::int64_t>(cursor_); }
    bool focused() const { return focused_; }
    bool enabled() const { return enabled_; }

    /// The record as the state stands.
    TextBoxFace face() const {
        return TextBoxFace{x_, y_, w_, h_, text_, placeholder_, cursor(), focused_, enabled_,
                           by_role(role::kGround), by_role(enabled_ ? role::kFill : role::kMuted),
                           by_role(focused_ ? role::kAccent : role::kMuted)};
    }

    /// Draws the box at its path, its text a cell in from its left edge; a press on it asks
    /// `action` of the area's owner while it is enabled, and nothing when `action` is empty.
    void sketch(Sketch& s, const std::string& action = {}) const {
        const TextBoxFace f = face();
        const bool hint = text_.empty() && !focused_;
        const std::string& shown = hint ? placeholder_ : text_;
        // A character a cell: the window of characters that holds the cursor, where the box's
        // width bounds it.
        const std::int64_t room = w_ / kCellPx - 2;
        const std::int64_t at = characters(std::string_view(text_).substr(0, cursor_));
        const std::int64_t first =
            room > 0 && !hint ? std::max<std::int64_t>(0, at - room + 1) : 0;
        const std::int64_t ty = h_ >= 3 * kCellPx ? (h_ / kCellPx - 1) / 2 * kCellPx : 0;
        s.group(path_, x_, y_, w_, h_);
        s.rect(path_ + "/box", 0, 0, w_, h_, f.fill, f.border);
        s.text(path_ + "/text", kCellPx, ty, window(shown, first, room),
               hint ? by_role(role::kMuted) : f.ink);
        if (focused_) {
            const std::int64_t cx = (1 + at - first) * kCellPx;
            s.line(path_ + "/cursor", cx, ty, cx, ty + kCellPx - 1, by_role(role::kAccent));
        }
        s.record(path_, f, enabled_ ? action : std::string());
    }

private:
    static bool continues(char c) { return (static_cast<unsigned char>(c) & 0xc0) == 0x80; }
    std::size_t before(std::size_t i) const {
        while (i > 0 && continues(text_[--i])) {
        }
        return i;
    }
    std::size_t after(std::size_t i) const {
        while (i < text_.size() && continues(text_[++i])) {
        }
        return std::min(i, text_.size());
    }
    static std::int64_t characters(std::string_view s) {
        return static_cast<std::int64_t>(std::count_if(s.begin(), s.end(),
                                                       [](char c) { return !continues(c); }));
    }
    /// The characters of `s` from the `first`th, at most `room` of them (all, when `room` is not
    /// positive).
    static std::string window(const std::string& s, std::int64_t first, std::int64_t room) {
        std::size_t from = 0;
        std::int64_t seen = 0;
        for (; from < s.size(); ++from) {
            if (!continues(s[from]) && seen++ == first) {
                break;
            }
        }
        if (room <= 0) {
            return s.substr(from);
        }
        std::size_t to = from;
        for (std::int64_t taken = 0; to < s.size(); ++to) {
            if (!continues(s[to]) && taken++ == room) {
                break;
            }
        }
        return s.substr(from, to - from);
    }

    std::string path_;
    std::string placeholder_;
    std::string text_;
    std::size_t cursor_ = 0;
    std::int64_t x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    bool focused_ = false;
    bool enabled_ = true;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_ELEMENTS_TEXT_BOX_HPP
