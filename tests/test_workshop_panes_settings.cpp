// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- a pane's settings across the seam: the declaration a pane sends
// beside its offer, judged whole under its office's stamp, and the settings the live layout keeps
// for a seated pane, handed to a holder that takes them before any room. A native seat stands in
// for a pane here, so a case can say the exact sentence at the exact moment; the Terminal's own
// setting is proven through its real image in the Terminal suite.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include <string>
#include <vector>

namespace {

constexpr const char* kDialOffice = "test.dial";
constexpr const char* kDial = "dial";

PaneRef dial_ref() { return PaneRef{kDialOffice, kDial}; }

PaneOffered dial_offer() { return PaneOffered{kDial, "Dial", "a pane that takes its settings"}; }

PaneSettingRow flag_row(const char* key, bool fallback) {
    PaneSettingRow row;
    row.key = key;
    row.flag = fallback;
    return row;
}

/// What the dial takes: a flag, a choice among words, and a step from 0 to 9.
PaneSettingsDeclared dial_declared() {
    PaneSettingsDeclared d;
    d.pane = kDial;
    PaneSettingRow mode;
    mode.key = "mode";
    mode.text = "wide";
    mode.choices = {"wide", "narrow"};
    PaneSettingRow step;
    step.key = "step";
    step.number = 1;
    step.low = 0;
    step.high = 9;
    d.rows = {flag_row("legend", true), mode, step};
    return d;
}

PaneSetting legend_off() { return PaneSetting{"legend", false, {}, {}}; }

/// A LIVE WORKSHOP WITH A DESK THAT ALREADY NAMES THE DIAL, keeping `settings` for it -- the
/// order a restored session meets a pane in -- and the seat that will offer it.
struct DialRig {
    PaneRig r;
    SettingsSeat* seat = nullptr;

    void open(const std::vector<PaneSetting>& settings) {
        r.mount_workshop();
        r.ready();
        r.extent(160, 48);
        Session& s = const_cast<Session&>(r.session());
        REQUIRE(add_pane(s.setup.active, dial_ref()));
        pane_of(s.setup.active, dial_ref())->settings = settings;
        seat = r.mount_settings_seat(kDialOffice);
    }
    void offer() {
        r.drive(seat, [](SettingsSeat& s, loom::Mail& m) { s.offer(m, dial_offer()); });
    }
    void declare(const PaneSettingsDeclared& d) {
        r.drive(seat, [d](SettingsSeat& s, loom::Mail& m) { s.declare(m, d); });
    }
    const RuntimePane* row() { return r.session().panes.runtime.find(kDialOffice, kDial); }
    bool seated() { return row() != nullptr && r.session().panes.has(row()->kind); }
};

} // namespace

TEST_CASE("a pane's settings are handed before its first room, whether or not it declared") {
    DialRig t;
    t.open({legend_off()});
    t.offer();
    REQUIRE(t.seated());
    // THE OFFER SEATS IT IN ITS OWN DELIVERY, AND THAT REPAINT HANDS THE SETTINGS FIRST: the
    // picture the room asks for is drawn with them. Nothing was declared, and none is waited on.
    REQUIRE(t.seat->heard.size() == 2);
    CHECK(t.seat->heard[0] == "settings");
    CHECK(t.seat->heard[1] == "room");
    CHECK(t.seat->handed.back().pane == kDial);
    CHECK(t.seat->handed.back().settings == std::vector<PaneSetting>{legend_off()});
    for (const std::string& author : t.seat->authors) {
        CHECK(author == kWorkshopProvider);
    }

    // ...AND A DECLARATION ARRIVING AFTER IT CHANGES NOTHING HANDED: what the layout keeps is
    // what the holder already heard.
    t.declare(dial_declared());
    CHECK(t.seat->heard.size() == 2);
}

