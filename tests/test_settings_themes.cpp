// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE SETTINGS THEMES SUITE: a theme's form, its file read and written on a real disk, the guard
// on a rewrite, resolution with its includes, the folder's listing, and every settings shape's
// round trip through its text.

#include "doctest.h"

#include "settings/theme_persist.hpp"
#include "settings/vocabulary.hpp"
#include "workshop/prefs_persist.hpp" // another file of the family, for its envelope

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace st = zengine::settings;

namespace {

/// A directory of this suite's own, removed with it.
class TempDir {
public:
    explicit TempDir(const char* tag) {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("zen-settings-themes-" + std::string(tag) + "-" + std::to_string(++counter));
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_, ec);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    std::string dir() const { return path_.generic_string(); }

private:
    std::filesystem::path path_;
};

std::string read_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void write_bytes(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

st::ThemeSetting flag(std::string target, std::string key, bool v) {
    st::ThemeSetting s;
    s.target = std::move(target);
    s.key = std::move(key);
    s.flag = v;
    return s;
}

st::ThemeSetting number(std::string target, std::string key, std::int64_t v) {
    st::ThemeSetting s;
    s.target = std::move(target);
    s.key = std::move(key);
    s.number = v;
    return s;
}

st::ThemeSetting text(std::string target, std::string key, std::string v) {
    st::ThemeSetting s;
    s.target = std::move(target);
    s.key = std::move(key);
    s.text = std::move(v);
    return s;
}

st::ThemeSetting cleared(std::string target, std::string key) {
    st::ThemeSetting s;
    s.target = std::move(target);
    s.key = std::move(key);
    s.clear = true;
    return s;
}

st::ThemeFile theme(std::string name, std::vector<std::string> includes = {},
                    std::vector<st::ThemeSetting> settings = {},
                    std::vector<st::ThemeKey> keys = {}) {
    st::ThemeFile t;
    t.name = std::move(name);
    t.includes = std::move(includes);
    t.settings = std::move(settings);
    t.keys = std::move(keys);
    return t;
}

/// Save a theme as a new file, and require that it was written.
void put(const TempDir& d, st::ThemeFile t) {
    const std::string name = t.name;
    const zengine::workshop::Written w = st::save_file(d.dir(), std::move(t));
    REQUIRE_MESSAGE(w.accepted, name << ": " << w.refusal);
}

/// The text without its canonical order: a hand's file, settings as written.
std::string as_written(st::ThemeFile t) {
    t.format = st::kThemeFormat;
    t.format_version = st::kThemeFormatVersion;
    return loom::compat::serialize(loom::to_value(t));
}

std::string replaced(std::string text, std::string_view from, std::string_view to) {
    const std::size_t at = text.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, "`" << from << "` is not in " << text);
    text.replace(at, from.size(), to);
    return text;
}

bool says(const std::string& refusal, std::string_view part) {
    return refusal.find(part) != std::string::npos;
}

const st::ResolvedThemeSetting* entry(const st::ThemeResolved& r, std::string_view target,
                                      std::string_view key) {
    for (const st::ResolvedThemeSetting& s : r.settings) {
        if (s.setting.target == target && s.setting.key == key) {
            return &s;
        }
    }
    return nullptr;
}

template <class T> T round_trip(const T& v) {
    const std::string text = loom::compat::serialize(loom::to_value(v));
    const loom::Admission admitted =
        loom::admit(loom::compat::parse(text), loom::schema_of<T>(), loom::Report::FirstError);
    REQUIRE_MESSAGE(admitted.ok(), T::zen_name << ": " << admitted.first_error().message());
    return loom::from_value<T>(admitted.value());
}

} // namespace

TEST_CASE("a theme's name is 1 to 32 bytes of lowercase letters, digits and dashes") {
    // ⚔ MUTATION: a name admitting `.` or `/` would let a theme's file name walk out of its folder.
    CHECK(st::check_theme_name("solarized-dark").accepted);
    CHECK(st::check_theme_name("a").accepted);
    CHECK(st::check_theme_name(std::string(st::kMaxThemeNameLen, 'x')).accepted);
    CHECK(says(st::check_theme_name("").refusal, "cannot be empty"));
    CHECK(says(st::check_theme_name(std::string(st::kMaxThemeNameLen + 1, 'x')).refusal,
               "at most 32 bytes"));
    for (const char* bad : {"Dark", "dark.theme", "../dark", "a/b", "a\\b", "dark theme", "dark_x"}) {
        CAPTURE(bad);
        CHECK(says(st::check_theme_name(bad).refusal, "is not"));
    }
}

