// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_THEME_THEME_PERSIST_HPP
#define ZENGINE_THEME_THEME_PERSIST_HPP

// A THEME'S FILES, paired by its name: `<name>.json` (format `zengine-theme`, version 3) holds a
// window of light's look -- colours, fonts, sizes, styles and each role's own values -- and
// `<name>.terminal.json` (format `zengine-terminal`, version 1) the terminal's: each colour's ink
// and fill, and the border characters. Each may include other themes' files of its medium. A theme
// is the look alone: a file of version 1, which held settings and keys, is refused, saying where
// they live. Reference: theme/README.md.

#include "graphics/terminal_inks.hpp" // the inks a terminal file names
#include "persist/file.hpp"           // the bounded read and the safe write
#include "persist/json.hpp"           // the files' plain JSON
#include "persist/written.hpp"        // `Written`

#include <zen/weave/shape.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::theme {

using persist::Written;

/// The two files' formats and the versions this build reads.
inline constexpr const char* kThemeFormat = "zengine-theme";
inline constexpr std::int64_t kThemeFormatVersion = 3;
inline constexpr const char* kTerminalFormat = "zengine-terminal";
inline constexpr std::int64_t kTerminalFormatVersion = 1;

/// The folder of themes under a host's configuration root, and the stem a terminal file adds.
inline constexpr const char* kThemesSubdir = "themes";
inline constexpr const char* kTerminalStem = ".terminal";

/// The bounds a file is judged against.
inline constexpr std::size_t kMaxThemeNameLen = 32;
inline constexpr std::size_t kMaxThemeSummaryLen = 96;
inline constexpr std::size_t kMaxThemeIncludes = 8;
inline constexpr std::size_t kMaxThemeColors = 128;
inline constexpr std::size_t kMaxThemeFonts = 16;
inline constexpr std::size_t kMaxThemeSizes = 32;
inline constexpr std::size_t kMaxThemeStyles = 32;
inline constexpr std::size_t kMaxThemeOwnRoles = 32;
inline constexpr std::size_t kMaxThemeOwnEntries = 64;
inline constexpr std::size_t kMaxThemeInks = 128;
inline constexpr std::size_t kMaxThemeEntryNameLen = 64;
inline constexpr std::size_t kMaxThemeFaceLen = 64;
inline constexpr std::int64_t kMinThemePoints = 6;
inline constexpr std::int64_t kMaxThemePoints = 96;
/// One resolution reads at most this many themes and nests this many includes below the theme
/// asked for.
inline constexpr std::size_t kMaxThemesResolved = 16;
inline constexpr std::size_t kMaxThemeDepth = 4;
/// A file's ceiling, in bytes, and how many files a listing reads.
inline constexpr std::uintmax_t kMaxThemeBytes = 1u << 16;
inline constexpr std::size_t kMaxThemesListed = 256;

// ---- The files' shapes ---------------------------------------------------------------------

/// One colour of a window file: a pool colour's name and its value, `#rrggbb` or `#rrggbbaa`.
struct ThemeColor {
    std::string name;
    std::string color;
    ZEN_SHAPE(ThemeColor, 1, ZEN_FIELD(name), ZEN_FIELD(color));
    friend bool operator==(const ThemeColor&, const ThemeColor&) = default;
};

/// One font of a window file: a pool font's name and the face it sets text in.
struct ThemeFont {
    std::string name;
    std::string face;
    ZEN_SHAPE(ThemeFont, 1, ZEN_FIELD(name), ZEN_FIELD(face));
    friend bool operator==(const ThemeFont&, const ThemeFont&) = default;
};

/// One size of a window file: a size's name and its points.
struct ThemeSize {
    std::string name;
    std::int64_t points = 0;
    ZEN_SHAPE(ThemeSize, 1, ZEN_FIELD(name), ZEN_FIELD(points));
    friend bool operator==(const ThemeSize&, const ThemeSize&) = default;
};

/// One style of a window file: its name, and the font, size and colour it names, each a pool
/// root's name, and its weight (`regular` or `bold`); a field left out stands as the style below.
struct ThemeStyle {
    std::string name;
    std::string font;
    std::string size;
    std::string color;
    std::string weight;
    ZEN_SHAPE(ThemeStyle, 1, ZEN_FIELD(name), ZEN_FIELD(font), ZEN_FIELD(size), ZEN_FIELD(color),
              ZEN_FIELD(weight));
    friend bool operator==(const ThemeStyle&, const ThemeStyle&) = default;
};

/// One role's own values a window file sets: its colours, sizes and styles by the names the role
/// declares them under.
struct ThemeOwn {
    std::string role;
    std::vector<ThemeColor> colors;
    std::vector<ThemeSize> sizes;
    std::vector<ThemeStyle> styles;
    ZEN_SHAPE(ThemeOwn, 1, ZEN_FIELD(role), ZEN_FIELD(colors), ZEN_FIELD(sizes),
              ZEN_FIELD(styles));
    friend bool operator==(const ThemeOwn&, const ThemeOwn&) = default;
};

/// A THEME'S WINDOW FILE, `<name>.json`: what it is, its name and summary, the themes whose window
/// files it is built from (applied first, in order), and its look.
struct ThemeFile {
    std::string format = kThemeFormat;
    std::int64_t format_version = kThemeFormatVersion;
    std::string name;
    std::string summary;
    std::vector<std::string> includes;
    std::vector<ThemeColor> colors;
    std::vector<ThemeFont> fonts;
    std::vector<ThemeSize> sizes;
    std::vector<ThemeStyle> styles;
    std::vector<ThemeOwn> own;
    ZEN_SHAPE(ThemeFile, 3, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(summary), ZEN_FIELD(includes), ZEN_FIELD(colors), ZEN_FIELD(fonts),
              ZEN_FIELD(sizes), ZEN_FIELD(styles), ZEN_FIELD(own));
    friend bool operator==(const ThemeFile&, const ThemeFile&) = default;
};

