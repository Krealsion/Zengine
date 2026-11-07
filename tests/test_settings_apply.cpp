// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// HOW A THEME APPLIES, as values: a version-2 theme's look read and written, each entry judged
// against what takes it, merge and reset, the keys and their gestures, slots that wait, and the
// palette a look gives (settings/apply.hpp). The weave that sends what these plan is witnessed in
// the desk suite, beside a live Workshop.

#include "doctest.h"

#include "settings/apply.hpp"
#include "settings/theme_persist.hpp"
#include "settings/vocabulary.hpp"

#include "flow/flow-pane/look.hpp"
#include "graphics/themes.hpp"
#include "persist/json.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace st = zengine::settings;
namespace ws = zengine::workshop;
namespace gfx = zengine::graphics;

namespace {

bool says(const std::string& text, std::string_view part) {
    return text.find(part) != std::string::npos;
}

/// A theme read from text, required to read.
st::ThemeFile read(const std::string& text) {
    const st::LoadedTheme loaded = st::from_text(text);
    REQUIRE_MESSAGE(loaded.outcome.accepted, loaded.outcome.refusal);
    return loaded.theme;
}

/// A theme resolved alone, from its file's text.
st::ThemeResolved alone(const std::string& text) {
    const st::ThemeFile t = read(text);
    const st::ThemeResolved r = st::resolve_theme_with(t.name, [&](std::string_view) {
        return st::LoadedTheme{st::Written::ok(), t, text};
    });
    REQUIRE_MESSAGE(r.accepted, r.refusal);
    return r;
}

/// A DECLARED SETTING: what a pane's row takes.
ws::v2::PaneSettingRow row(const char* key, std::optional<bool> flag,
                           std::optional<std::int64_t> number, std::vector<std::string> tags = {}) {
    ws::v2::PaneSettingRow r;
    r.key = key;
    r.flag = flag;
    r.number = number;
    r.tags = std::move(tags);
    return r;
}

/// THE WORLD A THEME IS JUDGED AGAINST: two layouts, the live one seating the dial and keeping its
/// `legend`, the other seating it too; the dial's declaration; one office-wide setting; the keymap
/// holding `layout.rename` and authoring `focus.left`; the pool's colours; Flow planned.
st::ApplyWorld world() {
    st::ApplyWorld w;
    w.layouts.revision = 4;
    w.layouts.active = 0;
    ws::PaneSetting legend;
    legend.key = "legend";
    legend.flag = false;
    ws::PaneSetting width;
    width.key = "width";
    width.number = 12;
    w.layouts.layouts = {
        ws::LayoutSettings{11, 0, "Working", {}, {ws::LayoutPaneSettings{"test.dial", "dial",
                                                                         {legend, width}, {}}}},
        ws::LayoutSettings{12, 1, "Other", {"night", 7},
                           {ws::LayoutPaneSettings{"test.dial", "dial", {}, {}}}}};
    w.layouts.declared = {
        ws::DeclaredSettings{"test.dial", "dial",
                             {row("legend", true, std::nullopt, {"chrome"}),
                              row("width", std::nullopt, 10, {"layout"})}},
        ws::DeclaredSettings{"test.office", "", {row("depth", std::nullopt, 3)}}};
    ws::ShownBinding rename;
    rename.id = "layout.rename";
    ws::ShownBinding left;
    left.id = "focus.left";
    left.authored = true;
    w.keymap.rows = {rename, left};
    w.pool_colors = st::pool_color_names();
    w.pool_fonts = st::pool_font_names();
    w.pool_sizes = st::pool_size_names();
    w.planned = {"zengine.flow"};
    return w;
}

st::ThemeApplyRequested asked(const char* theme, const char* mode = st::kApplyMerge) {
    st::ThemeApplyRequested a;
    a.theme = theme;
    a.mode = mode;
    return a;
}

const ws::SettingWrite* write_of(const st::ApplyPlan& p, std::int64_t layout, std::string_view target,
                                 std::string_view key) {
    for (const ws::SettingWrite& w : p.writes) {
        if (w.layout == layout && w.target == target && w.key == key) return &w;
    }
    return nullptr;
}

const gfx::PaletteColor* color_in(const gfx::PaletteOffered& p, std::string_view name) {
    for (const gfx::PaletteColor& c : p.colors) {
        if (c.name == name) return &c;
    }
    return nullptr;
}

} // namespace

