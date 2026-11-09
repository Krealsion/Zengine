// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_PALETTE_HPP
#define ZENGINE_GRAPHICS_WINDOW_PALETTE_HPP

// Where a colour a draw names becomes the colour it paints: the window, from the palette the area
// wears, at the moment it paints. An exact colour paints as itself; a pool name as the palette
// says, else as the pool's own default, else as `text`; a slot as the palette says for the area's
// owner, else as the default the drawing carries. Nothing else in the tree resolves a name.
// Reference: graphics/docs/graphics.md, "Colours by name".

#include "graphics/ops.hpp"
#include "graphics/painting.hpp"
#include "graphics/themes.hpp" // generated from graphics/pool.json and graphics/themes/

#include <string>
#include <string_view>
#include <vector>

namespace zengine::graphics::window {

/// The window's own palette: the pool's defaults, which an area wearing no palette the window
/// holds wears.
inline const PaletteOffered& built_in_palette() {
    static const PaletteOffered kBuiltIn = themes::built_in();
    return kBuiltIn;
}

inline const PaletteColor* find_color(const std::vector<PaletteColor>& colors,
                                      std::string_view name) {
    for (const PaletteColor& c : colors) {
        if (c.name == name) {
            return &c;
        }
    }
    return nullptr;
}

/// A pool colour in `palette`: its own, else the pool's default; nothing where neither holds it.
inline const PaletteColor* pool_color(const PaletteOffered& palette, std::string_view name) {
    if (const PaletteColor* c = find_color(palette.colors, name)) {
        return c;
    }
    return find_color(built_in_palette().colors, name);
}

/// A colour the palette holds, as a painter takes it: a fill character it names none of is its
/// palette's `text` fill.
inline PaintInk ink_of(const PaletteColor& c, const PaletteOffered& palette) {
    PaintInk ink{c.rgba, c.cells, c.fill};
    if (ink.fill.empty()) {
        if (const PaletteColor* text = pool_color(palette, colors::text.name)) {
            ink.fill = text->fill;
        }
    }
    return ink;
}

/// A colour of the draw's own (an exact one, or a slot no theme sets), as a painter takes it: what
/// a terminal shows of it is the pool colour it is `like`, else the terminal's nearest, filled with
/// the palette's `text` fill.
inline PaintInk own_ink(std::int64_t rgba, std::string_view like, const PaletteOffered& palette) {
    PaintInk ink{rgba, {}, {}};
    const PaletteColor* stand_in = like.empty() ? nullptr : pool_color(palette, like);
    if (stand_in != nullptr) {
        ink.cells = stand_in->cells;
        ink.fill = stand_in->fill;
    }
    if (ink.fill.empty()) {
        if (const PaletteColor* text = pool_color(palette, colors::text.name)) {
            ink.fill = text->fill;
        }
    }
    return ink;
}

/// A COLOUR A DRAW NAMES, AS PAINTED in `palette` in an area `role` owns.
inline PaintInk resolve(const Color& c, const PaletteOffered& palette, const std::string& role) {
    switch (c.from) {
    case color_from::kExact:
        return own_ink(c.rgba, c.like, palette);
    case color_from::kPool:
        if (const PaletteColor* p = pool_color(palette, c.name)) {
            return ink_of(*p, palette);
        }
        if (const PaletteColor* p = pool_color(palette, colors::text.name)) {
            return ink_of(*p, palette);
        }
        return own_ink(0xffffffff, {}, palette);
    case color_from::kSlot:
        for (const PaletteSlots& s : palette.slots) {
            if (s.role == role) {
                if (const PaletteColor* p = find_color(s.colors, c.name)) {
                    return ink_of(*p, palette);
                }
            }
        }
        return own_ink(c.rgba, c.like, palette);
    default:
        return PaintInk{};
    }
}

/// The colours a node's operands name, as painted in `palette` in an area `role` owns, in the
/// order a painter takes them (`colors_of`).
inline std::vector<PaintInk> inks_of(const std::string& op, const loom::Bytes& args,
                                     const PaletteOffered& palette, const std::string& role) {
    std::vector<PaintInk> out;
    for (const Color& c : colors_of(op, args)) {
        out.push_back(resolve(c, palette, role));
    }
    return out;
}

// ---- A text's size and font -----------------------------------------------------------------

/// A pool size in `palette`: its own, else the pool's default; nothing where neither holds it.
inline const PaletteSize* pool_size(const PaletteOffered& palette, std::string_view name) {
    for (const PaletteSize& s : palette.sizes) {
        if (s.name == name) {
            return &s;
        }
    }
    for (const PaletteSize& s : built_in_palette().sizes) {
        if (s.name == name) {
            return &s;
        }
    }
    return nullptr;
}

/// The points the pool's `body` stands at in `palette`, and the window's own where no palette
/// holds one.
inline std::int64_t body_points(const PaletteOffered& palette) {
    const PaletteSize* s = pool_size(palette, kBodySize);
    return s != nullptr && s->points > 0 ? s->points : 13;
}

/// A TEXT SIZE A DRAW NAMES, AS SET in `palette` in an area `role` owns: a pool size as the
/// palette says, else as the pool's own default, else as `body`; a slot as the palette says for the
/// area's owner, else at the points the drawing carries, else as `body`. Its name is the pool's, or
/// the slot's.
inline PaletteSize resolve_size(const TextSize& size, const PaletteOffered& palette,
                                const std::string& role) {
    if (size.from == size_from::kSlot) {
        for (const PaletteSlots& s : palette.slots) {
            if (s.role != role) {
                continue;
            }
            for (const PaletteSize& z : s.sizes) {
                if (z.name == size.name && z.points > 0) {
                    return z;
                }
            }
        }
        if (size.points > 0) {
            return PaletteSize{size.name, size.points};
        }
        return PaletteSize{kBodySize, body_points(palette)};
    }
    const std::string name = size.pool_name();
    if (const PaletteSize* s = pool_size(palette, name); s != nullptr && s->points > 0) {
        return *s;
    }
    return PaletteSize{kBodySize, body_points(palette)};
}

/// THE FACE A FONT A DRAW NAMES IS SET IN, in `palette`: the font's face as the palette says, else
/// as the pool's own default, else the pool's `text`'s.
inline std::string resolve_face(const std::string& font, const PaletteOffered& palette) {
    const std::string name = font.empty() ? std::string(kTextFont) : font;
    for (const std::vector<PaletteFont>* fonts : {&palette.fonts, &built_in_palette().fonts}) {
        for (const PaletteFont& f : *fonts) {
            if (f.name == name) {
                return f.face;
            }
        }
    }
    for (const PaletteFont& f : built_in_palette().fonts) {
        if (f.name == kTextFont) {
            return f.face;
        }
    }
    return {};
}

/// A task's own colours and outline, as `palette` gives them for an area `role` owns: its text,
/// its quiet, and a terminal's corner, across and down.
inline void dress(PaintTask& task, const PaletteOffered& palette, const std::string& role) {
    task.plain = resolve(colors::text, palette, role);
    task.quiet = resolve(colors::quiet, palette, role);
    task.outline = palette.outline;
}

/// The colour the medium shows where nothing is drawn, in `palette`: its `ground`.
inline std::int64_t ground_of(const PaletteOffered& palette) {
    const PaletteColor* g = pool_color(palette, colors::ground.name);
    return g != nullptr ? g->rgba : 0x000000ff;
}

/// The palettes a window holds, the latest revision of each, kept in its state.
inline const PaletteOffered* palette_named(const std::vector<PaletteOffered>& held,
                                           std::string_view name) {
    for (const PaletteOffered& p : held) {
        if (p.palette == name) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_PALETTE_HPP