/// One colour of a terminal file: a pool colour's name, the ink a terminal shows it in, and the
/// character it fills a shape with; either left out stands as the look below.
struct ThemeInk {
    std::string name;
    std::string ink;
    std::string fill;
    ZEN_SHAPE(ThemeInk, 1, ZEN_FIELD(name), ZEN_FIELD(ink), ZEN_FIELD(fill));
    friend bool operator==(const ThemeInk&, const ThemeInk&) = default;
};

/// A THEME'S TERMINAL FILE, `<name>.terminal.json`: what it is, its name and summary, the themes
/// whose terminal files it is built from, the border characters and each colour's ink.
struct TerminalFile {
    std::string format = kTerminalFormat;
    std::int64_t format_version = kTerminalFormatVersion;
    std::string name;
    std::string summary;
    std::vector<std::string> includes;
    std::string borders;
    std::vector<ThemeInk> inks;
    ZEN_SHAPE(TerminalFile, 1, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(summary), ZEN_FIELD(includes), ZEN_FIELD(borders), ZEN_FIELD(inks));
    friend bool operator==(const TerminalFile&, const TerminalFile&) = default;
};

// ---- Form ----------------------------------------------------------------------------------

/// A THEME'S NAME: 1 to `kMaxThemeNameLen` bytes of `a`-`z`, `0`-`9` and `-`, the files' own name.
inline Written check_theme_name(std::string_view name) {
    if (name.empty() || name.size() > kMaxThemeNameLen) {
        return Written::no("a theme's name is 1 to " + std::to_string(kMaxThemeNameLen) +
                           " bytes -- `" + std::string(name) + "` is not");
    }
    for (const char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return Written::no("a theme's name is `a`-`z`, `0`-`9` and `-` -- `" +
                               std::string(name) + "` is not");
        }
    }
    return Written::ok();
}

/// A colour's value, `#rrggbb` or `#rrggbbaa`, as 0xRRGGBBAA; none where it is neither.
inline std::optional<std::int64_t> rgba_of(std::string_view color) {
    if ((color.size() != 7 && color.size() != 9) || color[0] != '#') {
        return std::nullopt;
    }
    std::int64_t v = 0;
    for (std::size_t i = 1; i < color.size(); ++i) {
        const char c = color[i];
        int d = -1;
        if (c >= '0' && c <= '9') d = c - '0';
        if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        if (d < 0) {
            return std::nullopt;
        }
        v = v * 16 + d;
    }
    return color.size() == 7 ? v * 256 + 0xff : v;
}

namespace detail {

inline Written entry_name(std::string_view what, std::string_view name) {
    if (name.empty() || name.size() > kMaxThemeEntryNameLen) {
        return Written::no(std::string(what) + " is named by 1 to " +
                           std::to_string(kMaxThemeEntryNameLen) + " bytes -- `" +
                           std::string(name) + "` is not");
    }
    for (const char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == '.')) {
            return Written::no(std::string(what) + " `" + std::string(name) +
                               "` is named by `a`-`z`, `0`-`9`, `_`, `-` and `.`");
        }
    }
    return Written::ok();
}

inline Written within(std::size_t have, std::size_t most, std::string_view what) {
    if (have > most) {
        return Written::no("a theme file holds at most " + std::to_string(most) + " " +
                           std::string(what) + " -- this one holds " + std::to_string(have));
    }
    return Written::ok();
}

template <class Items> Written once_each(const Items& items, std::string_view what) {
    std::set<std::string> seen;
    for (const auto& i : items) {
        if (!seen.insert(i.name).second) {
            return Written::no(std::string(what) + " `" + i.name + "` is given twice");
        }
    }
    return Written::ok();
}

inline Written color(const ThemeColor& c) {
    if (Written n = entry_name("a colour", c.name); !n.accepted) return n;
    if (!rgba_of(c.color)) {
        return Written::no("the colour `" + c.name + "` is `" + c.color +
                           "`, not `#rrggbb` or `#rrggbbaa`");
    }
    return Written::ok();
}

inline Written size(const ThemeSize& s) {
    if (Written n = entry_name("a size", s.name); !n.accepted) return n;
    if (s.points < kMinThemePoints || s.points > kMaxThemePoints) {
        return Written::no("the size `" + s.name + "` stands at " + std::to_string(s.points) +
                           " points, not a whole number from " + std::to_string(kMinThemePoints) +
                           " to " + std::to_string(kMaxThemePoints));
    }
    return Written::ok();
}

inline Written font(const ThemeFont& f) {
    if (Written n = entry_name("a font", f.name); !n.accepted) return n;
    if (f.face.empty() || f.face.size() > kMaxThemeFaceLen) {
        return Written::no("the font `" + f.name + "` names a face of 1 to " +
                           std::to_string(kMaxThemeFaceLen) + " bytes");
    }
    return Written::ok();
}

inline Written style(const ThemeStyle& s) {
    if (Written n = entry_name("a style", s.name); !n.accepted) return n;
    for (const std::string* field : {&s.font, &s.size, &s.color}) {
        if (!field->empty()) {
            if (Written n = entry_name("a style's root", *field); !n.accepted) return n;
        }
    }
    if (!s.weight.empty() && s.weight != "regular" && s.weight != "bold") {
        return Written::no("the style `" + s.name + "` weighs `" + s.weight +
                           "`, not `regular` or `bold`");
    }
    return Written::ok();
}

