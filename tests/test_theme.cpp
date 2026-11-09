// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The theme suite: a theme's two files read, written and refused, a theme resolved with its
// includes, a folder listed, a stack laid -- the shipped themes laid as the build compiled them, a
// link of the other medium converted and warned, a pane's own values judged -- and the weave loaded
// over a real kernel, offering each stack worn as the palette named for it.

#include "doctest.h"

#include "lifecycle_door.hpp"
#include "maker/weave.hpp"
#include "theme/stack.hpp"
#include "theme/theme_persist.hpp"
#include "theme/vocabulary.hpp"

#include "graphics/terminal_inks.hpp"
#include "graphics/themes.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/kernel/kernel.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace th = zengine::theme;
namespace gfx = zengine::graphics;
namespace fs = std::filesystem;

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

/// A folder of the suite's own, removed when it ends.
struct Folder {
    fs::path dir;
    explicit Folder(const char* name) {
        dir = fs::temp_directory_path() / (std::string("zengine-theme-") + name);
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir);
    }
    ~Folder() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    std::string path() const { return dir.generic_string(); }
    void write(const std::string& file, const std::string& text) const {
        std::ofstream out(dir / file, std::ios::binary);
        out << text;
    }
};

std::string window_text(const std::string& name, const std::string& body = {}) {
    return "{ \"format\": \"zengine-theme\", \"format_version\": 3, \"name\": \"" + name + "\"" +
           (body.empty() ? std::string() : ", " + body) + " }";
}

std::string terminal_text(const std::string& name, const std::string& body = {}) {
    return "{ \"format\": \"zengine-terminal\", \"format_version\": 1, \"name\": \"" + name + "\"" +
           (body.empty() ? std::string() : ", " + body) + " }";
}

const gfx::TerminalInk* ink_named(const gfx::TerminalLook& look, const std::string& name) {
    for (const gfx::TerminalInk& i : look.inks) {
        if (i.name == name) return &i;
    }
    return nullptr;
}

const gfx::PaletteColor* color_named(const gfx::PaletteOffered& p, const std::string& name) {
    for (const gfx::PaletteColor& c : p.colors) {
        if (c.name == name) return &c;
    }
    return nullptr;
}

/// The folder the build stages the shipped themes in.
std::string shipped() { return THEME_SHIPPED_DIR; }

} // namespace

TEST_CASE("a theme's window file and terminal file are read as their formats say, and written back "
          "as they were read") {
    const auto w = th::window_from_text(window_text(
        "evening", "\"summary\": \"warm\", \"includes\": [\"night\"], \"colors\": [ { \"name\": "
                   "\"accent\", \"color\": \"#e0a050\" } ], \"styles\": [ { \"name\": \"heading\", "
                   "\"color\": \"accent\" } ], \"own\": [ { \"role\": \"zengine.flow\", \"colors\": "
                   "[ { \"name\": \"wire\", \"color\": \"#88c0d0\" } ] } ]"));
    REQUIRE(w.outcome.accepted);
    CHECK(w.file.includes == std::vector<std::string>{"night"});
    CHECK(w.file.styles.at(0).color == "accent");
    CHECK(w.file.own.at(0).role == "zengine.flow");
    const auto again = th::window_from_text(th::to_text(w.file));
    REQUIRE(again.outcome.accepted);
    CHECK(again.file == w.file);
    const auto t = th::terminal_from_text(terminal_text(
        "evening", "\"borders\": \"+=|\", \"inks\": [ { \"name\": \"accent\", \"ink\": \"yellow\", "
                   "\"fill\": \"O\" } ]"));
    REQUIRE(t.outcome.accepted);
    CHECK(t.file.borders == "+=|");
    const auto t2 = th::terminal_from_text(th::to_text(t.file));
    REQUIRE(t2.outcome.accepted);
    CHECK(t2.file == t.file);
}

