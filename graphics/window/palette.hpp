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

/// The terminal's own look where no palette sets one: graphics/terminal.json's.
inline const TerminalLook& terminal_defaults() {
    static const TerminalLook kDefaults = themes::terminal();
    return kDefaults;
}

namespace detail {
inline const TerminalInk* ink_entry(const std::vector<TerminalInk>& inks, std::string_view name) {
    for (const TerminalInk& i : inks) {
        if (i.name == name) {
            return &i;
        }
    }
    return nullptr;
}
} // namespace detail

/// A COLOUR'S INK AND FILL IN A TERMINAL, as `palette`'s terminal look says, each field else as
/// the terminal's defaults say; a fill named by neither is the `text` colour's.
inline TerminalInk terminal_ink_of(std::string_view name, const PaletteOffered& palette) {
    TerminalInk out{std::string(name), {}, {}};
    for (const std::vector<TerminalInk>* inks : {&palette.terminal.inks, &terminal_defaults().inks}) {
        if (const TerminalInk* i = detail::ink_entry(*inks, name)) {
            if (out.ink.empty()) {
                out.ink = i->ink;
            }
            if (out.fill.empty()) {
                out.fill = i->fill;
            }
        }
    }
    if (out.fill.empty() && name != colors::text.name) {
        out.fill = terminal_ink_of(colors::text.name, palette).fill;
    }
    return out;
}

/// A colour the palette holds, as a painter takes it: its colour, and its terminal look's ink and
/// fill.
inline PaintInk ink_of(const PaletteColor& c, const PaletteOffered& palette) {
    TerminalInk t = terminal_ink_of(c.name, palette);
    return PaintInk{c.rgba, std::move(t.ink), std::move(t.fill)};
}

/// A colour of the draw's own (an exact one, or an own value's exact default), as a painter takes
/// it: what a terminal shows of it is the pool colour it is `like`, else the terminal's nearest,
/// filled with the `text` colour's fill.
inline PaintInk own_ink(std::int64_t rgba, std::string_view like, const PaletteOffered& palette) {
    TerminalInk t = terminal_ink_of(like.empty() ? std::string_view(colors::text.name) : like,
                                    palette);
    return PaintInk{rgba, like.empty() ? std::string() : std::move(t.ink), std::move(t.fill)};
}

/// The own values `palette` sets for `role`; none where it sets none.
inline const OwnValues* own_values(const PaletteOffered& palette, const std::string& role) {
    for (const OwnValues& o : palette.own) {
        if (o.role == role) {
            return &o;
        }
    }
    return nullptr;
}

/// A pool colour as painted in `palette`: as the palette says, else the pool's default, else `text`.
inline PaintInk pool_ink(std::string_view name, const PaletteOffered& palette) {
    if (const PaletteColor* p = pool_color(palette, name)) {
        return ink_of(*p, palette);
    }
    if (const PaletteColor* p = pool_color(palette, colors::text.name)) {
        return ink_of(*p, palette);
    }
    return own_ink(0xffffffff, {}, palette);
}