inline Written ink(const ThemeInk& i) {
    if (Written n = entry_name("an ink's colour", i.name); !n.accepted) return n;
    if (!i.ink.empty() && graphics::window::sgr_codes_of_word(i.ink).empty()) {
        return Written::no("`" + i.name + "` is inked `" + i.ink +
                           "`, which is not `terminal default` nor one of a terminal's sixteen inks, "
                           "perhaps `bright`, perhaps `bold`");
    }
    if (i.fill.size() > 1 || (i.fill.size() == 1 && (i.fill[0] < ' ' || i.fill[0] > '~'))) {
        return Written::no("`" + i.name + "` fills with `" + i.fill +
                           "`: one printable ASCII character, or none");
    }
    return Written::ok();
}

inline Written includes(const std::vector<std::string>& in) {
    if (Written w = within(in.size(), kMaxThemeIncludes, "includes"); !w.accepted) return w;
    for (const std::string& i : in) {
        if (Written n = check_theme_name(i); !n.accepted) return n;
    }
    return Written::ok();
}

template <class Items, class Check> Written each(const Items& items, Check check) {
    for (const auto& i : items) {
        if (Written w = check(i); !w.accepted) return w;
    }
    return Written::ok();
}

} // namespace detail

/// A WINDOW FILE'S FORM: its name, every entry and every bound; refused naming the first fault.
inline Written check_theme_file(const ThemeFile& t) {
    if (Written w = check_theme_name(t.name); !w.accepted) return w;
    if (t.summary.size() > kMaxThemeSummaryLen) {
        return Written::no("a theme's summary is at most " + std::to_string(kMaxThemeSummaryLen) +
                           " bytes");
    }
    std::size_t own_entries = 0;
    for (const ThemeOwn& o : t.own) {
        own_entries += o.colors.size() + o.sizes.size() + o.styles.size();
    }
    for (const Written& w :
         {detail::includes(t.includes), detail::within(t.colors.size(), kMaxThemeColors, "colours"),
          detail::within(t.fonts.size(), kMaxThemeFonts, "fonts"),
          detail::within(t.sizes.size(), kMaxThemeSizes, "sizes"),
          detail::within(t.styles.size(), kMaxThemeStyles, "styles"),
          detail::within(t.own.size(), kMaxThemeOwnRoles, "roles' own values"),
          detail::within(own_entries, kMaxThemeOwnEntries, "own values"),
          detail::once_each(t.colors, "the colour"), detail::once_each(t.fonts, "the font"),
          detail::once_each(t.sizes, "the size"), detail::once_each(t.styles, "the style"),
          detail::each(t.colors, detail::color), detail::each(t.fonts, detail::font),
          detail::each(t.sizes, detail::size), detail::each(t.styles, detail::style)}) {
        if (!w.accepted) return w;
    }
    std::set<std::string> roles;
    for (const ThemeOwn& o : t.own) {
        if (o.role.empty() || !roles.insert(o.role).second) {
            return Written::no("own values are given once a role, under its name -- `" + o.role +
                               "` is not");
        }
        for (const Written& w :
             {detail::once_each(o.colors, "the own colour"), detail::once_each(o.sizes, "the own size"),
              detail::once_each(o.styles, "the own style"), detail::each(o.colors, detail::color),
              detail::each(o.sizes, detail::size), detail::each(o.styles, detail::style)}) {
            if (!w.accepted) return w;
        }
    }
    return Written::ok();
}

/// A TERMINAL FILE'S FORM: its name, its borders, every ink and every bound.
inline Written check_terminal_file(const TerminalFile& t) {
    if (Written w = check_theme_name(t.name); !w.accepted) return w;
    if (t.summary.size() > kMaxThemeSummaryLen) {
        return Written::no("a theme's summary is at most " + std::to_string(kMaxThemeSummaryLen) +
                           " bytes");
    }
    if (!t.borders.empty() && t.borders.size() != 3) {
        return Written::no("the borders `" + t.borders +
                           "` are not three characters: a corner, across and down");
    }
    for (const Written& w :
         {detail::includes(t.includes), detail::within(t.inks.size(), kMaxThemeInks, "inks"),
          detail::once_each(t.inks, "the ink of"), detail::each(t.inks, detail::ink)}) {
        if (!w.accepted) return w;
    }
    return Written::ok();
}

// ---- Text ----------------------------------------------------------------------------------

namespace detail {

namespace js = persist::json;

inline js::Node object_of(std::vector<std::pair<std::string, js::Node>> members) {
    js::Node o;
    o.kind = js::Node::Kind::kObject;
    for (auto& m : members) {
        o.members.push_back(std::move(m));
    }
    return o;
}

template <class T, class F> js::Node list_of(const std::vector<T>& items, F node) {
    std::vector<js::Node> out;
    for (const T& i : items) {
        out.push_back(node(i));
    }
    return js::list_node(std::move(out));
}

inline void add_text(std::vector<std::pair<std::string, js::Node>>& m, const char* key,
                     const std::string& v) {
    if (!v.empty()) {
        m.emplace_back(key, js::text_node(v));
    }
}

inline js::Node color_node(const ThemeColor& c) {
    return object_of({{"name", js::text_node(c.name)}, {"color", js::text_node(c.color)}});
}
inline js::Node size_node(const ThemeSize& s) {
    return object_of({{"name", js::text_node(s.name)}, {"points", js::number_node(s.points)}});
}
inline js::Node font_node(const ThemeFont& f) {
    return object_of({{"name", js::text_node(f.name)}, {"face", js::text_node(f.face)}});
}
inline js::Node style_node(const ThemeStyle& s) {
    std::vector<std::pair<std::string, js::Node>> m{{"name", js::text_node(s.name)}};
    add_text(m, "font", s.font);
    add_text(m, "size", s.size);
    add_text(m, "color", s.color);
    add_text(m, "weight", s.weight);
    return object_of(std::move(m));
}
inline js::Node own_node(const ThemeOwn& o) {
    std::vector<std::pair<std::string, js::Node>> m{{"role", js::text_node(o.role)}};
    if (!o.colors.empty()) m.emplace_back("colors", list_of(o.colors, color_node));
    if (!o.sizes.empty()) m.emplace_back("sizes", list_of(o.sizes, size_node));
    if (!o.styles.empty()) m.emplace_back("styles", list_of(o.styles, style_node));
    return object_of(std::move(m));
}
inline js::Node ink_node(const ThemeInk& i) {
    std::vector<std::pair<std::string, js::Node>> m{{"name", js::text_node(i.name)}};
    add_text(m, "ink", i.ink);
    add_text(m, "fill", i.fill);
    return object_of(std::move(m));
}
inline js::Node word_node(const std::string& w) { return js::text_node(w); }

} // namespace detail