TEST_CASE("a version-2 theme gives the pool its colours, fonts and sizes and each office its "
          "slots, and reads back as written") {
    const std::string text = R"({
  "format": "zengine-theme", "format_version": 2, "name": "evening",
  "summary": "light ink on a warm ground", "includes": ["night"], "on_failure": "skip",
  "outline": "+=!",
  "colors": [ { "name": "background", "color": "#2e3440", "cells": "terminal default" },
              { "name": "accent", "color": "#ebcb8b80", "cells": "bright yellow bold", "fill": "O" },
              { "name": "quiet", "fill": ":" } ],
  "fonts": [ { "name": "text", "face": "JetBrains Mono" } ],
  "sizes": [ { "name": "heading", "points": 15 } ],
  "slots": [ { "office": "zengine.flow",
               "colors": [ { "name": "wire", "color": "#88c0d0", "cells": "cyan" } ],
               "sizes": [ { "name": "label", "points": 11 } ] } ],
  "keys": [ { "action": "layout.rename", "gestures": ["ctrl+r", "alt+r"] } ]
})";
    const st::ThemeFile t = read(text);
    CHECK(t.on_failure == st::kOnFailureSkip);
    CHECK(t.outline == "+=!");
    REQUIRE(t.colors.size() == 3);
    CHECK(t.colors[1] == st::ThemeColor{"accent", "#ebcb8b80", "bright yellow bold", "O"});
    CHECK(t.colors[2] == st::ThemeColor{"quiet", "", "", ":"});
    CHECK(t.fonts == std::vector<st::ThemeFont>{{"text", "JetBrains Mono"}});
    CHECK(t.sizes == std::vector<st::ThemeSize>{{"heading", 15}});
    REQUIRE(t.slots.size() == 1);
    CHECK(t.slots[0].office == "zengine.flow");
    CHECK(t.keys == std::vector<st::ThemeKey>{{"layout.rename", {"ctrl+r", "alt+r"}}});
    // Written, it reads back the same; written again, it is the same text.
    const std::string written = st::to_text(t);
    CHECK(read(written) == t);
    CHECK(st::to_text(read(written)) == written);
    CHECK(st::rgba_of("#ebcb8b") == 0xebcb8bff);
    CHECK(st::rgba_of("#ebcb8b80") == 0xebcb8b80);
}

TEST_CASE("the shipped themes read as version-2 themes, and block is a look of the terminal alone") {
    for (const char* name : {"night", "day", "block"}) {
        CAPTURE(name);
        const st::LoadedTheme t = st::load_file(GRAPHICS_THEMES_DIR, name);
        REQUIRE_MESSAGE(t.outcome.accepted, t.outcome.refusal);
        CHECK(t.theme.name == name);
    }
    // Block gives no colour of its own: each entry is what a terminal shows and its fill.
    const st::LoadedTheme block = st::load_file(GRAPHICS_THEMES_DIR, "block");
    REQUIRE(block.outcome.accepted);
    CHECK(block.theme.outline == "===");
    for (const st::ThemeColor& c : block.theme.colors) {
        CAPTURE(c.name);
        CHECK(c.color.empty());
    }
}

TEST_CASE("a value that is no colour, or a terminal word no terminal shows, refuses the theme whatever on_failure says") {
    // ⚔ MUTATION (tests/mutants/settings_apply.md#bad-colour-taken): a colour judged only by its
    // length -- `#12345g` would paint as some colour no one wrote.
    const std::string bad = R"({ "format": "zengine-theme", "format_version": 2, "name": "bad",
  "on_failure": "skip", "colors": [ { "name": "accent", "color": "#12345g" } ] })";
    const st::LoadedTheme refused = st::from_text(bad);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(says(refused.outcome.refusal, "the colour `accent` is `#12345g`, which is no colour"));
    for (const char* wrong : {"#12345", "123456", "#1234567", "red"}) {
        CAPTURE(wrong);
        CHECK_FALSE(st::check_theme_color(st::ThemeColor{"accent", wrong, "", ""}).accepted);
    }
    CHECK(says(st::check_theme_color(st::ThemeColor{"accent", "", "purple", ""}).refusal,
               "none of what one shows"));
    CHECK(st::check_theme_color(st::ThemeColor{"accent", "", "bright magenta bold", ""}).accepted);
    CHECK(says(st::check_theme_color(st::ThemeColor{"accent", "", "", "ab"}).refusal,
               "one printable ASCII character"));
    CHECK(says(st::check_theme_color(st::ThemeColor{"accent", "", "", ""}).refusal, "gives nothing"));
    CHECK(says(st::check_theme_size(st::ThemeSize{"heading", 200}).refusal, "a whole number from 6 to 96"));
    st::ThemeFile twice;
    twice.name = "twice";
    twice.colors = {{"accent", "#000000", "", ""}, {"accent", "#ffffff", "", ""}};
    CHECK(says(st::check_theme_file(twice).refusal, "gives the colour `accent` twice"));
    twice.colors.pop_back();
    twice.on_failure = "maybe";
    CHECK(says(st::check_theme_file(twice).refusal, "`on_failure` is `refuse` or `skip`"));
}