TEST_CASE("a theme setting's target is every pane, an office, or one pane of an office") {
    for (const char* good : {"*", "zengine.workshop", "zengine.skin", "zengine.editor/editor",
                             "zengine.terminal/terminal"}) {
        CAPTURE(good);
        CHECK(st::check_theme_target(good).accepted);
    }
    // ⚔ MUTATION: an office with no `.`, a second `/`, or a pane of `*` taken as a target.
    for (const char* bad : {"foo", "a/b/c", "zengine.editor/*", "*/editor", "zengine.editor/",
                            "/editor", "foo/editor", "**"}) {
        CAPTURE(bad);
        CHECK(says(st::check_theme_target(bad).refusal, "is `*`, `<office>` or"));
    }
    CHECK(says(st::check_theme_target("").refusal, "1 to 128 bytes"));
    CHECK(says(st::check_theme_target("zengine." + std::string(st::kMaxThemeTargetLen, 'x')).refusal,
               "1 to 128 bytes"));
    CHECK(says(st::check_theme_target("zengine.editor/my editor").refusal, "no space"));
}

TEST_CASE("a theme setting holds exactly one value, or is a clear holding none") {
    CHECK(st::check_theme_setting(number("*", "wheel-rows", 5)).accepted);
    CHECK(st::check_theme_setting(flag("zengine.terminal/terminal", "legend", false)).accepted);
    CHECK(st::check_theme_setting(text("zengine.workshop", "workshop.skin", "block")).accepted);
    // Workshop's own keys are a theme's to set, under its office, a pane or every pane.
    CHECK(st::check_theme_setting(text("*", "workshop.skin", "classic")).accepted);
    CHECK(st::check_theme_setting(cleared("zengine.skin", "window.face")).accepted);

    st::ThemeSetting none = number("*", "wheel-rows", 1);
    none.number.reset();
    CHECK(says(st::check_theme_setting(none).refusal, "holds no value"));
    st::ThemeSetting two = number("*", "wheel-rows", 1);
    two.flag = true;
    CHECK(says(st::check_theme_setting(two).refusal, "holds more than one value"));
    // ⚔ MUTATION: a clear that also carries a value, admitted.
    st::ThemeSetting clear_and_value = number("*", "wheel-rows", 1);
    clear_and_value.clear = true;
    CHECK(says(st::check_theme_setting(clear_and_value).refusal, "a clear holds no value"));
    CHECK(says(st::check_theme_setting(number("*", "Wheel", 1)).refusal, "a setting's key"));
    CHECK(says(st::check_theme_setting(number("foo", "wheel-rows", 1)).refusal, "target"));
    CHECK(says(st::check_theme_setting(text("*", "label", std::string(65, 'x'))).refusal,
               "a text past 64 bytes"));
    CHECK(says(st::check_theme_setting(text("*", "label", "tab\there")).refusal, "printable"));
}

TEST_CASE("a theme key names an action and a gesture the keymap reads, none among them") {
    CHECK(st::check_theme_key(st::ThemeKey{"focus.left", "h"}).accepted);
    CHECK(st::check_theme_key(st::ThemeKey{"layout.next", "ctrl+shift+n"}).accepted);
    CHECK(st::check_theme_key(st::ThemeKey{"workshop.quit", "none"}).accepted);
    CHECK(says(st::check_theme_key(st::ThemeKey{"focus.left", "hyper+h"}).refusal,
               "is not a modifier"));
    CHECK(says(st::check_theme_key(st::ThemeKey{"focus.left", ""}).refusal, "cannot be empty"));
    CHECK(says(st::check_theme_key(st::ThemeKey{"Focus.Left", "h"}).refusal, "is not"));
    CHECK(says(st::check_theme_key(st::ThemeKey{"", "h"}).refusal, "1 to 64 bytes"));
    CHECK(says(st::check_theme_key(st::ThemeKey{std::string(65, 'a'), "h"}).refusal,
               "1 to 64 bytes"));
}

