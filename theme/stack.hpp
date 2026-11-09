// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_THEME_STACK_HPP
#define ZENGINE_THEME_STACK_HPP

// HOW A STACK IS LAID, as values: its themes, bottom first, each over the pool's defaults and the
// terminal's own look and over the theme below it, into the one palette its areas wear. A theme of
// one medium alone is a normal link, applied in its place and converted as well as it can be -- a
// window colour to its nearest ink, an ink to the colour xterm shows it in, `terminal default`
// falling through to the theme below -- and said with a warning. A pane's own value a theme sets is
// judged against what its role declares. Reference: theme/README.md, "A stack".

#include "theme/theme_persist.hpp"
#include "theme/vocabulary.hpp"

#include "graphics/colors.hpp" // generated: the pool's names
#include "graphics/terminal_inks.hpp"
#include "graphics/text_style.hpp"
#include "graphics/themes.hpp" // generated: the pool's defaults and the terminal's own look
#include "graphics/vocabulary.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace zengine::theme {

/// What a link of the other medium alone is said with, as it is put in.
inline constexpr const char* kWindowAloneWarning =
    "a window theme alone: a terminal shows each colour it gives as the nearest of its sixteen inks";
inline constexpr const char* kTerminalAloneWarning =
    "a terminal theme alone: a window shows each ink it gives as the colour xterm shows it in, and "
    "`terminal default` as the theme below";

/// A role's own values as it declared them.
using Declared = std::map<std::string, graphics::OwnValuesDeclared>;

/// A STACK LAID: the palette, named for the stack, and what laying it said.
struct Laid {
    graphics::PaletteOffered palette;
    StackLaid laid;
};

namespace detail {

template <class T> T* named(std::vector<T>& items, const std::string& name) {
    for (T& i : items) {
        if (i.name == name) return &i;
    }
    return nullptr;
}

template <class T> bool holds_name(const std::vector<T>& items, const std::string& name) {
    return std::any_of(items.begin(), items.end(), [&](const T& i) { return i.name == name; });
}

inline void color_over(std::vector<graphics::PaletteColor>& into, const std::string& name,
                       std::int64_t rgba) {
    if (graphics::PaletteColor* c = named(into, name)) {
        c->rgba = rgba;
    } else {
        into.push_back(graphics::PaletteColor{name, rgba});
    }
}

inline void ink_over(std::vector<graphics::TerminalInk>& into, const ThemeInk& i) {
    graphics::TerminalInk* have = named(into, i.name);
    if (have == nullptr) {
        into.push_back(graphics::TerminalInk{i.name, i.ink, i.fill});
        return;
    }
    if (!i.ink.empty()) have->ink = i.ink;
    if (!i.fill.empty()) have->fill = i.fill;
}

inline std::int64_t weight_of(const std::string& w, std::int64_t below) {
    return w == "bold" ? graphics::text_weight::kBold
                       : (w == "regular" ? graphics::text_weight::kRegular : below);
}

/// A style over `below`, each field it gives.
inline graphics::PaletteStyle style_over(graphics::PaletteStyle below, const ThemeStyle& s) {
    below.name = s.name;
    if (!s.font.empty()) below.font = s.font;
    if (!s.size.empty()) below.size = s.size;
    if (!s.color.empty()) below.color = s.color;
    below.weight = weight_of(s.weight, below.weight);
    return below;
}

/// Why a style's roots are not the pool's, or "".
inline std::string style_roots_refused(const ThemeStyle& s, const graphics::PaletteOffered& pool) {
    if (!s.font.empty() && !holds_name(pool.fonts, s.font)) return "the pool declares no font `" + s.font + "`";
    if (!s.size.empty() && !holds_name(pool.sizes, s.size)) return "the pool declares no size `" + s.size + "`";
    if (!s.color.empty() && !holds_name(pool.colors, s.color)) return "the pool declares no colour `" + s.color + "`";
    return {};
}

inline const graphics::OwnDeclared* declared_value(const graphics::OwnValuesDeclared& d,
                                                   const std::string& name, const char* kind) {
    for (const graphics::OwnDeclared& v : d.values) {
        if (v.name == name && v.kind == kind) return &v;
    }
    return nullptr;
}

} // namespace detail