TEST_CASE("a version-1 theme file reads as version 2: refused on a failure, each key one gesture, "
          "and applies as it would have") {
    st::v1::ThemeFile old;
    old.format = st::kThemeFormat;
    old.format_version = st::kThemeFormatVersionOne;
    old.name = "older";
    st::ThemeSetting legend;
    legend.target = "test.dial/dial";
    legend.key = "legend";
    legend.flag = true;
    old.settings = {legend};
    old.keys = {st::v1::ThemeKey{"layout.rename", "ctrl+r"}};
    const std::string text = loom::compat::serialize(loom::to_value(old));
    const st::ThemeFile t = read(text);
    CHECK(t.on_failure == st::kOnFailureRefuse);
    CHECK(t.keys == std::vector<st::ThemeKey>{{"layout.rename", {"ctrl+r"}}});
    CHECK(t.settings == old.settings);
    // ...ITS FINGERPRINT THE ONE VERSION 1 GAVE, so a layout's baseline still reads as current.
    const st::ThemeResolved r = alone(text);
    CHECK(r.fingerprint != 0);
    // APPLIED, it writes its setting to the live layout and sets its key, and nothing else.
    const st::ApplyPlan p = st::plan_apply(r, asked("older"), world());
    REQUIRE_MESSAGE(p.accepted, p.refusal);
    REQUIRE(p.writes.size() == 1);
    CHECK(write_of(p, 11, "test.dial/dial", "legend") != nullptr);
    CHECK(p.keys == std::vector<st::KeyEdit>{{"layout.rename", ws::keymap_edit::kSetSpelled, "ctrl+r"}});
    CHECK(p.skipped.empty());
    // Saved, it is written as version 2.
    CHECK(says(st::to_text(t), "\"format_version\": 2"));
}

TEST_CASE("an entry nothing takes is skipped and said where the theme says skip, and refuses it "
          "whole where it says refuse") {
    const std::string skip = R"({ "format": "zengine-theme", "format_version": 2, "name": "loose",
  "on_failure": "skip",
  "colors": [ { "name": "accent", "color": "#ff0000" }, { "name": "glow", "color": "#00ff00" } ],
  "slots": [ { "office": "zengine.unplanned", "colors": [ { "name": "rim", "color": "#0000ff" } ] } ],
  "settings": [ { "target": "test.dial/dial", "key": "brightness", "number": 3 },
                { "target": "test.dial/dial", "key": "width", "number": 14 } ],
  "keys": [ { "action": "no.such-action", "gestures": ["ctrl+k"] } ] })";
    const st::ApplyPlan p = st::plan_apply(alone(skip), asked("loose"), world());
    REQUIRE_MESSAGE(p.accepted, p.refusal);
    // What takes it is written; what nothing takes is said, each with why.
    CHECK(write_of(p, 11, "test.dial/dial", "width") != nullptr);
    CHECK(p.colors == std::vector<st::ThemeColor>{{"accent", "#ff0000", "", ""}});
    std::set<std::string> said;
    for (const st::ThemeSkip& s : p.skipped) said.insert(s.key + " of " + s.target + ": " + s.why);
    CHECK(said == std::set<std::string>{"brightness of test.dial/dial: nothing takes it",
                                        "glow of colors: nothing takes it",
                                        "rim of zengine.unplanned: nothing takes it",
                                        "no.such-action of keys: nothing takes it"});
    // ⚔ MUTATION (tests/mutants/settings_apply.md#refuse-read-as-skip): `on_failure` unread -- a
    // theme that refuses on a failure would be applied in part.
    std::string refuse = skip;
    refuse.replace(refuse.find("\"skip\""), 6, "\"refuse\"");
    const st::ApplyPlan whole = st::plan_apply(alone(refuse), asked("loose"), world());
    CHECK_FALSE(whole.accepted);
    CHECK(says(whole.refusal, "theme `loose`: `brightness` of `test.dial/dial` -- nothing takes it"));
    CHECK(says(whole.refusal, "the theme refuses on a failure"));
}