/// A COLOUR A DRAW NAMES, AS PAINTED in `palette` in an area `role` owns: an own value as the
/// palette sets it for that role, else as its default -- its exact colour, or its pool colour.
inline PaintInk resolve(const Color& c, const PaletteOffered& palette, const std::string& role) {
    switch (c.from) {
    case color_from::kExact:
        return own_ink(c.rgba, c.like, palette);
    case color_from::kPool:
        return pool_ink(c.name, palette);
    case color_from::kOwn:
    case color_from::kOwnPool:
        if (const OwnValues* o = own_values(palette, role)) {
            if (const PaletteColor* p = find_color(o->colors, c.name)) {
                return ink_of(*p, palette);
            }
        }
        return c.from == color_from::kOwn ? own_ink(c.rgba, c.like, palette)
                                          : pool_ink(c.like, palette);
    default:
        return PaintInk{};
    }
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

/// A pool style in `palette`: its own, else the pool's default; nothing where neither holds it.
inline const PaletteStyle* pool_style(const PaletteOffered& palette, std::string_view name) {
    for (const std::vector<PaletteStyle>* styles : {&palette.styles, &built_in_palette().styles}) {
        for (const PaletteStyle& s : *styles) {
            if (s.name == name) {
                return &s;
            }
        }
    }
    return nullptr;
}

/// The points a text set without a size of its own stands at where its style names no size the
/// palette holds: the window's own.
inline constexpr std::int64_t kFallbackPoints = 13;

/// A STYLE A DRAW NAMES, AS SET in `palette` in an area `role` owns: an own style as the palette
/// sets it for that role, else as the pool style it is `like`; a pool style as the palette says,
/// else as the pool's own default, else `body`'s; each of its fields a root's name.
inline PaletteStyle resolve_style(const TextStyle& style, const PaletteOffered& palette,
                                  const std::string& role = {}) {
    if (style.from == style_from::kOwn) {
        if (const OwnValues* o = own_values(palette, role)) {
            for (const PaletteStyle& s : o->styles) {
                if (s.name == style.name) {
                    return s;
                }
            }
        }
        return resolve_style(TextStyle{style_from::kPool, style.like, {}}, palette);
    }
    if (const PaletteStyle* s = pool_style(palette, style.pool_name())) {
        return *s;
    }
    if (const PaletteStyle* s = pool_style(palette, kBodyStyle)) {
        return *s;
    }
    return PaletteStyle{kBodyStyle, kTextFont, {}, colors::text.name, text_weight::kRegular};
}

/// The size a size root stands at in `palette`: its own, else the pool's default, else the
/// window's own points.
inline PaletteSize root_size(const std::string& name, const PaletteOffered& palette) {
    if (const PaletteSize* s = pool_size(palette, name); s != nullptr && s->points > 0) {
        return *s;
    }
    return PaletteSize{name, kFallbackPoints};
}

/// The points the pool's `body` style stands at in `palette`.
inline std::int64_t body_points(const PaletteOffered& palette) {
    return root_size(resolve_style(TextStyle{}, palette).size, palette).points;
}

/// A TEXT SIZE A DRAW NAMES, AS SET in `palette` in an area `role` owns, in a style that stands at
/// `style_size`: a pool size as the palette says, else as the pool's own default; an own value as
/// the palette sets it for the area's owner, else at its default, its points or its pool size;
/// none, the style's. Its name is the pool's, or the own value's.
inline PaletteSize resolve_size(const TextSize& size, const PaletteOffered& palette,
                                const std::string& role, const std::string& style_size) {
    if (size.from == size_from::kOwn || size.from == size_from::kOwnPool) {
        if (const OwnValues* o = own_values(palette, role)) {
            for (const PaletteSize& z : o->sizes) {
                if (z.name == size.name && z.points > 0) {
                    return z;
                }
            }
        }
        if (size.from == size_from::kOwn && size.points > 0) {
            return PaletteSize{size.name, size.points};
        }
        if (size.from == size_from::kOwnPool && !size.like.empty()) {
            return PaletteSize{size.name, root_size(size.like, palette).points};
        }
        return root_size(style_size, palette);
    }
    if (!size.name.empty()) {
        if (const PaletteSize* s = pool_size(palette, size.name); s != nullptr && s->points > 0) {
            return *s;
        }
    }
    return root_size(style_size, palette);
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

/// A TEXT AS SET in `palette` in an area `role` owns: its font, size, colour and weight, each its
/// own where its draw names one, else its style's.
struct TextSet {
    std::string font;
    PaletteSize size;
    Color color;
    std::int64_t weight = text_weight::kRegular;
};

inline TextSet resolve_text(const TextArgs& t, const PaletteOffered& palette,
                            const std::string& role) {
    const PaletteStyle style = resolve_style(t.style, palette, role);
    TextSet set;
    set.font = !t.font.empty() ? t.font : (!style.font.empty() ? style.font : kTextFont);
    set.size = resolve_size(t.size, palette, role, style.size);
    set.color = !t.ink.none() ? t.ink
                              : Color{color_from::kPool,
                                      style.color.empty() ? std::string(colors::text.name)
                                                          : style.color,
                                      0,
                                      {}};
    set.weight = t.weight != text_weight::kOfStyle ? t.weight : style.weight;
    return set;
}

/// The colours a node's operands name, as painted in `palette` in an area `role` owns, in the
/// order a painter takes them (`colors_of`): a text naming no colour of its own in its style's.
inline std::vector<PaintInk> inks_of(const Node& node, const PaletteOffered& palette,
                                     const std::string& role) {
    std::vector<PaintInk> out;
    if (const TextArgs* t = operands<TextArgs>(node)) {
        out.push_back(resolve(resolve_text(*t, palette, role).color, palette, role));
        return out;
    }
    for (const Color& c : colors_of(node)) {
        out.push_back(resolve(c, palette, role));
    }
    return out;
}

/// A task's own colours and border characters, as `palette` gives them for an area `role` owns: its text,
/// its quiet, and a terminal's corner, across and down.
inline void dress(PaintTask& task, const PaletteOffered& palette, const std::string& role) {
    task.plain = resolve(colors::text, palette, role);
    task.quiet = resolve(colors::quiet, palette, role);
    task.outline = !palette.terminal.borders.empty() ? palette.terminal.borders
                                                     : terminal_defaults().borders;
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