/// LAY `themes`, BOTTOM FIRST, through `resolve` (a name to a `ThemeResolved`), judging own values
/// against `declared`: the palette named `stack_palette(themes)` at revision 0, each link as it was
/// put in, and each entry left out with why. A link that does not resolve is left out and said; the
/// rest stand.
inline Laid lay_stack(const std::vector<std::string>& themes,
                      const std::function<ThemeResolved(const std::string&)>& resolve,
                      const Declared& declared = {}) {
    const graphics::PaletteOffered pool = graphics::themes::built_in();
    Laid out;
    out.palette = pool;
    out.palette.palette = stack_palette(themes);
    out.palette.revision = 0;
    out.laid.palette = out.palette.palette;
    graphics::PaletteOffered& p = out.palette;
    for (const std::string& name : themes) {
        const ThemeResolved r = resolve(name);
        ThemeLink link{name, r.window, r.terminal, {}, {}};
        if (!r.accepted) {
            link.window = link.terminal = false;
            link.refusal = r.refusal;
            out.laid.links.push_back(std::move(link));
            continue;
        }
        const auto skip = [&](std::string role, std::string entry, std::string why) {
            out.laid.skipped.push_back(OwnSkipped{name, std::move(role), std::move(entry), std::move(why)});
        };
        // ITS WINDOW LOOK: the window file's, or its inks as the colours xterm shows them in.
        if (r.window) {
            for (const ThemeColor& c : r.colors) {
                if (!detail::holds_name(pool.colors, c.name)) {
                    skip({}, c.name, "the pool declares no colour `" + c.name + "`");
                } else if (const auto v = rgba_of(c.color)) {
                    detail::color_over(p.colors, c.name, *v);
                }
            }
            for (const ThemeFont& f : r.fonts) {
                if (graphics::PaletteFont* have = detail::named(p.fonts, f.name)) {
                    have->face = f.face;
                } else {
                    skip({}, f.name, "the pool declares no font `" + f.name + "`");
                }
            }
            for (const ThemeSize& s : r.sizes) {
                if (graphics::PaletteSize* have = detail::named(p.sizes, s.name)) {
                    have->points = s.points;
                } else {
                    skip({}, s.name, "the pool declares no size `" + s.name + "`");
                }
            }
            for (const ThemeStyle& s : r.styles) {
                graphics::PaletteStyle* have = detail::named(p.styles, s.name);
                if (have == nullptr) {
                    skip({}, s.name, "the pool declares no style `" + s.name + "`");
                } else if (const std::string why = detail::style_roots_refused(s, pool); !why.empty()) {
                    skip({}, s.name, why);
                } else {
                    *have = detail::style_over(*have, s);
                }
            }
            for (const ThemeOwn& o : r.own) {
                const auto d = declared.find(o.role);
                const bool known = d != declared.end();
                graphics::OwnValues* values = nullptr;
                for (graphics::OwnValues& v : p.own) {
                    if (v.role == o.role) values = &v;
                }
                if (values == nullptr) {
                    p.own.push_back(graphics::OwnValues{o.role, {}, {}, {}});
                    values = &p.own.back();
                }
                const auto takes = [&](const std::string& entry, const char* kind) {
                    if (known && detail::declared_value(d->second, entry, kind) == nullptr) {
                        skip(o.role, entry, "`" + o.role + "` declares no own " + kind + " `" + entry + "`");
                        return false;
                    }
                    return true;
                };
                for (const ThemeColor& c : o.colors) {
                    if (const auto v = rgba_of(c.color); v && takes(c.name, graphics::own_kind::kColor)) {
                        detail::color_over(values->colors, c.name, *v);
                    }
                }
                for (const ThemeSize& s : o.sizes) {
                    if (takes(s.name, graphics::own_kind::kSize)) {
                        if (graphics::PaletteSize* have = detail::named(values->sizes, s.name)) {
                            have->points = s.points;
                        } else {
                            values->sizes.push_back(graphics::PaletteSize{s.name, s.points});
                        }
                    }
                }
                for (const ThemeStyle& s : o.styles) {
                    if (!takes(s.name, graphics::own_kind::kStyle)) continue;
                    if (const std::string why = detail::style_roots_refused(s, pool); !why.empty()) {
                        skip(o.role, s.name, why);
                        continue;
                    }
                    // BELOW IT: what it was set to, else the style its role declares for its default,
                    // else `body`.
                    graphics::PaletteStyle below{s.name, graphics::kTextFont, {},
                                                 graphics::colors::text.name,
                                                 graphics::text_weight::kRegular};
                    if (const graphics::PaletteStyle* body = detail::named(p.styles, graphics::kBodyStyle)) {
                        below = *body;
                    }
                    if (graphics::PaletteStyle* have = detail::named(values->styles, s.name)) {
                        below = *have;
                    } else if (known) {
                        const graphics::OwnDeclared* dv =
                            detail::declared_value(d->second, s.name, graphics::own_kind::kStyle);
                        if (dv != nullptr) {
                            if (const graphics::PaletteStyle* root = detail::named(p.styles, dv->default_name)) {
                                below = *root;
                            }
                        }
                    }
                    graphics::PaletteStyle laid = detail::style_over(below, s);
                    if (graphics::PaletteStyle* have = detail::named(values->styles, s.name)) {
                        *have = laid;
                    } else {
                        values->styles.push_back(laid);
                    }
                }
            }
        } else {
            for (const ThemeInk& i : r.inks) {
                if (!detail::holds_name(pool.colors, i.name)) continue;
                if (const auto v = graphics::window::ink_rgba(i.ink)) {
                    detail::color_over(p.colors, i.name, *v);
                }
            }
            link.warning = kTerminalAloneWarning;
        }
        // ITS TERMINAL LOOK: the terminal file's, or its colours as their nearest inks.
        if (r.terminal) {
            if (!r.borders.empty()) p.terminal.borders = r.borders;
            for (const ThemeInk& i : r.inks) {
                if (detail::holds_name(pool.colors, i.name)) {
                    detail::ink_over(p.terminal.inks, i);
                } else {
                    skip({}, i.name, "the pool declares no colour `" + i.name + "`");
                }
            }
        } else {
            for (const ThemeColor& c : r.colors) {
                const auto v = rgba_of(c.color);
                if (!v || !detail::holds_name(pool.colors, c.name)) continue;
                detail::ink_over(p.terminal.inks,
                                 ThemeInk{c.name,
                                          graphics::window::ink_word_of_code(
                                              graphics::window::nearest_terminal_ink(*v)),
                                          {}});
            }
            link.warning = kWindowAloneWarning;
        }
        out.laid.links.push_back(std::move(link));
    }
    return out;
}

/// Whether laying said anything worth saying: a link converted or left out, an entry left out.
inline bool worth_saying(const StackLaid& laid) {
    return !laid.skipped.empty() ||
           std::any_of(laid.links.begin(), laid.links.end(), [](const ThemeLink& l) {
               return !l.warning.empty() || !l.refusal.empty();
           });
}

} // namespace zengine::theme

#endif // ZENGINE_THEME_STACK_HPP
