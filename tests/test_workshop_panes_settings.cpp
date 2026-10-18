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

#include <optional>
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
    // NOT SENT AT ALL: neither delivered nor refused at the door it does not have.
    std::size_t handed = 0;
    const loom::ObserverId tap = r.bus.add_observer([&handed](const loom::BusEvent& ev) {
        if (ev.schema_name == PaneSettings::zen_name) {
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

    // ...AND A RE-OFFER OWES EVEN THAT: the row keeping none is handed none again.
    t.offer();
    REQUIRE(t.seat->heard.size() == 8);
    CHECK(t.seat->heard[6] == "settings");
    CHECK(t.seat->heard[7] == "room");
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
    // ...AND THE NEW HOLDER IS HANDED WHAT THE LAYOUT KEEPS, though it equals what the last one
    // heard: what a holder heard is that holder's.
    REQUIRE(successor->heard == std::vector<std::string>{"settings"});
    CHECK(successor->handed.back().settings.empty());
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

// ---- A pane's settings as rows of its subject, written through the layout's settings door ----

namespace {

/// THE DIAL SEATED, ITS SETTINGS DECLARED, AND INSPECTED through the inspector's door.
struct InspectedDial : DialRig {
    void open_inspected(const std::vector<PaneSetting>& settings = {}) {
        open(settings);
        offer();
        declare(dial_declared());
        REQUIRE(hand_inspect(r, dial_ref()).accepted);
    }
    std::vector<PaneSetting> stored() {
        const SetupPane* row = pane_of(r.session().setup.active, dial_ref());
        REQUIRE(row != nullptr);
        return row->settings;
    }
    std::vector<std::string> labels_after(const char* section) {
        std::vector<std::string> out;
        bool in = false;
        for (const Row& row : r.session().inspected.rows) {
            if (row.section()) {
                in = row.label() == section;
                continue;
            }
            if (in) {
                out.push_back(row.label());
            }
        }
        return out;
    }
};

} // namespace

TEST_CASE("a pane's settings are rows of its subject, named by their keys, after RESOLVED") {
    InspectedDial t;
    t.open_inspected({PaneSetting{"zoom", true, {}, {}}});
    // ONE ROW PER DECLARED SETTING, IN THE PANE'S ORDER, AND A KEPT ROW FOR THE KEY NONE TAKES.
    CHECK(t.labels_after("SETTINGS") == std::vector<std::string>{"legend", "mode", "step", "zoom"});
    std::vector<std::string> sections;
    for (const Row& row : t.r.session().inspected.rows) {
        if (row.section()) {
            sections.push_back(row.label());
        }
    }
    CHECK(sections == std::vector<std::string>{"AUTHORED", "RESOLVED", "SETTINGS", "INTERIOR"});
    // NOTHING STORED READS AS THE DEFAULT, SPELLED AS IT IS TYPED.
    CHECK(subject_value(t.r.session(), "legend", "SETTINGS") == "on");
    CHECK(subject_value(t.r.session(), "mode", "SETTINGS") == "wide");
    CHECK(subject_value(t.r.session(), "step", "SETTINGS") == "1");
    CHECK(subject_value(t.r.session(), "zoom", "SETTINGS") ==
          "on -- kept; no setting here takes it, and `-` clears it");
    for (const char* label : {"legend", "mode", "step", "zoom"}) {
        CAPTURE(label);
        CHECK(subject_row(t.r.session(), label, "SETTINGS")->editable());
    }
}

TEST_CASE("a value a setting does not take is refused at the edit, naming what it takes") {
    InspectedDial t;
    t.open_inspected();
    struct Case {
        const char* label;
        const char* typed;
        const char* says;
    };
    for (const Case& c : std::vector<Case>{
             {"legend", "hidden", "legend: takes on or off, not `hidden`"},
             {"legend", "true", "legend: takes on or off, not `true`"},
             {"step", "12", "step: takes a whole number from 0 to 9, not `12`"},
             {"step", "two", "step: takes a whole number from 0 to 9, not `two`"},
             {"mode", "tall", "mode: takes one of wide, narrow, not `tall`"},
         }) {
        CAPTURE(c.typed);
        const PaneSubjectActed said = hand_commit(t.r, c.label, c.typed, "SETTINGS");
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal == c.says);
        CHECK(t.stored().empty());
    }
}

TEST_CASE("a setting typed at its default is stored as its absence, and `-` clears one") {
    InspectedDial t;
    t.open_inspected();
    REQUIRE(hand_commit(t.r, "legend", "off", "SETTINGS").accepted);
    REQUIRE(hand_commit(t.r, "step", "3", "SETTINGS").accepted);
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"legend", false, {}, {}},
                                                 PaneSetting{"step", {}, 3, {}}});
    CHECK(subject_value(t.r.session(), "legend", "SETTINGS") == "off");
    // THE DEFAULT, TYPED, IS NOTHING STORED: one spelling of "the pane's default".
    REQUIRE(hand_commit(t.r, "legend", "on", "SETTINGS").accepted);
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"step", {}, 3, {}}});
    REQUIRE(hand_commit(t.r, "step", " - ", "SETTINGS").accepted);
    CHECK(t.stored().empty());
    // ...AND `-` WITH NOTHING STORED SAYS SO, writing nothing.
    const PaneSubjectActed again = hand_commit(t.r, "step", "-", "SETTINGS");
    CHECK_FALSE(again.accepted);
    CHECK(again.refusal == "step: step already takes its default -- nothing is stored to clear");
}

