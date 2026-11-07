// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SETTINGS_THEME_PERSIST_HPP
#define ZENGINE_SETTINGS_THEME_PERSIST_HPP

// THE THEME FILE -- one theme a file, `<name>.json` in a folder of themes, plain JSON a weaver
// writes by hand: the colours, fonts and sizes it gives the pool, each office's slots, the settings
// and keys it writes, the themes it includes first, and what it does with an entry nothing takes.
// A theme resolves with its includes to one set of entries, each setting and key naming the theme
// that gave it, or is refused whole.

#include "input/gesture_grammar.hpp"    // `parse_gesture`
#include "persist/file.hpp"             // the bounded read and the safe write
#include "persist/json.hpp"             // the file's plain JSON
#include "persist/written.hpp"          // `Written`
#include "workshop/pane_settings.hpp"   // the pane protocol's setting: its key, kinds and text
#include "workshop/pane_vocabulary.hpp" // an action id's bound

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace zengine::settings {

using persist::Written;

/// What a theme file says it is, and the one version this build writes; version 1, Loom's
/// envelope around settings and keys alone, is read and widened.
inline constexpr const char* kThemeFormat = "zengine-theme";
inline constexpr std::int64_t kThemeFormatVersion = 2;
inline constexpr std::int64_t kThemeFormatVersionOne = 1;

/// The folder under the per-user configuration root that holds the themes, one file each.
inline constexpr const char* kThemesSubdir = "themes";

/// What a theme does with an entry nothing takes: refuse the whole theme, or skip the entry and
/// say so.
inline constexpr const char* kOnFailureRefuse = "refuse";
inline constexpr const char* kOnFailureSkip = "skip";

/// What one file may hold: its name (the file's name too), summary and targets, and how many
/// settings, keys, includes, colours, fonts, sizes and offices' slots.
inline constexpr std::size_t kMaxThemeNameLen = 32;
inline constexpr std::size_t kMaxThemeSummaryLen = 64;
inline constexpr std::size_t kMaxThemeTargetLen = 128;
inline constexpr std::size_t kMaxThemeSettings = 512;
inline constexpr std::size_t kMaxThemeKeys = 128;
inline constexpr std::size_t kMaxThemeIncludes = 8;
inline constexpr std::size_t kMaxThemeColors = 128;
inline constexpr std::size_t kMaxThemeFonts = 16;
inline constexpr std::size_t kMaxThemeSizes = 32;
inline constexpr std::size_t kMaxThemeSlotOffices = 32;
inline constexpr std::size_t kMaxThemeSlotEntries = 64;

/// What one entry may hold: a name a pool or a pane declares, a face's name, a key's gestures,
/// and the points a size stands at.
inline constexpr std::size_t kMaxThemeEntryNameLen = 64;
inline constexpr std::size_t kMaxThemeFaceLen = 64;
inline constexpr std::size_t kMaxThemeGestures = 8;
inline constexpr std::int64_t kMinThemePoints = 6;
inline constexpr std::int64_t kMaxThemePoints = 96;

/// What one resolution may spend: the themes it reads, how deep an include nests below the theme
/// asked for, and the entries it resolves to.
inline constexpr std::size_t kMaxThemesResolved = 16;
inline constexpr std::size_t kMaxThemeDepth = 4;
inline constexpr std::size_t kMaxResolvedEntries = 1024;

/// The ceiling is the read side of the decoder's own materialisation law -- a hostile file does
/// not get to choose the cost of refusing it.
inline constexpr std::uintmax_t kMaxThemeBytes = 1u << 16;

/// How many files of a folder a listing reads: past it, the rest are not read.
inline constexpr std::size_t kMaxThemesListed = 256;

// ---- The file's shapes -------------------------------------------------------------------

/// ONE SETTING A THEME WRITES: where (`*`, `<office>` or `<office>/<pane>`), which key, and its
/// value in the one field of its kind -- or `clear`, which clears what an earlier theme set.
struct ThemeSetting {
    std::string target;
    std::string key;
    std::optional<bool> flag = std::nullopt;
    std::optional<std::int64_t> number = std::nullopt;
    std::optional<std::string> text = std::nullopt;
    bool clear = false;

    ZEN_SHAPE(ThemeSetting, 1, ZEN_FIELD(target), ZEN_FIELD(key), ZEN_FIELD(flag),
              ZEN_FIELD(number), ZEN_FIELD(text), ZEN_FIELD(clear));
    friend bool operator==(const ThemeSetting&, const ThemeSetting&) = default;
};

/// ONE COLOUR A THEME GIVES, by the name a pool or a pane declares it under: its value
/// (`#rrggbb` or `#rrggbbaa`, "" leaving the colour where it stands), what a terminal shows for it
/// (`terminal default`, or one of its sixteen inks, perhaps `bright`, perhaps `bold`; "" leaving
/// it), and the character a terminal fills a shape with in it ("" leaving it).
struct ThemeColor {
    std::string name;
    std::string color;
    std::string cells;
    std::string fill;

    ZEN_SHAPE(ThemeColor, 1, ZEN_FIELD(name), ZEN_FIELD(color), ZEN_FIELD(cells),
              ZEN_FIELD(fill));
    friend bool operator==(const ThemeColor&, const ThemeColor&) = default;
};

/// ONE FONT A THEME GIVES: the pool's font `name` set in the face named `face`.
struct ThemeFont {
    std::string name;
    std::string face;

    ZEN_SHAPE(ThemeFont, 1, ZEN_FIELD(name), ZEN_FIELD(face));
    friend bool operator==(const ThemeFont&, const ThemeFont&) = default;
};

/// ONE TEXT SIZE A THEME GIVES: the size `name` at `points`.
struct ThemeSize {
    std::string name;
    std::int64_t points = 0;

    ZEN_SHAPE(ThemeSize, 1, ZEN_FIELD(name), ZEN_FIELD(points));
    friend bool operator==(const ThemeSize&, const ThemeSize&) = default;
};

/// ONE OFFICE'S SLOTS A THEME SETS: the colours and sizes that office exposes, by its names.
struct ThemeSlots {
    std::string office;
    std::vector<ThemeColor> colors;
    std::vector<ThemeSize> sizes;

    ZEN_SHAPE(ThemeSlots, 1, ZEN_FIELD(office), ZEN_FIELD(colors), ZEN_FIELD(sizes));
    friend bool operator==(const ThemeSlots&, const ThemeSlots&) = default;
};

/// ONE ACTION'S KEYS A THEME SETS: its id and every gesture that asks for it, in the keymap's own
/// spelling -- `none` alone for no key at all.
struct ThemeKey {
    std::string action;
    std::vector<std::string> gestures;

    ZEN_SHAPE(ThemeKey, 2, ZEN_FIELD(action), ZEN_FIELD(gestures));
    friend bool operator==(const ThemeKey&, const ThemeKey&) = default;
};

/// A WHOLE THEME FILE, version 2: its includes, applied first and in order; what it does with an
/// entry nothing takes; a terminal's outline characters ("" leaving them); its colours, fonts and
/// sizes for the pool; each office's slots; its own settings (kept in target and key order) and
/// keys (in authored order).
struct ThemeFile {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::string summary;
    std::vector<std::string> includes;
    std::string on_failure = kOnFailureRefuse;
    std::string outline;
    std::vector<ThemeColor> colors;
    std::vector<ThemeFont> fonts;
    std::vector<ThemeSize> sizes;
    std::vector<ThemeSlots> slots;
    std::vector<ThemeSetting> settings;
    std::vector<ThemeKey> keys;

    ZEN_SHAPE(ThemeFile, 2, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(summary), ZEN_FIELD(includes), ZEN_FIELD(on_failure), ZEN_FIELD(outline),
              ZEN_FIELD(colors), ZEN_FIELD(fonts), ZEN_FIELD(sizes), ZEN_FIELD(slots),
              ZEN_FIELD(settings), ZEN_FIELD(keys));
    friend bool operator==(const ThemeFile&, const ThemeFile&) = default;
};

static_assert(ThemeFile::zen_version == static_cast<std::uint32_t>(kThemeFormatVersion),
              "the theme file's format version and its shape's version are one number");

/// VERSION 1, KEPT UNDER ITS OWN WIRE IDENTITY so a file written before version 2 is read: Loom's
/// envelope around settings and keys, one gesture a key. It reads as version 2 with `on_failure`
/// `refuse` and each key one gesture.
namespace v1 {

struct ThemeKey {
    std::string action;
    std::string gesture;

    ZEN_SHAPE(ThemeKey, 1, ZEN_FIELD(action), ZEN_FIELD(gesture));
    friend bool operator==(const ThemeKey&, const ThemeKey&) = default;
};

struct ThemeFile {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::string summary;
    std::vector<std::string> includes;
    std::vector<ThemeSetting> settings;
    std::vector<ThemeKey> keys;

    ZEN_SHAPE(ThemeFile, 1, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(summary), ZEN_FIELD(includes), ZEN_FIELD(settings), ZEN_FIELD(keys));
    friend bool operator==(const ThemeFile&, const ThemeFile&) = default;
};

static_assert(ThemeFile::zen_version == static_cast<std::uint32_t>(kThemeFormatVersionOne),
              "version 1's envelope and its format version are one number");

} // namespace v1

/// A version-1 theme as version 2: every entry it held, `on_failure` `refuse` (the whole batch,
/// as version 1 applied), and each key its one gesture.
inline ThemeFile widened(const v1::ThemeFile& old) {
    ThemeFile t;
    t.format = kThemeFormat;
    t.format_version = kThemeFormatVersion;
    t.name = old.name;
    t.summary = old.summary;
    t.includes = old.includes;
    t.on_failure = kOnFailureRefuse;
    t.settings = old.settings;
    for (const v1::ThemeKey& k : old.keys) {
        t.keys.push_back(ThemeKey{k.action, {k.gesture}});
    }
    return t;
}

// ---- Form ----------------------------------------------------------------------------------

/// A THEME'S NAME: 1 to `kMaxThemeNameLen` bytes of `a`-`z`, `0`-`9` and `-`, so the name is a
/// file name on every platform and never a path.
inline Written check_theme_name(std::string_view name) {
    if (name.empty()) {
        return Written::no("a theme's name cannot be empty");
    }
    if (name.size() > kMaxThemeNameLen) {
        return Written::no("a theme's name is at most " + std::to_string(kMaxThemeNameLen) +
                           " bytes -- this one has " + std::to_string(name.size()));
    }
    for (const char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return Written::no("a theme's name is `a`-`z`, `0`-`9` and `-` -- `" +
                               std::string(name) + "` is not");
        }
    }
    return Written::ok();
}