TEST_CASE("a theme file sets nothing twice, includes nothing twice or itself, and keeps its bounds") {
    CHECK(st::check_theme_file(theme("evening", {"dark", "compact"},
                                     {number("*", "wheel-rows", 5),
                                      number("zengine.skin", "wheel-rows", 3)},
                                     {{"focus.left", "h"}, {"focus.right", "l"}}))
              .accepted);
    // ⚔ MUTATION: the duplicate scan dropped -- one target and key written twice, by one file.
    CHECK(says(st::check_theme_file(theme("t", {}, {number("*", "wheel-rows", 5),
                                                    number("*", "wheel-rows", 6)}))
                   .refusal,
               "sets `wheel-rows` of `*` twice"));
    CHECK(says(st::check_theme_file(theme("t", {}, {}, {{"focus.left", "h"}, {"focus.left", "j"}}))
                   .refusal,
               "sets a key for `focus.left` twice"));
    CHECK(says(st::check_theme_file(theme("t", {"dark", "dark"})).refusal, "includes `dark` twice"));
    CHECK(says(st::check_theme_file(theme("t", {"t"})).refusal, "theme cycle: t -> t"));
    CHECK(says(st::check_theme_file(theme("t", {"Dark"})).refusal, "a theme's name"));
    st::ThemeFile summary = theme("t");
    summary.summary = std::string(st::kMaxThemeSummaryLen + 1, 's');
    CHECK(says(st::check_theme_file(summary).refusal, "a summary is at most 64 bytes"));

    std::vector<std::string> includes;
    for (std::size_t i = 0; i <= st::kMaxThemeIncludes; ++i) {
        includes.push_back("i" + std::to_string(i));
    }
    CHECK(says(st::check_theme_file(theme("t", includes)).refusal, "includes at most 8 themes"));
    std::vector<st::ThemeSetting> settings;
    for (std::size_t i = 0; i <= st::kMaxThemeSettings; ++i) {
        settings.push_back(number("*", "k" + std::to_string(i), 1));
    }
    CHECK(says(st::check_theme_file(theme("t", {}, settings)).refusal, "at most 512 settings"));
    settings.pop_back();
    CHECK(st::check_theme_file(theme("t", {}, settings)).accepted);
    std::vector<st::ThemeKey> keys;
    for (std::size_t i = 0; i <= st::kMaxThemeKeys; ++i) {
        keys.push_back(st::ThemeKey{"a" + std::to_string(i), "none"});
    }
    CHECK(says(st::check_theme_file(theme("t", {}, {}, keys)).refusal, "at most 128 keys"));
}

TEST_CASE("a theme reads back from its text with its settings in target and key order") {
    st::ThemeFile t = theme("evening", {"dark", "compact"},
                            {text("zengine.workshop", "workshop.skin", "block"),
                             flag("zengine.terminal/terminal", "legend", false),
                             number("*", "wheel-rows", 5), cleared("zengine.skin", "window.face")},
                            {{"focus.right", "l"}, {"focus.left", "h"}});
    t.summary = "a dark palette and vim movement";

    // A hand's file, its settings out of order, reads in order; the bytes are kept as read.
    const std::string hand = as_written(t);
    const st::LoadedTheme loaded = st::from_text(hand);
    REQUIRE_MESSAGE(loaded.outcome.accepted, loaded.outcome.refusal);
    CHECK(loaded.raw_bytes == hand);
    REQUIRE(loaded.theme.settings.size() == 4);
    CHECK(loaded.theme.settings[0].target == "*");
    CHECK(loaded.theme.settings[1].target == "zengine.skin");
    CHECK(loaded.theme.settings[1].clear);
    CHECK(loaded.theme.settings[2].target == "zengine.terminal/terminal");
    CHECK(loaded.theme.settings[3].target == "zengine.workshop");
    // Includes and keys keep the order they apply in.
    CHECK(loaded.theme.includes == std::vector<std::string>{"dark", "compact"});
    REQUIRE(loaded.theme.keys.size() == 2);
    CHECK(loaded.theme.keys[0].action == "focus.right");

    // The written text is the canonical one, and reads back as the same theme.
    const std::string written = st::to_text(t);
    CHECK(written != hand);
    const st::LoadedTheme again = st::from_text(written);
    REQUIRE(again.outcome.accepted);
    CHECK(again.theme == loaded.theme);
    CHECK(again.theme == st::canonical_theme(t));
    CHECK(st::to_text(again.theme) == written);
}