TEST_CASE("a version-1 theme, which held settings and keys, is refused saying where they live; a "
          "version-2 one saying what moved; and a member no file holds by its name") {
    // ⚔ MUTATION (tests/mutants/theme.md#version-one-read): a version-1 file read as version 3.
    const auto one = th::window_from_text(
        "{ \"format\": \"zengine-theme\", \"format_version\": 1, \"name\": \"old\" }");
    CHECK_FALSE(one.outcome.accepted);
    CHECK(has(one.outcome.refusal, "settings and keys"));
    CHECK(has(one.outcome.refusal, "layouts' owner"));
    CHECK(has(one.outcome.refusal, "keymap's own layers"));
    const auto envelope = th::window_from_text("{ \"zen\": { \"name\": \"ThemeFile\" } }");
    CHECK_FALSE(envelope.outcome.accepted);
    CHECK(has(envelope.outcome.refusal, "version-1"));
    const auto two = th::window_from_text(
        "{ \"format\": \"zengine-theme\", \"format_version\": 2, \"name\": \"old\" }");
    CHECK_FALSE(two.outcome.accepted);
    CHECK(has(two.outcome.refusal, ".terminal.json"));
    const auto misspelt = th::window_from_text(window_text("x", "\"colours\": []"));
    CHECK_FALSE(misspelt.outcome.accepted);
    CHECK(has(misspelt.outcome.refusal, "`colours`"));
    const auto ink = th::terminal_from_text(terminal_text(
        "x", "\"inks\": [ { \"name\": \"accent\", \"ink\": \"purple\" } ]"));
    CHECK_FALSE(ink.outcome.accepted);
    CHECK(has(ink.outcome.refusal, "purple"));
    const auto window_as_terminal = th::terminal_from_text(window_text("x"));
    CHECK_FALSE(window_as_terminal.outcome.accepted);
}

TEST_CASE("a theme resolves each medium with its includes, the later winning for one name; a cycle, "
          "an include with no file of that medium and a theme with no file are refused") {
    Folder f("resolve");
    f.write("base.json", window_text("base", "\"colors\": [ { \"name\": \"accent\", \"color\": \"#010203\" }, "
                                             "{ \"name\": \"alert\", \"color\": \"#040506\" } ]"));
    f.write("base.terminal.json", terminal_text("base", "\"borders\": \"===\", \"inks\": [ { \"name\": "
                                                        "\"accent\", \"ink\": \"red\", \"fill\": \"%\" } ]"));
    f.write("top.json", window_text("top", "\"includes\": [\"base\"], \"colors\": [ { \"name\": "
                                           "\"accent\", \"color\": \"#0a0b0c\" } ]"));
    f.write("top.terminal.json", terminal_text("top", "\"includes\": [\"base\"], \"inks\": [ { \"name\": "
                                                      "\"accent\", \"ink\": \"cyan\" } ]"));
    const th::ThemeResolved r = th::resolve_theme(f.path(), "top");
    REQUIRE(r.accepted);
    CHECK(r.window);
    CHECK(r.terminal);
    REQUIRE(r.colors.size() == 2);
    CHECK(r.colors[0].color == "#0a0b0c"); // its own over its include's
    CHECK(r.colors[1].color == "#040506"); // its include's where it names none
    CHECK(r.borders == "===");
    REQUIRE(r.inks.size() == 1);
    CHECK(r.inks[0].ink == "cyan");
    CHECK(r.inks[0].fill == "%"); // an ink's field it leaves out stands as its include's
    // A CYCLE.
    f.write("a.json", window_text("a", "\"includes\": [\"b\"]"));
    f.write("b.json", window_text("b", "\"includes\": [\"a\"]"));
    const th::ThemeResolved cycle = th::resolve_theme(f.path(), "a");
    CHECK_FALSE(cycle.accepted);
    CHECK(has(cycle.refusal, "includes itself"));
    // AN INCLUDE WITH NO FILE OF THAT MEDIUM.
    f.write("lone.terminal.json", terminal_text("lone"));
    f.write("wants.json", window_text("wants", "\"includes\": [\"lone\"]"));
    const th::ThemeResolved missing = th::resolve_theme(f.path(), "wants");
    CHECK_FALSE(missing.accepted);
    CHECK(has(missing.refusal, "no window file"));
    CHECK_FALSE(th::resolve_theme(f.path(), "nothing").accepted);
}