/// A WINDOW FILE AS PLAIN JSON, version 3: every member it holds a value for.
inline std::string to_text(const ThemeFile& t) {
    namespace js = persist::json;
    std::vector<std::pair<std::string, js::Node>> m{
        {"format", js::text_node(kThemeFormat)},
        {"format_version", js::number_node(kThemeFormatVersion)},
        {"name", js::text_node(t.name)}};
    detail::add_text(m, "summary", t.summary);
    if (!t.includes.empty()) m.emplace_back("includes", detail::list_of(t.includes, detail::word_node));
    if (!t.colors.empty()) m.emplace_back("colors", detail::list_of(t.colors, detail::color_node));
    if (!t.fonts.empty()) m.emplace_back("fonts", detail::list_of(t.fonts, detail::font_node));
    if (!t.sizes.empty()) m.emplace_back("sizes", detail::list_of(t.sizes, detail::size_node));
    if (!t.styles.empty()) m.emplace_back("styles", detail::list_of(t.styles, detail::style_node));
    if (!t.own.empty()) m.emplace_back("own", detail::list_of(t.own, detail::own_node));
    return js::to_text(detail::object_of(std::move(m)));
}

/// A TERMINAL FILE AS PLAIN JSON, version 1: every member it holds a value for.
inline std::string to_text(const TerminalFile& t) {
    namespace js = persist::json;
    std::vector<std::pair<std::string, js::Node>> m{
        {"format", js::text_node(kTerminalFormat)},
        {"format_version", js::number_node(kTerminalFormatVersion)},
        {"name", js::text_node(t.name)}};
    detail::add_text(m, "summary", t.summary);
    if (!t.includes.empty()) m.emplace_back("includes", detail::list_of(t.includes, detail::word_node));
    detail::add_text(m, "borders", t.borders);
    if (!t.inks.empty()) m.emplace_back("inks", detail::list_of(t.inks, detail::ink_node));
    return js::to_text(detail::object_of(std::move(m)));
}

/// WHY A THEME OF VERSION 1 IS NOT READ: it held settings and keys, which a theme holds no more.
inline constexpr const char* kVersionOneRefused =
    "a version-1 theme holds settings and keys, which a theme holds no more: a pane's settings are "
    "kept by the layouts' owner, and keys are the keymap's own layers -- nothing of it was read";
/// WHY A WINDOW FILE OF VERSION 2 IS NOT READ: what moved since.
inline constexpr const char* kVersionTwoRefused =
    "a version-2 theme gave each colour its terminal ink and fill, an outline, and named a pane's "
    "own values `slots`: a version-3 window file holds a window's look alone, under `own` for a "
    "pane's values, and the terminal's look is the theme's terminal file, `<name>.terminal.json`";

namespace detail {

/// PLAIN JSON READ AS A THEME'S FILE: each member one the file holds, of its kind; any other
/// member, or a kind a member is not, refused by name.
struct Reader {
    std::string refusal;

    bool fail(std::string why) {
        if (refusal.empty()) refusal = std::move(why);
        return false;
    }
    bool only(const js::Node& o, const std::string& where, std::initializer_list<const char*> known) {
        for (const auto& member : o.members) {
            bool ok = false;
            for (const char* k : known) ok = ok || member.first == k;
            if (!ok) {
                std::string list;
                for (const char* k : known) list += std::string(list.empty() ? "" : ", ") + k;
                return fail(where + " has `" + member.first + "`, which is none of: " + list);
            }
        }
        return true;
    }
    bool kind(const js::Node& n, js::Node::Kind k, const std::string& what) {
        return n.kind == k ||
               fail(what + " is " + js::kind_word(n.kind) + ", not " + js::kind_word(k));
    }
    bool text(const js::Node& o, const char* name, std::string& out, const std::string& where,
              bool required = false) {
        const js::Node* v = o.member(name);
        if (v == nullptr) return !required || fail(where + " needs `" + name + "`");
        if (!kind(*v, js::Node::Kind::kText, where + "'s `" + name + "`")) return false;
        out = v->text;
        return true;
    }
    bool number(const js::Node& o, const char* name, std::int64_t& out, const std::string& where,
                bool required = false) {
        const js::Node* v = o.member(name);
        if (v == nullptr) return !required || fail(where + " needs `" + name + "`");
        if (!kind(*v, js::Node::Kind::kNumber, where + "'s `" + name + "`")) return false;
        out = v->number;
        return true;
    }
    template <class F> bool list(const js::Node& o, const char* name, const std::string& where, F each) {
        const js::Node* v = o.member(name);
        if (v == nullptr) return true;
        if (!kind(*v, js::Node::Kind::kList, where + "'s `" + name + "`")) return false;
        for (std::size_t i = 0; i < v->items.size(); ++i) {
            if (!each(v->items[i], where + "'s " + name + " " + std::to_string(i))) return false;
        }
        return true;
    }
    template <class T, class Read> bool all(const js::Node& o, const char* name, const std::string& where,
                                           std::vector<T>& into, Read read) {
        return list(o, name, where, [&](const js::Node& n, const std::string& w) {
            T one;
            return (this->*read)(n, w, one) && (into.push_back(std::move(one)), true);
        });
    }
    bool words(const js::Node& o, const char* name, const std::string& where,
               std::vector<std::string>& into) {
        return list(o, name, where, [&](const js::Node& n, const std::string& w) {
            return kind(n, js::Node::Kind::kText, w) && (into.push_back(n.text), true);
        });
    }