TEST_CASE("a theme file is refused by what its envelope says it is before its fields are judged") {
    const std::string good = st::to_text(theme("evening", {}, {number("*", "wheel-rows", 5)}));
    REQUIRE(st::from_text(good).outcome.accepted);

    // Another file of the family is named by what it is, not by its first odd field.
    zengine::workshop::prefs_persist::Prefs prefs;
    const st::LoadedTheme other =
        st::from_text(zengine::workshop::prefs_persist::to_text(prefs), "x.json");
    CHECK(says(other.outcome.refusal, "x.json: not a theme file: it says it is `WorkshopPrefs`"));
    // ⚔ MUTATION: the envelope's version left to admission -- the renamed field would be named
    // where the version that renamed it should be.
    const std::string later = replaced(replaced(good, "\"version\":1", "\"version\":2"),
                                       "\"summary\"", "\"blurb\"");
    CHECK(says(st::from_text(later).outcome.refusal, "theme version 2 -- this build reads version 1"));

    const st::LoadedTheme format =
        st::from_text(replaced(good, "\"zengine-theme\"", "\"zengine-workshop-prefs\""));
    CHECK(says(format.outcome.refusal, "not a theme file: it says it is `zengine-workshop-prefs`"));
    const st::LoadedTheme version =
        st::from_text(replaced(good, "\"format_version\":\"1\"", "\"format_version\":\"2\""));
    CHECK(says(version.outcome.refusal, "theme version 2"));
    CHECK(says(st::from_text("{not json").outcome.refusal, "theme: not a theme file"));
    CHECK(says(st::from_text(replaced(good, "\"key\":\"wheel-rows\"", "\"key\":\"Wheel\""))
                   .outcome.refusal,
               "a setting's key"));
}

TEST_CASE("a theme file past its ceiling is refused before it is parsed, read or written") {
    const std::string big(static_cast<std::size_t>(st::kMaxThemeBytes) + 1, ' ');
    CHECK(says(st::from_text(big).outcome.refusal, "65537 bytes is larger than a theme file can be"));
    TempDir d("ceiling");
    write_bytes(st::theme_path(d.dir(), "big"), big);
    // ⚔ MUTATION: the read given no ceiling -- a hostile file chooses the cost of its refusal.
    CHECK(says(st::load_file(d.dir(), "big").outcome.refusal, "larger than a theme file can be"));
}

TEST_CASE("a theme file holding another name than its own is refused") {
    TempDir d("names");
    write_bytes(st::theme_path(d.dir(), "dark"), st::to_text(theme("light")));
    const st::LoadedTheme loaded = st::load_file(d.dir(), "dark");
    CHECK_FALSE(loaded.outcome.accepted);
    CHECK(says(loaded.outcome.refusal, "theme name `light` does not match file name `dark`"));
    CHECK(says(st::load_file(d.dir(), "../dark").outcome.refusal, "a theme's name"));
    CHECK(says(st::load_file(d.dir(), "absent").outcome.refusal, "cannot read"));
}

TEST_CASE("a theme is written new, and over itself only when asked and only while it holds what was read") {
    TempDir d("two-hands");
    const std::string folder = d.dir() + "/" + st::kThemesSubdir;
    const std::string path = st::theme_path(folder, "evening");

    // New, into a folder made on the first write; then read back as written.
    REQUIRE(st::save_file(folder, theme("evening", {}, {number("*", "wheel-rows", 5)})).accepted);
    const st::LoadedTheme first = st::load_file(folder, "evening");
    REQUIRE(first.outcome.accepted);
    CHECK(first.raw_bytes == read_bytes(path));

    // Over an existing theme only with `replace`.
    const zengine::workshop::Written refused =
        st::save_file(folder, theme("evening", {}, {number("*", "wheel-rows", 6)}));
    CHECK(says(refused.refusal, "theme `evening` already exists -- pass replace to overwrite it"));
    CHECK(read_bytes(path) == first.raw_bytes);

    // ...and with the bytes it was read from, only while the file still holds them.
    REQUIRE(st::save_file(folder, theme("evening", {}, {number("*", "wheel-rows", 7)}), true,
                          first.raw_bytes)
                .accepted);
    const std::string second = read_bytes(path);
    CHECK(second != first.raw_bytes);
    REQUIRE(st::load_file(folder, "evening").theme.settings.at(0).number == 7);
    // ⚔ MUTATION: the guard skipped -- a second hand's write lost to a stale reader.
    const zengine::workshop::Written stale = st::save_file(
        folder, theme("evening", {}, {number("*", "wheel-rows", 8)}), true, first.raw_bytes);
    CHECK(says(stale.refusal, "`evening` changed on disk since it was read -- nothing was written"));
    CHECK(read_bytes(path) == second);

    // An unsound theme writes nothing.
    CHECK_FALSE(st::save_file(folder, theme("Bad")).accepted);
    CHECK_FALSE(std::filesystem::exists(st::theme_path(folder, "Bad")));
    CHECK_FALSE(std::filesystem::exists(path + ".saving"));
}