/// WHERE A THEME SETTING GOES: `*` (every pane that takes its key), `<office>` (an office-wide
/// setting) or `<office>/<pane>` (that pane), an office named with a `.`.
inline Written check_theme_target(std::string_view target) {
    if (target.empty() || target.size() > kMaxThemeTargetLen) {
        return Written::no("a theme setting's target is 1 to " +
                           std::to_string(kMaxThemeTargetLen) + " bytes");
    }
    for (const char c : target) {
        if (static_cast<unsigned char>(c) <= 0x20u || static_cast<unsigned char>(c) > 0x7Eu) {
            return Written::no("a theme setting's target `" + std::string(target) +
                               "` is printable ASCII with no space");
        }
    }
    if (target == "*") {
        return Written::ok();
    }
    const std::size_t slash = target.find('/');
    const std::string_view office = target.substr(0, slash);
    const bool office_ok = !office.empty() && office.find('.') != std::string_view::npos;
    bool pane_ok = true;
    if (slash != std::string_view::npos) {
        const std::string_view pane = target.substr(slash + 1);
        pane_ok = !pane.empty() && pane != "*" && pane.find('/') == std::string_view::npos;
    }
    if (!office_ok || !pane_ok) {
        return Written::no("a theme setting's target is `*`, `<office>` or `<office>/<pane>` -- `" +
                           std::string(target) + "` is not");
    }
    return Written::ok();
}

/// ONE SETTING'S FORM: its target, its key, and exactly one value -- or, as a clear, none.
inline Written check_theme_setting(const ThemeSetting& s) {
    if (const Written target = check_theme_target(s.target); !target.accepted) {
        return target;
    }
    if (std::string wrong = workshop::pane_setting_key_problem(s.key); !wrong.empty()) {
        return Written::no("`" + s.target + "`: " + wrong);
    }
    const workshop::SettingKind kind = workshop::setting_kind(s);
    if (s.clear) {
        if (kind != workshop::SettingKind::kNone) {
            return Written::no("`" + s.key + "` of `" + s.target + "` is a clear and holds " +
                               workshop::setting_kind_word(kind) + " -- a clear holds no value");
        }
        return Written::ok();
    }
    if (kind == workshop::SettingKind::kNone || kind == workshop::SettingKind::kSeveral) {
        return Written::no("`" + s.key + "` of `" + s.target + "` holds " +
                           workshop::setting_kind_word(kind) +
                           " -- a theme setting holds exactly one of a flag, a number or a text, "
                           "or is a clear");
    }
    if (kind == workshop::SettingKind::kText && !workshop::pane_setting_text_ok(*s.text)) {
        return Written::no("`" + s.key + "` of `" + s.target + "` holds a text past " +
                           std::to_string(workshop::kMaxPaneSettingTextLen) +
                           " bytes of printable ASCII");
    }
    return Written::ok();
}

/// ONE KEY'S FORM: an action id of `a`-`z`, `0`-`9`, `.`, `/` and `-` within an action id's bound,
/// and 1 to `kMaxThemeGestures` gestures the keymap reads, none twice, `none` only alone.
inline Written check_theme_key(const ThemeKey& k) {
    if (k.action.empty() || k.action.size() > workshop::kMaxPaneActionIdLen) {
        return Written::no("a theme key's action is 1 to " +
                           std::to_string(workshop::kMaxPaneActionIdLen) + " bytes");
    }
    for (const char c : k.action) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' ||
              c == '/')) {
            return Written::no("a theme key's action is `a`-`z`, `0`-`9`, `.`, `/` and `-` -- `" +
                               k.action + "` is not");
        }
    }
    if (k.gestures.empty() || k.gestures.size() > kMaxThemeGestures) {
        return Written::no("`" + k.action + "`: a theme key names 1 to " +
                           std::to_string(kMaxThemeGestures) + " gestures -- it names " +
                           std::to_string(k.gestures.size()));
    }
    std::vector<input::Gesture> seen;
    for (const std::string& g : k.gestures) {
        const input::ParsedGesture parsed = input::parse_gesture(g);
        if (!parsed.accepted) {
            return Written::no("`" + k.action + "`: " + parsed.refusal);
        }
        if (!input::is_bound(parsed.gesture) && k.gestures.size() > 1) {
            return Written::no("`" + k.action + "`: `" + g +
                               "` is no key at all, so it stands alone");
        }
        if (std::find(seen.begin(), seen.end(), parsed.gesture) != seen.end()) {
            return Written::no("`" + k.action + "` names `" + g + "` twice");
        }
        seen.push_back(parsed.gesture);
    }
    return Written::ok();
}