    bool color(const js::Node& n, const std::string& w, ThemeColor& c) {
        return kind(n, js::Node::Kind::kObject, w) && only(n, w, {"name", "color", "about"}) &&
               text(n, "name", c.name, w, true) && text(n, "color", c.color, w, true);
    }
    bool size(const js::Node& n, const std::string& w, ThemeSize& s) {
        return kind(n, js::Node::Kind::kObject, w) && only(n, w, {"name", "points", "about"}) &&
               text(n, "name", s.name, w, true) && number(n, "points", s.points, w, true);
    }
    bool font(const js::Node& n, const std::string& w, ThemeFont& f) {
        return kind(n, js::Node::Kind::kObject, w) && only(n, w, {"name", "face", "about"}) &&
               text(n, "name", f.name, w, true) && text(n, "face", f.face, w, true);
    }
    bool style(const js::Node& n, const std::string& w, ThemeStyle& s) {
        return kind(n, js::Node::Kind::kObject, w) &&
               only(n, w, {"name", "font", "size", "color", "weight", "about"}) &&
               text(n, "name", s.name, w, true) && text(n, "font", s.font, w) &&
               text(n, "size", s.size, w) && text(n, "color", s.color, w) &&
               text(n, "weight", s.weight, w);
    }
    bool own(const js::Node& n, const std::string& w, ThemeOwn& o) {
        return kind(n, js::Node::Kind::kObject, w) &&
               only(n, w, {"role", "colors", "sizes", "styles"}) &&
               text(n, "role", o.role, w, true) && all(n, "colors", w, o.colors, &Reader::color) &&
               all(n, "sizes", w, o.sizes, &Reader::size) &&
               all(n, "styles", w, o.styles, &Reader::style);
    }
    bool ink(const js::Node& n, const std::string& w, ThemeInk& i) {
        return kind(n, js::Node::Kind::kObject, w) && only(n, w, {"name", "ink", "fill"}) &&
               text(n, "name", i.name, w, true) && text(n, "ink", i.ink, w) &&
               text(n, "fill", i.fill, w);
    }

    bool window(const js::Node& root, ThemeFile& t) {
        const std::string at = "the theme";
        return only(root, "a theme file",
                    {"format", "format_version", "name", "summary", "about", "includes",
                     "colors", "fonts", "sizes", "styles", "own"}) &&
               text(root, "name", t.name, at, true) && text(root, "summary", t.summary, at) &&
               words(root, "includes", at, t.includes) &&
               all(root, "colors", at, t.colors, &Reader::color) &&
               all(root, "fonts", at, t.fonts, &Reader::font) &&
               all(root, "sizes", at, t.sizes, &Reader::size) &&
               all(root, "styles", at, t.styles, &Reader::style) &&
               all(root, "own", at, t.own, &Reader::own);
    }
    bool terminal(const js::Node& root, TerminalFile& t) {
        const std::string at = "the terminal file";
        return only(root, "a terminal file",
                    {"format", "format_version", "name", "summary", "about", "includes",
                     "borders", "inks"}) &&
               text(root, "name", t.name, at, true) && text(root, "summary", t.summary, at) &&
               words(root, "includes", at, t.includes) && text(root, "borders", t.borders, at) &&
               all(root, "inks", at, t.inks, &Reader::ink);
    }
};

/// The file's plain JSON and what it says it is, or why it is not read.
struct Claimed {
    std::string refusal;
    std::optional<persist::json::Parsed> parsed;
    std::string format;
    std::int64_t version = 0;
};

inline Claimed claimed(std::string_view text) {
    Claimed c;
    if (text.size() > kMaxThemeBytes) {
        c.refusal = std::to_string(text.size()) + " bytes is larger than a theme file can be";
        return c;
    }
    persist::json::Parsed parsed = persist::json::parse(text);
    if (!parsed.root) {
        c.refusal = "not a theme file: " + parsed.refusal;
        return c;
    }
    const js::Node& root = *parsed.root;
    if (root.kind != js::Node::Kind::kObject) {
        c.refusal = "not a theme file: it is " + std::string(js::kind_word(root.kind));
        return c;
    }
    // A FILE IN LOOM'S ENVELOPE IS VERSION 1, whatever else it says.
    if (root.member("zen") != nullptr) {
        c.refusal = kVersionOneRefused;
        return c;
    }
    Reader r;
    if (!r.text(root, "format", c.format, "a theme file", true) ||
        !r.number(root, "format_version", c.version, "a theme file", true)) {
        c.refusal = r.refusal;
        return c;
    }
    c.parsed = std::move(parsed);
    return c;
}

} // namespace detail

/// WHAT READING ONE FILE PRODUCED: the file, or why it was not read, and the bytes it was read
/// from.
template <class File> struct Loaded {
    Written outcome;
    File file;
    std::string raw_bytes;
};
using LoadedWindow = Loaded<ThemeFile>;
using LoadedTerminal = Loaded<TerminalFile>;