TEST_CASE("a value of the wrong kind refuses the theme whatever on_failure says, and a value its "
          "row does not take is nothing taking it") {
    const std::string wrong = R"({ "format": "zengine-theme", "format_version": 2, "name": "wrong",
  "on_failure": "skip", "settings": [ { "target": "test.dial/dial", "key": "legend", "number": 1 } ] })";
    const st::ApplyPlan p = st::plan_apply(alone(wrong), asked("wrong"), world());
    CHECK_FALSE(p.accepted);
    CHECK(says(p.refusal, "`legend` of `test.dial/dial` -- `legend` takes a flag, not a number"));
    CHECK(p.writes.empty());
    // An office-wide setting of the wrong kind too.
    const std::string office = R"({ "format": "zengine-theme", "format_version": 2, "name": "office",
  "on_failure": "skip", "settings": [ { "target": "test.office", "key": "depth", "text": "deep" } ] })";
    CHECK(says(st::plan_apply(alone(office), asked("office"), world()).refusal, "takes a number"));
}

TEST_CASE("merge writes what the theme names and leaves the rest; reset also returns every other "
          "key of each pane it names to its default") {
    const std::string text = R"({ "format": "zengine-theme", "format_version": 2, "name": "wide",
  "settings": [ { "target": "test.dial/dial", "key": "width", "number": 20 } ],
  "keys": [ { "action": "layout.rename", "gestures": ["alt+r"] } ] })";
    const st::ThemeResolved r = alone(text);
    const st::ApplyPlan merge = st::plan_apply(r, asked("wide"), world());
    REQUIRE(merge.accepted);
    REQUIRE(merge.writes.size() == 1);
    CHECK(merge.writes[0].number == 20);
    CHECK(merge.keys.size() == 1);
    // ⚔ MUTATION (tests/mutants/settings_apply.md#reset-as-merge): reset read as merge -- the
    // row's `legend` and the keymap's `focus.left` would stay the weaver's.
    const st::ApplyPlan reset = st::plan_apply(r, asked("wide", st::kApplyReset), world());
    REQUIRE(reset.accepted);
    const ws::SettingWrite* legend = write_of(reset, 11, "test.dial/dial", "legend");
    REQUIRE(legend != nullptr);
    CHECK(legend->clear);
    CHECK(write_of(reset, 11, "test.dial/dial", "width")->number == 20);
    CHECK(reset.keys == std::vector<st::KeyEdit>{{"layout.rename", ws::keymap_edit::kSetSpelled, "alt+r"},
                                                 {"focus.left", ws::keymap_edit::kReset, ""}});
    // Every layout, by `all_layouts`; one named by its id; an id no layout has, refused.
    st::ThemeApplyRequested every = asked("wide");
    every.all_layouts = true;
    CHECK(st::plan_apply(r, every, world()).layouts == std::vector<std::int64_t>{11, 12});
    st::ThemeApplyRequested one = asked("wide");
    one.layouts = {12};
    CHECK(write_of(st::plan_apply(r, one, world()), 12, "test.dial/dial", "width") != nullptr);
    one.layouts = {99};
    CHECK(says(st::plan_apply(r, one, world()).refusal, "no layout has the id 99"));
    // ...and `only_tags` keeps the settings whose row carries one.
    st::ThemeApplyRequested tagged = asked("wide");
    tagged.only_tags = {"chrome"};
    CHECK(st::plan_apply(r, tagged, world()).writes.empty());
    // A baseline names the theme and its fingerprint, in each layout it applies to.
    st::ThemeApplyRequested base = asked("wide");
    base.baseline = true;
    const st::ApplyPlan based = st::plan_apply(r, base, world());
    REQUIRE(based.baselines.size() == 1);
    CHECK(based.baselines[0].layout == 11);
    CHECK(based.baselines[0].theme == ws::LayoutTheme{"wide", r.fingerprint});
}

