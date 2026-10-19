// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_SETTINGS_THEME_PERSIST_HPP
#define ZENGINE_SETTINGS_THEME_PERSIST_HPP

// THE THEME FILE -- one theme a file, `<config root>/themes/<name>.json`: the settings and keys it
// writes and the themes it includes first. A theme resolves with its includes to one list of
// entries, each naming the theme that gave it, or is refused whole.

#include "workshop/keymap.hpp"        // `parse_gesture`, an action id's bound
#include "workshop/pane_settings.hpp" // a setting's key, kinds and text
#include "workshop/persist.hpp"       // the bounded read and the safe write
#include "workshop/property.hpp"      // `Written`

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

/// What a theme file says it is, and the one version this build reads and writes.
inline constexpr const char* kThemeFormat = "zengine-theme";
inline constexpr std::int64_t kThemeFormatVersion = 1;

/// The folder under the per-user configuration root that holds the themes, one file each.
inline constexpr const char* kThemesSubdir = "themes";

/// What one file may hold: its name (the file's name too), summary and targets, and how many
/// settings, keys and includes.
inline constexpr std::size_t kMaxThemeNameLen = 32;
inline constexpr std::size_t kMaxThemeSummaryLen = 64;
inline constexpr std::size_t kMaxThemeTargetLen = 128;
inline constexpr std::size_t kMaxThemeSettings = 512;
inline constexpr std::size_t kMaxThemeKeys = 128;
inline constexpr std::size_t kMaxThemeIncludes = 8;

/// What one resolution may spend: the themes it reads, how deep an include nests below the theme
/// asked for, and the entries it resolves to.
inline constexpr std::size_t kMaxThemesResolved = 16;
inline constexpr std::size_t kMaxThemeDepth = 4;
inline constexpr std::size_t kMaxResolvedEntries = 1024;

/// The ceiling is the read side of the decoder's own materialisation law -- a hostile file does
/// not get to choose the cost of refusing it.
inline constexpr std::uintmax_t kMaxThemeBytes = 1u << 16;

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

/// ONE KEY A THEME SETS: an action id and a gesture in the keymap's own spelling, `none` included.
struct ThemeKey {
    std::string action;
    std::string gesture;

    ZEN_SHAPE(ThemeKey, 1, ZEN_FIELD(action), ZEN_FIELD(gesture));
    friend bool operator==(const ThemeKey&, const ThemeKey&) = default;
};

/// A WHOLE THEME FILE: its includes, applied first and in order, then its own settings (kept in
/// target and key order) and keys (in authored order).
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

/// The envelope's shape version and the format version are ONE NUMBER: a foreign version is
/// refused by its number, before its fields are judged against this shape.
static_assert(ThemeFile::zen_version == static_cast<std::uint32_t>(kThemeFormatVersion),
              "the theme file's format version and its envelope's shape version are one number");

// ---- Form ----------------------------------------------------------------------------------

/// A THEME'S NAME: 1 to `kMaxThemeNameLen` bytes of `a`-`z`, `0`-`9` and `-`, so the name is a
/// file name on every platform and never a path.
inline workshop::Written check_theme_name(std::string_view name) {
    if (name.empty()) {
        return workshop::Written::no("a theme's name cannot be empty");
    }
    if (name.size() > kMaxThemeNameLen) {
        return workshop::Written::no("a theme's name is at most " +
                                     std::to_string(kMaxThemeNameLen) + " bytes -- this one has " +
                                     std::to_string(name.size()));
    }
    for (const char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return workshop::Written::no("a theme's name is `a`-`z`, `0`-`9` and `-` -- `" +
                                         std::string(name) + "` is not");
        }
    }
    return workshop::Written::ok();
}

/// WHERE A THEME SETTING GOES: `*` (every pane that takes its key), `<office>` (an office-wide
/// setting) or `<office>/<pane>` (that pane), an office named with a `.`.
inline workshop::Written check_theme_target(std::string_view target) {
    if (target.empty() || target.size() > kMaxThemeTargetLen) {
        return workshop::Written::no("a theme setting's target is 1 to " +
                                     std::to_string(kMaxThemeTargetLen) + " bytes");
    }
    for (const char c : target) {
        if (static_cast<unsigned char>(c) <= 0x20u || static_cast<unsigned char>(c) > 0x7Eu) {
            return workshop::Written::no("a theme setting's target `" + std::string(target) +
                                         "` is printable ASCII with no space");
        }
    }
    if (target == "*") {
        return workshop::Written::ok();
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
        return workshop::Written::no("a theme setting's target is `*`, `<office>` or "
                                     "`<office>/<pane>` -- `" +
                                     std::string(target) + "` is not");
    }
    return workshop::Written::ok();
}