TEST_CASE("a theme alone resolves to its own entries, with a fingerprint that is never zero") {
    TempDir d("alone");
    put(d, theme("plain", {}, {number("*", "wheel-rows", 5), flag("zengine.terminal/terminal",
                                                                  "legend", true)},
                 {{"focus.left", "h"}}));
    const st::ThemeResolved r = st::resolve_theme(d.dir(), "plain");
    REQUIRE_MESSAGE(r.accepted, r.refusal);
    CHECK(r.theme == "plain");
    CHECK(r.chain == std::vector<std::string>{"plain"});
    REQUIRE(r.settings.size() == 2);
    CHECK(r.settings[0].from_theme == "plain");
    REQUIRE(r.keys.size() == 1);
    CHECK(r.keys[0].from_theme == "plain");
    CHECK(r.fingerprint != 0);
    CHECK(st::resolve_theme(d.dir(), "plain").fingerprint == r.fingerprint);

    // The same values give the same fingerprint whichever theme gave them; another value does not.
    put(d, theme("wrapper", {"plain"}));
    const st::ThemeResolved wrapped = st::resolve_theme(d.dir(), "wrapper");
    REQUIRE(wrapped.accepted);
    CHECK(wrapped.fingerprint == r.fingerprint);
    // ⚔ MUTATION: the giving theme mixed in -- a twin of the same values would read as changed.
    put(d, theme("twin", {}, {number("*", "wheel-rows", 5), flag("zengine.terminal/terminal",
                                                                 "legend", true)},
                 {{"focus.left", "h"}}));
    const st::ThemeResolved twin = st::resolve_theme(d.dir(), "twin");
    REQUIRE(twin.accepted);
    CHECK(twin.settings[0].from_theme == "twin");
    CHECK(twin.fingerprint == r.fingerprint);
    put(d, theme("moved", {"plain"}, {number("*", "wheel-rows", 6)}));
    CHECK(st::resolve_theme(d.dir(), "moved").fingerprint != r.fingerprint);
    put(d, theme("cleared", {"plain"}, {cleared("*", "wheel-rows")}));
    CHECK(st::resolve_theme(d.dir(), "cleared").fingerprint != r.fingerprint);
    CHECK(st::theme_fingerprint({}, {}) != 0);
}

TEST_CASE("a theme's includes apply first and in order, and later wins, a clear among them") {
    TempDir d("layers");
    put(d, theme("base", {},
                 {number("*", "wheel-rows", 3), number("zengine.skin", "window.face", 10),
                  flag("zengine.terminal/terminal", "legend", true)},
                 {{"focus.left", "h"}, {"focus.right", "l"}}));
    put(d, theme("dark", {"base"},
                 {number("zengine.skin", "window.face", 15),
                  cleared("zengine.terminal/terminal", "legend")}));
    put(d, theme("solarized-dark", {"dark"},
                 {number("*", "wheel-rows", 5), text("zengine.workshop", "workshop.skin", "block")},
                 {{"focus.left", "ctrl+h"}}));
    const st::ThemeResolved r = st::resolve_theme(d.dir(), "solarized-dark");
    REQUIRE_MESSAGE(r.accepted, r.refusal);
    CHECK(r.chain == std::vector<std::string>{"base", "dark", "solarized-dark"});
    REQUIRE(r.settings.size() == 4);
    // In target and key order, each naming the theme that gave it.
    CHECK(r.settings[0].setting == number("*", "wheel-rows", 5));
    CHECK(r.settings[0].from_theme == "solarized-dark");
    CHECK(r.settings[1].setting == number("zengine.skin", "window.face", 15));
    CHECK(r.settings[1].from_theme == "dark");
    // ⚔ MUTATION: a clear dropped from the resolution -- applying it would leave base's value.
    CHECK(r.settings[2].setting == cleared("zengine.terminal/terminal", "legend"));
    CHECK(r.settings[2].from_theme == "dark");
    CHECK(r.settings[3].from_theme == "solarized-dark");
    // A key keeps the place it was first set in, and takes the later gesture.
    REQUIRE(r.keys.size() == 2);
    CHECK(r.keys[0].key == st::ThemeKey{"focus.left", "ctrl+h"});
    CHECK(r.keys[0].from_theme == "solarized-dark");
    CHECK(r.keys[1].key == st::ThemeKey{"focus.right", "l"});
    CHECK(r.keys[1].from_theme == "base");

    // Of two includes, the later wins.
    put(d, theme("left", {}, {number("*", "wheel-rows", 1)}));
    put(d, theme("right", {}, {number("*", "wheel-rows", 2)}));
    put(d, theme("both", {"left", "right"}));
    const st::ThemeResolved both = st::resolve_theme(d.dir(), "both");
    REQUIRE(both.accepted);
    REQUIRE(entry(both, "*", "wheel-rows") != nullptr);
    CHECK(entry(both, "*", "wheel-rows")->setting.number == 2);
    CHECK(entry(both, "*", "wheel-rows")->from_theme == "right");
}