TEST_CASE("several gestures ask one action: the first sets it and the rest are added, and `none` alone disables it") {
    const std::string text = R"({ "format": "zengine-theme", "format_version": 2, "name": "keys",
  "keys": [ { "action": "layout.rename", "gestures": ["ctrl+r", "alt+r", "ctrl+shift+r"] },
            { "action": "focus.left", "gestures": ["none"] } ] })";
    const st::ApplyPlan p = st::plan_apply(alone(text), asked("keys"), world());
    REQUIRE(p.accepted);
    CHECK(p.keys == std::vector<st::KeyEdit>{{"layout.rename", ws::keymap_edit::kSetSpelled, "ctrl+r"},
                                             {"layout.rename", ws::keymap_edit::kAddSpelled, "alt+r"},
                                             {"layout.rename", ws::keymap_edit::kAddSpelled, "ctrl+shift+r"},
                                             {"focus.left", ws::keymap_edit::kDisable, ""}});
    // A gesture twice, or `none` beside a key, is no form.
    CHECK(says(st::check_theme_key(st::ThemeKey{"a.b", {"alt+r", "alt+r"}}).refusal, "names `alt+r` twice"));
    CHECK(says(st::check_theme_key(st::ThemeKey{"a.b", {"alt+r", "none"}}).refusal, "stands alone"));
    CHECK(says(st::check_theme_key(st::ThemeKey{"a.b", {}}).refusal, "names 1 to 8 gestures"));
}

TEST_CASE("an office the plan names that has not declared its slots waits, and what it declares applies when it does") {
    const std::string text = R"({ "format": "zengine-theme", "format_version": 2, "name": "wires",
  "on_failure": "refuse",
  "slots": [ { "office": "zengine.flow",
               "colors": [ { "name": "wire", "color": "#ff8800" }, { "name": "glow", "color": "#00ff00" } ] } ] })";
    // ⚔ MUTATION (tests/mutants/settings_apply.md#planned-slots-refused): an office not yet
    // declared read as nothing exposing it -- a theme that refuses would be refused while Flow
    // loads.
    const st::ApplyPlan p = st::plan_apply(alone(text), asked("wires"), world());
    REQUIRE_MESSAGE(p.accepted, p.refusal);
    REQUIRE(p.waiting.size() == 1);
    CHECK(p.waiting[0].office == "zengine.flow");
    CHECK(p.slots.empty());
    // Once it declares, its slots apply, and a miss found then is skipped and said.
    const st::SlotsDeclared flow{{{"wire", 0x88c0d0ff, "accent"}}, {}};
    const auto [ready, skipped] = st::judge_waiting(p.waiting[0], flow);
    CHECK(ready.colors == std::vector<st::ThemeColor>{{"wire", "#ff8800", "", ""}});
    REQUIRE(skipped.size() == 1);
    CHECK(skipped[0].key == "glow");
    CHECK(skipped[0].why == st::kNoSuchSlot);
    // DECLARED NOW, a slot it does not hold is no failure deferred: it refuses as any miss does.
    st::ApplyWorld declared = world();
    declared.declared["zengine.flow"] = flow;
    const st::ApplyPlan now = st::plan_apply(alone(text), asked("wires"), declared);
    CHECK_FALSE(now.accepted);
    CHECK(says(now.refusal, "`glow` of `zengine.flow` -- its office declares no such slot"));
}