TEST_CASE("a folder lists each theme once, by name, with the files it has, and a broken one with "
          "its refusal") {
    Folder f("list");
    f.write("both.json", window_text("both", "\"summary\": \"two files\""));
    f.write("both.terminal.json", terminal_text("both"));
    f.write("ink.terminal.json", terminal_text("ink"));
    f.write("broken.json", "{ not json");
    const std::vector<th::ThemeSummary> listed = th::list_themes(f.path());
    REQUIRE(listed.size() == 3);
    CHECK(listed[0].name == "both");
    CHECK(listed[0].window);
    CHECK(listed[0].terminal);
    CHECK(listed[0].summary == "two files");
    CHECK(listed[1].name == "broken");
    CHECK_FALSE(listed[1].refusal.empty());
    CHECK(listed[2].name == "ink");
    CHECK_FALSE(listed[2].window);
    CHECK(listed[2].terminal);
}

TEST_CASE("each shipped theme laid alone is the palette the build compiled from its two files, so "
          "no look changes when the theme weave paints it") {
    for (const gfx::PaletteOffered& compiled : gfx::themes::shipped()) {
        const th::Laid laid = th::lay_stack(
            {compiled.palette}, [](const std::string& n) { return th::resolve_theme(shipped(), n); });
        INFO(compiled.palette);
        REQUIRE(laid.laid.links.size() == 1);
        CHECK(laid.laid.links[0].refusal.empty());
        CHECK(laid.laid.links[0].warning.empty()); // both files: nothing converted
        CHECK(laid.palette.colors == compiled.colors);
        CHECK(laid.palette.sizes == compiled.sizes);
        CHECK(laid.palette.styles == compiled.styles);
        CHECK(laid.palette.fonts == compiled.fonts);
        for (const gfx::PaletteColor& c : compiled.colors) {
            const gfx::TerminalInk* a = ink_named(laid.palette.terminal, c.name);
            const gfx::TerminalInk* b = ink_named(compiled.terminal, c.name);
            CHECK_MESSAGE((a == nullptr) == (b == nullptr), c.name);
            if (a != nullptr && b != nullptr) {
                CHECK_MESSAGE(*a == *b, c.name);
            }
        }
        CHECK(laid.palette.terminal.borders == compiled.terminal.borders);
        CHECK(laid.palette.palette == compiled.palette);
    }
}

TEST_CASE("a stack lays each theme over the one below; a theme of the other medium alone is a link "
          "of it, converted and said with a warning; a link that will not resolve is left out and said") {
    Folder f("stack");
    // A WINDOW THEME ALONE: in a terminal, each colour it gives as its nearest ink.
    // ⚔ MUTATION (tests/mutants/theme.md#window-alone-unconverted): its colours left out of the
    // terminal look.
    f.write("warm.json", window_text("warm", "\"colors\": [ { \"name\": \"text\", \"color\": \"#cd0000\" } ]"));
    // A TERMINAL THEME ALONE: in a window, each ink as the colour xterm shows it in, and `terminal
    // default` as the theme below.
    // ⚔ MUTATION (tests/mutants/theme.md#terminal-default-painted): `terminal default` given a colour.
    f.write("inky.terminal.json", terminal_text("inky", "\"inks\": [ { \"name\": \"accent\", \"ink\": "
                                                        "\"bright cyan\" }, { \"name\": \"alert\", "
                                                        "\"ink\": \"terminal default\" } ]"));
    const auto resolve = [&](const std::string& n) {
        const th::ThemeResolved mine = th::resolve_theme(f.path(), n);
        return mine.accepted ? mine : th::resolve_theme(shipped(), n);
    };
    const th::Laid laid = th::lay_stack({"night", "warm", "inky", "gone"}, resolve);
    CHECK(laid.palette.palette == "night+warm+inky+gone");
    REQUIRE(laid.laid.links.size() == 4);
    CHECK(laid.laid.links[0].warning.empty());
    CHECK(laid.laid.links[1].warning == th::kWindowAloneWarning);
    CHECK(laid.laid.links[2].warning == th::kTerminalAloneWarning);
    CHECK_FALSE(laid.laid.links[3].refusal.empty());
    CHECK(th::worth_saying(laid.laid));
    const gfx::TerminalInk* text = ink_named(laid.palette.terminal, "text");
    REQUIRE(text != nullptr);
    CHECK(text->ink == "red"); // #cd0000, the nearest of the sixteen, over `terminal default`
    CHECK(text->fill == ink_named(gfx::themes::night().terminal, "text")->fill);
    const gfx::PaletteColor* accent = color_named(laid.palette, "accent");
    REQUIRE(accent != nullptr);
    CHECK(accent->rgba == *gfx::window::ink_rgba("bright cyan"));
    const gfx::PaletteColor* alert = color_named(laid.palette, "alert");
    REQUIRE(alert != nullptr);
    CHECK(alert->rgba == color_named(gfx::themes::night(), "alert")->rgba); // fell through
    // ...AND A STACK OF NONE IS THE POOL'S OWN, under no name.
    const th::Laid none = th::lay_stack({}, resolve);
    CHECK(none.palette.palette.empty());
    CHECK(none.palette.colors == gfx::themes::built_in().colors);
}