/// A NAME A POOL, A FONT OR A PANE'S SLOTS DECLARE: 1 to `kMaxThemeEntryNameLen` bytes, a letter
/// or `_` first, then letters, digits and `_` -- as the generated C++ names are.
inline Written check_entry_name(std::string_view what, std::string_view name) {
    bool ok = !name.empty() && name.size() <= kMaxThemeEntryNameLen &&
              !(name.front() >= '0' && name.front() <= '9');
    for (const char c : name) {
        ok = ok && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '_');
    }
    if (!ok) {
        return Written::no(std::string(what) + " `" + std::string(name) + "` is named by 1 to " +
                           std::to_string(kMaxThemeEntryNameLen) +
                           " letters, digits and `_`, not a digit first");
    }
    return Written::ok();
}

/// `#rrggbb` or `#rrggbbaa` as 0xRRGGBBAA, or nothing.
inline std::optional<std::int64_t> rgba_of(std::string_view color) {
    if ((color.size() != 7 && color.size() != 9) || color.front() != '#') {
        return std::nullopt;
    }
    std::int64_t v = 0;
    for (std::size_t i = 1; i < color.size(); ++i) {
        const char c = color[i];
        std::int64_t d = 0;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        } else {
            return std::nullopt;
        }
        v = v * 16 + d;
    }
    return color.size() == 7 ? v * 256 + 0xff : v;
}

/// WHAT A TERMINAL SHOWS FOR A COLOUR, as a theme may say it: `terminal default`, or one of the
/// sixteen inks (`bright` or not), perhaps `bold`.
inline bool is_cells_word(std::string_view cells) {
    if (cells.size() >= 5 && cells.substr(cells.size() - 5) == " bold") {
        cells.remove_suffix(5);
    }
    if (cells == "terminal default") {
        return true;
    }
    if (cells.substr(0, 7) == "bright ") {
        cells.remove_prefix(7);
    }
    for (const char* ink : {"black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"}) {
        if (cells == ink) {
            return true;
        }
    }
    return false;
}

/// ONE COLOUR'S FORM: its name, its value if it gives one, its terminal word and fill character.
/// A value that is no colour is a value of the wrong type, refused whatever the theme says.
inline Written check_theme_color(const ThemeColor& c) {
    if (const Written name = check_entry_name("a colour", c.name); !name.accepted) {
        return name;
    }
    if (!c.color.empty() && !rgba_of(c.color)) {
        return Written::no("the colour `" + c.name + "` is `" + c.color +
                           "`, which is no colour: `#rrggbb` or `#rrggbbaa`");
    }
    if (!c.cells.empty() && !is_cells_word(c.cells)) {
        return Written::no("the colour `" + c.name + "` shows `" + c.cells +
                           "` in a terminal, which is none of what one shows: `terminal default`, "
                           "or one of its sixteen inks (bright or not), perhaps bold");
    }
    if (c.fill.size() > 1 ||
        (c.fill.size() == 1 && (static_cast<unsigned char>(c.fill[0]) < 0x20u ||
                                static_cast<unsigned char>(c.fill[0]) > 0x7Eu))) {
        return Written::no("the colour `" + c.name + "` fills with `" + c.fill +
                           "`: one printable ASCII character, or none");
    }
    if (c.color.empty() && c.cells.empty() && c.fill.empty()) {
        return Written::no("the colour `" + c.name + "` gives nothing: a colour, what a terminal "
                           "shows for it, or its fill");
    }
    return Written::ok();
}

/// ONE SIZE'S FORM: its name and a whole number of points from `kMinThemePoints` to
/// `kMaxThemePoints`.
inline Written check_theme_size(const ThemeSize& s) {
    if (const Written name = check_entry_name("a size", s.name); !name.accepted) {
        return name;
    }
    if (s.points < kMinThemePoints || s.points > kMaxThemePoints) {
        return Written::no("the size `" + s.name + "` stands at " + std::to_string(s.points) +
                           " points: a whole number from " + std::to_string(kMinThemePoints) +
                           " to " + std::to_string(kMaxThemePoints));
    }
    return Written::ok();
}

/// ONE FONT'S FORM: its name and a face's name, 1 to `kMaxThemeFaceLen` bytes of printable ASCII.
inline Written check_theme_font(const ThemeFont& f) {
    if (const Written name = check_entry_name("a font", f.name); !name.accepted) {
        return name;
    }
    bool ok = !f.face.empty() && f.face.size() <= kMaxThemeFaceLen;
    for (const char c : f.face) {
        ok = ok && static_cast<unsigned char>(c) >= 0x20u && static_cast<unsigned char>(c) <= 0x7Eu;
    }
    if (!ok) {
        return Written::no("the font `" + f.name + "` names its face in 1 to " +
                           std::to_string(kMaxThemeFaceLen) + " bytes of printable ASCII");
    }
    return Written::ok();
}

namespace detail {

/// Each name once in `items`, by `name_of`, else a refusal naming the one written twice.
template <class Items, class Name>
Written once_each(const Items& items, Name name_of, std::string_view what, std::string_view where) {
    std::vector<std::string_view> names;
    names.reserve(items.size());
    for (const auto& i : items) {
        names.emplace_back(name_of(i));
    }
    std::sort(names.begin(), names.end());
    for (std::size_t i = 1; i < names.size(); ++i) {
        if (names[i - 1] == names[i]) {
            return Written::no(std::string(where) + " gives the " + std::string(what) + " `" +
                               std::string(names[i]) + "` twice");
        }
    }
    return Written::ok();
}

inline Written within(std::size_t have, std::size_t most, std::string_view what,
                      std::string_view where) {
    if (have > most) {
        return Written::no(std::string(where) + " holds at most " + std::to_string(most) + " " +
                           std::string(what) + " -- it holds " + std::to_string(have));
    }
    return Written::ok();
}

/// The first entry of `items` `check` refuses, its refusal opening with `where`.
template <class Items, class Check>
Written first_bad(const Items& items, Check check, const std::string& where) {
    for (const auto& i : items) {
        if (const Written one = check(i); !one.accepted) {
            return Written::no(where + ": " + one.refusal);
        }
    }
    return Written::ok();
}

inline std::string_view name_of(const ThemeColor& c) { return c.name; }
inline std::string_view name_of(const ThemeFont& f) { return f.name; }
inline std::string_view name_of(const ThemeSize& s) { return s.name; }

} // namespace detail