TEST_CASE("a theme included by two paths is no cycle, and its later application wins") {
    TempDir d("diamond");
    put(d, theme("common", {}, {number("*", "wheel-rows", 1)}, {{"focus.left", "h"}}));
    put(d, theme("b", {"common"}, {number("*", "wheel-rows", 2)}, {{"focus.left", "j"}}));
    put(d, theme("c", {"common"}, {number("zengine.skin", "window.face", 3)}));
    put(d, theme("a", {"b", "c"}));
    const st::ThemeResolved r = st::resolve_theme(d.dir(), "a");
    REQUIRE_MESSAGE(r.accepted, r.refusal);
    // Depth first and in order: common, b, common again, c, a. The chain names each theme once,
    // where it last applied, so a theme later in it wins over one before it.
    CHECK(r.chain == std::vector<std::string>{"b", "common", "c", "a"});
    REQUIRE(entry(r, "*", "wheel-rows") != nullptr);
    CHECK(entry(r, "*", "wheel-rows")->setting.number == 1);
    CHECK(entry(r, "*", "wheel-rows")->from_theme == "common");
    REQUIRE(entry(r, "zengine.skin", "window.face") != nullptr);
    CHECK(entry(r, "zengine.skin", "window.face")->from_theme == "c");
    REQUIRE(r.keys.size() == 1);
    CHECK(r.keys[0].key.gesture == "h");
    CHECK(r.keys[0].from_theme == "common");
}

TEST_CASE("a cycle refuses the whole resolution, naming its path") {
    TempDir d("cycle");
    put(d, theme("x", {}, {number("zengine.skin", "window.face", 4)}));
    put(d, theme("a", {"x", "b"}, {number("*", "wheel-rows", 1)}));
    put(d, theme("b", {"c"}));
    put(d, theme("c", {"a"}));
    const st::ThemeResolved r = st::resolve_theme(d.dir(), "a");
    CHECK_FALSE(r.accepted);
    // ⚔ MUTATION: the open path not consulted -- the walk would run until a bound refused it, and
    // the refusal would name the bound rather than the cycle.
    CHECK(r.refusal == "theme cycle: a -> b -> c -> a");
    // Never half resolved: what `x` gave before the cycle was met is not answered.
    CHECK(r.settings.empty());
    CHECK(r.chain.empty());
    CHECK(r.fingerprint == 0);
    const st::ThemeResolved from_b = st::resolve_theme(d.dir(), "b");
    CHECK(from_b.refusal == "theme cycle: b -> c -> a -> b");
    CHECK(from_b.settings.empty());

    // A theme including itself is refused as it is read; a loader that skips the form check
    // meets the same cycle on the path.
    write_bytes(st::theme_path(d.dir(), "self"), as_written(theme("self", {"self"})));
    CHECK(says(st::resolve_theme(d.dir(), "self").refusal, "theme cycle: self -> self"));
    const st::ThemeResolved unchecked = st::resolve_theme_with("self", [](std::string_view name) {
        return st::LoadedTheme{zengine::workshop::Written::ok(), theme(std::string(name), {"self"}),
                               std::string()};
    });
    CHECK(unchecked.refusal == "theme cycle: self -> self");
}