TEST_CASE("a pane's own value a theme sets is laid for its role, and one its role does not declare "
          "is left out and said; a role not yet declared has its values laid as they are") {
    Folder f("own");
    f.write("flowy.json", window_text("flowy", "\"own\": [ { \"role\": \"zengine.flow\", \"colors\": [ "
                                               "{ \"name\": \"wire\", \"color\": \"#0c0d0e\" }, "
                                               "{ \"name\": \"wirre\", \"color\": \"#0f1011\" } ] } ]"));
    const auto resolve = [&](const std::string& n) { return th::resolve_theme(f.path(), n); };
    const th::Laid undeclared = th::lay_stack({"flowy"}, resolve);
    REQUIRE(undeclared.palette.own.size() == 1);
    CHECK(undeclared.palette.own[0].colors.size() == 2);
    CHECK(undeclared.laid.skipped.empty());
    // ⚔ MUTATION (tests/mutants/theme.md#own-unjudged): an own value laid whatever its role declares.
    th::Declared declared;
    declared["zengine.flow"] = gfx::OwnValuesDeclared{{gfx::OwnDeclared{"wire", "color", "guide", 0, 0}}};
    const th::Laid judged = th::lay_stack({"flowy"}, resolve, declared);
    REQUIRE(judged.palette.own.size() == 1);
    REQUIRE(judged.palette.own[0].colors.size() == 1);
    CHECK(judged.palette.own[0].colors[0] == gfx::PaletteColor{"wire", 0x0c0d0eff});
    REQUIRE(judged.laid.skipped.size() == 1);
    CHECK(judged.laid.skipped[0].name == "wirre");
    CHECK(judged.laid.skipped[0].role == "zengine.flow");
}

TEST_CASE("a stack's palette is named for its themes, and a pane's theme is laid over its layout's "
          "stack unless the stack already ends with it") {
    CHECK(th::stack_palette({"night", "block"}) == "night+block");
    CHECK(th::stack_palette({}).empty());
    CHECK(th::stack_over({"night"}, "block") == std::vector<std::string>{"night", "block"});
    CHECK(th::stack_over({"night", "block"}, "block") == std::vector<std::string>{"night", "block"});
    CHECK(th::stack_over({"night"}, "") == std::vector<std::string>{"night"});
}

namespace {

/// The graphics role as the theme weave speaks to it: every palette offered, kept.
class GraphicsStub final : public loom::Weave {
public:
    std::vector<gfx::PaletteOffered> offered;
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<gfx::PaletteOffered>()};
    }
    void handle(const loom::Message& in, loom::Bus&) override {
        offered.push_back(loom::from_value<gfx::PaletteOffered>(in.payload));
    }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("tt.Graphics", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
};