TEST_CASE("a text setting is stored as it is typed, spaces included, and `-` clears it") {
    DialRig t;
    t.open({});
    t.offer();
    PaneSettingsDeclared d;
    d.pane = kDial;
    PaneSettingRow side;
    side.key = "side";
    side.text = "right";
    PaneSettingRow mode;
    mode.key = "mode";
    mode.text = "wide";
    mode.choices = {"wide", "narrow"};
    d.rows = {side, mode};
    t.declare(d);
    REQUIRE(hand_inspect(t.r, dial_ref()).accepted);
    const auto stored = [&t] { return pane_of(t.r.session().setup.active, dial_ref())->settings; };
    const auto text = [](const char* key, const char* value) {
        return std::vector<PaneSetting>{PaneSetting{key, {}, {}, std::string(value)}};
    };

    // A TRAILING SPACE IS PART OF THE TEXT: stored, read and handed as it was typed...
    REQUIRE(hand_commit(t.r, "side", "left ", "SETTINGS").accepted);
    CHECK(stored() == text("side", "left "));
    CHECK(subject_value(t.r.session(), "side", "SETTINGS") == "left ");
    CHECK(t.seat->handed.back().settings == text("side", "left "));
    // ...AND A DRAFT OPENED ON IT AND COMMITTED UNCHANGED LEAVES IT AS IT IS.
    REQUIRE(hand_commit(t.r, "side", subject_value(t.r.session(), "side", "SETTINGS"), "SETTINGS")
                .accepted);
    CHECK(stored() == text("side", "left "));

    // THREE SPACES ARE A TEXT OF THREE SPACES, not an empty one.
    REQUIRE(hand_commit(t.r, "side", "   ", "SETTINGS").accepted);
    CHECK(stored() == text("side", "   "));

    // A CHOICE IS TAKEN EXACTLY: one with a space the setting does not name is refused, saying
    // what it takes, and nothing is written.
    const PaneSubjectActed spaced = hand_commit(t.r, "mode", "narrow ", "SETTINGS");
    CHECK_FALSE(spaced.accepted);
    CHECK(spaced.refusal == "mode: takes one of wide, narrow, not `narrow `");
    CHECK(stored() == text("side", "   "));

    // `-`, WITH SPACES AROUND IT OR NOT, CLEARS -- as it resets every row Info edits.
    REQUIRE(hand_commit(t.r, "side", " - ", "SETTINGS").accepted);
    CHECK(stored().empty());
}

TEST_CASE("a settings write says it was stored and handed, or why it was not handed") {
    InspectedDial t;
    t.open_inspected();
    const std::size_t heard = t.seat->heard.size();
    REQUIRE(hand_commit(t.r, "legend", "off", "SETTINGS").accepted);
    CHECK(t.r.last_notice() == "committed legend of Dial = off; stored and handed");
    // HANDED IN THE COMMIT'S OWN DELIVERY, before any room it grants.
    REQUIRE(t.seat->heard.size() == heard + 1);
    CHECK(t.seat->heard.back() == "settings");
    CHECK(t.seat->handed.back().settings == std::vector<PaneSetting>{PaneSetting{"legend", false, {}, {}}});

    // NOTHING HOLDING THE OFFICE: the declared row is a kept one now, and `-` clears it; stored,
    // and the sentence says nobody holds the office.
    t.r.unmount_settings_seat(t.seat);
    t.r.key(input::scan::kUnknown);
    const PaneSubjectActed cleared = hand_commit(t.r, "legend", "-", "SETTINGS");
    REQUIRE_MESSAGE(cleared.accepted, cleared.refusal);
    CHECK(t.r.last_notice() ==
          "committed legend of Dial = --; stored; not handed: nothing holds `test.dial` now");

    // A HOLDER WITHOUT THE DOOR: stored, and the sentence says why it went no further.
    pane_of(const_cast<Session&>(t.r.session()).setup.active, dial_ref())->settings = {
        PaneSetting{"legend", false, {}, {}}};
    ProviderSeat* plain = t.r.mount_provider(kDialOffice);
    t.r.drive(plain, [](ProviderSeat& p, loom::Mail& m) { p.offer(m, dial_offer()); });
    REQUIRE(hand_inspect(t.r, dial_ref()).accepted);
    REQUIRE(hand_commit(t.r, "legend", "-", "SETTINGS").accepted);
    CHECK(t.r.last_notice() ==
          "committed legend of Dial = --; stored; not handed: test.dial takes no settings");
}

TEST_CASE("a kept setting takes only `-`, and the rows are named anew when it goes") {
    InspectedDial t;
    t.open_inspected({PaneSetting{"zoom", true, {}, {}}});
    const std::int64_t name = t.r.session().inspected.name;
    const PaneSubjectActed no = hand_commit(t.r, "zoom", "off", "SETTINGS");
    CHECK_FALSE(no.accepted);
    CHECK(no.refusal == "zoom: kept for a pane that declares it -- it takes only `-`, which clears it");
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"zoom", true, {}, {}}});
    REQUIRE(hand_commit(t.r, "zoom", "-", "SETTINGS").accepted);
    CHECK(t.stored().empty());
    // THE ROW WENT WITH ITS KEY, so the rows were named anew: a draft for them abandons aloud.
    CHECK(t.labels_after("SETTINGS") == std::vector<std::string>{"legend", "mode", "step"});
    CHECK(t.r.session().inspected.name != name);
}