TEST_CASE("a resolution past its depth, its themes read or its entries is refused whole, naming the bound") {
    TempDir d("bounds");
    // Depth: the theme asked for, then four includes below it, and no fifth.
    for (int i = 0; i < 6; ++i) {
        put(d, theme("deep" + std::to_string(i),
                     i < 5 ? std::vector<std::string>{"deep" + std::to_string(i + 1)}
                           : std::vector<std::string>{}));
    }
    CHECK(st::resolve_theme(d.dir(), "deep1").accepted);
    const st::ThemeResolved deep = st::resolve_theme(d.dir(), "deep0");
    CHECK_FALSE(deep.accepted);
    CHECK(says(deep.refusal, "theme `deep5` is included more than 4 deep: deep0 -> deep1 -> "
                             "deep2 -> deep3 -> deep4 -> deep5"));

    // Themes read: the root, eight includes and seven below them is sixteen; one more is not.
    std::vector<std::string> wide;
    for (int i = 0; i < 8; ++i) {
        const std::string mid = "mid" + std::to_string(i);
        wide.push_back(mid);
        put(d, theme(mid, i < 7 ? std::vector<std::string>{"leaf" + std::to_string(i)}
                                : std::vector<std::string>{}));
        put(d, theme("leaf" + std::to_string(i)));
    }
    put(d, theme("wide", wide));
    const st::ThemeResolved sixteen = st::resolve_theme(d.dir(), "wide");
    CHECK_MESSAGE(sixteen.accepted, sixteen.refusal);
    CHECK(sixteen.chain.size() == st::kMaxThemesResolved);
    put(d, theme("tip", {"leaf7"}));
    wide.back() = "tip";
    put(d, theme("wider", wide));
    // ⚔ MUTATION: the count of themes read dropped -- a wide include tree reads without end.
    const st::ThemeResolved seventeen = st::resolve_theme(d.dir(), "wider");
    CHECK_FALSE(seventeen.accepted);
    CHECK(seventeen.refusal == "resolving `wider` reads more than 16 themes");

    // Entries: two themes of 512 settings each resolve to 1024; one entry more is refused, a key
    // counted as an entry.
    std::vector<st::ThemeSetting> left;
    std::vector<st::ThemeSetting> right;
    for (std::size_t i = 0; i < st::kMaxThemeSettings; ++i) {
        left.push_back(number("*", "l" + std::to_string(i), 1));
        right.push_back(number("*", "r" + std::to_string(i), 1));
    }
    put(d, theme("left", {}, left));
    put(d, theme("right", {}, right));
    put(d, theme("full", {"left", "right"}));
    const st::ThemeResolved full = st::resolve_theme(d.dir(), "full");
    CHECK_MESSAGE(full.accepted, full.refusal);
    CHECK(full.settings.size() == st::kMaxResolvedEntries);
    put(d, theme("over", {"left", "right"}, {number("*", "one-more", 1)}));
    CHECK(st::resolve_theme(d.dir(), "over").refusal ==
          "resolving `over` gives more than 1024 entries");
    put(d, theme("over-key", {"left", "right"}, {}, {{"focus.left", "h"}}));
    const st::ThemeResolved over_key = st::resolve_theme(d.dir(), "over-key");
    CHECK_FALSE(over_key.accepted);
    CHECK(over_key.settings.empty());
}

TEST_CASE("a missing or broken include refuses the whole resolution, naming the theme that includes it") {
    TempDir d("broken");
    put(d, theme("evening", {"dark", "absent"}, {number("*", "wheel-rows", 5)}));
    put(d, theme("dark", {}, {number("zengine.skin", "window.face", 15)}));
    const st::ThemeResolved missing = st::resolve_theme(d.dir(), "evening");
    CHECK_FALSE(missing.accepted);
    CHECK(says(missing.refusal, "in theme `evening` include `absent`: cannot read"));
    // Never half resolved: what `dark` gave before `absent` failed is not answered.
    CHECK(missing.settings.empty());
    CHECK(missing.chain.empty());

    put(d, theme("night", {"garbled"}));
    write_bytes(st::theme_path(d.dir(), "garbled"), "{\"zen\":1,");
    CHECK(says(st::resolve_theme(d.dir(), "night").refusal,
               "in theme `night` include `garbled`: "));
    CHECK(says(st::resolve_theme(d.dir(), "night").refusal, "not a theme file"));
    put(d, theme("dusk", {"lies"}));
    write_bytes(st::theme_path(d.dir(), "lies"), st::to_text(theme("truth")));
    CHECK(says(st::resolve_theme(d.dir(), "dusk").refusal, "does not match file name `lies`"));
    CHECK(says(st::resolve_theme(d.dir(), "absent").refusal, "cannot read"));
    CHECK(says(st::resolve_theme(d.dir(), "../dark").refusal, "a theme's name"));
}