/// ONE SETTING'S FORM: its target, its key, and exactly one value -- or, as a clear, none.
inline workshop::Written check_theme_setting(const ThemeSetting& s) {
    if (const workshop::Written target = check_theme_target(s.target); !target.accepted) {
        return target;
    }
    if (std::string wrong = workshop::pane_setting_key_problem(s.key); !wrong.empty()) {
        return workshop::Written::no("`" + s.target + "`: " + wrong);
    }
    const workshop::SettingKind kind = workshop::setting_kind(s);
    if (s.clear) {
        if (kind != workshop::SettingKind::kNone) {
            return workshop::Written::no("`" + s.key + "` of `" + s.target +
                                         "` is a clear and holds " +
                                         workshop::setting_kind_word(kind) +
                                         " -- a clear holds no value");
        }
        return workshop::Written::ok();
    }
    if (kind == workshop::SettingKind::kNone || kind == workshop::SettingKind::kSeveral) {
        return workshop::Written::no("`" + s.key + "` of `" + s.target + "` holds " +
                                     workshop::setting_kind_word(kind) +
                                     " -- a theme setting holds exactly one of a flag, a number or "
                                     "a text, or is a clear");
    }
    if (kind == workshop::SettingKind::kText && !workshop::pane_setting_text_ok(*s.text)) {
        return workshop::Written::no("`" + s.key + "` of `" + s.target + "` holds a text past " +
                                     std::to_string(workshop::kMaxPaneSettingTextLen) +
                                     " bytes of printable ASCII");
    }
    return workshop::Written::ok();
}

/// ONE KEY'S FORM: an action id of `a`-`z`, `0`-`9`, `.` and `-` within an action id's bound, and
/// a gesture the keymap reads.
inline workshop::Written check_theme_key(const ThemeKey& k) {
    if (k.action.empty() || k.action.size() > workshop::kMaxPaneActionIdLen) {
        return workshop::Written::no("a theme key's action is 1 to " +
                                     std::to_string(workshop::kMaxPaneActionIdLen) + " bytes");
    }
    for (const char c : k.action) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) {
            return workshop::Written::no("a theme key's action is `a`-`z`, `0`-`9`, `.` and `-` "
                                         "-- `" +
                                         k.action + "` is not");
        }
    }
    if (const workshop::ParsedGesture parsed = workshop::parse_gesture(k.gesture);
        !parsed.accepted) {
        return workshop::Written::no("`" + k.action + "`: " + parsed.refusal);
    }
    return workshop::Written::ok();
}