/// Text to a window file, every refusal opening with `label`; nothing here throws. A version-1
/// file is refused saying where settings and keys live, and a version-2 file saying what moved.
inline LoadedWindow window_from_text(std::string_view text, std::string_view label = "theme") {
    const std::string at = std::string(label) + ": ";
    detail::Claimed c = detail::claimed(text);
    if (!c.refusal.empty()) {
        return LoadedWindow{Written::no(at + c.refusal), {}, {}};
    }
    if (c.format == kTerminalFormat) {
        return LoadedWindow{Written::no(at + "a terminal file, not a window file"), {}, {}};
    }
    if (c.format != kThemeFormat) {
        return LoadedWindow{Written::no(at + "not a theme file: it says it is `" + c.format + "`"), {}, {}};
    }
    if (c.version == 1) {
        return LoadedWindow{Written::no(at + kVersionOneRefused), {}, {}};
    }
    if (c.version == 2) {
        return LoadedWindow{Written::no(at + kVersionTwoRefused), {}, {}};
    }
    if (c.version != kThemeFormatVersion) {
        return LoadedWindow{Written::no(at + "theme file version " + std::to_string(c.version) +
                                        " -- this build reads version " +
                                        std::to_string(kThemeFormatVersion)),
                            {}, {}};
    }
    detail::Reader reader;
    ThemeFile t;
    if (!reader.window(*c.parsed->root, t)) {
        return LoadedWindow{Written::no(at + reader.refusal), {}, {}};
    }
    if (const Written form = check_theme_file(t); !form.accepted) {
        return LoadedWindow{Written::no(at + form.refusal), {}, {}};
    }
    return LoadedWindow{Written::ok(), std::move(t), std::string(text)};
}

/// Text to a terminal file, every refusal opening with `label`.
inline LoadedTerminal terminal_from_text(std::string_view text, std::string_view label = "theme") {
    const std::string at = std::string(label) + ": ";
    detail::Claimed c = detail::claimed(text);
    if (!c.refusal.empty()) {
        return LoadedTerminal{Written::no(at + c.refusal), {}, {}};
    }
    if (c.format != kTerminalFormat) {
        return LoadedTerminal{Written::no(at + "not a terminal file: it says it is `" + c.format + "`"),
                              {}, {}};
    }
    if (c.version != kTerminalFormatVersion) {
        return LoadedTerminal{Written::no(at + "terminal file version " + std::to_string(c.version) +
                                          " -- this build reads version " +
                                          std::to_string(kTerminalFormatVersion)),
                              {}, {}};
    }
    detail::Reader reader;
    TerminalFile t;
    if (!reader.terminal(*c.parsed->root, t)) {
        return LoadedTerminal{Written::no(at + reader.refusal), {}, {}};
    }
    if (const Written form = check_terminal_file(t); !form.accepted) {
        return LoadedTerminal{Written::no(at + form.refusal), {}, {}};
    }
    return LoadedTerminal{Written::ok(), std::move(t), std::string(text)};
}

// ---- The files -----------------------------------------------------------------------------

/// Where theme `name`'s window file lives under `dir`, and its terminal file.
inline std::string theme_path(const std::string& dir, std::string_view name) {
    return (std::filesystem::path(dir) / (std::string(name) + ".json")).lexically_normal().generic_string();
}
inline std::string terminal_path(const std::string& dir, std::string_view name) {
    return (std::filesystem::path(dir) / (std::string(name) + kTerminalStem + ".json"))
        .lexically_normal()
        .generic_string();
}

/// A THEME AS IT STANDS IN ONE FOLDER: each of its files there, read, or why one was not; a file
/// not there is no refusal, and a theme with neither is none.
struct ThemeFiles {
    bool window = false;
    LoadedWindow window_file;
    bool terminal = false;
    LoadedTerminal terminal_file;

    bool any() const { return window || terminal; }
    /// The first refusal of a file that is there, or "".
    std::string refusal() const {
        if (window && !window_file.outcome.accepted) return window_file.outcome.refusal;
        if (terminal && !terminal_file.outcome.accepted) return terminal_file.outcome.refusal;
        return {};
    }
};

namespace detail {
template <class Loaded, class Read>
Loaded read_one(const std::string& path, std::string_view name, Read from_text) {
    const persist::FileText read = persist::read_file(path, kMaxThemeBytes, "a theme file");
    if (!read.outcome.accepted) {
        return Loaded{read.outcome, {}, {}};
    }
    Loaded decoded = from_text(read.text, path);
    if (decoded.outcome.accepted && decoded.file.name != name) {
        return Loaded{Written::no(path + ": theme name `" + decoded.file.name +
                                  "` does not match its file's name `" + std::string(name) + "`"),
                      {}, {}};
    }
    return decoded;
}
} // namespace detail

/// READ THEME `name` FROM `dir`: a sound name, and each file of it there.
inline ThemeFiles load_theme(const std::string& dir, std::string_view name) {
    ThemeFiles out;
    if (const Written form = check_theme_name(name); !form.accepted) {
        out.window = true;
        out.window_file = LoadedWindow{form, {}, {}};
        return out;
    }
    std::error_code ec;
    const std::string window = theme_path(dir, name);
    const std::string terminal = terminal_path(dir, name);
    if (std::filesystem::exists(window, ec)) {
        out.window = true;
        out.window_file = detail::read_one<LoadedWindow>(
            window, name, [](std::string_view t, std::string_view l) { return window_from_text(t, l); });
    }
    if (std::filesystem::exists(terminal, ec)) {
        out.terminal = true;
        out.terminal_file = detail::read_one<LoadedTerminal>(
            terminal, name,
            [](std::string_view t, std::string_view l) { return terminal_from_text(t, l); });
    }
    return out;
}

