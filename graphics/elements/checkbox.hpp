// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENTS_CHECKBOX_HPP
#define ZENGINE_GRAPHICS_ELEMENTS_CHECKBOX_HPP

// A checkbox an owner draws into a Sketch: a mark, `[x]` or `[ ]`, and its label beside it, over
// a face as wide as its box, so a press anywhere on it lands. Its face is the record at its path,
// carrying the action a press asks of the owner, which flips its `checked` and draws it again.

#include "graphics/canvas.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace zengine::graphics {

/// What a drawn checkbox means to a reader: its box in its parent's space, its label, its state,
/// and the paints that state chose for its face, its mark and its label.
struct CheckboxFace {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::string label;
    bool checked = false;
    bool enabled = true;
    bool hovered = false;
    Paint box_fill;
    Paint check_ink;
    Paint label_ink;
    ZEN_SHAPE(CheckboxFace, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(label), ZEN_FIELD(checked), ZEN_FIELD(enabled), ZEN_FIELD(hovered),
              ZEN_FIELD(box_fill), ZEN_FIELD(check_ink), ZEN_FIELD(label_ink));
};

/// A label with a yes or no beside it. Hovered inks it with the accent; disabled inks it muted.
/// Its state is the owner's: `toggle` and `set_checked` change it whether or not it is enabled.
class Checkbox {
public:
    explicit Checkbox(std::string path, std::string label = {}, bool checked = false)
        : path_(std::move(path)), label_(std::move(label)), checked_(checked) {}

    void set_label(std::string label) { label_ = std::move(label); }
    void set_bounds(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) {
        x_ = x;
        y_ = y;
        w_ = w;
        h_ = h;
    }
    void set_checked(bool checked) { checked_ = checked; }
    void toggle() { checked_ = !checked_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    void hover(bool hovered) { hovered_ = hovered; }

    const std::string& path() const { return path_; }
    const std::string& label() const { return label_; }
    bool checked() const { return checked_; }
    bool enabled() const { return enabled_; }
    bool hovered() const { return hovered_; }

    /// The record as the state stands.
    CheckboxFace face() const {
        const Paint ink = !enabled_  ? by_role(role::kMuted)
                          : hovered_ ? by_role(role::kAccent)
                                     : by_role(role::kFill);
        return CheckboxFace{x_, y_, w_, h_, label_, checked_, enabled_, hovered_,
                            by_role(role::kGround), ink, ink};
    }

    /// Draws the checkbox at its path; a press on it asks `action` of the area's owner while it is
    /// enabled, and nothing when `action` is empty.
    void sketch(Sketch& s, const std::string& action = {}) const {
        const CheckboxFace f = face();
        s.group(path_, x_, y_, w_, h_);
        s.fill_rect(path_ + "/face", 0, 0, w_, h_, f.box_fill);
        s.text(path_ + "/mark", 0, 0, checked_ ? "[x]" : "[ ]", f.check_ink);
        s.text(path_ + "/label", 4 * kCellPx, 0, label_, f.label_ink);
        s.record(path_, f, enabled_ ? action : std::string());
    }

private:
    std::string path_;
    std::string label_;
    bool checked_ = false;
    std::int64_t x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    bool enabled_ = true;
    bool hovered_ = false;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_ELEMENTS_CHECKBOX_HPP