/// A WHOLE THEME'S FORM: its name and summary, each list within its bound, every entry sound, no
/// include of itself or twice, no setting twice under one target, no key twice for one action.
inline workshop::Written check_theme_file(const ThemeFile& t) {
    if (const workshop::Written name = check_theme_name(t.name); !name.accepted) {
        return name;
    }
    if (t.summary.size() > kMaxThemeSummaryLen || !workshop::pane_setting_text_ok(t.summary)) {
        return workshop::Written::no("theme `" + t.name + "`: a summary is at most " +
                                     std::to_string(kMaxThemeSummaryLen) +
                                     " bytes of printable ASCII");
    }
    if (t.includes.size() > kMaxThemeIncludes) {
        return workshop::Written::no("theme `" + t.name + "` includes at most " +
                                     std::to_string(kMaxThemeIncludes) + " themes -- it lists " +
                                     std::to_string(t.includes.size()));
    }
    for (std::size_t i = 0; i < t.includes.size(); ++i) {
        const std::string& inc = t.includes[i];
        if (const workshop::Written name = check_theme_name(inc); !name.accepted) {
            return workshop::Written::no("theme `" + t.name + "`: " + name.refusal);
        }
        if (inc == t.name) {
            return workshop::Written::no("theme `" + t.name +
                                         "` includes itself -- theme cycle: " + t.name + " -> " +
                                         t.name);
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (t.includes[j] == inc) {
                return workshop::Written::no("theme `" + t.name + "` includes `" + inc +
                                             "` twice");
            }
        }
    }
    if (t.settings.size() > kMaxThemeSettings) {
        return workshop::Written::no("theme `" + t.name + "` holds at most " +
                                     std::to_string(kMaxThemeSettings) + " settings -- it holds " +
                                     std::to_string(t.settings.size()));
    }
    std::vector<std::pair<std::string_view, std::string_view>> places;
    places.reserve(t.settings.size());
    for (const ThemeSetting& s : t.settings) {
        if (const workshop::Written one = check_theme_setting(s); !one.accepted) {
            return workshop::Written::no("theme `" + t.name + "`: " + one.refusal);
        }
        places.emplace_back(s.target, s.key);
    }
    std::sort(places.begin(), places.end());
    for (std::size_t i = 1; i < places.size(); ++i) {
        if (places[i - 1] == places[i]) {
            return workshop::Written::no("theme `" + t.name + "` sets `" +
                                         std::string(places[i].second) + "` of `" +
                                         std::string(places[i].first) + "` twice");
        }
    }
    if (t.keys.size() > kMaxThemeKeys) {
        return workshop::Written::no("theme `" + t.name + "` holds at most " +
                                     std::to_string(kMaxThemeKeys) + " keys -- it holds " +
                                     std::to_string(t.keys.size()));
    }
    std::vector<std::string_view> actions;
    actions.reserve(t.keys.size());
    for (const ThemeKey& k : t.keys) {
        if (const workshop::Written one = check_theme_key(k); !one.accepted) {
            return workshop::Written::no("theme `" + t.name + "`: " + one.refusal);
        }
        actions.emplace_back(k.action);
    }
    std::sort(actions.begin(), actions.end());
    for (std::size_t i = 1; i < actions.size(); ++i) {
        if (actions[i - 1] == actions[i]) {
            return workshop::Written::no("theme `" + t.name + "` sets a key for `" +
                                         std::string(actions[i]) + "` twice");
        }
    }
    return workshop::Written::ok();
}

// ---- Text --------------------------------------------------------------------------------

/// The order a setting is kept in: by target, then key.
inline bool theme_setting_before(const ThemeSetting& a, const ThemeSetting& b) {
    return std::tie(a.target, a.key) < std::tie(b.target, b.key);
}

/// THE THEME AS IT IS WRITTEN: its own format and version, its settings in target and key order;
/// its includes and keys stay in the order authored, which is the order they apply in.
inline ThemeFile canonical_theme(ThemeFile t) {
    t.format = kThemeFormat;
    t.format_version = kThemeFormatVersion;
    std::stable_sort(t.settings.begin(), t.settings.end(), theme_setting_before);
    return t;
}

inline std::string to_text(ThemeFile t) {
    return loom::compat::serialize(loom::to_value(canonical_theme(std::move(t))));
}

/// What reading a theme produced: whether it worked, the theme, and the bytes it was read from
/// (what a rewrite must find still on disk).
struct LoadedTheme {
    workshop::Written outcome;
    ThemeFile theme;
    std::string raw_bytes;

    static LoadedTheme no(std::string why) {
        return LoadedTheme{workshop::Written::no(std::move(why)), ThemeFile{}, std::string()};
    }
};

/// One sentence for a version this build does not read, from the envelope and from the field.
inline std::string wrong_theme_version(std::int64_t found) {
    return "theme version " + std::to_string(found) + " -- this build reads version " +
           std::to_string(kThemeFormatVersion);
}