/// A WHOLE THEME'S FORM: its name and summary, `on_failure` a word it knows, its outline three
/// characters or none, each list within its bound, every entry sound, no include of itself or
/// twice, and nothing given twice -- a colour, font or size by its name, an office's slots, a
/// setting under one target, a key for one action.
inline Written check_theme_file(const ThemeFile& t) {
    if (const Written name = check_theme_name(t.name); !name.accepted) {
        return name;
    }
    const std::string where = "theme `" + t.name + "`";
    if (t.summary.size() > kMaxThemeSummaryLen || !workshop::pane_setting_text_ok(t.summary)) {
        return Written::no(where + ": a summary is at most " + std::to_string(kMaxThemeSummaryLen) +
                           " bytes of printable ASCII");
    }
    if (t.on_failure != kOnFailureRefuse && t.on_failure != kOnFailureSkip) {
        return Written::no(where + ": `on_failure` is `refuse` or `skip`, not `" + t.on_failure +
                           "`");
    }
    bool outline_ok = t.outline.empty() || t.outline.size() == 3;
    for (const char c : t.outline) {
        outline_ok = outline_ok && static_cast<unsigned char>(c) >= 0x21u &&
                     static_cast<unsigned char>(c) <= 0x7Eu;
    }
    if (!outline_ok) {
        return Written::no(where + ": its outline `" + t.outline +
                           "` is three printable characters -- a corner, across and down -- or "
                           "none");
    }
    if (t.includes.size() > kMaxThemeIncludes) {
        return Written::no(where + " includes at most " + std::to_string(kMaxThemeIncludes) +
                           " themes -- it lists " + std::to_string(t.includes.size()));
    }
    for (std::size_t i = 0; i < t.includes.size(); ++i) {
        const std::string& inc = t.includes[i];
        if (const Written name = check_theme_name(inc); !name.accepted) {
            return Written::no(where + ": " + name.refusal);
        }
        if (inc == t.name) {
            return Written::no(where + " includes itself -- theme cycle: " + t.name + " -> " +
                               t.name);
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (t.includes[j] == inc) {
                return Written::no(where + " includes `" + inc + "` twice");
            }
        }
    }
    const auto by_name = [](const auto& i) -> std::string_view { return detail::name_of(i); };
    const auto by_office = [](const ThemeSlots& s) -> std::string_view { return s.office; };
    for (const Written& w : {detail::within(t.colors.size(), kMaxThemeColors, "colours", where),
                             detail::first_bad(t.colors, check_theme_color, where),
                             detail::once_each(t.colors, by_name, "colour", where),
                             detail::within(t.fonts.size(), kMaxThemeFonts, "fonts", where),
                             detail::first_bad(t.fonts, check_theme_font, where),
                             detail::once_each(t.fonts, by_name, "font", where),
                             detail::within(t.sizes.size(), kMaxThemeSizes, "sizes", where),
                             detail::first_bad(t.sizes, check_theme_size, where),
                             detail::once_each(t.sizes, by_name, "size", where),
                             detail::within(t.slots.size(), kMaxThemeSlotOffices,
                                            "offices' slots", where),
                             detail::once_each(t.slots, by_office, "slots of", where)}) {
        if (!w.accepted) {
            return w;
        }
    }
    for (const ThemeSlots& s : t.slots) {
        const std::string of = where + "'s slots of `" + s.office + "`";
        if (s.office.find('.') == std::string::npos || s.office.find('/') != std::string::npos ||
            !check_theme_target(s.office).accepted) {
            return Written::no(where + ": slots belong to an office, named with a `.` -- `" +
                               s.office + "` is none");
        }
        for (const Written& w :
             {detail::within(s.colors.size() + s.sizes.size(), kMaxThemeSlotEntries, "slots", of),
              detail::first_bad(s.colors, check_theme_color, of),
              detail::once_each(s.colors, by_name, "colour", of),
              detail::first_bad(s.sizes, check_theme_size, of),
              detail::once_each(s.sizes, by_name, "size", of)}) {
            if (!w.accepted) {
                return w;
            }
        }
    }
    if (const Written w = detail::within(t.settings.size(), kMaxThemeSettings, "settings", where);
        !w.accepted) {
        return w;
    }
    std::vector<std::pair<std::string_view, std::string_view>> places;
    places.reserve(t.settings.size());
    for (const ThemeSetting& s : t.settings) {
        if (const Written one = check_theme_setting(s); !one.accepted) {
            return Written::no(where + ": " + one.refusal);
        }
        places.emplace_back(s.target, s.key);
    }
    std::sort(places.begin(), places.end());
    for (std::size_t i = 1; i < places.size(); ++i) {
        if (places[i - 1] == places[i]) {
            return Written::no(where + " sets `" + std::string(places[i].second) + "` of `" +
                               std::string(places[i].first) + "` twice");
        }
    }
    if (const Written w = detail::within(t.keys.size(), kMaxThemeKeys, "keys", where);
        !w.accepted) {
        return w;
    }
    if (const Written w = detail::first_bad(t.keys, check_theme_key, where); !w.accepted) {
        return w;
    }
    std::vector<std::string_view> actions;
    for (const ThemeKey& k : t.keys) {
        actions.emplace_back(k.action);
    }
    std::sort(actions.begin(), actions.end());
    for (std::size_t i = 1; i < actions.size(); ++i) {
        if (actions[i - 1] == actions[i]) {
            return Written::no(where + " sets a key for `" + std::string(actions[i]) + "` twice");
        }
    }
    return Written::ok();
}

// ---- Text --------------------------------------------------------------------------------

/// The order a setting is kept in: by target, then key.
inline bool theme_setting_before(const ThemeSetting& a, const ThemeSetting& b) {
    return std::tie(a.target, a.key) < std::tie(b.target, b.key);
}

/// THE THEME AS IT IS WRITTEN: its own format and version, its settings in target and key order;
/// everything else stays in the order authored, which is the order it applies in.
inline ThemeFile canonical_theme(ThemeFile t) {
    t.format = kThemeFormat;
    t.format_version = kThemeFormatVersion;
    std::stable_sort(t.settings.begin(), t.settings.end(), theme_setting_before);
    return t;
}

namespace detail {

namespace js = persist::json;
using Members = std::vector<std::pair<std::string, js::Node>>;

inline js::Node object_of(Members members) {
    js::Node n;
    n.kind = js::Node::Kind::kObject;
    n.members = std::move(members);
    return n;
}

inline js::Node color_node(const ThemeColor& c) {
    Members m{{"name", js::text_node(c.name)}};
    if (!c.color.empty()) m.emplace_back("color", js::text_node(c.color));
    if (!c.cells.empty()) m.emplace_back("cells", js::text_node(c.cells));
    if (!c.fill.empty()) m.emplace_back("fill", js::text_node(c.fill));
    return object_of(std::move(m));
}

inline js::Node size_node(const ThemeSize& s) {
    return object_of({{"name", js::text_node(s.name)}, {"points", js::number_node(s.points)}});
}

inline js::Node font_node(const ThemeFont& f) {
    return object_of({{"name", js::text_node(f.name)}, {"face", js::text_node(f.face)}});
}

inline js::Node slots_node(const ThemeSlots& s);

inline js::Node setting_node(const ThemeSetting& s) {
    Members o{{"target", js::text_node(s.target)}, {"key", js::text_node(s.key)}};
    if (s.flag) o.emplace_back("flag", js::flag_node(*s.flag));
    if (s.number) o.emplace_back("number", js::number_node(*s.number));
    if (s.text) o.emplace_back("text", js::text_node(*s.text));
    if (s.clear) o.emplace_back("clear", js::flag_node(true));
    return object_of(std::move(o));
}

inline js::Node word_node(const std::string& w) { return js::text_node(w); }

template <class T, class F> js::Node list_of(const std::vector<T>& items, F node) {
    js::Node n = js::list_node();
    for (const T& i : items) {
        n.items.push_back(node(i));
    }
    return n;
}

inline js::Node slots_node(const ThemeSlots& s) {
    Members o{{"office", js::text_node(s.office)}};
    if (!s.colors.empty()) o.emplace_back("colors", list_of(s.colors, color_node));
    if (!s.sizes.empty()) o.emplace_back("sizes", list_of(s.sizes, size_node));
    return object_of(std::move(o));
}

inline js::Node key_node(const ThemeKey& k) {
    return object_of(
        {{"action", js::text_node(k.action)}, {"gestures", list_of(k.gestures, word_node)}});
}

} // namespace detail