/// WRITE A THEME'S FILES under `dir` by the safe write, the folder made on the first write: the
/// window file where `window` is given, the terminal file where `terminal` is. Each is judged
/// first, and nothing is written where either is refused; a theme already there is overwritten only
/// with `replace`.
inline Written save_theme(const std::string& dir, const ThemeFile* window,
                          const TerminalFile* terminal, bool replace,
                          std::vector<std::string>* paths = nullptr) {
    if (window == nullptr && terminal == nullptr) {
        return Written::no("a theme saved names a window file, a terminal file or both");
    }
    if (window != nullptr && terminal != nullptr && window->name != terminal->name) {
        return Written::no("a theme's two files carry one name -- `" + window->name + "` and `" +
                           terminal->name + "` are two");
    }
    if (window != nullptr) {
        if (const Written form = check_theme_file(*window); !form.accepted) return form;
    }
    if (terminal != nullptr) {
        if (const Written form = check_terminal_file(*terminal); !form.accepted) return form;
    }
    const std::string name = window != nullptr ? window->name : terminal->name;
    std::vector<std::pair<std::string, std::string>> writes;
    if (window != nullptr) writes.emplace_back(theme_path(dir, name), to_text(*window));
    if (terminal != nullptr) writes.emplace_back(terminal_path(dir, name), to_text(*terminal));
    for (const auto& [path, text] : writes) {
        std::error_code ec;
        const bool there = std::filesystem::exists(path, ec);
        if (ec) {
            return Written::no("cannot tell whether " + path + " exists: " + ec.message() +
                               " -- nothing was written");
        }
        if (there && !replace) {
            return Written::no("theme `" + name + "` already has " + path +
                               " -- pass replace to overwrite it; nothing was written");
        }
    }
    for (const auto& [path, text] : writes) {
        if (const Written w = persist::write_file_making_room(path, text); !w.accepted) {
            return w;
        }
        if (paths != nullptr) paths->push_back(path);
    }
    return Written::ok();
}

// ---- Resolution ----------------------------------------------------------------------------

/// A THEME RESOLVED WITH ITS INCLUDES: which of its files it has, its window look and its terminal
/// look, each the later include's and then its own winning for one name -- or the refusal, which
/// leaves nothing half done.
struct ThemeResolved {
    bool accepted = false;
    std::string refusal;
    std::string theme;
    std::string summary;
    bool window = false;
    bool terminal = false;
    std::vector<ThemeColor> colors;
    std::vector<ThemeFont> fonts;
    std::vector<ThemeSize> sizes;
    std::vector<ThemeStyle> styles;
    std::vector<ThemeOwn> own;
    std::string borders;
    std::vector<ThemeInk> inks;
    ZEN_SHAPE(ThemeResolved, 2, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(theme),
              ZEN_FIELD(summary), ZEN_FIELD(window), ZEN_FIELD(terminal), ZEN_FIELD(colors),
              ZEN_FIELD(fonts), ZEN_FIELD(sizes), ZEN_FIELD(styles), ZEN_FIELD(own),
              ZEN_FIELD(borders), ZEN_FIELD(inks));
    friend bool operator==(const ThemeResolved&, const ThemeResolved&) = default;
};

namespace detail {

template <class T> void by_name_into(std::vector<T>& into, const T& entry) {
    for (T& have : into) {
        if (have.name == entry.name) {
            have = entry;
            return;
        }
    }
    into.push_back(entry);
}

/// A style over the one below it of its name: each field it gives.
inline void style_into(std::vector<ThemeStyle>& into, const ThemeStyle& s) {
    for (ThemeStyle& have : into) {
        if (have.name == s.name) {
            if (!s.font.empty()) have.font = s.font;
            if (!s.size.empty()) have.size = s.size;
            if (!s.color.empty()) have.color = s.color;
            if (!s.weight.empty()) have.weight = s.weight;
            return;
        }
    }
    into.push_back(s);
}

/// An ink over the one below it of its name: each field it gives.
inline void ink_into(std::vector<ThemeInk>& into, const ThemeInk& i) {
    for (ThemeInk& have : into) {
        if (have.name == i.name) {
            if (!i.ink.empty()) have.ink = i.ink;
            if (!i.fill.empty()) have.fill = i.fill;
            return;
        }
    }
    into.push_back(i);
}

inline void own_into(std::vector<ThemeOwn>& into, const ThemeOwn& o) {
    for (ThemeOwn& have : into) {
        if (have.role == o.role) {
            for (const ThemeColor& c : o.colors) by_name_into(have.colors, c);
            for (const ThemeSize& s : o.sizes) by_name_into(have.sizes, s);
            for (const ThemeStyle& s : o.styles) style_into(have.styles, s);
            return;
        }
    }
    into.push_back(o);
}

/// ONE MEDIUM'S FILES WALKED with their includes, depth first, each include whole before the file
/// that names it; a cycle, a missing or broken include, or a bound passed refuses the walk.
template <class File, class Load, class Apply> struct Walk {
    Load load;
    Apply apply;
    std::size_t read = 0;
    std::vector<std::string> path;
    std::string refusal;

    bool walk(const std::string& name, std::size_t depth) {
        if (std::find(path.begin(), path.end(), name) != path.end()) {
            std::string cycle;
            for (const std::string& p : path) cycle += p + " -> ";
            refusal = "theme `" + path.front() + "` includes itself: " + cycle + name;
            return false;
        }
        if (depth > kMaxThemeDepth) {
            refusal = "theme `" + path.front() + "` nests its includes more than " +
                      std::to_string(kMaxThemeDepth) + " deep";
            return false;
        }
        if (++read > kMaxThemesResolved) {
            refusal = "theme `" + path.front() + "` reads more than " +
                      std::to_string(kMaxThemesResolved) + " themes";
            return false;
        }
        std::optional<File> file = load(name, refusal);
        if (!file) {
            return false;
        }
        path.push_back(name);
        for (const std::string& include : file->includes) {
            if (!walk(include, depth + 1)) return false;
        }
        path.pop_back();
        apply(*file);
        return true;
    }
};

} // namespace detail