TEST_CASE("a folder's themes are listed by name, a broken file with its refusal") {
    TempDir d("library");
    put(d, theme("zebra", {}, {number("*", "wheel-rows", 5)}, {{"focus.left", "h"}}));
    st::ThemeFile evening = theme("evening", {"zebra"},
                                  {number("*", "wheel-rows", 6), flag("*", "legend", true)});
    evening.summary = "the evening";
    put(d, evening);
    write_bytes(st::theme_path(d.dir(), "broken"), "not a theme");
    write_bytes(d.dir() + "/zebra.json.saving", "a write's candidate");
    write_bytes(d.dir() + "/notes.txt", "not listed");
    std::filesystem::create_directories(d.dir() + "/folder.json");

    const std::vector<st::ThemeSummary> listed = st::list_themes(d.dir());
    REQUIRE(listed.size() == 3);
    CHECK(listed[0].name == "broken");
    // ⚔ MUTATION: a file that does not load left out -- the weaver could never see why.
    CHECK(says(listed[0].refusal, "not a theme file"));
    CHECK(listed[1] == st::ThemeSummary{"evening", "the evening", {"zebra"}, 2, 0, ""});
    CHECK(listed[2] == st::ThemeSummary{"zebra", "", {}, 1, 1, ""});
    CHECK(st::list_themes(d.dir() + "/absent").empty());
}

TEST_CASE("every settings shape reads back from its text as it was") {
    const st::ThemeSetting setting = text("zengine.workshop", "workshop.skin", "block");
    const st::ThemeKey key{"focus.left", "ctrl+h"};
    st::ThemeFile file = theme("evening", {"dark"}, {setting, cleared("*", "legend")}, {key});
    file.format = st::kThemeFormat;
    file.format_version = st::kThemeFormatVersion;
    file.summary = "a summary";
    const st::ResolvedThemeSetting resolved_setting{setting, "dark"};
    const st::ResolvedThemeKey resolved_key{key, "evening"};
    const st::ThemeResolved resolved{true, "", "evening", "a summary", {"dark", "evening"},
                                     {resolved_setting}, {resolved_key}, -42};
    const st::ThemeSummary summary{"evening", "a summary", {"dark"}, 2, 1, ""};
    const st::ThemeSkip skip{2, "*", "legend", "locked in this layout"};

    CHECK(round_trip(setting) == setting);
    CHECK(round_trip(number("*", "wheel-rows", -9)) == number("*", "wheel-rows", -9));
    CHECK(round_trip(flag("*", "legend", false)) == flag("*", "legend", false));
    CHECK(round_trip(key) == key);
    CHECK(round_trip(file) == file);
    CHECK(round_trip(resolved_setting) == resolved_setting);
    CHECK(round_trip(resolved_key) == resolved_key);
    CHECK(round_trip(resolved) == resolved);
    CHECK(round_trip(summary) == summary);
    const st::ThemeApplyRequested apply{"evening", {0, 2}, false, {"layout"},
                                        st::kApplyKeepMine, true};
    CHECK(round_trip(apply) == apply);
    CHECK(round_trip(skip) == skip);
    const st::ThemeActed acted{true, "", "evening", 3, 1, {skip}, 2, true};
    CHECK(round_trip(acted) == acted);
    const st::ThemeSaveRequested save{file, true};
    CHECK(round_trip(save) == save);
    const st::ThemeSnapshotRequested snapshot{1, "snap", true};
    CHECK(round_trip(snapshot) == snapshot);
    const st::ThemePromoteRequested promote{1, "picked", {setting}, true};
    CHECK(round_trip(promote) == promote);
    const st::ThemeSaved saved{false, "already exists", "evening", "themes/evening.json"};
    CHECK(round_trip(saved) == saved);
    CHECK(round_trip(st::ThemesRequested{}) == st::ThemesRequested{});
    const st::ThemesShown shown{{summary, st::ThemeSummary{"broken", "", {}, 0, 0, "why"}}};
    CHECK(round_trip(shown) == shown);
    const st::ThemeResolveRequested resolve{"evening"};
    CHECK(round_trip(resolve) == resolve);
    const st::ThemeProvenanceRequested provenance{3};
    CHECK(round_trip(provenance) == provenance);
    const st::ThemeProvenanceEntry from{"*", "legend", "on", "dark", true};
    CHECK(round_trip(from) == from);
    const st::ThemeProvenanceShown provenance_shown{true, "", 3, "evening", "current", {from}};
    CHECK(round_trip(provenance_shown) == provenance_shown);
    CHECK(std::string(st::kSettingsRole) == "zengine.settings");
}