/// THE THEME AS PLAIN JSON, version 2: every member it holds a value for, in the order a weaver
/// reads a theme in -- what it is, what it includes, how it fails, its look, its settings, its keys.
inline std::string to_text(ThemeFile theme) {
    namespace js = persist::json;
    using detail::list_of;
    const ThemeFile t = canonical_theme(std::move(theme));
    detail::Members m{{"format", js::text_node(t.format)},
                      {"format_version", js::number_node(t.format_version)},
                      {"name", js::text_node(t.name)}};
    if (!t.summary.empty()) m.emplace_back("summary", js::text_node(t.summary));
    if (!t.includes.empty()) m.emplace_back("includes", list_of(t.includes, detail::word_node));
    m.emplace_back("on_failure", js::text_node(t.on_failure));
    if (!t.outline.empty()) m.emplace_back("outline", js::text_node(t.outline));
    if (!t.colors.empty()) m.emplace_back("colors", list_of(t.colors, detail::color_node));
    if (!t.fonts.empty()) m.emplace_back("fonts", list_of(t.fonts, detail::font_node));
    if (!t.sizes.empty()) m.emplace_back("sizes", list_of(t.sizes, detail::size_node));
    if (!t.slots.empty()) m.emplace_back("slots", list_of(t.slots, detail::slots_node));
    if (!t.settings.empty()) m.emplace_back("settings", list_of(t.settings, detail::setting_node));
    if (!t.keys.empty()) m.emplace_back("keys", list_of(t.keys, detail::key_node));
    return js::to_text(detail::object_of(std::move(m)));
}

/// What reading a theme produced: whether it worked, the theme, and the bytes it was read from
/// (what a rewrite must find still on disk).
struct LoadedTheme {
    Written outcome;
    ThemeFile theme;
    std::string raw_bytes;

    static LoadedTheme no(std::string why) {
        return LoadedTheme{Written::no(std::move(why)), ThemeFile{}, std::string()};
    }
};

/// One sentence for a version this build does not read.
inline std::string wrong_theme_version(std::int64_t found) {
    return "theme version " + std::to_string(found) + " -- this build reads versions " +
           std::to_string(kThemeFormatVersionOne) + " and " + std::to_string(kThemeFormatVersion);
}

namespace detail {

/// THE PLAIN JSON READ AS A THEME: each member one the file holds, of its kind; any other member,
/// or a kind a member is not, is refused by name.
struct ThemeReader {
    std::string refusal;

    bool fail(std::string why) {
        if (refusal.empty()) refusal = std::move(why);
        return false;
    }

    bool only(const js::Node& o, const std::string& where,
              std::initializer_list<const char*> known) {
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
        if (n.kind != k) {
            return fail(what + " is " + js::kind_word(n.kind) + ", not " + js::kind_word(k));
        }
        return true;
    }

    bool text(const js::Node& o, const char* name, std::string& out, const std::string& where,
              bool required = false) {
        const js::Node* v = o.member(name);
        if (v == nullptr) {
            return !required || fail(where + " needs `" + name + "`");
        }
        if (!kind(*v, js::Node::Kind::kText, where + "'s `" + name + "`")) {
            return false;
        }
        out = v->text;
        return true;
    }

    bool number(const js::Node& o, const char* name, std::int64_t& out, const std::string& where,
                bool required = false) {
        const js::Node* v = o.member(name);
        if (v == nullptr) {
            return !required || fail(where + " needs `" + name + "`");
        }
        if (!kind(*v, js::Node::Kind::kNumber, where + "'s `" + name + "`")) {
            return false;
        }
        out = v->number;
        return true;
    }

    template <class F>
    bool list(const js::Node& o, const char* name, const std::string& where, F each) {
        const js::Node* v = o.member(name);
        if (v == nullptr) {
            return true;
        }
        if (!kind(*v, js::Node::Kind::kList, where + "'s `" + name + "`")) {
            return false;
        }
        for (std::size_t i = 0; i < v->items.size(); ++i) {
            if (!each(v->items[i], where + "'s " + name + " " + std::to_string(i))) {
                return false;
            }
        }
        return true;
    }

    bool color(const js::Node& n, const std::string& where, ThemeColor& c) {
        return kind(n, js::Node::Kind::kObject, where) &&
               only(n, where, {"name", "color", "cells", "fill", "about"}) &&
               text(n, "name", c.name, where, true) && text(n, "color", c.color, where) &&
               text(n, "cells", c.cells, where) && text(n, "fill", c.fill, where);
    }

    bool size(const js::Node& n, const std::string& where, ThemeSize& s) {
        return kind(n, js::Node::Kind::kObject, where) &&
               only(n, where, {"name", "points", "about"}) && text(n, "name", s.name, where, true) &&
               number(n, "points", s.points, where, true);
    }

    bool font(const js::Node& n, const std::string& where, ThemeFont& f) {
        return kind(n, js::Node::Kind::kObject, where) &&
               only(n, where, {"name", "face", "about"}) && text(n, "name", f.name, where, true) &&
               text(n, "face", f.face, where, true);
    }

    bool slots(const js::Node& n, const std::string& where, ThemeSlots& s) {
        return kind(n, js::Node::Kind::kObject, where) &&
               only(n, where, {"office", "colors", "sizes"}) &&
               text(n, "office", s.office, where, true) &&
               list(n, "colors", where,
                    [&](const js::Node& c, const std::string& w) {
                        ThemeColor one;
                        return color(c, w, one) && (s.colors.push_back(std::move(one)), true);
                    }) &&
               list(n, "sizes", where, [&](const js::Node& c, const std::string& w) {
                   ThemeSize one;
                   return size(c, w, one) && (s.sizes.push_back(std::move(one)), true);
               });
    }

    bool flag_member(const js::Node& o, const char* name, const std::string& where,
                     std::optional<bool>& out) {
        if (const js::Node* v = o.member(name)) {
            if (!kind(*v, js::Node::Kind::kFlag, where + "'s `" + name + "`")) return false;
            out = v->flag;
        }
        return true;
    }

    bool setting(const js::Node& n, const std::string& where, ThemeSetting& s) {
        if (!kind(n, js::Node::Kind::kObject, where) ||
            !only(n, where, {"target", "key", "flag", "number", "text", "clear"}) ||
            !text(n, "target", s.target, where, true) || !text(n, "key", s.key, where, true) ||
            !flag_member(n, "flag", where, s.flag)) {
            return false;
        }
        if (n.member("number") != nullptr) {
            std::int64_t v = 0;
            if (!number(n, "number", v, where)) return false;
            s.number = v;
        }
        if (n.member("text") != nullptr) {
            std::string v;
            if (!text(n, "text", v, where)) return false;
            s.text = std::move(v);
        }
        std::optional<bool> clear;
        if (!flag_member(n, "clear", where, clear)) return false;
        s.clear = clear.value_or(false);
        return true;
    }

    bool key(const js::Node& n, const std::string& where, ThemeKey& k) {
        if (!kind(n, js::Node::Kind::kObject, where) || !only(n, where, {"action", "gestures"}) ||
            !text(n, "action", k.action, where, true)) {
            return false;
        }
        if (n.member("gestures") == nullptr) {
            return fail(where + " needs `gestures`");
        }
        return list(n, "gestures", where, [&](const js::Node& g, const std::string& w) {
            return kind(g, js::Node::Kind::kText, w) && (k.gestures.push_back(g.text), true);
        });
    }