/// HOW A THEME'S FILES ARE FOUND: a name to the theme's files as they stand (`load_theme`'s form).
using ThemeLoader = std::function<ThemeFiles(std::string_view)>;

/// RESOLVE THEME `name` THROUGH `load`: each of its files with its includes of that medium, in
/// order, then its own entries. A theme with no file at all is refused; an include naming a theme
/// with no file of that medium is refused.
inline ThemeResolved resolve_theme_with(const std::string& name, const ThemeLoader& load) {
    ThemeResolved out;
    out.theme = name;
    const ThemeFiles root = load(name);
    if (!root.any()) {
        out.refusal = "no theme `" + name + "`";
        return out;
    }
    if (const std::string why = root.refusal(); !why.empty()) {
        out.refusal = why;
        return out;
    }
    out.window = root.window;
    out.terminal = root.terminal;
    out.summary = root.window ? root.window_file.file.summary : root.terminal_file.file.summary;
    const auto missing = [&](const std::string& n, const char* medium, std::string& why) {
        why = "theme `" + name + "` includes `" + n + "`, which has no " + medium + " file";
    };
    if (root.window) {
        auto load_window = [&](const std::string& n, std::string& why) -> std::optional<ThemeFile> {
            const ThemeFiles f = n == name ? root : load(n);
            if (!f.window) {
                missing(n, "window", why);
                return std::nullopt;
            }
            if (!f.window_file.outcome.accepted) {
                why = f.window_file.outcome.refusal;
                return std::nullopt;
            }
            return f.window_file.file;
        };
        auto apply = [&](const ThemeFile& f) {
            for (const ThemeColor& c : f.colors) detail::by_name_into(out.colors, c);
            for (const ThemeFont& x : f.fonts) detail::by_name_into(out.fonts, x);
            for (const ThemeSize& x : f.sizes) detail::by_name_into(out.sizes, x);
            for (const ThemeStyle& x : f.styles) detail::style_into(out.styles, x);
            for (const ThemeOwn& x : f.own) detail::own_into(out.own, x);
        };
        detail::Walk<ThemeFile, decltype(load_window), decltype(apply)> w{load_window, apply, 0, {}, {}};
        if (!w.walk(name, 0)) {
            out = ThemeResolved{};
            out.theme = name;
            out.refusal = w.refusal;
            return out;
        }
    }
    if (root.terminal) {
        auto load_terminal = [&](const std::string& n, std::string& why) -> std::optional<TerminalFile> {
            const ThemeFiles f = n == name ? root : load(n);
            if (!f.terminal) {
                missing(n, "terminal", why);
                return std::nullopt;
            }
            if (!f.terminal_file.outcome.accepted) {
                why = f.terminal_file.outcome.refusal;
                return std::nullopt;
            }
            return f.terminal_file.file;
        };
        auto apply = [&](const TerminalFile& f) {
            if (!f.borders.empty()) out.borders = f.borders;
            for (const ThemeInk& i : f.inks) detail::ink_into(out.inks, i);
        };
        detail::Walk<TerminalFile, decltype(load_terminal), decltype(apply)> w{load_terminal, apply, 0, {}, {}};
        if (!w.walk(name, 0)) {
            out = ThemeResolved{};
            out.theme = name;
            out.refusal = w.refusal;
            return out;
        }
    }
    out.accepted = true;
    return out;
}

/// Theme `name` resolved from one folder.
inline ThemeResolved resolve_theme(const std::string& dir, const std::string& name) {
    return resolve_theme_with(name, [&](std::string_view n) { return load_theme(dir, n); });
}

// ---- The folder ----------------------------------------------------------------------------

/// ONE THEME IN A FOLDER: its name and summary, which of its files it has and the themes they
/// include -- or why a file of it would not load.
struct ThemeSummary {
    std::string name;
    std::string summary;
    bool window = false;
    bool terminal = false;
    std::vector<std::string> includes;
    std::string refusal;
    ZEN_SHAPE(ThemeSummary, 2, ZEN_FIELD(name), ZEN_FIELD(summary), ZEN_FIELD(window),
              ZEN_FIELD(terminal), ZEN_FIELD(includes), ZEN_FIELD(refusal));
    friend bool operator==(const ThemeSummary&, const ThemeSummary&) = default;
};

/// EVERY THEME IN `dir`, by name, the first `kMaxThemesListed` files found: each summarised from
/// its files, one that does not load listed by its name with its refusal. A folder that is not
/// there holds none; a write's candidate is no theme.
inline std::vector<ThemeSummary> list_themes(const std::string& dir) {
    std::map<std::string, ThemeSummary> found;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return {};
    }
    std::size_t files = 0;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code kind;
        if (!it->is_regular_file(kind) || it->path().extension() != ".json") {
            continue;
        }
        if (++files > kMaxThemesListed) {
            break;
        }
        std::string stem = it->path().stem().string();
        const std::string suffix = kTerminalStem;
        if (stem.size() > suffix.size() && stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0) {
            stem.resize(stem.size() - suffix.size());
        }
        if (found.count(stem) != 0) {
            continue;
        }
        const ThemeFiles f = load_theme(dir, stem);
        ThemeSummary s;
        s.name = stem;
        s.window = f.window;
        s.terminal = f.terminal;
        s.refusal = f.refusal();
        if (s.refusal.empty()) {
            s.summary = f.window ? f.window_file.file.summary : f.terminal_file.file.summary;
            s.includes = f.window ? f.window_file.file.includes : f.terminal_file.file.includes;
        }
        found[stem] = std::move(s);
    }
    std::vector<ThemeSummary> out;
    for (auto& [name, s] : found) {
        out.push_back(std::move(s));
    }
    return out;
}

} // namespace zengine::theme

#endif // ZENGINE_THEME_THEME_PERSIST_HPP