/// A host and a pane: what it says to the theme role, as its role, and what it hears said.
class Speaker final : public loom::Weave {
public:
    std::vector<th::StackSaid> said;
    std::vector<th::StacksShown> shown;
    int asked_placed = 0;
    int asked_own = 0;
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {loom::schema_of<th::StackSaid>(), loom::schema_of<th::StacksShown>(),
                loom::schema_of<th::ThemesPlacedRequested>(), loom::schema_of<gfx::OwnValuesAsked>()};
    }
    std::vector<std::shared_ptr<const loom::Schema>> emitted_schemas() const override {
        return {loom::schema_of<th::ThemesPlaced>(), loom::schema_of<th::ThemesWorn>(),
                loom::schema_of<th::StacksRequested>(), loom::schema_of<gfx::OwnValuesDeclared>()};
    }
    void handle(const loom::Message& in, loom::Bus&) override {
        const std::string name = in.payload.schema().name();
        if (name == th::StackSaid::zen_name) said.push_back(loom::from_value<th::StackSaid>(in.payload));
        if (name == th::StacksShown::zen_name) shown.push_back(loom::from_value<th::StacksShown>(in.payload));
        if (name == th::ThemesPlacedRequested::zen_name) ++asked_placed;
        if (name == gfx::OwnValuesAsked::zen_name) ++asked_own;
    }
    loom::Value snapshot() const override { return loom::Value(loom::make_schema("tt.Speaker", 1, {})); }
    loom::Value policy() const override { return zengine::maker::default_value(loom::lifecycle_policy_schema()); }
    void revive(const loom::Value&) override {}
};

struct ThemeRig {
    Folder mine{"weave"};
    loom::Switchboard bus;
    loom::Kernel kernel{bus, loom::trust_every_artifact("the suite loads the theme weave")};
    GraphicsStub* graphics = nullptr;
    Speaker* host = nullptr;
    Speaker* pane = nullptr;
    loom::WeaveId host_id{}, pane_id{};
    loom::WeaveId door{};
    std::uint64_t corr = 0;
    loom::WeaveId theme{};
    ThemeRig() {
        auto g = std::make_unique<GraphicsStub>();
        graphics = g.get();
        (void)bus.register_weave(std::move(g), loom::Grant{}.allow_any(), gfx::kGraphicsRole);
        auto h = std::make_unique<Speaker>();
        host = h.get();
        host_id = bus.register_weave(std::move(h), loom::Grant{}.allow_any(), "test.host");
        auto p = std::make_unique<Speaker>();
        pane = p.get();
        pane_id = bus.register_weave(std::move(p), loom::Grant{}.allow_any(), "zengine.flow");
        const auto loaded = kernel.load("theme", THEME_ARTIFACT, th::kThemeRole, loom::Grant{}.allow_any());
        INFO(loaded.error);
        REQUIRE(loaded.ok);
        theme = loaded.id;
        door = zengine::testing::mount_door(bus);
        zengine::testing::order_activation(bus, door, loaded.id, 1);
        pump();
    }
    void pump() {
        for (int n = 0; n < 64 && bus.pending() != 0; ++n) bus.pump_pending();
    }
    template <class Shape> void say(loom::WeaveId from, const char* role, const Shape& s) {
        REQUIRE(bus.role_send_to_role_as(from, role, th::kThemeRole,
                                           loom::Message(loom::to_value(s), from, {}, ++corr))
                    .valid());
        pump();
    }
    template <class Shape> void publish(loom::WeaveId from, const char* role, const Shape& s) {
        REQUIRE(static_cast<bool>(bus.role_publish_as(from, role, loom::Message(loom::to_value(s), from))));
        pump();
    }
    const gfx::PaletteOffered* last(const std::string& palette) const {
        for (auto it = graphics->offered.rbegin(); it != graphics->offered.rend(); ++it) {
            if (it->palette == palette) return &*it;
        }
        return nullptr;
    }
};

} // namespace