TEST_CASE("the subject is named anew when the settings its rows are built from change") {
    DialRig t;
    t.open({});
    t.offer();
    REQUIRE(hand_inspect(t.r, dial_ref()).accepted);
    // NOTHING DECLARED, NOTHING STORED: no SETTINGS section at all.
    for (const Row& row : t.r.session().inspected.rows) {
        CHECK(row.label() != "SETTINGS");
    }
    const std::int64_t bare = t.r.session().inspected.name;

    t.declare(dial_declared());
    const std::int64_t declared = t.r.session().inspected.name;
    CHECK(declared != bare);
    REQUIRE(subject_row(t.r.session(), "legend", "SETTINGS") != nullptr);

    // AN IDENTICAL DECLARATION, as every catalog request brings, keeps the name...
    t.declare(dial_declared());
    t.r.key(input::scan::kUnknown);
    CHECK(t.r.session().inspected.name == declared);

    // ...AND A COMMIT AGAINST THE OLD NAME IS REFUSED WITH NOTHING WRITTEN.
    DoorHand& h = door_hand(t.r);
    const std::size_t before = h.acted.size();
    h.next = [bare](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kOffice)
            .send_to_role(kWorkshopProvider, PaneCommitRequested{bare, 0, "off"});
    };
    (void)t.r.bus.send(t.r.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    t.r.bus.drain_until_idle();
    REQUIRE(h.acted.size() == before + 1);
    CHECK_FALSE(h.acted.back().accepted);
    CHECK(h.acted.back().refusal == WorkshopWeave::kPaneCommitSubjectGone);

    // A NEW HOLDER: the declaration stops counting, its rows become kept ones, named anew.
    REQUIRE(hand_commit(t.r, "legend", "off", "SETTINGS").accepted);
    t.r.unmount_settings_seat(t.seat);
    (void)t.r.mount_settings_seat(kDialOffice);
    t.r.key(input::scan::kUnknown);
    CHECK(t.r.session().inspected.name != declared);
    CHECK(subject_value(t.r.session(), "legend", "SETTINGS") ==
          "off -- kept; no setting here takes it, and `-` clears it");
}

TEST_CASE("the subject is named anew when its pane's row leaves the desk, and again when it comes back") {
    InspectedDial t;
    t.open_inspected();
    const std::int64_t before = t.r.session().inspected.name;
    const std::size_t at = subject_row_index(t.r.session(), "legend", "SETTINGS");
    REQUIRE(hand_close(t.r, dial_ref()).closed);
    const std::int64_t closed = t.r.session().inspected.name;
    CHECK(closed != before);
    REQUIRE(seat_pane(t.r, dial_ref()).opened);
    CHECK(t.r.session().inspected.name != closed);

    // A COMMIT DRAFTED BEFORE THE CLOSE NAMES A ROW THE DESK MADE AFRESH: refused, nothing written.
    DoorHand& h = door_hand(t.r);
    const std::size_t asked = h.acted.size();
    h.next = [before, at](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kOffice)
            .send_to_role(kWorkshopProvider,
                          PaneCommitRequested{before, static_cast<std::int64_t>(at), "off"});
    };
    (void)t.r.bus.send(t.r.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    t.r.bus.drain_until_idle();
    REQUIRE(h.acted.size() == asked + 1);
    CHECK_FALSE(h.acted.back().accepted);
    CHECK(h.acted.back().refusal == WorkshopWeave::kPaneCommitSubjectGone);
    CHECK(t.stored().empty());
}

TEST_CASE("a close discards a pane's settings with its row, and every close door says which") {
    InspectedDial t;
    t.open_inspected();
    REQUIRE(hand_commit(t.r, "legend", "off", "SETTINGS").accepted);
    REQUIRE(hand_close(t.r, dial_ref()).closed);
    CHECK(t.r.last_notice().find("hid Dial and its settings here (legend off) -- ") == 0);
    REQUIRE(seat_pane(t.r, dial_ref()).opened);
    CHECK(t.stored().empty());

    REQUIRE(hand_commit(t.r, "step", "4", "SETTINGS").accepted);
    REQUIRE(hand_toggle(t.r, dial_ref()).closed);
    CHECK(t.r.last_notice().find("hid Dial and its settings here (step 4) -- ") == 0);

    // ...AND A CLOSE OF A ROW THAT KEPT NONE SAYS NOTHING ABOUT SETTINGS.
    REQUIRE(hand_toggle(t.r, dial_ref()).opened);
    REQUIRE(hand_close(t.r, dial_ref()).closed);
    CHECK(t.r.last_notice().find("hid Dial -- ") == 0);
}

TEST_CASE("arrangement's remove says the settings it discarded too") {
    Live t;
    open_pane(t, ref_of(stock::kKind));
    pane_of(const_cast<Session&>(t.session()).setup.active, ref_of(stock::kKind))->settings = {
        PaneSetting{"mode", {}, {}, std::string("wide")}};
    enter_arrange_desk(t);
    select_pane(t, ref_of(stock::kKind));
    t.key(input::scan::kD);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(stock::kKind)));
    CHECK(t.notice().find("hid " + ref_text(ref_of(stock::kKind)) +
                          " and its settings here (mode wide) -- ") == 0);
    CHECK(t.notice().find("nothing behind it was touched") != std::string::npos);
}

TEST_CASE("a pane's settings are part of what makes a linked layout modified") {
    InspectedDial t;
    t.open_inspected();
    link_live_setup(const_cast<Session&>(t.r.session()).setup, "/kept/desk.json");
    REQUIRE(live_status(t.r.session().setup) == setup_link::kCurrent);
    REQUIRE(hand_commit(t.r, "legend", "off", "SETTINGS").accepted);
    CHECK(live_status(t.r.session().setup) == setup_link::kModified);
    // ...AND THE DEFAULT WRITES ITS ABSENCE, which is what the file holds: current again.
    REQUIRE(hand_commit(t.r, "legend", "-", "SETTINGS").accepted);
    CHECK(live_status(t.r.session().setup) == setup_link::kCurrent);

    // A FILE THAT SPELLED THE DEFAULT IS NOT THE ABSENCE: the layout stays modified.
    Session& s = const_cast<Session&>(t.r.session());
    pane_of(s.setup.active, dial_ref())->settings = {PaneSetting{"legend", true, {}, {}}};
    link_live_setup(s.setup, "/kept/desk.json");
    REQUIRE(hand_commit(t.r, "legend", "-", "SETTINGS").accepted);
    CHECK(live_status(t.r.session().setup) == setup_link::kModified);
}

