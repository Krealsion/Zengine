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

    // A HOLDER WITHOUT THE DOOR: stored, and the sentence says why it went no further.
    t.r.unmount_settings_seat(t.seat);
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