/// Text to a theme, every refusal opening with `label`. Total: every input is a theme or a
/// refusal with a reason, and nothing here throws. The envelope's claim is read first, so another
/// file or a later version is refused by what it says it is rather than by its first odd field.
inline LoadedTheme from_text(std::string_view text, std::string_view label = "theme") {
    const std::string at = std::string(label) + ": ";
    if (text.size() > kMaxThemeBytes) {
        return LoadedTheme::no(at + std::to_string(text.size()) +
                               " bytes is larger than a theme file can be");
    }
    const loom::Unverified claim = loom::compat::parse(text);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<ThemeFile>(), loom::Report::FirstError);
        return LoadedTheme::no(at + "not a theme file: " + refused.first_error().message());
    }
    if (claim.claimed_name() != std::string(ThemeFile::zen_name)) {
        return LoadedTheme::no(at + "not a theme file: it says it is `" + claim.claimed_name() +
                               "`");
    }
    if (claim.claimed_version() != ThemeFile::zen_version) {
        const auto found = static_cast<std::int64_t>(claim.claimed_version());
        return LoadedTheme::no(at + wrong_theme_version(found));
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<ThemeFile>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedTheme::no(at + admitted.first_error().message());
    }
    ThemeFile theme = loom::from_value<ThemeFile>(admitted.value());
    if (theme.format != kThemeFormat) {
        return LoadedTheme::no(at + "not a theme file: it says it is `" + theme.format + "`");
    }
    if (theme.format_version != kThemeFormatVersion) {
        return LoadedTheme::no(at + wrong_theme_version(theme.format_version));
    }
    if (const workshop::Written form = check_theme_file(theme); !form.accepted) {
        return LoadedTheme::no(at + form.refusal);
    }
    return LoadedTheme{workshop::Written::ok(), canonical_theme(std::move(theme)),
                       std::string(text)};
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
    if (const workshop::Written form = check_theme_name(name); !form.accepted) {
        return LoadedTheme::no(form.refusal);
    }
    const std::string path = theme_path(dir, name);
    const workshop::persist::FileText read =
        workshop::persist::read_file(path, kMaxThemeBytes, "a theme file");
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

/// WRITE A THEME under `dir` by the safe write, the directory made on the first write. A theme
/// already there is overwritten only with `replace`, and, given the bytes it was read from, only
/// while the file still holds them: a second hand that wrote it since is told, and nothing is
/// written.
inline workshop::Written
save_file(const std::string& dir, ThemeFile theme, bool replace = false,
          const std::optional<std::string>& expected_bytes = std::nullopt) {
    if (const workshop::Written form = check_theme_file(theme); !form.accepted) {
        return form;
    }
    const std::string path = theme_path(dir, theme.name);
    std::error_code ec;
    const bool there = std::filesystem::exists(path, ec);
    if (ec) {
        return workshop::Written::no("cannot tell whether " + path + " exists: " + ec.message() +
                                     " -- nothing was written");
    }
    if (there && !replace) {
        return workshop::Written::no("theme `" + theme.name +
                                     "` already exists -- pass replace to overwrite it");
    }
    if (there && expected_bytes.has_value()) {
        const workshop::persist::FileText current =
            workshop::persist::read_file(path, kMaxThemeBytes, "a theme file");
        if (!current.outcome.accepted || current.text != *expected_bytes) {
            return workshop::Written::no("`" + theme.name +
                                         "` changed on disk since it was read -- nothing was "
                                         "written");
        }
    }
    return workshop::persist::write_file_making_room(path, to_text(std::move(theme)));
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

    ZEN_SHAPE(ResolvedThemeKey, 1, ZEN_FIELD(key), ZEN_FIELD(from_theme));
    friend bool operator==(const ResolvedThemeKey&, const ResolvedThemeKey&) = default;
};

/// A THEME RESOLVED WITH ITS INCLUDES, or why not: the themes in the order each last applied, the
/// settings in target and key order (a clear kept, so applying it clears), the keys in the order
/// first set, and a fingerprint of those entries.
struct ThemeResolved {
    bool accepted = false;
    std::string refusal;
    std::string theme;
    std::string summary;
    std::vector<std::string> chain;
    std::vector<ResolvedThemeSetting> settings;
    std::vector<ResolvedThemeKey> keys;
    std::int64_t fingerprint = 0;

    ZEN_SHAPE(ThemeResolved, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal), ZEN_FIELD(theme),
              ZEN_FIELD(summary), ZEN_FIELD(chain), ZEN_FIELD(settings), ZEN_FIELD(keys),
              ZEN_FIELD(fingerprint));
    friend bool operator==(const ThemeResolved&, const ThemeResolved&) = default;
};