// ---- v2: how a weaver finds a setting, the revision a row is handed at, and the doors ----

namespace {

/// A NATIVE SEAT THAT TAKES v2's SETTINGS: it holds an office, offers and declares as it is told,
/// and records in order every room and every hand-off with its revision; as an office it also asks
/// the settings doors and keeps their answers. Apart from `SettingsSeat`, which takes v1.
class RevisionSeat
    : public loom::WeaveBase<RevisionSeat, SeatState,
                             loom::Accept<PaneCatalogRequested, PaneRoom, v2::PaneSettings,
                                          PaneSettingsShown, PaneSettingActed, SeatDo>,
                             loom::Emit<PaneOffered, v2::PaneSettingsDeclared,
                                        PaneSettingsRequested, PaneSettingCommitRequested,
                                        PaneSettingUnused>> {
public:
    explicit RevisionSeat(std::string office) : office_(std::move(office)) {}

    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const PaneRoom&, loom::Mail&) { heard.push_back("room"); }
    void on(const v2::PaneSettings& s, loom::Mail& mail) {
        heard.push_back("settings");
        handed.push_back(s);
        authors.push_back(std::string(mail.authored_role()));
    }
    void on(const PaneSettingsShown& s, loom::Mail& mail) {
        if (mail.answers_ask()) shown.push_back(s);
    }
    void on(const PaneSettingActed& a, loom::Mail& mail) {
        if (mail.answers_ask()) acted.push_back(a);
    }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(RevisionSeat&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }

    /// As the office it holds, or -- `personally` -- as the weave alone, which holds no office.
    template <class Shape> void say(loom::Mail& mail, const Shape& s, bool personally = false) {
        if (personally) {
            (void)mail.send_to_role(kWorkshopProvider, s);
        } else {
            (void)mail.as_role(office_).send_to_role(kWorkshopProvider, s);
        }
    }

    std::function<void(RevisionSeat&, loom::Mail&)> next;
    std::vector<std::string> heard;
    std::vector<v2::PaneSettings> handed;
    std::vector<std::string> authors;
    std::vector<PaneSettingsShown> shown;
    std::vector<PaneSettingActed> acted;

private:
    std::string office_;
};

/// MOUNT ONE IN `office` (or in none), granted what it says.
RevisionSeat* mount_revision_seat(PaneRig& r, const std::string& office, loom::WeaveId* id) {
    auto seat = std::make_unique<RevisionSeat>(office);
    RevisionSeat* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
    grant.allow_to_any(v2::PaneSettingsDeclared::zen_name, v2::PaneSettingsDeclared::zen_version);
    grant.allow_to_any(PaneSettingsRequested::zen_name, PaneSettingsRequested::zen_version);
    grant.allow_to_any(PaneSettingCommitRequested::zen_name,
                       PaneSettingCommitRequested::zen_version);
    grant.allow_to_any(PaneSettingUnused::zen_name, PaneSettingUnused::zen_version);
    *id = r.bus.register_weave(std::move(seat), std::move(grant), office);
    raw->zen_set_self(*id);
    return raw;
}

/// ...AND HAVE IT SAY ONE THING INSIDE ITS OWN DELIVERY, then let the bus settle.
void drive(PaneRig& r, loom::WeaveId id, RevisionSeat* seat,
           std::function<void(RevisionSeat&, loom::Mail&)> what) {
    seat->next = std::move(what);
    (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
}

v2::PaneSettingRow v2_row(const PaneSettingRow& v1, std::vector<std::string> tags, bool pinned,
                          std::string summary) {
    v2::PaneSettingRow row = widen_setting_row(v1);
    row.tags = std::move(tags);
    row.pinned = pinned;
    row.summary = std::move(summary);
    return row;
}

/// THE DIAL AS v2 SAYS IT: the legend pinned, every row tagged and summarised.
v2::PaneSettingsDeclared dial_declared_v2() {
    const PaneSettingsDeclared v1 = dial_declared();
    return v2::PaneSettingsDeclared{
        kDial,
        {v2_row(v1.rows[0], {"chrome", "help"}, true, "show the dial's legend"),
         v2_row(v1.rows[1], {"chrome"}, false, "how wide the dial draws"),
         v2_row(v1.rows[2], {"input"}, false, "how far one turn moves")}};
}

/// A LIVE WORKSHOP WHOSE DESK NAMES THE DIAL, the dial a v2 holder, and an editor's office that
/// asks the settings doors about it.
struct RevisionRig {
    PaneRig r;
    RevisionSeat* seat = nullptr;
    loom::WeaveId seat_id{};
    RevisionSeat* editor = nullptr;
    loom::WeaveId editor_id{};

    void open(const std::vector<PaneSetting>& settings = {}, bool declared = true) {
        r.mount_workshop();
        r.ready();
        r.extent(160, 48);
        Session& s = const_cast<Session&>(r.session());
        REQUIRE(add_pane(s.setup.active, dial_ref()));
        pane_of(s.setup.active, dial_ref())->settings = settings;
        seat = mount_revision_seat(r, kDialOffice, &seat_id);
        editor = mount_revision_seat(r, "test.editor", &editor_id);
        offer();
        if (declared) {
            declare(dial_declared_v2());
        }
    }
    void offer() {
        drive(r, seat_id, seat, [](RevisionSeat& s, loom::Mail& m) { s.say(m, dial_offer()); });
    }
    void declare(const v2::PaneSettingsDeclared& d) {
        drive(r, seat_id, seat, [d](RevisionSeat& s, loom::Mail& m) { s.say(m, d); });
    }
    PaneSettingsShown read(bool personally = false) {
        const std::size_t before = editor->shown.size();
        drive(r, editor_id, editor, [personally](RevisionSeat& s, loom::Mail& m) {
            s.say(m, PaneSettingsRequested{kDialOffice, kDial}, personally);
        });
        REQUIRE(editor->shown.size() == before + 1);
        return editor->shown.back();
    }
    PaneSettingActed commit(PaneSettingCommitRequested asked, bool personally = false) {
        asked.provider = kDialOffice;
        asked.pane = kDial;
        const std::size_t before = editor->acted.size();
        drive(r, editor_id, editor, [asked, personally](RevisionSeat& s, loom::Mail& m) {
            s.say(m, asked, personally);
        });
        REQUIRE(editor->acted.size() == before + 1);
        return editor->acted.back();
    }
    std::vector<PaneSetting> stored() {
        const SetupPane* row = pane_of(r.session().setup.active, dial_ref());
        REQUIRE(row != nullptr);
        return row->settings;
    }
    const RuntimePane* row() { return r.session().panes.runtime.find(kDialOffice, kDial); }
};

PaneSettingCommitRequested number_at(std::int64_t revision, const char* key, std::int64_t n) {
    PaneSettingCommitRequested c;
    c.revision = revision;
    c.key = key;
    c.number = n;
    return c;
}

} // namespace