    template <class T, class Read>
    bool each(const js::Node& root, const char* name, std::vector<T>& into, Read read) {
        return list(root, name, "the theme", [&](const js::Node& n, const std::string& w) {
            T one;
            return (this->*read)(n, w, one) && (into.push_back(std::move(one)), true);
        });
    }

    bool theme(const js::Node& root, ThemeFile& t) {
        // WHAT IT SAYS IT IS, FIRST: another file, or a later version, is refused by that rather
        // than by its first member this build does not know.
        const std::string file = "a theme file";
        if (!kind(root, js::Node::Kind::kObject, file) ||
            !text(root, "format", t.format, file, true) ||
            !number(root, "format_version", t.format_version, file, true)) {
            return false;
        }
        if (t.format != kThemeFormat) {
            return fail("not a theme file: it says it is `" + t.format + "`");
        }
        if (t.format_version != kThemeFormatVersion) {
            return fail(wrong_theme_version(t.format_version));
        }
        if (!only(root, file,
                  {"format", "format_version", "name", "summary", "includes", "on_failure",
                   "outline", "colors", "fonts", "sizes", "slots", "settings", "keys", "about"})) {
            return false;
        }
        const std::string at = "the theme";
        return text(root, "name", t.name, at, true) && text(root, "summary", t.summary, at) &&
               list(root, "includes", at,
                    [&](const js::Node& n, const std::string& w) {
                        return kind(n, js::Node::Kind::kText, w) &&
                               (t.includes.push_back(n.text), true);
                    }) &&
               text(root, "on_failure", t.on_failure, at) && text(root, "outline", t.outline, at) &&
               each(root, "colors", t.colors, &ThemeReader::color) &&
               each(root, "fonts", t.fonts, &ThemeReader::font) &&
               each(root, "sizes", t.sizes, &ThemeReader::size) &&
               each(root, "slots", t.slots, &ThemeReader::slots) &&
               each(root, "settings", t.settings, &ThemeReader::setting) &&
               each(root, "keys", t.keys, &ThemeReader::key);
    }
};

/// A version-1 file, Loom's envelope, admitted against version 1's shape and widened.
inline LoadedTheme from_envelope(std::string_view text, const std::string& at) {
    const loom::Unverified claim = loom::compat::parse(text);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<v1::ThemeFile>(), loom::Report::FirstError);
        return LoadedTheme::no(at + "not a theme file: " + refused.first_error().message());
    }
    if (claim.claimed_name() != std::string(v1::ThemeFile::zen_name)) {
        return LoadedTheme::no(at + "not a theme file: it says it is `" + claim.claimed_name() +
                               "`");
    }
    if (claim.claimed_version() != v1::ThemeFile::zen_version) {
        return LoadedTheme::no(at +
                               wrong_theme_version(static_cast<std::int64_t>(
                                   claim.claimed_version())) +
                               ", and a version-2 theme is plain JSON, not an envelope");
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<v1::ThemeFile>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedTheme::no(at + admitted.first_error().message());
    }
    const v1::ThemeFile old = loom::from_value<v1::ThemeFile>(admitted.value());
    if (old.format != kThemeFormat) {
        return LoadedTheme::no(at + "not a theme file: it says it is `" + old.format + "`");
    }
    if (old.format_version != kThemeFormatVersionOne) {
        return LoadedTheme::no(at + wrong_theme_version(old.format_version));
    }
    return LoadedTheme{Written::ok(), widened(old), std::string(text)};
}

} // namespace detail

/// Text to a theme, every refusal opening with `label`. Total: every input is a theme or a
/// refusal with a reason, and nothing here throws. A version-2 file is plain JSON; a file wrapped
/// in Loom's envelope (a `zen` member at its top) is version 1, read and widened.
inline LoadedTheme from_text(std::string_view text, std::string_view label = "theme") {
    const std::string at = std::string(label) + ": ";
    if (text.size() > kMaxThemeBytes) {
        return LoadedTheme::no(at + std::to_string(text.size()) +
                               " bytes is larger than a theme file can be");
    }
    const persist::json::Parsed parsed = persist::json::parse(text);
    if (!parsed.root) {
        return LoadedTheme::no(at + "not a theme file: " + parsed.refusal);
    }
    LoadedTheme out;
    if (parsed.root->kind == persist::json::Node::Kind::kObject &&
        parsed.root->member("zen") != nullptr) {
        out = detail::from_envelope(text, at);
        if (!out.outcome.accepted) {
            return out;
        }
    } else {
        detail::ThemeReader reader;
        ThemeFile theme;
        if (!reader.theme(*parsed.root, theme)) {
            return LoadedTheme::no(at + reader.refusal);
        }
        out = LoadedTheme{Written::ok(), std::move(theme), std::string(text)};
    }
    if (const Written form = check_theme_file(out.theme); !form.accepted) {
        return LoadedTheme::no(at + form.refusal);
    }
    out.theme = canonical_theme(std::move(out.theme));
    return out;
}

// ---- The files ---------------------------------------------------------------------------

/// Where theme `name` lives under `dir`.
inline std::string theme_path(const std::string& dir, std::string_view name) {
    return (std::filesystem::path(dir) / (std::string(name) + ".json"))
        .lexically_normal()
        .generic_string();
}

/// Read theme `name` from `dir`: a sound name, a file within the ceiling, a theme in form, and the
/// name inside the file the file's own.
inline LoadedTheme load_file(const std::string& dir, std::string_view name) {
    if (const Written form = check_theme_name(name); !form.accepted) {
        return LoadedTheme::no(form.refusal);
    }
    const std::string path = theme_path(dir, name);
    const persist::FileText read = persist::read_file(path, kMaxThemeBytes, "a theme file");
    if (!read.outcome.accepted) {
        return LoadedTheme{read.outcome, ThemeFile{}, std::string()};
    }
    LoadedTheme decoded = from_text(read.text, path);
    if (decoded.outcome.accepted && decoded.theme.name != name) {
        return LoadedTheme::no(path + ": theme name `" + decoded.theme.name +
                               "` does not match file name `" + std::string(name) + "`");
    }
    return decoded;
}

/// WRITE A THEME under `dir` by the safe write, the directory made on the first write, always as
/// version 2. A theme already there is overwritten only with `replace`, and, given the bytes it was
/// read from, only while the file still holds them: a second hand that wrote it since is told, and
/// nothing is written.
inline Written save_file(const std::string& dir, ThemeFile theme, bool replace = false,
                         const std::optional<std::string>& expected_bytes = std::nullopt) {
    if (const Written form = check_theme_file(theme); !form.accepted) {
        return form;
    }
    const std::string path = theme_path(dir, theme.name);
    std::error_code ec;
    const bool there = std::filesystem::exists(path, ec);
    if (ec) {
        return Written::no("cannot tell whether " + path + " exists: " + ec.message() +
                           " -- nothing was written");
    }
    if (there && !replace) {
        return Written::no("theme `" + theme.name +
                           "` already exists -- pass replace to overwrite it");
    }
    if (there && expected_bytes.has_value()) {
        const persist::FileText current = persist::read_file(path, kMaxThemeBytes, "a theme file");
        if (!current.outcome.accepted || current.text != *expected_bytes) {
            return Written::no("`" + theme.name +
                               "` changed on disk since it was read -- nothing was written");
        }
    }
    return persist::write_file_making_room(path, to_text(std::move(theme)));
}

// ---- Resolution --------------------------------------------------------------------------

/// One resolved setting and the theme that gave it.
struct ResolvedThemeSetting {
    ThemeSetting setting;
    std::string from_theme;

