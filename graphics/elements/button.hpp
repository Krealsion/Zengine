// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENTS_BUTTON_HPP
#define ZENGINE_GRAPHICS_ELEMENTS_BUTTON_HPP

// A button an owner draws into a Sketch: a face over its box and its label, `[label]` while
// enabled and `(label)` while not, so a medium without colour still says which. Its face is the
// record at its path, carrying the action a press asks of the area's owner; with no action, or
// disabled, a press on it sends nothing.

#include "graphics/canvas.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace zengine::graphics {

/// What a drawn button means to a reader: its box in its parent's space, its label, the paints
/// its state chose, and that state.
struct ButtonFace {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::string label;
    Paint fill;
    Paint ink;
    bool enabled = true;
    bool pressed = false;
    bool hovered = false;
    ZEN_SHAPE(ButtonFace, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h),
              ZEN_FIELD(label), ZEN_FIELD(fill), ZEN_FIELD(ink), ZEN_FIELD(enabled),
              ZEN_FIELD(pressed), ZEN_FIELD(hovered));
};

/// A label that asks for an action when pressed. Pressed fills its face with the accent; hovered
/// inks it with the accent; disabled inks it muted and never looks pressed.
class Button {
public:
    explicit Button(std::string path, std::string label = {})
        : path_(std::move(path)), label_(std::move(label)) {}

    void set_label(std::string label) { label_ = std::move(label); }
    void set_bounds(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) {
        x_ = x;
        y_ = y;
        w_ = w;
        h_ = h;
    }
    void set_enabled(bool enabled) {
        enabled_ = enabled;
        pressed_ = pressed_ && enabled;
    }
    void press() { pressed_ = enabled_; }
    void release() { pressed_ = false; }
    void hover(bool hovered) { hovered_ = hovered; }

    const std::string& path() const { return path_; }
    const std::string& label() const { return label_; }
    bool enabled() const { return enabled_; }
    bool pressed() const { return pressed_; }
    bool hovered() const { return hovered_; }

    /// The record as the state stands.
    ButtonFace face() const {
        ButtonFace f{x_, y_, w_, h_, label_, by_role(role::kGround), by_role(role::kFill),
                     enabled_, pressed_, hovered_};
        if (!enabled_) {
            f.ink = by_role(role::kMuted);
        } else if (pressed_) {
            f.fill = by_role(role::kAccent);
        } else if (hovered_) {
            f.ink = by_role(role::kAccent);
        }
        return f;
    }

    /// Draws the button at its path; a press on it asks `action` of the area's owner while it is
    /// enabled, and nothing when `action` is empty.
    void sketch(Sketch& s, const std::string& action = {}) const {
        const ButtonFace f = face();
        const std::string words = enabled_ ? "[" + label_ + "]" : "(" + label_ + ")";
        const auto cells = static_cast<std::int64_t>(words.size());
        const std::int64_t tx = std::max<std::int64_t>(0, (w_ / kCellPx - cells) / 2) * kCellPx;
        const std::int64_t ty = std::max<std::int64_t>(0, (h_ / kCellPx - 1) / 2) * kCellPx;
        s.group(path_, x_, y_, w_, h_);
        s.fill_rect(path_ + "/face", 0, 0, w_, h_, f.fill);
        s.text(path_ + "/label", tx, ty, words, f.ink);
        s.record(path_, f, enabled_ ? action : std::string());
    }

private:
    std::string path_;
    std::string label_;
    std::int64_t x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    bool enabled_ = true;
    bool pressed_ = false;
    bool hovered_ = false;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_ELEMENTS_BUTTON_HPP