TEST_CASE("a v2 declaration says how each setting is found, and each of its bounds is refused by name") {
    // A TAG IS A COMPACT WORD OF ITS OWN: the strip's own tabs are not tags.
    CHECK(pane_setting_tag_problem("chrome").empty());
    CHECK(pane_setting_tag_problem("x-2").empty());
    CHECK(pane_setting_tag_problem("1st").empty());
    for (const char* bad : {"", "-lead", "Chrome", "has space", "dot.ted", "seventeen-letters"}) {
        CAPTURE(bad);
        CHECK_FALSE(pane_setting_tag_problem(bad).empty());
    }
    for (const char* reserved : {"pinned", "all", "kept"}) {
        CAPTURE(reserved);
        CHECK(pane_setting_tag_problem(reserved) ==
              "`" + std::string(reserved) + "` is a tab of its own, never a setting's tag");
    }

    const v2::PaneSettingsDeclared sound = dial_declared_v2();
    CHECK(v2::pane_settings_declared_problem(sound).empty());
    struct Case {
        const char* what;
        std::function<void(v2::PaneSettingsDeclared&)> make;
        const char* said;
    };
    const std::vector<Case> cases = {
        {"a reserved tag", [](v2::PaneSettingsDeclared& d) { d.rows[1].tags = {"all"}; },
         "setting `mode`: `all` is a tab of its own, never a setting's tag"},
        {"a tag past its bound",
         [](v2::PaneSettingsDeclared& d) { d.rows[1].tags = {"abcdefghijklmnopq"}; },
         "setting `mode`: a setting's tag is at most 16 bytes -- `abcdefghijklmnopq` has 17"},
        {"an uppercase tag", [](v2::PaneSettingsDeclared& d) { d.rows[1].tags = {"Wide"}; },
         "setting `mode`: a setting's tag is `a`-`z`, `0`-`9` and `-` -- `Wide` is not"},
        {"a tag twice on one row",
         [](v2::PaneSettingsDeclared& d) { d.rows[2].tags = {"input", "input"}; },
         "setting `step` names the tag `input` twice"},
        {"five tags on one row",
         [](v2::PaneSettingsDeclared& d) { d.rows[2].tags = {"a", "b", "c", "d", "e"}; },
         "setting `step` is listed under 5 tags -- a setting takes at most 4"},
        {"thirteen tags between the rows",
         [](v2::PaneSettingsDeclared& d) {
             d.rows[0].tags = {"t1", "t2", "t3", "t4"};
             d.rows[1].tags = {"t5", "t6", "t7", "t8"};
             d.rows[2].tags = {"t9", "t10", "t11", "t12"};
             d.rows.push_back(v2_row(flag_row("zoom", false), {"t13"}, false, ""));
         },
         "a pane's settings name at most 12 tags between them -- these name 13"},
        {"four pins",
         [](v2::PaneSettingsDeclared& d) {
             for (v2::PaneSettingRow& row : d.rows) row.pinned = true;
             d.rows.push_back(v2_row(flag_row("zoom", false), {}, true, ""));
         },
         "a pane pins at most 3 settings by default -- this pins 4"},
        {"a summary a pane cannot draw",
         [](v2::PaneSettingsDeclared& d) { d.rows[0].summary = std::string(65, 'x'); },
         "setting `legend`'s summary is past 64 bytes of printable ASCII"},
        {"v1's own rules", [](v2::PaneSettingsDeclared& d) { d.rows[2].low = 10; },
         "setting `step`'s low is above its high"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        v2::PaneSettingsDeclared d = sound;
        c.make(d);
        CHECK(v2::pane_settings_declared_problem(d) == c.said);
    }
    // ...AND TWELVE TAGS AND THREE PINS ARE STILL A DECLARATION.
    v2::PaneSettingsDeclared full = sound;
    full.rows[0].tags = {"t1", "t2", "t3", "t4"};
    full.rows[1].tags = {"t5", "t6", "t7", "t8"};
    full.rows[2].tags = {"t9", "t10", "t11", "t12"};
    for (v2::PaneSettingRow& row : full.rows) row.pinned = true;
    CHECK(v2::pane_settings_declared_problem(full).empty());

    // THROUGH WORKSHOP: a v2 declaration counts in both versions; a refused one is said and the
    // declaration in force stands.
    RevisionRig t;
    t.open();
    REQUIRE(t.r.w->counted_settings_v2(*t.row()) != nullptr);
    CHECK(*t.r.w->counted_settings_v2(*t.row()) == sound.rows);
    CHECK(*t.r.w->counted_settings(*t.row()) == dial_declared().rows);
    v2::PaneSettingsDeclared reserved = sound;
    reserved.rows[1].tags = {"kept"};
    t.declare(reserved);
    CHECK(t.r.last_notice() ==
          "Dial @test.dial: its settings were not taken -- setting `mode`: `kept` is a tab of its "
          "own, never a setting's tag");
    CHECK(*t.r.w->counted_settings_v2(*t.row()) == sound.rows);
}

TEST_CASE("a v1 declaration is read as v2 rows, untagged, unpinned and unsummarised") {
    DialRig t;
    t.open({});
    t.offer();
    t.declare(dial_declared());
    REQUIRE(t.r.w->counted_settings_v2(*t.row()) != nullptr);
    const std::vector<v2::PaneSettingRow>& rows = *t.r.w->counted_settings_v2(*t.row());
    REQUIRE(rows.size() == 3);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        CHECK(narrow_setting_row(rows[i]) == dial_declared().rows[i]);
        CHECK(rows[i].tags.empty());
        CHECK_FALSE(rows[i].pinned);
        CHECK(rows[i].summary.empty());
    }
}