TEST_CASE("the theme weave asks on its activation for where the themes are and for every own value, "
          "and offers each stack worn as the palette named for it, laid again only where it changed") {
    ThemeRig r;
    CHECK(r.host->asked_placed == 1); // published: it names no host
    CHECK(r.pane->asked_own == 1);
    r.say(r.host_id, "test.host", th::ThemesPlaced{r.mine.path(), shipped()});
    r.say(r.host_id, "test.host", th::ThemesWorn{{th::ThemeStack{{"night"}}, th::ThemeStack{{"night", "block"}}}});
    const gfx::PaletteOffered* night = r.last("night");
    const gfx::PaletteOffered* both = r.last("night+block");
    REQUIRE(night != nullptr);
    REQUIRE(both != nullptr);
    CHECK(night->colors == gfx::themes::night().colors);
    CHECK(both->terminal.borders == "===");
    const std::size_t offers = r.graphics->offered.size();
    const std::int64_t night_revision = night->revision; // `offered` grows below
    // THE SAME STACKS SAID AGAIN: nothing offered.
    // ⚔ MUTATION (tests/mutants/theme.md#offer-unchanged): every stack offered again whatever changed.
    r.say(r.host_id, "test.host", th::ThemesWorn{{th::ThemeStack{{"night"}}, th::ThemeStack{{"night", "block"}}}});
    CHECK(r.graphics->offered.size() == offers);
    // A THEME OF THE WEAVER'S OVER A SHIPPED ONE OF ITS NAME, laid again once the stacks are said.
    r.mine.write("night.json", window_text("night", "\"colors\": [ { \"name\": \"accent\", \"color\": \"#123456\" } ]"));
    r.say(r.host_id, "test.host", th::ThemesPlaced{r.mine.path(), shipped()});
    const gfx::PaletteOffered* mine = r.last("night");
    REQUIRE(mine != nullptr);
    CHECK(color_named(*mine, "accent")->rgba == 0x123456ff);
    CHECK(mine->revision > night_revision);
}

TEST_CASE("the theme weave judges a stack's own values against what a pane's role declares once it "
          "declares, says what it left out, and keeps every stack across a reload") {
    ThemeRig r;
    r.mine.write("flowy.json", window_text("flowy", "\"own\": [ { \"role\": \"zengine.flow\", \"colors\": [ "
                                                    "{ \"name\": \"wire\", \"color\": \"#0c0d0e\" }, "
                                                    "{ \"name\": \"wirre\", \"color\": \"#0f1011\" } ] } ]"));
    r.say(r.host_id, "test.host", th::ThemesPlaced{r.mine.path(), shipped()});
    r.say(r.host_id, "test.host", th::ThemesWorn{{th::ThemeStack{{"flowy"}}}});
    REQUIRE(r.last("flowy") != nullptr);
    CHECK(r.last("flowy")->own.at(0).colors.size() == 2);
    r.publish(r.pane_id, "zengine.flow",
              gfx::OwnValuesDeclared{{gfx::OwnDeclared{"wire", "color", "", 0x88c0d0ff, 0}}});
    REQUIRE(r.last("flowy") != nullptr);
    CHECK(r.last("flowy")->own.at(0).colors.size() == 1);
    REQUIRE_FALSE(r.host->said.empty());
    CHECK(r.host->said.back().laid.skipped.at(0).name == "wirre");
    // A RELOAD KEEPS THE STACKS: asked, it shows them as they were laid.
    const auto reload = r.kernel.reload_from("theme", THEME_ARTIFACT);
    INFO(reload.error);
    REQUIRE(reload.reloaded);
    zengine::testing::order_activation(r.bus, r.door, r.theme, 2);
    r.pump();
    r.say(r.host_id, "test.host", th::StacksRequested{});
    REQUIRE_FALSE(r.host->shown.empty());
    REQUIRE(r.host->shown.back().stacks.size() == 1);
    CHECK(r.host->shown.back().stacks[0].palette == "flowy");
}