    ZEN_SHAPE(ResolvedThemeSetting, 1, ZEN_FIELD(setting), ZEN_FIELD(from_theme));
    friend bool operator==(const ResolvedThemeSetting&, const ResolvedThemeSetting&) = default;
};

/// One resolved key and the theme that gave it.
struct ResolvedThemeKey {
    ThemeKey key;
    std::string from_theme;

    ZEN_SHAPE(ResolvedThemeKey, 2, ZEN_FIELD(key), ZEN_FIELD(from_theme));
    friend bool operator==(const ResolvedThemeKey&, const ResolvedThemeKey&) = default;
};

/// A THEME RESOLVED WITH ITS INCLUDES, or why not: the themes in the order each last applied; the
/// theme asked for's own `on_failure`; the outline, colours, fonts and sizes the chain gives the
/// pool and each office's slots, later winning by name; the settings in target and key order (a
/// clear kept, so applying it clears); the keys in the order first set; and a fingerprint of those
/// entries.
struct ThemeResolved {
    bool accepted = false;
    std::string refusal;
    std::string theme;
    std::string summary;
    std::vector<std::string> chain;
    std::string on_failure = kOnFailureRefuse;
    std::string outline;
    std::vector<ThemeColor> colors;
    std::vector<ThemeFont> fonts;
    std::vector<ThemeSize> sizes;
    std::vector<ThemeSlots> slots;
    std::vector<ResolvedThemeSetting> settings;
    std::vector<ResolvedThemeKey> keys;
    std::int64_t fingerprint = 0;

    ZEN_SHAPE(ThemeResolved, 2, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(theme),
              ZEN_FIELD(summary), ZEN_FIELD(chain), ZEN_FIELD(on_failure), ZEN_FIELD(outline),
              ZEN_FIELD(colors), ZEN_FIELD(fonts), ZEN_FIELD(sizes), ZEN_FIELD(slots),
              ZEN_FIELD(settings), ZEN_FIELD(keys), ZEN_FIELD(fingerprint));
    friend bool operator==(const ThemeResolved&, const ThemeResolved&) = default;
};

/// THE RESOLVED ENTRIES' FINGERPRINT: 64-bit FNV-1a over each setting (target, key, kind, value),
/// each key (action, its gestures), and then, where the chain gives any, the outline, colours,
/// fonts, sizes and slots; the same in every build, and the same as version 1's for a theme of
/// settings and one-gesture keys alone; never 0, so a resolved theme always carries one. Which
/// theme gave an entry is not mixed in: the same values are the same theme.
inline std::int64_t theme_fingerprint(const ThemeResolved& r) {
    std::uint64_t h = 14695981039346656037ULL;
    const auto mix = [&h](std::string_view part) {
        for (const char c : part) {
            h ^= static_cast<unsigned char>(c);
            h *= 1099511628211ULL;
        }
    };
    const auto byte = [&mix](char c) { mix(std::string_view(&c, 1)); };
    for (const ResolvedThemeSetting& rs : r.settings) {
        const ThemeSetting& s = rs.setting;
        mix(s.target);
        byte('\0');
        mix(s.key);
        byte('\0');
        char tag = 'C';
        if (!s.clear) {
            switch (workshop::setting_kind(s)) {
            case workshop::SettingKind::kFlag: tag = 'F'; break;
            case workshop::SettingKind::kNumber: tag = 'N'; break;
            case workshop::SettingKind::kText: tag = 'T'; break;
            default: tag = '?'; break;
            }
        }
        byte(tag);
        mix(workshop::setting_value_text(s));
        byte('\n');
    }
    mix("\n--\n");
    for (const ResolvedThemeKey& rk : r.keys) {
        mix(rk.key.action);
        byte('\0');
        for (std::size_t i = 0; i < rk.key.gestures.size(); ++i) {
            if (i > 0) {
                byte(',');
            }
            mix(rk.key.gestures[i]);
        }
        byte('\n');
    }
    const auto color = [&](const ThemeColor& c) {
        mix(c.name);
        byte('\0');
        mix(c.color);
        byte('\0');
        mix(c.cells);
        byte('\0');
        mix(c.fill);
        byte('\n');
    };
    const auto size = [&](const ThemeSize& s) {
        mix(s.name);
        byte('\0');
        mix(std::to_string(s.points));
        byte('\n');
    };
    if (!r.outline.empty() || !r.colors.empty() || !r.fonts.empty() || !r.sizes.empty() ||
        !r.slots.empty()) {
        mix("\n==\n");
        mix(r.outline);
        byte('\n');
        for (const ThemeColor& c : r.colors) color(c);
        mix("\n--\n");
        for (const ThemeFont& f : r.fonts) {
            mix(f.name);
            byte('\0');
            mix(f.face);
            byte('\n');
        }
        mix("\n--\n");
        for (const ThemeSize& s : r.sizes) size(s);
        for (const ThemeSlots& s : r.slots) {
            mix("\n--\n");
            mix(s.office);
            byte('\n');
            for (const ThemeColor& c : s.colors) color(c);
            for (const ThemeSize& z : s.sizes) size(z);
        }
    }
    const auto out = static_cast<std::int64_t>(h);
    return out == 0 ? 1 : out;
}