TEST_CASE("a reloaded image that declares nothing shows none of its predecessor's settings") {
    InspectedDial t;
    t.open_inspected();
    REQUIRE(t.labels_after("SETTINGS") == std::vector<std::string>{"legend", "mode", "step"});
    // THE SAME IMAGE OFFERING AGAIN, as every catalog request brings, keeps its declaration.
    t.offer();
    CHECK(t.r.w->counted_settings(*t.row()) != nullptr);
    CHECK(t.labels_after("SETTINGS") == std::vector<std::string>{"legend", "mode", "step"});
    // A RELOAD IN PLACE: the same weave id, its next incarnation, offering and declaring nothing.
    const loom::WeaveId same = t.r.settings_seat_id(t.seat);
    REQUIRE(t.r.bus.swap_state(same, t.r.bus.snapshot_bytes(same)).revived);
    t.offer();
    REQUIRE(t.r.settings_seat_id(t.seat) == same);
    CHECK(t.r.w->counted_settings(*t.row()) == nullptr);
    CHECK(t.r.w->counted_settings_v2(*t.row()) == nullptr);
    CHECK(t.row()->settings_rows.empty());
    CHECK(t.row()->settings_rows_v2.empty());
    CHECK(t.labels_after("SETTINGS").empty());
    // ...AND A DECLARATION THE NEW IMAGE SENDS COUNTS.
    t.declare(dial_declared());
    CHECK(t.labels_after("SETTINGS") == std::vector<std::string>{"legend", "mode", "step"});
}

TEST_CASE("a holder that takes v2 is handed its row's revision, moved by a write and by a desk put live") {
    RevisionRig t;
    t.open({legend_off()});
    REQUIRE_FALSE(t.seat->handed.empty());
    CHECK(t.seat->heard.front() == "settings"); // before its first room, as v1 is
    const v2::PaneSettings first = t.seat->handed.back();
    CHECK(first.pane == kDial);
    CHECK(first.revision > 0);
    CHECK(first.settings == std::vector<PaneSetting>{legend_off()});
    for (const std::string& author : t.seat->authors) {
        CHECK(author == kWorkshopProvider);
    }
    // THE SAME DESK REPAINTED HANDS NOTHING: the revision has not moved.
    std::size_t handed = t.seat->handed.size();
    t.r.key(input::scan::kUnknown);
    CHECK(t.seat->handed.size() == handed);

    // A DUPLICATE PUT LIVE KEEPS THE SAME SETTINGS, AND IS HANDED ANEW AT A NEW REVISION -- where
    // a v1 holder is handed nothing (above) -- and each switch back moves it again.
    duplicate_live_layout(t.r);
    REQUIRE(t.seat->handed.size() == handed + 1);
    const v2::PaneSettings copy = t.seat->handed.back();
    CHECK(copy.settings == first.settings);
    CHECK(copy.revision > first.revision);
    layout_key(t.r, Act::kLayoutNext);
    REQUIRE(t.seat->handed.size() == handed + 2);
    CHECK(t.seat->handed.back().revision > copy.revision);

    // A WRITE MOVES IT TOO, and what is handed is the revision the commit answered.
    handed = t.seat->handed.size();
    const PaneSettingActed wrote =
        t.commit(number_at(t.read().revision, "step", 5));
    REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
    REQUIRE(t.seat->handed.size() == handed + 1);
    CHECK(t.seat->handed.back().revision == wrote.revision);
    CHECK(find_pane_setting(t.seat->handed.back().settings, "step")->number == 5);
}

TEST_CASE("a pane's row is read by message: its revision, what its declaration takes, its pins and what it keeps") {
    RevisionRig t;
    t.open({PaneSetting{"step", {}, 4, {}}});
    const PaneSettingsShown shown = t.read();
    CHECK(shown.provider == kDialOffice);
    CHECK(shown.pane == kDial);
    CHECK(shown.revision == t.seat->handed.back().revision);
    CHECK(shown.rows == dial_declared_v2().rows);
    CHECK(shown.pins == std::vector<std::string>{"legend"});
    CHECK(shown.settings == std::vector<PaneSetting>{PaneSetting{"step", {}, 4, {}}});
    CHECK(shown.locks.empty());
    CHECK(shown.said == "stored and handed");
    // READING AGAIN MOVES NOTHING.
    CHECK(t.read().revision == shown.revision);

    // A LAYOUT THAT DOES NOT SEAT THE PANE: revision 0, its declaration still said, and why.
    layout_key(t.r, Act::kLayoutNew);
    const PaneSettingsShown away = t.read();
    CHECK(away.revision == 0);
    CHECK(away.settings.empty());
    CHECK(away.rows == dial_declared_v2().rows);
    CHECK(away.said == "test.dial/dial has no row on the live layout");
}