TEST_CASE("a holder that takes no settings is handed none") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    Session& s = const_cast<Session&>(r.session());
    REQUIRE(add_pane(s.setup.active, dial_ref()));
    pane_of(s.setup.active, dial_ref())->settings = {legend_off()};
    std::size_t handed = 0;
    const loom::ObserverId tap = r.bus.add_observer([&handed](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Delivered && ev.schema_name == PaneSettings::zen_name) {
            ++handed;
        }
    });
    ProviderSeat* plain = r.mount_provider(kDialOffice);
    r.drive(plain, [](ProviderSeat& p, loom::Mail& m) { p.offer(m, dial_offer()); });
    const RuntimePane* row = r.session().panes.runtime.find(kDialOffice, kDial);
    REQUIRE(row != nullptr);
    REQUIRE(r.session().panes.has(row->kind));
    CHECK(plain->rooms.size() == 1);
    CHECK(handed == 0);
    r.bus.remove_observer(tap);
}

TEST_CASE("a layout switch hands a pane its new layout's settings once, and only when they differ") {
    DialRig t;
    t.open({legend_off()});
    t.offer();
    REQUIRE(t.seat->heard.size() == 2);

    // A DUPLICATE KEEPS THE SAME SETTINGS, so putting it live hands nothing, a switch between the
    // two hands nothing, and the same room grants nothing either.
    duplicate_live_layout(t.r);
    const std::size_t copy = t.r.session().setup.active_at;
    CHECK(t.seat->heard.size() == 2);
    layout_key(t.r, Act::kLayoutNext);
    REQUIRE(t.r.session().setup.active_at != copy);
    CHECK(t.seat->heard.size() == 2);

    // ...THIS LAYOUT NOW KEEPS NONE, and the next repaint hands that: the empty list is a hand-off.
    Session& s = const_cast<Session&>(t.r.session());
    pane_of(s.setup.active, dial_ref())->settings.clear();
    t.r.key(input::scan::kUnknown);
    REQUIRE(t.seat->heard.size() == 3);
    CHECK(t.seat->heard[2] == "settings");
    CHECK(t.seat->handed.back().settings.empty());

    // ...AND EACH SWITCH NOW HANDS THE OTHER LAYOUT'S, ONCE, and grants no room.
    layout_key(t.r, Act::kLayoutNext);
    REQUIRE(t.r.session().setup.active_at == copy);
    REQUIRE(t.seat->heard.size() == 4);
    CHECK(t.seat->heard[3] == "settings");
    CHECK(t.seat->handed.back().settings == std::vector<PaneSetting>{legend_off()});
    layout_key(t.r, Act::kLayoutNext);
    REQUIRE(t.seat->heard.size() == 5);
    CHECK(t.seat->handed.back().settings.empty());
    CHECK(t.seat->rooms.size() == 1);
}

TEST_CASE("a re-offer and a reopened row are handed their settings again, before their room") {
    DialRig t;
    t.open({legend_off()});
    t.offer();
    REQUIRE(t.seat->heard.size() == 2);

    // A RE-OFFER IS HOW A RELOADED IMAGE ARRIVES, holding nothing it was handed: owed again.
    t.offer();
    REQUIRE(t.seat->heard.size() == 4);
    CHECK(t.seat->heard[2] == "settings");
    CHECK(t.seat->heard[3] == "room");
    CHECK(t.seat->handed.back().settings == std::vector<PaneSetting>{legend_off()});

    // A CLOSE DISCARDS THE ROW, SETTINGS AND ALL; THE REOPENED ROW KEEPS NONE, AND THAT IS HANDED
    // TOO -- never handed is not handed none, so the pane is not left on its last layout's word.
    REQUIRE(hand_close(t.r, dial_ref()).closed);
    REQUIRE(seat_pane(t.r, dial_ref()).opened);
    REQUIRE(t.seat->heard.size() == 6);
    CHECK(t.seat->heard[4] == "settings");
    CHECK(t.seat->heard[5] == "room");
    CHECK(t.seat->handed.back().settings.empty());
}