namespace detail {

/// `entry` into `into` by its name: in place where the name stands, else after the rest.
template <class T> void by_name_into(std::vector<T>& into, const T& entry) {
    const auto at = std::find_if(into.begin(), into.end(),
                                 [&](const T& k) { return k.name == entry.name; });
    if (at != into.end()) {
        *at = entry;
    } else {
        into.push_back(entry);
    }
}

/// A colour given over one already given: each part it names replaces that part, and a part it
/// leaves empty keeps what stood.
inline void color_into(std::vector<ThemeColor>& into, const ThemeColor& c) {
    const auto at = std::find_if(into.begin(), into.end(),
                                 [&](const ThemeColor& k) { return k.name == c.name; });
    if (at == into.end()) {
        into.push_back(c);
        return;
    }
    if (!c.color.empty()) at->color = c.color;
    if (!c.cells.empty()) at->cells = c.cells;
    if (!c.fill.empty()) at->fill = c.fill;
}

/// One office's slots given over what stood: its colours and sizes by name.
inline void slots_into(std::vector<ThemeSlots>& into, const ThemeSlots& s) {
    auto at = std::find_if(into.begin(), into.end(),
                           [&](const ThemeSlots& k) { return k.office == s.office; });
    if (at == into.end()) {
        into.push_back(ThemeSlots{s.office, {}, {}});
        at = into.end() - 1;
    }
    for (const ThemeColor& c : s.colors) color_into(at->colors, c);
    for (const ThemeSize& z : s.sizes) by_name_into(at->sizes, z);
}

/// One resolution's walk: the themes open on the path, every visit counted, each theme read once.
template <class Loader> struct ThemeWalk {
    Loader& load;
    std::vector<std::string> path;
    std::size_t visited = 0;
    std::map<std::string, LoadedTheme, std::less<>> read;
    ThemeResolved out;

    bool refuse(std::string why) {
        out.refusal = std::move(why);
        return false;
    }

    std::size_t entries() const {
        std::size_t n = out.settings.size() + out.keys.size() + out.colors.size() +
                        out.fonts.size() + out.sizes.size();
        for (const ThemeSlots& s : out.slots) n += s.colors.size() + s.sizes.size();
        return n;
    }

    bool visit(const std::string& name) {
        if (const auto open = std::find(path.begin(), path.end(), name); open != path.end()) {
            std::string cycle;
            for (auto it = open; it != path.end(); ++it) {
                cycle += *it + " -> ";
            }
            return refuse("theme cycle: " + cycle + name);
        }
        if (path.size() > kMaxThemeDepth) {
            std::string chain;
            for (const std::string& p : path) {
                chain += p + " -> ";
            }
            return refuse("theme `" + name + "` is included more than " +
                          std::to_string(kMaxThemeDepth) + " deep: " + chain + name);
        }
        if (++visited > kMaxThemesResolved) {
            return refuse("resolving `" + path.front() + "` reads more than " +
                          std::to_string(kMaxThemesResolved) + " themes");
        }
        auto found = read.find(name);
        if (found == read.end()) {
            found = read.emplace(name, load(std::string_view(name))).first;
        }
        const LoadedTheme& loaded = found->second;
        if (!loaded.outcome.accepted) {
            if (path.empty()) {
                return refuse(loaded.outcome.refusal);
            }
            return refuse("in theme `" + path.back() + "` include `" + name +
                          "`: " + loaded.outcome.refusal);
        }
        path.push_back(name);
        for (const std::string& inc : loaded.theme.includes) {
            if (!visit(inc)) {
                return false;
            }
        }
        path.pop_back();

        // THE CHAIN SAYS THE ORDER EACH THEME LAST APPLIED IN, so a theme later in it wins over one
        // before it wherever both set an entry.
        const ThemeFile& t = loaded.theme;
        out.chain.erase(std::remove(out.chain.begin(), out.chain.end(), name), out.chain.end());
        out.chain.push_back(name);
        if (!t.outline.empty()) {
            out.outline = t.outline;
        }
        for (const ThemeColor& c : t.colors) color_into(out.colors, c);
        for (const ThemeFont& f : t.fonts) by_name_into(out.fonts, f);
        for (const ThemeSize& s : t.sizes) by_name_into(out.sizes, s);
        for (const ThemeSlots& s : t.slots) slots_into(out.slots, s);
        for (const ThemeSetting& s : t.settings) {
            ResolvedThemeSetting entry{s, t.name};
            const auto at = std::lower_bound(
                out.settings.begin(), out.settings.end(), s,
                [](const ResolvedThemeSetting& r, const ThemeSetting& v) {
                    return theme_setting_before(r.setting, v);
                });
            if (at != out.settings.end() && at->setting.target == s.target &&
                at->setting.key == s.key) {
                *at = std::move(entry);
            } else {
                out.settings.insert(at, std::move(entry));
            }
        }
        for (const ThemeKey& k : t.keys) {
            const auto at = std::find_if(out.keys.begin(), out.keys.end(),
                                         [&k](const ResolvedThemeKey& r) {
                                             return r.key.action == k.action;
                                         });
            if (at != out.keys.end()) {
                *at = ResolvedThemeKey{k, t.name};
            } else {
                out.keys.push_back(ResolvedThemeKey{k, t.name});
            }
        }
        if (entries() > kMaxResolvedEntries) {
            return refuse("resolving `" + (path.empty() ? name : path.front()) +
                          "` gives more than " + std::to_string(kMaxResolvedEntries) + " entries");
        }
        return true;
    }
};

} // namespace detail

/// RESOLVE THEME `root_name` THROUGH `load` (a name to a `LoadedTheme`): each include in order,
/// depth first and each resolved whole, then the theme's own entries; later wins per colour, font,
/// size and slot by its name, per target and key and per action. A cycle, an include nested past
/// `kMaxThemeDepth`, more than `kMaxThemesResolved` themes read, more than `kMaxResolvedEntries`
/// entries, or any theme that will not load refuses the whole resolution, naming it: never half
/// resolved.
template <class Loader>
ThemeResolved resolve_theme_with(std::string_view root_name, Loader&& load) {
    if (const Written form = check_theme_name(root_name); !form.accepted) {
        ThemeResolved refused;
        refused.refusal = form.refusal;
        return refused;
    }
    detail::ThemeWalk<std::remove_reference_t<Loader>> walk{load, {}, 0, {}, {}};
    const std::string root(root_name);
    if (!walk.visit(root)) {
        ThemeResolved refused;
        refused.theme = root;
        refused.refusal = std::move(walk.out.refusal);
        return refused;
    }
    ThemeResolved out = std::move(walk.out);
    const ThemeFile& theme = walk.read.at(root).theme;
    out.accepted = true;
    out.theme = theme.name;
    out.summary = theme.summary;
    out.on_failure = theme.on_failure;
    out.fingerprint = theme_fingerprint(out);
    return out;
}

/// ...with every theme read from `dir`.
inline ThemeResolved resolve_theme(const std::string& dir, std::string_view root_name) {
    return resolve_theme_with(root_name,
                              [&dir](std::string_view name) { return load_file(dir, name); });
}

// ---- The library -------------------------------------------------------------------------

/// ONE FILE IN THE THEMES FOLDER: its theme's name, summary, includes and how many settings, keys
/// and entries of its look (colours, fonts, sizes and slots) it holds -- or the file's stem and
/// why it would not load.
struct ThemeSummary {
    std::string name;
    std::string summary;
    std::vector<std::string> includes;
    std::int64_t settings = 0;
    std::int64_t keys = 0;
    std::int64_t look = 0;
    std::string refusal;

    ZEN_SHAPE(ThemeSummary, 2, ZEN_FIELD(name), ZEN_FIELD(summary), ZEN_FIELD(includes),
              ZEN_FIELD(settings), ZEN_FIELD(keys), ZEN_FIELD(look), ZEN_FIELD(refusal));
    friend bool operator==(const ThemeSummary&, const ThemeSummary&) = default;
};

/// How many entries of its look a theme holds: its colours, fonts and sizes, and every slot.
inline std::int64_t look_entries(const ThemeFile& t) {
    std::size_t n = t.colors.size() + t.fonts.size() + t.sizes.size();
    for (const ThemeSlots& s : t.slots) n += s.colors.size() + s.sizes.size();
    return static_cast<std::int64_t>(n);
}

/// EVERY `<stem>.json` IN `dir`, by name, the first `kMaxThemesListed` found: a theme that loads is
/// summarised, and one that does not is listed by its stem with its refusal, so a broken file is
/// seen rather than lost. A folder that is not there holds none; a write's `.saving` candidate is
/// no theme.
inline std::vector<ThemeSummary> list_themes(const std::string& dir) {
    std::vector<ThemeSummary> out;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return out;
    }
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code kind;
        if (!it->is_regular_file(kind) || it->path().extension() != ".json") {
            continue;
        }
        if (out.size() >= kMaxThemesListed) {
            break;
        }
        const std::string stem = it->path().stem().string();
        const LoadedTheme loaded = load_file(dir, stem);
        if (loaded.outcome.accepted) {
            out.push_back(ThemeSummary{loaded.theme.name, loaded.theme.summary,
                                       loaded.theme.includes,
                                       static_cast<std::int64_t>(loaded.theme.settings.size()),
                                       static_cast<std::int64_t>(loaded.theme.keys.size()),
                                       look_entries(loaded.theme), std::string()});
        } else {
            out.push_back(ThemeSummary{stem, std::string(), {}, 0, 0, 0, loaded.outcome.refusal});
        }
    }
    std::sort(out.begin(), out.end(),
              [](const ThemeSummary& a, const ThemeSummary& b) { return a.name < b.name; });
    return out;
}

} // namespace zengine::settings

#endif // ZENGINE_SETTINGS_THEME_PERSIST_HPP