TEST_CASE("a commit writes one setting at the revision it read, hands it, and says how far it got") {
    RevisionRig t;
    t.open();
    const std::int64_t before = t.read().revision;
    const PaneSettingActed wrote = t.commit(number_at(before, "step", 5));
    REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
    CHECK(wrote.refusal.empty());
    CHECK(wrote.revision > before);
    CHECK(wrote.handed == "stored and handed");
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"step", {}, 5, {}}});
    CHECK(t.r.last_notice() == "committed step of Dial = 5; stored and handed");
    CHECK(t.read().revision == wrote.revision);

    // A TEXT AMONG ITS CHOICES, AND A FLAG, EACH IN ITS OWN FIELD.
    PaneSettingCommitRequested mode;
    mode.revision = wrote.revision;
    mode.key = "mode";
    mode.text = "narrow";
    const PaneSettingActed narrow = t.commit(mode);
    REQUIRE_MESSAGE(narrow.accepted, narrow.refusal);
    PaneSettingCommitRequested legend;
    legend.revision = narrow.revision;
    legend.key = "legend";
    legend.flag = false;
    const PaneSettingActed off = t.commit(legend);
    REQUIRE_MESSAGE(off.accepted, off.refusal);
    CHECK(t.stored() == std::vector<PaneSetting>{legend_off(),
                                                 PaneSetting{"mode", {}, {}, std::string("narrow")},
                                                 PaneSetting{"step", {}, 5, {}}});

    // THE DEFAULT IS STORED AS ITS ABSENCE, through the same setter Info's rows use.
    const PaneSettingActed one = t.commit(number_at(off.revision, "step", 1));
    REQUIRE_MESSAGE(one.accepted, one.refusal);
    CHECK(find_pane_setting(t.stored(), "step") == nullptr);

    // ...AND A VALUE ALREADY KEPT MINTS NOTHING AND HANDS NOTHING.
    const std::size_t handed = t.seat->handed.size();
    const PaneSettingActed same = t.commit(number_at(one.revision, "step", 1));
    REQUIRE(same.accepted);
    CHECK(same.revision == one.revision);
    CHECK(t.seat->handed.size() == handed);
}

TEST_CASE("a commit is refused in words with nothing written: a stale revision, a value its setting does not take, a clear with a value, a key no declaration takes") {
    RevisionRig t;
    t.open({PaneSetting{"step", {}, 4, {}}});
    const std::int64_t now = t.read().revision;
    const std::size_t handed = t.seat->handed.size();
    PaneSettingCommitRequested text_on_number;
    text_on_number.revision = now;
    text_on_number.key = "step";
    text_on_number.text = "five";
    PaneSettingCommitRequested unchosen;
    unchosen.revision = now;
    unchosen.key = "mode";
    unchosen.text = "!";
    PaneSettingCommitRequested two;
    two.revision = now;
    two.key = "step";
    two.number = 2;
    two.flag = true;
    PaneSettingCommitRequested clear_with_value = number_at(now, "step", 2);
    clear_with_value.clear = true;
    struct Case {
        const char* what;
        PaneSettingCommitRequested asked;
        std::string said;
    };
    const std::vector<Case> cases = {
        {"a stale revision", number_at(now - 1, "step", 2),
         "stale revision " + std::to_string(now - 1) + " -- the row of test.dial/dial stands at " +
             std::to_string(now) + "; read it again"},
        {"no revision", number_at(0, "step", 2),
         "stale revision 0 -- the row of test.dial/dial stands at " + std::to_string(now) +
             "; read it again"},
        {"a text where a number is taken", text_on_number,
         "step takes a whole number from 0 to 9, not a text `five`"},
        {"a number out of range", number_at(now, "step", 99),
         "step takes a whole number from 0 to 9, not a number `99`"},
        {"a text among no choice", unchosen, "mode takes one of wide, narrow, not a text `!`"},
        {"two values", two, "step takes a whole number from 0 to 9, not more than one value"},
        {"a clear with a value", clear_with_value, "a clear takes no value -- name the key alone"},
        {"a key no declaration takes", number_at(now, "zoom", 1),
         "test.dial/dial has no counted setting `zoom` -- a key no declaration takes is only "
         "cleared"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        const PaneSettingActed refused = t.commit(c.asked);
        CHECK_FALSE(refused.accepted);
        CHECK(refused.refusal == c.said);
        CHECK(refused.revision == now);
        CHECK(refused.handed.empty());
        CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"step", {}, 4, {}}});
        CHECK(t.read().revision == now);
        CHECK(t.seat->handed.size() == handed);
    }
    // A REVISION READ BEFORE A SWITCH IS STALE AFTER IT, whatever the settings.
    duplicate_live_layout(t.r);
    const PaneSettingActed late = t.commit(number_at(now, "step", 2));
    CHECK_FALSE(late.accepted);
    CHECK(late.revision > now);
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"step", {}, 4, {}}});
}

