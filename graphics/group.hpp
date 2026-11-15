// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_GROUP_HPP
#define ZENGINE_GRAPHICS_GROUP_HPP

// A group in a drawing, and a writer for what is drawn inside it by each node's own name: it draws
// the group first, so a node in it always has its parent, and no call spells a whole path. A layer
// a game counts on is a group drawn in its place in the picture: everything drawn in it later paints
// over what an earlier group holds and under what a later one does. Reference:
// graphics/docs/graphics.md, "Helpers" and "Paint order".

#include "graphics/box.hpp"
#include "graphics/canvas.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace zengine::graphics {

/// A GROUP IN A DRAWING: drawn when it is made, at its place in the drawing's paint order, and the
/// calls for what goes inside it, each taking a node's own name -- `"walls"` in the group `"rogue"`
/// is the node `"rogue/walls"`. Places inside are the group's own: (0, 0) is its top left corner.
class Group {
public:
    /// The group `name` at (x, y), clipped to `clip_w` by `clip_h` where both are given.
    Group(Sketch& drawing, std::string name, std::int64_t x = 0, std::int64_t y = 0,
          std::int64_t clip_w = 0, std::int64_t clip_h = 0)
        : drawing_(&drawing), path_(std::move(name)) {
        drawing_->group(path_, x, y, clip_w, clip_h);
    }
    /// The group `name` placed at a box and clipped to it.
    Group(Sketch& drawing, std::string name, const Box& box)
        : Group(drawing, std::move(name), box.x, box.y, box.w, box.h) {}

    /// A group inside this one.
    Group group(const std::string& name, std::int64_t x = 0, std::int64_t y = 0,
                std::int64_t clip_w = 0, std::int64_t clip_h = 0) const {
        return Group(*drawing_, name_of(name), x, y, clip_w, clip_h);
    }
    Group group(const std::string& name, const Box& box) const {
        return Group(*drawing_, name_of(name), box);
    }

    // ---- the drawing calls, by a node's own name -----------------------------------------------
    void rect(const std::string& name, const Box& b, Color fill, Color stroke = no_color()) const {
        drawing_->rect(name_of(name), b.x, b.y, b.w, b.h, std::move(fill), std::move(stroke));
    }
    void fill_rect(const std::string& name, const Box& b, Color paint) const {
        drawing_->fill_rect(name_of(name), b.x, b.y, b.w, b.h, std::move(paint));
    }
    void outline_rect(const std::string& name, const Box& b, Color paint) const {
        drawing_->outline_rect(name_of(name), b.x, b.y, b.w, b.h, std::move(paint));
    }
    void ellipse(const std::string& name, const Box& b, Color fill,
                 Color stroke = no_color()) const {
        drawing_->ellipse(name_of(name), b.x, b.y, b.w, b.h, std::move(fill), std::move(stroke));
    }
    void line(const std::string& name, std::int64_t x1, std::int64_t y1, std::int64_t x2,
              std::int64_t y2, Color ink) const {
        drawing_->line(name_of(name), x1, y1, x2, y2, std::move(ink));
    }
    /// SVG path data (M L H V C Q Z), in this group's places.
    void path(const std::string& name, const std::string& d, Color fill,
              Color stroke = no_color()) const {
        drawing_->path(name_of(name), d, std::move(fill), std::move(stroke));
    }
    void text(const std::string& name, std::int64_t x, std::int64_t y, const std::string& words,
              Color ink, TextSize size = {}) const {
        drawing_->text(name_of(name), x, y, words, std::move(ink), std::move(size));
    }
    void text(const std::string& name, std::int64_t x, std::int64_t y, const std::string& words,
              const TextLook& look) const {
        drawing_->text(name_of(name), x, y, words, look);
    }
    void image(const std::string& name, const Box& b, std::int64_t pw, std::int64_t ph,
               loom::Bytes rgba) const {
        drawing_->image(name_of(name), b.x, b.y, b.w, b.h, pw, ph, std::move(rgba));
    }
    void sheet(const std::string& name, SheetArgs sheet) const {
        drawing_->sheet(name_of(name), std::move(sheet));
    }
    void tiles(const std::string& name, TilesArgs tiles) const {
        drawing_->tiles(name_of(name), std::move(tiles));
    }
    void sprites(const std::string& name, SpritesArgs sprites) const {
        drawing_->sprites(name_of(name), std::move(sprites));
    }
    void points(const std::string& name, PointsArgs points) const {
        drawing_->points(name_of(name), std::move(points));
    }
    /// What a press on the node `name` asks of the area's owner.
    void action(const std::string& name, const std::string& action) const {
        drawing_->action(name_of(name), action);
    }

    /// The whole name of the node `name` inside this group: `"rogue/walls"` for `"walls"`.
    std::string name_of(const std::string& name) const { return path_ + "/" + name; }
    /// The group's own whole name.
    const std::string& path() const { return path_; }
    Sketch& drawing() const { return *drawing_; }

private:
    Sketch* drawing_;
    std::string path_;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_GROUP_HPP