TEST_CASE("a look over a palette sets what it names: merge keeps the rest of what was worn, reset "
          "starts from the pool's defaults") {
    const std::map<std::string, st::SlotsDeclared> declared{
        {"zengine.flow", st::SlotsDeclared{{{"wire", 0x88c0d0ff, "accent"}}, {}}}};
    // BLOCK OVER WHAT IS WORN, day: day's colours stand, block's terminal words and fills over them.
    const gfx::PaletteOffered day = gfx::themes::day();
    const gfx::PaletteOffered layered =
        st::look_over(day, "===", {{"accent", "", "cyan", "@"}}, {}, declared);
    CHECK(layered.outline == "===");
    CHECK(color_in(layered, "accent")->rgba == color_in(day, "accent")->rgba);
    CHECK(color_in(layered, "accent")->cells == "cyan");
    CHECK(color_in(layered, "accent")->fill == "@");
    CHECK(color_in(layered, "background")->rgba == color_in(day, "background")->rgba);
    // From the pool's defaults, the same look leaves day behind.
    const gfx::PaletteOffered reset =
        st::look_over(st::default_palette("night"), "===", {{"accent", "", "cyan", "@"}}, {}, declared);
    CHECK(color_in(reset, "background")->rgba ==
          color_in(gfx::themes::built_in(), "background")->rgba);
    // A slot set in part paints the rest as its office declared it.
    const gfx::PaletteOffered wired = st::look_over(st::default_palette("night"), "", {},
                                                    {{"zengine.flow", {{"wire", "", "", "%"}}, {}}},
                                                    declared);
    REQUIRE(wired.slots.size() == 1);
    REQUIRE(wired.slots[0].colors.size() == 1);
    CHECK(wired.slots[0].colors[0].rgba == 0x88c0d0ff);
    CHECK(wired.slots[0].colors[0].fill == "%");
    CHECK(wired.slots[0].colors[0].cells == color_in(wired, "accent")->cells);
}

TEST_CASE("Flow says every slot its slots.json declares, each with its default and its stand-in") {
    std::ifstream in(FLOW_SLOTS_JSON, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const zengine::persist::json::Parsed parsed = zengine::persist::json::parse(text);
    REQUIRE_MESSAGE(parsed.root, parsed.refusal);
    std::set<std::string> written;
    for (const zengine::persist::json::Node& c : parsed.root->member("colors")->items) {
        written.insert(c.member("name")->text + " " + c.member("color")->text + " " +
                       c.member("like")->text);
    }
    std::set<std::string> said;
    for (const st::SlotColorDeclared& c : zengine::flow_pane::hue::declared().colors) {
        char hex[16];
        std::snprintf(hex, sizeof hex, "#%06llx",
                      static_cast<unsigned long long>(static_cast<std::uint64_t>(c.rgba) >> 8));
        said.insert(c.name + " " + hex + " " + c.like);
    }
    CHECK(said == written);
}

TEST_CASE("a theme's fonts and sizes are judged against the pool's and set by name in the palette, "
          "a slot's size in its office's") {
    const std::string text = R"({ "format": "zengine-theme", "format_version": 2, "name": "large",
  "on_failure": "skip",
  "fonts": [ { "name": "text", "face": "JetBrains Mono" }, { "name": "fancy", "face": "Serif" } ],
  "sizes": [ { "name": "heading", "points": 22 }, { "name": "huge", "points": 40 } ],
  "slots": [ { "office": "zengine.flow", "sizes": [ { "name": "label", "points": 9 } ] } ] })";
    st::ApplyWorld w = world();
    w.declared["zengine.flow"] = st::SlotsDeclared{{}, {{"label", 11}}};
    const st::ApplyPlan p = st::plan_apply(alone(text), asked("large"), w);
    REQUIRE_MESSAGE(p.accepted, p.refusal);
    CHECK(p.fonts == std::vector<st::ThemeFont>{{"text", "JetBrains Mono"}});
    CHECK(p.sizes == std::vector<st::ThemeSize>{{"heading", 22}});
    std::set<std::string> said;
    for (const st::ThemeSkip& s : p.skipped) said.insert(s.target + " " + s.key);
    CHECK(said == std::set<std::string>{"fonts fancy", "sizes huge"});
    // ⚔ MUTATION (tests/mutants/settings_apply.md#sizes-left-out): a look that sets colours alone
    // -- a theme's sizes would never reach the window.
    const gfx::PaletteOffered look = st::look_over(st::default_palette("night"), "", {}, p.slots,
                                                   w.declared, p.fonts, p.sizes);
    std::int64_t heading = 0;
    for (const gfx::PaletteSize& s : look.sizes) {
        if (s.name == "heading") heading = s.points;
    }
    CHECK(heading == 22);
    REQUIRE(look.slots.size() == 1);
    CHECK(look.slots[0].sizes == std::vector<gfx::PaletteSize>{{"label", 9}});
}
