// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENTS_ICON_HPP
#define ZENGINE_GRAPHICS_ELEMENTS_ICON_HPP

// An icon an owner draws into a Sketch: a named outline from the table below, scaled into a
// square and drawn with the `path` operator in the ink of its role. A name the table does not
// hold is drawn as a muted frame, and its record still names it, so a reader reads the name.

#include "graphics/canvas.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace zengine::graphics {

/// What a drawn icon means to a reader: its square in its parent's space, its name, and its ink.
struct IconFace {
    std::int64_t x = 0, y = 0, size = 0;
    std::string name;
    Paint ink;
    ZEN_SHAPE(IconFace, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(size), ZEN_FIELD(name),
              ZEN_FIELD(ink));
};

/// The icons by name: each a closed outline, as x y pairs in a square `kCellPx` on a side, and
/// each holding that square's centre, so an icon one terminal cell large fills its cell.
struct IconOutline {
    std::string_view name;
    std::string_view points;
};
inline constexpr IconOutline kIconOutlines[] = {
    {"check", "1 4 4 7 10 1 11 3 4 10 0 6"},
    {"cross", "2 1 6 5 10 1 11 2 7 6 11 10 10 11 6 7 2 11 1 10 5 6 1 2"},
    {"plus", "5 1 7 1 7 5 11 5 11 7 7 7 7 11 5 11 5 7 1 7 1 5 5 5"},
    {"minus", "1 5 11 5 11 7 1 7"},
    {"up", "6 2 11 10 1 10"},
    {"down", "1 2 11 2 6 10"},
    {"left", "2 6 10 1 10 11"},
    {"right", "2 1 10 6 2 11"},
    {"square", "2 2 10 2 10 10 2 10"},
    {"dot", "4 2 8 2 10 4 10 8 8 10 4 10 2 8 2 4"},
};

/// A named icon in a square of `size` pixels, inked by a theme role (`role::kFill` by default).
class Icon {
public:
    explicit Icon(std::string path, std::string name = {}, std::int64_t size = 16)
        : path_(std::move(path)), name_(std::move(name)), size_(size) {}

    void set_name(std::string name) { name_ = std::move(name); }
    void set_position(std::int64_t x, std::int64_t y) {
        x_ = x;
        y_ = y;
    }
    void set_size(std::int64_t size) { size_ = size; }
    void set_role(std::int64_t ink_role) { role_ = ink_role; }

    const std::string& path() const { return path_; }
    const std::string& name() const { return name_; }
    std::int64_t size() const { return size_; }
    std::int64_t role() const { return role_; }

    /// The outline the table holds for `name`, or "" for a name it does not hold.
    static std::string_view outline(std::string_view name) {
        for (const IconOutline& o : kIconOutlines) {
            if (o.name == name) {
                return o.points;
            }
        }
        return {};
    }
    /// An outline scaled from its `kCellPx` square to one of `size`, as SVG path data.
    static std::string path_data(std::string_view points, std::int64_t size) {
        std::string d;
        std::int64_t value = 0;
        bool digits = false;
        std::size_t count = 0;
        const auto flush = [&] {
            if (!digits) {
                return;
            }
            d += count == 0 ? "M " : count % 2 == 0 ? " L " : " ";
            d += std::to_string(value * size / kCellPx);
            ++count;
            value = 0;
            digits = false;
        };
        for (const char c : points) {
            if (c >= '0' && c <= '9') {
                value = value * 10 + (c - '0');
                digits = true;
            } else {
                flush();
            }
        }
        flush();
        return d.empty() ? d : d + " Z";
    }

    /// The record as the state stands.
    IconFace face() const { return IconFace{x_, y_, size_, name_, by_role(role_)}; }

    /// Draws the icon at its path; a press on it asks `action` of the area's owner, and nothing
    /// when `action` is empty.
    void sketch(Sketch& s, const std::string& action = {}) const {
        const IconFace f = face();
        s.group(path_, x_, y_, size_, size_);
        const std::string_view points = outline(name_);
        if (points.empty()) {
            s.outline_rect(path_ + "/shape", 0, 0, size_, size_, by_role(role::kMuted));
        } else {
            s.path(path_ + "/shape", path_data(points, size_), f.ink);
        }
        s.record(path_, f, action);
    }

private:
    std::string path_;
    std::string name_;
    std::int64_t size_ = 16;
    std::int64_t x_ = 0, y_ = 0;
    std::int64_t role_ = role::kFill;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_ELEMENTS_ICON_HPP