/// THE RESOLVED ENTRIES' FINGERPRINT: 64-bit FNV-1a over each setting (target, key, kind, value)
/// and each key (action, gesture), the same in every build; never 0, so a resolved theme always
/// carries one. Which theme gave an entry is not mixed in: the same values are the same theme.
inline std::int64_t theme_fingerprint(const std::vector<ResolvedThemeSetting>& settings,
                                      const std::vector<ResolvedThemeKey>& keys) {
    std::uint64_t h = 14695981039346656037ULL;
    const auto mix = [&h](std::string_view part) {
        for (const char c : part) {
            h ^= static_cast<unsigned char>(c);
            h *= 1099511628211ULL;
        }
    };
    const auto byte = [&mix](char c) { mix(std::string_view(&c, 1)); };
    for (const ResolvedThemeSetting& r : settings) {
        const ThemeSetting& s = r.setting;
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
    for (const ResolvedThemeKey& r : keys) {
        mix(r.key.action);
        byte('\0');
        mix(r.key.gesture);
        byte('\n');
    }
    const auto out = static_cast<std::int64_t>(h);
    return out == 0 ? 1 : out;
}

namespace detail {

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
        out.chain.erase(std::remove(out.chain.begin(), out.chain.end(), name), out.chain.end());
        out.chain.push_back(name);
        for (const ThemeSetting& s : loaded.theme.settings) {
            ResolvedThemeSetting entry{s, loaded.theme.name};
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
        for (const ThemeKey& k : loaded.theme.keys) {
            const auto at = std::find_if(out.keys.begin(), out.keys.end(),
                                         [&k](const ResolvedThemeKey& r) {
                                             return r.key.action == k.action;
                                         });
            if (at != out.keys.end()) {
                *at = ResolvedThemeKey{k, loaded.theme.name};
            } else {
                out.keys.push_back(ResolvedThemeKey{k, loaded.theme.name});
            }
        }
        if (out.settings.size() + out.keys.size() > kMaxResolvedEntries) {
            return refuse("resolving `" + (path.empty() ? name : path.front()) +
                          "` gives more than " + std::to_string(kMaxResolvedEntries) +
                          " entries");
        }
        return true;
    }
};

} // namespace detail

/// RESOLVE THEME `root_name` THROUGH `load` (a name to a `LoadedTheme`): each include in order,
/// depth first and each resolved whole, then the theme's own settings and keys; later wins per
/// target and key and per action. A cycle, an include nested past `kMaxThemeDepth`, more than
/// `kMaxThemesResolved` themes read, more than `kMaxResolvedEntries` entries, or any theme that
/// will not load refuses the whole resolution, naming it: never half resolved.
template <class Loader>
ThemeResolved resolve_theme_with(std::string_view root_name, Loader&& load) {
    if (const workshop::Written form = check_theme_name(root_name); !form.accepted) {
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
    out.fingerprint = theme_fingerprint(out.settings, out.keys);
    return out;
}

/// ...with every theme read from `dir`.
inline ThemeResolved resolve_theme(const std::string& dir, std::string_view root_name) {
    return resolve_theme_with(root_name,
                              [&dir](std::string_view name) { return load_file(dir, name); });
}

// ---- The library -------------------------------------------------------------------------

/// ONE FILE IN THE THEMES FOLDER: its theme's name, summary, includes and how many settings and
/// keys it holds -- or the file's stem and why it would not load.
struct ThemeSummary {
    std::string name;
    std::string summary;
    std::vector<std::string> includes;
    std::int64_t settings = 0;
    std::int64_t keys = 0;
    std::string refusal;

    ZEN_SHAPE(ThemeSummary, 1, ZEN_FIELD(name), ZEN_FIELD(summary), ZEN_FIELD(includes),
              ZEN_FIELD(settings), ZEN_FIELD(keys), ZEN_FIELD(refusal));
    friend bool operator==(const ThemeSummary&, const ThemeSummary&) = default;
};

/// EVERY `<stem>.json` IN `dir`, by name: a theme that loads is summarised, and one that does not
/// is listed by its stem with its refusal, so a broken file is seen rather than lost. A folder
/// that is not there holds none; a write's `.saving` candidate is no theme.
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
        const std::string stem = it->path().stem().string();
        const LoadedTheme loaded = load_file(dir, stem);
        if (loaded.outcome.accepted) {
            out.push_back(ThemeSummary{loaded.theme.name, loaded.theme.summary,
                                       loaded.theme.includes,
                                       static_cast<std::int64_t>(loaded.theme.settings.size()),
                                       static_cast<std::int64_t>(loaded.theme.keys.size()),
                                       std::string()});
        } else {
            out.push_back(ThemeSummary{stem, std::string(), {}, 0, 0, loaded.outcome.refusal});
        }
    }
    std::sort(out.begin(), out.end(),
              [](const ThemeSummary& a, const ThemeSummary& b) { return a.name < b.name; });
    return out;
}

} // namespace zengine::settings

#endif // ZENGINE_SETTINGS_THEME_PERSIST_HPP