TEST_CASE("a declaration is judged whole under the office stamp, refused aloud, and replaced") {
    DialRig t;
    t.open({});
    t.offer();
    REQUIRE(t.row() != nullptr);

    t.declare(dial_declared());
    CHECK(t.row()->settings_rows == dial_declared().rows);
    CHECK(t.row()->settings_from == t.r.settings_seat_id(t.seat));
    REQUIRE(t.r.w->counted_settings(*t.row()) != nullptr);
    CHECK(*t.r.w->counted_settings(*t.row()) == dial_declared().rows);

    struct Case {
        const char* what;
        PaneSettingsDeclared declared;
        const char* says;
    };
    const auto with = [](std::vector<PaneSettingRow> rows) {
        PaneSettingsDeclared d;
        d.pane = kDial;
        d.rows = std::move(rows);
        return d;
    };
    PaneSettingRow none;
    none.key = "legend";
    PaneSettingRow two = flag_row("legend", true);
    two.number = 1;
    PaneSettingRow chosen = flag_row("legend", true);
    chosen.choices = {"on"};
    PaneSettingRow bounded = flag_row("legend", true);
    bounded.low = 0;
    PaneSettingRow upside;
    upside.key = "step";
    upside.number = 5;
    upside.low = 9;
    upside.high = 0;
    PaneSettingRow outside;
    outside.key = "step";
    outside.number = 12;
    outside.high = 9;
    PaneSettingRow unlisted;
    unlisted.key = "mode";
    unlisted.text = "tall";
    unlisted.choices = {"wide", "narrow"};
    PaneSettingRow dash;
    dash.key = "mode";
    dash.text = "wide";
    dash.choices = {"wide", "-"};
    PaneSettingRow twice_listed;
    twice_listed.key = "mode";
    twice_listed.text = "wide";
    twice_listed.choices = {"wide", "wide"};
    PaneSettingRow crowded;
    crowded.key = "mode";
    crowded.text = "c0";
    for (std::size_t i = 0; i <= kMaxPaneSettingChoices; ++i) {
        crowded.choices.push_back("c" + std::to_string(i));
    }
    std::vector<PaneSettingRow> many;
    for (std::size_t i = 0; i <= kMaxPaneSettingsPerRow; ++i) {
        many.push_back(flag_row(("k" + std::to_string(100 + i)).c_str(), true));
    }
    const std::vector<Case> cases = {
        {"a default of no kind", with({none}), "setting `legend`'s default holds no value"},
        {"a default of two kinds", with({two}), "setting `legend`'s default holds more than one value"},
        {"choices on a flag", with({chosen}), "setting `legend` is a flag, and only a text names choices"},
        {"a low on a flag", with({bounded}), "setting `legend` is a flag, and only a number has a low"},
        {"a low above its high", with({upside}), "setting `step`'s low is above its high"},
        {"a default outside its range", with({outside}),
         "setting `step`'s default is not one it takes: step takes a whole number up to 9, not a number `12`"},
        {"a default not among its choices", with({unlisted}),
         "setting `mode`'s default is not one it takes: mode takes one of wide, narrow, not a text `tall`"},
        {"`-` as a choice", with({dash}), "setting `mode`'s choice `-` is not one a weaver can type"},
        {"a choice twice", with({twice_listed}), "setting `mode` names the choice `wide` twice"},
        {"too many choices", with({crowded}), "setting `mode` names more than"},
        {"a key twice", with({flag_row("legend", true), flag_row("legend", false)}),
         "setting `legend` is declared twice"},
        {"Workshop's own key", with({flag_row("workshop.text", true)}),
         "setting `workshop.text` is Workshop's own"},
        {"a key a weaver cannot type", with({flag_row("Legend", true)}), "`Legend` is not"},
        {"more settings than a row keeps", with(many), "a pane declares at most"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        t.declare(c.declared);
        // REFUSED ALOUD, NAMING THE PANE AND WHAT WOULD HAVE WORKED, and the settings in force
        // stand: a refused declaration replaces nothing.
        CHECK(t.r.session().notice_is_bad);
        CHECK(t.r.last_notice().find("Dial @test.dial: its settings were not taken") != std::string::npos);
        CHECK(t.r.last_notice().find(c.says) != std::string::npos);
        CHECK(t.row()->settings_rows == dial_declared().rows);
    }

    // A LATER ACCEPTED DECLARATION REPLACES THE SETTINGS WHOLE.
    t.declare(with({flag_row("legend", false)}));
    CHECK(t.row()->settings_rows == std::vector<PaneSettingRow>{flag_row("legend", false)});

    // ...A PANE THE OFFICE NEVER OFFERED IS REFUSED BY NAME...
    PaneSettingsDeclared stranger = dial_declared();
    stranger.pane = "other";
    t.declare(stranger);
    CHECK(t.r.last_notice().find("`test.dial/other` is not a pane that office has offered -- its "
                            "settings were not taken") != std::string::npos);

    // ...AND PERSONAL SPEECH, EVEN FROM THE HOLDER, DECLARES NOTHING AND SAYS NOTHING.
    const std::string before = t.r.last_notice();
    t.r.drive(t.seat, [](SettingsSeat& s, loom::Mail& m) { s.declare_personally(m, dial_declared()); });
    CHECK(t.row()->settings_rows == std::vector<PaneSettingRow>{flag_row("legend", false)});
    CHECK(t.r.last_notice() == before);
}

TEST_CASE("a declaration counts only while the weave that sent it holds the office") {
    DialRig t;
    t.open({});
    t.offer();
    t.declare(dial_declared());
    REQUIRE(t.r.w->counted_settings(*t.row()) != nullptr);

    // ANOTHER WEAVE TAKES THE OFFICE: the declaration stays where it was admitted and counts for
    // nothing, until the new holder declares its own.
    t.r.unmount_settings_seat(t.seat);
    SettingsSeat* successor = t.r.mount_settings_seat(kDialOffice);
    CHECK(t.r.w->counted_settings(*t.row()) == nullptr);
    PaneSettingsDeclared own;
    own.pane = kDial;
    own.rows = {flag_row("legend", false)};
    t.r.drive(successor, [own](SettingsSeat& s, loom::Mail& m) { s.declare(m, own); });
    REQUIRE(t.r.w->counted_settings(*t.row()) != nullptr);
    CHECK(*t.r.w->counted_settings(*t.row()) == own.rows);
}

TEST_CASE("what a declared setting takes is said in its own kind's words") {
    PaneSettingRow legend = flag_row("legend", true);
    CHECK(pane_setting_takes(legend) == "on or off");
    CHECK(pane_setting_refused_by(legend, PaneSetting{"legend", false, {}, {}}).empty());
    CHECK(pane_setting_refused_by(legend, PaneSetting{"legend", {}, {}, std::string("hidden")}) ==
          "legend takes on or off, not a text `hidden`");
    CHECK(pane_setting_refused_by(legend, PaneSetting{"legend", {}, {}, {}}) ==
          "legend takes on or off, not no value");

    PaneSettingRow step;
    step.key = "step";
    step.number = 1;
    step.low = 0;
    step.high = 9;
    CHECK(pane_setting_takes(step) == "a whole number from 0 to 9");
    CHECK(pane_setting_refused_by(step, PaneSetting{"step", {}, 10, {}}) ==
          "step takes a whole number from 0 to 9, not a number `10`");
    step.high.reset();
    CHECK(pane_setting_takes(step) == "a whole number from 0 up");

    PaneSettingRow title;
    title.key = "title";
    title.text = "";
    CHECK(pane_setting_takes(title) == "a text of at most 64 printable characters");
    CHECK(pane_setting_refused_by(title, PaneSetting{"title", {}, {}, std::string("any words")}).empty());
    CHECK(pane_setting_row_problem(title).empty());
    CHECK(pane_setting_default(title) == PaneSetting{"title", {}, {}, std::string()});
}