TEST_CASE("a clear takes back what the row keeps, declared or not, and clearing nothing changes nothing") {
    RevisionRig t;
    t.open({PaneSetting{"step", {}, 4, {}}, PaneSetting{"zoom", true, {}, {}}});
    PaneSettingCommitRequested zoom;
    zoom.revision = t.read().revision;
    zoom.key = "zoom";
    zoom.clear = true;
    const PaneSettingActed cleared = t.commit(zoom);
    REQUIRE_MESSAGE(cleared.accepted, cleared.refusal);
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"step", {}, 4, {}}});
    CHECK(t.seat->handed.back().settings == t.stored());
    CHECK(t.seat->handed.back().revision == cleared.revision);
    CHECK(t.r.last_notice() == "cleared zoom of Dial; stored and handed");
    // ...AND THE SAME CLEAR AGAIN IS ACCEPTED, minting and handing nothing.
    const std::size_t handed = t.seat->handed.size();
    zoom.revision = cleared.revision;
    const PaneSettingActed again = t.commit(zoom);
    CHECK(again.accepted);
    CHECK(again.revision == cleared.revision);
    CHECK(t.seat->handed.size() == handed);
    // A ROW WHOSE PANE NOTHING OFFERS IS CLEARED TOO: no declaration is needed to take a value back.
    Session& s = const_cast<Session&>(t.r.session());
    const PaneRef ghost{"test.ghost", "ghost"};
    REQUIRE(add_pane(s.setup.active, ghost));
    pane_of(s.setup.active, ghost)->settings = {PaneSetting{"tone", {}, 3, {}}};
    std::vector<PaneSettingsShown>& shown = t.editor->shown;
    drive(t.r, t.editor_id, t.editor, [ghost](RevisionSeat& seat, loom::Mail& m) {
        seat.say(m, PaneSettingsRequested{ghost.provider, ghost.pane});
    });
    REQUIRE_FALSE(shown.empty());
    CHECK(shown.back().revision > 0);
    CHECK(shown.back().rows.empty());
    PaneSettingCommitRequested tone;
    tone.provider = ghost.provider;
    tone.pane = ghost.pane;
    tone.revision = shown.back().revision;
    tone.key = "tone";
    tone.clear = true;
    drive(t.r, t.editor_id, t.editor,
          [tone](RevisionSeat& seat, loom::Mail& m) { seat.say(m, tone); });
    REQUIRE_FALSE(t.editor->acted.empty());
    CHECK_MESSAGE(t.editor->acted.back().accepted, t.editor->acted.back().refusal);
    CHECK(pane_of(t.r.session().setup.active, ghost)->settings.empty());
}

TEST_CASE("an asker the settings doors do not serve is answered that it was withheld, and nothing is written") {
    RevisionRig t;
    t.open({PaneSetting{"step", {}, 4, {}}});
    const std::int64_t now = t.read().revision;
    // PERSONAL SPEECH: no office, not the Terminal this host mounted, no guest row.
    const PaneSettingsShown read = t.read(/*personally=*/true);
    CHECK(read.revision == 0);
    CHECK(read.rows.empty());
    CHECK(read.settings.empty());
    CHECK(read.said ==
          "settings withheld: a pane's settings are read and written by an office, this "
          "Workshop's Terminal or a guest with `settings`");
    const PaneSettingActed wrote = t.commit(number_at(now, "step", 2), /*personally=*/true);
    CHECK_FALSE(wrote.accepted);
    CHECK(wrote.revision == 0);
    CHECK(wrote.refusal.rfind("settings commit withheld: ", 0) == 0);
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"step", {}, 4, {}}});
}

TEST_CASE("a pane's word that it cannot use a setting is kept against its revision, said, and goes when a newer one is handed") {
    RevisionRig t;
    t.open({PaneSetting{"step", {}, 4, {}}});
    const std::int64_t at = t.seat->handed.back().revision;
    const auto unused = [&t](std::int64_t revision, const char* key, const char* why) {
        drive(t.r, t.seat_id, t.seat, [revision, key, why](RevisionSeat& s, loom::Mail& m) {
            s.say(m, PaneSettingUnused{kDial, revision, key, why});
        });
    };
    const auto standing = [&t]() -> std::optional<Condition> {
        const std::vector<Condition> all = attention_conditions(t.r.session());
        const Condition* c = condition_by_key(all, "pane.setting-unused.test.dial/dial.step");
        return c != nullptr ? std::optional<Condition>(*c) : std::nullopt;
    };
    // A REVISION IT WAS NOT HANDED IS LATE, AND KEPT NOWHERE.
    unused(at - 1, "step", "the dial is jammed");
    CHECK_FALSE(standing().has_value());
    CHECK(t.read().said == "stored and handed");

    unused(at, "step", "the dial is jammed");
    REQUIRE(standing().has_value());
    CHECK(standing()->compact == "`test.dial/dial` cannot use its setting step");
    CHECK(standing()->detail == "the dial is jammed");
    CHECK(t.read().said == "stored and handed; it cannot use step -- the dial is jammed");
    // ...SAID PERSONALLY, IT IS NOBODY'S WORD ABOUT THE PANE.
    drive(t.r, t.seat_id, t.seat, [at](RevisionSeat& s, loom::Mail& m) {
        s.say(m, PaneSettingUnused{kDial, at, "mode", "personally"}, /*personally=*/true);
    });
    CHECK(t.read().said == "stored and handed; it cannot use step -- the dial is jammed");
    // ...A WORD IT CANNOT HAVE MEANT IS REFUSED ALOUD.
    unused(at, "Step", "the dial is jammed");
    CHECK(t.r.last_notice().find("Dial @test.dial: a setting it cannot use was not kept -- ") == 0);

    // A NEWER REVISION HANDED TAKES THE WORD WITH THE REVISION IT WAS ABOUT.
    const PaneSettingActed wrote = t.commit(number_at(t.read().revision, "step", 3));
    REQUIRE(wrote.accepted);
    CHECK_FALSE(standing().has_value());
    CHECK(wrote.handed == "stored and handed");
    // ...AND ONE ABOUT A DESK NO LONGER LIVE IS LATE TOO.
    const std::int64_t before_switch = t.seat->handed.back().revision;
    duplicate_live_layout(t.r);
    unused(before_switch, "step", "the dial is jammed");
    CHECK_FALSE(standing().has_value());
}
